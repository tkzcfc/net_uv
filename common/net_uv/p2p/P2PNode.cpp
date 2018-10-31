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
	
	m_turnAddr.ip = inet_addr(turnIP);
	m_turnAddr.port = turnPort;

	const char* loginJson = "{}";

	this->sendMsg(P2PMessageID::P2P_MSG_ID_C2T_CLIENT_LOGIN, (char*)loginJson, sizeof(loginJson), m_turnAddr.ip, m_turnAddr.port);

	return true;
}

void P2PNode::connect(uint64_t key)
{
	send_WantConnect(key);
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

void P2PNode::onSessionRemove(uint64_t sessionID)
{
	UDPPipe::onSessionRemove(sessionID);

	for (auto it = m_userList.begin(); it != m_userList.end(); )
	{
		if (it->key == sessionID)
		{
			printf("remove session %llu\n", sessionID);
			it = m_userList.erase(it);
		}
		else
		{
			it++;
		}
	}
}

void P2PNode::onNewSession(uint64_t sessionID)
{
	//UDPPipe::onNewSession(sessionID);
	printf("new session %llu\n", sessionID);

	if (m_turnAddr.key == sessionID)
		return;

	AddrInfo info;
	info.key = sessionID;
	
	m_userList.push_back(info);
}

void P2PNode::send_WantConnect(uint64_t key)
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();

	writer.Key("key");
	writer.Uint64(key);

	writer.EndObject();

	this->sendMsg(P2P_MSG_ID_C2T_WANT_TO_CONNECT, (char*)s.GetString(), s.GetLength(), m_turnAddr.ip, m_turnAddr.port);

	AddrInfo sendInfo;
	sendInfo.key = key;
	send_Hello(sendInfo.ip, sendInfo.port);
}

void P2PNode::send_Hello(uint32_t send_ip, uint32_t send_port)
{
	rapidjson::StringBuffer s;
	rapidjson::Writer<rapidjson::StringBuffer> writer(s);

	writer.StartObject();
	writer.Key("hello");
	writer.String("hello");
	writer.EndObject();

	printf("StartBurrow to %u : %u->\n", send_ip, send_port);

	this->sendMsg(P2P_MSG_ID_C2C_HELLO, (char*)s.GetString(), s.GetLength(), send_ip, send_port, 100);
}

void P2PNode::on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	switch (msgID)
	{
	case net_uv::P2P_MSG_ID_T2C_CLIENT_LOGIN_RESULT:
		on_msg_ClientLoginResult(document, key, addr);
		break;
	case net_uv::P2P_MSG_ID_T2C_START_BURROW:
		on_msg_StartBurrow(document, key, addr);
		break;
	}
}

void P2PNode::on_msg_ClientLoginResult(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	if (document.HasMember("ip") && document.HasMember("port"))
	{
		rapidjson::Value& ip_value = document["ip"];
		rapidjson::Value& port_value = document["port"];
		if (ip_value.IsUint() && port_value.IsUint())
		{
			m_selfAddr.ip = ip_value.GetUint();
			m_selfAddr.port = port_value.GetUint();

			printf("Login Suc my key : %llu->\n", m_selfAddr.key);
		}
	}
}

void P2PNode::on_msg_StartBurrow(rapidjson::Document& document, uint64_t key, const struct sockaddr* addr)
{
	if (document.HasMember("ip") && document.HasMember("port"))
	{
		rapidjson::Value& ip_value = document["ip"];
		rapidjson::Value& port_value = document["port"];
		if (ip_value.IsUint() && port_value.IsUint())
		{
			send_Hello(ip_value.GetUint(), port_value.GetUint());
		}
	}
}

NS_NET_UV_END
