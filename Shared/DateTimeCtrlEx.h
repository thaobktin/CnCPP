#if !defined(AFX_DATETIMECTRLEX_H__A8DB7BE7_A89F_4427_AD13_4743B5397314__INCLUDED_)
#define AFX_DATETIMECTRLEX_H__A8DB7BE7_A89F_4427_AD13_4743B5397314__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DateTimeCtrlEx.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDateTimeCtrlEx window

class CDateTimeCtrlEx : public CDateTimeCtrl
{
// Construction
public:
	CDateTimeCtrlEx(DWORD dwMonthCalStyle = MCS_WEEKNUMBERS);

	DWORD SetMonthCalStyle(DWORD dwStyle);
	DWORD GetMonthCalStyle() const;

// Attributes
protected:
	BOOL m_bButtonDown, m_bDropped, m_bWasSet;
	NMDATETIMECHANGE m_nmdtcFirst, m_nmdtcLast;
	DWORD m_dwMonthCalStyle;

// Overrides
protected:
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDateTimeCtrlEx)
	//}}AFX_VIRTUAL
	void PreSubclassWindow();

// Implementation
public:
	virtual ~CDateTimeCtrlEx();

	// Generated message map functions
protected:
	//{{AFX_MSG(CDateTimeCtrlEx)
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnDateTimeChange(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnCloseUp(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDropDown(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	//}}AFX_MSG
	afx_msg int OnCreate(LPCREATESTRUCT pCreate);

	DECLARE_MESSAGE_MAP()

protected:
	CRect GetDropButtonRect() const;
	void Reset();

	static BOOL IsKeyDown(UINT nVirtKey);
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DATETIMECTRLEX_H__A8DB7BE7_A89F_4427_AD13_4743B5397314__INCLUDED_)
