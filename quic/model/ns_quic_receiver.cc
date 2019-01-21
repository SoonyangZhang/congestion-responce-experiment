#include "ns_quic_receiver.h"
#include "net/quic/core/quic_data_reader.h"
#include "net/quic/core/quic_data_writer.h"
#include "my_quic_header.h"
#include "my_quic_framer.h"
#include "quic_framer_visitor.h"
#include "ns3/log.h"
using namespace ns3;
namespace net{
const uint8_t kPublicHeaderSequenceNumberShift = 4;
const uint64_t mMinReceivedBeforeAckDecimation = 100;

// Wait for up to 10 retransmittable packets before sending an ack.
const uint64_t mMaxRetransmittablePacketsBeforeAck = 10;
// Maximum delayed ack time, in ms.
const int64_t mMaxDelayedAckTimeMs = 25;
// TCP RFC calls for 1 second RTO however Linux differs from this default and
// define the minimum RTO to 200ms, we will use the same until we have data to
// support a higher or lower value.
static const int64_t mMinRetransmissionTimeMs = 200;
const uint32_t kMaxBufferSize=1500;

NsQuicReceiver::NsQuicReceiver()
:recv_packet_manager_(&stats_){
	versions_.push_back(ParsedQuicVersion( PROTOCOL_QUIC_CRYPTO, QUIC_VERSION_43));
}
NsQuicReceiver::~NsQuicReceiver(){}
void NsQuicReceiver::Bind(uint16_t port){
    if (socket_ == NULL) {
        socket_ = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = socket_->Bind (local);
        NS_ASSERT (res == 0);
    }
    bind_port_=port;
    socket_->SetRecvCallback (MakeCallback(&NsQuicReceiver::RecvPacket,this));
}
ns3::InetSocketAddress NsQuicReceiver::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
	return InetSocketAddress{local_ip,bind_port_};
}
void NsQuicReceiver::OnIncomingData(char *data,int len){
	if(!running_){
		return ;
	}
	QuicDataReader reader(data,len, NETWORK_BYTE_ORDER);
	my_quic_header_t header;
	uint8_t public_flags=0;
	reader.ReadBytes(&public_flags,1);
	header.seq_len= ReadSequenceNumberLength(
        public_flags >> kPublicHeaderSequenceNumberShift);
	uint64_t seq=0;
	reader.ReadBytesToUInt64(header.seq_len,&seq);
	//uint32_t header_len=sizeof(uint8_t)+header.seq_len;
	uint8_t type=0;
	reader.ReadBytes(&type,1);
	if(type==STREAM_FRAME){
		QuicTime now=clock_.Now();
		QuicPacketHeader fakeheader;
		fakeheader.packet_number=seq;
        QuicTime::Delta delta=now-QuicTime::Zero();
        uint32_t recv_ts=delta.ToMilliseconds();
        uint32_t sent_ts=0;
        reader.ReadUInt32(&sent_ts);
        owd_=recv_ts-sent_ts;
        RecordOwd(seq,owd_);
		{
		  recv_packet_manager_.RecordPacketReceived(fakeheader,now);
		  if(seq>=base_seq_+1){
            uint64_t i=0;
            for(i=base_seq_+1;i<seq;i++){
                RecordLoss(i);
            }
			//std::cout<<"l "<<base_seq_+1<<std::endl;
		  }
		  if(seq>=base_seq_+1){
			  base_seq_=seq;
		  }
		}
		m_num_packets_received_since_last_ack_sent++;
		received_++;
		recv_byte_+=len;
		RecordRate(now);
		if(recv_packet_manager_.ack_frame_updated()){
			MaybeSendAck();
		}
	}
	if(type==STOP_WAITING_FRAME){
		QuicPacketNumber least_unack;
		reader.ReadBytesToUInt64(header.seq_len,&least_unack);
		recv_packet_manager_.DontWaitForPacketsBefore(least_unack);
		//std::cout<<"stop waititng "<<least_unack<<std::endl;
	}
}
void NsQuicReceiver::HeartBeat(){
	if(!running_){
		return ;
	}
	if(heart_timer_.IsExpired()){
		QuicTime now=clock_.Now();
		if(ack_alarm_.IsExpired(clock_.Now())){
			SendAck();
		}
		Time next=MilliSeconds(heart_beat_t_);
		heart_timer_=Simulator::Schedule(next,
				&NsQuicReceiver::HeartBeat,this);
	}
}
void NsQuicReceiver::StartApplication(){
	heart_timer_=Simulator::ScheduleNow(&NsQuicReceiver::HeartBeat,this);
}
void NsQuicReceiver::StopApplication(){
	running_=false;
	if(!heart_timer_.IsExpired()){
		heart_timer_.Cancel();
	}
}
void NsQuicReceiver::RecvPacket(ns3::Ptr<ns3::Socket> socket){
	Address remoteAddr;
	auto packet = socket->RecvFrom (remoteAddr);
	if(first_){
        peer_ip_ = InetSocketAddress::ConvertFrom (remoteAddr).GetIpv4 ();
	    peer_port_= InetSocketAddress::ConvertFrom (remoteAddr).GetPort ();
		first_=false;
	}
	uint32_t recv=packet->GetSize ();
	char *buf=new char[recv];
	packet->CopyData((uint8_t*)buf,recv);
	OnIncomingData(buf,recv);
	delete buf;
}
void NsQuicReceiver::SendToNetwork(ns3::Ptr<ns3::Packet> p){
	socket_->SendTo(p,0,InetSocketAddress{peer_ip_,peer_port_});
}
void NsQuicReceiver::MaybeSendAck()
{
    bool should_send = false;
    if (received_< mMinReceivedBeforeAckDecimation)
    {
        should_send = true;
    }
    else
    {
        if (m_num_packets_received_since_last_ack_sent >= mMaxRetransmittablePacketsBeforeAck)
        {
            should_send = true;
        }
        else if (!ack_alarm_.IsSet())
        {
            uint64_t ack_delay = std::min(mMaxDelayedAckTimeMs, mMinRetransmissionTimeMs / 2);
            QuicTime next=clock_.Now()+QuicTime::Delta::FromMilliseconds(ack_delay);
            ack_alarm_.Update(next);
        }
    }

    if (should_send)
    {
        SendAck();
    }
}
void NsQuicReceiver::SendAck(){
	{
	QuicTime approx=clock_.Now();
	QuicFrame frame=recv_packet_manager_.GetUpdatedAckFrame(approx);
	QuicAckFrame ackframe(*frame.ack_frame);
 	char buffer[kMaxBufferSize];
	my_quic_header_t header;
	header.seq=seq_;
	header.seq_len=GetMinSeqLength(header.seq);
	uint8_t public_flags=0;
	public_flags |= GetPacketNumberFlags(header.seq_len)
                  << kPublicHeaderSequenceNumberShift;

	uint32_t header_len=sizeof(uint8_t)+header.seq_len;
	QuicDataWriter header_writer(header_len,buffer,NETWORK_BYTE_ORDER);
	header_writer.WriteBytes(&public_flags,1);
	header_writer.WriteBytesToUInt64(header.seq_len,seq_);
 	uint32_t packet_length=kMaxBufferSize-header_len;
 	QuicDataWriter writer(packet_length, buffer+header_len, NETWORK_BYTE_ORDER);
	MyQuicFramer framer(versions_,approx,Perspective::IS_CLIENT);
 	framer.AppendAckFrameAndTypeByte(ackframe,&writer);
	uint32_t total_len=writer.length()+header_len;
	Ptr<Packet> p=Create<Packet>((uint8_t*)buffer,total_len);
	SendToNetwork(p);
	ack_sent_=true;
	m_num_packets_received_since_last_ack_sent = 0;
	}
}
void NsQuicReceiver::RecordLoss(uint32_t seq){
	if(!trace_loss_cb_.IsNull()){
		trace_loss_cb_(seq,owd_);
	}
}
void NsQuicReceiver::RecordRate(QuicTime now){
	if(last_rate_ts_==QuicTime::Zero()){
		last_rate_ts_=now+QuicTime::Delta::FromMilliseconds(500);
		return ;
	}
	if(now>last_rate_ts_){
		uint32_t kbps=recv_byte_*8/(500);
		recv_byte_=0;
		if(!trace_rate_cb_.IsNull()){
			trace_rate_cb_(kbps);
		}
		last_rate_ts_=now+QuicTime::Delta::FromMilliseconds(500);
	}

}
void NsQuicReceiver::RecordOwd(uint32_t seq,uint32_t owd){
	if(!trace_owd_cb_.IsNull()){
		trace_owd_cb_(seq,owd);
	}
}
}
