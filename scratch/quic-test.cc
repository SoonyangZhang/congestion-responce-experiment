#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/quic-module.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include <stdarg.h>
#include <string>
#include <memory>
using namespace ns3;
using namespace std;
using namespace net;
NS_LOG_COMPONENT_DEFINE ("Quic-Test");
const uint32_t DEFAULT_PACKET_SIZE = 1000;
const static uint32_t rateArray[]=
{
2000000,
1500000,
1000000,
 500000,
1000000,
1500000,
};
class ChangeBw
{
public:
	ChangeBw(Ptr<NetDevice> netdevice)
	{
	m_total=sizeof(rateArray)/sizeof(rateArray[0]);
	m_netdevice=netdevice;
	}
	//ChangeBw(){}
	~ChangeBw(){}
	void Start()
	{
		Time next=Seconds(m_gap);
		m_timer=Simulator::Schedule(next,&ChangeBw::ChangeRate,this);		
	}
	void ChangeRate()
	{
		if(m_timer.IsExpired())
		{
		NS_LOG_INFO(Simulator::Now().GetSeconds()<<" "<<rateArray[m_index]/1000);
		//Config::Set ("/ChannelList/0/$ns3::PointToPointChannel/DataRate",DataRateValue (rateArray[m_index]));
		PointToPointNetDevice *device=static_cast<PointToPointNetDevice*>(PeekPointer(m_netdevice));
		device->SetDataRate(DataRate(rateArray[m_index]));
		m_index=(m_index+1)%m_total;
		Time next=Seconds(m_gap);
		m_timer=Simulator::Schedule(next,&ChangeBw::ChangeRate,this);
		}

	}
private:
	uint32_t m_index{1};
	uint32_t m_gap{20};
	uint32_t m_total{6};
	Ptr<NetDevice>m_netdevice;
	EventId m_timer;
};
static int ip=1;
static NodeContainer BuildExampleTopo (uint64_t bps,
                                       uint32_t msDelay,
                                       uint32_t msQdelay)
{
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    pointToPoint.SetQueue ("ns3::DropTailQueue",
                           "Mode", StringValue ("QUEUE_MODE_BYTES"),
                           "MaxBytes", UintegerValue (bufSize));
    NetDeviceContainer devices = pointToPoint.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);
    Ipv4AddressHelper address;
    std::string nodeip="10.1."+std::to_string(ip)+".0";
    ip++;
    address.SetBase (nodeip.c_str(), "255.255.255.0");
    address.Assign (devices);

    // Uncomment to capture simulated traffic
    // pointToPoint.EnablePcapAll ("rmcat-example");

    // disable tc for now, some bug in ns3 causes extra delay
    TrafficControlHelper tch;
    tch.Uninstall (devices);
/*
	std::string errorModelType = "ns3::RateErrorModel";
  	ObjectFactory factory;
  	factory.SetTypeId (errorModelType);
  	Ptr<ErrorModel> em = factory.Create<ErrorModel> ();
	devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));*/
    return nodes;
}

static void InstallTcp(
                         Ptr<Node> sender,
                         Ptr<Node> receiver,
                         uint16_t port,
                         float startTime,
                         float stopTime
)
{
    // configure TCP source/sender/client
    auto serverAddr = receiver->GetObject<Ipv4> ()->GetAddress (1,0).GetLocal ();
    BulkSendHelper source{"ns3::TcpSocketFactory",
                           InetSocketAddress{serverAddr, port}};
    // Set the amount of data to send in bytes. Zero is unlimited.
    source.SetAttribute ("MaxBytes", UintegerValue (0));
    source.SetAttribute ("SendSize", UintegerValue (DEFAULT_PACKET_SIZE));

    auto clientApps = source.Install (sender);
    clientApps.Start (Seconds (startTime));
    clientApps.Stop (Seconds (stopTime));

    // configure TCP sink/receiver/server
    PacketSinkHelper sink{"ns3::TcpSocketFactory",
                           InetSocketAddress{Ipv4Address::GetAny (), port}};
    auto serverApps = sink.Install (receiver);
    serverApps.Start (Seconds (startTime));
    serverApps.Stop (Seconds (stopTime));	
	
}
static double simDuration=200;
uint16_t client_port=1234;
uint16_t servPort=4321;
float appStart=0.0;
float appStop=simDuration-1;

int main(int argc, char *argv[])
{
	Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (0.1));
	Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));

	Config::SetDefault ("ns3::BurstErrorModel::ErrorRate", DoubleValue (0.05));
	Config::SetDefault ("ns3::BurstErrorModel::BurstSize", StringValue ("ns3::UniformRandomVariable[Min=1|Max=3]"));
    LogComponentEnable("NsQuicSender",LOG_LEVEL_ALL);
    LogComponentEnable("NsQuicReceiver",LOG_LEVEL_ALL);
	uint64_t linkBw   = 2000000;//4000000;
    uint32_t msDelay  = 100;//50;//100;
    uint32_t msQDelay = 300;

    NodeContainer nodes = BuildExampleTopo (linkBw, msDelay, msQDelay);

    Ptr<NsQuicSender> spath1_1=CreateObject<NsQuicSender>();
	Ptr<NsQuicReceiver> rpath1_1=CreateObject<NsQuicReceiver>();
    nodes.Get(0)->AddApplication (spath1_1);
    nodes.Get(1)->AddApplication (rpath1_1);
    spath1_1->Bind(client_port);
    rpath1_1->Bind(servPort);
    spath1_1->SetStartTime (Seconds (appStart));
    spath1_1->SetStopTime (Seconds (appStop));
    rpath1_1->SetStartTime (Seconds (appStart));
    rpath1_1->SetStopTime (Seconds (appStop));
    InetSocketAddress remote=rpath1_1->GetLocalAddress();
    spath1_1->ConfigurePeer(remote.GetIpv4(),remote.GetPort());

    std::string log_name_1=std::string("quic-1");
    NsQuicTrace trace1;
    trace1.OpenSenderRateFile(log_name_1);
    trace1.OpenLossFile(log_name_1);
    trace1.OpenReceiverRateFile(log_name_1);
    trace1.OpenOwdFile(log_name_1);
    spath1_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnSenderRate,&trace1));
    rpath1_1->SetLossTraceFunc(MakeCallback(&NsQuicTrace::OnLoss,&trace1));
    rpath1_1->SetOwdTraceFunc(MakeCallback(&NsQuicTrace::OnOwd,&trace1));
    rpath1_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnReceiverRate,&trace1));


    Ptr<NsQuicSender> spath2_1=CreateObject<NsQuicSender>();
	Ptr<NsQuicReceiver> rpath2_1=CreateObject<NsQuicReceiver>();
    nodes.Get(0)->AddApplication (spath2_1);
    nodes.Get(1)->AddApplication (rpath2_1);
    spath2_1->Bind(client_port+1);
    rpath2_1->Bind(servPort+1);
    spath2_1->SetStartTime (Seconds (appStart+40));
    spath2_1->SetStopTime (Seconds (appStop));
    rpath2_1->SetStartTime (Seconds (appStart+40));
    rpath2_1->SetStopTime (Seconds (appStop));
    remote=rpath2_1->GetLocalAddress();
    spath2_1->ConfigurePeer(remote.GetIpv4(),remote.GetPort());
    
    std::string log_name_2=std::string("quic-2");
    NsQuicTrace trace2;
    trace2.OpenSenderRateFile(log_name_2);
    trace2.OpenLossFile(log_name_2);
    trace2.OpenReceiverRateFile(log_name_2);
    trace2.OpenOwdFile(log_name_2);
    spath2_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnSenderRate,&trace2));
    rpath2_1->SetLossTraceFunc(MakeCallback(&NsQuicTrace::OnLoss,&trace2));
    rpath2_1->SetOwdTraceFunc(MakeCallback(&NsQuicTrace::OnOwd,&trace2));
    rpath2_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnReceiverRate,&trace2));
    
    /*
    Ptr<NsQuicSender> spath3_1=CreateObject<NsQuicSender>();
	Ptr<NsQuicReceiver> rpath3_1=CreateObject<NsQuicReceiver>();
    nodes.Get(0)->AddApplication (spath3_1);
    nodes.Get(1)->AddApplication (rpath3_1);
    spath3_1->Bind(client_port+2);
    rpath3_1->Bind(servPort+2);
    spath3_1->SetStartTime (Seconds (appStart+80));
    spath3_1->SetStopTime (Seconds (appStop));
    rpath3_1->SetStartTime (Seconds (appStart+80));
    rpath3_1->SetStopTime (Seconds (appStop));
    remote=rpath3_1->GetLocalAddress();
    spath3_1->ConfigurePeer(remote.GetIpv4(),remote.GetPort());
    
    std::string log_name_3=std::string("quic-3");
    NsQuicTrace trace3;
    trace3.OpenSenderRateFile(log_name_3);
    trace3.OpenLossFile(log_name_3;
    trace3.OpenReceiverRateFile(log_name_3);
    trace3.OpenOwdFile(log_name_3);
    spath3_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnSenderRate,&trace3));
    rpath3_1->SetLossTraceFunc(MakeCallback(&NsQuicTrace::OnLoss,&trace3));
    rpath3_1->SetOwdTraceFunc(MakeCallback(&NsQuicTrace::OnOwd,&trace3));
    rpath3_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnReceiverRate,&trace3));
    
    
    Ptr<NsQuicSender> spath4_1=CreateObject<NsQuicSender>();
	Ptr<NsQuicReceiver> rpath4_1=CreateObject<NsQuicReceiver>();
    nodes.Get(0)->AddApplication (spath4_1);
    nodes.Get(1)->AddApplication (rpath4_1);
    spath4_1->Bind(client_port+3);
    rpath4_1->Bind(servPort+3);
    spath4_1->SetStartTime (Seconds (appStart+150));
    spath4_1->SetStopTime (Seconds (appStop));
    rpath4_1->SetStartTime (Seconds (appStart+150));
    rpath4_1->SetStopTime (Seconds (appStop));
    remote=rpath4_1->GetLocalAddress();
    spath4_1->ConfigurePeer(remote.GetIpv4(),remote.GetPort());
    
    std::string log_name_4=std::string("quic-4");
    NsQuicTrace trace4;
    trace4.OpenSenderRateFile(log_name_4);
    trace4.OpenLossFile(log_name_4);
    trace4.OpenReceiverRateFile(log_name_4);
    trace4.OpenOwdFile(log_name_4);
    spath4_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnSenderRate,&trace4));
    rpath4_1->SetLossTraceFunc(MakeCallback(&NsQuicTrace::OnLoss,&trace4));
    rpath4_1->SetOwdTraceFunc(MakeCallback(&NsQuicTrace::OnOwd,&trace4));
    rpath4_1->SetRateTraceFunc(MakeCallback(&NsQuicTrace::OnReceiverRate,&trace4));*/

    //InstallTcp(nodes.Get(0),nodes.Get(1),4444,20,300);
    //Ptr<NetDevice> netDevice=nodes.Get(0)->GetDevice(0);
	//ChangeBw change(netDevice);
    //change.Start();
    Simulator::Stop (Seconds(simDuration));
    Simulator::Run ();
    Simulator::Destroy();

}
