#pragma once

#include "P2PCommon.h"

NS_NET_UV_BEGIN

// 消息ID
enum P2PMessageID
{
	P2P_MSG_ID_BEGIN = 1000,
	
	P2P_MSG_ID_C2T_CLIENT_LOGIN,			// 客户端登录
	P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT,		// 客户端登录结果

	P2P_MSG_ID_C2T_GET_LIST,				// 客户端请求用户列表
	P2P_MSG_ID_T2C_GET_LIST_RESULT,			// 客户端请求用户列表结果

	P2P_MSG_ID_C2C_HELLO,

	P2P_MSG_RECV_MSG_RESULT__EX,

	P2P_MSG_ID_END,
};

// 消息结构
struct P2PMessage
{
	uint32_t msgID;		// 消息ID
	uint32_t msgLen;	// 消息长度(不包括本结构体)
	uint64_t uniqueID;  // 该条消息唯一ID
};

// 内网地址信息
struct P2PIntranet_Address
{
	uint32_t addr;		// 地址
	uint32_t mask;		// 掩码
};

// 地址信息
union AddrInfo
{
	uint64_t key;		// key=前四字节为IP后四字节为端口组合
	struct
	{
		uint32_t ip;	// IP
		uint32_t port;  // 端口
	};
};

// 内网地址最大个数
#define P2P_INTERFACE_ADDR_MAX 4

// P2P节点信息
struct P2PNodeInfo
{
	// 公网地址信息
	AddrInfo addr;

	// 内网地址信息数量最大4个
	uint16_t interfaceCount;	
	// 内网地址信息
	P2PIntranet_Address interfaceAddr[P2P_INTERFACE_ADDR_MAX];
};

NS_NET_UV_END
