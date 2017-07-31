/*
 * gui_toplevel.cpp
 *
 *  Created on: Nov 4, 2008
 *      Author: gheckler
 */

#include "gui.h"

extern wxColor red;
extern wxColor green;
extern wxColor blue;
extern wxColor yellow;
extern wxColor white;
extern wxColor black;
extern wxColor grey;
extern wxColor htext;

DECLARE_APP(GUI_App)

/*----------------------------------------------------------------------------------------------*/
BEGIN_EVENT_TABLE(GUI_Ephemeris, wxFrame)
    EVT_CLOSE(GUI_Ephemeris::onClose)
    EVT_LEFT_DOWN(GUI_Ephemeris::onMouse)
    EVT_BUTTON(ID_EPHEMERIS_SAVE, GUI_Ephemeris::onSave)
    EVT_BUTTON(ID_EPHEMERIS_LOAD, GUI_Ephemeris::onLoad)
    EVT_BUTTON(ID_EPHEMERIS_REFRESH, GUI_Ephemeris::onRefresh)
END_EVENT_TABLE()
/*----------------------------------------------------------------------------------------------*/



GUI_Ephemeris::GUI_Ephemeris():iGUI_Ephemeris(NULL, wxID_ANY, wxT("Ephemeris"), wxDefaultPosition, wxSize(400,600), wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL)
{
	sv = 0;
	loaded = 0;
}

GUI_Ephemeris::~GUI_Ephemeris()
{


}

void GUI_Ephemeris::onClose(wxCloseEvent& evt)
{
	wxCommandEvent cevt;
	evt.Veto();
	pToplevel->onEphemeris(cevt);
}

void GUI_Ephemeris::paintNow()
{
    wxClientDC dc(this);
    render(dc);
}

void GUI_Ephemeris::render(wxDC& dc)
{
	int32 val;

	renderDecoded();
	renderSV();

	if(loaded == 0)
	{
		pSerial->formCommand(GET_EPHEMERIS_STATUS_C_ID, &loaded);
		loaded = 1;
	}
}


void GUI_Ephemeris::onMouse(wxMouseEvent& event)
{

	wxString str;
	wxPoint br, tl, sp, mp;
	wxCoord w, h;

	int32 row, col;
	int32 dX,dY;

	pDecoded->GetClientSize(&w, &h);
	sp = this->GetScreenPosition();
	tl = pDecoded->GetScreenPosition() - sp;
	mp = wxGetMousePosition() - sp;

	dY = (h/4);
	dX = (w/(MAX_SV>>2));

	br.x = tl.x + w;
	br.y = tl.y + h;

	/* Within the pDecoded area */
	if((mp.x >= tl.x) && (mp.x <= br.x) && (mp.y >= tl.y) && (mp.y <= br.y))
	{
		mp -= tl;

		row = mp.y/dY;
		col = mp.x/dX;
		sv = row*(MAX_SV>>2);
		sv += col;

		if(sv >= 0 && sv <= MAX_SV)
			pSerial->formCommand(GET_EPHEMERIS_C_ID, &sv);
		else
			sv = 0;
	}
}


void GUI_Ephemeris::renderDecoded()
{

	int32 dX, dY, lcv;
	wxPoint box[4];
	wxCoord w, h;
	wxString str;

	wxBufferedPaintDC dc(pDecoded, wxBUFFER_CLIENT_AREA);
	dc.Clear();

	pDecoded->GetClientSize(&w, &h);

	dY = (h/4);
	dX = (w/(MAX_SV>>2));

	box[0].x = 0;	box[0].y = 0;
	box[1].x = dX;	box[1].y = 0;
	box[2].x = dX;	box[2].y = dY;
	box[3].x = 0;	box[3].y = dY;

	dc.SetFont(wxFont(12, wxDEFAULT, wxNORMAL, wxNORMAL));

	/* Render the decoded ephemerides */
	for(lcv = 0; lcv < MAX_SV; lcv++)
	{
		if(p->ephemeris_status.valid[lcv])
			dc.SetBrush(wxBrush(green));
		else
			dc.SetBrush(wxBrush(grey));

		dc.SetPen(wxPen(black, 1));
		dc.DrawPolygon(4, box, (lcv%(MAX_SV>>2))*dX, (lcv/(MAX_SV>>2))*dY);

		str.Printf(wxT("%02d"),(int)lcv+1);
		dc.DrawText(str, (lcv%(MAX_SV>>2))*dX + dX/3, (lcv/(MAX_SV>>2))*dY + dY/5);
	}

	/* Draw highlighted box */
	if(p->ephemeris_status.valid[sv])
		dc.SetBrush(wxBrush(green));
	else
		dc.SetBrush(wxBrush(grey));

	dc.SetPen(wxPen(red, 1));
	dc.DrawPolygon(4, box, (sv%(MAX_SV>>2))*dX, (sv/(MAX_SV>>2))*dY);

	str.Printf(wxT("%02d"),(int)sv+1);
	dc.DrawText(str, (sv%(MAX_SV>>2))*dX + dX/3, (sv/(MAX_SV>>2))*dY + dY/5);

}

void GUI_Ephemeris::renderSV()
{

	wxString str;
	tDisplay->Clear();

	Ephemeris_M *e;
	e = &p->ephemerides[sv];
	str.Printf(wxT("************* Ephemeris for PRN-%02d *************\n"),sv+1);
	tDisplay->AppendText(str);
	str.Printf(wxT("ID:                        %02d\n"),sv+1);
	tDisplay->AppendText(str);
	str.Printf(wxT("Health:                    %03d\n"),e->subframe_1_health);
	tDisplay->AppendText(str);
	str.Printf(wxT("Eccentricity:              %.10g\n"),e->ecc);
	tDisplay->AppendText(str);
	str.Printf(wxT("Time of Ephemeris(s):      %.10g\n"),e->toe);
	tDisplay->AppendText(str);
	str.Printf(wxT("Orbital Inclination(rad):  %.10g\n"),e->in0);
	tDisplay->AppendText(str);
	str.Printf(wxT("Inclination Rate(r/s):     %.10g\n"),e->idot);
	tDisplay->AppendText(str);
	str.Printf(wxT("Rate of Right Ascen(r/s):  %.10g\n"),e->omd);
	tDisplay->AppendText(str);
	str.Printf(wxT("SQRT(A)  (m 1/2):          %.10g\n"),e->sqrta);
	tDisplay->AppendText(str);
	str.Printf(wxT("Right Ascen at Week(rad):  %.10g\n"),e->om0);
	tDisplay->AppendText(str);
	str.Printf(wxT("Argument of Perigee(rad):  %.10g\n"),e->argp);
	tDisplay->AppendText(str);
	str.Printf(wxT("Mean Anom(rad):            %.10g\n"),e->m0);
	tDisplay->AppendText(str);
	str.Printf(wxT("Mean Motion(rad):          %.10g\n"),e->deltan);
	tDisplay->AppendText(str);
	str.Printf(wxT("Time of Clock(s):          %.10g\n"),e->toc);
	tDisplay->AppendText(str);
	str.Printf(wxT("Af0(s):                    %.10g\n"),e->af0);
	tDisplay->AppendText(str);
	str.Printf(wxT("Af1(s/s):                  %.10g\n"),e->af1);
	tDisplay->AppendText(str);
	str.Printf(wxT("Af2(s/s/s):                %.10g\n"),e->af2);
	tDisplay->AppendText(str);
	str.Printf(wxT("CIC:                       %.10g\n"),e->cic);
	tDisplay->AppendText(str);
	str.Printf(wxT("CIS:                       %.10g\n"),e->cis);
	tDisplay->AppendText(str);
	str.Printf(wxT("CUC:                       %.10g\n"),e->cuc);
	tDisplay->AppendText(str);
	str.Printf(wxT("CUS:                       %.10g\n"),e->cus);
	tDisplay->AppendText(str);
	str.Printf(wxT("CRC:                       %.10g\n"),e->crc);
	tDisplay->AppendText(str);
	str.Printf(wxT("CRS:                       %.10g\n"),e->crs);
	tDisplay->AppendText(str);

}

void GUI_Ephemeris::onRefresh(wxCommandEvent& event)
{
	int32 val = 0;
	pSerial->formCommand(GET_EPHEMERIS_STATUS_C_ID, &val);
}

void GUI_Ephemeris::onSave(wxCommandEvent& event)
{

	int32 prn, week, lcv;
	wxString fileName, str;
	Ephemeris_M *e;
	FILE *fp;

	wxFileDialog * openFileDialog = new wxFileDialog(this, wxT("Save Ephemeris"), wxT(""), wxT(""), wxT("*.*"), wxSAVE);

	if(openFileDialog->ShowModal() == wxID_OK)
	{
		fileName = openFileDialog->GetPath();
		FILE *fp;

		fp = fopen(fileName.mb_str(),"w");

		if(fp != NULL)
		{
			for(lcv = 0; lcv < MAX_SV; lcv++)
			{
				pSerial->formCommand(GET_EPHEMERIS_C_ID, &lcv);

				e = &p->ephemerides[lcv];
				fwrite(e, sizeof(Ephemeris_M), MAX_SV, fp);
			}
			fflush(fp);
			fclose(fp);
		}
	}

}

void GUI_Ephemeris::onLoad(wxCommandEvent& event)
{

	int32 prn, week, lcv;
	wxString fileName, str;
	Set_Ephemeris_C c;
	FILE *fp;

	wxFileDialog * openFileDialog = new wxFileDialog(this, wxT("Load Ephemeris"), wxT(""), wxT(""), wxT("*.*"), wxOPEN);

	if(openFileDialog->ShowModal() == wxID_OK)
	{
		fileName = openFileDialog->GetPath();
		FILE *fp;

		fp = fopen(fileName.mb_str(),"r");

		if(fp != NULL)
		{
			for(lcv = 0; lcv < MAX_SV; lcv++)
			{
				c.sv = lcv;
				fread(&c.ephemeris, sizeof(Ephemeris_M), 1, fp);
				pSerial->formCommand(SET_EPHEMERIS_C_ID, &c);
			}
			fclose(fp);
		}
	}

}
