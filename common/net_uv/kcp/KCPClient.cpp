#include "KCPClient.h"

NS_NET_UV_BEGIN

enum
{
	KCP_CLI_OP_CONNECT,	//	连接
	KCP_CLI_OP_SENDDATA,	// 发送数据
	KCP_CLI_OP_DISCONNECT,	// 断开连接
	KCP_CLI_OP_CLIENT_CLOSE, //客户端退出
	KCP_CLI_OP_REMOVE_SESSION,//移除会话
	KCP_CLI_OP_DELETE_SESSION,//删除会话
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

KCPClient::KCPClient()
	: m_isStop(false)
{
	uv_loop_init(&m_loop);

	uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	uv_idle_start(&m_idle, uv_on_idle_run);

	m_clientStage = clientStage::START;

	this->startThread();
}

KCPClient::~KCPClient()
{
	this->closeClient();
	this->join();
	clearData();
}

/// Client
void KCPClient::connect(const char* ip, unsigned int port, unsigned int sessionId)
{
	if (m_isStop)
		return;

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
		const KCPThreadMsg_C& Msg = m_msgDispatchQue.front();
		switch (Msg.msgType)
		{
		case KCPThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case KCPThreadMsgType::CONNECT_FAIL:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 0);
			}
		}break;
		case KCPThreadMsgType::CONNECT:
		{
			if (m_connectCall != nullptr)
			{
				m_connectCall(this, Msg.pSession, 1);
			}
		}break;
		case KCPThreadMsgType::DIS_CONNECT:
		{
			if (m_disconnectCall != nullptr)
			{
				m_disconnectCall(this, Msg.pSession);
			}
		}break;
		case KCPThreadMsgType::EXIT_LOOP:
		{
			closeClientTag = true;
		}break;
		case KCPThreadMsgType::REMOVE_SESSION:
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
void KCPClient::send(unsigned int sessionId, char* data, unsigned int len)
{
	if (m_isStop)
		return;

	if (data == 0 || len <= 0)
		return;

	char* sendData = (char*)fc_malloc(len);
	memcpy(sendData, data, len);
	pushOperation(KCP_CLI_OP_SENDDATA, sendData, len, sessionId);
}

/// Runnable
void KCPClient::run()
{
	m_loop.data = NULL;

	uv_run(&m_loop, UV_RUN_DEFAULT);

	uv_loop_close(&m_loop);

	m_clientStage = clientStage::STOP;
	this->pushThreadMsg(KCPThreadMsgType::EXIT_LOOP, NULL);
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
			if (sessionData)
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
				sessionData->session->executeDisconnect();
				sessionData->connectState = DISCONNECTING;
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
		case KCP_CLI_OP_CLIENT_CLOSE://客户端关闭
		{
			if (m_clientStage == clientStage::START)
			{
				for (auto& it : m_allSessionMap)
				{
					it.second.session->executeDisconnect();
				}
			}
			m_clientStage = clientStage::CLEAR_SESSION;
		}break;
		case KCP_CLI_OP_REMOVE_SESSION://移除
		{
			auto sessionData = getClientSessionDataBySessionId(curOperation.sessionID);
			sessionData->removeTag = true;
			if (sessionData->connectState != CONNECTSTATE::DISCONNECT)
			{
				sessionData->session->executeDisconnect();
			}
			else
			{
				pushThreadMsg(KCPThreadMsgType::REMOVE_SESSION, sessionData->session);
			}
		}break;
		case KCP_CLI_OP_DELETE_SESSION://删除
		{
			auto it = m_allSessionMap.find(curOperation.sessionID);
			if (it != m_allSessionMap.end() && it->second.removeTag)
			{
				it->second.session->~KcpSession();
				fc_free(it->second.session);
				m_allSessionMap.erase(it);
			}
		}break;
		default:
			break;
		}
		m_operationDispatchQue.pop();
	}
}

void KCPClient::idle_run()
{
	executeOperation();
	if (m_clientStage == clientStage::CLEAR_SESSION)
	{
		for (auto& it : m_allSessionMap)
		{
			if (it.second.connectState != DISCONNECT)
			{
				it.second.removeTag = true;
				it.second.session->executeDisconnect();
			}
			else
			{
				if (!it.second.removeTag)
				{
					it.second.removeTag = true;
					pushThreadMsg(KCPThreadMsgType::REMOVE_SESSION, it.second.session);
				}
			}
		}
		if (m_allSessionMap.empty())
		{
			m_clientStage = clientStage::WAIT_EXIT;
		}
	}
	else if (m_clientStage == clientStage::WAIT_EXIT)
	{
		uv_stop(&m_loop);
	}

	ThreadSleep(1);
}

void KCPClient::clearData()
{
	for (auto& it : m_allSessionMap)
	{
		it.second.session->~KcpSession();
		fc_free(it.second.session);
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
		}
		m_operationQue.pop();
	}
}

void KCPClient::pushThreadMsg(KCPThreadMsgType type, Session* session, char* data/* = NULL*/, unsigned int len/* = 0U*/)
{
	KCPThreadMsg_C msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

KCPClient::clientSessionData* KCPClient::getClientSessionDataBySessionId(unsigned int sessionId)
{
	auto it = m_allSessionMap.find(sessionId);
	if (it != m_allSessionMap.end())
	{
		return &it->second;
	}
	return NULL;
}

void KCPClient::createNewConnect(void* data)
{
	if (data == NULL)
		return;

	KCPClientConnectOperation* opData = (KCPClientConnectOperation*)data;

	auto it = m_allSessionMap.find(opData->sessionID);
	if (it != m_allSessionMap.end())
	{
		if (it->second.removeTag)
			return;
		//对比端口和IP是否一致
		if (strcmp(opData->ip.c_str(), it->second.session->getIp().c_str()) != 0 &&
			opData->port != it->second.session->getPort())
		{
			pushThreadMsg(KCPThreadMsgType::CONNECT_FAIL, NULL);
			return;
		}

		if (it->second.connectState == CONNECTSTATE::DISCONNECT)
		{
			if (it->second.session->getUDPSocket()->connect(opData->ip.c_str(), opData->port))
			{
				it->second.connectState = CONNECTSTATE::CONNECT;
				pushThreadMsg(KCPThreadMsgType::CONNECT, it->second.session);
			}
			else
			{
				it->second.connectState = CONNECTSTATE::DISCONNECT;
				it->second.session->executeDisconnect();
				pushThreadMsg(KCPThreadMsgType::CONNECT_FAIL, it->second.session);
			}
		}
	}
	else
	{
		UDPSocket* socket = (UDPSocket*)fc_malloc(sizeof(UDPSocket));
		new (socket) UDPSocket(&m_loop);	
		socket->setReadCallback(std::bind(&KCPClient::onSocketRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

		KcpSession* session = KcpSession::createSession(this, socket, opData->sessionID);
		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_FATAL, "创建会话失败，可能是内存不足!!!");
			return;
		}
		session->setSessionID(opData->sessionID);
		session->setSessionClose(std::bind(&KCPClient::onSessionClose, this, std::placeholders::_1));

		clientSessionData cs;
		cs.removeTag = false;
		cs.session = session;

		if (socket->connect(opData->ip.c_str(), opData->port))
		{
			session->setIsOnline(true);
			cs.connectState = CONNECTSTATE::CONNECT;
			pushThreadMsg(KCPThreadMsgType::CONNECT, session);
		}
		else
		{
			session->setIsOnline(false);
			cs.connectState = CONNECTSTATE::DISCONNECT;
			pushThreadMsg(KCPThreadMsgType::CONNECT_FAIL, session);
		}
		m_allSessionMap.insert(std::make_pair(opData->sessionID, cs));
	}
}

void KCPClient::onSessionClose(Session* session)
{
	for (auto& it : m_allSessionMap)
	{
		if (it.second.session == session)
		{
			it.second.connectState = DISCONNECT;
			if (it.second.removeTag)
			{
				pushThreadMsg(KCPThreadMsgType::REMOVE_SESSION, session);
			}
			break;
		}
	}
}

void KCPClient::onSocketRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	IUINT32 conv = ikcp_getconv(buf->base);
	auto sessionData = getClientSessionDataBySessionId(conv);
	if (sessionData)
	{
		sessionData->session->input(buf->base, nread);
	}
}

//////////////////////////////////////////////////////////////////////////

void KCPClient::uv_on_idle_run(uv_idle_t* handle)
{
	KCPClient* c = (KCPClient*)handle->data;
	c->idle_run();
	ThreadSleep(1);
}

NS_NET_UV_END