///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Oct 26 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "MyFrame.h"

#include "../TestCommon.h"

static MyFrame* g_MyFrame;
static void uvLogOutput(int32_t level, const char* logstr)
{
	g_MyFrame->printLog(logstr);
}

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
EVT_TIMER(wxID_TIMER_ID, MyFrame::onTimer)
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////
MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer(wxVERTICAL);

	m_staticText4 = new wxStaticText(this, wxID_ANY, wxT("Turn地址:"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText4->Wrap(-1);
	bSizer2->Add(m_staticText4, 0, wxALL, 5);

	m_textCtrlTurnAddr = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_textCtrlTurnAddr, 0, wxALL | wxEXPAND, 5);

	m_buttonStart = new wxButton(this, wxID_START_PEER, wxT("启动"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_buttonStart, 0, wxALL | wxEXPAND, 5);

	m_buttonConnectToTurn = new wxButton(this, wxID_CONNECT_TO_TURN, wxT("连接到Turn"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_buttonConnectToTurn, 0, wxALL | wxEXPAND, 5);

	m_staticline1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
	bSizer2->Add(m_staticline1, 0, wxEXPAND | wxALL, 5);

	m_staticText1 = new wxStaticText(this, wxID_ANY, wxT("我的Key:"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText1->Wrap(-1);
	bSizer2->Add(m_staticText1, 0, wxALL, 5);

	m_textCtrlSelfKey = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
	bSizer2->Add(m_textCtrlSelfKey, 0, wxALL | wxEXPAND, 5);

	m_staticline2 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
	bSizer2->Add(m_staticline2, 0, wxEXPAND | wxALL, 5);

	m_staticText2 = new wxStaticText(this, wxID_ANY, wxT("请输入要连接的Key:"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText2->Wrap(-1);
	bSizer2->Add(m_staticText2, 0, wxALL, 5);

	m_textCtrlOtherKey = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_textCtrlOtherKey, 0, wxALL | wxEXPAND, 5);

	m_buttonConnectToPeer = new wxButton(this, wxID_CONNECT_TO_PEER, wxT("连接到Peer"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_buttonConnectToPeer, 0, wxALL | wxEXPAND, 5);

	m_staticline3 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
	bSizer2->Add(m_staticline3, 0, wxEXPAND | wxALL, 5);

	m_textCtrlSendMsg = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_textCtrlSendMsg, 0, wxALL | wxEXPAND, 5);

	m_buttonSendMsg = new wxButton(this, wxID_ANY, wxT("发送"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_buttonSendMsg, 0, wxALL | wxEXPAND, 5);

	m_staticline4 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);
	bSizer2->Add(m_staticline4, 0, wxEXPAND | wxALL, 5);

	m_buttonClearLog = new wxButton(this, wxID_CLEAR_LOG, wxT("清除日志"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer2->Add(m_buttonClearLog, 0, wxALL | wxEXPAND, 5);


	bSizer1->Add(bSizer2, 1, wxEXPAND, 5);

	wxStaticBoxSizer* sbSizer1;
	sbSizer1 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, wxEmptyString), wxVERTICAL);

	m_textCtrlLog = new wxTextCtrl(sbSizer1->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxTE_MULTILINE);
	sbSizer1->Add(m_textCtrlLog, 1, wxALL | wxEXPAND, 5);


	bSizer1->Add(sbSizer1, 3, wxEXPAND, 5);


	this->SetSizer(bSizer1);
	this->Layout();

	this->Centre(wxBOTH);

	// Connect Events
	this->Connect(wxEVT_IDLE, wxIdleEventHandler(MyFrame::onFrameIdle));
	m_buttonStart->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickStart), NULL, this);
	m_buttonConnectToTurn->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickConnectTurn), NULL, this);
	m_buttonConnectToPeer->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickConnectPeer), NULL, this);
	m_buttonSendMsg->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickSendMsg), NULL, this);
	m_buttonClearLog->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickClearLog), NULL, this);

	init();
}

MyFrame::~MyFrame()
{
	// Disconnect Events
	this->Disconnect(wxEVT_IDLE, wxIdleEventHandler(MyFrame::onFrameIdle));
	m_buttonStart->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickStart), NULL, this);
	m_buttonConnectToTurn->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickConnectTurn), NULL, this);
	m_buttonConnectToPeer->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickConnectPeer), NULL, this);
	m_buttonSendMsg->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickSendMsg), NULL, this);
	m_buttonClearLog->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::onClickClearLog), NULL, this);

	if (m_peer)
	{
		delete m_peer;
	}
	delete m_timer;
}

void MyFrame::init()
{
	REGISTER_EXCEPTION("p2pClient.dmp");

	m_timer = new wxTimer(this, wxID_TIMER_ID);
	m_timer->Start(200);

	m_peer = NULL;
	g_MyFrame = this;

	m_textCtrlTurnAddr->SetLabelText(P2P_CONNECT_IP);

	net_uv::setNetUVLogPrintFunc(uvLogOutput);
}

void MyFrame::startPeer(const char* turnAddr)
{
	if (m_peer != NULL)
	{
		printLog("Peer已存在");
		return;
	}

	m_peer = new net_uv::P2PPeer();
	m_peer->setStartCallback([=](bool isSuccess)
	{
		m_buttonStart->Enable(true);
		m_buttonStart->SetLabel(isSuccess ? "停止" : "启动");

		this->printLog("Peer启动%s", isSuccess ? "成功" : "失败");
	});

	m_peer->setNewConnectCallback([=](uint64_t key)
	{
		this->printLog("新连接[%llu]进入", key);
		m_allConnectPeer.push_back(key);
	});

	m_peer->setConnectToPeerCallback([=](uint64_t key, int status)
	{
		// 0 找不到目标  1 成功 2 超时
		if (status == 1)
		{
			m_allConnectPeer.push_back(key);
		}
		const char* msg[] = {"连接失败，错误:找不到目标", "连接成功", "连接失败，错误:超时", "连接失败，错误:该节点已经作为客户端连接到本节点"};
		this->printLog("[%llu]%s", key, msg[status]);
	});

	m_peer->setConnectToTurnCallback([=](bool isSuccess, uint64_t selfKey)
	{
		if (isSuccess)
		{
			this->printLog("连接到Turn成功，本机Key:[%llu]", selfKey);
			m_textCtrlSelfKey->SetLabelText(std::to_string(selfKey));
		}
		else
		{
			m_textCtrlSelfKey->SetLabelText("");
			this->printLog("连接到Turn失败");
		}
	});

	m_peer->setDisConnectToTurnCallback([=]()
	{
		m_textCtrlSelfKey->SetLabelText("");
		this->printLog("与Turn断开连接");
	});

	m_peer->setDisConnectToPeerCallback([=](uint64_t key)
	{
		this->printLog("连接[%llu]已断开", key);
		m_allConnectPeer.erase(std::find(m_allConnectPeer.begin(), m_allConnectPeer.end(), key));
	});

	m_peer->setRecvCallback([=](uint64_t key, char* data, uint32_t len)
	{
		std::string recvstr(data, len);
		this->printLog("[%llu]收到数据: %s", key, recvstr.c_str());
	});

	m_peer->setCloseCallback([=]()
	{
		m_buttonStart->Enable(true);
		m_buttonStart->SetLabel("启动");
		this->printLog("Peer关闭");

		auto pPeer = m_peer;
		m_peer = NULL;
		delete pPeer;
		net_uv::printMemInfo();
	});

	m_peer->start(turnAddr, P2P_CONNECT_PORT);
}

void MyFrame::onFrameIdle(wxIdleEvent& event)
{}

void MyFrame::onClickStart(wxCommandEvent& event)
{
	m_buttonStart->Enable(false);
	if (m_peer)
	{
		m_peer->stop();
	}
	else
	{
		std::string turnAddr = m_textCtrlTurnAddr->GetLineText(0).c_str().AsChar();

		if (turnAddr.empty())
		{
			printLog("请输入Turn地址");
			return;
		}
		startPeer(turnAddr.c_str());
	}
}

void MyFrame::onClickConnectTurn(wxCommandEvent& event)
{
	if (m_peer)
	{
		m_peer->restartConnectTurn();
	}
}

void MyFrame::onClickConnectPeer(wxCommandEvent& event)
{
	if (m_peer == NULL)
	{
		return;
	}

	try
	{
		wxString lineText = m_textCtrlOtherKey->GetLineText(0);
		if (lineText.IsEmpty())
			return;

		int64_t key = 0;
		key = std::stoll(lineText.c_str().AsChar());
		m_peer->connectToPeer(key);
	}
	catch (...)
	{
		printLog("Key格式错误");
	}
}

void MyFrame::onClickSendMsg(wxCommandEvent& event)
{
	if (m_peer == NULL)
	{
		return;
	}

	std::string sendContent = m_textCtrlSendMsg->GetLineText(0).c_str().AsChar();
	if (sendContent.empty())
	{
		printLog("发送内容不能为空");
		return;
	}
	
	for (auto &it : m_allConnectPeer)
	{
		m_peer->send(it, (char*)sendContent.c_str(), sendContent.length());
	}
}

void MyFrame::onClickClearLog(wxCommandEvent& event) 
{
	m_textCtrlLog->Clear();
}

void MyFrame::onTimer(wxTimerEvent &event)
{
	m_logLock.lock();

	for (auto& it : m_logStrArr)
	{
		m_textCtrlLog->AppendText(it);
	}
	m_logStrArr.clear();
	m_logLock.unlock();

	if (m_peer)
	{
		m_peer->updateFrame();
	}
}

void MyFrame::printLog(const char* format, ...)
{
	va_list args;
	char buf[2048];

	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	//time_t timep;
	//time(&timep);
	//char tmp[64];
	//strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));

	//std::string str = tmp;
	//str.append("[NET-UV]: ");
	//str.append(buf);
	//str.append("\n");
	//m_textCtrlLog->AppendText(str);

	std::string str = buf;
	str.append("\n");
	

	m_logLock.lock();
	m_logStrArr.push_back(str);
	m_logLock.unlock();
}