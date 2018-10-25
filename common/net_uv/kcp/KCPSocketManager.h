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

	void stop_listen();

	inline void setOwner(KCPSocket* socket);

	inline KCPSocket* getOwner();

	int32_t isContain(const struct sockaddr* addr);

	inline int32_t getAwaitConnectCount();

protected:

	void idleRun();

	void on_socket_connect(Socket* socket, int32_t status);

	void on_socket_close(Socket* socket);

	void connect(KCPSocket* socket);

	void removeAwaitConnectSocket(KCPSocket* socket);

	void clearInvalid();

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

	bool m_isConnectArrDirty;
	bool m_isAwaitConnectArrDirty;

	std::vector<SMData> m_allConnectSocket;
	std::vector<SMData> m_allAwaitConnectSocket;

	KCPSocket* m_owner;
	bool m_stop;
};

void KCPSocketManager::setOwner(KCPSocket* socket)
{
	m_owner = socket;
}

KCPSocket* KCPSocketManager::getOwner()
{
	return m_owner;
}

int32_t KCPSocketManager::getAwaitConnectCount()
{
	return (int)m_allAwaitConnectSocket.size();
}

NS_NET_UV_END