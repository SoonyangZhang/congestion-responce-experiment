#ifndef NS_QUIC_NS_QUIC_RECEIVER_H_
#define NS_QUIC_NS_QUIC_RECEIVER_H_
#include "net/quic/core/quic_received_packet_manager.h"
#include "net/quic/core/quic_connection_stats.h"
#include "ns_quic_time.h"
#include "ns3/callback.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/event-id.h"
#include <string>
namespace net{
class MyQuicAlarm{
public:
	MyQuicAlarm() : m_deadline(QuicTime::Zero()) {}
  bool IsSet() const {
	  return m_deadline >QuicTime::Zero();
  }
  bool IsExpired(QuicTime now_ms)
  {
      bool ret = false;
      if (IsSet())
      {
          ret = now_ms >= m_deadline;
          if (ret)
          {
              m_deadline = QuicTime::Zero();
          }
      }
      return ret;
  }
  void Update(QuicTime new_deadline_ms)
  {
      m_deadline = new_deadline_ms;
  }

private:
  QuicTime m_deadline;
};
class NsQuicReceiver:public ns3::Application{
public:
	NsQuicReceiver();
    ~NsQuicReceiver();
	void Bind(uint16_t port);
	ns3::InetSocketAddress GetLocalAddress();
	void OnIncomingData(char *data, int len);
	typedef ns3::Callback<void,uint32_t,uint32_t>TraceLoss;
	void SetLossTraceFunc(TraceLoss cb){
		trace_loss_cb_=cb;
	}
	typedef ns3::Callback<void,uint32_t,uint32_t>TraceOwd;
	void SetOwdTraceFunc(TraceOwd cb){
		trace_owd_cb_=cb;
	}
	typedef ns3::Callback<void,uint32_t>TraceRate;
	void SetRateTraceFunc(TraceRate cb){
		trace_rate_cb_=cb;
	}
	void HeartBeat();
private:
	virtual void StartApplication() override;
	virtual void StopApplication() override;
	void RecvPacket(ns3::Ptr<ns3::Socket> socket);
	void SendToNetwork(ns3::Ptr<ns3::Packet> p);
	void MaybeSendAck();
	void SendAck();
    void RecordLoss(uint32_t seq);
    void RecordRate(QuicTime now);
    void RecordOwd(uint32_t seq,uint32_t owd);
    bool first_{true};
    ns3::NsQuicClock clock_;
	QuicConnectionStats stats_;
	QuicReceivedPacketManager recv_packet_manager_;
	ParsedQuicVersionVector  versions_;
	QuicTime last_rate_ts_{QuicTime::Zero()};
	bool ack_sent_{false};
	uint64_t seq_{1};
	uint64_t base_seq_{0};
	bool running_{true};
	MyQuicAlarm ack_alarm_;
	uint64_t received_{0};
	uint64_t recv_byte_{0};
	uint64_t m_num_packets_received_since_last_ack_sent{0};
    TraceLoss trace_loss_cb_;
    TraceOwd trace_owd_cb_;
    TraceRate trace_rate_cb_;
    uint32_t owd_{0};
    ns3::Ipv4Address peer_ip_;
    uint16_t peer_port_;
    ns3::Ptr<ns3::Socket> socket_;
    uint16_t bind_port_;

    ns3::EventId heart_timer_;
    uint32_t heart_beat_t_{1};//1 ms;
};
}

#endif /* NS_QUIC_NS_QUIC_RECEIVER_H_ */
