#pragma once


#include "TCPSocket.h"
#include "TCPSession.h"

NS_NET_UV_BEGIN

class NET_UV_EXTERN TCPServer : public Server
{
	//服务器所处阶段
	enum class ServerStage
	{
		START,		//启动中
		RUN,		//运行中
		WAIT_CLOSE_SERVER_SOCKET,// 等待服务器套接字关闭
		CLEAR,		//清理会话
		WAIT_SESSION_CLOSE,// 等待会话关闭
		STOP		//退出
	};

	struct tcpSessionData
	{
		tcpSessionData()
		{
#if TCP_OPEN_UV_THREAD_HEARTBEAT == 1
			curHeartTime = 0;
			curHeartCount = 0;
#endif
			isInvalid = false;
			session = NULL;
		}
		TCPSession* session;
		bool isInvalid;
#if TCP_OPEN_UV_THREAD_HEARTBEAT == 1
		int curHeartTime;
		int curHeartCount;
#endif
	};

public:

	TCPServer();
	TCPServer(const TCPServer&) = delete;

	virtual ~TCPServer();

	/// Server
	virtual void startServer(const char* ip, int port, bool isIPV6)override;

	virtual bool stopServer()override;

	virtual void updateFrame()override;

	/// SessionManager
	virtual void send(Session* session, char* data, unsigned int len)override;

	virtual void disconnect(Session* session)override;

	/// TCPServer
	bool isCloseFinish();

protected:

	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// TCPServer
	void onNewConnect(uv_stream_t* server, int status);

	void onServerSocketClose(Socket* svr);

	void onServerIdleRun();

	void onSessionRecvData(Session* session, char* data, unsigned int len, NetMsgTag tag);
	
protected:

	void pushThreadMsg(NetThreadMsgType type, Session* session, char* data = NULL, unsigned int len = 0, const NetMsgTag& tag = NetMsgTag::MT_DEFAULT);

	void addNewSession(TCPSession* session);

	void onSessionClose(Session* session);

	void clearData();

#if TCP_OPEN_UV_THREAD_HEARTBEAT == 1
	void heartRun();
#endif

protected:
	bool m_start;

	std::string m_ip;
	int m_port;
	bool m_isIPV6;

	TCPSocket* m_server;

	uv_idle_t m_idle;
	uv_loop_t m_loop;
	
	// 服务器所处阶段
	ServerStage m_serverStage;

#if TCP_OPEN_UV_THREAD_HEARTBEAT == 1
	uv_timer_t* m_heartTimer;
#endif

	// 会话管理
	std::map<unsigned int, tcpSessionData> m_allSession;

	unsigned int m_sessionID;
protected:

	static void uv_on_idle_run(uv_idle_t* handle);

#if TCP_OPEN_UV_THREAD_HEARTBEAT == 1
	static void uv_heart_timer_callback(uv_timer_t* handle);
#endif
};



NS_NET_UV_END
