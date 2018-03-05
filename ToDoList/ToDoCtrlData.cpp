// ToDoCtrlData.cpp: implementation of the CToDoCtrlData class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ToDoCtrlData.h"
#include "TDCCustomAttributeHelper.h"
#include "resource.h"

#include "..\shared\xmlfile.h"
#include "..\shared\timehelper.h"
#include "..\shared\datehelper.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\enstring.h"
#include "..\shared\treectrlhelper.h"
#include "..\shared\treedragdrophelper.h"

#include <float.h>
#include <math.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

// what got changed as a result of AdjustTaskDates
enum 
{
	ADJUSTED_NONE	= 0x00,
	ADJUSTED_START	= 0x01,
	ADJUSTED_DUE	= 0x02,
	ADJUSTED_DONE	= 0x04,
};

//////////////////////////////////////////////////////////////////////

#define EDIT_GET_TDI(id, tdi)	\
{								\
	GET_TDI(id, tdi, SET_FAILED)\
}					

#define GET_TDI(id, tdi, ret)	\
{								\
	if (id == 0)				\
		return ret;				\
	tdi = GetTask(id);			\
	ASSERT(tdi);				\
	if (tdi == NULL)			\
		return ret;				\
}

#define GET_TDI_TDS(id, tdi, tds, ret)	\
{										\
	if (id == 0)						\
		return ret;						\
	VERIFY(GetTask(id, tdi, tds));		\
	ASSERT(tdi && tds);					\
	if (tdi == NULL || tds == NULL)		\
		return ret;						\
}

//////////////////////////////////////////////////////////////////////

CUndoAction::CUndoAction(CToDoCtrlData& data, TDCUNDOACTIONTYPE nType) : m_data(data)
{
	m_bSuccess = m_data.BeginNewUndoAction(nType);
}

CUndoAction::~CUndoAction()
{
	if (m_bSuccess)
		m_data.EndCurrentUndoAction();
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CToDoCtrlData::CToDoCtrlData(const CWordArray& aStyles) 
	: 
	m_aStyles(aStyles),
	m_nDefTimeEstUnits(THU_HOURS),
	m_nDefTimeSpentUnits(THU_HOURS)
{
	m_mapID2TDI.InitHashTable(2000);
}

CToDoCtrlData::~CToDoCtrlData()
{
	DeleteAllTasks();
}

int CToDoCtrlData::BuildDataModel(const CTaskFile& tasks)
{
	DeleteAllTasks();

	// add top-level items
	VERIFY(AddTaskToDataModel(tasks, NULL, &m_struct));

	return GetTaskCount();
}

BOOL CToDoCtrlData::AddTaskToDataModel(const CTaskFile& tasks, HTASKITEM hTask, TODOSTRUCTURE* pTDSParent)
{
	ASSERT(pTDSParent);

	if (!pTDSParent)
		return FALSE;

	if (hTask)	
	{
		DWORD dwTaskID = tasks.GetTaskID(hTask);

		TODOSTRUCTURE* pTDS = m_struct.AddTask(dwTaskID, pTDSParent);

		if (pTDS)
		{
			TODOITEM* pTDI = NewTask(tasks, hTask);
			ASSERT(pTDI);

			m_mapID2TDI[dwTaskID] = pTDI;
			//TRACE(_T("CToDoCtrlData::AddTaskToDataModel(%s, %ld)\n"), pTDI->sTitle, dwTaskID);
		}

		// newly added task becomes the parent
		pTDSParent = pTDS;
	}

	// tasks children
	HTASKITEM hSubtask = tasks.GetFirstTask(hTask);

	while (hSubtask)
	{
		VERIFY(AddTaskToDataModel(tasks, hSubtask, pTDSParent));

		// next task
		hSubtask = tasks.GetNextTask(hSubtask);
	}

	return TRUE;
}

TODOITEM* CToDoCtrlData::NewTask(const TODOITEM* pTDIRef, DWORD dwParentTaskID, const CTDCAttributeArray* pParentAttribs) const
{
	TODOITEM* pTDI = new TODOITEM(pTDIRef);

	if (dwParentTaskID && pParentAttribs)
		CopyTaskAttributes(pTDI, dwParentTaskID, *pParentAttribs);

	return pTDI;
}

TODOITEM* CToDoCtrlData::NewTask(const CTaskFile& tasks, HTASKITEM hTask) const
{
	ASSERT(hTask);

	if (!hTask)
		return NULL;

	TODOITEM* pTDI = NewTask();
	ASSERT(pTDI);

	if (!pTDI)
		return NULL;
	
	tasks.GetTaskAttributes(hTask, pTDI);
	
	// make sure incomplete tasks are not 100% and vice versa
	if (pTDI->IsDone())
		pTDI->nPercentDone = 100;
	else
		pTDI->nPercentDone = max(0, min(99, pTDI->nPercentDone));
	
	// set comments type if not set
	if (pTDI->sCommentsTypeID.IsEmpty()) 
		pTDI->sCommentsTypeID = m_cfDefault;

	return pTDI;
}

BOOL CToDoCtrlData::HasTask(DWORD& dwTaskID, BOOL bTrue) const
{
	return (GetTask(dwTaskID, bTrue) != NULL);
}

// external const version
const TODOITEM* CToDoCtrlData::GetTask(DWORD& dwTaskID, BOOL bTrue) const
{
	TODOITEM* pTDI = NULL;

	if (dwTaskID && m_mapID2TDI.Lookup(dwTaskID, pTDI))
	{
		// check for reference task
		if (bTrue && pTDI->dwTaskRefID)
		{
			DWORD dwRefID = pTDI->dwTaskRefID;
			
			if (dwRefID && m_mapID2TDI.Lookup(dwRefID, pTDI))
			{
				dwTaskID = dwRefID;
			}
		}
	}
	
	return pTDI;
}

// internal non-const version
TODOITEM* CToDoCtrlData::GetTask(DWORD& dwTaskID, BOOL bTrue)
{
	const CToDoCtrlData* pData = this;
	const TODOITEM* pTDI = pData->GetTask(dwTaskID, bTrue);

	return const_cast<TODOITEM*>(pTDI);
}

// internal non-const version
TODOITEM* CToDoCtrlData::GetTask(const TODOSTRUCTURE* pTDS) const
{
	if (!pTDS || !pTDS->GetTaskID())
		return NULL;
	
	// else
	DWORD dwTaskID = pTDS->GetTaskID();
	const TODOITEM* pTDI = GetTask(dwTaskID);

	return const_cast<TODOITEM*>(pTDI);
}

BOOL CToDoCtrlData::Locate(DWORD dwParentID, DWORD dwPrevSiblingID, TODOSTRUCTURE*& pTDSParent, int& nPos) const
{
	pTDSParent = NULL;
	nPos = -1;
	
	if (dwPrevSiblingID)
		m_struct.FindTask(dwPrevSiblingID, pTDSParent, nPos);
	
	else if (dwParentID)
		pTDSParent = m_struct.FindTask(dwParentID);
	else
		pTDSParent = const_cast<CToDoCtrlStructure*>(&m_struct); // root
	
	ASSERT (pTDSParent);
	return (pTDSParent != NULL);
}

BOOL CToDoCtrlData::LocateTask(DWORD dwTaskID, TODOSTRUCTURE*& pTDSParent, int& nPos) const
{
	return m_struct.FindTask(dwTaskID, pTDSParent, nPos);
}

const TODOSTRUCTURE* CToDoCtrlData::LocateTask(DWORD dwTaskID) const
{
	// special case
	if (dwTaskID == 0)
		return &m_struct;
	
	// else
	return m_struct.FindTask(dwTaskID);
}

void CToDoCtrlData::AddTask(DWORD dwTaskID, TODOITEM* pTDI, DWORD dwParentID, DWORD dwPrevSiblingID) 
{ 
	if (dwTaskID && pTDI)
	{
		// must delete duplicates else we'll get a memory leak
		TODOITEM* pExist = GetTask(dwTaskID, FALSE);
		
		if (pExist)
		{
			m_mapID2TDI.RemoveKey(dwTaskID);
			delete pExist;
		}
	}
	
	// add to structure
	TODOSTRUCTURE* pTDSParent = NULL;
	int nPrevSibling = -1;
	
	if (!Locate(dwParentID, dwPrevSiblingID, pTDSParent, nPrevSibling))
		return;
	
	m_struct.InsertTask(dwTaskID, pTDSParent, nPrevSibling + 1);
	m_mapID2TDI.SetAt(dwTaskID, pTDI); 
	
	AddUndoElement(TDCUEO_ADD, dwTaskID, dwParentID, dwPrevSiblingID);
	ResetCachedCalculations(dwTaskID, pTDI, TDCA_NEWTASK);
}

void CToDoCtrlData::DeleteAllTasks()
{
	// delete the data
	DWORD dwID;
	TODOITEM* pTDI;
	POSITION pos = m_mapID2TDI.GetStartPosition();

	while (pos)
	{
		m_mapID2TDI.GetNextAssoc(pos, dwID, pTDI);
		delete pTDI;
	}

	m_mapID2TDI.RemoveAll();

	// delete the structure
	m_struct.DeleteAll();

	ASSERT(GetTaskCount() == 0);
}

CString CToDoCtrlData::GetTaskTitle(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sTitle;
}

CString CToDoCtrlData::GetTaskIcon(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sIcon;
}

CString CToDoCtrlData::GetTaskComments(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sComments;
}

const CBinaryData& CToDoCtrlData::GetTaskCustomComments(DWORD dwTaskID, CString& sCommentsTypeID) const
{
	static CBinaryData content;
	sCommentsTypeID.Empty();

	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, content);
	
	sCommentsTypeID = pTDI->sCommentsTypeID;
	return pTDI->customComments;
}

double CToDoCtrlData::GetTaskCost(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	return pTDI->dCost;
}

double CToDoCtrlData::GetTaskTimeEstimate(DWORD dwTaskID, TCHAR& nUnits) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);

	nUnits = pTDI->nTimeEstUnits;
	return pTDI->dTimeEstimate;
}

double CToDoCtrlData::GetTaskTimeSpent(DWORD dwTaskID, TCHAR& nUnits) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	nUnits = pTDI->nTimeSpentUnits;
	return pTDI->dTimeSpent;
}

int CToDoCtrlData::GetTaskAllocTo(DWORD dwTaskID, CStringArray& aAllocTo) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	aAllocTo.Copy(pTDI->aAllocTo);
	return aAllocTo.GetSize();
}

CString CToDoCtrlData::GetTaskAllocBy(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sAllocBy;
}

CString CToDoCtrlData::GetTaskVersion(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sVersion;
}

CString CToDoCtrlData::GetTaskStatus(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sStatus;
}

int CToDoCtrlData::GetTaskCategories(DWORD dwTaskID, CStringArray& aCategories) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	aCategories.Copy(pTDI->aCategories);
	return aCategories.GetSize();
}

int CToDoCtrlData::GetTaskTags(DWORD dwTaskID, CStringArray& aTags) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	aTags.Copy(pTDI->aTags);
	return aTags.GetSize();
}

CString CToDoCtrlData::GetTaskPosition(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, _T(""));
	
	return GetTaskPosition(pTDI, pTDS);
}

CString CToDoCtrlData::GetTaskPath(DWORD dwTaskID, int nMaxLen) const
{ 
	if (nMaxLen == 0)
		return _T("");
	
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, _T(""));

	CString sPath = GetTaskPath(pTDI, pTDS);

	if (nMaxLen == -1 || sPath.IsEmpty() || sPath.GetLength() <= nMaxLen)
		return sPath;

	CStringArray aElements;

	int nNumElm = Misc::Split(sPath, '\\', aElements);
	ASSERT (nNumElm >= 2);

	int nTrimElm = ((sPath.GetLength() - nMaxLen) / nNumElm) + 2;

	for (int nElm = 0; nElm < nNumElm; nElm++)
	{
		CString& sElm = aElements[nElm];
		sElm = sElm.Left(sElm.GetLength() - nTrimElm) + "...";
	}

	return Misc::FormatArray(aElements, _T("\\"));
}

int CToDoCtrlData::GetTaskDependencies(DWORD dwTaskID, CStringArray& aDependencies) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	aDependencies.Copy(pTDI->aDependencies);
	return aDependencies.GetSize();
}

int CToDoCtrlData::GetTaskDependents(DWORD dwTaskID, CDWordArray& aDependents) const
{
	aDependents.RemoveAll();

	if (dwTaskID)
	{
		POSITION pos = m_mapID2TDI.GetStartPosition();
		CString sTaskID = Misc::Format(dwTaskID);
		
		CDWordArray aTemp;
		
		while (pos)
		{
			TODOITEM* pTDI = NULL;
			DWORD dwDependsID;
			
			m_mapID2TDI.GetNextAssoc(pos, dwDependsID, pTDI);
			ASSERT (pTDI);
			
			if (pTDI && dwDependsID != dwTaskID && Misc::Find(pTDI->aDependencies, sTaskID) != -1)
				aDependents.Add(dwDependsID);
		}
	}	
	
	return aDependents.GetSize();
}

void CToDoCtrlData::FixupTaskDependents(DWORD dwTaskID, DWORD dwPrevTaskID)
{
	CDWordArray aDependents;
	
	if (GetTaskDependents(dwPrevTaskID, aDependents))
	{
		CString sPrevTaskID = Misc::Format(dwPrevTaskID);
		CString sTaskID = Misc::Format(dwTaskID);

		int nTask = aDependents.GetSize();

		while (nTask--)
		{
			DWORD dwDependentID = aDependents[nTask];
			CStringArray aDepends;
	
			// delete existing dependency
			if (GetTaskDependencies(dwDependentID, aDepends))
				Misc::RemoveItem(sPrevTaskID, aDepends);

			// add new dependency
			aDepends.Add(sTaskID);

			// update dependencies
			SetTaskDependencies(dwDependentID, aDepends);
		}
	}
}

BOOL CToDoCtrlData::TaskHasCircularDependencies(DWORD dwTaskID) const
{
	// trace each of this tasks dependencies to see if it ever comes
	// back to itself.
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FALSE);
	
	CID2IDMap mapVisited;
	
	for (int nDepends = 0; nDepends < pTDI->aDependencies.GetSize(); nDepends++)
	{
		// we only handle 'same file' links
		DWORD dwIDLink = _ttoi(pTDI->aDependencies[nDepends]);
		
		if (dwIDLink)
		{
			if (FindDependency(dwIDLink, dwTaskID, mapVisited) == -1)
				return TRUE;
		}
	}
	
	// else
	return FALSE;
}

int CToDoCtrlData::FindDependency(DWORD dwTaskID, DWORD dwDependsID, CID2IDMap& mapVisited) const
{
	// simple checks
	if (!dwTaskID)
		return 0; // no such task == not found
	
	if (dwTaskID == dwDependsID)
		return -1; // same task == circular
	
	// have we been here before
	DWORD dwDummy;
	
	// if 'yes', we can stop
	if (mapVisited.Lookup(dwTaskID, dwDummy))
		return -1; // circular
	
	// else mark this task as having been visited
	mapVisited[dwTaskID] = TRUE;
	
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	for (int nDepends = 0; nDepends < pTDI->aDependencies.GetSize(); nDepends++)
	{
		// we only handle 'same file' links
		DWORD dwIDLink = _ttoi(pTDI->aDependencies[nDepends]);
		
		if (dwIDLink)
		{
			int nFind = FindDependency(dwIDLink, dwDependsID, mapVisited);
			
			if (nFind != 0) // found or circular
				return nFind;
			
			// else keep going
		}
	}
	
	// else
	return 0; 
}

BOOL CToDoCtrlData::FindDependency(DWORD dwTaskID, DWORD dwDependsID) const
{
	if (dwTaskID)
	{
		CID2IDMap mapVisited;
		return FindDependency(dwTaskID, dwDependsID, mapVisited);
	}

	// else
	return FALSE;
}

CString CToDoCtrlData::GetTaskExtID(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sExternalID;
}

CString CToDoCtrlData::GetTaskCreatedBy(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sCreatedBy;
}

COLORREF CToDoCtrlData::GetTaskColor(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	return pTDI->color;
}

int CToDoCtrlData::GetTaskPriority(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FM_NOPRIORITY);
	
	return pTDI->nPriority;
}

int CToDoCtrlData::GetTaskRisk(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FM_NORISK);
	
	return pTDI->nRisk;
}

CString CToDoCtrlData::GetTaskCustomAttributeData(DWORD dwTaskID, const CString& sAttribID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->GetCustomAttribValue(sAttribID);
}

BOOL CToDoCtrlData::CalcTaskCustomAttributeData(DWORD dwTaskID, const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, double& dValue) const
{
	if (dwTaskID && attribDef.SupportsCalculation())
	{
		const TODOITEM* pTDI = NULL;
		const TODOSTRUCTURE* pTDS = NULL;
		
		if (GetTask(dwTaskID, pTDI, pTDS))
			return CalcTaskCustomAttributeData(pTDI, pTDS, attribDef, dValue);

		ASSERT(0);
	}
	
	// all else
	return FALSE;
}

BOOL CToDoCtrlData::CalcTaskCustomAttributeData(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, double& dValue) const
{
	if (!attribDef.SupportsCalculation())
		return FALSE;

	const CString& sAttribID = attribDef.sUniqueID;

	if (!pTDI->CustomAttribNeedsRecalc(sAttribID))
		return pTDI->GetCustomAttribCalcValue(sAttribID, dValue);

	// else calculate
	CString sTaskVal = pTDI->GetCustomAttribValue(sAttribID);
	DWORD dwDataType = attribDef.GetDataType();

	// easier to handle by feature 
	// -----------------------------------------------------------
	if (attribDef.HasFeature(TDCCAF_ACCUMULATE))
	{
		ASSERT(dwDataType == TDCCA_DOUBLE || 
				dwDataType == TDCCA_INTEGER);

		// our value
		dValue = Misc::Atof(sTaskVal);
		
		// our children's values
		for (int i = 0; i < pTDS->GetSubTaskCount(); i++)
		{
			double dSubtaskVal;
			DWORD dwSubtaskID = pTDS->GetSubTaskID(i);
			
			// ignore references else risk of infinite loop
			if (!IsTaskReference(dwSubtaskID) && CalcTaskCustomAttributeData(dwSubtaskID, attribDef, dSubtaskVal))
				dValue += dSubtaskVal;
		}
	}
	// -----------------------------------------------------------
	else if (attribDef.HasFeature(TDCCAF_MAXIMIZE))
	{
		ASSERT(dwDataType == TDCCA_DOUBLE || 
				dwDataType == TDCCA_INTEGER ||
				dwDataType == TDCCA_DATE);

		if (sTaskVal.IsEmpty())
			dValue = -DBL_MAX;
		else
			dValue = Misc::Atof(sTaskVal);

		// our children's values
		for (int i = 0; i < pTDS->GetSubTaskCount(); i++)
		{
			double dSubtaskVal;
			DWORD dwSubtaskID = pTDS->GetSubTaskID(i);
			
			// ignore references else risk of infinite loop
			if (!IsTaskReference(dwSubtaskID) && CalcTaskCustomAttributeData(dwSubtaskID, attribDef, dSubtaskVal))
				dValue = max(dSubtaskVal, dValue);
		}

		if (dValue <= -DBL_MAX)
			dValue = TODOITEM::NULL_VALUE;
	}
	// -----------------------------------------------------------
	else if (attribDef.HasFeature(TDCCAF_MINIMIZE))
	{
		ASSERT(dwDataType == TDCCA_DOUBLE || 
			dwDataType == TDCCA_INTEGER ||
			dwDataType == TDCCA_DATE);
		
		if (sTaskVal.IsEmpty())
			dValue = DBL_MAX;
		else
			dValue = Misc::Atof(sTaskVal);

		// our children's values
		for (int i = 0; i < pTDS->GetSubTaskCount(); i++)
		{
			double dSubtaskVal;
			DWORD dwSubtaskID = pTDS->GetSubTaskID(i);
			
			// ignore references else risk of infinite loop
			if (!IsTaskReference(dwSubtaskID) && CalcTaskCustomAttributeData(dwSubtaskID, attribDef, dSubtaskVal))
				dValue = min(dSubtaskVal, dValue);
		}

		if (dValue >= DBL_MAX)
			dValue = TODOITEM::NULL_VALUE;
	}
	// -----------------------------------------------------------
	else
	{
		return FALSE;
	}

	// update cached value
	pTDI->SetCustomAttribCalcValue(sAttribID, dValue);

	return (dValue != TODOITEM::NULL_VALUE);
}

BOOL CToDoCtrlData::IsTaskFlagged(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FALSE);
	
	return pTDI->bFlagged;
}

BOOL CToDoCtrlData::IsTaskRecurring(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FALSE);
	
	return (pTDI->trRecurrence.nRegularity != TDIR_ONCE);
}

BOOL CToDoCtrlData::GetTaskRecurrence(DWORD dwTaskID, TDIRECURRENCE& tr) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FALSE);
	
	tr = pTDI->trRecurrence;
	return TRUE;
}

BOOL CToDoCtrlData::GetTaskNextOccurrence(DWORD dwTaskID, COleDateTime& dtNext, BOOL& bDue) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, FALSE);
	
	return pTDI->GetNextOccurence(dtNext, bDue);
}

COleDateTime CToDoCtrlData::GetTaskDate(DWORD dwTaskID, TDC_DATE nDate) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0.0);
	
	switch (nDate)
	{
	case TDCD_CREATE:		return pTDI->dateCreated;
	case TDCD_START:		return pTDI->dateStart;
	case TDCD_STARTDATE:	return CDateHelper::GetDateOnly(pTDI->dateStart);
	case TDCD_STARTTIME:	return CDateHelper::GetTimeOnly(pTDI->dateStart);
	case TDCD_DUE:			return pTDI->dateDue;
	case TDCD_DUEDATE:		return CDateHelper::GetDateOnly(pTDI->dateDue);
	case TDCD_DUETIME:		return CDateHelper::GetTimeOnly(pTDI->dateDue);
	case TDCD_DONE:			return pTDI->dateDone;
	case TDCD_DONEDATE:		return CDateHelper::GetDateOnly(pTDI->dateDone);
	case TDCD_DONETIME:		return CDateHelper::GetTimeOnly(pTDI->dateDone);
	}

	ASSERT(0);
	return 0.0;
}

BOOL CToDoCtrlData::TaskHasDate(DWORD dwTaskID, TDC_DATE nDate) const
{
	return CDateHelper::IsDateSet(GetTaskDate(dwTaskID, nDate));
}

int CToDoCtrlData::GetTaskPercent(DWORD dwTaskID, BOOL bCheckIfDone) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, 0);
	
	if (bCheckIfDone)
		return pTDI->IsDone() ? 100 : pTDI->nPercentDone;
	
	// else
	return pTDI->nPercentDone;
}

CString CToDoCtrlData::GetTaskFileRef(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	GET_TDI(dwTaskID, pTDI, _T(""));
	
	return pTDI->sFileRefPath;
}

DWORD CToDoCtrlData::GetTaskParentID(DWORD dwTaskID) const
{
	if (dwTaskID)
	{
		const TODOSTRUCTURE* pTDS = LocateTask(dwTaskID);
		ASSERT(pTDS);

		if (pTDS)
			return pTDS->GetParentTaskID();
	}

	// else
	return 0;
}

BOOL CToDoCtrlData::RemoveOrphanTaskReferences(TODOSTRUCTURE* pTDSParent)
{
	ASSERT(pTDSParent);

	int nChild = pTDSParent->GetSubTaskCount();
	BOOL bRemoved = FALSE;

	while (nChild--)
	{
		TODOSTRUCTURE* pTDSChild = pTDSParent->GetSubTask(nChild);

		// children's children first
		if (RemoveOrphanTaskReferences(pTDSChild))
			bRemoved = TRUE;

		// then child
		TODOITEM* pTDIChild = GetTask(pTDSChild);
		ASSERT(pTDIChild);

		if (pTDIChild)
		{
			DWORD dwTaskRef = pTDIChild->dwTaskRefID;

			if (dwTaskRef && GetTask(dwTaskRef, FALSE) == NULL)
			{
				DeleteTask(pTDSParent, nChild, FALSE);
				bRemoved = TRUE;
			}
		}
	}

	return bRemoved;
}

DWORD CToDoCtrlData::GetTaskReferenceID(DWORD dwTaskID) const
{
	// NOTE: can't use GET_TDI macro here because 
	// that'll give us the 'true' task
	if (dwTaskID == 0)
		return 0;

	const TODOITEM* pTDI = GetTask(dwTaskID, FALSE);
	ASSERT(pTDI);

	return (pTDI ? pTDI->dwTaskRefID : 0);
}

BOOL CToDoCtrlData::CanMoveTask(DWORD /*dwTaskID*/, DWORD /*dwDestParentID*/) const
{
	return TRUE;
}

BOOL CToDoCtrlData::IsTaskReference(DWORD dwTaskID) const
{
	return (GetTaskReferenceID(dwTaskID) != 0);
}

BOOL CToDoCtrlData::DeleteTask(DWORD dwTaskID)
{
	if (dwTaskID)
	{
		TODOSTRUCTURE* pTDSParent = NULL;
		int nPos = -1;
		
		if (!m_struct.FindTask(dwTaskID, pTDSParent, nPos))
		{
			ASSERT(0);
			return FALSE;
		}

		ResetCachedCalculations(dwTaskID, NULL, TDCA_DELETE);
		
		// delete the task itself so that we don't have to worry
		// about checking if it has a reference to itself
		return DeleteTask(pTDSParent, nPos);
	}

	// else
	return FALSE;
}

BOOL CToDoCtrlData::DeleteTask(TODOSTRUCTURE* pTDSParent, int nPos, BOOL bWithUndo)
{
	TODOSTRUCTURE* pTDS = pTDSParent->GetSubTask(nPos);
	ASSERT(pTDS);
	
	// do children first to ensure entire branch is deleted
	while (pTDS->GetSubTaskCount() > 0)
		DeleteTask(pTDS, 0);

	// is this task a reference?
	DWORD dwTaskID = pTDS->GetTaskID();
	BOOL bRef = IsTaskReference(dwTaskID);

	// save undo 
	DWORD dwParentID = pTDSParent->GetTaskID();
	DWORD dwPrevSiblingID = nPos ? pTDSParent->GetSubTaskID(nPos - 1) : 0;
	
	if (bWithUndo)
		AddUndoElement(TDCUEO_DELETE, dwTaskID, dwParentID, dwPrevSiblingID);
	
	// then this item
	delete GetTask(dwTaskID, FALSE);
	m_mapID2TDI.RemoveKey(dwTaskID);
	m_struct.DeleteTask(dwTaskID);

	// then it's referees unless it was a reference itself
	if (!bRef)
		RemoveOrphanTaskReferences(&m_struct);
	
	return TRUE;
}

void CToDoCtrlData::ResetCachedCalculations(BOOL bIncCustom) const
{
	POSITION pos = m_mapID2TDI.GetStartPosition();
	TODOITEM* pTDI = NULL;
	DWORD dwTaskID = 0;
	
	while (pos)
	{
		m_mapID2TDI.GetNextAssoc(pos, dwTaskID, pTDI);

		if (pTDI)
			pTDI->ResetCalcs(TDCA_ALL, bIncCustom);
	}
}

void CToDoCtrlData::ResetCustomCachedCalculations() const
{
	POSITION pos = m_mapID2TDI.GetStartPosition();
	TODOITEM* pTDI = NULL;
	DWORD dwTaskID = 0;
	
	while (pos)
	{
		m_mapID2TDI.GetNextAssoc(pos, dwTaskID, pTDI);

		if (pTDI)
			pTDI->ResetCustomAttribCalc();
	}
}

void CToDoCtrlData::ResetCachedCalculations(DWORD dwTaskID, const TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, BOOL bAndParents, BOOL bAndSubtasks) const
{
	if (!dwTaskID)
		return;

	// References don't HAVE attributes and can only have
	// children that themselves are references.
	// Also (and more importantly) we avoid possibility of 
	// circular referencing => stack overflow
	if (IsTaskReference(dwTaskID))
		return;

	// first handle the task itself
	if (!pTDI)
	{
		pTDI = GetTask(dwTaskID);
		ASSERT(pTDI);

		if (!pTDI)
			return;
	}

	pTDI->ResetCalcs(nAttrib);

	// then optionally handle parents
	const TODOSTRUCTURE* pTDS = NULL;

	if (bAndParents)
	{
		pTDS = LocateTask(dwTaskID);
		ASSERT(pTDS);

		if (!pTDS)
			return;

		// TRUE = reset parent of parent
		// FALSE = don't reset children
		ResetCachedCalculations(pTDS->GetParentTaskID(), NULL, nAttrib, TRUE, FALSE);
	}

	// and/or children
	if (bAndSubtasks)
	{
		if (pTDS == NULL)
		{
			pTDS = LocateTask(dwTaskID);
			ASSERT(pTDS);
			
			if (!pTDS)
				return;
		}

		// FALSE = don't reset parent (already done)
		// TRUE = reset children of item
		int nPos = pTDS->GetSubTaskCount();

		while (nPos--)
			ResetCachedCalculations(pTDS->GetSubTaskID(nPos), NULL, nAttrib, FALSE, TRUE);
	}
}

void CToDoCtrlData::ResetCachedCalculations(DWORD dwTaskID, const TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib) const
{
	switch (nAttrib)
	{
	case TDCA_TIMEEST:
	case TDCA_TIMESPENT:
		{
			// affects parent tasks but not children
			ResetCachedCalculations(dwTaskID, pTDI, nAttrib, TRUE, FALSE);

			// and optionally percent complete
			if (HasStyle(TDCS_AUTOCALCPERCENTDONE))
				ResetCachedCalculations(dwTaskID, pTDI, TDCA_PERCENT); // RECURSIVE call
		}
		break;
		
	case TDCA_DONEDATE:
		{
			// affects parents and children
			ResetCachedCalculations(dwTaskID, pTDI, nAttrib, TRUE, TRUE);
		}
		break;

	case TDCA_PERCENT:
		if (HasStyle(TDCS_USEPERCENTDONEINTIMEEST))
		{
			// affects parents but not children
			ResetCachedCalculations(dwTaskID, pTDI, TDCA_TIMEEST, TRUE, FALSE);
		}
		// fall thru

	case TDCA_DUEDATE:
	case TDCA_STARTDATE:
	case TDCA_RISK:
	case TDCA_PRIORITY:
	case TDCA_COST:
	case TDCA_CUSTOMATTRIB:
	case TDCA_CUSTOMATTRIBDEFS:
		{
			// affects parents but not children
			ResetCachedCalculations(dwTaskID, pTDI, nAttrib, TRUE, FALSE);
		}
		break;

	case TDCA_NEWTASK:
	case TDCA_DELETE:
		{
			// affects siblings of the task and their subtasks
			TODOSTRUCTURE* pTDSParent = NULL;
			int nPos = 0;

			if (LocateTask(dwTaskID, pTDSParent, nPos) && pTDSParent)
			{
				while (++nPos < pTDSParent->GetSubTaskCount())
				{
					// FALSE = don't reset parent
					// TRUE = reset children
					ResetCachedCalculations(pTDSParent->GetSubTaskID(nPos), NULL, nAttrib, FALSE, TRUE);
				}
			}
		}
		break;
		
	case TDCA_TASKNAME:
		{
			// affects children but not parents
			ResetCachedCalculations(dwTaskID, pTDI, nAttrib, FALSE, TRUE);
		}
		break;

	case TDCA_CATEGORY:
	case TDCA_ALLOCTO:
	case TDCA_TAGS:
		{
			// affects neither parents not children
			ResetCachedCalculations(dwTaskID, pTDI, nAttrib, FALSE, FALSE);
		}
		break;
	}
}

void CToDoCtrlData::ResetCachedCalculations(DWORD dwTaskID, const TODOITEM* pTDI, const CString& sAttribID) const
{
	if (!dwTaskID)
		return;

	if (!pTDI)
	{
		pTDI = GetTask(dwTaskID);
		ASSERT(pTDI);
	}

	// work our way up the parent chain, resetting as we go
	while (pTDI)
	{
		pTDI->ResetCustomAttribCalc(sAttribID);

		dwTaskID = GetTaskParentID(dwTaskID);
		pTDI = GetTask(dwTaskID);
	}
}


int CToDoCtrlData::CopyTaskAttributes(TODOITEM* pToTDI, DWORD dwFromTaskID, const CTDCAttributeArray& aAttribs) const
{
	if (!pToTDI)
		return SET_FAILED;
	
	const TODOITEM* pFromTDI = NULL;
	EDIT_GET_TDI(dwFromTaskID, pFromTDI);
	
	int nRes = SET_NOCHANGE;
	
	// helper macros
#define COPYATTRIB(a) if (pToTDI->a != pFromTDI->a) { pToTDI->a = pFromTDI->a; nRes = SET_CHANGE; }
#define COPYATTRIBARR(a) if (!Misc::MatchAll(pToTDI->a, pFromTDI->a)) { pToTDI->a.Copy(pFromTDI->a); nRes = SET_CHANGE; }
	
	// note: we don't use the public SetTask* methods purely so we can
	// capture all the edits as a single atomic change that can be undone
	for (int nAtt = 0; nAtt < aAttribs.GetSize(); nAtt++)
	{
		TDC_ATTRIBUTE nAttrib = aAttribs[nAtt];
		
		switch (nAttrib)
		{
		case TDCA_TASKNAME:		COPYATTRIB(sTitle); break;
		case TDCA_DONEDATE:		COPYATTRIB(dateDone); break;
		case TDCA_DUEDATE:		COPYATTRIB(dateDue); break;
		case TDCA_STARTDATE:	COPYATTRIB(dateStart); break;
		case TDCA_PRIORITY:		COPYATTRIB(nPriority); break;
		case TDCA_RISK:			COPYATTRIB(nRisk); break;
		case TDCA_COLOR:		COPYATTRIB(color); break;
		case TDCA_ALLOCBY:		COPYATTRIB(sAllocBy); break;
		case TDCA_STATUS:		COPYATTRIB(sStatus); break;
		case TDCA_PERCENT:		COPYATTRIB(nPercentDone); break;
		case TDCA_FILEREF:		COPYATTRIB(sFileRefPath); break;
		case TDCA_VERSION:		COPYATTRIB(sVersion); break;
		case TDCA_EXTERNALID:	COPYATTRIB(sExternalID); break;
		case TDCA_FLAG:			COPYATTRIB(bFlagged); break;
			
		case TDCA_TIMEEST:		COPYATTRIB(dTimeEstimate); 
								COPYATTRIB(nTimeEstUnits); break;
		case TDCA_TIMESPENT:	COPYATTRIB(dTimeSpent);	
								COPYATTRIB(nTimeSpentUnits); break;
			
		case TDCA_COMMENTS:		COPYATTRIB(sComments); 
								COPYATTRIB(customComments); 
								COPYATTRIB(sCommentsTypeID); break;
			
		case TDCA_ALLOCTO:		COPYATTRIBARR(aAllocTo); break;
		case TDCA_CATEGORY:		COPYATTRIBARR(aCategories); break;
		case TDCA_TAGS:			COPYATTRIBARR(aTags); break;
		case TDCA_DEPENDENCY:	COPYATTRIBARR(aDependencies); break;

		default:
			ASSERT(0);
		}
	}
	
	return nRes;
}

int CToDoCtrlData::ClearTaskAttribute(DWORD dwTaskID, TDC_ATTRIBUTE nAttrib)
{
	switch (nAttrib)
	{
	case TDCA_DONEDATE:		return SetTaskDate(dwTaskID, TDCD_DONE, 0.0);
	case TDCA_DUEDATE:		return SetTaskDate(dwTaskID, TDCD_DUE, 0.0);
	case TDCA_STARTDATE:	return SetTaskDate(dwTaskID, TDCD_START, 0.0);
		
	case TDCA_PRIORITY:		return SetTaskPriority(dwTaskID, FM_NOPRIORITY);
	case TDCA_RISK:			return SetTaskRisk(dwTaskID, FM_NORISK);
		
	case TDCA_ALLOCTO:		return SetTaskAllocTo(dwTaskID, CStringArray());
	case TDCA_CATEGORY:		return SetTaskCategories(dwTaskID, CStringArray());
	case TDCA_TAGS:			return SetTaskTags(dwTaskID, CStringArray());
	case TDCA_DEPENDENCY:	return SetTaskDependencies(dwTaskID, CStringArray());
		
	case TDCA_ALLOCBY:		return SetTaskAllocBy(dwTaskID, _T(""));
	case TDCA_STATUS:		return SetTaskStatus(dwTaskID, _T(""));
	case TDCA_FILEREF:		return SetTaskFileRef(dwTaskID, _T(""));
	case TDCA_VERSION:		return SetTaskVersion(dwTaskID, _T(""));
	case TDCA_EXTERNALID:	return SetTaskExtID(dwTaskID, _T(""));
	case TDCA_ICON:			return SetTaskIcon(dwTaskID, _T(""));
		
	case TDCA_PERCENT:		return SetTaskPercent(dwTaskID, 0);
	case TDCA_FLAG:			return SetTaskFlag(dwTaskID, FALSE);
	case TDCA_COST:			return SetTaskCost(dwTaskID, 0.0);
	case TDCA_COLOR:		return SetTaskColor(dwTaskID, 0);
	case TDCA_RECURRENCE:	return SetTaskRecurrence(dwTaskID, TDIRECURRENCE());
		
	case TDCA_TIMEEST:		
		{
			TCHAR nUnits;
			GetTaskTimeEstimate(dwTaskID, nUnits); // preserve existing units
			return SetTaskTimeEstimate(dwTaskID, 0.0, nUnits);
		}

	case TDCA_TIMESPENT:
		{
			TCHAR nUnits;
			GetTaskTimeSpent(dwTaskID, nUnits); // preserve existing units
			return SetTaskTimeSpent(dwTaskID, 0.0, nUnits);
		}
	}
	
	ASSERT (0);
	return FALSE;
}

int CToDoCtrlData::ClearTaskCustomAttribute(DWORD dwTaskID, const CString& sAttribID)
{
	return SetTaskCustomAttributeData(dwTaskID, sAttribID, _T(""));
}

BOOL CToDoCtrlData::ApplyLastChangeToSubtasks(DWORD dwTaskID, TDC_ATTRIBUTE nAttrib, BOOL bIncludeBlank)
{
	if (dwTaskID)
	{
		const TODOITEM* pTDI = NULL;
		const TODOSTRUCTURE* pTDS = NULL;

		if (GetTask(dwTaskID, pTDI, pTDS))
			return ApplyLastChangeToSubtasks(pTDI, pTDS, nAttrib, bIncludeBlank);
	}

	// else
	return FALSE;
}

BOOL CToDoCtrlData::ApplyLastChangeToSubtasks(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, 
											  TDC_ATTRIBUTE nAttrib, BOOL bIncludeBlank)
{
	ASSERT(pTDI && pTDS);
	
	if (!pTDI || !pTDS)
		return FALSE;
	
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		DWORD dwChildID = pTDS->GetSubTaskID(nSubTask);
		TODOITEM* pTDIChild = GetTask(dwChildID);
		ASSERT(pTDIChild);

		if (!pTDIChild)
			return FALSE;

		// save undo data
		SaveEditUndo(dwChildID, pTDIChild, nAttrib);
		
		// apply the change based on nAttrib
		switch (nAttrib)
		{
		case TDCA_DONEDATE:
			if (bIncludeBlank || pTDI->IsDone())
				pTDIChild->dateDone = pTDI->dateDone;
			break;
			
		case TDCA_DUEDATE:
			if (bIncludeBlank || pTDI->HasDue())
				pTDIChild->dateDue = pTDI->dateDue;
			break;
			
		case TDCA_STARTDATE:
			if (bIncludeBlank || pTDI->HasStart())
				pTDIChild->dateStart = pTDI->dateStart;
			break;
			
		case TDCA_PRIORITY:
			if (bIncludeBlank || pTDI->nPriority != FM_NOPRIORITY)
				pTDIChild->nPriority = pTDI->nPriority;
			break;
			
		case TDCA_RISK:
			if (bIncludeBlank || pTDI->nRisk != FM_NORISK)
				pTDIChild->nRisk = pTDI->nRisk;
			break;
			
		case TDCA_COLOR:
			if (bIncludeBlank || pTDI->color != 0)
				pTDIChild->color = pTDI->color;
			break;
			
		case TDCA_ALLOCTO:
			if (bIncludeBlank || pTDI->aAllocTo.GetSize())
				pTDIChild->aAllocTo.Copy(pTDI->aAllocTo);
			break;
			
		case TDCA_ALLOCBY:
			if (bIncludeBlank || !pTDI->sAllocBy.IsEmpty())
				pTDIChild->sAllocBy = pTDI->sAllocBy;
			break;
			
		case TDCA_STATUS:
			if (bIncludeBlank || !pTDI->sStatus.IsEmpty())
				pTDIChild->sStatus = pTDI->sStatus;
			break;
			
		case TDCA_CATEGORY:
			if (bIncludeBlank || pTDI->aCategories.GetSize())
				pTDIChild->aCategories.Copy(pTDI->aCategories);
			break;
			
		case TDCA_TAGS:
			if (bIncludeBlank || pTDI->aTags.GetSize())
				pTDIChild->aTags.Copy(pTDI->aTags);
			break;
			
		case TDCA_PERCENT:
			if (bIncludeBlank || pTDI->nPercentDone)
				pTDIChild->nPercentDone = pTDI->nPercentDone;
			break;
			
		case TDCA_TIMEEST:
			if (bIncludeBlank || pTDI->dTimeEstimate > 0)
				pTDIChild->dTimeEstimate = pTDI->dTimeEstimate;
			break;
			
		case TDCA_FILEREF:
			if (bIncludeBlank || !pTDI->sFileRefPath.IsEmpty())
				pTDIChild->sFileRefPath = pTDI->sFileRefPath;
			break;
			
		case TDCA_VERSION:
			if (bIncludeBlank || !pTDI->sVersion.IsEmpty())
				pTDIChild->sVersion = pTDI->sVersion;
			break;
			
		case TDCA_FLAG:
			if (bIncludeBlank || pTDI->bFlagged)
				pTDIChild->bFlagged = pTDI->bFlagged;
			break;
			
		case TDCA_EXTERNALID:
			if (bIncludeBlank || !pTDI->sExternalID.IsEmpty())
				pTDIChild->sExternalID = pTDI->sExternalID;
			break;
			
		default:
			ASSERT (0);
			return FALSE;
		}

		// we can reset calc directly on the task without worrying
		// about the parent because the parent has already been
		// handled as a consequence of arriving here!
		pTDIChild->ResetCalcs(nAttrib);
		
		// and its children too
		ApplyLastChangeToSubtasks(pTDIChild, pTDS->GetSubTask(nSubTask), nAttrib, bIncludeBlank);
	}
	
	return TRUE;
}

int CToDoCtrlData::SetTaskColor(DWORD dwTaskID, COLORREF color)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	// if the color is 0 then add 1 to discern from unset
	if (color == 0)
		color = 1;
	
	else if (color == CLR_NONE) 
		color = 0;
			
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_COLOR, (int&)pTDI->color, (int)color);
}

int CToDoCtrlData::SetTaskComments(DWORD dwTaskID, const CString& sComments, const CBinaryData& customComments, const CString& sCommentsTypeID)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	BOOL bCommentsChange = (pTDI->sComments != sComments);
	BOOL bCustomCommentsChange = bCommentsChange;
	
	if (!bCustomCommentsChange)
	{
		bCustomCommentsChange = (pTDI->sCommentsTypeID != sCommentsTypeID) ||
			(pTDI->customComments != customComments);
	}
	
	if (bCommentsChange || bCustomCommentsChange)
	{
		// save undo data
		SaveEditUndo(dwTaskID, pTDI, TDCA_COMMENTS);
		
		if (bCommentsChange)
			pTDI->sComments = sComments;
		
		if (bCustomCommentsChange)
		{
			// if we're changing comments type we clear the custom comments
			if (pTDI->sCommentsTypeID != sCommentsTypeID)
				pTDI->customComments.Empty();
			else
				pTDI->customComments = customComments;
			
			pTDI->sCommentsTypeID = sCommentsTypeID;
		}
		
		pTDI->SetModified();
		
		ResetCachedCalculations(dwTaskID, pTDI, TDCA_COMMENTS);
		
		return SET_CHANGE;
	}
	
	// else
	return SET_NOCHANGE;
}

int CToDoCtrlData::SetTaskCommentsType(DWORD dwTaskID, const CString& sCommentsTypeID)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_COMMENTS, pTDI->sCommentsTypeID, sCommentsTypeID);
}

int CToDoCtrlData::SetTaskTitle(DWORD dwTaskID, const CString& sTitle)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_TASKNAME, pTDI->sTitle, sTitle);
}

int CToDoCtrlData::SetTaskIcon(DWORD dwTaskID, const CString& sIcon)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_ICON, pTDI->sIcon, sIcon);
}

int CToDoCtrlData::SetTaskFlag(DWORD dwTaskID, BOOL bFlagged)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_FLAG, pTDI->bFlagged, bFlagged);
}

int CToDoCtrlData::SetTaskCustomAttributeData(DWORD dwTaskID, const CString& sAttribID, const CString& sData)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	CString sOldData = GetTaskCustomAttributeData(dwTaskID, sAttribID);
	
	if (sOldData != sData)
	{
		// save undo data
		SaveEditUndo(dwTaskID, pTDI, TDCA_CUSTOMATTRIB);
		
		// make changes
		pTDI->mapCustomData[sAttribID] = sData;
		pTDI->SetModified();
		
		// update calcs
		ResetCachedCalculations(dwTaskID, pTDI, sAttribID);
		return SET_CHANGE;
	}
	return SET_NOCHANGE;
}

int CToDoCtrlData::SetTaskRecurrence(DWORD dwTaskID, const TDIRECURRENCE& tr)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	if (pTDI->trRecurrence != tr)
	{
		// save undo data
		SaveEditUndo(dwTaskID, pTDI, TDCA_RECURRENCE);
		
		// make changes
		pTDI->trRecurrence = tr;
		pTDI->SetModified();
		
		// update calcs
		ResetCachedCalculations(dwTaskID, pTDI, TDCA_RECURRENCE);
		
		return SET_CHANGE;
	}
	return SET_NOCHANGE;
}

int CToDoCtrlData::SetTaskPriority(DWORD dwTaskID, int nPriority)
{
	if (!(nPriority == FM_NOPRIORITY || (nPriority >= 0 && nPriority <= 10)))
		return SET_FAILED;

	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_PRIORITY, pTDI->nPriority, nPriority);
}

int CToDoCtrlData::SetTaskRisk(DWORD dwTaskID, int nRisk)
{
	if (!(nRisk == FM_NORISK || (nRisk >= 0 && nRisk <= 10)))
		return SET_FAILED;

	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	return EditTaskAttribute(dwTaskID, pTDI, TDCA_RISK, pTDI->nRisk, nRisk);
}

TDC_ATTRIBUTE CToDoCtrlData::MapDateToAttribute(TDC_DATE nDate)
{
	switch (nDate)
	{
	case TDCD_CREATE:	return TDCA_CREATIONDATE;	
	case TDCD_START:	
	case TDCD_STARTDATE:
	case TDCD_STARTTIME:return TDCA_STARTDATE;	
	case TDCD_DUE:		
	case TDCD_DUEDATE:
	case TDCD_DUETIME:	return TDCA_DUEDATE;	
	case TDCD_DONE:		
	case TDCD_DONEDATE:
	case TDCD_DONETIME:	return TDCA_DONEDATE;
	}
	
	// else
	ASSERT(0);
	return TDCA_NONE;
}

int CToDoCtrlData::SetTaskDate(DWORD dwTaskID, TDC_DATE nDate, const COleDateTime& date)
{
	COleDateTime dtDate = date;
	
	// whole days only for creation
	if (nDate == TDCD_CREATE)
		dtDate = CDateHelper::GetDateOnly(dtDate);
	
	COleDateTime dtCur = GetTaskDate(dwTaskID, nDate);
	
	if (dtCur != dtDate)
	{
		TODOITEM* pTDI = NULL;
		EDIT_GET_TDI(dwTaskID, pTDI);
		
		// save undo data
		SaveEditUndo(dwTaskID, pTDI, MapDateToAttribute(nDate));
		
		switch (nDate)
		{
		case TDCD_CREATE:	
			pTDI->dateCreated = dtDate;	
			break;
			
		case TDCD_START:	
			pTDI->dateStart = dtDate;		
			break;
			
		case TDCD_STARTDATE:		
			// add date to existing time component unless existing date is 0.0
			if (!CDateHelper::IsDateSet(dtDate) || !pTDI->HasStart())
				pTDI->dateStart = CDateHelper::GetDateOnly(dtDate);
			else
				pTDI->dateStart = CDateHelper::MakeDate(dtDate, pTDI->dateStart);		
			break;
			
		case TDCD_STARTTIME:		
			// add time to date component only if it exists
			if (pTDI->HasStart())
				pTDI->dateStart = CDateHelper::MakeDate(pTDI->dateStart, dtDate);		
			break;
			
		case TDCD_DUE:		
			pTDI->dateDue = dtDate;		
			break;
			
		case TDCD_DUEDATE:		
			// add date to existing time component unless existing date is 0.0
			if (!CDateHelper::IsDateSet(dtDate) || !pTDI->HasDue())
				pTDI->dateDue = CDateHelper::GetDateOnly(dtDate);
			else
				pTDI->dateDue = CDateHelper::MakeDate(dtDate, pTDI->dateDue);		
			break;
			
		case TDCD_DUETIME:		
			// add time to date component only if it exists
			if (pTDI->HasDue())
				pTDI->dateDue = CDateHelper::MakeDate(pTDI->dateDue, dtDate);		
			break;
			
		case TDCD_DONE:		
			{
				BOOL bWasDone = pTDI->IsDone();
				pTDI->dateDone = dtDate;
				
				// reset % completion if going from done to not-done
				if (bWasDone && !pTDI->IsDone() && pTDI->nPercentDone == 100)
					pTDI->nPercentDone = 0;
			}
			break;
			
		case TDCD_DONEDATE:		
			// add date to existing time component unless date is 0.0
			if (!CDateHelper::IsDateSet(dtDate) || !pTDI->IsDone())
				pTDI->dateDone = CDateHelper::GetDateOnly(dtDate);
			else
				pTDI->dateDone = CDateHelper::MakeDate(dtDate, pTDI->dateDone);		
			break;
			
		case TDCD_DONETIME:		
			// add time to date component only if it exists
			if (pTDI->IsDone())
				pTDI->dateDone = CDateHelper::MakeDate(pTDI->dateDone, dtDate);		
			break;
			
		default:
			ASSERT(0);
			return SET_FAILED;
		}
		
		pTDI->SetModified();
		
		ResetCachedCalculations(dwTaskID, pTDI, MapDateToAttribute(nDate));
		
		// update dependent dates
		FixupTaskDependencyDates(dwTaskID, nDate);
		
		return SET_CHANGE;
	}
	return SET_NOCHANGE;
}

int CToDoCtrlData::OffsetTaskDate(DWORD dwTaskID, TDC_DATE nDate, int nAmount, int nUnits, BOOL bAndSubtasks, BOOL bForceWeekday)
{
	DH_UNITS nDHUnits = (DH_UNITS)nUnits;
	ASSERT (nDHUnits == DHU_DAYS || nDHUnits == DHU_WEEKS || nDHUnits == DHU_MONTHS);
	
	COleDateTime date = GetTaskDate(dwTaskID, nDate);
	CDateHelper::OffsetDate(date, nAmount, nDHUnits, bForceWeekday);
	
	int nRes = SET_NOCHANGE;
	
	if (SetTaskDate(dwTaskID, nDate, date) == SET_CHANGE)
		nRes = SET_CHANGE;
	
	// children
	if (bAndSubtasks)
	{
		TODOSTRUCTURE* pTDS = m_struct.FindTask(dwTaskID);
		ASSERT(pTDS);

		if (pTDS)
		{
			for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
			{
				DWORD dwChildID = pTDS->GetSubTaskID(nSubTask);
				
				if (OffsetTaskDate(dwChildID, nDate, nAmount, nUnits, TRUE, FALSE) == SET_CHANGE)
					nRes = SET_CHANGE;
			}
		}
	}
	
	return nRes;
}

int CToDoCtrlData::SetTaskPercent(DWORD dwTaskID, int nPercent)
{
	if (nPercent < 0 || nPercent > 100)
		return SET_FAILED;
	
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	return EditTaskAttribute(dwTaskID, pTDI, TDCA_PERCENT, pTDI->nPercentDone, nPercent);
}

int CToDoCtrlData::SetTaskCost(DWORD dwTaskID, double dCost)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	return EditTaskAttribute(dwTaskID, pTDI, TDCA_COST, pTDI->dCost, dCost);
}

int CToDoCtrlData::SetTaskTimeEstimate(DWORD dwTaskID, double dTime, TCHAR nUnits)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_TIMEEST, pTDI->dTimeEstimate, dTime, pTDI->nTimeEstUnits, nUnits);
}

int CToDoCtrlData::SetTaskTimeSpent(DWORD dwTaskID, double dTime, TCHAR nUnits)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_TIMESPENT, pTDI->dTimeSpent, dTime, pTDI->nTimeSpentUnits, nUnits);
}

int CToDoCtrlData::ResetTaskTimeSpent(DWORD dwTaskID, BOOL bAndChildren)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	int nChange = EditTaskAttribute(dwTaskID, pTDI, TDCA_TIMESPENT, pTDI->dTimeSpent, 0.0);

	// children?
	if (bAndChildren)
	{
		const TODOSTRUCTURE* pTDS = m_struct.FindTask(dwTaskID);
		
		for (int nSubtask = 0; nSubtask < pTDS->GetSubTaskCount(); nSubtask++)
		{
			DWORD dwSubtaskID = pTDS->GetSubTaskID(nSubtask);
			
			if (ResetTaskTimeSpent(dwSubtaskID, TRUE) == SET_CHANGE)
				nChange = SET_CHANGE;
		}
	}
	
	return nChange;
}

int CToDoCtrlData::SetTaskAllocTo(DWORD dwTaskID, const CStringArray& aAllocTo)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_ALLOCTO, pTDI->aAllocTo, aAllocTo);
}

int CToDoCtrlData::SetTaskAllocBy(DWORD dwTaskID, const CString& sAllocBy)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	return EditTaskAttribute(dwTaskID, pTDI, TDCA_ALLOCBY, pTDI->sAllocBy, sAllocBy);
}

int CToDoCtrlData::SetTaskVersion(DWORD dwTaskID, const CString& sVersion)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	return EditTaskAttribute(dwTaskID, pTDI, TDCA_VERSION, pTDI->sVersion, sVersion);
}

int CToDoCtrlData::SetTaskStatus(DWORD dwTaskID, const CString& sStatus)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_STATUS, pTDI->sStatus, sStatus);
}

int CToDoCtrlData::SetTaskCategories(DWORD dwTaskID, const CStringArray& aCategories)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_CATEGORY, pTDI->aCategories, aCategories);
}

int CToDoCtrlData::SetTaskTags(DWORD dwTaskID, const CStringArray& aTags)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_TAGS, pTDI->aTags, aTags);
}

int CToDoCtrlData::SetTaskDependencies(DWORD dwTaskID, const CStringArray& aDepends)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);

	int nRes = EditTaskAttribute(dwTaskID, pTDI, TDCA_DEPENDENCY, pTDI->aDependencies, aDepends);

	if (nRes == SET_CHANGE)
	{
		// make sure our start date matches our dependents due date
		if (HasStyle(TDCS_AUTOADJUSTDEPENDENCYDATES))
		{
			for (int nDepends = 0; nDepends < aDepends.GetSize(); nDepends++)
			{
				DWORD dwIDDepends = _ttoi(aDepends[nDepends]);
				
				const TODOITEM* pTDIDepends = GetTask(dwIDDepends);
				ASSERT (pTDIDepends);
				
				if (pTDIDepends && (pTDIDepends->IsDone() || pTDIDepends->HasDue()))
				{
					TDC_DATE nDate = pTDIDepends->IsDone() ? TDCD_DONE : TDCD_DUE;
					UINT nAdjusted = AdjustTaskDates(dwTaskID, dwIDDepends, nDate);
					
					// and then fix up our dependents
					if (Misc::HasFlag(nAdjusted, ADJUSTED_DUE))
						FixupTaskDependencyDates(dwTaskID, TDCD_DUE);
				}
			}
		}
	}

	return nRes;
}

int CToDoCtrlData::SetTaskExtID(DWORD dwTaskID, const CString& sID)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_EXTERNALID, pTDI->sExternalID, sID);
}

BOOL CToDoCtrlData::SaveEditUndo(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib)
{
	if (m_undo.IsActive())										
		return m_undo.SaveElement(TDCUEO_EDIT, dwTaskID, 0, 0, (WORD)nAttrib, pTDI);

	// else
	return FALSE;
}

int CToDoCtrlData::EditTaskAttribute(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, CString& sValue, const CString& sNewValue, BOOL bCaseSensitive)
{
	ASSERT(dwTaskID);
	ASSERT(pTDI);
	ASSERT(nAttrib != TDCA_NONE);

	// test for actual change
	if (bCaseSensitive)
	{
		if (sValue == sNewValue)
			return SET_NOCHANGE;
	}
	else 
	{
		if (Misc::NaturalCompare(sValue, sNewValue) == 0)
			return SET_NOCHANGE;
	}

	// save undo data
	SaveEditUndo(dwTaskID, pTDI, nAttrib);

	// make the change
	sValue = sNewValue;
	pTDI->SetModified();
				
	// update calculations
	ResetCachedCalculations(dwTaskID, pTDI, nAttrib);
	return SET_CHANGE;
}

int CToDoCtrlData::EditTaskAttribute(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, CStringArray& aValues, const CStringArray& aNewValues)
{
	ASSERT(dwTaskID);
	ASSERT(pTDI);
	ASSERT(nAttrib != TDCA_NONE);
	
	// test for actual change
	if (Misc::MatchAll(aValues, aNewValues))
		return SET_NOCHANGE;
	
	// save undo data
	SaveEditUndo(dwTaskID, pTDI, nAttrib);
	
	// make the change
	aValues.Copy(aNewValues);
	pTDI->SetModified();
				
	// update calculations
	ResetCachedCalculations(dwTaskID, pTDI, nAttrib);
	return SET_CHANGE;
}

int CToDoCtrlData::EditTaskAttribute(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, int& nValue, int nNewValue)
{
	ASSERT(dwTaskID);
	ASSERT(pTDI);
	ASSERT(nAttrib != TDCA_NONE);
	
	// test for actual change
	if (nValue == nNewValue)
		return SET_NOCHANGE;
	
	// save undo data
	SaveEditUndo(dwTaskID, pTDI, nAttrib);
	
	// make the change
	nValue = nNewValue;
	pTDI->SetModified();
				
	// update calculations
	ResetCachedCalculations(dwTaskID, pTDI, nAttrib);
	return SET_CHANGE;
}

int CToDoCtrlData::EditTaskAttribute(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, double& dValue, double dNewValue)
{
	ASSERT(dwTaskID);
	ASSERT(pTDI);
	ASSERT(nAttrib != TDCA_NONE);
	
	// test for actual change
	if (dValue == dNewValue)
		return SET_NOCHANGE;
	
	// save undo data
	SaveEditUndo(dwTaskID, pTDI, nAttrib);
	
	// make the change
	dValue = dNewValue;
	pTDI->SetModified();
				
	// update calculations
	ResetCachedCalculations(dwTaskID, pTDI, nAttrib);
	return SET_CHANGE;
}

int CToDoCtrlData::EditTaskAttribute(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, double& dValue, double dNewValue, TCHAR& nUnits, TCHAR nNewUnits)
{
	ASSERT(dwTaskID);
	ASSERT(pTDI);
	ASSERT(nAttrib != TDCA_NONE);
	
	// test for actual change
	if ((dValue == dNewValue) && (nUnits == nNewUnits))
		return SET_NOCHANGE;
	
	// save undo data
	SaveEditUndo(dwTaskID, pTDI, nAttrib);
	
	// make the change
	dValue = dNewValue;
	nUnits = nNewUnits;

	pTDI->SetModified();
				
	// update calculations
	ResetCachedCalculations(dwTaskID, pTDI, nAttrib);
	return SET_CHANGE;

}

int CToDoCtrlData::EditTaskAttribute(DWORD dwTaskID, TODOITEM* pTDI, TDC_ATTRIBUTE nAttrib, COleDateTime& dateValue, const COleDateTime& dateNewValue)
{
	ASSERT(dwTaskID);
	ASSERT(pTDI);
	ASSERT(nAttrib != TDCA_NONE);
	
	// test for actual change
	if (dateValue == dateNewValue)
		return SET_NOCHANGE;
	
	// save undo data
	SaveEditUndo(dwTaskID, pTDI, nAttrib);
	
	// make the change
	dateValue = dateNewValue;
	pTDI->SetModified();
				
	// update calculations
	ResetCachedCalculations(dwTaskID, pTDI, nAttrib);
	return SET_CHANGE;
}

int CToDoCtrlData::SetTaskFileRef(DWORD dwTaskID, const CString& sFilePath)
{
	TODOITEM* pTDI = NULL;
	EDIT_GET_TDI(dwTaskID, pTDI);
	
	return EditTaskAttribute(dwTaskID, pTDI, TDCA_FILEREF, pTDI->sFileRefPath, sFilePath, FALSE);
}

CString CToDoCtrlData::MapTimeUnits(int nUnits)
{
	switch (nUnits)
	{
	case TDITU_MINS:	return "I";
	case TDITU_DAYS:	return "D";
	case TDITU_WEEKS:	return "W";
	case TDITU_MONTHS:	return "M";
	case TDITU_YEARS:	return "Y";
	}
	
	// all else
	return "H";
}

BOOL CToDoCtrlData::BeginNewUndoAction(TDCUNDOACTIONTYPE nType)
{
	return m_undo.BeginNewAction(nType);
}

BOOL CToDoCtrlData::EndCurrentUndoAction()
{
	return m_undo.EndCurrentAction();
}

BOOL CToDoCtrlData::AddUndoElement(TDCUNDOELMOP nOp, DWORD dwTaskID, DWORD dwParentID, DWORD dwPrevSiblingID, WORD wFlags)
{
	if (!m_undo.IsActive())
		return FALSE;
	
	const TODOITEM* pTDI = GetTask(dwTaskID, FALSE);
	ASSERT (pTDI);
	
	if (!pTDI)
		return FALSE;
	
	// save task state
	VERIFY (m_undo.SaveElement(nOp, dwTaskID, dwParentID, dwPrevSiblingID, wFlags, pTDI));
	
	// save children recursively if an add
	if (nOp == TDCUEO_ADD)
	{
		const TODOSTRUCTURE* pTDS = m_struct.FindTask(dwTaskID);
		dwPrevSiblingID = 0; // reuse
		dwParentID = dwTaskID; // reuse
		
		for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
		{
			TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
			ASSERT(pTDSChild);
			
			dwTaskID = pTDSChild->GetTaskID(); // reuse
			VERIFY (AddUndoElement(nOp, dwTaskID, dwParentID, dwPrevSiblingID));
			
			dwPrevSiblingID = dwTaskID;
		}
	}
	
	return TRUE;
}

int CToDoCtrlData::GetLastUndoActionTaskIDs(BOOL bUndo, CDWordArray& aIDs) const 
{
	return bUndo ? m_undo.GetLastUndoActionTaskIDs(aIDs) : m_undo.GetLastRedoActionTaskIDs(aIDs);
}

TDCUNDOACTIONTYPE CToDoCtrlData::GetLastUndoActionType(BOOL bUndo) const
{
	return (bUndo ? m_undo.GetLastUndoType() : m_undo.GetLastRedoType());
}

BOOL CToDoCtrlData::DeleteLastUndoAction()
{
	return m_undo.DeleteLastUndoAction();
}

BOOL CToDoCtrlData::UndoLastAction(BOOL bUndo, CArrayUndoElements& aElms)
{
	aElms.RemoveAll();
	TDCUNDOACTION* pAction = bUndo ? m_undo.UndoLastAction() : m_undo.RedoLastAction();
	
	if (!pAction)
		return FALSE;
	
	// restore elements
	int nNumElm = pAction->aElements.GetSize();
	
	// note: if we are undoing then we need to undo in the reverse order
	// of the initial edits unless it was a move because moves always
	// work off the first item.
	int nStart = 0, nEnd = nNumElm, nInc = 1;
	
	if (bUndo && pAction->nType != TDCUAT_MOVE)
	{
		nStart = nNumElm - 1;
		nEnd = -1;
		nInc = -1;
	}
	
	// copy the structre because we're going to be changing it and we need 
	// to be able to lookup the original previous sibling IDs for undo info
	CToDoCtrlStructure tdsCopy(m_struct);
	
	// now undo
	for (int nElm = nStart; nElm != nEnd; nElm += nInc)
	{
		TDCUNDOELEMENT& elm = pAction->aElements[nElm];
		
		if (elm.nOp == TDCUEO_EDIT)
		{
			TODOITEM* pTDI = NULL;
			GET_TDI(elm.dwTaskID, pTDI, FALSE);
			ASSERT(pTDI);
			
			// copy current task state so we can update redo info
			TODOITEM tdiRedo = *pTDI;
			*pTDI = elm.tdi;
			elm.tdi = tdiRedo;
			
			// no need to return this item nothing to be done
		}
		else if ((elm.nOp == TDCUEO_ADD && bUndo) || (elm.nOp == TDCUEO_DELETE && !bUndo))
		{
			// this is effectively a delete so make the returned elem that way
			TDCUNDOELEMENT elmRet(TDCUEO_DELETE, elm.dwTaskID);
			aElms.Add(elmRet);
			
			DeleteTask(elm.dwTaskID);
		}
		else if ((elm.nOp == TDCUEO_DELETE && bUndo) || (elm.nOp == TDCUEO_ADD && !bUndo))
		{
			// this is effectively an add so make the returned elem that way
			TDCUNDOELEMENT elmRet(TDCUEO_ADD, elm.dwTaskID, elm.dwParentID, elm.dwPrevSiblingID);
			aElms.Add(elmRet);
			
			// restore task
			TODOITEM* pTDI = GetTask(elm.dwTaskID, FALSE);
			ASSERT(pTDI == NULL);
			
			if (!pTDI)
			{
				pTDI = NewTask(&elm.tdi);
				AddTask(elm.dwTaskID, pTDI, elm.dwParentID, elm.dwPrevSiblingID);
			}
		}
		else if (elm.nOp == TDCUEO_MOVE)
		{
			TDCUNDOELEMENT elmRet(TDCUEO_MOVE, elm.dwTaskID, elm.dwParentID, elm.dwPrevSiblingID);
			aElms.Add(elmRet);
			
			MoveTask(elm.dwTaskID, elm.dwParentID, elm.dwPrevSiblingID);
			
			// adjust undo element so these changes can be undone
			elm.dwParentID = tdsCopy.GetParentTaskID(elm.dwTaskID);
			elm.dwPrevSiblingID = tdsCopy.GetPreviousTaskID(elm.dwTaskID);
		}
		else
			return FALSE;
	}
	
	ResetCachedCalculations();
	
	return TRUE;
}

BOOL CToDoCtrlData::CanUndoLastAction(BOOL bUndo) const
{
	return bUndo ? m_undo.CanUndo() : m_undo.CanRedo();
}

BOOL CToDoCtrlData::MoveTask(DWORD dwTaskID, DWORD dwDestParentID, DWORD dwDestPrevSiblingID)
{
	// get source location
	TODOSTRUCTURE* pTDSParent = NULL;
	int nPos = 0;
	
	if (!LocateTask(dwTaskID, pTDSParent, nPos))
	{
		ASSERT(0);
		return FALSE;
	}
	
	DWORD dwPrevSiblingID = pTDSParent->GetPreviousSubTaskID(nPos);
	
	// get destination
	TODOSTRUCTURE* pTDSDestParent = NULL;
	int nDestPos = -1;
	
	if (!Locate(dwDestParentID, dwDestPrevSiblingID, pTDSDestParent, nDestPos))
	{
		ASSERT(0);
		return FALSE;
	}
	// we want the location after the dest previous sibling
	else
		nDestPos++;
	
	if (MoveTask(pTDSParent, nPos, dwPrevSiblingID, pTDSDestParent, nDestPos) != -1)
	{
		ResetCachedCalculations();
		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CToDoCtrlData::MoveTasks(const CDWordArray& aTaskIDs, DWORD dwDestParentID, DWORD dwDestPrevSiblingID)
{
	if (aTaskIDs.GetSize() == 0) 
		return FALSE;
	
	else if (aTaskIDs.GetSize() == 1)
		return MoveTask(aTaskIDs[0], dwDestParentID, dwDestPrevSiblingID);
	
	// copy the structure because we're going to be changing it and we need 
	// to be able to lookup the original previous sibling IDs for undo info
	CToDoCtrlStructure tdsCopy(m_struct);
	
	// get destination location
	TODOSTRUCTURE* pTDSDestParent = NULL;
	int nDestPos = -1;
	
	if (!Locate(dwDestParentID, dwDestPrevSiblingID, pTDSDestParent, nDestPos))
	{
		ASSERT(0);
		return FALSE;
	}
	else // we want the location after the dest previous sibling
	{
		nDestPos++;
	}
	
	// move source tasks 
	for (int nTask = 0; nTask < aTaskIDs.GetSize(); nTask++, nDestPos++)
	{
		TODOSTRUCTURE* pTDSSrcParent = NULL;
		int nSrcPos = -1;

		DWORD dwTaskID = aTaskIDs[nTask];

		if (!LocateTask(dwTaskID, pTDSSrcParent, nSrcPos))
		{
			ASSERT(0);
			return FALSE;
		}
		
		// get previous subtask ID
		// for this we must use our copy of the task structure because
		// the original task structure may already have been altered
		TODOSTRUCTURE* pTDSDummy = NULL;
		int nDummyPos = -1;
		tdsCopy.FindTask(dwTaskID, pTDSDummy, nDummyPos);
		
		DWORD dwPrevSiblingID = pTDSDummy->GetPreviousSubTaskID(nDummyPos);
		nDestPos = MoveTask(pTDSSrcParent, nSrcPos, dwPrevSiblingID, pTDSDestParent, nDestPos);
	}

	ResetCachedCalculations();
	
	return TRUE;
}

int CToDoCtrlData::MoveTask(TODOSTRUCTURE* pTDSSrcParent, int nSrcPos, DWORD dwSrcPrevSiblingID,
							TODOSTRUCTURE* pTDSDestParent, int nDestPos)
{
	DWORD dwTaskID = pTDSSrcParent->GetSubTaskID(nSrcPos);
	DWORD dwParentID = pTDSSrcParent->GetTaskID();
	
	// save undo info
	AddUndoElement(TDCUEO_MOVE, dwTaskID, dwParentID, dwSrcPrevSiblingID);
	
	return pTDSSrcParent->MoveSubTask(nSrcPos, pTDSDestParent, nDestPos);
}

BOOL CToDoCtrlData::TaskHasIncompleteSubtasks(DWORD dwTaskID, BOOL bExcludeRecurring) const
{
	const TODOSTRUCTURE* pTDS = LocateTask(dwTaskID);

	if (pTDS)
		return TaskHasIncompleteSubtasks(pTDS, bExcludeRecurring);

	ASSERT(0);
	return FALSE;
}

BOOL CToDoCtrlData::TaskHasIncompleteSubtasks(const TODOSTRUCTURE* pTDS, BOOL bExcludeRecurring) const
{
	// process its subtasks
	int nPos = pTDS->GetSubTaskCount();

	while (nPos--)
	{
		const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nPos);
		const TODOITEM* pTDIChild = GetTask(pTDSChild);
		
		// ignore recurring tasks
		if (bExcludeRecurring && pTDIChild->IsRecurring())
			continue;

		// ignore completed tasks and their children
		if (pTDIChild->IsDone())
			continue;

		// test for leaf-tasks or parents that do not depend
		// on their children's completed state
		if (!pTDSChild->HasSubTasks() || !HasStyle(TDCS_TREATSUBCOMPLETEDASDONE))
			return TRUE;
		
		// test its subtasks
		if (TaskHasIncompleteSubtasks(pTDSChild, bExcludeRecurring)) // RECURSIVE call
			return TRUE;
	}

	return FALSE;
}

BOOL CToDoCtrlData::IsParentTaskDone(DWORD dwTaskID) const
{
	const TODOSTRUCTURE* pTDS = LocateTask(dwTaskID);
	return IsParentTaskDone(pTDS);
}

BOOL CToDoCtrlData::IsParentTaskDone(const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS);
	
	if (!pTDS || pTDS->ParentIsRoot())
		return FALSE;
	
	const TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();
	const TODOITEM* pTDIParent = GetTask(pTDSParent);
	
	ASSERT (pTDIParent && pTDSParent);
	
	if (!pTDIParent || !pTDSParent)
		return FALSE;
	
	if (pTDIParent->IsDone())
		return TRUE;
	
	// else check parent's parent
	return IsParentTaskDone(pTDSParent);
}

BOOL CToDoCtrlData::GetSubtaskTotals(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS,
									 int& nSubtasksCount, int& nSubtasksDone) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDS->HasSubTasks() || !pTDI)
		return FALSE;
	
	// do we need to recalc?
	if (pTDI->AttribNeedsRecalc(TDIR_SUBTASKSCOUNT) || 
		pTDI->AttribNeedsRecalc(TDIR_SUBTASKSDONE))
	{
		nSubtasksDone = nSubtasksCount = 0;
		
		for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
		{
			nSubtasksCount++;
			
			const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
			const TODOITEM* pTDIChild = GetTask(pTDSChild);
			
			if (IsTaskDone(pTDIChild, pTDSChild, TDCCHECKCHILDREN))
				nSubtasksDone++;
		}
		
		pTDI->nSubtasksDone = nSubtasksDone;
		pTDI->SetAttribNeedsRecalc(TDIR_SUBTASKSDONE, FALSE);
		
		pTDI->nSubtasksCount = nSubtasksCount;
		pTDI->SetAttribNeedsRecalc(TDIR_SUBTASKSCOUNT, FALSE);
	}
	else // use cached values
	{
		nSubtasksDone = pTDI->nSubtasksDone;
		nSubtasksCount = pTDI->nSubtasksCount;
	}
	
	return TRUE;
}

CString CToDoCtrlData::GetTaskPosition(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return _T("");
	
	if (pTDI->AttribNeedsRecalc(TDIR_POSITION))
	{
		const TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();
		pTDI->sPosition.Empty();
		
		if (pTDSParent->IsRoot())
		{
			pTDI->sPosition.Format(_T("%d"), pTDS->GetPosition() + 1);
		}
		else
		{
			TODOITEM* pTDIParent = GetTask(pTDSParent);
			ASSERT (pTDIParent);
				
			if (!pTDIParent)
				return _T("");
				
			CString sParentPos = GetTaskPosition(pTDIParent, pTDSParent);
			pTDI->sPosition.Format(_T("%s.%d"), sParentPos, pTDS->GetPosition() + 1);
		}
		
		pTDI->SetAttribNeedsRecalc(TDIR_POSITION, FALSE);
	}

	return pTDI->sPosition;
}

CString CToDoCtrlData::GetTaskPath(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return _T("");
	
	if (pTDI->AttribNeedsRecalc(TDIR_PATH))
	{
		const TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();
		pTDI->sPath.Empty();
		
		while (pTDSParent && !pTDSParent->IsRoot())
		{
			TODOITEM* pTDIParent = GetTask(pTDSParent);
			ASSERT (pTDIParent);
			
			if (!pTDIParent)
				return _T("");
			
			pTDI->sPath = pTDIParent->sTitle + "\\"+ pTDI->sPath;
			
			pTDSParent = pTDSParent->GetParentTask();
		}
		
		pTDI->SetAttribNeedsRecalc(TDIR_PATH, FALSE);
	}

	return pTDI->sPath;
}

int CToDoCtrlData::CalcPercentDone(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0;
	
	// do we need to recalc?
	if (pTDI->AttribNeedsRecalc(TDIR_PERCENT))
	{
		int nPercent = 0;
		
		if (!HasStyle(TDCS_AVERAGEPERCENTSUBCOMPLETION) || !pTDS->HasSubTasks())
		{
			if (pTDI->IsDone())
			{
				nPercent = 100;
			}
			else if(HasStyle(TDCS_AUTOCALCPERCENTDONE))
			{
				nPercent = CalcPercentFromTime(pTDI, pTDS);
			}
			else
			{
				nPercent = pTDI->nPercentDone;
			}
		}
		else if (HasStyle(TDCS_AVERAGEPERCENTSUBCOMPLETION)) // has subtasks and we must average their completion
		{
			// note: we have separate functions for weighted/unweighted
			// just to keep the logic for each as clear as possible
			if (HasStyle(TDCS_WEIGHTPERCENTCALCBYNUMSUB))
			{
				nPercent = (int)Misc::Round(SumWeightedPercentDone(pTDI, pTDS));
			}
			else
			{
				nPercent = (int)Misc::Round(SumPercentDone(pTDI, pTDS));
			}
		}
		
		// update calc'ed value
		pTDI->nCalcPercent = nPercent;
		pTDI->SetAttribNeedsRecalc(TDIR_PERCENT, FALSE);
	}
	
	return pTDI->nCalcPercent;
}

int CToDoCtrlData::CalcPercentFromTime(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0;
	
	ASSERT (HasStyle(TDCS_AUTOCALCPERCENTDONE)); // sanity check
	
	double dSpent = CalcTimeSpent(pTDI, pTDS, THU_HOURS);
	double dEstimate = CalcTimeEstimate(pTDI, pTDS, THU_HOURS, FALSE); // FALSE == unadjusted by %
	
	if (dSpent > 0 && dEstimate > 0)
		return min(100, (int)(100 * dSpent / dEstimate));
	else
		return 0;
}

double CToDoCtrlData::SumPercentDone(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0;
	
	ASSERT (HasStyle(TDCS_AVERAGEPERCENTSUBCOMPLETION)); // sanity check
	
	if (!pTDS->HasSubTasks() || pTDI->IsDone())
	{
		// base percent
		if (pTDI->IsDone())
			return 100;
		
		else if(HasStyle(TDCS_AUTOCALCPERCENTDONE))
			return CalcPercentFromTime(pTDI, pTDS);
		
		else
			return pTDI->nPercentDone;
	}

	// else aggregate child percentages
	// optionally ignoring completed tasks
	BOOL bIncludeDone = HasStyle(TDCS_INCLUDEDONEINAVERAGECALC);
	int nNumSubtasks = 0, nNumDoneSubtasks = 0;

	if (bIncludeDone)
	{
		nNumSubtasks = pTDS->GetSubTaskCount();
		ASSERT(nNumSubtasks);
	}
	else
	{
		GetSubtaskTotals(pTDI, pTDS, nNumSubtasks, nNumDoneSubtasks);

		if (nNumDoneSubtasks == nNumSubtasks) // all subtasks are completed
			return 0;
	}
	
	// Get default done value for each child (ex.4 child = 25, 3 child = 33.33, etc.)
	double dSplitDoneValue = (1.0 / (nNumSubtasks - nNumDoneSubtasks)); 
	double dTotalPercentDone = 0;
	
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		ASSERT(pTDSChild);
		
		const TODOITEM* pTDIChild = GetTask(pTDSChild); // I assume this is for acquiring the child
		ASSERT(pTDIChild);

		if (pTDSChild && pTDIChild)
		{
			if (HasStyle(TDCS_INCLUDEDONEINAVERAGECALC) || !IsTaskDone(pTDIChild, pTDSChild, TDCCHECKALL))
			{
				//add percent per child(ex.2 child = 50 each if 1st child has 75% completed then will add 50*75/100 = 37.5)
				dTotalPercentDone += dSplitDoneValue * SumPercentDone(pTDIChild, pTDSChild);
			}
		}
	}

	return dTotalPercentDone;
}

int CToDoCtrlData::GetTaskLeafCount(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bIncludeDone) const
{
	ASSERT(pTDS && pTDI);

	if (!pTDS || !pTDI)
		return 0;

	if (bIncludeDone)
		return pTDS->GetLeafCount();

	else if (pTDI->IsDone())
		return 0;

	// else traverse sub items
	int nLeafCount = 0;

	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		const TODOITEM* pTDIChild = GetTask(pTDSChild); 

		nLeafCount += GetTaskLeafCount(pTDIChild, pTDSChild, bIncludeDone);
	}

	return nLeafCount;
}

double CToDoCtrlData::SumWeightedPercentDone(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0;
	
	ASSERT (HasStyle(TDCS_AVERAGEPERCENTSUBCOMPLETION)); // sanity check
	ASSERT (HasStyle(TDCS_WEIGHTPERCENTCALCBYNUMSUB)); // sanity check
	
	if (!pTDS->HasSubTasks() || pTDI->IsDone())
	{
		if (pTDI->IsDone())
			return 100;

		else if(HasStyle(TDCS_AUTOCALCPERCENTDONE))
			return CalcPercentFromTime(pTDI, pTDS);
		
		else
			return pTDI->nPercentDone;
	}

	// calculate the total number of task leaves for this task
	// we will proportion our children percentages against these values
	int nTotalNumSubtasks = GetTaskLeafCount(pTDI, pTDS, HasStyle(TDCS_INCLUDEDONEINAVERAGECALC));
	double dTotalPercentDone = 0;

	// process the children multiplying the split percent by the 
	// proportion of this subtask's subtasks to the whole
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		const TODOITEM* pTDIChild = GetTask(pTDSChild);

		int nChildNumSubtasks = GetTaskLeafCount(pTDIChild, pTDSChild, HasStyle(TDCS_INCLUDEDONEINAVERAGECALC));

		if (HasStyle(TDCS_INCLUDEDONEINAVERAGECALC) || !IsTaskDone(pTDIChild, pTDSChild, TDCCHECKCHILDREN))
		{
			double dChildPercent = SumWeightedPercentDone(pTDIChild, pTDSChild);

			dTotalPercentDone += dChildPercent * ((double)nChildNumSubtasks / (double)nTotalNumSubtasks);
		}
	}

	return dTotalPercentDone;
}

double CToDoCtrlData::CalcCost(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0.0;
	
	// do we need to recalc?
	if (pTDI->AttribNeedsRecalc(TDIR_COST))
	{
		double dCost = pTDI->dCost; // own cost
		
		if (pTDS->HasSubTasks())
		{
			for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
			{
				const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
				const TODOITEM* pTDIChild = GetTask(pTDSChild);
														  
				dCost += CalcCost(pTDIChild, pTDSChild);
			}
		}
		
		// update calc'ed value
		pTDI->dCalcCost = dCost;
		pTDI->SetAttribNeedsRecalc(TDIR_COST, FALSE);
	}
	
	return pTDI->dCalcCost;
}

void CToDoCtrlData::SetDefaultCommentsFormat(const CString& format) 
{ 
	m_cfDefault = format; 
}

void CToDoCtrlData::SetDefaultTimeUnits(TCHAR nTimeEstUnits, TCHAR nTimeSpentUnits)
{
	m_nDefTimeEstUnits = nTimeEstUnits;
	m_nDefTimeSpentUnits = nTimeSpentUnits;
}

TCHAR CToDoCtrlData::GetBestCalcTimeEstUnits(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return m_nDefTimeEstUnits;

	TCHAR cUnits = m_nDefTimeEstUnits;

	if (pTDI->dTimeEstimate > 0)
		cUnits = pTDI->nTimeEstUnits;

	else if (pTDS->HasSubTasks())
	{
		DWORD dwID = pTDS->GetSubTaskID(0);

		if (GetTask(dwID, pTDI, pTDS, FALSE))
			cUnits = GetBestCalcTimeEstUnits(pTDI, pTDS); // RECURSIVE CALL
	}

	return cUnits;
}

TCHAR CToDoCtrlData::GetBestCalcTimeSpentUnits(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return m_nDefTimeSpentUnits;

	TCHAR cUnits = m_nDefTimeSpentUnits;

	if (pTDI->dTimeSpent > 0)
		cUnits = pTDI->nTimeSpentUnits;

	else if (pTDS->HasSubTasks())
	{
		DWORD dwID = pTDS->GetSubTaskID(0);

		if (GetTask(dwID, pTDI, pTDS, FALSE))
			cUnits = GetBestCalcTimeSpentUnits(pTDI, pTDS); // RECURSIVE CALL
	}

	return cUnits;
}

// external version
double CToDoCtrlData::CalcTimeEstimate(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, int nUnits) const
{
	return CalcTimeEstimate(pTDI, pTDS, nUnits, HasStyle(TDCS_USEPERCENTDONEINTIMEEST));
}

// internal version
double CToDoCtrlData::CalcTimeEstimate(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, 
										   int nUnits, BOOL bAdjustByPercent) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0.0;
	
	// do we need to recalc?
	double dEstimate = 0;

	if (pTDI->AttribNeedsRecalc(TDIR_TIMEESTIMATE))
	{
		// NOTE: we do NOT adjust by % complete until the very end
		BOOL bIsParent = pTDS->HasSubTasks();
		
		// task's own time
		if (!bIsParent || HasStyle(TDCS_ALLOWPARENTTIMETRACKING))
		{
			dEstimate = CTimeHelper().GetTime(pTDI->dTimeEstimate, pTDI->nTimeEstUnits, THU_HOURS);
		}
		
		if (bIsParent) // children's time
		{
			for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
			{
				const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
				const TODOITEM* pTDIChild = GetTask(pTDSChild);
				
				dEstimate += CalcTimeEstimate(pTDIChild, pTDSChild, THU_HOURS, FALSE); // FALSE == unadjusted by %
			}
		}
		
		// update calc'ed value
		pTDI->dCalcTimeEstimate = dEstimate;
		pTDI->SetAttribNeedsRecalc(TDIR_TIMEESTIMATE, FALSE);
	}
	else
	{
		dEstimate = pTDI->dCalcTimeEstimate;
	}
	
	// if adjusting by % completion we always do that dynamically 
	// ie. we don't store the result
	if (bAdjustByPercent)
	{
		ASSERT(HasStyle(TDCS_USEPERCENTDONEINTIMEEST));

		if (pTDI->IsDone())
		{
			dEstimate = 0;
		}
		else
		{
			int nPercent = CalcPercentDone(pTDI, pTDS);
			dEstimate *= ((100 - nPercent) / 100.0); // estimating time left
		}
	}

	// estimate is in hours (always) so we need to convert it to nUnits
	return CTimeHelper().GetTime(dEstimate, THU_HOURS, nUnits);
}

double CToDoCtrlData::CalcRemainingTime(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, int& nUnits) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0.0;

	double dRemain = 0.0;

	if (HasStyle(TDCS_CALCREMAININGTIMEBYDUEDATE))
	{
		COleDateTime date = CalcDueDate(pTDI, pTDS);
		
		if (CDateHelper::IsDateSet(date)) 
		{
			nUnits = THU_DAYS; // always
			dRemain = TODOITEM::GetRemainingDueTime(date);
		}
	}
	else
	{
		nUnits = pTDI->nTimeEstUnits;
		dRemain = CalcTimeEstimate(pTDI, pTDS, nUnits, FALSE); // FALSE == unadjusted by %

		if (dRemain > 0)
		{
			if (HasStyle(TDCS_CALCREMAININGTIMEBYPERCENT))
			{
				double dPercent = CalcPercentDone(pTDI, pTDS);
				dRemain *= ((100.0 - dPercent) / 100.0);
			}
			else if (HasStyle(TDCS_CALCREMAININGTIMEBYSPENT))
			{
				dRemain -= CalcTimeSpent(pTDI, pTDS, nUnits);
			}
		}
	}

	// else
	return dRemain;
}

double CToDoCtrlData::CalcTimeSpent(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, int nUnits) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0.0;
	
	// do we need to recalc?
	if (pTDI->AttribNeedsRecalc(TDIR_TIMESPENT))
	{
		double dSpent = 0;
		BOOL bIsParent = pTDS->HasSubTasks();
		
		// task's own time
		if (!bIsParent || HasStyle(TDCS_ALLOWPARENTTIMETRACKING))
		{
			dSpent = CTimeHelper().GetTime(pTDI->dTimeSpent, pTDI->nTimeSpentUnits, THU_HOURS);
		}
		
		if (bIsParent) // children's time
		{
			for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
			{
				const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
				const TODOITEM* pTDIChild = GetTask(pTDSChild);
				
				dSpent += CalcTimeSpent(pTDIChild, pTDSChild, THU_HOURS);
			}
		}
		
		// update calc'ed value
		pTDI->dCalcTimeSpent = dSpent;
		pTDI->SetAttribNeedsRecalc(TDIR_TIMESPENT, FALSE);
	}
	
	// convert it back from hours to nUnits
	return CTimeHelper().GetTime(pTDI->dCalcTimeSpent, THU_HOURS, nUnits);
}

BOOL CToDoCtrlData::IsTaskStarted(DWORD dwTaskID, BOOL bToday) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FALSE);

	return IsTaskStarted(pTDI, pTDS, bToday);
}

BOOL CToDoCtrlData::IsTaskStarted(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bToday) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return FALSE;
	
	double dStarted = CalcStartDate(pTDI, pTDS);
	double dNow = COleDateTime::GetCurrentTime();
	
	if (bToday)
	{
		double dToday = floor(dNow); // 12 midnight
		return (dStarted >= dToday && dStarted < dNow);
	}

	// else
	return (dStarted > 0 && dStarted < dNow);
}

BOOL CToDoCtrlData::IsTaskDue(DWORD dwTaskID, BOOL bToday) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FALSE);

	return IsTaskDue(pTDI, pTDS, bToday);
}

BOOL CToDoCtrlData::IsTaskOverDue(DWORD dwTaskID) const
{
	return IsTaskDue(dwTaskID, FALSE);
}

BOOL CToDoCtrlData::IsTaskOverDue(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	// we need to check that it's due BUT not due today
	return IsTaskDue(pTDI, pTDS, FALSE) && !IsTaskDue(pTDI, pTDS, TRUE);
}

BOOL CToDoCtrlData::HasOverdueTasks() const
{
	if (GetTaskCount() == 0)
		return FALSE;
	
	COleDateTime dtToday = CDateHelper::GetDate(DHD_TODAY);
	double dEarliest = GetEarliestDueDate();

	return ((dEarliest > 0.0) && (dEarliest < dtToday));
}

BOOL CToDoCtrlData::HasDueTodayTasks() const
{
	if (GetTaskCount() == 0)
		return FALSE;
	
	// search all tasks for first due today
	return HasDueTodayTasks(GetStructure());
}

BOOL CToDoCtrlData::HasDueTodayTasks(const TODOSTRUCTURE* pTDS) const
{
	ASSERT(pTDS);

	if (!pTDS)
		return FALSE;

	if (pTDS->GetTaskID())
	{
		const TODOITEM* pTDI = NULL;
		DWORD dwTaskID = pTDS->GetTaskID();

		GET_TDI(dwTaskID, pTDI, FALSE);

		if (IsTaskDue(pTDI, pTDS, TRUE))
			return TRUE;
	}

	// subtasks
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		
		if (HasDueTodayTasks(pTDSChild))
			return TRUE;
	}

	// no due todays found
	return FALSE;
}

double CToDoCtrlData::GetEarliestDueDate() const
{
	double dEarliest = DBL_MAX;
	
	// traverse top level items
	for (int nSubTask = 0; nSubTask < m_struct.GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDS = m_struct.GetSubTask(nSubTask);
		const TODOITEM* pTDI = GetTask(pTDS);
		
		double dTaskEarliest = CalcStartDueDate(pTDI, pTDS, TRUE, TRUE, TRUE);
		
		if (dTaskEarliest > 0.0)
			dEarliest = min(dTaskEarliest, dEarliest);
	}
	
	return (dEarliest == DBL_MAX) ? 0.0 : dEarliest;
}

BOOL CToDoCtrlData::GetTask(DWORD& dwTaskID, const TODOITEM*& pTDI, const TODOSTRUCTURE*& pTDS, BOOL bTrue) const
{
	// get item and structure
	pTDI = GetTask(dwTaskID, bTrue);
	pTDS = LocateTask(dwTaskID);

	// we only need to assert if one could be found but not the other
	ASSERT((pTDI && pTDS) || (!pTDI && !pTDS));

	if (!pTDI || !pTDS)
	{
		pTDI = NULL;
		pTDS = NULL;
		return FALSE;
	}
	
	return TRUE;
}

DWORD CToDoCtrlData::GetTrueTaskID(DWORD dwTaskID) const
{
	DWORD dwTaskRefID = GetTaskReferenceID(dwTaskID);
	
	if (dwTaskRefID)
		return dwTaskRefID;
	
	// all else
	return dwTaskID;
}

BOOL CToDoCtrlData::TaskHasReferences(DWORD dwTaskID) const
{
	ASSERT(dwTaskID);

	// if it is already a reference then it cannot have refs itself
	if (dwTaskID == 0 || IsTaskReference(dwTaskID))
		return FALSE;

	return TaskHasReferences(dwTaskID, &m_struct);
}

BOOL CToDoCtrlData::TaskHasReferences(DWORD dwTaskID, const TODOSTRUCTURE* pTDS) const
{
	// test this task
	if (GetTaskReferenceID(pTDS->GetTaskID()) == dwTaskID)
		return TRUE;

	// then its children
	for (int nChild = 0; nChild < pTDS->GetSubTaskCount(); nChild++)
	{
		if (TaskHasReferences(dwTaskID, pTDS->GetSubTask(nChild)))
			return TRUE;
	}

	// else
	return FALSE;
}

int CToDoCtrlData::GetReferencesToTask(DWORD dwTaskID, CDWordArray& aRefIDs) const
{
	ASSERT(dwTaskID);

	aRefIDs.RemoveAll();

	if (dwTaskID)
		return GetReferencesToTask(dwTaskID, &m_struct, aRefIDs);

	// else
	return 0;
}

int CToDoCtrlData::GetReferencesToTask(DWORD dwTaskID, const TODOSTRUCTURE* pTDS, CDWordArray& aRefIDs) const
{
	// test this task
	if (GetTaskReferenceID(pTDS->GetTaskID()) == dwTaskID)
		aRefIDs.Add(pTDS->GetTaskID());

	// then its children
	for (int nChild = 0; nChild < pTDS->GetSubTaskCount(); nChild++)
	{
		GetReferencesToTask(dwTaskID, pTDS->GetSubTask(nChild), aRefIDs);
	}

	return aRefIDs.GetSize();
}

int CToDoCtrlData::GetHighestPriority(DWORD dwTaskID, BOOL bIncludeDue) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FM_NOPRIORITY);
		
	return GetHighestPriority(pTDI, pTDS, bIncludeDue);
}

int CToDoCtrlData::GetHighestRisk(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FM_NORISK);
		
	return GetHighestRisk(pTDI, pTDS);
}

double CToDoCtrlData::CalcDueDate(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcDueDate(pTDI, pTDS);
}

double CToDoCtrlData::CalcStartDate(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcStartDate(pTDI, pTDS);
}

CString CToDoCtrlData::FormatTaskAllocTo(DWORD dwTaskID) const
{
	return FormatTaskAllocTo(GetTask(dwTaskID));
}

CString CToDoCtrlData::FormatTaskCategories(DWORD dwTaskID) const
{
	return FormatTaskCategories(GetTask(dwTaskID));
}

CString CToDoCtrlData::FormatTaskTags(DWORD dwTaskID) const
{
	return FormatTaskTags(GetTask(dwTaskID));
}

double CToDoCtrlData::CalcCost(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcCost(pTDI, pTDS);
}

int CToDoCtrlData::CalcPercentDone(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcPercentDone(pTDI, pTDS);
}

double CToDoCtrlData::CalcTimeEstimate(DWORD dwTaskID, int nUnits) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcTimeEstimate(pTDI, pTDS, nUnits);
}

double CToDoCtrlData::CalcTimeSpent(DWORD dwTaskID, int nUnits) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcTimeSpent(pTDI, pTDS, nUnits);
}

double CToDoCtrlData::CalcRemainingTime(DWORD dwTaskID, int& nUnits) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, 0);
		
	return CalcRemainingTime(pTDI, pTDS, nUnits);
}

BOOL CToDoCtrlData::GetSubtaskTotals(DWORD dwTaskID, int& nSubtasksTotal, int& nSubtasksDone) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FALSE);

	return GetSubtaskTotals(pTDI, pTDS, nSubtasksTotal, nSubtasksDone);
}

BOOL CToDoCtrlData::IsTaskDone(DWORD dwTaskID, DWORD dwExtraCheck) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FALSE);

	BOOL bDone = IsTaskDone(pTDI, pTDS, dwExtraCheck);
				
	// update cached value
	if (dwExtraCheck == TDCCHECKALL)
	{
		pTDI->bGoodAsDone = bDone;
		pTDI->SetAttribNeedsRecalc(TDIR_GOODASDONE, FALSE);
	}
	
	return bDone;
}

BOOL CToDoCtrlData::IsTaskDue(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bToday) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return FALSE;
	
	double dDue = CDateHelper::GetDateOnly(CalcDueDate(pTDI, pTDS));
	
	if (bToday)
	{
		double dToday = CDateHelper::GetDate(DHD_TODAY); // 12 midnight
		return (dDue >= dToday && dDue < (dToday + 1));
	}

	// else
	return (dDue > 0 && dDue < COleDateTime::GetCurrentTime());
}

double CToDoCtrlData::CalcDueDate(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	if (HasStyle(TDCS_USEEARLIESTDUEDATE))
		return CalcStartDueDate(pTDI, pTDS, TRUE, TRUE, TRUE);

	else if (HasStyle(TDCS_USELATESTDUEDATE))
		return CalcStartDueDate(pTDI, pTDS, TRUE, TRUE, FALSE);

	// else
	return CalcStartDueDate(pTDI, pTDS, FALSE, TRUE, FALSE);
}

double CToDoCtrlData::CalcStartDate(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	if (HasStyle(TDCS_USEEARLIESTSTARTDATE))
		return CalcStartDueDate(pTDI, pTDS, TRUE, FALSE, TRUE);

	else if (HasStyle(TDCS_USELATESTSTARTDATE))
		return CalcStartDueDate(pTDI, pTDS, TRUE, FALSE, FALSE);

	// else
	return CalcStartDueDate(pTDI, pTDS, FALSE, FALSE, FALSE);
}

double CToDoCtrlData::CalcStartDueDate(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bCheckChildren, BOOL bDue, BOOL bEarliest) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return 0.0;
	
	// when 'bEarliest' matches HasStyle(TDCS_USEEARLIESTDUEDATE)
	// we can optimise this function by returning our cached values
	// otherwise we're being called from GetEarliestDueDate
	BOOL bFromGetEarliestDueDate = (bDue && bEarliest && bEarliest != HasStyle(TDCS_USEEARLIESTDUEDATE));
	
	if (!bFromGetEarliestDueDate)
	{
		if (bDue && !pTDI->AttribNeedsRecalc(TDIR_CALCULATEDDUE))
		{
			return pTDI->dateCalcDue;
		}
		else if (!bDue && !pTDI->AttribNeedsRecalc(TDIR_CALCULATEDSTART))
		{
			return pTDI->dateCalcStart;
		}
	}
	
	BOOL bDone = IsTaskDone(pTDI, pTDS, TDCCHECKCHILDREN);
	double dBest = 0;
	
	if (bDone)
	{
		// nothing to do
	}
	else if (bCheckChildren && pTDS->HasSubTasks())
	{
		// initialize dBest to Parent's dates
		if (bDue)
			dBest = GetBestDate(dBest, pTDI->dateDue, FALSE); // FALSE = latest
		else
			dBest = GetBestDate(dBest, pTDI->dateStart, FALSE); // FALSE = latest

		// handle pTDI not having dates
		if (dBest == 0.0)
			dBest = (bEarliest ? DBL_MAX : -DBL_MAX);
		
		// check children
		for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
		{
			const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
			const TODOITEM* pTDIChild = GetTask(pTDSChild);

			ASSERT (pTDSChild && pTDIChild);
			
			double dChildDate = CalcStartDueDate(pTDIChild, pTDSChild, TRUE, bDue, bEarliest);
			dBest = GetBestDate(dBest, dChildDate, bEarliest);
		}
		
		if (fabs(dBest) == DBL_MAX) // no children had dates
			dBest = 0;
	}
	else // leaf task
	{
		dBest = (bDue ? pTDI->dateDue : pTDI->dateStart);
	}
	
	// if we're being called from GetEarliestDueDate
	// we quit here to avoid updating the cached values
	if (bFromGetEarliestDueDate)
		return dBest;
	
	if (bDue)
	{
		// finally if no date set then use today
		if (dBest == 0 && !bDone && HasStyle(TDCS_NODUEDATEISDUETODAY))
			dBest = CDateHelper::GetDateOnly(COleDateTime::GetCurrentTime());
		
		pTDI->dateCalcDue = dBest;		
		pTDI->SetAttribNeedsRecalc(TDIR_CALCULATEDDUE, FALSE);
	}
	else // start
	{
		pTDI->dateCalcStart = dBest;
		pTDI->SetAttribNeedsRecalc(TDIR_CALCULATEDSTART, FALSE);
	}
	
	return (bDue ? pTDI->dateCalcDue : pTDI->dateCalcStart);
}

double CToDoCtrlData::GetBestDate(double dBest, double dDate, BOOL bEarliest)
{
	if (dDate == 0.0)
		return dBest;

	// else
	if (bEarliest)
		return min(dDate, dBest);
	
	// else
	return max(dDate, dBest);
}

int CToDoCtrlData::GetHighestPriority(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, BOOL bIncludeDue) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return -1;
	
	// some optimizations
	// try pre-calculated value first
	if (bIncludeDue)
	{
		if (!pTDI->AttribNeedsRecalc(TDIR_PRIORITYINCDUE))
			return pTDI->nCalcPriorityIncDue;
	}
	else if (!pTDI->AttribNeedsRecalc(TDIR_PRIORITY))
		return pTDI->nCalcPriority;
	
	// else need to recalc
	int nHighest = pTDI->nPriority;
	
	if (pTDI->IsDone() && !HasStyle(TDCS_INCLUDEDONEINPRIORITYCALC) && HasStyle(TDCS_DONEHAVELOWESTPRIORITY))
		nHighest = min(nHighest, MIN_TDPRIORITY);
	
	else if (nHighest < MAX_TDPRIORITY)
	{
		if (bIncludeDue && HasStyle(TDCS_DUEHAVEHIGHESTPRIORITY) && IsTaskDue(pTDI, pTDS))
			nHighest = MAX_TDPRIORITY;
		
		else if (HasStyle(TDCS_USEHIGHESTPRIORITY) && pTDS->HasSubTasks())
		{
			// check children
			for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
			{
				const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
				const TODOITEM* pTDIChild = GetTask(pTDSChild);
				ASSERT (pTDSChild && pTDIChild);
				
				if (HasStyle(TDCS_INCLUDEDONEINPRIORITYCALC) || !IsTaskDone(pTDIChild, pTDSChild, TDCCHECKALL))
				{
					int nChildHighest = GetHighestPriority(pTDIChild, pTDSChild, bIncludeDue);
					
					// optimization
					if (nChildHighest == MAX_TDPRIORITY)
					{
						nHighest = MAX_TDPRIORITY;
						break;
					}
					else
						nHighest = max(nChildHighest, nHighest);
				}
			}
		}
	}
	
	// update calc'ed value
	if (bIncludeDue)
	{
		pTDI->nCalcPriorityIncDue = nHighest;
		pTDI->SetAttribNeedsRecalc(TDIR_PRIORITYINCDUE, FALSE);
	}
	else
	{
		pTDI->nCalcPriority = nHighest;
		pTDI->SetAttribNeedsRecalc(TDIR_PRIORITY, FALSE);
	}
	
	return nHighest;
}

int CToDoCtrlData::GetHighestRisk(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDI)
		return -1;
	
	// some optimizations
	// try pre-calculated value first
	if (!pTDI->AttribNeedsRecalc(TDIR_RISK))
		return pTDI->nCalcRisk;
	
	int nHighest = pTDI->nRisk;
	
	if (pTDI->IsDone() && !HasStyle(TDCS_INCLUDEDONEINRISKCALC) && HasStyle(TDCS_DONEHAVELOWESTRISK))
		nHighest = min(nHighest, MIN_TDRISK);
	
	else if (nHighest < MAX_TDRISK)
	{
		if (HasStyle(TDCS_USEHIGHESTRISK) && pTDS->HasSubTasks())
		{
			// check children
			nHighest = max(nHighest, FM_NORISK);//MIN_TDRISK;
			
			for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
			{
				const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
				const TODOITEM* pTDIChild = GetTask(pTDSChild);
				ASSERT (pTDSChild && pTDIChild);
				
				if (HasStyle(TDCS_INCLUDEDONEINRISKCALC) || !IsTaskDone(pTDIChild, pTDSChild, TDCCHECKALL))
				{
					  int nChildHighest = GetHighestRisk(pTDIChild, pTDSChild);
					  
					  // optimization
					  if (nChildHighest == MAX_TDRISK)
					  {
						  nHighest = MAX_TDRISK;
						  break;
					  }
					  else
						  nHighest = max(nChildHighest, nHighest);
				}
			}
		}
	}
	
	// update calc'ed value
	pTDI->nCalcRisk = nHighest;
	pTDI->SetAttribNeedsRecalc(TDIR_RISK, FALSE);
	
	return pTDI->nCalcRisk;
}

CString CToDoCtrlData::FormatTaskAllocTo(const TODOITEM* pTDI) const
{
	ASSERT(pTDI);
	
	if (pTDI)
	{
		if (pTDI->AttribNeedsRecalc(TDIR_ALLOCTO))
		{
			switch (pTDI->aAllocTo.GetSize())
			{
			case 0:
				pTDI->sAllocToList.Empty();
				break;

			case 1:
				pTDI->sAllocToList = pTDI->aAllocTo[0];
				break;

			default:
				pTDI->sAllocToList = Misc::FormatArray(pTDI->aAllocTo);
				break;
			}

			pTDI->SetAttribNeedsRecalc(TDIR_ALLOCTO, FALSE);
		}

		return pTDI->sAllocToList;
	}

	// all else
	return "";
}

CString CToDoCtrlData::FormatTaskCategories(const TODOITEM* pTDI) const
{
	ASSERT(pTDI);
	
	if (pTDI)
	{
		if (pTDI->AttribNeedsRecalc(TDIR_CATEGORY))
		{
			switch (pTDI->aCategories.GetSize())
			{
			case 0:
				pTDI->sCategoryList.Empty();
				break;

			case 1:
				pTDI->sCategoryList = pTDI->aCategories[0];
				break;

			default:
				pTDI->sCategoryList = Misc::FormatArray(pTDI->aCategories);
				break;
			}

			pTDI->SetAttribNeedsRecalc(TDIR_CATEGORY, FALSE);
		}

		return pTDI->sCategoryList;
	}

	// all else
	return "";
}

CString CToDoCtrlData::FormatTaskTags(const TODOITEM* pTDI) const
{
	ASSERT(pTDI);
	
	if (pTDI)
	{
		if (pTDI->AttribNeedsRecalc(TDIR_TAGS))
		{
			switch (pTDI->aTags.GetSize())
			{
			case 0:
				pTDI->sTagList.Empty();
				break;

			case 1:
				pTDI->sTagList = pTDI->aTags[0];
				break;

			default:
				pTDI->sTagList = Misc::FormatArray(pTDI->aTags);
				break;
			}

			pTDI->SetAttribNeedsRecalc(TDIR_TAGS, FALSE);
		}

		return pTDI->sTagList;
	}

	// all else
	return "";
}

BOOL CToDoCtrlData::IsTaskDone(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, DWORD dwExtraCheck) const
{
	ASSERT (pTDS && pTDI);
	
	if (!pTDS || !pTDS->GetTaskID() || !pTDI)
		return FALSE;
	
	if (pTDI->IsDone())
		return TRUE;
	
	// check cached value
	if (dwExtraCheck == TDCCHECKALL && !pTDI->AttribNeedsRecalc(TDIR_GOODASDONE))
		return pTDI->bGoodAsDone;
	
	// check parent first because it's quicker
	BOOL bDone = FALSE;
	
	if (dwExtraCheck & TDCCHECKPARENT)
		bDone = IsParentTaskDone(pTDS);
	
	// else check children for 'good-as-done'
	BOOL bCheckChildren = (!bDone && 
							(dwExtraCheck & TDCCHECKCHILDREN) && 
							HasStyle(TDCS_TREATSUBCOMPLETEDASDONE) &&
							pTDS->HasSubTasks());

	if (bCheckChildren)
		bDone = (FALSE == TaskHasIncompleteSubtasks(pTDS, FALSE));
	
	// update cached value
	if (dwExtraCheck == TDCCHECKALL)
	{
		pTDI->bGoodAsDone = bDone;
		pTDI->SetAttribNeedsRecalc(TDIR_GOODASDONE, FALSE);
	}
	
	// else return as is
	return bDone;
}

BOOL CToDoCtrlData::IsTaskTimeTrackable(DWORD dwTaskID) const
{
	const TODOITEM* pTDI = NULL;
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FALSE);
	
	// not trackable if a container or complete
	if (!HasStyle(TDCS_ALLOWPARENTTIMETRACKING))
		return (!pTDS->HasSubTasks());
	
	return (!pTDI->IsDone());
}

int CToDoCtrlData::CompareTasks(DWORD dwTask1ID, DWORD dwTask2ID, const TDCCUSTOMATTRIBUTEDEFINITION& attribDef, BOOL bAscending) const
{
	const TODOITEM* pTDI1 = NULL, *pTDI2 = NULL;
	const TODOSTRUCTURE* pTDS1 = NULL, *pTDS2 = NULL;

	GET_TDI_TDS(dwTask1ID, pTDI1, pTDS1, 0);
	GET_TDI_TDS(dwTask2ID, pTDI2, pTDS2, 0);

	// handle 'sort done below'
	BOOL bSortDoneBelow = HasStyle(TDCS_SORTDONETASKSATBOTTOM);
		
	if (bSortDoneBelow)
	{
		BOOL bDone1 = IsTaskDone(pTDI1, pTDS1, TDCCHECKALL);
		BOOL bDone2 = IsTaskDone(pTDI2, pTDS2, TDCCHECKALL);
			
		if (bDone1 != bDone2)
			return bDone1 ? 1 : -1;
	}

	// else compare actual data if 
	int nCompare = 0;
	double dVal1, dVal2;

	if (CalcTaskCustomAttributeData(pTDI1, pTDS1, attribDef, dVal1) &&
		CalcTaskCustomAttributeData(pTDI2, pTDS2, attribDef, dVal2))
	{
		nCompare = Compare(dVal1, dVal2);
	}
	else
	{
		nCompare = Compare(pTDI1->GetCustomAttribValue(attribDef.sUniqueID), 
							pTDI2->GetCustomAttribValue(attribDef.sUniqueID));
	}

	return bAscending ? nCompare : -nCompare;
}


int CToDoCtrlData::CompareTasks(DWORD dwTask1ID, DWORD dwTask2ID, TDC_COLUMN nSortBy, BOOL bAscending, BOOL bSortDueTodayHigh) const
{
	ASSERT(dwTask1ID > 0 && dwTask2ID > 0 && dwTask1ID != dwTask2ID);

	if (dwTask1ID <= 0 || dwTask2ID <= 0 || dwTask1ID == dwTask2ID)
		return 0;

	int nCompare = 0;

	// special case: sort by ID can be optimized
	if (nSortBy == TDCC_ID)
	{
		nCompare = (dwTask1ID - dwTask2ID);
	}
	else
	{
		const TODOITEM* pTDI1 = NULL, *pTDI2 = NULL;
		const TODOSTRUCTURE* pTDS1 = NULL, *pTDS2 = NULL;

		GET_TDI_TDS(dwTask1ID, pTDI1, pTDS1, 0);
		GET_TDI_TDS(dwTask2ID, pTDI2, pTDS2, 0);

		// special case: 'unsorted' == (sort by position + ascending)
		if (nSortBy == TDCC_NONE)
		{
			nSortBy = TDCC_POSITION;
			bAscending = TRUE;
		}

		// figure out if either or both tasks are completed
		// but only if the user has specified to sort these differently
		BOOL bHideDone = HasStyle(TDCS_HIDESTARTDUEFORDONETASKS);
		BOOL bSortDoneBelow = HasStyle(TDCS_SORTDONETASKSATBOTTOM);
		
		BOOL bDone1 = IsTaskDone(pTDI1, pTDS1, TDCCHECKALL);
		BOOL bDone2 = IsTaskDone(pTDI2, pTDS2, TDCCHECKALL);

		// can also do a partial optimization
		if (bSortDoneBelow && 
			(nSortBy != TDCC_DONE) && 
			(nSortBy != TDCC_DONEDATE) && 
			(nSortBy != TDCC_POSITION))
		{
			if (bDone1 != bDone2)
				return bDone1 ? 1 : -1;
		}

		// else default attribute
		switch (nSortBy)
		{
		case TDCC_POSITION:
			nCompare = Compare(GetTaskPosition(pTDI1, pTDS1), GetTaskPosition(pTDI2, pTDS2));
			break;
			
		case TDCC_PATH:
			nCompare = Compare(GetTaskPath(pTDI1, pTDS1), GetTaskPath(pTDI2, pTDS2));
			break;
			
		case TDCC_CLIENT:
			nCompare = Compare(pTDI1->sTitle, pTDI2->sTitle);
			break;
			
		case TDCC_DONE:
			nCompare = Compare(bDone1, bDone2);
			break;
			
		case TDCC_FLAG:
			nCompare = Compare(pTDI1->bFlagged, pTDI2->bFlagged);
			break;
			
		case TDCC_RECURRENCE:
			nCompare = Compare(pTDI1->trRecurrence.nRegularity, pTDI2->trRecurrence.nRegularity);
			break;
			
		case TDCC_VERSION:
			nCompare = FileMisc::CompareVersions(pTDI1->sVersion, pTDI2->sVersion);
			break;
			
		case TDCC_CREATIONDATE:
			nCompare = Compare(pTDI1->dateCreated, pTDI2->dateCreated);
			break;
			
		case TDCC_LASTMOD:
			{
				BOOL bHasModify1 = pTDI1->HasLastMod();
				BOOL bHasModify2 = pTDI2->HasLastMod();
				
				if (bHasModify1 != bHasModify2)
					return bHasModify1 ? -1 : 1;
				
				else if (!bHasModify1) //  and !bHasModify2
					return 0;
				
				nCompare = Compare(pTDI1->dateLastMod, pTDI2->dateLastMod);
			}
			break;
			
		case TDCC_DONEDATE:
			{
				COleDateTime date1 = pTDI1->dateDone;
				COleDateTime date2 = pTDI2->dateDone;
				
				// sort tasks 'good as done' between done and not-done
				if (date1 <= 0 && bDone1)
					date1 = 0.1;
				
				if (date2 <= 0 && bDone2)
					date2 = 0.1;
				
				nCompare = Compare(date1, date2);
			}
			break;
			
		case TDCC_DUEDATE:
			{
				COleDateTime date1 = CalcDueDate(pTDI1, pTDS1);
				COleDateTime date2 = CalcDueDate(pTDI2, pTDS2);

				if (bDone1 && !bHideDone)
					date1 = pTDI1->dateDue;
				
				if (bDone2 && !bHideDone)
					date2 = pTDI2->dateDue;
				
				// Sort undated options below others
				BOOL bHasDue1 = CDateHelper::IsDateSet(date1);
				BOOL bHasDue2 = CDateHelper::IsDateSet(date2);
				
				if (bHasDue1 != bHasDue2)
					return bHasDue1 ? -1 : 1;
				
				else if (!bHasDue1 && !bHasDue2)
					return 0;
				
				nCompare = Compare(date1, date2);
			}
			break;
			
		case TDCC_REMAINING:
			{
				COleDateTime date1 = CalcDueDate(pTDI1, pTDS1);
				COleDateTime date2 = CalcDueDate(pTDI2, pTDS2);
								
				// Sort undated options below others
				BOOL bHasDue1 = CDateHelper::IsDateSet(date1);
				BOOL bHasDue2 = CDateHelper::IsDateSet(date2);
				
				if (bHasDue1 != bHasDue2)
					return bHasDue1 ? -1 : 1;
				
				else if (!bHasDue1 && !bHasDue2)
					return 0;
				
				// compare
				if (nSortBy == TDCC_REMAINING)
				{
					// calc remaining time
					COleDateTime dtCur = COleDateTime::GetCurrentTime();
					
					double dRemain1 = date1 - dtCur;
					double dRemain2 = date2 - dtCur;
					
					// compare
					nCompare = Compare(dRemain1, dRemain2);
				}
				else
					nCompare = Compare(date1, date2);
			}
			break;

		case TDCC_STARTDATE:
			{
				COleDateTime date1 = CalcStartDate(pTDI1, pTDS1);
				COleDateTime date2 = CalcStartDate(pTDI2, pTDS2);
								
				if (bDone1 && !bHideDone)
					date1 = pTDI1->dateStart;
				
				if (bDone2 && !bHideDone)
					date2 = pTDI2->dateStart;
				
				// Sort undated options below others
				BOOL bHasStart1 = CDateHelper::IsDateSet(date1);
				BOOL bHasStart2 = CDateHelper::IsDateSet(date2);
				
				if (bHasStart1 != bHasStart2)
				{
					return bHasStart1 ? -1 : 1;
				}
				else if (!bHasStart1 && !bHasStart2)
				{
					return 0;
				}
				
				// compare
				nCompare = Compare(date1, date2);
			}
			break;
			
		case TDCC_PRIORITY:
			{
				// done items have even less than zero priority!
				// and due items have greater than the highest priority
				int nPriority1 = pTDI1->nPriority; // default
				int nPriority2 = pTDI2->nPriority; // default
				
				BOOL bUseHighestPriority = HasStyle(TDCS_USEHIGHESTPRIORITY);
				
				// item1
				if (bDone1)
				{
					nPriority1 = -1;
				}
				else if (IsTaskDue(pTDI1, pTDS1) && 
						HasStyle(TDCS_DUEHAVEHIGHESTPRIORITY) &&
						(bSortDueTodayHigh || !IsTaskDue(pTDI1, pTDS1, TRUE)))
				{
					nPriority1 = pTDI1->nPriority + 11;
				}
				else if (bUseHighestPriority)
				{
					nPriority1 = GetHighestPriority(pTDI1, pTDS1);
				}
				
				// item2
				if (bDone2)
				{
					nPriority2 = -1;
				}
				else if (IsTaskDue(pTDI2, pTDS2) && HasStyle(TDCS_DUEHAVEHIGHESTPRIORITY) && 
						(bSortDueTodayHigh || !IsTaskDue(pTDI2, pTDS2, TRUE)))
				{
					nPriority2 = pTDI2->nPriority + 11;
				}
				else if (bUseHighestPriority)
				{
					nPriority2 = GetHighestPriority(pTDI2, pTDS2);
				}
				
				nCompare = Compare(nPriority1, nPriority2);
			}
			break;
			
		case TDCC_RISK:
			{
				// done items have even less than zero priority!
				// and due items have greater than the highest priority
				int nRisk1 = pTDI1->nRisk; // default
				int nRisk2 = pTDI2->nRisk; // default
				
				BOOL bUseHighestRisk = HasStyle(TDCS_USEHIGHESTRISK);
				
				// item1
				if (bDone1)
				{
					nRisk1 = -1;
				}
				else if (bUseHighestRisk)
				{
					nRisk1 = GetHighestRisk(pTDI1, pTDS1);
				}
				
				// item2
				if (bDone2)
				{
					nRisk2 = -1;
				}
				else if (bUseHighestRisk)
				{
					nRisk2 = GetHighestRisk(pTDI2, pTDS2);
				}
				
				nCompare = Compare(nRisk1, nRisk2);
			}
			break;
			
		case TDCC_COLOR:
			nCompare = Compare((int)pTDI1->color, (int)pTDI2->color);
			break;
			
		case TDCC_ALLOCTO:
			nCompare = Compare(Misc::FormatArray(pTDI1->aAllocTo), 
								Misc::FormatArray(pTDI2->aAllocTo), TRUE);
			break;
			
		case TDCC_ALLOCBY:
			nCompare = Compare(pTDI1->sAllocBy, pTDI2->sAllocBy, TRUE);
			break;
			
		case TDCC_CREATEDBY:
			nCompare = Compare(pTDI1->sCreatedBy, pTDI2->sCreatedBy, TRUE);
			break;
			
		case TDCC_STATUS:
			nCompare = Compare(pTDI1->sStatus, pTDI2->sStatus, TRUE);
			break;
			
		case TDCC_EXTERNALID:
			nCompare = Compare(pTDI1->sExternalID, pTDI2->sExternalID, TRUE);
			break;
			
		case TDCC_CATEGORY:
			nCompare = Compare(Misc::FormatArray(pTDI1->aCategories), 
								Misc::FormatArray(pTDI2->aCategories), TRUE);
			break;
			
		case TDCC_TAGS:
			nCompare = Compare(Misc::FormatArray(pTDI1->aTags), 
								Misc::FormatArray(pTDI2->aTags), TRUE);
			break;
			
		case TDCC_PERCENT:
			{
				int nPercent1 = CalcPercentDone(pTDI1, pTDS1);
				int nPercent2 = CalcPercentDone(pTDI2, pTDS2);
				
				nCompare = Compare(nPercent1, nPercent2);
			}
			break;

		case TDCC_ICON:
			{
				CString sIcon1 = pTDI1->sIcon; 
				CString sIcon2 = pTDI2->sIcon; 

				nCompare = Compare(sIcon1, sIcon2);
			}
			break;

		case TDCC_PARENTID:
			{
				DWORD dwPID1 = pTDS1 ? pTDS1->GetParentTaskID() : 0;
				DWORD dwPID2 = pTDS2 ? pTDS2->GetParentTaskID() : 0;

				nCompare = (dwPID1 - dwPID2);
			}
			break;
			
		case TDCC_COST:
			{
				double dCost1 = CalcCost(pTDI1, pTDS1);
				double dCost2 = CalcCost(pTDI2, pTDS2);
				
				nCompare = Compare(dCost1, dCost2);
			}
			break;
			
		case TDCC_TIMEEST:
			{
				double dTime1 = CalcTimeEstimate(pTDI1, pTDS1, TDITU_HOURS);
				double dTime2 = CalcTimeEstimate(pTDI2, pTDS2, TDITU_HOURS);
				
				nCompare = Compare(dTime1, dTime2);
			}
			break;
			
		case TDCC_TIMESPENT:
			{
				double dTime1 = CalcTimeSpent(pTDI1, pTDS1, TDITU_HOURS);
				double dTime2 = CalcTimeSpent(pTDI2, pTDS2, TDITU_HOURS);
				
				nCompare = Compare(dTime1, dTime2);
			}
			break;
			
		case TDCC_RECENTEDIT:
			{
				BOOL bRecent1 = pTDI1->IsRecentlyEdited();
				BOOL bRecent2 = pTDI2->IsRecentlyEdited();
				
				nCompare = Compare(bRecent1, bRecent2);
			}
			break;

		case TDCC_FILEREF:
			nCompare = Compare(pTDI1->sFileRefPath, pTDI2->sFileRefPath, TRUE);
			break;

		case TDCC_DEPENDENCY:
			{
				int nNumDepends1 = pTDI1->aDependencies.GetSize();
				int nNumDepends2 = pTDI2->aDependencies.GetSize();

				nCompare = Compare(nNumDepends1, nNumDepends2);
			}
			break;
			
		default:
			ASSERT(0);
			return 0;
		}
	}
	
	return bAscending ? nCompare : -nCompare;
}

int CToDoCtrlData::Compare(const COleDateTime& date1, const COleDateTime& date2)
{
	return (date1 < date2) ? -1 : (date1 > date2) ? 1 : 0;
}

int CToDoCtrlData::Compare(const CString& sText1, const CString& sText2, BOOL bCheckEmpty)
{
	if (bCheckEmpty)
	{
		BOOL bEmpty1 = sText1.IsEmpty();
		BOOL bEmpty2 = sText2.IsEmpty();
		
		if (bEmpty1 != bEmpty2)
			return bEmpty1 ? 1 : -1;
	}
	
	return Misc::NaturalCompare(sText1, sText2);
}

int CToDoCtrlData::Compare(int nNum1, int nNum2)
{
    return (nNum1 - nNum2);
}

int CToDoCtrlData::Compare(double dNum1, double dNum2)
{
    return (dNum1 < dNum2) ? -1 : (dNum1 > dNum2) ? 1 : 0;
}

int CToDoCtrlData::FindTasks(const SEARCHPARAMS& params, CResultArray& aResults) const
{
	if (!GetTaskCount())
		return 0;
	
	for (int nSubTask = 0; nSubTask < m_struct.GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDS = m_struct.GetSubTask(nSubTask);
		ASSERT(pTDS);

		const TODOITEM* pTDI = GetTask(pTDS);
		ASSERT(pTDI);
		
		FindTasks(pTDI, pTDS, params, aResults);
	}
	
	return aResults.GetSize();
}

int CToDoCtrlData::FindTasks(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, const SEARCHPARAMS& params, CResultArray& aResults) const
{
	ASSERT(pTDI && pTDS);

	if (!pTDI || !pTDS)
		return 0;

	SEARCHRESULT result;
	int nResults = aResults.GetSize();
	
	// if the item is done and we're ignoring completed tasks 
	// (and by corollary their children) then we can stop right-away
	if (params.bIgnoreDone && IsTaskDone(pTDI, pTDS, TDCCHECKALL))
		return 0;
	
	if (TaskMatches(pTDI, pTDS, params, result))
	{
		aResults.Add(result);
	}
	
	// process children
	for (int nSubTask = 0; nSubTask < pTDS->GetSubTaskCount(); nSubTask++)
	{
		const TODOSTRUCTURE* pTDSChild = pTDS->GetSubTask(nSubTask);
		ASSERT(pTDSChild);

		const TODOITEM* pTDIChild = GetTask(pTDSChild);
		ASSERT(pTDIChild);
		
		FindTasks(pTDIChild, pTDSChild, params, aResults);
	}
	
	return (aResults.GetSize() - nResults);
}

BOOL CToDoCtrlData::TaskMatches(DWORD dwTaskID, const SEARCHPARAMS& params, SEARCHRESULT& result) const
{
	if (!dwTaskID)
		return FALSE;
	
	const TODOITEM* pTDI = NULL; 
	const TODOSTRUCTURE* pTDS = NULL;
	GET_TDI_TDS(dwTaskID, pTDI, pTDS, FALSE);

	return TaskMatches(pTDI, pTDS, params, result);
}

BOOL CToDoCtrlData::TaskMatches(const TODOITEM* pTDI, const TODOSTRUCTURE* pTDS, 
								const SEARCHPARAMS& params, SEARCHRESULT& result) const
{
	ASSERT(pTDI && pTDS);
	
	if (!pTDI || !pTDS)
		return FALSE;
	
	BOOL bIsDone = IsTaskDone(pTDI, pTDS, TDCCHECKALL);
	
	// if the item is done and we're ignoring completed tasks 
	// (and by corollary their children) then we can stop right-away
	if (bIsDone && params.bIgnoreDone/* && !params.bWantAllSubtasks*/)
		return FALSE;
	
	BOOL bMatches = TRUE;
	
	int nNumRules = params.aRules.GetSize();
	
	for (int nRule = 0; nRule < nNumRules && bMatches; nRule++)
	{
		SEARCHRESULT resTask;
		const SEARCHPARAM& sp = params.aRules[nRule];
		BOOL bMatch = TRUE, bLastRule = (nRule == nNumRules - 1);
		
		switch (sp.GetAttribute())
		{
		case TDCA_TASKNAME:
			bMatch = TaskMatches(pTDI->sTitle, sp, resTask);
			break;
			
		case TDCA_TASKNAMEORCOMMENTS:
			bMatch = TaskMatches(pTDI->sTitle, sp, resTask) ||
					TaskCommentsMatch(pTDI, sp, resTask);
			break;
			
		case TDCA_COMMENTS:
			bMatch = TaskCommentsMatch(pTDI, sp, resTask);
			break;
			
		case TDCA_ALLOCTO:
			bMatch = TaskMatches(pTDI->aAllocTo, sp, resTask);
			break;
			
		case TDCA_ALLOCBY:
			bMatch = TaskMatches(pTDI->sAllocBy, sp, resTask, TRUE);
			break;
			
		case TDCA_CREATEDBY:
			bMatch = TaskMatches(pTDI->sCreatedBy, sp, resTask, TRUE);
			break;
			
		case TDCA_STATUS:
			bMatch = TaskMatches(pTDI->sStatus, sp, resTask, TRUE);
			break;
			
		case TDCA_CATEGORY:
			bMatch = TaskMatches(pTDI->aCategories, sp, resTask);
			break;
			
		case TDCA_TAGS:
			bMatch = TaskMatches(pTDI->aTags, sp, resTask);
			break;
			
		case TDCA_EXTERNALID:
			bMatch = TaskMatches(pTDI->sExternalID, sp, resTask, TRUE);
			break;

		case TDCA_RECENTMODIFIED:
			bMatch = pTDI->IsRecentlyEdited();

			if (bMatch)
				resTask.aMatched.Add(FormatResultDate(pTDI->dateLastMod));
			break;
			
		case TDCA_CREATIONDATE:
			bMatch = TaskMatches(pTDI->dateCreated, sp, resTask);
			break;
			
		case TDCA_STARTDATE:
		case TDCA_STARTTIME:
			{
				BOOL bFloor = (sp.GetAttribute() == TDCA_STARTDATE);

				// CalcStartDate will ignore completed tasks so we have
				// to handle that specific situation
				if (bIsDone && !params.bIgnoreDone)
				{
					bMatch = TaskMatches(pTDI->dateStart, sp, resTask, bFloor);
				}
				else
				{
					COleDateTime dtStart = CalcStartDate(pTDI, pTDS);
					bMatch = TaskMatches(dtStart, sp, resTask, bFloor);
				}
			}
			break;
			
		case TDCA_DUEDATE:
		case TDCA_DUETIME:
			{
				BOOL bFloor = (sp.GetAttribute() == TDCA_DUEDATE);

				// CalcDueDate will ignore completed tasks so we have
				// to handle that specific situation
				if (bIsDone && !params.bIgnoreDone)
				{
					bMatch = TaskMatches(pTDI->dateDue, sp, resTask, bFloor);
				}
				else
				{
					COleDateTime dtDue = CalcDueDate(pTDI, pTDS);
					bMatch = TaskMatches(dtDue, sp, resTask, bFloor);
					
					// handle overdue tasks
					if (bMatch && params.bIgnoreOverDue && IsTaskOverDue(pTDI, pTDS))
					{
						bMatch = FALSE;
					}
				}
			}
			break;
			
		case TDCA_DONEDATE:
			// there's a special case here where if the parent
			// is completed then the task is also treated as completed
			if (sp.OperatorIs(FO_SET) && bIsDone)
			{
				bMatch = TRUE;
			}
			else if (sp.OperatorIs(FO_NOT_SET) && bIsDone)
			{
				bMatch = FALSE;
			}
			else
			{
				bMatch = TaskMatches(pTDI->dateDone, sp, resTask);
			}
			break;
			
		case TDCA_LASTMOD:
			bMatch = TaskMatches(pTDI->dateLastMod, sp, resTask);
			break;
			
		case TDCA_PRIORITY:
			{
				int nPriority = GetHighestPriority(pTDI, pTDS);
				bMatch = TaskMatches(nPriority, sp, resTask);
			}
			break;
			
		case TDCA_RISK:
			{
				int nRisk = GetHighestRisk(pTDI, pTDS);
				bMatch = TaskMatches(nRisk, sp, resTask);
			}
			break;
			
		case TDCA_ID:
			bMatch = TaskMatches((int)pTDS->GetTaskID(), sp, resTask);
			break;
			
		case TDCA_PARENTID:
			bMatch = TaskMatches((int)pTDS->GetParentTaskID(), sp, resTask);
			break;
			
		case TDCA_PERCENT:
			{
				int nPercent = CalcPercentDone(pTDI, pTDS);
				bMatch = TaskMatches(nPercent, sp, resTask);
			}
			break;
			
		case TDCA_TIMEEST:
			{
				double dTime = CalcTimeEstimate(pTDI, pTDS, THU_HOURS);
				bMatch = TaskMatches(dTime, sp, resTask);
			}
			break;
			
		case TDCA_TIMESPENT:
			{
				double dTime = CalcTimeSpent(pTDI, pTDS, THU_HOURS);
				bMatch = TaskMatches(dTime, sp, resTask);
			}
			break;
			
		case TDCA_COST:
			{
				double dCost = CalcCost(pTDI, pTDS);
				bMatch = TaskMatches(dCost, sp, resTask);
			}
			break;
			
		case TDCA_FLAG:
			bMatch = (sp.OperatorIs(FO_SET) ? pTDI->bFlagged : !pTDI->bFlagged);
				
			if (bMatch)
			{
				resTask.aMatched.Add(CEnString(sp.OperatorIs(FO_SET) ? IDS_FLAGGED : IDS_UNFLAGGED));
			}
			break;
			
		case TDCA_VERSION:
			bMatch = TaskMatches(pTDI->sVersion, sp, resTask, TRUE);
			break;

		case TDCA_FILEREF:
			bMatch = TaskMatches(pTDI->sFileRefPath, sp, resTask);
			break;
			
		case TDCA_ANYTEXTATTRIBUTE:
			bMatch = (TaskMatches(pTDI->sTitle, sp, resTask) ||
						TaskMatches(pTDI->sComments, sp, resTask) ||
						TaskMatches(pTDI->aAllocTo, sp, resTask) ||
						TaskMatches(pTDI->sAllocBy, sp, resTask, TRUE) ||
						TaskMatches(pTDI->aCategories, sp, resTask) ||
						TaskMatches(pTDI->sStatus, sp, resTask, TRUE) ||
						TaskMatches(pTDI->sVersion, sp, resTask, TRUE) ||
						TaskMatches(pTDI->sExternalID, sp, resTask, TRUE) ||
						TaskMatches(pTDI->sFileRefPath, sp, resTask) ||
						TaskMatches(pTDI->sCreatedBy, sp, resTask, TRUE));
			break;

		default:
			// test for custom attributes
			if (sp.IsCustomAttribute())
			{
				CString sUniqueID = sp.GetCustomAttributeID();
				ASSERT (!sUniqueID.IsEmpty());

				TDCCUSTOMATTRIBUTEDEFINITION attribDef;

				if (CTDCCustomAttributeHelper::GetAttributeDef(sUniqueID, params.aAttribDefs, attribDef))
				{
					double bCalc;

					if (pTDI->GetCustomAttribCalcValue(sUniqueID, bCalc))
					{
						bMatch = TaskMatches(bCalc, sp, resTask);
					}
					else
					{
						TDCCADATA data(pTDI->GetCustomAttribValue(attribDef.sUniqueID));
						bMatch = TaskMatches(data, attribDef.GetAttributeType(), sp, resTask);
					}
				}
				else
					ASSERT (0);
			}
			break;
		}
		
		// save off result
		if (bMatch)
			result.aMatched.Append(resTask.aMatched);
		
		// handle this result
		bMatches &= bMatch;
		
		// are we at the end of this group?
		if ((sp.GetAnd() == FALSE) || bLastRule) // == 'OR' or end of aRules
		{
			// if the group result is a match then we're done because
			// whatever may come after this is 'ORed' and so cannot change 
			// the result
			if (bMatches || bLastRule)
				break;
			
			else // we're not at the end so we reset bMatches and keep going
				bMatches = TRUE;
		}
		// or is there another group ahead of us ?
		else if (!bMatches) 
		{
			int nNext = nRule + 1;
			
			while (nNext < nNumRules)
			{
				const SEARCHPARAM& spNext = params.aRules[nNext];
				bLastRule = (nNext == nNumRules - 1);
				
				if ((spNext.GetAnd() == FALSE) && !bLastRule)
				{
					nRule = nNext; // start of next group
					bMatches = TRUE;
					break;
				}
				
				// keep looking
				nNext++;
			}
		}	
	}

	// check for parent match if user wants all subtasks
	if (!bMatches && params.bWantAllSubtasks)
	{
		const TODOSTRUCTURE* pTDSParent = pTDS->GetParentTask();

		while (pTDSParent)
		{
			const TODOITEM* pTDIParent = GetTask(pTDSParent);

			if (pTDIParent)
			{
				if (TaskMatches(pTDIParent, pTDSParent, params, result))
				{
					bMatches = TRUE;
					break;
				}
				
				// next parent
				pTDSParent = pTDSParent->GetParentTask();
			}
			else
				break;
		}
	}
	
	if (bMatches)
	{
		if (bIsDone)
			result.dwFlags |= RF_DONE;

		if (IsTaskReference(pTDS->GetTaskID()))
			result.dwFlags |= RF_REFERENCE;

		if (pTDS->HasSubTasks())
			result.dwFlags |= RF_PARENT;
		
		result.dwTaskID = pTDS->GetTaskID();
	}
	
	return bMatches;
}

BOOL CToDoCtrlData::TaskCommentsMatch(const TODOITEM* pTDI, const SEARCHPARAM& sp, SEARCHRESULT& result) const
{
	BOOL bMatch = TaskMatches(pTDI->sComments, sp, result);
				
	// handle custom comments for 'SET' and 'NOT SET'
	if (!bMatch)
	{
		if (sp.OperatorIs(FO_SET))
			bMatch = !pTDI->customComments.IsEmpty();
		
		else if (sp.OperatorIs(FO_NOT_SET))
			bMatch = pTDI->customComments.IsEmpty();
	}

	return bMatch;
}

BOOL CToDoCtrlData::TaskMatches(const COleDateTime& date, const SEARCHPARAM& sp, SEARCHRESULT& result, BOOL bFloor) const
{
	double dDate = date, dSearch = sp.ValueAsDate();
	BOOL bMatch = FALSE;

	if (bFloor)
	{
		dDate = CDateHelper::GetDateOnly(dDate);
		dSearch = CDateHelper::GetDateOnly(dSearch);
	}
	
	switch (sp.GetOperator())
	{
	case FO_EQUALS:
		bMatch = (dDate != 0.0) && (dDate == dSearch);
		break;
		
	case FO_NOT_EQUALS:
		bMatch = (dDate != 0.0) && (dDate != dSearch);
		break;
		
	case FO_ON_OR_AFTER:
		bMatch = (dDate != 0.0) && (dDate >= dSearch);
		break;
		
	case FO_AFTER:
		bMatch = (dDate != 0.0) && (dDate > dSearch);
		break;
		
	case FO_ON_OR_BEFORE:
		bMatch = (dDate != 0.0) && (dDate <= dSearch);
		break;
		
	case FO_BEFORE:
		bMatch = (dDate != 0.0) && (dDate < dSearch);
		break;
		
	case FO_SET:
		bMatch = (dDate > 0.0);
		break;
		
	case FO_NOT_SET:
		bMatch = (dDate == 0.0);
		break;
	}
	
	if (bMatch)
		result.aMatched.Add(FormatResultDate(date));
	
	return bMatch;
}

CString CToDoCtrlData::FormatResultDate(const COleDateTime& date) const
{
	CString sDate;

	if (CDateHelper::IsDateSet(date))
	{
		DWORD dwFmt = 0;
		
		if (HasStyle(TDCS_SHOWWEEKDAYINDATES))
			dwFmt |= DHFD_DOW;
		
		if (HasStyle(TDCS_SHOWDATESINISO))
			dwFmt |= DHFD_ISO;
		
		sDate = CDateHelper::FormatDate(date, dwFmt);
	}

	return sDate;
}

BOOL CToDoCtrlData::TaskMatches(const CString& sText, const SEARCHPARAM& sp, SEARCHRESULT& result, BOOL bMatchAsArray) const
{
	// special case: search param may hold multiple delimited items
	if (bMatchAsArray && (!sText.IsEmpty() || !sp.ValueAsString().IsEmpty()))
	{
		CStringArray aText;
		Misc::Split(sText, aText);

		return TaskMatches(aText, sp, result);
	}

	// all else normal text search
	BOOL bMatch = FALSE;
	
	switch (sp.GetOperator())
	{
	case FO_EQUALS:
		bMatch = (Misc::NaturalCompare(sText, sp.ValueAsString()) == 0);
		break;
		
	case FO_NOT_EQUALS:
		bMatch = (Misc::NaturalCompare(sText, sp.ValueAsString()) != 0);
		break;
		
	case FO_INCLUDES:
	case FO_NOT_INCLUDES:
		{
			CStringArray aWords;
			
			if (!Misc::ParseSearchString(sp.ValueAsString(), aWords))
				return FALSE;
			
			// cycle all the words
			for (int nWord = 0; nWord < aWords.GetSize() && !bMatch; nWord++)
			{
				CString sWord = aWords.GetAt(nWord);
				bMatch = Misc::FindWord(sWord, sText, FALSE, FALSE);
			}
			
			// handle !=
			if (sp.OperatorIs(FO_NOT_INCLUDES))
				bMatch = !bMatch;
		}
		break;
		
	case FO_SET:
		bMatch = !sText.IsEmpty();
		break;
		
	case FO_NOT_SET:
		bMatch = sText.IsEmpty();
		break;
	}
	
	if (bMatch)
		result.aMatched.Add(sText);
	
	return bMatch;
}

BOOL CToDoCtrlData::TaskMatches(const CStringArray& aItems, const SEARCHPARAM& sp, SEARCHRESULT& result) const
{
	// special cases
	if (sp.OperatorIs(FO_SET) && aItems.GetSize())
	{
		int nItem = aItems.GetSize();
		
		while (nItem--)
			result.aMatched.Add(aItems[nItem]);
		
		return TRUE;
	}
	else if (sp.OperatorIs(FO_NOT_SET) && !aItems.GetSize())
		return TRUE;
	
	// general case
	BOOL bMatch = FALSE;
	BOOL bMatchAll = (sp.OperatorIs(FO_EQUALS) || sp.OperatorIs(FO_NOT_EQUALS));
	
	CStringArray aSearchItems;
	Misc::Split(sp.ValueAsString(), aSearchItems, TRUE);
	
	if (bMatchAll)
	{
		bMatch = Misc::MatchAll(aItems, aSearchItems);
	}
	else // partial matches okay
	{
		if (aItems.GetSize())
		{
			bMatch = Misc::MatchAny(aSearchItems, aItems, FALSE, FALSE);
		}
		else
		{
			// special case: task has no item and param.aItems
			// contains an empty item
			bMatch = (Misc::Find(aSearchItems, _T("")) != -1);
		}
	}
	
	// handle !=
	if (sp.OperatorIs(FO_NOT_EQUALS) || sp.OperatorIs(FO_NOT_INCLUDES))
		bMatch = !bMatch;
	
	if (bMatch)
		result.aMatched.Add(Misc::FormatArray(aItems));
	
	return bMatch;
}

BOOL CToDoCtrlData::TaskMatches(const TDCCADATA& data, DWORD dwAttribType, const SEARCHPARAM& sp, SEARCHRESULT& result) const
{
	DWORD dwdataType = (dwAttribType & TDCCA_DATAMASK);
	BOOL bIsList = (dwAttribType & TDCCA_LISTMASK);
	BOOL bMatch = FALSE;

	if (bIsList)
	{
		CStringArray aData;
		data.AsArray(aData);
		
		bMatch = TaskMatches(aData, sp, result);
	}
	else
	{
		// Optimisation: Handle not-set by checking if string is empty
		if (sp.OperatorIs(FO_NOT_SET) && data.IsEmpty())
			dwdataType = TDCCA_STRING;
        
		switch (dwdataType)
		{
		case TDCCA_STRING:	
			bMatch = TaskMatches(data.AsString(), sp, result);
			break;
			
		case TDCCA_INTEGER:	
			bMatch = TaskMatches(data.AsInteger(), sp, result);
			break;
			
		case TDCCA_DOUBLE:	
			bMatch = TaskMatches(data.AsDouble(), sp, result);
			break;
			
		case TDCCA_DATE:	
			bMatch = TaskMatches(data.AsDate(), sp, result);
			break;
			
		case TDCCA_BOOL:	
			bMatch = TaskMatches(data.AsBool(), sp, result);
			break;
			
		default:
			ASSERT(0);
			break;
		}
	}

	return bMatch;
}


BOOL CToDoCtrlData::TaskMatches(double dValue, const SEARCHPARAM& sp, SEARCHRESULT& result) const
{
	BOOL bMatch = FALSE;
	BOOL bTime = (sp.AttributeIs(TDCA_TIMEEST) || sp.AttributeIs(TDCA_TIMESPENT));
	double dSearchVal = sp.ValueAsDouble();
	
	if (bTime)
		dSearchVal = CTimeHelper().GetTime(dSearchVal, sp.GetFlags(), THU_HOURS);
	
	switch (sp.GetOperator())
	{
	case FO_EQUALS:
		bMatch = (dValue == dSearchVal);
		break;
		
	case FO_NOT_EQUALS:
		bMatch = (dValue != dSearchVal);
		break;
		
	case FO_GREATER_OR_EQUAL:
		bMatch = (dValue >= dSearchVal);
		break;
		
	case FO_GREATER:
		bMatch = (dValue > dSearchVal);
		break;
		
	case FO_LESS_OR_EQUAL:
		bMatch = (dValue <= dSearchVal);
		break;
		
	case FO_LESS:
		bMatch = (dValue < dSearchVal);
		break;
		
	case FO_SET:
		bMatch = (dValue > 0.0);
		break;
		
	case FO_NOT_SET:
		bMatch = (dValue == 0.0);
		break;
	}
	
	if (bMatch)
	{
		CString sMatch;
		
		if (bTime)
			sMatch.Format(_T("%.3f H"), dValue);
		else
			sMatch.Format(_T("%.3f"), dValue);
		
		result.aMatched.Add(sMatch);
	}
	
	return bMatch;
}

BOOL CToDoCtrlData::TaskMatches(int nValue, const SEARCHPARAM& sp, SEARCHRESULT& result) const
{
	BOOL bMatch = FALSE;
	BOOL bPriorityRisk = (sp.AttributeIs(TDCA_PRIORITY) || sp.AttributeIs(TDCA_RISK));
	int nSearchVal = sp.ValueAsInteger();
	
	switch (sp.GetOperator())
	{
	case FO_EQUALS:
		bMatch = (nValue == nSearchVal);
		break;
		
	case FO_NOT_EQUALS:
		bMatch = (nValue != nSearchVal);
		break;
		
	case FO_GREATER_OR_EQUAL:
		bMatch = (nValue >= nSearchVal);
		break;
		
	case FO_GREATER:
		bMatch = (nValue > nSearchVal);
		break;
		
	case FO_LESS_OR_EQUAL:
		bMatch = (nValue <= nSearchVal);
		break;
		
	case FO_LESS:
		bMatch = (nValue < nSearchVal);
		break;
		
	case FO_SET:
		if (bPriorityRisk)
			bMatch = (nValue != FM_NOPRIORITY);
		else
			bMatch = (nValue > 0);
		break;
		
	case FO_NOT_SET:
		if (bPriorityRisk)
			bMatch = (nValue == FM_NOPRIORITY);
		else
			bMatch = (nValue == 0);
		break;
	}
	
	if (bMatch)
	{
		// we don't add a result for not set priority/risk because
		// this would appear as -2
		if (sp.OperatorIs(FO_NOT_SET) && bPriorityRisk)
		{
			CEnString sMatch(IDS_TDC_NONE);
			result.aMatched.Add(sMatch);
		}
		else
		{
			CString sMatch = Misc::Format(nValue);
			result.aMatched.Add(sMatch);
		}
	}
	
	return bMatch;
}

UINT CToDoCtrlData::AdjustTaskDates(DWORD dwTaskID, DWORD dwDependencyID, TDC_DATE nDate)
{
	UINT nAdjusted = ADJUSTED_NONE;
	TODOITEM* pTDI = GetTask(dwTaskID);
	ASSERT (pTDI);
	
	// if we're already completed then no adjustment is necessary
	if (pTDI && !pTDI->IsDone())
	{
		const TODOITEM* pTDIDepends = GetTask(dwDependencyID);
		ASSERT (pTDIDepends);
		
		if (pTDIDepends)
		{
			COleDateTime dtNewStart(pTDI->dateStart);
			
			switch (nDate)
			{
			case TDCD_DONE:
				if (pTDIDepends->IsDone())
				{
					// start dates match the preceding task's end date 
					dtNewStart = pTDIDepends->dateDone.m_dt + 1;
				}
				break;
				
			case TDCD_DUE:
			case TDCD_DUEDATE:
				if (!pTDIDepends->IsDone() && pTDIDepends->HasDue())
				{	
					// or it's due date if it's not yet finished
					dtNewStart = pTDIDepends->dateDue.m_dt + 1;
				}
				break;
			}
			
			if (dtNewStart != pTDI->dateStart)
			{
				// bump the due date too if present
				if (pTDI->HasDue() && pTDI->HasStart())
				{
					double dLength = max(0, pTDI->dateDue - pTDI->dateStart);
					pTDI->dateDue.m_dt = dtNewStart.m_dt + dLength;
					nAdjusted |= ADJUSTED_DUE;

					ResetCachedCalculations(dwTaskID, pTDI, TDCA_DUEDATE);
				}

				// always set the start date
				pTDI->dateStart = dtNewStart;
				nAdjusted |= ADJUSTED_START;
				
				ResetCachedCalculations(dwTaskID, pTDI, TDCA_STARTDATE);
			}
		}
	}
	
	return nAdjusted;
}

void CToDoCtrlData::FixupTaskDependencyDates(DWORD dwTaskID, TDC_DATE nDate)
{
	if (!HasStyle(TDCS_AUTOADJUSTDEPENDENCYDATES))
		return;
	
	// check for circular dependency before continuing
	if (TaskHasCircularDependencies(dwTaskID))
		return;
	
	// who is dependent on us -> GetTaskDependents
	CDWordArray aDependents;
	
	if (GetTaskDependents(dwTaskID, aDependents))
	{
		for (int nLink = 0; nLink < aDependents.GetSize(); nLink++)
		{
			DWORD dwIDDependent = aDependents[nLink];
			UINT nAdjusted = AdjustTaskDates(dwIDDependent, dwTaskID, nDate);
			
			if (Misc::HasFlag(nAdjusted, ADJUSTED_DUE))
			{
				// then process this task's dependents
				FixupTaskDependencyDates(dwIDDependent, TDCD_DUE);
			}
		}
	}
}

