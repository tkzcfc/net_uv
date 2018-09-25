#pragma once

#include "ikcp.h"
#include "UDPCommon.h"
#include "UDPSocket.h"

struct UDPBlockBuffer
{
	void* base;
	int len;
};

class KcpSession
{
public:
	KcpSession();
	KcpSession(const KcpSession&) = delete;
	virtual ~KcpSession();

	void send(const char* buffer, int len);
	
	void init(IUINT32 conv, UDPSocket* socket);
	
	inline void setWndSize(int sndwnd, int rcvwnd);

	void setMode(int mode);

	inline void update();
	
	inline void input(const char* data, long size);

	inline void recv(char* buffer, int len);

	inline UDPSocket* getSocket();

	inline void setSocket(UDPSocket* socket);

protected:

	static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user);

protected:

	uv_mutex_t m_writeMutex;
	std::list<UDPBlockBuffer> m_writeCache;
	std::list<UDPBlockBuffer> m_readCache;

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

UDPSocket* KcpSession::getSocket()
{
	return m_socket;
}

void KcpSession::setSocket(UDPSocket* socket)
{
	m_socket = socket;
}
