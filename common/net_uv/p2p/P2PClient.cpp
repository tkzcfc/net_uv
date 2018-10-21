#include "P2PClient.h"

NS_NET_UV_BEGIN

// 打洞数据持续发送时间
#define P2P_SEND_BURROW_DATA_DURATION 3000

// 打洞数据发送间隔时间(ms)
#define P2P_SEND_BURROW_DATA_SPACE_TIME 100

// 打洞数据最大并发数量(超过该数量则不进行打洞处理)
#define P2P_SEND_BURROW_DATA_MAX_COUNT 10


#define P2P_MSG_LEN_CHECK(MsgType) do { if(len != sizeof(MsgType)) return; } while (0);

P2PClient::P2PClient()
	: m_isConnectP2PSVR(false)
	, m_svrStartSuccess(false)
	, m_server(nullptr)
	, m_client(nullptr)
	, m_svr_closeCall(nullptr)
	, m_svr_startCall(nullptr)
	, m_svr_newConnectCall(nullptr)
	, m_svr_recvCall(nullptr)
	, m_svr_disconnectCall(nullptr)
	, m_client_connectCall(nullptr)
	, m_client_disconnectCall(nullptr)
	, m_client_recvCall(nullptr)
	, m_client_clientCloseCall(nullptr)
	, m_client_removeSessionCall(nullptr)
	, m_p2p_connectResultCall(nullptr)
	, m_p2p_registerResultCall(nullptr)
	, m_p2p_getUserListResultCall(nullptr)
	, m_password(0)
	, m_userID(0)
{
	m_burrowData = kcp_making_connect_packet();
	alloc_kcpClient();
}

P2PClient::~P2PClient()
{
	if (m_server)
	{
		m_server->stopServer();
	}
	m_client->closeClient();

	free_kcpServer();
	free_kcpClient();
	clear_burrowData();
}

void P2PClient::connectToCenterServer(const char* ip, unsigned int port)
{
	m_client->connect(ip, port, P2P_CONNECT_SVR_SESSION_ID);
}

bool P2PClient::server_startServer(const char* name, int password/* = 0*/, const char* ip/* = "0.0.0.0"*/, unsigned int port/* = 0*/, bool isIPV6/* = false*/)
{
	if (strlen(name) > P2P_CLIENT_USER_NAME_MAX_LEN - 1)
		return false;
	m_name = name;
	m_password = password;
	if (m_server == NULL)
	{
		alloc_kcpServer();
		m_server->startServer(ip, port, isIPV6);
	}
	try_send_RegisterMsg();
	return true;
}

bool P2PClient::server_stopServer()
{
	if (m_server == NULL)
		return false;
	return m_server->stopServer();
}

bool P2PClient::client_connect(unsigned int userID, unsigned int sessionID, int password, bool autoConnect/* = false*/)
{
	assert(sessionID != P2P_CONNECT_SVR_SESSION_ID);
	if (!m_isConnectP2PSVR)
		return false;

	auto it = m_connectInfoMap.find(sessionID);
	if (it != m_connectInfoMap.end())
	{
		if (it->second.userID != userID)
			return false;
	}
	else
	{
		ConnectInfoData data;
		data.userID = userID;
		data.autoConnect = autoConnect;
		data.password = password;
		m_connectInfoMap.insert(std::make_pair(sessionID, data));
	}

	send_WantConnectMsg(userID, password);

	return true;
}

void P2PClient::client_setAutoReconnectBySessionID(unsigned int sessionID, bool isAuto)
{
	m_client->setAutoReconnectBySessionID(sessionID, isAuto);

	auto it = m_connectInfoMap.find(sessionID);
	if (it != m_connectInfoMap.end())
	{
		it->second.autoConnect = isAuto;
	}
}

bool P2PClient::getUserList(int pageIndex)
{
	if (!m_isConnectP2PSVR)
		return false;

	send_GetUserList(pageIndex);

	return true;
}

void P2PClient::updateFrame()
{
	if (m_server)
	{
		m_server->updateFrame();
	}
	if (m_client)
	{
		m_client->updateFrame();
	}
	do_burrowLogic();
}

void P2PClient::alloc_kcpServer()
{
	free_kcpServer();
	m_server = (KCPServer*)fc_malloc(sizeof(KCPServer));
	new (m_server)KCPServer();

	m_server->setUserData(this);

	m_server->setStartCallback(std::bind(&P2PClient::on_server_StartCall, this, std::placeholders::_1, std::placeholders::_2));
	m_server->setCloseCallback(std::bind(&P2PClient::on_server_CloseCall, this, std::placeholders::_1));
	m_server->setNewConnectCallback(std::bind(&P2PClient::on_server_NewConnectCall, this, std::placeholders::_1, std::placeholders::_2));
	m_server->setRecvCallback(std::bind(&P2PClient::on_server_RecvCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_server->setDisconnectCallback(std::bind(&P2PClient::on_server_DisconnectCall, this, std::placeholders::_1, std::placeholders::_2));
}

void P2PClient::alloc_kcpClient()
{
	free_kcpClient();

	m_client = (KCPClient*)fc_malloc(sizeof(KCPClient));
	new (m_client)KCPClient();

	m_client->setUserData(this);

	m_client->setClientCloseCallback(std::bind(&P2PClient::on_client_ClientCloseCall, this, std::placeholders::_1));
	m_client->setConnectCallback(std::bind(&P2PClient::on_client_ConnectCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_client->setRecvCallback(std::bind(&P2PClient::on_client_RecvCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_client->setDisconnectCallback(std::bind(&P2PClient::on_client_DisconnectCall, this, std::placeholders::_1, std::placeholders::_2));
}

void P2PClient::free_kcpServer()
{
	if (m_server == NULL)
		return;

	m_server->~KCPServer();
	fc_free(m_server);
	m_server = NULL;
}

void P2PClient::free_kcpClient()
{
	if (m_client == NULL)
		return;

	m_client->~KCPClient();
	fc_free(m_client);
	m_client = NULL;
}

void P2PClient::do_burrowLogic()
{
	if (m_server == NULL || !m_svrStartSuccess || m_allBurrowDataArr.empty())
		return;

	IUINT32 clock = iclock();
	for (auto it = m_allBurrowDataArr.begin(); it != m_allBurrowDataArr.end(); )
	{
		if (it->endTime >= clock)
		{
			fc_free(it->addr);
			it = m_allBurrowDataArr.erase(it);
		}
		else
		{
			if (clock - it->lastSendTime > P2P_SEND_BURROW_DATA_SPACE_TIME)
			{
				send_BurrowData(it->addr, it->addrlen);
			}
			it++;
		}
	}
}

void P2PClient::remove_burrowData(const char* ip, unsigned port)
{
	for (auto it = m_allBurrowDataArr.begin(); it != m_allBurrowDataArr.end(); it++)
	{
		if (strcmp(it->ip.c_str(), ip) == 0 && it->port == port)
		{
			fc_free(it->addr);
			m_allBurrowDataArr.erase(it);
			break;
		}
	}
}

void P2PClient::clear_burrowData()
{
	for (auto & it : m_allBurrowDataArr)
	{
		fc_free(it.addr);
	}
	m_allBurrowDataArr.clear();
}

//////////////////////////////////////////////////////////////////////////

void P2PClient::sendMsg(unsigned int sessionID, unsigned int msgID, char* data, unsigned int len)
{
	P2PMessageBase* msgData = (P2PMessageBase*)fc_malloc(sizeof(P2PMessageBase) + len);
	msgData->ID = msgID;

	if (len > 0)
	{
		memcpy(msgData + sizeof(P2PMessageBase), data, len);
	}
	m_client->send(sessionID, (char*)msgData, len);
	fc_free(msgData);
}

void P2PClient::try_send_RegisterMsg()
{
	if (m_svrStartSuccess && m_isConnectP2PSVR)
	{
		send_RegisterMsg();
	}
}

void P2PClient::send_RegisterMsg()
{
	P2PMessage_C2S_Register data;
	data.password = m_password;
	data.port = m_server->getListenPort();
	data.userID = m_userID;
	strcpy(data.szName, m_name.c_str());

	sendMsg(P2P_CONNECT_SVR_SESSION_ID, P2P_MSG_ID_C2S_REGISTER, (char*)&data, sizeof(P2PMessage_C2S_Register));
}

void P2PClient::send_WantConnectMsg(unsigned int userId, int password)
{
	P2PMessage_C2S_WantToConnect data;
	data.userID = userId;
	data.password = password;
	sendMsg(P2P_CONNECT_SVR_SESSION_ID, P2P_MSG_ID_C2S_WANT_TO_CONNECT, (char*)&data, sizeof(P2PMessage_C2S_WantToConnect));
}

void P2PClient::send_GetUserList(int pageIndex)
{
	P2PMessage_C2S_GetUserList data;
	data.pageIndex = pageIndex;
	sendMsg(P2P_CONNECT_SVR_SESSION_ID, P2P_MSG_ID_C2S_GET_USER_LIST, (char*)&data, sizeof(P2PMessage_C2S_GetUserList));
}

void P2PClient::send_BurrowData(struct sockaddr* addr, unsigned int addrlen)
{
	if (m_server && m_svrStartSuccess)
	{
		m_server->svrUdpSend(addr, addrlen, (char*)m_burrowData.c_str(), m_burrowData.length());
	}
}

void P2PClient::send_ConnectSuccess(unsigned int userId)
{
	P2PMessage_C2S_ConnectSuccess data;
	data.userID = userId;
	sendMsg(0, P2P_MSG_ID_C2S_CONNECT_SUCCESS, (char*)&data, sizeof(P2PMessage_C2S_ConnectSuccess));
}

void P2PClient::on_client_ConnectCall(Client* client, Session* session, int status)
{
	if (session->getSessionID() == P2P_CONNECT_SVR_SESSION_ID)
	{
		m_isConnectP2PSVR = (status == 1);
		try_send_RegisterMsg();
	}
	else
	{
		if (status == 1)
		{
			// 通知中央服务器已连接
			auto it = m_connectInfoMap.find(session->getSessionID());
			if (it != m_connectInfoMap.end())
			{
				send_ConnectSuccess(it->second.userID);
			}
		}
		else
		{
			auto it = m_connectInfoMap.find(session->getSessionID());
			if (it != m_connectInfoMap.end() && it->second.autoConnect)
			{
				send_WantConnectMsg(it->second.userID, it->second.password);
			}
		}
		m_client_connectCall(client, session, status);
	}
}

void P2PClient::on_client_DisconnectCall(Client* client, Session* session)
{
	if (session->getSessionID() == P2P_CONNECT_SVR_SESSION_ID)
	{
		m_isConnectP2PSVR = false;
	}
	else
	{
		for (auto &it : m_connectInfoMap)
		{
			if (it.first == session->getSessionID() && it.second.autoConnect)
			{
				send_WantConnectMsg(it.second.userID, it.second.password);
				break;
			}
		}
		m_client_disconnectCall(client, session);
	}
}

void P2PClient::on_client_RecvCallback(Client* client, Session* session, char* data, unsigned int len)
{
	if (session->getSessionID() != P2P_CONNECT_SVR_SESSION_ID)
	{
		m_client_recvCall(client, session, data, len);
		return;
	}

	P2PMessageBase* Msg = (P2PMessageBase*)data;
	
	void* MsgContent = data + sizeof(P2PMessageBase);
	unsigned int ContentLen = len - sizeof(P2PMessageBase);
	switch (Msg->ID)
	{
	case P2P_MSG_ID_S2C_REGISTER_RESULT:// 注册结果
	{
		on_net_RegisterResult(MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_LOGOUT_RESULT:	// 登出结果
	{
		m_server->stopServer();
	}break;
	case P2P_MSG_ID_S2C_USER_LIST:		// 用户列表
	{
		on_net_UserListResult(MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_WANT_TO_CONNECT_RESULT:// 连接请求结果
	{
		on_net_ConnectResult(MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_START_BURROW: // 开始打洞指令
	{
		on_net_StartBurrow(MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_STOP_BURROW: // 停止打洞指令
	{
		on_net_StopBurrow(MsgContent, ContentLen);
	}break;
	default:
		break;
	}
}

void P2PClient::on_client_ClientCloseCall(Client* client)
{
	m_isConnectP2PSVR = false;
	if (m_client_clientCloseCall)
	{
		m_client_clientCloseCall(client);
	}
}

void P2PClient::on_server_StartCall(Server* svr, bool issuc)
{
	m_svrStartSuccess = issuc;
	try_send_RegisterMsg();
	m_svr_startCall(svr, issuc);
}

void P2PClient::on_server_CloseCall(Server* svr)
{
	m_svrStartSuccess = false;
	if (m_svr_closeCall)
	{
		m_svr_closeCall(svr);
	}
	free_kcpServer();
}

void P2PClient::on_server_NewConnectCall(Server* svr, Session* session)
{
	m_svr_newConnectCall(svr, session);
}

void P2PClient::on_server_RecvCall(Server* svr, Session* session, char* data, unsigned int len)
{
	m_svr_recvCall(svr, session, data, len);
}

void P2PClient::on_server_DisconnectCall(Server* svr, Session* session)
{
	m_svr_disconnectCall(svr, session);
}

//////////////////////////////////////////////////////////////////////////

void P2PClient::on_net_RegisterResult(void* data, unsigned int len)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_Register_Result);

	P2PMessage_S2C_Register_Result* result = (P2PMessage_S2C_Register_Result*)data;
	m_userID = result->userID;
	if (m_p2p_registerResultCall)
	{
		m_p2p_registerResultCall(this, result->code);
	}
}

void P2PClient::on_net_UserListResult(void* data, unsigned int len)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_UserList);

	if (m_p2p_getUserListResultCall == nullptr)
		return;

	P2PMessage_S2C_UserList* list = (P2PMessage_S2C_UserList*)data;

	char* pData = (char*)data;
	P2PClientUserInfo* userInfoArr = (P2PClientUserInfo*)(pData + sizeof(P2PMessage_S2C_UserList));

	std::vector<P2PClientUserInfo> userList;
	for (int i = 0; i < list->curUserCount; ++i)
	{
		userList.push_back(userInfoArr[i]);
	}
	m_p2p_getUserListResultCall(this, list, userList);
}

void P2PClient::on_net_ConnectResult(void* data, unsigned int len)
{
	P2P_MSG_LEN_CHECK(P2PMessage_C2S_WantToConnectResult);

	P2PMessage_C2S_WantToConnectResult* result = (P2PMessage_C2S_WantToConnectResult*)data;
	if (result->code == 0)
	{
		for (auto &it : m_connectInfoMap)
		{
			if (it.second.userID == result->userID)
			{
				m_client->connect(result->ip, result->port, it.first);
				m_client->setAutoReconnectBySessionID(it.first, it.second.autoConnect);
				break;
			}
		}
	}
	else
	{
		for (auto it = m_connectInfoMap.begin(); it != m_connectInfoMap.end(); ++it)
		{
			if (it->second.userID == result->userID)
			{
				m_client->removeSession(it->first);
				m_connectInfoMap.erase(it);
				break;
			}
		}
	}

	if (m_p2p_connectResultCall)
	{
		m_p2p_connectResultCall(this, result->code, result->userID);
	}
}

void P2PClient::on_net_StartBurrow(void* data, unsigned int len)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_StartBurrow);

	// 本机服务器已关闭
	if (m_server == NULL || !m_svrStartSuccess)
	{
		return;
	}

	P2PMessage_S2C_StartBurrow* result = (P2PMessage_S2C_StartBurrow*)data;

	for (auto& it : m_allBurrowDataArr)
	{
		if (strcmp(it.ip.c_str(), result->ip) == 0 && it.port == result->port)
		{
			IUINT32 clock = iclock();
			it.endTime = clock + P2P_SEND_BURROW_DATA_DURATION;
			return;
		}
	}

	// 本机繁忙
	if (m_allBurrowDataArr.size() > P2P_SEND_BURROW_DATA_MAX_COUNT)
	{
		return;
	}
	unsigned int addrlen = 0;
	struct sockaddr* addr = net_getsocketAddr_no(result->ip, result->port, result->isIPV6, &addrlen);
	// 地址解析失败
	if (addr == NULL)
	{
		return;
	}

	IUINT32 clock = iclock();

	BurrowData burrowData;
	burrowData.addr = addr;
	burrowData.addrlen = addrlen;
	burrowData.ip = result->ip;
	burrowData.port = result->port;
	burrowData.endTime = clock + P2P_SEND_BURROW_DATA_DURATION;
	burrowData.lastSendTime = clock;

	m_allBurrowDataArr.emplace_back(burrowData);

	send_BurrowData(addr, addrlen);
}

void P2PClient::on_net_StopBurrow(void* data, unsigned int len)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_StopBurrow);

	P2PMessage_S2C_StopBurrow* result = (P2PMessage_S2C_StopBurrow*)data;
	remove_burrowData(result->ip, result->port);
}


NS_NET_UV_END