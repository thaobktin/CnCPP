// checkcombobox.cpp : implementation file
//

#include "stdafx.h"
#include "checkcombobox.h"
#include "misc.h"
#include "dlgunits.h"
#include "themed.h"
#include "autoflag.h"
#include "misc.h"
#include "dialoghelper.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

struct CCB_CHECK_DATA
{
public:
	BOOL m_bChecked;
	//BOOL m_bEnabled;
	DWORD m_dwUserData;

	CCB_CHECK_DATA()
	{
		m_bChecked = FALSE;
		//m_bEnabled = TRUE;
		m_dwUserData = 0;
	};
};


/////////////////////////////////////////////////////////////////////////////
// CCheckComboBox

CCheckComboBox::CCheckComboBox(DWORD dwFlags) : CAutoComboBox(dwFlags)
{
	m_bDrawing = TRUE;
	m_bTextFits = TRUE;
	m_bChecking = FALSE;
}

CCheckComboBox::~CCheckComboBox()
{
}


BEGIN_MESSAGE_MAP(CCheckComboBox, CAutoComboBox)
	//{{AFX_MSG_MAP(CCheckComboBox)
	ON_WM_CTLCOLOR()
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
	ON_WM_DESTROY()
	ON_WM_CHAR()
	ON_CONTROL_REFLECT_EX(CBN_EDITCHANGE, OnEditchange)
	ON_CONTROL_REFLECT_EX(CBN_DROPDOWN, OnDropdown)
	ON_CONTROL(LBN_SELCHANGE, 1000, OnLBSelChange)
	ON_MESSAGE(CB_GETITEMDATA, OnCBGetItemData)
	ON_MESSAGE(CB_SETITEMDATA, OnCBSetItemData)
	ON_MESSAGE(CB_DELETESTRING, OnCBDeleteString)
	ON_MESSAGE(CB_RESETCONTENT, OnCBResetContent)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCheckComboBox message handlers

void CCheckComboBox::OnDestroy()
{
	// cleanup check state memory
	DeleteAllCheckData();
}

void CCheckComboBox::DrawItemText(CDC& dc, const CRect& rect, int nItem, UINT nItemState,
								DWORD dwItemData, const CString& sItem, BOOL /*bList*/)
{
	// 0 - No check, 1 - Empty check, 2 - Checked
	int nCheck = 0;
	CRect rCheck(rect), rText(rect);

	// Check if we are drawing the static portion of the combobox
	if (nItem < 0) 
	{
		// Don't draw any boxes on this item
		nCheck = 0;
	}
	else // Otherwise it is one of the items
	{
		nCheck = 1; // unchecked

		CCB_CHECK_DATA* pState = (CCB_CHECK_DATA*)dwItemData;

		if (pState && pState->m_bChecked)
			nCheck = 2;

		rCheck.right = rText.left = CalcCheckBoxWidth(dc);
	}
	
	if (nCheck > 0) 
	{
		UINT nCheckState = DFCS_BUTTONCHECK | (nCheck > 1 ? DFCS_CHECKED : 0);

		// Draw the checkmark using DrawFrameControl
		CThemed::DrawFrameControl(ScGetCWnd(), &dc, rCheck, DFC_BUTTON, nCheckState);

		CAutoComboBox::DrawItemText(dc, rText, nItem, nItemState, dwItemData, sItem, TRUE);
	}
	else
	{
		CString sText(!sItem.IsEmpty() ? sItem : m_sText);

		if (!sText.IsEmpty())
		{
			CAutoComboBox::DrawItemText(dc, rText, nItem, nItemState, dwItemData, sText, FALSE);

			CSize sizeText = dc.GetTextExtent(sText);
			m_bTextFits = (sizeText.cx <= rText.Width());
		}
	}
}

int CCheckComboBox::CalcCheckBoxWidth(HDC hdc, HWND hwndRef)
{
	ASSERT(hdc || hwndRef);

	BOOL bFreeDC = (hdc == NULL);
	hdc = hdc ? hdc : ::GetDC(hwndRef);

	ASSERT(hdc);

	TEXTMETRIC metrics;
	GetTextMetrics(hdc, &metrics);

	if (bFreeDC)
		::ReleaseDC(hwndRef, hdc);

	return (metrics.tmHeight + metrics.tmExternalLeading + 6);
}

void CCheckComboBox::RefreshMaxDropWidth()
{
   int nMaxWidth = CDialogHelper::CalcMaxTextWidth(*this, 0, TRUE) + 18;
   SetDroppedWidth(nMaxWidth);
}

void CCheckComboBox::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == 0x01 && GetDroppedState())
		CheckAll(IsAnyUnchecked());

	CAutoComboBox::OnChar(nChar, nRepCnt, nFlags);
}

void CCheckComboBox::CheckAll(BOOL bCheck)
{
	int nCount = GetCount();

	for (int i = 0; i < nCount; i++)
		SetCheck(i, bCheck, FALSE);

	// derived classes
	OnCheckChange(-1);
	
	// update text
	RecalcText();
	
	// redraw listbox if dropped
	if (GetDroppedState())
	{
		::InvalidateRect(m_list, NULL, FALSE);
		::UpdateWindow(m_list);
	}
}

BOOL CCheckComboBox::IsAnyChecked() const
{
	int nCount = GetCount();

	for (int i = 0; i < nCount; i++)
	{
		if (GetCheck(i))
			return true;
	}

	return false;
}

BOOL CCheckComboBox::IsAnyUnchecked() const
{
	int nCount = GetCount();

	for (int i = 0; i < nCount; i++)
	{
		if (!GetCheck(i))
			return true;
	}

	return false;
}


void CCheckComboBox::RecalcText(BOOL bUpdate, BOOL bNotify)
{
	CStringArray aItems;
	GetChecked(aItems);
	
	CString sPrevText = m_sText;
	m_sText = Misc::FormatArray(aItems);
	
	// update edit field if necessary
	if (CDialogHelper::ComboHasEdit(*this))
	{
		if (bUpdate)
		{
			GetDlgItem(1001)->SetWindowText(m_sText);
				
			// notify parent
			if (bNotify && sPrevText != m_sText)
				ParentNotify(CBN_EDITCHANGE);
		}
	}
	else
	{
		SetWindowText(m_sText);
		COwnerdrawComboBoxBase::Invalidate(FALSE);
	}
}

int CCheckComboBox::SelectString(int nStartAfter, LPCTSTR lpszString)
{
   CStringArray aItems;

   if (Misc::Split(lpszString, aItems) > 1) // multiple items
   {
      SetChecked(aItems);
      return GetCurSel();
   }
   
   // else
   return CAutoComboBox::SelectString(nStartAfter, lpszString);
}

int CCheckComboBox::SetCheckByData(DWORD dwItemData, BOOL bCheck)
{
	int nIndex = CDialogHelper::FindItemByData(*this, dwItemData);

	if (nIndex == CB_ERR)
		return CB_ERR;

	// else
	return SetCheck(nIndex, bCheck, TRUE);
}

int CCheckComboBox::SetCheck(int nIndex, BOOL bCheck)
{
	return SetCheck(nIndex, bCheck, TRUE);
}

int CCheckComboBox::SetCheck(int nIndex, BOOL bCheck, BOOL bUpdate)
{
	CCB_CHECK_DATA* pState = GetAddItemCheckData(nIndex);
	ASSERT(pState);

	if (pState == NULL)
		return CB_ERR;

	pState->m_bChecked = bCheck;

	if (bUpdate && !m_bChecking) // prevent re-entrancy
	{
		CAutoFlag af(m_bChecking, TRUE);

		// derived classes
		OnCheckChange(nIndex);
		
		// update text
		RecalcText();
		
		// Redraw the window
		CComboBox::Invalidate(FALSE);
	}
	
	return CB_OKAY;
}

BOOL CCheckComboBox::GetCheck(int nIndex) const
{
	CCB_CHECK_DATA* pState = GetItemCheckData(nIndex);

	if (pState == NULL)
		return FALSE; // default

	return pState->m_bChecked;
}

BOOL CCheckComboBox::ParseText(BOOL bAutoAdd)
{
	if (IsType(CBS_DROPDOWNLIST))
		return FALSE;

	CString sEditText;
	GetDlgItem(1001)->GetWindowText(sEditText);
	
	// clear existing checks first but don't update window
	int nCount = GetCount();
	
	for (int i = 0; i < nCount; i++)
		SetCheck(i, FALSE, FALSE);
	
	// now parse the text and set the check states manually
	CStringArray aText;
	BOOL bAdded = FALSE;

	if (Misc::Split(sEditText, aText))
	{
		int nText = aText.GetSize();

		while (nText--)
		{
			const CString& sItem = Misc::GetItem(aText, nText);
			int nIndex = FindStringExact(-1, sItem);
			
			if ((nIndex == CB_ERR) && bAutoAdd)
			{
				nIndex = AddUniqueItem(sItem);
				ASSERT(nIndex != CB_ERR);

				bAdded = TRUE;
			}
			
			if (nIndex != CB_ERR)
				SetCheck(nIndex, TRUE, FALSE);
		}
	}

	if (GetDroppedState())
		ScGetCWnd()->Invalidate();
	
	// Redraw the window
	CComboBox::Invalidate(FALSE);

	return bAdded;
}

// this handles messages destined for the dropped listbox
LRESULT CCheckComboBox::ScWindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) 
	{
	// Make the combobox always return -1 as the current selection for
	// combos without an edit potion when drawing. This
	// causes the lpDrawItemStruct->itemID in DrawItem() to be -1
	// when the always-visible-portion of the combo is drawn
	case LB_GETCURSEL: 
		if (IsType(CBS_DROPDOWNLIST) && m_bDrawing)
			return -1;
		break;
				
	case WM_CHAR: // sent by the edit control (if present) or the list box
		if (wp == VK_SPACE && GetDroppedState()) 
		{
			if ((hRealWnd == GetHwnd() && Misc::KeyIsPressed(VK_CONTROL)) /*Edit*/ ||
				(hRealWnd == ScGetHwnd())) /*Listbox*/
			{
				// Get the current selection
				int nIndex = GetCurSel();
				
				CRect rcItem;
				::SendMessage(ScGetHwnd(), LB_GETITEMRECT, nIndex, (LPARAM)(LPRECT)&rcItem);
				::InvalidateRect(ScGetHwnd(), rcItem, FALSE);
				
				// Invert the check mark
				SetCheck(nIndex, !GetCheck(nIndex));

				m_bEditChange = TRUE;
				
				// Notify that selection has changed
				if (IsType(CBS_DROPDOWNLIST))
					ParentNotify(CBN_SELCHANGE);

				return 0;
			}
		}
		break;
	
	case WM_LBUTTONDOWN: 
		{
			CPoint pt(lp);
			int nItem = GetCount();

			while (nItem--)
			{
				CRect rCheck;
				::SendMessage(hRealWnd, LB_GETITEMRECT, nItem, (LPARAM)(LPRECT)rCheck);

				// check item 
				if (rCheck.PtInRect(pt))
				{
					// then check box
					//rCheck.right = rCheck.left + CalcCheckBoxWidth(NULL, hRealWnd);
					
					//if (rCheck.PtInRect(pt))
					{
						// toggle check state
						::InvalidateRect(hRealWnd, rCheck, FALSE);
						SetCheck(nItem, !GetCheck(nItem));
						m_bEditChange = TRUE;
						
						// Notify that selection has changed
						if (IsType(CBS_DROPDOWNLIST))
							ParentNotify(CBN_SELCHANGE);
					}
					
					return 0;
				}			
			}
		}
		// Do the default handling now (such as close the popup
		// window when clicked outside)
		break;
		
	case WM_LBUTTONUP: 
		{
			// Don't do anything here. This causes the combobox popup
			// windows to remain open after a selection has been made
			if (IsType(CBS_SIMPLE))
				return 0;
			else
			{
				LRESULT lr = CSubclasser::ScWindowProc(hRealWnd, msg, wp, lp);
				SetCheck(0, GetCheck(0));
				return lr;
			}
		}
		break;
	}

	return CSubclasser::ScWindowProc(hRealWnd, msg, wp, lp);
}

int CCheckComboBox::GetCurSel() const
{
	CAutoFlag af(m_bDrawing, FALSE);

	return CComboBox::GetCurSel();
}

BOOL CCheckComboBox::IsType(UINT nComboType)
{
	return ((CWnd::GetStyle() & 0xf) == nComboType);
}

int CCheckComboBox::AddUniqueItem(const CString& sItem)
{
	if (sItem.Find(Misc::GetListSeparator()) != -1)
	{
		CStringArray aText;
		
		Misc::Split(sItem, aText);
		return CAutoComboBox::AddUniqueItems(aText);
	}
	
	// else add single item
	return CAutoComboBox::AddUniqueItem(sItem);;
}

HBRUSH CCheckComboBox::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) 
{
	// hook list box before base class subclasses it
 	if (nCtlColor == CTLCOLOR_LISTBOX && !ScIsHooked())
 		ScHookWindow(pWnd->GetSafeHwnd());

	HBRUSH hbr = CAutoComboBox::OnCtlColor(pDC, pWnd, nCtlColor);
	
	return hbr;
}

BOOL CCheckComboBox::OnEditchange() 
{
	// if we generated this call as a result 
	// of an explicit call to SetCheck then eat it.
	if (m_bChecking)
		return TRUE; 

	// update check states
	if (GetDroppedState())
	{
		ParseText(FALSE);

		// update m_sText manually to point to
		// whatever has been input
		GetDlgItem(1001)->GetWindowText(m_sText);
	}

	m_bEditChange = TRUE;
	
	return FALSE; // pass to parent
}

BOOL CCheckComboBox::OnDropdown() 
{
	CAutoComboBox::OnDropDown();

	ParseText();
	RecalcText(FALSE); // updates m_sText only
	
	return FALSE; // pass to parent
}

// this handles messages destined for the embedded edit field
LRESULT CCheckComboBox::WindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	static UINT nLastMsg = 0;

	switch (msg)
	{
	case WM_KEYDOWN:
		if (wp == VK_DOWN && !GetDroppedState())
		{
			ShowDropDown();
			return 0L; // eat
		}
 		else if (wp == VK_RETURN)
 		{
			if (GetDroppedState())
				ShowDropDown(FALSE);

 			HandleReturnKey();
			return 0L; // eat
 		}
		break;

	case WM_CHAR: // if CTRL+Space then forward to listbox
		if (wp == VK_SPACE && Misc::KeyIsPressed(VK_CONTROL))
		{
			if ((GetDroppedState() && IsType(CBS_DROPDOWN)) || IsType(CBS_SIMPLE)) 
			{
				//ScGetCWnd()->SendMessage(msg, wp, lp);
				ScWindowProc(hRealWnd, msg, wp, lp);
				return 0;
			}
		}
		else if (wp == VK_ESCAPE)
		{
			m_bEditChange = FALSE;
			ShowDropDown(FALSE);
		}
		break;

	case EM_SETSEL:
		if (GetDroppedState() && (wp == 0) && (lp == (LPARAM)-1))
		{
			// avoid duplicate messages
			static LONG lLastParams = 0;
			
			// if this is a 'select all' and we're dropped then
			// assume it was in response to CTRL+A in the edit field
			if (lLastParams == MAKELONG(wp, lp))
			{
				lLastParams = 0; // duplicate
				break;
			}
			else
			{
				lLastParams = MAKELONG(wp, lp); // cache new value
				
				CheckAll(IsAnyUnchecked());

				// Notify that selection has changed
				if (IsType(CBS_DROPDOWNLIST))
					ParentNotify(CBN_SELCHANGE);

				m_bEditChange = TRUE;

				return 0; // eat it
			}
		}
		break;
	}

	// default handling
	return CAutoComboBox::WindowProc(hRealWnd, msg, wp, lp);
}

void CCheckComboBox::HandleReturnKey()
{
	m_bEditChange = FALSE;

	if (!GetDroppedState())
	{
		BOOL bSomeAdded = ParseText();
		RecalcText(FALSE); // update m_sText only

		CAutoComboBox::HandleReturnKey();

		if (bSomeAdded)		
			ParentNotifyItemAdded(0xFFFF, NULL);
	}
	else
	{
		ShowDropDown(FALSE);

		// notify parent of (possible) selection change
		ParentNotify(CBN_SELCHANGE);
	}
}

CString CCheckComboBox::GetSelectedItem() const
{
	return m_sText;
}

void CCheckComboBox::OnLBSelChange()
{
	// do nothing
}

BOOL CCheckComboBox::DeleteSelectedLBItem()
{
	if (CAutoComboBox::DeleteSelectedLBItem())
	{
		RecalcText();
		return TRUE;
	}

	// else 
	return FALSE;
}

int CCheckComboBox::GetChecked(CStringArray& aItems) const
{
	aItems.RemoveAll();

	int nCount = GetCount();
	
	for (int i = 0; i < nCount; i++)
	{
		if (GetCheck(i))
		{
			CString sItem;
			GetLBText(i, sItem);

			aItems.Add(sItem);
		}
	}	

	return aItems.GetSize();
}

CString CCheckComboBox::FormatCheckedItems(LPCTSTR szSep) const
{
	CStringArray aChecked;
	GetChecked(aChecked);

	return Misc::FormatArray(aChecked, szSep);
}

void CCheckComboBox::SetChecked(const CStringArray& aItems)
{
	// make sure the items exist in the list
	if (CAutoComboBox::AddUniqueItems(aItems) == CB_ERR)
	{
		// if nothing was added
		// check that something has actually changed
		CStringArray aCBItems;
		GetChecked(aCBItems);
		
		if (Misc::MatchAll(aCBItems, aItems, FALSE, TRUE))
		{
			// make sure window text matches checked items
			RecalcText(TRUE, FALSE);
			return; // nothing to do
		}
	}

	// clear existing checks first but don't update window
	int nCount = GetCount();
	
	for (int i = 0; i < nCount; i++)
		SetCheck(i, FALSE, FALSE);
	
	// now set the check states
	int nItem = aItems.GetSize(), nChecked = 0;
	
	while (nItem--)
	{
		const CString& sItem = Misc::GetItem(aItems, nItem);

		int nIndex = FindStringExact(-1, sItem);
		
		if (nIndex != CB_ERR)
		{
			SetCheck(nIndex, TRUE, TRUE);
			nChecked++;
		}
	}
	RecalcText(TRUE, FALSE);

	if (GetDroppedState())
		ScGetCWnd()->Invalidate();

	// Redraw the window
	CComboBox::Invalidate(FALSE);
}

void CCheckComboBox::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_DOWN && !GetDroppedState())
	{
		ShowDropDown();
		return;
	}
		
	CAutoComboBox::OnKeyDown(nChar, nRepCnt, nFlags);
}

int CCheckComboBox::GetCheckedCount() const
{
	int nCount = 0;

	for (int nItem = 0; nItem < GetCount(); nItem++)
	{
		if (GetCheck(nItem))
			nCount++;
	}

	return nCount;
}

CString CCheckComboBox::GetTooltip() const
{
	if (!m_bTextFits)
		return FormatCheckedItems(_T("\n"));

	// else
	return "";
}

LRESULT CCheckComboBox::OnCBSetItemData(WPARAM wParam, LPARAM lParam)
{
	CCB_CHECK_DATA* pState = GetAddItemCheckData(wParam);
	ASSERT(pState);

	if (pState == NULL)
		return CB_ERR;

	pState->m_dwUserData = lParam;
	return CB_OKAY;
}

LRESULT CCheckComboBox::OnCBGetItemData(WPARAM wParam, LPARAM /*lParam*/)
{
	CCB_CHECK_DATA* pState = GetItemCheckData(wParam);

	if (pState == NULL)
		return 0; // default

	return pState->m_dwUserData;
}

LRESULT CCheckComboBox::OnCBDeleteString(WPARAM wParam, LPARAM lParam)
{
	CCB_CHECK_DATA* pState = GetItemCheckData(wParam);

	LRESULT lResult = DefWindowProc(CB_DELETESTRING, wParam, lParam);

	if (lResult != CB_ERR)
		delete pState;

	return lResult;
}

void CCheckComboBox::DeleteAllCheckData()
{
	int nCount = GetCount();

	for (int i = 0; i < nCount; i++)
		delete GetItemCheckData(i);
}

LRESULT CCheckComboBox::OnCBResetContent(WPARAM wParam, LPARAM lParam)
{
	// delete check states first
	DeleteAllCheckData();

	return DefWindowProc(CB_RESETCONTENT, wParam, lParam);
}

CCB_CHECK_DATA* CCheckComboBox::GetItemCheckData(int nItem) const
{
	CCheckComboBox* pThis = const_cast<CCheckComboBox*>(this);

	LRESULT lResult = pThis->DefWindowProc(CB_GETITEMDATA, nItem, 0);
	
	if (lResult == CB_ERR)
	{
		ASSERT(0);
		lResult = 0;
	}

	return (CCB_CHECK_DATA*)lResult;
}

CCB_CHECK_DATA* CCheckComboBox::GetAddItemCheckData(int nItem)
{
	CCB_CHECK_DATA* pState = GetItemCheckData(nItem);

	if (pState == NULL)
	{
		pState = new CCB_CHECK_DATA;

		if (DefWindowProc(CB_SETITEMDATA, nItem, (LPARAM)pState) == CB_ERR)
		{
			delete pState;
			pState = NULL;
		}
	}

	return pState;
}

