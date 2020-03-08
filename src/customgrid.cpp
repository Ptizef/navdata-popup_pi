/************************************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  personalized GRID
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *
 */

#include "customgrid.h"

#include <wx/graphics.h>

#define SCROLL_SENSIBILITY    20

#include "navdata_pi.h"
#include "icons.h"

extern wxString g_shareLocn;
extern wxString g_activeRouteGuid;
extern int      g_blinkTrigger;
extern int      g_selectedPointCol;
extern bool     g_showTripData;
extern bool     g_withSog;
extern wxPoint  g_scrollPos;



//------------------------------------------------------------------------------
//          custom grid implementation
//------------------------------------------------------------------------------
CustomGrid::CustomGrid( wxWindow *parent, wxWindowID id, const wxPoint &pos,
					   const wxSize &size, long style,
                       const wxString &name )
  : wxGrid( parent, id, pos, size, style, name )
{
    //create grid
    SetTable( new wxGridStringTable(0, 0), true, wxGridSelectRows );
    //some general settings
    EnableEditing( false );
    EnableGridLines( true );
    EnableDragGridSize( false );
    SetMargins( 0, 0 );
    EnableDragColMove( false );
    EnableDragColSize( false );
    EnableDragRowSize( false );
    //init labels attr
    GetGlobalColor(_T("DILG0"), &m_labelBackgroundColour);
    GetGlobalColor(_T("DILG3"), &m_labelTextColour );

    //init variables
    m_colLongname = wxNOT_FOUND;
#ifdef __WXOSX__
    m_bLeftDown = false;
#endif
    //connect events at dialog level
    Connect(wxEVT_SCROLLWIN_THUMBTRACK, wxScrollEventHandler( CustomGrid::OnScroll ), NULL, this );
    Connect(wxEVT_SIZE, wxSizeEventHandler( CustomGrid::OnResize ), NULL, this );
    Connect(wxEVT_GRID_LABEL_LEFT_CLICK, wxGridEventHandler( CustomGrid::OnLabelClik ), NULL, this );
    //connect events at grid window level
    GetGridWindow()->Bind( wxEVT_LEFT_DOWN, &CustomGrid::OnMouseEvent, this );
    GetGridWindow()->Bind( wxEVT_RIGHT_DOWN, &CustomGrid::OnMouseEvent, this );
    GetGridWindow()->Bind( wxEVT_LEFT_UP, &CustomGrid::OnMouseEvent, this );
    GetGridWindow()->Bind( wxEVT_MOTION, &CustomGrid::OnMouseEvent, this );
    //connect events at column labels window level
    if( IsTouchInterface_PlugIn() )
        GetGridColLabelWindow()->Bind( wxEVT_LEFT_UP, &CustomGrid::OnMouseRollOverColLabel,this);
    else {
        GetGridColLabelWindow()->Bind( wxEVT_MOTION, &CustomGrid::OnMouseRollOverColLabel,this);
        GetGridColLabelWindow()->Bind( wxEVT_LEAVE_WINDOW, &CustomGrid::OnMouseRollOverColLabel,this);
    }
    GetGridColLabelWindow()->Bind( wxEVT_LEFT_DOWN, &CustomGrid::OnMouseRollOverColLabel,this);
    GetGridColLabelWindow()->Bind( wxEVT_RIGHT_DOWN, &CustomGrid::OnMouseRollOverColLabel,this);
    GetGridRowLabelWindow()->Bind( wxEVT_LEFT_DOWN, &CustomGrid::OnMouseRollOverColLabel,this);
    GetGridRowLabelWindow()->Bind( wxEVT_RIGHT_DOWN, &CustomGrid::OnMouseRollOverColLabel,this);
    //connect timer event
    m_resizeTimer.Bind(wxEVT_TIMER, &CustomGrid::OnResizeTimer, this);
    m_stopLoopTimer.Bind(wxEVT_TIMER, &CustomGrid::OnStopLoopTimer, this);
    m_nameLoopTimer.Bind(wxEVT_TIMER, &CustomGrid::OnNameLoopTimer, this);
}

 CustomGrid::~CustomGrid()
 {}


void CustomGrid::DrawColLabel( wxDC& dc, int col )
{
    if(col == m_colLongname){
        //draw long wpt name
        if(m_nameFlag == NAME_LOOP_READY){
            m_nameFlag = NAME_LOOP_STARTED;
            m_LongName = GetColLabelValue(m_colLongname) + _T(" ... ");
            m_nameLoopTimer.Start(TIMER_INTERVAL_MSECOND, wxTIMER_ONE_SHOT);
        }
    } else {
        //draw wpt name only inside width available
        int w;
        wxString label = GetColLabelValue(col);
        GetTextExtent( label, &w, NULL, 0, 0, &m_labelFont);
        if( w > (GetColWidth(col) - 2)){
            int len =  label.Len() * ((double)(GetColWidth(col) - 2) / (double)w);
            label = GetColLabelValue(col).Mid(0, len);
        }
        //draw rectangle
        dc.SetFont( m_labelFont );
        dc.SetPen(GetDefaultGridLinePen());
        dc.SetBrush(wxBrush(m_labelBackgroundColour, wxBRUSHSTYLE_SOLID));
        dc.DrawRectangle(wxRect(GetColLeft(col), 0, GetColWidth(col),  m_colLabelHeight));
        //draw selected or active mark
        if( (col == 0 && g_selectedPointCol == wxNOT_FOUND )
                    || col == g_selectedPointCol ){
            if( g_blinkTrigger & 1 ) {
                wxImage image;
                int w = 0, h = 0;
                if( col == 0  ){
                    w = _img_activewpt->GetWidth();
                    h = _img_activewpt->GetHeight();
                    wxString file = g_shareLocn + _T("activewpt.svg");
                    if( wxFile::Exists( file ) ){
                        wxBitmap bmp = GetBitmapFromSVGFile( file, w, h);
                        image = bmp.ConvertToImage();
                    } else
                        image = _img_activewpt->ConvertToImage();
                } else {
                    w = _img_targetwpt->GetWidth();
                    h = _img_targetwpt->GetHeight();
                    wxString file = g_shareLocn + _T("targetwpt.svg");
                    if( wxFile::Exists( file ) ){
                        wxBitmap bmp = GetBitmapFromSVGFile( file, w, h);
                        image = bmp.ConvertToImage();
                    } else
                        image = _img_targetwpt->ConvertToImage();
                }

                unsigned char *i = image.GetData();
                if (i == 0)
                    return;
                double scale = ((((double)m_colLabelHeight/2)/h)*4)/4;
                w *= scale;
                h *= scale;
                wxBitmap scaled;
                scaled =  wxBitmap(image.Scale( w, h) );
                dc.DrawBitmap( scaled, GetColLeft(col), 0 );
            }
        }
        //draw label
        dc.SetPen( wxPen(m_labelTextColour, 1, wxPENSTYLE_SOLID));
        dc.DrawLabel(label, wxRect( GetColLeft(col) + 1, 1, GetColWidth(col) - 2,
                            m_colLabelHeight - 2), wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL);
    }
}

void CustomGrid::DrawCornerLabel( wxDC& dc )
{
    //draw corner rectangle
    dc.SetPen(GetDefaultGridLinePen());
    dc.SetBrush(wxBrush(m_labelBackgroundColour, wxBRUSHSTYLE_SOLID));
    dc.DrawRectangle(0, 0, m_rowLabelWidth, m_colLabelHeight);
    //draw setting button
    wxImage image;
    int w = _img_setting->GetWidth();
    int h = _img_setting->GetHeight();
    wxString file = g_shareLocn + _T("setting.svg");
    if( wxFile::Exists( file ) ){
        wxBitmap bmp = GetBitmapFromSVGFile( file, w, h);
        image = bmp.ConvertToImage();
    } else
        image = _img_setting->ConvertToImage();
    unsigned char *i = image.GetData();
    if (i == 0)
        return;
    double scale = ((((double)m_colLabelHeight/1.5)/h)*4)/4;
    w *= scale;
    h *= scale;
    int x = (m_rowLabelWidth/2) - (w/2);
    int y = (m_colLabelHeight-h)/2;
    wxBitmap scaled;
    scaled =  wxBitmap(image.Scale( w, h) );
    dc.DrawBitmap( scaled, x, y );
}

void CustomGrid::OnScroll( wxScrollEvent& event )
{
    m_resizeTimer.Start( TIMER_INTERVAL_10MSECOND, wxTIMER_ONE_SHOT );
    event.Skip();
}

void CustomGrid::CorrectUnwantedScroll()
{
    int frow, fcol, lrow, lcol;
    GetFirstVisibleCell(frow, fcol);
    GetLastVisibleCell(lrow, lcol);
    int x = g_scrollPos.x - fcol;
    int y = g_scrollPos.y - frow;
    MakeCellVisible(frow, lcol + x);
    MakeCellVisible(lrow + y, g_scrollPos.x);

    GetFirstVisibleCell(g_scrollPos.y, g_scrollPos.x);
}

void CustomGrid::OnLabelClik( wxGridEvent& event)
{
    ClearSelection();
    CorrectUnwantedScroll();
    if( event.GetCol() == wxNOT_FOUND && event.GetRow() == wxNOT_FOUND ){
        int x = event.GetPosition().x;
        int o = m_rowLabelWidth / 4;
        if( x > o || x < (m_rowLabelWidth - o ) ){
           bool showTrip = g_showTripData;

            Settings *dialog = new Settings( GetCanvasByIndex(0), wxID_ANY, _("Settings"), wxDefaultPosition, wxDefaultSize, wxCAPTION );

            dialog->ShowModal();

			if (showTrip != g_showTripData) {
				m_pParent->SetTableSizePosition(false);
				if(g_showTripData )
					m_pParent->pPlugin->m_lenghtTimer.Start(TIMER_INTERVAL_MSECOND, wxTIMER_ONE_SHOT);
			}
            wxString s = g_withSog? _("SOG"): _("VMG");
            SetRowLabelValue( 3, _("TTG") + _T("@") + s );
            SetRowLabelValue( 4, _("ETA") + _T("@") + s );
        }
    }
}

void CustomGrid::OnNameLoopTimer( wxTimerEvent& event )
{
    m_nameFlag++;
    DrawLongWptName();
}

void CustomGrid::OnResize( wxSizeEvent& event )
{
    m_resizeTimer.Start( TIMER_INTERVAL_10MSECOND, wxTIMER_ONE_SHOT );
    event.Skip();
}

void CustomGrid::OnResizeTimer(wxTimerEvent& event)
{
    ForceRefresh();
    GetFirstVisibleCell(g_scrollPos.y, g_scrollPos.x);
}

void CustomGrid::OnMouseRollOverColLabel( wxMouseEvent& event)
{
    if(event.LeftDown() || event.RightDown())
        CorrectUnwantedScroll();

    bool endLoop = event.Leaving();
    if( event.Moving() || event.LeftUp() ){
        int cx = event.GetPosition().x;
        int col = XToCol(cx);
        int r,c;
        GetFirstVisibleCell( r, c );
        col +=  c;
        //end processus no valid column selected
        if( col != wxNOT_FOUND ) {           //column label
            int w, h;
            GetTextExtent( GetColLabelValue( col ), &w, &h, 0, 0, &m_labelFont);
            //end processus if the wpt name can be entirely
            if( w > GetColSize(col) ){
                if( col != m_colLongname){
                    m_colLongname = col;
                    m_nameFlag = -2;
                    ForceRefresh();
                }
                /* in case of touch screen will stop long name display
                 *  after 10s if nothing is done*/
                if(IsTouchInterface_PlugIn())
                    m_stopLoopTimer.Start( TIMER_INTERVAL_10SECOND, wxTIMER_ONE_SHOT );
                return;
            } else
               endLoop = true;
        } else
            endLoop = true;
    }
    if(endLoop)
        m_stopLoopTimer.Start( TIMER_INTERVAL_MSECOND, wxTIMER_ONE_SHOT );
}

void CustomGrid::DrawLongWptName()
{
    if(m_colLongname == wxNOT_FOUND)
        return;

    static int x;
    if(m_nameFlag == NAME_NEW_LOOP){
        int r,c;
        GetFirstVisibleCell( r, c );
        x = GetColLeft(m_colLongname - c);
    }
    wxString s = wxEmptyString, label;
    if(m_nameFlag > (int)m_LongName.Len())
        m_nameFlag = NAME_NEW_LOOP;
    int i = m_nameFlag;
    int j = 0;
    while(1){
        if((i + j) > (int)m_LongName.Len() -1){
            i = 0; j = 0;
        }
        s.Append(m_LongName.GetChar(i + j));
        int w;
        GetTextExtent(s, &w, NULL, 0, 0, &m_labelFont);
        if( w > GetColWidth(m_colLongname))
            break;
        label = s;
        j++;
    }
    wxClientDC *cdc = new wxClientDC(GetGridColLabelWindow());
    if( cdc ) {
        //draw rectangle
        cdc->SetFont( m_labelFont );
        cdc->SetPen(GetDefaultGridLinePen());
        cdc->SetBrush(wxBrush(m_labelBackgroundColour, wxBRUSHSTYLE_SOLID));
        cdc->DrawRectangle(wxRect(x, 0, GetColWidth(m_colLongname),  m_colLabelHeight));
        //draw label
        wxColour clf;
        GetGlobalColor( _T("BLUE2"), &clf );
        cdc->SetTextForeground(clf);
        cdc->DrawLabel(label, wxRect(x, 1, GetColWidth(m_colLongname) - 2,
                        m_colLabelHeight - 2), wxALIGN_LEFT|wxALIGN_CENTER_VERTICAL);


        m_nameLoopTimer.Start(TIMER_INTERVAL_75MSECOND, wxTIMER_ONE_SHOT);
    }
}

void CustomGrid::OnStopLoopTimer( wxTimerEvent& event )
{
    if(m_nameLoopTimer.IsRunning())
        m_nameLoopTimer.Stop();

    m_colLongname = wxNOT_FOUND;
    m_nameFlag = IDLE_STATE_NUMBER;

    ForceRefresh();
}

void CustomGrid::OnMouseEvent( wxMouseEvent& event )
{
    if((event.LeftDown() || event.RightDown())){
        CorrectUnwantedScroll();
        if( m_colLongname != wxNOT_FOUND )
            m_stopLoopTimer.Start( TIMER_INTERVAL_MSECOND, wxTIMER_ONE_SHOT );
    }

    static wxPoint s_pevt;
    wxPoint pevt = event.GetPosition();
#ifdef __WXOSX__
    if (!m_bLeftDown && event.LeftIsDown()){
        m_bLeftDown = true;
        s_pevt = pevt;
    }
    else if (m_bLeftDown && !event.LeftIsDown()){
        m_bLeftDown = false;
        if (HasCapture()) ReleaseMouse();
    }
#else
    if(event.LeftDown())
        s_pevt = pevt;
    if(event.LeftUp()){
        if(HasCapture()) ReleaseMouse();
    }
#endif
    if(event.Dragging()){
        m_pParent->SetTargetFlag( false );
        int frow, fcol, lrow, lcol;
        GetFirstVisibleCell(frow, fcol);
        GetLastVisibleCell(lrow, lcol);
        if( pevt != s_pevt ) {
            bool rfh = false;
            int diff = pevt.x - s_pevt.x;
            //scrolling right
            if( diff > SCROLL_SENSIBILITY ){
                s_pevt.x = pevt.x;
                if( fcol > 0 ){
                    MakeCellVisible(frow, fcol - 1 );
                    rfh = true;
                }
            }
            //scrolling left
            else  if ( -diff > SCROLL_SENSIBILITY ){
                s_pevt.x = pevt.x;
                if( lcol < m_numCols - 1 ){
                    MakeCellVisible(frow, lcol + 1);
                    rfh = true;
                }
            }
            //scrolling down
            diff = pevt.y - s_pevt.y;
            if( diff > SCROLL_SENSIBILITY ){
                s_pevt.y = pevt.y;
                if( frow > 0 ){
                    MakeCellVisible(frow - 1, fcol);
                    rfh = true;
                }
            }
            //scrolling up
            else if( -diff > SCROLL_SENSIBILITY ) {
                s_pevt.y = pevt.y;
                if( lrow < m_numRows - 1 ){
                    MakeCellVisible(lrow + 1, fcol);
                    MakeCellVisible(frow + 1, fcol);      //workaroud for what seems curious moving 2 rows instead of 1 in previous line
                }
            }
            if(rfh)
                m_resizeTimer.Start( TIMER_INTERVAL_10MSECOND, wxTIMER_ONE_SHOT );
        }
    }
}

//find the first top/left visible cell coords
void CustomGrid::GetFirstVisibleCell(int& frow, int& fcol)
{
    frow = 0;
	for(fcol = 0; fcol < m_numCols; fcol++){
		for(frow = 0; frow < m_numRows; frow++) {
            if(IsVisible(frow, fcol)) //find the first row/col
                return;
		}
	}
}

//find the visible cell coords
void CustomGrid::GetLastVisibleCell(int& lrow, int& lcol)
{
    lrow = wxMax(m_numRows - 1, 0);
	for(lcol = wxMax(m_numCols - 1, 0); lcol > -1; lcol--){
		for(lrow = m_numRows - 1; lrow > -1; lrow--) {
            if(IsVisible(lrow, lcol)){
                return;
            }
		}
	}
}

