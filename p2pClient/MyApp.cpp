#include "MyApp.h"
#include "MyFrame.h"


wxIMPLEMENT_APP(MyApp);

MyApp::MyApp()
{}

bool MyApp::OnInit()
{
	if (!wxApp::OnInit())
		return false;
	MyFrame * frame = new MyFrame(0L);
	frame->Show();

	AllocConsole();

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"zxczxcz", 8, NULL, NULL);

	

	printf("-----------------");

	return true;
}

int MyApp::OnExit()
{
	return wxApp::OnExit();
}

void MyApp::OnIdle(wxIdleEvent& event)
{
	wxApp::OnIdle(event);
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), L"zxczxcz", 8, NULL, NULL);
	printf("-----------------");
}
