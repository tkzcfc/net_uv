#include "Turn.h"

NS_NET_UV_BEGIN

Turn::Turn()
{
}

Turn::~Turn()
{}

bool Turn::start(const char* ip, uint32_t port)
{
	if (m_state != RunState::STOP)
	{
		return false;
	}

	if (startBind(ip, port) == false)
	{
		return false;
	}

	m_state = RunState::RUN;

	this->startLoop();
	
	return true;
}

void Turn::onIdleRun()
{
	UDPPipe::onIdleRun();

	ThreadSleep(1);
}

void Turn::onSessionRemove(uint64_t sessionID)
{
	UDPPipe::onSessionRemove(sessionID);

	auto it = m_nodeInfoMap.find(sessionID);
	if (it != m_nodeInfoMap.end())
	{
		printf("remove user %llu\n", sessionID);
		m_nodeInfoMap.erase(it);
	}
}

void Turn::on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	switch (msgID)
	{
	case net_uv::P2P_MSG_ID_C2T_CLIENT_LOGIN:
		on_msg_ClientLogin(document, key, addr);
		break;
	case net_uv::P2P_MSG_ID_C2T_WANT_TO_CONNECT:
		on_msg_ClientWantConnect(document, key, addr);
		break;
	default:
		break;
	}
}

void Turn::on_msg_ClientLogin(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	P2PNodeInfo info;
	memset(&info, 0, sizeof(info));

	info.addr.key = key;

	auto it = m_nodeInfoMap.find(key);
	if (it == m_nodeInfoMap.end())
	{
		m_nodeInfoMap.insert(std::make_pair(key, info));
	}
	else
	{
		it->second = info;
	}

	// 返回登录结果
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("ip");
	writer.Uint(info.addr.ip);
	writer.Key("port");
	writer.Uint(info.addr.port);

	writer.EndObject();

	this->sendMsg(P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT, (char*)s.GetString(), s.GetLength(), info.addr.ip, info.addr.port);
}

void Turn::on_msg_ClientWantConnect(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	if (document.HasMember("key"))
	{
		rapidjson::Value& key_value = document["key"];
		if (key_value.IsUint64())
		{
			uint64_t want_connect_to_key = key_value.GetInt64();
			auto it = m_nodeInfoMap.find(want_connect_to_key);
			if (it != m_nodeInfoMap.end())
			{
				AddrInfo info;
				info.key = key;

				// 发送打洞指令
				rapidjson::StringBuffer s;
				rapidjson::Writer<rapidjson::StringBuffer> writer(s);

				writer.StartObject();

				writer.Key("ip");
				writer.Uint(info.ip);
				writer.Key("port");
				writer.Uint(info.port);

				writer.EndObject();

				this->sendMsg(P2P_MSG_ID_T2C_START_BURROW, (char*)s.GetString(), s.GetLength(), it->second.addr.ip, it->second.addr.port, 5);
			}
		}
	}
}

NS_NET_UV_END