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
}

bool KcpSession::init(UDPSocket* socket, IUINT32 conv)
{
	assert(socket != 0);
	m_socket = socket;

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
		// Ĭ��ģʽ
		ikcp_nodelay(m_kcp, 0, 10, 0, 0);
	}
	else if (mode == 1) 
	{
		// ��ͨģʽ���ر����ص�
		ikcp_nodelay(m_kcp, 0, 10, 0, 1);
	}
	else 
	{
		// ��������ģʽ
		// �ڶ������� nodelay-�����Ժ����ɳ�����ٽ�����
		// ���������� intervalΪ�ڲ�����ʱ�ӣ�Ĭ������Ϊ 10ms
		// ���ĸ����� resendΪ�����ش�ָ�꣬����Ϊ2
		// ��������� Ϊ�Ƿ���ó������أ������ֹ
		ikcp_nodelay(m_kcp, 1, 10, 2, 1);
		m_kcp->rx_minrto = 10;
		m_kcp->fastresend = 1;
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
	else
	{
		fc_free(data);
	}
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
