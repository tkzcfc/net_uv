///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Oct 26 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include "net_uv/net_uv.h"

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/gdicmn.h>
#include <wx/button.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/frame.h>
#include <wx/timer.h>

///////////////////////////////////////////////////////////////////////////

#define wxID_START_PEER 1000
#define wxID_CONNECT_TO_TURN 1001
#define wxID_CONNECT_TO_PEER 1002
#define wxID_CLEAR_LOG 1003
#define wxID_TIMER_ID 1004

///////////////////////////////////////////////////////////////////////////////
/// Class MyFrame
///////////////////////////////////////////////////////////////////////////////
class MyFrame : public wxFrame
{
private:

protected:
	wxStaticText* m_staticText4;
	wxTextCtrl* m_textCtrlTurnAddr;
	wxButton* m_buttonStart;
	wxButton* m_buttonConnectToTurn;
	wxStaticLine* m_staticline1;
	wxStaticText* m_staticText1;
	wxTextCtrl* m_textCtrlSelfKey;
	wxStaticLine* m_staticline2;
	wxStaticText* m_staticText2;
	wxTextCtrl* m_textCtrlOtherKey;
	wxButton* m_buttonConnectToPeer;
	wxStaticLine* m_staticline3;
	wxTextCtrl* m_textCtrlSendMsg;
	wxButton* m_buttonSendMsg;
	wxStaticLine* m_staticline4;
	wxButton* m_buttonClearLog;
	wxTextCtrl* m_textCtrlLog;

	// Virtual event handlers, overide them in your derived class
	virtual void onFrameIdle(wxIdleEvent& event);
	virtual void onClickStart(wxCommandEvent& event);
	virtual void onClickConnectTurn(wxCommandEvent& event);
	virtual void onClickConnectPeer(wxCommandEvent& event);
	virtual void onClickSendMsg(wxCommandEvent& event);
	virtual void onClickClearLog(wxCommandEvent& event);

	void onTimer(wxTimerEvent &event);

private:

	DECLARE_EVENT_TABLE()

public:

	void printLog(const char* format, ...);

	void init();

	void startPeer(const char* turnAddr);

public:

	MyFrame(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize(960, 720), long style = wxDEFAULT_FRAME_STYLE | wxTAB_TRAVERSAL);

	~MyFrame();

protected:
	wxTimer* m_timer;
	net_uv::P2PPeer* m_peer;
	std::vector<uint64_t> m_allConnectPeer;

	net_uv::Mutex m_logLock;
	std::vector<std::string> m_logStrArr;
};
