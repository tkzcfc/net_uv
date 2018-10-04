#pragma once

#include "KCPCommon.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

class NET_UV_EXTERN KCPSession : public Session
{
public:
	KCPSession() = delete;
	KCPSession(const KCPSession&) = delete;
	virtual ~KCPSession();

	/// Session
	virtual inline unsigned int getPort()override;

	virtual inline const std::string& getIp()override;
	
protected:

	static KCPSession* createSession(SessionManager* sessionManager, KCPSocket* socket);

	KCPSession(SessionManager* sessionManager);

protected:
	
	/// Session
	virtual void executeSend(char* data, unsigned int len)override;

	virtual void executeDisconnect()override;

	/// KCPSession
	inline void setKCPSocket(KCPSocket* socket);

	inline KCPSocket* getKCPSocket();

	void init(KCPSocket* socket);

	void on_socket_close(Socket* socket);

	void on_socket_recv(char* data, ssize_t len);

protected:

	friend class UDPClient;
	friend class UDPServer;
protected:
	Buffer* m_recvBuf;
	KCPSocket* m_socket;
};

unsigned int KCPSession::getPort()
{
	return getKCPSocket()->getPort();
}

inline const std::string& KCPSession::getIp()
{
	return getKCPSocket()->getIp();
}

void KCPSession::setKCPSocket(KCPSocket* socket)
{
	m_socket = socket;
}

KCPSocket* KCPSession::getKCPSocket()
{
	return m_socket;
}

NS_NET_UV_END