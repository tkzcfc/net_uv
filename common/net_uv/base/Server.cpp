#include "Server.h"


NS_NET_UV_BEGIN

Server::Server()
	: m_closeCall(nullptr)
	, m_startCall(nullptr)
	, m_newConnectCall(nullptr)
	, m_recvCall(nullptr)
	, m_disconnectCall(nullptr)
{}

Server::~Server()
{}

void Server::startServer(const char* ip, int port, bool isIPV6)
{
	assert(m_startCall != nullptr);
	assert(m_closeCall != nullptr);
	assert(m_newConnectCall != nullptr);
	assert(m_recvCall != nullptr);
	assert(m_disconnectCall != nullptr);
}

NS_NET_UV_END
