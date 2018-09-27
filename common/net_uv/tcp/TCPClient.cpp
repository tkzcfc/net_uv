#include "TCPClient.h"
#include "TCPCommon.h"
#include "TCPUtils.h"

NS_NET_UV_BEGIN

//断线重连定时器检测间隔
#define CLIENT_TIMER_DELAY (0.2f)


enum 
{
	TCP_CLI_OP_CONNECT,	//	连接
	TCP_CLI_OP_SENDDATA,	// 发送数据
	TCP_CLI_OP_DISCONNECT,	// 断开连接
	TCP_CLI_OP_SET_AUTO_CONNECT, //设置自动连接
	TCP_CLI_OP_SET_RECON_TIME,//设置重连时间
	TCP_CLI_OP_SET_KEEP_ALIVE,//设置心跳
	TCP_CLI_OP_SET_NO_DELAY, //设置NoDelay
	TCP_CLI_OP_CLIENT_CLOSE, //客户端退出
};

// 连接操作
struct TCPClientConnectOperation
{
	TCPClientConnectOperation() {}
	~TCPClientConnectOperation() {}
	std::string ip;
	unsigned int port;
	unsigned int sessionID;
};

// 设置自动连接操作
struct TCPClientAutoConnectOperation
{
	TCPClientAutoConnectOperation() {}
	~TCPClientAutoConnectOperation() {}
	bool isAuto;
	unsigned int sessionID;
};

// 设置重连时间操作
struct TCPClientReconnectTimeOperation
{
	TCPClientReconnectTimeOperation() {}
	~TCPClientReconnectTimeOperation() {}
	float time;
	unsigned int sessionID;
};




//////////////////////////////////////////////////////////////////////////////////
TCPClient::TCPClient()
	: m_reconnect(true)
	, m_totalTime(3.0f)
	, m_enableNoDelay(true)
	, m_enableKeepAlive(true)
	, m_keepAliveDelay(10)
	, m_isStop(false)
{
	uv_loop_init(&m_loop);
	
	m_clientStage = clientStage::START;
	
	uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	uv_idle_start(&m_idle, uv_on_idle_run);

	uv_timer_init(&m_loop, &m_timer);
	m_timer.data = this;
	uv_timer_start(&m_timer, uv_timer_run, (uint64_t)(CLIENT_TIMER_DELAY * 1000), (uint64_t)(CLIENT_TIMER_DELAY * 1000));

#if OPEN_UV_THREAD_HEARTBEAT == 1
	m_heartTimer.data = this;
	uv_timer_init(&m_loop, &m_heartTimer);
	uv_timer_start(&m_heartTimer, TCPClient::uv_heart_timer_callback, HEARTBEAT_TIMER_DELAY, HEARTBEAT_TIMER_DELAY);
#endif
	this->startThread();
}

TCPClient::~TCPClient()
{
	closeClient();
	this->join();
	clearData();
}


void TCPClient::connect(const char* ip, unsigned int port, unsigned int sessionId)
{
	if (m_isStop)
		return;

	assert(ip != NULL);

	TCPClientConnectOperation* opData = (TCPClientConnectOperation*)fc_malloc(sizeof(TCPClientConnectOperation));
	new (opData)TCPClientConnectOperation();

	opData->ip = ip;
	opData->port = port;
	opData->sessionID = sessionId;
	pushOperation(TCP_CLI_OP_CONNECT, opData, 0U, 0U);
}

void TCPClient::disconnect(unsigned int sessionId)
{
	if (m_isStop)
		return;

	pushOperation(TCP_CLI_OP_DISCONNECT, NULL, 0U, sessionId);
}

void TCPClient::disconnect(Session* session)
{
	disconnect(session->getSessionID());
}

void TCPClient::closeClient()
{
	if (m_isStop)
		return;
	m_isStop = true;
	pushOperation(TCP_CLI_OP_CLIENT_CLOSE, NULL, 0U, 0U);
}

bool TCPClient::isCloseFinish()
{
	return (m_clientStage == clientStage::STOP);
}

void TCPClient::send(unsigned int sessionId, char* data, unsigned int len)
{
	if (m_isStop)
		return;

	if (data == 0 || len <= 0)
		return;
	int bufCount = 0;
#if OPEN_UV_THREAD_HEARTBEAT == 1
	uv_buf_t* bufArr = packageData(data, len, &bufCount, TCPMsgTag::MT_DEFAULT);
#else
	uv_buf_t* bufArr = packageData(data, len, &bufCount);
#endif
	if (bufArr == NULL)
		return;

	for (int i = 0; i < bufCount; ++i)
	{
		pushOperation(TCP_CLI_OP_SENDDATA, (bufArr + i)->base, (bufArr + i)->len, sessionId);
	}
	fc_free(bufArr);
}

void TCPClient::send(Session* session, char* data, unsigned int len)
{
	send(session->getSessionID(), data, len);
}

bool TCPClient::setSocketNoDelay(bool enable)
{
	if (m_isStop)
		return false;

	m_enableNoDelay = enable;
	pushOperation(TCP_CLI_OP_SET_NO_DELAY, NULL, 0U, 0U);
	return true;
}


bool TCPClient::setSocketKeepAlive(int enable, unsigned int delay)
{
	if (m_isStop)
		return false;

	m_enableKeepAlive = enable;
	m_keepAliveDelay = delay;
	
	pushOperation(TCP_CLI_OP_SET_KEEP_ALIVE, NULL, 0U, 0U);

	return true;
}

void TCPClient::executeOperation()
{
	if (m_operationMutex.trylock() != 0)
	{
		return;
	}

	if (m_operationQue.empty())
	{
		m_operationMutex.unlock();
		return;
	}

	while (!m_operationQue.empty())
	{
		m_operationDispatchQue.push(m_operationQue.front());
		m_operationQue.pop();
	}
	m_operationMutex.unlock();

	while (!m_operationDispatchQue.empty())
	{
		auto & curOperation = m_operationDispatchQue.front();
		switch (curOperation.operationType)
		{
		case TCP_CLI_OP_SENDDATA:		// 数据发送
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData)
			{
				sessionData->session->executeSend((char*)curOperation.operationData, curOperation.operationDataLen);
			}
			else
			{
				fc_free(curOperation.operationData);
			}
		}break;
		case TCP_CLI_OP_DISCONNECT:	// 断开连接
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData->connectState == CONNECT)
			{
				sessionData->connectState = DISCONNECTING;
				sessionData->session->executeDisconnect();
			}
		}break;
		case TCP_CLI_OP_CONNECT:	// 连接
		{
			if (curOperation.operationData)
			{
				createNewConnect(curOperation.operationData);
				((TCPClientConnectOperation*)curOperation.operationData)->~TCPClientConnectOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		case TCP_CLI_OP_SET_AUTO_CONNECT: //设置自动连接
		{
			if (curOperation.operationData)
			{
				TCPClientAutoConnectOperation* opData = (TCPClientAutoConnectOperation*)curOperation.operationData;
				if (opData->sessionID == -1)
				{
					for (auto& it : m_allSessionMap)
					{
						it.second->reconnect = opData->isAuto;
					}
				}
				else
				{
					auto sessionData = getClientSessionDataBySessionId(opData->sessionID);
					if (sessionData)
					{
						sessionData->reconnect = opData->isAuto;
					}
				}
				opData->~TCPClientAutoConnectOperation();
				fc_free(opData);
			}
		}break;
		case TCP_CLI_OP_SET_RECON_TIME: //设置重连时间
		{
			if (curOperation.operationData)
			{
				TCPClientReconnectTimeOperation* opData = (TCPClientReconnectTimeOperation*)curOperation.operationData;
				if (opData->sessionID == -1)
				{
					for (auto& it : m_allSessionMap)
					{
						it.second->totaltime = opData->time;
					}
				}
				else
				{
					auto sessionData = getClientSessionDataBySessionId(opData->sessionID);
					if (sessionData)
					{
						sessionData->totaltime = opData->time;
					}
				}
				opData->~TCPClientReconnectTimeOperation();
				fc_free(opData);
			}
		}break;
		case TCP_CLI_OP_SET_KEEP_ALIVE: //心跳设置
		{
			for (auto& it : m_allSessionMap)
			{
				auto socket = it.second->session->getTCPSocket();
				if (socket)
				{
					socket->setKeepAlive(m_keepAliveDelay, m_keepAliveDelay);
				}
			}
		}break;
		case TCP_CLI_OP_SET_NO_DELAY:// 设置nodelay
		{
			for (auto& it : m_allSessionMap)
			{
				auto socket = it.second->session->getTCPSocket();
				if (socket)
				{
					socket->setNoDelay(m_enableNoDelay);
				}
			}
		}break;
		case TCP_CLI_OP_CLIENT_CLOSE://客户端关闭
		{
#if OPEN_UV_THREAD_HEARTBEAT == 1
			uv_timer_stop(&m_heartTimer);
#endif
			m_clientStage = clientStage::CLEAR_SOCKET;
		}break;
		default:
			break;
		}
		m_operationDispatchQue.pop();
	}
}

void TCPClient::createNewConnect(void* data)
{
	if (data == NULL)
		return;
	TCPClientConnectOperation* opData = (TCPClientConnectOperation*)data;

	auto it = m_allSessionMap.find(opData->sessionID);
	if (it != m_allSessionMap.end())
	{
		//对比端口和IP是否一致
		if (strcmp(opData->ip.c_str(), it->second->ip.c_str()) != 0 && opData->port != it->second->port)
		{
			return;
		}

		if (it->second->connectState == CONNECTSTATE::DISCONNECT)
		{
			if (it->second->session->getTCPSocket()->connect(opData->ip.c_str(), opData->port))
			{
				it->second->connectState = CONNECTSTATE::CONNECTING;
			}
			else
			{
				it->second->connectState = CONNECTSTATE::DISCONNECT;
				it->second->session->executeDisconnect();
				pushThreadMsg(CONNECT_FAIL, it->second->session);
			}
		}
	}
	else
	{
		TCPSocket* socket = (TCPSocket*)fc_malloc(sizeof(TCPSocket));
		new (socket) TCPSocket(&m_loop); 
		socket->setConnectCallback(std::bind(&TCPClient::onSocketConnect, this, std::placeholders::_1, std::placeholders::_2));

#if OPEN_UV_THREAD_HEARTBEAT == 1
		TCPSession* session = TCPSession::createSession(this, socket, std::bind(&TCPClient::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
#else
		TCPSession* session = TCPSession::createSession(this, socket, std::bind(&TCPClient::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
#endif
		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_FATAL, "创建会话失败，可能是内存不足!!!");
			return;
		}
		session->setSessionClose(std::bind(&TCPClient::onSessionClose, this, std::placeholders::_1));
		session->setSessionID(opData->sessionID);
		session->setIsOnline(false);
		

		clientSessionData* cs = (clientSessionData*)fc_malloc(sizeof(clientSessionData));
		new (cs) clientSessionData();
		cs->ip = opData->ip;
		cs->port = opData->port;
		cs->session = session;
		cs->curtime = 0;
		cs->reconnect = m_reconnect;
		cs->totaltime = m_totalTime;
		cs->connectState = CONNECTSTATE::CONNECTING;
#if OPEN_UV_THREAD_HEARTBEAT == 1
		cs->curHeartCount = HEARTBEAT_COUNT_RESET_VALUE_CLIENT;
		cs->curHeartTime = 0;
#endif
		m_allSessionMap.insert(std::make_pair(opData->sessionID, cs));
		
		if (socket->connect(opData->ip.c_str(), opData->port))
		{
			cs->connectState = CONNECTSTATE::CONNECTING;
		}
		else
		{
			cs->connectState = CONNECTSTATE::DISCONNECT;
			pushThreadMsg(CONNECT_FAIL, it->second->session);
		}
	}
}

#if OPEN_UV_THREAD_HEARTBEAT == 1
void TCPClient::onSessionRecvData(TCPSession* session, char* data, unsigned int len, TCPMsgTag tag)
{
	pushThreadMsg(RECV_DATA, session, data, len, tag);
}
#else
void TCPClient::onSessionRecvData(TCPSession* session, char* data, unsigned int len)
{
	pushThreadMsg(RECV_DATA, session, data, len, TCPMsgTag::MT_DEFAULT);
}
#endif

void TCPClient::updateFrame()
{
	if (m_msgMutex.trylock() != 0)
	{
		return;
	}

	if (m_msgQue.empty())
	{
		m_msgMutex.unlock();
		return;
	}
	
	while (!m_msgQue.empty())
	{
		m_msgDispatchQue.push(m_msgQue.front());
		m_msgQue.pop();
	}
	m_msgMutex.unlock();

	while (!m_msgDispatchQue.empty())
	{
		const ThreadMsg_C& Msg = m_msgDispatchQue.front();
		switch (Msg.msgType)
		{
		case ThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case ThreadMsgType::CONNECT_FAIL:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, false);
			}
		}break;
		case ThreadMsgType::CONNECT:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, true);
			}
		}break;
		case ThreadMsgType::DIS_CONNECT:
		{
			if (m_disconnectCall != nullptr)
			{
				m_disconnectCall(this, Msg.pSession);
			}
		}break;
		case ThreadMsgType::EXIT_LOOP:
		{
			if (m_clientCloseCall != nullptr)
			{
				m_clientCloseCall(this);
			}
		}break;
		default:
			break;
		}
		m_msgDispatchQue.pop();
	}
}

void TCPClient::run()
{
	m_loop.data = NULL;

	uv_run(&m_loop, UV_RUN_DEFAULT);

	uv_loop_close(&m_loop);

	m_clientStage = clientStage::STOP;

	this->pushThreadMsg(ThreadMsgType::EXIT_LOOP, NULL);
}

void TCPClient::onSocketConnect(Socket* socket, bool isSuc)
{
	Session* pSession = NULL;

	for (auto& it : m_allSessionMap)
	{
		if (it.second->session->getTCPSocket() == socket)
		{
			pSession = it.second->session;
			it.second->session->setIsOnline(isSuc);
			it.second->connectState = isSuc ? CONNECTSTATE::CONNECT : CONNECTSTATE::DISCONNECT;
			if (isSuc)
			{
				if (m_clientStage != clientStage::START)
				{
					it.second->session->executeDisconnect();
					pSession = NULL;
				}
				else
				{
					it.second->session->getTCPSocket()->setNoDelay(m_enableNoDelay);
					it.second->session->getTCPSocket()->setKeepAlive(m_enableKeepAlive, m_keepAliveDelay);
				}
			}
			break;
		}
	}

	if (pSession)
	{
		pushThreadMsg(isSuc ? ThreadMsgType::CONNECT : ThreadMsgType::CONNECT_FAIL, pSession);
	}
}

void TCPClient::onSessionClose(Session* session)
{
	auto sessionData = getClientSessionDataBySession(session);
	if (sessionData)
	{
		sessionData->connectState = CONNECTSTATE::DISCONNECT;
		pushThreadMsg(ThreadMsgType::DIS_CONNECT, sessionData->session);
	}
}

void TCPClient::pushThreadMsg(ThreadMsgType type, Session* session, char* data, unsigned int len, TCPMsgTag tag)
{
#if OPEN_UV_THREAD_HEARTBEAT == 1
	if (type == ThreadMsgType::RECV_DATA)
	{
		auto it = m_allSessionMap.find(session->getSessionID());
		if (it != m_allSessionMap.end())
		{
			auto clientdata = it->second;
			clientdata->curHeartCount = HEARTBEAT_COUNT_RESET_VALUE_CLIENT;
			clientdata->curHeartTime = 0;
			if (tag == TCPMsgTag::MT_HEARTBEAT)
			{
				if (len == HEARTBEAT_MSG_SIZE)
				{
					if (*((char*)data) == HEARTBEAT_MSG_S2C)
					{
						NET_UV_LOG(NET_UV_L_HEART, "recv heart s->c");
						unsigned int sendlen = 0;
						char* senddata = packageHeartMsgData(HEARTBEAT_RET_MSG_C2S, &sendlen);
						clientdata->session->executeSend(senddata, sendlen);
					}
				}
				else// 不合法心跳
				{
					clientdata->session->executeDisconnect();
				}
				fc_free(data);
				return;
			}
		}
	}
#endif

	ThreadMsg_C msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;
	msg.tag = tag;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

TCPClient::clientSessionData* TCPClient::getClientSessionDataBySessionId(unsigned int sessionId)
{
	auto it = m_allSessionMap.find(sessionId);
	if (it != m_allSessionMap.end())
		return it->second;
	return NULL;
}

TCPClient::clientSessionData* TCPClient::getClientSessionDataBySession(Session* session)
{
	for (auto &it : m_allSessionMap)
	{
		if (it.second->session == session)
		{
			return it.second;
		}
	}
	return NULL;
}

#if OPEN_UV_THREAD_HEARTBEAT == 1
void TCPClient::heartRun()
{
	for (auto &it : m_allSessionMap)
	{
		auto clientdata = it.second;

		if(clientdata->connectState != CONNECT)
			continue;

		clientdata->curHeartTime += HEARTBEAT_TIMER_DELAY;
		if (clientdata->curHeartTime > HEARTBEAT_CHECK_DELAY)
		{
			clientdata->curHeartTime = 0;
			clientdata->curHeartCount++;
			if (clientdata->curHeartCount > 0)
			{
				if (clientdata->curHeartCount > HEARTBEAT_MAX_COUNT_CLIENT)
				{
					clientdata->curHeartCount = 0;
					clientdata->session->executeDisconnect();
				}
				else
				{
					unsigned int sendlen = 0;
					char* senddata = packageHeartMsgData(HEARTBEAT_MSG_C2S, &sendlen);
					clientdata->session->executeSend(senddata, sendlen);
					NET_UV_LOG(NET_UV_L_HEART, "send heart c->s");
				}
			}
		}
	}
}
#endif

void TCPClient::setAutoReconnect(bool isAuto)
{
	if (m_isStop)
		return;

	m_reconnect = isAuto;

	TCPClientAutoConnectOperation* opData = (TCPClientAutoConnectOperation*)fc_malloc(sizeof(TCPClientAutoConnectOperation));
	new(opData) TCPClientAutoConnectOperation();

	opData->isAuto = isAuto;
	opData->sessionID = -1;

	pushOperation(TCP_CLI_OP_SET_AUTO_CONNECT, opData, NULL, NULL);
}

void TCPClient::setAutoReconnectTime(float time)
{
	if (m_isStop)
		return;

	m_totalTime = time;

	TCPClientReconnectTimeOperation* opData = (TCPClientReconnectTimeOperation*)fc_malloc(sizeof(TCPClientReconnectTimeOperation));
	new(opData) TCPClientReconnectTimeOperation();

	opData->sessionID = -1;
	opData->time = time;

	pushOperation(TCP_CLI_OP_SET_RECON_TIME, opData, NULL, NULL);
}

void TCPClient::setAutoReconnectBySessionID(unsigned int sessionID, bool isAuto)
{
	if (m_isStop)
		return;

	TCPClientAutoConnectOperation* opData = (TCPClientAutoConnectOperation*)fc_malloc(sizeof(TCPClientAutoConnectOperation));
	new(opData) TCPClientAutoConnectOperation();

	opData->isAuto = isAuto;
	opData->sessionID = sessionID;

	pushOperation(TCP_CLI_OP_SET_AUTO_CONNECT, opData, NULL, NULL);
}

void TCPClient::setAutoReconnectTimeBySessionID(unsigned int sessionID, float time)
{
	if (m_isStop)
		return;

	TCPClientReconnectTimeOperation* opData = (TCPClientReconnectTimeOperation*)fc_malloc(sizeof(TCPClientReconnectTimeOperation));
	new(opData) TCPClientReconnectTimeOperation();

	opData->sessionID = sessionID;
	opData->time = time;

	pushOperation(TCP_CLI_OP_SET_RECON_TIME, opData, NULL, NULL);
}

void TCPClient::clearData()
{
	for (auto & it : m_allSessionMap)
	{
		it.second->session->~TCPSession();
		fc_free(it.second->session);
		fc_free(it.second);
	}
	m_allSessionMap.clear();

	m_msgMutex.lock();
	while (!m_msgQue.empty())
	{
		if (m_msgQue.front().data)
		{
			fc_free(m_msgQue.front().data);
		}
		m_msgQue.pop();
	}
	m_msgMutex.unlock();

	while (!m_operationQue.empty())
	{
		auto & curOperation = m_operationQue.front();
		switch (curOperation.operationType)
		{
		case TCP_CLI_OP_SENDDATA:			// 数据发送
		case TCP_CLI_OP_CONNECT:			// 连接
		case TCP_CLI_OP_SET_AUTO_CONNECT:	//设置自动连接
		case TCP_CLI_OP_SET_RECON_TIME:		//设置重连时间
		{
			if (curOperation.operationData)
			{
				fc_free(curOperation.operationData);
			}
		}break;
		}
		m_operationQue.pop();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void TCPClient::uv_timer_run(uv_timer_t* handle)
{
	TCPClient* c = (TCPClient*)handle->data;

	if (c->m_clientStage == clientStage::START)
	{
		clientSessionData* data = NULL;
		for (auto& it : c->m_allSessionMap)
		{
			data = it.second;
			if (data->connectState == CONNECTSTATE::DISCONNECT && data->reconnect)
			{
				data->curtime = data->curtime + CLIENT_TIMER_DELAY;

				if (data->curtime >= data->totaltime)
				{
					data->curtime = 0.0f;
					if (data->session->getTCPSocket()->connect(data->ip.c_str(), data->port))
					{
						data->connectState = CONNECTSTATE::CONNECTING;
					}
					else
					{
						data->connectState = CONNECTSTATE::DISCONNECT;
					}
				}
			}
		}
	}
	else if(c->m_clientStage == clientStage::CLEAR_SOCKET)
	{
		//检测所有是否已经断开
		bool isConnect = false;
		clientSessionData* data = NULL;
		for (auto& it : c->m_allSessionMap)
		{
			data = it.second;
			if (data->connectState != CONNECTSTATE::DISCONNECT)
			{
				isConnect = true;
				if (data->connectState == CONNECTSTATE::CONNECT)
				{
					data->connectState = CONNECTSTATE::DISCONNECTING;
					data->session->executeDisconnect();
				}
			}
		}
		
		if (!isConnect)
		{
			for (auto & it : c->m_allSessionMap)
			{
				it.second->session->~TCPSession();
				fc_free(it.second->session);
				it.second->~clientSessionData();
				fc_free(it.second);
			}

			c->m_allSessionMap.clear();
			c->m_clientStage = clientStage::WAIT_EXIT;
		}
	}
	else if (c->m_clientStage == clientStage::WAIT_EXIT)
	{
		uv_stop(&c->m_loop);
	}
}

void TCPClient::uv_on_idle_run(uv_idle_t* handle)
{
	TCPClient* c =  (TCPClient*)handle->data;
	c->executeOperation();
	ThreadSleep(1);
}

#if OPEN_UV_THREAD_HEARTBEAT == 1
void TCPClient::uv_heart_timer_callback(uv_timer_t* handle)
{
	TCPClient* s = (TCPClient*)handle->data;
	s->heartRun();
}
#endif

NS_NET_UV_END