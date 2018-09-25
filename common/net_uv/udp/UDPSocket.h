#pragma once

#include "UDPCommon.h"
#include <list>

NS_NET_UV_BEGIN

struct UDPBlockBuffer
{
	void* base;
	int len;
};

enum udp_socket_call_type
{
	resolve_suc,	// 连接成功(解析地址成功)
	resolve_fail,	// 连接失败(解析地址失败)
	recv_stop,		// socket关闭回调
};

class TCPSocket;
typedef void(*udp_socket_call)(udp_socket_call_type, UDPSocket*, void* userdata);

class UDPSocket : public Socket
{
public:
	UDPSocket() = delete;

	UDPSocket(const UDPSocket&) = delete;

	UDPSocket(uv_loop_t* loop);

	virtual ~UDPSocket();

	virtual void listen(const char* ip, unsigned int port, bool isIPV6)override;

	inline uv_udp_t* getUdp();

	void connect(const char* ip, unsigned int port);

	void send(void* data, int dataLen);

	void close();

	inline bool isClosed();

	void setSocketCall(udp_socket_call call, void* userdata);

protected:
	inline void setUdp(uv_udp_t* tcp);

	void udp_send(void* data, int len, struct sockaddr* addr);

protected:
	virtual void init()override;

	virtual void onAsyncListen()override;

	virtual void onAsyncConnet()override;

	virtual void onAsyncWrite()override;

	virtual void onAsyncClose()override;

protected:
	uv_udp_t* m_udp;
	
	std::list<UDPBlockBuffer> m_writeCache;
	std::list<UDPBlockBuffer> m_readCache;

	struct sockaddr* m_connectSockaddr;
	std::string m_connectIP;
	unsigned int m_connectPort;

	bool m_isClose;

	udp_socket_call m_call;
	void* m_userdata;
protected:

	static void server_on_after_new_connection(uv_stream_t *server, int status);
	static void uv_on_udp_send(uv_udp_send_t *req, int status);
	static void uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);

};

void UDPSocket::setUdp(uv_udp_t* udp)
{
	m_udp = udp;
}

uv_udp_t* UDPSocket::getUdp()
{
	return m_udp;
}

bool UDPSocket::isClosed()
{
	return m_isClose;
}


NS_NET_UV_END

