#include "KCPServer.h"
#include "KCPUtils.h"

NS_NET_UV_BEGIN

enum
{
	KCP_SVR_OP_STOP_SERVER,	// 停止服务器
	KCP_SVR_OP_SEND_DATA,	// 发送消息给某个会话
	KCP_SVR_OP_DIS_SESSION,	// 断开某个会话
	KCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD,//向主线程发送会话已断开
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////

KCPServer::KCPServer()
	: m_start(false)
	, m_port(0)
	, m_server(NULL)
	, m_isIPV6(false)
	, m_sessionID(0)
{
	m_serverStage = ServerStage::STOP;

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	m_heartTimer = (uv_timer_t*)fc_malloc(sizeof(uv_timer_t));
#endif
}

KCPServer::~KCPServer()
{
	stopServer();
	this->join();
	clearData();

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	fc_free(m_heartTimer);
#endif

	NET_UV_LOG(NET_UV_L_INFO, "KCPServer destroy...");
}

void KCPServer::startServer(const char* ip, int port, bool isIPV6)
{
	if (m_serverStage != ServerStage::STOP)
		return;
	if (m_start)
		return;

	Server::startServer(ip, port, isIPV6);

	m_ip = ip;
	m_port = port;
	m_isIPV6 = isIPV6;

	m_start = true;
	m_serverStage = ServerStage::START;

	NET_UV_LOG(NET_UV_L_INFO, "KCPServer start-up...");
	this->startThread();
}

bool KCPServer::stopServer()
{
	if (!m_start)
		return false;
	m_start = false;
	pushOperation(KCP_SVR_OP_STOP_SERVER, NULL, NULL, 0U);
	return true;
}

void KCPServer::updateFrame()
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

	bool closeServerTag = false;
	while (!m_msgDispatchQue.empty())
	{
		const NetThreadMsg_S& Msg = m_msgDispatchQue.front();

		switch (Msg.msgType)
		{
		case NetThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case NetThreadMsgType::START_SERVER_SUC:
		{
			if (m_startCall != nullptr)
			{
				m_startCall(this, true);
			}
		}break;
		case NetThreadMsgType::START_SERVER_FAIL:
		{
			if (m_startCall != nullptr)
			{
				m_startCall(this, false);
			}
		}break;
		case NetThreadMsgType::NEW_CONNECT:
		{
			m_newConnectCall(this, Msg.pSession);
		}break;
		case NetThreadMsgType::DIS_CONNECT:
		{
			m_disconnectCall(this, Msg.pSession);
			pushOperation(KCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD, NULL, 0, Msg.pSession->getSessionID());
		}break;
		case NetThreadMsgType::EXIT_LOOP:
		{
			closeServerTag = true;
		}break;
		default:
			break;
		}
		m_msgDispatchQue.pop();
	}
	if (closeServerTag && m_closeCall != nullptr)
	{
		m_closeCall(this);
	}
}

void KCPServer::send(Session* session, char* data, unsigned int len)
{
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
		pushOperation(KCP_SVR_OP_SEND_DATA, (bufArr + i)->base, (bufArr + i)->len, session->getSessionID());
	}
	fc_free(bufArr);
}

void KCPServer::disconnect(Session* session)
{
	pushOperation(KCP_SVR_OP_DIS_SESSION, NULL, 0, session->getSessionID());
}

bool KCPServer::isCloseFinish()
{
	return (m_serverStage == ServerStage::STOP);
}

void KCPServer::run()
{
	int r = uv_loop_init(&m_loop);
	CHECK_UV_ASSERT(r);

	r = uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	CHECK_UV_ASSERT(r);

	uv_idle_start(&m_idle, uv_on_idle_run);

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1 
	m_heartTimer->data = this;
	uv_timer_init(&m_loop, m_heartTimer);
	uv_timer_start(m_heartTimer, KCPServer::uv_heart_timer_callback, KCP_HEARTBEAT_TIMER_DELAY, KCP_HEARTBEAT_TIMER_DELAY);
#endif

	m_server = (KCPSocket*)fc_malloc(sizeof(KCPSocket));
	if (m_server == NULL)
	{
		m_serverStage = ServerStage::STOP;
		pushThreadMsg(NetThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}
	new (m_server) KCPSocket(&m_loop);
	m_server->setCloseCallback(std::bind(&KCPServer::onServerSocketClose, this, std::placeholders::_1));
	m_server->setNewConnectionCallback(std::bind(&KCPServer::onNewConnect, this, std::placeholders::_1));
	m_server->setConnectFilterCallback(std::bind(&KCPServer::onServerSocketConnectFilter, this, std::placeholders::_1));

	bool suc = false;
	if (m_isIPV6)
	{
		suc = m_server->bind6(m_ip.c_str(), m_port);
	}
	else
	{
		suc = m_server->bind(m_ip.c_str(), m_port);
	}

	if (!suc)
	{
		m_server->~KCPSocket();
		fc_free(m_server);
		m_server = NULL;

		m_serverStage = ServerStage::STOP;
		pushThreadMsg(NetThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}

	suc = m_server->listen();
	if (!suc)
	{
		m_server->~KCPSocket();
		fc_free(m_server);
		m_server = NULL;

		m_serverStage = ServerStage::STOP;
		pushThreadMsg(NetThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}
	m_serverStage = ServerStage::RUN;
	pushThreadMsg(NetThreadMsgType::START_SERVER_SUC, NULL);

	uv_run(&m_loop, UV_RUN_DEFAULT);

	m_server->~KCPSocket();
	fc_free(m_server);
	m_server = NULL;

	uv_loop_close(&m_loop);

	m_serverStage = ServerStage::STOP;
	pushThreadMsg(NetThreadMsgType::EXIT_LOOP, NULL);
}


void KCPServer::onNewConnect(Socket* socket)
{
	if (socket != NULL)
	{
		KCPSession* session = KCPSession::createSession(this, (KCPSocket*)socket, std::bind(&KCPServer::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_ERROR, "服务器创建新会话失败,可能是内存不足");
		}
		else
		{
			session->setIsOnline(true);
			session->setSessionClose(std::bind(&KCPServer::onSessionClose, this, std::placeholders::_1));
			session->setSessionID(((KCPSocket*)socket)->getConv());
			addNewSession(session);
		}
	}
	else
	{
		NET_UV_LOG(NET_UV_L_ERROR, "接受新连接失败");
	}
}

void KCPServer::onServerSocketClose(Socket* svr)
{
	m_serverStage = ServerStage::CLEAR;
}

bool KCPServer::onServerSocketConnectFilter(const struct sockaddr* addr)
{
	return true;
}

void KCPServer::pushThreadMsg(NetThreadMsgType type, Session* session, char* data, unsigned int len, const NetMsgTag& tag)
{
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	if (type == NetThreadMsgType::RECV_DATA)
	{
		auto it = m_allSession.find(session->getSessionID());
		if (it != m_allSession.end())
		{
			it->second.curHeartCount = KCP_HEARTBEAT_COUNT_RESET_VALUE_SERVER;
			it->second.curHeartTime = 0;
			if (tag == NetMsgTag::MT_HEARTBEAT)
			{
				if (len == KCP_HEARTBEAT_MSG_SIZE)
				{
					if (*((char*)data) == KCP_HEARTBEAT_MSG_C2S)
					{
						unsigned int sendlen = 0;
						char* senddata = kcp_packageHeartMsgData(KCP_HEARTBEAT_RET_MSG_S2C, &sendlen);
						it->second.session->executeSend(senddata, sendlen);
						NET_UV_LOG(NET_UV_L_HEART, "recv heart c->s");
					}
				}
				else// 不合法心跳
				{
					it->second.session->disconnect();
				}
				fc_free(data);
				return;
			}
		}
	}
#endif

	NetThreadMsg_S msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;
	msg.tag = tag;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

void KCPServer::addNewSession(KCPSession* session)
{
	if (session == NULL)
	{
		return;
	}
	kcpSessionData data;
	data.isInvalid = false;
	data.session = session;
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	data.curHeartCount = KCP_HEARTBEAT_COUNT_RESET_VALUE_SERVER;
	data.curHeartTime = 0;
#endif
	session->setSessionID(m_sessionID);
	m_allSession.insert(std::make_pair(m_sessionID, data));

	m_sessionID++;

	pushThreadMsg(NetThreadMsgType::NEW_CONNECT, session);
	//NET_UV_LOG(NET_UV_L_INFO, "[%p] add", session);
}

void KCPServer::onSessionClose(Session* session)
{
	if (session == NULL)
	{
		return;
	}
	auto it = m_allSession.find(session->getSessionID());
	if (it != m_allSession.end())
	{
		it->second.isInvalid = true;
	}

	pushThreadMsg(NetThreadMsgType::DIS_CONNECT, session);
	//NET_UV_LOG(NET_UV_L_INFO, "[%p] remove", session);
}

void KCPServer::onSessionRecvData(Session* session, char* data, unsigned int len, NetMsgTag tag)
{
	pushThreadMsg(NetThreadMsgType::RECV_DATA, session, data, len, tag);
}


void KCPServer::onServerIdleRun()
{
	executeOperation();
	switch (m_serverStage)
	{
	case KCPServer::ServerStage::CLEAR:
	{
		for (auto& it : m_allSession)
		{
			if (!it.second.isInvalid)
			{
				it.second.session->executeDisconnect();
			}
		}
		m_serverStage = ServerStage::WAIT_SESSION_CLOSE;
	}
	break;
	case KCPServer::ServerStage::WAIT_SESSION_CLOSE:
	{
		if (m_allSession.empty())
		{
			uv_stop(&m_loop);
			m_serverStage = ServerStage::STOP;
		}
	}
	break;
	default:
		break;
	}
	ThreadSleep(1);
}

void KCPServer::executeOperation()
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
		case KCP_SVR_OP_SEND_DATA:		// 数据发送
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->executeSend((char*)curOperation.operationData, curOperation.operationDataLen);
			}
			else//该会话已失效
			{
				fc_free(curOperation.operationData);
			}
		}break;
		case KCP_SVR_OP_DIS_SESSION:	// 断开连接
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->executeDisconnect();
			}
		}break;
		case KCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD:
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->~KCPSession();
				fc_free(it->second.session);
				it = m_allSession.erase(it);
			}
		}break;
		case KCP_SVR_OP_STOP_SERVER:
		{
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1 
			uv_timer_stop(m_heartTimer);
#endif
			for (auto & it : m_allSession)
			{
				if (!it.second.isInvalid)
				{
					it.second.session->executeDisconnect();
				}
			}
			m_server->disconnect();
			m_serverStage = ServerStage::WAIT_CLOSE_SERVER_SOCKET;
		}break;
		default:
			break;
		}
		m_operationDispatchQue.pop();
	}
}

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
void KCPServer::heartRun()
{
	for (auto &it : m_allSession)
	{
		if (!it.second.isInvalid)
		{
			it.second.curHeartTime += KCP_HEARTBEAT_TIMER_DELAY;
			if (it.second.curHeartTime >= KCP_HEARTBEAT_CHECK_DELAY)
			{
				it.second.curHeartTime = 0;
				it.second.curHeartCount++;
				if (it.second.curHeartCount > 0)
				{
					if (it.second.curHeartCount > KCP_HEARTBEAT_MAX_COUNT_SERVER)
					{
						it.second.curHeartCount = 0;
						it.second.session->executeDisconnect();
					}
					else
					{
						unsigned int sendlen = 0;
						char* senddata = kcp_packageHeartMsgData(KCP_HEARTBEAT_MSG_S2C, &sendlen);
						it.second.session->executeSend(senddata, sendlen);
						NET_UV_LOG(NET_UV_L_HEART, "send heart s->c");
					}
				}
			}
		}
	}
}
#endif


void KCPServer::clearData()
{
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
		if (m_operationQue.front().operationType == KCP_SVR_OP_SEND_DATA)
		{
			fc_free(m_operationQue.front().operationData);
		}
		m_operationQue.pop();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void KCPServer::uv_on_idle_run(uv_idle_t* handle)
{
	KCPServer* s = (KCPServer*)handle->data;
	s->onServerIdleRun();
}

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
void KCPServer::uv_heart_timer_callback(uv_timer_t* handle)
{
	KCPServer* s = (KCPServer*)handle->data;
	s->heartRun();
}
#endif

NS_NET_UV_END