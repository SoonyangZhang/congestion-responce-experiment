#include "net/my_controller_interface.h"
namespace quic{
SentClusterInfo::SentClusterInfo(uint64_t cluster_id,BandwidthEstimateInteface *target,bool is_probe)
:cluster_id_(cluster_id)
,first_send_ts_(QuicTime::Zero())
,last_send_ts_(QuicTime::Zero())
,first_ack_ts_(QuicTime::Zero())
,last_ack_ts_(QuicTime::Zero())
,bw_target_(target)
,bw_(QuicBandwidth::Zero())
,is_probe_(is_probe){}
SentClusterInfo::~SentClusterInfo(){
	while(!packet_infos_.empty()){
		auto it=packet_infos_.begin();
		SentPacketInfo *packet_info=(*it);
		delete packet_info;
        packet_infos_.erase(it);
	}
	std::vector<SentPacketInfo*> null_vec;
	null_vec.swap(packet_infos_);
}
void SentClusterInfo::OnPacketSent(QuicTime now,
		QuicPacketNumber seq,
		uint64_t len){
	if(first_send_ts_==QuicTime::Zero()){
		first_send_ts_=now;
	}
	last_send_ts_=now;
	SentPacketInfo *packet_info=new SentPacketInfo(seq,now,QuicTime::Zero(),len);
	packet_infos_.push_back(packet_info);
	sent_packets_+=len;
}
void SentClusterInfo::OnAck(QuicPacketNumber seq,
		QuicTime now){
	if(first_ack_ts_==QuicTime::Zero()){
		first_ack_ts_=now;
		if(prev_){
			prev_->TriggerBandwidthEstimate();
		}
	}
	last_ack_ts_=now;
	SentPacketInfo *head=packet_infos_[0];
	SentPacketInfo *target=NULL;
	QuicPacketNumber head_seq=head->seq;
    if(seq>=head->seq){
	uint32_t offset=seq-head_seq;
	    if(offset<packet_infos_.size()){
	    	target=packet_infos_[offset];
	    }
	    if(target){
		    target->ack_ts=now;
		    acked_packets_+=target->len;
	    }
    }
}
void SentClusterInfo::GetSentInfo(uint64_t *ms,QuicByteCount *sent){
	QuicTime::Delta delta=last_send_ts_-first_send_ts_;
	*ms=delta.ToMilliseconds();
	*sent=sent_packets_;
}
void SentClusterInfo::GetAckedInfo(uint64_t *ms,QuicByteCount *acked){
	QuicTime::Delta delta=last_ack_ts_-first_ack_ts_;
	*ms=delta.ToMilliseconds();
	*acked=acked_packets_;
}
void SentClusterInfo::TriggerBandwidthEstimate(){
	if(acked_packets_==0||last_ack_ts_==first_ack_ts_){
		return ;
	}
	QuicBandwidth send_rate = QuicBandwidth::Infinite();
	if(last_send_ts_>first_send_ts_){
		send_rate=QuicBandwidth::FromBytesAndTimeDelta(sent_packets_,
				last_send_ts_-first_send_ts_);
	}
	 QuicBandwidth ack_rate=QuicBandwidth::FromBytesAndTimeDelta(acked_packets_,
			 last_ack_ts_-first_ack_ts_);
	bw_=std::min(send_rate,ack_rate);
	if(bw_>QuicBandwidth::Zero()){
		bw_target_->OnEstimateBandwidth(this,cluster_id_,send_rate,ack_rate,is_probe_);
	}
}
}



