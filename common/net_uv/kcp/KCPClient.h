#pragma once

#include "KCPSocket.h"
#include "KCPSession.h"

NS_NET_UV_BEGIN

class NET_UV_EXTERN KCPClient : public Client
{
public:
	enum CONNECTSTATE
	{
		CONNECT,		//已连接
		CONNECTING,		//正在连接
		DISCONNECTING,	//正在断开
		DISCONNECT,		//已断开
	};
public:

	KCPClient();
	KCPClient(const KCPClient&) = delete;
	virtual ~KCPClient();

	/// Client
	virtual void connect(const char* ip, unsigned int port, unsigned int sessionId)override;

	virtual void disconnect(unsigned int sessionId)override;

	virtual void closeClient()override;

	virtual void updateFrame()override;

	virtual void send(unsigned int sessionId, char* data, unsigned int len)override;

	virtual void removeSession(unsigned int sessionId)override;

	/// SessionManager
	virtual void send(Session* session, char* data, unsigned int len)override;

	virtual void disconnect(Session* session)override;

	/// KCPClient
	bool isCloseFinish();

	//设置所有socket是否自动重连
	void setAutoReconnect(bool isAuto);

	//设置所有socket自动重连时间(单位：S)
	void setAutoReconnectTime(float time);

	//是否自动重连
	void setAutoReconnectBySessionID(unsigned int sessionID, bool isAuto);

	//自动重连时间(单位：S)
	void setAutoReconnectTimeBySessionID(unsigned int sessionID, float time);

protected:

	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// KCPClient
	void onSocketConnect(Socket* socket, int status);

	void onSessionClose(Session* session);

	void onSessionRecvData(Session* session, char* data, unsigned int len, NetMsgTag tag);

	void createNewConnect(void* data);

	void pushThreadMsg(NetThreadMsgType type, Session* session, char* data = NULL, unsigned int len = 0U, NetMsgTag tag = NetMsgTag::MT_DEFAULT);

	void clearData();

	struct clientSessionData;
	KCPClient::clientSessionData* getClientSessionDataBySessionId(unsigned int sessionId);
	KCPClient::clientSessionData* getClientSessionDataBySession(Session* session);

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	void heartRun();
#endif

protected:
	uv_loop_t m_loop;
	uv_idle_t m_idle;
	uv_timer_t m_timer;

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	uv_timer_t m_heartTimer;
#endif

	bool m_reconnect;		// 是否自动断线重连
	float m_totalTime;		// 断线重连时间
	bool m_enableNoDelay;
	int m_enableKeepAlive;
	int m_keepAliveDelay;

	struct clientSessionData
	{
		clientSessionData() {}
		~clientSessionData() {}
		CONNECTSTATE connectState;
		bool removeTag; // 是否被标记移除
		bool reconnect;	// 是否断线重连
		float curtime;
		float totaltime;
		std::string ip;
		unsigned int port;
		KCPSession* session;
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
		int curHeartTime;
		int curHeartCount;
#endif
	};

	// 所有会话
	std::map<unsigned int, clientSessionData*> m_allSessionMap;

	//客户端所处阶段
	enum class clientStage
	{
		START,
		CLEAR_SESSION,//清理会话
		WAIT_EXIT,//即将退出
		STOP
	};
	clientStage m_clientStage;
	bool m_isStop;
protected:

	static void uv_timer_run(uv_timer_t* handle);
	static void uv_on_idle_run(uv_idle_t* handle);
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	static void uv_heart_timer_callback(uv_timer_t* handle);
#endif
};
NS_NET_UV_END


