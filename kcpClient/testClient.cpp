#include "../TestCommon.h"



void main()
{
	REGISTER_EXCEPTION("p2pNode.dmp");

	bool continueRun = true;

	P2PPeer* node = new P2PPeer();

	node->setStartCallback([](bool isSuccess) 
	{
		printf("Peer启动%s\n", isSuccess ? "成功" : "失败");
	});

	node->setNewConnectCallback([](uint64_t key) 
	{
		printf("新连接[%llu]进入\n", key);
	});

	node->setConnectToPeerCallback([](uint64_t key, bool isSuccess) 
	{
		printf("连接到[%llu]%s\n", key, isSuccess ? "成功" : "失败");
	});

	node->setConnectToTurnCallback([](bool isSuccess, uint64_t selfKey) 
	{
		if(isSuccess)
			printf("连接到Turn成功，本机Key:[%llu]\n", selfKey);
		else
			printf("连接到Turn失败\n");
	});

	node->setDisConnectCallback([](uint64_t key) 
	{
		printf("连接[%llu]已断开\n", key);
	});

	node->setRecvCallback([](uint64_t key, char* data, uint32_t len) 
	{
		std::string recvstr(data, len);
		printf("[%llu]收到数据: %s\n", key, recvstr.c_str());
	});

	node->setCloseCallback([&]() 
	{
		printf("Peer关闭\n");
		continueRun = false;
	});


	//node->start("39.105.20.204", 1234);
	node->start("127.0.0.1", 1234);

	while (continueRun)
	{
		if (KEY_DOWN(VK_SPACE))
		{
			char buf[128] = {0};
			printf("请输入要连接的Key:");
			std::cin >> buf;
			try
			{
				int64_t key = 0;
				key = std::stoll(buf); 
				node->connectToPeer(key);
			}
			catch (...)
			{
				continue;
			}
		}
		node->updateFrame();
		Sleep(1);
	}

	system("pause");
}



//
//bool autosend = true;
//uint32_t keyIndex = 0;
//char szWriteBuf[1024] = {0};
//bool clientClose = false;
//
//
//// 命令解析
//bool cmdResolve(char* cmd, uint32_t key);
//
//KCPClient* client = new KCPClient();
//
//void main()
//{
//	REGISTER_EXCEPTION("kcpClient.dmp");
//	
//	client->setClientCloseCallback([](Client*)
//	{
//		clientClose = true;
//		printf("客户端已关闭\n");
//	});
//
//	client->setConnectCallback([=](Client*, Session* session, int32_t status)
//	{
//		if (status == 0)
//		{
//			printf("[%d]连接失败\n", session->getSessionID());
//		}
//		else if (status == 1)
//		{
//			printf("[%d]连接成功\n", session->getSessionID());
//		}
//		else if (status == 2)
//		{
//			printf("[%d]连接超时\n", session->getSessionID());
//		}
//	});
//
//	client->setDisconnectCallback([=](Client*, Session* session) {
//		printf("[%d]断开连接\n", session->getSessionID());
//		//client->removeSession(session->getSessionID());
//	});
//
//	client->setRemoveSessionCallback([](Client*, Session* session) {
//		printf("[%d]删除连接\n", session->getSessionID());
//	});
//
//	client->setRecvCallback([](Client*, Session* session, char* data, uint32_t len)
//	{
//		char* msg = (char*)fc_malloc(len + 1);
//		memcpy(msg, data, len);
//		msg[len] = '\0';
//
//		if (!cmdResolve(msg, session->getSessionID()))
//		{
//			if (len < 100)
//			{
//				sprintf(szWriteBuf, "this is %d send data...", session->getSessionID());
//				if (strcmp(szWriteBuf, msg) != 0)
//				{
//					printf("收到非法消息:[%s]\n", msg);
//				}
//			}
//			else
//			{
//				printf("[%d]接收到消息:%d个字节\n", session->getSessionID(), len);
//			}
//		}
//		fc_free(msg);
//		if (rand() % 100 == 0)
//		{
//			session->disconnect();
//		}
//	});
//
//	for (int32_t i = 0; i < 10; ++i)
//	{
//		client->connect(KCP_CONNECT_IP, KCP_CONNECT_PORT, keyIndex++);
//	}
//
//	int32_t curCount = 0;
//	while (!clientClose)
//	{
//		client->updateFrame();
//
//		//自动发送
//		if (autosend)
//		{
//			curCount++;
//			if (curCount > 200)
//			{
//				for (int32_t i = 0; i < keyIndex; ++i)
//				{
//					sprintf(szWriteBuf, "this is %d send data...", i);
//					client->send(i, szWriteBuf, (uint32_t)strlen(szWriteBuf));
//				}
//				curCount = 0;
//			}
//		}
//
//		ThreadSleep(1);
//	}
//
//	delete client;
//
//	printMemInfo();
//
//	system("pause");
//}
//
//
//bool cmdResolve(char* cmd, uint32_t key)
//{
//	if (CMD_STRCMP("print"))
//	{
//		//打印内存信息
//		printMemInfo();
//	}
//	else if (CMD_STRCMP("dis"))
//	{
//		//断开连接
//		for (int32_t i = 0; i < keyIndex; ++i)
//		{
//			client->disconnect(i);
//		}
//	}
//	else if (CMD_STRCMP("add"))
//	{
//		//新添加连接
//		client->connect(KCP_CONNECT_IP, KCP_CONNECT_PORT, keyIndex++);
//	}
//	else if (CMD_STRCMP("close"))
//	{
//		client->closeClient();
//	}
//	else if (CMD_STRCMP("sendb"))
//	{
//		autosend = true;
//	}
//	else if (CMD_STRCMP("sende"))
//	{
//		autosend = false;
//	}
//	else if (CMD_STRCMP("closeclient"))
//	{
//		client->closeClient();
//	}
//	else if (CMD_STRCMP("big"))
//	{
//		int32_t msgLen = KCP_WRITE_MAX_LEN * 100 * 4;
//		char* szMsg = (char*)fc_malloc(msgLen);
//		for (int32_t i = 0; i < msgLen; ++i)
//		{
//			szMsg[i] = 'A';
//		}
//		szMsg[msgLen - 1] = '\0';
//		client->send(key, szMsg, (uint32_t)strlen(szMsg));
//		fc_free(szMsg);
//	}
//	else
//	{
//		return false;
//	}
//	return true;
//}
