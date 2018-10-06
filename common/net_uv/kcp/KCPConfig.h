#pragma once

NS_NET_UV_BEGIN
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 基础配置
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 套接字最小接收缓存大小
#define KCP_UV_SOCKET_RECV_BUF_LEN (1024 * 4)
// 套接字最小发送缓存大小
#define KCP_UV_SOCKET_SEND_BUF_LEN (1024 * 4)

// 大消息最大发送大小
// 如果消息头的长度字段大于该值
// 则直接认定为该客户端发送的消息为非法消息
// (4MB)
#define KCP_BIG_MSG_MAX_LEN (1024 * 1024 * 4)

// 单次消息发送最大字节
// 若超过该长度，则进行分片发送
// (1K)
#define KCP_WRITE_MAX_LEN (1024 * 1)

// IP地址长度
#define KCP_IP_ADDR_LEN (32)


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 消息校验 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 消息开启md5校验
#define KCP_UV_OPEN_MD5_CHECK 1
// 校验密码
#define KCP_UV_ENCODE_KEY "net_uv_KCP_md5_key"


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 心跳相关
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 是否开启在UV线程进行心跳校验
// 如果关闭该选项，则需要在应用层自己做心跳校验
#define KCP_OPEN_UV_THREAD_HEARTBEAT 0

#if KCP_OPEN_UV_THREAD_HEARTBEAT == 1
#define KCP_HEARTBEAT_TIMER_DELAY (400)		// 心跳检测定时器间隔
#define KCP_HEARTBEAT_CHECK_DELAY (1200)	// 心跳检测时间
#define KCP_HEARTBEAT_MAX_COUNT_SERVER 3	// 心跳不回复最大次数(服务端)
#define KCP_HEARTBEAT_MAX_COUNT_CLIENT 3	// 心跳不回复最大次数(客户端)

// 心跳次数计数重置值(服务端) 小于0 
// 当服务端该值比客户端小时，心跳请求一般由客户端发送，服务端进行回复
#define KCP_HEARTBEAT_COUNT_RESET_VALUE_SERVER (-2) 
// 心跳次数计数重置值(客户端) 小于0
#define KCP_HEARTBEAT_COUNT_RESET_VALUE_CLIENT (-1)	
#endif

#define KCP_HEARTBEAT_MSG_C2S ((unsigned int)0)		// 客户端->服务器心跳探测消息
#define KCP_HEARTBEAT_MSG_S2C ((unsigned int)1)		// 服务器->客户端心跳探测消息
#define KCP_HEARTBEAT_RET_MSG_C2S ((unsigned int)2) // 客户端->服务器心跳回复消息
#define KCP_HEARTBEAT_RET_MSG_S2C ((unsigned int)3) // 服务器->客户端心跳回复消息
#define KCP_HEARTBEAT_MSG_SIZE sizeof(unsigned int)	// 心跳消息大小


NS_NET_UV_END