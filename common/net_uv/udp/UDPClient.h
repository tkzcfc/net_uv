#pragma once

#include "UDPSocket.h"
#include "KcpSession.h"
#include "UDPThreadMsg.h"

NS_NET_UV_BEGIN

class UDPClient : public Client
{
public:
	enum CONNECTSTATE
	{
		CONNECT,		//已连接
		DISCONNECT,		//已断开
	};
public:
	UDPClient();
	UDPClient(const UDPClient&) = delete;
	virtual ~UDPClient();

	/// Client
	virtual void connect(const char* ip, unsigned int port, unsigned int sessionId)override;

	virtual void disconnect(unsigned int sessionId)override;

	virtual void closeClient()override;

	virtual void updateFrame()override;

	/// SessionManager
	virtual void send(Session* session, char* data, unsigned int len)override;

	virtual void disconnect(Session* session)override;

	/// UDPClient
	void send(unsigned int sessionId, char* data, unsigned int len);

protected:
	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// UDPClient
	void idle_run();

	void clearData();

	void pushThreadMsg(UDPThreadMsgType type, Session* session, char* data = NULL, unsigned int len = 0U);

	struct clientSessionData;
	UDPClient::clientSessionData* getClientSessionDataBySessionId(unsigned int sessionId);

	void createNewConnect(void* data);

protected:
	struct clientSessionData
	{
		clientSessionData() {}
		~clientSessionData() {}
		CONNECTSTATE connectState;
		KcpSession* session;
	};
	std::map<unsigned int, clientSessionData> m_allSessionMap;

	uv_loop_t m_loop;
	uv_idle_t m_idle;
	bool m_isStop;

	//客户端所处阶段
	enum class clientStage
	{
		START,
		CLEAR_SESSION,
		STOP
	};
	clientStage m_clientStage;

	//线程消息队列
	Mutex m_msgMutex;
	std::queue<UDPThreadMsg_C> m_msgQue;
	std::queue<UDPThreadMsg_C> m_msgDispatchQue;

protected:

	static void uv_on_idle_run(uv_idle_t* handle);
};

NS_NET_UV_END
