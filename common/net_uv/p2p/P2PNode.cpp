#include "P2PNode.h"

NS_NET_UV_BEGIN

P2PNode::P2PNode()
{
	setIsClient(true);
	m_count = 0;

	m_selfAddr.key = 0;
	m_turnAddr.key = 0;
}

P2PNode::~P2PNode()
{}

bool P2PNode::start(const char* turnIP, uint32_t turnPort)
{
	if (m_state != RunState::STOP)
	{
		return false;
	}

	if (startBind("0.0.0.0", 0) == false)
	{
		return false;
	}

	m_state = RunState::RUN;

	this->startLoop();

	getIntranetIP(m_InterfaceAddressInfo);

	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartArray();

	for (auto i = 0; i < m_InterfaceAddressInfo.size(); ++i)
	{
		writer.StartObject();
		writer.Key("IP");
		writer.Uint(m_InterfaceAddressInfo[i].addr);
		writer.Key("mask");
		writer.Uint(m_InterfaceAddressInfo[i].mask);
		writer.EndObject();
	}

	writer.EndArray();

	m_turnAddr.ip = inet_addr(turnIP);
	m_turnAddr.port = turnPort;

	const char* sendData = s.GetString();
	this->sendMsg(P2PMessageID::P2P_MSG_ID_C2T_CLIENT_LOGIN, (char*)sendData, s.GetLength(), m_turnAddr.ip, m_turnAddr.port);

	return true;
}

	/// UDPPipe
void P2PNode::onIdleRun()
{
	UDPPipe::onIdleRun();

	ThreadSleep(1);
}


void P2PNode::onTimerRun()
{
	UDPPipe::onTimerRun();

	m_count++;
	if (m_count >= 15 && m_selfAddr.key != 0)
	{
		m_count = 0;

		rapidjson::StringBuffer s;
		rapidjson::Writer<rapidjson::StringBuffer> writer(s);

		writer.StartObject();
		writer.Key("hello");
		writer.String("hello");
		writer.EndObject();

		for (auto& it : m_userList)
		{
			if (it.key != m_selfAddr.key)
			{
				this->sendMsg(P2P_MSG_ID_C2C_HELLO, (char*)s.GetString(), s.GetLength(), it.ip, it.port, 0);
			}
		}
	}
}

void P2PNode::on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	switch (msgID)
	{
	case net_uv::P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT:
		on_msg_ClientLoginResult(document, key, addr);
		break;
	case net_uv::P2P_MSG_ID_T2C_GET_LIST_RESULT:
		on_msg_UserListResult(document, key, addr);
		break;
	}
}

void P2PNode::on_msg_ClientLoginResult(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	m_selfAddr.ip = document["ip"].GetUint();
	m_selfAddr.port = document["port"].GetUint();
}

void P2PNode::on_msg_UserListResult(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	m_userList.clear();

	AddrInfo info;

	if (document.IsArray())
	{
		rapidjson::Value obj = document.GetArray();
		for (size_t i = 0; i < obj.Size(); ++i)
		{
			rapidjson::Value & v = obj[i];

			if (v.HasMember("ip") && v["ip"].IsUint())
			{
				info.ip = v["ip"].GetUint();
			}
			if (v.HasMember("port") && v["port"].IsUint())
			{
				info.port = v["port"].GetUint();
			}
			m_userList.push_back(info);
		}
	}
}

void P2PNode::getIntranetIP(std::vector<P2PIntranet_Address>& IPArr)
{
	uv_interface_address_t *info;
	int count, i;

	uv_interface_addresses(&info, &count);
	i = count;

	P2PIntranet_Address addressData;
	while (i--)
	{
		uv_interface_address_t interface = info[i];

		if (!interface.is_internal && interface.address.address4.sin_family == AF_INET)
		{
			//char buf[256];
			//uv_ip4_name(&interface.address.address4, buf, sizeof(buf));
			//printf("ip: %s\n", buf);
			//uv_ip4_name(&interface.netmask.netmask4, buf, sizeof(buf));
			//printf("mask: %s\n", buf);

			addressData.addr = interface.address.address4.sin_addr.s_addr;
			addressData.mask = interface.netmask.netmask4.sin_addr.s_addr;

			//char * ipaddr = NULL;
			//char addr[20];
			//in_addr inaddr;
			//inaddr.s_addr = addressData.addr;
			//ipaddr = inet_ntoa(inaddr);
			//strcpy(addr, ipaddr);
			//printf("new ip: %s\n", addr);

			//char* ipaddr_mask = NULL;
			//in_addr inaddr_mask;
			//inaddr_mask.s_addr = addressData.mask;
			//ipaddr_mask = inet_ntoa(inaddr_mask);
			//strcpy(addr, ipaddr_mask);
			//printf("new mask: %s\n", addr);

			IPArr.push_back(addressData);
		}
	}
	uv_free_interface_addresses(info, count);
}


NS_NET_UV_END
