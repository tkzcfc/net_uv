#include "UDPSocket.h"

NS_NET_UV_BEGIN

UDPSocket::UDPSocket(uv_loop_t* loop, uv_udp_t* udp)
	: m_socketAddr(NULL)
	, m_udp(NULL)
	, m_readCall(nullptr)
{
	m_loop = loop;
	this->setUdp(udp);
}

UDPSocket::~UDPSocket()
{
	if (m_socketAddr)
	{
		fc_free(m_socketAddr);
		m_socketAddr = NULL;
	}
	if (m_udp)
	{
		net_closeHandle((uv_handle_t*)m_udp, net_closehandle_defaultcallback);
		m_udp = NULL;
	}
}

bool UDPSocket::bind(const char* ip, unsigned int port)
{
	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(false);

	struct sockaddr_in bind_addr;
	int r = uv_ip4_addr(getIp().c_str(), getPort(), &bind_addr);

	if (r != 0)
	{
		return false;
	}

	if (m_udp)
	{
		net_closeHandle((uv_handle_t*)m_udp, net_closehandle_defaultcallback);
		m_udp = NULL;
	}

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);
	m_udp->data = this;
	
	r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, UV_UDP_REUSEADDR);
	CHECK_UV_ASSERT(r);

	r = uv_udp_recv_start(m_udp, net_alloc_buffer, uv_on_after_read);
	CHECK_UV_ASSERT(r);

	if (r == 0)
	{
		struct sockaddr* p_bind_addr = (struct sockaddr*)&bind_addr;
		struct sockaddr* socker_addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr));
		memcpy(socker_addr, p_bind_addr, sizeof(struct sockaddr));
		this->setSocketAddr(socker_addr);
	}

	return (r == 0);
}

bool UDPSocket::bind6(const char* ip, unsigned int port)
{
	this->setIp(ip);
	this->setPort(port);
	this->setIsIPV6(true);

	struct sockaddr_in6 bind_addr;
	int r = uv_ip6_addr(getIp().c_str(), getPort(), &bind_addr);

	if (r != 0)
	{
		return false;
	}

	if (m_udp)
	{
		net_closeHandle((uv_handle_t*)m_udp, net_closehandle_defaultcallback);
		m_udp = NULL;
	}

	m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
	r = uv_udp_init(m_loop, m_udp);
	CHECK_UV_ASSERT(r);
	m_udp->data = this;

	r = uv_udp_bind(m_udp, (const struct sockaddr*) &bind_addr, UV_UDP_REUSEADDR);
	CHECK_UV_ASSERT(r);

	r = uv_udp_recv_start(m_udp, net_alloc_buffer, uv_on_after_read);
	CHECK_UV_ASSERT(r);

	if (r == 0)
	{
		struct sockaddr* p_bind_addr = (struct sockaddr*)&bind_addr;
		struct sockaddr* socker_addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr));
		memcpy(socker_addr, p_bind_addr, sizeof(struct sockaddr));
		this->setSocketAddr(socker_addr);
	}

	return (r == 0);
}

bool UDPSocket::listen()
{
	return true;
}

bool UDPSocket::connect(const char* ip, unsigned int port)
{
	this->setIp(ip);
	this->setPort(port);

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

	int ret = getaddrinfo(ip, NULL, &hints, &ainfo);

	if (ret == 0)
	{
		for (rp = ainfo; rp; rp = rp->ai_next)
		{
			if (rp->ai_family == AF_INET)
			{
				addr4 = (struct sockaddr_in*)rp->ai_addr;
				addr4->sin_port = htons(port);
				break;

			}
			else if (rp->ai_family == AF_INET6)
			{
				addr6 = (struct sockaddr_in6*)rp->ai_addr;
				addr6->sin6_port = htons(port);
				break;
			}
			else
			{
				continue;
			}
		}
		addr = addr4 ? (struct sockaddr*)addr4 : (struct sockaddr*)addr6;
		struct sockaddr* socker_addr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr));
		memcpy(socker_addr, addr, sizeof(struct sockaddr));
		this->setSocketAddr(socker_addr);

		if (m_udp)
		{
			net_closeHandle((uv_handle_t*)m_udp, net_closehandle_defaultcallback);
			m_udp = NULL;
		}
		m_udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
		int r = uv_udp_init(m_loop, m_udp);
		CHECK_UV_ASSERT(r);


		//struct sockaddr_in broadcast_addr;
		//r = uv_ip4_addr("0.0.0.0", 0, &broadcast_addr);
		//CHECK_UV_ASSERT(r);
		//r = uv_udp_bind(m_udp, (const struct sockaddr *)&broadcast_addr, 0);
		//CHECK_UV_ASSERT(r);
		//r = uv_udp_set_broadcast(m_udp, 1);
		//CHECK_UV_ASSERT(r);

		r = uv_udp_recv_start(m_udp, net_alloc_buffer, uv_on_after_read);
		CHECK_UV_ASSERT(r);

		m_udp->data = this;

		return true;
	}
	NET_UV_LOG(NET_UV_L_ERROR, "[%s:%d]地址信息获取失败", ip, port);
	return false;
}

bool UDPSocket::send(char* data, int len)
{
	if (getSocketAddr() == NULL || m_udp == NULL)
	{
		return false;
	}
	uv_buf_t* buf = (uv_buf_t*)fc_malloc(sizeof(uv_buf_t));
	buf->base = (char*)fc_malloc(len);
	buf->len = len;

	memcpy(buf->base, data, len);

	uv_udp_send_t* udp_send = (uv_udp_send_t*)fc_malloc(sizeof(uv_udp_send_t));
	udp_send->data = buf;
	int r = uv_udp_send(udp_send, m_udp, buf, 1, m_socketAddr, uv_on_udp_send);
	
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp send error %s", uv_strerror(r));
	}

	return (r == 0);
}

void UDPSocket::disconnect()
{
	shutdownSocket();
}

void UDPSocket::shutdownSocket()
{
	if (m_udp == NULL)
	{
		return;
	}
	uv_udp_recv_stop(m_udp);
	net_closeHandle((uv_handle_t*)m_udp, uv_on_close_socket);
	m_udp = NULL;
}

void UDPSocket::setSocketAddr(struct sockaddr* addr)
{
	if (m_socketAddr == addr)
		return;
	if (m_socketAddr)
	{
		fc_free(m_socketAddr);
	}
	m_socketAddr = addr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UDPSocket::uv_on_close_socket(uv_handle_t* socket)
{
	UDPSocket* s = (UDPSocket*)(socket->data);
	if (s->m_closeCall != nullptr)
	{
		s->m_closeCall(s);
	}
	fc_free(socket);
}

void UDPSocket::uv_on_udp_send(uv_udp_send_t *req, int status) 
{
	if (status != 0) 
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp send error %s", uv_strerror(status));
	}
	uv_buf_t* buf = (uv_buf_t*)req->data;
	fc_free(buf->base);
	fc_free(buf);
	fc_free(req);
}

void UDPSocket::uv_on_after_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	if (nread < 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "udp read error %s\n", uv_err_name(nread));
		fc_free(buf->base);
		return;
	}
	if (addr == NULL)
	{
		fc_free(buf->base);
		NET_UV_LOG(NET_UV_L_ERROR, "addr is null");
		return;
	}

	//char sender[17] = { 0 };
	//uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
	//UV_LOG(UV_L_INFO, "[%p]Recv from %s\n", addr, sender);

	if (nread > 0)
	{
		UDPSocket* s = (UDPSocket*)handle->data;
		s->m_readCall(handle, nread, buf, addr, flags);
	}

	fc_free(buf->base);
}

NS_NET_UV_END