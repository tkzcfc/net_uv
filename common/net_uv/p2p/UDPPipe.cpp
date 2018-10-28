#include "UDPPipe.h"

NS_NET_UV_BEGIN


UDPPipe::UDPPipe()
	: m_socket(NULL)
	, m_state(RunState::STOP)
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

	if (m_socket->bind(bindIP, bindPort) == 0)
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
		m_pipeLock.lock();

		auto it = m_pipeInfoMap.find(info.key);
		if (it != m_pipeInfoMap.end())
		{
			auto it_send = it->second.sendMap.find(msg->uniqueID);
			if (it_send != it->second.sendMap.end())
			{
				fc_free(it_send->second.msg);
				it->second.sendMap.erase(it_send);
			}
			else
			{
				auto it_timout = it->second.sendTimeOutMap.find(msg->uniqueID);
				if (it_timout != it->second.sendTimeOutMap.end())
				{
					it->second.sendTimeOutMap.erase(it_timout);
				}
			}
		}

		m_pipeLock.unlock();
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

	// 向对方通知收到该消息
	P2PMessage send_msg;
	send_msg.msgID = P2P_MSG_RECV_MSG_RESULT__EX;
	send_msg.msgLen = 0;
	send_msg.uniqueID = msg->uniqueID;
	m_socket->udpSend((const char*)&send_msg, sizeof(P2PMessage), addr);
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
	if (m_pipeLock.trylock() != 0)
	{
		return;
	}

	time_t curTime = time(NULL);

	for (auto & it : m_pipeInfoMap)
	{
		if (it.second.sendMap.empty() == false)
		{
			for (auto it_send = it.second.sendMap.begin(); it_send != it.second.sendMap.end(); )
			{
				//std::string send_str((char*)&it_send->second.msg[1], it_send->second.msg->msgLen);
				//NET_UV_LOG(NET_UV_L_INFO, "send[%d]: %s", it_send->second.sendCount, send_str.c_str());

				m_socket->udpSend((const char*)it_send->second.msg, (it_send->second.msg->msgLen + sizeof(P2PMessage)), (const struct sockaddr*)&it_send->second.send_addr);
				
				it_send->second.sendCount--;

				if (it_send->second.sendCount <= 0)
				{
					fc_free(it_send->second.msg);
					it.second.sendTimeOutMap.insert(std::make_pair(it_send->first, curTime + 120));
					it_send = it.second.sendMap.erase(it_send);
				}
				else
				{
					it_send++;
				}
			}
		}
		if (it.second.sendTimeOutMap.empty() == false)
		{
			for (auto it_timeout = it.second.sendTimeOutMap.begin(); it_timeout != it.second.sendTimeOutMap.end(); )
			{
				if (it_timeout->second >= curTime)
				{
					it_timeout = it.second.sendTimeOutMap.erase(it_timeout);
				}
				else
				{
					it_timeout++;
				}
			}
		}
	}

	m_pipeLock.unlock();
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

	m_pipeLock.lock();
	auto it = m_pipeInfoMap.find(info.key);
	if (it == m_pipeInfoMap.end())
	{
		PIPEData pipeData;
		pipeData.sendMap.insert(std::make_pair(uniqueID, sendInfo));
		m_pipeInfoMap.insert(std::make_pair(info.key, pipeData));
	}
	else
	{
		it->second.sendMap.insert(std::make_pair(uniqueID, sendInfo));
	}
	m_pipeLock.unlock();
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
	m_pipeLock.lock();

	for (auto it : m_pipeInfoMap)
	{
		for (auto it_send : it.second.sendMap)
		{
			fc_free(it_send.second.msg);
		}
	}
	m_pipeInfoMap.clear();

	m_pipeLock.unlock();
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