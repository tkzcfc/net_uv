#include "KCPSocketManager.h"
#include "KCPSocket.h"

NS_NET_UV_BEGIN

KCPSocketManager::KCPSocketManager(uv_loop_t* loop)
	: m_convCount(1000)
	, m_owner(NULL)
	, m_stop(false)
	, m_isConnectArrDirty(false)
	, m_isAwaitConnectArrDirty(false)
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
	for (auto& it : m_allAwaitConnectSocket)
	{
		if (it.socket == socket)
		{
			return;
		}
	}

	SMData data;
	data.invalid = false;
	data.socket = socket;
	m_allAwaitConnectSocket.push_back(data);

	socket->setConnectCallback(std::bind(&KCPSocketManager::on_socket_connect, this, std::placeholders::_1, std::placeholders::_2));
	socket->setCloseCallback(std::bind(&KCPSocketManager::on_socket_close, this, std::placeholders::_1));
}

IUINT32 KCPSocketManager::getNewConv()
{
	m_convCount++;
	return m_convCount;
}

void KCPSocketManager::remove(KCPSocket* socket)
{
	for (auto& it : m_allConnectSocket)
	{
		if (it.socket == socket)
		{
			it.invalid = true;
			m_isConnectArrDirty = true;
			break;
		}
	}
}

void KCPSocketManager::stop_listen()
{
	m_stop = true;

	for (auto& it : m_allConnectSocket)
	{
		it.socket->disconnect();
	}

	for (auto& it : m_allAwaitConnectSocket)
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

	for (auto& it : m_allAwaitConnectSocket)
	{
		if (!it.invalid && it.socket && it.socket->getIp() == strip && it.socket->getPort() == port)
		{
			return 1;
		}
	}

	for (auto& it : m_allConnectSocket)
	{
		if (!it.invalid && it.socket->getIp() == strip && it.socket->getPort() == port)
		{
			return 1;
		}
	}
	return 0;
}

void KCPSocketManager::on_socket_connect(Socket* socket, int status)
{
	if (!m_stop && status == 1)
	{
		socket->setConnectCallback(nullptr);
		connect((KCPSocket*)socket);
	}
	else
	{
		removeAwaitConnectSocket((KCPSocket*)socket);
	}
}

void KCPSocketManager::on_socket_close(Socket* socket)
{
	removeAwaitConnectSocket((KCPSocket*)socket);
}

void KCPSocketManager::connect(KCPSocket* socket)
{
	for (auto& it : m_allConnectSocket)
	{
		if (it.socket == socket)
		{
			return;
		}
	}

	SMData data;
	data.invalid = false;
	data.socket = socket;
	m_allConnectSocket.push_back(data);

	m_owner->m_newConnectionCall(socket);

	for (auto& it : m_allAwaitConnectSocket)
	{
		if (it.socket == socket)
		{
			it.invalid = true;
			it.socket = NULL;
			m_isAwaitConnectArrDirty = true;
			break;
		}
	}
}

void KCPSocketManager::removeAwaitConnectSocket(KCPSocket* socket)
{
	for (auto& it : m_allAwaitConnectSocket)
	{
		if (it.socket == socket)
		{
			it.invalid = true;
			m_isAwaitConnectArrDirty = true;
			break;
		}
	}
}

void KCPSocketManager::clearInvalid()
{
	if (m_isAwaitConnectArrDirty)
	{
		auto it = m_allAwaitConnectSocket.begin();
		for (; it != m_allAwaitConnectSocket.end(); )
		{
			if (it->invalid)
			{
				if (it->socket != NULL)
				{
					it->socket->~KCPSocket();
					fc_free(it->socket);
				}
				it = m_allAwaitConnectSocket.erase(it);
			}
			else
			{
				it++;
			}
		}

		m_isAwaitConnectArrDirty = false;
	}

	if (m_isConnectArrDirty)
	{
		auto it = m_allConnectSocket.begin();
		for (; it != m_allConnectSocket.end(); )
		{
			if (it->invalid)
			{
				it = m_allConnectSocket.erase(it);
			}
			else
			{
				it++;
			}
		}
		m_isConnectArrDirty = false;
	}
}

void KCPSocketManager::idleRun()
{
	clearInvalid();

	IUINT32 update_clock = iclock();

	if (m_owner)
	{
		m_owner->updateKcp(update_clock);
	}
	
	for (auto& it : m_allAwaitConnectSocket)
	{
		it.socket->socketUpdate(update_clock);
	}

	for (auto& it : m_allConnectSocket)
	{
		it.socket->socketUpdate(update_clock);
	}
}

void KCPSocketManager::uv_on_idle_run(uv_idle_t* handle)
{
	((KCPSocketManager*)handle->data)->idleRun();
}


NS_NET_UV_END