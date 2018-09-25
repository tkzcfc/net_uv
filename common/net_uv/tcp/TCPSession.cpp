#include "TCPSession.h"
#include "TCPUtils.h"

NS_NET_UV_BEGIN

TCPSession* TCPSession::createSession(SessionManager* sessionManager, TCPSocket* socket, const TCPSessionRecvCall& call)
{
	TCPSession* session = (TCPSession*)fc_malloc(sizeof(TCPSession));
	new(session)TCPSession(sessionManager, socket);
	
	if (session == NULL)
	{
		socket->disconnect();
		socket->~TCPSocket();
		fc_free(socket);
		return NULL;
	}

	if (session->initWithSocket(socket))
	{
		session->setMsgCallback(call);
		return session;
	}
	else
	{
		session->~TCPSession();
		fc_free(session);
	}
	return NULL;
}

TCPSession::TCPSession(SessionManager* sessionManager, TCPSocket* socket)
	: Session(sessionManager)
	, m_recvBuffer(NULL)
	, m_socket(NULL)
	, m_recvCallback(nullptr)
{
	assert(sessionManager != NULL);
	assert(socket != NULL);
}

TCPSession::~TCPSession()
{
	if (m_recvBuffer)
	{
		m_recvBuffer->~Buffer();
		fc_free(m_recvBuffer);
		m_recvBuffer = NULL;
	}

	if (m_socket)
	{
		m_socket->~TCPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}
}

bool TCPSession::initWithSocket(TCPSocket* socket)
{
	m_recvBuffer = (Buffer*)fc_malloc(sizeof(Buffer));
	if (m_recvBuffer == NULL)
	{
		socket->disconnect();
		socket->~TCPSocket();
		fc_free(socket);
		return false;
	}
	new (m_recvBuffer)Buffer(1024 * 4);

	socket->setRecvCallback(std::bind(&TCPSession::on_socket_recv, this, std::placeholders::_1, std::placeholders::_2));
	socket->setCloseCallback(std::bind(&TCPSession::on_socket_close, this, std::placeholders::_1));

	m_socket = socket;
	return true;
}

void TCPSession::executeSend(char* data, unsigned int len)
{
	if (data == NULL || len <= 0)
		return;

	if (isOnline())
	{
		if (!m_socket->send(data, len))
		{
			executeDisconnect();
		}
	}
	else
	{
		fc_free(data);
	}
}

void TCPSession::executeDisconnect()
{
	if (isOnline())
	{
		setIsOnline(false);
		getTCPSocket()->disconnect();
	}
}

void TCPSession::on_socket_recv(char* data, ssize_t len)
{
	if (!isOnline())
		return;

	m_recvBuffer->add(data, len);

	const unsigned int headlen = sizeof(TCPMsgHead);

	while (m_recvBuffer->getDataLength() >= headlen)
	{
		TCPMsgHead* h = (TCPMsgHead*)m_recvBuffer->getHeadBlockData();

		//长度大于最大包长或长度小于等于零，不合法客户端
		if (h->len > TCP_BIG_MSG_MAX_LEN || h->len <= 0)
		{
#if OPEN_NET_UV_DEBUG == 1
			char* pMsg = (char*)fc_malloc(m_recvBuffer->getDataLength());
			m_recvBuffer->get(pMsg);
			std::string errdata(pMsg, m_recvBuffer->getDataLength());
			fc_free(pMsg);
			NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
			m_recvBuffer->clear();
			NET_UV_LOG(NET_UV_L_WARNING, "数据不合法 (1)!!!!");
			executeDisconnect();
			return;
		}
		// 消息内容标记不合法
#if OPEN_UV_THREAD_HEARTBEAT == 1
		if (h->tag > TCPMsgTag::MT_DEFAULT)
		{
			NET_UV_LOG(NET_UV_L_WARNING, "数据不合法 (2)!!!!");

#if OPEN_NET_UV_DEBUG == 1
			char* pMsg = (char*)fc_malloc(m_recvBuffer->getDataLength());
			m_recvBuffer->get(pMsg);
			std::string errdata(pMsg, m_recvBuffer->getDataLength());
			fc_free(pMsg);
			NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
			m_recvBuffer->clear();
			executeDisconnect();
			return;
		}
#endif

		int subv = m_recvBuffer->getDataLength() - (h->len + headlen);

		//消息接收完成
		if (subv >= 0)
		{
			char* pMsg = (char*)fc_malloc(m_recvBuffer->getDataLength());
			m_recvBuffer->get(pMsg);

			char* src = pMsg + headlen;

#if OPEN_TCP_UV_MD5_CHECK == 1
			unsigned int recvLen = 0;
			char* recvData = tcp_uv_decode(src, h->len, recvLen);

			if (recvData != NULL && recvLen > 0)
			{
				m_recvCallback(this, recvData, recvLen, h->tag);
			}
			else//数据不合法
			{
				NET_UV_LOG(NET_UV_L_WARNING, "数据不合法 (3)!!!!");
#if OPEN_NET_UV_DEBUG == 1
				std::string errdata(pMsg, m_recvBuffer->getDataLength());
				NET_UV_LOG(NET_UV_L_WARNING, errdata.c_str());
#endif
				m_recvBuffer->clear();
				fc_free(pMsg);
				executeDisconnect();
				return;
			}
#else
			char* recvData = (char*)fc_malloc(h->len + 1);
			memcpy(recvData, src, h->len);
			recvData[h->len] = '\0';

#if OPEN_UV_THREAD_HEARTBEAT == 1
			m_recvCallback(this, recvData, h->len, h->tag);
#else
			m_recvCallback(this, recvData, h->len);
#endif
#endif
			m_recvBuffer->clear();

			if (subv > 0)
			{
				m_recvBuffer->add(data + (len - subv), subv);
			}
			fc_free(pMsg);
		}
		else
		{
			break;
		}
	}
}

void TCPSession::on_socket_close(Socket* socket)
{
	this->setIsOnline(false);
	if (m_sessionCloseCallback)
	{
		m_sessionCloseCallback(this);
	}
}

void TCPSession::setIsOnline(bool isOnline)
{
	Session::setIsOnline(isOnline);
	m_recvBuffer->clear();
}

NS_NET_UV_END