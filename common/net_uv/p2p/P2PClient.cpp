#include "P2PClient.h"

NS_NET_UV_BEGIN

// 打洞数据持续发送时间
#define P2P_SEND_BURROW_DATA_DURATION 3000

// 打洞数据发送间隔时间(ms)
#define P2P_SEND_BURROW_DATA_SPACE_TIME 50

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
	, m_centerSVRPort(0)
	, m_centerSVRIP("")
	, m_token(0)
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
	m_centerSVRIP = ip;
	m_centerSVRPort = port;
	m_client->connect(ip, port, P2P_CONNECT_SVR_SESSION_ID);
	m_client->setAutoReconnectBySessionID(P2P_CONNECT_SVR_SESSION_ID, true);
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

// 获取内网IP
void P2PClient::getIntranetIP(std::vector<P2PMessageInterface_Address>& IPArr)
{
	char buf[512];
	uv_interface_address_t *info;
	int count, i;

	uv_interface_addresses(&info, &count);
	i = count;

	P2PMessageInterface_Address addressData;
	while (i--) 
	{
		uv_interface_address_t interface = info[i];

		if (!interface.is_internal)
		{
			if (interface.address.address4.sin_family == AF_INET) {
				uv_ip4_name(&interface.address.address4, buf, sizeof(buf));
				
				if(strlen(buf) >= P2P_IP_MAX_LEN)
					continue;

				strcpy(addressData.szIP, buf);

				uv_ip4_name(&interface.netmask.netmask4, buf, sizeof(buf));

				if (strlen(buf) >= P2P_IP_MAX_LEN)
					continue;

				strcpy(addressData.szMask, buf);

				IPArr.push_back(addressData);
			}
			else if (interface.address.address4.sin_family == AF_INET6) {
				uv_ip6_name(&interface.address.address6, buf, sizeof(buf));

				if (strlen(buf) >= P2P_IP_MAX_LEN)
					continue;

				strcpy(addressData.szIP, buf);

				uv_ip6_name(&interface.netmask.netmask6, buf, sizeof(buf));

				if (strlen(buf) >= P2P_IP_MAX_LEN)
					continue;

				strcpy(addressData.szMask, buf);

				IPArr.push_back(addressData);
			}
		}
	}
	uv_free_interface_addresses(info, count);
}

bool P2PClient::client_connect(unsigned int userID, unsigned int sessionID, int password)
{
	assert(sessionID != P2P_CONNECT_SVR_SESSION_ID);
	if (!m_isConnectP2PSVR)
		return false;

	auto it = m_connectInfoMap.find(sessionID);
	if (it != m_connectInfoMap.end())
	{
		if (it->second.userID != userID)
			return false;

		it->second.password = password;

		if (it->second.state == ConnectInfoData::ConnectState::WAIT_CENTER_MSG)
		{
			send_WantConnectMsg(userID, password, it->second.bindPort);
		}
	}
	else
	{
		ConnectInfoData data;
		data.userID = userID;
		data.password = password;
		data.state = ConnectInfoData::ConnectState::CREATE_PRE_SOCKET;
		m_connectInfoMap.insert(std::make_pair(sessionID, data));

		m_client->removeSession(sessionID);
		m_client->createPrefabricationSocket(sessionID);
	}
	return true;
}

void P2PClient::client_removeSessionByUserID(unsigned int userID)
{
	for (auto it = m_connectInfoMap.begin(); it != m_connectInfoMap.end(); ++it)
	{
		if (it->second.userID == userID)
		{
			m_client->removeSession(it->first);
			m_connectInfoMap.erase(it);
			break;
		}
	}
	send_ConnectSuccess(userID);
}

void P2PClient::client_removeSession(unsigned int sessionId)
{
	assert(P2P_CONNECT_SVR_SESSION_ID != sessionId);

	auto it = m_connectInfoMap.find(sessionId);
	if (it != m_connectInfoMap.end())
	{
		m_connectInfoMap.erase(it);
	}

	m_client->removeSession(sessionId);
}

void P2PClient::client_disconnect(unsigned int sessionId)
{
	assert(P2P_CONNECT_SVR_SESSION_ID != sessionId);
	client_removeSession(sessionId);
}

void P2PClient::client_send(unsigned int sessionId, char* data, unsigned int len)
{
	assert(P2P_CONNECT_SVR_SESSION_ID != sessionId);
	auto it = m_connectInfoMap.find(sessionId);
	if (it != m_connectInfoMap.end() && it->second.state == ConnectInfoData::ConnectState::CONNECT)
	{
		m_client->send(sessionId, data, len);
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

	// 关闭所有自动重连
	m_client->setAutoReconnect(false);
	m_client->setClientCloseCallback(std::bind(&P2PClient::on_client_ClientCloseCall, this, std::placeholders::_1));
	m_client->setConnectCallback(std::bind(&P2PClient::on_client_ConnectCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	m_client->setRecvCallback(std::bind(&P2PClient::on_client_RecvCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
	m_client->setDisconnectCallback(std::bind(&P2PClient::on_client_DisconnectCall, this, std::placeholders::_1, std::placeholders::_2));
	m_client->setCreatePreSocketCallback(std::bind(&P2PClient::on_client_CreatePreSocketCall, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
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
		if (it->endTime < clock)
		{
			fc_free(it->addr);
			it = m_allBurrowDataArr.erase(it);
		}
		else
		{
			if (P2P_SEND_BURROW_DATA_SPACE_TIME < clock - it->lastSendTime)
			{
				it->lastSendTime = clock;
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

// 发送消息
void P2PClient::sendMsg(unsigned int msgID, const char* msgContent)
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("msgID");
	writer.Uint(msgID);

	writer.Key("msgContent");
	writer.String(msgContent);

	writer.EndObject();

	const char* sendData = s.GetString();

	m_client->send(P2P_CONNECT_SVR_SESSION_ID, (char*)sendData, strlen(sendData));

	printf("send [%d]:\n%s\n", msgID, sendData);
}

// 发送登录消息
void P2PClient::send_LoginMeg()
{
	if (m_intranetIPInfoArr.empty())
	{
		getIntranetIP(m_intranetIPInfoArr);
	}

	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();               
	writer.Key("userID");
	writer.Uint(m_userID);

	writer.Key("token");
	writer.Uint(m_token);

	writer.Key("intranetInfo");
	writer.StartObject();

	writer.StartArray();
	for (std::size_t i = 0; i < m_intranetIPInfoArr.size(); i++)
	{
		writer.StartObject();

		writer.Key("IP");
		writer.String(m_intranetIPInfoArr[i].szIP);

		writer.Key("mask");
		writer.String(m_intranetIPInfoArr[i].szMask);

		writer.EndObject();
	}
	writer.EndArray();

	writer.EndObject();
	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_LOGIN, s.GetString());
}

// 发送登出消息
void P2PClient::send_LogoutMeg()
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("userID");
	writer.Uint(m_userID);

	writer.Key("token");
	writer.Uint(m_token);

	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_LOGOUT, s.GetString());
}

void P2PClient::try_send_RegisterSVRMsg()
{
	if (m_svrStartSuccess && m_isConnectP2PSVR)
	{
		send_RegisterSVRMsg();
	}
}

void P2PClient::send_RegisterSVRMsg()
{
	if (m_intranetIPInfoArr.empty())
	{
		getIntranetIP(m_intranetIPInfoArr);
	}

	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();
	writer.Key("svrID");
	writer.Uint(m_svrID);
	writer.Key("svrToken");
	writer.Uint(m_svrToken);
	writer.Key("svrPassword");
	writer.Uint(m_svrPassword);
	writer.Key("svrName");
	writer.String(m_svrName.c_str());

	writer.Key("intranetInfo");
	writer.StartObject();
	writer.StartArray();
	for (std::size_t i = 0; i < m_intranetIPInfoArr.size(); i++)
	{
		writer.StartObject();

		writer.Key("IP");
		writer.String(m_intranetIPInfoArr[i].szIP);
		writer.Key("mask");
		writer.String(m_intranetIPInfoArr[i].szMask);

		writer.EndObject();
	}
	writer.EndArray();
	writer.EndObject();
	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_REGISTER_SVR, s.GetString());
}

// 发送取消服务器注册消息
void P2PClient::send_UnRegisterSVRMsg()
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("svrID");
	writer.Uint(m_svrID);
	writer.Key("svrToken");
	writer.Uint(m_svrToken);

	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_UN_REGISTER_SVR, s.GetString());
}


// 发送想要连接到某个服务器消息
void P2PClient::send_WantConnectMsg(unsigned int userId, const std::string& password, unsigned int port)
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("svrID");
	writer.Uint(m_svrID);

	writer.Key("password");
	writer.String(password.c_str());

	writer.Key("port");
	writer.Uint(port);

	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_WANT_TO_CONNECT, s.GetString());
}

void P2PClient::send_ConnectSuccess(unsigned int svrID)
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("svrID");
	writer.Uint(m_svrID);

	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_CONNECT_SUCCESS, s.GetString());
}

void P2PClient::send_GetSVRList(int pageIndex)
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("pageIndex");
	writer.Int(pageIndex);

	writer.EndObject();

	sendMsg(P2P_MSG_ID_C2S_GET_SVR_LIST, s.GetString());
}

void P2PClient::send_BurrowData(struct sockaddr* addr, unsigned int addrlen)
{
	if (m_server && m_svrStartSuccess)
	{
		m_server->svrUdpSend(addr, addrlen, (char*)m_burrowData.c_str(), m_burrowData.length());
	}
}


void P2PClient::on_client_ConnectCall(Client* client, Session* session, int status)
{
	if (session->getSessionID() == P2P_CONNECT_SVR_SESSION_ID)
	{
		NET_UV_LOG(NET_UV_L_INFO, "连接到中央服务器%s", (status == 1) ? "成功" : "失败");
	}
	else
	{
		auto it = m_connectInfoMap.find(session->getSessionID());
		if (it != m_connectInfoMap.end())
		{
			if (status == 1)
			{
				it->second.state = ConnectInfoData::ConnectState::CONNECT;
				// 连接成功
				if (m_p2p_connectResultCall)
				{
					m_p2p_connectResultCall(this, 0, it->second.userID);
				}
			}
			else
			{
				auto userID = it->second.userID;
				send_ConnectSuccess(userID);
				m_connectInfoMap.erase(it);

				client->removeSession(session->getSessionID());

				// 连接失败，与P2PClient的本地服务器连接失败
				if (m_p2p_connectResultCall)
				{
					m_p2p_connectResultCall(this, 3, userID);
				}
			}
			m_client_connectCall(client, session, status);
		}
		else
		{
			client->removeSession(session->getSessionID());
		}
	}
}

void P2PClient::on_client_DisconnectCall(Client* client, Session* session)
{
	if (session->getSessionID() == P2P_CONNECT_SVR_SESSION_ID)
	{
		m_isConnectP2PSVR = false;
		NET_UV_LOG(NET_UV_L_INFO, "与中央服务器断开连接");
	}
	else
	{
		client_removeSession(session->getSessionID());

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

	rapidjson::Document d;
	d.Parse(data, len);

	if (!d.HasParseError())
	{
		if (d.HasMember("msgID") && d.HasMember("msgContent"))
		{
			rapidjson::Value& v_msgID = d["msgID"];
			
			unsigned int msgID = v_msgID.GetUint();
			if (msgID <= P2PMessageType::P2P_MSG_ID_BEGIN || msgID >= P2PMessageType::P2P_MSG_ID_END)
			{
				NET_UV_LOG(NET_UV_L_ERROR, "msgid error: %d", msgID);
				return;
			}
			rapidjson::Value& v_Content = d["msgContent"];
			const char* content_json = v_Content.GetString();

			if (strlen(content_json) <= 0)
			{
				on_net_DispatchMsg(session, msgID, d);
			}
			else
			{
				d.Parse(content_json);
				if (!d.HasParseError())
				{
					on_net_DispatchMsg(session, msgID, d);
				}
				else
				{
					NET_UV_LOG(NET_UV_L_ERROR, "json content parse error: %d", d.GetParseError());
				}
			}
		}
	}
	else
	{
		NET_UV_LOG(NET_UV_L_ERROR, "json parse error: %d", d.GetParseError());
	}
}

void P2PClient::on_client_ClientCloseCall(Client* client)
{
	m_connectInfoMap.clear();
	m_isConnectP2PSVR = false;
	if (m_client_clientCloseCall)
	{
		m_client_clientCloseCall(client);
	}
}

void P2PClient::on_client_CreatePreSocketCall(KCPClient* client, unsigned int sessionID, unsigned int bindPort)
{
	auto it = m_connectInfoMap.find(sessionID);
	
	if (it != m_connectInfoMap.end())
	{
		if (it->second.state == ConnectInfoData::ConnectState::CREATE_PRE_SOCKET)
		{
			if (bindPort == 0)
			{
				auto userID = it->second.userID;
				m_connectInfoMap.erase(it);

				// 4连接失败，创建预制socket失败，sessionID冲突
				if (m_p2p_connectResultCall)
				{
					m_p2p_connectResultCall(this, 4, userID);
				}
				printf("预制socket创建失败\n");
			}
			else
			{
				printf("预制socket创建成功 %d\n", bindPort);
				it->second.state = ConnectInfoData::ConnectState::WAIT_CENTER_MSG;
				it->second.bindPort = bindPort;
				send_WantConnectMsg(it->second.userID, it->second.password, bindPort);
			}
		}
		else
		{
			m_connectInfoMap.erase(it);
		}
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
void P2PClient::on_net_DispatchMsg(Session* session, unsigned int msgID, rapidjson::Document& doc)
{

	switch (msgID)
	{
	case P2P_MSG_ID_S2C_REGISTER_SVR_RESULT:// 注册结果
	{
		on_net_RegisterSVRResult(session, doc);
	}break;
	case P2P_MSG_ID_S2C_UN_REGISTER_SVR_RESULT:// 取消注册结果
	{
		on_net_UnRegisterSVRResult(session, doc);
	}break;
	case P2P_MSG_ID_S2C_LOGIN_RESULT:  // 登录结果
	{}break;
	case P2P_MSG_ID_S2C_LOGOUT_RESULT:	// 登出结果
	{
	}break;
	case P2P_MSG_ID_S2C_USER_LIST:		// 用户列表
	{
		printf("\nRecv: 用户列表\n");
		on_net_UserListResult(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_WANT_TO_CONNECT_RESULT:// 连接请求结果
	{
		printf("\nRecv: 连接请求结果\n");
		on_net_ConnectResult(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_START_BURROW: // 开始打洞指令
	{
		printf("\nRecv: 开始打洞指令\n");
		on_net_StartBurrow(session, MsgContent, ContentLen);
	}break;
	case P2P_MSG_ID_S2C_STOP_BURROW: // 停止打洞指令
	{
		printf("\nRecv: 停止打洞指令\n");
		on_net_StopBurrow(session, MsgContent, ContentLen);
	}break;
	default:
		printf("\nRecv: 其他消息\n");
		break;
	}
}

void P2PClient::on_net_RegisterSVRResult(Session* session, rapidjson::Document& doc)
{
	m_svrID = doc["svrID"].GetUint();
	m_svrToken = doc["svrToken"].GetUint();
	
	int code = doc["code"].GetInt();
	if (m_p2p_registerResultCall)
	{
		m_p2p_registerResultCall(this, code);
	}
}

void P2PClient::on_net_UnRegisterSVRResult(Session* session, rapidjson::Document& doc)
{
	m_server->stopServer();
}

void P2PClient::on_net_LoginResult(Session* session, rapidjson::Document& doc)
{
	m_userID = doc["userID"].GetUint();
	m_token = doc["token"].GetUint();

	m_isConnectP2PSVR = true;
	try_send_RegisterSVRMsg();
}

void P2PClient::on_net_LogoutResult(Session* session, rapidjson::Document& doc)
{
	m_userID = 0;
	m_token = 0;
	m_isConnectP2PSVR = false;
}

void P2PClient::on_net_RegisterResult(Session* session, rapidjson::Document& doc)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_Register_Result);

	P2PMessage_S2C_Register_Result* result = (P2PMessage_S2C_Register_Result*)data;
	m_userID = result->userID;
	m_token = result->token;

	if (m_p2p_registerResultCall)
	{
		m_p2p_registerResultCall(this, result->code);
	}
}

void P2PClient::on_net_UserListResult(Session* session, rapidjson::Document& doc)
{
	if (sizeof(P2PMessage_S2C_UserList) > len)
		return;
	
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

void P2PClient::on_net_ConnectResult(Session* session, rapidjson::Document& doc)
{
	P2P_MSG_LEN_CHECK(P2PMessage_C2S_WantToConnectResult);

	P2PMessage_C2S_WantToConnectResult* result = (P2PMessage_C2S_WantToConnectResult*)data;

	for (auto& it : m_connectInfoMap)
	{
		if (it.second.userID == result->userID)
		{
			// 请求成功
			if (result->code == 0)
			{
				printf("开始连接到%s:%d\n", result->ip, result->port);
				it.second.state = ConnectInfoData::ConnectState::CONNECTING;
				m_client->connect(result->ip, result->port, it.first);
			}
			else
			{
				// 1连接失败，用户不存在
				if (m_p2p_connectResultCall)
				{
					m_p2p_connectResultCall(this, 1, result->userID);
				}
			}
		}
	}
}

void P2PClient::on_net_StartBurrow(Session* session, rapidjson::Document& doc)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_StartBurrow);

	// 本机服务器已关闭
	if (m_server == NULL || !m_svrStartSuccess)
	{
		return;
	}

	P2PMessage_S2C_StartBurrow* result = (P2PMessage_S2C_StartBurrow*)data;

	printf("开始打洞:%s:%d\n", result->ip, result->port);
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

	m_allBurrowDataArr.push_back(burrowData);

	send_BurrowData(addr, addrlen);
}

void P2PClient::on_net_StopBurrow(Session* session, rapidjson::Document& doc)
{
	P2P_MSG_LEN_CHECK(P2PMessage_S2C_StopBurrow);

	P2PMessage_S2C_StopBurrow* result = (P2PMessage_S2C_StopBurrow*)data;
	remove_burrowData(result->ip, result->port);
}


NS_NET_UV_END