// TaskListCsvImporter.cpp: implementation of the CTaskListCsvImporter class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TaskListCsvImporter.h"
#include "tdlschemadef.h"
#include "resource.h"
#include "recurringtaskedit.h"
#include "TDLCsvImportExportDlg.h"

#include <locale.h>

#include "..\shared\timehelper.h"
#include "..\shared\enstring.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\Preferences.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LPCTSTR SPACE = _T(" ");
LPCTSTR ENDL = _T("\n");
LPCTSTR ONEDBLQUOTE = _T("\"");
LPCTSTR TWODBLQUOTE = _T("\"\"");

// private structure for sorting
struct CSVSORT
{
	void Set(DWORD id, DWORD parentID, int originalLineIndex)
	{
		dwID = id;
		dwParentID = parentID;
		nOriginalLineIndex = originalLineIndex;
	}

	DWORD dwID;
	DWORD dwParentID;
	int nOriginalLineIndex;
};

////////////////////////////////////////////////////////

CTaskListCsvImporter::CTaskListCsvImporter()
{

}

CTaskListCsvImporter::~CTaskListCsvImporter()
{

}

bool CTaskListCsvImporter::InitConsts(LPCTSTR szSrcFilePath, BOOL bSilent, 
									  IPreferences* pPrefs, LPCTSTR szKey)
{
	// we go straight for the application preferences
	szKey = _T("Preferences");

	if (pPrefs->GetProfileInt(_T("Preferences"), _T("UseSpaceIndents"), TRUE))
		INDENT = CString(' ', pPrefs->GetProfileInt(_T("Preferences"), _T("TextIndent"), 2));
	else
		INDENT = "  "; // don't use tabs - excel strips them off

	CTDLCsvImportExportDlg dialog(szSrcFilePath, pPrefs, szKey);

	if (!bSilent)
	{
		while (true)
		{
			if (dialog.DoModal() != IDOK)
				return false;

			// valid mapping must include title
			if (!dialog.GetColumnMapping(m_aColumnMapping) ||
				(AttributeIndex(TDCA_TASKNAME) == -1))
			{
				AfxMessageBox(IDS_CSV_MUSTMAPTASKTITLE);
				// try again
			}
			else
			{
				break; // all good
			}
		} 
	}
	else
	{
		VERIFY (dialog.GetColumnMapping(m_aColumnMapping));
		ASSERT (AttributeIndex(TDCA_TASKNAME) != -1);
	}

	DELIM = dialog.GetDelimiter();

	return true;
}

bool CTaskListCsvImporter::Import(LPCTSTR szSrcFilePath, ITaskList* pDestTaskFile, BOOL bSilent, IPreferences* pPrefs, LPCTSTR szKey)
{
	if (!InitConsts(szSrcFilePath, bSilent, pPrefs, szKey))
		return false;

	// Load csv
	CStringArray aLines;
	FileMisc::LoadFileLines(szSrcFilePath, aLines);

	// remove header line
	aLines.RemoveAt(0);

	ITaskList12* pTasks = GetITLInterface<ITaskList12>(pDestTaskFile, IID_TASKLIST12);
	BOOL bSomeFailed = FALSE;

	for (int nLine = 0; nLine < aLines.GetSize(); nLine++)
	{
	    CString sLine = GetLine(aLines, nLine);

		if (!ImportTask(pTasks, sLine))
			bSomeFailed = TRUE;
	}

	if (!bSilent && bSomeFailed)
		AfxMessageBox(IDS_IMPORT_SOMEFAILED);

	return true;
}

CString CTaskListCsvImporter::GetLine(const CStringArray& aLines, int& nLine)
{
    CEnString sLine(aLines[nLine]);
	
    // if the line contains an odd number of double-quotes
    // then assume we're in a comment and keep appending lines
    // until we hit the closing double-quote
    if (sLine.GetCharacterCount('\"') % 2)
    {
		while (++nLine < aLines.GetSize())
		{
			CEnString sNextLine(aLines[nLine]);
			sLine += "\n" + sNextLine;
			
			if (sNextLine.GetCharacterCount('\"') % 2)
				break;
		}
    }
	
    return sLine;
}

void CTaskListCsvImporter::GetTaskAndParentIDs(const CStringArray& sValues, DWORD& dwTaskID, DWORD& dwParentID) const
{
	dwTaskID = dwParentID = 0;

	int nCol = AttributeIndex(TDCA_ID);

	if (nCol != -1)
		dwTaskID = _ttoi(sValues[nCol]);

	nCol = AttributeIndex(TDCA_PARENTID);

	if (nCol != -1)
		dwParentID = _ttoi(sValues[nCol]);
}

CString CTaskListCsvImporter::GetTaskTitle(const CStringArray& sValues) const
{
	int nCol = AttributeIndex(TDCA_TASKNAME);

	if (nCol != -1)
		return sValues[nCol];

	// else
	static CString sEmpty;
	return sEmpty;
}

BOOL CTaskListCsvImporter::ImportTask(ITaskList12* pTasks, const CString& sLine) const
{
	// must have at least one field
	CStringArray aValues;
	int nNumVals = Misc::Split(sLine, aValues, TRUE, DELIM);

	ASSERT(nNumVals >= 1);

	if (nNumVals < 1)
		return FALSE; // then we can report this when we've finished importing

	// must have a title
	CString sTitle = GetTaskTitle(aValues);
	ASSERT(!sTitle.IsEmpty());

	if (sTitle.IsEmpty())
		return FALSE;

	// get taskID and ParentID
	DWORD dwTaskID, dwParentID;
	GetTaskAndParentIDs(aValues, dwTaskID, dwParentID);

	// find the parent task
	HTASKITEM hParent = pTasks->FindTask(dwParentID);

	// create task
	HTASKITEM hTask = pTasks->NewTask(sTitle, hParent, dwTaskID);

	AddAttributeToTask(pTasks, hTask, TDCA_CREATEDBY, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_CATEGORY, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_STATUS, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_EXTERNALID, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_ALLOCBY, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_ALLOCTO, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_VERSION, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_FILEREF, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_DEPENDENCY, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_COMMENTS, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_PRIORITY, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_RISK, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_FLAG, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_TIMEEST, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_TIMESPENT, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_COST, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_STARTDATE, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_DUEDATE, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_DONEDATE, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_LASTMOD, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_CREATIONDATE, aValues);
	AddAttributeToTask(pTasks, hTask, TDCA_TAGS, aValues);

	// custom attributes have to be handled differently because
	// multiple unique attributes may exist
	AddCustomAttributesToTask(pTasks, hTask, aValues);

	return TRUE;
}

int CTaskListCsvImporter::AttributeIndex(TDC_ATTRIBUTE attrib) const
{
	int nAttrib = m_aColumnMapping.GetSize();

	while (nAttrib--)
	{
		const CSVCOLUMNMAPPING& col = m_aColumnMapping[nAttrib];

		if (col.nTDCAttrib == attrib && !col.sColumnName.IsEmpty())
			break;
	}

	return nAttrib;
}

int CTaskListCsvImporter::GetDepth(const CString& sLine)
{
	if (INDENT.IsEmpty() || sLine.IsEmpty())
		return 0;

	// else
	int nDepth = 0;
	
	if (INDENT == _T("\t"))
	{
		while (nDepth < sLine.GetLength())
		{
			if (sLine[nDepth] == '\t')
				nDepth++;
			else
				break;
		}
	}
	else // one or more spaces
	{
		int nPos = 0;

		while (nPos < sLine.GetLength())
		{
			if (sLine.Find(INDENT, nPos) == nPos)
				nDepth++;
			else
				break;

			// next
			nPos = nDepth * INDENT.GetLength();
		}
	}

	// set root depth if not set 
	if (ROOTDEPTH == -1)
		ROOTDEPTH = nDepth;

	// and take allowance for it
	nDepth -= ROOTDEPTH;

	return nDepth;
}

BOOL CTaskListCsvImporter::String2Date(const CString& sDate, time64_t& t64, BOOL bAndTime)
{
	return (CDateHelper::DecodeDate(sDate, t64, bAndTime));
}

void CTaskListCsvImporter::AddCustomAttributesToTask(ITaskList12* pTasks, HTASKITEM hTask, const CStringArray& aValues) const
{
	int nAttrib = m_aColumnMapping.GetSize();
	
	while (nAttrib--)
	{
		const CSVCOLUMNMAPPING& col = m_aColumnMapping[nAttrib];
		
		if (col.nTDCAttrib == TDCA_CUSTOMATTRIB && !col.sColumnName.IsEmpty())
		{
			CString sCustID = col.sColumnName;

			// extract custom ID in brackets
			int nCustIDStart = col.sColumnName.Find('(') + 1;

			if (nCustIDStart > 0)
			{
				int nCustIDEnd = col.sColumnName.Find(')', nCustIDStart);
				
				if (nCustIDEnd == -1)
					continue;

				sCustID = sCustID.Mid(nCustIDStart, nCustIDEnd - nCustIDStart);
			}

			pTasks->SetTaskCustomAttributeData(hTask, sCustID, aValues[nAttrib]);
		}
	}
}

void CTaskListCsvImporter::AddAttributeToTask(ITaskList12* pTasks, HTASKITEM hTask, TDC_ATTRIBUTE nAttrib, const CStringArray& aValues) const
{
	ASSERT(nAttrib < TDCA_CUSTOMATTRIB_FIRST);

	int nCol = AttributeIndex(nAttrib);
	
	if (nCol == -1)
		return;

	const CString& sValue = Misc::GetItem(aValues, nCol);
	time64_t t64 = 0;
	
	switch(nAttrib)
	{
	case TDCA_CREATEDBY: 
		pTasks->SetTaskCreatedBy(hTask, sValue);
		break;

	case TDCA_CATEGORY: 
		{
			CStringArray aCats;

			if (Misc::Split(sValue, '+', aCats))
			{
				for (int nCat = 0; nCat < aCats.GetSize(); nCat++)
					pTasks->AddTaskCategory(hTask, aCats[nCat]);
			}
		}
		break;

	case TDCA_TAGS: 
		{
			CStringArray aTags;

			if (Misc::Split(sValue, '+', aTags))
			{
				for (int nCat = 0; nCat < aTags.GetSize(); nCat++)
					pTasks->AddTaskTag(hTask, aTags[nCat]);
			}
		}
		break;

	case TDCA_STATUS: 
		pTasks->SetTaskStatus(hTask, sValue);
		break;

	case TDCA_EXTERNALID: 
		pTasks->SetTaskExternalID(hTask, sValue);
		break;

	case TDCA_ALLOCBY: 
		pTasks->SetTaskAllocatedBy(hTask, sValue);
		break;

	case TDCA_ALLOCTO: 
		{
			CStringArray aAlloc;

			if (Misc::Split(sValue, '+', aAlloc))
			{
				for (int nAlloc = 0; nAlloc < aAlloc.GetSize(); nAlloc++)
					pTasks->AddTaskAllocatedTo(hTask, aAlloc[nAlloc]);
			}
		}
		break;

	case TDCA_VERSION: 
		pTasks->SetTaskVersion(hTask, sValue);
		break;

	case TDCA_FILEREF: 
		pTasks->SetTaskFileReferencePath(hTask, sValue);
		break;

	case TDCA_DEPENDENCY: 
		{
			CStringArray aDepends;

			if (Misc::Split(sValue, '+', aDepends))
			{
				for (int nDep = 0; nDep < aDepends.GetSize(); nDep++)
					pTasks->AddTaskDependency(hTask, aDepends[nDep]);
			}
		}
		break;

	case TDCA_COMMENTS: 
		pTasks->SetTaskComments(hTask, sValue);
		break;

	case TDCA_STARTDATE: 
		if (String2Date(sValue, t64, TRUE))
			pTasks->SetTaskStartDate64(hTask, t64);
		break;

	case TDCA_DUEDATE: 
		if (String2Date(sValue, t64, TRUE))
			pTasks->SetTaskDueDate64(hTask, t64);
		break;

	case TDCA_DONEDATE: 
		if (String2Date(sValue, t64, TRUE))
			pTasks->SetTaskDoneDate64(hTask, t64);
		break;

	case TDCA_LASTMOD: 
		if (String2Date(sValue, t64, TRUE))
			pTasks->SetTaskLastModified64(hTask, t64);
		break;

	case TDCA_CREATIONDATE: 
		if (String2Date(sValue, t64, FALSE))
			pTasks->SetTaskCreationDate64(hTask, t64);
		break;

	case TDCA_PRIORITY: 
		pTasks->SetTaskPriority(hTask, (unsigned char)_ttoi(sValue));
		break;

	case TDCA_RISK: 
		pTasks->SetTaskRisk(hTask, (unsigned char)_ttoi(sValue));
		break;

	case TDCA_FLAG: 
		pTasks->SetTaskFlag(hTask, (_ttoi(sValue) != 0));
		break;

	case TDCA_COST: 
		pTasks->SetTaskCost(hTask, _ttof(sValue));
		break;

	case TDCA_TIMEEST: 
		{
			// try to get units from following column
			TCHAR nUnits = GetTimeUnits(aValues, nCol + 1);

			if (nUnits)
				pTasks->SetTaskTimeEstimate(hTask, _ttof(sValue), nUnits);
		}
		break;

	case TDCA_TIMESPENT: 
		{
			// try to get units from following column
			TCHAR nUnits = GetTimeUnits(aValues, nCol + 1);
			
			if (nUnits)
				pTasks->SetTaskTimeSpent(hTask, _ttof(sValue), nUnits);
		}
		break;
	}
}

TCHAR CTaskListCsvImporter::GetTimeUnits(const CStringArray& aValues, int nCol)
{
	TCHAR nUnits = 0;
	
	// get units from following column
	if (nCol < aValues.GetSize())
	{
		const CString& sUnits = Misc::GetItem(aValues, nCol);
		
		if (sUnits.GetLength() == 1)
		{
			switch (sUnits[0])
			{
			case THU_MINS:
			case THU_HOURS:
			case THU_DAYS:
			case THU_WEEKS:
			case THU_MONTHS:
			case THU_YEARS:
				nUnits = sUnits[0];
				break;
			}
		}
	}
	
	return nUnits;
}
