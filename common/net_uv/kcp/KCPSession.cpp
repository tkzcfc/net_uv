#include "KCPSession.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

KCPSession* KCPSession::createSession(SessionManager* sessionManager, KCPSocket* socket)
{
	KCPSession* session = (KCPSession*)fc_malloc(sizeof(KCPSession));
	new(session)KCPSession(sessionManager);

	if (session == NULL)
	{
		socket->~KCPSocket();
		fc_free(socket);
		return NULL;
	}
	session->init(socket);
	return session;
}

KCPSession::KCPSession(SessionManager* sessionManager)
	: Session(sessionManager)
	, m_socket(NULL)
	, m_recvBuf(NULL)
{}

KCPSession::~KCPSession()
{
	if (m_socket)
	{
		m_socket->~KCPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}

	if (m_recvBuf)
	{
		fc_free(m_recvBuf);
		m_recvBuf = NULL;
	}
}

void KCPSession::init(KCPSocket* socket)
{
	assert(socket != 0);
	m_socket = socket;
	m_socket->setCloseCallback(std::bind(&KCPSession::on_socket_close, this, std::placeholders::_1));
	m_socket->setRecvCallback(std::bind(&KCPSession::on_socket_recv, this, std::placeholders::_1, std::placeholders::_2));
}

void KCPSession::executeSend(char* data, unsigned int len)
{
	if (data == NULL || len <= 0)
		return;

	if (isOnline())
	{
		getKCPSocket()->send(data, len);
	}
	fc_free(data);
}

void KCPSession::executeDisconnect()
{
	if (isOnline())
	{
		setIsOnline(false);
		getKCPSocket()->disconnect();
	}
}

void KCPSession::on_socket_close(Socket* socket)
{
	this->setIsOnline(false);
	if (m_sessionCloseCallback)
	{
		m_sessionCloseCallback(this);
	}
}

void KCPSession::on_socket_recv(char* data, ssize_t len)
{

}

NS_NET_UV_END