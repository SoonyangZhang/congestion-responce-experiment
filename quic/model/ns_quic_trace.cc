#include "ns_quic_trace.h"
#include <unistd.h>
#include "ns3/simulator.h"
namespace ns3{
NsQuicTrace::~NsQuicTrace(){
	Close();
}
void NsQuicTrace::OpenSenderRateFile(std::string filename){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+ filename+"-sender-rate.txt";
	m_sender_rate.open(path.c_str(), std::fstream::out);
}
void NsQuicTrace::OpenReceiverRateFile(std::string filename){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+ filename+"-receiver-rate.txt";
	m_receiver_rate.open(path.c_str(), std::fstream::out);
}
void NsQuicTrace::OpenLossRateFile(std::string filename){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+ filename+"-loss.txt";
	m_loss.open(path.c_str(), std::fstream::out);
}
void NsQuicTrace::OpenOwdFile(std::string filename){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+ filename+"-owd.txt";
	m_owd.open(path.c_str(), std::fstream::out);
}
void NsQuicTrace::OnSenderRate(uint32_t kbps){
	if(m_sender_rate.is_open()){
		char line [256];
		memset(line,0,256);
		float now=Simulator::Now().GetSeconds();
		float rate=(float)kbps;
		sprintf(line, "%f %16f",now,rate);
		m_sender_rate<<line<<std::endl;
	}
}
void NsQuicTrace::OnReceiverRate(uint32_t kbps){
	if(m_receiver_rate.is_open()){
		char line [256];
		memset(line,0,256);
		float now=Simulator::Now().GetSeconds();
		float rate=(float)kbps;
		sprintf(line, "%f %16f",now,rate);
		m_receiver_rate<<line<<std::endl;
	}
}
void NsQuicTrace::OnLoss(uint32_t seq,uint32_t owd){
	if(m_loss.is_open()){
		char line [256];
		memset(line,0,256);
		float now=Simulator::Now().GetSeconds();
		sprintf(line, "%f %16d %16d",now,owd,seq);
		m_loss<<line<<std::endl;
	}
}
void NsQuicTrace::OnOwd(uint32_t seq,uint32_t owd){
	if(m_owd.is_open()){
		char line [256];
		memset(line,0,256);
		float now=Simulator::Now().GetSeconds();
		sprintf(line, "%d %16d",seq,owd);
		m_owd<<line<<std::endl;
	}
}
void NsQuicTrace::Close(){
	CloseSenderRateFile();
	CloseReceiverRateFile();
	CloseLossFile();
	CloseOwdFile();
}
void NsQuicTrace::CloseSenderRateFile(){
	if(m_sender_rate.is_open()){
		m_sender_rate.close();
	}
}
void NsQuicTrace::CloseReceiverRateFile(){
	if(m_receiver_rate.is_open()){
		m_receiver_rate.close();
	}
}
void NsQuicTrace::CloseLossFile(){
	if(m_loss.is_open()){
		m_loss.close();
	}
}
void NsQuicTrace::CloseOwdFile(){
	if(m_owd.is_open()){
		m_owd.close();
	}
}
}




