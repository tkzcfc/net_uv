#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

void net_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf);

void net_closehandle_defaultcallback(uv_handle_t* handle);

void net_closeHandle(uv_handle_t* handle, uv_close_cb closecb);

// 调整socket缓冲区大小
void net_adjustBuffSize(uv_handle_t* handle, int minRecvBufSize, int minSendBufSize);

//hash
unsigned int net_getBufHash(const void *buf, unsigned int len);

unsigned int net_getsockAddrIPAndPort(const struct sockaddr* addr, std::string& outIP, unsigned int& outPort);

struct sockaddr* net_getsocketAddr(const char* ip, unsigned int port, unsigned int* outAddrLen);

NS_NET_UV_END
