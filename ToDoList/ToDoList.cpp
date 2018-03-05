// ToDoList.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "ToDoList.h"
#include "ToDoListWnd.h"
#include "Preferencesdlg.h"
#include "welcomewizard.h"
#include "tdcmsg.h"
#include "tdlprefmigrationdlg.h"
#include "tdllanguagedlg.h"
#include "TDLCmdlineOptionsDlg.h"
#include "tdlolemessagefilter.h"

#include "..\shared\encommandlineinfo.h"
#include "..\shared\driveinfo.h"
#include "..\shared\dialoghelper.h"
#include "..\shared\enfiledialog.h"
#include "..\shared\enstring.h"
#include "..\shared\regkey.h"
#include "..\shared\filemisc.h"
#include "..\shared\autoflag.h"
#include "..\shared\preferences.h"
#include "..\shared\localizer.h"
#include "..\shared\fileregister.h"
#include "..\shared\osversion.h"

#include "..\3rdparty\xmlnodewrapper.h"
#include "..\3rdparty\ini.h"

#include <afxpriv.h>

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

LPCTSTR REGKEY = _T("AbstractSpoon");
LPCTSTR APPREGKEY = _T("Software\\AbstractSpoon\\ToDoList");

LPCTSTR ONLINEHELP = _T("http://abstractspoon.pbwiki.com/"); 
LPCTSTR CONTACTUS = _T("mailto:abstractspoon2@optusnet.com.au"); 
LPCTSTR FEEDBACKANDSUPPORT = _T("http://www.codeproject.com/KB/applications/todolist2.aspx"); 
LPCTSTR LICENSE = _T("http://www.opensource.org/licenses/eclipse-1.0.php"); 
LPCTSTR ONLINE = _T("http://www.abstractspoon.com/tdl_resources.html"); 
LPCTSTR WIKI = _T("http://abstractspoon.pbwiki.com/"); 
LPCTSTR DONATE = _T("https://www.paypal.com/cgi-bin/webscr?cmd=_xclick&business=abstractspoon2%40optusnet%2ecom%2eau&item_name=Software"); 

LPCTSTR FILESTATEKEY = _T("FileStates");
LPCTSTR REMINDERKEY = _T("Reminders");
LPCTSTR DEFAULTKEY = _T("Default");

/////////////////////////////////////////////////////////////////////////////
// CToDoListApp

BEGIN_MESSAGE_MAP(CToDoListApp, CWinApp)
	//{{AFX_MSG_MAP(CToDoListApp)
	ON_COMMAND(ID_HELP_CONTACTUS, OnHelpContactus)
	ON_COMMAND(ID_HELP_FEEDBACKANDSUPPORT, OnHelpFeedbackandsupport)
	ON_COMMAND(ID_HELP_LICENSE, OnHelpLicense)
	ON_COMMAND(ID_HELP_WIKI, OnHelpWiki)
	ON_COMMAND(ID_HELP_COMMANDLINE, OnHelpCommandline)
	ON_COMMAND(ID_HELP_DONATE, OnHelpDonate)
	ON_COMMAND(ID_HELP_UNINSTALL, OnHelpUninstall)
	//}}AFX_MSG_MAP
	ON_COMMAND(ID_HELP, OnHelp)
	ON_COMMAND(ID_TOOLS_IMPORTPREFS, OnImportPrefs)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_IMPORTPREFS, OnUpdateImportPrefs)
	ON_COMMAND(ID_TOOLS_EXPORTPREFS, OnExportPrefs)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_EXPORTPREFS, OnUpdateExportPrefs)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToDoListApp construction

CToDoListApp::CToDoListApp() : CWinApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CToDoListApp object

CToDoListApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CToDoListApp initialization

BOOL CToDoListApp::InitInstance()
{
	CEnCommandLineInfo cmdInfo(_T(".tdl;.xml"));
	ParseCommandLine(cmdInfo);

	// see if the user wants to uninstall
	if (cmdInfo.HasOption(SWITCH_UNINSTALL))
	{
		CString sUninstallFolder = cmdInfo.GetOption(SWITCH_UNINSTALL);

		if (sUninstallFolder.IsEmpty())
			RunUninstaller();
		else
			DoUnistall(sUninstallFolder);

		return FALSE;
	}

	// see if the user just wants to see the commandline options
	if (cmdInfo.HasOption(SWITCH_HELP1) || 
		cmdInfo.HasOption(SWITCH_HELP2) || 
		cmdInfo.HasOption(SWITCH_HELP3))
	{
		OnHelpCommandline();
		return FALSE;
	}

	AfxOleInit(); // for initializing COM and handling drag and drop via explorer
	AfxEnableControlContainer(); // embedding IE

	// before anything else make sure we've got MSXML3 installed
	if (!CXmlDocumentWrapper::IsVersion3orGreater())
	{
		AfxMessageBox(IDS_BADMSXML);
		return FALSE;
	}

	// init prefs 
	if (!InitPreferences(cmdInfo))
		return FALSE;

	// commandline options
	TDCSTARTUP startup(cmdInfo);

	// If we are single instance or this is an inter-tasklist task link, 
	// then we pass on the startup info to whoever can handle it.
	BOOL bSingleInstance = !CPreferencesDlg().GetMultiInstance();

	if (bSingleInstance || startup.HasFlag(TLD_TASKLINK))
	{
		// Get a list of all non-closing TDL instances
		TDCFINDWND find;
		int nNumWnds = FindToDoListWnds(find);

		// pass startup info to first instance
		for (int nWnd = 0; nWnd < nNumWnds; nWnd++)
		{
			HWND hWnd = find.aResults[nWnd];

			// final check for validity
			if (::IsWindow(hWnd))
			{
				COPYDATASTRUCT cds;
				
				cds.dwData = TDL_STARTUP;
				cds.cbData = sizeof(startup);
				cds.lpData = (void*)&startup;

				// note: A window will return FALSE if this is a tasklink
				// and it does not handle it
				if (::SendMessage(hWnd, WM_COPYDATA, NULL, (LPARAM)&cds) != 0)
				{
					::SendMessage(hWnd, WM_TDL_SHOWWINDOW, 0, 0);
					::SetForegroundWindow(hWnd);
								
					return FALSE; // to quit this instance
				}
			}
		}
	}

	// if no one handled it create a new instance
	CToDoListWnd* pTDL = new CToDoListWnd;
	
	if (pTDL && pTDL->Create(startup))
	{
		m_pMainWnd = pTDL;
		return TRUE;
	}

	// else
	return FALSE;
}

BOOL CToDoListApp::ValidateTasklistPath(CString& sPath)
{
	if (FileMisc::HasExtension(sPath, _T("tdl")) ||
		FileMisc::HasExtension(sPath, _T("xml")))
	{
		return ValidateFilePath(sPath);
	}

	// else log it
	FileMisc::LogText(_T("Taskfile '%s' had an invalid extension\n"), sPath);

	return FALSE;
}

BOOL CToDoListApp::ValidateFilePath(CString& sPath, const CString& sExt)
{
	if (sPath.IsEmpty())
		return FALSE;

	CString sTemp(sPath);

	if (!sExt.IsEmpty())
		FileMisc::ReplaceExtension(sTemp, sExt);

	// don't change sTemp
	CString sFullPath(sTemp);

	// if relative check app folder first
	if (::PathIsRelative(sTemp))
	{
		sFullPath = FileMisc::GetFullPath(sTemp, FileMisc::GetAppFolder());

		// then try CWD
		if (!FileMisc::FileExists(sFullPath))
			sFullPath = FileMisc::GetFullPath(sTemp);
	}

	// test file existence
	if (FileMisc::FileExists(sFullPath))
	{
		sPath = sFullPath;
		return TRUE;
	}

	// else log it
	if (FileMisc::IsLoggingEnabled())
	{
		if (::PathIsRelative(sTemp))
		{
			FileMisc::LogText(_T("File '%s' not found in '%s' or '%s'\n"),
								sTemp, 
								FileMisc::GetAppFolder(), 
								FileMisc::GetCwd());
		}
		else 
		{
			FileMisc::LogText(_T("File '%s' not found\n"), sFullPath);
		}
	}

	return FALSE;
}

// our own local version
CString CToDoListApp::AfxGetAppName()
{
	return ((CToDoListWnd*)m_pMainWnd)->GetTitle();
}

void CToDoListApp::ParseCommandLine(CEnCommandLineInfo& cmdInfo)
{
	CWinApp::ParseCommandLine(cmdInfo); // default

	// turn on logging if requested
    if (cmdInfo.HasOption(SWITCH_LOGGING))
		FileMisc::EnableLogging();

	// validate ini path if present
    if (cmdInfo.HasOption(SWITCH_INIFILE))
	{
		CString sIniPath = cmdInfo.GetOption(SWITCH_INIFILE);

		// always delete existing item
		cmdInfo.DeleteOption(SWITCH_INIFILE);

		// must have ini extension
		if (FileMisc::HasExtension(sIniPath, _T("ini")) &&
			ValidateFilePath(sIniPath))
		{
			// save full path
			cmdInfo.SetOption(SWITCH_INIFILE, sIniPath);
		}
	}

	// validate import path
	if (cmdInfo.HasOption(SWITCH_IMPORT))
	{
		CString sImportPath = cmdInfo.GetOption(SWITCH_IMPORT);

		// always delete existing item
		cmdInfo.DeleteOption(SWITCH_IMPORT);
		
		if (ValidateFilePath(sImportPath))
		{
			// save full path
			cmdInfo.SetOption(SWITCH_IMPORT, sImportPath);
		}
	}

	// validate main file path
	if (!cmdInfo.m_strFileName.IsEmpty())
	{
		if (!ValidateTasklistPath(cmdInfo.m_strFileName))
			cmdInfo.m_strFileName.Empty();
	}

	// validate multiple filepaths
	if (cmdInfo.HasOption(SWITCH_TASKFILE))
	{
		CString sTaskFiles = cmdInfo.GetOption(SWITCH_TASKFILE);
		CStringArray aTaskFiles; 

		int nFile = Misc::Split(sTaskFiles, '|', aTaskFiles, FALSE);

		while (nFile--)
		{
			CString& sTaskfile = aTaskFiles[nFile];

			if (!ValidateTasklistPath(sTaskfile))
			{
				aTaskFiles.RemoveAt(nFile);
			}
			// also remove if it's a dupe of m_strFileName
			else if (cmdInfo.m_strFileName == sTaskfile)
			{
				aTaskFiles.RemoveAt(nFile);
			}
		}

		// save results
		cmdInfo.DeleteOption(SWITCH_TASKFILE);

		if (aTaskFiles.GetSize())
			cmdInfo.SetOption(SWITCH_TASKFILE, Misc::FormatArray(aTaskFiles, '|'));
	}
}

BOOL CToDoListApp::PreTranslateMessage(MSG* pMsg) 
{
	// give first chance to main window for handling accelerators
	if (m_pMainWnd && m_pMainWnd->PreTranslateMessage(pMsg))
		return TRUE;

	// -------------------------------------------------------------------
	// Implement CWinApp::PreTranslateMessage(pMsg)	ourselves
	// so as to not call CMainFrame::PreTranslateMessage(pMsg) twice

	// if this is a thread-message, short-circuit this function
	if (pMsg->hwnd == NULL && DispatchThreadMessageEx(pMsg))
		return TRUE;

	// walk from target to main window but excluding main window
	for (HWND hWnd = pMsg->hwnd; 
		hWnd != NULL && hWnd != m_pMainWnd->GetSafeHwnd(); 
		hWnd = ::GetParent(hWnd))
	{
		CWnd* pWnd = CWnd::FromHandlePermanent(hWnd);

		if (pWnd != NULL)
		{
			// target window is a C++ window
			if (pWnd->PreTranslateMessage(pMsg))
				return TRUE; // trapped by target window (eg: accelerators)
		}
	}
	// -------------------------------------------------------------------

	return FALSE;       // no special processing
	//return CWinApp::PreTranslateMessage(pMsg);
}

void CToDoListApp::OnHelp() 
{ 
	DoHelp();
}

void CToDoListApp::WinHelp(DWORD dwData, UINT nCmd) 
{
	if (nCmd == HELP_CONTEXT)
		DoHelp((LPCTSTR)dwData);
}

void CToDoListApp::DoHelp(const CString& sHelpRef)
{
	CString sHelpUrl(ONLINEHELP);

	if (sHelpRef.IsEmpty())
		sHelpUrl += _T("FrontPage");
	else
		sHelpUrl += sHelpRef;

	FileMisc::Run(*m_pMainWnd, sHelpUrl, NULL, SW_SHOWNORMAL);
}

BOOL CToDoListApp::InitPreferences(CEnCommandLineInfo& cmdInfo)
{
	BOOL bUseIni = FALSE;
	BOOL bSetMultiInstance = FALSE;

    CString sIniPath;

    // try command line override first
    if (cmdInfo.GetOption(SWITCH_INIFILE, sIniPath))
    {
		ASSERT(!sIniPath.IsEmpty());
		ASSERT(!::PathIsRelative(sIniPath));
		ASSERT(FileMisc::PathExists(sIniPath));

		bUseIni = TRUE;
	}
	else if (!cmdInfo.m_strFileName.IsEmpty())
	{
		ASSERT(!::PathIsRelative(cmdInfo.m_strFileName));
		ASSERT(FileMisc::PathExists(cmdInfo.m_strFileName));
		
		// else if there is a tasklist on the commandline 
		// then try for an ini file of the same name
		sIniPath = cmdInfo.m_strFileName;

		if (ValidateFilePath(sIniPath, _T("ini")))
		{
			bUseIni = TRUE;

			// enable multi-instance because this tasklist
			// has its own ini file
			bSetMultiInstance = TRUE;
		}
	}

	// if all else fails then try for an ini file 
	// having the same name as the executable (default)
	if (!bUseIni)
	{
		sIniPath = FileMisc::ReplaceExtension(FileMisc::GetAppFileName(), _T("ini"));
		bUseIni = ValidateFilePath(sIniPath);
	}

	// Has the user already chosen a language?
	CString sLanguageFile;
	BOOL bAdd2Dictionary = FALSE;
	BOOL bFirstTime = (!bUseIni && !CRegKey::KeyExists(HKEY_CURRENT_USER, _T("Software\\Abstractspoon\\ToDoList")));

	// check existing prefs
	if (!bFirstTime)
	{
		if (bUseIni)
		{
			free((void*)m_pszProfileName);
			m_pszProfileName = _tcsdup(sIniPath);
		}
		else
		{
			SetRegistryKey(REGKEY);
		}

		CPreferences prefs;
		bAdd2Dictionary = prefs.GetProfileInt(_T("Preferences"), _T("EnableAdd2Dictionary"), FALSE);

		// language is stored as relative path
		sLanguageFile = prefs.GetProfileString(_T("Preferences"), _T("LanguageFile"));

		if (!sLanguageFile.IsEmpty() && sLanguageFile != CTDLLanguageComboBox::GetDefaultLanguage())
		{
			FileMisc::MakeFullPath(sLanguageFile, FileMisc::GetAppFolder());
		}
		else if (bAdd2Dictionary)
		{
			sLanguageFile = CTDLLanguageComboBox::GetLanguageFile(_T("YourLanguage"), _T("csv"));
		}
	}

	// show language dialog if no language set
	if (sLanguageFile.IsEmpty())
	{
		CTDLLanguageDlg dialog;
		
		if (dialog.DoModal() == IDCANCEL)
			return FALSE;

		// else
		sLanguageFile = dialog.GetLanguageFile();
	}

	// init language translation. 
	// 'u' indicates uppercase mode
	if (cmdInfo.HasOption(SWITCH_TRANSUPPER))
	{
		CLocalizer::Release();
		CLocalizer::Initialize(sLanguageFile, ITTTO_UPPERCASE);
	}
	// 't' indicates 'translation' mode (aka 'Add2Dictionary')
	else if (FileMisc::FileExists(sLanguageFile))
	{
		CLocalizer::Release();

		if (bAdd2Dictionary || cmdInfo.HasOption(SWITCH_ADDTODICT))
			CLocalizer::Initialize(sLanguageFile, ITTTO_ADD2DICTIONARY);
		else
			CLocalizer::Initialize(sLanguageFile, ITTTO_TRANSLATEONLY);
	}
	
	// save it off
	CPreferences prefs;

	if (!bFirstTime)
	{
		FileMisc::MakeRelativePath(sLanguageFile, FileMisc::GetAppFolder(), FALSE);
		prefs.WriteProfileString(_T("Preferences"), _T("LanguageFile"), sLanguageFile);

		prefs.WriteProfileInt(_T("Preferences"), _T("EnableAdd2Dictionary"), bAdd2Dictionary);

		if (bSetMultiInstance)
			WriteProfileInt(_T("Preferences"), _T("MultiInstance"), TRUE);

		prefs.Save();
		
		UpgradePreferences(prefs);
	}
	else  // first time so no ini file exists. show wizard
	{
		CTDLWelcomeWizard wizard;

		// before we show the wizard we need to enable '.tdl' 
		// as a recognized extension else the file icon will 
		// not display correctly on the last page of the wizard
		CFileRegister filereg(_T("tdl"), _T("tdl_Tasklist"));
		filereg.RegisterFileType(_T("Tasklist"), 0);
		
		int nRet = wizard.DoModal();
			
		// remove file extension enabling before continuing
		filereg.UnRegisterFileType();
			
		if (nRet != ID_WIZFINISH)
		{
			// delete ini file that was created to store language choice
			if (bUseIni)
				::DeleteFile(sIniPath);

			return FALSE; // user cancelled so quit app
		}
		
		bUseIni = wizard.GetUseIniFile();

		if (bUseIni)
		{
			free((void*)m_pszProfileName);
			m_pszProfileName = _tcsdup(sIniPath);
		}
		else
			SetRegistryKey(REGKEY);

		// initialize prefs to defaults
		CPreferences prefs;
		CPreferencesDlg dlg;

		dlg.Initialize(prefs);

		// set up some default preferences
		if (wizard.GetShareTasklists()) 
		{
			// set up source control for remote tasklists
			prefs.WriteProfileInt(_T("Preferences"), _T("EnableSourceControl"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("SourceControlLanOnly"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("AutoCheckOut"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("CheckoutOnCheckin"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("CheckinOnClose"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("CheckinNoEditTime"), 1);
			prefs.WriteProfileInt(_T("Preferences"), _T("CheckinNoEdit"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("Use3rdPartySourceControl"), FALSE);
		}
		
		// setup default columns
		CTDCColumnIDArray aColumns;
		wizard.GetVisibleColumns(aColumns);
		
		int nCol = aColumns.GetSize();
		prefs.WriteProfileInt(_T("Preferences\\ColumnVisibility"), _T("Count"), nCol);
		
		while (nCol--)
		{
			CString sKey = Misc::MakeKey(_T("Col%d"), nCol);
			prefs.WriteProfileInt(_T("Preferences\\ColumnVisibility"), sKey, aColumns[nCol]);
		}
		
		if (wizard.GetHideAttributes())
		{
			// hide clutter
			prefs.WriteProfileInt(_T("Preferences"), _T("ShowCtrlsAsColumns"), TRUE);
			prefs.WriteProfileInt(_T("Preferences"), _T("ShowEditMenuAsColumns"), TRUE);
			//prefs.WriteProfileInt("Settings", "ShowFilterBar", FALSE);
		}
		
		// set up initial file
		CString sSample = wizard.GetSampleFilePath();
		
		if (!sSample.IsEmpty())
			cmdInfo.m_strFileName = sSample;

		// save language choice
		FileMisc::MakeRelativePath(sLanguageFile, FileMisc::GetAppFolder(), FALSE);
		prefs.WriteProfileString(_T("Preferences"), _T("LanguageFile"), sLanguageFile);
		prefs.WriteProfileInt(_T("Preferences"), _T("EnableAdd2Dictionary"), bAdd2Dictionary);

		// setup uninstaller to point to us
		if (!bUseIni)
		{
			CRegKey reg;

			if (reg.Open(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AbstractSpoon_ToDoList")) == ERROR_SUCCESS)
			{
				CString sUninstall;
				sUninstall.Format(_T("\"%s\" -%s"), FileMisc::GetAppFileName(), SWITCH_UNINSTALL);

				reg.Write(_T("DisplayName"), CToDoListWnd::GetTitle());			
				reg.Write(_T("NoModify"), 1);
				reg.Write(_T("NoRepair"), 1);
				reg.Write(_T("UninstallString"), sUninstall);
			}
		}
	}

	return TRUE;
}

void CToDoListApp::UpgradePreferences(CPreferences& prefs)
{
	// we don't handle the registry because it's too hard (for now)
	if (m_pszRegistryKey)
		return;
	
	// remove preferences for all files _not_ in the MRU list
	// provided there's at least one file in the MRU list
	BOOL bUseMRU = prefs.GetProfileInt(_T("Preferences"), _T("AddFilesToMRU"), FALSE);

	if (!bUseMRU)
		return;

	CStringArray aMRU;
	
	for (int nFile = 0; nFile < 16; nFile++)
	{
		CString sItem, sFile;
		
		sItem.Format(_T("TaskList%d"), nFile + 1);
		sFile = prefs.GetProfileString(_T("MRU"), sItem);
		
		if (sFile.IsEmpty())
			break;
		
		// else
		sFile = FileMisc::GetFileNameFromPath(sFile);
		Misc::AddUniqueItem(sFile, aMRU);
	}
	
	if (aMRU.GetSize())
	{
		CStringArray aSections;
		int nSection = prefs.GetSectionNames(aSections);
		
		while (nSection--)
		{
			const CString& sSection = aSections[nSection];
			
			// does it start with "FileStates\\" 
			if (sSection.Find(FILESTATEKEY) == 0 || sSection.Find(REMINDERKEY) == 0)
			{
				// split the section name into its parts
				CStringArray aSubSections;
				int nSubSections = Misc::Split(sSection, aSubSections, FALSE, _T("\\"));
				
				if (nSubSections > 1)
				{
					// the file name is the second item
					const CString& sFilename = aSubSections[1];
					
					// ignore 'Default'
					if (sFilename.CompareNoCase(DEFAULTKEY) != 0 && Misc::Find(aMRU, sFilename) == -1)
						prefs.DeleteSection(sSection);
				}
			}
		}
	}
}

int CToDoListApp::DoMessageBox(LPCTSTR lpszPrompt, UINT nType, UINT /*nIDPrompt*/) 
{
	HWND hwndMain = NULL;

	// make sure app window is visible
	if (m_pMainWnd)
	{
		hwndMain = *m_pMainWnd;
		m_pMainWnd->SendMessage(WM_TDL_SHOWWINDOW, 0, 0);
	}
	else
		hwndMain = ::GetDesktopWindow();
	
	CString sTitle(AfxGetAppName()), sInstruction, sText(lpszPrompt);
	CStringArray aPrompt;
	
	int nNumInputs = Misc::Split(lpszPrompt, aPrompt, FALSE, "|");
	
	switch (nNumInputs)
	{
	case 0:
		ASSERT(0);
		break;
		
	case 1:
		// do nothing
		break;
		
	case 2:
		sInstruction = aPrompt[0];
		sText = aPrompt[1];
		break;
		
	case 3:
		sTitle.Format(_T("%s - %s"), AfxGetAppName(), aPrompt[0]);
		sInstruction = aPrompt[1];
		sText = aPrompt[2];
	}
	
	return CDialogHelper::ShowMessageBox(hwndMain, sTitle, sInstruction, sText, nType);
}

void CToDoListApp::OnImportPrefs() 
{
	// default location is always app folder
	CString sIniPath = FileMisc::GetAppFileName();
	sIniPath.MakeLower();
	sIniPath.Replace(_T("exe"), _T("ini"));
	
	CPreferences prefs;
	CFileOpenDialog dialog(IDS_IMPORTPREFS_TITLE, 
							_T("ini"), 
							sIniPath, 
							EOFN_DEFAULTOPEN, 
							CEnString(IDS_INIFILEFILTER));
	
	if (dialog.DoModal(&prefs) == IDOK)
	{
		CRegKey reg;
		
		if (reg.Open(HKEY_CURRENT_USER, APPREGKEY) == ERROR_SUCCESS)
		{
			sIniPath = dialog.GetPathName();
			
			if (reg.ImportFromIni(sIniPath)) // => import ini to registry
			{
				// use them now?
				// only ask if we're not already using the registry
				BOOL bUsingIni = (m_pszRegistryKey == NULL);

				if (bUsingIni)
				{
					if (AfxMessageBox(CEnString(IDS_POSTIMPORTPREFS), MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						// renames existing prefs file
						CString sNewName(sIniPath);
						sNewName += _T(".bak");
						DeleteFile(sNewName);
						
						if (MoveFile(sIniPath, sNewName))
						{
							// and initialize the registry 
							SetRegistryKey(REGKEY);
							
							// reset prefs
							m_pMainWnd->SendMessage(WM_TDL_REFRESHPREFS);
						}
					}
				}
				else // reset prefs
					m_pMainWnd->SendMessage(WM_TDL_REFRESHPREFS);
			}
			else // notify user
			{
				CEnString sMessage(CEnString(IDS_INVALIDPREFFILE), dialog.GetFileName());
				AfxMessageBox(sMessage, MB_OK | MB_ICONEXCLAMATION);
			}
		}
	}
}

void CToDoListApp::OnUpdateImportPrefs(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(TRUE);
}

void CToDoListApp::OnExportPrefs() 
{
	ASSERT (m_pszRegistryKey != NULL);

	CRegKey reg;

	if (reg.Open(HKEY_CURRENT_USER, APPREGKEY) == ERROR_SUCCESS)
	{
		// default location is always app folder
		CString sAppPath = FileMisc::GetAppFileName();

		CString sIniPath(sAppPath);
		sIniPath.MakeLower();
		sIniPath.Replace(_T("exe"), _T("ini"));
		
		CPreferences prefs;
		CFileSaveDialog dialog(IDS_IMPORTPREFS_TITLE, 
								_T("ini"), 
								sIniPath, 
								EOFN_DEFAULTSAVE, 
								CEnString(IDS_INIFILEFILTER));
		
		if (dialog.DoModal(&prefs) == IDOK)
		{
			BOOL bUsingReg = (m_pszRegistryKey != NULL);
			sIniPath = dialog.GetPathName();

			if (bUsingReg && reg.ExportToIni(sIniPath))
			{
				// use them now? 
				CString sAppFolder, sIniFolder;
				
				FileMisc::SplitPath(sAppPath, NULL, &sAppFolder);
				FileMisc::SplitPath(sIniPath, NULL, &sIniFolder);
				
				// only if they're in the same folder as the exe
				if (sIniFolder.CompareNoCase(sAppFolder) == 0)
				{
					if (AfxMessageBox(CEnString(IDS_POSTEXPORTPREFS), MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						free((void*)m_pszRegistryKey);
						m_pszRegistryKey = NULL;
						
						free((void*)m_pszProfileName);
						m_pszProfileName = _tcsdup(sIniPath);
						
						// reset prefs
						m_pMainWnd->SendMessage(WM_TDL_REFRESHPREFS);
					}
				}
			}
		}
	}
}

void CToDoListApp::OnUpdateExportPrefs(CCmdUI* pCmdUI) 
{
	BOOL bUsingReg = (m_pszRegistryKey != NULL);
	pCmdUI->Enable(bUsingReg);
}

void CToDoListApp::OnHelpContactus() 
{
	CString sParams;
	//FormatEmailParams(sParams);

	FileMisc::Run(*m_pMainWnd, CONTACTUS + sParams, NULL, SW_SHOWNORMAL);
}

BOOL CToDoListApp::FormatEmailParams(CString& sParams)
{
	const CString ENDL(_T("%0A"));
	
	sParams += _T("?subject=ToDoList: Bug Report&body=");
	sParams += _T("Details of Bug: <description>") + ENDL + ENDL;
	sParams += _T("Steps to Reproduce: <description>") + ENDL + ENDL;
	sParams += _T("Windows Version: ") + COSVersion().FormatOSVersion() + ENDL;
	sParams += _T("ToDoList Version: ") + FileMisc::GetModuleVersion() + ENDL;
	
	return TRUE;
}


void CToDoListApp::OnHelpFeedbackandsupport() 
{
	FileMisc::Run(*m_pMainWnd, FEEDBACKANDSUPPORT, NULL, SW_SHOWNORMAL);
}

void CToDoListApp::OnHelpLicense() 
{
	FileMisc::Run(*m_pMainWnd, LICENSE, NULL, SW_SHOWNORMAL);
}

void CToDoListApp::OnHelpWiki() 
{
	FileMisc::Run(*m_pMainWnd, WIKI, NULL, SW_SHOWNORMAL);
}

void CToDoListApp::OnHelpCommandline() 
{
	CTDLCmdlineOptionsDlg dialog;
	dialog.DoModal();
}

void CToDoListApp::OnHelpDonate() 
{
	FileMisc::Run(*m_pMainWnd, DONATE, NULL, SW_SHOWNORMAL);
}

int CToDoListApp::ExitInstance() 
{
	// TODO: Add your specialized code here and/or call the base class
	CLocalizer::Release();

	return CWinApp::ExitInstance();
}

void CToDoListApp::OnHelpUninstall() 
{
	RunUninstaller();
}

void CToDoListApp::RunUninstaller()
{
	HWND hwndMain = NULL;

	if (m_pMainWnd != NULL)
	{
		HWND hwndMain = m_pMainWnd->GetSafeHwnd();
		ASSERT(hwndMain);
		
		// close the application
		::SendMessage(hwndMain, WM_CLOSE, 0, 0);
	}
	
	// check the user didn't cancel
	if (!::IsWindow(hwndMain))
	{
		// copy ourselves to temp folder and execute from there
		CString sAppPath = FileMisc::GetAppFileName();
		CString sTempPath = FileMisc::GetTempFileName(_T("tdluninst"), _T("exe"));
		
		if (::CopyFile(sAppPath, sTempPath, FALSE))
		{
			CString sParams;
			sParams.Format(_T("-%s \"%s\""), SWITCH_UNINSTALL, FileMisc::GetAppFolder());
			
			DWORD dwRes = (DWORD)ShellExecute(NULL, NULL, sTempPath, sParams, NULL, SW_HIDE);
			
			if (dwRes > 32)
				return; // success
		}

		// notify user
		AfxMessageBox(IDS_UNINSTALL_FAILURE);
	}
}

void CToDoListApp::DoUnistall(const CString& sAppFolder)
{
	// sanity checks
	if (sAppFolder.IsEmpty() || !FileMisc::FolderExists(sAppFolder))
		return;

	// make sure we're in the temp folder
	CString sAppPath = FileMisc::GetAppFileName();

	if (FileMisc::IsTempFile(sAppPath))
	{
		// confirm uninstall first
		CEnString sConfirm(IDS_CONFIRM_UNINSTALL, sAppFolder);
		
		if (AfxMessageBox(sConfirm, MB_YESNO | MB_ICONWARNING | MB_SYSTEMMODAL) == IDYES)
		{
			// give the app some time to close
			Sleep(1000);

			// cleanup registry
			// Note: Delete key regardless of existence to handle old installs
			CRegKey::DeleteKey(HKEY_CURRENT_USER, _T("Software\\AbstractSpoon\\ToDoList"));
			CRegKey::DeleteKey(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\AbstractSpoon_ToDoList"));

			// Delete application folder
			int nRet = FileMisc::DeleteFolder(sAppFolder, FMDF_ALLOWDELETEONREBOOT | FMDF_SUBFOLDERS);
			
			switch (nRet)
			{
			case  0: AfxMessageBox(IDS_UNINSTALL_FAILURE, MB_SYSTEMMODAL); break;
			case  1: AfxMessageBox(IDS_UNINSTALL_SUCCESS, MB_SYSTEMMODAL); break;
			case -1: AfxMessageBox(IDS_UNINSTALL_DELAY,   MB_SYSTEMMODAL); break;
			}
		}

		// Delete ourselves always
		::MoveFileEx(sAppPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
	}
	else
	{
		RunUninstaller();
	}
}

int CToDoListApp::FindToDoListWnds(TDCFINDWND& find)
{
	ASSERT(find.hWndIgnore == NULL || ::IsWindow(find.hWndIgnore));

	find.aResults.RemoveAll();
	EnumWindows(FindOtherInstance, (LPARAM)&find);

	return find.aResults.GetSize();
}

BOOL CALLBACK CToDoListApp::FindOtherInstance(HWND hwnd, LPARAM lParam)
{
	static CString COPYRIGHT(MAKEINTRESOURCE(IDS_COPYRIGHT));

	CString sCaption;
	CWnd::FromHandle(hwnd)->GetWindowText(sCaption);

	if (sCaption.Find(COPYRIGHT) != -1)
	{
		TDCFINDWND* pFind = (TDCFINDWND*)lParam;
		ASSERT(pFind);

		// check window to ignore
		if ((pFind->hWndIgnore == NULL) || (pFind->hWndIgnore == hwnd))
		{
			// check if it's closing
			DWORD bClosing = FALSE;
			BOOL bSendSucceeded = ::SendMessageTimeout(hwnd, 
														WM_TDL_ISCLOSING, 
														0, 
														0, 
														SMTO_ABORTIFHUNG | SMTO_BLOCK, 
														1000, 
														&bClosing);

			// good to go
			if (bSendSucceeded && !bClosing)
				pFind->aResults.Add(hwnd);
		}
	}

	return TRUE; // keep going to the end
}

/////////////////////////////////////////////////////////////////////////////
