
#include "net_uv/tcp/TCPServer.h"
#include <iostream>

NS_NET_UV_OPEN

#if OPEN_NET_UV_DEBUG == 1
#include "DbgHelp.h"
#pragma comment(lib, "DbgHelp.lib")
// ����Unhandled Exception�Ļص�����
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
	// ע���쳣��������
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

	printf("������˿ں�:");
	int port;
	std::cin >> port;

	TCPServer* svr = new TCPServer();
	svr->setStartCallback([](Server* svr, bool issuc) {
		if (issuc)
		{
			printf("�����������ɹ�\n");
		}
		else
		{
			printf("����������ʧ��\n");
		}
	});

	svr->setCloseCallback([](Server* svr)
	{
		printf("�������ر�\n");
		gServerStop = true;
	});

	svr->setNewConnectCallback([](Server* svr, Session* session)
	{
		allSession.push_back(session);
		//printf("%s:%d���������\n", session->getIp().c_str(), session->getPort());
	});

	svr->setRecvCallback([=](Server* svr, Session* session, char* data, unsigned int len)
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';

		if (len > 100)
		{
			NET_UV_LOG(NET_UV_L_INFO, "[%p]���յ���Ϣ%d���ֽ�", session, len);
			session->send(data, len);
		}
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
			session->send(data, len);
		}
		fc_free(msg);

		//printf("%s:%d �յ�%d���ֽ�����\n", session->getIp().c_str(), session->getPort(), len);
	});

	svr->setDisconnectCallback([=](Server* svr, Session* session)
	{
		auto it = std::find(allSession.begin(), allSession.end(), session);
		if (it != allSession.end())
		{
			allSession.erase(it);
		}
		//printf("%s:%d�뿪������\n", session->getIp().c_str(), session->getPort());
		if (session == controlClient)
		{
			controlClient = NULL;
		}
	});

	svr->startServer("0.0.0.0", port, false);

	while (!gServerStop)
	{
		svr->updateFrame();
	}
	delete svr;
	printf("-----------------------------\n");
	printMemInfo();
	printf("\n-----------------------------\n");

	system("pause");
}



#if OPEN_NET_UV_DEBUG == 1
// ����dump�ļ�
void CreateDempFile(LPCWSTR lpstrDumpFilePathName, EXCEPTION_POINTERS* pException)
{
	HANDLE hDumpFile = CreateFile(lpstrDumpFilePathName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	//dump��Ϣ
	MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
	dumpInfo.ExceptionPointers = pException;
	dumpInfo.ThreadId = GetCurrentThreadId();
	dumpInfo.ClientPointers = TRUE;
	// д��dump�ļ�����
	MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
	CloseHandle(hDumpFile);
}

LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException)
{
	CreateDempFile(TEXT("tcpServer.dmp"), pException);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif