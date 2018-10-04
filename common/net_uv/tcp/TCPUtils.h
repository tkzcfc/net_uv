#pragma once

#include "TCPCommon.h"

NS_NET_UV_BEGIN

//加密
NET_UV_EXTERN char* tcp_uv_encode(const char* data, unsigned int len, unsigned int &outLen);
//解密
NET_UV_EXTERN char* tcp_uv_decode(const char* data, unsigned int len, unsigned int &outLen);
// 打包数据
#if OPEN_UV_THREAD_HEARTBEAT == 1
NET_UV_EXTERN uv_buf_t* packageData(char* data, unsigned int len, int* bufCount, TCPMsgTag msgTag);
#else
NET_UV_EXTERN uv_buf_t* packageData(char* data, unsigned int len, int* bufCount);
#endif
// 打包心跳消息
#if OPEN_UV_THREAD_HEARTBEAT == 1
NET_UV_EXTERN char* packageHeartMsgData(char msg, unsigned int* outBufSize);
#endif
NS_NET_UV_END