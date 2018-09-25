#include "Client.h"

NS_NET_UV_BEGIN

Client::Client()
	: m_connectCall(nullptr)
	, m_disconnectCall(nullptr)
	, m_recvCall(nullptr)
{
}

Client::~Client()
{}

NS_NET_UV_END