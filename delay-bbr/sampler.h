#ifndef NS3_MPVIDEO_CONGESTION_SAMPLER_H_
#define NS3_MPVIDEO_CONGESTION_SAMPLER_H_
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/core/quic_bandwidth.h"
#include <memory>
#include <map>
namespace quic{
class MySampler{
public:
	MySampler();
	~MySampler();
	  void OnPacketSent(QuicTime sent_time,
	                    QuicPacketNumber packet_number,
	                    QuicByteCount bytes,
	                    QuicByteCount bytes_in_flight);
	  QuicBandwidth OnPacketAcknowledged(QuicTime ack_time,
	                                       QuicPacketNumber packet_number,
										   bool round_update);
	  void OnPacketLost(QuicPacketNumber packet_number);
private:
	  // The total number of congestion controlled bytes sent during the connection.
	  QuicByteCount total_bytes_sent_;

	  // The total number of congestion controlled bytes which were acknowledged.
	  QuicByteCount total_bytes_acked_;

	  // The value of |total_bytes_sent_| at the time the last acknowledged packet
	  // was sent. Valid only when |last_acked_packet_sent_time_| is valid.
	  QuicByteCount total_bytes_sent_at_last_acked_packet_;

	  // The time at which the last acknowledged packet was sent. Set to
	  // QuicTime::Zero() if no valid timestamp is available.
	  QuicTime last_acked_packet_sent_time_;

	  // The time at which the most recent packet was acknowledged.
	  QuicTime last_acked_packet_ack_time_;

	  // The most recently sent packet.
	  QuicPacketNumber last_sent_packet_;
private:
	  struct PacketState{
		  // Time at which the packet is sent.
		    QuicTime sent_time;

		    // Size of the packet.
		    QuicByteCount size;

		    // The value of |total_bytes_sent_| at the time the packet was sent.
		    // Includes the packet itself.
		    QuicByteCount total_bytes_sent;

		    // The value of |total_bytes_sent_at_last_acked_packet_| at the time the
		    // packet was sent.
		    QuicByteCount total_bytes_sent_at_last_acked_packet;

		    // The value of |last_acked_packet_sent_time_| at the time the packet was
		    // sent.
		    QuicTime last_acked_packet_sent_time;

		    // The value of |last_acked_packet_ack_time_| at the time the packet was
		    // sent.
		    QuicTime last_acked_packet_ack_time;

		    // The value of |total_bytes_acked_| at the time the packet was
		    // sent.
		    QuicByteCount total_bytes_acked_at_the_last_acked_packet;
		    // Snapshot constructor. Records the current state of the bandwidth
		    // sampler.
		    PacketState(QuicTime sent_time,
		                                QuicByteCount size,
		                                const MySampler& sampler)
		        : sent_time(sent_time),
		          size(size),
		          total_bytes_sent(sampler.total_bytes_sent_),
		          total_bytes_sent_at_last_acked_packet(
		              sampler.total_bytes_sent_at_last_acked_packet_),
		          last_acked_packet_sent_time(sampler.last_acked_packet_sent_time_),
		          last_acked_packet_ack_time(sampler.last_acked_packet_ack_time_),
		          total_bytes_acked_at_the_last_acked_packet(
		              sampler.total_bytes_acked_){}

		    // Default constructor.  Required to put this structure into
		    // PacketNumberIndexedQueue.
		    PacketState()
		        : sent_time(QuicTime::Zero()),
		          size(0),
		          total_bytes_sent(0),
		          total_bytes_sent_at_last_acked_packet(0),
		          last_acked_packet_sent_time(QuicTime::Zero()),
		          last_acked_packet_ack_time(QuicTime::Zero()),
		          total_bytes_acked_at_the_last_acked_packet(0){}
	  };
	  std::shared_ptr<PacketState> GetPacketInfoEntry(QuicPacketNumber seq);
	  void PacketInfoRemove(QuicPacketNumber seq);
	  QuicBandwidth OnPacketAcknowledgedInner(
	      QuicTime ack_time,
	      QuicPacketNumber packet_number,
	      const PacketState& sent_packet,bool round_update);
	  std::map<QuicPacketNumber,std::shared_ptr<PacketState>> packet_infos_;
};
template<typename T>
T Max(const T &a,const T&b){
    T ret;
    if(a>b){
        ret=a;
    }else{
        ret=b;
    }
    return ret;
}
template<typename T>
T Min(const T &a,const T&b){
    T ret;
    if(a>b){
        ret=b;
    }else{
        ret=a;
    }
    return ret;
}
}
#endif /* NS3_MPVIDEO_CONGESTION_SAMPLER_H_ */
