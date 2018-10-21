#pragma once

#include "Common.h"
#include "Session.h"
#include "Mutex.h"

NS_NET_UV_BEGIN

class SessionManager
{
public:

	SessionManager();

	virtual ~SessionManager();

	virtual void send(unsigned int sessionID, char* data, unsigned int len) = 0;

	void send(Session* session, char* data, unsigned int len);

	virtual void disconnect(unsigned int sessionID) = 0;

	void disconnect(Session* session);

protected:

	void pushOperation(int type, void* data, unsigned int len, unsigned int sessionID);

	virtual void executeOperation() = 0;
	
protected:

	struct SessionOperation
	{
		int operationType;
		void* operationData;
		unsigned int operationDataLen;
		unsigned int sessionID;
	};

protected:
	Mutex m_operationMutex;
	std::queue<SessionOperation> m_operationQue;
	std::queue<SessionOperation> m_operationDispatchQue;
};
NS_NET_UV_END
