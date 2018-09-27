#pragma once

#include "UDPSocket.h"
#include "KcpSession.h"

NS_NET_UV_BEGIN

class UDPClient : public Client
{
public:
	UDPClient();
	UDPClient(const UDPClient&) = delete;
	virtual ~UDPClient();

	/// Client
	virtual void connect(const char* ip, unsigned int port, unsigned int sessionId)override;

	virtual void disconnect(unsigned int sessionId)override;

	virtual void closeClient()override;

	virtual void updateFrame()override;

	/// SessionManager
	virtual void send(Session* session, char* data, unsigned int len)override;

	virtual void disconnect(Session* session)override;

	/// UDPClient
	void send(unsigned int sessionId, char* data, unsigned int len);

protected:
	/// Runnable
	virtual void run()override;

	/// SessionManager
	virtual void executeOperation()override;

	/// UDPClient
	void idle_run();

	void removeSessionBySocket(UDPSocket* socket);

	void clearData();

protected:

	std::map<unsigned int, KcpSession*> m_allSession;

	uv_loop_t m_loop;
	uv_idle_t m_idle;
	bool m_isStop;
protected:

	static void uv_on_idle_run(uv_idle_t* handle);
};

NS_NET_UV_END
