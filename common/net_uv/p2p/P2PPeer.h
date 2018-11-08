#pragma once

#include "P2PPipe.h"

NS_NET_UV_BEGIN

using P2PPeerStartCallback = std::function<void(bool isSuccess)>;
using P2PPeerNewConnectCallback = std::function<void(uint64_t key)>;
using P2PPeerConnectToPeerCallback = std::function<void(uint64_t key, bool isSuccess)>;
using P2PPeerConnectToTurnCallback = std::function<void(bool isSuccess, uint64_t selfKey)>;
using P2PPeerDisConnectCallback = std::function<void(uint64_t key)>;
using P2PPeerRecvCallback = std::function<void(uint64_t key, char* data, uint32_t len)>;
using P2PPeerCloseCallback = std::function<void()>;

class P2PPeer : public Runnable
{
public:

	P2PPeer();

	P2PPeer(const P2PPeer&) = delete;

	virtual ~P2PPeer();

	bool start(const char* ip, uint32_t port);

	void stop();

	void restartConnectTurn();

	void connectToPeer(uint64_t key);

	void updateFrame();

	void send(uint64_t key, char* data, uint32_t len);

	void disconnect(uint64_t key);

	inline void setStartCallback(const P2PPeerStartCallback& call);
	
	inline void setNewConnectCallback(const P2PPeerNewConnectCallback& call);
	
	inline void setConnectToPeerCallback(const P2PPeerConnectToPeerCallback& call);
	
	inline void setConnectToTurnCallback(const P2PPeerConnectToTurnCallback& call);

	inline void setDisConnectCallback(const P2PPeerDisConnectCallback& call);

	inline void setRecvCallback(const P2PPeerRecvCallback& call);

	inline void setCloseCallback(const P2PPeerCloseCallback& call);

protected:
	/// Runnable
	virtual void run()override;

	void onIdleRun();

	void onTimerRun();

	void startFailureLogic();

	void onPipeRecvJsonCallback(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);

	void onPipeRecvKcpCallback(char* data, uint32_t len, uint64_t key, const struct sockaddr* addr);

	void onPipeNewSessionCallback(uint64_t key);

	void onPipeNewKcpCreateCallback(uint64_t key);

	void onPipeRemoveSessionCallback(uint64_t key);

	void pushInputOperation(uint64_t key, uint32_t what, void* data, uint32_t datalen);

	void pushOutputOperation(uint64_t key, uint32_t what, void* data, uint32_t datalen);

	void runInputOperation();

	void startBurrow(uint64_t toKey);

	void doConnectToTurn();

	void doSendCreateKcp(uint64_t toKey);

	void clearData();

protected:

	static void uv_on_idle_run(uv_idle_t* handle);

	static void uv_on_timer_run(uv_timer_t* handle);

protected:

	P2PPipe m_pipe;

	enum PeerState
	{
		STOP,
		START,
		WILL_STOP,
	};
	PeerState m_state;
	uv_loop_t m_loop;
	uv_idle_t m_idle;
	uv_timer_t m_timer;

	std::string m_turnIP;
	uint32_t m_turnPort;
	AddrInfo m_turnAddrInfo;
	AddrInfo m_selfAddrInfo;
	bool m_isConnectTurn;
	bool m_isStopConnectToTurn;
	uint16_t m_tryConnectTurnCount;

	struct OperationData
	{
		uint64_t key;
		uint32_t what;
		void* data;
		uint32_t datalen;
	};
	std::queue<OperationData> m_inputQue;
	std::queue<OperationData> m_inputQueCache;
	std::queue<OperationData> m_outputQue;
	std::queue<OperationData> m_outputQueCache;
	Mutex m_inputLock;
	Mutex m_outputLock;

	// �Ự����
	enum SessionState
	{
		DISCONNECT,
		CONNECT,
	};
	struct SessionData
	{
		SessionState state;
		uint16_t tryConnectCount;
		std::string sendData;
		bool isClient;
	};
	std::map<uint64_t, SessionData> m_sessionManager;

	// ������
	struct BurrowData
	{
		sockaddr_in targetAddr;
		uint16_t sendCount;
	};
	std::map<uint64_t, BurrowData> m_burrowManager;

	P2PPeerStartCallback		m_startCallback;
	P2PPeerNewConnectCallback	m_newConnectCallback;
	P2PPeerConnectToPeerCallback		m_connectToPeerCallback;
	P2PPeerConnectToTurnCallback		m_connectToTurnCallback;
	P2PPeerDisConnectCallback	m_disConnectCallback;
	P2PPeerRecvCallback			m_recvCallback;
	P2PPeerCloseCallback		m_closeCallback;
};


void P2PPeer::setStartCallback(const P2PPeerStartCallback& call)
{
	m_startCallback = std::move(call);
}

void P2PPeer::setNewConnectCallback(const P2PPeerNewConnectCallback& call)
{
	m_newConnectCallback = std::move(call);
}

void P2PPeer::setConnectToPeerCallback(const P2PPeerConnectToPeerCallback& call)
{
	m_connectToPeerCallback = std::move(call);
}

void P2PPeer::setConnectToTurnCallback(const P2PPeerConnectToTurnCallback& call)
{
	m_connectToTurnCallback = std::move(call);
}

void P2PPeer::setDisConnectCallback(const P2PPeerDisConnectCallback& call)
{
	m_disConnectCallback = std::move(call);
}

void P2PPeer::setRecvCallback(const P2PPeerRecvCallback& call)
{
	m_recvCallback = std::move(call);
}

void P2PPeer::setCloseCallback(const P2PPeerCloseCallback& call)
{
	m_closeCallback = std::move(call);
}

NS_NET_UV_END