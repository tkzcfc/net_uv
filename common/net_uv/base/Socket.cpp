#include "Socket.h"

NS_NET_UV_BEGIN

Socket::Socket()
	: m_port(0)
	, m_isIPV6(false)
	, m_loop(nullptr)
	, m_connectCall(nullptr)
	, m_closeCall(nullptr)
	, m_userdata(nullptr)
	, m_recvCall(nullptr)
{
}

Socket::~Socket()
{
}

NS_NET_UV_END