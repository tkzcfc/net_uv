#include "UDPClient.h"

enum
{
	UDP_CLI_OP_CONNECT,	//	连接
	UDP_CLI_OP_SENDDATA,	// 发送数据
	UDP_CLI_OP_DISCONNECT,	// 断开连接
	UDP_CLI_OP_CLIENT_CLOSE, //客户端退出
};

// 连接操作
struct UDPClientConnectOperation
{
	UDPClientConnectOperation() {}
	~UDPClientConnectOperation() {}
	std::string ip;
	unsigned int port;
	unsigned int sessionID;
};

UDPClient::UDPClient()
	: m_isStop(false)
{
	uv_loop_init(&m_loop);

	uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	uv_idle_start(&m_idle, uv_on_idle_run);

	this->startThread();
}

UDPClient::~UDPClient()
{
	this->closeClient();
	this->join();
	clearData();
}

/// Client
void UDPClient::connect(const char* ip, unsigned int port, unsigned int sessionId)
{
	if (m_isStop)
		return;

	UDPClientConnectOperation* opData = (UDPClientConnectOperation*)fc_malloc(sizeof(UDPClientConnectOperation));
	new (opData)UDPClientConnectOperation();

	opData->ip = ip;
	opData->port = port;
	opData->sessionID = sessionId;

	pushOperation(UDP_CLI_OP_CONNECT, opData, 0U, 0U);
}

void UDPClient::disconnect(unsigned int sessionId)
{
	if (m_isStop)
		return;

	pushOperation(UDP_CLI_OP_DISCONNECT, NULL, 0U, sessionId);
}

void UDPClient::closeClient()
{
	if (m_isStop)
		return;
	m_isStop = true;
	pushOperation(UDP_CLI_OP_CLIENT_CLOSE, NULL, 0U, 0U);
}

void UDPClient::updateFrame()
{}

/// SessionManager
void UDPClient::send(Session* session, char* data, unsigned int len)
{
	send(session->getSessionID(), data, len);
}

void UDPClient::disconnect(Session* session)
{
	disconnect(session->getSessionID());
}

/// UDPClient
void UDPClient::send(unsigned int sessionId, char* data, unsigned int len)
{
	if (m_isStop)
		return;

	if (data == 0 || len <= 0)
		return;

	char* sendData = (char*)fc_malloc(len);
	memcpy(sendData, data, len);
	pushOperation(UDP_CLI_OP_SENDDATA, data, len, sessionId);
}

/// Runnable
void UDPClient::run()
{
	m_loop.data = NULL;

	uv_run(&m_loop, UV_RUN_DEFAULT);

	uv_loop_close(&m_loop);
}

	/// SessionManager
void UDPClient::executeOperation()
{}

void UDPClient::idle_run()
{
	ThreadSleep(1);
}

void UDPClient::removeSessionBySocket(UDPSocket* socket)
{}

void UDPClient::clearData()
{



	while (!m_operationQue.empty())
	{
		auto & curOperation = m_operationQue.front();
		switch (curOperation.operationType)
		{
		case UDP_CLI_OP_SENDDATA:			// 数据发送
		case UDP_CLI_OP_CONNECT:			// 连接
		{
			if (curOperation.operationData)
			{
				fc_free(curOperation.operationData);
			}
		}break;
		}
		m_operationQue.pop();
	}
}

//////////////////////////////////////////////////////////////////////////

void UDPClient::uv_on_idle_run(uv_idle_t* handle)
{
	UDPClient* c = (UDPClient*)handle->data;
	c->idle_run();
	ThreadSleep(1);
}

