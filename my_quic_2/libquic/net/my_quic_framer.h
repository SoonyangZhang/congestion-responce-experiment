#ifndef MY_QUIC_FRAMER_H_
#define MY_QUIC_FRAMER_H_
#include <cstddef>
#include <cstdint>
#include <memory>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_random.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/core/quic_versions.h"
namespace quic{
class QuicDataReader;
class QuicDataWriter;
class MyQuicFramer{
public:
	MyQuicFramer(const ParsedQuicVersionVector& supported_versions,
			QuicTime creation_time);
	void set_process_timestamps(bool process_timestamps) {
	    process_timestamps_ = process_timestamps;
	  }
	bool AppendAckFrameAndTypeByte(const QuicAckFrame& frame,
	                                           QuicDataWriter* writer);
private:
	  struct AckFrameInfo {
	    AckFrameInfo();
	    AckFrameInfo(const AckFrameInfo& other);
	    ~AckFrameInfo();

	    // The maximum ack block length.
	    QuicPacketNumber max_block_length;
	    // Length of first ack block.
	    QuicPacketNumber first_block_length;
	    // Number of ACK blocks needed for the ACK frame.
	    size_t num_ack_blocks;
	  };
	static AckFrameInfo GetAckFrameInfo(
		    const QuicAckFrame& frame);
	static QuicPacketNumberLength GetMinPacketNumberLength(    QuicTransportVersion version,
		    QuicPacketNumber packet_number);
	static size_t GetMinAckFrameSize(
	    QuicTransportVersion version,
	    QuicPacketNumberLength largest_observed_length);
	static uint8_t GetPacketNumberFlags(
	    QuicPacketNumberLength packet_number_length);
	static bool AppendPacketNumber(QuicPacketNumberLength packet_number_length,
	                                    QuicPacketNumber packet_number,
	                                    QuicDataWriter* writer);
	static bool AppendAckBlock(uint8_t gap,
	                                QuicPacketNumberLength length_length,
	                                QuicPacketNumber length,
	                                QuicDataWriter* writer);
	  // Computes the wire size in bytes of time stamps in |ack|.
	size_t GetAckFrameTimeStampSize(const QuicAckFrame& ack);
	bool AppendTimestampsToAckFrame(const QuicAckFrame& frame,
	                                QuicDataWriter* writer);
	bool process_timestamps_{false};
	ParsedQuicVersionVector supported_versions_;
	ParsedQuicVersion version_;
	QuicTime creation_time_;
};
}




#endif /* MY_QUIC_FRAMER_H_ */
