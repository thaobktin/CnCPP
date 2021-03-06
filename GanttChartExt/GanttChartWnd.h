#if !defined(AFX_GANTTCHARTWND_H__1571B442_7ED5_45D8_A040_C359EAE9FDE1__INCLUDED_)
#define AFX_GANTTCHARTWND_H__1571B442_7ED5_45D8_A040_C359EAE9FDE1__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// GanttChartWnd.h : header file
//

#include "GanttTreeListCtrl.h"
#include "GanttPreferencesDlg.h"

#include "..\Shared\IUIExtension.h"
#include "..\Shared\uitheme.h"
#include "..\Shared\tabbedcombobox.h"
#include "..\Shared\enstring.h"

/////////////////////////////////////////////////////////////////////////////
// CGanttChartWnd window

class CGanttChartWnd : public CDialog, public IUIExtensionWindow
{
// Construction
public:
	CGanttChartWnd(CWnd* pParent = NULL);
	virtual ~CGanttChartWnd();

	// IUIExtensionWindow
	BOOL Create(DWORD dwStyle, const RECT &rect, CWnd* pParentWnd, UINT nID);
	void Release();

	LPCTSTR GetMenuText() const { return _T("Gantt Chart"); }
	HICON GetIcon() const { return m_hIcon; }
	LPCTSTR GetTypeID() const { return GANTT_TYPEID; }

	void SetReadOnly(bool bReadOnly) {}
	HWND GetHwnd() const { return GetSafeHwnd(); }
	void SetUITheme(const UITHEME* pTheme);

	bool GetLabelEditRect(LPRECT pEdit);
	IUI_HITTEST HitTest(const POINT& ptScreen) const;

	void LoadPreferences(const IPreferences* pPrefs, LPCTSTR szKey, BOOL bAppOnly);
	void SavePreferences(IPreferences* pPrefs, LPCTSTR szKey) const;

	void UpdateTasks(const ITaskList* pTasks, IUI_UPDATETYPE nUpdate, int nEditAttribute);
	bool WantUpdate(int nAttribute) const;
	bool PrepareNewTask(ITaskList* pTask) const;

	bool SelectTask(DWORD dwTaskID);
	bool SelectTasks(DWORD* pdwTaskIDs, int nTaskCount);

	bool ProcessMessage(MSG* pMsg);

	void DoAppCommand(IUI_APPCOMMAND nCmd);
	bool CanDoAppCommand(IUI_APPCOMMAND nCmd) const;

protected:
// Dialog Data
	//{{AFX_DATA(CGanttChartWnd)
	CComboBox	m_cbSnapModes;
	CStatic	m_stDivider;
	CListCtrl	m_list;
	CTreeCtrl	m_tree;
	//}}AFX_DATA
	GTLC_MONTH_DISPLAY m_nDisplay;
	CTabbedComboBox	m_cbDisplayOptions;
	CGanttTreeListCtrl m_ctrlGantt;
	HICON m_hIcon;
	CBrush m_brBack;
	UITHEME m_theme;
	CGanttPreferencesDlg m_dlgPrefs;
	CString m_sSelectedTaskDates;
	CMap<GTLC_MONTH_DISPLAY, GTLC_MONTH_DISPLAY, GTLC_SNAPMODE, GTLC_SNAPMODE> m_mapDisplaySnapModes;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CGanttChartWnd)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL
	virtual BOOL OnInitDialog();
	virtual void OnCancel() {}
	virtual void OnOK() {}

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CGanttChartWnd)
	afx_msg void OnGototoday();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	afx_msg void OnKeyUpGantt(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangeDisplay();
	afx_msg void OnClickGanttList(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangedGanttTree(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnPreferences();
	//}}AFX_MSG
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg LRESULT OnNotifyZoomChange(WPARAM wp, LPARAM lp);
	afx_msg void OnSelchangeSnapMode();

	afx_msg LRESULT OnGanttNotifyDateChange(WPARAM wp, LPARAM lp);
	afx_msg LRESULT OnGanttNotifyDragChange(WPARAM wp, LPARAM lp);
	DECLARE_MESSAGE_MAP()

protected:
	void Resize(int cx, int cy);
	void EnableSplitting(BOOL bEnable = TRUE);
	void UpdateGanttCtrlPreferences();
	void SendParentSelectionUpdate();
	void UpdateSelectedTaskDates();
	void BuildSnapCombo();
	void UpdateMonthDisplay();
	void LoadSnapModePreference(const IPreferences* pPrefs, LPCTSTR szSnapKey, GTLC_MONTH_DISPLAY nDisplay, LPCTSTR szDisplay, GTLC_SNAPMODE nDefaultSnap);
	void SaveSnapModePreference(IPreferences* pPrefs, LPCTSTR szSnapKey, GTLC_MONTH_DISPLAY nDisplay, LPCTSTR szDisplay) const;

};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GANTTCHARTWND_H__1571B442_7ED5_45D8_A040_C359EAE9FDE1__INCLUDED_)
