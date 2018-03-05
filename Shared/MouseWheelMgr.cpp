// MouseWheelMgr.cpp: implementation of the CMouseWheelMgr class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "MouseWheelMgr.h"
#include "winclasses.h"
#include "wclassdefines.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#ifndef GET_WHEEL_DELTA_WPARAM
#	define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif // GET_WHEEL_DELTA_WPARAM

const int HORZ_LINE_SIZE = 20; // pixels

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CMouseWheelMgr::CMouseWheelMgr() : m_bShiftHorzScrollingEnabled(FALSE)
{

}

CMouseWheelMgr::~CMouseWheelMgr()
{

}

BOOL CMouseWheelMgr::Initialize(BOOL bEnableShiftHorzScrolling)
{
	if (GetInstance().InitHooks(HM_MOUSE))
	{
		GetInstance().m_bShiftHorzScrollingEnabled = bEnableShiftHorzScrolling;
		return TRUE;
	}

	// else
	return FALSE;
}

void CMouseWheelMgr::Release()
{
	GetInstance().ReleaseHooks();
}

BOOL CMouseWheelMgr::OnMouseEx(UINT uMouseMsg, const MOUSEHOOKSTRUCTEX& info)
{
	if (uMouseMsg == WM_MOUSEWHEEL)
	{
		// Normally WM_MOUSEWHEEL goes to the window with the
		// focus (info.hwnd) but we want to re-direct this message
		// to whatever window is beneath the cursor regardless of focus
		// This also means that if the window with the focus is beneath
		// the mouse then we just let Windows apply its default handling.
		HWND hwndPt = ::WindowFromPoint(info.pt);
		int zDelta = GET_WHEEL_DELTA_WPARAM(info.mouseData);

		// However this is complicated because we may also want to support
		// Shift+MouseWheel horizontal scrolling and there is no default
		// handling for this so we must handle this for all Windows.
		BOOL bShift = (GetKeyState(VK_SHIFT) & 0x8000);
		BOOL bHorzScroll = (m_bShiftHorzScrollingEnabled && bShift);

		if (bHorzScroll)
		{
			::PostMessage(hwndPt, WM_HSCROLL, (zDelta > 0 ? SB_PAGERIGHT : SB_PAGELEFT), 0L);

/*			SCROLLINFO si = { 0 };

			si.cbSize = sizeof(si);
			si.fMask = SIF_POS | SIF_RANGE;

			if (::GetScrollInfo(hwndPt, SB_HORZ, &si))
			{
				int nInc = MulDiv(zDelta, HORZ_LINE_SIZE, 120);

				if (zDelta > 0)
					nInc = min(nInc, si.nMax - si.nPos);
				else
					nInc = max(nInc, si.nMin - si.nPos);

				if (nInc)
				{
					int nResult = ::ScrollWindowEx(hwndPt, nInc, 0, NULL, NULL, NULL, NULL, SW_ERASE);
					TRACE(_T("ScrollWindowEx(%d) returned %d\n"), nInc, nResult);

					if (nResult == SIMPLEREGION || nResult == COMPLEXREGION)
					{
						// update scrollbar
						si.nPos += nInc;
						si.fMask = SIF_POS;

						::SetScrollInfo(hwndPt, SB_HORZ, &si, TRUE);

						// redraw window
						::InvalidateRect(hwndPt, NULL, FALSE);
						::UpdateWindow(hwndPt);
					}
				}
			}
*/			
			return TRUE;
		}
		else if (info.hwnd != hwndPt) // non-focus windows
		{
			// modifier keys are not reported in MOUSEHOOKSTRUCTEX
			// so we have to figure them out
			WORD wKeys = 0;

			if (bShift)
				wKeys |= MK_SHIFT;

			if (GetKeyState(VK_CONTROL) & 0x8000)
				wKeys |= MK_CONTROL;

			if (GetKeyState(VK_LBUTTON) & 0x8000)
				wKeys |= MK_LBUTTON;

			if (GetKeyState(VK_RBUTTON) & 0x8000)
				wKeys |= MK_RBUTTON;

			if (GetKeyState(VK_MBUTTON) & 0x8000)
				wKeys |= MK_MBUTTON;
			
			::PostMessage(hwndPt, WM_MOUSEWHEEL, MAKEWPARAM(wKeys, zDelta), MAKELPARAM(info.pt.x, info.pt.y));
			return TRUE; // eat
		}
		else // special window classes not natively supporting mouse wheel
		{
			CString sClass = CWinClasses::GetClass(hwndPt);
			
			if (CWinClasses::IsClass(sClass, WC_DATETIMEPICK) ||
				CWinClasses::IsClass(sClass, WC_MONTHCAL))
			{
				::PostMessage(hwndPt, WM_KEYDOWN, (zDelta > 0 ? VK_UP : VK_DOWN), 0L);
			}
		}
	}
	
	// all else
	return FALSE;
}