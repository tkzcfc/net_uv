#pragma once

#include "ikcp.h"
#include "UDPCommon.h"
#include "UDPSocket.h"

struct UDPBlockBuffer
{
	void* base;
	int len;
};

class KcpSession : public Session
{
public:
	KcpSession() = delete;
	KcpSession(const KcpSession&) = delete;
	virtual ~KcpSession();
		
	inline void setWndSize(int sndwnd, int rcvwnd);

	void setMode(int mode);

	inline void update();
	
	inline void input(const char* data, long size);

	inline void recv(char* buffer, int len);

	inline UDPSocket* getUDPSocket();

	virtual inline unsigned int getPort()override;

	virtual inline const std::string& getIp()override;
	
protected:

	static KcpSession* createSession(SessionManager* sessionManager, UDPSocket* socket, IUINT32 conv);

	KcpSession(SessionManager* sessionManager);

protected:

	static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user);

protected:

	bool init(UDPSocket* socket, IUINT32 conv);

	virtual void executeSend(char* data, unsigned int len)override;

	virtual void executeDisconnect()override;

protected:
	ikcpcb* m_kcp;
	UDPSocket* m_socket;
};

void KcpSession::setWndSize(int sndwnd, int rcvwnd)
{
	ikcp_wndsize(m_kcp, sndwnd, rcvwnd);
}

void KcpSession::update()
{
	ikcp_update(m_kcp, iclock());
}

void KcpSession::input(const char* data, long size)
{
	ikcp_input(m_kcp, data, size);
}

void KcpSession::recv(char* buffer, int len)
{
	ikcp_recv(m_kcp, buffer, len);
}

UDPSocket* KcpSession::getUDPSocket()
{
	return m_socket;
}

unsigned int KcpSession::getPort()
{
	return getUDPSocket()->getPort();
}

inline const std::string& KcpSession::getIp()
{
	return getUDPSocket()->getIp();
}

