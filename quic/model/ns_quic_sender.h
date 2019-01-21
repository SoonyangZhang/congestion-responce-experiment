#ifndef NS_QUIC_NS_QUIC_SENDER_H_
#define NS_QUIC_NS_QUIC_SENDER_H_
#include "net/quic/core/quic_sent_packet_manager.h"
#include "net/quic/core/quic_connection_stats.h"
#include "net/quic/core/quic_versions.h"
#include "net/quic/core/quic_pending_retransmission.h"
#include "ns_quic_time.h"
#include "quic_framer_visitor.h"
#include <string>
#include "ns3/callback.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/event-id.h"
namespace net{
class NsQuicSender:public AbstractQuicFramerVisitor
,public ns3::Application{
public:
	NsQuicSender();
	~NsQuicSender();
	void Bind(uint16_t port);
	ns3::InetSocketAddress GetLocalAddress();
	void ConfigurePeer(ns3::Ipv4Address addr,uint16_t port);
	typedef ns3::Callback<void,uint32_t>TraceRate;
	void SetRateTraceFunc(TraceRate cb){
		trace_rate_cb_=cb;
	}
	void OnIncomingData(uint8_t *data,int len);
	bool OnAckFrame(const QuicAckFrame& frame) override;
	bool OnAckFrameStart(QuicPacketNumber largest_acked,
		                 QuicTime::Delta ack_delay_time) override;
	bool OnAckRange(QuicPacketNumber start,
		            QuicPacketNumber end,
		            bool last_range) override;
	void HeartBeat();
private:
	virtual void StartApplication() override;
	virtual void StopApplication() override;
	void RecvPacket(ns3::Ptr<ns3::Socket> socket);
	void SendToNetwork(ns3::Ptr<ns3::Packet> p);

	void RecordRate(QuicTime now);
	void OnPacketSent(QuicPacketNumber packet_number,
                   QuicPacketNumberLength packet_number_length, QuicPacketLength encrypted_length);
	void SendFakePacket(QuicTime now);
	void SendRetransmission(QuicTime now);
	void OnRetransPacket(QuicPendingRetransmission pending,QuicTime now);
	void SendStopWaitingFrame();
	QuicPacketNumber GetLeastUnacked() const;
	void PostProcessAfterAckFrame(bool send_stop_waiting, bool acked_new_packet);
	Perspective pespective_;
	ns3::NsQuicClock clock_;
	QuicConnectionStats stats_;
	QuicSentPacketManager sent_packet_manager_;
	ParsedQuicVersionVector  versions_;
	QuicTime time_of_last_received_packet_;
	QuicTime last_rate_ts_;
	bool running_{true};
    TraceRate trace_rate_cb_;
    ns3::Ipv4Address peer_ip_;
    uint16_t peer_port_;
    ns3::Ptr<ns3::Socket> socket_;
    uint16_t bind_port_;
	uint64_t seq_{1};
	int stop_waiting_count_{0};
	QuicPacketNumber largest_acked_{1};
    ns3::EventId heart_timer_;
    uint32_t heart_beat_t_{1};//1 ms;
    int64_t bps_{-1};
};
}



#endif /* NS_QUIC_NS_QUIC_SENDER_H_ */
