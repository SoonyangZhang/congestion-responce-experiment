#include "net/my_quic_random.h"
#include <iostream>
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/core/quic_data_reader.h"
#include<string>
using namespace std;
#define MAX_BUF 1000
int main(){
/*quic::MyQuicRandom rand;
    for (int n = 0; n < 20; ++n) {
            std::cout << rand.RandUint64()%10<<std::endl;
    }*/
char buf[MAX_BUF];
uint16_t b=1234;
quic::QuicDataWriter  w(MAX_BUF,buf,quic::NETWORK_BYTE_ORDER);
w.WriteUInt16(b);
quic::QuicDataReader r(buf,MAX_BUF,quic::NETWORK_BYTE_ORDER);
    uint16_t b1;
    r.ReadUInt16(&b1);
    std::cout<<std::to_string(b1)<<std::endl;

}
