// TaskCalendarCtrl.cpp : implementation file
//

#include "stdafx.h"
#include "TaskCalendarCtrl.h"

#include "..\todolist\tdcenum.h"

#include "..\Shared\GraphicsMisc.h"
#include "..\Shared\themed.h"
#include "..\Shared\DateHelper.h"
#include "..\Shared\misc.h"

#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

const double ONE_HOUR = (1.0 / 24.0);
const double END_OF_DAY = 0.999988425923; //(((24 * 60 * 60) - 1) / (24.0 * 60 * 60));
const int MIN_TEXT_HEIGHT = (DEF_TASK_HEIGHT - 2);

/////////////////////////////////////////////////////////////////////////////

TASKCALITEM::TASKCALITEM()
	: 
	crText(CLR_NONE), 
	crBack(CLR_NONE),
	dwTaskID(0)
{

}
	
TASKCALITEM::TASKCALITEM(const ITaskList12* pTasks, HTASKITEM hTask, DWORD dwCalcDates) 
	: 
	crText(CLR_NONE), 
	crBack(CLR_NONE),
	dwTaskID(0)
{
	UpdateTask(pTasks, hTask, TDCA_ALL, dwCalcDates);
}

void TASKCALITEM::UpdateTaskDates(const ITaskList12* pTasks, HTASKITEM hTask, int nEditAttrib, DWORD dwCalcDates)
{
	// check for quick exit
	BOOL bUpdateStart = (nEditAttrib == TDCA_ALL || nEditAttrib == TDCA_STARTDATE);
	BOOL bUpdateEnd = (nEditAttrib == TDCA_ALL || nEditAttrib == TDCA_DUEDATE);

	if (!bUpdateStart && !bUpdateEnd)
		return;

	time64_t tDate = 0;

	// get creation date once only
	if (!CDateHelper::IsDateSet(dtCreation) && pTasks->GetTaskCreationDate64(hTask, tDate))
		dtCreation = GetDate(tDate);

	// retrieve new dates
	if (bUpdateStart && pTasks->GetTaskStartDate64(hTask, FALSE, tDate))
		dtStart = GetDate(tDate);

	if (bUpdateEnd && pTasks->GetTaskDueDate64(hTask, FALSE, tDate))
		dtEnd = GetDate(tDate);

	RecalcDates(dwCalcDates);
}

void TASKCALITEM::RecalcDates(DWORD dwCalcDates)
{
	const COleDateTime dtNow = CDateHelper::GetDateOnly(COleDateTime::GetCurrentTime());

	CDateHelper::ClearDate(dtStartCalc);
	CDateHelper::ClearDate(dtEndCalc);

	BOOL bHasStartDate = IsStartDateSet();
	BOOL bHasEndDate = IsEndDateSet();

	// calculate missing dates
	if (!bHasStartDate)
	{
		if (Misc::HasFlag(dwCalcDates, TCCO_CALCMISSINGSTARTASCREATION))
		{
			if (bHasEndDate)
				dtStartCalc = min(dtEnd, dtCreation);
			else
				dtStartCalc = dtCreation;
		}
		else if (Misc::HasFlag(dwCalcDates, TCCO_CALCMISSINGSTARTASDUE))
		{
			if (bHasEndDate)
				dtStartCalc = dtEnd;
		}
		else //if (Misc::HasFlag(dwCalcDates, TCCO_CALCMISSINGSTARTASEARLIESTDUEANDTODAY))
		{
			if (bHasEndDate)
				dtStartCalc = min(dtEnd, dtNow);
			else 
				dtStartCalc = dtNow;
		}

		dtStartCalc = CDateHelper::GetDateOnly(dtStartCalc);
	}

	if (!bHasEndDate)
	{
		if (Misc::HasFlag(dwCalcDates, TCCO_CALCMISSINGDUEASSTART))
		{
			if (bHasStartDate)
				dtEndCalc = dtStart;
		}
		else //if (Misc::HasFlag(dwCalcDates, TCCO_CALCMISSINGDUEASLATESTSTARTANDTODAY))
		{
			// take the later of today and dtStart
			if (bHasStartDate)
				dtEndCalc = max(dtStart, dtNow);
			else
				dtEndCalc = dtNow;
		}

		dtEndCalc = CDateHelper::GetDateOnly(dtEndCalc);
	}
	else if (bHasStartDate) // special case
	{
		if (dtStart > dtEnd)
			dtStartCalc = CDateHelper::GetDateOnly(dtEnd);
	}

	// adjust dtEnd to point to end of day if it has no time component
	if (IsEndDateSet())
	{
		if (!CDateHelper::DateHasTime(dtEnd))
			dtEnd.m_dt += END_OF_DAY;
	}
	else if (HasAnyEndDate())
	{
		if (!CDateHelper::DateHasTime(dtEndCalc))
			dtEndCalc.m_dt += END_OF_DAY;
	}
}

void TASKCALITEM::UpdateTask(const ITaskList12* pTasks, HTASKITEM hTask, int nEditAttrib, DWORD dwCalcDates)
{
	ASSERT(dwTaskID == 0 || pTasks->GetTaskID(hTask) == dwTaskID);

	if (dwTaskID == 0)
		dwTaskID = pTasks->GetTaskID(hTask);

	if (nEditAttrib == TDCA_ALL || nEditAttrib == TDCA_TASKNAME)
		sName = pTasks->GetTaskTitle(hTask);

	UpdateTaskDates(pTasks, hTask, nEditAttrib, dwCalcDates);

	// always update colour
	crText = pTasks->GetTaskTextColor(hTask);
	crBack = pTasks->GetTaskBkgndColor(hTask);
}

BOOL TASKCALITEM::IsValid() const
{
	return (IsStartDateSet() || IsEndDateSet());
}

COleDateTime TASKCALITEM::GetDate(time64_t tDate)
{
	COleDateTime date;

	if (tDate > 0)
		date = CDateHelper::GetDate(tDate);
	else
		CDateHelper::ClearDate(date);

	// else
	return date;
}

BOOL TASKCALITEM::IsStartDateSet() const
{
	return (CDateHelper::IsDateSet(dtStart) && !CDateHelper::IsDateSet(dtStartCalc));
}

BOOL TASKCALITEM::IsEndDateSet() const
{
	return (CDateHelper::IsDateSet(dtEnd) && !CDateHelper::IsDateSet(dtEndCalc));
}

BOOL TASKCALITEM::HasAnyStartDate() const
{
	return CDateHelper::IsDateSet(GetAnyStartDate());
}

BOOL TASKCALITEM::HasAnyEndDate() const
{
	return CDateHelper::IsDateSet(GetAnyEndDate());
}

COleDateTime TASKCALITEM::GetAnyStartDate() const
{
	// take calculated value in preference
	if (CDateHelper::IsDateSet(dtStartCalc))
		return dtStartCalc;
	else
		return dtStart;
}

COleDateTime TASKCALITEM::GetAnyEndDate() const
{
	// take calculated value in preference
	if (CDateHelper::IsDateSet(dtEndCalc))
		return dtEndCalc;
	else
		return dtEnd;
}

void TASKCALITEM::SetStartDate(const COleDateTime& date)
{
	ASSERT(CDateHelper::IsDateSet(date));

	dtStart = date;
}

void TASKCALITEM::SetEndDate(const COleDateTime& date)
{
	ASSERT(CDateHelper::IsDateSet(date));

	dtEnd = date;
}

COLORREF TASKCALITEM::GetDefaultFillColor() const
{
	if (crBack != CLR_NONE)
		return crBack;

	if ((crText != CLR_NONE) && (crText != 0))
		return crText;

	// else
	return GetSysColor(COLOR_WINDOW);
}

COLORREF TASKCALITEM::GetDefaultBorderColor() const
{
	COLORREF crDefFill = GetDefaultFillColor();

	if (crDefFill == GetSysColor(COLOR_WINDOW))
		return 0;

	// else
	return GraphicsMisc::Darker(crDefFill, 0.5);
}

/////////////////////////////////////////////////////////////////////////////
// CTaskCalendarCtrl

CTaskCalendarCtrl::CTaskCalendarCtrl() 
	: 
	m_nMaxDayTaskCount(0), 
	m_dwSelectedTaskID(0),
	m_bDraggingStart(FALSE), 
	m_bDraggingEnd(FALSE),
	m_bDragging(FALSE),
	m_ptDragOrigin(0),
	m_nSnapMode(TCCSM_NEARESTHOUR),
	m_dwOptions(TCCO_DISPLAYCONTINUOUS)
{
	GraphicsMisc::CreateFont(m_DefaultFont, _T("Tahoma"));
}

CTaskCalendarCtrl::~CTaskCalendarCtrl()
{
	DeleteData();

	// clean up other heap allocations
	for(int i=0; i<CALENDAR_MAX_ROWS ; i++)
	{
		for(int u=0; u<CALENDAR_NUM_COLUMNS ; u++)
		{
			CCalendarCell* pCell = GetCell(i, u);

			delete static_cast<CTaskCalItemArray*>(pCell->pUserData);
			pCell->pUserData = NULL;
		}
	}
}

BEGIN_MESSAGE_MAP(CTaskCalendarCtrl, CCalendarCtrl)
	//{{AFX_MSG_MAP(CTaskCalendarCtrl)
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()
	ON_WM_LBUTTONUP()
	ON_WM_CAPTURECHANGED()
	ON_WM_KEYDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTaskCalendarCtrl message handlers

int CTaskCalendarCtrl::OnToolHitTest(CPoint point, TOOLINFO* pTI) const
{
	// perform a hit-test
	DWORD dwTaskID = HitTest(point);

	if (dwTaskID)
	{
		int nTextOffset = GetTaskTextOffset(dwTaskID);

		if ((nTextOffset > 0) || 
			!HasOption(TCCO_DISPLAYCONTINUOUS) ||
			(GetTaskHeight() < MIN_TEXT_HEIGHT))
		{
			pTI->hwnd = GetSafeHwnd();
			pTI->uId = dwTaskID;
			pTI->uFlags |= TTF_ALWAYSTIP;

			const TASKCALITEM* pTCI = GetTaskCalItem(dwTaskID);
			ASSERT(pTCI);

			pTI->lpszText = _tcsdup(pTCI->sName);

			GetClientRect(&(pTI->rect));

			return TRUE;
		}
	}

	return CCalendarCtrl::OnToolHitTest(point, pTI);
}

void CTaskCalendarCtrl::SetOption(DWORD dwOption, BOOL bSet)
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
			RecalcTaskDates();
		}
	}
}

void CTaskCalendarCtrl::RecalcTaskDates()
{
	POSITION pos = m_mapData.GetStartPosition();
	DWORD dwTaskID = 0;
	TASKCALITEM* pTCI = NULL;

	while (pos)
	{
		m_mapData.GetNextAssoc(pos, dwTaskID, pTCI);
		ASSERT(pTCI);
		ASSERT(pTCI->dwTaskID == dwTaskID);

		pTCI->RecalcDates(m_dwOptions);
	}
}

bool CTaskCalendarCtrl::PrepareNewTask(ITaskList* pTask) const
{
	// give the task a date that will make it appear in the calendar
	time_t tDate = DateToSeconds((GetMaxDate().m_dt + GetMinDate().m_dt) / 2);

	int nRow, nCol;

	if (GetLastSelectedGridCell(nRow, nCol))
	{
		const CCalendarCell* pCell = GetCell(nRow, nCol);
		ASSERT(pCell);

		tDate = DateToSeconds(pCell->date);
	}

	if (tDate <= 0)
		return false;

	HTASKITEM hNewTask = pTask->GetFirstTask();
	ASSERT(hNewTask);

	pTask->SetTaskStartDate(hNewTask, tDate);
	pTask->SetTaskDueDate(hNewTask, tDate);

	return true;
}

void CTaskCalendarCtrl::UpdateTasks(const ITaskList* pTasks, IUI_UPDATETYPE nUpdate, int nEditAttribute)
{
	const ITaskList12* pTasks12 = GetITLInterface<ITaskList12>(pTasks, IID_TASKLIST12);

	switch (nUpdate)
	{
	case IUI_ALL:
		DeleteData();
		BuildData(pTasks12, pTasks->GetFirstTask());
		break;
		
	case IUI_EDIT:
		UpdateTask(pTasks12, pTasks12->GetFirstTask(), nUpdate, nEditAttribute);
		break;
		
	case IUI_DELETE:
		RemoveDeletedTasks(pTasks12);
		break;
		
	case IUI_ADD:
	case IUI_MOVE:
		ASSERT(0);
		break;
		
	default:
		ASSERT(0);
	}
	
	Invalidate(FALSE);
}

void CTaskCalendarCtrl::RemoveDeletedTasks(const ITaskList12* pTasks)
{
	// traverse the data looking for items that do not 
	// exist in pTasks and delete them
	POSITION pos = m_mapData.GetStartPosition();
	DWORD dwTaskID = 0;
	TASKCALITEM* pTCI = NULL;

	while (pos)
	{
		m_mapData.GetNextAssoc(pos, dwTaskID, pTCI);
		ASSERT(pTCI);
		ASSERT(pTCI->dwTaskID);

		if (pTasks->FindTask(pTCI->dwTaskID) == NULL)
		{
			delete pTCI;
			m_mapData.RemoveKey(dwTaskID);

			// clear selection if necessary
			if (m_dwSelectedTaskID == dwTaskID)
				m_dwSelectedTaskID = 0;
		}
	}

	Invalidate(FALSE);
}

void CTaskCalendarCtrl::RemoveCompletedTask(const ITaskList12* pTasks, HTASKITEM hTask)
{
	ASSERT(pTasks->IsTaskDone(hTask));

	// delete this task and then its subtasks
	DWORD dwTaskID = pTasks->GetTaskID(hTask);

	if (HasTask(dwTaskID))
	{
		TASKCALITEM* pTCI = GetTaskCalItem(dwTaskID);

		delete pTCI;
		m_mapData.RemoveKey(dwTaskID);
	}

	// subtasks
	HTASKITEM hSubTask = pTasks->GetFirstTask(hTask);

	while (hSubTask)
	{
		RemoveCompletedTask(pTasks, hSubTask);
		hSubTask = pTasks->GetNextTask(hSubTask);
	}
}

void CTaskCalendarCtrl::UpdateTask(const ITaskList12* pTasks, HTASKITEM hTask, IUI_UPDATETYPE nUpdate, int nEditAttribute)
{
	if (hTask == NULL)
		return;

	ASSERT(nUpdate == IUI_EDIT);

	// handle task if not a reference
	if (_ttoi(pTasks->GetTaskAttribute(hTask, _T("REFID"))) > 0)
		return;

	// if the task is completed remove it
	if (pTasks->IsTaskDone(hTask))
	{
		RemoveCompletedTask(pTasks, hTask);
		return;
	}
	else // update task
	{
		DWORD dwTaskID = pTasks->GetTaskID(hTask);

		// Note: because we weed out completed tasks
		// we may come across a task we don't know about
		// if a completed task has just been made incomplete.
		// So we need to detect this an dynamically add as nec.
		if (HasTask(dwTaskID))
		{
			TASKCALITEM* pTCI = GetTaskCalItem(dwTaskID);
			pTCI->UpdateTask(pTasks, hTask, nEditAttribute, m_dwOptions);

			// then children
			UpdateTask(pTasks, pTasks->GetFirstTask(hTask), nUpdate, nEditAttribute);
		}
		else
		{
			ASSERT(nEditAttribute == TDCA_DONEDATE);
			ASSERT(!pTasks->IsTaskDone(hTask));

			BuildData(pTasks, hTask, FALSE);
		}
	}
	
	// next sibling
	UpdateTask(pTasks, pTasks->GetNextTask(hTask), nUpdate, nEditAttribute);
}

void CTaskCalendarCtrl::NotifyParentDateChange(TCC_HITTEST nHit)
{
	ASSERT(m_dwSelectedTaskID);
	ASSERT(nHit != TCCHT_NOWHERE);

	GetParent()->SendMessage(WM_CALENDAR_DATECHANGE, (WPARAM)nHit, m_dwSelectedTaskID);
}

void CTaskCalendarCtrl::NotifyParentDragChange()
{
	ASSERT(m_dwSelectedTaskID);

	GetParent()->SendMessage(WM_CALENDAR_DRAGCHANGE, (WPARAM)GetSnapMode(), m_dwSelectedTaskID);
}

BOOL CTaskCalendarCtrl::IsSpecialDate(const COleDateTime& date) const
{
	BOOL bDummy;
	return m_mapSpecial.Lookup(CDateHelper::GetDateOnly(date).m_dt, bDummy);
}

void CTaskCalendarCtrl::BuildData(const ITaskList12* pTasks, HTASKITEM hTask, BOOL bAndSiblings)
{
	if (hTask == NULL)
		return;

	// handle task unless it's a reference
	if (_ttoi(pTasks->GetTaskAttribute(hTask, _T("REFID"))) > 0)
		return;

	// or it's completed
	if (!pTasks->IsTaskDone(hTask))
	{
		// only interested in leaf-tasks
		HTASKITEM hSubTask = pTasks->GetFirstTask(hTask);

		if (hSubTask == NULL) // leaf-task
		{
			// sanity check
			DWORD dwTaskID = pTasks->GetTaskID(hTask);
			ASSERT(!HasTask(dwTaskID));

			TASKCALITEM* pTCI = new TASKCALITEM(pTasks, hTask, m_dwOptions);
			m_mapData[dwTaskID] = pTCI;

			// process item for special dates
			if (pTCI->IsStartDateSet())
				m_mapSpecial[CDateHelper::GetDateOnly(pTCI->GetAnyStartDate())] = TRUE;

			if (pTCI->IsEndDateSet())
				m_mapSpecial[CDateHelper::GetDateOnly(pTCI->GetAnyEndDate())] = TRUE;
		}
		else // process children
		{
			BuildData(pTasks, hSubTask);
		}
	}

	// then siblings
	if (bAndSiblings)
		BuildData(pTasks, pTasks->GetNextTask(hTask));

	// check that selected task is still valid
	if ((hTask == NULL) && m_dwSelectedTaskID && !HasTask(m_dwSelectedTaskID))
		m_dwSelectedTaskID = 0;
}

void CTaskCalendarCtrl::DeleteData()
{
	POSITION pos = m_mapData.GetStartPosition();
	DWORD dwTaskID = 0;
	TASKCALITEM* pTCI = NULL;

	while (pos)
	{
		m_mapData.GetNextAssoc(pos, dwTaskID, pTCI);
		delete pTCI;
	}

	m_mapData.RemoveAll();
	m_mapSpecial.RemoveAll();
}

void CTaskCalendarCtrl::DrawHeader(CDC* pDC)
{
	CThemed th;
	BOOL bThemed = (th.AreControlsThemed() && th.Open(this, _T("HEADER")));
	
	if (!bThemed)
	{
		CCalendarCtrl::DrawHeader(pDC);
	}
	else
	{
		CRect rc;
		GetClientRect(&rc);

		rc.bottom = CALENDAR_HEADER_HEIGHT;
		int nWidth = (rc.Width() / CALENDAR_NUM_COLUMNS);

		CFont* pOldFont = pDC->SelectObject(&m_DefaultFont);
		bool bShort = false;

		for (int i = 0; i < CALENDAR_NUM_COLUMNS && !bShort; i++)
		{
			int nDOW = GetDayOfWeek(i);

			CString csTitle = CDateHelper::GetWeekdayName(nDOW, FALSE);
			bShort = (pDC->GetTextExtent(csTitle).cx > nWidth);
		}

		CRect rCol(rc);

		for(i = 0 ; i < CALENDAR_NUM_COLUMNS; i++)
		{
			int nDOW = GetDayOfWeek(i);

			if (i == (CALENDAR_NUM_COLUMNS - 1))
			{
				rCol.right = rc.right;
			}
			else if (i == 0)
			{
				rCol.right = nWidth + 1;
			}
			else
			{
				rCol.right = rCol.left + nWidth;
			}

			// draw background
			th.DrawBackground(pDC, HP_HEADERITEM, HIS_NORMAL, rCol);

			CString csTitle = CDateHelper::GetWeekdayName(nDOW, bShort);
			CRect rText(rCol);
			rText.DeflateRect(0, 2, 0, 0);

			pDC->DrawText(csTitle, rText, DT_CENTER|DT_VCENTER);

			// next column
			rCol.left = rCol.right;
		}
		pDC->SelectObject(pOldFont);
	}
}

void CTaskCalendarCtrl::DrawGrid(CDC* pDC)
{
	CCalendarCtrl::DrawGrid(pDC);
}

void CTaskCalendarCtrl::DrawCells(CDC* pDC)
{
	// rebuild build display
	m_nMaxDayTaskCount = 0;
	m_mapVertPos.RemoveAll();
	m_mapTextOffset.RemoveAll();

	if (m_mapData.GetCount())
	{
		for(int i=0; i<CALENDAR_MAX_ROWS ; i++)
		{
			for(int u=0; u<CALENDAR_NUM_COLUMNS ; u++)
			{
				CCalendarCell* pCell = GetCell(i, u);
				ASSERT(pCell);

				CTaskCalItemArray* pTasks = static_cast<CTaskCalItemArray*>(pCell->pUserData);

				if (pTasks == NULL)
				{
					pTasks = new CTaskCalItemArray();
					pCell->pUserData = pTasks;
				}

				ASSERT(pTasks);
				GetCellTasks(pCell->date, *pTasks, TRUE);
			}
		}
	}

	CCalendarCtrl::DrawCells(pDC);
}

void CTaskCalendarCtrl::DrawCell(CDC* pDC, const CCalendarCell* pCell, const CRect& rCell, 
							 BOOL bSelected, BOOL bToday, BOOL bShowMonth)
{
	CCalendarCtrl::DrawCell(pDC, pCell, rCell, bSelected, bToday, bShowMonth);
}

void CTaskCalendarCtrl::DrawCellContent(CDC* pDC, const CCalendarCell* pCell, const CRect& rCell, 
									BOOL bSelected, BOOL bToday)
{
	// default drawing
	CCalendarCtrl::DrawCellContent(pDC, pCell, rCell, bSelected, bToday);

	// then ours
	if (!m_nMaxDayTaskCount)
		return;

	CTaskCalItemArray* pTasks = static_cast<CTaskCalItemArray*>(pCell->pUserData);
	ASSERT(pTasks);

	if (pTasks && pTasks->GetSize())
	{
		int nTaskHeight = GetTaskHeight(/*rCell*/);

		for (int nTask = 0; nTask < pTasks->GetSize(); nTask++)
		{
			const TASKCALITEM* pTCI = (*pTasks)[nTask];
			ASSERT(pTCI);

			CRect rTask;

			if (!CalcTaskCellRect(pTCI->dwTaskID, pCell, rCell, rTask))
				continue;

			// determine what borders we need to draw
			DWORD dwFlags = GMDR_TOP;

			if (rTask.left > rCell.left)
				dwFlags |= GMDR_LEFT;
			
			if (rTask.right < rCell.right)
				dwFlags |= GMDR_RIGHT;

			if (rTask.bottom < rCell.bottom)
				dwFlags |= GMDR_BOTTOM;

			COLORREF crFill = pTCI->GetDefaultFillColor();
			COLORREF crBorder = pTCI->GetDefaultBorderColor();

			if (pTCI->dwTaskID == m_dwSelectedTaskID)
			{
				crFill = GetSysColor(COLOR_HIGHLIGHT);
				crBorder = GetSysColor(COLOR_HIGHLIGHTTEXT);
			}

			GraphicsMisc::DrawRect(pDC, rTask, crFill, crBorder, 0, dwFlags);

			// draw text if there is enough space
			if (nTaskHeight >= MIN_TEXT_HEIGHT)
			{
				int nOffset = GetTaskTextOffset(pTCI->dwTaskID);

				if (nOffset != -1)
				{
					if (nOffset == 0)
						rTask.left += 3; // initial pos

					// use 'best' crText for bkgnd
					COLORREF crText = GraphicsMisc::GetBestTextColor(crFill);
					COLORREF crOld = pDC->SetTextColor(crText);

					pDC->ExtTextOut(rTask.left - nOffset, rTask.top + 1, ETO_CLIPPED, rTask, pTCI->sName, NULL);
					pDC->SetTextColor(crOld);

					// update text pos
					nOffset += rTask.Width();

					// if the offset now exceeds the text extent we can stop
					int nExtent = pDC->GetTextExtent(pTCI->sName).cx;

					if (nOffset >= nExtent)
						nOffset = -1;

					m_mapTextOffset[pTCI->dwTaskID] = nOffset;
				}

				if (rTask.bottom >= rCell.bottom)
					break;
			}
		}
	}
}

void CTaskCalendarCtrl::DrawCellFocus(CDC* pDC, const CCalendarCell* pCell, const CRect& rCell)
{
	// we handle the focus during drawing
	// CCalendarCtrl::DrawCellFocus(pDC, pCell, rCell);
}

int CTaskCalendarCtrl::GetTaskHeight() const
{
	int nHeight = DEF_TASK_HEIGHT;

	if (HasOption(TCCO_ADJUSTTASKHEIGHTS))
	{
		CRect rCell;
		GetCellRect(0, 0, rCell, TRUE);

		nHeight = (rCell.Height() / m_nMaxDayTaskCount);
	}

	return max(1, min(nHeight, DEF_TASK_HEIGHT));
}

/*
BOOL CTaskCalendarCtrl::WantShowTaskAsContinuous(const TASKCALITEM* pTCI) const
{
	ASSERT(pTCI);
	ASSERT(pTCI->IsValid());

	// display continuous if the option is set and dates
	// are at least calculated
	if (HasOption(TCCO_DISPLAYCONTINUOUS))
	{
		if (pTCI->HasAnyStartDate() && pTCI->HasAnyEndDate())
			return TRUE;
	}

	// otherwise if the item is selected and we are displaying
	// both start and due dates then display continuous
	if (m_dwSelectedTaskID == pTCI->dwTaskID)
	{
		BOOL bStartIsSet = FALSE, bEndIsSet = FALSE;

		if (HasOption(TCCO_DISPLAYCALCSTART))
		{
			bStartIsSet = pTCI->HasAnyStartDate();
		}
		else if (HasOption(TCCO_DISPLAYSTART))
		{
			bStartIsSet = pTCI->IsStartDateSet();
		}

		if (HasOption(TCCO_DISPLAYCALCDUE))
		{
			bEndIsSet = pTCI->HasAnyEndDate();
		}
		else if (HasOption(TCCO_DISPLAYDUE))
		{
			bEndIsSet = pTCI->IsEndDateSet();
		}
		
		// both dates have to be set
		return (bStartIsSet && bEndIsSet);
	}

	// all else
	return FALSE;
}
*/

int CTaskCalendarCtrl::GetCellTasks(const COleDateTime& dtCell, CTaskCalItemArray& aTasks, BOOL bOrdered) const
{
	ASSERT(dtCell);

#ifdef _DEBUG
	int nDay = dtCell.GetDay();
	int nMonth = dtCell.GetMonth();
#endif

	aTasks.RemoveAll();

	double dCellStart = dtCell, dCellEnd = dCellStart + 1.0;
	POSITION pos = m_mapData.GetStartPosition();
	DWORD dwTaskID = 0;
	TASKCALITEM* pTCI = NULL;

	while (pos)
	{
		m_mapData.GetNextAssoc(pos, dwTaskID, pTCI);

		// ignore tasks with both start and end dates calculated
		if (!pTCI->IsValid())
			continue;

		// draw continuous either if the flag is set or the item is selected
		if (HasOption(TCCO_DISPLAYCONTINUOUS)/*WantShowTaskAsContinuous(pTCI)*/)
		{
			if ((pTCI->GetAnyStartDate().m_dt < dCellEnd) && 
				(pTCI->GetAnyEndDate().m_dt >= dCellStart))
			{
				aTasks.Add(pTCI);
			}
		}
		else
		{
			BOOL bAdded = FALSE; // only add once

			if (HasOption(TCCO_DISPLAYCALCSTART) || (HasOption(TCCO_DISPLAYSTART) && pTCI->IsStartDateSet()))
			{
				if (CDateHelper::GetDateOnly(pTCI->GetAnyStartDate()) == dtCell)
				{
					aTasks.Add(pTCI);
					bAdded = TRUE;
				}
			}

			// only test for due if start not added
			if (!bAdded)
			{
				if (HasOption(TCCO_DISPLAYCALCDUE) || (HasOption(TCCO_DISPLAYDUE) && pTCI->IsEndDateSet()))
				{
					if (CDateHelper::GetDateOnly(pTCI->GetAnyEndDate()) == dtCell)
						aTasks.Add(pTCI);
				}
			}
		}
	}

	if (bOrdered && aTasks.GetSize() > 1)
		qsort(aTasks.GetData(), aTasks.GetSize(), sizeof(TASKCALITEM*), CompareTCItems);

	// now go thru the list and set the position of each item 
	// if not already done
	int nMaxPos = 0;

	for (int nTask = 0; nTask < aTasks.GetSize(); nTask++)
	{
		const TASKCALITEM* pTCI = aTasks[nTask];
		ASSERT(pTCI);

		int nPos;

		if (!m_mapVertPos.Lookup(pTCI->dwTaskID, nPos))
		{
			nPos = max(nMaxPos, nTask);
			m_mapVertPos[pTCI->dwTaskID] = nPos;
		}

		nMaxPos = max(nMaxPos, nPos+1);
	}

	m_nMaxDayTaskCount = max(m_nMaxDayTaskCount, nMaxPos);

	return aTasks.GetSize();
}

int CTaskCalendarCtrl::CompareTCItems(const void* pV1, const void* pV2)
{
	typedef TASKCALITEM* PTASKCALITEM;

	const TASKCALITEM* pTCI1 = *(static_cast<const PTASKCALITEM*>(pV1));
	const TASKCALITEM* pTCI2 = *(static_cast<const PTASKCALITEM*>(pV2));

	// special case: Not drawing tasks continuous means that
	// the same task can appear twice
	if (pTCI1->dwTaskID == pTCI2->dwTaskID)
	{
		//ASSERT(!HasOption(TCCO_DISPLAYCONTINUOUS));
		return 0;
	}
	
	// earlier start date
	if (pTCI1->GetAnyStartDate() < pTCI2->GetAnyStartDate())
		return -1;

	if (pTCI1->GetAnyStartDate() > pTCI2->GetAnyStartDate())
		return 1;

	// equal so test for later end date
	if (pTCI1->GetAnyEndDate() > pTCI2->GetAnyEndDate())
		return -1;

	if (pTCI1->GetAnyEndDate() > pTCI2->GetAnyEndDate())
		return 1;

	// equal so test for task ID
	if (pTCI1->dwTaskID < pTCI2->dwTaskID)
		return -1;

	ASSERT(pTCI1->dwTaskID > pTCI2->dwTaskID);
	return 1;
}

DWORD CTaskCalendarCtrl::HitTest(const CPoint& ptCursor) const
{
	TCC_HITTEST nHit = TCCHT_NOWHERE;
	return HitTest(ptCursor, nHit);
}

DWORD CTaskCalendarCtrl::HitTest(const CPoint& ptCursor, TCC_HITTEST& nHit) const
{
	nHit = TCCHT_NOWHERE;

	if (!m_nMaxDayTaskCount)
		return 0;

	int nRow, nCol;

	if (!GetGridCellFromPoint(ptCursor, nRow, nCol))
		return 0;

	const CCalendarCell* pCell = GetCell(nRow, nCol);
	ASSERT(pCell);
	
	if (pCell == NULL)
		return 0;

	CTaskCalItemArray* pTasks = static_cast<CTaskCalItemArray*>(pCell->pUserData);
	ASSERT(pTasks);
	
	if (!pTasks || !pTasks->GetSize())
		return 0;
	
	// determine the vertical 'task pos' of the cursor
	CRect rCell;
	GetCellRect(nRow, nCol, rCell, TRUE);
	
	// handle clicking above tasks
	if (ptCursor.y < rCell.top)
		return 0;
	
	int nTaskHeight = GetTaskHeight(/*rCell*/);
	int nPos = ((ptCursor.y - rCell.top) / nTaskHeight);
	
	// look thru the tasks for this pos
	for (int nTask = 0; nTask < pTasks->GetSize(); nTask++)
	{
		const TASKCALITEM* pTCI = (*pTasks)[nTask];
		ASSERT(pTCI);
		
		int nTaskPos = GetTaskVertPos(pTCI->dwTaskID);
		
		if (nTaskPos == nPos)
		{
			// now we figure out where on the item we hit
			COleDateTime dtHit;
			VERIFY(GetDateFromPoint(ptCursor, dtHit));
			
			// now check for closeness to ends
			double dDateTol = CalcDateDragTolerance();
			
			if (fabs(dtHit.m_dt - pTCI->GetAnyStartDate().m_dt) < dDateTol)
			{
				nHit = TCCHT_BEGIN;
			}
			else if (fabs(dtHit.m_dt - pTCI->GetAnyEndDate().m_dt) < dDateTol)
			{
				nHit = TCCHT_END;
			}
			else if (dtHit > pTCI->GetAnyStartDate() && dtHit < pTCI->GetAnyEndDate())
			{
				nHit = TCCHT_MIDDLE;
			}
			
			return (nHit == TCCHT_NOWHERE) ? 0 : pTCI->dwTaskID;
		}
	}

	// nothing hit
	return 0;
}

double CTaskCalendarCtrl::CalcDateDragTolerance() const
{
	CRect rClient;
	GetClientRect(rClient);

	// calc equivalent of DRAG_WIDTH in days
	double dOneDay = ((double)rClient.Width() / CALENDAR_NUM_COLUMNS);
	double dDragTol = (GetSystemMetrics(SM_CXSIZEFRAME) / dOneDay);

	return min(dDragTol, 1.0);
}

void CTaskCalendarCtrl::EnsureVisible(DWORD dwTaskID, BOOL bShowStart)
{
	if (!bShowStart) // partial visibility ok
	{
		// is the task already visible to some degree
		int nRow, nCol;

		if (GetGridCellFromTask(dwTaskID, nRow, nCol))
 			return;
	}

	// else make it visible
	TASKCALITEM* pTCI = GetTaskCalItem(dwTaskID);
	ASSERT(pTCI);

	if (!pTCI)
		return;

	COleDateTime dtMin = GetMinDate(), dtMax = GetMaxDate();

	if (bShowStart)
	{
		if (pTCI->GetAnyStartDate() < dtMin || pTCI->GetAnyStartDate() > dtMax)
		{
			// need to scroll
			Goto(pTCI->GetAnyStartDate());
		}
	}
	else // allow any visibility
	{
		if (pTCI->GetAnyEndDate() <= dtMin || pTCI->GetAnyStartDate() >= dtMax)
		{
			// need to scroll
			Goto(pTCI->GetAnyStartDate());
		}
	}
}

bool CTaskCalendarCtrl::GetGridCellFromTask(DWORD dwTaskID, int &nRow, int &nCol) const
{
	// iterate the visible cells for the specified task
	CTaskCalItemArray aTasks;

	for(int i=0; i < GetVisibleWeeks() ; i++)
	{
		for(int u=0; u<CALENDAR_NUM_COLUMNS; u++)
		{
			const CCalendarCell* pCell = GetCell(i, u);
			
			int nTask = GetCellTasks(pCell->date, aTasks, FALSE);

			while (nTask--)
			{
				if (aTasks[nTask]->dwTaskID == dwTaskID)
				{
					nRow = i;
					nCol = u;

					return true;
				}
			}
		}
	}

	return false;
}

BOOL CTaskCalendarCtrl::GetTaskLabelRect(DWORD dwTaskID, CRect& rLabel)
{
	int nRow, nCol;

	// start with visibility check
	if (!GetGridCellFromTask(dwTaskID, nRow, nCol))
 		return FALSE;

	CCalendarCell* pCell = GetCell(nRow, nCol);
	ASSERT(pCell);

	CRect rCell;
	VERIFY(GetCellRect(nRow, nCol, rCell, TRUE));

	return CalcTaskCellRect(dwTaskID, pCell, rCell, rLabel);
}

BOOL CTaskCalendarCtrl::CalcTaskCellRect(DWORD dwTaskID, const CCalendarCell* pCell, const CRect& rCell, CRect& rTask) const
{
	// check horizontal (date) intersection first
	TASKCALITEM* pTCI = GetTaskCalItem(dwTaskID);
	ASSERT(pTCI);

	double dCellStart = pCell->date;
	double dCellEnd = dCellStart + END_OF_DAY;

	if (pTCI->GetAnyEndDate().m_dt < dCellStart || pTCI->GetAnyStartDate().m_dt > dCellEnd)
		return FALSE;

	// check vertical (pos) intersection next
	int nPos = GetTaskVertPos(dwTaskID);
	ASSERT(nPos >= 0 && nPos < m_nMaxDayTaskCount);

	int nTaskHeight = GetTaskHeight(/*rCell*/);

	if ((nPos * nTaskHeight) >= rCell.bottom)
		return FALSE;

	// calc rest of sides
	rTask = rCell;

	rTask.top += (nPos * nTaskHeight);
	rTask.bottom = min(rCell.bottom, (rTask.top + nTaskHeight - 1));

	// left edge
	if (pTCI->GetAnyStartDate().m_dt == dCellStart)
	{ 
		// whole day
		rTask.left++;
	}
	else if (pTCI->GetAnyStartDate().m_dt > dCellStart)
	{ 
		// partial day
		rTask.left += (int)((pTCI->GetAnyStartDate().m_dt - dCellStart) * rCell.Width());
	}	
			
	// right edge
	if (pTCI->GetAnyEndDate().m_dt == dCellEnd) // whole day
	{
		rTask.right--;
	}
	else if (pTCI->GetAnyEndDate().m_dt < dCellEnd)
	{
		// partial day
		rTask.right -= (int)((dCellEnd - pTCI->GetAnyEndDate().m_dt) * rCell.Width());
	}

	return TRUE;
}

int CTaskCalendarCtrl::GetTaskVertPos(DWORD dwTaskID) const
{
	ASSERT(dwTaskID);
	int nPos = -1;

	m_mapVertPos.Lookup(dwTaskID, nPos);
	ASSERT(nPos >= 0 && nPos < m_nMaxDayTaskCount);

	return nPos;
}

int CTaskCalendarCtrl::GetTaskTextOffset(DWORD dwTaskID) const
{
	ASSERT(dwTaskID);
	int nPos = 0;

	// special case: Always return zero if NOT drawing continuous
	// and this task is NOT selected
	if (!HasOption(TCCO_DISPLAYCONTINUOUS)/* && (m_dwSelectedTaskID != dwTaskID)*/)
		return 0;

	if (!m_mapTextOffset.Lookup(dwTaskID, nPos))
		m_mapTextOffset[dwTaskID] = nPos;

	return nPos;
}

TASKCALITEM* CTaskCalendarCtrl::GetTaskCalItem(DWORD dwTaskID) const
{
	ASSERT(dwTaskID);
	TASKCALITEM* pTCI = NULL;

	m_mapData.Lookup(dwTaskID, pTCI);
	ASSERT(pTCI);

	return pTCI;
}

BOOL CTaskCalendarCtrl::HasTask(DWORD dwTaskID) const
{
	if (dwTaskID == 0)
		return FALSE;

	TASKCALITEM* pTCI = NULL;
	return m_mapData.Lookup(dwTaskID, pTCI);
}

// external version
BOOL CTaskCalendarCtrl::SelectTask(DWORD dwTaskID)
{
	return SelectTask(dwTaskID, FALSE);
}

// internal version
BOOL CTaskCalendarCtrl::SelectTask(DWORD dwTaskID, BOOL bNotify)
{
	if (!HasTask(dwTaskID))
		return FALSE;

	if (dwTaskID != m_dwSelectedTaskID)
	{
		m_dwSelectedTaskID = dwTaskID;

		if (bNotify)
			GetParent()->SendMessage(WM_CALENDAR_SELCHANGE, 0, dwTaskID);

		Invalidate(FALSE);
		UpdateWindow();
	}

	return TRUE;
}

void CTaskCalendarCtrl::OnLButtonDown(UINT nFlags, CPoint point) 
{
	UpdateWindow();

	DWORD dwSelID = HitTest(point);
	
	if (dwSelID)
	{
		SelectTask(dwSelID, TRUE);

		if (StartDragging(point))
			return;
	}

	CCalendarCtrl::OnLButtonDown(nFlags, point);
}

BOOL CTaskCalendarCtrl::StartDragging(const CPoint& ptCursor)
{
	TCC_HITTEST nHit = TCCHT_NOWHERE;
	DWORD dwTaskID = HitTest(ptCursor, nHit);

	ASSERT((nHit == TCCHT_NOWHERE) || (dwTaskID != 0));

	if (nHit == TCCHT_NOWHERE)
		return FALSE;

	// when not drawing tasks continuously, it's possible
	// for the act of selecting a task to change its
	// position and thus its hit-test result
	if (dwTaskID != m_dwSelectedTaskID)
		return FALSE;
	
	switch (nHit)
	{
	case TCCHT_BEGIN:
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		m_bDraggingStart = TRUE;
		break;
		
	case TCCHT_END:
		m_bDraggingEnd = TRUE;
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
		break;
		
	case TCCHT_MIDDLE:
		m_bDragging = TRUE;
		break;
		
	default:
		ASSERT(0);
		return FALSE;
	}
	
	m_tciPreDrag = *(GetTaskCalItem(dwTaskID));
	m_ptDragOrigin = ptCursor;

	SetCapture();

	// keep parent informed
	NotifyParentDragChange();

	return TRUE;
}

BOOL CTaskCalendarCtrl::GetValidDragDate(const CPoint& ptCursor, COleDateTime& dtDrag) const
{
	CPoint ptDrag(ptCursor);

	if (!ValidateDragPoint(ptDrag))
		return FALSE;

	if (!GetDateFromPoint(ptDrag, dtDrag))
		return FALSE;

	// if dragging the whole task, then we calculate
	// dtDrag as TASKCALITEM::dtStart/dtEnd offset by the
	// difference between the current drag pos and the
	// initial drag pos
	if (m_bDragging)
	{
		COleDateTime dtOrg;
		GetDateFromPoint(m_ptDragOrigin, dtOrg);
		
		// offset from pre-drag position
		double dOffset = dtDrag.m_dt - dtOrg.m_dt;

		if (m_tciPreDrag.IsStartDateSet())
			dtDrag = m_tciPreDrag.GetAnyStartDate().m_dt + dOffset;
		else
		{
			ASSERT(m_tciPreDrag.IsEndDateSet());
			dtDrag = m_tciPreDrag.GetAnyEndDate().m_dt + dOffset;
		}
		
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL));
	}
	
	// adjust date depending on snap mode
	switch (GetSnapMode())
	{
	case TCCSM_NEARESTHOUR:
		dtDrag = CDateHelper::GetNearestHour(dtDrag, m_bDraggingEnd);
		break;

	case TCCSM_NEARESTDAY:
		dtDrag = CDateHelper::GetNearestDay(dtDrag, m_bDraggingEnd);
		break;

	case TCCSM_NEARESTHALFDAY:
		dtDrag = CDateHelper::GetNearestHalfDay(dtDrag, m_bDraggingEnd);
		break;

	case TCCSM_FREE:
		if (m_bDraggingEnd)
		{
			double dTime = CDateHelper::GetTimeOnly(dtDrag);

			if (dTime >= END_OF_DAY)
				dtDrag = CDateHelper::MakeDate(dtDrag, END_OF_DAY);
		}
		break;

	case TCCSM_NONE:
	default:
		ASSERT(0);
		return FALSE;
	}
	
	return TRUE;
}

TCC_SNAPMODE CTaskCalendarCtrl::GetSnapMode() const
{
	if (IsDragging())
	{
		// active keys override
		if (Misc::ModKeysArePressed(MKS_CTRL))
		{
			m_nSnapMode = TCCSM_NEARESTHOUR;
		}
		else if (Misc::ModKeysArePressed(MKS_SHIFT))
		{
			m_nSnapMode = TCCSM_NEARESTDAY;
		}
		else if (Misc::ModKeysArePressed(MKS_CTRL | MKS_SHIFT))
		{
			m_nSnapMode = TCCSM_NEARESTHALFDAY;
		}
	}

	return m_nSnapMode;
}

void CTaskCalendarCtrl::OnMouseMove(UINT nFlags, CPoint point) 
{
	if (UpdateDragging(point))
		return;

	CCalendarCtrl::OnMouseMove(nFlags, point);
}

BOOL CTaskCalendarCtrl::UpdateDragging(const CPoint& ptCursor)
{
	if (IsDragging())
	{
		TASKCALITEM* pTCI = GetTaskCalItem(m_dwSelectedTaskID);
		ASSERT(pTCI);
			
		if (IsValidDrag(ptCursor))
		{
			COleDateTime dtDrag;

			if (GetValidDragDate(ptCursor, dtDrag))
			{
				// prevent the start and end dates from overlapping
				if (m_bDraggingStart)
				{
					pTCI->SetStartDate(min(dtDrag.m_dt, pTCI->GetAnyEndDate().m_dt - ONE_HOUR));
				}
				else if (m_bDraggingEnd)
				{
					pTCI->SetEndDate(max(dtDrag.m_dt, pTCI->GetAnyStartDate().m_dt + ONE_HOUR));
				}
				else // m_bDragging
				{
					if (pTCI->IsStartDateSet() && pTCI->IsEndDateSet())
					{
						double dDuration = (pTCI->GetAnyEndDate().m_dt - pTCI->GetAnyStartDate().m_dt);
						
						pTCI->SetStartDate(dtDrag);
						pTCI->SetEndDate(dtDrag.m_dt + dDuration);
					}
					else if (pTCI->IsStartDateSet())
					{
						pTCI->SetStartDate(dtDrag);
					}
					else if (pTCI->IsEndDateSet())
					{
						pTCI->SetEndDate(dtDrag);
					}
				}
			}
		}
		else
		{
			*pTCI = m_tciPreDrag;
		}

		// always recalc dates
		if (pTCI->IsEndDateSet() && !pTCI->IsStartDateSet() && (pTCI->GetAnyEndDate() < pTCI->GetAnyStartDate()))
		{
			int breakpoint = 0;
		}
		pTCI->RecalcDates(m_dwOptions);
			
		Invalidate();
		UpdateWindow();

		// keep parent informed
		NotifyParentDragChange();

		return TRUE;
	}

	// else
	return FALSE;
}

void CTaskCalendarCtrl::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (EndDragging(point))
		return;
	
	CCalendarCtrl::OnLButtonUp(nFlags, point);
}

BOOL CTaskCalendarCtrl::EndDragging(const CPoint& ptCursor)
{
	if (IsDragging())
	{
		TASKCALITEM* pTCI = GetTaskCalItem(m_dwSelectedTaskID);
		ASSERT(pTCI);

		// dropping outside the calendar is a cancel
		CRect rClient;
		GetClientRect(rClient);

		rClient.top += CALENDAR_HEADER_HEIGHT;

		if (!rClient.PtInRect(ptCursor) || !IsValidDrag(ptCursor))
		{
			*pTCI = m_tciPreDrag;
		}
		else if (m_bDraggingStart)
		{
			NotifyParentDateChange(TCCHT_BEGIN);
		}
		else if (m_bDraggingEnd)
		{
			NotifyParentDateChange(TCCHT_END);
		}
		else
		{
			ASSERT(m_bDragging);

			// if the start is calculated, treat this like an end move
			if (!pTCI->IsStartDateSet())
			{
				NotifyParentDateChange(TCCHT_END);
			}
			else if (!pTCI->IsEndDateSet())
			{
				NotifyParentDateChange(TCCHT_BEGIN);
			}
			else
			{
				ASSERT(pTCI->IsStartDateSet() && pTCI->IsEndDateSet());
				NotifyParentDateChange(TCCHT_MIDDLE);
			}
		}

		// cleanup
		m_bDraggingStart = m_bDraggingEnd = m_bDragging = FALSE;
		ReleaseCapture();
		Invalidate(FALSE);

		// keep parent informed
		NotifyParentDragChange();

		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CTaskCalendarCtrl::GetSelectedTaskDates(COleDateTime& dtStart, COleDateTime& dtDue) const
{
	if ((m_dwSelectedTaskID == 0) || !HasTask(m_dwSelectedTaskID))
		return FALSE;

	TASKCALITEM* pTCI = GetTaskCalItem(m_dwSelectedTaskID);
	ASSERT(pTCI);

	if (!pTCI)
		return FALSE;

	dtStart = pTCI->GetAnyStartDate();
	dtDue = pTCI->GetAnyEndDate();

	// handle END_OF_DAY
	if (CDateHelper::GetTimeOnly(dtDue).m_dt >= END_OF_DAY)
	{
		dtDue = CDateHelper::GetDateOnly(dtDue);
	}

	return TRUE;
}

BOOL CTaskCalendarCtrl::GetDateFromPoint(const CPoint& ptCursor, COleDateTime& date) const
{
	int nRow, nCol;

	if (GetGridCellFromPoint(ptCursor, nRow, nCol))
	{
		const CCalendarCell* pCell = GetCell(nRow, nCol);
		ASSERT(pCell);

		// calc proportion of day by 'x' coordinate
		CRect rCell;
		VERIFY(GetCellRect(nRow, nCol, rCell));

		double dTime = ((ptCursor.x - rCell.left) / (double)rCell.Width());
		date = pCell->date.m_dt + dTime;

		return TRUE;
	}

	// all else
	return FALSE;
}

BOOL CTaskCalendarCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	// this is only for when we are NOT yet dragging
	ASSERT(!IsDragging());

	if (!m_bSelectionStarted && 
		nHitTest == HTCLIENT && 
		message == WM_MOUSEMOVE &&
		!Misc::KeyIsPressed(VK_MBUTTON) &&
		!Misc::KeyIsPressed(VK_RBUTTON))
	{
		CPoint ptCursor = CWnd::GetCurrentMessage()->pt;
		ScreenToClient(&ptCursor);

		TCC_HITTEST nHit = TCCHT_NOWHERE;
		DWORD dwHitID = HitTest(ptCursor, nHit);

		switch (nHit)
		{
		case TCCHT_BEGIN:
		case TCCHT_END:
			SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
			return TRUE;

		case TCCHT_MIDDLE:
			//SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEALL));
			break;
		}
	}
	
	// else
	return CCalendarCtrl::OnSetCursor(pWnd, nHitTest, message);
}

BOOL CTaskCalendarCtrl::IsDragging() const
{
	return (m_bDragging || m_bDraggingStart || m_bDraggingEnd);
}

BOOL CTaskCalendarCtrl::IsValidDrag(const CPoint& ptDrag) const
{
	if (!IsDragging())
		return FALSE;

	CSize size = (m_ptDragOrigin - ptDrag);

	int nCxDrag = (GetSystemMetrics(SM_CXDRAG) / 2);
	int nCyDrag = (GetSystemMetrics(SM_CYDRAG) / 2);

	return ((abs(size.cx) > nCxDrag) || 
			(abs(size.cy) > nCyDrag));
}

BOOL CTaskCalendarCtrl::ValidateDragPoint(CPoint& ptDrag) const
{
	if (!IsDragging())
		return FALSE;

	CRect rClient;
	GetClientRect(rClient);

	rClient.top += CALENDAR_HEADER_HEIGHT;

	ptDrag.x = max(ptDrag.x, rClient.left);
	ptDrag.x = min(ptDrag.x, rClient.right);
	ptDrag.y = max(ptDrag.y, rClient.top);
	ptDrag.y = min(ptDrag.y, rClient.bottom);

	return TRUE;
}

void CTaskCalendarCtrl::OnCaptureChanged(CWnd *pWnd) 
{
	// if someone else grabs the capture we cancel any drag
	if (pWnd && (pWnd != this) && IsDragging())
		CancelDrag(FALSE);
	
	CCalendarCtrl::OnCaptureChanged(pWnd);
}

void CTaskCalendarCtrl::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	switch (nChar)
	{
	case VK_ESCAPE:
		if (IsDragging())
		{
			CancelDrag(TRUE);
			return;
		}
		break;

	case VK_CONTROL:
	case VK_SHIFT:
		if (IsDragging())
			NotifyParentDragChange();
		break;
	}
	
	CCalendarCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
}

// external version
BOOL CTaskCalendarCtrl::CancelDrag()
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
void CTaskCalendarCtrl::CancelDrag(BOOL bReleaseCapture)
{
	ASSERT(IsDragging());

	// cancel drag, restoring original task dates
	TASKCALITEM* pTCI = GetTaskCalItem(m_dwSelectedTaskID);
	ASSERT(pTCI);
	
	*pTCI = m_tciPreDrag;
	m_bDragging = m_bDraggingStart = m_bDraggingEnd = FALSE;
	
	if (bReleaseCapture)
		ReleaseCapture();

	Invalidate(FALSE);
	UpdateWindow();

	// keep parent informed
	NotifyParentDragChange();
}

void CTaskCalendarCtrl::OnRButtonDown(UINT nFlags, CPoint point) 
{
	DWORD dwTaskID = HitTest(point);
	SelectTask(dwTaskID, TRUE);
	
	CCalendarCtrl::OnRButtonDown(nFlags, point);
}

int CTaskCalendarCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CCalendarCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	EnableToolTips(TRUE);

	return 0;
}
