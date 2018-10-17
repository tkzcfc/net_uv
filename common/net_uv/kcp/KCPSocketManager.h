#pragma once

#include "KCPCommon.h"
#include "KCPUtils.h"

NS_NET_UV_BEGIN

class KCPSocket;
class KCPSocketManager
{
public:
	KCPSocketManager(uv_loop_t* loop);

	virtual ~KCPSocketManager();
	
	IUINT32 getNewConv();

	void push(KCPSocket* socket);

	void remove(KCPSocket* socket);

	void connect(KCPSocket* socket);

	void stop_listen();

	inline void setOwner(KCPSocket* socket);

	int isContain(const struct sockaddr* addr);

protected:

	void idleRun();

protected:

	static void uv_on_idle_run(uv_idle_t* handle);

protected:

	struct SMData
	{
		KCPSocket* socket;
		bool invalid;
	};

	uv_loop_t* m_loop;
	uv_idle_t m_idle;
	IUINT32 m_convCount;
	std::vector<SMData> m_allSocket;

	KCPSocket* m_owner;
};

void KCPSocketManager::setOwner(KCPSocket* socket)
{
	m_owner = socket;
}

NS_NET_UV_END