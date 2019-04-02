#ifndef MY_QUIC_RANDOM_H_
#define MY_QUIC_RANDOM_H_
#include <random>
#include <cmath>
#include "net/third_party/quic/core/crypto/quic_random.h"
namespace quic{
class MyQuicRandom:public QuicRandom{
public:
	MyQuicRandom();
	void RandBytes(void* data, size_t len) override;
	uint64_t RandUint64() override;
private:
	std::random_device rd_;
	std::mt19937_64 e2_;
	std::uniform_int_distribution<long long int> dist_;
};
}




#endif /* MY_QUIC_RANDOM_H_ */
