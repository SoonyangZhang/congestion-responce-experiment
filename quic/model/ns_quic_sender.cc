#include "ns_quic_sender.h"
#include "my_quic_header.h"
#include "net/quic/core/quic_data_reader.h"
#include "net/quic/core/quic_data_writer.h"
#include "net/quic/core/frames/quic_stream_frame.h"
#include "net/quic/core/frames/quic_frame.h"
#include "net/quic/core/quic_pending_retransmission.h"
#include "my_quic_framer.h"
#include "ns3/log.h"
using namespace ns3;
namespace net{
NS_LOG_COMPONENT_DEFINE("NsQuicSender");
const uint8_t kPublicHeaderSequenceNumberShift = 4;
const uint32_t kMaxBufferSize=1500;
const uint32_t kPaddingSize=1000;
NsQuicSender::NsQuicSender(Perspective pespective)
:pespective_(pespective)
,sent_packet_manager_(pespective,&clock_,&stats_,kBBR,kNack)
,time_of_last_received_packet_(clock_.ApproximateNow())
,last_rate_ts_(QuicTime::Zero())
{
	sent_packet_manager_.SetHandshakeConfirmed();
	 versions_.push_back(ParsedQuicVersion(PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43));
}
NsQuicSender::~NsQuicSender(){}
void NsQuicSender::Bind(uint16_t port){
    if (socket_ == NULL) {
        socket_ = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = socket_->Bind (local);
        NS_ASSERT (res == 0);
    }
    bind_port_=port;
    socket_->SetRecvCallback (MakeCallback(&NsQuicSender::RecvPacket,this));
}
ns3::InetSocketAddress NsQuicSender::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
	return InetSocketAddress{local_ip,bind_port_};
}
void NsQuicSender::ConfigurePeer(ns3::Ipv4Address addr,uint16_t port){
	peer_ip_=addr;
	peer_port_=port;
}
void NsQuicSender::OnIncomingData(uint8_t *data,int len){
	if(!running_){
		return;
	}
	QuicTime now=clock_.Now();
	time_of_last_received_packet_=now;
	char buf[kMaxBufferSize]={0};
	memcpy(buf,data,len);
	my_quic_header_t header;
	uint8_t public_flags=0;
	QuicDataReader header_reader(buf,len, NETWORK_BYTE_ORDER);
	header_reader.ReadBytes(&public_flags,1);
	header.seq_len= ReadSequenceNumberLength(
        public_flags >> kPublicHeaderSequenceNumberShift);
	uint64_t seq=0;
	header_reader.ReadBytesToUInt64(header.seq_len,&seq);
	uint32_t header_len=sizeof(uint8_t)+header.seq_len;
	uint32_t remain=len-header_len;
	QuicDataReader reader(buf+header_len,remain, NETWORK_BYTE_ORDER);
	MyQuicFramer framer(versions_,now,pespective_);
	framer.set_visitor(this);
	QuicPacketHeader quic_header;// no use
	framer.ProcessFrameData(&reader,quic_header);
	return ;
}
bool NsQuicSender::OnAckFrame(const QuicAckFrame& frame){
	return false;
}
bool NsQuicSender::OnAckFrameStart(QuicPacketNumber largest_acked,
	                 QuicTime::Delta ack_delay_time){
	sent_packet_manager_.OnAckFrameStart(largest_acked,ack_delay_time,
			time_of_last_received_packet_);
	if(largest_acked>=largest_acked_){
		largest_acked_=largest_acked;
	}
	return true;
}
bool NsQuicSender::OnAckRange(QuicPacketNumber start,
	            QuicPacketNumber end,
	            bool last_range){
	sent_packet_manager_.OnAckRange(start, end);
	if (!last_range) {
	    return true;
	  }
	  bool acked_new_packet =
	      sent_packet_manager_.OnAckFrameEnd(time_of_last_received_packet_);
	  PostProcessAfterAckFrame(GetLeastUnacked() > start, acked_new_packet);
	  return true;
}
void NsQuicSender::HeartBeat(){
	if(!running_){
		return ;
	}
	if(heart_timer_.IsExpired()){
		QuicTime quic_now=clock_.Now();
		RecordRate(quic_now);
		SendRetransmission(quic_now);
		if(stop_waiting_count_>2){
			SendStopWaitingFrame();
		}
		if(sent_packet_manager_.TimeUntilSend(quic_now)==QuicTime::Delta::Zero()){
		SendFakePacket(quic_now);
		}
        uint32_t now=Simulator::Now().GetMilliSeconds();
		Time next=MilliSeconds(heart_beat_t_);
		heart_timer_=Simulator::Schedule(next,
				&NsQuicSender::HeartBeat,this);
	}
}
void NsQuicSender::StartApplication(){
	heart_timer_=Simulator::ScheduleNow(&NsQuicSender::HeartBeat,this);
}
void NsQuicSender::StopApplication(){
	running_=false;
	if(!heart_timer_.IsExpired()){
		heart_timer_.Cancel();
	}
}
void NsQuicSender::RecvPacket(ns3::Ptr<ns3::Socket> socket){
	Address remoteAddr;
	auto packet = socket->RecvFrom (remoteAddr);
	uint32_t recv=packet->GetSize ();
	char *buf=new char[recv];
	packet->CopyData((uint8_t*)buf,recv);
	OnIncomingData((uint8_t*)buf,recv);
	delete buf;
}
void NsQuicSender::SendToNetwork(ns3::Ptr<ns3::Packet> p){
	socket_->SendTo(p,0,InetSocketAddress{peer_ip_,peer_port_});
}
void NsQuicSender::RecordRate(QuicTime now){
	if(last_rate_ts_==QuicTime::Zero()){
		last_rate_ts_=now+QuicTime::Delta::FromMilliseconds(100);
		return ;
	}
	if(now>last_rate_ts_){
		uint32_t bw_kbps=0;
		bw_kbps=sent_packet_manager_.BandwidthEstimate().ToKBitsPerSecond();
		if(!trace_rate_cb_.IsNull()){
			trace_rate_cb_(bw_kbps);
		}
		last_rate_ts_=now+QuicTime::Delta::FromMilliseconds(100);
	}
}
void NsQuicSender::OnPacketSent(QuicPacketNumber packet_number,
                   QuicPacketNumberLength packet_number_length, QuicPacketLength encrypted_length){
	QuicTime now=clock_.Now();
	SerializedPacket info(packet_number,packet_number_length,NULL,encrypted_length,false,false);
	bool retransmittable =HAS_RETRANSMITTABLE_DATA;
	if (retransmittable == HAS_RETRANSMITTABLE_DATA) {
		QuicStreamFrame *stream=new QuicStreamFrame();
      		info.retransmittable_frames.push_back(
         	 QuicFrame(stream));
    	}
	sent_packet_manager_.OnPacketSent(&info,0,now, NOT_RETRANSMISSION,HAS_RETRANSMITTABLE_DATA);

}
void NsQuicSender::SendFakePacket(QuicTime now){
	char buf[kPaddingSize]={0};
	my_quic_header_t header;
	header.seq=seq_;
	header.seq_len=GetMinSeqLength(header.seq);
	uint8_t public_flags=0;
	public_flags |= GetPacketNumberFlags(header.seq_len)
                  << kPublicHeaderSequenceNumberShift;
	QuicDataWriter writer(kPaddingSize, buf, NETWORK_BYTE_ORDER);
	writer.WriteBytes(&public_flags,1);
	writer.WriteBytesToUInt64(header.seq_len,seq_);
	uint8_t type=STREAM_FRAME;
	writer.WriteBytes(&type,1);
    QuicTime::Delta delta=now-QuicTime::Zero();
    uint32_t ts=delta.ToMilliseconds();
    writer.WriteUInt32(ts);
	Ptr<Packet> p=Create<Packet>((uint8_t*)buf,kPaddingSize);
	SendToNetwork(p);
	uint16_t payload=kPaddingSize-(1+header.seq_len+1);
	OnPacketSent(header.seq,(QuicPacketNumberLength)header.seq_len,payload);
    seq_++;
	//std::cout<<"new "<<header.seq<<std::endl;
}
void NsQuicSender::SendRetransmission(QuicTime now){
	bool retrans=false;
	while(sent_packet_manager_.TimeUntilSend(now)==QuicTime::Delta::Zero()){
		if(sent_packet_manager_.HasPendingRetransmissions()){
			QuicPendingRetransmission pending=sent_packet_manager_.NextPendingRetransmission();
			OnRetransPacket(pending,now);
			retrans=true;
		}else{
			break;
		}
	}
	if(retrans){
		//SendStopWaitingFrame();
	}
	return ;
}
void NsQuicSender::OnRetransPacket(QuicPendingRetransmission pending,QuicTime now){
	char buf[kPaddingSize]={0};
	my_quic_header_t header;
	header.seq=seq_;
	header.seq_len=GetMinSeqLength(header.seq);
	uint8_t public_flags=0;
	public_flags |= GetPacketNumberFlags(header.seq_len)
                  << kPublicHeaderSequenceNumberShift;
	QuicDataWriter writer(kPaddingSize, buf, NETWORK_BYTE_ORDER);
	writer.WriteBytes(&public_flags,1);
	writer.WriteBytesToUInt64(header.seq_len,seq_);
	uint8_t type=STREAM_FRAME;
	writer.WriteBytes(&type,1);
    QuicTime::Delta delta=now-QuicTime::Zero();
    uint32_t ts=delta.ToMilliseconds();
    writer.WriteUInt32(ts);
	Ptr<Packet> p=Create<Packet>((uint8_t*)buf,kPaddingSize);
	SendToNetwork(p);
	uint16_t payload=kPaddingSize-(1+header.seq_len+1);
	SerializedPacket info(header.seq,(QuicPacketNumberLength)header.seq_len,NULL,payload,false,false);
	//void QuicPacketCreator::ReserializeAllFrames
	for(const QuicFrame& frame : pending.retransmittable_frames){
		info.retransmittable_frames.push_back(frame);
	}
	uint64_t old_seq=pending. packet_number;
	sent_packet_manager_.OnPacketSent(&info,old_seq,now,LOSS_RETRANSMISSION,HAS_RETRANSMITTABLE_DATA);
    seq_++;
	//std::cout<<"retrans "<<old_seq<<" new seq "<<header.seq<<std::endl;
}
void NsQuicSender::SendStopWaitingFrame(){
	  stop_waiting_count_=0;
	  QuicPacketNumber unack_seq=sent_packet_manager_.GetLeastUnacked();
	  //std::cout<<"unack "<<unack_seq<<" largest ack "<<largest_acked_<<std::endl;
	  char buf[kMaxBufferSize]={0};
	  my_quic_header_t header;
	  header.seq=seq_;
	  header.seq_len=GetMinSeqLength(header.seq);
	  uint8_t public_flags=0;
	  public_flags |= GetPacketNumberFlags(header.seq_len)
	                  << kPublicHeaderSequenceNumberShift;
	  QuicDataWriter writer(kMaxBufferSize, buf, NETWORK_BYTE_ORDER);
	  writer.WriteBytes(&public_flags,1);
	  writer.WriteBytesToUInt64(header.seq_len,seq_);
	  uint8_t type=STOP_WAITING_FRAME;
	  writer.WriteBytes(&type,1);
	  writer.WriteBytesToUInt64(header.seq_len,unack_seq);
	  uint32_t len=writer.length();
	  Ptr<Packet> p=Create<Packet>((uint8_t*)buf,len);
	  SendToNetwork(p);
}
QuicPacketNumber NsQuicSender::GetLeastUnacked() const {
  return sent_packet_manager_.GetLeastUnacked();
}
void NsQuicSender::PostProcessAfterAckFrame(bool send_stop_waiting,
                                              bool acked_new_packet){
	  if (send_stop_waiting) {
	    ++stop_waiting_count_;
	  } else {
	    stop_waiting_count_ = 0;
	  }
}
}
