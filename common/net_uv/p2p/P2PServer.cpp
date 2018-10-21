#include "P2PServer.h"

NS_NET_UV_BEGIN

#define P2P_SVR_MSG_LEN_CHECK(MsgType) do { if(len != sizeof(MsgType)) return; } while (0);

P2PServer::P2PServer()
	: m_svr_startCall(nullptr)
	, m_svr_closeCall(nullptr)
	, m_userIDSpawn(0)
{
	alloc_server();
}

P2PServer::~P2PServer()
{
	free_server();
}

void P2PServer::startServer(const char* ip, unsigned int port, bool isIPV6/* = false*/)
{
	m_server->startServer(ip, port, isIPV6);
}

void P2PServer::updateFrame()
{
	m_server->updateFrame();
}

void P2PServer::alloc_server()
{
	m_server = (KCPServer*)fc_malloc(sizeof(KCPServer));
	new (m_server)KCPServer();

	m_server->setUserData(this);

	m_server->setStartCallback(std::bind(&P2PServer::on_server_StartCall, this, std::placeholders::_1, std::placeholders::_2));
	m_server->setCloseCallback(std::bind(&P2PServer::on_server_CloseCall, this, std::placeholders::_1));
	m_server->setNewConnectCallback(std::bind(&P2PServer::on_server_NewConnectCall, this, std::placeholders::_1, std::placeholders::_2));
	m_server->setRecvCallback(std::bind(&P2PServer::on_server_RecvCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_server->setDisconnectCallback(std::bind(&P2PServer::on_server_DisconnectCall, this, std::placeholders::_1, std::placeholders::_2));
}

void P2PServer::free_server()
{
	if (m_server)
	{
		m_server->~KCPServer();
		m_server = NULL;
	}
}

void P2PServer::on_server_StartCall(Server* svr, bool issuc)
{
	if (m_svr_startCall)
	{
		m_svr_startCall(svr, issuc);
	}
}

void P2PServer::on_server_CloseCall(Server* svr)
{
	if (m_svr_closeCall)
	{
		m_svr_closeCall(svr);
	}
}

void P2PServer::on_server_NewConnectCall(Server* svr, Session* session)
{
}

void P2PServer::on_server_RecvCall(Server* svr, Session* session, char* data, unsigned int len)
{
	P2PMessageBase* Msg = (P2PMessageBase*)data;

	void* MsgContent = data + sizeof(P2PMessageBase);
	unsigned int ContentLen = len - sizeof(P2PMessageBase);
	switch (Msg->ID)
	{
	case P2P_MSG_ID_C2S_REGISTER:// 注册结果
	{
		on_net_RegisterRequest(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_C2S_LOGOUT:	// 登出结果
	{
		on_net_LogoutRequest(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_C2S_GET_USER_LIST:		// 用户列表
	{
		on_net_UserListRequest(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_C2S_WANT_TO_CONNECT:// 连接请求
	{
		on_net_ConnectRequest(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_C2S_CONNECT_SUCCESS: // 连接成功
	{
		on_net_ConnectSuccessRequest(session, MsgContent, ContentLen);
	}break;
	default:
		break;
	}
}

void P2PServer::on_server_DisconnectCall(Server* svr, Session* session)
{
	auto it_user = m_userList.find(session->getSessionID());
	if (it_user != m_userList.end())
	{
		if (it_user->second.isOnline)
		{
			it_user->second.isOnline = false;
			it_user->second.overdueTime = iclock() + 20000;
			m_userListOverdueTime = true;
		}
		else
		{
			m_userList.erase(it_user);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void P2PServer::sendMsg(unsigned int sessionID, unsigned int msgID, char* data, unsigned int len)
{
	P2PMessageBase* msgData = (P2PMessageBase*)fc_malloc(sizeof(P2PMessageBase) + len);
	msgData->ID = msgID;

	if (len > 0)
	{
		memcpy(msgData + sizeof(P2PMessageBase), data, len);
	}
	m_server->send(sessionID, (char*)msgData, len);
	fc_free(msgData);
}

void P2PServer::send_RegisterResult(unsigned int sessionID, int code, unsigned int userId)
{
	P2PMessage_S2C_Register_Result data;
	data.code = code;
	data.userID = userId;
	sendMsg(sessionID, P2P_MSG_ID_S2C_REGISTER_RESULT, (char*)&data, sizeof(P2PMessage_S2C_Register_Result));
}

void P2PServer::send_UserListResult(unsigned int sessionID, int pageIndex, int pageMaxCount)
{
	int beginIndex = pageIndex * pageMaxCount;
	if (beginIndex > m_userList.size() - 1)
	{
		beginIndex = m_userList.size() - 1;
	}

	int endIndex = beginIndex + pageMaxCount;
	if (endIndex > m_userList.size() - 1)
	{
		endIndex = m_userList.size() - 1;
	}

	int curUserCount = endIndex - beginIndex;
	const int memSize = sizeof(P2PMessageBase) + sizeof(P2PMessage_S2C_UserList) + sizeof(P2PClientUserInfo) * curUserCount;

	char* data = (char*)fc_malloc(memSize);;
	memset(data, 0, memSize);

	// P2PMessageBase
	P2PMessageBase* msgBase = (P2PMessageBase*)data;
	msgBase->ID = P2P_MSG_ID_S2C_USER_LIST;

	// P2PMessage_S2C_UserList
	P2PMessage_S2C_UserList* userList = (P2PMessage_S2C_UserList*)(data + sizeof(P2PMessageBase));
	userList->curPageIndex = pageIndex + 1;
	userList->totalPageCount = m_userList.size() / pageMaxCount;
	if (m_userList.size() % pageMaxCount != 0)
	{
		userList->totalPageCount++;
	}
	userList->curUserCount = curUserCount;
	
	// P2PClientUserInfo
	P2PClientUserInfo* userInfoArr = (P2PClientUserInfo*)(data + sizeof(P2PMessage_S2C_UserList));

	if (curUserCount > 0)
	{
		int curIndex = 0;
		for (auto it : m_userList)
		{
			if (curIndex >= beginIndex)
			{
				if (curIndex >= endIndex)
				{
					break;
				}
				userInfoArr[curIndex - beginIndex].hasPassword = (it.second.passworld != 0);
				userInfoArr[curIndex - beginIndex].userID = it.second.userID;
				strcpy(userInfoArr[curIndex - beginIndex].szName, it.second.szName.c_str());
			}
			curIndex++;
		}
	}

	m_server->send(sessionID, data, memSize);
}

void P2PServer::send_ConnectResult(unsigned int sessionID, int code, unsigned int userId, unsigned int port, const std::string& ip)
{
	P2PMessage_C2S_WantToConnectResult data;
	memset(&data, 0, sizeof(P2PMessage_C2S_WantToConnectResult));

	data.code = code;
	data.userID = userId;
	data.port = port;
	strcpy(data.ip, ip.c_str());

	sendMsg(sessionID, P2P_MSG_ID_S2C_WANT_TO_CONNECT_RESULT, (char*)&data, sizeof(P2PMessage_C2S_WantToConnectResult));
}

void P2PServer::send_StartBurrow(unsigned int sessionID, const std::string& ip, unsigned int port, bool isIPV6)
{
	P2PMessage_S2C_StartBurrow data;
	memset(&data, 0, sizeof(P2PMessage_S2C_StartBurrow));

	data.port = port;
	data.isIPV6 = isIPV6;
	data.sessionID = sessionID;

	strcpy(data.ip, ip.c_str());

	sendMsg(sessionID, P2P_MSG_ID_S2C_START_BURROW, (char*)&data, sizeof(P2PMessage_S2C_StartBurrow));
}

void P2PServer::send_StopBurrow(unsigned int sessionID, const std::string& ip, unsigned int port)
{
	P2PMessage_S2C_StopBurrow data;
	memset(&data, 0, sizeof(P2PMessage_S2C_StopBurrow));

	data.port = port;
	strcpy(data.ip, ip.c_str());

	sendMsg(sessionID, P2P_MSG_ID_S2C_STOP_BURROW, (char*)&data, sizeof(P2PMessage_S2C_StopBurrow));
}

//////////////////////////////////////////////////////////////////////////
void P2PServer::on_net_RegisterRequest(Session* session, void* data, unsigned int len)
{
	P2P_SVR_MSG_LEN_CHECK(P2PMessage_C2S_Register);
	
	P2PMessage_C2S_Register* Msg = (P2PMessage_C2S_Register*)data;

	if (Msg->szName[P2P_CLIENT_USER_NAME_MAX_LEN - 1] != '0' || strlen(Msg->szName) <= 0)
	{
		// 名称非法
		send_RegisterResult(session->getSessionID(), 2, 0);
		return;
	}

	// 断线重连
	if (Msg->userID > 0)
	{
		auto it = m_userList.begin();
		for (; it != m_userList.end(); ++it)
		{
			if (it->second.userID == Msg->userID && it->second.szIP == session->getIp())
			{
				break;
			}
		}

		if (it != m_userList.end() && !it->second.isOnline)
		{
			auto userData = it->second;
			m_userList.erase(it);

			userData.isOnline = true;
			userData.passworld = Msg->password;
			userData.port = Msg->port;
			userData.szName = Msg->szName;
			userData.szIP = session->getIp();
			userData.overdueTime = 0;

			m_userList.insert(std::make_pair(session->getSessionID(), userData));
			// 注册成功
			send_RegisterResult(session->getSessionID(), 0, userData.userID);
			return;
		}
	}

	for (auto it = m_userList.begin(); it != m_userList.end(); ++it)
	{
		if (strcmp(it->second.szName.c_str(), Msg->szName) == 0)
		{			
			// 名称重复成功
			send_RegisterResult(session->getSessionID(), 1, 0);
			return;
		}
	}

	P2P_SVR_UserInfo userData;
	userData.isOnline = true;
	userData.overdueTime = 0;
	userData.passworld = Msg->password;
	userData.port = Msg->port;
	userData.szIP = session->getIp();
	userData.szName = Msg->szName;
	userData.userID = createNewUserID();

	m_userList.insert(std::make_pair(session->getSessionID(), userData));

	// 注册成功
	send_RegisterResult(session->getSessionID(), 0, userData.userID);
}

void P2PServer::on_net_LogoutRequest(Session* session, void* data, unsigned int len)
{
	auto it = m_userList.find(session->getSessionID());
	if (it != m_userList.end())
	{
		m_userList.erase(session->getSessionID());
	}
	// 返回结果
	sendMsg(session->getSessionID(), P2P_MSG_ID_S2C_LOGOUT_RESULT, NULL, 0);
}

void P2PServer::on_net_UserListRequest(Session* session, void* data, unsigned int len)
{
	P2P_SVR_MSG_LEN_CHECK(P2PMessage_C2S_GetUserList);

	P2PMessage_C2S_GetUserList* Msg = (P2PMessage_C2S_GetUserList*)data;

	static const int pageMaxCount = 10;

	if (Msg->pageIndex <= 0)
	{
		int totalPageCount = m_userList.size() / pageMaxCount;
		if (m_userList.size() % pageMaxCount != 0)
		{
			totalPageCount++;
		}
		for (int i = 0; i < totalPageCount; ++i)
		{
			send_UserListResult(session->getSessionID(), i, pageMaxCount);
		}
	}
	else
	{
		send_UserListResult(session->getSessionID(), Msg->pageIndex - 1, pageMaxCount);
	}
}

void P2PServer::on_net_ConnectRequest(Session* session, void* data, unsigned int len)
{
	P2P_SVR_MSG_LEN_CHECK(P2PMessage_C2S_WantToConnect);

	P2PMessage_C2S_WantToConnect* Msg = (P2PMessage_C2S_WantToConnect*)data;
	
	for (auto it : m_userList)
	{
		if (it.second.userID == Msg->userID)
		{
			if (it.second.isOnline)
			{
				// 开始打洞指令
				send_StartBurrow(session->getSessionID(), session->getIp(), session->getPort(), false);
				// 返回连接请求
				send_ConnectResult(session->getSessionID(), 0, Msg->userID, it.second.port, it.second.szIP);
			}
			else
			{
				// 对方离线
				send_ConnectResult(session->getSessionID(), 2, 0, 0, "");
			}
			return;
		}
	}

	// 用户不存在
	send_ConnectResult(session->getSessionID(), 1, 0, 0, "");
}

void P2PServer::on_net_ConnectSuccessRequest(Session* session, void* data, unsigned int len)
{
	P2P_SVR_MSG_LEN_CHECK(P2PMessage_C2S_ConnectSuccess);

	P2PMessage_C2S_ConnectSuccess* Msg = (P2PMessage_C2S_ConnectSuccess*)data;
	
	for (auto& it : m_userList)
	{
		if (it.second.userID == Msg->userID)
		{
			send_StopBurrow(it.first, session->getIp(), session->getPort());
		}
	}
}

NS_NET_UV_END
