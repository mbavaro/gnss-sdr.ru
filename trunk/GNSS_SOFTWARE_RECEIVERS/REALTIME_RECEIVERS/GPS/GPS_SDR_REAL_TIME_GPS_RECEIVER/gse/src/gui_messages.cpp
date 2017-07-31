/*
 * gui_messages.cpp
 *
 *  Created on: Nov 4, 2008
 *      Author: gheckler
 */

#include "gui.h"

extern wxColor grey;
extern wxColor blue;
extern wxColor white;
extern wxColor yellow;
extern wxColor green;
extern wxColor red;
extern wxColor black;
extern wxColor htext;

wxString mname[LAST_M_ID+1];

DECLARE_APP(GUI_App)

/*----------------------------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(GUI_Messages, wxFrame)
    EVT_CLOSE(GUI_Messages::onClose)
END_EVENT_TABLE()
/*----------------------------------------------------------------------------------------------*/

GUI_Messages::GUI_Messages():iGUI_Messages(NULL, wxID_ANY, wxT("Messages"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL)
{

	int32 k = 0;

	mname[FIRST_M_ID]            = wxT("First Message      ");
	mname[FIRST_PERIODIC_M_ID]   = wxT("First Periodic     ");
	mname[TASK_HEALTH_M_ID]      = wxT("Task Health        ");
	mname[SPS_M_ID]              = wxT("SPS                ");
	mname[TOT_M_ID]              = wxT("Time of Tone       ");
	mname[PPS_M_ID]              = wxT("PPS                ");
	mname[CLOCK_M_ID]            = wxT("Clock              ");
	mname[EKF_STATE_M_ID]        = wxT("EKF State          ");
	mname[EKF_COVARIANCE_M_ID]   = wxT("EKF Covariance     ");
	mname[EKF_RESIDUAL_M_ID]     = wxT("EKF Residual       ");
	mname[CHANNEL_M_ID]          = wxT("Channel            ");
	mname[SV_POSITION_M_ID]      = wxT("SV Position        ");
	mname[MEASUREMENT_M_ID]      = wxT("Measurement        ");
	mname[PSEUDORANGE_M_ID]      = wxT("Pseudorange        ");
	mname[LAST_PERIODIC_M_ID]    = wxT("Last Periodic      ");
	mname[COMMAND_M_ID]          = wxT("Command            ");
	mname[COMMAND_ACK_M_ID]      = wxT("Command Ack        ");
	mname[EPHEMERIS_M_ID]        = wxT("Ephemeris          ");
	mname[ALMANAC_M_ID]          = wxT("Almanac            ");
	mname[EPHEMERIS_STATUS_M_ID] = wxT("Ephemeris Status   ");
	mname[SV_SELECT_STATUS_M_ID] = wxT("SV Select Status   ");
	mname[SV_PREDICTION_M_ID]    = wxT("SV Prediction      ");
	mname[BOARD_HEALTH_M_ID]     = wxT("Board Health       ");
	mname[EEPROM_M_ID]           = wxT("EEPROM             ");
	mname[EEPROM_CHKSUM_M_ID]    = wxT("EEPROM Checksum    ");
	mname[MEMORY_M_ID]           = wxT("Memory             ");
	mname[MEMORY_CHKSUM_M_ID]    = wxT("Memory Checksum    ");
	mname[LAST_M_ID]             = wxT("Last Message       ");

	int32 strwidth;
	wxString str, str2;

	str += mname[FIRST_M_ID];
	str2.Printf(wxT("%8d"),0);
	str += str2;
	str += wxT("\n");
	strwidth = str.Length();
	SetSize(tMess->GetCharWidth()*strwidth+1, tMess->GetCharHeight()*(LAST_M_ID+2)+1);

}

GUI_Messages::~GUI_Messages()
{


}

void GUI_Messages::onClose(wxCloseEvent& evt)
{
	wxCommandEvent cevt;
	evt.Veto();
	pToplevel->onMessages(cevt);
}

void GUI_Messages::paintNow()
{

    wxClientDC dc(this);
    render(dc);

}

void GUI_Messages::render(wxDC& dc)
{
	renderMessages();
}

void GUI_Messages::renderMessages()
{

	wxTextAttr style;
    wxString str, str2;
	int32 lcv, start, stop, lines;
	int32 x, y;

	tMess->Clear();

	for(lcv = 0; lcv < LAST_M_ID+1; lcv++)
	{
		if((lcv+1) & 0x1)
			style.SetBackgroundColour(htext);
		else
			style.SetBackgroundColour(white);

		tMess->SetDefaultStyle(style);

		str.Clear();
		str += mname[lcv];
		str2.Printf(wxT("%8d"),pSerial->packet_count[lcv]);
		str += str2;

		if(lcv < LAST_M_ID)
			str += wxT("\n");

		tMess->AppendText(str);
	}

}
