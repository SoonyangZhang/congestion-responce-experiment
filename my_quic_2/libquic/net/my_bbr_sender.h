#ifndef MYBBR_MY_BBR_SENDER_H_
#define MYBBR_MY_BBR_SENDER_H_
#include "net/third_party/quic/core/congestion_control/windowed_filter.h"
#include "net/my_quic_random.h"
#include "net/my_controller_interface.h"
#include <map>
#include <vector>
#include <list>
namespace quic{
class MyBbrSender:public BandwidthEstimateInteface,
public CongestionController{
public:
	//in mode START_UP PROBE_RTT,the minimal packet sent rate
	//was maintained.
enum Mode{
		START_UP,
		PROBE_BW,
		PROBE_RTT,
	};
	MyBbrSender(uint64_t min_bps,BandwidthObserver *observer);
	~MyBbrSender();
	QuicBandwidth PaceRate() override;
	QuicBandwidth GetReferenceRate() override;
	QuicBandwidth BandwidthEstimate();
	QuicBandwidth AverageBandwidthEstimate();
	bool ShouldSendProbePacket() override;
	void OnAck(QuicTime event_time,
			QuicPacketNumber packet_number) override;
	void OnPacketSent(QuicTime event_time,
			QuicPacketNumber packet_number,
			QuicByteCount bytes) override;
	void OnEstimateBandwidth(SentClusterInfo *cluster,uint64_t cluster_id,QuicBandwidth sent_bw,
QuicBandwidth acked_bw,bool is_probe) override;
    void UserDefinePeriod(uint32_t ms);
private:
	typedef WindowedFilter<QuicBandwidth,
	                       MaxFilter<QuicBandwidth>,
	                       uint64_t,
	                       uint64_t>
	      MaxBandwidthFilter;
	//may be 20 rtt;
	typedef WindowedFilter<QuicBandwidth,
	                       MaxFilter<QuicBandwidth>,
	                       uint64_t,
	                       uint64_t>
	      AverageBandwidthFilter;
	void UpdateRtt(QuicTime now,
			QuicPacketNumber packet_number);
	void EnterProbeBandwidthMode(QuicTime now);
	void UpdateGainCyclePhase(QuicTime now);
	  // Decides whether to enter or exit PROBE_RTT.
	void MaybeEnterOrExitProbeRtt(QuicTime now,
	                              bool min_rtt_expired);
	void CalculatePacingRate();
	void AddPacketInfoSampler(QuicTime now,
			QuicPacketNumber packet_number,
			QuicByteCount bytes);
	void ClusterInfoClear();
	void RemoveClusterLE(uint64_t round);
	void AddSeqAndTimestamp(QuicTime now,QuicPacketNumber packet_number);
	bool HasSampledLastMinRtt();
	uint64_t min_bps_;
	Mode mode_{START_UP};
	uint64_t round_trip_count_{0};
	std::map<uint64_t,SentClusterInfo*> connection_info_;
	//for handle loss packet
	std::map<QuicPacketNumber,uint64_t> seq_round_map_;
	std::map<QuicPacketNumber,QuicTime> seq_ts_map_;
	QuicTime::Delta min_rtt_;
	QuicTime::Delta recored_min_rtt_;
    QuicTime::Delta global_min_rtt_;
	QuicTime min_rtt_timestamp_;
	QuicTime exit_probe_rtt_at_;
	QuicBandwidth pacing_rate_;
	// The gain currently applied to the pacing rate.
	float pacing_gain_{1.0};
	int cycle_current_offset_{0};
	QuicTime last_cycle_start_;
	QuicPacketNumber last_ack_seq_{0};
	MaxBandwidthFilter max_bandwidth_;
	//get the average of the last 10 round.
	AverageBandwidthFilter avergae_bandwidth_;
    QuicBandwidth max_rate_record_;
    QuicBandwidth sending_rate_;
    QuicBandwidth recover_rate_;
    QuicBandwidth last_target_rate_;
    bool is_recover_effective_{false};
    float insist_factor_{0.95};
    float insist_back_off_{0.6};//0.7
// the insist_back_off_ factor affect fairness and packet loss rate,better smaller than 0.7
    float cogestion_back_off_{0.5};
	uint64_t average_round_{0};
	std::list<SentClusterInfo*> acked_clusters_;
	MyQuicRandom *random_;
	QuicTime::Delta	user_period_;
    BandwidthObserver *observer_{NULL};
};
}
#endif /* MYBBR_MY_BBR_SENDER_H_ */
