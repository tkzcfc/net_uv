#pragma once

#include "wx/wxprec.h"
#include "wx/app.h"
#include "wx/defs.h"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif


class MyApp : public wxApp
{
public:
	MyApp();

	virtual bool OnInit()override;
	virtual int OnExit()override; 
	void OnIdle(wxIdleEvent& event);
};
