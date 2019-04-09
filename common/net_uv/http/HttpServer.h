#pragma once

#include "HttpCommon.h"
#include <unordered_map>

NS_NET_UV_BEGIN

class HttpRequest;
class HttpResponse;
class HttpContext;

using HttpServerCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

class HttpServer
{
public:

	HttpServer();

	~HttpServer();

	bool startServer(const char* ip, uint32_t port, bool isIPV6, int32_t maxCount);

	bool stopServer();

	void updateFrame();

public:

	inline void setHttpCallback(const HttpServerCallback& cb);

	inline void setCloseCallback(const ServerCloseCall& call);

protected:

	void onSvrNewConnectCallback(Server* svr, Session* session);

	void onSvrRecvCallback(Server* svr, Session* session, char* data, uint32_t len);

	void onSvrDisconnectCallback(Server* svr, Session* session);

private:

	HttpServerCallback m_svrCallback;

	Pure_TCPServer* m_svr;

	std::unordered_map<Session*, HttpContext*> m_contextMap;
};


void HttpServer::setHttpCallback(const HttpServerCallback& cb)
{
	m_svrCallback = std::move(cb);
}

void HttpServer::setCloseCallback(const ServerCloseCall& call)
{
	m_svr->setCloseCallback(call);
}

NS_NET_UV_END
