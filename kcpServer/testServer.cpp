#include "../TestCommon.h"

Session* controlClient = NULL;
bool gServerStop = false;


std::vector<Session*> allSession;

void main()
{
	REGISTER_EXCEPTION("kcpServer.dmp");

	//printf("请输入端口号:");
	//int32_t port;
	//std::cin >> port;
	int32_t port = KCP_CONNECT_PORT;

	KCPServer* svr = new KCPServer();

	svr->setCloseCallback([](Server* svr)
	{
		printf("服务器关闭\n");
		gServerStop = true;
	});

	svr->setNewConnectCallback([](Server* svr, Session* session)
	{
		allSession.push_back(session);
		printf("[%d] %s:%d进入服务器\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
	});

	svr->setRecvCallback([=](Server* svr, Session* session, char* data, uint32_t len)
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';

		if (strcmp(msg, "control") == 0)
		{
			sendString(session, "conrol client");
			controlClient = session;
		}
		else if (controlClient == session && strcmp(msg, "close") == 0)
		{
			svr->stopServer();
		}
		else if (controlClient == session && strcmp(msg, "print") == 0)
		{
			printMemInfo();
		}
		else if (controlClient == session && strstr(msg, "send"))
		{
			char* sendbegin = strstr(msg, "send") + 4;
			for (auto &it : allSession)
			{
				if (it != controlClient)
				{
					it->send(sendbegin, strlen(sendbegin));
				}
			}
		}
		else
		{
			if (len > 100)
			{
				printf("[%p]接收到消息%d个字节\n", session, len);
			}
			session->send(data, len);
		}
		fc_free(msg);

		//printf("%s:%d 收到%d个字节数据\n", session->getIp().c_str(), session->getPort(), len);
	});

	svr->setDisconnectCallback([=](Server* svr, Session* session)
	{
		auto it = std::find(allSession.begin(), allSession.end(), session);
		if (it != allSession.end())
		{
			allSession.erase(it);
		}
		printf("[%d] %s:%d离开服务器\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
		if (session == controlClient)
		{
			controlClient = NULL;
		}
	});

	bool issuc = svr->startServer("0.0.0.0", port, false, 0xFFFF);
	if (issuc)
	{
		printf("服务器启动成功\n");
	}
	else
	{
		printf("服务器启动失败\n");
		gServerStop = true;
	}

	while (!gServerStop)
	{
		svr->updateFrame();
		ThreadSleep(1);
	}

	delete svr;

	printf("-----------------------------\n");
	printMemInfo();
	printf("\n-----------------------------\n");

	system("pause");
}


