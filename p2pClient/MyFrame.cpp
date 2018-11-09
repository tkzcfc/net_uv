///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Oct 26 2018)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "MyFrame.h"

///////////////////////////////////////////////////////////////////////////

MyFrame::MyFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style) : wxFrame(parent, id, title, pos, size, style)
{
	this->SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer(wxHORIZONTAL);

	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer(wxVERTICAL);

	m_staticText2 = new wxStaticText(this, wxID_ANY, wxT("请输入对方Key:"), wxDefaultPosition, wxDefaultSize, 0);
	m_staticText2->Wrap(-1);
	bSizer3->Add(m_staticText2, 0, wxALL, 5);

	m_textCtrlKey = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	bSizer3->Add(m_textCtrlKey, 0, wxALL | wxEXPAND, 5);

	m_buttonConnect = new wxButton(this, wxID_ANY, wxT("连接"), wxDefaultPosition, wxDefaultSize, 0);
	bSizer3->Add(m_buttonConnect, 0, wxALL | wxEXPAND, 5);


	bSizer2->Add(bSizer3, 1, wxEXPAND, 5);

	wxStaticBoxSizer* sbSizer1;
	sbSizer1 = new wxStaticBoxSizer(new wxStaticBox(this, wxID_ANY, wxEmptyString), wxVERTICAL);

	m_textCtrlLog = new wxTextCtrl(sbSizer1->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
	sbSizer1->Add(m_textCtrlLog, 1, wxALL | wxEXPAND, 5);


	bSizer2->Add(sbSizer1, 3, wxEXPAND, 5);


	this->SetSizer(bSizer2);
	this->Layout();

	this->Centre(wxBOTH);
}

MyFrame::~MyFrame()
{
}
