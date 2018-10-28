#pragma once

#include "P2PCommon.h"
#include "UDPPipe.h"

NS_NET_UV_BEGIN

class Turn : public UDPPipe
{
public:
	Turn();

	Turn(const Turn&) = delete;

	virtual ~Turn();

	bool start(const char* ip, uint32_t port);

protected:

	/// UDPPipe
	virtual void onIdleRun()override;

	virtual void on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)override;


	void on_msg_ClientLogin(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr);

protected:

	std::map<uint64_t, P2PNodeInfo> m_nodeInfoMap;

};

NS_NET_UV_END
