#include "KCPClient.h"
#include "KCPCommon.h"
#include "KCPUtils.h"

NS_NET_UV_BEGIN

//断线重连定时器检测间隔
#define KCP_CLIENT_TIMER_DELAY (0.1f)


enum
{
	KCP_CLI_OP_CONNECT,			//	连接
	KCP_CLI_OP_SENDDATA,		// 发送数据
	KCP_CLI_OP_DISCONNECT,		// 断开连接
	KCP_CLI_OP_SET_AUTO_CONNECT,//设置自动连接
	KCP_CLI_OP_SET_RECON_TIME,	//设置重连时间
	KCP_CLI_OP_CLIENT_CLOSE,	//客户端退出
	KCP_CLI_OP_REMOVE_SESSION,	//移除会话命令
	KCP_CLI_OP_DELETE_SESSION,	//删除会话
};

// 连接操作
struct KCPClientConnectOperation
{
	KCPClientConnectOperation() {}
	~KCPClientConnectOperation() {}
	std::string ip;
	unsigned int port;
	unsigned int sessionID;
};

// 设置自动连接操作
struct KCPClientAutoConnectOperation
{
	KCPClientAutoConnectOperation() {}
	~KCPClientAutoConnectOperation() {}
	bool isAuto;
	unsigned int sessionID;
};

// 设置重连时间操作
struct KCPClientReconnectTimeOperation
{
	KCPClientReconnectTimeOperation() {}
	~KCPClientReconnectTimeOperation() {}
	float time;
	unsigned int sessionID;
};




//////////////////////////////////////////////////////////////////////////////////
KCPClient::KCPClient()
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
	uv_timer_start(&m_timer, uv_timer_run, (uint64_t)(KCP_CLIENT_TIMER_DELAY * 1000), (uint64_t)(KCP_CLIENT_TIMER_DELAY * 1000));

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	m_heartTimer.data = this;
	uv_timer_init(&m_loop, &m_heartTimer);
	uv_timer_start(&m_heartTimer, KCPClient::uv_heart_timer_callback, KCP_HEARTBEAT_TIMER_DELAY, KCP_HEARTBEAT_TIMER_DELAY);
#endif
	this->startThread();
}

KCPClient::~KCPClient()
{
	closeClient();
	this->join();
	clearData();
}


void KCPClient::connect(const char* ip, unsigned int port, unsigned int sessionId)
{
	if (m_isStop)
		return;

	assert(ip != NULL);

	KCPClientConnectOperation* opData = (KCPClientConnectOperation*)fc_malloc(sizeof(KCPClientConnectOperation));
	new (opData)KCPClientConnectOperation();

	opData->ip = ip;
	opData->port = port;
	opData->sessionID = sessionId;
	pushOperation(KCP_CLI_OP_CONNECT, opData, 0U, 0U);
}

void KCPClient::disconnect(unsigned int sessionId)
{
	if (m_isStop)
		return;

	pushOperation(KCP_CLI_OP_DISCONNECT, NULL, 0U, sessionId);
}

void KCPClient::closeClient()
{
	if (m_isStop)
		return;
	m_isStop = true;
	pushOperation(KCP_CLI_OP_CLIENT_CLOSE, NULL, 0U, 0U);
}

void KCPClient::updateFrame()
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

	bool closeClientTag = false;
	while (!m_msgDispatchQue.empty())
	{
		const NetThreadMsg_C& Msg = m_msgDispatchQue.front();
		switch (Msg.msgType)
		{
		case NetThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case NetThreadMsgType::CONNECT_FAIL:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 0);
			}
		}break;
		case NetThreadMsgType::CONNECT:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 1);
			}
		}break;
		case NetThreadMsgType::CONNECT_TIMOUT:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 2);
			}
		}break;
		case NetThreadMsgType::CONNECT_SESSIONID_EXIST:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 3);
			}
		}break;
		case NetThreadMsgType::DIS_CONNECT:
		{
			if (m_disconnectCall != nullptr)
			{
				m_disconnectCall(this, Msg.pSession);
			}
		}break;
		case NetThreadMsgType::EXIT_LOOP:
		{
			closeClientTag = true;
		}break;
		case NetThreadMsgType::REMOVE_SESSION:
		{
			if (m_removeSessionCall != nullptr)
			{
				m_removeSessionCall(this, Msg.pSession);
			}
			pushOperation(KCP_CLI_OP_DELETE_SESSION, NULL, 0U, Msg.pSession->getSessionID());
		}break;
		default:
			break;
		}
		m_msgDispatchQue.pop();
	}
	if (closeClientTag && m_clientCloseCall != nullptr)
	{
		m_clientCloseCall(this);
	}
}

void KCPClient::send(unsigned int sessionId, char* data, unsigned int len)
{
	if (m_isStop)
		return;

	if (data == 0 || len <= 0)
		return;
	int bufCount = 0;
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	uv_buf_t* bufArr = kcp_packageData(data, len, &bufCount, NetMsgTag::MT_DEFAULT);
#else
	uv_buf_t* bufArr = kcp_packageData(data, len, &bufCount);
#endif
	if (bufArr == NULL)
		return;

	for (int i = 0; i < bufCount; ++i)
	{
		pushOperation(KCP_CLI_OP_SENDDATA, (bufArr + i)->base, (bufArr + i)->len, sessionId);
	}
	fc_free(bufArr);
}

void KCPClient::removeSession(unsigned int sessionId)
{
	pushOperation(KCP_CLI_OP_REMOVE_SESSION, NULL, 0U, sessionId);
}

/// SessionManager
void KCPClient::send(Session* session, char* data, unsigned int len)
{
	send(session->getSessionID(), data, len);
}

void KCPClient::disconnect(Session* session)
{
	disconnect(session->getSessionID());
}

/// KCPClient
bool KCPClient::isCloseFinish()
{
	return (m_clientStage == clientStage::STOP);
}

void KCPClient::setAutoReconnect(bool isAuto)
{
	if (m_isStop)
		return;

	m_reconnect = isAuto;

	KCPClientAutoConnectOperation* opData = (KCPClientAutoConnectOperation*)fc_malloc(sizeof(KCPClientAutoConnectOperation));
	new(opData) KCPClientAutoConnectOperation();

	opData->isAuto = isAuto;
	opData->sessionID = -1;

	pushOperation(KCP_CLI_OP_SET_AUTO_CONNECT, opData, NULL, NULL);
}

void KCPClient::setAutoReconnectTime(float time)
{
	if (m_isStop)
		return;

	m_totalTime = time;

	KCPClientReconnectTimeOperation* opData = (KCPClientReconnectTimeOperation*)fc_malloc(sizeof(KCPClientReconnectTimeOperation));
	new(opData) KCPClientReconnectTimeOperation();

	opData->sessionID = -1;
	opData->time = time;

	pushOperation(KCP_CLI_OP_SET_RECON_TIME, opData, NULL, NULL);
}

void KCPClient::setAutoReconnectBySessionID(unsigned int sessionID, bool isAuto)
{
	if (m_isStop)
		return;

	KCPClientAutoConnectOperation* opData = (KCPClientAutoConnectOperation*)fc_malloc(sizeof(KCPClientAutoConnectOperation));
	new(opData) KCPClientAutoConnectOperation();

	opData->isAuto = isAuto;
	opData->sessionID = sessionID;

	pushOperation(KCP_CLI_OP_SET_AUTO_CONNECT, opData, NULL, NULL);
}

void KCPClient::setAutoReconnectTimeBySessionID(unsigned int sessionID, float time)
{
	if (m_isStop)
		return;

	KCPClientReconnectTimeOperation* opData = (KCPClientReconnectTimeOperation*)fc_malloc(sizeof(KCPClientReconnectTimeOperation));
	new(opData) KCPClientReconnectTimeOperation();

	opData->sessionID = sessionID;
	opData->time = time;

	pushOperation(KCP_CLI_OP_SET_RECON_TIME, opData, NULL, NULL);
}

/// Runnable
void KCPClient::run()
{
	uv_run(&m_loop, UV_RUN_DEFAULT);

	uv_loop_close(&m_loop);

	m_clientStage = clientStage::STOP;

	this->pushThreadMsg(NetThreadMsgType::EXIT_LOOP, NULL);
}

/// SessionManager
void KCPClient::executeOperation()
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
		case KCP_CLI_OP_SENDDATA:		// 数据发送
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData && !sessionData->removeTag)
			{
				sessionData->session->executeSend((char*)curOperation.operationData, curOperation.operationDataLen);
			}
			else
			{
				fc_free(curOperation.operationData);
			}
		}break;
		case KCP_CLI_OP_DISCONNECT:	// 断开连接
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData->connectState == CONNECT)
			{
				sessionData->connectState = DISCONNECTING;
				sessionData->session->executeDisconnect();
			}
		}break;
		case KCP_CLI_OP_CONNECT:	// 连接
		{
			if (curOperation.operationData)
			{
				createNewConnect(curOperation.operationData);
				((KCPClientConnectOperation*)curOperation.operationData)->~KCPClientConnectOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		case KCP_CLI_OP_SET_AUTO_CONNECT: //设置自动连接
		{
			if (curOperation.operationData)
			{
				KCPClientAutoConnectOperation* opData = (KCPClientAutoConnectOperation*)curOperation.operationData;
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
				opData->~KCPClientAutoConnectOperation();
				fc_free(opData);
			}
		}break;
		case KCP_CLI_OP_SET_RECON_TIME: //设置重连时间
		{
			if (curOperation.operationData)
			{
				KCPClientReconnectTimeOperation* opData = (KCPClientReconnectTimeOperation*)curOperation.operationData;
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
				opData->~KCPClientReconnectTimeOperation();
				fc_free(opData);
			}
		}break;
		case KCP_CLI_OP_CLIENT_CLOSE://客户端关闭
		{
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
			uv_timer_stop(&m_heartTimer);
#endif
			m_clientStage = clientStage::CLEAR_SESSION;
		}break;
		case KCP_CLI_OP_REMOVE_SESSION:
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			if (sessionData)
			{
				if (sessionData->connectState != DISCONNECT)
				{
					sessionData->removeTag = true;
					sessionData->session->executeDisconnect();
				}
				else
				{
					if (!sessionData->removeTag)
					{
						sessionData->removeTag = true;
						pushThreadMsg(NetThreadMsgType::REMOVE_SESSION, sessionData->session);
					}
				}
			}
		}break;
		case KCP_CLI_OP_DELETE_SESSION://删除会话
		{
			auto it = m_allSessionMap.find(curOperation.sessionID);
			if (it != m_allSessionMap.end() && it->second->removeTag)
			{
				it->second->session->~KCPSession();
				fc_free(it->second->session);
				it->second->~clientSessionData();
				fc_free(it->second);
				m_allSessionMap.erase(it);
			}
		}break;
		default:
			break;
		}
		m_operationDispatchQue.pop();
	}
}

/// KCPClient
void KCPClient::onSocketConnect(Socket* socket, int status)
{
	Session* pSession = NULL;
	bool isSuc = (status == 1);

	for (auto& it : m_allSessionMap)
	{
		if (it.second->session->getKCPSocket() == socket)
		{
			pSession = it.second->session;
			it.second->session->setIsOnline(isSuc);
			it.second->connectState = isSuc ? CONNECTSTATE::CONNECT : CONNECTSTATE::DISCONNECT;

			if (m_clientStage != clientStage::START)
			{
				it.second->session->executeDisconnect();
				pSession = NULL;
			}
			else
			{
				if (it.second->removeTag)
				{
					it.second->session->executeDisconnect();
					pSession = NULL;
				}
			}
			break;
		}
	}

	if (pSession)
	{
		if (status == 0)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT_FAIL, pSession);
		}
		else if (status == 1)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT, pSession);
		}
		else if (status == 2)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT_TIMOUT, pSession);
		}
	}
}

void KCPClient::onSessionClose(Session* session)
{
	auto sessionData = getClientSessionDataBySession(session);
	if (sessionData)
	{
		sessionData->connectState = CONNECTSTATE::DISCONNECT;
		pushThreadMsg(NetThreadMsgType::DIS_CONNECT, sessionData->session);

		if (sessionData->removeTag)
		{
			pushThreadMsg(NetThreadMsgType::REMOVE_SESSION, sessionData->session);
		}
	}
}

void KCPClient::createNewConnect(void* data)
{
	if (data == NULL)
		return;
	KCPClientConnectOperation* opData = (KCPClientConnectOperation*)data;

	auto it = m_allSessionMap.find(opData->sessionID);
	if (it != m_allSessionMap.end())
	{
		if (it->second->removeTag)
			return;

		//对比端口和IP是否一致
		if (strcmp(opData->ip.c_str(), it->second->ip.c_str()) != 0 && opData->port != it->second->port)
		{
			pushThreadMsg(NetThreadMsgType::CONNECT_SESSIONID_EXIST, NULL);
			return;
		}

		if (it->second->connectState == CONNECTSTATE::DISCONNECT)
		{
			if (it->second->session->executeConnect(opData->ip.c_str(), opData->port))
			{
				it->second->connectState = CONNECTSTATE::CONNECTING;
			}
			else
			{
				it->second->connectState = CONNECTSTATE::DISCONNECT;
				it->second->session->executeDisconnect();
				pushThreadMsg(NetThreadMsgType::CONNECT_FAIL, it->second->session);
			}
		}
	}
	else
	{
		KCPSocket* socket = (KCPSocket*)fc_malloc(sizeof(KCPSocket));
		new (socket) KCPSocket(&m_loop);
		socket->setConnectCallback(std::bind(&KCPClient::onSocketConnect, this, std::placeholders::_1, std::placeholders::_2));

		KCPSession* session = KCPSession::createSession(this, socket, std::bind(&KCPClient::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_FATAL, "创建会话失败，可能是内存不足!!!");
			return;
		}
		session->setSessionClose(std::bind(&KCPClient::onSessionClose, this, std::placeholders::_1));
		session->setSessionID(opData->sessionID);
		session->setIsOnline(false);


		clientSessionData* cs = (clientSessionData*)fc_malloc(sizeof(clientSessionData));
		new (cs) clientSessionData();
		cs->removeTag = false;
		cs->ip = opData->ip;
		cs->port = opData->port;
		cs->session = session;
		cs->curtime = 0;
		cs->reconnect = m_reconnect;
		cs->totaltime = m_totalTime;
		cs->connectState = CONNECTSTATE::CONNECTING;
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
		cs->curHeartCount = KCP_HEARTBEAT_COUNT_RESET_VALUE_CLIENT;
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
			pushThreadMsg(NetThreadMsgType::CONNECT_FAIL, session);
		}
	}
}

void KCPClient::onSessionRecvData(Session* session, char* data, unsigned int len, NetMsgTag tag)
{
	pushThreadMsg(NetThreadMsgType::RECV_DATA, session, data, len, tag);
}


void KCPClient::pushThreadMsg(NetThreadMsgType type, Session* session, char* data, unsigned int len, NetMsgTag tag)
{
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	if (type == NetThreadMsgType::RECV_DATA)
	{
		auto it = m_allSessionMap.find(session->getSessionID());
		if (it != m_allSessionMap.end())
		{
			auto clientdata = it->second;
			clientdata->curHeartCount = KCP_HEARTBEAT_COUNT_RESET_VALUE_CLIENT;
			clientdata->curHeartTime = 0;
			if (tag == NetMsgTag::MT_HEARTBEAT)
			{
				if (len == KCP_HEARTBEAT_MSG_SIZE)
				{
					if (*((char*)data) == KCP_HEARTBEAT_MSG_S2C)
					{
						NET_UV_LOG(NET_UV_L_HEART, "recv heart s->c");
						unsigned int sendlen = 0;
						char* senddata = kcp_packageHeartMsgData(KCP_HEARTBEAT_RET_MSG_C2S, &sendlen);
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

	NetThreadMsg_C msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;
	msg.tag = tag;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

KCPClient::clientSessionData* KCPClient::getClientSessionDataBySessionId(unsigned int sessionId)
{
	auto it = m_allSessionMap.find(sessionId);
	if (it != m_allSessionMap.end())
		return it->second;
	return NULL;
}

KCPClient::clientSessionData* KCPClient::getClientSessionDataBySession(Session* session)
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

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
void KCPClient::heartRun()
{
	for (auto &it : m_allSessionMap)
	{
		auto clientdata = it.second;

		if (clientdata->connectState != CONNECT)
			continue;

		clientdata->curHeartTime += KCP_HEARTBEAT_TIMER_DELAY;
		if (clientdata->curHeartTime > KCP_HEARTBEAT_CHECK_DELAY)
		{
			clientdata->curHeartTime = 0;
			clientdata->curHeartCount++;
			if (clientdata->curHeartCount > 0)
			{
				if (clientdata->curHeartCount > KCP_HEARTBEAT_MAX_COUNT_CLIENT)
				{
					clientdata->curHeartCount = 0;
					clientdata->session->executeDisconnect();
				}
				else
				{
					unsigned int sendlen = 0;
					char* senddata = kcp_packageHeartMsgData(KCP_HEARTBEAT_MSG_C2S, &sendlen);
					clientdata->session->executeSend(senddata, sendlen);
					NET_UV_LOG(NET_UV_L_HEART, "send heart c->s");
				}
			}
		}
	}
}
#endif


void KCPClient::clearData()
{
	for (auto & it : m_allSessionMap)
	{
		it.second->session->~KCPSession();
		fc_free(it.second->session);
		it.second->~clientSessionData();
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
		case KCP_CLI_OP_SENDDATA:			// 数据发送
		{
			if (curOperation.operationData)
			{
				fc_free(curOperation.operationData);
			}
		}break;
		case KCP_CLI_OP_CONNECT:			// 连接
		{
			if (curOperation.operationData)
			{
				((KCPClientConnectOperation*)curOperation.operationData)->~KCPClientConnectOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		case KCP_CLI_OP_SET_AUTO_CONNECT:	//设置自动连接
		{
			if (curOperation.operationData)
			{
				((KCPClientAutoConnectOperation*)curOperation.operationData)->~KCPClientAutoConnectOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		case KCP_CLI_OP_SET_RECON_TIME:		//设置重连时间
		{
			if (curOperation.operationData)
			{
				((KCPClientReconnectTimeOperation*)curOperation.operationData)->~KCPClientReconnectTimeOperation();
				fc_free(curOperation.operationData);
			}
		}break;
		}
		m_operationQue.pop();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void KCPClient::uv_timer_run(uv_timer_t* handle)
{
	KCPClient* c = (KCPClient*)handle->data;

	if (c->m_clientStage == clientStage::START)
	{
		clientSessionData* data = NULL;
		for (auto& it : c->m_allSessionMap)
		{
			data = it.second;
			if (data->connectState == CONNECTSTATE::DISCONNECT && data->reconnect && data->removeTag == false)
			{
				data->curtime = data->curtime + KCP_CLIENT_TIMER_DELAY;

				if (data->curtime >= data->totaltime)
				{
					data->curtime = 0.0f;
					if (data->session->executeConnect(data->ip.c_str(), data->port))
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
	else if (c->m_clientStage == clientStage::CLEAR_SESSION)
	{
		clientSessionData* data = NULL;
		for (auto& it : c->m_allSessionMap)
		{
			data = it.second;

			if (data->connectState != DISCONNECT)
			{
				data->removeTag = true;
				data->session->executeDisconnect();
			}
			else
			{
				if (!data->removeTag)
				{
					data->removeTag = true;
					c->pushThreadMsg(NetThreadMsgType::REMOVE_SESSION, data->session);
				}
			}
		}
		if (c->m_allSessionMap.empty())
		{
			c->m_clientStage = clientStage::WAIT_EXIT;
		}
	}
	else if (c->m_clientStage == clientStage::WAIT_EXIT)
	{
		uv_stop(&c->m_loop);
	}
}

void KCPClient::uv_on_idle_run(uv_idle_t* handle)
{
	KCPClient* c = (KCPClient*)handle->data;
	c->executeOperation();
	ThreadSleep(1);
}

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
void KCPClient::uv_heart_timer_callback(uv_timer_t* handle)
{
	KCPClient* s = (KCPClient*)handle->data;
	s->heartRun();
}
#endif

NS_NET_UV_END