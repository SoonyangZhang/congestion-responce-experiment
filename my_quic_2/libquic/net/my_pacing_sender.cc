#include "net/my_pacing_sender.h"
namespace quic{
MyPacingSender::MyPacingSender()
:ideal_next_packet_send_time_(QuicTime::Zero())
,alarm_granularity_(QuicTime::Delta::FromMilliseconds(1))
,sender_(NULL)
,max_pacing_rate_(QuicBandwidth::Zero()){

}
MyPacingSender::~MyPacingSender(){

}
void MyPacingSender::OnPacketSent(QuicTime now,
		QuicPacketNumber packet_number,
		 QuicByteCount bytes){
	if(sender_){
		sender_->OnPacketSent(now,packet_number,bytes);
		QuicBandwidth pacing_rate=sender_->PaceRate();
        if(max_pacing_rate_!=QuicBandwidth::Zero()){
            if(pacing_rate>max_pacing_rate_){
                pacing_rate=max_pacing_rate_;
            }
        }
		QuicTime::Delta delay=pacing_rate.TransferTime(bytes);
	    ideal_next_packet_send_time_ =
	        std::max(ideal_next_packet_send_time_ + delay, now + delay);
	}
}
void MyPacingSender::OnCongestionEvent(QuicTime now,
		QuicPacketNumber packet_number){
	if(sender_){
		sender_->OnAck(now,packet_number);
	}
}
QuicTime::Delta MyPacingSender::TimeUntilSend(QuicTime now) const{
	if(ideal_next_packet_send_time_>now+alarm_granularity_){
		return ideal_next_packet_send_time_ - now;
	}
	return QuicTime::Delta::Zero();
}
}
