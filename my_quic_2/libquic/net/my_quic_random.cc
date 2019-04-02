#include "my_quic_random.h"
#include <fcntl.h>
#include <unistd.h>
namespace quic{
MyQuicRandom::MyQuicRandom()
:e2_(rd_())
,dist_(std::llround(std::pow(2,61)), std::llround(std::pow(2,62))){

}
void MyQuicRandom::RandBytes(void* data, size_t len){

}
uint64_t GetRandom(){
	uint64_t rnum=0;
	int fd=open("/dev/urandom",O_RDONLY);
	if(fd!=-1){
		(void)read(fd,(void*)&rnum,sizeof(uint64_t));
		(void)close(fd);
	}
	return rnum;
}
uint64_t MyQuicRandom::RandUint64(){
	//uint64_t random=dist_(e2_);
	//return random;
    return GetRandom();
}
}



