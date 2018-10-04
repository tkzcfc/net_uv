#include "KcpSession.h"
#include "UDPSocket.h"

NS_NET_UV_BEGIN

KcpSession* KcpSession::createSession(SessionManager* sessionManager, uv_udp_t* udp/* = NULL*/)
{
	KcpSession* session = (KcpSession*)fc_malloc(sizeof(KcpSession));
	new(session)KcpSession(sessionManager);

	if (session == NULL)
	{
		socket->~UDPSocket();
		fc_free(socket);
		return NULL;
	}

	if (session->init(socket, conv))
	{
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
	, m_connectCall(nullptr)
	, m_recvBuf(NULL)
{
	m_idle.data = NULL;
}

KcpSession::~KcpSession()
{
	if (m_idle.data)
	{
		uv_idle_stop(&m_idle);
		m_idle.data = NULL;
	}

	if (m_socket)
	{
		m_socket->~UDPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}

	if (m_kcp)
	{
		ikcp_release(m_kcp);
		m_kcp = NULL;
	}

	if (m_recvBuf)
	{
		fc_free(m_recvBuf);
		m_recvBuf = NULL;
	}
}

bool KcpSession::init(UDPSocket* socket, IUINT32 conv)
{
	assert(socket != 0);
	m_socket = socket;
	m_socket->setCloseCallback(std::bind(&KcpSession::on_socket_close, this, std::placeholders::_1));

	m_idle.data = this;
	uv_idle_init(socket->getLoop(), &m_idle);
	uv_idle_start(&m_idle, uv_on_idle_run);

	m_kcp = ikcp_create(conv, this);
	if (m_kcp == NULL)
	{
		return false;
	}

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

void KcpSession::update()
{
	ikcp_update(m_kcp, iclock());

	if (m_recvBuf = NULL)
	{
		m_recvBuf = (char*)fc_malloc(1024 * 1024 * 10);
	}
	int recvLen = ikcp_recv(m_kcp, m_recvBuf, 1024 * 1024 * 10);
	if (recvLen > 0)
	{
		NET_UV_LOG(NET_UV_L_INFO, "[%d]受到%d个字节", getSessionID(), recvLen);
	}
}

int KcpSession::udp_output(const char *buf, int len, ikcpcb *kcp, void *user)
{
	KcpSession* s = (KcpSession*)user;
	if (s->getUDPSocket())
	{
		s->getUDPSocket()->send((char*)buf, len);
	}
	return 0;
}

void KcpSession::executeSend(char* data, unsigned int len)
{
	if (data == NULL || len <= 0)
		return;

	if (isOnline())
	{
		if (ikcp_send(m_kcp, data, len) != 0)
		{
			executeDisconnect();
		}
	}
	fc_free(data);
}

void KcpSession::executeDisconnect()
{
	if (isOnline())
	{
		setIsOnline(false);
		getUDPSocket()->disconnect();
	}
}

void KcpSession::uv_on_idle_run(uv_idle_t* handle)
{
	KcpSession* session = (KcpSession*)handle->data;
	if (session)
	{
		session->update();
	}
}

void KcpSession::on_socket_close(Socket* socket)
{
	this->setIsOnline(false);
	if (m_sessionCloseCallback)
	{
		m_sessionCloseCallback(this);
	}
}

NS_NET_UV_END