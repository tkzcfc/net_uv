#pragma once

#include "../base/Macros.h"

NS_NET_UV_BEGIN

enum P2PMessageType : unsigned int
{
	P2P_MSG_ID_BEGIN,
	
	P2P_MSG_ID_C2S_REGISTER,		// P2PClient向中央服务器发送注册请求
	P2P_MSG_ID_S2C_REGISTER_RESULT,	// P2PClient注册结果

	P2P_MSG_ID_C2S_LOGOUT,			// P2PClient向中央服务器发送登出请求
	P2P_MSG_ID_S2C_LOGOUT_RESULT,	// P2PClient登出结果

	P2P_MSG_ID_C2S_GET_USER_LIST,	// P2PClient向中央服务器请求用户列表
	P2P_MSG_ID_S2C_USER_LIST,		// 中央服务器向P2PClient推送用户列表

	P2P_MSG_ID_C2S_WANT_TO_CONNECT, // P2PClient向中央服务器发送想要连接到某个连接请求
	P2P_MSG_ID_S2C_WANT_TO_CONNECT_RESULT, //请求返回
	P2P_MSG_ID_C2S_CONNECT_SUCCESS, // P2PClient告诉中央服务器已经连接成功

	P2P_MSG_ID_S2C_START_BURROW,	// 中央服务器向某个P2PClient发送开始打洞指令
	P2P_MSG_ID_S2C_STOP_BURROW,	// 中央服务器向某个P2PClient发送停止打洞指令

	P2P_MSG_ID_C2C_SONME_DATA,		// 打洞数据

	P2P_MSG_ID_END
};


struct P2PMessageBase
{
	unsigned int ID;
};

//////////////////////////////////////////////////////////////////////////
#define P2P_IP_MAX_LEN NET_UV_INET6_ADDRSTRLEN
#define P2P_CLIENT_USER_NAME_MAX_LEN 128

// 用户信息
struct P2PClientUserInfo
{
	P2PClientUserInfo()
	{
		memset(szName, P2P_CLIENT_USER_NAME_MAX_LEN, 0);
	}
	// ID
	unsigned int userID;

	// 名称
	char szName[P2P_CLIENT_USER_NAME_MAX_LEN];

	// 是否需要密码
	bool hasPassword;								
};


//////////////////////////////////////////////////////////////////////////

struct P2PMessage_C2S_Register
{
	P2PMessage_C2S_Register()
	{
		memset(szName, 0, P2P_CLIENT_USER_NAME_MAX_LEN);
	}
	// P2PClient 监听端口
	unsigned int port;	

	// 用户名
	char szName[P2P_CLIENT_USER_NAME_MAX_LEN];

	// 用户ID，0表示由服务器分配，其他则表示断线重连
	// 如果服务器确认该断线重连有效则返回此ID，否则服务器重新分配一个新的ID
	unsigned int userID;

	// 进入密码，如果为0则表明不需要密码
	int password;		 
};

struct P2PMessage_S2C_Register_Result
{
	// 0: 注册成功 1: 名称重复 2:名称非法
	int code;

	// 用户ID
	unsigned int userID;
};

struct P2PMessage_C2S_GetUserList
{
	// 请求页下标 0则表示请求所有
	int pageIndex;	
};

struct P2PMessage_S2C_UserList
{
	// 当前页下标
	int curPageIndex;	

	// 一共有多少页
	int totalPageCount;	

	// 当前页共有多少个用户信息
	int curUserCount;
	//P2PClientUserInfo* userInfoArr;
};

struct P2PMessage_C2S_WantToConnect
{
	// 想要连接的用户ID
	unsigned int userID;

	// 连接密码
	int password;
};

struct P2PMessage_C2S_WantToConnectResult
{
	// 0:请求成功 1: 用户不存在  2:对方离线中
	int code;			

	// 对方用户ID
	unsigned int userID;		

	// 对方监听端口
	unsigned int port;

	// 对方IP地址
	char ip[P2P_IP_MAX_LEN];
};

struct P2PMessage_C2S_ConnectSuccess
{
	// 连接成功的用户ID
	unsigned int userID;			
};

struct P2PMessage_S2C_StartBurrow
{
	// 打洞端口
	unsigned int port;

	// 打洞IP
	char ip[P2P_IP_MAX_LEN];

	// 是否为IPV6
	bool isIPV6;

	// 会话ID
	unsigned int sessionID;
};

struct P2PMessage_S2C_StopBurrow
{
	// 打洞端口
	unsigned int port;

	// 打洞IP
	char ip[P2P_IP_MAX_LEN];
};

NS_NET_UV_END
