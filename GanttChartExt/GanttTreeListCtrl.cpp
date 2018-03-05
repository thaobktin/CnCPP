// GanttTreeList.cpp: implementation of the CGanttTreeList class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "GanttTreeListCtrl.h"
#include "resource.h"

#include "..\shared\DialogHelper.h"
#include "..\shared\DateHelper.h"
#include "..\shared\holdredraw.h"
#include "..\shared\graphicsMisc.h"
#include "..\shared\TreeCtrlHelper.h"
#include "..\shared\autoflag.h"
#include "..\shared\misc.h"
#include "..\shared\iuiextension.h"
#include "..\shared\enstring.h"
#include "..\shared\localizer.h"
#include "..\shared\themed.h"

#include "..\3rdparty\shellicons.h"

#include "..\todolist\tdcenum.h"
#include "..\todolist\tdlschemadef.h"

#include <float.h> // for DBL_MAX
#include <math.h>  // for fabs()

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

#ifndef GET_WHEEL_DELTA_WPARAM
#	define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif 

#ifndef CDRF_SKIPPOSTPAINT
#	define CDRF_SKIPPOSTPAINT	(0x00000100)
#endif

//////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
const int MAX_YEAR = 2100;
#else
const int MAX_YEAR = 2200;
#endif

const int DEF_MONTH_WIDTH = 72;
const int TREE_TITLE_MIN_WIDTH = 200;
const int TREE_TITLE_WIDTH = 150;
const int TREE_ATTRIB_WIDTH = 75;
const int SPLITTER_WIDTH = 5;
const int COLUMN_PADDING = 15;
const int MIN_MONTH_WIDTH = 10;
const double DAY_WEEK_MULTIPLIER = 1.5;
const COLORREF NO_COLOR = (COLORREF)-1;

const double END_OF_DAY = 0.999988425923; //(((24 * 60 * 60) - 1) / (24.0 * 60 * 60));
const int MINS_IN_HOUR = 60;
const int MINS_IN_DAY = (MINS_IN_HOUR * 24);

//////////////////////////////////////////////////////////////////////

class CGanttLockUpdates
{
public:
	CGanttLockUpdates(CGanttTreeListCtrl* pCtrl, BOOL bTree, BOOL bAndSync) : m_bAndSync(bAndSync), m_pCtrl(pCtrl)
	{
		ASSERT(m_pCtrl);

		::LockWindowUpdate(bTree ? m_pCtrl->m_hwndTree : m_pCtrl->m_hwndList);

		if (m_bAndSync)
			m_pCtrl->EnableResync(FALSE);
	}

	~CGanttLockUpdates()
	{
		ASSERT(m_pCtrl);

		::LockWindowUpdate(NULL);

		if (m_bAndSync)
			m_pCtrl->EnableResync(TRUE);
	}

private:
	BOOL m_bAndSync;
	CGanttTreeListCtrl* m_pCtrl;
};

//////////////////////////////////////////////////////////////////////

enum 
{
	TREE_COLUMN_NONE = -1,
	TREE_COLUMN_TITLE,
	TREE_COLUMN_STARTDATE,
	TREE_COLUMN_ENDDATE,
	TREE_COLUMN_ALLOCTO,
};

//////////////////////////////////////////////////////////////////////

CString GANTTITEM::MILESTONE_TAG;

//////////////////////////////////////////////////////////////////////

GANTTITEM::GANTTITEM() : crText(CLR_NONE), crBack(CLR_NONE), bParent(FALSE), dwRefID(0)
{

}

GANTTITEM& GANTTITEM::operator=(const GANTTITEM& gi)
{
	sTitle = gi.sTitle;
	dtStart = gi.dtStart;
	dtStartCalc = gi.dtStartCalc;
	dtDue = gi.dtDue;
	dtDueCalc = gi.dtDueCalc;
	dtDone = gi.dtDone;
	crText = gi.crText;
	crBack = gi.crBack;
	sAllocTo = gi.sAllocTo;
	bParent = gi.bParent;
	dwRefID = gi.dwRefID;
	nPercent = gi.nPercent;
	
	aTags.Copy(gi.aTags);
	
	return (*this);
}

void GANTTITEM::MinMaxDates(const GANTTITEM& giOther)
{
	if (giOther.bParent)
	{
		dtDueCalc.m_dt = max(dtDueCalc.m_dt, giOther.dtDueCalc.m_dt);

		if (CDateHelper::IsDateSet(giOther.dtStartCalc))
		{
			dtStartCalc.m_dt = min(dtStartCalc.m_dt, giOther.dtStartCalc.m_dt);
		}
	}
	else // leaf task
	{
		dtDueCalc.m_dt = max(dtDueCalc.m_dt, giOther.dtDue.m_dt);
		
		if (giOther.HasStart())
		{
			dtStartCalc.m_dt = min(dtStartCalc.m_dt, giOther.dtStart.m_dt);
		}
	}
}

BOOL GANTTITEM::IsDone() const
{
	return CDateHelper::IsDateSet(dtDone);
}

BOOL GANTTITEM::HasStart() const
{
	return CDateHelper::IsDateSet(dtStart);
}

BOOL GANTTITEM::HasDue() const
{
	return CDateHelper::IsDateSet(dtDue);
}

BOOL GANTTITEM::IsMilestone() const
{
	if (MILESTONE_TAG.IsEmpty() || (aTags.GetSize() == 0))
		return FALSE;

	if (!bParent && !CDateHelper::IsDateSet(dtDue))
		return FALSE;
	
	if (bParent && !CDateHelper::IsDateSet(dtDueCalc))
		return FALSE;

	// else
	return (Misc::Find(aTags, MILESTONE_TAG, FALSE, FALSE) != -1);
}

COLORREF GANTTITEM::GetDefaultFillColor() const
{
	if (crBack != CLR_NONE)
		return crBack;

	if ((crText != CLR_NONE) && (crText != 0))
		return crText;

	// else
	return GetSysColor(COLOR_WINDOW);
}

COLORREF GANTTITEM::GetDefaultBorderColor() const
{
	COLORREF crDefFill = GetDefaultFillColor();

	if (crDefFill == GetSysColor(COLOR_WINDOW))
		return 0;

	// else
	return GraphicsMisc::Darker(crDefFill, 0.5);
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CGanttTreeListCtrl::CGanttTreeListCtrl() 
	:
	CTreeListSyncer(TLSF_SYNCSELECTION | TLSF_MULTISELECTION | TLSF_SYNCFOCUS),
	m_bSortAscending(-1), 
	m_nSortBy(TREE_COLUMN_NONE),
	m_pTCH(NULL),
	m_nMonthWidth(DEF_MONTH_WIDTH),
	m_nMonthDisplay(GTLC_DISPLAY_QUARTERS_LONG),
	m_dwOptions(GTLCF_AUTOSCROLLTOTASK),
	m_crAltLine(-1),
	m_nSplitWidth(TREE_TITLE_MIN_WIDTH + (3 * TREE_ATTRIB_WIDTH)),
	m_nParentColoring(GTLPC_DEFAULTCOLORING),
	m_crParent(NO_COLOR),
	m_nItemHeight(-1),
	m_dtEarliest(COleDateTime::GetCurrentTime()),
	m_dtLatest(COleDateTime::GetCurrentTime()),
	m_bDraggingStart(FALSE), 
	m_bDraggingEnd(FALSE),
	m_bDragging(FALSE),
	m_ptDragStart(0)
{

}

CGanttTreeListCtrl::~CGanttTreeListCtrl()
{
	Release();
}

BOOL CGanttTreeListCtrl::Initialize(HWND hwndTree, HWND hwndList, UINT nIDHeader, int nMinItemHeight)
{
	// misc
	m_nMonthWidth = DEF_MONTH_WIDTH;

	// initialize tree header
	if (!m_treeHeader.SubclassDlgItem(nIDHeader, CWnd::FromHandle(::GetParent(hwndTree))))
		return FALSE;

	m_treeHeader.ModifyStyle(WS_VISIBLE, HDS_BUTTONS);
	m_treeHeader.EnableTracking(FALSE);

	// keep our own handles to these to speed lookups
	m_hwndList = hwndList;
	m_hwndTree = hwndTree;

	// subclass the tree and list
	if (!CTreeListSyncer::Sync(hwndTree, hwndList, TLSL_RIGHTDATA_IS_LEFTDATA, m_treeHeader))
		return FALSE;

	// subclass the list header
	VERIFY(m_listHeader.SubclassWindow(ListView_GetHeader(hwndList)));

	// prevent translation of the list header
	CLocalizer::EnableTranslation(m_listHeader, FALSE);

	// init item height
	m_nItemHeight = min(-nMinItemHeight, -1);
	InitItemHeight();
	
	// misc
	CWnd::ModifyStyle(hwndTree, TVS_SHOWSELALWAYS, 0, 0);
	CWnd::ModifyStyle(hwndList, LVS_SHOWSELALWAYS, 0, 0);
	ListView_SetExtendedListViewStyleEx(hwndList, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

	BuildTreeColumns();
	BuildListColumns();

	CalculateMinMonthWidths();

	if (m_nMonthWidth != DEF_MONTH_WIDTH)
		RecalcListColumnWidths(DEF_MONTH_WIDTH, m_nMonthWidth);

	ExpandList();

	return TRUE;
}

void CGanttTreeListCtrl::InitItemHeight()
{
	if (m_nItemHeight < 0)
	{
		// the only reliable way to calculate the list
		// item height is to actually query for the item rect
		// so if the list has no contents we bail
		if (ListView_GetItemCount(m_hwndList) == 0)
			return;

		CRect rItem;
		VERIFY(ListView_GetItemRect(m_hwndList, 0, rItem, LVIR_BOUNDS));

		int nListItemHeight = rItem.Height();
		int nTreeItemHeight = TreeView_GetItemHeight(m_hwndTree);

		// if m_nItemHeight is not -1 then it represents
		// the negative minimum height
		int nMinItemHeight = -m_nItemHeight;

		m_nItemHeight = max(nListItemHeight, nTreeItemHeight);
		m_nItemHeight = max(m_nItemHeight, nMinItemHeight);

		if (m_nItemHeight > nTreeItemHeight)
		{
			TreeView_SetItemHeight(m_hwndTree, m_nItemHeight);
		}

		if (m_nItemHeight > nListItemHeight)
		{
			m_ilSize.Create(1, m_nItemHeight-1, ILC_COLOR, 1, 1);
			ListView_SetImageList(m_hwndList, m_ilSize, LVSIL_STATE);
		}
	}
}

void CGanttTreeListCtrl::Release() 
{ 
	if (::IsWindow(m_treeHeader))
		m_treeHeader.UnsubclassWindow();

	if (::IsWindow(m_listHeader))
		m_listHeader.UnsubclassWindow();

	Unsync(); 
	m_hwndTree = m_hwndList = NULL;

	m_ilSize.DeleteImageList();
	GraphicsMisc::VerifyDeleteObject(m_hFontDone);

	delete m_pTCH;
	m_pTCH = NULL;
}

HTREEITEM CGanttTreeListCtrl::GetSelectedItem() const
{
	return GetTreeSelItem();
}

DWORD CGanttTreeListCtrl::GetSelectedTaskID() const
{
	HTREEITEM hti = GetTreeSelItem();

	return (hti ? GetTreeItemData(GetTree(), hti) : 0);
}

BOOL CGanttTreeListCtrl::GetSelectedTaskDates(COleDateTime& dtStart, COleDateTime& dtDue) const
{
	DWORD dwTaskID = GetSelectedTaskID();

	if (dwTaskID == 0)
		return FALSE;

	GANTTITEM gi;
	
	if (GetGanttItem(dwTaskID, gi) && GetStartDueDates(gi, dtStart, dtDue))
	{
		// handle END_OF_DAY
		if (CDateHelper::GetTimeOnly(dtDue).m_dt >= END_OF_DAY)
		{
			dtDue = CDateHelper::GetDateOnly(dtDue);
		}

		return TRUE;
	}

	// all else
	ASSERT(0);
	return FALSE;
}

BOOL CGanttTreeListCtrl::SelectTask(DWORD dwTaskID)
{
	HTREEITEM hti = FindTreeItem(GetTree(), dwTaskID);

	if (hti == NULL)
		return FALSE;

	SelectTreeItem(GetTree(), hti);
	return TRUE;
}

void CGanttTreeListCtrl::RecalcParentDates()
{
	GANTTITEM dummy;
	const CTreeCtrl* pTree = (const CTreeCtrl*)CWnd::FromHandle(GetTree());

	RecalcParentDates(*pTree, NULL, dummy);
}

void CGanttTreeListCtrl::RecalcParentDates(const CTreeCtrl& tree, HTREEITEM htiParent, GANTTITEM& gi)
{
	// ignore root
	DWORD dwTaskID = 0;

	// get gantt item for this tree item
	if (htiParent)
	{
		dwTaskID = tree.GetItemData(htiParent);
		VERIFY(GetGanttItem(dwTaskID, gi));
	}

	// bail if this is a reference
	if (gi.dwRefID)
		return;

	// bail if this is not a parent
	HTREEITEM htiChild = tree.GetChildItem(htiParent);

	if (htiChild == NULL)
		return;

	// reset dates
	gi.dtStartCalc.m_dt = CDateHelper::IsDateSet(gi.dtStart) ? gi.dtStart : DBL_MAX;
	gi.dtDueCalc = gi.dtDue;

	// iterate children 
	while (htiChild)
	{
		GANTTITEM giChild;

		// recalc child if it has children itself
		RecalcParentDates(tree, htiChild, giChild);

		// keep track of earliest start date and latest due date
		gi.MinMaxDates(giChild);

		// next child
		htiChild = tree.GetNextItem(htiChild, TVGN_NEXT);
	}

	// update parent item
	if (dwTaskID)
		m_data[dwTaskID] = gi;
}

int CGanttTreeListCtrl::GetExpandedState(CDWordArray& aExpanded, HTREEITEM hti) const
{
	HWND hwndTree = GetTree();
	int nStart = 0;

	if (hti == NULL)
	{
		// guestimate initial size
		aExpanded.SetSize(0, TreeView_GetCount(hwndTree) / 4);
	}
	else if (TCH()->IsItemExpanded(hti) <= 0)
	{
		return 0; // nothing added
	}
	else // expanded
	{
		nStart = aExpanded.GetSize();
		aExpanded.Add(GetTreeItemData(hwndTree, hti));
	}

	// process children
	HTREEITEM htiChild = TreeView_GetChild(hwndTree, hti);

	while (htiChild)
	{
		GetExpandedState(aExpanded, htiChild);
		htiChild = TreeView_GetNextItem(hwndTree, htiChild, TVGN_NEXT);
	}

	return (aExpanded.GetSize() - nStart);
}

void CGanttTreeListCtrl::SetExpandedState(const CDWordArray& aExpanded)
{
	CHTIMap mapID;
	TCH()->BuildHTIMap(mapID);
	
	int nNumExpanded = aExpanded.GetSize();
	HTREEITEM hti = NULL;
	
	for (int nItem = 0; nItem < nNumExpanded; nItem++)
	{
		if (mapID.Lookup(aExpanded[nItem], hti) && hti)
			TreeView_Expand(m_hwndTree, hti, TVE_EXPAND);
	}
	
	if (nNumExpanded)
		ExpandList();
}

void CGanttTreeListCtrl::UpdateTasks(const ITaskList* pTasks, IUI_UPDATETYPE nUpdate, int nEditAttribute)
{
	// we must have been initialized already
	ASSERT(GetList() && GetTree());

	const ITaskList12* pTasks12 = GetITLInterface<ITaskList12>(pTasks, IID_TASKLIST12);
	
	switch (nUpdate)
	{
	case IUI_ALL:
		{
			CGanttLockUpdates glu(this, TRUE, TRUE);
			
			CDWordArray aExpanded;
			GetExpandedState(aExpanded);
			
			DWORD dwSelID = GetSelectedTaskID();
			
			RebuildTree(pTasks12);
			UpdateListColumns();
			
			SetExpandedState(aExpanded);
			ScrollToToday();
			SelectTask(dwSelID);
		}
		break;
		
	case IUI_EDIT:
		{
			CHoldRedraw hr(GetTree());
			CHoldRedraw hr2(GetList());
			
			// cache current year range to test for changes
			int nNumMonths = GetNumMonths();
			
			// update the task(s)
			UpdateTask(pTasks12, pTasks12->GetFirstTask(), nUpdate, nEditAttribute);
			
			// recalc parent dates as required
			if (nEditAttribute == TDCA_STARTDATE || nEditAttribute == TDCA_DUEDATE)
				RecalcParentDates();
			
			// fixup list columns as required
			if (GetNumMonths() > nNumMonths)
				UpdateListColumns();
		}
		break;
		
	case IUI_DELETE:
		{
			CHoldRedraw hr(GetTree());
			CHoldRedraw hr2(GetList());
			
			RemoveDeletedTasks(NULL, pTasks12);
		}
		break;
		
	case IUI_ADD:
	case IUI_MOVE:
		ASSERT(0);
		break;
		
	default:
		ASSERT(0);
	}
	
	// init tree/list item heights if not yet done
	if (m_nItemHeight < 0)
		InitItemHeight();
}

CString CGanttTreeListCtrl::GetTaskAllocTo(const ITaskList12* pTasks, HTASKITEM hTask)
{
	int nAllocTo = pTasks->GetTaskAllocatedToCount(hTask);
	
	if (nAllocTo == 0)
	{
		return _T("");
	}
	else if (nAllocTo == 1)
	{
		return pTasks->GetTaskAllocatedTo(hTask, 0);
	}
	
	// nAllocTo > 1 
	CStringArray aAllocTo;
	
	while (nAllocTo--)
		aAllocTo.InsertAt(0, pTasks->GetTaskAllocatedTo(hTask, nAllocTo));
	
	return Misc::FormatArray(aAllocTo);
}

void CGanttTreeListCtrl::UpdateTask(const ITaskList12* pTasks, HTASKITEM hTask, IUI_UPDATETYPE nUpdate, int nEditAttribute)
{
	if (hTask == NULL)
		return;

	ASSERT(nUpdate == IUI_EDIT);

	// handle task if not NULL (== root)
	DWORD dwTaskID = pTasks->GetTaskID(hTask);
	time64_t tDate = 0;
	
	GANTTITEM gi;
	VERIFY(GetGanttItem(dwTaskID, gi));

	// update taskID to refID 
	if (gi.dwRefID)
	{
		dwTaskID = gi.dwRefID;
		gi.dwRefID = 0;
	}

	switch (nEditAttribute)
	{
	case TDCA_TASKNAME:
		gi.sTitle = pTasks->GetTaskTitle(hTask);
		break;
		
	case TDCA_STARTDATE:
		if (pTasks->GetTaskStartDate64(hTask, gi.bParent, tDate))
			gi.dtStart = GetDate(tDate, FALSE);
		break;
		
	case TDCA_DUEDATE:
		if (pTasks->GetTaskDueDate64(hTask, gi.bParent, tDate))
			gi.dtDue = GetDate(tDate, TRUE);
		break;
		
	case TDCA_DONEDATE:
		if (pTasks->GetTaskDoneDate64(hTask, tDate))
			gi.dtDone = GetDate(tDate, TRUE);
		break;
		
	case TDCA_ALLOCTO:
		gi.sAllocTo = GetTaskAllocTo(pTasks, hTask);
		break;
		
	case TDCA_PERCENT:
		gi.nPercent = pTasks->GetTaskPercentDone(hTask, TRUE);
		break;
		
	case TDCA_TAGS:
		{
			int nTag = pTasks->GetTaskTagCount(hTask);
			gi.aTags.RemoveAll();

			while (nTag--)
				gi.aTags.Add(pTasks->GetTaskTag(hTask, nTag));
		}
		break;
		
	case TDCA_COLOR:
		// handled below
		break;
		
	default:
		ASSERT(0);
	}

	// update date range
	MinMaxDates(gi);
	
	// always update colour because it can change for
	// so many reasons
	gi.crText = pTasks->GetTaskTextColor(hTask);
	gi.crBack = pTasks->GetTaskBkgndColor(hTask);
	
	// update data
	m_data[dwTaskID] = gi;

	// clear display props
	m_display.RemoveKey(dwTaskID);
	
	// next sibling
	UpdateTask(pTasks, pTasks->GetNextTask(hTask), nUpdate, nEditAttribute);
	
	// children
	UpdateTask(pTasks, pTasks->GetFirstTask(hTask), nUpdate, nEditAttribute);
}

void CGanttTreeListCtrl::RemoveDeletedTasks(HTREEITEM hti, const ITaskList12* pTasks)
{
	// traverse the tree looking for items that do not 
	// exist in pTasks and delete them
	if (hti)
	{
		DWORD dwTaskID = GetTreeItemData(GetTree(), hti);
		HTASKITEM hTask = pTasks->FindTask(dwTaskID);

		if (hTask == NULL)
		{
			DeleteTreeItem(hti);
			return;
		}
	}

	// check its children
	HTREEITEM htiChild = TreeView_GetChild(GetTree(), hti);
	
	while (htiChild)
	{
		// get next sibling before we (might) delete this one
		HTREEITEM htiNext = TreeView_GetNextItem(GetTree(), htiChild, TVGN_NEXT);
		
		RemoveDeletedTasks(htiChild, pTasks);
		htiChild = htiNext;
	}
}

BOOL CGanttTreeListCtrl::GetGanttItem(DWORD dwTaskID, GANTTITEM& gi, BOOL bCopyRefID) const
{
	if (dwTaskID && m_data.Lookup(dwTaskID, gi))
	{
		// handle reference tasks
		DWORD dwRefID = gi.dwRefID;

		if (dwRefID && m_data.Lookup(dwRefID, gi))
		{
			// copy over the reference id so that the caller can still detect it
			if (bCopyRefID)
			{
				ASSERT(gi.dwRefID == 0);
				gi.dwRefID = dwRefID;
			}
		}

		return TRUE;
	}

	return FALSE;
}

BOOL CGanttTreeListCtrl::GetGanttDisplay(DWORD dwTaskID, GANTTDISPLAY& gd) const
{
	return m_display.Lookup(dwTaskID, gd);
}

void CGanttTreeListCtrl::RebuildTree(const ITaskList12* pTasks)
{
	CTreeCtrl* pTree = (CTreeCtrl*)CWnd::FromHandle(GetTree());

	TreeView_DeleteAllItems(GetTree());
	ListView_DeleteAllItems(GetList());

	m_data.RemoveAll();
	TCH()->ResetIndexMap();

	// cache and reset year range which will get 
	// recalculated as we build the tree
	COleDateTime dtPrevEarliest = m_dtEarliest, dtPrevLatest = m_dtLatest;

	CDateHelper::ClearDate(m_dtEarliest);
	CDateHelper::ClearDate(m_dtLatest);

	BuildTreeItem(pTasks, pTasks->GetFirstTask(), *pTree, NULL);

	// restore previous date range if no data
	if (m_data.GetCount() == 0)
	{
		m_dtEarliest = dtPrevEarliest;
		m_dtLatest = dtPrevLatest;
	}

	RecalcParentDates();
	ExpandList();
}

COleDateTime CGanttTreeListCtrl::GetDate(time64_t tDate, BOOL bEndOfDay)
{
	COleDateTime date;

	if (tDate > 0)
	{
		date = CDateHelper::GetDate(tDate);

		// only implement 'end of day' if the date has no time
		if (CDateHelper::IsDateSet(date) && bEndOfDay && !CDateHelper::DateHasTime(date))
		{
			date.m_dt += END_OF_DAY;
		}
	}

	return date;
}

void CGanttTreeListCtrl::BuildTreeItem(const ITaskList12* pTasks, HTASKITEM hTask, CTreeCtrl& tree, HTREEITEM htiParent)
{
	if (hTask == NULL)
		return;

	// map the data
	GANTTITEM gi;
	time64_t tDate = 0;
	
	gi.dwRefID = _ttoi(pTasks->GetTaskAttribute(hTask, TDL_TASKREFID));

	if (gi.dwRefID == 0) // 'true' task
	{
		gi.sTitle = pTasks->GetTaskTitle(hTask);
		gi.crText = pTasks->GetTaskTextColor(hTask);
		gi.crBack = pTasks->GetTaskBkgndColor(hTask);
		gi.sAllocTo = GetTaskAllocTo(pTasks, hTask);
		gi.bParent = (pTasks->GetFirstTask(hTask) != NULL);
		gi.nPercent = pTasks->GetTaskPercentDone(hTask, TRUE);

		if (pTasks->GetTaskStartDate64(hTask, gi.bParent, tDate))
			gi.dtStart = GetDate(tDate, FALSE);

		if (pTasks->GetTaskDueDate64(hTask, gi.bParent, tDate))
			gi.dtDue = GetDate(tDate, TRUE);

		if (pTasks->GetTaskDoneDate64(hTask, tDate))
			gi.dtDone = GetDate(tDate, TRUE);

		int nTag = pTasks->GetTaskTagCount(hTask);

		while (nTag--)
			gi.aTags.Add(pTasks->GetTaskTag(hTask, nTag));

		// track earliest and latest dates
		MinMaxDates(gi);
	}
	
	DWORD dwTaskID = pTasks->GetTaskID(hTask);
	m_data[dwTaskID] = gi;
	
	// add item to tree
	HTREEITEM hti = tree.InsertItem(LPSTR_TEXTCALLBACK, htiParent);
	tree.SetItemData(hti, dwTaskID);
	
	// add first sibling which will add all the rest
	BuildTreeItem(pTasks, pTasks->GetNextTask(hTask), tree, htiParent);
	
	// add first child which will add all the rest
	BuildTreeItem(pTasks, pTasks->GetFirstTask(hTask), tree, hti);
}

void CGanttTreeListCtrl::MinMaxDates(const GANTTITEM& gi)
{
	MinMaxDates(gi.dtStart);
	MinMaxDates(gi.dtDue);
	MinMaxDates(gi.dtDone);
}

void CGanttTreeListCtrl::MinMaxDates(const COleDateTime& date)
{
	if (CDateHelper::IsDateSet(date))
	{
		if (CDateHelper::IsDateSet(m_dtEarliest))
			m_dtEarliest = min(m_dtEarliest, date);
		else
			m_dtEarliest = date;

		if (CDateHelper::IsDateSet(m_dtLatest))
			m_dtLatest = max(m_dtLatest, date);
		else
			m_dtLatest = date;
	}
}

int CGanttTreeListCtrl::GetStartYear() const
{
	return m_dtEarliest.GetYear();
}

int CGanttTreeListCtrl::GetEndYear() const
{
	// for now, do not let end year exceed MAX_YEAR
	int nYear = m_dtLatest.GetYear();

	return min(nYear, MAX_YEAR);
}

int CGanttTreeListCtrl::GetNumMonths() const
{
	return ((GetEndYear() - GetStartYear() + 1) * 12);
}

void CGanttTreeListCtrl::SetOption(DWORD dwOption, BOOL bSet)
{
	if (dwOption)
	{
		DWORD dwPrev = m_dwOptions;

		if (bSet)
			m_dwOptions |= dwOption;
		else
			m_dwOptions &= ~dwOption;

		// specific handling
		if (m_dwOptions != dwPrev)
		{
			if (dwOption == GTLCF_DISPLAYALLOCTOCOLUMN)
			{
				Resize(m_rect);
			}
			else if (IsSyncing())
			{
				m_display.RemoveAll();

				RedrawList();
			}
		}
	}
}

void CGanttTreeListCtrl::AddListColumn(int nMonth, int nYear)
{
	ASSERT(nMonth >= 1 && nMonth <= 12 && nYear > 1900);

	LVCOLUMN lvc = { LVCF_FMT | LVCF_TEXT | LVCF_WIDTH, 0 };

	lvc.cx = GetColumnWidth(m_nMonthDisplay);
	lvc.fmt = LVCFMT_CENTER;

	CString sTitle = FormatColumnHeaderText(m_nMonthDisplay, nMonth, nYear);

	lvc.pszText = (LPTSTR)(LPCTSTR)sTitle;
	lvc.cchTextMax = sTitle.GetLength() + 1;

	int nCol = m_listHeader.GetItemCount();
	HWND hwndList = GetList();

	ListView_InsertColumn(hwndList, nCol, &lvc);

	// encode month and year into header item data
	SetListColumnDate(nCol, nMonth, nYear);
}

void CGanttTreeListCtrl::SetListColumnDate(int nCol, int nMonth, int nYear)
{
	ASSERT(nMonth >= 1 && nMonth <= 12 && nYear > 1900);

	// encode month and year into header item data
	m_listHeader.SetItemData(nCol, MAKELONG(nMonth, nYear));
}

CString CGanttTreeListCtrl::FormatColumnHeaderText(GTLC_MONTH_DISPLAY nDisplay, int nMonth, int nYear)
{
	if (nMonth == 0)
	{
// #ifdef _DEBUG
// 		return _T("List Item");
// #else
		return _T("");
//#endif
	}
	
	//else
	CString sHeader;
	
	switch (nDisplay)
	{
	case GTLC_DISPLAY_YEARS:
		sHeader.Format(_T("%d"), nYear);
		break;
		
	case GTLC_DISPLAY_QUARTERS_SHORT:
		sHeader.Format(_T("Q%d %d"), (1 + ((nMonth-1) / 3)), nYear);
		break;
		
	case GTLC_DISPLAY_QUARTERS_MID:
		sHeader.Format(_T("%s-%s %d"), CDateHelper::GetMonthName(nMonth, TRUE),
			CDateHelper::GetMonthName(nMonth+2, TRUE), nYear);
		break;
		
	case GTLC_DISPLAY_QUARTERS_LONG:
		sHeader.Format(_T("%s-%s %d"), CDateHelper::GetMonthName(nMonth, FALSE),
			CDateHelper::GetMonthName(nMonth+2, FALSE), nYear);
		break;
		
	case GTLC_DISPLAY_MONTHS_SHORT:
		sHeader.Format(_T("%02d/%d"), nMonth, (nYear%1000));
		//sHeader.Format(_T("%s %02d"), CDateHelper::GetMonthName(nMonth, TRUE).Left(1), nYear);
		break;
		
	case GTLC_DISPLAY_MONTHS_MID:
		sHeader.Format(_T("%s %d"), CDateHelper::GetMonthName(nMonth, TRUE), nYear);
		break;
		
	case GTLC_DISPLAY_MONTHS_LONG:
		sHeader.Format(_T("%s %d"), CDateHelper::GetMonthName(nMonth, FALSE), nYear);
		break;

	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
		sHeader.Format(_T("%s %d (%s)"), CDateHelper::GetMonthName(nMonth, FALSE), nYear, CEnString(IDS_GANTT_WEEKS));
		break;

	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		sHeader.Format(_T("%s %d (%s)"), CDateHelper::GetMonthName(nMonth, FALSE), nYear, CEnString(IDS_GANTT_DAYS));
		break;
		
	default:
		ASSERT(0);
		break;
	}

	return sHeader;
}

int CGanttTreeListCtrl::GetMonthWidth(int nColWidth) const
{
	switch (m_nMonthDisplay)
	{
	case GTLC_DISPLAY_YEARS:
		return (nColWidth / 12);
		
	case GTLC_DISPLAY_QUARTERS_SHORT:
	case GTLC_DISPLAY_QUARTERS_MID:
	case GTLC_DISPLAY_QUARTERS_LONG:
		return (nColWidth / 3);
		
	case GTLC_DISPLAY_MONTHS_SHORT:
	case GTLC_DISPLAY_MONTHS_MID:
	case GTLC_DISPLAY_MONTHS_LONG:
		// fall thru

	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
		// fall thru

	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		return nColWidth;
		
	default:
		ASSERT(0);
		break;
	}

	return 0;
}

int CGanttTreeListCtrl::GetRequiredColumnCount() const
{
	switch (m_nMonthDisplay)
	{
	case GTLC_DISPLAY_YEARS:
		return (GetNumMonths() / 12);
		
	case GTLC_DISPLAY_QUARTERS_SHORT:
	case GTLC_DISPLAY_QUARTERS_MID:
	case GTLC_DISPLAY_QUARTERS_LONG:
		return (GetNumMonths() / 3);
		break;
		
	case GTLC_DISPLAY_MONTHS_SHORT:
	case GTLC_DISPLAY_MONTHS_MID:
	case GTLC_DISPLAY_MONTHS_LONG:
		// fall thru

	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
		// fall thru

	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		return GetNumMonths();
		
	default:
		ASSERT(0);
		break;
	}

	return 0;
}

int CGanttTreeListCtrl::GetColumnWidth() const
{
	return GetColumnWidth(m_nMonthDisplay, m_nMonthWidth);
}

int CGanttTreeListCtrl::GetColumnWidth(GTLC_MONTH_DISPLAY nDisplay) const
{
	return GetColumnWidth(nDisplay, m_nMonthWidth);
}

int CGanttTreeListCtrl::GetColumnWidth(GTLC_MONTH_DISPLAY nDisplay, int nMonthWidth)
{
	int nColWidth = 0;

	switch (nDisplay)
	{
	case GTLC_DISPLAY_YEARS:
		nColWidth = nMonthWidth * 12;
		break;
		
	case GTLC_DISPLAY_QUARTERS_SHORT:
	case GTLC_DISPLAY_QUARTERS_MID:
	case GTLC_DISPLAY_QUARTERS_LONG:
		nColWidth = nMonthWidth * 3;
		break;
		
	case GTLC_DISPLAY_MONTHS_SHORT:
	case GTLC_DISPLAY_MONTHS_MID:
	case GTLC_DISPLAY_MONTHS_LONG:
		// fall thru

	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
		// fall thru

	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		nColWidth = nMonthWidth;
		break;
		
	default:
		ASSERT(0);
		break;
	}

	return nColWidth;
}

BOOL CGanttTreeListCtrl::GetListColumnDate(int nCol, int& nMonth, int& nYear) const
{
	ASSERT (nCol > 0);
	nMonth = nYear = 0;
	
	if (nCol >= 1)
	{
		DWORD dwData = m_listHeader.GetItemData(nCol);

		nMonth = LOWORD(dwData);
		nYear = HIWORD(dwData);
	}

	return (nMonth >= 1 && nMonth <= 12 && nYear > 1900);
}

void CGanttTreeListCtrl::BuildTreeColumns()
{
	// delete existing columns
	while (m_treeHeader.DeleteItem(0));

	// add columns
	HDITEM  hdi = { HDI_TEXT | HDI_WIDTH | HDI_FORMAT, 0 };

	hdi.cchTextMax = 10;
	hdi.fmt = HDF_LEFT | HDF_STRING;
	hdi.cxy = TREE_TITLE_MIN_WIDTH;
	hdi.pszText = _T("Task");
	
	m_treeHeader.InsertItem(TREE_COLUMN_TITLE, &hdi); 

	hdi.cxy = TREE_ATTRIB_WIDTH;
	hdi.pszText = _T("Start");
	m_treeHeader.InsertItem(TREE_COLUMN_STARTDATE, &hdi); 

	hdi.cxy = TREE_ATTRIB_WIDTH;
	hdi.pszText = _T("Due");
	m_treeHeader.InsertItem(TREE_COLUMN_ENDDATE, &hdi); 

	hdi.cxy = TREE_ATTRIB_WIDTH;
	hdi.pszText = _T("Alloc To");
	m_treeHeader.InsertItem(TREE_COLUMN_ALLOCTO, &hdi); 
}

CTreeCtrlHelper* CGanttTreeListCtrl::TCH()
{
	if (m_pTCH == NULL) // first time init
	{
		CTreeCtrl* pTree = (CTreeCtrl*)CWnd::FromHandle(GetTree());
		m_pTCH = new CTreeCtrlHelper(*pTree);
	}

	ASSERT(m_pTCH);
	return m_pTCH;
}

const CTreeCtrlHelper* CGanttTreeListCtrl::TCH() const
{
	if (m_pTCH == NULL) // first time init
	{
		CTreeCtrl* pTree = (CTreeCtrl*)CWnd::FromHandle(GetTree());
		m_pTCH = new CTreeCtrlHelper(*pTree);
	}

	ASSERT(m_pTCH);
	return m_pTCH;
}

BOOL CGanttTreeListCtrl::IsTreeItemLineOdd(HTREEITEM hti)
{
	return TCH()->ItemLineIsOdd(hti);
}

void CGanttTreeListCtrl::SetFocus()
{
	if (!HasFocus())
		::SetFocus(m_hwndTree);
}

void CGanttTreeListCtrl::Resize(const CRect& rect)
{
	m_rect = rect;

	// we draw a border around the controls so deflate a bit
	if (HasOption(GTLCF_ENABLESPLITTER))
	{
	    CRect rLeft(rect), rRight(rect);
	
		rLeft.right = rLeft.left + m_nSplitWidth;
		rRight.left = rLeft.right + SPLITTER_WIDTH;

		rLeft.DeflateRect(1, 1);
		rRight.DeflateRect(1, 1);

		CTreeListSyncer::Resize(rLeft, rRight);
	}
	else 
	{
		CRect rGantt(rect);
		rGantt.DeflateRect(1, 1);

		CTreeListSyncer::Resize(rGantt, RecalcTreeWidth());
	}
}

BOOL CGanttTreeListCtrl::HandleEraseBkgnd(CDC* pDC) 
{ 
	CTreeListSyncer::HandleEraseBkgnd(pDC); 

	// draw borders as required, excluding controls first
	CWnd* pParent = m_treeHeader.GetParent();

	CDialogHelper::ExcludeChild(pParent, pDC, m_scLeft.GetCWnd(), TRUE);
	CDialogHelper::ExcludeChild(pParent, pDC, m_scRight.GetCWnd(), TRUE);
	CDialogHelper::ExcludeChild(pParent, pDC, &m_treeHeader, TRUE);

	if (HasOption(GTLCF_ENABLESPLITTER))
	{
		// draw separate borders around tree and list
		CRect rLeft(m_rect), rRight(m_rect);

		rLeft.right = rLeft.left + m_nSplitWidth;
		rRight.left = rLeft.right + SPLITTER_WIDTH;

		// fill and exclude rect
		pDC->FillSolidRect(rLeft, GetSysColor(COLOR_3DSHADOW));
		pDC->ExcludeClipRect(rLeft);

		pDC->FillSolidRect(rRight, GetSysColor(COLOR_3DSHADOW));
		pDC->ExcludeClipRect(rRight);
	}
	else // single border
	{
		pDC->FillSolidRect(m_rect, GetSysColor(COLOR_3DSHADOW));
		pDC->ExcludeClipRect(m_rect);
	}

	return TRUE;
}

void CGanttTreeListCtrl::ExpandAll(BOOL bExpand)
{
	ExpandItem(NULL, bExpand, TRUE);
}

void CGanttTreeListCtrl::ExpandItem(HTREEITEM hti, BOOL bExpand, BOOL bAndChildren)
{
	// avoid unnecessary processing
	if (hti && !CanExpandItem(hti, bExpand))
		return;

	CAutoFlag af(m_bTreeExpanding, TRUE);

	TCH()->ResetIndexMap();
	EnableResync(FALSE);

	CHoldRedraw hr(GetList());
	CHoldRedraw hr2(GetTree());

	TCH()->ExpandItem(hti, bExpand, bAndChildren);

	if (bExpand)
	{
		if (hti)
		{
			int nNextIndex = GetListItem(hti) + 1;
			ExpandList(hti, nNextIndex);
		}
		else
			ExpandList(); // all
	}
	else
	{
		if (hti)
			CollapseList(hti);
		else
			CollapseList();
	}

	TreeView_EnsureVisible(GetTree(), TreeView_GetChild(GetTree(), NULL));

	EnableResync(TRUE, GetTree());
}

BOOL CGanttTreeListCtrl::CanExpandItem(HTREEITEM hti, BOOL bExpand) const
{
	int nFullyExpanded = TCH()->IsItemExpanded(hti, TRUE);
			
	if (nFullyExpanded == -1)	// item has no children
	{
		return FALSE; // can neither expand nor collapse
	}
			
	if (bExpand)
		return !nFullyExpanded;
			
	// else
	return TCH()->IsItemExpanded(hti, FALSE);
}

LRESULT CGanttTreeListCtrl::OnCustomDrawTree(HWND hwndTree, NMTVCUSTOMDRAW* pTVCD)
{
	ASSERT(IsTree(hwndTree));

	HTREEITEM hti = (HTREEITEM)pTVCD->nmcd.dwItemSpec;
	
	switch (pTVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;
								
	case CDDS_ITEMPREPAINT:
		{
			CDC* pDC = CDC::FromHandle(pTVCD->nmcd.hdc);
			BOOL bSel = IsTreeItemSelected(hwndTree, hti);
			BOOL bFullRow = IsTreeFullRowSelect(hwndTree);
			BOOL bAlternate = (HasAltLineColor() && IsTreeItemLineOdd(hti));

			GANTTITEM gi;
			VERIFY (GetGanttItem(pTVCD->nmcd.lItemlParam, gi));

			// handle strikethru completed items
			if (m_hFontDone && CDateHelper::IsDateSet(gi.dtDone))
			{
				::SelectObject(pTVCD->nmcd.hdc, m_hFontDone);
			}

			// text colors for default tree drawing
			pTVCD->clrText = GetTreeTextColor(gi, bSel, bFullRow, TRUE);
			pTVCD->clrTextBk = GetTreeTextBkColor(gi, bSel, bAlternate, bFullRow, TRUE);

			// special background handling when not full row because:
			if (!bFullRow)
			{
				CRect rItem;
				GetTreeItemRect(hti, -1, rItem);

				// 1. the extra tree columns must be filled to avoid 
				// artifacts with clear-type
				COLORREF crOldColor = pDC->GetBkColor();
				COLORREF crBkColor = GetTreeTextBkColor(gi, bSel, bAlternate, FALSE, FALSE);

				rItem.left = GetTreeTitleWidth();
				pDC->FillSolidRect(rItem, crBkColor);

				// 2. alternate line colour needs to fill the whole
				// of the first column
				if (bAlternate)
				{
					GetTreeItemRect(hti, -1, rItem);
					rItem.right = GetTreeTitleWidth();

					pDC->FillSolidRect(rItem, m_crAltLine);
				}

				// cleanup
				pDC->SetBkColor(crOldColor);
			}

			return (CDRF_NOTIFYPOSTPAINT | CDRF_NEWFONT);
		}
		break;
								
	case CDDS_ITEMPOSTPAINT:
		{
			// check row is visible
			CRect rItem;
			GetTreeItemRect(hti, -1, rItem);

			CRect rClient;
			::GetClientRect(hwndTree, rClient);
			
			if (!(rItem.bottom <= 0 || rItem.top >= rClient.bottom))
			{
				CDC* pDC = CDC::FromHandle(pTVCD->nmcd.hdc);
				BOOL bSel = IsTreeItemSelected(hwndTree, hti);
				BOOL bFullRow = IsTreeFullRowSelect(hwndTree);
				
				// draw horz gridline
				if (bFullRow)
				{
					if (!bSel)
						DrawItemDivider(pDC, rItem, FALSE, FALSE, NULL);
				}
				else 
				{
					CRect rFocus;

					if (bSel)
						GetTreeItemRect(hti, 0, rFocus, TRUE);

					DrawItemDivider(pDC, rItem, FALSE, FALSE, bSel ? &rFocus : NULL);
				}

				// draw gantt item attributes
				GANTTITEM gi;
				DWORD dwTaskID = pTVCD->nmcd.lItemlParam;

				VERIFY(GetGanttItem(dwTaskID, gi));
				
				//TRACE(_T("CGanttTreeListCtrl::OnCustomDrawTree(%s)\n"), gi.sTitle);
				DrawTreeItem(pDC, hti, TREE_COLUMN_TITLE, gi);
				DrawTreeItem(pDC, hti, TREE_COLUMN_STARTDATE, gi);
				DrawTreeItem(pDC, hti, TREE_COLUMN_ENDDATE, gi);
				DrawTreeItem(pDC, hti, TREE_COLUMN_ALLOCTO, gi);
			}			
	
			return CDRF_SKIPDEFAULT;
		}
		break;
	}

	return 0;
}

LRESULT CGanttTreeListCtrl::OnCustomDrawList(HWND hwndList, NMLVCUSTOMDRAW* pLVCD)
{
	ASSERT(IsList(hwndList));

	int nItem = (int)pLVCD->nmcd.dwItemSpec;
	
	switch (pLVCD->nmcd.dwDrawStage)
	{
	case CDDS_PREPAINT:
		return CDRF_NOTIFYITEMDRAW;
								
	case CDDS_ITEMPREPAINT:
		{
			LRESULT lr = CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;

			// paint alternate lines
			if (HasAltLineColor() && !IsListItemSelected(hwndList, nItem) && (nItem % 2))
			{
				pLVCD->clrTextBk = m_crAltLine;
				pLVCD->clrText = GetSysColor(COLOR_WINDOWTEXT);
				
				lr |= CDRF_NEWFONT;
			}

			return lr;
		}
		break;
								
	case (CDDS_ITEMPREPAINT | CDDS_SUBITEM):
		return CDRF_NOTIFYPOSTPAINT;
								
	case CDDS_ITEMPOSTPAINT:
		if (nItem < ListView_GetItemCount(hwndList))
		{
			CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
			PostDrawListItem(pDC, nItem, pLVCD->nmcd.lItemlParam);
		}
		break;
								
	case (CDDS_ITEMPOSTPAINT | CDDS_SUBITEM):
		if (nItem < ListView_GetItemCount(hwndList))
		{
			CDC* pDC = CDC::FromHandle(pLVCD->nmcd.hdc);
			DrawListItem(pDC, nItem, pLVCD->iSubItem, pLVCD->nmcd.lItemlParam);

			return CDRF_SKIPDEFAULT;
		}
		break;
	}

	return 0;
}

LRESULT CGanttTreeListCtrl::OnCustomDrawListHeader(NMCUSTOMDRAW* pNMCD)
{
	
	switch (pNMCD->dwDrawStage)
	{
	case CDDS_PREPAINT:
		// only need handle drawing for double row height
		if (m_listHeader.GetRowCount() > 1)
			return CDRF_NOTIFYITEMDRAW;
						
	case CDDS_ITEMPREPAINT:
		// only need handle drawing for double row height
		if (m_listHeader.GetRowCount() > 1)
		{
			CDC* pDC = CDC::FromHandle(pNMCD->hdc);
			int nItem = (int)pNMCD->dwItemSpec;

			DrawListHeaderItem(pDC, nItem);

			return CDRF_SKIPDEFAULT;
		}
		break;
	}

	return CDRF_DODEFAULT;
}

void CGanttTreeListCtrl::Resort(BOOL bAllowToggle)
{
	if (IsSorted())
	{
		// toggle direction
		if (bAllowToggle)
		{
			if (m_bSortAscending == -1)
				m_bSortAscending = 1;
			else
				m_bSortAscending = !m_bSortAscending;
		}

		CHoldRedraw hr(GetTree());
		TCH()->ResetIndexMap();

		CTreeListSyncer::Sort(SortProc, (DWORD)this);
	}
}

BOOL CGanttTreeListCtrl::IsSorted() const
{
	return (m_nSortBy != TREE_COLUMN_NONE);
}

LRESULT CGanttTreeListCtrl::WindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!m_bResyncEnabled)
		return CTreeListSyncer::WindowProc(hRealWnd, msg, wp, lp);

	ASSERT(hRealWnd == GetHwnd());
	
	// only interested in notifications from the tree/list pair to their parent
	if (wp == m_scLeft.GetDlgCtrlID() || 
		wp == m_scRight.GetDlgCtrlID() ||
		wp == (UINT)m_treeHeader.GetDlgCtrlID() ||
		wp == (UINT)m_listHeader.GetDlgCtrlID())
	{
		switch (msg)
		{
		case WM_NOTIFY:
			{
				LPNMHDR pNMHDR = (LPNMHDR)lp;
				HWND hwnd = pNMHDR->hwndFrom;
				
				// let base class have its turn first
				// except for expanding tree items
				LRESULT lr = 0;
				
				if (pNMHDR->code != TVN_ITEMEXPANDING && pNMHDR->code != TVN_ITEMEXPANDED)
					lr = CTreeListSyncer::WindowProc(hRealWnd, msg, wp, lp);

				// our extra handling
				switch (pNMHDR->code)
				{
				case HDN_ITEMCLICK:
					if (hwnd == m_treeHeader)
					{
						HD_NOTIFY *phdn = (HD_NOTIFY *) pNMHDR;
						
						if (phdn->iItem == m_nSortBy) // toggle direction
						{
							if (m_bSortAscending == -1)
								m_bSortAscending = 1;
							else
								m_bSortAscending = !m_bSortAscending;
						}
						else // reset
						{
							m_nSortBy = phdn->iItem;
							m_bSortAscending = 1;
						}
						
						CHoldRedraw hr(GetTree());
						TCH()->ResetIndexMap();

						CTreeListSyncer::Sort(SortProc, (DWORD)this);
					}
					break;

				case LVN_GETDISPINFO:
					{
						LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;

						if (pDispInfo->item.iSubItem == 0)
						{
							GANTTITEM gi;

							if (GetGanttItem(pDispInfo->item.lParam, gi))
							{
								static CString sCallback;
								sCallback = gi.sTitle;
								pDispInfo->item.pszText = (LPTSTR)(LPCTSTR)sCallback;
							}
						}
					}
					break;

				case TVN_SELCHANGED:
					if (HasOption(GTLCF_AUTOSCROLLTOTASK))
						ScrollToSelectedTask();
					break;

				case TVN_GETDISPINFO:
					{
						TV_DISPINFO* pDispInfo = (TV_DISPINFO*)pNMHDR;

						if (pDispInfo->item.mask & TVIF_TEXT)
						{
							if (!IsTreeFullRowSelect(hwnd))
							{
								CRect rItem;
								GetTreeItemRect(pDispInfo->item.hItem, 0, rItem);

								if (!rItem.IsRectEmpty())
								{
									DWORD dwTaskID = pDispInfo->item.lParam;

									GANTTDISPLAY gd;
									GetGanttDisplay(dwTaskID, gd);

									if (gd.sDisplayText.IsEmpty())
									{
										GANTTITEM gi;
										VERIFY (GetGanttItem(dwTaskID, gi));

										CWnd* pTree = CWnd::FromHandle(hwnd);
										CClientDC dc(pTree);
										CFont* pOldFont = GraphicsMisc::PrepareDCFont(&dc, pTree);
										
										CEnString sDisplay(gi.sTitle);
										sDisplay.FormatDC(&dc, rItem.Width() - 4, ES_END);
										dc.SelectObject(pOldFont);
										
										gd.sDisplayText = sDisplay;
										m_display[dwTaskID] = gd;
									}
							
									pDispInfo->item.pszText = (LPTSTR)(LPCTSTR)gd.sDisplayText;
								}
							}
						}
					}
					break;

//				case TVN_ITEMEXPANDING:
				case TVN_ITEMEXPANDED:
					{
						NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
						HTREEITEM hti = pNMTreeView->itemNew.hItem;
						BOOL bExpand = (pNMTreeView->action == TVE_EXPAND);

						// calc the minimum required width of the tree and resize as necessary
						if (bExpand)
							Resize(m_rect);

						// clear index map (for alternate line coloring)
						// and force redraw of tree
						TCH()->ResetIndexMap();
						RedrawTree();

						CHoldRedraw hr(GetList());
						EnableResync(FALSE);

						if (bExpand)
						{
							int nNextIndex = GetListItem(hti) + 1;
							ExpandList(hti, nNextIndex);
						}
						else // collapse
						{
							CollapseList(hti);
						}

						EnableResync(TRUE, m_hwndTree);
					}
					break;

				case TVN_DELETEITEM:
					break;

				case NM_CUSTOMDRAW:
					{
						if (hwnd == m_listHeader)
						{
							lr |= OnCustomDrawListHeader((NMCUSTOMDRAW*)pNMHDR);
						}
						else if (IsList(hwnd))
						{
							lr |= OnCustomDrawList(hwnd, (NMLVCUSTOMDRAW*)pNMHDR);
						}
						// when the tree expands Windows sends custom draw notifications
						// for _every_ tree item, visible or not. For trees with many items
						// this can take a significant amount of time to process so we 
						// disable custom draw while expanding
						else if (IsTree(hwnd) && !m_bTreeExpanding && m_bResyncEnabled)
						{
							lr |= OnCustomDrawTree(hwnd, (NMTVCUSTOMDRAW*)pNMHDR);
						}
					}
					break;
				}
				
				return lr;
			}
			break;
		}
	}
	
	return CTreeListSyncer::WindowProc(hRealWnd, msg, wp, lp);
}

LRESULT CGanttTreeListCtrl::ScWindowProc(HWND hRealWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (!m_bResyncEnabled)
		return CTreeListSyncer::ScWindowProc(hRealWnd, msg, wp, lp);

	if (hRealWnd == GetList())
	{
		switch (msg)
		{
		case WM_NOTIFY:
			{
				LPNMHDR pNMHDR = (LPNMHDR)lp;
				HWND hwnd = pNMHDR->hwndFrom;
				
				// let base class have its turn first
				LRESULT lr = CTreeListSyncer::ScWindowProc(hRealWnd, msg, wp, lp);

				switch (pNMHDR->code)
				{
				case HDN_BEGINTRACK:
					{
						HD_NOTIFY* pHDN = (HD_NOTIFY*)pNMHDR;

						// prevent tracking of first (invisible) column
						if (pHDN->iItem ==0)
							lr = 1;

						return lr;
					}
					break;
				}
			}
			break;

		case WM_ERASEBKGND:
			return TRUE;

		case WM_SETCURSOR:
			{
				POINT ptCursor = { 0 };
				GetCursorPos(&ptCursor);
		
				GTLC_HITTEST nHit = GTLCHT_NOWHERE;

				DWORD dwTaskID = HitTest(ptCursor, nHit);
				ASSERT((nHit == GTLCHT_NOWHERE) || (dwTaskID != 0));

				if (nHit != GTLCHT_NOWHERE)
				{
					switch (nHit)
					{
					case GTLCHT_BEGIN:
					case GTLCHT_END:
						SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
						return TRUE;

					case GTLCHT_MIDDLE:
						//SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL));
						break;
					}
				}
			}
			break;

		case WM_LBUTTONDOWN:
			if (StartDragging(lp))
			{
				return 0; // eat
			}
			else
			{
				CPoint ptScreen(lp);
				::ClientToScreen(hRealWnd, &ptScreen);

				// don't let the selection to be set to -1
				if (HitTest(ptScreen) == 0)
					return 0;
			}
			break;

		case WM_LBUTTONUP:
			if (EndDragging(lp))
			{
				return 0; // eat
			}
			break;

		case WM_MOUSEMOVE:
			if (UpdateDragging(lp))
			{
				return 0; // eat
			}
			break;

		case WM_CAPTURECHANGED:
			// if someone else grabs the capture we cancel any drag
			if (lp && (HWND(lp) != hRealWnd) && IsDragging())
			{
				CancelDrag(FALSE);
				return 0; // eat
			}
			break;

		case WM_KEYDOWN:
			if (wp == VK_ESCAPE && IsDragging())
			{
				CancelDrag(TRUE);
				return 0; // eat
			}
			break;

		case WM_RBUTTONDOWN:
			{
				CPoint ptScreen(lp);
				::ClientToScreen(hRealWnd, &ptScreen);

				GTLC_HITTEST nHit = GTLCHT_NOWHERE;

				DWORD dwTaskID = HitTest(ptScreen, nHit);
				ASSERT((nHit == GTLCHT_NOWHERE) || (dwTaskID != 0));

				if (nHit != GTLCHT_NOWHERE)
					SelectTask(dwTaskID);
			}
			break;
		}
	}
	else if (hRealWnd == GetTree())
	{
		switch (msg)
		{
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
			{
				TVHITTESTINFO tvi = { { LOWORD(lp), HIWORD(lp) }, 0 };
				HTREEITEM hti = TreeView_HitTest(hRealWnd, &tvi);

				if (hti && (hti != GetTreeSelItem(hRealWnd)))
					SelectTreeItem(hRealWnd, hti);
			}
			break;
		}
	}

	// else tree or list
	switch (msg)
	{
	case WM_MOUSEWHEEL:
		{
			int zDelta = GET_WHEEL_DELTA_WPARAM(wp);
			WORD wKeys = LOWORD(wp);
			
			if (wKeys == MK_CONTROL && zDelta != 0)
			{
				// cache prev value
				GTLC_MONTH_DISPLAY nPrevDisplay = m_nMonthDisplay;

				// do the zoom
				ZoomIn(zDelta > 0);

				// notify parent
				GetCWnd()->SendMessage(WM_GANTTCTRL_NOTIFY_ZOOM, nPrevDisplay, m_nMonthDisplay);
			}
		}
		break;

	case WM_KEYUP:
		{
			LRESULT lr = CTreeListSyncer::ScWindowProc(hRealWnd, msg, wp, lp);

			NMTVKEYDOWN tvkd = { 0 };

			tvkd.hdr.hwndFrom = hRealWnd;
			tvkd.hdr.idFrom = ::GetDlgCtrlID(hRealWnd);
			tvkd.hdr.code = TVN_KEYUP;
			tvkd.wVKey = wp;
			tvkd.flags = lp;

			SendMessage(WM_NOTIFY, ::GetDlgCtrlID(hRealWnd), (LPARAM)&tvkd);
			return lr;
		}
	}
	
	return CTreeListSyncer::ScWindowProc(hRealWnd, msg, wp, lp);
}

void CGanttTreeListCtrl::SetAlternateLineColor(COLORREF crAltLine)
{
	SetColor(m_crAltLine, crAltLine);
}

void CGanttTreeListCtrl::SetGridLineColor(COLORREF crGridLine)
{
	SetColor(m_crGridLine, crGridLine);
}

void CGanttTreeListCtrl::SetTodayColor(COLORREF crToday)
{
	SetColor(m_crToday, crToday);
}

void CGanttTreeListCtrl::SetWeekendColor(COLORREF crWeekend)
{
	SetColor(m_crWeekend, crWeekend);
}

void CGanttTreeListCtrl::SetDoneTaskAttributes(COLORREF color, BOOL bStrikeThru)
{
	SetColor(m_crDone, color);

	if (bStrikeThru)
	{
		HFONT hFont = (HFONT)::SendMessage(GetTree(), WM_GETFONT, 0, 0);
		m_hFontDone = GraphicsMisc::CreateFont(hFont, -1, GMFS_STRIKETHRU);
	}
	else
	{
		GraphicsMisc::VerifyDeleteObject(m_hFontDone);
		m_hFontDone = NULL;
	}
}

void CGanttTreeListCtrl::SetMilestoneTag(const CString& sTag)
{
	if (sTag != GANTTITEM::MILESTONE_TAG)
	{
		GANTTITEM::MILESTONE_TAG = sTag;

		if (IsHooked())
			Invalidate(FALSE);
	}
}

void CGanttTreeListCtrl::SetParentColoring(GTLC_PARENTCOLORING nOption, COLORREF color)
{
	SetColor(m_crParent, color);

	if (IsHooked() && (m_nParentColoring != nOption))
		Invalidate(FALSE);

	m_nParentColoring = nOption;
}

void CGanttTreeListCtrl::SetColor(COLORREF& color, COLORREF crNew)
{
	if (IsHooked() && (crNew != color))
		Invalidate(FALSE);

	color = crNew;
}

void CGanttTreeListCtrl::DrawTreeItem(CDC* pDC, HTREEITEM hti, int nCol, const GANTTITEM& gi)
{
	CRect rItem;
	GetTreeItemRect(hti, nCol, rItem);

	if (rItem.IsRectEmpty())
		return;

	HWND hwndTree = GetTree();
	BOOL bSel = IsTreeItemSelected(hwndTree, hti);
	BOOL bFullRow = IsTreeFullRowSelect(hwndTree);

	// draw vertical item divider
	DrawItemDivider(pDC, rItem, FALSE, TRUE);

	// make sure dates are valid
	CString sItem;

	switch (nCol)
	{
		case TREE_COLUMN_TITLE:
			if (bFullRow)
				sItem = gi.sTitle;
			// else handled by treectrl
			break;

		case TREE_COLUMN_STARTDATE:
			sItem = CDateHelper::FormatDate(gi.dtStart);
			break;

		case TREE_COLUMN_ENDDATE:
			sItem = CDateHelper::FormatDate(gi.dtDue);
			break;

		case TREE_COLUMN_ALLOCTO:
			sItem = gi.sAllocTo;
			break;
	}

	// draw text
	if (!sItem.IsEmpty())
	{
		// text color
		COLORREF crText = GetTreeTextColor(gi, bSel, bFullRow, (nCol == TREE_COLUMN_TITLE));
		COLORREF crOldColor = pDC->SetTextColor(crText);

		if (nCol > 0)
			rItem.left += 6;
		else
			rItem.left += 2;

		rItem.top += 2;

		pDC->SetBkMode(TRANSPARENT);
		pDC->DrawText(sItem, rItem, DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
		pDC->SetTextColor(crOldColor);
	}

	// special case: drawing shortcut icon for reference tasks
	if (nCol == 0 && gi.dwRefID)
	{
		GetTreeItemRect(hti, nCol, rItem, TRUE);
		ShellIcons::DrawIcon(pDC, ShellIcons::SI_SHORTCUT, rItem.TopLeft(), false);
	}
}

void CGanttTreeListCtrl::DrawItemDivider(CDC* pDC, const CRect& rItem, BOOL bColumn, BOOL bVert, LPRECT prFocus)
{
	if (m_crGridLine == (COLORREF)-1)
		return;

	COLORREF color = m_crGridLine;

	if (bColumn && bVert)
		color = GraphicsMisc::Darker(m_crGridLine, 0.5);

	CRect rDiv(rItem);

	if (bVert)
		rDiv.left = (rDiv.right - 1);
	else
		rDiv.top = (rDiv.bottom - 1);

	COLORREF crOld = pDC->GetBkColor();

	if (prFocus)
	{
		ASSERT(!bVert);

		// draw portion to left of prFocus
		rDiv.right = prFocus->left;
		pDC->FillSolidRect(rDiv, color);

		// then bit to right
		rDiv.left = prFocus->right;
		rDiv.right = rItem.right;
		pDC->FillSolidRect(rDiv, color);
	}
	else
	{
		pDC->FillSolidRect(rDiv, color);
	}

	pDC->SetBkColor(crOld);
}

int CGanttTreeListCtrl::GetTreeTitleWidth() const
{
	int nNumAttrib = (HasOption(GTLCF_DISPLAYALLOCTOCOLUMN) ? 3 : 2);

	CRect rTree;
	::GetClientRect(GetTree(), rTree);

	return (rTree.right - (nNumAttrib * TREE_ATTRIB_WIDTH));
}

int CGanttTreeListCtrl::CalcMinTreeTitleWidth() const
{
	int nTitleWidth = 0;

	if (m_hwndTree)
	{
		int nDepth = 0;
		HTREEITEM htiDeepest = TCH()->FindDeepestTask(nDepth, TRUE);
					
		if (htiDeepest == NULL)
			return TREE_ATTRIB_WIDTH;

		// else
		CRect rItem;
		TreeView_GetItemRect(GetTree(), htiDeepest, &rItem, TRUE); // text rect only

		nTitleWidth = rItem.left + TREE_TITLE_WIDTH;
	}
				
	return max(TREE_TITLE_MIN_WIDTH, nTitleWidth);
}

int CGanttTreeListCtrl::RecalcTreeWidth()
{
	int nCurWidth = GetTreeTitleWidth();
	int nTitleWidth = CalcMinTreeTitleWidth();

	if (nTitleWidth > nCurWidth)
	{
		// clear calculated graphics info
		m_display.RemoveAll();

		// resize tree header column
		m_treeHeader.SetItemWidth(0, nTitleWidth);
	}

	int nNumAttrib = (HasOption(GTLCF_DISPLAYALLOCTOCOLUMN) ? 3 : 2);

	return (nTitleWidth + (nNumAttrib * TREE_ATTRIB_WIDTH));
}

void CGanttTreeListCtrl::GetTreeItemRect(HTREEITEM hti, int nCol, CRect& rItem, BOOL bText) const
{
	rItem.SetRectEmpty();

	if (TreeView_GetItemRect(GetTree(), hti, &rItem, FALSE))
	{
		switch (nCol)
		{
		case -1:
			break; // whole rect
			
		case 0:
			{
				CRect rText;
				TreeView_GetItemRect(GetTree(), hti, &rText, TRUE); // text rect only

				if (bText)
				{
					rItem = rText;
					rItem.right = min(GetTreeTitleWidth(), rItem.right);
				}
				else if (rText.right)
				{
					rItem.left = rText.left;
					rItem.right = GetTreeTitleWidth();
				}
			}
			break;

		case 1:
		case 2:
		case 3:
			rItem.right = rItem.left + GetTreeTitleWidth() + (nCol * TREE_ATTRIB_WIDTH);
			rItem.left = rItem.right - TREE_ATTRIB_WIDTH + 1;
			break;
		}
	}
}

void CGanttTreeListCtrl::PostDrawListItem(CDC* pDC, int nItem, DWORD dwTaskID)
{
	HWND hwndList = GetList();
	BOOL bSelected = IsListItemSelected(hwndList, nItem);

	CRect rItem;
	ListView_GetItemRect(hwndList, nItem, rItem, LVIR_BOUNDS);

	// draw horz gridline
	if (!bSelected)
		DrawItemDivider(pDC, rItem, FALSE, FALSE);

	// draw alloc-to name after bar
	if (HasOption(GTLCF_DISPLAYALLOCTOAFTERITEM))
	{
		// get the data for this item
		GANTTITEM gi;
		VERIFY(GetGanttItem(dwTaskID, gi));

		if (/*!gi.bParent && */!gi.sAllocTo.IsEmpty())
		{
			// get the end pos for this item relative to start of window
			GANTTDISPLAY gd;
			GetGanttDisplay(dwTaskID, gd);

			if (gd.nEndPos != -1)
			{
				int nScrollPos = ::GetScrollPos(hwndList, SB_HORZ);

				rItem.left = gd.nEndPos + 3 - nScrollPos;
				rItem.top += 2;

				COLORREF crFill, crBorder;
				GetGanttBarColors(gi, crBorder, crFill, bSelected);

				pDC->SetBkMode(TRANSPARENT);
				pDC->SetTextColor(crBorder);
				pDC->DrawText(gi.sAllocTo, rItem, DT_LEFT);
			}
		}
	}
}

void CGanttTreeListCtrl::DrawListItem(CDC* pDC, int nItem, int nCol, DWORD dwTaskID)
{
	if (nCol == 0)
	{
// #ifdef _DEBUG
// 		rItem.right = DEF_MONTH_WIDTH;
// 		DrawItemDivider(pDC, rItem, FALSE, TRUE);
// #endif
		return;
	}

	// see if we can avoid drawing this sub-item at all
	HWND hwndList = GetList();

	CRect rItem, rClip;
	ListView_GetSubItemRect(hwndList, nItem, nCol, LVIR_BOUNDS, &rItem);
	pDC->GetClipBox(rClip);

	if (rItem.right < rClip.left || rItem.left > rClip.right)
		return;

	// get the date range for this column
	int nMonth = 0, nYear = 0, i;
	
	if (!GetListColumnDate(nCol, nMonth, nYear))
		return;

	// get the data and display for this item
	GANTTITEM gi;
	VERIFY(GetGanttItem(dwTaskID, gi));

	GANTTDISPLAY gd;
	GetGanttDisplay(dwTaskID, gd);

	// is item selected
	BOOL bSelected = IsListItemSelected(hwndList, nItem);

	int nSaveDC = pDC->SaveDC();

	int nMonthWidth = GetMonthWidth(rItem.Width());
	int nEndPos = -1, nDonePos = -1;
	BOOL bToday = FALSE;
	CRect rMonth(rItem);

	switch (m_nMonthDisplay)
	{
	case GTLC_DISPLAY_YEARS:
		for (i = 0; i < 12; i++)
		{
			rMonth.right = rMonth.left + nMonthWidth;

			// draw vertical month divider
			DrawItemDivider(pDC, rMonth, (i == 11), TRUE);
			
			// if we're passed the end of the item then we can stop drawing
			if (!bToday)
				bToday = DrawToday(pDC, rMonth, nMonth + i, nYear, bSelected);

			if (nEndPos == -1)
				nEndPos = DrawGanttBar(pDC, rMonth, nMonth + i, nYear, gi);

			if (nDonePos == -1)
				nDonePos = DrawGanttDone(pDC, rMonth, nMonth + i, nYear, gi);

			// next item
			rMonth.left = rMonth.right; 
		}
		break;
		
	case GTLC_DISPLAY_QUARTERS_SHORT:
	case GTLC_DISPLAY_QUARTERS_MID:
	case GTLC_DISPLAY_QUARTERS_LONG:
		for (i = 0; i < 3; i++)
		{
			rMonth.right = rMonth.left + nMonthWidth;

			// draw vertical month divider
			DrawItemDivider(pDC, rMonth, (i == 2), TRUE);
			
			// if we're passed the end of the item then we can stop drawing
			if (!bToday)
				bToday = DrawToday(pDC, rMonth, nMonth + i, nYear, bSelected);

			if (nEndPos == -1)
				nEndPos = DrawGanttBar(pDC, rMonth, nMonth + i, nYear, gi, bSelected);

			if (nDonePos == -1)
				nDonePos = DrawGanttDone(pDC, rMonth, nMonth + i, nYear, gi, bSelected);

			// next item
			rMonth.left = rMonth.right;
		}
		break;
		
	case GTLC_DISPLAY_MONTHS_SHORT:
	case GTLC_DISPLAY_MONTHS_MID:
	case GTLC_DISPLAY_MONTHS_LONG:
		// draw vertical month divider
		DrawItemDivider(pDC, rMonth, (nMonth == 12), TRUE);
			
		bToday = DrawToday(pDC, rMonth, nMonth, nYear, bSelected);
		nEndPos = DrawGanttBar(pDC, rMonth, nMonth, nYear, gi, bSelected);
		nDonePos = DrawGanttDone(pDC, rMonth, nMonth, nYear, gi, bSelected);
		break;
		
	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
		{
 			// draw vertical week dividers
			int nNumDays = CDateHelper::GetDaysInMonth(nMonth, nYear);
			double dMonthWidth = rMonth.Width();

			int nFirstDOW = CDateHelper::GetFirstDayOfWeek();
			CRect rDay(rMonth);

			// omit the first one so as not to overwrite the previous month divider
			COleDateTime dtDay = COleDateTime(nYear, nMonth, 1, 0, 0, 0);
			COLORREF crWeekends = GetWeekendColor(bSelected);

			for (int nDay = 1; nDay <= nNumDays; nDay++)
			{
				rDay.left = rMonth.left + (int)((nDay - 1) * dMonthWidth / nNumDays);
				rDay.right = rMonth.left + (int)(nDay * dMonthWidth / nNumDays);

				// fill weekends
				if (crWeekends != NO_COLOR)
				{
					if (CDateHelper::IsWeekend(dtDay))
						pDC->FillSolidRect(rDay, crWeekends);
				}

				// draw divider
				if (dtDay.GetDayOfWeek() == nFirstDOW && nDay > 1)
				{
					rDay.right = rDay.left; // draw at start of day
					DrawItemDivider(pDC, rDay, FALSE, TRUE);
				}

				// next day
				dtDay += 1;
			}

			// draw vertical month divider
			DrawItemDivider(pDC, rMonth, TRUE, TRUE);

			bToday = DrawToday(pDC, rMonth, nMonth, nYear, bSelected);
			nEndPos = DrawGanttBar(pDC, rMonth, nMonth, nYear, gi, bSelected);
			nDonePos = DrawGanttDone(pDC, rMonth, nMonth, nYear, gi, bSelected);
		}
		break;

	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		{
			// draw vertical day dividers
			int nNumDays = CDateHelper::GetDaysInMonth(nMonth, nYear);
			double dMonthWidth = rMonth.Width();
			CRect rDay(rMonth);

			// omit the first one so as not to overwrite the previous month divider
			COleDateTime dtDay = COleDateTime(nYear, nMonth, 1, 0, 0, 0);
			COLORREF crWeekends = GetWeekendColor(bSelected);

			for (int nDay = 1; nDay <= nNumDays; nDay++)
			{
				rDay.left = rMonth.left + (int)((nDay - 1) * dMonthWidth / nNumDays);
				rDay.right = rMonth.left + (int)((nDay * dMonthWidth / nNumDays) + 0.5);

				// fill weekends
				if (crWeekends != NO_COLOR)
				{
					if (CDateHelper::IsWeekend(dtDay))
						pDC->FillSolidRect(rDay, crWeekends);
				}

				// draw all but the last divider
				if (nDay < nNumDays)
					DrawItemDivider(pDC, rDay, FALSE, TRUE);

				// next day
				dtDay += 1;
			}
				
			// draw vertical month divider
			DrawItemDivider(pDC, rMonth, TRUE, TRUE);

			bToday = DrawToday(pDC, rMonth, nMonth, nYear, bSelected);
			nEndPos = DrawGanttBar(pDC, rMonth, nMonth, nYear, gi, bSelected);
			nDonePos = DrawGanttDone(pDC, rMonth, nMonth, nYear, gi, bSelected);
		}
		break;
		
	default:
		ASSERT(0);
		break;
	}

	// save off the absolute item end pos
	int nScrollPos = ::GetScrollPos(hwndList, SB_HORZ);

	if (nEndPos != -1/* && nEndPos != gd.nEndPos*/)
	{
		nEndPos = max(nEndPos, nDonePos);

		gd.nEndPos = nEndPos + nScrollPos;
		m_display[dwTaskID] = gd;
	}

	pDC->RestoreDC(nSaveDC);
}

void CGanttTreeListCtrl::DrawListHeaderItem(CDC* pDC, int nCol)
{
	CRect rItem;
	m_listHeader.GetItemRect(nCol, rItem);

	if (nCol == 0)
		return;

	// get the date range for this column
	int nMonth = 0, nYear = 0;
	
	if (!GetListColumnDate(nCol, nMonth, nYear))
		return;

	int nSaveDC = pDC->SaveDC();
	int nMonthWidth = GetMonthWidth(rItem.Width());
	CFont* pOldFont = GraphicsMisc::PrepareDCFont(pDC, &m_listHeader);
	
	CThemed th;
	BOOL bThemed = (th.AreControlsThemed() && th.Open(GetCWnd(), _T("HEADER")));

	CRect rMonth(rItem), rClip;
	pDC->GetClipBox(rClip);

	switch (m_nMonthDisplay)
	{
	case GTLC_DISPLAY_YEARS:
	case GTLC_DISPLAY_QUARTERS_SHORT:
	case GTLC_DISPLAY_QUARTERS_MID:
	case GTLC_DISPLAY_QUARTERS_LONG:
	case GTLC_DISPLAY_MONTHS_SHORT:
	case GTLC_DISPLAY_MONTHS_MID:
	case GTLC_DISPLAY_MONTHS_LONG:
		// should never get here
		ASSERT(0);
		break;
		
	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
		{
 			// copy month rect because we are going to modify it
			CRect rWeek(rMonth);
			rWeek.top += (rWeek.Height() / 2);

			// draw month header
			rMonth.bottom = rWeek.top;
			DrawListHeaderRect(pDC, rMonth, m_listHeader.GetItemText(nCol), bThemed ? &th :NULL);

			// draw week elements
			int nNumDays = CDateHelper::GetDaysInMonth(nMonth, nYear);
			double dMonthWidth = rMonth.Width();

			// first week starts at 'First week of first DOW'
			int nFirstDOW = CDateHelper::GetFirstDayOfWeek();
			int nDay = CDateHelper::CalcDayOfMonth(nFirstDOW, 1, nMonth, nYear);

			// calc number of first week
			COleDateTime dtWeek(nYear, nMonth, nDay, 0, 0, 0);
			int nWeek = CDateHelper::CalcWeekofYear(dtWeek);
			BOOL bDone = FALSE;

			while (!bDone)
			{
				rWeek.left = rMonth.left + (int)((nDay - 1) * dMonthWidth / nNumDays);

				// if this week bridges into next month this needs special handling
				if ((nDay + 6) > nNumDays)
				{
					// rest of this month
					rWeek.right = rMonth.right;
					
					// plus some of next month
					nDay += (6 - nNumDays);
					nMonth++;
					
					if (nMonth > 12)
					{
						nMonth = 1;
						nYear++;
					}
					
					if (m_listHeader.GetItemRect(nCol+1, rMonth))
					{
						nNumDays = CDateHelper::GetDaysInMonth(nMonth, nYear);
						dMonthWidth = rMonth.Width();

						rWeek.right += (int)(nDay * dMonthWidth / nNumDays);
					}

					// if this is week 53, check that its not really week 1 of the next year
					if (nWeek == 53)
					{
						ASSERT(nMonth == 1);

						COleDateTime dtWeek(nYear, nMonth, nDay, 0, 0, 0);
						nWeek = CDateHelper::CalcWeekofYear(dtWeek);
					}

					bDone = TRUE;
				}
				else 
					rWeek.right = rMonth.left + (int)((nDay + 6) * dMonthWidth / nNumDays);

				// check if we can stop
				if (rWeek.left > rClip.right)
					break; 

				// check if we need to draw
				if (rWeek.right >= rClip.left)
					DrawListHeaderRect(pDC, rWeek, Misc::Format(nWeek), bThemed ? &th :NULL);

				// next week
				nDay += 7;
				nWeek++;

				// are we done?
				bDone = (bDone || nDay > nNumDays);
			}
		}
		break;

	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		{
			// copy month rect because we are going to modify it
			CRect rDay(rMonth);
			rDay.top += (rDay.Height() / 2);

			// draw month header
			rMonth.bottom = rDay.top;
			DrawListHeaderRect(pDC, rMonth, m_listHeader.GetItemText(nCol), bThemed ? &th :NULL);

			// draw day elements
			int nNumDays = CDateHelper::GetDaysInMonth(nMonth, nYear);
			double dMonthWidth = rMonth.Width();

			// precalc end of last day to reduce loop calculations
			rDay.right = rDay.left;
			
			for (int nDay = 0; nDay < nNumDays; nDay++)
			{
				rDay.left = rDay.right;
				rDay.right = rMonth.left + (int)((nDay + 1) * dMonthWidth / nNumDays);

				// check if we can stop
				if (rDay.left > rClip.right)
					break; 

				// check if we need to draw
				if (rDay.right >= rClip.left)
					DrawListHeaderRect(pDC, rDay, Misc::Format(nDay+1), bThemed ? &th :NULL);
			}
		}
		break;
		
	default:
		ASSERT(0);
		break;
	}

	pDC->SelectObject(pOldFont); // not sure if this is nec but play safe
	pDC->RestoreDC(nSaveDC);
}

void CGanttTreeListCtrl::DrawListHeaderRect(CDC* pDC, const CRect& rItem, const CString& sItem, CThemed* pTheme)
{
	if (!pTheme)
	{
		pDC->FillSolidRect(rItem, ::GetSysColor(COLOR_3DFACE));
		pDC->Draw3dRect(rItem, ::GetSysColor(COLOR_3DHIGHLIGHT), ::GetSysColor(COLOR_3DSHADOW));
	}
	else
	{
		pTheme->DrawBackground(pDC, HP_HEADERITEM, HIS_NORMAL, rItem);
	}
	
	// text
	if (!sItem.IsEmpty())
	{
		pDC->SetBkMode(TRANSPARENT);
		pDC->SetTextColor(GetSysColor(COLOR_WINDOWTEXT));

		const UINT nFlags = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_CENTER;
		pDC->DrawText(sItem, (LPRECT)(LPCRECT)rItem, nFlags);
	}
}

BOOL CGanttTreeListCtrl::HasDisplayDates(const GANTTITEM& gi) const
{
	if (HasDoneDate(gi))
		return TRUE;

	// else
	COleDateTime dummy1, dummy2;
	return GetStartDueDates(gi, dummy1, dummy2);
}

BOOL CGanttTreeListCtrl::HasDoneDate(const GANTTITEM& gi) const
{
	if (gi.bParent && HasOption(GTLCF_CALCPARENTDATES))
		return FALSE;

	// else
	return CDateHelper::IsDateSet(gi.dtDone);
}

BOOL CGanttTreeListCtrl::GetStartDueDates(const GANTTITEM& gi, COleDateTime& dtStart, COleDateTime& dtDue) const
{
	BOOL bDoneSet = FALSE;

	if (gi.bParent && HasOption(GTLCF_CALCPARENTDATES))
	{
		dtStart = gi.dtStartCalc;
		dtDue = gi.dtDueCalc;
	}
	else
	{
		dtStart = gi.dtStart;
		dtDue = gi.dtDue;

		bDoneSet = CDateHelper::IsDateSet(gi.dtDone);
	}
		
	BOOL bStartSet = CDateHelper::IsDateSet(dtStart);
	BOOL bDueSet = CDateHelper::IsDateSet(dtDue);
		
	// do we need to calculate due date?
	if (!bDueSet && HasOption(GTLCF_CALCMISSINGDUEDATES))
	{
		// take completed date if that is set
		if (bDoneSet)
		{
			dtDue = gi.dtDone;
		}
		else // choose between start date and today
		{
			COleDateTime dtToday = CDateHelper::GetDate(DHD_TODAY);
			
			// if start date is set, use the later of the start date
			// or today
			if (bStartSet)
			{
				if (dtStart < dtToday)
				{
					dtDue = dtToday;
				}
				else
				{
					dtDue = CDateHelper::GetDateOnly(dtStart);
				}
			}
			else // use today
			{
				dtDue = dtToday;
			}
			
			// and move to end of the day
			dtDue.m_dt += END_OF_DAY;
		}
		
		bDueSet = TRUE;
		ASSERT(CDateHelper::IsDateSet(dtDue));
	}
	
	// do we need to calculate start date?
	if (!bStartSet && HasOption(GTLCF_CALCMISSINGSTARTDATES))
	{
		// take earlier of due or completed date
		if (bDueSet && bDoneSet)
		{
			if (dtDue < gi.dtDone)
			{
				dtStart = CDateHelper::GetDateOnly(dtDue);
			}
			else
			{
				dtStart = CDateHelper::GetDateOnly(gi.dtDone);
			}
		}
		else if (bDueSet)
		{
			dtStart = CDateHelper::GetDateOnly(dtDue);
		}
		else if (bDoneSet)
		{
			dtStart = CDateHelper::GetDateOnly(gi.dtDone);
		}
		else // use today
		{
			dtStart = CDateHelper::GetDate(DHD_TODAY);
		}
		
		ASSERT(CDateHelper::IsDateSet(dtStart));
	}

	return (CDateHelper::IsDateSet(dtStart) && CDateHelper::IsDateSet(dtDue));
}

COLORREF CGanttTreeListCtrl::GetWeekendColor(BOOL bSelected) const
{
	return GetColor(m_crWeekend, 0.0, bSelected);
}

COLORREF CGanttTreeListCtrl::GetTreeTextBkColor(const GANTTITEM& gi, BOOL bSelected, BOOL bAlternate, BOOL bFullRow, BOOL bTitle) const
{
	if (bSelected)
	{
		return GetSysColor(COLOR_HIGHLIGHT);
	}
	else if (!gi.IsDone() && (gi.crBack != CLR_NONE))
	{
		return gi.crBack;
	}
	else if (bAlternate)
	{
		return m_crAltLine;
	}
	
	// else
	return GetSysColor(COLOR_WINDOW);
}

COLORREF CGanttTreeListCtrl::GetTreeTextColor(const GANTTITEM& gi, BOOL bSelected, BOOL bFullRow, BOOL bTitle) const
{
	if (bSelected)
	{
		return GetSysColor(COLOR_HIGHLIGHTTEXT);
	}
	else if (gi.IsDone() && m_crDone != NO_COLOR)
	{
		return m_crDone;
	}
	else if (!gi.IsDone() && (gi.crText != CLR_NONE))
	{
		return gi.crText;
	}

	// all else
	return GetSysColor(COLOR_WINDOWTEXT);
}

HBRUSH CGanttTreeListCtrl::GetGanttBarColors(const GANTTITEM& gi, COLORREF& crBorder, COLORREF& crFill, BOOL bSelected) const
{
	// darker shade of the item crText/crBack
	COLORREF crDefFill = gi.GetDefaultFillColor();
	COLORREF crDefBorder = gi.GetDefaultBorderColor();

	HBRUSH hbrParent = NULL;
	
	if (gi.bParent)
	{
		switch (m_nParentColoring)
		{
		case GTLPC_NOCOLOR:
			crBorder = crDefBorder;
			crFill = NO_COLOR;
			break;
			
		case GTLPC_SPECIFIEDCOLOR:
			crBorder = GraphicsMisc::Darker(m_crParent, 0.5);
			crFill = m_crParent;
			break;
			
		case GTLPC_DEFAULTCOLORING:
		default:
			crBorder = crDefBorder;
			crFill = crDefFill;
			break;
		}
		
		hbrParent = GetSysColorBrush(COLOR_WINDOWTEXT);
	}
	else
	{
		crBorder = crDefBorder;
		crFill = crDefFill;
	}

	// colors are modified by selection status
	if (bSelected)
	{
		//crFill = GraphicsMisc::Darker(crFill, 0.5);
		crBorder = GetSysColor(COLOR_HIGHLIGHTTEXT);

		if (gi.bParent && HasOption(GTLCF_CALCPARENTDATES))
			hbrParent = GetSysColorBrush(COLOR_HIGHLIGHTTEXT);
	}

	return hbrParent;
}

BOOL CGanttTreeListCtrl::CalcDateRect(const CRect& rMonth, int nMonth, int nYear, 
							const COleDateTime& dtFrom, const COleDateTime& dtTo, CRect& rDate)
{
	int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);

	if (nDaysInMonth == 0)
		return FALSE;
	
	COleDateTime dtMonthStart(nYear, nMonth, 1, 0, 0, 0);
	COleDateTime dtMonthEnd(nYear, nMonth, nDaysInMonth, 23, 59, 59); // end of last day

	return CalcDateRect(rMonth, nDaysInMonth, dtMonthStart, dtMonthEnd, dtFrom, dtTo, rDate);
}

BOOL CGanttTreeListCtrl::CalcDateRect(const CRect& rMonth, int nDaysInMonth, 
							const COleDateTime& dtMonthStart, const COleDateTime& dtMonthEnd, 
							const COleDateTime& dtFrom, const COleDateTime& dtTo, CRect& rDate)
{
	if (dtFrom > dtTo || dtTo < dtMonthStart || dtFrom > dtMonthEnd)
		return FALSE;

	rDate = rMonth;

	double dMultiplier = (rMonth.Width() / (double)nDaysInMonth);

	if (dtFrom > dtMonthStart)
		rDate.left += (int)(((dtFrom - dtMonthStart) * dMultiplier));

	if (dtTo < dtMonthEnd)
	{
		if (dtTo == dtFrom)
			rDate.right = rDate.left;
		else
			rDate.right -= (int)(((dtMonthEnd - dtTo) * dMultiplier) + 0.5);
	}

	return TRUE;
}

int CGanttTreeListCtrl::DrawGanttBar(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, const GANTTITEM& gi, BOOL bSelected)
{
	if (gi.IsMilestone())
		return DrawGanttMilestone(pDC, rMonth, nMonth, nYear, gi, bSelected);

	// else normal bar
	COleDateTime dtStart, dtDue;
	
	if (!GetStartDueDates(gi, dtStart, dtDue))
	{
		// indicate no further drawing required
		return 0;
	}

	int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);
	
	if (nDaysInMonth == 0)
		return -1;
	
	COleDateTime dtMonthStart, dtMonthEnd;

	if (!GetMonthDates(nMonth, nYear, dtMonthStart, dtMonthEnd))
		return -1;

	// check for visibility
	CRect rBar(rMonth);

	if (!CalcDateRect(rMonth, nDaysInMonth, dtMonthStart, dtMonthEnd, dtStart, dtDue, rBar))
		return -1;

	int nEndPos = -1;
	COLORREF crBorder, crFill;
	HBRUSH hbrParent = GetGanttBarColors(gi, crBorder, crFill, bSelected);
	
	// adjust bar height
	if (gi.bParent && HasOption(GTLCF_CALCPARENTDATES))
		rBar.DeflateRect(0, 4, 0, 6);
	else
		rBar.DeflateRect(0, 2, 0, 3);
	
	// calculate what borders to draw
	DWORD dwBorders = GMDR_TOP | GMDR_BOTTOM;

	// draw the ends of the border by deflating in width
	// if the date does not range beyond the month
	if (dtStart >= dtMonthStart)
		dwBorders |= GMDR_LEFT;
	
	if (dtDue <= dtMonthEnd)
	{
		nEndPos = rBar.right; // else left as -1
		dwBorders |= GMDR_RIGHT;
	}

	// if parent and no-fill then clear fill crText
	if (gi.bParent && (m_nParentColoring == GTLPC_NOCOLOR))
		crFill = CLR_NONE;

	// draw rect with borders
	GraphicsMisc::DrawRect(pDC, rBar, crFill, crBorder, 0, dwBorders);
	
	// for parent items draw downward facing pointers at the ends
	DrawGanttParentEnds(pDC, gi, rBar, dtMonthStart, dtMonthEnd, hbrParent);

	return nEndPos;
}

void CGanttTreeListCtrl::DrawGanttParentEnds(CDC* pDC, const GANTTITEM& gi, const CRect& rBar, 
											 const COleDateTime& dtMonthStart, const COleDateTime& dtMonthEnd,
											 HBRUSH hbrParent)
{
	if (!gi.bParent || !HasOption(GTLCF_CALCPARENTDATES))
		return;

	ASSERT(hbrParent);
	
	pDC->SelectObject(hbrParent);
	pDC->SelectStockObject(NULL_PEN);

	COleDateTime dtStart, dtDue;
	GetStartDueDates(gi, dtStart, dtDue);
	
	if (dtStart >= dtMonthStart)
	{
		POINT pt[3] = 
		{ 
			{ rBar.left, rBar.bottom }, 
			{ rBar.left, rBar.bottom + 6 }, 
			{ rBar.left + 6, rBar.bottom } 
		};
		
		pDC->Polygon(pt, 3);
	}
	
	if (dtDue <= dtMonthEnd)
	{
		POINT pt[3] = 
		{ 
			{ rBar.right, rBar.bottom }, 
			{ rBar.right, rBar.bottom + 6 }, 
			{ rBar.right - 6, rBar.bottom } 
		};
		
		pDC->Polygon(pt, 3);
	}
}

BOOL CGanttTreeListCtrl::GetMonthDates(int nMonth, int nYear, COleDateTime& dtStart, COleDateTime& dtEnd)
{
	int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);
	ASSERT(nDaysInMonth);

	if (nDaysInMonth == 0)
		return FALSE;
	
	dtStart.SetDate(nYear, nMonth, 1);
	dtEnd.m_dt = dtStart.m_dt + nDaysInMonth;
	
	return TRUE;
}

int CGanttTreeListCtrl::DrawGanttDone(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, const GANTTITEM& gi, BOOL bSelected)
{
	if (!HasDoneDate(gi) || gi.IsMilestone())
	{
		// indicate no further drawing required
		return 0;
	}

	int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);

	if (nDaysInMonth == 0)
		return -1;

	COleDateTime dtMonthStart, dtMonthEnd;

	if (!GetMonthDates(nMonth, nYear, dtMonthStart, dtMonthEnd))
		return -1;

	if (gi.dtDone < dtMonthStart || gi.dtDone > dtMonthEnd)
		return -1;

	CRect rDone(rMonth);

	if (!CalcDateRect(rMonth, nDaysInMonth, dtMonthStart, dtMonthEnd, gi.dtDone, gi.dtDone, rDone))
		return -1;
	
	// draw done date
	COLORREF crBorder, crFill;
	GetGanttBarColors(gi, crBorder, crFill, bSelected);

	// resize to a square
	rDone.DeflateRect(0, 6, 0, 6);
	rDone.right = min(rMonth.right, rDone.left + rDone.Height());
	rDone.left = rDone.right - rDone.Height();

	pDC->FillSolidRect(rDone, crBorder);

	return rDone.right;
}

int CGanttTreeListCtrl::DrawGanttMilestone(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, const GANTTITEM& gi, BOOL bSelected)
{
	// sanity check
	ASSERT(gi.IsMilestone());

	int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);

	if (nDaysInMonth == 0)
		return -1;

	COleDateTime dtMonthStart, dtMonthEnd;

	if (!GetMonthDates(nMonth, nYear, dtMonthStart, dtMonthEnd))
		return -1;

	COleDateTime dtDue = ((gi.bParent && HasOption(GTLCF_CALCPARENTDATES)) ? gi.dtDueCalc : gi.dtDue);
	ASSERT(CDateHelper::IsDateSet(dtDue));

	if (dtDue < dtMonthStart || dtDue > dtMonthEnd)
		return -1;

	CRect rMilestone(rMonth);

	if (!CalcDateRect(rMonth, nDaysInMonth, dtMonthStart, dtMonthEnd, dtDue, dtDue, rMilestone))
		return -1;
	
	// resize to a square
	rMilestone.DeflateRect(0, 3, 0, 4);
	rMilestone.right = min(rMonth.right, rMilestone.left + rMilestone.Height());
	rMilestone.left = rMilestone.right - rMilestone.Height();

	// build a polygon
	CPoint ptMid = rMilestone.CenterPoint();

	// draw milestone as diamond
	POINT pt[5] = 
	{ 
		{ rMilestone.left, ptMid.y }, 
		{ ptMid.x, rMilestone.top }, 
		{ rMilestone.right, ptMid.y }, 
		{ ptMid.x, rMilestone.bottom }, 
		{ rMilestone.left, ptMid.y }
	};
	
	pDC->SelectStockObject(BLACK_BRUSH);
	pDC->SelectStockObject(NULL_PEN);
	pDC->Polygon(pt, 5);

	return rMilestone.right;
}

BOOL CGanttTreeListCtrl::DrawToday(CDC* pDC, const CRect& rMonth, int nMonth, int nYear, BOOL bSelected)
{
	if (m_crToday == NO_COLOR)
		return TRUE; // so we don't keep trying to draw it

	int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);

	if (nDaysInMonth == 0)
		return FALSE;

	COleDateTime dtMonthStart, dtMonthEnd;

	if (!GetMonthDates(nMonth, nYear, dtMonthStart, dtMonthEnd))
		return FALSE;

	// draw 'today'
	COleDateTime dtToday = CDateHelper::GetDateOnly(COleDateTime::GetCurrentTime());
	
	// check for overlap
	if (dtToday < dtMonthStart || dtToday > dtMonthEnd)
		return FALSE;

	CRect rToday;

	if (!CalcDateRect(rMonth, nDaysInMonth, dtMonthStart, dtMonthEnd, dtToday, dtToday.m_dt + 1.0, rToday))
		return FALSE;

	// draw border
	pDC->FillSolidRect(rToday, GetColor(m_crToday, 0.0, bSelected));

	// fill interior if enough space
	if (rToday.Width() > 2)
	{
		rToday.DeflateRect(1, 0);
		pDC->FillSolidRect(rToday, GetColor(m_crToday, 0.5, bSelected));
	}

	return TRUE;
}

COLORREF CGanttTreeListCtrl::GetColor(COLORREF crBase, double dLighter, BOOL bSelected)
{
	COLORREF crResult(crBase);

	if (dLighter > 0.0)
		crResult = GraphicsMisc::Lighter(crResult, dLighter);

	if (bSelected)
		crResult = GraphicsMisc::Darker(crResult, 0.35);

	return crResult;
}

int CGanttTreeListCtrl::GetListItem(HTREEITEM hti)
{
	return FindListItem(GetList(), GetTreeItemData(GetTree(), hti));
}

void CGanttTreeListCtrl::ExpandList()
{
	int nNextIndex = 0;
	ExpandList(NULL, nNextIndex);
}

void CGanttTreeListCtrl::CollapseList()
{
	// collapse top-level items
	HTREEITEM hti = TreeView_GetChild(GetTree(), NULL);
	
	while (hti)
	{
		CollapseList(hti);
		hti = TreeView_GetNextItem(GetTree(), hti, TVGN_NEXT);
	}
}

void CGanttTreeListCtrl::ExpandList(HTREEITEM hti, int& nNextIndex)
{
	// insert children (and grand-children) below
	HTREEITEM htiChild = TreeView_GetChild(GetTree(), hti);
	
	while (htiChild)
	{
		DWORD dwData = GetTreeItemData(GetTree(), htiChild);

		int nItem = GetListItem(htiChild);

		if (nItem == -1)
		{
			LVITEM lvi = { LVIF_TEXT | LVIF_PARAM, 0 };
			
			lvi.iItem = nNextIndex++;
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.lParam = dwData;

			int nIndex = ListView_InsertItem(GetList(), &lvi);
			
			GANTTITEM gi;
			VERIFY(GetGanttItem(dwData, gi));
		}
		else
		{
			// we want to insert htiChild's children immediately after it
			nNextIndex = nItem + 1;
		}
			
		// expand list further if this tree item is expanded
		if (TreeItemHasState(GetTree(), htiChild, TVIS_EXPANDED))
			ExpandList(htiChild, nNextIndex);
		
		htiChild = TreeView_GetNextItem(GetTree(), htiChild, TVGN_NEXT);
	}
}

void CGanttTreeListCtrl::CollapseList(HTREEITEM hti)
{
	ASSERT(hti);

	// remove items and their children
	HTREEITEM htiChild = TreeView_GetChild(GetTree(), hti);
	
	while (htiChild)
	{
		int nItem = GetListItem(htiChild);
		
		if (nItem != -1)
		{
			// remove it
			ListView_DeleteItem(GetList(), nItem);
			
			// and it's children
			CollapseList(htiChild);
		}
		
		htiChild = TreeView_GetNextItem(GetTree(), htiChild, TVGN_NEXT);
	}
}

void CGanttTreeListCtrl::DeleteTreeItem(HTREEITEM hti)
{
	ASSERT(hti);

	// remove items and their children
	HTREEITEM htiChild = TreeView_GetChild(GetTree(), hti);
	
	while (htiChild)
	{
		HTREEITEM htiNext = TreeView_GetNextItem(GetTree(), htiChild, TVGN_NEXT);

		DeleteTreeItem(htiChild);
		htiChild = htiNext;
	}

	// then the associated list item
	int nItem = GetListItem(hti);

	if (nItem != -1)
		ListView_DeleteItem(GetList(), nItem);

	// then the tree data
	DWORD dwTaskID = GetTreeItemData(GetTree(), hti);
	VERIFY(m_data.RemoveKey(dwTaskID));

	// finally we remove the tree item itself
	TreeView_DeleteItem(GetTree(), hti);
}

BOOL CGanttTreeListCtrl::ZoomIn(BOOL bIn)
{
	return SetMonthDisplay((GTLC_MONTH_DISPLAY)(m_nMonthDisplay + (bIn ? 1 : -1)));
}

BOOL CGanttTreeListCtrl::SetMonthDisplay(GTLC_MONTH_DISPLAY nNewDisplay)
{
	// validate new display
	if (nNewDisplay < GTLC_DISPLAY_FIRST || nNewDisplay > GTLC_DISPLAY_LAST)
		return FALSE;

	// calculate the min month width for this display
	int nMinMonthWidth = m_aMinMonthWidths[nNewDisplay];

	// adjust the header height where we need to draw days/weeks
	switch (nNewDisplay)
	{
	case GTLC_DISPLAY_YEARS:
	case GTLC_DISPLAY_QUARTERS_SHORT:
	case GTLC_DISPLAY_QUARTERS_MID:
	case GTLC_DISPLAY_QUARTERS_LONG:
	case GTLC_DISPLAY_MONTHS_SHORT:
	case GTLC_DISPLAY_MONTHS_MID:
	case GTLC_DISPLAY_MONTHS_LONG:
		m_listHeader.SetRowCount(1);
		m_treeHeader.SetRowCount(1);
		break;

	case GTLC_DISPLAY_WEEKS_SHORT:
	case GTLC_DISPLAY_WEEKS_MID:
	case GTLC_DISPLAY_WEEKS_LONG:
	case GTLC_DISPLAY_DAYS_SHORT:
	case GTLC_DISPLAY_DAYS_MID:
	case GTLC_DISPLAY_DAYS_LONG:
		m_listHeader.SetRowCount(2);
		m_treeHeader.SetRowCount(2);
		break;
		
	default:
		ASSERT(0);
		break;
	}

	return ZoomTo(nNewDisplay, max(MIN_MONTH_WIDTH, nMinMonthWidth));
}

BOOL CGanttTreeListCtrl::ZoomTo(GTLC_MONTH_DISPLAY nNewDisplay, int nNewMonthWidth)
{
	ASSERT (nNewDisplay >= GTLC_DISPLAY_FIRST && nNewDisplay <= GTLC_DISPLAY_LAST);

	// validate new display
	if (nNewDisplay < GTLC_DISPLAY_FIRST || nNewDisplay > GTLC_DISPLAY_LAST)
		return FALSE;

	// validate month width
	if (nNewMonthWidth < 10/* || nNewMonthWidth > 1000*/)
		return FALSE;

	// cache the current scroll-pos so we can restore it
	HWND hwndList = GetList();
	int nScrollPos = ::GetScrollPos(hwndList, SB_HORZ);

	COleDateTime dtPos;
	VERIFY(GetDateFromScrollPos(nScrollPos, dtPos));

	// clear display cache since it's all about to change
	m_display.RemoveAll();

	// do the update
	CGanttLockUpdates glu(this, FALSE, FALSE);

	if (nNewDisplay != m_nMonthDisplay)
	{
		int nNewColWidth = GetColumnWidth(nNewDisplay, nNewMonthWidth);

		m_nMonthWidth = nNewMonthWidth;
		m_nMonthDisplay = nNewDisplay;

		UpdateListColumns(nNewColWidth);
	}
	else
	{
		int nCurColWidth = GetColumnWidth();
		int nNewColWidth = GetColumnWidth(m_nMonthDisplay, nNewMonthWidth);

		RecalcListColumnWidths(nCurColWidth, nNewColWidth);

		m_nMonthWidth = GetMonthWidth(nNewColWidth);
	}

	RefreshSize(GetTree());

	// restore scroll-pos
	if (nScrollPos)
	{
		nScrollPos = GetScrollPosFromDate(dtPos);
		ListView_Scroll(hwndList, nScrollPos - ::GetScrollPos(hwndList, SB_HORZ), 0);
	}

	return TRUE;
}

BOOL CGanttTreeListCtrl::ZoomBy(int nAmount)
{
	int nNewMonthWidth = (m_nMonthWidth + nAmount);

	// will this trigger a min-width change?
	GTLC_MONTH_DISPLAY nNewMonthDisplay = GetColumnDisplay(nNewMonthWidth);

	return ZoomTo(nNewMonthDisplay, nNewMonthWidth);
}

void CGanttTreeListCtrl::RecalcListColumnWidths(int nFromWidth, int nToWidth)
{
	HWND hwndList = GetList();

	// resize the required number of columns
	int nNumReqColumns = GetRequiredColumnCount(), i;

	for (i = 1; i <= nNumReqColumns; i++)
	{
		int nWidth = ListView_GetColumnWidth(hwndList, i);

		if (nFromWidth < nToWidth && nWidth < nToWidth)
		{
			ListView_SetColumnWidth(hwndList, i, nToWidth);
		}
		else if (nFromWidth > nToWidth && nWidth > nToWidth)
		{
			ListView_SetColumnWidth(hwndList, i, nToWidth);
		}
	}

	// and zero out the rest
	for (; i <= GetNumMonths(); i++)
	{
		ListView_SetColumnWidth(hwndList, i, 0);
	}
}

void CGanttTreeListCtrl::UpdateColumnsWidthAndText(int nWidth)
{
	int nNumReqColumns = GetRequiredColumnCount(), i;
	HDITEM hdi = { HDI_TEXT | HDI_LPARAM | HDI_WIDTH, 0 };

	if (nWidth == -1)
		nWidth = GetColumnWidth();

	for (i = 1; i <= nNumReqColumns; i++)
	{
		int nYear = 0, nMonth = 0;

		switch (m_nMonthDisplay)
		{
		case GTLC_DISPLAY_YEARS:
			nMonth = 1;
			nYear = GetStartYear() + (i - 1);
			break;
			
		case GTLC_DISPLAY_QUARTERS_SHORT:
		case GTLC_DISPLAY_QUARTERS_MID:
		case GTLC_DISPLAY_QUARTERS_LONG:
			// each column represents 3 months
			// the first month of each quarter has the 
			// indices: 1, 4, 7, 10
			nMonth = (((i - 1) % 4) * 3) + 1;
			nYear = (GetStartYear() + ((i - 1) / 4));
			break;
			
		case GTLC_DISPLAY_MONTHS_SHORT:
		case GTLC_DISPLAY_MONTHS_MID:
		case GTLC_DISPLAY_MONTHS_LONG:
			// fall thru

		case GTLC_DISPLAY_WEEKS_SHORT:
		case GTLC_DISPLAY_WEEKS_MID:
		case GTLC_DISPLAY_WEEKS_LONG:
			// fall thru

		case GTLC_DISPLAY_DAYS_SHORT:
		case GTLC_DISPLAY_DAYS_MID:
		case GTLC_DISPLAY_DAYS_LONG:
			nMonth = ((i - 1) % 12) + 1;
			nYear = GetStartYear() + ((i - 1) / 12);
			break;
			
		default:
			ASSERT(0);
			break;
		}

		if (nMonth && nYear)
		{
			CString sTitle = FormatColumnHeaderText(m_nMonthDisplay, nMonth, nYear);
			DWORD dwData = MAKELONG(nMonth, nYear);

			m_listHeader.SetItem(i, nWidth, sTitle, dwData);
		}
	}

	// for the rest, clear the text and item data
	for (; i <= GetNumMonths(); i++)
		m_listHeader.SetItem(i, 0, _T(""), 0);
}

void CGanttTreeListCtrl::BuildListColumns()
{
	// once only
	if (m_listHeader.GetItemCount())
		return;

	HWND hwndList = GetList();
	LVCOLUMN lvc = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, 0 };

	// add empty column as placeholder so we can
	// easily replace the other columns without
	// losing all our items too
	ListView_InsertColumn(hwndList, 0, &lvc);

	// add other columns
	for (int i = 0; i < GetNumMonths(); i++)
	{
		lvc.cx = 0;
		lvc.fmt = LVCFMT_CENTER | HDF_STRING;
		lvc.pszText = _T("");
		lvc.cchTextMax = 50;

		int nCol = m_listHeader.GetItemCount();
		ListView_InsertColumn(hwndList, nCol, &lvc);
	}

	UpdateColumnsWidthAndText();
}

void CGanttTreeListCtrl::UpdateListColumns(int nWidth)
{
	// cache the scrolled position
	HWND hwndList = GetList();
	int nScrollPos = ::GetScrollPos(hwndList, SB_HORZ);

	COleDateTime dtPos;
	BOOL bRestorePos = GetDateFromScrollPos(nScrollPos, dtPos);

	if (nWidth == -1)
		nWidth = GetColumnWidth();

	int nMoreMonths = (GetNumMonths() - (m_listHeader.GetItemCount() - 1));

	if (nMoreMonths > 0)
	{
		// add other columns
		LVCOLUMN lvc = { LVCF_FMT | LVCF_WIDTH | LVCF_TEXT, 0 };

		for (int i = 0; i < nMoreMonths; i++)
		{
			lvc.cx = 0;
			lvc.fmt = LVCFMT_CENTER | HDF_STRING;
			lvc.pszText = _T("");
			lvc.cchTextMax = 50;

			int nCol = m_listHeader.GetItemCount();
			ListView_InsertColumn(hwndList, nCol, &lvc);
		}
	}

	UpdateColumnsWidthAndText(nWidth);

	// restore scroll-pos
	if (bRestorePos)
	{
		nScrollPos = GetScrollPosFromDate(dtPos);
		ListView_Scroll(hwndList, nScrollPos - ::GetScrollPos(hwndList, SB_HORZ), 0);
	}
	else
		::SetScrollPos(hwndList, SB_HORZ, 0, TRUE);

	// clear display cache since it's probably going to change
	m_display.RemoveAll();
}

void CGanttTreeListCtrl::CalculateMinMonthWidths()
{
	m_aMinMonthWidths.SetSize(GTLC_DISPLAY_COUNT);

	CClientDC dcClient(&m_treeHeader);
	CFont* pOldFont = GraphicsMisc::PrepareDCFont(&dcClient, CWnd::FromHandle(GetList()));

	for (int nCol = 0; nCol < GTLC_DISPLAY_COUNT; nCol++)
	{
		GTLC_MONTH_DISPLAY nDisplay = (GTLC_MONTH_DISPLAY)nCol;
		int nMinMonthWidth = 0;

		switch (nDisplay)
		{
			case GTLC_DISPLAY_YEARS:
				{
					CString sText = FormatColumnHeaderText(nDisplay, 1, 2013);

					int nMinTextWidth = GraphicsMisc::GetTextWidth(&dcClient, sText);
					nMinMonthWidth = (nMinTextWidth + COLUMN_PADDING) / 12;
				}
				break;

			case GTLC_DISPLAY_QUARTERS_SHORT:
				{
					CString sText = FormatColumnHeaderText(nDisplay, 1, 2013);

					int nMinTextWidth = GraphicsMisc::GetTextWidth(&dcClient, sText);
					nMinMonthWidth = (nMinTextWidth + COLUMN_PADDING) / 3;
				}
				break;

			case GTLC_DISPLAY_QUARTERS_MID:
			case GTLC_DISPLAY_QUARTERS_LONG:
				{
					int nMinTextWidth = 0;

					for (int nMonth = 1; nMonth <= 12; nMonth += 3)
					{
						CString sText = FormatColumnHeaderText(nDisplay, nMonth, 2013);

						int nWidth = GraphicsMisc::GetTextWidth(&dcClient, sText);
						nMinTextWidth = max(nWidth, nMinTextWidth);
					}

					nMinMonthWidth = (nMinTextWidth + COLUMN_PADDING) / 3;
				}
				break;

			case GTLC_DISPLAY_MONTHS_SHORT:
			case GTLC_DISPLAY_MONTHS_MID:
			case GTLC_DISPLAY_MONTHS_LONG:
				{
					int nMinTextWidth = 0;

					for (int nMonth = 1; nMonth <= 12; nMonth++)
					{
						CString sText = FormatColumnHeaderText(nDisplay, nMonth, 2013);

						int nWidth = GraphicsMisc::GetTextWidth(&dcClient, sText);
						nMinTextWidth = max(nWidth, nMinTextWidth);
					}

					nMinMonthWidth = (nMinTextWidth + COLUMN_PADDING);
				}
				break;

			case GTLC_DISPLAY_WEEKS_SHORT:
			case GTLC_DISPLAY_WEEKS_MID:
			case GTLC_DISPLAY_WEEKS_LONG:
				// fall thru

			case GTLC_DISPLAY_DAYS_SHORT:
			case GTLC_DISPLAY_DAYS_MID:
			case GTLC_DISPLAY_DAYS_LONG:
				// just increase the length of the preceding display
				nMinMonthWidth = (int)(m_aMinMonthWidths[nCol-1] * DAY_WEEK_MULTIPLIER);
				break;

			default:
				ASSERT(0);
				break;
		}

		if (nMinMonthWidth > 0)
		{
			nMinMonthWidth++; // for rounding
			m_aMinMonthWidths[nCol] = nMinMonthWidth; 
		}
	}

	dcClient.SelectObject(pOldFont);
}

GTLC_MONTH_DISPLAY CGanttTreeListCtrl::GetColumnDisplay(int nMonthWidth)
{
	int nNumDisplay = GTLC_DISPLAY_COUNT;
	int nMinWidth = 0;

	for (int nDisp = GTLC_DISPLAY_FIRST; nDisp < GTLC_DISPLAY_LAST; nDisp++)
	{
		int nFrom = m_aMinMonthWidths[nDisp];
		int nTo = m_aMinMonthWidths[nDisp+1];

		if (nMonthWidth >= nFrom && nMonthWidth < nTo)
			return (GTLC_MONTH_DISPLAY)nDisp;
	}

	return GTLC_DISPLAY_LAST;
}

int CALLBACK CGanttTreeListCtrl::SortProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	CGanttTreeListCtrl* pThis = (CGanttTreeListCtrl*)lParamSort;
	GANTTITEM gi1, gi2;

	int nCompare = 0;

	if (pThis->GetGanttItem(lParam1, gi1) && pThis->GetGanttItem(lParam2, gi2))
	{
		switch (pThis->m_nSortBy)
		{
		case TREE_COLUMN_TITLE:
			nCompare = gi1.sTitle.CompareNoCase(gi2.sTitle);
			break;
			
		case TREE_COLUMN_STARTDATE:
			nCompare = (int)(gi1.dtStart - gi2.dtStart);
			break;
			
		case TREE_COLUMN_ENDDATE:
			nCompare = (int)(gi1.dtDue - gi2.dtDue);
			break;
			
		case TREE_COLUMN_ALLOCTO:
			nCompare = gi1.sAllocTo.CompareNoCase(gi2.sAllocTo);
			break;
		}
	}

	return (pThis->m_bSortAscending ? nCompare : -nCompare);
}

void CGanttTreeListCtrl::ScrollToToday()
{
	ScrollTo(COleDateTime::GetCurrentTime());
}

void CGanttTreeListCtrl::ScrollToSelectedTask()
{
	GANTTITEM gi;

	if (GetGanttItem(GetSelectedTaskID(), gi))
	{
		COleDateTime dtStart, dtDue;

		if (GetStartDueDates(gi, dtStart, dtDue))
		{
			ScrollTo(dtStart);
		}
		else if (HasDoneDate(gi))
		{
			ScrollTo(gi.dtDone);
		}
	}
}

void CGanttTreeListCtrl::ScrollTo(const COleDateTime& date)
{
	HWND hwndList = GetList();
	COleDateTime dateOnly = CDateHelper::GetDateOnly(date);

	int nStartPos = GetScrollPosFromDate(dateOnly);
	int nEndPos = GetScrollPosFromDate(dateOnly.m_dt + 1.0);

	// if it is already visible no need to scroll
	int nListStart = ::GetScrollPos(hwndList, SB_HORZ);

	CRect rList;
	::GetClientRect(hwndList, rList);

	if (nStartPos >= nListStart && nEndPos <= (nListStart + rList.Width()))
		return;

	// deduct current scroll pos for relative move
	nStartPos -= nListStart;

	// and arbitrary offset
	nStartPos -= 50;

	ListView_Scroll(hwndList, nStartPos, 0);
}

BOOL CGanttTreeListCtrl::GetListColumnRect(int nCol, CRect& rColumn, BOOL bScrolled) const
{
	HWND hwndList = GetList();
	
	if (ListView_GetSubItemRect(hwndList, 0, nCol, LVIR_BOUNDS, (LPRECT)rColumn))
	{
		if (!bScrolled)
			rColumn.OffsetRect(::GetScrollPos(hwndList, SB_HORZ), 0);

		return TRUE;
	}

	return FALSE;
}

BOOL CGanttTreeListCtrl::GetDateFromScrollPos(int nScrollPos, COleDateTime& date) const
{
	// find the column containing this scroll pos
	int nCol = FindColumn(nScrollPos);

	if (nCol != -1)
	{
		CRect rColumn;
		VERIFY(GetListColumnRect(nCol, rColumn, FALSE));
		ASSERT(nScrollPos >= rColumn.left && nScrollPos < rColumn.right);

		int nYear, nMonth;
		VERIFY(GetListColumnDate(nCol, nMonth, nYear));

		// further clip 'rColumn' if in 'Year' or 'Quarter' mode
		switch (m_nMonthDisplay)
		{
		case GTLC_DISPLAY_YEARS:
		case GTLC_DISPLAY_QUARTERS_SHORT:
		case GTLC_DISPLAY_QUARTERS_MID:
		case GTLC_DISPLAY_QUARTERS_LONG:
			{
				int nMonthWidth = GetMonthWidth(rColumn.Width());

				// calc month as offset to start of column
				int nOffset = ((nScrollPos - rColumn.left) / nMonthWidth);

				// clip rect to this month
				rColumn.left += (nOffset * nMonthWidth);
				rColumn.right = (rColumn.left + nMonthWidth);

				nMonth += nOffset;
			}
			break;
		}

		int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);
		int nNumMins = MulDiv(nScrollPos - rColumn.left, (60 * 24 * nDaysInMonth), rColumn.Width());

		int nDay = (1 + (nNumMins / MINS_IN_DAY));
		int nHour = ((nNumMins % MINS_IN_DAY) / MINS_IN_HOUR);
		int nMin = (nNumMins % MINS_IN_HOUR);

		ASSERT(nDay >= 1 && nDay <= nDaysInMonth);
		ASSERT(nHour >= 0 && nHour < 24);

		date.SetDateTime(nYear, nMonth, nDay, nHour, nMin, 0);
		return TRUE;
	}

	// else
	return FALSE;
}

int CGanttTreeListCtrl::GetScrollPosFromDate(const COleDateTime& date) const
{
	// figure out which column contains 'date'
	int nCol = FindColumn(date);

	if (nCol != -1)
	{
		CRect rColumn;

		if (GetListColumnRect(nCol, rColumn, FALSE))
		{
			int nDay = date.GetDay();
			int nMonth = date.GetMonth();
			int nYear = date.GetYear();

			// then figure what proportion of the current month
			int nDaysInMonth = CDateHelper::GetDaysInMonth(nMonth, nYear);

			return (rColumn.left + MulDiv(rColumn.Width(), nDay-1, nDaysInMonth));
		}
	}

	// else
	return 0;
}

int CGanttTreeListCtrl::GetDateInMonths(int nMonth, int nYear) const
{
	return ((nYear * 12) + (nMonth - 1));
}

int CGanttTreeListCtrl::FindColumn(int nMonth, int nYear) const
{
	int nMonths = GetDateInMonths(nMonth, nYear);
	int nNumReqColumns = GetRequiredColumnCount();

	for (int i = 1; i < nNumReqColumns; i++)
	{
		// get date for current column
		VERIFY (GetListColumnDate(i, nMonth, nYear));

		if (nMonths >= GetDateInMonths(nMonth, nYear))
		{
			// get date for next column
			VERIFY (GetListColumnDate(i+1, nMonth, nYear));

			if (nMonths <= GetDateInMonths(nMonth, nYear))
			{
				return i;
			}
		}
	}

	return -1;
}

int CGanttTreeListCtrl::FindColumn(const COleDateTime& date) const
{
	return FindColumn(date.GetMonth(), date.GetYear());
}

int CGanttTreeListCtrl::FindColumn(int nScrollPos) const
{
	int nNumReqColumns = GetRequiredColumnCount();

	for (int i = 1; i <= nNumReqColumns; i++)
	{
		CRect rColumn;
		VERIFY(GetListColumnRect(i, rColumn, FALSE));

		if (nScrollPos >= rColumn.left && nScrollPos < rColumn.right)
			return i;
	}

	// not found
	return -1;
}

bool CGanttTreeListCtrl::PrepareNewTask(ITaskList* pTask) const
{
	HTASKITEM hNewTask = pTask->GetFirstTask();
	ASSERT(hNewTask);

	// give the task a date that will make it appear in the chart
// 	double dDate = (GetMaxDate().m_dt + GetMinDate().m_dt) / 2;
// 
// 	pTask->SetTaskStartDate(hNewTask, CDateHelper::GetTimeT(dDate));
// 	pTask->SetTaskDueDate(hNewTask, CDateHelper::GetTimeT(dDate));

	return true;
}

DWORD CGanttTreeListCtrl::HitTest(const CPoint& ptScreen) const
{
	GTLC_HITTEST nHit = GTLCHT_NOWHERE;
	return HitTest(ptScreen, nHit);
}

BOOL CGanttTreeListCtrl::PtInHeader(const CPoint& ptScreen) const
{
	CRect rHeader;

	// try tree
	m_treeHeader.GetWindowRect(rHeader);

	if (rHeader.PtInRect(ptScreen))
		return TRUE;

	// then list
	m_listHeader.GetWindowRect(rHeader);

	return rHeader.PtInRect(ptScreen);
}

void CGanttTreeListCtrl::GetWindowRect(CRect& rWindow, BOOL bWithHeader) const
{
	CRect rTree, rList;

	::GetWindowRect(m_hwndTree, rTree);
	::GetWindowRect(m_hwndList, rList);

	if (bWithHeader)
		rWindow = rList; // height will include header
	else
		rWindow = rTree; // height will not include header

	rWindow.left  = min(rTree.left, rList.left);
	rWindow.right = max(rTree.right, rList.right);
}

DWORD CGanttTreeListCtrl::HitTest(const CPoint& ptScreen, GTLC_HITTEST& nHit) const
{
	nHit = GTLCHT_NOWHERE;

	if (m_data.GetCount() == 0)
		return 0;

	// only interested in list
	HWND hwndList = GetList();

	// convert to list coords
	LVHITTESTINFO lvht = { 0 };

	lvht.pt = ptScreen;
	::ScreenToClient(hwndList, &(lvht.pt));

	if (ListView_SubItemHitTest(hwndList, &lvht) != -1)
	{
		ASSERT(lvht.iItem != -1);
		ASSERT(lvht.iSubItem > 0);

		// convert pos to date
		COleDateTime dtHit;

		if (!GetDateFromPoint(lvht.pt, dtHit))
			return 0;

		// get task item and see where we hit
		DWORD dwTaskID = GetListItemData(hwndList, lvht.iItem);
		GANTTITEM gi;

		VERIFY(GetGanttItem(dwTaskID, gi));

		COleDateTime dtStart, dtDue;
		GetStartDueDates(gi, dtStart, dtDue);

		// now check for closeness to ends
		double dDateTol = CalcDateDragTolerance(lvht.iSubItem);
		
		if (fabs(dtHit.m_dt - dtStart.m_dt) < dDateTol)
		{
			nHit = GTLCHT_BEGIN;
		}
		else if (fabs(dtHit.m_dt - dtDue.m_dt) < dDateTol)
		{
			nHit = GTLCHT_END;
		}
		else if (dtHit > dtStart && dtHit < dtDue)
		{
			nHit = GTLCHT_MIDDLE;
		}

		return dwTaskID;
	}

	// nothing hit
	return 0;
}

double CGanttTreeListCtrl::CalcDateDragTolerance(int nCol) const
{
	ASSERT(nCol >= 1 && nCol <= GetRequiredColumnCount());

 	CRect rCol;
 	VERIFY(GetListColumnRect(1, rCol));

	int nMonth = GetMonthWidth(rCol.Width());
	double dOneDay = (rCol.Width() / 30.33);

	if (dOneDay > 0.0)
		return (GetSystemMetrics(SM_CXSIZEFRAME) / dOneDay);

	// else
	return 0.25;
}

BOOL CGanttTreeListCtrl::IsDragging() const
{
	return (m_bDragging || m_bDraggingStart || m_bDraggingEnd);
}

BOOL CGanttTreeListCtrl::IsValidDrag(const CPoint& ptDrag) const
{
	if (!IsDragging())
		return FALSE;

	CSize size = (m_ptDragStart - ptDrag);

	int nCxDrag = (GetSystemMetrics(SM_CXDRAG) / 2);
	int nCyDrag = (GetSystemMetrics(SM_CYDRAG) / 2);

	return ((abs(size.cx) > nCxDrag) || 
			(abs(size.cy) > nCyDrag));
}

BOOL CGanttTreeListCtrl::ValidateDragPoint(CPoint& ptDrag) const
{
	if (!IsDragging())
		return FALSE;

	CRect rClient;
	::GetClientRect(GetList(), rClient);

	ptDrag.x = max(ptDrag.x, rClient.left);
	ptDrag.x = min(ptDrag.x, rClient.right);
	ptDrag.y = max(ptDrag.y, rClient.top);
	ptDrag.y = min(ptDrag.y, rClient.bottom);

	return TRUE;
}

BOOL CGanttTreeListCtrl::StartDragging(const CPoint& ptCursor)
{
	GTLC_HITTEST nHit = GTLCHT_NOWHERE;
	HWND hwndList = GetList();
	
	CPoint ptScreen(ptCursor);
	::ClientToScreen(hwndList, &ptScreen);

	DWORD dwTaskID = HitTest(ptScreen, nHit);
	ASSERT((nHit == GTLCHT_NOWHERE) || (dwTaskID != 0));

	if (nHit == GTLCHT_NOWHERE)
		return FALSE;

	if (dwTaskID != GetSelectedTaskID())
		SelectTask(dwTaskID);
	
	switch (nHit)
	{
	case GTLCHT_BEGIN:
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		m_bDraggingStart = TRUE;
		break;
		
	case GTLCHT_END:
		m_bDraggingEnd = TRUE;
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		break;
		
	case GTLCHT_MIDDLE:
		m_bDragging = TRUE;
		break;
		
	default:
		ASSERT(0);
		return FALSE;
	}
	
	// cache the original task
	VERIFY(GetGanttItem(dwTaskID, m_giPreDrag, FALSE));

	m_ptDragStart = ptCursor;

	::SetCapture(hwndList);

	// keep parent informed
	NotifyParentDragChange();

	return TRUE;
}

BOOL CGanttTreeListCtrl::EndDragging(const CPoint& ptCursor)
{
	if (IsDragging())
	{
		DWORD dwTaskID = GetSelectedTaskID();
		
		GANTTITEM gi;
		VERIFY (GetGanttItem(dwTaskID, gi));

		// update taskID to refID because we're really dragging the refID
		if (gi.dwRefID)
		{
			dwTaskID = gi.dwRefID;
			gi.dwRefID = 0;
		}

		// dropping outside the list is a cancel
		CRect rList;
		::GetClientRect(GetList(), rList);
		
		if (!rList.PtInRect(ptCursor) || !IsValidDrag(ptCursor))
		{
			m_data[dwTaskID] = m_giPreDrag;
		}
		else if (m_bDraggingStart)
		{
			NotifyParentDateChange(GTLCHT_BEGIN);
		}
		else if (m_bDraggingEnd)
		{
			NotifyParentDateChange(GTLCHT_END);
		}
		else
		{
			ASSERT(m_bDragging);
			NotifyParentDateChange(GTLCHT_MIDDLE);
		}
		
		// cleanup
		m_bDraggingStart = m_bDraggingEnd = m_bDragging = FALSE;

		::ReleaseCapture();
		RedrawList();

		// keep parent informed
		NotifyParentDragChange();

		return TRUE;
	}

	// else
	return FALSE;
				
}

void CGanttTreeListCtrl::NotifyParentDragChange()
{
	ASSERT(GetSelectedTaskID());

	GetCWnd()->SendMessage(WM_GANTT_DRAGCHANGE, (WPARAM)GetSnapMode(), GetSelectedTaskID());
}

void CGanttTreeListCtrl::NotifyParentDateChange(GTLC_HITTEST nHit)
{
	ASSERT(GetSelectedTaskID());
	ASSERT(nHit != GTLCHT_NOWHERE);

	GetCWnd()->SendMessage(WM_GANTT_DATECHANGE, (WPARAM)nHit, GetSelectedTaskID());
}

BOOL CGanttTreeListCtrl::UpdateDragging(const CPoint& ptCursor)
{
	if (IsDragging())
	{
		DWORD dwTaskID = GetSelectedTaskID();
		
		GANTTITEM gi;
		VERIFY (GetGanttItem(dwTaskID, gi));

		COleDateTime dtStart, dtDue;
		GetStartDueDates(gi, dtStart, dtDue);

		// update taskID to refID because we're really dragging the refID
		if (gi.dwRefID)
		{
			dwTaskID = gi.dwRefID;
			gi.dwRefID = 0;
		}
		
		if (IsValidDrag(ptCursor))
		{
			COleDateTime dtDrag;
			
			if (GetValidDragDate(ptCursor, dtDrag))
			{
				// prevent the start and end dates from overlapping
				if (m_bDraggingStart)
				{
					gi.dtStart.m_dt = min(dtDrag.m_dt, dtDue.m_dt - 1.0);
				}
				else if (m_bDraggingEnd)
				{
					gi.dtDue.m_dt = max(dtDrag.m_dt, dtStart.m_dt + 1.0);
				}
				else 
				{
					double dDuration = (dtDue.m_dt - dtStart.m_dt);
					
					gi.dtStart = dtDrag;
					gi.dtDue = dtDrag.m_dt + dDuration;
				}

				m_data[dwTaskID] = gi;
			}
		}
		else
		{
			m_data[dwTaskID] = m_giPreDrag;
		}
			
		RecalcParentDates();
		RedrawList();

		// keep parent informed
		NotifyParentDragChange();

		return TRUE;
	}

	// else
	return FALSE;
}

void CGanttTreeListCtrl::RedrawList(BOOL bErase)
{
	::InvalidateRect(GetList(), NULL, bErase);
	::UpdateWindow(GetList());
}

void CGanttTreeListCtrl::RedrawTree(BOOL bErase)
{
	::InvalidateRect(GetTree(), NULL, bErase);
	::UpdateWindow(GetTree());
}

BOOL CGanttTreeListCtrl::GetValidDragDate(const CPoint& ptCursor, COleDateTime& dtDrag) const
{
	CPoint ptDrag(ptCursor);

	if (!ValidateDragPoint(ptDrag))
		return FALSE;

	if (!GetDateFromPoint(ptDrag, dtDrag))
		return FALSE;

	// if dragging the whole task, then we calculate
	// dtDrag as GANTTITEM::dtStart offset by the
	// difference between the current drag pos and the
	// initial drag pos
	if (m_bDragging)
	{
		COleDateTime dtOrg;
		GetDateFromPoint(m_ptDragStart, dtOrg);
		
		// offset from pre-drag position
		double dOffset = dtDrag.m_dt - dtOrg.m_dt;
		dtDrag = m_giPreDrag.dtStart.m_dt + dOffset;
		
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL));
	}
	
	// adjust date depending on modifier keys 
	dtDrag = GetNearestDate(dtDrag);
	
	return TRUE;
}

BOOL CGanttTreeListCtrl::GetDateFromPoint(const CPoint& ptCursor, COleDateTime& date) const
{
	HWND hwndList = GetList();

	// quick hit test
	CRect rList;
	::GetClientRect(hwndList, rList);

	if (!rList.PtInRect(ptCursor))
		return FALSE;

	// convert pos to date
	int nScrollPos = ::GetScrollPos(hwndList, SB_HORZ) + ptCursor.x;

	return (GetDateFromScrollPos(nScrollPos, date) && CDateHelper::IsDateSet(date));
}

// external version
BOOL CGanttTreeListCtrl::CancelDrag()
{
	if (IsDragging())
	{
		CancelDrag(TRUE);
		return TRUE;
	}

	// else
	return FALSE;
}

// internal version
void CGanttTreeListCtrl::CancelDrag(BOOL bReleaseCapture)
{
	ASSERT(IsDragging());

	// cancel drag, restoring original task dates
	DWORD dwTaskID = GetSelectedTaskID();

	m_data[dwTaskID] = m_giPreDrag;
	m_bDragging = m_bDraggingStart = m_bDraggingEnd = FALSE;
	
	if (bReleaseCapture)
		ReleaseCapture();

	RedrawList();

	// keep parent informed
	NotifyParentDragChange();
}

GTLC_SNAPMODE CGanttTreeListCtrl::GetSnapMode() const
{
	if (IsDragging())
	{
		BOOL bCtrl = Misc::KeyIsPressed(VK_CONTROL);
		BOOL bShift = Misc::KeyIsPressed(VK_SHIFT);

		if (bCtrl || bShift)
		{
			switch (m_nMonthDisplay)
			{
			case GTLC_DISPLAY_YEARS:
				if (bCtrl && bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTHALFYEAR;
				}
				else if (bCtrl)
				{
					m_nSnapMode = GTLCSM_NEARESTMONTH;
				}
				else if (bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTYEAR;
				}
				break;
				
			case GTLC_DISPLAY_QUARTERS_SHORT:
			case GTLC_DISPLAY_QUARTERS_MID:
			case GTLC_DISPLAY_QUARTERS_LONG:
				if (bCtrl)
				{
					m_nSnapMode = GTLCSM_NEARESTMONTH;
				}
				else if (bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTQUARTER;
				}
				break;
				
			case GTLC_DISPLAY_MONTHS_SHORT:
			case GTLC_DISPLAY_MONTHS_MID:
			case GTLC_DISPLAY_MONTHS_LONG:
				if (bCtrl)
				{
					m_nSnapMode = GTLCSM_NEARESTDAY;
				}
				else if (bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTMONTH;
				}
				break;
				
			case GTLC_DISPLAY_WEEKS_SHORT:
			case GTLC_DISPLAY_WEEKS_MID:
			case GTLC_DISPLAY_WEEKS_LONG:
				if (bCtrl)
				{
					m_nSnapMode = GTLCSM_NEARESTDAY;
				}
				else if (bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTWEEK;
				}
				break;
				
			case GTLC_DISPLAY_DAYS_SHORT:
			case GTLC_DISPLAY_DAYS_MID:
			case GTLC_DISPLAY_DAYS_LONG:
				if (bCtrl && bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTHALFDAY;
				}
				else if (bCtrl)
				{
					m_nSnapMode = GTLCSM_NEARESTHOUR;
				}
				else if (bShift)
				{
					m_nSnapMode = GTLCSM_NEARESTDAY;
				}
				break;
				
			default:
				ASSERT(0);
				// fall thru to GTLCSM_NONE
				break;
			}
		}
	}

	// else
	return m_nSnapMode;
}

COleDateTime CGanttTreeListCtrl::GetNearestDate(const COleDateTime& dtDrag) const
{
	ASSERT(IsDragging());

	switch (GetSnapMode())
	{
	case GTLCSM_NEARESTYEAR:
		return CDateHelper::GetNearestYear(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTHALFYEAR:
		return CDateHelper::GetNearestHalfYear(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTQUARTER:
		return CDateHelper::GetNearestQuarter(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTMONTH:
		return CDateHelper::GetNearestMonth(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTWEEK:
		return CDateHelper::GetNearestWeek(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTDAY:
		return CDateHelper::GetNearestDay(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTHALFDAY:
		return CDateHelper::GetNearestHalfDay(dtDrag, m_bDraggingEnd);

	case GTLCSM_NEARESTHOUR:
		return CDateHelper::GetNearestHour(dtDrag, m_bDraggingEnd);

	case GTLCSM_FREE:
		if (m_bDraggingEnd)
		{
			double dTime = CDateHelper::GetTimeOnly(dtDrag);

			if (dTime >= END_OF_DAY)
				return CDateHelper::MakeDate(dtDrag, END_OF_DAY);
		}
		// else
		return dtDrag;
	}

	// all else
	ASSERT(0);
	return dtDrag;
}
