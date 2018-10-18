#include "uv_func.h"

NS_NET_UV_BEGIN


void net_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf)
{
	buf->base = (char*)fc_malloc(size);
	buf->len = size;
}

void net_closehandle_defaultcallback(uv_handle_t* handle)
{
	fc_free(handle);
}

void net_closeHandle(uv_handle_t* handle, uv_close_cb closecb)
{
	if (handle && !uv_is_closing(handle))
	{
		uv_close(handle, closecb);
	}
}

// 调整socket缓冲区大小
void net_adjustBuffSize(uv_handle_t* handle, int minRecvBufSize, int minSendBufSize)
{
	int len = 0;
	int r = uv_recv_buffer_size(handle, &len);
	CHECK_UV_ASSERT(r);

	if (len < minRecvBufSize)
	{
		len = minRecvBufSize;
		r = uv_recv_buffer_size(handle, &len);
		CHECK_UV_ASSERT(r);
	}

	len = 0;
	r = uv_send_buffer_size(handle, &len);
	CHECK_UV_ASSERT(r);

	if (len < minSendBufSize)
	{
		len = minSendBufSize;
		r = uv_send_buffer_size(handle, &len);
		CHECK_UV_ASSERT(r);
	}
}

// hash
unsigned int net_getBufHash(const void *buf, unsigned int len)
{
	unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
	unsigned int hash = 0;
	unsigned int i = 0;
	char *str = (char *)buf;
	while (i < len)
	{
		hash = hash * seed + (*str++);
		++i;
	}

	return (hash & 0x7FFFFFFF);
}

unsigned int net_getsockAddrIPAndPort(const struct sockaddr* addr, std::string& outIP, unsigned int& outPort)
{
	if (addr == NULL)
		return 0;

	std::string strip;
	unsigned int addrlen = 0;
	unsigned int port = 0;

	if (addr->sa_family == AF_INET6)
	{
		addrlen = sizeof(struct sockaddr_in6);

		const struct sockaddr_in6* addr_in = (const struct sockaddr_in6*) addr;

		char szIp[NET_UV_INET6_ADDRSTRLEN + 1] = { 0 };
		int r = uv_ip6_name(addr_in, szIp, NET_UV_INET6_ADDRSTRLEN);
		if (r != 0)
		{
			return 0;
		}

		strip = szIp;
		port = ntohs(addr_in->sin6_port);
	}
	else
	{
		addrlen = sizeof(struct sockaddr_in);

		const struct sockaddr_in* addr_in = (const struct sockaddr_in*) addr;

		char szIp[NET_UV_INET_ADDRSTRLEN + 1] = { 0 };
		int r = uv_ip4_name(addr_in, szIp, NET_UV_INET_ADDRSTRLEN);
		if (r != 0)
		{
			return 0;
		}

		strip = szIp;
		port = ntohs(addr_in->sin_port);
	}

	outIP = strip;
	outPort = port;

	return addrlen;
}

struct sockaddr* net_getsocketAddr(const char* ip, unsigned int port, unsigned int* outAddrLen)
{
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
				if (outAddrLen != NULL)
				{
					*outAddrLen = sizeof(struct sockaddr_in);
				}
				break;

			}
			else if (rp->ai_family == AF_INET6)
			{
				addr6 = (struct sockaddr_in6*)rp->ai_addr;
				addr6->sin6_port = htons(port);
				if (outAddrLen != NULL)
				{
					*outAddrLen = sizeof(struct sockaddr_in6);
				}
				break;
			}
			else
			{
				continue;
			}
		}
		addr = addr4 ? (struct sockaddr*)addr4 : (struct sockaddr*)addr6;
		return addr;
	}
	return NULL;
}

NS_NET_UV_END
