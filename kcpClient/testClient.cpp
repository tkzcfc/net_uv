#include "net_uv/net_uv.h"

NS_NET_UV_OPEN

#if OPEN_NET_UV_DEBUG == 1
#include "DbgHelp.h"
#pragma comment(lib, "DbgHelp.lib")
// 处理Unhandled Exception的回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException);
#endif

#define CONNECT_IP "127.0.0.1"
//#define CONNECT_IP "www.kurumi.xin"
#define CONNECT_PORT 1002

bool autosend = true;
uint32_t keyIndex = 0;
char *szWriteBuf = new char[1024];

bool clientClose = false;


// 命令解析
bool cmdResolve(char* cmd, uint32_t key);

KCPClient* client = new KCPClient();

void main()
{
#if OPEN_NET_UV_DEBUG == 1
	// 注册异常处理函数
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

	client->setClientCloseCallback([](Client*)
	{
		clientClose = true;
		printf("客户端已关闭\n");
	});

	client->setConnectCallback([=](Client*, Session* session, int32_t status)
	{
		if (status == 0)
		{
			printf("[%d]连接失败\n", session->getSessionID());
		}
		else if (status == 1)
		{
			printf("[%d]连接成功\n", session->getSessionID());
		}
		else if (status == 2)
		{
			printf("[%d]连接超时\n", session->getSessionID());
		}
	});

	client->setDisconnectCallback([=](Client*, Session* session) {
		printf("[%d]断开连接\n", session->getSessionID());
		//client->removeSession(session->getSessionID());
	});

	client->setRemoveSessionCallback([](Client*, Session* session) {
		printf("[%d]删除连接\n", session->getSessionID());
	});

	client->setRecvCallback([](Client*, Session* session, char* data, uint32_t len)
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';

		if (!cmdResolve(msg, session->getSessionID()))
		{
			if (len < 100)
			{
				sprintf(szWriteBuf, "this is %d send data...", session->getSessionID());
				if (strcmp(szWriteBuf, msg) != 0)
				{
					printf("收到非法消息:[%s]\n", msg);
				}
			}
			else
			{
				printf("[%d]接收到消息:%d个字节\n", session->getSessionID(), len);
			}
		}
		fc_free(msg);
		if (rand() % 100 == 0)
		{
			session->disconnect();
		}
	});

	for (int32_t i = 0; i < 10; ++i)
	{
		client->connect(CONNECT_IP, CONNECT_PORT, keyIndex++);
	}

	int32_t curCount = 0;
	while (!clientClose)
	{
		client->updateFrame();

		//自动发送
		if (autosend)
		{
			curCount++;
			if (curCount > 200)
			{
				for (int32_t i = 0; i < keyIndex; ++i)
				{
					sprintf(szWriteBuf, "this is %d send data...", i);
					client->send(i, szWriteBuf, (uint32_t)strlen(szWriteBuf));
				}
				curCount = 0;
			}
		}

		ThreadSleep(1);
	}

	delete client;

	printMemInfo();

	system("pause");
}


#define CMD_STRCMP(v) (strcmp(cmd, v) == 0)

bool cmdResolve(char* cmd, uint32_t key)
{
	if (CMD_STRCMP("print"))
	{
		//打印内存信息
		printMemInfo();
	}
	else if (CMD_STRCMP("dis"))
	{
		//断开连接
		for (int32_t i = 0; i < keyIndex; ++i)
		{
			client->disconnect(i);
		}
	}
	else if (CMD_STRCMP("add"))
	{
		//新添加连接
		client->connect(CONNECT_IP, CONNECT_PORT, keyIndex++);
	}
	else if (CMD_STRCMP("close"))
	{
		client->closeClient();
	}
	else if (CMD_STRCMP("sendb"))
	{
		autosend = true;
	}
	else if (CMD_STRCMP("sende"))
	{
		autosend = false;
	}
	else if (CMD_STRCMP("closeclient"))
	{
		client->closeClient();
	}
	else if (CMD_STRCMP("big"))
	{
		int32_t msgLen = KCP_WRITE_MAX_LEN * 100 * 4;
		char* szMsg = (char*)fc_malloc(msgLen);
		for (int32_t i = 0; i < msgLen; ++i)
		{
			szMsg[i] = 'A';
		}
		szMsg[msgLen - 1] = '\0';
		client->send(key, szMsg, (uint32_t)strlen(szMsg));
		fc_free(szMsg);
	}
	else
	{
		return false;
	}
	return true;
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
	CreateDempFile(TEXT("kcpClient.dmp"), pException);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif
