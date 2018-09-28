#include "net_uv/tcp/TCPServer.h"
#include "net_uv/tcp/TCPClient.h"

#include <iostream>

NS_NET_UV_OPEN

#if OPEN_NET_UV_DEBUG == 1
#include "DbgHelp.h"
#pragma comment(lib, "DbgHelp.lib")
// 处理Unhandled Exception的回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException);
#endif

//#define CONNECT_IP "127.0.0.1"
#define CONNECT_IP "www.kurumi.xin"
#define CONNECT_PORT 1234

#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)
#define CMD_STR_STR(v) (strstr(cmdBuf, v))


auto instance = new TCPClient();

char *cmdBuf = new char[1024];
char *szWriteBuf = new char[1024];


bool exitloop = false;

unsigned int control_key = 0;

void myPrintLog(int level, const char* log)
{
	if (level > NET_UV_L_HEART)
	{
		printf("[LOG]:%s\n", log);
	}
}

void main()
{
#if OPEN_NET_UV_DEBUG == 1
	// 注册异常处理函数
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

	setNetUVLogPrintFunc(myPrintLog);

	//启动自动重连
	instance->setAutoReconnect(true);
	//设置重连时间
	instance->setAutoReconnectTime(3.0f);

	memset(cmdBuf, 0, 1024);
	memset(szWriteBuf, 0, 1024);


	instance->setClientCloseCallback([](Client*) {
		printf("客户端已关闭\n");
		exitloop = true;
	});
	instance->setConnectCallback([=](Client*, Session* session, int status) {
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
	instance->setDisconnectCallback([](Client*, Session* session) {
		printf("[%d]断开连接\n", session->getSessionID());
	});
	instance->setRecvCallback([](Client*, Session* session, char* data, unsigned int len)
	{
		char* msg = (char*)fc_malloc(len + 1);
		memcpy(msg, data, len);
		msg[len] = '\0';
		NET_UV_LOG(NET_UV_L_INFO, "[%d]接收到消息:%s\n", session->getSessionID(), msg);
		fc_free(msg);
	});

	instance->connect(CONNECT_IP, CONNECT_PORT, control_key);

	while (!exitloop)
	{
		if (KEY_DOWN(VK_SPACE))
		{
			std::cin >> cmdBuf;
			if (CMD_STR_STR("send"))
			{
				NET_UV_LOG(NET_UV_L_INFO, "请输入要发送的内容:");
				std::cin >> szWriteBuf;
				instance->send(control_key, szWriteBuf, strlen(szWriteBuf));
			}
			else if (CMD_STR_STR("print"))
			{
				printMemInfo();
			}
			else if (CMD_STR_STR("close"))
			{
				instance->closeClient();
			}
		}
		instance->updateFrame();
		Sleep(1);
	}
	delete instance;

	printMemInfo();

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
	CreateDempFile(TEXT("tcpControl.dmp"), pException);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif