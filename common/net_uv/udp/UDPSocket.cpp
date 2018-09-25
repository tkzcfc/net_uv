#include "UDPSocket.h"

NS_NET_UV_BEGIN

UDPSocket::UDPSocket(uv_loop_t* loop)
	: m_connectSockaddr(NULL)
	, m_isClose(true)
	, m_call(NULL)
	, m_userdata(NULL)
	, m_udp(NULL)
{
	m_loop = loop;
	init();
}

UDPSocket::~UDPSocket()
{
	if (m_connectSockaddr)
	{
		fc_free(m_connectSockaddr);
	}
}

void UDPSocket::init()
{
	Socket::init();

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	int r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);

	r = uv_udp_recv_start(m_udp, uv_alloc_buffer, uv_on_after_read);
	CHECK_UV_ASSERT(r);

	m_udp->data = this;
}

void UDPSocket::listen(const char* ip, unsigned int port, bool isIPV6)
{
	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(isIPV6);
	int r = uv_async_send(m_listenAsync);

	CHECK_UV_ASSERT(r);

	UV_LOG(UV_L_INFO, "listen [%p][%s]:[%d]...", this, ip, port);
}

void UDPSocket::connect(const char* ip, unsigned int port)
{
	m_connectIP = ip;
	m_connectPort = port;

	int r = uv_async_send(m_connectAsync);
	CHECK_UV_ASSERT(r);
}

void UDPSocket::send(void* data, int dataLen)
{
	UDPBlockBuffer buf;
	buf.base = fc_malloc(dataLen);
	buf.len = dataLen;
	memcpy(buf.base, data, dataLen);

	uv_mutex_lock(&m_writeMutex);
	m_writeCache.emplace_back(buf);
	uv_mutex_unlock(&m_writeMutex);

	uv_async_send(m_writeAsync);
}

void UDPSocket::close()
{
	uv_async_send(m_closeAsync);
}

void UDPSocket::udp_send(void* data, int len, struct sockaddr* addr)
{
	uv_buf_t* buf = (uv_buf_t*)fc_malloc(sizeof(uv_buf_t));
	buf->base = (char*)data;
	buf->len = len;

	uv_udp_send_t* udp_send = (uv_udp_send_t*)fc_malloc(sizeof(uv_udp_send_t));
	udp_send->data = buf;
	int r = uv_udp_send(udp_send, m_udp, buf, 1, addr, uv_on_udp_send);
	if (r)
	{
		UV_LOG(UV_L_ERROR, "%s", getUVError(r).c_str());
	}
}

void UDPSocket::setSocketCall(udp_socket_call call, void* userdata)
{
	m_call = call;
	m_userdata = userdata;
}

void UDPSocket::onAsyncListen()
{
	if (!m_isClose)
		return;
	int r = 0;
	if (isIPV6())
	{
		struct sockaddr_in6 bind_addr;
		r = uv_ip6_addr(getIp().c_str(), getPort(), &bind_addr);
		CHECK_UV_ASSERT(r);
		r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, 0);
		CHECK_UV_ASSERT(r);
	}
	else
	{
		struct sockaddr_in bind_addr;
		r = uv_ip4_addr(getIp().c_str(), getPort(), &bind_addr);
		CHECK_UV_ASSERT(r);
		r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, 0);
		CHECK_UV_ASSERT(r);
	}
	adjustBuffSize((uv_handle_t*)m_udp, UDP_UV_SOCKET_RECV_BUF_LEN, UDP_UV_SOCKET_SEND_BUF_LEN);
	m_isClose = false;
}

void UDPSocket::onAsyncConnet()
{
	if (!m_isClose)
		return;
	struct addrinfo hints;
	struct addrinfo* ainfo;
	struct addrinfo* rp;
	struct sockaddr_in* addr4 = NULL;
	struct sockaddr_in6* addr6 = NULL;
	struct sockaddr* addr = NULL;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_ADDRCONFIG;
	hints.ai_socktype = SOCK_STREAM;

	int ret = getaddrinfo(m_connectIP.c_str(), NULL, &hints, &ainfo);

	if (ret == 0)
	{
		for (rp = ainfo; rp; rp = rp->ai_next)
		{
			if (rp->ai_family == AF_INET)
			{
				addr4 = (struct sockaddr_in*)rp->ai_addr;
				addr4->sin_port = htons(m_connectPort);
				break;

			}
			else if (rp->ai_family == AF_INET6)
			{
				addr6 = (struct sockaddr_in6*)rp->ai_addr;
				addr6->sin6_port = htons(m_connectPort);
				break;
			}
			else
			{
				continue;
			}
		}
		addr = addr4 ? (struct sockaddr*)addr4 : (struct sockaddr*)addr6;
		m_connectSockaddr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr));
		memcpy(m_connectSockaddr, addr, sizeof(struct sockaddr));

		m_isClose = false;
		if (m_call)
		{
			m_call(udp_socket_call_type::resolve_suc, this, m_userdata);
		}
	}
	else
	{
		UV_LOG(UV_L_ERROR, "get addr info fail!!!");
		if (m_call)
		{
			m_call(udp_socket_call_type::resolve_fail, this, m_userdata);
		}
	}	
}

void UDPSocket::onAsyncWrite()
{
	if (m_connectSockaddr == NULL)
	{
		uv_async_send(m_writeAsync);
		return;
	}

	if (uv_mutex_trylock(&m_writeMutex) != 0)
	{
		uv_async_send(m_writeAsync);
		return;
	}

	auto size = m_writeCache.size();
	if (size <= 0)
	{
		uv_mutex_unlock(&m_writeMutex);
		return;
	}

	auto it = m_writeCache.begin();
	void* data = it->base;
	int len = it->len;
	m_writeCache.erase(it);

	uv_mutex_unlock(&m_writeMutex);

	this->udp_send(data, len, m_connectSockaddr);
	UV_LOG(UV_L_INFO, "send...");

	if (size > 1)
	{
		uv_async_send(m_writeAsync);
	}
}

void UDPSocket::onAsyncClose()
{
	uv_udp_recv_stop(m_udp);
	if (m_call)
	{
		m_call(udp_socket_call_type::recv_stop, this, m_userdata);
	}
}

void UDPSocket::server_on_after_new_connection(uv_stream_t *server, int status)
{
}

void UDPSocket::uv_on_udp_send(uv_udp_send_t *req, int status) 
{
	if (status) 
	{
		UV_LOG(UV_L_ERROR, "udp send error %s", uv_strerror(status));
	}
	uv_buf_t* buf = (uv_buf_t*)req->data;
	if (buf)
	{
		fc_free(buf->base);
		fc_free(buf);
	}
	fc_free(req);
}

void UDPSocket::uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	UDPSocket* s = (UDPSocket*)handle->data;
	if (nread < 0) 
	{
		UV_LOG(UV_L_ERROR, "Read error %s\n", uv_err_name(nread));
		fc_free(buf->base);
		return;
	}
	if (addr == NULL)
	{
		fc_free(buf->base);
		UV_LOG(UV_L_ERROR, "addr is null");
		return;
	}

	//char sender[17] = { 0 };
	//uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
	//UV_LOG(UV_L_INFO, "[%p]Recv from %s\n", addr, sender);
	
	fc_free(buf->base);
}

NS_NET_UV_END