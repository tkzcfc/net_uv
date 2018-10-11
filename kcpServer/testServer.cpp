
#include "net_uv/kcp/KCPServer.h"
#include <iostream>

NS_NET_UV_OPEN

#if OPEN_NET_UV_DEBUG == 1
#include "DbgHelp.h"
#pragma comment(lib, "DbgHelp.lib")
// 处理Unhandled Exception的回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException);
#endif


void sendString(Session* s, char* szMsg)
{
	s->send(szMsg, strlen(szMsg));
}

Session* controlClient = NULL;
bool gServerStop = false;


std::vector<Session*> allSession;

void main()
{
#if OPEN_NET_UV_DEBUG == 1
	// 注册异常处理函数
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

	printf("请输入端口号:");
	int port;
	std::cin >> port;

	KCPServer* svr = new KCPServer();
	svr->setStartCallback([](Server* svr, bool issuc) {
		if (issuc)
		{
			printf("服务器启动成功\n");
		}
		else
		{
			printf("服务器启动失败\n");
		}
	});

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

	svr->setRecvCallback([=](Server* svr, Session* session, char* data, unsigned int len)
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

	svr->startServer("0.0.0.0", port, false);

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



#if OPEN_NET_UV_DEBUG == 1
// 创建dump文件
void CreateDempFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS* pException)
{
	HANDLE hDumpFile = CreateFile(lpstrDumpFilePathName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//dump信息
	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
	dumpInfo.ExceptionPointers = pException;
	dumpInfo.ThreadId = GetCurrentThreadId();
	dumpInfo.ClientPointers = TRUE;
	// 写入dump文件内容
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
	CloseHandle(hDumpFile);
}

LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException)
{
	CreateDempFile(TEXT("kcpServer.dmp"), pException);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif
