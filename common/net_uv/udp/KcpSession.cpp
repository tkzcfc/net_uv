#include "KcpSession.h"
#include "UDPSocket.h"


KcpSession::KcpSession()
	: m_kcp(NULL)
	, m_socket(NULL)
{}

KcpSession::~KcpSession()
{
	if (m_kcp)
	{
		ikcp_release(m_kcp);
		m_kcp = NULL;
	}
}

void KcpSession::init(IUINT32 conv, UDPSocket* socket)
{
	if (m_kcp != NULL)
	{
		return;
	}
	this->setSocket(socket);

	m_kcp = ikcp_create(conv, this);
	m_kcp->output = udp_output;

	this->setWndSize(128, 128);
	this->setMode(2);
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

void KcpSession::send(const char* buffer, int len)
{
	ikcp_send(m_kcp, buffer, len);
}

int KcpSession::udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	KcpSession* s = (KcpSession*)user;
	if (s->m_socket)
	{
		s->m_socket->send((void*)buf, len);
	}
	return 0;
}
