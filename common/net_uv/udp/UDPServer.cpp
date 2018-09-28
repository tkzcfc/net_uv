#include "UDPServer.h"

NS_NET_UV_BEGIN

enum
{
	UDP_SVR_OP_STOP_SERVER,	// 停止服务器
	UDP_SVR_OP_SEND_DATA,	// 发送消息给某个会话
	UDP_SVR_OP_DIS_SESSION,	// 断开某个会话
	UDP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD,//向主线程发送会话已断开
};

UDPServer::UDPServer()
	: m_start(false)
	, m_server(NULL)
	, m_isIPV6(false)
{
	m_serverStage = ServerStage::STOP;
}

UDPServer::~UDPServer()
{
	stopServer();
	this->join();
	clearData();

	NET_UV_LOG(NET_UV_L_INFO, "UDPServer destroy...");
}

///UDPServer
void UDPServer::startServer(const char* ip, int port, bool isIPV6)
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

bool UDPServer::stopServer()
{
	if (!m_start)
		return false;
	m_start = false;
	pushOperation(UDP_SVR_OP_STOP_SERVER, NULL, NULL, 0U);
	return true;
}

// 主线程轮询
void UDPServer::updateFrame()
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
		const UDPThreadMsg_S& Msg = m_msgDispatchQue.front();

		switch (Msg.msgType)
		{
		case UDPThreadMsgType::RECV_DATA:
		{
			m_recvCall(this, Msg.pSession, Msg.data, Msg.dataLen);
			fc_free(Msg.data);
		}break;
		case UDPThreadMsgType::START_SERVER_SUC:
		{
			if (m_startCall != nullptr)
			{
				m_startCall(this, true);
			}
		}break;
		case UDPThreadMsgType::START_SERVER_FAIL:
		{
			if (m_startCall != nullptr)
			{
				m_startCall(this, false);
			}
		}break;
		case UDPThreadMsgType::NEW_CONNECT:
		{
			m_newConnectCall(this, Msg.pSession);
		}break;
		case UDPThreadMsgType::DIS_CONNECT:
		{
			m_disconnectCall(this, Msg.pSession);
			pushOperation(UDP_SVR_OP_SEND_DIS_SESSION_MSG_TO_MAIN_THREAD, NULL, 0, Msg.pSession->getSessionID());
		}break;
		case UDPThreadMsgType::EXIT_LOOP:
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

/// SessionManager
void UDPServer::send(Session* session, char* data, unsigned int len)
{}

void UDPServer::disconnect(Session* session)
{
	pushOperation(UDP_SVR_OP_DIS_SESSION, NULL, 0, session->getSessionID());
}

/// Runnable
void UDPServer::run()
{
	int r = uv_loop_init(&m_loop);
	CHECK_UV_ASSERT(r);

	r = uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	CHECK_UV_ASSERT(r);

	uv_idle_start(&m_idle, uv_on_idle_run);

	m_server = (UDPSocket*)fc_malloc(sizeof(UDPSocket));
	if (m_server == NULL)
	{
		m_serverStage = ServerStage::STOP;
		pushThreadMsg(UDPThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}
	new (m_server) UDPSocket(&m_loop);
	m_server->setCloseCallback(std::bind(&UDPServer::onServerSocketClose, this, std::placeholders::_1));
	m_server->setReadCallback(std::bind(&UDPServer::onServerSocketRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

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
		m_server->~UDPSocket();
		fc_free(m_server);
		m_server = NULL;

		m_serverStage = ServerStage::STOP;
		pushThreadMsg(UDPThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}

	suc = m_server->listen();
	if (!suc)
	{
		m_server->~UDPSocket();
		fc_free(m_server);
		m_server = NULL;

		m_serverStage = ServerStage::STOP;
		pushThreadMsg(UDPThreadMsgType::START_SERVER_FAIL, NULL);
		return;
	}
	m_serverStage = ServerStage::RUN;
	pushThreadMsg(UDPThreadMsgType::START_SERVER_SUC, NULL);

	uv_run(&m_loop, UV_RUN_DEFAULT);

	uv_loop_close(&m_loop);

	m_server->~UDPSocket();
	fc_free(m_server);
	m_server = NULL;

	m_serverStage = ServerStage::STOP;
	pushThreadMsg(UDPThreadMsgType::EXIT_LOOP, NULL);
}

/// SessionManager
void UDPServer::executeOperation()
{}

void UDPServer::idleRun()
{}

void UDPServer::clearData()
{}

void UDPServer::pushThreadMsg(UDPThreadMsgType type, Session* session, char* data, unsigned int len)
{
	UDPThreadMsg_S msg;
	msg.msgType = type;
	msg.data = data;
	msg.dataLen = len;
	msg.pSession = session;

	m_msgMutex.lock();
	m_msgQue.push(msg);
	m_msgMutex.unlock();
}

void UDPServer::onServerSocketClose(Socket* svr)
{
	m_serverStage = ServerStage::CLEAR;
}

void UDPServer::onServerSocketRead(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	IUINT32 conv = ikcp_getconv(buf->base);
	auto it = m_allSession.find(conv);
	if (it == m_allSession.end())
	{
		uv_udp_t* udp = (uv_udp_t*)fc_malloc(sizeof(uv_udp_t));
		uv_udp_init(&m_loop, udp);

		struct sockaddr* socketAddr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr));
		memcpy(socketAddr, addr, sizeof(struct sockaddr));

		UDPSocket* socket = (UDPSocket*)fc_malloc(sizeof(UDPSocket));
		new (socket)UDPSocket(&m_loop, udp);
		socket->setSocketAddr(socketAddr);
		
		KcpSession* session = KcpSession::createSession(this, socket, conv);
		if (session == NULL)
		{
			NET_UV_LOG(NET_UV_L_ERROR, "服务器创建新会话失败,可能是内存不足");
			return;
		}

		tcpSessionData data;
		data.isInvalid = false;
		data.session = session;

		m_allSession.insert(std::make_pair(conv, data));
		pushThreadMsg(UDPThreadMsgType::NEW_CONNECT, session);
	}
	else
	{
		it->second.session->input(buf->base, nread);
	}
}

//////////////////////////////////////////////////////////////////////////
void UDPServer::uv_on_idle_run(uv_idle_t* handle)
{
	UDPServer* svr = (UDPServer*)handle->data;
	svr->idleRun();
}

NS_NET_UV_END