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

void Turn::on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	switch (msgID)
	{
	case net_uv::P2P_MSG_ID_C2T_CLIENT_LOGIN:
		on_msg_ClientLogin(document, key, addr);
		break;
	case net_uv::P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT:
		break;
	case net_uv::P2P_MSG_ID_C2T_GET_LIST:
		break;
	case net_uv::P2P_MSG_ID_T2C_GET_LIST_RESULT:
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
	
	if (document.IsArray())
	{
		rapidjson::Value obj = document.GetArray();

		info.interfaceCount = obj.Size();

		for (size_t i = 0; i < obj.Size(); ++i) 
		{
			if (i >= P2P_INTERFACE_ADDR_MAX)
			{
				info.interfaceCount = P2P_INTERFACE_ADDR_MAX;
				break;
			}

			rapidjson::Value & v = obj[i];

			if (v.HasMember("IP") && v["IP"].IsUint())
			{
				info.interfaceAddr[i].addr = v["IP"].GetUint();
			}
			if (v.HasMember("mask") && v["mask"].IsUint())
			{
				info.interfaceAddr[i].addr = v["mask"].GetUint();
			}
		}
	}

	auto it = m_nodeInfoMap.find(key);
	if (it == m_nodeInfoMap.end())
	{
		m_nodeInfoMap.insert(std::make_pair(key, info));
	}
	else
	{
		it->second = info;
	}

	// ·µ»ØµÇÂ¼½á¹û
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("ip");
	writer.Uint(info.addr.ip);
	writer.Key("port");
	writer.Uint(info.addr.port);

	writer.EndObject();

	this->sendMsg(P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT, (char*)s.GetString(), s.GetLength(), info.addr.ip, info.addr.port, 5);




	for (auto & it : m_nodeInfoMap)
	{
		rapidjson::StringBuffer s;
		rapidjson::Writer<rapidjson::StringBuffer> writer(s);

		writer.StartArray();
		for (auto & it_node : m_nodeInfoMap)
		{
			if (it.first != it_node.first)
			{
				writer.StartObject();
				writer.Key("id");
				writer.Uint64(it_node.first);
				writer.Key("ip");
				writer.Uint(it_node.second.addr.ip);
				writer.Key("port");
				writer.Uint(it_node.second.addr.port);

				writer.Key("intranet");
				writer.StartArray();

				for (int i = 0; i < it_node.second.interfaceCount; ++i)
				{
					writer.StartObject();
					writer.Key("ip");
					writer.Uint(it_node.second.interfaceAddr[i].addr);
					writer.Key("mask");
					writer.Uint(it_node.second.interfaceAddr[i].mask);
					writer.EndObject();
				}

				writer.EndArray();

				writer.EndObject();
			}
		}
		writer.EndArray();
		this->sendMsg(P2P_MSG_ID_T2C_GET_LIST_RESULT, (char*)s.GetString(), s.GetLength(), it.second.addr.ip, it.second.addr.port, 5);
	}
}

NS_NET_UV_END