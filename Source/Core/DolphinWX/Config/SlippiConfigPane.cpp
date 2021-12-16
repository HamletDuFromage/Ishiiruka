// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/SlippiConfigPane.h"

#include <cassert>
#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/filepicker.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/GCPad.h"
#include "Core/NetPlayProto.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Input/MicButtonConfigDiag.h"
#include "DolphinWX/WxEventUtils.h"
#include "DolphinWX/WxUtils.h"
#include <wx/valtext.h>

SlippiNetplayConfigPane::SlippiNetplayConfigPane(wxWindow *parent, wxWindowID id)
    : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void SlippiNetplayConfigPane::InitializeGUI()
{

	// Slippi settings
	m_replay_enable_checkbox = new wxCheckBox(this, wxID_ANY, _("Save Slippi Replays"));
	m_replay_enable_checkbox->SetToolTip(
	    _("Enable this to make Slippi automatically save .slp recordings of your games."));

	m_replay_month_folders_checkbox = new wxCheckBox(this, wxID_ANY, _("Save Replays to Monthly Subfolders"));
	m_replay_month_folders_checkbox->SetToolTip(
	    _("Enable this to save your replays into subfolders by month (YYYY-MM)."));

	m_replay_directory_picker =
	    new wxDirPickerCtrl(this, wxID_ANY, wxEmptyString, _("Slippi Replay Folder:"), wxDefaultPosition, wxDefaultSize,
	                        wxDIRP_USE_TEXTCTRL | wxDIRP_SMALL);
	m_replay_directory_picker->SetToolTip(_("Choose where your Slippi replay files are saved."));

	// Online settings
	m_slippi_delay_frames_txt = new wxStaticText(this, wxID_ANY, _("Delay Frames:"));
	m_slippi_delay_frames_ctrl = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(50, -1));
	m_slippi_delay_frames_ctrl->SetToolTip(
	    _("Leave this at 2 unless consistently playing on 120+ ping. "
	      "Increasing this can cause unplayable input delay, and lowering it can cause visual artifacts/lag."));
	m_slippi_delay_frames_ctrl->SetRange(1, 9);

	m_slippi_enable_quick_chat_txt = new wxStaticText(this, wxID_ANY, _("Quick Chat:"));
	m_slippi_enable_quick_chat_choice =
	    new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_slippi_enable_quick_chat_strings);
	m_slippi_enable_quick_chat_choice->SetToolTip(
	    _("Enable this to send and receive Quick Chat Messages when online."));

	m_slippi_anonymize_opponents_checkbox = new wxCheckBox(this, wxID_ANY, _("Anonymize Opponents Names"));
	m_slippi_anonymize_opponents_checkbox->SetToolTip(
	    _("Replace opponents display names by \'Player [port]\' on Direct and Ranked mode."));

	m_slippi_banlist_button = new wxButton(this, wxID_ANY, _("Character ban list"));

	m_slippi_force_netplay_port_checkbox = new wxCheckBox(this, wxID_ANY, _("Force Netplay Port"));
	m_slippi_force_netplay_port_checkbox->SetToolTip(
	    _("Enable this to force Slippi to use a specific network port for online peer-to-peer connections."));
	m_slippi_force_netplay_port_ctrl =
	    new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(100, -1));
	m_slippi_force_netplay_port_ctrl->SetRange(1, 65535);

	m_slippi_force_netplay_lan_ip_checkbox = new wxCheckBox(this, wxID_ANY, _("Force LAN IP"));
	m_slippi_force_netplay_lan_ip_checkbox->SetToolTip(
	    _("Enable this to force Slippi to use a specific LAN IP when connecting to users with a matching WAN IP. "
	      "Should not be required for most users."));
	m_slippi_netplay_lan_ip_ctrl = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(120, -1));
	m_slippi_netplay_lan_ip_ctrl->SetMaxLength(20);
	wxArrayString charsToFilter;
	wxTextValidator ipTextValidator(wxFILTER_INCLUDE_CHAR_LIST);
	charsToFilter.Add(wxT("0123456789."));
	ipTextValidator.SetIncludes(charsToFilter);
	m_slippi_netplay_lan_ip_ctrl->SetValidator(ipTextValidator);

	// Input settings
	m_reduce_timing_dispersion_checkbox = new wxCheckBox(this, wxID_ANY, _("Reduce Timing Dispersion"));
	m_reduce_timing_dispersion_checkbox->SetToolTip(
	    _("Make inputs feel more console-like for overclocked GCC to USB "
	      "adapters at the cost of 1.6ms of input lag (2ms for single-port official adapter)."));

	const int space5 = FromDIP(5);
	const int space10 = FromDIP(10);

	wxGridBagSizer *const sSlippiReplaySettings = new wxGridBagSizer(space5, space5);
	sSlippiReplaySettings->Add(m_replay_enable_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiReplaySettings->Add(m_replay_month_folders_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2),
	                           wxRESERVE_SPACE_EVEN_IF_HIDDEN);
	sSlippiReplaySettings->Add(new wxStaticText(this, wxID_ANY, _("Replay folder:")), wxGBPosition(2, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiReplaySettings->Add(m_replay_directory_picker, wxGBPosition(2, 1), wxDefaultSpan, wxEXPAND);
	sSlippiReplaySettings->AddGrowableCol(1);

	wxStaticBoxSizer *const sbSlippiReplaySettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Replay Settings"));
	sbSlippiReplaySettings->AddSpacer(space5);
	sbSlippiReplaySettings->Add(sSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiReplaySettings->AddSpacer(space5);

	wxGridBagSizer *const sSlippiOnlineSettings = new wxGridBagSizer(space10, space5);
	sSlippiOnlineSettings->Add(m_slippi_delay_frames_txt, wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_delay_frames_ctrl, wxGBPosition(0, 1), wxDefaultSpan, wxALIGN_LEFT);

	sSlippiOnlineSettings->Add(m_slippi_enable_quick_chat_txt, wxGBPosition(1, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_enable_quick_chat_choice, wxGBPosition(1, 1), wxDefaultSpan, wxALIGN_LEFT);

	sSlippiOnlineSettings->Add(m_slippi_anonymize_opponents_checkbox, wxGBPosition(2, 0), wxGBSpan(1, 2));

	sSlippiOnlineSettings->Add(m_slippi_banlist_button, wxGBPosition(3, 0), wxGBSpan(1, 2));

	sSlippiOnlineSettings->Add(m_slippi_force_netplay_port_checkbox, wxGBPosition(4, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_force_netplay_port_ctrl, wxGBPosition(4, 1), wxDefaultSpan,
	                           wxALIGN_LEFT | wxRESERVE_SPACE_EVEN_IF_HIDDEN);
	sSlippiOnlineSettings->Add(m_slippi_force_netplay_lan_ip_checkbox, wxGBPosition(5, 0), wxDefaultSpan,
	                           wxALIGN_CENTER_VERTICAL);
	sSlippiOnlineSettings->Add(m_slippi_netplay_lan_ip_ctrl, wxGBPosition(5, 1), wxDefaultSpan,
	                           wxALIGN_LEFT | wxRESERVE_SPACE_EVEN_IF_HIDDEN);

	wxStaticBoxSizer *const sbSlippiOnlineSettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Online Settings"));
	sbSlippiOnlineSettings->AddSpacer(space5);
	sbSlippiOnlineSettings->Add(sSlippiOnlineSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiOnlineSettings->AddSpacer(space5);

	wxStaticBoxSizer *const sbSlippiInputSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Input Settings"));
	sbSlippiInputSettings->AddSpacer(space5);
	sbSlippiInputSettings->Add(m_reduce_timing_dispersion_checkbox, 0, wxLEFT | wxRIGHT, space5);
	sbSlippiInputSettings->AddSpacer(space5);

	wxBoxSizer *const main_sizer = new wxBoxSizer(wxVERTICAL);

	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiOnlineSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiInputSettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void SlippiNetplayConfigPane::LoadGUIValues()
{
	const SConfig &startup_params = SConfig::GetInstance();

#if HAVE_PORTAUDIO
#endif

	bool enableReplays = startup_params.m_slippiSaveReplays;
	bool forceNetplayPort = startup_params.m_slippiForceNetplayPort;
	bool forceLanIp = startup_params.m_slippiForceLanIp;

	m_replay_enable_checkbox->SetValue(enableReplays);
	m_replay_month_folders_checkbox->SetValue(startup_params.m_slippiReplayMonthFolders);
	m_replay_directory_picker->SetPath(StrToWxStr(startup_params.m_strSlippiReplayDir));

	if (!enableReplays)
	{
		m_replay_month_folders_checkbox->Hide();
	}

	m_slippi_delay_frames_ctrl->SetValue(startup_params.m_slippiOnlineDelay);
	m_slippi_anonymize_opponents_checkbox->SetValue(startup_params.m_slippiAnonymizeOpponents);
	PopulateEnableChatChoiceBox();

	m_slippi_force_netplay_port_checkbox->SetValue(startup_params.m_slippiForceNetplayPort);
	m_slippi_force_netplay_port_ctrl->SetValue(startup_params.m_slippiNetplayPort);
	if (!forceNetplayPort)
	{
		m_slippi_force_netplay_port_ctrl->Hide();
	}

	m_slippi_force_netplay_lan_ip_checkbox->SetValue(startup_params.m_slippiForceLanIp);
	m_slippi_netplay_lan_ip_ctrl->SetValue(startup_params.m_slippiLanIp);
	if (!forceLanIp)
	{
		m_slippi_netplay_lan_ip_ctrl->Hide();
	}

	m_reduce_timing_dispersion_checkbox->SetValue(startup_params.bReduceTimingDispersion);
}

void SlippiNetplayConfigPane::BindEvents()
{
	m_replay_enable_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnReplaySavingToggle, this);

	m_replay_month_folders_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnReplayMonthFoldersToggle, this);

	m_replay_directory_picker->Bind(wxEVT_DIRPICKER_CHANGED, &SlippiNetplayConfigPane::OnReplayDirChanged, this);

	m_slippi_delay_frames_ctrl->Bind(wxEVT_SPINCTRL, &SlippiNetplayConfigPane::OnDelayFramesChanged, this);
	m_slippi_enable_quick_chat_choice->Bind(wxEVT_CHOICE, &SlippiNetplayConfigPane::OnQuickChatChanged, this);
	m_slippi_anonymize_opponents_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnAnonymizeOpponentsChanged, this);
	m_slippi_banlist_button->Bind(wxEVT_BUTTON, &SlippiNetplayConfigPane::OnBanlistClick, this);
	m_slippi_force_netplay_port_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnForceNetplayPortToggle,
	                                           this);
	m_slippi_force_netplay_port_ctrl->Bind(wxEVT_SPINCTRL, &SlippiNetplayConfigPane::OnNetplayPortChanged, this);

	m_slippi_force_netplay_lan_ip_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnForceNetplayLanIpToggle,
	                                             this);
	m_slippi_netplay_lan_ip_ctrl->Bind(wxEVT_TEXT, &SlippiNetplayConfigPane::OnNetplayLanIpChanged, this);

	m_reduce_timing_dispersion_checkbox->Bind(wxEVT_CHECKBOX, &SlippiNetplayConfigPane::OnReduceTimingDispersionToggle,
	                                          this);
}

void SlippiNetplayConfigPane::OnQuickChatChanged(wxCommandEvent &event)
{
	auto selectedStr = m_slippi_enable_quick_chat_choice->GetSelection() != wxNOT_FOUND
	                       ? WxStrToStr(m_slippi_enable_quick_chat_choice->GetStringSelection())
	                       : quickChatOptions[SLIPPI_CHAT_ON];

	int selectedChoice = SLIPPI_CHAT_ON; // default is enabled

	for (auto it = quickChatOptions.begin(); it != quickChatOptions.end(); it++)
		if (strcmp(it->second.c_str(), selectedStr.c_str()) == 0)
		{
			selectedChoice = it->first;
			break;
		}

	SConfig::GetInstance().m_slippiEnableQuickChat = selectedChoice;
}

void SlippiNetplayConfigPane::OnReplaySavingToggle(wxCommandEvent &event)
{
	bool enableReplays = m_replay_enable_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiSaveReplays = enableReplays;

	if (enableReplays)
	{
		m_replay_month_folders_checkbox->Show();
	}
	else
	{
		m_replay_month_folders_checkbox->SetValue(false);
		m_replay_month_folders_checkbox->Hide();
		SConfig::GetInstance().m_slippiReplayMonthFolders = false;
	}
}

void SlippiNetplayConfigPane::OnReplayMonthFoldersToggle(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiReplayMonthFolders =
	    m_replay_enable_checkbox->IsChecked() && m_replay_month_folders_checkbox->IsChecked();
}

void SlippiNetplayConfigPane::OnReplayDirChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_strSlippiReplayDir = WxStrToStr(m_replay_directory_picker->GetPath());
}

void SlippiNetplayConfigPane::OnDelayFramesChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiOnlineDelay = m_slippi_delay_frames_ctrl->GetValue();
}

void SlippiNetplayConfigPane::OnAnonymizeOpponentsChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiAnonymizeOpponents = m_slippi_anonymize_opponents_checkbox->IsChecked();
}

void SlippiNetplayConfigPane::OnBanlistClick(wxCommandEvent &event)
{
	std::map<u8, std::string> characters = {
		{0, "Captain Falcon"},
		{1, "Donkey Kong"},
		{2, "Fox"},
		{3, "Game & Watch"},
		{4, "Kirby"},
		{5, "Bowser"},
		{6, "Link"},
		{7, "Luigi"},
		{8, "Mario"},
		{9, "Marth"},
		{10, "Mewtwo"},
		{11, "Ness"},
		{12, "Peach"},
		{13, "Pikachu"},
		{14, "Ice Climbers"},
		{15, "Jigglypuff"},
		{16, "Samus"},
		{17, "Yoshi"},
		{18, "Zelda"},
		{19, "Sheik"},
		{20, "Falco"},
		{21, "Young Link"},
		{22, "Dr. Mario"},
		{23, "Roy"},
		{24, "Pichu"},
		{25, "Ganondorf"}
	};

	const int space1 = FromDIP(1);
	wxDialog banlistDialog{ this, wxID_ANY, _("Ban characters"), wxDefaultPosition, wxSize(400, 700), wxDD_DEFAULT_STYLE};	
	wxStaticText* m_banlist_desc = new wxStaticText(&banlistDialog, wxID_ANY, _("Select characters you don't want to encounter online"));
	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer* const padding_szr = new wxBoxSizer(wxVERTICAL);

	szr->Add(m_banlist_desc, 0, wxEXPAND);
	szr->AddSpacer(space1);

	u32 banlist = SConfig::GetInstance().m_slippiBanlist;
	for (u8 i = 0; i < 26; i++) {
		wxCheckBox* character = new wxCheckBox(&banlistDialog, wxID_ANY, _(characters[i]));
		character->SetValue((banlist & (1 << i)));
		character->Bind(wxEVT_CHECKBOX, [i, character, banlist](wxCommandEvent&) {
			if (character->IsChecked())
				SConfig::GetInstance().m_slippiBanlist |= 1UL << i;
			else
				SConfig::GetInstance().m_slippiBanlist &= ~(1UL << i);
		});
		szr->Add(character, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND);
		szr->AddSpacer(space1);
	}
	szr->Add(banlistDialog.CreateButtonSizer(wxCLOSE | wxNO_DEFAULT), 0, wxEXPAND | wxBOTTOM, space1);
	szr->AddSpacer(space1);
	padding_szr->Add(szr, 1, wxALL, 12);
	SetSizerAndFit(padding_szr, false);
	Center();

	banlistDialog.ShowModal();
}

void SlippiNetplayConfigPane::OnForceNetplayPortToggle(wxCommandEvent &event)
{
	bool enableForcePort = m_slippi_force_netplay_port_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiForceNetplayPort = enableForcePort;

	if (enableForcePort)
		m_slippi_force_netplay_port_ctrl->Show();
	else
		m_slippi_force_netplay_port_ctrl->Hide();
}

void SlippiNetplayConfigPane::OnNetplayPortChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiNetplayPort = m_slippi_force_netplay_port_ctrl->GetValue();
}

void SlippiNetplayConfigPane::OnForceNetplayLanIpToggle(wxCommandEvent &event)
{
	bool enableForceLanIp = m_slippi_force_netplay_lan_ip_checkbox->IsChecked();

	SConfig::GetInstance().m_slippiForceLanIp = enableForceLanIp;

	if (enableForceLanIp)
		m_slippi_netplay_lan_ip_ctrl->Show();
	else
		m_slippi_netplay_lan_ip_ctrl->Hide();
}

void SlippiNetplayConfigPane::OnNetplayLanIpChanged(wxCommandEvent &event)
{
	SConfig::GetInstance().m_slippiLanIp = m_slippi_netplay_lan_ip_ctrl->GetValue().c_str();
}

void SlippiNetplayConfigPane::OnReduceTimingDispersionToggle(wxCommandEvent &event)
{
	SConfig::GetInstance().bReduceTimingDispersion = m_reduce_timing_dispersion_checkbox->GetValue();
}

void SlippiNetplayConfigPane::PopulateEnableChatChoiceBox()
{

	for (auto it = quickChatOptions.begin(); it != quickChatOptions.end(); it++)
	{
		m_slippi_enable_quick_chat_choice->Append(StrToWxStr(it->second));
	}

	auto currentChoice = SConfig::GetInstance().m_slippiEnableQuickChat;
	auto currentChoiceStr = quickChatOptions[currentChoice];
	int num = m_slippi_enable_quick_chat_choice->FindString(StrToWxStr(currentChoiceStr));
	m_slippi_enable_quick_chat_choice->SetSelection(num);
}

SlippiPlaybackConfigPane::SlippiPlaybackConfigPane(wxWindow *parent, wxWindowID id)
    : wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	BindEvents();
}

void SlippiPlaybackConfigPane::InitializeGUI()
{
	// Slippi Replay settings
	m_display_frame_index = new wxCheckBox(this, wxID_ANY, _("Display Frame Index"));
	m_display_frame_index->SetToolTip(
	    _("Displays the Frame Index when viewing replays. On-Screen Display Messages must also be enabled"));

	const int space5 = FromDIP(5);

	wxGridBagSizer *const sSlippiReplaySettings = new wxGridBagSizer(space5, space5);
	sSlippiReplaySettings->Add(m_display_frame_index, wxGBPosition(0, 0), wxGBSpan(1, 2));
	sSlippiReplaySettings->AddGrowableCol(1);

	wxStaticBoxSizer *const sbSlippiReplaySettings =
	    new wxStaticBoxSizer(wxVERTICAL, this, _("Slippi Replay Settings"));
	sbSlippiReplaySettings->AddSpacer(space5);
	sbSlippiReplaySettings->Add(sSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	sbSlippiReplaySettings->AddSpacer(space5);

	wxBoxSizer *const main_sizer = new wxBoxSizer(wxVERTICAL);

	main_sizer->AddSpacer(space5);
	main_sizer->Add(sbSlippiReplaySettings, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
	main_sizer->AddSpacer(space5);

	SetSizer(main_sizer);
}

void SlippiPlaybackConfigPane::LoadGUIValues()
{
	const SConfig &startup_params = SConfig::GetInstance();

	bool enableFrameIndex = startup_params.m_slippiEnableFrameIndex;

	m_display_frame_index->SetValue(enableFrameIndex);
}

void SlippiPlaybackConfigPane::BindEvents()
{
	m_display_frame_index->Bind(wxEVT_CHECKBOX, &SlippiPlaybackConfigPane::OnDisplayFrameIndexToggle, this);
}

void SlippiPlaybackConfigPane::OnDisplayFrameIndexToggle(wxCommandEvent &event)
{
	bool enableFrameIndex = m_display_frame_index->IsChecked();
	SConfig::GetInstance().m_slippiEnableFrameIndex = enableFrameIndex;
}
