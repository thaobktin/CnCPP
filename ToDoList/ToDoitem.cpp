// ToDoCtrlData.cpp: implementation of the CToDoCtrlData class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <math.h>

#include "ToDoitem.h"
#include "tdcenum.h"

#include "..\shared\DateHelper.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

COleDateTimeSpan TODOITEM::s_dtsRecentModPeriod = (1.0 / 24); // one hour

double TODOITEM::NULL_VALUE = (double)0xFFFFFFFFFFFFFFFF;

//////////////////////////////////////////////////////////////////////

TODOITEM::TODOITEM(LPCTSTR szTitle, LPCTSTR szComments) :
	sTitle(szTitle), 
	sComments(szComments),
	color(0), 
	nPriority(5),
	nRisk(0),
	nPercentDone(0),
	dTimeEstimate(0),
	dTimeSpent(0),
	nTimeEstUnits(TDITU_HOURS),
	nTimeSpentUnits(TDITU_HOURS),
	dCost(0),
	bFlagged(FALSE),
	dateCreated(COleDateTime::GetCurrentTime()),
	dwCalculated(0),
	nCalcPriority(0),
	nCalcPriorityIncDue(0),
	nCalcPercent(0),
	nCalcRisk(0),
	dCalcTimeEstimate(0),
	dCalcTimeSpent(0),
	dCalcCost(0),
	bGoodAsDone(0),
	nSubtasksCount(0),
	nSubtasksDone(0),
	dwTaskRefID(0)
{ 
}

TODOITEM::TODOITEM() :
	color(0), 
	nPriority(5),
	nRisk(0),
	nPercentDone(0),
	dTimeEstimate(0),
	dTimeSpent(0),
	nTimeEstUnits(TDITU_HOURS),
	nTimeSpentUnits(TDITU_HOURS),
	dCost(0),
	bFlagged(FALSE),
	dateCreated(COleDateTime::GetCurrentTime()),
	dwCalculated(0),
	nCalcPriority(0),
	nCalcPriorityIncDue(0),
	nCalcPercent(0),
	nCalcRisk(0),
	dCalcTimeEstimate(0),
	dCalcTimeSpent(0),
	dCalcCost(0),
	bGoodAsDone(0),
	nSubtasksCount(0),
	nSubtasksDone(0),
	dwTaskRefID(0)
{ 
}

TODOITEM::TODOITEM(const TODOITEM& tdi)
{ 
	*this = tdi;
	
    if (!CDateHelper::IsDateSet(dateCreated))
		dateCreated = COleDateTime::GetCurrentTime();
}

TODOITEM::TODOITEM(const TODOITEM* pTDI)
{
	if (pTDI)
	{
		*this = *pTDI;
		
		if (!CDateHelper::IsDateSet(dateCreated))
			dateCreated = COleDateTime::GetCurrentTime();
	}
}

const TODOITEM& TODOITEM::operator=(const TODOITEM& tdi) 
{
	sTitle = tdi.sTitle;
	sComments = tdi.sComments;
	customComments = tdi.customComments;
	sCommentsTypeID = tdi.sCommentsTypeID;
	color = tdi.color; 
	sFileRefPath = tdi.sFileRefPath;
	sAllocBy = tdi.sAllocBy;
	sStatus = tdi.sStatus;
	nPriority = tdi.nPriority;
	nPercentDone = tdi.nPercentDone;
	dTimeEstimate = tdi.dTimeEstimate;
	dTimeSpent = tdi.dTimeSpent;
	nTimeEstUnits = tdi.nTimeEstUnits;
	nTimeSpentUnits = tdi.nTimeSpentUnits;
	dCost = tdi.dCost;
	dateStart = tdi.dateStart;
	dateDue = tdi.dateDue;
	dateDone = tdi.dateDone;
	dateCreated = tdi.dateCreated;
	bFlagged = tdi.bFlagged;
	sCreatedBy = tdi.sCreatedBy;
	nRisk = tdi.nRisk;
	sExternalID = tdi.sExternalID;
	trRecurrence = tdi.trRecurrence;
	dateLastMod = tdi.dateLastMod;
	sVersion = tdi.sVersion;
	sIcon = tdi.sIcon;
	dwTaskRefID = tdi.dwTaskRefID;
	
	aCategories.Copy(tdi.aCategories);
	aTags.Copy(tdi.aTags);
	aAllocTo.Copy(tdi.aAllocTo);
	aDependencies.Copy(tdi.aDependencies);

	// meta data
	mapMetaData.RemoveAll();
	POSITION pos = tdi.mapMetaData.GetStartPosition();

	while (pos)
	{
		CString sKey, sValue;
		tdi.mapMetaData.GetNextAssoc(pos, sKey, sValue);
		mapMetaData[sKey] = sValue;
	}
	
	// custom attributes
	mapCustomData.RemoveAll();
	pos = tdi.mapCustomData.GetStartPosition();
	
	while (pos)
	{
		CString sKey, sValue;
		tdi.mapCustomData.GetNextAssoc(pos, sKey, sValue);
		mapCustomData[sKey] = sValue;
	}

	dwCalculated = 0; // reset calcs
	nCalcPriority = 0;
	nCalcPriorityIncDue = 0;
	nCalcPercent = 0;
	nCalcRisk = 0;
	dCalcTimeEstimate = 0;
	dCalcTimeSpent = 0;
	dCalcCost = 0;
	bGoodAsDone = 0;
	nSubtasksCount = 0;
	nSubtasksDone = 0;
	sPath.Empty();
	sPosition.Empty();
	
	return *this;
}

BOOL TODOITEM::AttribNeedsRecalc(DWORD dwAttrib) const
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		ASSERT(0);
		return FALSE;
	}
	
	if ((dwCalculated & dwAttrib) == 0)
		return TRUE;

	// else
	return FALSE;
}

void TODOITEM::SetAttribNeedsRecalc(DWORD dwAttrib, BOOL bSet) const
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		ASSERT(0);
		return;
	}
	
	if (bSet)
		dwCalculated &= ~dwAttrib;
	else
		dwCalculated |= dwAttrib;
}

BOOL TODOITEM::CustomAttribNeedsRecalc(const CString& sAttribID) const
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		ASSERT(0);
		return FALSE;
	}
	
	double dTemp;
	return !mapCustomCalcs.Lookup(sAttribID, dTemp);
}

void TODOITEM::ResetCalcs(TDC_ATTRIBUTE nAttrib, BOOL bIncCustom) const 
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		// we make one concession
		ASSERT(nAttrib == TDCA_ALL);
		return;
	}

	switch (nAttrib)
	{
	case TDCA_CATEGORY:		SetAttribNeedsRecalc(TDIR_CATEGORY); break;
	case TDCA_ALLOCTO:		SetAttribNeedsRecalc(TDIR_ALLOCTO); break;
	case TDCA_TAGS:			SetAttribNeedsRecalc(TDIR_TAGS); break;
	case TDCA_DUEDATE:		SetAttribNeedsRecalc(TDIR_CALCULATEDDUE | TDIR_PRIORITYINCDUE); break;
	case TDCA_STARTDATE:	SetAttribNeedsRecalc(TDIR_CALCULATEDSTART); break;
	case TDCA_RISK:			SetAttribNeedsRecalc(TDIR_RISK); break;
	case TDCA_PRIORITY:		SetAttribNeedsRecalc(TDIR_PRIORITY | TDIR_PRIORITYINCDUE); break;
	case TDCA_PERCENT:		SetAttribNeedsRecalc(TDIR_PERCENT); break;
	case TDCA_TIMEEST:		SetAttribNeedsRecalc(TDIR_TIMEESTIMATE); break;
	case TDCA_TIMESPENT:	SetAttribNeedsRecalc(TDIR_TIMESPENT); break;
	case TDCA_COST:			SetAttribNeedsRecalc(TDIR_COST); break;
	case TDCA_TASKNAME:		SetAttribNeedsRecalc(TDIR_PATH); break;

	// these affects so much that it's easier just to reset
	case TDCA_NEWTASK:  
	case TDCA_DONEDATE: 
	case TDCA_DELETE:
	case TDCA_ALL:
		dwCalculated = 0;

		if (bIncCustom)
			mapCustomCalcs.RemoveAll();
		break;

	// all other attributes have no effect
	}
}

void TODOITEM::ResetCustomAttribCalc(const CString& sAttribID) const
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		ASSERT(0);
		return;
	}
	
	if (sAttribID.IsEmpty())
		mapCustomCalcs.RemoveAll();
	else
		mapCustomCalcs.RemoveKey(sAttribID);
}

BOOL TODOITEM::GetCustomAttribCalcValue(const CString& sAttribID, double& dValue) const
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		ASSERT(0);
		return FALSE;
	}
	
	double dTemp = NULL_VALUE;

	if (!mapCustomCalcs.Lookup(sAttribID, dTemp) || (dTemp == NULL_VALUE))
		return FALSE;

	dValue = dTemp;
	return TRUE;
}

void TODOITEM::SetCustomAttribCalcValue(const CString& sAttribID, double dValue) const
{
	// ought never be called on references
	if (dwTaskRefID)
	{
		ASSERT(0);
		return;
	}
	
	mapCustomCalcs[sAttribID] = dValue;
}

BOOL TODOITEM::HasCreation() const 
{ 
	return CDateHelper::IsDateSet(dateCreated); 
}

BOOL TODOITEM::HasLastMod() const 
{ 
	return CDateHelper::IsDateSet(dateLastMod); 
}

BOOL TODOITEM::HasStart() const 
{ 
	return CDateHelper::IsDateSet(dateStart); 
}

BOOL TODOITEM::HasStartTime() const 
{ 
	return HasTime(dateStart); 
}

BOOL TODOITEM::HasDue() const 
{ 
	return CDateHelper::IsDateSet(dateDue); 
}

BOOL TODOITEM::HasDueTime() const 
{ 
	return HasTime(dateDue); 
}

BOOL TODOITEM::HasTime(const COleDateTime& date) 
{ 
	return CDateHelper::DateHasTime(date); 
}

BOOL TODOITEM::IsDone() const 
{ 
	return CDateHelper::IsDateSet(dateDone); 
}

BOOL TODOITEM::HasDoneTime() const 
{ 
	return HasTime(dateDone); 
}

void TODOITEM::ClearStart() 
{ 
	CDateHelper::ClearDate(dateStart); 
}

void TODOITEM::ClearDue() 
{ 
	CDateHelper::ClearDate(dateDue); 
}

void TODOITEM::ClearDone() 
{ 
	CDateHelper::ClearDate(dateDone); 
}

BOOL TODOITEM::IsDue() const
{ 
	return IsDue(COleDateTime::GetCurrentTime());
}

BOOL TODOITEM::IsDue(const COleDateTime& dateDueBy) const
{ 
	if (IsDone() || !HasDue())
		return FALSE;
	
	return (CDateHelper::GetDateOnly(dateDue) <= CDateHelper::GetDateOnly(dateDueBy)); 
}

void TODOITEM::SetModified() 
{ 
	dateLastMod = COleDateTime::GetCurrentTime(); 
}

CString TODOITEM::GetFirstCategory() const
{
	return aCategories.GetSize() ? aCategories[0] : "";
}

CString TODOITEM::GetFirstAllocTo() const
{
	return aAllocTo.GetSize() ? aAllocTo[0] : "";
}

CString TODOITEM::GetFirstDependency() const
{
	return aDependencies.GetSize() ? aDependencies[0] : "";
}

CString TODOITEM::GetFirstTag() const
{
	return aTags.GetSize() ? aTags[0] : "";
}

BOOL TODOITEM::IsRecurring() const
{
	return (trRecurrence.nRegularity != TDIR_ONCE);
}

BOOL TODOITEM::GetNextOccurence(COleDateTime& dtNext, BOOL& bDue) const
{
	if (!IsRecurring())
		return FALSE;

	if (trRecurrence.nRecalcFrom == TDIRO_DUEDATE && HasDue())
	{
		bDue = TRUE;
		return trRecurrence.GetNextOccurence(dateDue, dtNext);
	}
	else if (trRecurrence.nRecalcFrom == TDIRO_STARTDATE && HasStart())
	{
		bDue = FALSE;
		return trRecurrence.GetNextOccurence(dateStart, dtNext);
	}
	
	// else fall thru to here.
	// use completed date but with the current due/start time
	if (trRecurrence.GetNextOccurence(COleDateTime::GetCurrentTime(), dtNext))
	{
		// restore the due time to be whatever it was
		dtNext = CDateHelper::GetDateOnly(dtNext);

		if (HasDue())
		{
			bDue = TRUE;
			dtNext += CDateHelper::GetTimeOnly(dateDue).m_dt;
		}
		else if (HasStart())
		{
			bDue = FALSE;
			dtNext += CDateHelper::GetTimeOnly(dateStart).m_dt;
		}
		else
			bDue = TRUE;
		
		return TRUE;
	}
	
	// else
	return FALSE;
}

BOOL TODOITEM::IsRecentlyEdited() const
{
	if (!HasLastMod())
		return FALSE; // never

	if (s_dtsRecentModPeriod.m_span == 0.0)
		return TRUE; // always
	
	// else
	return ((COleDateTime::GetCurrentTime() - dateLastMod) < s_dtsRecentModPeriod);
}

COleDateTimeSpan TODOITEM::GetRemainingDueTime() const
{
	return GetRemainingDueTime(dateDue);
}

CString TODOITEM::GetCustomAttribValue(const CString& sAttribID) const
{
	CString sValue;
	mapCustomData.Lookup(sAttribID, sValue);
	return sValue;
}

BOOL TODOITEM::HasCustomAttribValue(const CString& sAttribID) const
{
	CString sTemp;
	return mapCustomData.Lookup(sAttribID, sTemp);
}

COleDateTimeSpan TODOITEM::GetRemainingDueTime(const COleDateTime& date)
{
	COleDateTimeSpan dtsRemaining = date - COleDateTime::GetCurrentTime();
	
	if (!HasTime(date))
		dtsRemaining += 1; // midnight on the day
	
	return dtsRemaining;
}

void TODOITEM::ParseTaskLink(const CString& sLink, DWORD& dwTaskID, CString& sFile)
{
	sFile = sLink;
	dwTaskID = 0;

	CString sTaskID;

	if (Misc::Split(sFile, sTaskID, '?'))
	{
		dwTaskID = _ttoi(sTaskID);

		// remove trailing back slash appended by Macro Express Pro
		sFile.TrimRight(_T("\\"));
	}
	else if (!sLink.IsEmpty())
	{
		if (isdigit(sLink[0])) // number
		{
			dwTaskID = _ttoi(sLink);
			sFile.Empty();
		}
	}
}

CString TODOITEM::MakeTaskLink(DWORD dwTaskID, const CString& sFile)
{
	CString sLink;
	
	if (sFile.IsEmpty() && dwTaskID > 0)
		sLink.Format(_T("%d"), dwTaskID);
	
	else if (!sFile.IsEmpty())
	{
		if (dwTaskID > 0)
			sLink.Format(_T("%s?%d"), sFile, dwTaskID);
		else
			sLink = sFile;
	}
	
	return sLink;
}

void TODOITEM::SetRecentlyModifiedPeriod(double dDays)
{
	s_dtsRecentModPeriod = max(dDays, 0.0);
}

TCHAR TODOITEM::GetTimeUnits(BOOL bTimeEst) const
{
	return (bTimeEst ? nTimeEstUnits : nTimeSpentUnits);
}

double TODOITEM::GetTime(BOOL bTimeEst, BOOL bCalc) const
{
	if (bCalc)
		return (bTimeEst ? dCalcTimeEstimate : dCalcTimeSpent);

	// else
	return (bTimeEst ? dTimeEstimate : dTimeSpent);
}

//////////////////////////////////////////////////////////////////////////////////////////////

TODOSTRUCTURE::TODOSTRUCTURE(DWORD dwID) : m_dwID(dwID), m_pTDSParent(NULL)
{
	ASSERT(dwID);
}

TODOSTRUCTURE::TODOSTRUCTURE(const TODOSTRUCTURE& tds)
{
	*this = tds; // invoke assignment operator
}

TODOSTRUCTURE::~TODOSTRUCTURE()
{
	CleanUp();
}

const TODOSTRUCTURE& TODOSTRUCTURE::operator=(const TODOSTRUCTURE& tds)
{
	// reset our own contents
	CleanUp();
	
	// copy target
	m_dwID = tds.m_dwID;

	// clear parent
	m_pTDSParent = NULL; // caller must add to parent explicitly
	
	// copy children
	for (int nSubTask = 0; nSubTask < tds.GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDSOther = tds.GetSubTask(nSubTask);
		ASSERT(pTDSOther);
		
		TODOSTRUCTURE* pTDSChild = new TODOSTRUCTURE(*pTDSOther); // this will copy the children's children
		m_aSubTasks.Add(pTDSChild);
		
		// set parent
		pTDSChild->m_pTDSParent = this;
	}
	
	return *this;
}

int TODOSTRUCTURE::GetLeafCount() const
{
	int nLeafCount = 0;

	for (int nSubTask = 0; nSubTask < GetSubTaskCount(); nSubTask++)
	{
		nLeafCount += GetSubTask(nSubTask)->GetLeafCount();
	}

	return (nLeafCount == 0) ? 1 : nLeafCount;
}

TODOSTRUCTURE* TODOSTRUCTURE::GetSubTask(int nPos) const
{
	if (nPos >= 0 && nPos < GetSubTaskCount())
		return m_aSubTasks[nPos];
	
	// else
	ASSERT(0);
	return NULL;
}

int TODOSTRUCTURE::GetSubTaskPos(TODOSTRUCTURE* pTDS) const
{
	for (int nSubTask = 0; nSubTask < GetSubTaskCount(); nSubTask++)
	{
		if (GetSubTaskID(nSubTask) == pTDS->GetTaskID())
			return nSubTask;
	}
	
	// else
	ASSERT(0);
	return -1;
}

DWORD TODOSTRUCTURE::GetSubTaskID(int nPos) const
{
	const TODOSTRUCTURE* pTDS = GetSubTask(nPos);
	return pTDS ? pTDS->GetTaskID() : 0;
}

int TODOSTRUCTURE::GetSubTaskPosition(DWORD dwID) const
{
	ASSERT(dwID);
	
	if (!dwID)
		return -1;
	
	for (int nSubTask = 0; nSubTask < GetSubTaskCount(); nSubTask++)
	{
		if (GetSubTaskID(nSubTask) == dwID)
			return nSubTask;
	}
	
	// not found
	return -1;
}

int TODOSTRUCTURE::GetPosition() const
{
	if (m_pTDSParent == NULL) // root
		return -1;
	
	// get the position of 'this' task in its parent
	return m_pTDSParent->GetSubTaskPosition(GetTaskID());
}

DWORD TODOSTRUCTURE::GetParentTaskID() const
{
	if (m_pTDSParent == NULL) // root
		return NULL;
	
	return m_pTDSParent->GetTaskID();
}

TODOSTRUCTURE* TODOSTRUCTURE::GetParentTask() const
{
	return m_pTDSParent;
}

DWORD TODOSTRUCTURE::GetPreviousSubTaskID(int nPos)
{
	if (nPos <= 0 || nPos >= GetSubTaskCount())
		return 0;
	
	// else
	return GetSubTaskID(nPos - 1);
}

BOOL TODOSTRUCTURE::HasSameParent(const TODOSTRUCTURE* pTDS) const
{
	return (GetParentTask() == pTDS->GetParentTask());
}

// protected helper
BOOL TODOSTRUCTURE::InsertSubTask(TODOSTRUCTURE* pTDS, int nPos)
{
	// sanity checks
	ASSERT(pTDS && pTDS->GetTaskID());
	
	if (!pTDS)
		return FALSE;
	
	ASSERT(nPos >= 0 && nPos <= GetSubTaskCount());
	
	if (nPos < 0 || nPos > GetSubTaskCount())
		return FALSE;
	
	// check task with this ID does not already exist
	if (GetSubTaskPosition(pTDS->GetTaskID()) != -1)
	{
		ASSERT(0);
		return FALSE;
	}
	
	if (nPos == GetSubTaskCount())
		m_aSubTasks.Add(pTDS);
	else
		m_aSubTasks.InsertAt(nPos, pTDS);
	
	// setup ourselves as parent
	pTDS->m_pTDSParent = this;
	
	return TRUE;
}

// protected helper
BOOL TODOSTRUCTURE::AddSubTask(TODOSTRUCTURE* pTDS)
{
	// sanity checks
	ASSERT(pTDS && pTDS->GetTaskID());
	
	if (!pTDS || pTDS->GetTaskID() == 0)
		return FALSE;
	
	// check task with this ID does not already exist
	ASSERT(GetSubTaskPosition(pTDS->GetTaskID()) == -1);
	
	m_aSubTasks.Add(pTDS);
	
	// setup ourselves as parent
	pTDS->m_pTDSParent = this;
	
	return TRUE;
}

// protected helper
TODOSTRUCTURE* TODOSTRUCTURE::AddSubTask(DWORD dwID)
{
	TODOSTRUCTURE* pTDSChild = new TODOSTRUCTURE(dwID);
	VERIFY (AddSubTask(pTDSChild));
	return pTDSChild;
}


BOOL TODOSTRUCTURE::DeleteSubTask(int nPos)
{
	ASSERT(nPos >= 0 && nPos < GetSubTaskCount());
	
	if (nPos < 0 || nPos >= GetSubTaskCount())
		return FALSE;
	
	delete GetSubTask(nPos);
	m_aSubTasks.RemoveAt(nPos);
	
	return TRUE;
}

void TODOSTRUCTURE::CleanUp()
{
	// clean up children
	for (int nSubTask = 0; nSubTask < GetSubTaskCount(); nSubTask++)
	{
		TODOSTRUCTURE* pTDSChild = GetSubTask(nSubTask);
		ASSERT(pTDSChild);
		
		delete pTDSChild;
	}
	
	m_aSubTasks.RemoveAll();
}

int TODOSTRUCTURE::MoveSubTask(int nPos, TODOSTRUCTURE* pTDSDestParent, int nDestPos)
{
	// check destination is okay
	ASSERT (pTDSDestParent && nDestPos >= 0 && nDestPos <= pTDSDestParent->GetSubTaskCount());
	
	if (!pTDSDestParent || nDestPos < 0 || nDestPos > pTDSDestParent->GetSubTaskCount())
		return -1;
	
	TODOSTRUCTURE* pTDS = GetSubTask(nPos);
	ASSERT(pTDS);
	
	if (!pTDS)
		return -1;
	
	m_aSubTasks.RemoveAt(nPos); // remove from 'this' TODOSTRUCTURE
	
	// special case: the the source and destination are the same and the source
	// pos precedes the destination then we need to decrement the destination
	// to allow for having just deleted the source
	if (this == pTDSDestParent && nPos < nDestPos)
		nDestPos--;
	
	// add to destination
	pTDSDestParent->InsertSubTask(pTDS, nDestPos);
	
	return nDestPos;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

CToDoCtrlStructure::CToDoCtrlStructure(const CToDoCtrlStructure& tds)
{
   *this = tds;
}

CToDoCtrlStructure::~CToDoCtrlStructure()
{
	m_mapStructure.RemoveAll();
}

const CToDoCtrlStructure& CToDoCtrlStructure::operator=(const CToDoCtrlStructure& tds)
{
   TODOSTRUCTURE::operator=(tds);

   BuildMap();

   return *this;
}

DWORD CToDoCtrlStructure::GetPreviousTaskID(DWORD dwID) const
{
	TODOSTRUCTURE* pTDSParent = NULL;
	int nPos = -1;
	
	if (!FindTask(dwID, pTDSParent, nPos))
		return 0;
	
	// else
	return pTDSParent->GetPreviousSubTaskID(nPos);
}

DWORD CToDoCtrlStructure::GetParentTaskID(DWORD dwID) const
{
	TODOSTRUCTURE* pTDSParent = GetParentTask(dwID);
	
	if (!pTDSParent)
		return 0;
	
	return pTDSParent->GetTaskID();
}

TODOSTRUCTURE* CToDoCtrlStructure::GetParentTask(DWORD dwID) const
{
	TODOSTRUCTURE* pTDSParent = NULL;
	int nPos = -1;
	
	if (!FindTask(dwID, pTDSParent, nPos))
		return NULL;
	
	return pTDSParent;
}

TODOSTRUCTURE* CToDoCtrlStructure::AddTask(DWORD dwID, TODOSTRUCTURE* pTDSParent)
{
	TODOSTRUCTURE* pTDSChild = pTDSParent->AddSubTask(dwID);
	
	if (pTDSChild)
		AddToMap(pTDSChild);
	else
		ASSERT(0);

	return pTDSChild;
}

BOOL CToDoCtrlStructure::DeleteTask(DWORD dwID)
{
	ASSERT(dwID);
	
	if (!dwID)
		return FALSE;
	
	TODOSTRUCTURE* pTDS = FindTask(dwID);
	ASSERT(pTDS);
	
	if (!pTDS)
		return FALSE;
	
	TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();
	ASSERT(pTDSParent);
	
	if (!pTDSParent)
		return FALSE;
	
	int nPos = pTDSParent->GetSubTaskPos(pTDS);
	ASSERT(nPos != -1);
	
	if (nPos == -1)
		return FALSE;
	
	m_mapStructure.RemoveKey(dwID);
	
	return pTDSParent->DeleteSubTask(nPos);
}

TODOSTRUCTURE* CToDoCtrlStructure::FindTask(DWORD dwID) const
{
	TODOSTRUCTURE* pTDS = NULL;
	
	return (dwID && m_mapStructure.Lookup(dwID, pTDS)) ? pTDS : NULL;
}

BOOL CToDoCtrlStructure::FindTask(DWORD dwID, TODOSTRUCTURE*& pTDSParent, int& nPos) const
{
	pTDSParent = NULL;
	nPos = -1;
	
	TODOSTRUCTURE* pTDS = FindTask(dwID);
	
	if (!pTDS)
		return FALSE;
	
	pTDSParent = pTDS->GetParentTask();
	ASSERT(pTDSParent);
	
	if (!pTDSParent)
		return FALSE;
	
	nPos = pTDSParent->GetSubTaskPos(pTDS);
	ASSERT(nPos != -1);
	
	return (nPos !=-1);
}

BOOL CToDoCtrlStructure::InsertTask(DWORD dwID, TODOSTRUCTURE* pTDSParent, int nPos)
{
	return InsertTask(TODOSTRUCTURE(dwID), pTDSParent, nPos);
}

BOOL CToDoCtrlStructure::InsertTask(const TODOSTRUCTURE& tds, TODOSTRUCTURE* pTDSParent, int nPos)
{
	TODOSTRUCTURE* pTDSChild = new TODOSTRUCTURE(tds);
	
	if (!InsertTask(pTDSChild, pTDSParent, nPos))
	{
		delete pTDSChild;
		return FALSE;
	}
	
	return TRUE;
}

BOOL CToDoCtrlStructure::InsertTask(TODOSTRUCTURE* pTDS, TODOSTRUCTURE* pTDSParent, int nPos)
{
	if (pTDSParent->InsertSubTask(pTDS, nPos))
	{
		AddToMap(pTDS);
		return TRUE;
	}
	
	// else
	return FALSE;
}

void CToDoCtrlStructure::BuildMap()
{
	for (int nSubTask = 0; nSubTask < GetSubTaskCount(); nSubTask++)
	{
		TODOSTRUCTURE* pTDSChild = GetSubTask(nSubTask);
		ASSERT(pTDSChild);

      AddToMap(pTDSChild);
	}
}

void CToDoCtrlStructure::AddToMap(const TODOSTRUCTURE* pTDS)
{
	ASSERT(!pTDS->IsRoot());
	
	if (pTDS->IsRoot())
		return;
	
	m_mapStructure[pTDS->GetTaskID()] = const_cast<TODOSTRUCTURE*>(pTDS);
	
	// children
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		ASSERT(pTDSChild);
		
		AddToMap(pTDSChild);
	}
}

void CToDoCtrlStructure::RemoveFromMap(const TODOSTRUCTURE* pTDS)
{
	ASSERT(!pTDS->IsRoot());
	
	if (pTDS->IsRoot())
		return;
	
	m_mapStructure.RemoveKey(pTDS->GetTaskID());
	
	// children
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		ASSERT(pTDSChild);
		
		RemoveFromMap(pTDSChild);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////
