#include "TCPServer.h"
#include "TCPUtils.h"

NS_NET_UV_BEGIN

enum 
{
	TCP_SVR_OP_STOP_SERVER,	// 停止服务器
	TCP_SVR_OP_SEND_DATA,	// 发送消息给某个会话
	TCP_SVR_OP_DIS_SESSION,	// 断开某个会话
	TCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD,//向主线程发送会话已断开
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////

TCPServer::TCPServer()
	: m_start(false)
	, m_port(0)
	, m_server(NULL)
	, m_isIPV6(false)
	, m_sessionID(0)
{
	m_serverStage = ServerStage::STOP;
	
#if OPEN_UV_THREAD_HEARTBEAT == 1
	m_heartTimer = (uv_timer_t*)fc_malloc(sizeof(uv_timer_t));
#endif
}

TCPServer::~TCPServer()
{
	stopServer();
	this->join();
	clearData();
	
#if OPEN_UV_THREAD_HEARTBEAT == 1
	fc_free(m_heartTimer);
#endif

	NET_UV_LOG(NET_UV_L_INFO, "TCPServer destroy...");
}

void TCPServer::startServer(const char* ip, int port, bool isIPV6)
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

	NET_UV_LOG(NET_UV_L_INFO, "TCPServer start-up...");
	this->startThread();
}

bool TCPServer::stopServer()
{
	if (!m_start)
		return false;
	m_start = false;
	pushOperation(TCP_SVR_OP_STOP_SERVER, NULL, NULL, 0U);
	return true;
}

void TCPServer::updateFrame()
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
		const TCPThreadMsg_S& Msg = m_msgDispatchQue.front();

		switch (Msg.msgType)
		{
		case TCPThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case TCPThreadMsgType::START_SERVER_SUC:
		{
			if (m_startCall != nullptr)
			{
				m_startCall(this, true);
			}
		}break;
		case TCPThreadMsgType::START_SERVER_FAIL:
		{
			if (m_startCall != nullptr)
			{
				m_startCall(this, false);
			}
		}break;
		case TCPThreadMsgType::NEW_CONNECT:
		{
			m_newConnectCall(this, Msg.pSession);
		}break;
		case TCPThreadMsgType::DIS_CONNECT:
		{
			m_disconnectCall(this, Msg.pSession);
			pushOperation(TCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD, NULL, 0, Msg.pSession->getSessionID());
		}break;
		case TCPThreadMsgType::EXIT_LOOP:
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

void TCPServer::send(Session* session, char* data, unsigned int len)
{
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
		pushOperation(TCP_SVR_OP_SEND_DATA, (bufArr + i)->base, (bufArr + i)->len, session->getSessionID());
	}
	fc_free(bufArr);
}

void TCPServer::disconnect(Session* session)
{
	pushOperation(TCP_SVR_OP_DIS_SESSION, NULL, 0, session->getSessionID());
}

bool TCPServer::isCloseFinish()
{
	return (m_serverStage == ServerStage::STOP);
}

void TCPServer::run()
{
	int r = uv_loop_init(&m_loop);
	CHECK_UV_ASSERT(r);

	r = uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	CHECK_UV_ASSERT(r);

	uv_idle_start(&m_idle, uv_on_idle_run);
	
#if OPEN_UV_THREAD_HEARTBEAT == 1 
	m_heartTimer->data = this;
	uv_timer_init(&m_loop, m_heartTimer);
	uv_timer_start(m_heartTimer, TCPServer::uv_heart_timer_callback, HEARTBEAT_TIMER_DELAY, HEARTBEAT_TIMER_DELAY);
#endif

	m_server = (TCPSocket*)fc_malloc(sizeof(TCPSocket));
	if (m_server == NULL)
	{
		m_serverStage = ServerStage::STOP;
		pushThreadMsg(TCPThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}
	new (m_server) TCPSocket(&m_loop);
	m_server->setCloseCallback(std::bind(&TCPServer::onServerSocketClose, this, std::placeholders::_1));
	m_server->setNewConnectionCallback(std::bind(&TCPServer::onNewConnect, this, std::placeholders::_1, std::placeholders::_2));

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
		m_server->~TCPSocket();
		fc_free(m_server);
		m_server = NULL;

		m_serverStage = ServerStage::STOP;
		pushThreadMsg(TCPThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}

	suc = m_server->listen();
	if (!suc)
	{
		m_server->~TCPSocket();
		fc_free(m_server);
		m_server = NULL;

		m_serverStage = ServerStage::STOP;
		pushThreadMsg(TCPThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}
	m_serverStage = ServerStage::RUN;
	pushThreadMsg(TCPThreadMsgType::START_SERVER_SUC, NULL);

	uv_run(&m_loop, UV_RUN_DEFAULT);

	uv_loop_close(&m_loop);
	
	m_server->~TCPSocket();
	fc_free(m_server);
	m_server = NULL;
	
	m_serverStage = ServerStage::STOP;
	pushThreadMsg(TCPThreadMsgType::EXIT_LOOP, NULL);
}


void TCPServer::onNewConnect(uv_stream_t* server, int status)
{
	TCPSocket* client = m_server->accept(server, status);
	if (client != NULL)
	{
#if OPEN_UV_THREAD_HEARTBEAT == 1
		TCPSession* session = TCPSession::createSession(this, client, std::bind(&TCPServer::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
#else
		TCPSession* session = TCPSession::createSession(this, client, std::bind(&TCPServer::onSessionRecvData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
#endif
		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_ERROR, "服务器创建新会话失败,可能是内存不足");
		}
		else
		{
			session->setIsOnline(true);
			session->setSessionClose(std::bind(&TCPServer::onSessionClose, this, std::placeholders::_1));
			addNewSession(session);
		}
	}
	else
	{
		NET_UV_LOG(NET_UV_L_ERROR, "接受新连接失败");
	}
}

void TCPServer::onServerSocketClose(Socket* svr)
{
	m_serverStage = ServerStage::CLEAR;
}

void TCPServer::pushThreadMsg(TCPThreadMsgType type, Session* session, char* data, unsigned int len, const TCPMsgTag& tag)
{
#if OPEN_UV_THREAD_HEARTBEAT == 1
	if (type == TCPThreadMsgType::RECV_DATA)
	{
		auto it = m_allSession.find(session->getSessionID());
		if (it != m_allSession.end())
		{
			it->second.curHeartCount = HEARTBEAT_COUNT_RESET_VALUE_SERVER;
			it->second.curHeartTime = 0;
			if (tag == TCPMsgTag::MT_HEARTBEAT)
			{
				if (len == HEARTBEAT_MSG_SIZE)
				{
					if (*((char*)data) == HEARTBEAT_MSG_C2S)
					{
						unsigned int sendlen = 0;
						char* senddata = packageHeartMsgData(HEARTBEAT_RET_MSG_S2C, &sendlen);
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

	TCPThreadMsg_S msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;
	msg.tag = tag;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

void TCPServer::addNewSession(TCPSession* session)
{
	if (session == NULL)
	{
		return;
	}
	tcpSessionData data;
	data.isInvalid = false;
	data.session = session;
#if OPEN_UV_THREAD_HEARTBEAT == 1
	data.curHeartCount = HEARTBEAT_COUNT_RESET_VALUE_SERVER;
	data.curHeartTime = 0;
#endif
	session->setSessionID(m_sessionID);
	m_allSession.insert(std::make_pair(m_sessionID, data));

	m_sessionID++;

	pushThreadMsg(TCPThreadMsgType::NEW_CONNECT, session);
	//NET_UV_LOG(NET_UV_L_INFO, "[%p] add", session);
}

void TCPServer::onSessionClose(Session* session)
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

	pushThreadMsg(TCPThreadMsgType::DIS_CONNECT, session);
	//NET_UV_LOG(NET_UV_L_INFO, "[%p] remove", session);
}

#if OPEN_UV_THREAD_HEARTBEAT == 1
void TCPServer::onSessionRecvData(TCPSession* session, char* data, unsigned int len, TCPMsgTag tag)
{
	pushThreadMsg(TCPThreadMsgType::RECV_DATA, session, data, len, tag);
}
#else
void TCPServer::onSessionRecvData(TCPSession* session, char* data, unsigned int len)
{
	pushThreadMsg(TCPThreadMsgType::RECV_DATA, session, data, len, TCPMsgTag::MT_DEFAULT);
}
#endif


void TCPServer::onServerIdleRun()
{
	executeOperation();
	switch (m_serverStage)
	{
	case TCPServer::ServerStage::CLEAR:
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
	case TCPServer::ServerStage::WAIT_SESSION_CLOSE:
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

void TCPServer::executeOperation()
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
		case TCP_SVR_OP_SEND_DATA :		// 数据发送
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
		case TCP_SVR_OP_DIS_SESSION:	// 断开连接
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->executeDisconnect();
			}
		}break;
		case TCP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD:
		{
			auto it = m_allSession.find(curOperation.sessionID);
			if (it != m_allSession.end())
			{
				it->second.session->~TCPSession();
				fc_free(it->second.session);
				it = m_allSession.erase(it);
			}
		}break;
		case TCP_SVR_OP_STOP_SERVER:
		{
#if OPEN_UV_THREAD_HEARTBEAT == 1 
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

#if OPEN_UV_THREAD_HEARTBEAT == 1
void TCPServer::heartRun()
{
	for (auto &it : m_allSession)
	{
		if (!it.second.isInvalid)
		{
			it.second.curHeartTime += HEARTBEAT_TIMER_DELAY;
			if (it.second.curHeartTime >= HEARTBEAT_CHECK_DELAY)
			{
				it.second.curHeartTime = 0;
				it.second.curHeartCount++;
				if (it.second.curHeartCount > 0)
				{
					if (it.second.curHeartCount > HEARTBEAT_MAX_COUNT_SERVER)
					{
						it.second.curHeartCount = 0;
						it.second.session->executeDisconnect();
					}
					else
					{
						unsigned int sendlen = 0;
						char* senddata = packageHeartMsgData(HEARTBEAT_MSG_S2C, &sendlen);
						it.second.session->executeSend(senddata, sendlen);
						NET_UV_LOG(NET_UV_L_HEART, "send heart s->c");
					}
				}
			}
		}
	}
}
#endif


void TCPServer::clearData()
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
		if (m_operationQue.front().operationType == TCP_SVR_OP_SEND_DATA)
		{
			fc_free(m_operationQue.front().operationData);
		}
		m_operationQue.pop();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TCPServer::uv_on_idle_run(uv_idle_t* handle)
{
	TCPServer* s = (TCPServer*)handle->data;
	s->onServerIdleRun();
}

#if OPEN_UV_THREAD_HEARTBEAT == 1
void TCPServer::uv_heart_timer_callback(uv_timer_t* handle)
{
	TCPServer* s = (TCPServer*)handle->data;
	s->heartRun();
}
#endif

NS_NET_UV_END