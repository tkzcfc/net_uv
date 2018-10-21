#pragma once

#include "P2PCommon.h"
#include "P2PMessage.h"

NS_NET_UV_BEGIN

// P2PClient连接中央服务器使用的会话ID
#define P2P_CONNECT_SVR_SESSION_ID 0

class P2PClient;


// 连接请求返回回调
/// status: 0 请求成功 1用户不存在 2对方繁忙
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


	// client

	// 连接其他P2P客户端
	/// sessionID 不能为0,0为保留会话ID，用于和P2P服务器连接
	bool client_connect(unsigned int userID, unsigned int sessionID, int password, bool autoConnect = false);

	void client_setAutoReconnectBySessionID(unsigned int sessionID, bool isAuto);

	inline void client_disconnect(unsigned int sessionId);

	inline void client_closeClient();

	inline void client_send(unsigned int sessionId, char* data, unsigned int len);

	inline void client_removeSession(unsigned int sessionId);

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
	void sendMsg(unsigned int sessionID, unsigned int msgID, char* data, unsigned int len);

	void try_send_RegisterMsg();

	void send_RegisterMsg();

	void send_WantConnectMsg(unsigned int userId, int password);

	void send_GetUserList(int pageIndex);

	void send_BurrowData(struct sockaddr* addr, unsigned int addrlen);
	
	void send_ConnectSuccess(unsigned int userId);

	//////////////////////////////////////////////////////////////////////////
	
protected:

	void on_client_ConnectCall(Client* client, Session* session, int status);

	void on_client_DisconnectCall(Client* client, Session* session);

	void on_client_RecvCallback(Client* client, Session* session, char* data, unsigned int len);

	void on_client_ClientCloseCall(Client* client);

	void on_server_StartCall(Server* svr, bool issuc);

	void on_server_CloseCall(Server* svr);

	void on_server_NewConnectCall(Server* svr, Session* session);

	void on_server_RecvCall(Server* svr, Session* session, char* data, unsigned int len);

	void on_server_DisconnectCall(Server* svr, Session* session);


	/// net
	void on_net_RegisterResult(void* data, unsigned int len);

	void on_net_UserListResult(void* data, unsigned int len);

	void on_net_ConnectResult(void* data, unsigned int len);

	void on_net_StartBurrow(void* data, unsigned int len);

	void on_net_StopBurrow(void* data, unsigned int len);
	
protected:
	KCPClient* m_client;
	KCPServer* m_server;

	bool m_isConnectP2PSVR;
	bool m_svrStartSuccess;
	std::string m_name;
	int m_password;
	unsigned int m_userID;

	struct ConnectInfoData
	{
		unsigned int userID;
		bool autoConnect;
		int password;
	};
	std::map<unsigned int, ConnectInfoData> m_connectInfoMap;


	struct BurrowData
	{
		struct sockaddr* addr;
		unsigned int addrlen;
		std::string ip;
		unsigned int port;
		IUINT32 endTime;		// 结束时间
		IUINT32 lastSendTime;	// 上一次发送时间
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

void P2PClient::client_disconnect(unsigned int sessionId)
{
	assert(P2P_CONNECT_SVR_SESSION_ID != sessionId);
	m_client->disconnect(sessionId);
}

void P2PClient::client_closeClient()
{
	m_client->closeClient();
}

void P2PClient::client_send(unsigned int sessionId, char* data, unsigned int len)
{
	assert(P2P_CONNECT_SVR_SESSION_ID != sessionId);
	m_client->send(sessionId, data, len);
}

void P2PClient::client_removeSession(unsigned int sessionId)
{
	assert(P2P_CONNECT_SVR_SESSION_ID != sessionId);
	m_client->removeSession(sessionId);
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