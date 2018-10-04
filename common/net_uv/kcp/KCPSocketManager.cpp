#include "KCPSocketManager.h"
#include "KCPSocket.h"

KCPSocketManager::KCPSocketManager(uv_loop_t* loop)
	: m_convCount(1)
{
	m_loop = loop;

	uv_idle_init(loop, &m_idle);
	uv_idle_start(&m_idle, KCPSocketManager::uv_on_idle_run);
}

KCPSocketManager::~KCPSocketManager()
{
	uv_idle_stop(&m_idle);
}

void KCPSocketManager::push(KCPSocket* socket, IUINT32 conv)
{
	auto it = m_allSocket.find(conv);
	if (it != m_allSocket.end())
	{
		m_allSocket.insert(std::make_pair(conv, socket));
	}
	else
	{
		assert(0);
	}
}

KCPSocket* KCPSocketManager::accept(uv_udp_t* handle, const struct sockaddr* addr)
{
	char szIp[17] = { 0 };
	int r = uv_ip4_name((const struct sockaddr_in*) addr, szIp, 16);
	if (r != 0)
	{
		NET_UV_LOG(NET_UV_L_ERROR, "kcp服务器创建KCPSocket失败,地址解析失败");
		return NULL;
	}

	struct sockaddr* curAddr = (struct sockaddr*)fc_malloc(sizeof(struct sockaddr));
	memcpy(curAddr, addr, sizeof(struct sockaddr));

	KCPSocket* socket = (KCPSocket*)fc_malloc(sizeof(KCPSocket));
	new (socket)KCPSocket(m_loop);

	socket->setIp(szIp);
	socket->setConv(m_convCount);
	socket->initKcp(m_convCount);
	socket->setWeakRefUdp(handle);
	socket->setSocketAddr(curAddr);
	this->push(socket, m_convCount);

	m_convCount++;

	return socket;
}

void KCPSocketManager::remove(IUINT32 conv)
{
	auto it = m_allSocket.find(conv);
	if (it != m_allSocket.end())
	{
		m_allSocket.erase(it);
	}
}

void KCPSocketManager::resetLastPacketRecvTime(IUINT32 conv)
{
	auto it = m_allSocket.find(conv);
	if (it != m_allSocket.end())
	{
		it->second->resetLastPacketRecvTime();
	}
}

void KCPSocketManager::input(IUINT32 conv, const char* data, long size)
{
	auto it = m_allSocket.find(conv);
	if (it != m_allSocket.end())
	{
		it->second->kcpInput(data, size);
	}
}

void KCPSocketManager::disconnect(IUINT32 conv)
{
	auto it = m_allSocket.find(conv);
	if (it != m_allSocket.end())
	{
		it->second->shutdownSocket();
	}
}

void KCPSocketManager::stop_listen()
{
	for (auto& it : m_allSocket)
	{
		it.second->disconnect();
	}
	m_allSocket.clear();
}

void KCPSocketManager::idleRun()
{
	IUINT32 update_clock = iclock();
	for (auto& it : m_allSocket)
	{
		it.second->socketUpdate(update_clock);
	}
}

void KCPSocketManager::uv_on_idle_run(uv_idle_t* handle)
{
	((KCPSocketManager*)handle->data)->idleRun();
}

