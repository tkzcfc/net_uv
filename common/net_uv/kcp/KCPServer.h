#pragma once


#include "KCPSocket.h"
#include "KCPSession.h"

NS_NET_UV_BEGIN

class KCPServer : public Server
{
	struct serverSessionData
	{
		serverSessionData()
		{
			isInvalid = false;
			session = NULL;
		}
		KCPSession* session;
		bool isInvalid;
	};

public:

	KCPServer();
	KCPServer(const KCPServer&) = delete;

	virtual ~KCPServer();

	/// Server
	virtual void startServer(const char* ip, unsigned int port, bool isIPV6)override;

	virtual bool stopServer()override;

	virtual void updateFrame()override;

	/// SessionManager
	virtual void send(unsigned int sessionID, char* data, unsigned int len)override;

	virtual void disconnect(unsigned int sessionID)override;

	/// KCPServer
	/// 使用服务器Socket向某个地址发送消息
	/// ip: 仅支持IP地址 不支持域名解析
	bool svrUdpSend(const char* ip, unsigned int port, bool isIPV6, char* data, unsigned int len);

	bool svrUdpSend(struct sockaddr* addr, unsigned int addrlen, char* data, unsigned int len);

protected:

	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// KCPServer
	void onNewConnect(Socket* socket);

	void onServerSocketClose(Socket* svr);

	bool onServerSocketConnectFilter(const struct sockaddr* addr);

	void onSessionRecvData(Session* session, char* data, unsigned int len);

	/// Server
	virtual void onIdleRun()override;

	virtual void onSessionUpdateRun()override;

protected:

	void addNewSession(KCPSession* session);

	void onSessionClose(Session* session);

	void clearData();

protected:
	bool m_start;

	KCPSocket* m_server;

	// 会话管理
	std::map<unsigned int, serverSessionData> m_allSession;
};



NS_NET_UV_END
