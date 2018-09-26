//#include "UDPClient.h"
//
//UDPClient::UDPClient()
//{
//	uv_loop_init(&m_loop);
//
//	uv_idle_init(&m_loop, &m_idle);
//	m_idle.data = this;
//	uv_idle_start(&m_idle, uv_on_idle_run);
//
//	uv_mutex_init(&m_sessionMutex);
//	
//	this->startThread();
//}
//
//UDPClient::~UDPClient()
//{
//	this->join();
//	uv_mutex_destroy(&m_sessionMutex);
//}
//
////开始连接
//bool UDPClient::connect(const char* ip, int port, unsigned int key)
//{
//	uv_mutex_lock(&m_sessionMutex);
//
//	auto it = m_allSession.find(key);
//	if (it != m_allSession.end())
//	{
//		uv_mutex_unlock(&m_sessionMutex);
//		return false;
//	}
//	UDPSocket* s = new UDPSocket(&m_loop);
//	s->setSocketCall(on_udp_socket_call, this);
//
//	KcpSession* session = new KcpSession();
//	session->init(key, s);
//	m_allSession.insert(std::make_pair(key, session));
//
//	uv_mutex_unlock(&m_sessionMutex);
//	return true;
//}
//
//void UDPClient::disconnect(unsigned int key)
//{
//	uv_mutex_lock(&m_sessionMutex);
//
//	auto it = m_allSession.find(key);
//	if (it == m_allSession.end())
//	{
//		uv_mutex_unlock(&m_sessionMutex);
//		return;
//	}
//	it->second->getSocket()->close();
//
//	uv_mutex_unlock(&m_sessionMutex);
//}
//
////关闭客户端，调用此函数之后客户端将准备退出线程，该类其他方法将统统无法调用
//void UDPClient::closeClient()
//{
//
//}
//
//void UDPClient::run()
//{
//	m_loop.data = NULL;
//
//	uv_run(&m_loop, UV_RUN_DEFAULT);
//
//	uv_loop_close(&m_loop);
//}
//
//void UDPClient::idle_run()
//{
//	for (auto& it : m_allSession)
//	{
//		it.second->update();
//	}
//}
//
//void UDPClient::removeSessionBySocket(UDPSocket* socket)
//{
//	uv_mutex_lock(&m_sessionMutex);
//
//	UDPSocket* s;
//	for (auto it = m_allSession.begin(); it != m_allSession.end(); ++it)
//	{
//		s = it->second->getSocket();
//		if (s == socket)
//		{
//			if (s)
//			{
//				delete s;
//			}
//			delete it->second;
//			m_allSession.erase(it);
//			break;
//		}
//	}
//
//	uv_mutex_unlock(&m_sessionMutex);
//}
//
////////////////////////////////////////////////////////////////////////////
//void UDPClient::on_udp_socket_call(udp_socket_call_type type, UDPSocket* s, void* userdata)
//{
//	UDPClient* client = (UDPClient*)userdata;
//	switch (type)
//	{
//	case resolve_suc:
//		break;
//	case resolve_fail:
//		break;
//	case recv_stop:
//		client->removeSessionBySocket(s);
//		break;
//	default:
//		break;
//	}
//}
//
//void UDPClient::uv_on_idle_run(uv_idle_t* handle)
//{
//	UDPClient* c = (UDPClient*)handle->data;
//	c->idle_run();
//	ThreadSleep(1);
//}
//
