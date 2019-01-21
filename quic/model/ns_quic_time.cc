#include "ns_quic_time.h"
#include "ns3/simulator.h"
namespace ns3{
net::QuicTime Ns3QuicTime::Now(){
uint32_t ms=Simulator::Now().GetMilliSeconds();
net::QuicTime time=net::QuicTime::Zero() + net::QuicTime::Delta::FromMilliseconds(ms);
return time;
}
uint32_t Ns3QuicTime::GetMilliSeconds(){
net::QuicTime now=Ns3QuicTime::Now();
net::QuicTime::Delta delta=now-net::QuicTime::Zero();
uint32_t ms=delta.ToMilliseconds();
return ms;
}
net::QuicTime NsQuicClock::Now() const{
    int64_t  ms=ns3::Simulator::Now().GetMilliSeconds();
    net::QuicTime now=net::QuicTime::Zero() + net::QuicTime::Delta::FromMilliseconds(ms);
    return now;    
}
net::QuicWallTime NsQuicClock::WallNow() const{
    int64_t time_since_unix_epoch_micro=ns3::Simulator::Now().GetMilliSeconds()*1000;
    return net::QuicWallTime::FromUNIXMicroseconds(time_since_unix_epoch_micro);
}
}



