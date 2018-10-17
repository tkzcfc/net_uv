#include "KCPSocketManager.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

KCPSocketManager::KCPSocketManager(uv_loop_t* loop)
	: m_convCount(1000)
	, m_owner(NULL)
{
	m_loop = loop;

	uv_idle_init(loop, &m_idle);
	m_idle.data = this;
	uv_idle_start(&m_idle, KCPSocketManager::uv_on_idle_run);
}

KCPSocketManager::~KCPSocketManager()
{
	uv_idle_stop(&m_idle);
}

void KCPSocketManager::push(KCPSocket* socket)
{
	for (auto& it : m_allSocket)
	{
		if (it.socket == socket)
		{
			return;
		}
	}
	SMData data;
	data.invalid = false;
	data.socket = socket;
	m_allSocket.push_back(data);
}

IUINT32 KCPSocketManager::getNewConv()
{
	m_convCount++;
	return m_convCount;
}

void KCPSocketManager::remove(KCPSocket* socket)
{
	for (auto& it : m_allSocket)
	{
		if (it.socket == socket)
		{
			it.invalid = true;
		}
	}
}

void KCPSocketManager::connect(KCPSocket* socket)
{
	for (auto& it : m_allSocket)
	{
		if (it.invalid == false && it.socket == socket)
		{
			m_owner->m_newConnectionCall(socket);
		}
	}
}

void KCPSocketManager::stop_listen()
{
	std::vector<SMData> tmp = m_allSocket;
	m_allSocket.clear();

	for (auto& it : tmp)
	{
		it.socket->disconnect();
	}
}

int KCPSocketManager::isContain(const struct sockaddr* addr)
{
	std::string strip;
	unsigned int port;
	unsigned int addrlen = net_getsockAddrIPAndPort(addr, strip, port);
	if (addrlen == 0)
	{
		return -1;
	}

	for (auto& it : m_allSocket)
	{
		if (!it.invalid && it.socket->getIp() == strip && it.socket->getPort() == port)
		{
			return 1;
		}
	}
	return 0;
}

void KCPSocketManager::idleRun()
{
	IUINT32 update_clock = iclock();

	auto it = m_allSocket.begin();
	for (; it != m_allSocket.end(); )
	{
		if (it->invalid)
		{
			it = m_allSocket.erase(it);
		}
		else
		{
			it->socket->socketUpdate(update_clock);
			it++;
		}
	}

	if (m_owner)
	{
		m_owner->updateKcp(update_clock);
	}
}

void KCPSocketManager::uv_on_idle_run(uv_idle_t* handle)
{
	((KCPSocketManager*)handle->data)->idleRun();
}


NS_NET_UV_END