#include "quic_framer_visitor.h"
#include <iostream>
namespace net{
// decide by use_incremental_ack_processing_;
bool  AckFrameVisitor::OnAckFrame(const QuicAckFrame& frame){
	ack_frame_=frame;
        std::cout<<"on ack frame"<<std::endl;
	return false;
}
bool AckFrameVisitor::OnAckFrameStart(QuicPacketNumber largest_acked,
	                 	 	 	 	 QuicTime::Delta ack_delay_time){
	std::cout<<"largest acked "<<largest_acked<<std::endl;
	return true;
}
bool AckFrameVisitor::OnAckRange(QuicPacketNumber start,
	            QuicPacketNumber end,
	            bool last_range){
    std::cout<<"s "<<start<<" e "<<end<<std::endl;
	return true;
}
}



