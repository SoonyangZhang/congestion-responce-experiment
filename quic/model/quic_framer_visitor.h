#ifndef QUIC_FRAMER_VISITOR_H_
#define QUIC_FRAMER_VISITOR_H_
#include "my_quic_framer.h"
namespace net{
class AbstractQuicFramerVisitor:public MyQuicFramerVisitorInterface{
public:
	virtual ~AbstractQuicFramerVisitor(){}
	virtual void OnError(MyQuicFramer* framer) override{}
	virtual bool OnProtocolVersionMismatch(
	      ParsedQuicVersion received_version) override{ return true;}
	virtual void OnPacket() override{}
	virtual void OnPublicResetPacket(const QuicPublicResetPacket& packet) override{}
	virtual void OnVersionNegotiationPacket(
	      const QuicVersionNegotiationPacket& packet) override{}
	virtual bool OnUnauthenticatedPublicHeader(
	      const QuicPacketHeader& header) override{return true;}
	virtual bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override{return true;}
	virtual void OnDecryptedPacket(EncryptionLevel level) override{}
	virtual bool OnPacketHeader(const QuicPacketHeader& header) override{return true;}
	virtual bool OnStreamFrame(const QuicStreamFrame& frame) override{return true;}
	virtual bool OnAckFrame(const QuicAckFrame& frame) override{return true;}
	virtual bool OnAckFrameStart(QuicPacketNumber largest_acked,
	                               QuicTime::Delta ack_delay_time) override{return true;}
	virtual bool OnAckRange(QuicPacketNumber start,
	                          QuicPacketNumber end,
	                          bool last_range) override{return true;}
	  virtual bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override{return true;}
	  virtual bool OnPaddingFrame(const QuicPaddingFrame& frame) override{return true;}
	  virtual bool OnPingFrame(const QuicPingFrame& frame) override{return true;}
	  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override{return true;}
	  virtual bool OnConnectionCloseFrame(
	      const QuicConnectionCloseFrame& frame) override{return true;}
	  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override{return true;}
	  virtual bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override{return true;}
	  virtual bool OnBlockedFrame(const QuicBlockedFrame& frame) override{return true;}
	  virtual void OnPacketComplete() override {}
	  virtual bool IsValidStatelessResetToken(uint128 token) const override{ return true;}

	  virtual void OnAuthenticatedIetfStatelessResetPacket(
	      const QuicIetfStatelessResetPacket& packet) override{}
};
class AckFrameVisitor:public AbstractQuicFramerVisitor{
public:
	AckFrameVisitor(){}
	~AckFrameVisitor(){}
	bool OnAckFrame(const QuicAckFrame& frame) override;
	bool OnAckFrameStart(QuicPacketNumber largest_acked,
		                 QuicTime::Delta ack_delay_time) override;
	bool OnAckRange(QuicPacketNumber start,
		            QuicPacketNumber end,
		            bool last_range) override;
private:
	QuicAckFrame ack_frame_;
};
}
#endif /* QUIC_FRAMER_VISITOR_H_ */
