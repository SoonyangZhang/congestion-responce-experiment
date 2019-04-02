#ifndef MYQUIC_MY_QUIC_HEADER_H_
#define MYQUIC_MY_QUIC_HEADER_H_
#include <stdint.h>
#include "net/third_party/quic/core/quic_types.h"
namespace quic{
typedef struct{
	uint8_t seq_len;
	uint64_t seq;
}my_quic_header_t;
QuicPacketNumberLength GetMinSeqLength(uint64_t packet_number);
QuicPacketNumberLength ReadSequenceNumberLength(uint8_t flags);
uint8_t GetPacketNumberFlags(uint8_t seq_len);
}
#endif /* MYQUIC_MY_QUIC_HEADER_H_ */

