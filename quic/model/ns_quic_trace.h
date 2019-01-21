#ifndef NS_QUIC_NS_QUIC_TRACE_H_
#define NS_QUIC_NS_QUIC_TRACE_H_
#include <stdint.h>
#include <string>
#include <iostream>
#include <fstream>
namespace ns3{
class NsQuicTrace{
public:
	NsQuicTrace(){}
	~NsQuicTrace();
	void OpenSenderRateFile(std::string filename);
	void OpenReceiverRateFile(std::string filename);
	void OpenLossRateFile(std::string filename);
	void OpenOwdFile(std::string filename);
	void OnSenderRate(uint32_t kbps);
	void OnReceiverRate(uint32_t kbps);
	void OnLoss(uint32_t seq,uint32_t owd);
	void OnOwd(uint32_t seq,uint32_t owd);
private:
	void Close();
	void CloseSenderRateFile();
	void CloseReceiverRateFile();
	void CloseLossFile();
	void CloseOwdFile();
	std::fstream m_sender_rate;
	std::fstream m_receiver_rate;
	std::fstream m_loss;
	std::fstream m_owd;
};
}
#endif /* NS_QUIC_NS_QUIC_TRACE_H_ */
