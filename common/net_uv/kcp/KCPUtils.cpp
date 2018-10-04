#include "KcpUtils.h"

NS_NET_UV_BEGIN


#define NET_KCP_CONNECT_PACKET "kcp_connect_package get_conv"
#define NET_KCP_SEND_BACK_CONV_PACKET "kcp_connect_back_package get_conv:"
#define NET_KCP_DISCONNECT_PACKET "kcp_disconnect_package"
#define NET_KCP_HEART_PACKET "kcp_heart_package"
#define NET_KCP_HEART_BACK_PACKET "kcp_heart_back_package"

std::string kcp_making_connect_packet(void)
{
	return std::string(NET_KCP_CONNECT_PACKET, sizeof(NET_KCP_CONNECT_PACKET));
}

bool kcp_is_connect_packet(const char* data, size_t len)
{
	return (len == sizeof(NET_KCP_CONNECT_PACKET) &&
		memcmp(data, NET_KCP_CONNECT_PACKET, sizeof(NET_KCP_CONNECT_PACKET) - 1) == 0);
}

bool kcp_is_send_back_conv_packet(const char* data, size_t len)
{
	return (len > sizeof(NET_KCP_SEND_BACK_CONV_PACKET) &&
		memcmp(data, NET_KCP_SEND_BACK_CONV_PACKET, sizeof(NET_KCP_SEND_BACK_CONV_PACKET) - 1) == 0);
}

std::string kcp_making_send_back_conv_packet(uint32_t conv)
{
	char str_send_back_conv[256] = "";
	size_t n = snprintf(str_send_back_conv, sizeof(str_send_back_conv), "%s %u", NET_KCP_SEND_BACK_CONV_PACKET, conv);
	return std::string(str_send_back_conv, n);
}

uint32_t kcp_grab_conv_from_send_back_conv_packet(const char* data, size_t len)
{
	uint32_t conv = atol(data + sizeof(NET_KCP_SEND_BACK_CONV_PACKET));
	return conv;
}




std::string kcp_making_disconnect_packet(uint32_t conv)
{
	char str_disconnect_packet[256] = "";
	size_t n = snprintf(str_disconnect_packet, sizeof(str_disconnect_packet), "%s %u", NET_KCP_DISCONNECT_PACKET, conv);
	return std::string(str_disconnect_packet, n);
}

bool kcp_is_disconnect_packet(const char* data, size_t len)
{
	return (len > sizeof(NET_KCP_DISCONNECT_PACKET) &&
		memcmp(data, NET_KCP_DISCONNECT_PACKET, sizeof(NET_KCP_DISCONNECT_PACKET) - 1) == 0);
}

uint32_t kcp_grab_conv_from_disconnect_packet(const char* data, size_t len)
{
	uint32_t conv = atol(data + sizeof(NET_KCP_DISCONNECT_PACKET));
	return conv;
}



std::string kcp_making_heart_packet()
{
	return std::string(NET_KCP_HEART_PACKET, sizeof(NET_KCP_HEART_PACKET));
}

bool kcp_is_heart_packet(const char* data, size_t len)
{
	return (len == sizeof(NET_KCP_HEART_PACKET) &&
		memcmp(data, NET_KCP_HEART_PACKET, sizeof(NET_KCP_HEART_PACKET) - 1) == 0);
}

std::string kcp_making_heart_back_packet()
{
	return std::string(NET_KCP_HEART_BACK_PACKET, sizeof(NET_KCP_HEART_BACK_PACKET));
}

bool kcp_is_heart_back_packet(const char* data, size_t len)
{
	return (len == sizeof(NET_KCP_HEART_BACK_PACKET) &&
		memcmp(data, NET_KCP_HEART_BACK_PACKET, sizeof(NET_KCP_HEART_BACK_PACKET) - 1) == 0);
}

NS_NET_UV_END
