#pragma once

#include "P2PCommon.h"
#include "P2PMessage.h"

NS_NET_UV_BEGIN

struct P2P_SVR_UserInfo
{
	unsigned int userID; // 用户ID
	std::string szName;	//用户名
	std::string szIP;	//公网IP地址
	std::string szIntranetIP;//内网IP地址
	unsigned int port;	// 监听端口号
	int passworld;		// 连接密码
	bool isOnline;		// 是否在线
	unsigned int token;	// 令牌
	IUINT32 overdueTime;// 过期时间
	std::vector<P2PMessageInterface_Address> interfaceAddress; // 内网地址信息
};

class P2PServer
{
public:
	P2PServer();

	P2PServer(const P2PServer&) = delete;

	virtual ~P2PServer();

	void startServer(const char* ip, unsigned int port, bool isIPV6 = false);

	void stopServer();

	void updateFrame();

	inline void set_server_StartCallback(const ServerStartCall& call);

	inline void set_server_CloseCallback(const ServerCloseCall& call);

protected:

	void alloc_server();

	void free_server();

	inline unsigned int createNewUserID();

	bool isSameLAN(const P2P_SVR_UserInfo* info1, const P2P_SVR_UserInfo* info2);

protected:

	// svr
	void on_server_StartCall(Server* svr, bool issuc);

	void on_server_CloseCall(Server* svr);

	void on_server_NewConnectCall(Server* svr, Session* session);

	void on_server_RecvCall(Server* svr, Session* session, char* data, unsigned int len);

	void on_server_DisconnectCall(Server* svr, Session* session);

	// send
	void sendMsg(unsigned int sessionID, unsigned int msgID, char* data, unsigned int len);

	void send_RegisterResult(unsigned int sessionID, int code, unsigned int userId, unsigned int token);

	void send_UserListResult(unsigned int sessionID, int pageIndex, int pageMaxCount);

	void send_ConnectResult(unsigned int sessionID, int code, unsigned int userId, unsigned int port, const std::string& ip);

	void send_StartBurrow(unsigned int sessionID, const std::string& ip, unsigned int port, bool isIPV6);

	void send_StopBurrow(unsigned int sessionID, const std::string& ip, unsigned int port);

	// net
	void on_net_RegisterRequest(Session* session, void* data, unsigned int len);

	void on_net_LogoutRequest(Session* session, void* data, unsigned int len);

	void on_net_UserListRequest(Session* session, void* data, unsigned int len);

	void on_net_ConnectRequest(Session* session, void* data, unsigned int len);

	void on_net_ConnectSuccessRequest(Session* session, void* data, unsigned int len);
protected:

	KCPServer* m_server;

	std::map<unsigned int, P2P_SVR_UserInfo> m_userList;
	

	// 打洞关联数据
	struct BurrowRelationData
	{
		unsigned int who;
		unsigned int to;
		IUINT32 overtime;
	};
	std::vector<BurrowRelationData> m_burrowRelationList;


	ServerStartCall			m_svr_startCall;
	ServerCloseCall			m_svr_closeCall;

	bool m_userListOverdueTime;
	unsigned int m_userIDSpawn;
};

void P2PServer::set_server_StartCallback(const ServerStartCall& call)
{
	m_svr_startCall = std::move(call);
}

void P2PServer::set_server_CloseCallback(const ServerCloseCall& call)
{
	m_svr_closeCall = std::move(call);
}

unsigned int P2PServer::createNewUserID()
{
	return ++m_userIDSpawn;
}

NS_NET_UV_END
