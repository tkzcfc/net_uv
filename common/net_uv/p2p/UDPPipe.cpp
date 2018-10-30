#include "UDPPipe.h"

NS_NET_UV_BEGIN


UDPPipe::UDPPipe()
	: m_socket(NULL)
	, m_state(RunState::STOP)
	, m_overdueDetectionSpace(0)
	, m_invalidSendMsgDetectionSpace(0)
{
	memset(&m_loop, 0, sizeof(uv_loop_t));
	memset(&m_idle, 0, sizeof(uv_idle_t));
	memset(&m_timer, 0, sizeof(uv_timer_t));

	setIsClient(false);
}

UDPPipe::~UDPPipe()
{
	stop();
	join();
}

void UDPPipe::stop()
{
	if (m_state != RunState::STOP)
	{
		m_state = RunState::WILL_STOP;
	}
}

bool UDPPipe::startBind(const char* bindIP, uint32_t bindPort)
{
	uv_loop_init(&m_loop);

	m_socket = (UDPSocket*)fc_malloc(sizeof(UDPSocket));
	new (m_socket)UDPSocket(&m_loop);

	m_socket->setReadCallback(std::bind(&UDPPipe::on_udp_read, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5));

	bindPort = m_socket->bind(bindIP, bindPort);

	NET_UV_LOG(NET_UV_L_INFO, "bind [%u]", bindPort);

	if (bindPort == 0)
	{
		startFailureLogic();
		return false;
	}

	return true;
}

void UDPPipe::startLoop()
{
	startThread();
}

void UDPPipe::startFailureLogic()
{
	m_socket->~UDPSocket();
	fc_free(m_socket);
	m_socket = NULL;

	uv_run(&m_loop, UV_RUN_DEFAULT);
	uv_loop_close(&m_loop);
}

void UDPPipe::run()
{
	uv_idle_init(&m_loop, &m_idle);
	m_idle.data = this;
	uv_idle_start(&m_idle, UDPPipe::uv_on_idle_run);

	startTimer(100);

	uv_run(&m_loop, UV_RUN_DEFAULT);
	uv_loop_close(&m_loop);

	m_state = RunState::STOP;
}

void UDPPipe::on_udp_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags)
{
	// 不支持IPV6
	if (addr->sa_family != AF_INET)
		return;

	// 长度校验失败
	if (nread < sizeof(P2PMessage))
		return;

	P2PMessage* msg = (P2PMessage*)buf->base;

	// 长度校验失败
	if (msg->msgLen != nread - sizeof(P2PMessage))
	{
		return;
	}

	// 消息ID校验失败
	if (msg->msgID <= P2PMessageID::P2P_MSG_ID_BEGIN || P2PMessageID::P2P_MSG_ID_END <= msg->msgID)
		return;

	const struct sockaddr_in* recv_addr = (const struct sockaddr_in*)addr;

	AddrInfo info;
	info.ip = recv_addr->sin_addr.s_addr;
	info.port = ntohs(recv_addr->sin_port);

	// 对方收到消息的回复
	if (msg->msgID == P2P_MSG_RECV_MSG_RESULT__EX)
	{
		// 将对应消息移除发送队列
		m_sendLock.lock();

		auto it = m_sendMsgMap.find(info.key);
		if (it != m_sendMsgMap.end())
		{
			auto it_send = it->second.find(msg->uniqueID);
			if (it_send != it->second.end())
			{
				fc_free(it_send->second.msg);
				it->second.erase(it_send);
			}
		}

		m_sendLock.unlock();
		return;
	}

	// 心跳消息
	if (msg->msgID == P2P_MSG_ID_HEART_SEND || msg->msgID == P2P_MSG_ID_HEART_RESULT)
	{
		// 心跳探测消息
		if (msg->msgID == P2P_MSG_ID_HEART_SEND)
		{
			P2PMessage send_msg;
			send_msg.msgID = P2P_MSG_ID_HEART_RESULT;
			send_msg.msgLen = 0;
			send_msg.uniqueID = 0;
			m_socket->udpSend((const char*)&send_msg, sizeof(P2PMessage), addr);
		}
		//printf("recv heart\n");
		addMsgToRecv(info.key, 0, addr);
		return;
	}

	// 向对方通知收到该消息
	P2PMessage send_msg;
	send_msg.msgID = P2P_MSG_RECV_MSG_RESULT__EX;
	send_msg.msgLen = 0;
	send_msg.uniqueID = msg->uniqueID;
	m_socket->udpSend((const char*)&send_msg, sizeof(P2PMessage), addr);

	// 消息已经接收过
	if (!addMsgToRecv(info.key, msg->uniqueID, addr))
	{
		return;
	}

	// 合法消息ID
	char* data = buf->base + sizeof(P2PMessage);

	rapidjson::Document document;
	document.Parse(data, msg->msgLen);

	std::string recv_str(data, msg->msgLen);
	NET_UV_LOG(NET_UV_L_INFO, "recv[%u]:[%u] : %s", info.ip, info.port, recv_str.c_str());

	if (!document.HasParseError())
	{
		on_recv_msg((P2PMessageID)msg->msgID, document, info.key, addr);
	}
	else
	{
		NET_UV_LOG(NET_UV_L_ERROR, "json parse error: %u", document.GetParseError());
	}
}

void UDPPipe::onIdleRun()
{
	if (m_state == RunState::WILL_STOP)
	{
		onStop();
	}
}

void UDPPipe::onTimerRun()
{
	if (m_sendLock.trylock() != 0)
	{
		return;
	}

	for (auto& it : m_sendMsgMap)
	{
		for (auto it_send = it.second.begin(); it_send != it.second.end(); )
		{
			//std::string send_str((char*)&it_send->second.msg[1], it_send->second.msg->msgLen);
			//NET_UV_LOG(NET_UV_L_INFO, "send[%d]: %s", it_send->second.sendCount, send_str.c_str());

			m_socket->udpSend((const char*)it_send->second.msg, (it_send->second.msg->msgLen + sizeof(P2PMessage)), (const struct sockaddr*)&it_send->second.send_addr);

			it_send->second.sendCount--;

			if (it_send->second.sendCount <= 0)
			{
				fc_free(it_send->second.msg);
				it_send = it.second.erase(it_send);
			}
			else
			{
				it_send++;
			}
		}
	}
	m_sendLock.unlock();

	overdueDetection();
}

void UDPPipe::startTimer(uint64_t time)
{
	stopTimer();

	uv_timer_init(&m_loop, &m_timer);
	m_timer.data = this;

	uv_timer_start(&m_timer, UDPPipe::uv_on_timer_run, 0, time);
}

void UDPPipe::stopTimer()
{
	if (m_timer.data)
	{
		uv_timer_stop(&m_timer);
		m_timer.data = NULL;
	}
}

void UDPPipe::onStop()
{
	if (m_socket)
	{
		m_socket->~UDPSocket();
		fc_free(m_socket);
		m_socket = NULL;
	}
	uv_idle_stop(&m_idle);
	stopTimer();
}

void UDPPipe::onSessionRemove(uint64_t sessionID)
{
	m_sendLock.lock();

	auto it = m_sendMsgMap.find(sessionID);
	if (it == m_sendMsgMap.end())
	{
		m_sendLock.unlock();
		return;
	}
	m_sendMsgMap.erase(it);

	m_sendLock.unlock();
}

void UDPPipe::onNewSession(uint64_t sessionID)
{}

void UDPPipe::sendMsg(P2PMessageID msgID, char* data, uint32_t len, uint32_t toIP, uint32_t toPort, uint32_t timeoutCount/* = 10*/)
{
	char* senddata = (char*)fc_malloc(sizeof(P2PMessage) + len);

	uint64_t uniqueID = newUniqueID();;

	P2PMessage* msg = (P2PMessage*)senddata;
	msg->msgID = msgID;
	msg->msgLen = len;
	msg->uniqueID = uniqueID;

	if (len > 0)
	{
		memcpy(senddata + sizeof(P2PMessage), data, len);
	}
	
	AddrInfo info;
	info.ip = toIP;
	info.port = toPort;
	
	char * ipaddr = NULL;
	char addr[20];
	in_addr inaddr;
	inaddr.s_addr = toIP;
	ipaddr = inet_ntoa(inaddr);
	strcpy(addr, ipaddr);

	SendDataInfo sendInfo;
	sendInfo.msg = msg;
	sendInfo.sendCount = timeoutCount;
	uv_ip4_addr(addr, toPort, &sendInfo.send_addr);

	m_sendLock.lock();
	auto it = m_sendMsgMap.find(info.key);
	if (it == m_sendMsgMap.end())
	{
		std::map<uint64_t, SendDataInfo> tmpMap;
		tmpMap.insert(std::make_pair(uniqueID, sendInfo));
		m_sendMsgMap.insert(std::make_pair(info.key, tmpMap));
	}
	else
	{
		it->second.insert(std::make_pair(uniqueID, sendInfo));
	}
	m_sendLock.unlock();
}

uint64_t UDPPipe::newUniqueID()
{
	m_curUniqueID++;
	if (m_curUniqueID > m_uniqueIDEnd)
	{
		m_curUniqueID = m_uniqueIDBegin;
	}
	return m_curUniqueID;
}

void UDPPipe::setIsClient(bool isClient)
{
	if (isClient)
	{
		m_uniqueIDBegin = 0xFF;
		m_uniqueIDEnd = 0xFFFFEE;
		m_curUniqueID = m_uniqueIDBegin;
	}
	else
	{
		m_uniqueIDBegin = 0xFFFFFF;
		m_uniqueIDEnd = 0xFFFFFFFF;
		m_curUniqueID = m_uniqueIDBegin;
	}
}

void UDPPipe::clearData()
{
	m_sendLock.lock();

	for (auto it : m_sendMsgMap)
	{
		for (auto it_send : it.second)
		{
			fc_free(it_send.second.msg);
		}
	}

	m_sendLock.unlock();

	m_allSessionDataMap.clear();
}

bool UDPPipe::addMsgToRecv(uint64_t sessionID, uint64_t uniqueID, const struct sockaddr* addr)
{
	auto it_info = m_allSessionDataMap.find(sessionID);
	if (it_info == m_allSessionDataMap.end())
	{
		const struct sockaddr_in* recv_addr = (const struct sockaddr_in*)addr;

		SessionData data;
		memcpy(&data.send_addr, recv_addr, sizeof(struct sockaddr_in));
		data.recvData(uniqueID);

		m_allSessionDataMap.insert(std::make_pair(sessionID, data));

		this->onNewSession(sessionID);

		return true;
	}
	return it_info->second.recvData(uniqueID);
}

void UDPPipe::overdueDetection()
{
	m_overdueDetectionSpace++;

	if (m_overdueDetectionSpace < 20)
	{
		return;
	}
	m_overdueDetectionSpace = 0;

	//printf("update\n");

	auto curTime = time(NULL);

	// 心跳检测
	for (auto it = m_allSessionDataMap.begin(); it != m_allSessionDataMap.end(); )
	{
		if (it->second.recvDataMap.empty() == false)
		{
			for (auto it_recv = it->second.recvDataMap.begin(); it_recv != it->second.recvDataMap.end(); )
			{
				if (it_recv->second > curTime)
				{
					it_recv = it->second.recvDataMap.erase(it_recv);
				}
				else
				{
					it_recv++;
				}
			}
		}

		if (it->second.noResponseCount >= 4)
		{
			onSessionRemove(it->first);
			it = m_allSessionDataMap.erase(it);
		}
		else
		{
			it->second.noResponseCount++;

			P2PMessage send_msg;
			send_msg.msgID = P2P_MSG_ID_HEART_RESULT;
			send_msg.msgLen = 0;
			send_msg.uniqueID = 0;
			m_socket->udpSend((const char*)&send_msg, sizeof(P2PMessage), (const sockaddr*)&it->second.send_addr);
			it++;
		}
	}

	// 无效发送数据Map删除
	m_invalidSendMsgDetectionSpace++;
	if (m_invalidSendMsgDetectionSpace < 5)
	{
		return;
	}
	m_invalidSendMsgDetectionSpace = 0;

	if (m_sendLock.trylock() != 0)
	{
		return;
	}

	for (auto it = m_sendMsgMap.begin(); it != m_sendMsgMap.end(); )
	{
		if (it->second.empty())
		{
			auto it_session = m_allSessionDataMap.find(it->first);
			if (it_session == m_allSessionDataMap.end())
			{
				it = m_sendMsgMap.erase(it);
			}
			else
			{
				it++;
			}
		}
		else
		{
			it++;
		}
	}

	m_sendLock.unlock();
}

void UDPPipe::uv_on_idle_run(uv_idle_t* handle)
{
	((UDPPipe*)handle->data)->onIdleRun();
}

void UDPPipe::uv_on_timer_run(uv_timer_t* handle)
{
	((UDPPipe*)handle->data)->onTimerRun();
}

NS_NET_UV_END