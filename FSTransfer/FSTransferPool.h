#pragma once

#include "net_uv/net_uv.h"
#include "FSTransfer.h"
#include <memory>

typedef net_uv::TCPClient FS_Client;
typedef net_uv::TCPServer FS_Server;

class FSTransferPool
{
public:

	FSTransferPool();

	~FSTransferPool();

	bool listen(const char* ip, uint32_t port, bool isIPV6);

	void stopListen(void(*stopCall)() = NULL);

	bool isListen();

	void upload(const char* ip, uint32_t port, const char* filename);

	void download(const char* ip, uint32_t port, const char* filename);

	void close(const std::function<void()>& call);

	void updateFrame();

protected:

	void checkNetClient();

	void executeTask();
	
protected:	
	std::shared_ptr<FS_Client> m_netClient;
	std::shared_ptr<FS_Server> m_netServer;

	void(*m_stopListenCall)();

	std::function<void()> m_closeClientCall;

	std::map<net_uv::Session*, std::shared_ptr<FSTransfer> > m_listenTransferMap;

	struct TaskInfo
	{
		std::string name;
		bool isdownload;
		std::string ip;
		uint32_t port;
	};
	std::vector<TaskInfo> m_allWaitTaskInfo;
	
	struct ConnectTransferInfo
	{
		std::shared_ptr<FSTransfer> transfer;
		TaskInfo task;
	};
	std::map<uint32_t, ConnectTransferInfo > m_allExecuteTransferMap;

	uint32_t m_sessionIDSpawn;
};
