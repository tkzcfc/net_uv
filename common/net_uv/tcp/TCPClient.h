#pragma once

#include "TCPSocket.h"
#include "TCPSession.h"

NS_NET_UV_BEGIN

class TCPClient : public Client
{
protected:

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
		TCPSession* session;
	};

public:

	TCPClient();
	TCPClient(const TCPClient&) = delete;
	virtual ~TCPClient();

	/// Client
	virtual void connect(const char* ip, unsigned int port, unsigned int sessionId)override;

	virtual void closeClient()override;
	
	virtual void updateFrame()override;

	virtual void removeSession(unsigned int sessionId)override;

	/// SessionManager
	virtual void disconnect(unsigned int sessionId)override;

	virtual void send(unsigned int sessionId, char* data, unsigned int len)override;

	/// TCPClient
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

	/// Client
	virtual void onIdleRun()override;

	virtual void onSessionUpdateRun()override;

	/// TCPClient
	void onSocketConnect(Socket* socket, int status);

	void onSessionClose(Session* session);

	void onSessionRecvData(Session* session, char* data, unsigned int len);

	void createNewConnect(void* data);

	void clearData();

	clientSessionData* getClientSessionDataBySessionId(unsigned int sessionId);

	clientSessionData* getClientSessionDataBySession(Session* session);

	void onClientUpdate();

protected:
	uv_timer_t m_clientUpdateTimer;

	bool m_reconnect;		// 是否自动断线重连
	float m_totalTime;		// 断线重连时间
	bool m_enableNoDelay;	
	int m_enableKeepAlive; 
	unsigned int m_keepAliveDelay;

	// 所有会话
	std::map<unsigned int, clientSessionData*> m_allSessionMap;
	
	bool m_isStop;
protected:

	static void uv_client_update_timer_run(uv_timer_t* handle);
};
NS_NET_UV_END


