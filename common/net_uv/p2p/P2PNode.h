#pragma once

#include "P2PCommon.h"
#include "UDPPipe.h"

NS_NET_UV_BEGIN

class P2PNode : public UDPPipe
{
public:
	P2PNode();

	P2PNode(const P2PNode&) = delete;

	virtual ~P2PNode();

	bool start(const char* turnIP, uint32_t turnPort);

protected:
	// 获取内网IP(只获取IPV4地址)
	void getIntranetIP(std::vector<P2PIntranet_Address>& IPArr);

	/// UDPPipe
	virtual void onIdleRun()override;

	void onTimerRun();

	virtual void on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)override;

	void on_msg_ClientLoginResult(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);

	void on_msg_UserListResult(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);

public:

	std::vector<P2PIntranet_Address> m_InterfaceAddressInfo;

	AddrInfo m_turnAddr;	// turn地址信息
	AddrInfo m_selfAddr;	// 自己地址信息

	uint32_t m_count;


	std::vector<AddrInfo> m_userList;
};

NS_NET_UV_END