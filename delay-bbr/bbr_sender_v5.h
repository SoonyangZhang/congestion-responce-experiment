#ifndef NS3_MPVIDEO_CONGESTION_BBR_SENDER_V5_H_
#define NS3_MPVIDEO_CONGESTION_BBR_SENDER_V5_H_
#include "net/third_party/quic/core/congestion_control/windowed_filter.h"
#include "net/my_quic_random.h"
#include "net/my_controller_interface.h"
#include <map>
#include <vector>
#include <list>
#include "net/third_party/quic/core/congestion_control/windowed_filter.h"
#include "net/my_quic_random.h"
#include <map>
#include <vector>
#include <list>
#include <memory>
#include <string>
#include "sampler.h"
#include "ns3/callback.h"
//consider queue delay into consideration,
namespace quic{
class MyBbrSenderV5:public BandwidthEstimateInteface,
public CongestionController{
public:
	enum Mode{
			ST_START,
			ST_DRAIN,
			ST_INCREASE,
			ST_DECREASE,
		};
	MyBbrSenderV5(uint64_t min_bps,BandwidthObserver *observer);
	~MyBbrSenderV5();
	QuicBandwidth PaceRate() override;
	QuicBandwidth GetReferenceRate() override;
	QuicBandwidth BandwidthEstimate();
	bool ShouldSendProbePacket() override;
	void OnAck(QuicTime event_time,
			QuicPacketNumber packet_number) override;
	void OnPacketSent(QuicTime event_time,
			QuicPacketNumber packet_number,
			QuicByteCount bytes) override;
	void OnEstimateBandwidth(SentClusterInfo *cluster,uint64_t cluster_id,QuicBandwidth sent_bw,
QuicBandwidth acked_bw,bool is_probe) override{

	}
	typedef ns3::Callback<void,uint64_t,std::string> TraceState;
	void SetStateTraceFunc(TraceState cb){
		trace_state_cb_=cb;
	}
private:
	TraceState trace_state_cb_;
private:
	struct PerPacket{
		QuicTime sent_ts;
		QuicByteCount bytes;
		PerPacket(QuicTime now,QuicByteCount payload)
		:sent_ts(now),bytes(payload){}
	};
	typedef WindowedFilter<QuicBandwidth,
	                       MaxFilter<QuicBandwidth>,
	                       uint64_t,
	                       uint64_t>
	      MaxBandwidthFilter;
	void EnterStartUpMode();
	void MaybeExitStartupOrDrain(QuicTime now);
	void EnterIncreaseMode(QuicTime now);
	// ST_INCREASE-->ST_DECREASE
	// the network is in congestion status.
	void MaybeEnterOrExitDecrease(QuicTime now,
	                              bool min_rtt_expired,int congestion_reason,int64_t round);
	void CheckIfFullBandwidthReached();
	int CheckIfCongestion(QuicPacketNumber packet_number);
	QuicTime::Delta GetMinRtt();
	QuicByteCount GetTargetCongestionWindow(float gain);
	QuicByteCount GetTargetInflightInDecrease(float gain) ;
	void UpdateGainCyclePhase(QuicTime now);
	void CalculatePacingRate();
	bool UpdateRoundTripCounter(QuicPacketNumber last_acked_packet);
	void AddPacketInfo(QuicTime now,QuicPacketNumber packet_number,QuicByteCount payload);
	void UpdateRttAndInflight(QuicTime now,
			QuicPacketNumber packet_number);
	void UpdateMaxBw();
	std::string GetStateString();
	void PrintDebugInfo(uint64_t bps,std::string state);
   // void UpdateCongestionSignal(QuicPacketNumber ack_seq);
	QuicBandwidth min_bw_;
	BandwidthObserver *observer_{NULL};
	Mode mode_{ST_START};
	// The packet number of the most recently sent packet.
	QuicPacketNumber last_sent_packet_{0};
	QuicPacketNumber last_ack_seq_{0};
	// Acknowledgement of any packet after |current_round_trip_end_| will cause
	// the round trip counter to advance.
	QuicPacketCount current_round_trip_end_{0};
	uint64_t round_trip_count_{0};
	uint64_t last_backoff_count_{0};
	QuicPacketNumber seq_at_backoff_{0};
    bool congestion_backoff_flag_{false};
	QuicTime::Delta  base_line_rtt_{QuicTime::Delta::Infinite()};
    QuicTime base_line_rtt_timestamp_{QuicTime::Zero()};
	std::map<QuicPacketNumber,std::shared_ptr<PerPacket>> sent_packets_map_;
	QuicTime::Delta min_rtt_;
    //QuicTime::Delta cur_rtt_{QuicTime::Delta::Zero()};
    //QuicTime cur_rtt_timestamp_{QuicTime::Zero()};
	QuicTime::Delta s_rtt_;
	QuicTime min_rtt_timestamp_;
	QuicTime::Delta min_rtt_in_decrease_;
	QuicTime min_rtt_timestamp_in_decrease_;
	QuicTime::Delta min_rtt_record_;
	QuicTime exit_probe_rtt_at_;
	QuicBandwidth max_bw_in_increase_;
	QuicBandwidth pacing_rate_;
    QuicBandwidth reference_rate_{QuicBandwidth::Zero()};
	MaxBandwidthFilter max_bandwidth_;
	float pacing_gain_{1.0};
	// The pacing gain applied during the STARTUP phase.
	float high_gain_;
	  // The pacing gain applied during the DRAIN phase.
	float drain_gain_;
	// The number of RTTs to stay in STARTUP mode.  Defaults to 3.
	int64_t num_startup_rtts_;
	//max_bw_in_increase_*loss_gain, last 3 times, the link is in congestion status
	int64_t num_bw_decrease_rtts_;
	// Indicates whether the connection has reached the full bandwidth mode.
	bool is_at_full_bandwidth_{false};

	// Number of round-trips in PROBE_BW mode, used for determining the current
	// pacing gain cycle.
	int cycle_current_offset_{0};
	// The time at which the last pacing gain cycle was started.
	QuicTime last_cycle_start_{QuicTime::Zero()};
	std::shared_ptr<MyQuicRandom> random_;

	// Number of rounds during which there was no significant bandwidth increase.
	uint64_t rounds_without_bandwidth_gain_{0};
	// The bandwidth compared to which the increase is measured.
	QuicBandwidth bandwidth_at_last_round_;
	QuicByteCount bytes_in_flight_{0};
    QuicByteCount bdp_{0};
	// The initial value of the |congestion_window_|.
	QuicByteCount initial_congestion_window_;
    float dynamic_congestion_back_off_{0.0};
    QuicPacketNumber congestion_recv_seq_{0};
    QuicPacketNumber congestion_recv_cut_seq_{0};
    bool min_rtt_expired_{false};
	MySampler sampler_;
};
}
#endif /* NS3_MPVIDEO_CONGESTION_BBR_SENDER_V5_H_ */
