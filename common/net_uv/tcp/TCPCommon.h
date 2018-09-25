#pragma once

#include "../base/Socket.h"
#include "../base/Server.h"
#include "../base/Client.h"
#include "../base/Runnable.h"
#include "../base/uv_func.h"
#include "../base/Mutex.h"
#include "../base/Session.h"
#include "../base/SessionManager.h"
#include "TCPConfig.h"

NS_NET_UV_BEGIN


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 消息包头
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 消息内容标记
enum TCPMsgTag : unsigned char
{
	MT_HEARTBEAT,	// 心跳消息
	MT_DEFAULT
};

struct TCPMsgHead
{
	unsigned int len;// 消息长度，不包括本结构体
#if OPEN_UV_THREAD_HEARTBEAT == 1
	TCPMsgTag tag;// 消息标记
#endif
};


NS_NET_UV_END