#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

class SessionManager;
class NET_UV_EXTERN Session
{
public:
	Session(SessionManager* manager);
	virtual ~Session();

	virtual void send(char* data, unsigned int len);

	virtual void disconnect();

	inline unsigned int getSessionID();

	virtual inline unsigned int getPort() = 0;

	virtual inline const std::string& getIp() = 0;

protected:

	virtual void executeSend(char* data, unsigned int len) = 0;

	virtual void executeDisconnect() = 0;

protected:
		
	inline SessionManager* getSessionManager();

	inline void setSessionClose(const std::function<void(Session*)>& call);

	inline bool isOnline();

	virtual void setIsOnline(bool isOnline);

	inline void setSessionID(unsigned int sessionId);

protected:
	friend class SessionManager;

	SessionManager* m_sessionManager;
	std::function<void(Session*)> m_sessionCloseCallback;

	bool m_isOnline;
	unsigned int m_sessionID;
};


void Session::setSessionClose(const std::function<void(Session*)>& call)
{
	m_sessionCloseCallback = std::move(call);
}

SessionManager* Session::getSessionManager()
{
	return m_sessionManager;
}

bool Session::isOnline()
{
	return m_isOnline;
}

unsigned int Session::getSessionID()
{
#if OPEN_NET_UV_DEBUG == 1
	assert(m_sessionID != -1);
#endif
	return m_sessionID;
}

void Session::setSessionID(unsigned int sessionId)
{
	m_sessionID = sessionId;
}

NS_NET_UV_END