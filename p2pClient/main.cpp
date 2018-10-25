#include "net_uv/net_uv.h"
#include <iostream>

NS_NET_UV_OPEN

#if OPEN_NET_UV_DEBUG == 1
#include "DbgHelp.h"
#pragma comment(lib, "DbgHelp.lib")
// 处理Unhandled Exception的回调函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException);
#endif

bool runLoop = true;

std::vector<P2PClientUserInfo> allUserInfoList;

P2PClient* p2pClient = NULL;

void runCMD();

void main()
{
#if OPEN_NET_UV_DEBUG == 1
	// 注册异常处理函数
	SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);
#endif

	p2pClient = new P2PClient();

	p2pClient->set_p2p_ConnectResultCallback([=](P2PClient* client, int status, unsigned int userId)
	{
		/// status: 0 连接成功 1连接失败，用户不存在 2连接失败，与中央服务器连接失败 3连接失败，与P2PClient的本地服务器连接失败
		if (status == 0)
		{
			printf("连接[%d] 连接成功\n", userId);
		}
		else if (status == 1)
		{
			printf("连接[%d] 连接失败，用户不存在\n", userId);
		}
		else if (status == 2)
		{
			printf("连接[%d] 连接失败，与中央服务器连接失败\n", userId);
		}
		else if (status == 3)
		{
			printf("连接[%d] 连接失败，与P2PClient的本地服务器连接失败\n", userId);
		}
	});
	p2pClient->set_p2p_RegisterResultCallback([](P2PClient* client, int status) {
		if (status == 0)
		{
			printf("注册到中央服务器成功\n");
		}
		else if (status == 1)
		{
			printf("注册到中央服务器失败，名称重复\n");
		}
	});
	p2pClient->set_p2p_UserListResultCallback([](P2PClient* client, P2PMessage_S2C_UserList* listInfo, const std::vector<P2PClientUserInfo>&userList) 
	{
		if (listInfo->curPageIndex == 1)
		{
			allUserInfoList.clear();
		}
		for (auto &it : userList)
		{
			allUserInfoList.push_back(it);
		}
		if (listInfo->curPageIndex == listInfo->totalPageCount)
		{
			printf("\n--------------------------------------\n");
			int index = 0;
			for (auto& it : allUserInfoList)
			{
				index++;
				printf("%02d userID:[%d]\t needPassword:[%s]\t name:[%s]\n", index, it.userID, it.hasPassword ? "true" : "false", it.szName);
			}
			printf("--------------------------------------\n");
		}
	});


	p2pClient->set_client_ConnectCallback([](Client* client, Session* session, int status) 
	{
		if (status == 0)
		{
			printf("session[%d] 连接失败\n", session->getSessionID());
		}
		else if (status == 1)
		{
			printf("session[%d] 连接成功\n", session->getSessionID());
		}
		else if (status == 2)
		{
			printf("session[%d] 连接超时\n", session->getSessionID());
		}
	});
	p2pClient->set_client_DisconnectCallback([](Client* client, Session* session) 
	{
		printf("session[%d] 断开连接\n", session->getSessionID());
	});
	p2pClient->set_client_RecvCallback([](Client* client, Session* session, char* data, unsigned int len) 
	{
		printf("session[%d] 收到%d个字节\n", session->getSessionID(), len);
	});
	p2pClient->set_client_ClientCloseCallback([](Client* client) 
	{
		printf("客户端关闭\n");
	});
	p2pClient->set_client_RemoveSessionCallback([](Client* client, Session* session) 
	{
		printf("删除session[%d]\n", session->getSessionID());
	});


	p2pClient->set_server_StartCallback([](Server* svr, bool suc) 
	{
		printf("本地服务器启动%s\n", suc ? "成功" : "失败");
	});
	p2pClient->set_server_CloseCallback([](Server* svr)
	{
		printf("本地服务器关闭\n");
	});
	p2pClient->set_server_NewConnectCallback([](Server* svr, Session* session) 
	{
		printf("session[%d] ip:%s port:%d进入服务器\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
	});
	p2pClient->set_server_RecvCallback([](Server* svr, Session* session, char* data, unsigned int len) 
	{
		printf("服务器收到 session[%d] %d个字节\n", session->getSessionID(), len);
	});
	p2pClient->set_server_DisconnectCallback([](Server* svr, Session* session)
	{
		printf("session[%d] ip:%s port:%d离开服务器\n", session->getSessionID(), session->getIp().c_str(), session->getPort());
	});

	p2pClient->connectToCenterServer("www.kurumi.xin", 1003);
	//p2pClient->connectToCenterServer("127.0.0.1", 1003);

	while (runLoop)
	{
		runCMD();
		p2pClient->updateFrame();
		Sleep(1);
	}

	delete p2pClient;

	printMemInfo();

	system("pause");
}

#define KEY_DOWN(VK_NONAME) ((GetAsyncKeyState(VK_NONAME) & 0x8000) ? 1:0)

std::vector<std::string> split(const std::string& str, const std::string& delim) {
	std::vector<std::string> res;
	if ("" == str) return res;
	//先将要切割的字符串从string类型转换为char*类型
	char * strs = new char[str.length() + 1]; //不要忘了
	strcpy(strs, str.c_str());

	char * d = new char[delim.length() + 1];
	strcpy(d, delim.c_str());

	char *p = strtok(strs, d);
	while (p) {
		std::string s = p; //分割得到的字符串转换为string类型
		res.push_back(s); //存入结果数组
		p = strtok(NULL, d);
	}

	return res;
}

bool isNumber(const std::string& str)
{
	for (int i = 0; i < str.size(); ++i)
	{
		if (str[i] < '0' || str[i] > '9')
			return false;
	}
	return true;
}


char szBuf[512];
int sessionID_Count = 0;
void runCMD()
{
	if (KEY_DOWN(VK_SPACE) && GetConsoleWindow() == GetForegroundWindow())
	{
		printf("请输入命令:");
		std::cin.getline(szBuf, 512);
		std::vector<std::string> strlist = split(szBuf, " ");
		if (strlist.size() > 0)
		{
			if (strlist[0] == "getlist")
			{
				p2pClient->getUserList(0);
			}
			else if(strlist[0] == "newconnect" && strlist.size() > 1)
			{
				if (isNumber(strlist[1]))
				{
					unsigned int userID = atoi(strlist[1].c_str());
					for (auto &it : allUserInfoList)
					{
						if (userID == it.userID)
						{
							if (!it.hasPassword)
							{
								sessionID_Count++;
								if (!p2pClient->client_connect(userID, sessionID_Count, 0))
								{
									printf("创建连接失败\n");
								}
								else
								{
									printf("创建连接成功\n");
								}
								return;
							}
							while (true)
							{
								printf("请输入连接密码:\n");
								char szPassword[64];
								std::cin >> szPassword;
								if (isNumber(szPassword))
								{
									sessionID_Count++;
									if (!p2pClient->client_connect(userID, sessionID_Count, atoi(szPassword)))
									{
										printf("创建连接失败\n");
									}
									else
									{
										printf("创建连接成功\n");
									}
									return;
								}
								else
								{
									printf("密码只能为数字\n");
								}
							}
						}
					}
					printf("找不到userID : %u\n", userID);
				}
				else
				{
					printf("命令不正确，连接命令格式为:newconnect [userID]");
				}
			}
			else if (strlist[0] == "connect" && strlist.size() > 2)
			{
				if (isNumber(strlist[1]) && isNumber(strlist[2]))
				{
					unsigned int userID = atoi(strlist[1].c_str());
					unsigned int sessionID = atoi(strlist[2].c_str());
					for (auto &it : allUserInfoList)
					{
						if (userID == it.userID)
						{
							if (!it.hasPassword)
							{
								sessionID_Count++;
								if (!p2pClient->client_connect(userID, sessionID_Count, 0))
								{
									printf("创建连接失败\n");
								}
								else
								{
									printf("创建连接成功\n");
								}
								return;
							}
							while (true)
							{
								printf("请输入连接密码:\n");
								char szPassword[64];
								std::cin >> szPassword;
								if (isNumber(szPassword))
								{
									if (!p2pClient->client_connect(userID, sessionID, atoi(szPassword)))
									{
										printf("创建连接失败\n");
									}
									else
									{
										printf("创建连接成功\n");
									}
									return;
								}
								else
								{
									printf("密码只能为数字\n");
								}
							}
						}
					}
					printf("找不到userID : %u\n", userID);
				}
				else
				{
					printf("命令不正确，连接命令格式为:connect [userID] [sessionID]");
				}
			}
			else if (strlist[0] == "dis" && strlist.size() > 1)
			{
				if (isNumber(strlist[1]))
				{
					unsigned int sessionID = atoi(strlist[1].c_str());
					p2pClient->client_disconnect(sessionID);
				}
				else
				{
					printf("命令不正确，连接命令格式为:dis [sessionID]");
				}
			}
			else if (strlist[0] == "send" && strlist.size() > 2)
			{
				if (isNumber(strlist[1]))
				{
					unsigned int sessionID = atoi(strlist[1].c_str());
					p2pClient->client_send(sessionID, (char*)strlist[2].c_str(), strlist[2].size());
				}
				else
				{
					printf("命令不正确，连接命令格式为:send [sessionID] data");
				}
			}
			else if (strlist[0] == "startsvr" && strlist.size() > 2)
			{
				if (isNumber(strlist[2]))
				{
					std::string name = strlist[1];
					unsigned int password = atoi(strlist[2].c_str());
					if (!p2pClient->server_startServer(name.c_str(), password))
					{
						printf("启动失败\n");
					}
				}
				else
				{
					printf("命令不正确，连接命令格式为:startsvr name password");
				}
			}
			else if (strlist[0] == "stopsvr")
			{
				p2pClient->server_stopServer();
			}
			else
			{
				printf("无效命令\n");
			}
		}
	}
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
	CreateDempFile(TEXT("p2pClient.dmp"), pException);
	return EXCEPTION_EXECUTE_HANDLER;
}
#endif
