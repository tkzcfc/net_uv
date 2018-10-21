#pragma once

#include "Common.h"

NS_NET_UV_BEGIN

std::string net_getUVError(int errcode);

std::string net_getTime();

void net_alloc_buffer(uv_handle_t* handle, size_t  size, uv_buf_t* buf);

void net_closehandle_defaultcallback(uv_handle_t* handle);

void net_closeHandle(uv_handle_t* handle, uv_close_cb closecb);

// 调整socket缓冲区大小
void net_adjustBuffSize(uv_handle_t* handle, int minRecvBufSize, int minSendBufSize);

//hash
unsigned int net_getBufHash(const void *buf, unsigned int len);

unsigned int net_getsockAddrIPAndPort(const struct sockaddr* addr, std::string& outIP, unsigned int& outPort);

struct sockaddr* net_getsocketAddr(const char* ip, unsigned int port, unsigned int* outAddrLen);

struct sockaddr* net_getsocketAddr_no(const char* ip, unsigned int port, bool isIPV6, unsigned int* outAddrLen);

unsigned int net_getsockAddrPort(const struct sockaddr* addr);

struct sockaddr* net_tcp_getAddr(const uv_tcp_t* handle);

unsigned int net_getAddrPort(const struct sockaddr* addr);

unsigned int net_udp_getPort(uv_udp_t* handle);

NS_NET_UV_END
