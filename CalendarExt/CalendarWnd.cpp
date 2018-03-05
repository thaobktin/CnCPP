// CalendarFrameWnd.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "CalendarExt.h"
#include "CalendarWnd.h"

#include "..\Shared\DialogHelper.h"
#include "..\Shared\DateHelper.h"
#include "..\Shared\TimeHelper.h"
#include "..\Shared\FileMisc.h"
#include "..\Shared\UITheme.h"
#include "..\Shared\themed.h"
#include "..\Shared\dlgunits.h"
#include "..\shared\IPreferences.h"
#include "..\shared\misc.h"
#include "..\shared\graphicsmisc.h"
#include "..\shared\localizer.h"

#include "..\todolist\tdcmsg.h"
#include "..\todolist\tdcenum.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

const UINT IDC_BIG_CALENDAR = 101;
const UINT IDC_MINI_CALENDAR = 102;

/////////////////////////////////////////////////////////////////////////////
// CCalendarWnd

IMPLEMENT_DYNAMIC(CCalendarWnd, CDialog)

CCalendarWnd::CCalendarWnd()
	:	
	m_hIcon(AfxGetApp()->LoadIcon(IDR_CALENDAR))
{
}

CCalendarWnd::~CCalendarWnd()
{
}

void CCalendarWnd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_DIVIDER, m_stDivider);
	DDX_Control(pDX, IDC_NUMWEEKS, m_cbNumWeeks);
	DDX_Control(pDX, IDC_SNAPMODES, m_cbSnapModes);
	DDX_Text(pDX, IDC_SELECTEDTASKDATES, m_sSelectedTaskDates);
}

BEGIN_MESSAGE_MAP(CCalendarWnd, CDialog)
	//{{AFX_MSG_MAP(CCalendarWnd)
	ON_WM_SETFOCUS()
	ON_WM_CTLCOLOR()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_BN_CLICKED(IDC_GOTOTODAY, OnGototoday)
	ON_BN_CLICKED(IDC_PREFERENCES, OnPreferences)
	//}}AFX_MSG_MAP
	ON_CBN_SELCHANGE(IDC_NUMWEEKS, OnSelChangeNumWeeks)
	ON_CBN_SELCHANGE(IDC_SNAPMODES, OnSelChangeSnapMode)
	ON_NOTIFY(NM_CLICK, IDC_MINI_CALENDAR, OnMiniCalendarNotifyClick)
	ON_NOTIFY(NM_CLICK, IDC_BIG_CALENDAR, OnBigCalendarNotifyClick)
	ON_NOTIFY(NM_DBLCLK, IDC_MINI_CALENDAR, OnMiniCalendarNotifyDblClk)
	ON_NOTIFY(NM_DBLCLK, IDC_BIG_CALENDAR, OnBigCalendarNotifyDblClk)
	ON_REGISTERED_MESSAGE(WM_CALENDAR_DATECHANGE, OnBigCalendarNotifyDateChange)
	ON_REGISTERED_MESSAGE(WM_CALENDAR_SELCHANGE, OnBigCalendarNotifySelectionChange)
	ON_REGISTERED_MESSAGE(WM_CALENDAR_DRAGCHANGE, OnBigCalendarNotifyDragChange)
	ON_REGISTERED_MESSAGE(WM_CALENDAR_VISIBLEWEEKCHANGE, OnBigCalendarNotifyVisibleWeekChange)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCalendarWnd message handlers

BOOL CCalendarWnd::Create(DWORD dwStyle, const RECT &rect, CWnd* pParentWnd, UINT nID)
{
	if (CDialog::Create(IDD_CALENDAR_DIALOG, pParentWnd))
	{
		SetWindowLong(*this, GWL_STYLE, dwStyle);
		SetDlgCtrlID(nID);

		return TRUE;
	}

	return FALSE;
}

BOOL CCalendarWnd::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// non-translatables
	CLocalizer::EnableTranslation(*GetDlgItem(IDC_SELECTEDTASKDATES), FALSE);

	// create mini-calendar ctrl
	m_MiniCalendar.Create(NULL, NULL, WS_CHILD | WS_VISIBLE | FMC_NO3DBORDER, 
                          CRect(0,0,0,0), this, IDC_MINI_CALENDAR);

    m_MiniCalendar.SetDate(COleDateTime::GetCurrentTime());
    m_MiniCalendar.SetRowsAndColumns(3, 1);
	m_MiniCalendar.SetSpecialDaysCallback(CCalendarWnd::IsMiniCalSpecialDateCallback, (DWORD)this);

	// create big-calendar ctrl
	m_BigCalendar.Create(WS_CHILD | WS_VISIBLE, CRect(0,0,0,0), this, IDC_BIG_CALENDAR);
	m_BigCalendar.EnableMultiSelection(FALSE);
	m_BigCalendar.SetFirstWeekDay(CDateHelper::GetFirstDayOfWeek());

	m_cbNumWeeks.SetCurSel(m_BigCalendar.GetVisibleWeeks() - 1);

	// build snap combobox
	InitSnapCombo();

	return 0;
}

void CCalendarWnd::InitSnapCombo()
{
	CDialogHelper::AddString(m_cbSnapModes, IDS_SNAP_NEARESTHOUR,	TCCSM_NEARESTHOUR);
	CDialogHelper::AddString(m_cbSnapModes, IDS_SNAP_NEARESTDAY,	TCCSM_NEARESTDAY);
	CDialogHelper::AddString(m_cbSnapModes, IDS_SNAP_NEARESTHALFDAY, TCCSM_NEARESTHALFDAY);
	CDialogHelper::AddString(m_cbSnapModes, IDS_SNAP_FREE,			TCCSM_FREE);

	CDialogHelper::SelectItemByData(m_cbSnapModes, m_BigCalendar.GetSnapMode());
}

void CCalendarWnd::OnSelChangeNumWeeks()
{
	m_BigCalendar.SetVisibleWeeks(m_cbNumWeeks.GetCurSel() + 1);
}

void CCalendarWnd::OnSelChangeSnapMode()
{
	m_BigCalendar.SetSnapMode((TCC_SNAPMODE)CDialogHelper::GetSelectedItemData(m_cbSnapModes));
}

void CCalendarWnd::SavePreferences(IPreferences* pPrefs, LPCTSTR szKey) const 
{
	m_dlgPrefs.SavePreferences(pPrefs, szKey);

	pPrefs->WriteProfileInt(szKey, _T("NumWeeks"), m_BigCalendar.GetVisibleWeeks());
}

void CCalendarWnd::LoadPreferences(const IPreferences* pPrefs, LPCTSTR szKey, BOOL bAppOnly) 
{
	// app preferences
	// TODO

	// calendar specific preferences
	if (!bAppOnly)
	{
		m_dlgPrefs.LoadPreferences(pPrefs, szKey);
		UpdateCalendarCtrlPreferences();

		int nWeeks = pPrefs->GetProfileInt(szKey, _T("NumWeeks"), 6);
		
		if (m_BigCalendar.SetVisibleWeeks(nWeeks))
			m_cbNumWeeks.SetCurSel(nWeeks-1);

		// make sure 'today' is visible
		COleDateTime dtToday = COleDateTime::GetCurrentTime();
		m_MiniCalendar.SetDate(dtToday);
		m_BigCalendar.GotoToday();
	}
}

void CCalendarWnd::UpdateCalendarCtrlPreferences()
{
	m_BigCalendar.SetOption(TCCO_CALCMISSINGSTARTASCREATION, m_dlgPrefs.GetCalcMissingStartAsCreation());
	m_BigCalendar.SetOption(TCCO_CALCMISSINGSTARTASDUE, m_dlgPrefs.GetCalcMissingStartAsDue());
	m_BigCalendar.SetOption(TCCO_CALCMISSINGSTARTASEARLIESTDUEANDTODAY, m_dlgPrefs.GetCalcMissingStartAsEarliestDueAndToday());
	m_BigCalendar.SetOption(TCCO_CALCMISSINGDUEASSTART, m_dlgPrefs.GetCalcMissingDueAsStart());
	m_BigCalendar.SetOption(TCCO_CALCMISSINGDUEASLATESTSTARTANDTODAY, m_dlgPrefs.GetCalcMissingDueAsLatestStartAndToday());
	m_BigCalendar.SetOption(TCCO_DISPLAYCONTINUOUS, m_dlgPrefs.GetDisplayAsContinuous());
	m_BigCalendar.SetOption(TCCO_DISPLAYSTART, m_dlgPrefs.GetDisplayStart());
	m_BigCalendar.SetOption(TCCO_DISPLAYDUE, m_dlgPrefs.GetDisplayDue());
	m_BigCalendar.SetOption(TCCO_DISPLAYCALCSTART, m_dlgPrefs.GetDisplayCalcStart());
	m_BigCalendar.SetOption(TCCO_DISPLAYCALCDUE, m_dlgPrefs.GetDisplayCalcDue());
	m_BigCalendar.SetOption(TCCO_ADJUSTTASKHEIGHTS, m_dlgPrefs.GetAdjustTaskHeights());
}

void CCalendarWnd::SetUITheme(const UITHEME* pTheme)
{
	GraphicsMisc::VerifyDeleteObject(m_brBack);

	if (CThemed::IsThemeActive() && pTheme)
	{
		m_theme = *pTheme;
		m_brBack.CreateSolidBrush(pTheme->crAppBackLight);

		m_BigCalendar.SetThemeColour(pTheme->crAppBackDark);

// 		CFPSMiniCalendarCtrlFontInfo font;
// 		m_MiniCalendar.GetFontInfo(FMC_FONT_SPECIALDAYS, font);
// 
// 		font.m_crBkColor = pTheme->crAppBackLight;
// 		m_MiniCalendar.SetFontInfo(FMC_FONT_SPECIALDAYS, font);

		m_MiniCalendar.SetHighlightToday(TRUE, pTheme->crAppLinesDark);
	}
}

LPCTSTR CCalendarWnd::GetMenuText() const
{
	return _T("Calendar");
}

LPCTSTR CCalendarWnd::GetTypeID() const
{
	static CString sID;

	Misc::GuidToString(CAL_TYPEID, sID); 

	return sID;
}

bool CCalendarWnd::ProcessMessage(MSG* pMsg) 
{
	if (!IsWindowEnabled())
		return false;

	// process editing shortcuts
	// TODO

	return false;
}

void CCalendarWnd::DoAppCommand(IUI_APPCOMMAND nCmd) 
{ 
	// do nothing for now 
}

bool CCalendarWnd::CanDoAppCommand(IUI_APPCOMMAND nCmd) const 
{ 
	// do nothing for now 
	return false;
}

bool CCalendarWnd::GetLabelEditRect(LPRECT pEdit)
{
	DWORD dwSelTask = m_BigCalendar.GetSelectedTaskID();
	m_BigCalendar.EnsureVisible(dwSelTask, FALSE);

	CRect rLabel;

	if (m_BigCalendar.GetTaskLabelRect(dwSelTask, rLabel))
	{
		m_BigCalendar.ClientToScreen(rLabel);

		// make sure it has a minimum height
		rLabel.bottom = (rLabel.top + max(rLabel.Height(), DEF_TASK_HEIGHT));
		*pEdit = rLabel;

		return true;
	}

	// else
	return false;
}

IUI_HITTEST CCalendarWnd::HitTest(const POINT& ptScreen) const
{
	// try header
	if (m_BigCalendar.PtInHeader(ptScreen))
		return IUI_COLUMNHEADER;

	// then specific task
	CPoint ptBigCal(ptScreen);
	m_BigCalendar.ScreenToClient(&ptBigCal);

	if (m_BigCalendar.HitTest(ptBigCal))
		return IUI_TASK;

	// else try rest of big cal
	CRect rCal;
	m_BigCalendar.GetClientRect(rCal);

	return (rCal.PtInRect(ptBigCal) ? IUI_TASKLIST : IUI_NOWHERE);
}

bool CCalendarWnd::SelectTask(DWORD dwTaskID)
{
	bool bSet = (m_BigCalendar.SelectTask(dwTaskID) != FALSE);

	UpdateSelectedTaskDates();

	return bSet;
}

bool CCalendarWnd::SelectTasks(DWORD* pdwTaskIDs, int nTaskCount)
{
	return false; // only support single selection
}

bool CCalendarWnd::WantUpdate(int nAttribute) const
{
	switch (nAttribute)
	{
	case TDCA_TASKNAME:
	case TDCA_DONEDATE:
	case TDCA_DUEDATE:
	case TDCA_STARTDATE:
	case TDCA_COLOR:
		return true;
	}

	// all else 
	return false;
}

bool CCalendarWnd::PrepareNewTask(ITaskList* pTask) const
{
	return m_BigCalendar.PrepareNewTask(pTask);
}

void CCalendarWnd::UpdateTasks(const ITaskList* pTasks, IUI_UPDATETYPE nUpdate, int nEditAttribute)
{
	m_BigCalendar.UpdateTasks(pTasks, nUpdate, nEditAttribute);

	UpdateSelectedTaskDates();
}

void CCalendarWnd::Release()
{
	if (GetSafeHwnd())
		DestroyWindow();
	
	delete this;
}

void CCalendarWnd::OnSetFocus(CWnd* pOldWnd)
{
	//set focus to big calendar (seems the focus gets lost after switching back to the Calendar window from another app)
	m_BigCalendar.SetFocus();
}

void CCalendarWnd::OnSize(UINT nType, int cx, int cy) 
{
	CDialog::OnSize(nType, cx, cy);

	if (m_BigCalendar.GetSafeHwnd())
		ResizeControls(cx, cy);
}

BOOL CALLBACK CCalendarWnd::IsMiniCalSpecialDateCallback(COleDateTime &dt, DWORD dwUserData)
{
	ASSERT(dwUserData);

	CCalendarWnd* pThis = (CCalendarWnd*)dwUserData;

	return pThis->m_BigCalendar.IsSpecialDate(dt);
}


HBRUSH CCalendarWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
	
	if (nCtlColor == CTLCOLOR_STATIC && m_brBack.GetSafeHandle())
	{
		pDC->SetTextColor(m_theme.crAppText);
		pDC->SetBkMode(TRANSPARENT);
		hbr = m_brBack;
	}

	return hbr;
}

BOOL CCalendarWnd::OnEraseBkgnd(CDC* pDC)
{
	// clip out children
	CDialogHelper::ExcludeChild(this, pDC, IDC_CALENDAR_FRAME);

	// then our background
	if (m_brBack.GetSafeHandle())
	{
		CRect rClient;
		GetClientRect(rClient);

		pDC->FillSolidRect(rClient, m_theme.crAppBackLight);
		return TRUE;
	}
	
	// else
	return CDialog::OnEraseBkgnd(pDC);
}

void CCalendarWnd::OnPreferences()
{
	if (m_dlgPrefs.DoModal() == IDOK)
	{
		// show/hide the minicalendar as necessary
		if (m_dlgPrefs.GetShowMiniCalendar())
			m_MiniCalendar.ShowWindow(SW_SHOW);
		else
			m_MiniCalendar.ShowWindow(SW_HIDE);

		// forward options to calendar ctrl
		UpdateCalendarCtrlPreferences();

		// call ResizeControls which will resize both calendars
		CRect rcFrame;
		GetClientRect(rcFrame);
		ResizeControls(rcFrame.Width(), rcFrame.Height());
	}

	//set focus to big calendar (seems the focus gets lost after the dialog is shown)
	m_BigCalendar.SetFocus();
}

void CCalendarWnd::ResizeControls(int cx, int cy)
{
	m_stDivider.MoveWindow(CRect(0, 0, cx, 1));

	// selected task dates takes available space
	int nOffset = cx - CDialogHelper::GetCtrlRect(this, IDC_SELECTEDTASKDATES).right;
	CDialogHelper::ResizeCtrl(this, IDC_SELECTEDTASKDATES, nOffset, 0);

	// calendar frame
	CRect rFrame = CDialogHelper::GetCtrlRect(this, IDC_CALENDAR_FRAME);

	rFrame.right = cx;
	rFrame.bottom = cy;
	CDialogHelper::ResizeCtrl(this, IDC_CALENDAR_FRAME, cx, rFrame.Height());

	// mini-cal
	rFrame.DeflateRect(1, 1);
	int nMiniCalWidth = (m_dlgPrefs.GetShowMiniCalendar() ? m_MiniCalendar.GetMinWidth() : 0);

	CRect rItem(rFrame);
	rItem.right = rItem.left + nMiniCalWidth;

	m_MiniCalendar.MoveWindow(rItem);

	// big-cal
	rItem.left = rItem.right;
	rItem.right = rFrame.right;

	m_BigCalendar.MoveWindow(rItem);

	Invalidate(TRUE);
}

void CCalendarWnd::OnBigCalendarNotifyClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	time_t dtSel = m_BigCalendar.GetFirstSelectedItem();

	if (dtSel > 0)
		m_MiniCalendar.SetDate(dtSel);

	*pResult = 0;
}

void CCalendarWnd::OnBigCalendarNotifyDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
	time_t dtSel = m_BigCalendar.GetFirstSelectedItem();

	if (dtSel > 0)
		m_MiniCalendar.SetDate(dtSel);

	*pResult = 0;
}

void CCalendarWnd::OnMiniCalendarNotifyClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	COleDateTime dtSel = m_MiniCalendar.GetDate();
	m_BigCalendar.Goto(dtSel, true);

	*pResult = 0;
}

void CCalendarWnd::OnMiniCalendarNotifyDblClk(NMHDR* pNMHDR, LRESULT* pResult)
{
	COleDateTime dtSel = m_MiniCalendar.GetDate();
	m_BigCalendar.Goto(dtSel, true);

	*pResult = 0;
}

void CCalendarWnd::OnGototoday() 
{
	COleDateTime dtToday = COleDateTime::GetCurrentTime();

	m_MiniCalendar.SetDate(dtToday);
	m_BigCalendar.GotoToday(true);
}

LRESULT CCalendarWnd::OnBigCalendarNotifyDateChange(WPARAM wp, LPARAM lp)
{
	COleDateTime dtStart, dtDue;

	if (m_BigCalendar.GetSelectedTaskDates(dtStart, dtDue))
	{
		switch (wp)
		{
		case TCCHT_BEGIN:
			GetParent()->SendMessage(WM_IUI_MODIFYSELECTEDTASK, TDCA_STARTDATE, (LPARAM)&(dtStart.m_dt));
			break;
			
		case TCCHT_MIDDLE:
			GetParent()->SendMessage(WM_IUI_MODIFYSELECTEDTASK, TDCA_STARTDATE, (LPARAM)&(dtStart.m_dt));
			GetParent()->SendMessage(WM_IUI_MODIFYSELECTEDTASK, TDCA_DUEDATE, (LPARAM)&(dtDue.m_dt));
			break;

		case TCCHT_END:
			GetParent()->SendMessage(WM_IUI_MODIFYSELECTEDTASK, TDCA_DUEDATE, (LPARAM)&(dtDue.m_dt));
			break;
		}
	}

	return 0L;
}

LRESULT CCalendarWnd::OnBigCalendarNotifySelectionChange(WPARAM wp, LPARAM lp)
{
	if (lp)
	{
		UpdateSelectedTaskDates();
		GetParent()->SendMessage(WM_IUI_SELECTTASK, 0, lp);
	}

	return 0L;
}

void CCalendarWnd::OnCancel()
{
	m_BigCalendar.CancelDrag();
}

void CCalendarWnd::UpdateSelectedTaskDates()
{
	COleDateTime dtStart, dtDue;
	
	if (m_BigCalendar.GetSelectedTaskDates(dtStart, dtDue))
	{
		CString sStart, sDue;

		if (CDateHelper::DateHasTime(dtStart))
			sStart = CDateHelper::FormatDate(dtStart, DHFD_TIME | DHFD_NOSEC);
		else
			sStart = CDateHelper::FormatDate(dtStart);

		if (CDateHelper::DateHasTime(dtDue))
			sDue = CDateHelper::FormatDate(dtDue, DHFD_TIME | DHFD_NOSEC);
		else
			sDue = CDateHelper::FormatDate(dtDue);

		m_sSelectedTaskDates.Format(_T("%s - %s"), sStart, sDue);
	}
	else
	{
		m_sSelectedTaskDates.Empty();
	}

	UpdateData(FALSE);
}

LRESULT CCalendarWnd::OnBigCalendarNotifyDragChange(WPARAM wp, LPARAM /*lp*/)
{
	CDialogHelper::SelectItemByData(m_cbSnapModes, wp);
	UpdateSelectedTaskDates();

	return 0L;
}

LRESULT CCalendarWnd::OnBigCalendarNotifyVisibleWeekChange(WPARAM wp, LPARAM /*lp*/)
{
	m_cbNumWeeks.SetCurSel(m_BigCalendar.GetVisibleWeeks() - 1);

	return 0L;
}
