#pragma once

#include "TCPSocket.h"
#include "ThreadMsg.h"

NS_NET_UV_BEGIN

class TCPSession;

#if OPEN_UV_THREAD_HEARTBEAT == 1
using TCPSessionRecvCall = std::function<void(TCPSession* session, char* data, unsigned int len, TCPMsgTag tag)>;
#else
using TCPSessionRecvCall = std::function<void(TCPSession* session, char* data, unsigned int len)>;
#endif
class NET_UV_EXTERN TCPSession : public Session
{
public:
	TCPSession() = delete;
	TCPSession(const TCPSession&) = delete;
	virtual ~TCPSession();

	virtual inline unsigned int getPort()override;

	virtual inline const std::string& getIp()override;
	
protected:

	static TCPSession* createSession(SessionManager* sessionManager, TCPSocket* socket, const TCPSessionRecvCall& call);

	TCPSession(SessionManager* sessionManager);

protected:

	virtual void executeSend(char* data, unsigned int len) override;

	virtual void executeDisconnect() override;

	virtual void setIsOnline(bool isOnline)override;

protected:

	bool initWithSocket(TCPSocket* socket);
	
	inline void setMsgCallback(const TCPSessionRecvCall& call);

	inline TCPSocket* getTCPSocket();

protected:

	void on_socket_recv(char* data, ssize_t len);

	void on_socket_close(Socket* socket);

	friend class TCPServer;
	friend class TCPClient;

protected:	
	TCPSessionRecvCall m_recvCallback;

	Buffer* m_recvBuffer;
	TCPSocket* m_socket;
};


void TCPSession::setMsgCallback(const TCPSessionRecvCall& call)
{
	m_recvCallback = std::move(call);
}

TCPSocket* TCPSession::getTCPSocket()
{
	return m_socket;
}

unsigned int TCPSession::getPort()
{
	return getTCPSocket()->getPort();
}

const std::string& TCPSession::getIp()
{
	return getTCPSocket()->getIp();
}


NS_NET_UV_END
