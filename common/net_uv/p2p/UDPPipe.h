#pragma once

#include "P2PCommon.h"
#include "P2PMessage.h"

NS_NET_UV_BEGIN

class UDPPipe : public Runnable
{
public:

	UDPPipe();

	UDPPipe(const UDPPipe&) = delete;

	virtual ~UDPPipe();

	void stop();

	// 发送消息
	// 数据一直会在对方发送收到该条消息之前开启定时器不停的发，
	// 每发送一次timeoutCount减一，减到0时该条消息视为发送失败
	// 不在进行继续发送，这么做的目的是尽量保证消息发送成功...
	void sendMsg(P2PMessageID msgID, char* data, uint32_t len, uint32_t toIP, uint32_t toPort, uint32_t timeoutCount = 30);

protected:

	bool startBind(const char* bindIP, uint32_t bindPort);

	void startLoop();

	void startTimer(uint64_t time);

	void stopTimer();

protected:

	virtual void startFailureLogic();

	virtual void on_udp_read(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf, const struct sockaddr* addr, unsigned flags);

	virtual void on_recv_msg(P2PMessageID msgID, rapidjson::Document& document, uint64_t key, const struct sockaddr* addr) = 0;

	virtual void onIdleRun();

	virtual void onTimerRun();

	virtual void onStop();


	void setIsClient(bool isClient);

	uint64_t newUniqueID();

	void clearData();

protected:
	/// Runnable
	virtual void run()override;

	static void uv_on_idle_run(uv_idle_t* handle);

	static void uv_on_timer_run(uv_timer_t* handle);

protected:

	UDPSocket* m_socket;
	uv_loop_t m_loop;
	uv_idle_t m_idle;
	uv_timer_t m_timer;

	uint64_t m_curUniqueID;
	uint64_t m_uniqueIDBegin;
	uint64_t m_uniqueIDEnd;

	enum RunState
	{
		RUN,
		WILL_STOP,
		STOP
	};

	RunState m_state;


	struct SendDataInfo
	{
		P2PMessage* msg;
		uint16_t sendCount;
		sockaddr_in send_addr;
	};

	struct PIPEData
	{
		// 发送超时的数据
		// key: 消息uniqueID
		// value : 过期时间
		std::map<uint64_t, uint64_t> sendTimeOutMap;

		// 正在发送的数据
		std::map<uint64_t, SendDataInfo> sendMap;
	};

	std::map<uint64_t, PIPEData> m_pipeInfoMap;

	Mutex m_pipeLock;
};

NS_NET_UV_END
