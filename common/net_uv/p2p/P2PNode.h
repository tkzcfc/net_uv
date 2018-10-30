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

	/// UDPPipe
	virtual void onIdleRun()override;

	virtual void onTimerRun()override;

	virtual void on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)override;

	virtual void onSessionRemove(uint64_t sessionID)override;

	virtual void onNewSession(uint64_t sessionID)override;


	// send
	void send_WantConnect(uint64_t key);

	// 
	void on_msg_ClientLoginResult(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);

	void on_msg_StartBurrow(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);

public:
	
	AddrInfo m_turnAddr;	// turn地址信息
	AddrInfo m_selfAddr;	// 自己地址信息

	uint32_t m_count;


	std::vector<AddrInfo> m_userList;
};

NS_NET_UV_END