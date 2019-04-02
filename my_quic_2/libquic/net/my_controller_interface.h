#ifndef MY_CONTROLLER_INTRFACE_H_
#define MY_CONTROLLER_INTRFACE_H_
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/core/quic_time.h"
#include "net/third_party/quic/core/quic_bandwidth.h"
namespace quic{
class BandwidthObserver{
public:
    virtual ~BandwidthObserver(){}
    virtual void OnBandwidthUpdate()=0;
};
class CongestionController{
public:
	virtual ~CongestionController(){}
	virtual void OnAck(QuicTime event_time,
			QuicPacketNumber packet_number) =0;
	virtual void OnPacketSent(QuicTime event_time,
			QuicPacketNumber packet_number,
			QuicByteCount bytes) =0;
	virtual QuicBandwidth PaceRate()=0;
	virtual QuicBandwidth GetReferenceRate()=0;
	virtual bool ShouldSendProbePacket()=0;
};
struct SentPacketInfo{
	SentPacketInfo(QuicPacketNumber seq,
			QuicTime sent_ts,
			QuicTime ack_ts,
			uint64_t len)
	:seq(seq)
	,sent_ts(sent_ts)
	,ack_ts(ack_ts)
	,len(len){}
	QuicPacketNumber seq;
	QuicTime sent_ts;
	QuicTime ack_ts;
	uint64_t len;
};
class SentClusterInfo;
class BandwidthEstimateInteface{
public:
	virtual ~BandwidthEstimateInteface(){}
	virtual	void OnEstimateBandwidth(SentClusterInfo *cluster,uint64_t cluster_id,QuicBandwidth sent_bw,QuicBandwidth acked_bw,bool is_probe)=0;
};
class SentClusterInfo{
public:
	SentClusterInfo(uint64_t cluster_id,BandwidthEstimateInteface *target,bool is_probe);
	~SentClusterInfo();
	void SetPrev(SentClusterInfo *prev){
		prev_=prev;
	}
	void SetNext(SentClusterInfo *next){
		next_=next;
	}
	SentClusterInfo* GetNext(){
		return next_;
	}
	uint64_t GetClusterId(){return cluster_id_;}
	void OnPacketSent(QuicTime now,
			QuicPacketNumber seq,
			uint64_t len);
	void OnAck(QuicPacketNumber seq,
			QuicTime now);
	void GetSentInfo(uint64_t *ms,QuicByteCount *sent);
	void GetAckedInfo(uint64_t *ms,QuicByteCount *acked);
private:
	void TriggerBandwidthEstimate();
	uint64_t cluster_id_{0};
	QuicTime first_send_ts_;
	QuicTime last_send_ts_;
	QuicTime first_ack_ts_;
	QuicTime last_ack_ts_;
	QuicByteCount acked_packets_{0};
	QuicByteCount sent_packets_{0};
	std::vector<SentPacketInfo*> packet_infos_;
	BandwidthEstimateInteface *bw_target_;
	SentClusterInfo *prev_{NULL};
	SentClusterInfo *next_{NULL};
	QuicBandwidth bw_;
    bool is_probe_{false};
};
}
#endif
