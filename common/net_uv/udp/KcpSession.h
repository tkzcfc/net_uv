#pragma once

#include "ikcp.h"
#include "UDPCommon.h"
#include "UDPSocket.h"

NS_NET_UV_BEGIN

struct UDPBlockBuffer
{
	void* base;
	int len;
};

class KcpSession;
using KcpSessionConnectCallback = std::function<void(KcpSession* session, int status)>;

class NET_UV_EXTERN KcpSession : public Session
{
public:
	KcpSession() = delete;
	KcpSession(const KcpSession&) = delete;
	virtual ~KcpSession();

	/// Session
	virtual inline unsigned int getPort()override;

	virtual inline const std::string& getIp()override;
	
protected:

	static KcpSession* createSession(SessionManager* sessionManager, uv_udp_t* udp = NULL);

	KcpSession(SessionManager* sessionManager);

protected:
	/// kcp
	inline void setWndSize(int sndwnd, int rcvwnd);

	void setMode(int mode);

	void update();

	inline void input(const char* data, long size);

	inline void recv(char* buffer, int len);

	/// Session
	virtual void executeSend(char* data, unsigned int len)override;

	virtual void executeDisconnect()override;

	/// KcpSession
	bool executeConnect(const char* ip, unsigned int port);

	bool udp_send(char* data, unsigned int len);

	inline void setConnectCallback(const KcpSessionConnectCallback& call);

protected:

	static int udp_output(const char *buf, int len, ikcpcb *kcp, void *user);

	static void uv_on_idle_run(uv_idle_t* handle);

	friend class UDPClient;
	friend class UDPServer;
protected:
	ikcpcb* m_kcp;
	uv_idle_t m_idle;

	char* m_recvBuf;

	IUINT32 m_connectStartTime;
	IUINT32 m_sendStartTime;

	std::string m_ip;
	unsigned int m_port;
	bool m_weakRefUdp;

	KcpSessionConnectCallback m_connectCall;
};

void KcpSession::setWndSize(int sndwnd, int rcvwnd)
{
	ikcp_wndsize(m_kcp, sndwnd, rcvwnd);
}

void KcpSession::input(const char* data, long size)
{
	ikcp_input(m_kcp, data, size);
}

void KcpSession::recv(char* buffer, int len)
{
	ikcp_recv(m_kcp, buffer, len);
}

unsigned int KcpSession::getPort()
{
	return m_port;
}

inline const std::string& KcpSession::getIp()
{
	return m_ip;
}
void KcpSession::setConnectCallback(const KcpSessionConnectCallback& call)
{
	m_connectCall = std::move(call);
}

NS_NET_UV_END