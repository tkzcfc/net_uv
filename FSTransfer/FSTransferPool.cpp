#include "FSTransferPool.h"

#include "uv.h"

NS_NET_UV_OPEN

FSTransferPool::FSTransferPool()
	: m_netClient(NULL)
	, m_netServer(NULL)
	, m_stopListenCall(NULL)
	, m_closeClientCall(nullptr)
	, m_sessionIDSpawn(0)
{}

FSTransferPool::~FSTransferPool()
{}

bool FSTransferPool::listen(const char* ip, uint32_t port, bool isIPV6)
{
	if (m_netServer == NULL)
	{
		m_netServer = std::make_shared<FS_Server>();

		m_netServer->setCloseCallback([=](Server* svr)
		{
			m_listenTransferMap.clear();
			if (m_stopListenCall)
			{
				m_stopListenCall();
			}
		});

		m_netServer->setNewConnectCallback([=](Server* svr, Session* session)
		{
			if (m_listenTransferMap.find(session) == m_listenTransferMap.end())
			{
				std::shared_ptr<FSTransfer> transfer = std::make_shared<FSTransfer>();
				transfer->setOutput([=](FSTransfer* fst, char* data, uint32_t len) 
				{
					session->send(data, len);
				});
				m_listenTransferMap.insert(std::make_pair(session, transfer));
			}
		});

		m_netServer->setRecvCallback([=](Server* svr, Session* session, char* data, uint32_t len)
		{
			m_listenTransferMap[session]->input(data, len);
		});

		m_netServer->setDisconnectCallback([=](Server* svr, Session* session)
		{
			m_listenTransferMap.erase(m_listenTransferMap.find(session));
		});
	}

	return m_netServer->startServer(ip, port, isIPV6, 0xFFFF);
}

void FSTransferPool::stopListen(void(*stopCall)())
{
	m_stopListenCall = stopCall;
	m_netServer->stopServer();
}

bool FSTransferPool::isListen()
{
	return !m_netServer->isCloseFinish();
}

void FSTransferPool::upload(const char* ip, uint32_t port, const char* filename)
{
	if (strlen(ip) <= 0 || strlen(filename) <= 0)
		return;

	TaskInfo task;
	task.ip = ip;
	task.port = port;
	task.isdownload = false;
	task.name = filename;
	m_allWaitTaskInfo.emplace_back(task);

	executeTask();
}

void FSTransferPool::download(const char* ip, uint32_t port, const char* filename)
{
	if (strlen(ip) <= 0 || strlen(filename) <= 0)
		return;

	TaskInfo task;
	task.ip = ip;
	task.port = port;
	task.isdownload = true;
	task.name = filename;
	m_allWaitTaskInfo.emplace_back(task);

	executeTask();
}

void FSTransferPool::close(const std::function<void()>& call)
{
	m_closeClientCall = call;
	m_netClient->closeClient();
}

void FSTransferPool::updateFrame()
{
	for (auto &it : m_allExecuteTransferMap)
	{
		if (it.second.transfer)
		{
			it.second.transfer->updateFrame();
		}
	}
	for (auto& it : m_listenTransferMap)
	{
		it.second->updateFrame();
	}
	if (m_netClient)
	{
		m_netClient->updateFrame();
	}
	if (m_netServer)
	{
		m_netServer->updateFrame();
	}
}

void FSTransferPool::executeTask()
{
	if (m_allWaitTaskInfo.empty())
		return;

	if (m_allExecuteTransferMap.size() >= 8)
	{
		return;
	}
	checkNetClient();

	TaskInfo task = m_allWaitTaskInfo.back();
	m_allWaitTaskInfo.pop_back();
	m_sessionIDSpawn++;

	ConnectTransferInfo info;
	info.task = task;
	info.transfer = NULL;
	m_allExecuteTransferMap[m_sessionIDSpawn] = info;

	m_netClient->connect(task.ip.c_str(), task.port, m_sessionIDSpawn);
}

void FSTransferPool::checkNetClient()
{
	if (m_netClient == NULL)
	{
		m_netClient = std::make_shared<FS_Client>();

		m_netClient->setClientCloseCallback([=](Client*)
		{
			m_netClient = NULL;
			m_allExecuteTransferMap.clear();
			if (m_closeClientCall)
			{
				m_closeClientCall();
			}
		});

		m_netClient->setConnectCallback([=](Client*, Session* session, int32_t status)
		{
			auto it = m_allExecuteTransferMap.find(session->getSessionID());
			if (it != m_allExecuteTransferMap.end())
			{
				if (it->second.transfer == NULL)
				{
					it->second.transfer = std::make_shared<FSTransfer>();
					it->second.transfer->setOutput([=](FSTransfer* fst, char* data, uint32_t len)
					{
						session->send(data, len);
					});
					if (it->second.task.isdownload)
					{
						it->second.transfer->downLoadFile(it->second.task.name);
					}
					else
					{
						it->second.transfer->postFile(it->second.task.name);
					}
				}
				else
				{
					it->second.transfer->online(true);
				}
			}
		});

		m_netClient->setDisconnectCallback([=](Client*, Session* session) 
		{
			if (m_allExecuteTransferMap.find(session->getSessionID()) == m_allExecuteTransferMap.end())
			{
				m_netClient->removeSession(session->getSessionID());
			}
		});

		m_netClient->setRemoveSessionCallback([](Client*, Session* session) 
		{
			printf("[%d]É¾³ýÁ¬½Ó\n", session->getSessionID());
		});

		m_netClient->setRecvCallback([=](Client*, Session* session, char* data, uint32_t len)
		{
			m_allExecuteTransferMap[session->getSessionID()].transfer->input(data, len);
		});
	}
}

