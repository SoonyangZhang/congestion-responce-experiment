#include "sampler.h"
namespace quic{
MySampler::MySampler()
:total_bytes_sent_(0)
,total_bytes_acked_(0)
,total_bytes_sent_at_last_acked_packet_(0)
,last_acked_packet_sent_time_(QuicTime::Zero())
,last_acked_packet_ack_time_(QuicTime::Zero())
,last_sent_packet_(0){}
MySampler::~MySampler(){}
void MySampler::OnPacketSent(QuicTime sent_time,
                  QuicPacketNumber packet_number,
                  QuicByteCount bytes,
                  QuicByteCount bytes_in_flight){
	total_bytes_sent_ += bytes;
	if (bytes_in_flight == 0) {
		last_acked_packet_ack_time_ = sent_time;
	    total_bytes_sent_at_last_acked_packet_ = total_bytes_sent_;

	    // In this situation ack compression is not a concern, set send rate to
	    // effectively infinite.
	    last_acked_packet_sent_time_ = sent_time;
	 }
	std::shared_ptr<PacketState> state(new PacketState(sent_time,bytes,*this));
	packet_infos_.insert(std::make_pair(packet_number,state));
}
QuicBandwidth MySampler::OnPacketAcknowledged(QuicTime ack_time,
                                     QuicPacketNumber packet_number,
									   bool round_update){
	QuicBandwidth bw=QuicBandwidth::Zero();
	std::shared_ptr<PacketState> state=GetPacketInfoEntry(packet_number);
	if(state==NULL){
		return bw;
	}
	bw=OnPacketAcknowledgedInner(ack_time,packet_number,*state.get(),round_update);
	PacketInfoRemove(packet_number);
	return bw;
}
void MySampler::OnPacketLost(QuicPacketNumber packet_number){
	PacketInfoRemove(packet_number);
}
std::shared_ptr<MySampler::PacketState> MySampler::GetPacketInfoEntry(QuicPacketNumber seq){
	std::shared_ptr<PacketState> packet;
	auto it=packet_infos_.find(seq);
	if(it!=packet_infos_.end()){
		packet=it->second;
	}
	return packet;
}
void MySampler::PacketInfoRemove(QuicPacketNumber seq){
	auto it=packet_infos_.find(seq);
	if(it!=packet_infos_.end()){
		packet_infos_.erase(it);
	}
}
QuicBandwidth MySampler::OnPacketAcknowledgedInner(
    QuicTime ack_time,
    QuicPacketNumber packet_number,
    const PacketState& sent_packet,bool round_update){
	  total_bytes_acked_ += sent_packet.size;
	  total_bytes_sent_at_last_acked_packet_ = sent_packet.total_bytes_sent;
	  last_acked_packet_sent_time_ = sent_packet.sent_time;
	  last_acked_packet_ack_time_ = ack_time;
	  QuicBandwidth bw=QuicBandwidth::Zero();
	  // There might have been no packets acknowledged at the moment when the
	  // current packet was sent. In that case, there is no bandwidth sample to
	  // make.
	  if(!round_update){
		  return bw;
	  }
	  // only round update, compute the bandwidth
	  if (sent_packet.last_acked_packet_sent_time == QuicTime::Zero()) {
	    return bw;
	  }
	  // Infinite rate indicates that the sampler is supposed to discard the
	  // current send rate sample and use only the ack rate.
	  QuicBandwidth send_rate = QuicBandwidth::Infinite();
	  if (sent_packet.sent_time > sent_packet.last_acked_packet_sent_time) {
	    send_rate = QuicBandwidth::FromBytesAndTimeDelta(
	        sent_packet.total_bytes_sent -
	            sent_packet.total_bytes_sent_at_last_acked_packet,
	        sent_packet.sent_time - sent_packet.last_acked_packet_sent_time);
	  }

	  // During the slope calculation, ensure that ack time of the current packet is
	  // always larger than the time of the previous packet, otherwise division by
	  // zero or integer underflow can occur.
	  if (ack_time <= sent_packet.last_acked_packet_ack_time) {
	    // TODO(wub): Compare this code count before and after fixing clock jitter
	    // issue.
	    if (sent_packet.last_acked_packet_ack_time == sent_packet.sent_time) {
	      // This is the 1st packet after quiescense.
	      //QUIC_CODE_COUNT_N(quic_prev_ack_time_larger_than_current_ack_time, 1, 2);
	    } else {
	      //QUIC_CODE_COUNT_N(quic_prev_ack_time_larger_than_current_ack_time, 2, 2);
	    }
	    //QUIC_LOG(ERROR) << "Time of the previously acked packet:"
	    //                << sent_packet.last_acked_packet_ack_time.ToDebuggingValue()
	    //                << " is larger than the ack time of the current packet:"
	     //               << ack_time.ToDebuggingValue();
	    return bw;
	  }
	  QuicBandwidth ack_rate = QuicBandwidth::FromBytesAndTimeDelta(
	      total_bytes_acked_ -
	          sent_packet.total_bytes_acked_at_the_last_acked_packet,
	      ack_time - sent_packet.last_acked_packet_ack_time);

	  bw = std::min(send_rate, ack_rate);
	  return bw;
}
}




