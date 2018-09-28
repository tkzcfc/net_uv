#pragma once

#include "TCPCommon.h"

NS_NET_UV_BEGIN

//消息类型
enum class TCPThreadMsgType
{
	START_SERVER_SUC,	//服务器启动成功
	START_SERVER_FAIL,	//服务器启动失败
	CONNECT_FAIL,		//连接失败
	CONNECT_TIMOUT,		//连接超时
	CONNECT_SESSIONID_EXIST,//会话ID已存在，且新连接IP和端口和之前会话不一致
	CONNECT_ING,		//正在连接
	CONNECT,			//连接成功
	NEW_CONNECT,		//新连接
	DIS_CONNECT,		//断开连接
	EXIT_LOOP,			//退出loop
	RECV_DATA			//收到消息
};


struct TCPThreadMsg_S
{
	TCPThreadMsgType msgType;
	Session* pSession;
	char* data;
	unsigned int dataLen;
	TCPMsgTag tag;
};

struct TCPThreadMsg_C
{
	TCPThreadMsgType msgType;
	Session* pSession;
	char* data;
	unsigned int dataLen;
	TCPMsgTag tag;
};

NS_NET_UV_END
