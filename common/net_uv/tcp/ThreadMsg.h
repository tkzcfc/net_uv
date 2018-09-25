#pragma once

#include "TCPCommon.h"

NS_NET_UV_BEGIN

//消息类型
enum ThreadMsgType
{
	START_SERVER_SUC,	//服务器启动成功
	START_SERVER_FAIL,//服务器启动失败
	CONNECT_FAIL,	//连接失败
	CONNECT_ING,	//正在连接
	CONNECT,		//连接成功
	NEW_CONNECT,	//新连接
	DIS_CONNECT,	//断开连接
	EXIT_LOOP,		//退出loop
	RECV_DATA		//收到消息
};


struct ThreadMsg_S
{
public:
	ThreadMsgType msgType;
	Session* pSession;//TcpSession指针
	char* data;
	unsigned int dataLen;
	TCPMsgTag tag;
};

struct ThreadMsg_C
{
public:
	ThreadMsgType msgType;
	Session* pSession;
	char* data;
	unsigned int dataLen;
	TCPMsgTag tag;
};

NS_NET_UV_END
