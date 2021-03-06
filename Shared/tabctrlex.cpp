// tabctrlex.cpp : implementation file
//

#include "stdafx.h"
#include "tabctrlex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTabCtrlEx

CTabCtrlEx::CTabCtrlEx(DWORD dwFlags, ETabOrientation orientation) 
	: 
	m_dwFlags(dwFlags), m_bMBtnDown(FALSE), CXPTabCtrl(orientation)
{
}

CTabCtrlEx::~CTabCtrlEx()
{
}


BEGIN_MESSAGE_MAP(CTabCtrlEx, CXPTabCtrl)
	//{{AFX_MSG_MAP(CTabCtrlEx)
	//}}AFX_MSG_MAP
	ON_WM_PAINT()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTabCtrlEx message handlers

void CTabCtrlEx::OnPaint() 
{
	if (!IsExtendedTabThemedXP() && !(m_dwFlags & TCE_POSTDRAW))
	{
		Default();
		return;
	}

	CPaintDC dc(this); // device context for painting
	
	// always do default
	if(!IsExtendedTabThemedXP())
		DefWindowProc(WM_PAINT, (WPARAM)dc.m_hDC, 0L);
	else
		CXPTabCtrl::DoPaint(&dc);

	// then post draw if required
	if (m_dwFlags & TCE_POSTDRAW)
	{
		CRect rClip;
		dc.GetClipBox(rClip);

		DRAWITEMSTRUCT dis;
		dis.CtlType = ODT_TAB;
		dis.CtlID = GetDlgCtrlID();
		dis.hwndItem = GetSafeHwnd();
		dis.hDC = dc;
		dis.itemAction = ODA_DRAWENTIRE;
		
		// paint the tabs 
		int nTab = GetItemCount();
		int nSel = GetCurSel();
		
		while (nTab--)
		{
			if (nTab != nSel)
			{
				dis.itemID = nTab;
				dis.itemState = 0;
				
				VERIFY(GetItemRect(nTab, &dis.rcItem));

				dis.rcItem.bottom -= 2;
				dis.rcItem.top += 2;
				dis.rcItem.left += 2;
				dis.rcItem.right -= 2;

				if (CRect().IntersectRect(rClip, &dis.rcItem))
					GetParent()->SendMessage(WM_DRAWITEM, dis.CtlID, (LPARAM)&dis);
			}
		}
		
		// now selected tab
		if (nSel != -1)
		{
			dis.itemID = nSel;
			dis.itemState = ODS_SELECTED;
			
			VERIFY(GetItemRect(nSel, &dis.rcItem));
			dis.rcItem.bottom += 2;
			
			if (CRect().IntersectRect(rClip, &dis.rcItem))
				GetParent()->SendMessage(WM_DRAWITEM, dis.CtlID, (LPARAM)&dis);
		}
	}
}

void CTabCtrlEx::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (m_dwFlags & TCE_POSTDRAW)
		return; // ignore because we probably sent it

	CXPTabCtrl::DrawItem(lpDrawItemStruct);
}

void CTabCtrlEx::OnMButtonDown(UINT nFlags, CPoint point) 
{
	CXPTabCtrl::OnMButtonDown(nFlags, point);
		
	if (m_dwFlags & TCE_MBUTTONCLICK)
	{
		TCHITTESTINFO tchi = { { point.x, point.y }, 0 };

		if (HitTest(&tchi) != -1)
		{
			m_bMBtnDown = TRUE;
			m_ptMBtnDown = point;
		}
	}
}

void CTabCtrlEx::OnMButtonUp(UINT nFlags, CPoint point) 
{
	if ((m_dwFlags & TCE_MBUTTONCLICK) && m_bMBtnDown)
	{
		int nXBorder = GetSystemMetrics(SM_CXDOUBLECLK) / 2;
		int nYBorder = GetSystemMetrics(SM_CYDOUBLECLK) / 2;

		CRect rect(m_ptMBtnDown.x - nXBorder,
					m_ptMBtnDown.y - nYBorder,
					m_ptMBtnDown.x + nXBorder,
					m_ptMBtnDown.y + nYBorder);

		if (rect.PtInRect(point))
		{
			TCHITTESTINFO tchi = { { point.x, point.y }, 0 };
			int nTab = HitTest(&tchi);

			if (nTab >= 0)
			{
				NMTCMBTNCLK tcnmh = { { *this, GetDlgCtrlID(), NM_MCLICK }, nTab, nFlags };
				GetParent()->SendMessage(WM_NOTIFY, tcnmh.hdr.idFrom, (LPARAM)&tcnmh);
			}
		}

		m_bMBtnDown = FALSE;
	}	
	
	CXPTabCtrl::OnMButtonUp(nFlags, point);
}

void CTabCtrlEx::OnCaptureChanged(CWnd *pWnd) 
{
	if ((m_dwFlags & TCE_MBUTTONCLICK) && m_bMBtnDown)
	{
		m_bMBtnDown = FALSE;
	}
	
	CXPTabCtrl::OnCaptureChanged(pWnd);
}
