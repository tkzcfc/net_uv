#pragma once

#include "Common.h"
#include "Session.h"

NS_NET_UV_BEGIN

class Server;
using ServerStartCall = std::function<void(Server*, bool)>;
using ServerCloseCall = std::function<void(Server*)>;
using ServerNewConnectCall = std::function<void(Server*, Session*)>;
using ServerRecvCall = std::function<void(Server*, Session*, char* data, unsigned int len)>;
using ServerDisconnectCall = std::function<void(Server*, Session*)>;

class NET_UV_EXTERN Server
{
public:
	Server();
	virtual ~Server();

	virtual void startServer(const char* ip, int port, bool isIPV6);

	virtual bool stopServer() = 0;

	// Ö÷Ïß³ÌÂÖÑ¯
	virtual void updateFrame() = 0;

	inline void setCloseCallback(const ServerCloseCall& call);

	inline void setStartCallback(const ServerStartCall& call);

	inline void setNewConnectCallback(const ServerNewConnectCall& call);

	inline void setRecvCallback(const ServerRecvCall& call);

	inline void setDisconnectCallback(const ServerDisconnectCall& call);

protected:
	ServerStartCall m_startCall;
	ServerCloseCall m_closeCall;
	ServerNewConnectCall m_newConnectCall;
	ServerRecvCall m_recvCall;
	ServerDisconnectCall m_disconnectCall;
};

void Server::setCloseCallback(const ServerCloseCall& call)
{
	m_closeCall = std::move(call);
}

void Server::setStartCallback(const ServerStartCall& call)
{
	m_startCall = std::move(call);
}

void Server::setNewConnectCallback(const ServerNewConnectCall& call)
{
	m_newConnectCall = std::move(call);
}

void Server::setRecvCallback(const ServerRecvCall& call)
{
	m_recvCall = std::move(call);
}

void Server::setDisconnectCallback(const ServerDisconnectCall& call)
{
	m_disconnectCall = std::move(call);
}


NS_NET_UV_END
