//#include "FSTransfer.h"
//#include "net_uv/net_uv.h"
//#include <iostream>
//
//NS_NET_UV_OPEN
//
//struct UserData
//{
//	FSTransfer* fst;
//	Session* session;
//};
//
//void g_out_put_func(FSTransfer* fst, char* data, int32_t len)
//{
//	UserData* userdata = (UserData*)fst->getUserData();
//	userdata->session->send(data, len);
//}
//
//std::map<Session*, UserData> gAllUser;
//
//bool fstOnline(Session* session)
//{
//	auto it = gAllUser.find(session);
//	if (it == gAllUser.end())
//	{
//		UserData data;
//		data.fst = new FSTransfer();
//		
//		data.fst->setOutput(g_out_put_func);
//		data.session = session;
//		gAllUser[session] = data;
//
//		data.fst->setUserData(&gAllUser[session]);
//
//		return true;
//	}
//	return false;
//}
//
//void fstClear()
//{
//	for (auto& it : gAllUser)
//	{
//		delete it.second.fst;
//	}
//	gAllUser.clear();
//}
//
//FSTransfer* getFST(Session* session)
//{
//	auto it = gAllUser.find(session);
//	if (it != gAllUser.end())
//	{
//		return it->second.fst;
//	}
//	return NULL;
//}
//
//bool gRunLoop = true;
//
//void runClient()
//{
//	KCPClient* client = new KCPClient();
//
//	client->setClientCloseCallback([=](Client*)
//	{
//		gRunLoop = false;
//	});
//
//	client->setConnectCallback([=](Client*, Session* session, int32_t status)
//	{
//		if (status == 1)
//		{
//			bool newfst = fstOnline(session);
//			auto fst = getFST(session);
//			if (fst)
//			{
//				fst->online(true);
//
//				if (newfst)
//				{
//					char inputFileBuf[256];
//					std::cout << "请输入要传输的文件名:";
//					std::cin >> inputFileBuf;
//					if (fst->postFile(inputFileBuf) != 0)
//					{
//						client->closeClient();
//					}
//				}
//			}
//		}
//		else
//		{
//			auto fst = getFST(session);
//			if (fst)
//			{
//				fst->online(false);
//			}
//		}
//
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
//	});
//
//	client->setRemoveSessionCallback([](Client*, Session* session) {
//		printf("[%d]删除连接\n", session->getSessionID());
//	});
//
//	client->setRecvCallback([=](Client*, Session* session, char* data, uint32_t len)
//	{
//		auto fst = getFST(session);
//		if (fst)
//		{
//			fst->input(data, len);
//		}
//	});
//
//	client->connect("127.0.0.1", 1005, 0);
//
//	while (gRunLoop)
//	{
//		client->updateFrame();
//		ThreadSleep(1);
//	}
//	delete client;
//	fstClear();
//}
//
//
//void runServer()
//{
//	KCPServer* svr = new KCPServer();
//
//	svr->setCloseCallback([](Server* svr)
//	{
//		gRunLoop = false;
//		printf("服务器关闭\n");
//	});
//
//	svr->setNewConnectCallback([](Server* svr, Session* session)
//	{
//		fstOnline(session);
//		auto fst = getFST(session);
//		if (fst)
//		{
//			fst->online(true);
//		}
//	});
//
//	svr->setRecvCallback([=](Server* svr, Session* session, char* data, uint32_t len)
//	{
//		auto fst = getFST(session);
//		if (fst)
//		{
//			fst->input(data, len);
//		}
//	});
//
//	svr->setDisconnectCallback([=](Server* svr, Session* session)
//	{
//		auto fst = getFST(session);
//		if (fst)
//		{
//			fst->online(false);
//		}
//	});
//
//	bool issuc = svr->startServer("0.0.0.0", 1005, false);
//	if (issuc)
//	{
//		printf("服务器启动成功\n");
//	}
//	else
//	{
//		printf("服务器启动失败\n");
//	}
//	while (gRunLoop)
//	{
//		svr->updateFrame();
//		ThreadSleep(1);
//	}
//	delete svr;
//}
//
//void main()
//{
//	char type;
//	std::cin >> type;
//
//	if (type == 'c')
//	{
//		runClient();
//	}
//	else if (type == 's')
//	{
//		runServer();
//	}
//	printMemInfo();
//	system("pause");
//}
//

#include "FSTransferPool.h"
#include <iostream>

void main()
{
	FSTransferPool* pool = new FSTransferPool();;

	char type;
	std::cin >> type;

	if (type == 'c')
	{
		char inputFileBuf[256];
		std::cout << "请输入要传输的文件名:";
		std::cin >> inputFileBuf;
		pool->download("39.105.20.204", 1005, inputFileBuf);
	}
	else if (type == 's')
	{
		pool->listen("0.0.0.0", 1005, false);
	}

	while (true)
	{
		pool->updateFrame();
		ThreadSleep(1);
	}
	net_uv::printMemInfo();
	system("pause");
}