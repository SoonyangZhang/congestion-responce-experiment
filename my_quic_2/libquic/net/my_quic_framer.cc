#include "my_quic_framer.h"
#include "net/third_party/quic/core/quic_data_reader.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include <iostream>
#include "my_check.h"
#define QUIC_PREDICT_FALSE(x) x
#define  QUIC_BUG  std::cout
namespace quic{
namespace{
// Number of bytes reserved for the frame type preceding each frame.
const size_t kQuicFrameTypeSize = 1;
// Number of bytes reserved for error code.
const size_t kQuicErrorCodeSize = 4;
// Number of bytes reserved to denote the length of error details field.
const size_t kQuicErrorDetailsLengthSize = 2;

// Maximum number of bytes reserved for stream id.
const size_t kQuicMaxStreamIdSize = 4;
// Maximum number of bytes reserved for byte offset in stream frame.
const size_t kQuicMaxStreamOffsetSize = 8;
// Number of bytes reserved to store payload length in stream frame.
const size_t kQuicStreamPayloadLengthSize = 2;
// Number of bytes to reserve for IQ Error codes (for the Connection Close,
// Application Close, and Reset Stream frames).
const size_t kQuicIetfQuicErrorCodeSize = 2;
// Minimum size of the IETF QUIC Error Phrase's length field
const size_t kIetfQuicMinErrorPhraseLengthSize = 1;

// Size in bytes reserved for the delta time of the largest observed
// packet number in ack frames.
const size_t kQuicDeltaTimeLargestObservedSize = 2;
// Size in bytes reserved for the number of received packets with timestamps.
const size_t kQuicNumTimestampsSize = 1;
// Size in bytes reserved for the number of missing packets in ack frames.
const size_t kNumberOfNackRangesSize = 1;
// Size in bytes reserved for the number of ack blocks in ack frames.
const size_t kNumberOfAckBlocksSize = 1;
// Maximum number of missing packet ranges that can fit within an ack frame.
const size_t kMaxNackRanges = (1 << (kNumberOfNackRangesSize * 8)) - 1;
// Maximum number of ack blocks that can fit within an ack frame.
const size_t kMaxAckBlocks = (1 << (kNumberOfAckBlocksSize * 8)) - 1;

}
namespace {

#define ENDPOINT \
  (perspective_ == Perspective::IS_SERVER ? "Server: " : "Client: ")

// How much to shift the timestamp in the IETF Ack frame.
// TODO(fkastenholz) when we get real IETF QUIC, need to get
// the currect shift from the transport parameters.
const int kIetfAckTimestampShift = 3;

// Number of bits the packet number length bits are shifted from the right
// edge of the header.
const uint8_t kPublicHeaderSequenceNumberShift = 4;

// There are two interpretations for the Frame Type byte in the QUIC protocol,
// resulting in two Frame Types: Special Frame Types and Regular Frame Types.
//
// Regular Frame Types use the Frame Type byte simply. Currently defined
// Regular Frame Types are:
// Padding            : 0b 00000000 (0x00)
// ResetStream        : 0b 00000001 (0x01)
// ConnectionClose    : 0b 00000010 (0x02)
// GoAway             : 0b 00000011 (0x03)
// WindowUpdate       : 0b 00000100 (0x04)
// Blocked            : 0b 00000101 (0x05)
//
// Special Frame Types encode both a Frame Type and corresponding flags
// all in the Frame Type byte. Currently defined Special Frame Types
// are:
// Stream             : 0b 1xxxxxxx
// Ack                : 0b 01xxxxxx
//
// Semantics of the flag bits above (the x bits) depends on the frame type.

// Masks to determine if the frame type is a special use
// and for specific special frame types.
const uint8_t kQuicFrameTypeBrokenMask = 0xE0;   // 0b 11100000
const uint8_t kQuicFrameTypeSpecialMask = 0xC0;  // 0b 11000000
const uint8_t kQuicFrameTypeStreamMask = 0x80;
const uint8_t kQuicFrameTypeAckMask = 0x40;
static_assert(kQuicFrameTypeSpecialMask ==
                  (kQuicFrameTypeStreamMask | kQuicFrameTypeAckMask),
              "Invalid kQuicFrameTypeSpecialMask");

// The stream type format is 1FDOOOSS, where
//    F is the fin bit.
//    D is the data length bit (0 or 2 bytes).
//    OO/OOO are the size of the offset.
//    SS is the size of the stream ID.
// Note that the stream encoding can not be determined by inspection. It can
// be determined only by knowing the QUIC Version.
// Stream frame relative shifts and masks for interpreting the stream flags.
// StreamID may be 1, 2, 3, or 4 bytes.
const uint8_t kQuicStreamIdShift = 2;
const uint8_t kQuicStreamIDLengthMask = 0x03;

// Offset may be 0, 2, 4, or 8 bytes.
const uint8_t kQuicStreamShift = 3;
const uint8_t kQuicStreamOffsetMask = 0x07;

// Data length may be 0 or 2 bytes.
const uint8_t kQuicStreamDataLengthShift = 1;
const uint8_t kQuicStreamDataLengthMask = 0x01;

// Fin bit may be set or not.
const uint8_t kQuicStreamFinShift = 1;
const uint8_t kQuicStreamFinMask = 0x01;

// The format is 01M0LLOO, where
//   M if set, there are multiple ack blocks in the frame.
//  LL is the size of the largest ack field.
//  OO is the size of the ack blocks offset field.
// packet number size shift used in AckFrames.
const uint8_t kQuicSequenceNumberLengthNumBits = 2;
const uint8_t kActBlockLengthOffset = 0;
const uint8_t kLargestAckedOffset = 2;

// Acks may have only one ack block.
const uint8_t kQuicHasMultipleAckBlocksOffset = 5;

// Timestamps are 4 bytes followed by 2 bytes.
const uint8_t kQuicNumTimestampsLength = 1;
const uint8_t kQuicFirstTimestampLength = 4;
const uint8_t kQuicTimestampLength = 2;
// Gaps between packet numbers are 1 byte.
const uint8_t kQuicTimestampPacketNumberGapLength = 1;

// Maximum length of encoded error strings.
const int kMaxErrorStringLength = 256;

const uint8_t kQuicLongHeaderTypeMask = 0x7F;
const uint8_t kQuicShortHeaderTypeMask = 0x07;

const uint8_t kConnectionIdLengthAdjustment = 3;
const uint8_t kDestinationConnectionIdLengthMask = 0xF0;
const uint8_t kSourceConnectionIdLengthMask = 0x0F;

// Returns the absolute value of the difference between |a| and |b|.
QuicPacketNumber Delta(QuicPacketNumber a, QuicPacketNumber b) {
  // Since these are unsigned numbers, we can't just return abs(a - b)
  if (a < b) {
    return b - a;
  }
  return a - b;
}

QuicPacketNumber ClosestTo(QuicPacketNumber target,
                           QuicPacketNumber a,
                           QuicPacketNumber b) {
  return (Delta(target, a) < Delta(target, b)) ? a : b;
}

QuicPacketNumberLength ReadSequenceNumberLength(uint8_t flags) {
  switch (flags & PACKET_FLAGS_8BYTE_PACKET) {
    case PACKET_FLAGS_8BYTE_PACKET:
      return PACKET_6BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_4BYTE_PACKET:
      return PACKET_4BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_2BYTE_PACKET:
      return PACKET_2BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_1BYTE_PACKET:
      return PACKET_1BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_6BYTE_PACKET_NUMBER;
  }
}

QuicPacketNumberLength ReadAckPacketNumberLength(QuicTransportVersion version,
                                                 uint8_t flags) {
  switch (flags & PACKET_FLAGS_8BYTE_PACKET) {
    case PACKET_FLAGS_8BYTE_PACKET:
      return PACKET_6BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_4BYTE_PACKET:
      return PACKET_4BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_2BYTE_PACKET:
      return PACKET_2BYTE_PACKET_NUMBER;
    case PACKET_FLAGS_1BYTE_PACKET:
      return PACKET_1BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_6BYTE_PACKET_NUMBER;
  }
}

QuicShortHeaderType PacketNumberLengthToShortHeaderType(
    QuicPacketNumberLength packet_number_length) {
  switch (packet_number_length) {
    case PACKET_1BYTE_PACKET_NUMBER:
      return SHORT_HEADER_1_BYTE_PACKET_NUMBER;
    case PACKET_2BYTE_PACKET_NUMBER:
      return SHORT_HEADER_2_BYTE_PACKET_NUMBER;
    case PACKET_4BYTE_PACKET_NUMBER:
      return SHORT_HEADER_4_BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Invalid packet number length for short header.";
      return SHORT_HEADER_1_BYTE_PACKET_NUMBER;
  }
}

QuicPacketNumberLength ShortHeaderTypeToPacketNumberLength(
    QuicShortHeaderType short_header_type) {
  switch (short_header_type) {
    case SHORT_HEADER_1_BYTE_PACKET_NUMBER:
      return PACKET_1BYTE_PACKET_NUMBER;
    case SHORT_HEADER_2_BYTE_PACKET_NUMBER:
      return PACKET_2BYTE_PACKET_NUMBER;
    case SHORT_HEADER_4_BYTE_PACKET_NUMBER:
      return PACKET_4BYTE_PACKET_NUMBER;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_6BYTE_PACKET_NUMBER;
  }
}

QuicStringPiece TruncateErrorString(QuicStringPiece error) {
  if (error.length() <= kMaxErrorStringLength) {
    return error;
  }
  return QuicStringPiece(error.data(), kMaxErrorStringLength);
}

size_t TruncatedErrorStringSize(const QuicStringPiece& error) {
  if (error.length() < kMaxErrorStringLength) {
    return error.length();
  }
  return kMaxErrorStringLength;
}

uint8_t GetConnectionIdLengthValue(QuicConnectionIdLength length) {
  if (length == 0) {
    return 0;
  }
  return static_cast<uint8_t>(length - kConnectionIdLengthAdjustment);
}

}  // namespace

namespace {
// Create a mask that sets the last |num_bits| to 1 and the rest to 0.
inline uint8_t GetMaskFromNumBits(uint8_t num_bits) {
  return (1u << num_bits) - 1;
}

// Extract |num_bits| from |flags| offset by |offset|.
uint8_t ExtractBits(uint8_t flags, uint8_t num_bits, uint8_t offset) {
  return (flags >> offset) & GetMaskFromNumBits(num_bits);
}

// Extract the bit at position |offset| from |flags| as a bool.
bool ExtractBit(uint8_t flags, uint8_t offset) {
  return ((flags >> offset) & GetMaskFromNumBits(1)) != 0;
}

// Set |num_bits|, offset by |offset| to |val| in |flags|.
void SetBits(uint8_t* flags, uint8_t val, uint8_t num_bits, uint8_t offset) {
  DCHECK_LE(val, GetMaskFromNumBits(num_bits));
  *flags |= val << offset;
}
// Set the bit at position |offset| to |val| in |flags|.
void SetBit(uint8_t* flags, bool val, uint8_t offset) {
  SetBits(flags, val ? 1 : 0, 1, offset);
}
}//namespace
MyQuicFramer::MyQuicFramer(const ParsedQuicVersionVector& supported_versions
,QuicTime creation_time)
:supported_versions_(supported_versions)
,version_(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED)
,creation_time_(creation_time){
	version_=supported_versions_[0];
}
// static
MyQuicFramer::AckFrameInfo MyQuicFramer::GetAckFrameInfo(
    const QuicAckFrame& frame) {
  AckFrameInfo new_ack_info;
  if (frame.packets.Empty()) {
    return new_ack_info;
  }
  // The first block is the last interval. It isn't encoded with the gap-length
  // encoding, so skip it.
  new_ack_info.first_block_length = frame.packets.LastIntervalLength();
  auto itr = frame.packets.rbegin();
  QuicPacketNumber previous_start = itr->min();
  new_ack_info.max_block_length = itr->Length();
  ++itr;

  // Don't do any more work after getting information for 256 ACK blocks; any
  // more can't be encoded anyway.
  for (; itr != frame.packets.rend() &&
         new_ack_info.num_ack_blocks < std::numeric_limits<uint8_t>::max();
       previous_start = itr->min(), ++itr) {
    const auto& interval = *itr;
    const QuicPacketNumber total_gap = previous_start - interval.max();
    new_ack_info.num_ack_blocks +=
        (total_gap + std::numeric_limits<uint8_t>::max() - 1) /
        std::numeric_limits<uint8_t>::max();
    new_ack_info.max_block_length =
        std::max(new_ack_info.max_block_length, interval.Length());
  }
  return new_ack_info;
}
// static
QuicPacketNumberLength MyQuicFramer::GetMinPacketNumberLength(
    QuicTransportVersion version,
    QuicPacketNumber packet_number) {
  if (packet_number < 1 << (PACKET_1BYTE_PACKET_NUMBER * 8)) {
    return PACKET_1BYTE_PACKET_NUMBER;
  } else if (packet_number < 1 << (PACKET_2BYTE_PACKET_NUMBER * 8)) {
    return PACKET_2BYTE_PACKET_NUMBER;
  } else if (packet_number < UINT64_C(1) << (PACKET_4BYTE_PACKET_NUMBER * 8)) {
    return PACKET_4BYTE_PACKET_NUMBER;
  } else {
    return PACKET_6BYTE_PACKET_NUMBER;
  }
}
// static
size_t MyQuicFramer::GetMinAckFrameSize(
    QuicTransportVersion version,
    QuicPacketNumberLength largest_observed_length) {
  if (version == QUIC_VERSION_99) {
    // The minimal ack frame consists of the following four fields: Largest
    // Acknowledged, ACK Delay, ACK Block Count, and First ACK Block. Minimum
    // size of each is 1 byte.
    return kQuicFrameTypeSize + 4;
  }
  size_t min_size = kQuicFrameTypeSize + largest_observed_length +
                    kQuicDeltaTimeLargestObservedSize;
  return min_size + kQuicNumTimestampsSize;
}
// static
uint8_t MyQuicFramer::GetPacketNumberFlags(
    QuicPacketNumberLength packet_number_length) {
  switch (packet_number_length) {
    case PACKET_1BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_1BYTE_PACKET;
    case PACKET_2BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_2BYTE_PACKET;
    case PACKET_4BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_4BYTE_PACKET;
    case PACKET_6BYTE_PACKET_NUMBER:
    case PACKET_8BYTE_PACKET_NUMBER:
      return PACKET_FLAGS_8BYTE_PACKET;
    default:
      QUIC_BUG << "Unreachable case statement.";
      return PACKET_FLAGS_8BYTE_PACKET;
  }
}
// static
bool MyQuicFramer::AppendPacketNumber(QuicPacketNumberLength packet_number_length,
                                    QuicPacketNumber packet_number,
                                    QuicDataWriter* writer) {
  size_t length = packet_number_length;
  if (length != 1 && length != 2 && length != 4 && length != 6 && length != 8) {
    QUIC_BUG << "Invalid packet_number_length: " << length;
    return false;
  }
  return writer->WriteBytesToUInt64(packet_number_length, packet_number);
}
// static
bool MyQuicFramer::AppendAckBlock(uint8_t gap,
                                QuicPacketNumberLength length_length,
                                QuicPacketNumber length,
                                QuicDataWriter* writer) {
  return writer->WriteUInt8(gap) &&
         AppendPacketNumber(length_length, length, writer);
}
bool MyQuicFramer::AppendAckFrameAndTypeByte(const QuicAckFrame& frame,
                                           QuicDataWriter* writer){
	  const AckFrameInfo new_ack_info = GetAckFrameInfo(frame);
	  QuicPacketNumber largest_acked = LargestAcked(frame);
	  QuicPacketNumberLength largest_acked_length =
	      GetMinPacketNumberLength(version_.transport_version, largest_acked);
	  QuicPacketNumberLength ack_block_length = GetMinPacketNumberLength(
	      version_.transport_version, new_ack_info.max_block_length);
	  // Calculate available bytes for timestamps and ack blocks.
	  int32_t available_timestamp_and_ack_block_bytes =
	      writer->capacity() - writer->length() - ack_block_length -
	      GetMinAckFrameSize(version_.transport_version, largest_acked_length) -
	      (new_ack_info.num_ack_blocks != 0 ? kNumberOfAckBlocksSize : 0);

	  // Write out the type byte by setting the low order bits and doing shifts
	  // to make room for the next bit flags to be set.
	  // Whether there are multiple ack blocks.
	  uint8_t type_byte = 0;
	  SetBit(&type_byte, new_ack_info.num_ack_blocks != 0,
	         kQuicHasMultipleAckBlocksOffset);

	  SetBits(&type_byte, GetPacketNumberFlags(largest_acked_length),
	          kQuicSequenceNumberLengthNumBits, kLargestAckedOffset);

	  SetBits(&type_byte, GetPacketNumberFlags(ack_block_length),
	          kQuicSequenceNumberLengthNumBits, kActBlockLengthOffset);

	  type_byte |= kQuicFrameTypeAckMask;

	  if (!writer->WriteUInt8(type_byte)) {
	    return false;
	  }
	  size_t max_num_ack_blocks = available_timestamp_and_ack_block_bytes /
	                              (ack_block_length + PACKET_1BYTE_PACKET_NUMBER);

	  // Number of ack blocks.
	  size_t num_ack_blocks =
	      std::min(new_ack_info.num_ack_blocks, max_num_ack_blocks);
	  if (num_ack_blocks > std::numeric_limits<uint8_t>::max()) {
	    num_ack_blocks = std::numeric_limits<uint8_t>::max();
	  }

	  // Largest acked.
	  if (!AppendPacketNumber(largest_acked_length, largest_acked, writer)) {
	    return false;
	  }
	  // Largest acked delta time.
	  uint64_t ack_delay_time_us = kUFloat16MaxValue;
	  if (!frame.ack_delay_time.IsInfinite()) {
	    DCHECK_LE(0u, frame.ack_delay_time.ToMicroseconds());
	    ack_delay_time_us = frame.ack_delay_time.ToMicroseconds();
	  }
	  if (!writer->WriteUFloat16(ack_delay_time_us)) {
	    return false;
	  }

	  if (num_ack_blocks > 0) {
	    if (!writer->WriteBytes(&num_ack_blocks, 1)) {
	      return false;
	    }
	  }

	  // First ack block length.
	  if (!AppendPacketNumber(ack_block_length, new_ack_info.first_block_length,
	                          writer)) {
	    return false;
	  }

	  // Ack blocks.
	  if (num_ack_blocks > 0) {
	    size_t num_ack_blocks_written = 0;
	    // Append, in descending order from the largest ACKed packet, a series of
	    // ACK blocks that represents the successfully acknoweldged packets. Each
	    // appended gap/block length represents a descending delta from the previous
	    // block. i.e.:
	    // |--- length ---|--- gap ---|--- length ---|--- gap ---|--- largest ---|
	    // For gaps larger than can be represented by a single encoded gap, a 0
	    // length gap of the maximum is used, i.e.:
	    // |--- length ---|--- gap ---|- 0 -|--- gap ---|--- largest ---|
	    auto itr = frame.packets.rbegin();
	    QuicPacketNumber previous_start = itr->min();
	    ++itr;

	    for (;
	         itr != frame.packets.rend() && num_ack_blocks_written < num_ack_blocks;
	         previous_start = itr->min(), ++itr) {
	      const auto& interval = *itr;
	      const QuicPacketNumber total_gap = previous_start - interval.max();
	      const size_t num_encoded_gaps =
	          (total_gap + std::numeric_limits<uint8_t>::max() - 1) /
	          std::numeric_limits<uint8_t>::max();
	      DCHECK_LE(0u, num_encoded_gaps);

	      // Append empty ACK blocks because the gap is longer than a single gap.
	      for (size_t i = 1;
	           i < num_encoded_gaps && num_ack_blocks_written < num_ack_blocks;
	           ++i) {
	        if (!AppendAckBlock(std::numeric_limits<uint8_t>::max(),
	                            ack_block_length, 0, writer)) {
	          return false;
	        }
	        ++num_ack_blocks_written;
	      }
	      if (num_ack_blocks_written >= num_ack_blocks) {
	        if (QUIC_PREDICT_FALSE(num_ack_blocks_written != num_ack_blocks)) {
	          QUIC_BUG << "Wrote " << num_ack_blocks_written
	                   << ", expected to write " << num_ack_blocks;
	        }
	        break;
	      }

	      const uint8_t last_gap =
	          total_gap -
	          (num_encoded_gaps - 1) * std::numeric_limits<uint8_t>::max();
	      // Append the final ACK block with a non-empty size.
	      if (!AppendAckBlock(last_gap, ack_block_length, interval.Length(),
	                          writer)) {
	        return false;
	      }
	      ++num_ack_blocks_written;
	    }
	    DCHECK_EQ(num_ack_blocks, num_ack_blocks_written);
	  }
	  // Timestamps.
	  // If we don't process timestamps or if we don't have enough available space
	  // to append all the timestamps, don't append any of them.
	  if (process_timestamps_ && writer->capacity() - writer->length() >=
	                                 GetAckFrameTimeStampSize(frame)) {
	    if (!AppendTimestampsToAckFrame(frame, writer)) {
	      return false;
	    }
	  } else {
	    uint8_t num_received_packets = 0;
	    if (!writer->WriteBytes(&num_received_packets, 1)) {
	      return false;
	    }
	  }

	  return true;
}
size_t MyQuicFramer::GetAckFrameTimeStampSize(const QuicAckFrame& ack) {
  if (ack.received_packet_times.empty()) {
    return 0;
  }

  return kQuicNumTimestampsLength + kQuicFirstTimestampLength +
         (kQuicTimestampLength + kQuicTimestampPacketNumberGapLength) *
             (ack.received_packet_times.size() - 1);
}
bool MyQuicFramer::AppendTimestampsToAckFrame(const QuicAckFrame& frame,
                                            QuicDataWriter* writer) {
  DCHECK_GE(std::numeric_limits<uint8_t>::max(),
            frame.received_packet_times.size());
  // num_received_packets is only 1 byte.
  if (frame.received_packet_times.size() >
      std::numeric_limits<uint8_t>::max()) {
    return false;
  }
  uint8_t num_received_packets = frame.received_packet_times.size();
  if (!writer->WriteBytes(&num_received_packets, 1)) {
    return false;
  }
  if (num_received_packets == 0) {
    return true;
  }

  auto it = frame.received_packet_times.begin();
  QuicPacketNumber packet_number = it->first;
  QuicPacketNumber delta_from_largest_observed =
      LargestAcked(frame) - packet_number;

  DCHECK_GE(std::numeric_limits<uint8_t>::max(), delta_from_largest_observed);
  if (delta_from_largest_observed > std::numeric_limits<uint8_t>::max()) {
    return false;
  }

  if (!writer->WriteUInt8(delta_from_largest_observed)) {
    return false;
  }

  // Use the lowest 4 bytes of the time delta from the creation_time_.
  const uint64_t time_epoch_delta_us = UINT64_C(1) << 32;
  uint32_t time_delta_us =
      static_cast<uint32_t>((it->second - creation_time_).ToMicroseconds() &
                            (time_epoch_delta_us - 1));
  if (!writer->WriteUInt32(time_delta_us)) {
    return false;
  }

  QuicTime prev_time = it->second;

  for (++it; it != frame.received_packet_times.end(); ++it) {
    packet_number = it->first;
    delta_from_largest_observed = LargestAcked(frame) - packet_number;

    if (delta_from_largest_observed > std::numeric_limits<uint8_t>::max()) {
      return false;
    }

    if (!writer->WriteUInt8(delta_from_largest_observed)) {
      return false;
    }

    uint64_t frame_time_delta_us = (it->second - prev_time).ToMicroseconds();
    prev_time = it->second;
    if (!writer->WriteUFloat16(frame_time_delta_us)) {
      return false;
    }
  }
  return true;
}
MyQuicFramer::AckFrameInfo::AckFrameInfo()
    : max_block_length(0), first_block_length(0), num_ack_blocks(0) {}

MyQuicFramer::AckFrameInfo::AckFrameInfo(const AckFrameInfo& other) = default;

MyQuicFramer::AckFrameInfo::~AckFrameInfo() {}
}




