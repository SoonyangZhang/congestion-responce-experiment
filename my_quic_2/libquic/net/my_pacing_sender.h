#ifndef MYBBR_MY_PACING_SENDER_H_
#define MYBBR_MY_PACING_SENDER_H_
#include "net/my_controller_interface.h"
namespace quic{
class MyPacingSender{
public:
	MyPacingSender();
	~MyPacingSender();
	void set_sender( CongestionController *sender){
        sender_=sender;
    }
	void OnPacketSent(QuicTime now,
			QuicPacketNumber packet_number,
			 QuicByteCount bytes);
	void OnCongestionEvent(QuicTime now,
			QuicPacketNumber packet_number);
	QuicTime::Delta TimeUntilSend(QuicTime now) const;
    void set_max_pacing_rate(QuicBandwidth max_bandwidth){
       max_pacing_rate_=max_bandwidth;
    }
private:
	 QuicTime ideal_next_packet_send_time_;
	 QuicTime::Delta alarm_granularity_;
	 CongestionController *sender_{NULL};
    QuicBandwidth max_pacing_rate_;
};
}
#endif /* MYBBR_MY_PACING_SENDER_H_ */
