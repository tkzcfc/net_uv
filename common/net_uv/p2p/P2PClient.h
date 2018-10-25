#pragma once

#include "P2PCommon.h"
#include "P2PMessage.h"

NS_NET_UV_BEGIN

// P2PClient连接中央服务器使用的会话ID
#define P2P_CONNECT_SVR_SESSION_ID 0

class P2PClient;


// 连接请求返回回调
/// status: 0 连接成功 1连接失败，用户不存在 2连接失败，与中央服务器连接失败 3连接失败，与P2PClient的本地服务器连接失败 4 sessionID冲突
using P2PClientConnectResultCall = std::function<void(P2PClient* client, int status, unsigned int userId)>;

// 注册请求返回回调
/// status:  0: 注册成功 1: 名称重复
using P2PClientRegisterResultCall = std::function<void(P2PClient* client, int status)>;

// 用户列表返回回调
using P2PClientUserListResultCall = std::function<void(P2PClient* client, P2PMessage_S2C_UserList* listInfo, const std::vector<P2PClientUserInfo>&userList)>;


class P2PClient
{
public:
	P2PClient();
	
	P2PClient(const P2PClient&) = delete;

	virtual ~P2PClient();

	void connectToCenterServer(const char* ip, unsigned int port);

	bool getUserList(int pageIndex);

	void updateFrame();

	// server
	bool server_startServer(const char* name, int password = 0, const char* ip = "0.0.0.0", unsigned int port = 0, bool isIPV6 = false);

	bool server_stopServer();

	inline unsigned int server_getListenPort();

	// 获取内网IP
	void getIntranetIP(std::vector<P2PMessageInterface_Address>& IPArr);

	// client

	// 连接其他P2P客户端
	/// sessionID 不能为0,0为保留会话ID，用于和P2P服务器连接
	bool client_connect(unsigned int userID, unsigned int sessionID, int password);
	
	void client_removeSessionByUserID(unsigned int userID);
	
	void client_disconnect(unsigned int sessionId);

	inline void client_closeClient();

	void client_send(unsigned int sessionId, char* data, unsigned int len);

	void client_removeSession(unsigned int sessionId);

	inline KCPClient* getKCPClient();

	inline KCPServer* getKCPServer();

public:

	// client
	inline void set_client_ConnectCallback(const ClientConnectCall& call);

	inline void set_client_DisconnectCallback(const ClientDisconnectCall& call);

	inline void set_client_RecvCallback(const ClientRecvCall& call);

	inline void set_client_ClientCloseCallback(const ClientCloseCall& call);

	inline void set_client_RemoveSessionCallback(const ClientRemoveSessionCall& call);

	// server
	inline void set_server_CloseCallback(const ServerCloseCall& call);

	inline void set_server_StartCallback(const ServerStartCall& call);

	inline void set_server_NewConnectCallback(const ServerNewConnectCall& call);

	inline void set_server_RecvCallback(const ServerRecvCall& call);

	inline void set_server_DisconnectCallback(const ServerDisconnectCall& call);

	// P2PClient
	inline void set_p2p_ConnectResultCallback(const P2PClientConnectResultCall& call);

	inline void set_p2p_RegisterResultCallback(const P2PClientRegisterResultCall& call);

	inline void set_p2p_UserListResultCallback(const P2PClientUserListResultCall& call);

protected:

	void alloc_kcpServer();

	void alloc_kcpClient();

	void free_kcpServer();

	void free_kcpClient();

	void do_burrowLogic();

	void remove_burrowData(const char* ip, unsigned port);

	void clear_burrowData();

	//////////////////////////////////////////////////////////////////////////
	// 发送消息
	void sendMsg(unsigned int msgID, const char* msgContent);

	// 发送登录消息
	void send_LoginMeg();
	// 发送登出消息
	void send_LogoutMeg();

	// 发送注册服务器消息
	void try_send_RegisterSVRMsg();
	void send_RegisterSVRMsg();

	// 发送取消服务器注册消息
	void send_UnRegisterSVRMsg();
	
	// 发送想要连接到某个服务器消息
	void send_WantConnectMsg(unsigned int svrID, const std::string& password, unsigned int port);

	// 发送连接到某个服务器成功消息
	void send_ConnectSuccess(unsigned int svrID);

	// 发送获取服务器列表
	void send_GetSVRList(int pageIndex);

	// 发送打洞数据
	void send_BurrowData(struct sockaddr* addr, unsigned int addrlen);
	//////////////////////////////////////////////////////////////////////////
	
protected:

	void on_client_ConnectCall(Client* client, Session* session, int status);

	void on_client_DisconnectCall(Client* client, Session* session);

	void on_client_RecvCallback(Client* client, Session* session, char* data, unsigned int len);

	void on_client_ClientCloseCall(Client* client);

	void on_client_CreatePreSocketCall(KCPClient* client, unsigned int sessionID, unsigned int bindPort);

	void on_server_StartCall(Server* svr, bool issuc);

	void on_server_CloseCall(Server* svr);

	void on_server_NewConnectCall(Server* svr, Session* session);

	void on_server_RecvCall(Server* svr, Session* session, char* data, unsigned int len);

	void on_server_DisconnectCall(Server* svr, Session* session);


	/// net
	void on_net_DispatchMsg(Session* session, unsigned int msgID, rapidjson::Document& doc);

	void on_net_RegisterSVRResult(Session* session, rapidjson::Document& doc);

	void on_net_UnRegisterSVRResult(Session* session, rapidjson::Document& doc);

	void on_net_LoginResult(Session* session, rapidjson::Document& doc);

	void on_net_LogoutResult(Session* session, rapidjson::Document& doc);

	void on_net_UserListResult(Session* session, rapidjson::Document& doc);

	void on_net_ConnectResult(Session* session, rapidjson::Document& doc);

	void on_net_StartBurrow(Session* session, rapidjson::Document& doc);

	void on_net_StopBurrow(Session* session, rapidjson::Document& doc);
	
protected:
	KCPClient* m_client;
	KCPServer* m_server;

	// P2PClient是否连接到中央服务器
	bool m_isConnectP2PSVR;
	// 本地服务器是否启动成功
	bool m_svrStartSuccess;
	
	// 用户ID
	unsigned int m_userID;
	// 用户令牌
	unsigned int m_token;


	// 服务器密码
	int m_svrPassword;
	// 服务器ID
	unsigned int m_svrID;
	// 服务器令牌
	unsigned int m_svrToken;
	// 服务器名称
	std::string m_svrName;

	// 中央服务器IP
	std::string m_centerSVRIP;
	// 中央服务器端口
	unsigned int m_centerSVRPort;

	std::vector<P2PMessageInterface_Address> m_intranetIPInfoArr;

	// 连接信息
	struct ConnectInfoData
	{
		enum class ConnectState
		{
			CREATE_PRE_SOCKET,	// 创建预制socket
			WAIT_CENTER_MSG,	// 等待中央服务器消息结果
			CONNECTING,			// 连接中...
			CONNECT,			// 已连接
		};
		unsigned int userID;// 对方用户ID
		int password;		// 对方密码
		unsigned int bindPort; // 绑定的端口
		ConnectState state;
	};
	std::map<unsigned int, ConnectInfoData> m_connectInfoMap;

	// 打洞信息
	struct BurrowData
	{
		struct sockaddr* addr;	// 地址
		unsigned int addrlen;	// 地址长度
		std::string ip;			// 打洞IP
		unsigned int port;		// 打洞端口
		IUINT32 endTime;		// 结束时间
		IUINT32 lastSendTime;	// 最后一次发送时间
	};
	std::vector<BurrowData> m_allBurrowDataArr;

	// 打洞数据
	std::string m_burrowData;

	ClientConnectCall		m_client_connectCall;
	ClientDisconnectCall	m_client_disconnectCall;
	ClientRecvCall			m_client_recvCall;
	ClientCloseCall			m_client_clientCloseCall;
	ClientRemoveSessionCall m_client_removeSessionCall;

	ServerStartCall			m_svr_startCall;
	ServerCloseCall			m_svr_closeCall;
	ServerNewConnectCall	m_svr_newConnectCall;
	ServerRecvCall			m_svr_recvCall;
	ServerDisconnectCall	m_svr_disconnectCall;

	P2PClientConnectResultCall m_p2p_connectResultCall;
	P2PClientRegisterResultCall m_p2p_registerResultCall;
	P2PClientUserListResultCall m_p2p_getUserListResultCall;
};

unsigned int P2PClient::server_getListenPort()
{
	if (m_server)
	{
		return m_server->getListenPort();
	}
	return 0;
}

void P2PClient::client_closeClient()
{
	m_client->closeClient();
}

void P2PClient::set_client_ConnectCallback(const ClientConnectCall& call)
{
	m_client_connectCall = std::move(call);
}

void P2PClient::set_client_DisconnectCallback(const ClientDisconnectCall& call)
{
	m_client_disconnectCall = std::move(call);
}

void P2PClient::set_client_RecvCallback(const ClientRecvCall& call)
{
	m_client_recvCall = std::move(call);
}

void P2PClient::set_client_ClientCloseCallback(const ClientCloseCall& call)
{
	m_client_clientCloseCall = std::move(call);
}

void P2PClient::set_client_RemoveSessionCallback(const ClientRemoveSessionCall& call)
{
	m_client_removeSessionCall = std::move(call);
}

void P2PClient::set_server_CloseCallback(const ServerCloseCall& call)
{
	m_svr_closeCall = std::move(call);
}

void P2PClient::set_server_StartCallback(const ServerStartCall& call)
{
	m_svr_startCall = std::move(call);
}

void P2PClient::set_server_NewConnectCallback(const ServerNewConnectCall& call)
{
	m_svr_newConnectCall = std::move(call);
}

void P2PClient::set_server_RecvCallback(const ServerRecvCall& call)
{
	m_svr_recvCall = std::move(call);
}

void P2PClient::set_server_DisconnectCallback(const ServerDisconnectCall& call)
{
	m_svr_disconnectCall = std::move(call);
}

void P2PClient::set_p2p_ConnectResultCallback(const P2PClientConnectResultCall& call)
{
	m_p2p_connectResultCall = std::move(call);
}

void P2PClient::set_p2p_RegisterResultCallback(const P2PClientRegisterResultCall& call)
{
	m_p2p_registerResultCall = std::move(call);
}

void P2PClient::set_p2p_UserListResultCallback(const P2PClientUserListResultCall& call)
{
	m_p2p_getUserListResultCall = std::move(call);
}

KCPClient* P2PClient::getKCPClient()
{
	return m_client;
}

KCPServer* P2PClient::getKCPServer()
{
	return m_server;
}

NS_NET_UV_END