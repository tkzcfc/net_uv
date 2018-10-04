#pragma once

#include "../base/Common.h"
#include "../base/Session.h"

NS_NET_UV_BEGIN

//消息类型
enum class NetThreadMsgType
{
	START_SERVER_SUC,	//服务器启动成功
	START_SERVER_FAIL,	//服务器启动失败
	CONNECT_FAIL,		//连接失败
	CONNECT_ING,		//正在连接
	CONNECT,			//连接成功
	NEW_CONNECT,		//新连接
	DIS_CONNECT,		//断开连接
	EXIT_LOOP,			//退出loop
	RECV_DATA,			//收到消息
	REMOVE_SESSION,		//移除会话
};


struct NetThreadMsg_S
{
	NetThreadMsgType msgType;
	Session* pSession;
	char* data;
	unsigned int dataLen;
};

struct NetThreadMsg_C
{
	NetThreadMsgType msgType;
	Session* pSession;
	char* data;
	unsigned int dataLen;
};

NS_NET_UV_END
