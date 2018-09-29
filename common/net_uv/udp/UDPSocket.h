#pragma once

#include "UDPCommon.h"
#include <list>

NS_NET_UV_BEGIN

using UDPReadCallback = std::function<void(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)>;

class NET_UV_EXTERN UDPSocket : public Socket
{
public:
	UDPSocket() = delete;

	UDPSocket(const UDPSocket&) = delete;

	UDPSocket(uv_loop_t* loop, uv_udp_t* udp = NULL);

	virtual ~UDPSocket();

	virtual bool bind(const char* ip, unsigned int port)override;

	virtual bool bind6(const char* ip, unsigned int port)override;

	virtual bool listen()override;

	virtual bool connect(const char* ip, unsigned int port)override;

	virtual bool send(char* data, int len)override;

	virtual void disconnect()override;

	inline void setReadCallback(const UDPReadCallback& call);
	
protected:
	inline void setUdp(uv_udp_t* tcp);

	inline uv_udp_t* getUdp();
	
	void setSocketAddr(struct sockaddr* addr);
	
	inline struct sockaddr* getSocketAddr();

protected:
	
	void shutdownSocket();

protected:

	uv_udp_t* m_udp;
	struct sockaddr* m_socketAddr;
	UDPReadCallback m_readCall;

	friend class UDPServer;

protected:
	static void uv_on_close_socket(uv_handle_t* socket);
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

struct sockaddr* UDPSocket::getSocketAddr()
{
	return m_socketAddr;
}

void UDPSocket::setReadCallback(const UDPReadCallback& call)
{
	m_readCall = std::move(call);
}


NS_NET_UV_END

