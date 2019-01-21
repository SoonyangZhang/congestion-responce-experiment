#ifndef NS3_MPVIDEO_NS_QUIC_TIME_H_
#define NS3_MPVIDEO_NS_QUIC_TIME_H_
#include "net/quic/core/quic_time.h"
#include "net/quic/platform/api/quic_clock.h"
#include <stdint.h>
namespace ns3{
class Ns3QuicTime{
public:
static net::QuicTime Now();
static uint32_t GetMilliSeconds();
};
class NsQuicClock:public net::QuicClock{
public:
    NsQuicClock(){}
    ~NsQuicClock(){}
    net::QuicTime Now() const override;
    net::QuicWallTime WallNow() const override;
    net::QuicTime ApproximateNow() const override{
    return Now();}
};
}
#endif /* NS3_MPVIDEO_NS_QUIC_TIME_H_ */
