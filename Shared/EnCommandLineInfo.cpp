// EnCommandLineInfo.cpp: implementation of the CEnCommandLineInfo class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EnCommandLineInfo.h"

#include "misc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEnCommandLineInfo::CEnCommandLineInfo(const CString& sFileExts, TCHAR cDelim)
{
	m_nLastParameter = -1;	
	
	if (!sFileExts.IsEmpty())
	{
		if (cDelim == 0)
			m_aFileExt.Add(sFileExts);
		else
			Misc::Split(sFileExts, cDelim, m_aFileExt, FALSE);
	}
}

CEnCommandLineInfo::~CEnCommandLineInfo()
{

}

void CEnCommandLineInfo::ParseParam(LPCTSTR lpszParam, BOOL bFlag, BOOL /*bLast*/)
{
	CString sLookup;

	if (bFlag) 
	{
		m_sCurFlag = lpszParam; 	   // save in case other value specified
		m_sCurFlag.MakeUpper();

		// this is a "flag" (begins with / or -)
		m_mapCommandLine[m_sCurFlag] = _T(""); // default value is "TRUE"
		m_nLastParameter = -1;		
	} 
	else // must be a parameter
	{
		if (!m_sCurFlag.IsEmpty())
		{
			m_nLastParameter++;

			sLookup.Format(_T("%s_PARAMETER_%d"), m_sCurFlag, m_nLastParameter);
			m_mapCommandLine[sLookup] = lpszParam;
		}

		// set m_strFilename to the first parameter having
		// a (matching) file extension 
		if (m_strFileName.IsEmpty())
		{
			TCHAR szExt[_MAX_EXT] = { 0 };

	#if _MSC_VER >= 1400
			_tsplitpath_s(lpszParam, NULL,_MAX_DRIVE, NULL,_MAX_DIR, NULL, _MAX_FNAME, szExt, _MAX_EXT);
	#else
			_tsplitpath(lpszParam, NULL, NULL, NULL, szExt);
	#endif

			if (szExt[0]) // found something
			{
				if (!m_aFileExt.GetSize() || (Misc::Find(m_aFileExt, szExt, FALSE, FALSE) >= 0))
				{
					m_strFileName = lpszParam;
				}
			}
		}
	}
}

BOOL CEnCommandLineInfo::GetOption(LPCTSTR szFlag, CStringArray& aParams) const
{
	CString sFlag(szFlag), sLookup, sParameter;
	sFlag.MakeUpper();

	if (!m_mapCommandLine.Lookup(sFlag, sParameter))
		return FALSE;

	aParams.RemoveAll();

	int nParam = 0;
	sLookup.Format(_T("%s_PARAMETER_%d"), sFlag, nParam);

	while (m_mapCommandLine.Lookup(sLookup, sParameter))
	{
		aParams.Add(sParameter);

		nParam++;
		sLookup.Format(_T("%s_PARAMETER_%d"), sFlag, nParam);
	}

	return TRUE;
}

BOOL CEnCommandLineInfo::SetOption(LPCTSTR szFlag, LPCTSTR szParam)
{
	CString sFlag(szFlag), sLookup, sParameter;
	sFlag.MakeUpper();

	// option cannot already exist
	if (m_mapCommandLine.Lookup(sFlag, sParameter))
		return FALSE;

	// create flag
	m_mapCommandLine[sFlag] = _T("");

	// set szParam as the one and only option parameter
	sLookup.Format(_T("%s_PARAMETER_0"), sFlag);
	m_mapCommandLine[sLookup] = szParam;

	return TRUE;
}

BOOL CEnCommandLineInfo::SetOption(LPCTSTR szFlag, DWORD dwParam)
{
	CString sParam;
	sParam.Format(_T("%d"), dwParam);

	return SetOption(szFlag, sParam);
}

BOOL CEnCommandLineInfo::GetOption(LPCTSTR szFlag, CString& sParam) const
{
	CStringArray aParams;

	if (GetOption(szFlag, aParams))
	{
		if (aParams.GetSize())
			sParam = aParams[0];

		return TRUE;
	}

	return FALSE;
}

CString CEnCommandLineInfo::GetOption(LPCTSTR szFlag) const
{
	CString sOption;
	GetOption(szFlag, sOption);

	return sOption;
}

BOOL CEnCommandLineInfo::HasOption(LPCTSTR szFlag) const
{
	CString sOption;
	
	return GetOption(szFlag, sOption);
}

void CEnCommandLineInfo::DeleteOption(LPCTSTR szFlag)
{
	CString sFlag(szFlag);
	sFlag.MakeUpper();

	m_mapCommandLine.RemoveKey(sFlag);
}

