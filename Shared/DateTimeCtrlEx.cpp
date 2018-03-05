// DateTimeCtrlEx.cpp : implementation file
//

#include "stdafx.h"
#include "DateTimeCtrlEx.h"
#include "OSVersion.h"

#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

#ifndef DTM_GETDATETIMEPICKERINFO
#	define DTM_GETDATETIMEPICKERINFO (DTM_FIRST + 14)

struct DATETIMEPICKERINFO
{
    DWORD cbSize;
	
    RECT rcCheck;
    DWORD stateCheck;
	
    RECT rcButton;
    DWORD stateButton;
	
    HWND hwndEdit;
    HWND hwndUD;
    HWND hwndDropDown;
};

#endif

#ifndef DTM_SETMCSTYLE
#	define DTM_SETMCSTYLE (DTM_FIRST + 11)
#	define DTM_GETMCSTYLE (DTM_FIRST + 12)
#endif

/////////////////////////////////////////////////////////////////////////////
// CDateTimeCtrlEx

CDateTimeCtrlEx::CDateTimeCtrlEx(DWORD dwMonthCalStyle) : m_dwMonthCalStyle(dwMonthCalStyle)
{
	// clear state
	Reset();
}

CDateTimeCtrlEx::~CDateTimeCtrlEx()
{
}

void CDateTimeCtrlEx::Reset()
{
	m_bButtonDown = m_bDropped = m_bWasSet = FALSE;
	
	ZeroMemory(&m_nmdtcFirst, sizeof(m_nmdtcFirst));
	ZeroMemory(&m_nmdtcLast, sizeof(m_nmdtcLast));
}

BEGIN_MESSAGE_MAP(CDateTimeCtrlEx, CDateTimeCtrl)
	//{{AFX_MSG_MAP(CDateTimeCtrlEx)
	ON_WM_LBUTTONDOWN()
	ON_NOTIFY_REFLECT_EX(DTN_DATETIMECHANGE, OnDateTimeChange)
	ON_NOTIFY_REFLECT(DTN_CLOSEUP, OnCloseUp)
	ON_NOTIFY_REFLECT(DTN_DROPDOWN, OnDropDown)
	ON_WM_SYSKEYDOWN()
	//}}AFX_MSG_MAP
	ON_WM_CREATE()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDateTimeCtrlEx message handlers

DWORD CDateTimeCtrlEx::SetMonthCalStyle(DWORD dwStyle)
{ 
	if (!GetSafeHwnd())
	{
		m_dwMonthCalStyle = dwStyle;
		return 0;
	}

	// else
	ASSERT(::IsWindow(m_hWnd)); 
	
	return (DWORD)::SendMessage(m_hWnd, DTM_SETMCSTYLE, 0, (LPARAM)dwStyle); 
}

DWORD CDateTimeCtrlEx::GetMonthCalStyle() const
{ 
	if (!GetSafeHwnd())
		return m_dwMonthCalStyle;

	ASSERT(::IsWindow(m_hWnd)); 
	
	return (DWORD)::SendMessage(m_hWnd, DTM_GETMCSTYLE, 0, 0);
}

int CDateTimeCtrlEx::OnCreate(LPCREATESTRUCT pCreate)
{
	if (m_dwMonthCalStyle)
	{
		SetMonthCalStyle(m_dwMonthCalStyle);
		m_dwMonthCalStyle = 0;
	}

	return CDateTimeCtrl::OnCreate(pCreate);
}

void CDateTimeCtrlEx::PreSubclassWindow()
{
	if (m_dwMonthCalStyle)
	{
		SetMonthCalStyle(m_dwMonthCalStyle);
		m_dwMonthCalStyle = 0;
	}

	CDateTimeCtrl::PreSubclassWindow();
}

void CDateTimeCtrlEx::OnLButtonDown(UINT nFlags, CPoint point) 
{
	// the sequence of logic once the dropdown button
	// has been clicked appears to be this:

	// 1. This message
	// 2. If the state was GDT_NONE then the date is 
	//    set to the current date and a DTN_DATECHANGE
	//    is sent to the parent and reflected here first
	// 3. Then we get no more mouse messages until the MonthCal 
	//    control closes which makes interpreting the
	//    user's intentions tricky
	// 4. If the user clicks outside the MonthCal then
	//    we receive DTN_CLOSEUP but no DTN_DATECHANGE
	// 5. If the user presses <escape> then we receive
	//    DTN_DATECHANGE followed by DTN_CLOSEUP
	// 6. If the user clicks on a day on the MonthCal
	//    we also get DTN_DATECHANGE followed by DTN_CLOSEUP
	// 7. If the user navigates using the cursor keys
	//    then we get multiple DTN_DATECHANGE
	// 8. If the user presses <enter> we get DTN_CLOSEUP
	//    without a DTN_DATECHANGE

	// clear state
	Reset();
	
	if (GetDropButtonRect().PtInRect(point))
	{
		m_bButtonDown = TRUE;

		// was the date set before ?
		COleDateTime date;
		m_bWasSet = (GetTime(date) && (date.GetStatus() == COleDateTime::valid));
	}

	CDateTimeCtrl::OnLButtonDown(nFlags, point);
}

BOOL CDateTimeCtrlEx::OnDateTimeChange(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMDATETIMECHANGE* pNMDTC = (NMDATETIMECHANGE*)pNMHDR;

	// if we get a date change between clicking the
	// button and receiving DTN_DROPDOWN then we must
	// have been unset
	if (m_bButtonDown)
	{
		// capture the first change in case we need it later
		m_nmdtcFirst = (*pNMDTC);
		return TRUE; 
	}
	else if (m_bDropped)
	{
		// capture the last change for sending to our 
		// parent at the end and then eat it
		m_nmdtcLast = (*pNMDTC);
		return TRUE; 
	}
	
	*pResult = 0;
	return FALSE;
}

void CDateTimeCtrlEx::OnCloseUp(NMHDR* /*pNMHDR*/, LRESULT* pResult) 
{
	ASSERT(!m_bButtonDown);
	ASSERT(m_bDropped);

	m_bDropped = FALSE;

	// see if we can figure out what key was pressed
	BOOL bCancel = (GetKeyState(VK_ESCAPE) & 0x8000);
	BOOL bSetDate = (GetKeyState(VK_RETURN) & 0x8000);

	ASSERT(!(bCancel && bSetDate));

	// if neither is pressed then try figure out whether 
	// the date is set
	if (!bCancel && !bSetDate)
	{
		COleDateTime date(m_nmdtcLast.st);
		
		bSetDate = ((date.m_dt != 0.0) && (date.GetStatus() == COleDateTime::valid));
		bCancel = !bSetDate;
	}
	
	if (bCancel)
	{
		// just set the checkbox state if originally unset
		if (!m_bWasSet)
			SendMessage(DTM_SETSYSTEMTIME, GDT_NONE);
	}
	else if (bSetDate)
	{
		// notify our parent of the last change
		// because we've been eating them all

		// special case: User pressed <return> with no
		// other action in which case we never received
		// a DTN_DATECHANGE and we have to fix it up here
		if (m_nmdtcLast.nmhdr.hwndFrom != NULL)
		{
			GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)(&m_nmdtcLast));
		}
		else
		{
			// fall back on the first notification
			ASSERT(m_nmdtcFirst.nmhdr.hwndFrom != NULL);
			GetParent()->SendMessage(WM_NOTIFY, GetDlgCtrlID(), (LPARAM)(&m_nmdtcFirst));
		}
	}

	// clear state
	Reset();

	*pResult = 0;
}

void CDateTimeCtrlEx::OnDropDown(NMHDR* /*pNMHDR*/, LRESULT* pResult) 
{
	if (m_bButtonDown)
	{
		m_bDropped = TRUE;
		m_bButtonDown = FALSE;
	}

	*pResult = 0;
}

BOOL CDateTimeCtrlEx::IsKeyDown(UINT nVirtKey)
{
	return (GetKeyState(nVirtKey) & 0x8000);
}

CRect CDateTimeCtrlEx::GetDropButtonRect() const
{
	CRect rButton;
	GetClientRect(rButton);

	// versions <= XP
	rButton.left = rButton.right - GetSystemMetrics(SM_CXVSCROLL);

	// versions >= Vista, query the control itself
	if (COSVersion() >= OSV_VISTA)
	{
		DATETIMEPICKERINFO dtpi = { sizeof(dtpi), 0 };

		CDateTimeCtrlEx* pThis = const_cast<CDateTimeCtrlEx*>(this);

		if (pThis->SendMessage(DTM_GETDATETIMEPICKERINFO, 0, (LPARAM)&dtpi))
			rButton = dtpi.rcButton;
	}

	return rButton;
}

void CDateTimeCtrlEx::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	// this handles opening the calendar with 'ALT + Down'
	m_bButtonDown = FALSE;
	ZeroMemory(&m_nmdtcLast, sizeof(m_nmdtcLast));
	
	if ((nChar == VK_DOWN) && IsKeyDown(VK_MENU))
	{
		m_bButtonDown = TRUE;
		
		// was the date set before ?
		COleDateTime date;
		m_bWasSet = (GetTime(date) && (date.GetStatus() == COleDateTime::valid));
	}
	
	CDateTimeCtrl::OnSysKeyDown(nChar, nRepCnt, nFlags);
}
