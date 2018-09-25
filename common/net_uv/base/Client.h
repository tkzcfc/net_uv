#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

class Client;
class Session;

using ClientConnectCall = std::function<void(Client*, Session*, bool)>;
using ClientDisconnectCall = std::function<void(Client*, Session*)>;
using ClientRecvCall = std::function<void(Client*, Session*, char*, unsigned int)>;
using ClientCloseCall = std::function<void(Client*)>;

class NET_UV_EXTERN Client
{
public:
	Client();

	virtual ~Client();

	virtual void connect(const char* ip, unsigned int port, unsigned int key) = 0;

	virtual void disconnect(unsigned int key) = 0;

	virtual void closeClient() = 0;

	virtual void updateFrame() = 0;

	inline void setConnectCallback(const ClientConnectCall& call);

	inline void setDisconnectCallback(const ClientDisconnectCall& call);

	inline void setRecvCallback(const ClientRecvCall& call);

	inline void setClientCloseCallback(const ClientCloseCall& call);

protected:
	ClientConnectCall m_connectCall;
	ClientDisconnectCall m_disconnectCall;
	ClientRecvCall m_recvCall;
	ClientCloseCall m_clientCloseCall;
};

void Client::setConnectCallback(const ClientConnectCall& call)
{
	m_connectCall = std::move(call);
}

void Client::setDisconnectCallback(const ClientDisconnectCall& call)
{
	m_disconnectCall = std::move(call);
}

void Client::setRecvCallback(const ClientRecvCall& call)
{
	m_recvCall = std::move(call);
}

void Client::setClientCloseCallback(const ClientCloseCall& call)
{
	m_clientCloseCall = std::move(call);
}

NS_NET_UV_END
