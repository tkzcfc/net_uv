#pragma once

#include "P2P_UCommon.h"
#include "P2P_UMessage.h"

NS_NET_UV_BEGIN

class P2P_UNode
{
public:
	void send(P2P_UMessageType msgType, uint32_t toIP, uint32_t toPort, char* data, uint32_t len);

	void sendKcp(uint32_t toIP, uint32_t toPort, char* data, uint32_t len);

	void sendPing(uint32_t toIP, uint32_t toPort);

	void sendPong(uint32_t toIP, uint32_t toPort);

	
	
	std::map<uint64_t, ikcpcb*> m_kcpManager;
	long xx = MAKELONG(0, 0);
};


NS_NET_UV_END