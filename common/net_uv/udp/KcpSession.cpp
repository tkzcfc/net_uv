#include "KcpSession.h"
#include "UDPSocket.h"

KcpSession* KcpSession::createSession(SessionManager* sessionManager, UDPSocket* socket, IUINT32 conv)
{
	KcpSession* session = (KcpSession*)fc_malloc(sizeof(KcpSession));
	new(session)KcpSession(sessionManager);

	if (session == NULL)
	{
		socket->disconnect();
		socket->~UDPSocket();
		fc_free(socket);
		return NULL;
	}

	if (session->init(socket, conv))
	{
		//session->setMsgCallback(call);
		return session;
	}
	else
	{
		session->~KcpSession();
		fc_free(session);
	}
	return NULL;
}

KcpSession::KcpSession(SessionManager* sessionManager)
	: Session(sessionManager)
	, m_kcp(NULL)
	, m_socket(NULL)
{
	if (m_socket)
	{
		m_socket->~UDPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}
}

KcpSession::~KcpSession()
{
	if (m_kcp)
	{
		ikcp_release(m_kcp);
		m_kcp = NULL;
	}
}

bool KcpSession::init(UDPSocket* socket, IUINT32 conv)
{
	assert(socket != 0);
	m_socket = socket;

	m_kcp = ikcp_create(conv, this);
	if (m_kcp == NULL)
		return false;

	m_kcp->output = udp_output;

	this->setWndSize(128, 128);
	this->setMode(2);
	return true;
}

void KcpSession::setMode(int mode)
{
	if (mode == 0) 
	{
		// 默认模式
		ikcp_nodelay(m_kcp, 0, 10, 0, 0);
	}
	else if (mode == 1) 
	{
		// 普通模式，关闭流控等
		ikcp_nodelay(m_kcp, 0, 10, 0, 1);
	}
	else 
	{
		// 启动快速模式
		// 第二个参数 nodelay-启用以后若干常规加速将启动
		// 第三个参数 interval为内部处理时钟，默认设置为 10ms
		// 第四个参数 resend为快速重传指标，设置为2
		// 第五个参数 为是否禁用常规流控，这里禁止
		ikcp_nodelay(m_kcp, 1, 10, 2, 1);
		m_kcp->rx_minrto = 10;
		m_kcp->fastresend = 1;
	}
}

int KcpSession::udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	KcpSession* s = (KcpSession*)user;
	if (s->m_socket)
	{
		s->m_socket->send((char*)buf, len);
	}
	return 0;
}

void KcpSession::executeSend(char* data, unsigned int len)
{
	ikcp_send(m_kcp, data, len);
}

void KcpSession::executeDisconnect()
{}

