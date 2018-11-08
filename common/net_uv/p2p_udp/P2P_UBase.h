#pragma once

#include "P2P_UCommon.h"

NS_NET_UV_BEGIN

class P2P_UBase
{
public:

	void sendTo(uint32_t toIP, uint32_t toPort, char* data, uint32_t len);

};

NS_NET_UV_END
