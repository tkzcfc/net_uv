#pragma once

#include "TCPThreadMsg.h"
#include "TCPSocket.h"
#include "TCPSession.h"

NS_NET_UV_BEGIN

class NET_UV_EXTERN TCPClient : public Client
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

	TCPClient();
	TCPClient(const TCPClient&) = delete;
	virtual ~TCPClient();

	/// Client
	virtual void connect(const char* ip, unsigned int port, unsigned int sessionId)override;

	virtual void disconnect(unsigned int sessionId)override;

	virtual void closeClient()override;
	
	virtual void updateFrame()override;

	virtual void send(unsigned int sessionId, char* data, unsigned int len)override;

	/// SessionManager
	virtual void send(Session* session, char* data, unsigned int len)override;

	virtual void disconnect(Session* session)override;

	/// TCPClient
	//查询客户端是否已经关闭完毕
	//如果返回true，则可以进行该类的内存释放
	//若返回false就进行内存释放时，该类将阻塞至线程完全退出
	bool isCloseFinish();

	//是否启用TCP_NODELAY
	bool setSocketNoDelay(bool enable);

	//设置心跳
	bool setSocketKeepAlive(int enable, unsigned int delay);

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

	/// TCPClient
	void onSocketConnect(Socket* socket, int status);

	void onSessionClose(Session* session);

#if OPEN_UV_THREAD_HEARTBEAT == 1
	void onSessionRecvData(TCPSession* session, char* data, unsigned int len, TCPMsgTag tag);
#else
	void onSessionRecvData(TCPSession* session, char* data, unsigned int len);
#endif

	void createNewConnect(void* data);

	void pushThreadMsg(TCPThreadMsgType type, Session* session, char* data = NULL, unsigned int len = 0U, TCPMsgTag tag = TCPMsgTag::MT_DEFAULT);

	void clearData();

	struct clientSessionData;
	TCPClient::clientSessionData* getClientSessionDataBySessionId(unsigned int sessionId);
	TCPClient::clientSessionData* getClientSessionDataBySession(Session* session);

#if OPEN_UV_THREAD_HEARTBEAT == 1
	void heartRun();
#endif

protected:
	uv_loop_t m_loop;
	uv_idle_t m_idle;
	uv_timer_t m_timer;

#if OPEN_UV_THREAD_HEARTBEAT == 1
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
		bool reconnect;
		float curtime;
		float totaltime;
		std::string ip;
		unsigned int port;
		TCPSession* session;
#if OPEN_UV_THREAD_HEARTBEAT == 1
		int curHeartTime;
		int curHeartCount;
#endif
	};

	// 所有会话
	std::map<unsigned int, clientSessionData*> m_allSessionMap;

	//线程消息队列
	Mutex m_msgMutex;
	std::queue<TCPThreadMsg_C> m_msgQue;
	std::queue<TCPThreadMsg_C> m_msgDispatchQue;

	//客户端所处阶段
	enum class clientStage
	{
		START,
		CLEAR_SOCKET,//即将销毁socket
		WAIT_EXIT,//即将退出
		STOP
	};
	clientStage m_clientStage;
	bool m_isStop;
protected:

	static void uv_timer_run(uv_timer_t* handle);
	static void uv_on_idle_run(uv_idle_t* handle);
#if OPEN_UV_THREAD_HEARTBEAT == 1
	static void uv_heart_timer_callback(uv_timer_t* handle);
#endif
};
NS_NET_UV_END


