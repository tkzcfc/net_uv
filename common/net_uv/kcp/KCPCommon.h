#pragma once

#include "../base/Common.h"
#include "../base/Socket.h"
#include "../base/Runnable.h"
#include "../base/uv_func.h"
#include "../base/Session.h"
#include "../base/SessionManager.h"
#include "../base/Mutex.h"
#include "../base/Client.h"
#include "../base/Server.h"
#include "../common/NetUVThreadMsg.h"
#include "KCPConfig.h"

NS_NET_UV_BEGIN

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 消息包头
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct KCPMsgHead
{
	unsigned int len;// 消息长度，不包括本结构体
#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
	NetMsgTag tag;// 消息标记
#endif
};

NS_NET_UV_END
