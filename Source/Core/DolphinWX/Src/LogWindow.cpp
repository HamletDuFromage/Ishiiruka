// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


#include <wx/wx.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/listbox.h>
#include <wx/checklst.h>

#include "Core.h" // for Core::GetState()
#include "LogWindow.h"
#include "Console.h"

// milliseconds between msgQueue flushes to wxTextCtrl
#define UPDATETIME 100

BEGIN_EVENT_TABLE(CLogWindow, wxDialog)
	EVT_TEXT_ENTER(IDM_SUBMITCMD, CLogWindow::OnSubmit)
	EVT_BUTTON(IDM_CLEARLOG, CLogWindow::OnClear)
	EVT_BUTTON(IDM_TOGGLEALL, CLogWindow::OnToggleAll)
	EVT_RADIOBOX(IDM_VERBOSITY, CLogWindow::OnOptionsCheck)
	EVT_CHECKBOX(IDM_WRITEFILE, CLogWindow::OnOptionsCheck)
	EVT_CHECKBOX(IDM_WRITECONSOLE, CLogWindow::OnOptionsCheck)
	EVT_CHECKLISTBOX(IDM_LOGCHECKS, CLogWindow::OnLogCheck)
	EVT_TIMER(IDTM_UPDATELOG, CLogWindow::OnLogTimer)
END_EVENT_TABLE()

CLogWindow::CLogWindow(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, wxT("Log/Console"), 
			   wxPoint(100, 700), wxSize(800, 270),
			   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), 
	Listener("LogWindow")
{
	m_logManager = LogManager::GetInstance();
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		m_logManager->addListener((LogTypes::LOG_TYPE)i, this);
	m_fileLog = m_logManager->getFileListener();
	m_console = m_logManager->getConsoleListener();

	CreateGUIControls();

	LoadSettings();
}

void CLogWindow::CreateGUIControls()
{
	wxBoxSizer* sUber = new wxBoxSizer(wxHORIZONTAL),	// whole plane
		* sLeft = new wxBoxSizer(wxVERTICAL),			// LEFT sizer
		* sRight = new wxBoxSizer(wxVERTICAL),			// RIGHT sizer
		* sRightBottom = new wxBoxSizer(wxHORIZONTAL);	// submit row

	// Left side: buttons (-submit), options, and log type selection
	wxStaticBoxSizer* sbLeftOptions = new wxStaticBoxSizer(wxVERTICAL, this, wxT("Options"));

	wxArrayString wxLevels;
	for (int i = 0; i < LOGLEVEL; ++i)
		wxLevels.Add(wxString::Format(wxT("%i"), i));
	m_verbosity = new wxRadioBox(this, IDM_VERBOSITY, wxT("Verbosity"), wxDefaultPosition, wxDefaultSize, wxLevels, 0, wxRA_SPECIFY_COLS, wxDefaultValidator);
	sbLeftOptions->Add(m_verbosity);

	m_writeFileCB = new wxCheckBox(this, IDM_WRITEFILE, wxT("Write to File"), wxDefaultPosition, wxDefaultSize, 0);
	sbLeftOptions->Add(m_writeFileCB);

	m_writeConsoleCB = new wxCheckBox(this, IDM_WRITECONSOLE, wxT("Write to Console"), wxDefaultPosition, wxDefaultSize, 0);
	sbLeftOptions->Add(m_writeConsoleCB);

	sLeft->Add(sbLeftOptions, 0, wxEXPAND);

	wxBoxSizer* sLogCtrl = new wxBoxSizer(wxHORIZONTAL);
	sLogCtrl->Add(new wxButton(this, IDM_TOGGLEALL, wxT("Toggle all"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 1);
	sLogCtrl->Add(new wxButton(this, IDM_CLEARLOG, wxT("Clear"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 1);
	sLeft->Add(sLogCtrl, 0, wxEXPAND);

	m_checks = new wxCheckListBox(this, IDM_LOGCHECKS, wxDefaultPosition, wxDefaultSize);
	sLeft->Add(m_checks, 1, wxEXPAND);

	// Right side: Log viewer and submit row
	m_log = new wxTextCtrl(this, IDM_LOG, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_RICH | wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	//m_log->SetFont(DebuggerFont);

	m_cmdline = new wxTextCtrl(this, IDM_SUBMITCMD, wxEmptyString, wxDefaultPosition, wxDefaultSize,
		wxTE_PROCESS_ENTER | wxTE_PROCESS_TAB);
	//m_cmdline->SetFont(DebuggerFont);

	sRightBottom->Add(m_cmdline, 1, wxEXPAND);
	sRight->Add(m_log, 1, wxEXPAND | wxSHRINK);
	sRight->Add(sRightBottom, 0, wxEXPAND);

	// Take care of the main sizer and some settings
	sUber->Add(sLeft, 0, wxEXPAND);
	sUber->Add(sRight, 1, wxEXPAND);

	SetSizer(sUber);
	SetAffirmativeId(IDM_SUBMITCMD);
	UpdateChecks();
	m_cmdline->SetFocus();

	m_logTimer = new wxTimer(this, IDTM_UPDATELOG);
	m_logTimer->Start(UPDATETIME);
}

CLogWindow::~CLogWindow()
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, this);
	}
	m_logTimer->Stop();
	delete m_logTimer;

	SaveSettings();
}

void CLogWindow::SaveSettings()
{
	IniFile ini;
	ini.Set("LogWindow", "x", GetPosition().x);
	ini.Set("LogWindow", "y", GetPosition().y);
	ini.Set("LogWindow", "w", GetSize().GetWidth());
	ini.Set("LogWindow", "h", GetSize().GetHeight());
	ini.Set("Options", "Verbosity", m_verbosity->GetSelection());
	ini.Set("Options", "WriteToFile", m_writeFile);
	ini.Set("Options", "WriteToConsole", m_writeConsole);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		ini.Set("Logs", m_logManager->getShortName((LogTypes::LOG_TYPE)i), m_checks->IsChecked(i));
	ini.Save(LOGGER_CONFIG_FILE);
}

void CLogWindow::LoadSettings()
{
	IniFile ini;
	ini.Load(LOGGER_CONFIG_FILE);
	int x,y,w,h,verbosity;
	ini.Get("LogWindow", "x", &x, GetPosition().x);
	ini.Get("LogWindow", "y", &y, GetPosition().y);
	ini.Get("LogWindow", "w", &w, GetSize().GetWidth());
	ini.Get("LogWindow", "h", &h, GetSize().GetHeight());
	SetSize(x, y, w, h);
	ini.Get("Options", "Verbosity", &verbosity, 2);
	m_verbosity->SetSelection(verbosity);
	ini.Get("Options", "WriteToFile", &m_writeFile, true);
	m_writeFileCB->SetValue(m_writeFile);
	ini.Get("Options", "WriteToConsole", &m_writeConsole, true);
	m_writeConsoleCB->SetValue(m_writeConsole);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		bool enable;
		ini.Get("Logs", m_logManager->getShortName((LogTypes::LOG_TYPE)i), &enable, true);

		if (enable)
			m_logManager->addListener((LogTypes::LOG_TYPE)i, this);
		else
			m_logManager->removeListener((LogTypes::LOG_TYPE)i, this);

		if (m_writeFile && enable)
			m_logManager->addListener((LogTypes::LOG_TYPE)i, m_fileLog);
		else
			m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);

		if (m_writeConsole && enable)
			m_logManager->addListener((LogTypes::LOG_TYPE)i, m_console);
		else
			m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_console);
	}
	UpdateChecks();
}

void CLogWindow::OnSubmit(wxCommandEvent& WXUNUSED (event))
{
	Console_Submit(m_cmdline->GetValue().To8BitData());
	m_cmdline->SetValue(wxEmptyString);
	NotifyUpdate();
}

void CLogWindow::OnClear(wxCommandEvent& WXUNUSED (event))
{
	m_log->Clear();

	//msgQueue.Clear()
	for (unsigned int i = 0; i < msgQueue.size(); i++) 
		msgQueue.pop();

	m_console->ClearScreen();
	NOTICE_LOG(CONSOLE, "Console cleared");
	NotifyUpdate();
}

// Enable or disable all boxes for the current verbosity level and save the changes.
void CLogWindow::OnToggleAll(wxCommandEvent& WXUNUSED (event))
{
	static bool enable = false;

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_checks->Check(i, enable);
		m_logManager->setEnable((LogTypes::LOG_TYPE)i, enable);

		if (enable)
		{
			m_logManager->addListener((LogTypes::LOG_TYPE)i, this);

			if (m_writeFile)
				m_logManager->addListener((LogTypes::LOG_TYPE)i, m_fileLog);
			if (m_writeConsole)
				m_logManager->addListener((LogTypes::LOG_TYPE)i, m_console);
		}
		else
		{
			m_logManager->removeListener((LogTypes::LOG_TYPE)i, this);
			m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);
			m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_console);
		}
	}

	enable = !enable;
	SaveSettings();
}

// Append checkboxes and update checked groups. 
void CLogWindow::UpdateChecks()
{
	// This is only run once to append checkboxes to the wxCheckListBox.
	if (m_checks->GetCount() == 0)
	{
		// [F|RES] hide the window while we fill it... wxwidgets gets trouble
		// if you don't do it (at least the win version)
		m_checks->Show(false);

		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		{
			m_checks->Append(wxString::FromAscii(m_logManager->getFullName( (LogTypes::LOG_TYPE)i )));
		}
		m_checks->Show(true);
	}

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		m_checks->Check(i, m_logManager->isListener((LogTypes::LOG_TYPE)i, this));
	}
}

// When an option is changed, save the change
void CLogWindow::OnOptionsCheck(wxCommandEvent& event)
{
 	switch (event.GetId())
	{
	case IDM_VERBOSITY:
		{
		// get selection
		int v = m_verbosity->GetSelection();

		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		{
			m_logManager->setLogLevel((LogTypes::LOG_TYPE)i, (LogTypes::LOG_LEVELS)v);
		}
		}
		break;

	case IDM_WRITEFILE:
		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		{
			m_writeFile = event.IsChecked();
			if (m_checks->IsChecked(i))
			{
				if (m_writeFile)
					m_logManager->addListener((LogTypes::LOG_TYPE)i, m_fileLog);
				else
					m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);
			}
		}
 		break;

	case IDM_WRITECONSOLE:
		for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		{
			m_writeConsole = event.IsChecked();
			if (m_checks->IsChecked(i))
			{
				if (m_writeConsole)
					m_logManager->addListener((LogTypes::LOG_TYPE)i, m_console);
				else
					m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_console);
			}
		}
		break;
	}
	SaveSettings();
}

// When a checkbox is changed
void CLogWindow::OnLogCheck(wxCommandEvent& event)
{
	int i = event.GetInt();
	if (m_checks->IsChecked(i))
	{
		m_logManager->addListener((LogTypes::LOG_TYPE)i, this);

		if (m_writeFile)
			m_logManager->addListener((LogTypes::LOG_TYPE)i, m_fileLog);
		if (m_writeConsole)
			m_logManager->addListener((LogTypes::LOG_TYPE)i, m_console);
	}
	else
	{
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, this);
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_fileLog);
		m_logManager->removeListener((LogTypes::LOG_TYPE)i, m_console);
	}
	SaveSettings();
}

void CLogWindow::OnLogTimer(wxTimerEvent& WXUNUSED(event))
{
	UpdateLog();
}

void CLogWindow::NotifyUpdate()
{
	UpdateChecks();
	UpdateLog();
}

void CLogWindow::UpdateLog()
{
	m_logTimer->Stop();
	for (unsigned int i = 0; i < msgQueue.size(); i++)
	{
		switch (msgQueue.front().first)
		{
			// AGGH why is this not working
		case ERROR_LEVEL: // red
			m_log->SetDefaultStyle(wxTextAttr(wxColour(255, 0, 0), wxColour(0, 0, 0)));
			break;
		case WARNING_LEVEL: // yellow
			m_log->SetDefaultStyle(wxTextAttr(wxColour(255, 255, 0), wxColour(0, 0, 0)));
			break;
		case NOTICE_LEVEL: // green
			m_log->SetDefaultStyle(wxTextAttr(wxColour(0, 255, 0), wxColour(0, 0, 0)));
			break;
		case INFO_LEVEL: // cyan
			m_log->SetDefaultStyle(wxTextAttr(wxColour(0, 255, 255), wxColour(0, 0, 0)));
			break;
		case DEBUG_LEVEL: // light gray
			m_log->SetDefaultStyle(wxTextAttr(wxColour(211, 211, 211), wxColour(0, 0, 0)));
			break;
		default: // white
			m_log->SetDefaultStyle(wxTextAttr(wxColour(255, 255, 255), wxColour(0, 0, 0)));
			break;
		}
		m_log->AppendText(msgQueue.front().second);
		msgQueue.pop();
	}
	m_logTimer->Start(UPDATETIME);
}

void CLogWindow::Log(LogTypes::LOG_LEVELS level, const char *text) 
{
	if (level > NOTICE_LEVEL)
		return;

	if (msgQueue.size() >= 100)
		msgQueue.pop();
	msgQueue.push(std::pair<u8, wxString>((u8)level, wxString::FromAscii(text)));
}
