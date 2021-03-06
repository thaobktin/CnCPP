// ToDoListWnd.cpp : implementation file
//

#include "stdafx.h"
#include "ToDoList.h"
#include "ToDoListWnd.h"
#include "ToolsCmdlineParser.h"
#include "ToolsUserInputDlg.h"
#include "Toolshelper.h"
#include "tdlexportDlg.h"
#include "tasklisthtmlexporter.h"
#include "tasklisttxtexporter.h"
#include "tdcmsg.h"
#include "tdlschemadef.h"
#include "tdlprintdialog.h"
#include "tdltransformdialog.h"
#include "tdstringres.h"
#include "tdlcolumnselectiondlg.h"
#include "tdlfilterdlg.h"
#include "OffsetDatesDlg.h"
#include "KeyboardShortcutDisplayDlg.h"
#include "tdlimportdialog.h"
#include "tdlsetreminderdlg.h"
#include "tdlshowreminderdlg.h"
#include "tdlmultisortdlg.h"
#include "tdladdloggedtimedlg.h"
#include "multitaskfile.h"
#include "tdcstatic.h"
#include "tdlcustomattributedlg.h"
#include "tdccustomattributehelper.h"
#include "welcomewizard.h"

#include "..\shared\aboutdlg.h"
#include "..\shared\holdredraw.h"
#include "..\shared\autoflag.h"
#include "..\shared\enbitmap.h"
#include "..\shared\spellcheckdlg.h"
#include "..\shared\encolordialog.h"
#include "..\shared\winclasses.h"
#include "..\shared\wclassdefines.h"
#include "..\shared\datehelper.h"
#include "..\shared\osversion.h"
#include "..\shared\enfiledialog.h"
#include "..\shared\misc.h"
#include "..\shared\graphicsmisc.h"
#include "..\shared\filemisc.h"
#include "..\shared\themed.h"
#include "..\shared\enstring.h"
#include "..\shared\fileregister.h"
#include "..\shared\mousewheelMgr.h"
#include "..\shared\editshortcutMgr.h"
#include "..\shared\dlgunits.h"
#include "..\shared\passworddialog.h"
#include "..\shared\sysimagelist.h"
#include "..\shared\regkey.h"
#include "..\shared\uiextensionuihelper.h"
#include "..\shared\lightbox.h"
#include "..\shared\remotefile.h"
#include "..\shared\serverdlg.h"
#include "..\shared\focuswatcher.h"
#include "..\shared\localizer.h"
#include "..\shared\outlookhelper.h"

#include "..\3rdparty\gui.h"
#include "..\3rdparty\sendfileto.h"

#include <shlwapi.h>
#include <windowsx.h>
#include <direct.h>
#include <math.h>

#include <afxpriv.h>        // for WM_KICKIDLE

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CToDoListWnd dialog

// popup menus
enum { TRAYICON, TASKCONTEXT, TABCTRLCONTEXT, HEADERCONTEXT };
enum { TB_TOOLBARHIDDEN, TB_DUMMY, TB_TOOLBARANDMENU };
enum { FILEALL, NEWTASK, EDITTASK, VIEW, MOVE, SORTTASK, TOOLS, HELP };

const int TD_VERSION = 31000;
const int BEVEL = 2; // pixels
const int BORDER = 3; // pixels
const int TB_VOFFSET = 4;
const int ONE_MINUTE = 60000; // milliseconds

const LPCTSTR UPDATE_SCRIPT_PATH = _T("http://www.abstractspoon.com/todolist_update.txt");
const LPCTSTR UPDATE_SCRIPT_PATH_MANUAL = _T("http://www.abstractspoon.com/todolist_update_manual.txt");
const LPCTSTR UPDATE_SCRIPT_PATH_STAGING = _T("http://www.abstractspoon.com/todolist_update_manual_staging.txt");
const LPCTSTR PREF_KEY = _T("Preferences");

#define DOPROGRESS(stringID) \
	CWaitCursor cursor; \
	CStatusBarProgressProxy prog(&m_sbProgress, m_statusBar, CEnString(stringID));

enum
{
	WM_POSTONCREATE = (WM_APP+1),
	WM_WEBUPDATEWIZARD,
	WM_ADDTOOLBARTOOLS,
	WM_APPRESTOREFOCUS,
};

enum 
{
	TIMER_READONLYSTATUS = 1,
	TIMER_TIMESTAMPCHANGE,
	TIMER_AUTOSAVE,
	TIMER_CHECKOUTSTATUS,
	TIMER_DUEITEMS,
	TIMER_TIMETRACKING,
	TIMER_AUTOMINIMIZE,
};

enum 
{
	INTERVAL_READONLYSTATUS = 1000,
	INTERVAL_TIMESTAMPCHANGE = 10000,
	//	INTERVAL_AUTOSAVE, // dynamically determined
	INTERVAL_CHECKOUTSTATUS = 5000,
	INTERVAL_DUEITEMS = ONE_MINUTE,
	INTERVAL_TIMETRACKING = 5000,
	//	INTERVAL_AUTOMINIMIZE, // dynamically determined
};

/////////////////////////////////////////////////////////////////////////////

CToDoListWnd::CToDoListWnd() : CFrameWnd(), 
		m_bVisible(-1), 
		m_mruList(0, _T("MRU"), _T("TaskList%d"), 16, AFX_ABBREV_FILENAME_LEN, CEnString(IDS_RECENTFILES)),
		m_nLastSelItem(-1), 
		m_nMaxState(TDCMS_NORMAL), 
		m_nPrevMaxState(TDCMS_NORMAL),
		m_bShowFilterBar(TRUE),
		m_bShowStatusBar(TRUE),
		m_bInNewTask(FALSE),
		m_bSaving(FALSE),
		m_mgrShortcuts(FALSE),
		m_pPrefs(NULL),
		m_bClosing(FALSE),
		m_mgrToDoCtrls(m_tabCtrl),
		m_bFindShowing(FALSE),
		m_bShowProjectName(TRUE),
		m_bQueryOpenAllow(FALSE),
		m_bPasswordPrompting(TRUE),
		m_bShowToolbar(TRUE),
		m_bReloading(FALSE),
		m_hIcon(NULL),
		m_hwndLastFocus(NULL),
		m_bStartHidden(FALSE),
		m_cbQuickFind(ACBS_ALLOWDELETE | ACBS_ADDTOSTART),
		m_bShowTasklistBar(TRUE), 
		m_bShowTreeListBar(TRUE),
		m_bEndingSession(FALSE),
		m_nContextColumnID(TDCC_NONE)
{
	// must do this before initializing any controls
	SetupUIStrings();

	// delete temporary web-update-wizard
	CString sWuwPathTemp = FileMisc::GetAppFolder() + _T("\\WebUpdateSvc2.exe");
	::DeleteFile(sWuwPathTemp);

	// init preferences
	ResetPrefs();

	CFilteredToDoCtrl::EnableExtendedSelection(FALSE, TRUE);

	m_bAutoMenuEnable = FALSE;
}

CToDoListWnd::~CToDoListWnd()
{
	delete m_pPrefs;

	if (m_hIcon)
		DestroyIcon(m_hIcon);

	// cleanup temp files
	// Note: Due task notifications are removed by CToDoCtrlMgr
	::DeleteFile(FileMisc::GetTempFileName(_T("ToDoList.print"), _T("html")));
	::DeleteFile(FileMisc::GetTempFileName(_T("ToDoList.import"), _T("txt")));
	::DeleteFile(FileMisc::GetTempFileName(_T("tdt")));
}

BEGIN_MESSAGE_MAP(CToDoListWnd, CFrameWnd)
//{{AFX_MSG_MAP(CToDoListWnd)
	ON_WM_QUERYOPEN()
	ON_COMMAND(ID_ADDTIMETOLOGFILE, OnAddtimetologfile)
	ON_COMMAND(ID_ARCHIVE_SELECTEDTASKS, OnArchiveSelectedTasks)
	ON_COMMAND(ID_CLOSEALLBUTTHIS, OnCloseallbutthis)
	ON_COMMAND(ID_EDIT_COPYAS_DEPEND, OnCopyTaskasDependency)
	ON_COMMAND(ID_EDIT_COPYAS_DEPENDFULL, OnCopyTaskasDependencyFull)
	ON_COMMAND(ID_EDIT_COPYAS_PATH, OnCopyTaskasPath)
	ON_COMMAND(ID_EDIT_COPYAS_LINK, OnCopyTaskasLink)
	ON_COMMAND(ID_EDIT_COPYAS_LINKFULL, OnCopyTaskasLinkFull)
	ON_COMMAND(ID_EDIT_CLEARREMINDER, OnEditClearReminder)
	ON_COMMAND(ID_EDIT_CLEARTASKCOLOR, OnEditCleartaskcolor)
	ON_COMMAND(ID_EDIT_CLEARTASKICON, OnEditCleartaskicon)
	ON_COMMAND(ID_EDIT_DECTASKPERCENTDONE, OnEditDectaskpercentdone)
	ON_COMMAND(ID_EDIT_DECTASKPRIORITY, OnEditDectaskpriority)
	ON_COMMAND(ID_EDIT_FLAGTASK, OnEditFlagtask)
	ON_COMMAND(ID_EDIT_INCTASKPERCENTDONE, OnEditInctaskpercentdone)
	ON_COMMAND(ID_EDIT_INCTASKPRIORITY, OnEditInctaskpriority)
	ON_COMMAND(ID_EDIT_INSERTDATE, OnEditInsertdate)
	ON_COMMAND(ID_EDIT_INSERTDATETIME, OnEditInsertdatetime)
	ON_COMMAND(ID_EDIT_INSERTTIME, OnEditInserttime)
	ON_COMMAND(ID_EDIT_OFFSETDATES, OnEditOffsetdates)
	ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_COMMAND(ID_EDIT_SELECTALL, OnEditSelectall)
	ON_COMMAND(ID_EDIT_SETREMINDER, OnEditSetReminder)
	ON_COMMAND(ID_EDIT_SETTASKICON, OnEditSettaskicon)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_WM_ENABLE()
	ON_COMMAND(ID_FILE_CHANGEPASSWORD, OnFileChangePassword)
	ON_COMMAND(ID_FILE_OPENARCHIVE, OnFileOpenarchive)
	ON_COMMAND(ID_NEXTTASK, OnGotoNexttask)
	ON_COMMAND(ID_PREVTASK, OnGotoPrevtask)
	ON_WM_MEASUREITEM()
	ON_COMMAND(ID_NEWSUBTASK, OnNewsubtask)
	ON_COMMAND(ID_NEWTASK, OnNewtask)
	ON_COMMAND(ID_PRINTPREVIEW, OnPrintpreview)
	ON_COMMAND(ID_EDIT_QUICKFIND, OnQuickFind)
	ON_COMMAND(ID_EDIT_QUICKFINDNEXT, OnQuickFindNext)
	ON_COMMAND(ID_EDIT_QUICKFINDPREV, OnQuickFindPrev)
	ON_COMMAND(ID_SENDTASKS, OnSendTasks)
	ON_COMMAND(ID_SEND_SELTASKS, OnSendSelectedTasks)
	ON_COMMAND(ID_HELP_KEYBOARDSHORTCUTS, OnShowKeyboardshortcuts)
	ON_COMMAND(ID_SHOWTIMELOGFILE, OnShowTimelogfile)
	ON_COMMAND(ID_SORT_MULTI, OnSortMulti)
	ON_WM_SYSCOLORCHANGE()
	ON_COMMAND(ID_TABCTRL_PREFERENCES, OnTabctrlPreferences)
	ON_COMMAND(ID_TASKLIST_SELECTCOLUMNS, OnTasklistSelectColumns)
	ON_COMMAND(ID_TOOLS_CHECKFORUPDATES, OnToolsCheckforupdates)
	ON_COMMAND(ID_TOOLS_TRANSFORM, OnToolsTransformactivetasklist)
	ON_UPDATE_COMMAND_UI(ID_ADDTIMETOLOGFILE, OnUpdateAddtimetologfile)
	ON_UPDATE_COMMAND_UI(ID_ARCHIVE_SELECTEDTASKS, OnUpdateArchiveSelectedCompletedTasks)
	ON_UPDATE_COMMAND_UI(ID_CLOSEALLBUTTHIS, OnUpdateCloseallbutthis)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_DEPEND, OnUpdateCopyTaskasDependency)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_DEPENDFULL, OnUpdateCopyTaskasDependencyFull)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_PATH, OnUpdateCopyTaskasPath)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_LINK, OnUpdateCopyTaskasLink)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_LINKFULL, OnUpdateCopyTaskasLinkFull)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEARREMINDER, OnUpdateEditClearReminder)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEARTASKCOLOR, OnUpdateEditCleartaskcolor)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEARTASKICON, OnUpdateEditCleartaskicon)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DECTASKPERCENTDONE, OnUpdateEditDectaskpercentdone)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DECTASKPRIORITY, OnUpdateEditDectaskpriority)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FLAGTASK, OnUpdateEditFlagtask)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INCTASKPERCENTDONE, OnUpdateEditInctaskpercentdone)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INCTASKPRIORITY, OnUpdateEditInctaskpriority)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERTDATE, OnUpdateEditInsertdate)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERTDATETIME, OnUpdateEditInsertdatetime)
	ON_UPDATE_COMMAND_UI(ID_EDIT_INSERTTIME, OnUpdateEditInserttime)
	ON_UPDATE_COMMAND_UI(ID_EDIT_OFFSETDATES, OnUpdateEditOffsetdates)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECTALL, OnUpdateEditSelectall)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETREMINDER, OnUpdateEditSetReminder)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETTASKICON, OnUpdateEditSettaskicon)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_UPDATE_COMMAND_UI(ID_FILE_CHANGEPASSWORD, OnUpdateFileChangePassword)
	ON_UPDATE_COMMAND_UI(ID_FILE_OPENARCHIVE, OnUpdateFileOpenarchive)
	ON_UPDATE_COMMAND_UI(ID_NEXTTASK, OnUpdateGotoNexttask)
	ON_UPDATE_COMMAND_UI(ID_PREVTASK, OnUpdateGotoPrevtask)
	ON_UPDATE_COMMAND_UI(ID_NEWSUBTASK, OnUpdateNewsubtask)
	ON_UPDATE_COMMAND_UI(ID_EDIT_QUICKFIND, OnUpdateQuickFind)
	ON_UPDATE_COMMAND_UI(ID_EDIT_QUICKFINDNEXT, OnUpdateQuickFindNext)
	ON_UPDATE_COMMAND_UI(ID_EDIT_QUICKFINDPREV, OnUpdateQuickFindPrev)
	ON_UPDATE_COMMAND_UI(ID_SHOWTIMELOGFILE, OnUpdateShowTimelogfile)
	ON_UPDATE_COMMAND_UI(ID_SENDTASKS, OnUpdateSendTasks)
	ON_UPDATE_COMMAND_UI(ID_SEND_SELTASKS, OnUpdateSendSelectedTasks)
	ON_UPDATE_COMMAND_UI(ID_SORT_MULTI, OnUpdateSortMulti)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CLEARFILTER, OnUpdateViewClearfilter)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COLLAPSEDUE, OnUpdateViewCollapseDuetasks)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COLLAPSESTARTED, OnUpdateViewCollapseStartedtasks)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COLLAPSEALL, OnUpdateViewCollapseall)
	ON_UPDATE_COMMAND_UI(ID_VIEW_COLLAPSETASK, OnUpdateViewCollapsetask)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPANDDUE, OnUpdateViewExpandDuetasks)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPANDSTARTED, OnUpdateViewExpandStartedtasks)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPANDALL, OnUpdateViewExpandall)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXPANDTASK, OnUpdateViewExpandtask)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FILTER, OnUpdateViewFilter)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PROJECTNAME, OnUpdateViewProjectname)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTASKLISTTABBAR, OnUpdateViewShowTasklistTabbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTREELISTTABBAR, OnUpdateViewShowTreeListTabbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFILTERBAR, OnUpdateViewShowfilterbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SORTTASKLISTTABS, OnUpdateViewSorttasklisttabs)
	ON_UPDATE_COMMAND_UI(ID_VIEW_STATUS_BAR, OnUpdateViewStatusBar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_CYCLETASKVIEWS, OnUpdateViewCycleTaskViews)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLETREEANDLIST, OnUpdateViewToggleTreeandList)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLEFILTER, OnUpdateViewTogglefilter)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLETASKEXPANDED, OnUpdateViewToggletaskexpanded)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOGGLETASKSANDCOMMENTS, OnUpdateViewToggletasksandcomments)
	ON_UPDATE_COMMAND_UI(ID_WINDOW1, OnUpdateWindow)
	ON_COMMAND(ID_VIEW_CLEARFILTER, OnViewClearfilter)
	ON_COMMAND(ID_VIEW_COLLAPSEDUE, OnViewCollapseDuetasks)
	ON_COMMAND(ID_VIEW_COLLAPSESTARTED, OnViewCollapseStartedtasks)
	ON_COMMAND(ID_VIEW_COLLAPSEALL, OnViewCollapseall)
	ON_COMMAND(ID_VIEW_COLLAPSETASK, OnViewCollapsetask)
	ON_COMMAND(ID_VIEW_EXPANDDUE, OnViewExpandDuetasks)
	ON_COMMAND(ID_VIEW_EXPANDSTARTED, OnViewExpandStartedtasks)
	ON_COMMAND(ID_VIEW_EXPANDALL, OnViewExpandall)
	ON_COMMAND(ID_VIEW_EXPANDTASK, OnViewExpandtask)
	ON_COMMAND(ID_VIEW_FILTER, OnViewFilter)
	ON_COMMAND(ID_VIEW_PROJECTNAME, OnViewProjectname)
	ON_COMMAND(ID_VIEW_SHOWTASKLISTTABBAR, OnViewShowTasklistTabbar) 
	ON_COMMAND(ID_VIEW_SHOWTREELISTTABBAR, OnViewShowTreeListTabbar)
	ON_COMMAND(ID_VIEW_SHOWFILTERBAR, OnViewShowfilterbar)
	ON_COMMAND(ID_VIEW_SORTTASKLISTTABS, OnViewSorttasklisttabs)
	ON_COMMAND(ID_VIEW_STATUS_BAR, OnViewStatusBar)
	ON_COMMAND(ID_VIEW_CYCLETASKVIEWS, OnViewCycleTaskViews)
	ON_COMMAND(ID_VIEW_TOGGLETREEANDLIST, OnViewToggleTreeandList)
	ON_COMMAND(ID_VIEW_TOGGLEFILTER, OnViewTogglefilter)
	ON_COMMAND(ID_VIEW_TOGGLETASKEXPANDED, OnViewToggletaskexpanded)
	ON_COMMAND(ID_VIEW_TOGGLETASKSANDCOMMENTS, OnViewToggletasksandcomments)
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_WINDOWPOSCHANGING()
	ON_COMMAND(ID_TASKLIST_CUSTOMCOLUMNS, OnTasklistCustomColumns)
	ON_COMMAND(ID_EDIT_GOTODEPENDENCY, OnEditGotoDependency)
	ON_UPDATE_COMMAND_UI(ID_EDIT_GOTODEPENDENCY, OnUpdateEditGotoDependency)
	ON_COMMAND(ID_EDIT_RECURRENCE, OnEditRecurrence)
	ON_UPDATE_COMMAND_UI(ID_EDIT_RECURRENCE, OnUpdateEditRecurrence)
	ON_WM_QUERYENDSESSION()
	ON_COMMAND(ID_EDIT_CLEARFIELD, OnEditClearAttribute)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEARFIELD, OnUpdateEditClearAttribute)
	ON_COMMAND(ID_EDIT_CLEARFOCUSEDFIELD, OnEditClearFocusedAttribute)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLEARFOCUSEDFIELD, OnUpdateEditClearFocusedAttribute)
	ON_UPDATE_COMMAND_UI(ID_TASKLIST_CUSTOMCOLUMNS, OnUpdateTasklistCustomcolumns)
	ON_COMMAND(ID_ADDTIMETOLOGFILE, OnAddtimetologfile)
	ON_UPDATE_COMMAND_UI(ID_ADDTIMETOLOGFILE, OnUpdateAddtimetologfile)
	ON_WM_ACTIVATEAPP()
	ON_WM_ERASEBKGND()
	ON_WM_INITMENUPOPUP()
	ON_WM_MOVE()
	ON_WM_SYSCOMMAND()
	ON_COMMAND(ID_EDIT_PASTEASREF, OnEditPasteAsRef)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTEASREF, OnUpdateEditPasteAsRef)
	//}}AFX_MSG_MAP
	ON_CBN_EDITCHANGE(IDC_QUICKFIND, OnEditChangeQuickFind)
	ON_CBN_SELCHANGE(IDC_QUICKFIND, OnSelChangeQuickFind)
	ON_COMMAND(ID_ABOUT, OnAbout)
	ON_COMMAND(ID_ARCHIVE_COMPLETEDTASKS, OnArchiveCompletedtasks)
	ON_COMMAND(ID_CLOSE, OnCloseTasklist)
	ON_COMMAND(ID_CLOSEALL, OnCloseall)
	ON_COMMAND(ID_DELETEALLTASKS, OnDeleteAllTasks)
	ON_COMMAND(ID_DELETETASK, OnDeleteTask)
	ON_COMMAND(ID_EDIT_CLOCK_TASK, OnEditTimeTrackTask)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_COPYAS_HTML, OnEditCopyashtml)
	ON_COMMAND(ID_EDIT_COPYAS_TEXT, OnEditCopyastext)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_FINDTASKS, OnFindTasks)
	ON_COMMAND(ID_EDIT_OPENFILEREF, OnEditOpenfileref)
	ON_COMMAND(ID_EDIT_PASTEAFTER, OnEditPasteAfter)
	ON_COMMAND(ID_EDIT_PASTESUB, OnEditPasteSub)
	ON_COMMAND(ID_EDIT_SETFILEREF, OnEditSetfileref)
	ON_COMMAND(ID_EDIT_SPELLCHECKCOMMENTS, OnSpellcheckcomments)
	ON_COMMAND(ID_EDIT_SPELLCHECKTITLE, OnSpellchecktitle)
	ON_COMMAND(ID_EDIT_TASKCOLOR, OnEditTaskcolor)
	ON_COMMAND(ID_EDIT_TASKDONE, OnEditTaskdone)
	ON_COMMAND(ID_EDIT_TASKTEXT, OnEditTasktext)
	ON_COMMAND(ID_EXIT, OnExit)
	ON_COMMAND(ID_FILE_ENCRYPT, OnFileEncrypt)
	ON_COMMAND(ID_FILE_RESETVERSION, OnFileResetversion)
	ON_COMMAND(ID_LOAD_NORMAL, OnLoad)
	ON_COMMAND(ID_MAXCOMMENTS, OnMaximizeComments)
	ON_COMMAND(ID_MAXTASKLIST, OnMaximizeTasklist)
	ON_COMMAND(ID_MINIMIZETOTRAY, OnMinimizeToTray)
	ON_COMMAND(ID_MOVETASKDOWN, OnMovetaskdown)
	ON_COMMAND(ID_MOVETASKLEFT, OnMovetaskleft)
	ON_COMMAND(ID_MOVETASKRIGHT, OnMovetaskright)
	ON_COMMAND(ID_MOVETASKUP, OnMovetaskup)
	ON_COMMAND(ID_NEW, OnNew)
	ON_COMMAND(ID_NEWSUBTASK_ATBOTTOM, OnNewsubtaskAtbottom)
	ON_COMMAND(ID_NEWSUBTASK_ATTOP, OnNewsubtaskAttop)
	ON_COMMAND(ID_NEWTASK_AFTERSELECTEDTASK, OnNewtaskAfterselectedtask)
	ON_COMMAND(ID_NEWTASK_ATBOTTOM, OnNewtaskAtbottom)
	ON_COMMAND(ID_NEWTASK_ATBOTTOMSELECTED, OnNewtaskAtbottomSelected)
	ON_COMMAND(ID_NEWTASK_ATTOP, OnNewtaskAttop)
	ON_COMMAND(ID_NEWTASK_ATTOPSELECTED, OnNewtaskAttopSelected)
	ON_COMMAND(ID_NEWTASK_BEFORESELECTEDTASK, OnNewtaskBeforeselectedtask)
	ON_COMMAND(ID_NEXTTOPLEVELTASK, OnNexttopleveltask)
	ON_COMMAND(ID_OPEN_RELOAD, OnReload)
	ON_COMMAND(ID_PREFERENCES, OnPreferences)
	ON_COMMAND(ID_PREVTOPLEVELTASK, OnPrevtopleveltask)
	ON_COMMAND(ID_PRINT, OnPrint)
	ON_COMMAND(ID_SAVEALL, OnSaveall)
	ON_COMMAND(ID_SAVEAS, OnSaveas)
	ON_COMMAND(ID_SAVE_NORMAL, OnSave)
	ON_COMMAND(ID_SORT, OnSort)
	ON_COMMAND(ID_TOOLS_CHECKIN, OnToolsCheckin)
	ON_COMMAND(ID_TOOLS_CHECKOUT, OnToolsCheckout)
	ON_COMMAND(ID_TOOLS_EXPORT, OnExport)
	ON_COMMAND(ID_TOOLS_IMPORT, OnImportTasklist)
	ON_COMMAND(ID_TOOLS_SPELLCHECKTASKLIST, OnSpellcheckTasklist)
 	ON_COMMAND(ID_TOOLS_REMOVEFROMSOURCECONTROL, OnToolsRemovefromsourcecontrol)
	ON_COMMAND(ID_TOOLS_TOGGLECHECKIN, OnToolsToggleCheckin)
	ON_COMMAND(ID_TRAYICON_CLOSE, OnTrayiconClose)
	ON_COMMAND(ID_TRAYICON_CREATETASK, OnTrayiconCreatetask)
	ON_COMMAND(ID_TRAYICON_SHOW, OnTrayiconShow)
	ON_COMMAND(ID_VIEW_MOVETASKLISTLEFT, OnViewMovetasklistleft)
	ON_COMMAND(ID_VIEW_MOVETASKLISTRIGHT, OnViewMovetasklistright)
	ON_COMMAND(ID_VIEW_NEXT, OnViewNext)
	ON_COMMAND(ID_VIEW_NEXT_SEL, OnViewNextSel)
	ON_COMMAND(ID_VIEW_PREV, OnViewPrev)
	ON_COMMAND(ID_VIEW_PREV_SEL, OnViewPrevSel)
	ON_COMMAND(ID_VIEW_REFRESHFILTER, OnViewRefreshfilter)
	ON_COMMAND(ID_VIEW_TOOLBAR, OnViewToolbar)
	ON_COMMAND_EX_RANGE(ID_FILE_MRU_FILE1, ID_FILE_MRU_FILE16, OnOpenRecentFile)
	ON_COMMAND_RANGE(ID_EDIT_SETPRIORITYNONE, ID_EDIT_SETPRIORITY10, OnSetPriority)
	ON_COMMAND_RANGE(ID_NEWTASK_SPLITTASKINTO_TWO, ID_NEWTASK_SPLITTASKINTO_FIVE, OnSplitTaskIntoPieces)
	ON_COMMAND_RANGE(ID_SORT_BYFIRST, ID_SORT_BYLAST, OnSortBy)
	ON_COMMAND_RANGE(ID_TOOLS_SHOWTASKS_DUETODAY, ID_TOOLS_SHOWTASKS_DUEENDNEXTMONTH, OnToolsShowtasksDue)
//	ON_COMMAND_RANGE(ID_TOOLS_USEREXT1, ID_TOOLS_USEREXT16, OnUserUIExtension)
	ON_COMMAND_RANGE(ID_TOOLS_USERTOOL1, ID_TOOLS_USERTOOL16, OnUserTool)
	ON_COMMAND_RANGE(ID_FILE_OPEN_USERSTORAGE1, ID_FILE_OPEN_USERSTORAGE16, OnFileOpenFromUserStorage)
	ON_COMMAND_RANGE(ID_FILE_SAVE_USERSTORAGE1, ID_FILE_SAVE_USERSTORAGE16, OnFileSaveToUserStorage)
	ON_COMMAND_RANGE(ID_TRAYICON_SHOWDUETASKS1, ID_TRAYICON_SHOWDUETASKS20, OnTrayiconShowDueTasks)
	ON_COMMAND_RANGE(ID_WINDOW1, ID_WINDOW16, OnWindow)
	ON_MESSAGE(WM_ADDTOOLBARTOOLS, OnAddToolbarTools)
	ON_MESSAGE(WM_APPRESTOREFOCUS, OnAppRestoreFocus)
	ON_MESSAGE(WM_GETFONT, OnGetFont)
	ON_MESSAGE(WM_GETICON, OnGetIcon)
	ON_MESSAGE(WM_HOTKEY, OnHotkey)
	ON_MESSAGE(WM_POSTONCREATE, OnPostOnCreate)
	ON_MESSAGE(WM_POWERBROADCAST, OnPowerBroadcast)
	ON_MESSAGE(WM_WEBUPDATEWIZARD, OnWebUpdateWizard)
	ON_NOTIFY(NM_CLICK, IDC_TRAYICON, OnTrayIconClick) 
	ON_NOTIFY(NM_DBLCLK, IDC_TRAYICON, OnTrayIconDblClk)
	ON_NOTIFY(NM_MCLICK, IDC_TABCONTROL, OnMBtnClickTabcontrol)
	ON_NOTIFY(NM_RCLICK, IDC_TRAYICON, OnTrayIconRClick)
	ON_NOTIFY(TCN_SELCHANGE, IDC_TABCONTROL, OnSelchangeTabcontrol)
	ON_NOTIFY(TCN_SELCHANGING, IDC_TABCONTROL, OnSelchangingTabcontrol)
//	ON_NOTIFY(TTN_NEEDTEXTA, 0, OnNeedTooltipText)
	ON_NOTIFY(TTN_NEEDTEXTW, 0, OnNeedTooltipText)
	ON_REGISTERED_MESSAGE(WM_ACB_ITEMADDED, OnQuickFindItemAdded)
	ON_REGISTERED_MESSAGE(WM_FBN_FILTERCHNG, OnSelchangeFilter)
	ON_REGISTERED_MESSAGE(WM_FTD_APPLYASFILTER, OnFindApplyAsFilter)
	ON_REGISTERED_MESSAGE(WM_FTD_CLOSE, OnFindDlgClose)
	ON_REGISTERED_MESSAGE(WM_FTD_ADDSEARCH, OnFindAddSearch)
	ON_REGISTERED_MESSAGE(WM_FTD_DELETESEARCH, OnFindDeleteSearch)
	ON_REGISTERED_MESSAGE(WM_FTD_FIND, OnFindDlgFind)
	ON_REGISTERED_MESSAGE(WM_FTD_SELECTALL, OnFindSelectAll)
	ON_REGISTERED_MESSAGE(WM_FTD_SELECTRESULT, OnFindSelectResult)
	ON_REGISTERED_MESSAGE(WM_FW_FOCUSCHANGE, OnFocusChange)
	ON_REGISTERED_MESSAGE(WM_PGP_CLEARMRU, OnPreferencesClearMRU)
	ON_REGISTERED_MESSAGE(WM_PGP_CLEANUPDICTIONARY, OnPreferencesCleanupDictionary)
	ON_REGISTERED_MESSAGE(WM_PTDP_LISTCHANGE, OnPreferencesDefaultListChange)
	ON_REGISTERED_MESSAGE(WM_PTP_TESTTOOL, OnPreferencesTestTool)
	ON_REGISTERED_MESSAGE(WM_TDCM_TASKHASREMINDER, OnToDoCtrlTaskHasReminder)
	ON_REGISTERED_MESSAGE(WM_TDCM_TASKISDONE, OnToDoCtrlTaskIsDone)
	ON_REGISTERED_MESSAGE(WM_TDCM_LENGTHYOPERATION, OnToDoCtrlDoLengthyOperation)
	ON_REGISTERED_MESSAGE(WM_TDCM_TASKLINK, OnToDoCtrlDoTaskLink)
	ON_REGISTERED_MESSAGE(WM_TDCM_FAILEDLINK, OnTodoCtrlFailedLink)
	ON_REGISTERED_MESSAGE(WM_TDCN_DOUBLECLKREMINDERCOL, OnDoubleClkReminderCol)
	ON_REGISTERED_MESSAGE(WM_TDCN_LISTCHANGE, OnToDoCtrlNotifyListChange)
	ON_REGISTERED_MESSAGE(WM_TDCN_MODIFY, OnToDoCtrlNotifyMod)
	ON_REGISTERED_MESSAGE(WM_TDCN_RECREATERECURRINGTASK, OnToDoCtrlNotifyRecreateRecurringTask)
	ON_REGISTERED_MESSAGE(WM_TDCN_TIMETRACK, OnToDoCtrlNotifyTimeTrack)
	ON_REGISTERED_MESSAGE(WM_TDCN_VIEWPOSTCHANGE, OnToDoCtrlNotifyViewChange)
	ON_REGISTERED_MESSAGE(WM_TDL_GETVERSION , OnToDoListGetVersion)
	ON_REGISTERED_MESSAGE(WM_TDL_ISCLOSING , OnToDoListIsClosing)
	ON_REGISTERED_MESSAGE(WM_TDL_REFRESHPREFS , OnToDoListRefreshPrefs)
	ON_REGISTERED_MESSAGE(WM_TDL_RESTORE , OnToDoListRestore)
	ON_REGISTERED_MESSAGE(WM_TDL_SHOWWINDOW , OnToDoListShowWindow)
	ON_REGISTERED_MESSAGE(WM_TD_REMINDER, OnToDoCtrlReminder)
	ON_REGISTERED_MESSAGE(WM_TLDT_DROP, OnDropFile)
	ON_UPDATE_COMMAND_UI(ID_ARCHIVE_COMPLETEDTASKS, OnUpdateArchiveCompletedtasks)
	ON_UPDATE_COMMAND_UI(ID_CLOSEALL, OnUpdateCloseall)
	ON_UPDATE_COMMAND_UI(ID_DELETEALLTASKS, OnUpdateDeletealltasks) 
	ON_UPDATE_COMMAND_UI(ID_DELETETASK, OnUpdateDeletetask)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CLOCK_TASK, OnUpdateEditTimeTrackTask)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_HTML, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYAS_TEXT, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_OPENFILEREF, OnUpdateEditOpenfileref)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTEAFTER, OnUpdateEditPasteAfter)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTESUB, OnUpdateEditPasteSub)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SETFILEREF, OnUpdateEditSetfileref)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SPELLCHECKCOMMENTS, OnUpdateSpellcheckcomments)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SPELLCHECKTITLE, OnUpdateSpellchecktitle)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TASKCOLOR, OnUpdateTaskcolor)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TASKDONE, OnUpdateTaskdone)
	ON_UPDATE_COMMAND_UI(ID_EDIT_TASKTEXT, OnUpdateEditTasktext)
	ON_UPDATE_COMMAND_UI(ID_FILE_ENCRYPT, OnUpdateFileEncrypt)
	ON_UPDATE_COMMAND_UI(ID_FILE_MRU_FILE1, OnUpdateRecentFileMenu)
	ON_UPDATE_COMMAND_UI(ID_FILE_RESETVERSION, OnUpdateFileResetversion)
	ON_UPDATE_COMMAND_UI(ID_MAXCOMMENTS, OnUpdateMaximizeComments)
	ON_UPDATE_COMMAND_UI(ID_MAXTASKLIST, OnUpdateMaximizeTasklist)
	ON_UPDATE_COMMAND_UI(ID_MOVETASKDOWN, OnUpdateMovetaskdown)
	ON_UPDATE_COMMAND_UI(ID_MOVETASKLEFT, OnUpdateMovetaskleft)
	ON_UPDATE_COMMAND_UI(ID_MOVETASKRIGHT, OnUpdateMovetaskright)
	ON_UPDATE_COMMAND_UI(ID_MOVETASKUP, OnUpdateMovetaskup)
	ON_UPDATE_COMMAND_UI(ID_NEW, OnUpdateNew)
	ON_UPDATE_COMMAND_UI(ID_NEWSUBTASK_ATBOTTOM, OnUpdateNewsubtaskAtBottom)
	ON_UPDATE_COMMAND_UI(ID_NEWSUBTASK_ATTOP, OnUpdateNewsubtaskAttop)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK, OnUpdateNewtask)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK_AFTERSELECTEDTASK, OnUpdateNewtaskAfterselectedtask)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK_ATBOTTOM, OnUpdateNewtaskAtbottom)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK_ATBOTTOMSELECTED, OnUpdateNewtaskAtbottomSelected)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK_ATTOP, OnUpdateNewtaskAttop)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK_ATTOPSELECTED, OnUpdateNewtaskAttopSelected)
	ON_UPDATE_COMMAND_UI(ID_NEWTASK_BEFORESELECTEDTASK, OnUpdateNewtaskBeforeselectedtask)
	ON_UPDATE_COMMAND_UI(ID_NEXTTOPLEVELTASK, OnUpdateNexttopleveltask)
	ON_UPDATE_COMMAND_UI(ID_OPEN_RELOAD, OnUpdateReload)
	ON_UPDATE_COMMAND_UI(ID_PREVTOPLEVELTASK, OnUpdatePrevtopleveltask)
	ON_UPDATE_COMMAND_UI(ID_PRINT, OnUpdatePrint)
	ON_UPDATE_COMMAND_UI(ID_SAVEALL, OnUpdateSaveall)
	ON_UPDATE_COMMAND_UI(ID_SAVEAS, OnUpdateSaveas)
	ON_UPDATE_COMMAND_UI(ID_SAVE_NORMAL, OnUpdateSave)
	ON_UPDATE_COMMAND_UI(ID_SB_SELCOUNT, OnUpdateSBSelectionCount)
	ON_UPDATE_COMMAND_UI(ID_SB_TASKCOUNT, OnUpdateSBTaskCount)
	ON_UPDATE_COMMAND_UI(ID_SORT, OnUpdateSort)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_CHECKIN, OnUpdateToolsCheckin)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_CHECKOUT, OnUpdateToolsCheckout)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_EXPORT, OnUpdateExport)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_IMPORT, OnUpdateImport)
 	ON_UPDATE_COMMAND_UI(ID_TOOLS_REMOVEFROMSOURCECONTROL, OnUpdateToolsRemovefromsourcecontrol)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_SPELLCHECKTASKLIST, OnUpdateSpellcheckTasklist)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TOGGLECHECKIN, OnUpdateToolsToggleCheckin)
	ON_UPDATE_COMMAND_UI(ID_TOOLS_TRANSFORM, OnUpdateExport) // use same text as export
	ON_UPDATE_COMMAND_UI(ID_VIEW_MOVETASKLISTLEFT, OnUpdateViewMovetasklistleft)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MOVETASKLISTRIGHT, OnUpdateViewMovetasklistright)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NEXT, OnUpdateViewNext)
	ON_UPDATE_COMMAND_UI(ID_VIEW_NEXT_SEL, OnUpdateViewNextSel)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PREV, OnUpdateViewPrev)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PREV_SEL, OnUpdateViewPrevSel)
	ON_UPDATE_COMMAND_UI(ID_VIEW_REFRESHFILTER, OnUpdateViewRefreshfilter)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR, OnUpdateViewToolbar)
	ON_UPDATE_COMMAND_UI_RANGE(ID_EDIT_SETPRIORITYNONE, ID_EDIT_SETPRIORITY10, OnUpdateSetPriority)	
	ON_UPDATE_COMMAND_UI_RANGE(ID_NEWTASK_SPLITTASKINTO_TWO, ID_NEWTASK_SPLITTASKINTO_FIVE, OnUpdateSplitTaskIntoPieces)
	ON_UPDATE_COMMAND_UI_RANGE(ID_SORT_BYFIRST, ID_SORT_BYLAST, OnUpdateSortBy)
//	ON_UPDATE_COMMAND_UI_RANGE(ID_TOOLS_USEREXT1, ID_TOOLS_USEREXT16, OnUpdateUserUIExtension)
	ON_UPDATE_COMMAND_UI_RANGE(ID_TOOLS_USERTOOL1, ID_TOOLS_USERTOOL16, OnUpdateUserTool)
	ON_WM_CLOSE()
	ON_WM_CONTEXTMENU()
	ON_WM_COPYDATA()
	ON_WM_CREATE()
	ON_WM_DRAWITEM()
	ON_WM_ERASEBKGND()
	ON_WM_HELPINFO()
	ON_WM_INITMENUPOPUP()
	ON_WM_ENDSESSION()
	ON_WM_SIZE()
	ON_WM_SYSCOMMAND()
	ON_WM_TIMER()
	
#ifdef _DEBUG
	ON_COMMAND(ID_DEBUGENDSESSION, OnDebugEndSession)
	ON_COMMAND(ID_DEBUGSHOWSETUPDLG, OnDebugShowSetupDlg)
#endif

END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CToDoListWnd message handlers

void CToDoListWnd::SetupUIStrings()
{
	// set up UI strings for helper classes
	CTimeEdit::SetUnits(THU_MINS, CEnString(IDS_TE_MINS), CEnString(IDS_MINS_ABBREV));
	CTimeEdit::SetUnits(THU_HOURS, CEnString(IDS_TE_HOURS), CEnString(IDS_HOURS_ABBREV));
	CTimeEdit::SetUnits(THU_DAYS, CEnString(IDS_TE_DAYS), CEnString(IDS_DAYS_ABBREV));
	CTimeEdit::SetUnits(THU_WEEKS, CEnString(IDS_TE_WEEKS), CEnString(IDS_WEEKS_ABBREV));
	CTimeEdit::SetUnits(THU_MONTHS, CEnString(IDS_TE_MONTHS), CEnString(IDS_MONTHS_ABBREV));
	CTimeEdit::SetUnits(THU_YEARS, CEnString(IDS_TE_YEARS), CEnString(IDS_YEARS_ABBREV));
	CTimeEdit::SetDefaultButtonTip(CEnString(IDS_TIMEUNITS));

	CFileEdit::SetDefaultButtonTips(CEnString(IDS_BROWSE), CEnString(IDS_VIEW));
	CFileEdit::SetDefaultBrowseTitles(CEnString(IDS_BROWSEFILE_TITLE), CEnString(IDS_BROWSEFOLDER_TITLE));

	CTDLRecurringTaskEdit::SetDefaultButtonTip(CEnString(IDS_OPTIONS));

	CXmlFileEx::SetUIStrings(CEnString(IDS_ENCRYPTEDFILE), CEnString(IDS_DECRYPTFAILED));

/*
	CServerDlg::SetItemText(SD_TITLE, IDS_SCD_TITLE);
	CServerDlg::SetItemText(IDC_SD_SERVERLABEL, IDS_SD_SERVERLABEL);
	CServerDlg::SetItemText(IDC_SD_USERNAMELABEL, IDS_SD_USERNAMELABEL);
	CServerDlg::SetItemText(IDC_SD_PASSWORDLABEL, IDS_SD_PASSWORDLABEL);
	CServerDlg::SetItemText(IDC_SD_ANONLOGIN, IDS_SD_ANONLOGIN);
	CServerDlg::SetItemText(IDOK, IDS_OK);
	CServerDlg::SetItemText(IDCANCEL, IDS_CANCEL);


	CPasswordDialog::SetItemText(PD_TITLE, IDS_PD_TITLE);
	CPasswordDialog::SetItemText(IDC_PD_PASSWORDLABEL, IDS_PD_PASSWORDLABEL);
	CPasswordDialog::SetItemText(IDC_PD_CONFIRMLABEL, IDS_PD_CONFIRMLABEL);
	CPasswordDialog::SetItemText(DLG_PD_CONFIRMFAILED, IDS_PD_CONFIRMFAILED);
	CPasswordDialog::SetItemText(IDOK, IDS_OK);
	CPasswordDialog::SetItemText(IDCANCEL, IDS_CANCEL);

	CSpellCheckDlg::SetItemText(IDC_SCD_DICTLABEL, IDS_SCD_DICTLABEL);
	CSpellCheckDlg::SetItemText(IDC_SCD_BROWSE, IDS_SCD_BROWSE);
	CSpellCheckDlg::SetItemText(IDC_SCD_URL, IDS_SCD_URL);
	CSpellCheckDlg::SetItemText(IDC_SCD_CHECKINGLABEL, IDS_SCD_CHECKINGLABEL);
	CSpellCheckDlg::SetItemText(IDC_SCD_RESTART, IDS_SCD_RESTART);
	CSpellCheckDlg::SetItemText(IDC_SCD_REPLACELABEL, IDS_SCD_REPLACELABEL);
	CSpellCheckDlg::SetItemText(IDC_SCD_WITHLABEL, IDS_SCD_WITHLABEL);
	CSpellCheckDlg::SetItemText(IDC_SCD_REPLACE, IDS_SCD_REPLACE);
	CSpellCheckDlg::SetItemText(IDC_SCD_NEXT, IDS_SCD_NEXT);
	CSpellCheckDlg::SetItemText(IDOK, IDS_OK);
	CSpellCheckDlg::SetItemText(IDCANCEL, IDS_CANCEL);
	CSpellCheckDlg::SetItemText(SCD_TITLE, IDS_SCD_TITLE);
	CSpellCheckDlg::SetItemText(DLG_SCD_SETUPMSG, IDS_SCD_SETUPMSG);
	CSpellCheckDlg::SetItemText(DLG_SCD_DICTFILTER, IDS_SCD_DICTFILTER);
	CSpellCheckDlg::SetItemText(DLG_SCD_ENGINEFILTER, IDS_SCD_ENGINEFILTER);
	CSpellCheckDlg::SetItemText(DLG_SCD_ENGINETITLE, IDS_SCD_ENGINETITLE);
*/
	CSpellCheckDlg::SetItemText(DLG_SCD_BROWSETITLE, IDS_SCD_BROWSETITLE);
}

BOOL CToDoListWnd::OnHelpInfo(HELPINFO* /*pHelpInfo*/)
{
	// always eat this because we handle the F1 ourselves
	return FALSE;
}

void CToDoListWnd::SetUITheme(const CString& sThemeFile)
{
	if (COSVersion() < OSV_XP)
		return;

	// cache existing theme
	CUIThemeFile themeCur = m_theme;

	if (CThemed::IsThemeActive() && m_theme.LoadThemeFile(sThemeFile)) 
		m_sThemeFile = sThemeFile;
	else
	{
		m_sThemeFile.Empty();
		m_theme.Reset();
	}
	
	// update the UI
	if (themeCur != m_theme)
	{
		m_cbQuickFind.DestroyWindow();
		m_toolbar.DestroyWindow();
		m_tbHelper.Release();

		InitToolbar();

		// reinitialize the menu icon manager
		m_mgrMenuIcons.Release();
		InitMenuIconManager();
	}
	else
		m_toolbar.SetBackgroundColors(m_theme.crToolbarLight, 
										m_theme.crToolbarDark, 
										m_theme.HasGradient(), 
										m_theme.HasGlass());

	m_statusBar.SetUIColors(m_theme.crStatusBarLight, 
							m_theme.crStatusBarDark, 
							m_theme.crStatusBarText, 
							m_theme.HasGradient(), 
							m_theme.HasGlass());

	m_menubar.SetBackgroundColor(m_theme.crMenuBack);
	m_filterBar.SetUIColors(m_theme.crAppBackLight, m_theme.crAppText);
	m_tabCtrl.SetBackgroundColor(m_theme.crAppBackDark);

	for (int nCtl = 0; nCtl < GetTDCCount(); nCtl++)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtl);
		tdc.SetUITheme(m_theme);
	}

	if (m_findDlg.GetSafeHwnd())
		m_findDlg.SetUITheme(m_theme);

	DrawMenuBar();
	Invalidate();
}

BOOL CToDoListWnd::Create(const TDCSTARTUP& startup)
{
	m_startupOptions = startup;
	m_bVisible = startup.HasFlag(TLD_FORCEVISIBLE) ? 1 : -1;
	m_bUseStagingScript = startup.HasFlag(TLD_STAGING);

#ifdef _DEBUG
	m_bPasswordPrompting = FALSE;
#else
	m_bPasswordPrompting = startup.HasFlag(TLD_PASSWORDPROMPTING);
#endif

	FileMisc::EnableLogging(startup.HasFlag(TLD_LOGGING), TRUE, FALSE);
	
	return CFrameWnd::LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, NULL, NULL);
}

int CToDoListWnd::GetVersion()
{
	return TD_VERSION;
}

int CToDoListWnd::MessageBox(UINT nIDText, UINT nIDCaption, UINT nType, LPCTSTR szData)
{
	if (szData && *szData)
		return MessageBox(CEnString(nIDText, szData), nIDCaption, nType);
	else
		return MessageBox(CEnString(nIDText), nIDCaption, nType);
}

int CToDoListWnd::MessageBox(const CString& sText, UINT nIDCaption, UINT nType)
{
	return MessageBox(sText, CEnString(nIDCaption), nType);
}

int CToDoListWnd::MessageBox(const CString& sText, const CString& sCaption, UINT nType)
{
	CString sMessage;

	if (!sCaption.IsEmpty())
		sMessage.Format(_T("%s|%s"), sCaption, sText);
	else
		sMessage = sText;

	return AfxMessageBox(sMessage, nType);
}

int CToDoListWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	// set frame icon
	HICON hIcon = GraphicsMisc::LoadIcon(IDR_MAINFRAME);
	SetIcon(hIcon, FALSE);
		
	// set taskbar icon
	hIcon = GraphicsMisc::LoadIcon(IDR_MAINFRAME, 32);
	SetIcon(hIcon, TRUE);

	SetWindowPos(NULL, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOSIZE | SWP_NOZORDER | SWP_NOMOVE);

	// menu
	if (!LoadMenubar())
		return -1;

	// drop target
	if (!m_dropTarget.Register(this, this))
		return -1;

	// trayicon
	// we always create the trayicon (for simplicity) but we only
	// show it if required
	BOOL bUseSysTray = Prefs().GetUseSysTray();
	
	m_trayIcon.Create(WS_CHILD | (bUseSysTray ? WS_VISIBLE : 0), 
						this, 
						IDC_TRAYICON, 
						IDI_TRAY_STD, 
						CString(IDS_COPYRIGHT));
	
	// toolbar
	if (!InitToolbar())
		return -1;

	// statusbar
	if (!InitStatusbar())
		return -1;

	// filterbar
	if (!InitFilterbar())
		return -1;
	
	// tabctrl
	if (!m_tabCtrl.Create(WS_CHILD | WS_VISIBLE | TCS_HOTTRACK | TCS_TABS | TCS_SINGLELINE | TCS_RIGHTJUSTIFY | TCS_TOOLTIPS, 
							CRect(0, 0, 10, 10), this, IDC_TABCONTROL))
		return -1;

	m_tabCtrl.GetToolTips()->ModifyStyle(0, TTS_ALWAYSTIP);
	CLocalizer::EnableTranslation(m_tabCtrl, FALSE);

	BOOL bStackTabbar = Prefs().GetStackTabbarItems();
	
	m_tabCtrl.ModifyStyle(bStackTabbar ? 0 : TCS_MULTILINE, bStackTabbar ? TCS_MULTILINE : 0);
	UpdateTabSwitchTooltip();
	
	if (m_ilTabCtrl.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 1))
	{
		CBitmap bm;
		bm.LoadBitmap(IDB_SOURCECONTROL_STD);
		m_ilTabCtrl.Add(&bm, RGB(255, 0, 255));
		m_tabCtrl.SetImageList(&m_ilTabCtrl);
	}
	else
		return -1;
	
	// UI Font
	InitUIFont();

	LoadSettings();

	// add a barebones tasklist while we're still hidden
	if (!CreateNewTaskList(FALSE))
		return -1;

	// timers
	SetTimer(TIMER_DUEITEMS, TRUE);
	SetTimer(TIMER_TIMETRACKING, TRUE);

	// notify users about dodgy content plugins
	if (m_mgrContent.SomePluginsHaveBadversions())
	{
		if (MessageBox(IDS_BADPLUGINVERSIONS, IDS_BADPLUGINTITLE, MB_OKCANCEL) == IDCANCEL)
			return -1;
	}

	// inherited parent task attributes for new tasks
	CTDCAttributeArray aParentAttrib;
	BOOL bUpdateAttrib;

	Prefs().GetParentAttribsUsed(aParentAttrib, bUpdateAttrib);
	CFilteredToDoCtrl::SetInheritedParentAttributes(aParentAttrib, bUpdateAttrib);

	// theme
	SetUITheme(Prefs().GetUITheme());
				
	// late initialization
	PostMessage(WM_POSTONCREATE);
	
	return 0; // success
}

void CToDoListWnd::InitUIFont()
{
	GraphicsMisc::VerifyDeleteObject(m_fontMain);

	HFONT hFontUI = GraphicsMisc::CreateFont(_T("Tahoma"), 8);

	if (m_fontMain.Attach(hFontUI))
		CDialogHelper::SetFont(this, m_fontMain); // will update all child controls
}

void CToDoListWnd::InitShortcutManager()
{
	// setup defaults first
	m_mgrShortcuts.AddShortcut(ID_LOAD_NORMAL, 'O', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_OPEN_RELOAD, 'R', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_SAVE_NORMAL, 'S', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_NEWTASK_BEFORESELECTEDTASK, 'N', HOTKEYF_CONTROL | HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_NEWTASK_AFTERSELECTEDTASK, 'N', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_NEWSUBTASK_ATBOTTOM, 'N', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_MAXTASKLIST, 'M', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_MAXCOMMENTS, 'M', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_PRINT, 'P', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_VIEW_NEXT, VK_TAB, HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_VIEW_PREV, VK_TAB, HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_CUT, 'X', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_COPY, 'C', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_PASTEAFTER, 'V', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_PASTESUB, 'V', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_INSERTDATETIME, 'D', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_OPENFILEREF, 'G', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EXIT, VK_F4, HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_CLOSE, VK_F4, HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_MOVETASKDOWN, VK_DOWN, HOTKEYF_CONTROL | HOTKEYF_EXT);
	m_mgrShortcuts.AddShortcut(ID_MOVETASKUP, VK_UP, HOTKEYF_CONTROL | HOTKEYF_EXT);
	m_mgrShortcuts.AddShortcut(ID_MOVETASKLEFT, VK_LEFT, HOTKEYF_CONTROL | HOTKEYF_EXT);
	m_mgrShortcuts.AddShortcut(ID_MOVETASKRIGHT, VK_RIGHT, HOTKEYF_CONTROL | HOTKEYF_EXT);
	m_mgrShortcuts.AddShortcut(ID_DELETETASK, VK_DELETE, HOTKEYF_EXT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_TASKTEXT, VK_F2, 0);
	m_mgrShortcuts.AddShortcut(ID_EDIT_TASKDONE, VK_SPACE, HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_TOOLS_IMPORT, 'I', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_TOOLS_EXPORT, 'E', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_HELP, VK_F1, 0);
	m_mgrShortcuts.AddShortcut(ID_WINDOW1, '1', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW2, '2', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW3, '3', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW4, '4', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW5, '5', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW6, '6', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW7, '7', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW8, '8', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_WINDOW9, '9', HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_TOOLS_TRANSFORM, 'T', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_FINDTASKS, 'F', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_QUICKFIND, 'Q', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_QUICKFINDNEXT, VK_F3, 0);
	m_mgrShortcuts.AddShortcut(ID_EDIT_QUICKFINDPREV, VK_F3, HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_VIEW_REFRESHFILTER, VK_F5, HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_VIEW_TOGGLEFILTER, VK_F12, 0);
	m_mgrShortcuts.AddShortcut(ID_EDIT_SELECTALL, 'A', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_PREVTASK, VK_F7, 0);
	m_mgrShortcuts.AddShortcut(ID_NEXTTASK, VK_F8, 0);
	m_mgrShortcuts.AddShortcut(ID_EDIT_UNDO, 'Z', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_REDO, 'Y', HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_VIEW_TOGGLETREEANDLIST, VK_F10, 0);
	m_mgrShortcuts.AddShortcut(ID_VIEW_CYCLETASKVIEWS, VK_F10, HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_VIEW_TOGGLETASKSANDCOMMENTS, VK_F11, 0);
	m_mgrShortcuts.AddShortcut(ID_VIEW_TOGGLETASKEXPANDED, VK_SPACE, HOTKEYF_CONTROL | HOTKEYF_ALT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_INCTASKPERCENTDONE, VK_ADD, HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_EDIT_DECTASKPERCENTDONE, VK_SUBTRACT, HOTKEYF_CONTROL);
	m_mgrShortcuts.AddShortcut(ID_VIEW_PREV_SEL, VK_LEFT, HOTKEYF_ALT | HOTKEYF_EXT);
	m_mgrShortcuts.AddShortcut(ID_VIEW_NEXT_SEL, VK_RIGHT, HOTKEYF_ALT | HOTKEYF_EXT);
	
	// now init shortcut mgr which will override the defaults
	// with the users actual settings
	CPreferences prefs;

	if (m_mgrShortcuts.Initialize(this, &prefs))
	{
		// fix for previously adding escape key as a shortcut for IDCLOSE 
		// (big mistake)
		if (m_mgrShortcuts.GetShortcut(IDCLOSE) == VK_ESCAPE)
			m_mgrShortcuts.DeleteShortcut(IDCLOSE);

		// fix for paste being wrongly set up
		if (m_mgrShortcuts.GetShortcut(ID_EDIT_PASTE))
		{
			// delete existing
			m_mgrShortcuts.DeleteShortcut(ID_EDIT_PASTE);

			// if nothing already assigned use Ctrl+V
			if (!m_mgrShortcuts.GetShortcut(ID_EDIT_PASTESUB))
				m_mgrShortcuts.AddShortcut(ID_EDIT_PASTESUB, 'V', HOTKEYF_CONTROL);
		}
	}
}

void CToDoListWnd::InitMenuIconManager()
{
	if (!m_mgrMenuIcons.Initialize(this))
		return;
	
	m_mgrMenuIcons.ClearImages();
	
	// images
	UINT nToolbarImageID = IDB_APP_TOOLBAR_STD;
	UINT nExtraImageID = IDB_APP_EXTRA_STD;
				
	CUIntArray aCmdIDs;
	
	// toolbar
	aCmdIDs.Add(ID_LOAD_NORMAL);
	aCmdIDs.Add(ID_SAVE_NORMAL);
	aCmdIDs.Add(ID_SAVEALL);
	
	// new tasks
	aCmdIDs.Add(GetNewTaskCmdID());
	aCmdIDs.Add(GetNewSubtaskCmdID());
	
	aCmdIDs.Add(ID_EDIT_TASKTEXT);
	aCmdIDs.Add(ID_EDIT_SETTASKICON);
	aCmdIDs.Add(ID_EDIT_SETREMINDER);
	aCmdIDs.Add(ID_EDIT_UNDO);
	aCmdIDs.Add(ID_EDIT_REDO);
	aCmdIDs.Add(ID_MAXTASKLIST);
	aCmdIDs.Add(ID_VIEW_EXPANDTASK);
	aCmdIDs.Add(ID_VIEW_COLLAPSETASK);
	aCmdIDs.Add(ID_VIEW_PREV_SEL);
	aCmdIDs.Add(ID_VIEW_NEXT_SEL);
	aCmdIDs.Add(ID_EDIT_FINDTASKS);
	aCmdIDs.Add(ID_SORT);
	aCmdIDs.Add(ID_DELETETASK);
	
	// source control
	if (GetTDCCount() && m_mgrToDoCtrls.PathSupportsSourceControl(GetSelToDoCtrl()))
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl();

		if (tdc.IsCheckedOut())
			aCmdIDs.Add(ID_TOOLS_CHECKIN);
		else
			aCmdIDs.Add(ID_TOOLS_CHECKOUT);
	}
	else
		aCmdIDs.Add(ID_TOOLS_TOGGLECHECKIN);
		
	aCmdIDs.Add(ID_PREFERENCES);

	if (m_theme.HasToolbarImageFile(_T("TODOLIST")))
	{
		COLORREF crMask = CLR_NONE;
		CString sImagePath = m_theme.GetToolbarImageFile(_T("TODOLIST"), crMask);

		VERIFY(m_mgrMenuIcons.AddImages(aCmdIDs, sImagePath, 16, crMask));
	}
	else
		m_mgrMenuIcons.AddImages(aCmdIDs, nToolbarImageID, 16, RGB(255, 0, 255));

	// extra
	aCmdIDs.RemoveAll();

  	aCmdIDs.Add(ID_HELP_WIKI);
  	aCmdIDs.Add(ID_HELP_DONATE);
  	aCmdIDs.Add(ID_HELP);

	if (m_theme.HasToolbarImageFile(_T("TODOLIST_EXTRA")))
	{
		COLORREF crMask = CLR_NONE;
		CString sImagePath = m_theme.GetToolbarImageFile(_T("TODOLIST_EXTRA"), crMask);

		VERIFY(m_mgrMenuIcons.AddImages(aCmdIDs, sImagePath, 16, crMask));
	}
	else
		m_mgrMenuIcons.AddImages(aCmdIDs, nExtraImageID, 16, RGB(255, 0, 255));
}

void CToDoListWnd::OnShowKeyboardshortcuts() 
{
	CStringArray aMapping;

	if (m_mgrShortcuts.BuildMapping(IDR_MAINFRAME, aMapping, '|'))
	{
		// add a few misc items that don't appear in the menus
		CString sMisc;

		for (int nItem = 0; nItem < NUM_MISCSHORTCUTS; nItem++)
		{
			if (MISC_SHORTCUTS[nItem].dwShortcut)
				sMisc.Format(_T("%s|%s"), m_mgrShortcuts.GetShortcutText(MISC_SHORTCUTS[nItem].dwShortcut), 
									CEnString(MISC_SHORTCUTS[nItem].nIDShortcut));
			else
				sMisc.Empty();

			aMapping.Add(sMisc);
		}
	
		CKeyboardShortcutDisplayDlg dialog(aMapping, '|');

		dialog.DoModal();
	}
}

LRESULT CToDoListWnd::OnFocusChange(WPARAM wp, LPARAM /*lp*/)
{
	if (m_statusBar.GetSafeHwnd() && IsWindowEnabled() && GetTDCCount() && wp)
	{
		// grab the previous window in the z-order and if its
		// static text then use that as the focus hint
		CWnd* pFocus = CWnd::FromHandle((HWND)wp);
		const CFilteredToDoCtrl& tdc = GetToDoCtrl();
		m_sCurrentFocus.Empty();

		if (CDialogHelper::IsChildOrSame(tdc.GetSafeHwnd(), (HWND)wp))
		{
			m_sCurrentFocus.LoadString(IDS_FOCUS_TASKS);
			m_sCurrentFocus += ": ";
			m_sCurrentFocus += tdc.GetControlDescription(pFocus);
		}
		else if (pFocus == m_cbQuickFind.GetWindow(GW_CHILD))
		{
			m_sCurrentFocus.LoadString(IDS_QUICKFIND);
		}
		else
		{
			if (m_findDlg.GetSafeHwnd() && m_findDlg.IsChild(pFocus))
			{
				m_sCurrentFocus.LoadString(IDS_FINDTASKS);
			}
			else if (m_filterBar.GetSafeHwnd() && m_filterBar.IsChild(pFocus))
			{
				m_sCurrentFocus.LoadString(IDS_FOCUS_FILTERBAR);
			}
			
			if (!m_sCurrentFocus.IsEmpty())
				m_sCurrentFocus += ": ";
			
			m_sCurrentFocus += GetControlLabel(pFocus);
		}		

		// limit length of string
		if (m_sCurrentFocus.GetLength() > 22)
			m_sCurrentFocus = m_sCurrentFocus.Left(20) + _T("...");

		m_statusBar.SetPaneText(m_statusBar.CommandToIndex(ID_SB_FOCUS), m_sCurrentFocus);
		
		// if the status bar is hidden then add text to title bar
		if (!m_bShowStatusBar)
			UpdateCaption();
	}

	return 0L;
}

LRESULT CToDoListWnd::OnGetIcon(WPARAM bLargeIcon, LPARAM /*not used*/)
{
	if (!bLargeIcon)
	{
		// cache small icon for reuse
		if (!m_hIcon)
			m_hIcon = CSysImageList(FALSE).ExtractAppIcon();
		
		return (LRESULT)m_hIcon;
	}
	else
		return Default();
}

BOOL CToDoListWnd::InitStatusbar()
{
	static SBACTPANEINFO SB_PANES[] = 
	{
	  { ID_SB_FILEPATH,		MAKEINTRESOURCE(IDS_SB_FILEPATH_TIP), SBACTF_STRETCHY | SBACTF_RESOURCETIP }, 
	  { ID_SB_FILEVERSION,	MAKEINTRESOURCE(IDS_SB_FILEVERSION_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	  { ID_SB_TASKCOUNT,	MAKEINTRESOURCE(IDS_SB_TASKCOUNT_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	  //{ ID_SB_SPACER }, 
	  { ID_SB_SELCOUNT,		MAKEINTRESOURCE(0), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	  { ID_SB_SELTIMEEST,	MAKEINTRESOURCE(IDS_SB_SELTIMEEST_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	  { ID_SB_SELTIMESPENT,	MAKEINTRESOURCE(IDS_SB_SELTIMESPENT_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	  { ID_SB_SELCOST,		MAKEINTRESOURCE(IDS_SB_SELCOST_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	  //{ ID_SB_SPACER }, 
	  { ID_SB_FOCUS,		MAKEINTRESOURCE(IDS_SB_FOCUS_TIP), SBACTF_AUTOFIT | SBACTF_RESOURCETIP }, 
	};

	static int SB_PANECOUNT = sizeof(SB_PANES) / sizeof(SBACTPANEINFO);

	if (!m_statusBar.Create(this, WS_CHILD | WS_VISIBLE | CBRS_BOTTOM, IDC_FILENAME))
		return FALSE;

	// prevent translation because we handle it manually
	CLocalizer::EnableTranslation(m_statusBar, FALSE);

	if (!m_statusBar.SetPanes(SB_PANES, SB_PANECOUNT))
		return FALSE;

	return TRUE;
}

BOOL CToDoListWnd::InitFilterbar()
{
	if (!m_filterBar.Create(this))
		return FALSE;

	m_filterBar.EnableMultiSelection(Prefs().GetMultiSelFilters());
	m_filterBar.ShowDefaultFilters(Prefs().GetShowDefaultFilters());

	RefreshFilterBarCustomFilters();

	return TRUE;
}

BOOL CToDoListWnd::InitToolbar()
{
	if (m_toolbar.GetSafeHwnd())
		return TRUE;

	UINT nStyle = WS_CHILD | CBRS_ALIGN_TOP | WS_CLIPCHILDREN | CBRS_TOOLTIPS;

	if (m_bShowToolbar)
		nStyle |= WS_VISIBLE;

	if (!m_toolbar.CreateEx(this, TBSTYLE_FLAT | TBSTYLE_WRAPABLE, nStyle))
		return FALSE;

	if (!m_toolbar.LoadToolBar(IDR_APP_TOOLBAR))
		return FALSE;

	// colors
	if (CThemed::IsThemeActive())
	{
		m_toolbar.SetBackgroundColors(m_theme.crToolbarLight, 
												m_theme.crToolbarDark, 
												m_theme.HasGradient(), 
												m_theme.HasGlass());
	}
	
	// toolbar images
	if (m_theme.HasToolbarImageFile(_T("TODOLIST")))
	{
		COLORREF crMask = CLR_NONE;
		CString sImagePath = m_theme.GetToolbarImageFile(_T("TODOLIST"), crMask);

		VERIFY(m_toolbar.SetImage(sImagePath, crMask));
	}
	else 
	{
		const COLORREF MAGENTA = RGB(255, 0, 255);
		m_toolbar.SetImage(IDB_APP_TOOLBAR_STD, MAGENTA);
	}
	
	// resize the toolbar in one row so that our subsequent calculations work
	m_toolbar.GetToolBarCtrl().HideButton(ID_TOOLS_TOGGLECHECKIN, !Prefs().GetEnableSourceControl());
	m_toolbar.MoveWindow(0, 0, 1000, 32); 
	
	// insert combobox for quick Find after Find Tasks button
	int nPos = m_toolbar.CommandToIndex(ID_EDIT_FINDTASKS) + 1;
	
	TBBUTTON tbbQuickFind = { 0, nPos, 0, TBSTYLE_SEP, 0, NULL };
	TBBUTTON tbbSep = { 0, nPos + 1, 0, TBSTYLE_SEP, 0, NULL };
	
	m_toolbar.GetToolBarCtrl().InsertButton(nPos, &tbbQuickFind);
	m_toolbar.GetToolBarCtrl().InsertButton(nPos + 1, &tbbSep);
	
	TBBUTTONINFO tbi;
	tbi.cbSize = sizeof( TBBUTTONINFO );
	tbi.cx = 150;
	tbi.dwMask = TBIF_SIZE;  // By index
	
	m_toolbar.GetToolBarCtrl().SetButtonInfo(nPos + 1, &tbi);
	
	CRect rect;
	m_toolbar.GetToolBarCtrl().GetItemRect(nPos + 1, &rect);
	rect.bottom += 200;
	
	if (!m_cbQuickFind.Create(WS_CHILD | WS_VSCROLL | WS_VISIBLE | CBS_AUTOHSCROLL | 
		CBS_DROPDOWN, rect, &m_toolbar, IDC_QUICKFIND))
		return FALSE;
	
	m_cbQuickFind.SetFont(&m_fontMain);
	m_mgrPrompts.SetComboEditPrompt(m_cbQuickFind, IDS_QUICKFIND);
	
	m_tbHelper.Initialize(&m_toolbar, this);
	
	return TRUE;
}

void CToDoListWnd::OnEditChangeQuickFind()
{
	m_cbQuickFind.GetWindowText(m_sQuickFind);
	
	if (!GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTNEXTINCLCURRENT))
		GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTFIRST);
}

void CToDoListWnd::OnSelChangeQuickFind()
{
	int nSel = m_cbQuickFind.GetCurSel();

	if (nSel != CB_ERR)
	{
		m_sQuickFind = CDialogHelper::GetSelectedItem(m_cbQuickFind);
		
		if (!GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTNEXT))
			GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTFIRST);
	}
}

BOOL CToDoListWnd::PreTranslateMessage(MSG* pMsg)
{
	// the only way we get a WM_CLOSE here is if it was sent from an external app
	// so we shut down as gracefully as possible
	if (pMsg->message == WM_CLOSE && IsWindowEnabled())
	{
		DoExit();
		return TRUE;
	}
	
	if (ProcessDialogControlShortcut(pMsg))
		return TRUE;

	if (IsDroppedComboBox(pMsg->hwnd))
		return FALSE;
	
	// process for app level shortcuts first so we can handle
	// reserved shortcuts
	DWORD dwShortcut = 0;
	UINT nCmdID = m_mgrShortcuts.ProcessMessage(pMsg, &dwShortcut);
	
	// if it's a reserved shortcut let's notify the user to change it
	if (CFilteredToDoCtrl::IsReservedShortcut(dwShortcut))
	{
		int nRet = MessageBox(IDS_RESERVEDSHORTCUT_MSG, IDS_RESERVEDSHORTCUT_TITLE, MB_YESNOCANCEL);
		
		if (nRet == IDYES)
			DoPreferences(PREFPAGE_SHORTCUT);
		
		// and keep eating it until the user changes it
		return TRUE;
	}
	
	// also we handle undo/redo
	if (nCmdID != ID_EDIT_UNDO && nCmdID != ID_EDIT_REDO)
	{
		// now try active task list
		if (GetTDCCount() && GetToDoCtrl().PreTranslateMessage(pMsg))
			return TRUE;
	}
	
	if (nCmdID)
	{
		BOOL bSendMessage = TRUE; // default
		
		// focus checks
		switch (nCmdID)
		{
		case ID_EDIT_CUT:
		case ID_EDIT_COPY:
			// tree must have the focus
			if (!GetToDoCtrl().TasksHaveFocus())
			{
				bSendMessage = FALSE;
				GetToDoCtrl().ClearCopiedItem();
			}
			break;
			
			// tree must have the focus
		case ID_EDIT_SELECT_ALL: 
		case ID_EDIT_PASTE: 
		case ID_DELETEALLTASKS:
		case ID_DELETETASK:
			bSendMessage = GetToDoCtrl().TasksHaveFocus();
			break;
		}
		
		// send off
		if (bSendMessage)
		{
			SendMessage(WM_COMMAND, nCmdID);
			return TRUE;
		}
	}
	
	// we need to check for <escape>, <tab> and <return>
	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		{
			switch (pMsg->wParam)
			{
			case VK_ESCAPE:
				if (Prefs().GetEscapeMinimizes() && GetCapture() == NULL)
				{
					// if the window with the target is either a combobox or
					// the child edit of a combobox and the combo is
					// dropped down then let it thru else if the target is
					// a child of ours then treat as a cancel
					BOOL bHandle = TRUE;
					
					if (CWinClasses::IsClass(pMsg->hwnd, WC_COMBOBOX))
						bHandle = !ComboBox_GetDroppedState(pMsg->hwnd);
					
					else if (CWinClasses::IsClass(::GetParent(pMsg->hwnd), WC_COMBOBOX))
						bHandle = !ComboBox_GetDroppedState(::GetParent(pMsg->hwnd));
					
					else if (GetTDCCount() && GetToDoCtrl().IsTaskLabelEditing())
						bHandle = FALSE;
					
					if (bHandle && ::IsChild(*this, pMsg->hwnd))
					{
						OnCancel();
						return TRUE;
					}
				}
				break;
				
			case VK_TAB: // tabbing away from Quick Find -> tasks
				if (::IsChild(m_cbQuickFind, pMsg->hwnd))
				{
					GetToDoCtrl().SetFocusToTasks();
					return TRUE;
				}
				break;
				
			case VK_RETURN: // hitting return in filter bar and quick find
				if (Prefs().GetFocusTreeOnEnter())
				{
					CWnd* pFocus = GetFocus();
					
					if (pFocus && (m_filterBar.IsChild(pFocus) || m_cbQuickFind.IsChild(pFocus)))
					{
						if (!ControlWantsEnter(*pFocus))
							GetToDoCtrl().SetFocusToTasks();

						return FALSE; // continue routing
					}
				}
				break;
			}
		}
		break;
	}
	
	return CFrameWnd::PreTranslateMessage(pMsg);
}

void CToDoListWnd::OnCancel()
{
	ASSERT (Prefs().GetEscapeMinimizes());

	// if the close button has been configured to Minimize to tray
	// then do that here else normal minimize 
	int nOption = Prefs().GetSysTrayOption();
	
	if (nOption == STO_ONMINCLOSE || nOption == STO_ONCLOSE)
		MinimizeToTray();
	else
		SendMessage(WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void CToDoListWnd::OnDeleteTask() 
{
	if (GetToDoCtrl().GetSelectedItem())
		GetToDoCtrl().DeleteSelectedTask();
}

void CToDoListWnd::OnDeleteAllTasks() 
{
	if (GetToDoCtrl().DeleteAllTasks())
	{
//		m_mgrToDoCtrls.ClearFilePath(GetSelToDoCtrl()); // this will ensure that the user must explicitly overwrite the original file
		UpdateStatusbar();
	}
}

void CToDoListWnd::OnSave() 
{
	if (SaveTaskList(GetSelToDoCtrl()) == TDCO_SUCCESS)
		UpdateCaption();
}

BOOL CToDoListWnd::DoBackup(int nIndex)
{
	if (!Prefs().GetBackupOnSave())
		return TRUE;

	CString sTDLPath = m_mgrToDoCtrls.GetFilePath(nIndex);

	if (sTDLPath.IsEmpty())
		return TRUE; // not yet saved

	// get backup path
	CString sBackupFolder = Prefs().GetBackupLocation(sTDLPath);
	sBackupFolder.TrimRight();

	// cull old backups
	int nKeepBackups = Prefs().GetKeepBackupCount();

	if (nKeepBackups)
	{
		CStringArray aFiles;
		CString sPath = CFileBackup::BuildBackupPath(sTDLPath, sBackupFolder, FBS_APPVERSION, _T(""));

		CString sDrive, sFolder, sFName, sExt, sPattern;

		FileMisc::SplitPath(sPath, &sDrive, &sFolder, &sFName, &sExt);
		FileMisc::MakePath(sPath, sDrive, sFolder);
		
		int nFiles = FileMisc::FindFiles(sPath, aFiles, FALSE, sFName + _T("*") + sExt);

		if (nFiles >= nKeepBackups)
		{
			Misc::SortArray(aFiles); // sorts oldest backups first

			// cull as required
			while (aFiles.GetSize() >= nKeepBackups)
			{
				DeleteFile(aFiles[0]);
				aFiles.RemoveAt(0);
			}
		}
	}

	CFileBackup backup;
	return backup.MakeBackup(sTDLPath, sBackupFolder, FBS_APPVERSION | FBS_TIMESTAMP, _T(""));
}

TDC_FILE CToDoListWnd::SaveTaskList(int nIndex, LPCTSTR szFilePath, BOOL bAuto)
{
	CAutoFlag af(m_bSaving, TRUE);
	CString sFilePath = szFilePath ? szFilePath : m_mgrToDoCtrls.GetFilePath(nIndex);

	CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);
	tdc.Flush();

	// build dialog title, incorporating tasklist name
	CString sName = m_mgrToDoCtrls.GetFriendlyProjectName(nIndex);
	CEnString sTitle(IDS_SAVETASKLIST_TITLE, sName);
	
	// conditions for saving
	// 1. Save As... ie szFilePath != NULL and not empty
	// 2. tasklist has been modified
	if ((szFilePath && !sFilePath.IsEmpty()) || tdc.IsModified())
	{
		CPreferences prefs;
		
		// do this in a loop in case the save fails for _any_ reason
		while (TRUE)
		{
			if (sFilePath.IsEmpty()) // means first time save
			{
				// activate tasklist
				if (!SelectToDoCtrl(nIndex, (nIndex != GetSelToDoCtrl())))
					return TDCO_CANCELLED;
				
				// use tab text as hint to user
				sFilePath = m_mgrToDoCtrls.GetFilePath(nIndex, FALSE);

				CFileSaveDialog dialog(sTitle, 
										GetDefaultFileExt(), 
										sFilePath, 
										EOFN_DEFAULTSAVE, 
										GetFileFilter(FALSE), 
										this);
				
				// get the user's last choice of saving new tasklists
				// as the best hint for this time
				BOOL bUnicode = prefs.GetProfileInt(PREF_KEY, _T("UnicodeNewTasklists"), TRUE);
				dialog.m_ofn.nFilterIndex = (bUnicode ? 2 : 1);
				
				if (dialog.DoModal(&prefs) != IDOK)
					return TDCO_CANCELLED; // user elected not to proceed
				
				// else make sure the file is not readonly
				sFilePath = dialog.GetPathName();

				// check for format change
				bUnicode = (dialog.m_ofn.nFilterIndex == 2);
				tdc.SetUnicode(bUnicode);
		
				// save this choice as the best hint for the next new tasklist
				prefs.WriteProfileInt(PREF_KEY, _T("UnicodeNewTasklists"), bUnicode);
				
				// else make sure the file is not readonly
				if (CDriveInfo::IsReadonlyPath(sFilePath) > 0)
				{
					CEnString sMessage(IDS_SAVEREADONLY, sFilePath);
					
					if (MessageBox(sMessage, sTitle, MB_OKCANCEL) == IDCANCEL)
						return TDCO_CANCELLED; // user elected not to proceed
					else
					{
						sFilePath.Empty(); // try again
						continue;
					}
				}
			}

			// back file up
			DoBackup(nIndex);
			
			// update source control status
			const CPreferencesDlg& userPrefs = Prefs();
			BOOL bSrcControl = m_mgrToDoCtrls.PathSupportsSourceControl(sFilePath);
			
			tdc.SetStyle(TDCS_ENABLESOURCECONTROL, bSrcControl);
			tdc.SetStyle(TDCS_CHECKOUTONLOAD, bSrcControl ? userPrefs.GetAutoCheckOut() : FALSE);
			
			TDC_FILE nSave = TDCO_SUCCESS;
			CTaskFile tasks;

			// scoped to end status bar progress
			// before calling UpdateStatusbar
			{
				DOPROGRESS(IDS_SAVINGPROGRESS)
				nSave = tdc.Save(tasks, sFilePath);
				
				// send to storage as appropriate
				TSM_TASKLISTINFO storageInfo;
				
				if (nSave == TDCO_SUCCESS && m_mgrToDoCtrls.GetStorageDetails(nIndex, storageInfo))
				{
					m_mgrStorage.StoreTasklist(&storageInfo, &tasks, -1, &prefs);
				}
			}

			if (nSave == TDCO_SUCCESS)
			{
				m_mgrToDoCtrls.SetModifiedStatus(nIndex, FALSE);
				m_mgrToDoCtrls.RefreshLastModified(nIndex);
				m_mgrToDoCtrls.RefreshReadOnlyStatus(nIndex);
				m_mgrToDoCtrls.RefreshPathType(nIndex);
				
				if (userPrefs.GetAddFilesToMRU() && !m_mgrToDoCtrls.UsesStorage(nIndex))
					m_mruList.Add(sFilePath);
				
				UpdateCaption();
				UpdateStatusbar();
				
				// auto-export after saving
				CString sExt;
				
				if (userPrefs.GetAutoExport() && GetAutoExportExtension(sExt))
				{
					DOPROGRESS(IDS_EXPORTPROGRESS)

					// construct output path
					CString sExportPath = userPrefs.GetAutoExportFolderPath();
					CString sDrive, sFolder, sFName;
					
					FileMisc::SplitPath(sFilePath, &sDrive, &sFolder, &sFName);
					
					if (!sExportPath.IsEmpty() && FileMisc::CreateFolder(sExportPath))
						FileMisc::MakePath(sFilePath, NULL, sExportPath, sFName, sExt);
					else
						FileMisc::MakePath(sFilePath, sDrive, sFolder, sFName, sExt);
					
					// The current contents of 'tasks' is 'All Tasks' and 
					// 'All Columns' but NOT 'Html Comments'.
					// So if user either wants 'Filtered Tasks' or 'Html Comments' or
					// only 'Visible Columns' we need to grab the tasks again.
					BOOL bFiltered = (userPrefs.GetExportFilteredOnly() && (tdc.HasCustomFilter() || tdc.HasFilter()));
					
					if (bFiltered || userPrefs.GetExportToHTML() || !userPrefs.GetExportAllAttributes())
					{
						TSD_TASKS nWhatTasks = bFiltered ? TSDT_FILTERED : TSDT_ALL;
						TDCGETTASKS filter;
						
						if (!userPrefs.GetExportAllAttributes())
						{
							CTDCColumnIDArray aCols;
							tdc.GetVisibleColumns(aCols);
							
							MapColumnsToAttributes(aCols, filter.aAttribs);
							
							// add comments always
							filter.aAttribs.Add(TDCA_COMMENTS);
						}
						
						BOOL bHtmlComments = userPrefs.GetExportToHTML();
						BOOL bTransform = FileMisc::FileExists(userPrefs.GetSaveExportStylesheet());
						
						// set the html image folder to be the output path with
						// an different extension
						CString sImgFolder;
						
						if (bHtmlComments)
						{
							sImgFolder = sFilePath;
							FileMisc::ReplaceExtension(sImgFolder, _T("html_images"));
						}
						
						tasks.Reset();
						GetTasks(tdc, bHtmlComments, bTransform, nWhatTasks, filter, 0, tasks, sImgFolder); 
					}
					
					// save intermediate tasklist to file as required
					LogIntermediateTaskList(tasks, tdc.GetFilePath());

					// want HTML 
					if (userPrefs.GetExportToHTML())
					{
						Export2Html(tasks, sFilePath, userPrefs.GetSaveExportStylesheet());
					}
					else if (userPrefs.GetOtherExporter() != -1)
					{
						int nExporter = userPrefs.GetOtherExporter();
						m_mgrImportExport.ExportTaskList(&tasks, sFilePath, nExporter, TRUE);
					}
				}
				
				// we're done
				break;
			}
			else if (!bAuto)
			{
				// error handling if this is not an auto-save
				if (nSave == TDCO_NOTALLOWED)
				{
					CEnString sMessage(IDS_SAVEACCESSDENIED, sFilePath);
					
					if (IDYES == MessageBox(sMessage, sTitle, MB_YESNOCANCEL | MB_ICONEXCLAMATION))
					{
						sFilePath.Empty(); // try again
						continue;
					}
					else // clear modified status
					{
						tdc.SetModified(FALSE);
						m_mgrToDoCtrls.SetModifiedStatus(nIndex, FALSE);
						break;
					}
				}
				else
				{
					CString sMessage;
					
					switch (nSave)
					{
					case TDCO_CANCELLED:
						break;
						
					case TDCO_BADMSXML:
						sMessage.Format(IDS_SAVEBADXML, sFilePath);
						break;
						
					case TDCO_INUSE:
						sMessage.Format(IDS_SAVESHARINGVIOLATION, sFilePath);
						break;
						
					case TDCO_NOTCHECKEDOUT:
						sMessage.Format(IDS_SAVENOTCHECKEDOUT, sFilePath);
						break;
						
					default:
						sMessage.Format(IDS_UNKNOWNSAVEERROR2, sFilePath, (nSave - (int)TDCO_OTHER));
						break;
					}
					
					if (!sMessage.IsEmpty())
						MessageBox(sMessage, sTitle, MB_OK);
				}
				break; // we're done
			}
			else // bAuto == TRUE
			{
				break; // we're done
			}
		}
	}

	return TDCO_SUCCESS;
}

BOOL CToDoListWnd::GetAutoExportExtension(CString& sExt) const
{
	sExt.Empty();

	if (Prefs().GetExportToHTML())
		sExt = ".html";
	else
	{
		int nExporter = Prefs().GetOtherExporter();

		if (nExporter != -1)
			sExt = m_mgrImportExport.GetExporterFileExtension(nExporter);
	}

	return !sExt.IsEmpty();
}

LPCTSTR CToDoListWnd::GetFileFilter(BOOL bOpen)
{
	static CEnString TDLFILEOPENFILTER(IDS_TDLFILEOPENFILTER);
	static CEnString XMLFILEOPENFILTER(IDS_XMLFILEOPENFILTER);
	static CEnString TDLFILESAVEFILTER(IDS_TDLFILESAVEFILTER);
	static CEnString XMLFILESAVEFILTER(IDS_XMLFILESAVEFILTER);
	
	if (Prefs().GetEnableTDLExtension())
		return bOpen ? TDLFILEOPENFILTER : TDLFILESAVEFILTER;
	
	// else
	return bOpen ? XMLFILEOPENFILTER : XMLFILESAVEFILTER;
}

LPCTSTR CToDoListWnd::GetDefaultFileExt()
{
	static LPCTSTR TDLEXT = _T("tdl");
	static LPCTSTR XMLEXT = _T("xml");
	
	if (Prefs().GetEnableTDLExtension())
		return TDLEXT;
	else
		return XMLEXT;
}

void CToDoListWnd::UpdateStatusbar()
{
	if (!m_sbProgress.IsActive() && GetTDCCount())
	{
		// get display path
		int nTasklist = GetSelToDoCtrl();
		const CFilteredToDoCtrl& tdc = GetToDoCtrl(nTasklist);

		CEnString sText = m_mgrToDoCtrls.GetDisplayPath(nTasklist);

		if (sText.IsEmpty())
			sText.LoadString(ID_SB_FILEPATH);
		
		else if (tdc.IsUnicode())
			sText += _T(" (Unicode)");
		
		m_statusBar.SetPaneText(m_statusBar.CommandToIndex(ID_SB_FILEPATH), sText);

		// get file version
		sText.Format(ID_SB_FILEVERSION, tdc.GetFileVersion());
		m_statusBar.SetPaneText(m_statusBar.CommandToIndex(ID_SB_FILEVERSION), sText);
	}
}

void CToDoListWnd::OnLoad() 
{
	CPreferences prefs;
	CFileOpenDialog dialog(IDS_OPENTASKLIST_TITLE, 
							GetDefaultFileExt(), 
							NULL, 
							EOFN_DEFAULTOPEN | OFN_ALLOWMULTISELECT,
							GetFileFilter(TRUE), 
							this);
	
	const UINT BUFSIZE = 1024 * 5;
	static TCHAR FILEBUF[BUFSIZE] = { 0 };
	
	dialog.m_ofn.lpstrFile = FILEBUF;
	dialog.m_ofn.nMaxFile = BUFSIZE;
	
	if (dialog.DoModal(&prefs) == IDOK)
	{
		CWaitCursor cursor;
		POSITION pos = dialog.GetStartPosition();
		
		while (pos)
		{
			CString sTaskList = dialog.GetNextPathName(pos);
			TDC_FILE nOpen = OpenTaskList(sTaskList);
			
			if (nOpen == TDCO_SUCCESS)
			{
				Resize();
				UpdateWindow();
			}
			else
				HandleLoadTasklistError(nOpen, sTaskList);
		}
		
		RefreshTabOrder();
	}
}

void CToDoListWnd::HandleLoadTasklistError(TDC_FILE nErr, LPCTSTR szTaskList)
{
	CEnString sMessage, sTitle(IDS_OPENTASKLIST_TITLE);
	
	switch (nErr)
	{
	case TDCO_SUCCESS:
		break; // not an error!
		
	case TDCO_CANCELLED:
		break; 
		
	case TDCO_NOTEXIST:
		sMessage.Format(IDS_TASKLISTNOTFOUND, szTaskList);
		break;
		
	case TDCO_NOTTASKLIST:
		sMessage.Format(IDS_INVALIDTASKLIST, szTaskList);
		break;
		
	case TDCO_NOTALLOWED:
		sMessage.Format(IDS_OPENACCESSDENIED, szTaskList);
		break;

	case TDCO_INUSE:
		sMessage.Format(IDS_OPENSHARINGVIOLATION, szTaskList);
		break;
		
	case TDCO_BADMSXML:
		sMessage.Format(IDS_BADXML, szTaskList);
		break;
		
	case TDCO_NOENCRYPTIONDLL:
		sMessage.Format(IDS_NOENCRYPTIONDLL, szTaskList);
		break;
		
	case TDCO_UNKNOWNENCRYPTION:
		sMessage.Format(IDS_UNKNOWNENCRYPTION, szTaskList);
		break;
		
	default: // all the other errors
		sMessage.Format(IDS_UNKNOWNOPENERROR, szTaskList, nErr - (int)TDCO_OTHER);
		break;
	}
	
	if (!sMessage.IsEmpty())
		MessageBox(sMessage, sTitle, MB_OK);
}

void CToDoListWnd::SaveSettings()
{
	CPreferences prefs;

	// pos
	WINDOWPLACEMENT wp;
	GetWindowPlacement(&wp);
	
	prefs.WriteProfileInt(_T("Pos"), _T("Left"), wp.rcNormalPosition.left);
	prefs.WriteProfileInt(_T("Pos"), _T("Top"), wp.rcNormalPosition.top);
	prefs.WriteProfileInt(_T("Pos"), _T("Right"), wp.rcNormalPosition.right);
	prefs.WriteProfileInt(_T("Pos"), _T("Bottom"), wp.rcNormalPosition.bottom);

// 	FileMisc::LogText(_T("SavePosition: TopLeft=(%d,%d) BotRight=(%d,%d) MinPos=(%d,%d) MaxPos=(%d,%d)"), 
// 						wp.rcNormalPosition.left, wp.rcNormalPosition.top,
// 						wp.rcNormalPosition.right, wp.rcNormalPosition.bottom,
// 						wp.ptMaxPosition.x, wp.ptMaxPosition.y,
// 						wp.ptMinPosition.x, wp.ptMinPosition.y);

	prefs.WriteProfileInt(_T("Pos"), _T("Hidden"), !m_bVisible);
	prefs.WriteProfileInt(_T("Pos"), _T("Maximized"), IsZoomed());
	
	// version
	prefs.WriteProfileInt(_T("Version"), _T("Version"), GetVersion());
	
	// last open files
	int nCount = GetTDCCount();
	int nSel = GetSelToDoCtrl(); // and last active file
	
	if (nCount) // but don't overwrite files saved in OnQueryEndSession() or OnClose()
	{
		for (int nTDC = 0, nItem = 0; nTDC < nCount; nTDC++)
		{
			CString sFilePath = m_mgrToDoCtrls.GetFilePath(nTDC);

			TSM_TASKLISTINFO storageInfo;

			if (m_mgrToDoCtrls.GetStorageDetails(nTDC, storageInfo))
			{
				sFilePath = storageInfo.EncodeInfo(Prefs().GetSaveStoragePasswords());

#ifdef _DEBUG
				ASSERT(storageInfo.DecodeInfo(sFilePath));
				ASSERT(storageInfo.EncodeInfo(TRUE) == sFilePath);
#endif
			}
			else // make file paths relative
			{
				FileMisc::MakeRelativePath(sFilePath, FileMisc::GetAppFolder(), FALSE);
			}

			CString sKey = Misc::MakeKey(_T("LastFile%d"), nItem);
			prefs.WriteProfileString(_T("Settings"), sKey, sFilePath);
			
			if (nSel == nTDC)
				prefs.WriteProfileString(_T("Settings"), _T("LastActiveFile"), sFilePath);

			nItem++;
		}
		
		prefs.WriteProfileInt(_T("Settings"), _T("NumLastFiles"), nCount);
	}

	// other settings
	prefs.WriteProfileInt(_T("Settings"), _T("ViewState"), m_nMaxState);
	prefs.WriteProfileInt(_T("Settings"), _T("ShowFilterBar"), m_bShowFilterBar);
	prefs.WriteProfileInt(_T("Settings"), _T("ToolbarOption"), m_bShowToolbar ? TB_TOOLBARANDMENU : TB_TOOLBARHIDDEN);
	prefs.WriteProfileInt(_T("Settings"), _T("ShowProjectName"), m_bShowProjectName);
	prefs.WriteProfileInt(_T("Settings"), _T("ShowStatusBar"), m_bShowStatusBar);
	prefs.WriteProfileInt(_T("Settings"), _T("ShowTasklistBar"), m_bShowTasklistBar);
	prefs.WriteProfileInt(_T("Settings"), _T("ShowTreeListBar"), m_bShowTreeListBar);

	if (m_findDlg.GetSafeHwnd())
		prefs.WriteProfileInt(_T("Settings"), _T("FindTasksVisible"), m_bFindShowing && m_findDlg.IsWindowVisible());
	
	if (Prefs().GetAddFilesToMRU())
		m_mruList.WriteList(prefs, TRUE);

	// quick find items
	nCount = m_cbQuickFind.GetCount();
	prefs.WriteProfileInt(_T("QuickFind"), _T("Count"), nCount);

	for (int nItem = 0; nItem < nCount; nItem++)
	{
		CString sItem, sKey = Misc::MakeKey(_T("Item%d"), nItem);
		m_cbQuickFind.GetLBText(nItem, sItem);

		prefs.WriteProfileString(_T("QuickFind"), sKey, sItem);
	}

	// save to permanent storage
	prefs.Save();
}

LRESULT CToDoListWnd::OnWebUpdateWizard(WPARAM /*wp*/, LPARAM /*lp*/)
{
	ASSERT (Prefs().GetAutoCheckForUpdates());

	CheckForUpdates(FALSE);
	return 0L;
}

LRESULT CToDoListWnd::OnAddToolbarTools(WPARAM /*wp*/, LPARAM /*lp*/)
{
	Misc::ProcessMsgLoop();
	AppendTools2Toolbar();
	return 0L;
}

TDC_PREPAREPATH CToDoListWnd::PrepareFilePath(CString& sFilePath, TSM_TASKLISTINFO* pInfo)
{
	TDC_PREPAREPATH nType = TDCPP_NONE;
	TSM_TASKLISTINFO temp;

	if (pInfo == NULL)
		pInfo = &temp;

	// first test for storage
	if (pInfo->DecodeInfo(sFilePath, Prefs().GetSaveStoragePasswords()))
	{
		sFilePath = pInfo->szLocalFileName;

		// check for overflow and non-existence
		if (FileMisc::FileExists(sFilePath))
			nType = TDCPP_STORAGE;
		else
			sFilePath.Empty();
	}
	// else it's a file path.
	// if it starts with a colon then we need to find the removable drive it's stored on
	else if (!sFilePath.IsEmpty())
	{
		if (sFilePath[0] == ':')
		{
			for (int nDrive = 4; nDrive <= 26; nDrive++) // from D: upwards
			{
				if (CDriveInfo::GetType(nDrive) == DRIVE_REMOVABLE)
				{
					CString sTryPath = CDriveInfo::GetLetter(nDrive) + sFilePath;

					if (FileMisc::FileExists(sTryPath))
					{
						sFilePath = sTryPath;
						break; // finished
					}
				}
			}
		}
		else
			FileMisc::MakeFullPath(sFilePath, FileMisc::GetAppFolder()); // handle relative paths

		// check for existence
		if (FileMisc::FileExists(sFilePath))
			nType = TDCPP_FILE;
		else
			sFilePath.Empty();
	}
	
	return nType;
}

LRESULT CToDoListWnd::OnPostOnCreate(WPARAM /*wp*/, LPARAM /*lp*/)
{
	// late initialization
	CMouseWheelMgr::Initialize();
	CEditShortcutMgr::Initialize();
	CFocusWatcher::Initialize(this);

	InitShortcutManager();
	InitMenuIconManager();

	// reminders
	m_reminders.Initialize(this);

	// with or without Stickies Support
	const CPreferencesDlg& userPrefs = Prefs();
	CString sStickiesPath;
	
	if (userPrefs.GetUseStickies(sStickiesPath))
		VERIFY(m_reminders.UseStickies(TRUE, sStickiesPath));
	
	// light boxing
 	if (Prefs().GetEnableLightboxMgr())
		CLightBoxMgr::Initialize(this, m_theme.crAppBackDark);
	
	// add outstanding translated items to dictionary
	if (CLocalizer::GetTranslationOption() == ITTTO_ADD2DICTIONARY)
	{
		CUIntArray aDictVersion, aAppVersion;
		VERIFY (FileMisc::GetAppVersion(aAppVersion));

		BOOL bUpdateDict = !CLocalizer::GetDictionaryVersion(aDictVersion) ||
							aDictVersion.GetSize() < 2;

		if (!bUpdateDict)
		{
			// check if the major or minor version has increased
			bUpdateDict = (FileMisc::CompareVersions(aAppVersion, aDictVersion, 2) > 0);

			// check for pre-release build then update
			if (!bUpdateDict && aAppVersion[2] >= 297)
			{
				// compare entire version string
				bUpdateDict = (FileMisc::CompareVersions(aAppVersion, aDictVersion) > 0);
			}
		}

		if (bUpdateDict)
			TranslateUIElements();
	}

	RestoreVisibility();
	
	// load last open tasklists
	CAutoFlag af(m_bReloading, TRUE);
	CPreferences prefs;

	// initialize Progress first time
	m_sbProgress.BeginProgress(m_statusBar, CEnString(IDS_STARTUPPROGRESS));

	// open cmdline tasklist
	int nTDCCount = prefs.GetProfileInt(_T("Settings"), _T("NumLastFiles"), 0);

	if (!m_startupOptions.HasFilePath() || nTDCCount)
	{
		// if we have a file on the commandline or any previous tasklists
		// set the prompt of the initial tasklist to something appropriate
		//	TODO
	}
	
	// theme
	SetUITheme(userPrefs.GetUITheme());

	// cache empty flag for later
	BOOL bStartupEmpty = m_startupOptions.HasFlag(TLD_STARTEMPTY);

	// what to (re)load?
	BOOL bReloadTasklists = (!bStartupEmpty && userPrefs.GetReloadTasklists());
	
	// filepath overrides
	if (m_startupOptions.HasFilePath())
	{
		ProcessStartupOptions(m_startupOptions);

		// don't reload previous if a tasklist was actually loaded
		if (!m_mgrToDoCtrls.IsPristine(0))
			bReloadTasklists = FALSE;
	}
	
	m_startupOptions.Reset(); // always
	
	// load last files
	if (bReloadTasklists)
	{
		// get the last active tasklist
		CString sLastActiveFile = prefs.GetProfileString(_T("Settings"), _T("LastActiveFile")), sOrgLastActiveFile;
		BOOL bCanDelayLoad = userPrefs.GetEnableDelayedLoading();

		for (int nTDC = 0; nTDC < nTDCCount; nTDC++)
		{
			CString sKey = Misc::MakeKey(_T("LastFile%d"), nTDC);
			CString sLastFile = prefs.GetProfileString(_T("Settings"), sKey);

			if (!sLastFile.IsEmpty())
			{
				// delay-open all but the non-active tasklist
				// unless the tasklist has reminders
				BOOL bActiveTDC = (sLastFile == sLastActiveFile);

				if (!bActiveTDC && bCanDelayLoad && !m_reminders.ToDoCtrlHasReminders(sLastFile))
				{
					DelayOpenTaskList(sLastFile);
				}
				else
				{
					TDC_FILE nResult = OpenTaskList(sLastFile, FALSE);

					// if the last active tasklist was cancelled then
					// delay load it and mark the last active todoctrl as not found
					if (bActiveTDC && nResult != TDCO_SUCCESS)
					{
						sOrgLastActiveFile = sLastActiveFile;
						sLastActiveFile.Empty();

						if (nResult == TDCO_CANCELLED && bCanDelayLoad)
							DelayOpenTaskList(sLastFile);
					}
				}
			}
		}
		
		// process all pending messages
		Misc::ProcessMsgLoop();

		// if the last active tasklist could not be loaded then we need to find another
		if (GetTDCCount())
		{
			// make Last Active Files actual filepaths
			PrepareFilePath(sLastActiveFile);
			PrepareFilePath(sOrgLastActiveFile);

			if (sLastActiveFile.IsEmpty())
			{
				for (int nTDC = 0; nTDC < GetTDCCount() && sLastActiveFile.IsEmpty(); nTDC++)
				{
					CFilteredToDoCtrl& tdc = GetToDoCtrl(nTDC);

					// ignore original active tasklist
					if (tdc.GetFilePath() != sOrgLastActiveFile)
					{
						if (VerifyTaskListOpen(nTDC, FALSE))
							sLastActiveFile = tdc.GetFilePath();
					}
				}
			}

			// if nothing suitable found then create an empty tasklist
			if (sLastActiveFile.IsEmpty())
			{
				if (GetTDCCount() == 0)
					CreateNewTaskList(FALSE);
			}
			else if (!SelectToDoCtrl(sLastActiveFile, FALSE))
				SelectToDoCtrl(0, FALSE); // the first one

			Resize();
		}
	}

	// if there's only one tasklist open and it's pristine then 
	// it's the original one so add a sample task unless 
	// 'empty' flag is set
	if (GetTDCCount() == 1 && m_mgrToDoCtrls.IsPristine(0))
	{
		if (!bStartupEmpty)
		{
			CFilteredToDoCtrl& tdc = GetToDoCtrl();
			ASSERT (tdc.GetTaskCount() == 0);

			tdc.CreateNewTask(CEnString(IDS_SAMPLETASK), TDC_INSERTATTOP, FALSE);
			tdc.SetModified(FALSE);
			
			m_mgrToDoCtrls.SetModifiedStatus(0, FALSE);

			UpdateCaption();
		}
	}
	else // due task notifications
	{
		int nDueBy = userPrefs.GetNotifyDueByOnLoad();
		
		if (nDueBy != PFP_DONTNOTIFY)
		{
			UpdateWindow();
			m_tabCtrl.UpdateWindow();
			
			int nCtrls = GetTDCCount();
			
			for (int nCtrl = 0; nCtrl < nCtrls; nCtrl++)
			{
				if (m_mgrToDoCtrls.IsLoaded(nCtrl))
					DoDueTaskNotification(nCtrl, nDueBy);
			}
		}
	}

	// refresh toolbar 'tools' buttons unless minimized because
	// we must handle it when we're first shown
	if (m_bShowToolbar && AfxGetApp()->m_nCmdShow != SW_SHOWMINIMIZED)
		AppendTools2Toolbar();

	// web update
	if (Prefs().GetAutoCheckForUpdates())
		PostMessage(WM_WEBUPDATEWIZARD);

	// current focus
	PostMessage(WM_FW_FOCUSCHANGE, (WPARAM)::GetFocus(), 0L);

	RefreshTabOrder();

	// end progress before refreshing statusbar
	m_sbProgress.EndProgress();
	UpdateStatusbar();

	// find tasks dialog
	InitFindDialog();

	if (prefs.GetProfileInt(_T("Settings"), _T("FindTasksVisible"), 0))
	{
		OnFindTasks();
		
		if (userPrefs.GetRefreshFindOnLoad())
			m_findDlg.RefreshSearch();
	}

	// log the app and its dlls for debugging
	FileMisc::LogAppModuleState(FBM_SORTBY_HMODULE);

	return 0L;
}

void CToDoListWnd::CheckForUpdates(BOOL bManual)
{
	CPreferences prefs;

	// only check once a day
	int nLastUpdate = prefs.GetProfileInt(_T("Updates"), _T("LastUpdate"), 0);
	int nToday = (int)CDateHelper::GetDate(DHD_TODAY);

	if (!bManual && nLastUpdate >= nToday)
		return;

	prefs.WriteProfileInt(_T("Updates"), _T("LastUpdate"), nToday);

	// get the app wuw path
	CString sFolder, sDrive;
	CString sWuwPath = FileMisc::GetAppFolder() + _T("\\WebUpdateSvc.exe");

	// check for existence if manual
	if (bManual && !FileMisc::FileExists(sWuwPath))
	{
		LPCTSTR DOWNLOAD_WUW_PATH = _T("http://www.abstractspoon.com/todolist_wuw.zip");

		if (MessageBox(IDS_NOWUW, 0, MB_YESNO) == IDYES)
			::ShellExecute(NULL, _T("open"), DOWNLOAD_WUW_PATH, NULL, NULL, SW_HIDE);
		else
			return;
	}

	// because the download may include the WuW we copy it to a temp name
	// so that the original can be overwritten.
	CString sWuwPathTemp = FileMisc::GetAppFolder() + _T("\\WebUpdateSvc2.exe");

	if (::CopyFile(sWuwPath, sWuwPathTemp, FALSE))
		sWuwPath = sWuwPathTemp;

	if (bManual)
	{
		if (m_bUseStagingScript)
			::ShellExecute(NULL, _T("open"), sWuwPath, UPDATE_SCRIPT_PATH_STAGING, NULL, SW_HIDE);
		else
			::ShellExecute(NULL, _T("open"), sWuwPath, UPDATE_SCRIPT_PATH_MANUAL, NULL, SW_HIDE);
	}
	else
		::ShellExecute(NULL, _T("open"), sWuwPath, UPDATE_SCRIPT_PATH, NULL, SW_HIDE);
} 
	
void CToDoListWnd::LoadSettings()
{
	// settings
	CPreferences prefs;

	BOOL bMaxTasklists = prefs.GetProfileInt(_T("Settings"), _T("SimpleMode"), FALSE); // backward compatibility
	m_nMaxState = (TDC_MAXSTATE)prefs.GetProfileInt(_T("Settings"), _T("ViewState"), bMaxTasklists ? TDCMS_MAXTASKLIST : TDCMS_NORMAL);

	m_bShowFilterBar = prefs.GetProfileInt(_T("Settings"), _T("ShowFilterBar"), m_bShowFilterBar);
	m_bShowProjectName = prefs.GetProfileInt(_T("Settings"), _T("ShowProjectName"), m_bShowProjectName);
	
	m_bShowStatusBar = prefs.GetProfileInt(_T("Settings"), _T("ShowStatusBar"), m_bShowStatusBar);
	m_statusBar.ShowWindow(m_bShowStatusBar ? SW_SHOW : SW_HIDE);
	
	// toolbar
	m_bShowToolbar = (prefs.GetProfileInt(_T("Settings"), _T("ToolbarOption"), TB_TOOLBARANDMENU) != TB_TOOLBARHIDDEN);
	m_toolbar.ShowWindow(m_bShowToolbar ? SW_SHOW : SW_HIDE);
	m_toolbar.EnableWindow(m_bShowToolbar);

	// tabbars
	m_bShowTasklistBar = prefs.GetProfileInt(_T("Settings"), _T("ShowTasklistBar"), TRUE);
	m_bShowTreeListBar = prefs.GetProfileInt(_T("Settings"), _T("ShowTreeListBar"), TRUE);

	// pos
	RestorePosition();

	// user preferences
	const CPreferencesDlg& userPrefs = Prefs();
	
	// MRU
	if (userPrefs.GetAddFilesToMRU())
		m_mruList.ReadList(prefs);
	
	// note: we do not restore visibility until OnPostOnCreate

	// default attributes
	UpdateDefaultTaskAttributes(userPrefs);
	
	// hotkey
	UpdateGlobalHotkey();
	
	// time periods
	CTimeHelper::SetHoursInOneDay(userPrefs.GetHoursInOneDay());
	CTimeHelper::SetDaysInOneWeek(userPrefs.GetDaysInOneWeek());

	// support for .tdl
	CFileRegister filereg(_T("tdl"), _T("tdl_Tasklist"));
	
	if (userPrefs.GetEnableTDLExtension())
		filereg.RegisterFileType(_T("Tasklist"), 0);
	else
		filereg.UnRegisterFileType();

	// support for tdl protocol
	EnableTDLProtocol(userPrefs.GetEnableTDLProtocol());

	// previous quick find items
	int nCount = prefs.GetProfileInt(_T("QuickFind"), _T("Count"), 0);

	for (int nItem = 0; nItem < nCount; nItem++)
	{
		CString sKey = Misc::MakeKey(_T("Item%d"), nItem);
		m_cbQuickFind.AddUniqueItem(prefs.GetProfileString(_T("QuickFind"), sKey));
	}

	// Recently modified period
	CFilteredToDoCtrl::SetRecentlyModifiedPeriod(userPrefs.GetRecentlyModifiedPeriod());
}

void CToDoListWnd::EnableTDLProtocol(BOOL bEnable)
{
	if (bEnable)
	{
		CRegKey reg;

		if (reg.Open(HKEY_CLASSES_ROOT, _T("tdl")) == ERROR_SUCCESS)
		{
			reg.Write(_T(""), _T("URL: ToDoList protocol"));
			reg.Write(_T("URL Protocol"), _T(""));

			// write exe name out
			CString sAppPath = FileMisc::GetAppFileName() + _T(" -l \"%1\"");

			reg.Close();

			if (reg.Open(HKEY_CLASSES_ROOT, _T("tdl\\shell\\open\\command")) == ERROR_SUCCESS)
				reg.Write(_T(""), sAppPath);
		}
	}
	else
		CRegKey::DeleteKey(HKEY_CLASSES_ROOT, _T("tdl"));
}

void CToDoListWnd::RestoreVisibility()
{
	const CPreferencesDlg& userPrefs = Prefs();
	CPreferences prefs;

	int nDefShowState = AfxGetApp()->m_nCmdShow;
	BOOL bShowOnStartup = userPrefs.GetShowOnStartup();

	BOOL bMaximized = prefs.GetProfileInt(_T("Pos"), _T("Maximized"), FALSE) || (nDefShowState == SW_SHOWMAXIMIZED);
	BOOL bMinimized = !bShowOnStartup && (nDefShowState == SW_SHOWMINIMIZED || nDefShowState == SW_SHOWMINNOACTIVE);
	
	if (bMinimized)
	{
		bMaximized = FALSE; // can't be max-ed and min-ed
		m_bStartHidden = TRUE;
	}
	
	if (m_bVisible == -1) // not yet set
	{
		m_bVisible = TRUE;
		
		// the only reason it can be hidden is if it uses the systray
		// and the user has elected to not have it show at startup
		// and it was hidden the last time it closed or its set to run
		// minimized and that is the trigger to hide it
		if (!bShowOnStartup && userPrefs.GetUseSysTray())
		{
			if (prefs.GetProfileInt(_T("Pos"), _T("Hidden"), FALSE))
				m_bVisible = FALSE;
			
			// also if wp.showCmd == minimized and we would hide to sys
			// tray when minimized then hide here too
			else if (nDefShowState == SW_SHOWMINIMIZED || nDefShowState == SW_SHOWMINNOACTIVE)
			{
				int nSysTrayOption = Prefs().GetSysTrayOption();
				
				if (nSysTrayOption == STO_ONMINIMIZE || nSysTrayOption == STO_ONMINCLOSE)
					m_bVisible = FALSE;
			}
		}
	}
	
	if (m_bVisible)
	{
		int nShowCmd = (bMaximized ? SW_SHOWMAXIMIZED : 
						(bMinimized ? SW_SHOWMINIMIZED : SW_SHOW));
		
 		ShowWindow(nShowCmd);
 		Invalidate();
 		UpdateWindow();
	}
	else
		m_bStartHidden = TRUE;

	// don't set topmost if maximized
	if (userPrefs.GetAlwaysOnTop() && !bMaximized)
		SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void CToDoListWnd::RestorePosition()
{
	CPreferences prefs;

	int nLeft = prefs.GetProfileInt(_T("Pos"), _T("Left"), -1);
	int nTop = prefs.GetProfileInt(_T("Pos"), _T("Top"), -1);
	int nRight = prefs.GetProfileInt(_T("Pos"), _T("Right"), -1);
	int nBottom = prefs.GetProfileInt(_T("Pos"), _T("Bottom"), -1);
	
	CRect rect(nLeft, nTop, nRight, nBottom);
	
	if (rect.Width() > 0 && rect.Height() > 0)
	{
		// ensure this intersects with the desktop by a decent amount
		int BORDER = 200;
		rect.DeflateRect(BORDER, BORDER);

		CRect rScreen;

		if (GraphicsMisc::GetAvailableScreenSpace(rect, rScreen))
		{
			rect.InflateRect(BORDER, BORDER);

			// because the position was saved using the results of 
			// GetWindowPlacement we must use SetWindowPlacement
			// to restore the window
			WINDOWPLACEMENT wp = { 0 };

			wp.rcNormalPosition = rect;
			wp.ptMaxPosition.x = -1;
			wp.ptMaxPosition.y = -1;
			wp.ptMinPosition.x = -1;
			wp.ptMinPosition.y = -1;

// 			FileMisc::LogText(_T("RestorePosition: TopLeft=(%d,%d) BotRight=(%d,%d) MinPos=(%d,%d) MaxPos=(%d,%d)"), 
// 								wp.rcNormalPosition.left, wp.rcNormalPosition.top,
// 								wp.rcNormalPosition.right, wp.rcNormalPosition.bottom,
// 								wp.ptMaxPosition.x, wp.ptMaxPosition.y,
// 								wp.ptMinPosition.x, wp.ptMinPosition.y);

			SetWindowPlacement(&wp);
		}
		else
			rect.SetRectEmpty();
	}
	
	// first time or monitors changed?
	if (rect.IsRectEmpty())
	{
		rect.SetRect(0, 0, 1024, 730); // default

		// make sure it fits the screen
		CRect rScreen;
		GraphicsMisc::GetAvailableScreenSpace(rScreen);

		if (rect.Height() > rScreen.Height())
			rect.bottom = rScreen.Height();

		MoveWindow(rect);
		CenterWindow();
	}
}

void CToDoListWnd::OnNew() 
{
	CreateNewTaskList(FALSE);
	RefreshTabOrder();
}

BOOL CToDoListWnd::CreateNewTaskList(BOOL bAddDefTask)
{
	CFilteredToDoCtrl* pNew = NewToDoCtrl();
	
	if (pNew)
	{
		int nNew = AddToDoCtrl(pNew);
		
		// insert a default task
		if (bAddDefTask)
		{
			if (pNew->GetTaskCount() == 0)
				VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATTOP, FALSE));
		}
		else // ensure it is empty
			pNew->DeleteAllTasks();
		
		// clear modified flag
		pNew->SetModified(FALSE);
		m_mgrToDoCtrls.SetModifiedStatus(nNew, FALSE);
	}

	return (pNew != NULL);
}

void CToDoListWnd::OnUpdateDeletetask(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.HasSelection());	
}

void CToDoListWnd::OnUpdateEditTasktext(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount == 1 && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnUpdateTaskcolor(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.HasSelection() && 
		Prefs().GetTextColorOption() == COLOROPT_DEFAULT);	
}

void CToDoListWnd::OnUpdateTaskdone(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	if (nSelCount == 1)
		pCmdUI->SetCheck(tdc.IsSelectedTaskDone() ? 1 : 0);
	
	pCmdUI->Enable(!tdc.IsReadOnly() && GetToDoCtrl().GetSelectedItem());	
}

void CToDoListWnd::OnUpdateDeletealltasks(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && GetToDoCtrl().GetTaskCount());	
}

void CToDoListWnd::OnUpdateSave(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(GetTDCCount() && tdc.IsModified() && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnUpdateNew(CCmdUI* pCmdUI)  
{
	pCmdUI->Enable(TRUE);	
}

BOOL CToDoListWnd::OnEraseBkgnd(CDC* pDC) 
{
	if (!GetTDCCount())
		return CFrameWnd::OnEraseBkgnd(pDC);

	CDialogHelper::ExcludeChild(this, pDC, &m_toolbar);
	CDialogHelper::ExcludeChild(this, pDC, &m_statusBar);
	CDialogHelper::ExcludeChild(this, pDC, &m_tabCtrl);
	CDialogHelper::ExcludeChild(this, pDC, &m_filterBar);
	CDialogHelper::ExcludeChild(this, pDC, &GetToDoCtrl());

	CRect rClient;
	GetClientRect(rClient);

	if (CThemed::IsThemeActive())
	{
		// paint between the window top and the top of the toolbar
		// in toolbar color
		if (m_bShowToolbar)
		{
			CRect rToolbar = CDialogHelper::OffsetCtrl(this, &m_toolbar);

			pDC->FillSolidRect(rClient.left, rClient.top, rClient.Width(), rToolbar.top, m_theme.crToolbarLight);
			rClient.top += rToolbar.bottom;// + 2;
		}

		// we need to paint a smidgen between the base of the toolbar
		// and the top of the tab bar in crAppBackDark 
		if (WantTasklistTabbarVisible())
		{
			pDC->FillSolidRect(rClient.left, rClient.top, rClient.Width(), 5, m_theme.crAppBackDark);
			rClient.top += 5;
		}

		// and then the rest in crAppBackLight
		pDC->FillSolidRect(rClient, m_theme.crAppBackLight);
	}
	else
		pDC->FillSolidRect(rClient, GetSysColor(COLOR_3DFACE));

	// we must draw out own bevel below the toolbar (or menu if the toolbar is not visible)
	int nVPos = 0;
	
	if (m_bShowToolbar)
	{
		if (COSVersion() <= OSV_XP)
			pDC->FillSolidRect(rClient.left, nVPos, rClient.Width(), 1, m_theme.crAppLinesDark);
		
		nVPos = m_toolbar.GetHeight() + TB_VOFFSET;
	}	

	pDC->FillSolidRect(rClient.left, nVPos, rClient.Width(), 1, m_theme.crAppLinesDark);
	pDC->FillSolidRect(rClient.left, nVPos + 1, rClient.Width(), 1, m_theme.crAppLinesLight);
	
	// bevel below filter bar
	if (m_bShowFilterBar)
	{
		CRect rFilter;
		m_filterBar.GetWindowRect(rFilter);
		ScreenToClient(rFilter);

		int nVPos = rFilter.bottom;

		pDC->FillSolidRect(rClient.left, nVPos, rClient.Width(), 1, m_theme.crAppLinesDark);
		pDC->FillSolidRect(rClient.left, nVPos + 1, rClient.Width(), 1, m_theme.crAppLinesLight);
	}

	// bevel above the statusbar if themed
	if (m_bShowStatusBar && CThemed::IsThemeActive())
	{
		CRect rStatBar;
		m_statusBar.GetWindowRect(rStatBar);
		ScreenToClient(rStatBar);

		pDC->FillSolidRect(0, rStatBar.top - 2, rClient.Width(), 1, m_theme.crAppLinesDark);
		pDC->FillSolidRect(0, rStatBar.top - 1, rClient.Width(), 1, m_theme.crAppLinesLight);
	}

	// this is definitely amongst the worst hacks I've ever had to implement.
	// It occurs because the CSysImageList class seems to not initialize 
	// properly unless the main window is visible. so in the case of starting hidden
	// or starting minimized we must wait until we become visible before
	// adding the tools to the toolbar.
	if (m_bStartHidden)
	{
		m_bStartHidden = FALSE;
		PostMessage(WM_ADDTOOLBARTOOLS);
	}

	return TRUE;
}

void CToDoListWnd::OnSortBy(UINT nCmdID) 
{
	if (nCmdID == ID_SORT_MULTI)
		return;

	TDC_COLUMN nSortBy = GetSortBy(nCmdID);
	
	// update todoctrl
	GetToDoCtrl().Sort(nSortBy);
}

void CToDoListWnd::OnUpdateSortBy(CCmdUI* pCmdUI)
{
	if (pCmdUI->m_nID == ID_SORT_MULTI)
		return;

	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	// disable if column not visible
	TDC_COLUMN nSortBy = GetSortBy(pCmdUI->m_nID);
	pCmdUI->Enable(tdc.IsColumnShowing(nSortBy));

	// only set the radio button if we're not multisorting
	BOOL bChecked = !tdc.IsMultiSorting() && (tdc.GetSortBy() == nSortBy);
	pCmdUI->SetRadio(bChecked);

	// let menu icon manager handle setting selected state
	switch (pCmdUI->m_nID)
	{
	case ID_SORT_NONE:
		pCmdUI->Enable(TRUE);
		break;

	case ID_SORT_BYCOLOR:
		pCmdUI->Enable(Prefs().GetTextColorOption() == COLOROPT_DEFAULT);
		break;

	case ID_SORT_BYPATH:
		pCmdUI->Enable(tdc.GetView() != FTCV_TASKTREE);
		break;
	}
}

TDC_COLUMN CToDoListWnd::GetSortBy(UINT nSortID) 
{
	switch (nSortID)
	{
	case ID_SORT_BYNAME:			return TDCC_CLIENT;
	case ID_SORT_BYID:				return TDCC_ID;
	case ID_SORT_BYALLOCTO:			return TDCC_ALLOCTO;
	case ID_SORT_BYALLOCBY:			return TDCC_ALLOCBY;
	case ID_SORT_BYSTATUS:			return TDCC_STATUS;
	case ID_SORT_BYCATEGORY:		return TDCC_CATEGORY;
	case ID_SORT_BYTAG:				return TDCC_TAGS;
	case ID_SORT_BYPERCENT:			return TDCC_PERCENT;
	case ID_SORT_BYTIMEEST:			return TDCC_TIMEEST;
	case ID_SORT_BYTIMESPENT:		return TDCC_TIMESPENT;
	case ID_SORT_BYSTARTDATE:		return TDCC_STARTDATE;
	case ID_SORT_BYDUEDATE:			return TDCC_DUEDATE;
	case ID_SORT_BYDONEDATE:		return TDCC_DONEDATE; 
	case ID_SORT_BYDONE:			return TDCC_DONE;
	case ID_SORT_BYPRIORITY:		return TDCC_PRIORITY;
	case ID_SORT_BYCREATEDBY:		return TDCC_CREATEDBY;
	case ID_SORT_BYCREATIONDATE:	return TDCC_CREATIONDATE;
	case ID_SORT_BYMODIFYDATE:		return TDCC_LASTMOD;
	case ID_SORT_BYRISK:			return TDCC_RISK;
	case ID_SORT_BYEXTERNALID:		return TDCC_EXTERNALID;
	case ID_SORT_BYCOST:			return TDCC_COST;
	case ID_SORT_BYVERSION:			return TDCC_VERSION;
	case ID_SORT_BYRECURRENCE:		return TDCC_RECURRENCE;
	case ID_SORT_NONE:				return TDCC_NONE;
	case ID_SORT_BYFLAG:			return TDCC_FLAG;
	case ID_SORT_BYREMAINING:		return TDCC_REMAINING;
	case ID_SORT_BYRECENTEDIT:		return TDCC_RECENTEDIT;
	case ID_SORT_BYICON:			return TDCC_ICON;
	case ID_SORT_BYFILEREF:			return TDCC_FILEREF;
	case ID_SORT_BYTIMETRACKING:	return TDCC_TRACKTIME;
	case ID_SORT_BYPATH:			return TDCC_PATH;
	case ID_SORT_BYCOLOR:			return TDCC_COLOR;
	case ID_SORT_BYDEPENDENCY:		return TDCC_DEPENDENCY;
	case ID_SORT_BYPOSITION:		return TDCC_POSITION;
	}
	
	// handle custom columns
	if (nSortID >= ID_SORT_BYCUSTOMCOLUMN_FIRST && nSortID <= ID_SORT_BYCUSTOMCOLUMN_LAST)
		return (TDC_COLUMN)(TDCC_CUSTOMCOLUMN_FIRST + (nSortID - ID_SORT_BYCUSTOMCOLUMN_FIRST));

	// all else
	ASSERT (0);
	return TDCC_NONE;
}

UINT CToDoListWnd::GetSortID(TDC_COLUMN nSortBy) 
{
	switch (nSortBy)
	{
	case TDCC_CLIENT:		return ID_SORT_BYNAME;
	case TDCC_ID:			return ID_SORT_BYID;
	case TDCC_ALLOCTO:		return ID_SORT_BYALLOCTO;
	case TDCC_ALLOCBY:		return ID_SORT_BYALLOCBY;
	case TDCC_STATUS:		return ID_SORT_BYSTATUS;
	case TDCC_CATEGORY:		return ID_SORT_BYCATEGORY;
	case TDCC_TAGS:			return ID_SORT_BYTAG;
	case TDCC_PERCENT:		return ID_SORT_BYPERCENT;
	case TDCC_TIMEEST:		return ID_SORT_BYTIMEEST;
	case TDCC_TIMESPENT:	return ID_SORT_BYTIMESPENT;
	case TDCC_STARTDATE:	return ID_SORT_BYSTARTDATE;
	case TDCC_DUEDATE:		return ID_SORT_BYDUEDATE;
	case TDCC_DONEDATE:		return ID_SORT_BYDONEDATE;
	case TDCC_DONE:			return ID_SORT_BYDONE;
	case TDCC_PRIORITY:		return ID_SORT_BYPRIORITY;
	case TDCC_FLAG:			return ID_SORT_BYFLAG;
	case TDCC_CREATEDBY:	return ID_SORT_BYCREATEDBY;
	case TDCC_CREATIONDATE:	return ID_SORT_BYCREATIONDATE;
	case TDCC_LASTMOD:		return ID_SORT_BYMODIFYDATE;
	case TDCC_RISK:			return ID_SORT_BYRISK;
	case TDCC_EXTERNALID:	return ID_SORT_BYEXTERNALID;
	case TDCC_COST:			return ID_SORT_BYCOST;
	case TDCC_VERSION:		return ID_SORT_BYVERSION;
	case TDCC_RECURRENCE:	return ID_SORT_BYRECURRENCE;
	case TDCC_REMAINING:	return ID_SORT_BYREMAINING;
	case TDCC_RECENTEDIT:	return ID_SORT_BYRECENTEDIT;
	case TDCC_NONE:			return ID_SORT_NONE;
	case TDCC_ICON:			return ID_SORT_BYICON;
	case TDCC_FILEREF:		return ID_SORT_BYFILEREF;
	case TDCC_TRACKTIME:	return ID_SORT_BYTIMETRACKING;
	case TDCC_PATH:			return ID_SORT_BYPATH;
	case TDCC_COLOR:		return ID_SORT_BYCOLOR;
	case TDCC_POSITION:		return ID_SORT_BYPOSITION;
	}
	
	// handle custom columns
	if (nSortBy >= TDCC_CUSTOMCOLUMN_FIRST && nSortBy < TDCC_CUSTOMCOLUMN_LAST)
		return (ID_SORT_BYCUSTOMCOLUMN_FIRST + (nSortBy - TDCC_CUSTOMCOLUMN_FIRST));

	// all else
	ASSERT (0);
	return 0;
}

void CToDoListWnd::OnNewtaskAttopSelected() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATTOPOFSELTASKPARENT));
}

void CToDoListWnd::OnNewtaskAtbottomSelected() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATBOTTOMOFSELTASKPARENT));
}

void CToDoListWnd::OnNewtaskAfterselectedtask() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTAFTERSELTASK));
}

void CToDoListWnd::OnNewtaskBeforeselectedtask() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTBEFORESELTASK));
}

void CToDoListWnd::OnNewsubtaskAtbottom() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATBOTTOMOFSELTASK));
}

void CToDoListWnd::OnNewsubtaskAttop() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATTOPOFSELTASK));
}

void CToDoListWnd::OnNewtaskAttop() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATTOP));
}

void CToDoListWnd::OnNewtaskAtbottom() 
{
	VERIFY (CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATBOTTOM));
}

BOOL CToDoListWnd::CreateNewTask(LPCTSTR szTitle, TDC_INSERTWHERE nInsertWhere, BOOL bEdit)
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	return (tdc.CreateNewTask(szTitle, nInsertWhere, bEdit) != NULL);
}

void CToDoListWnd::OnUpdateSort(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.IsSortable() && tdc.GetTaskCount());	
}

void CToDoListWnd::OnEditTaskcolor() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (!tdc.IsReadOnly() && tdc.HasSelection())
	{
		CEnColorDialog dialog(tdc.GetSelectedTaskColor(), CC_FULLOPEN | CC_RGBINIT);
		
		if (dialog.DoModal() == IDOK)
			tdc.SetSelectedTaskColor(dialog.GetColor());
	}
}


void CToDoListWnd::OnEditCleartaskcolor() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (!tdc.IsReadOnly() && tdc.HasSelection())
		tdc.ClearSelectedTaskColor();
}

void CToDoListWnd::OnUpdateEditCleartaskcolor(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.HasSelection() && 
					Prefs().GetTextColorOption() == COLOROPT_DEFAULT &&
					tdc.GetSelectedTaskColor() != 0);	
}

void CToDoListWnd::OnEditTaskdone() 
{
	GetToDoCtrl().SetSelectedTaskDone(!GetToDoCtrl().IsSelectedTaskDone());
}

void CToDoListWnd::OnEditTasktext() 
{
	GetToDoCtrl().EditSelectedTask();
}

void CToDoListWnd::OnTrayIconClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	SetFocus();
	Show(Prefs().GetToggleTrayVisibility());
	*pResult = 0;
}

LRESULT CToDoListWnd::OnToDoListShowWindow(WPARAM /*wp*/, LPARAM /*lp*/)
{
	Show(FALSE);
	return 0;
}

LRESULT CToDoListWnd::OnToDoListGetVersion(WPARAM /*wp*/, LPARAM /*lp*/)
{
	return GetVersion();
}

LRESULT CToDoListWnd::OnToDoListRefreshPrefs(WPARAM /*wp*/, LPARAM /*lp*/)
{
	// sent by the app object if registry settings have changed
	ResetPrefs();

	// mark all tasklists as needing update
	m_mgrToDoCtrls.SetAllNeedPreferenceUpdate(TRUE);
	
	// then update active tasklist
	UpdateToDoCtrlPreferences();

	return 0;
}

void CToDoListWnd::OnTrayIconDblClk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
	Show(FALSE);
	
	*pResult = 0;
}

void CToDoListWnd::OnTrayiconCreatetask() 
{
	Show(FALSE);
	
	// create a task at the top of the tree
	GetToDoCtrl().CreateNewTask(CEnString(IDS_TASK), TDC_INSERTATTOP);
}

void CToDoListWnd::OnTrayIconRClick(NMHDR* pNMHDR, LRESULT* pResult)
{
	SetForegroundWindow();
	
	// show context menu
	CEnMenu menu;
	
	if (menu.LoadMenu(IDR_MISC, m_trayIcon.GetSafeHwnd(), TRUE))
	{
		CMenu* pSubMenu = menu.GetSubMenu(TRAYICON);
		pSubMenu->SetDefaultItem(ID_TRAYICON_SHOW);
		
		if (pSubMenu)
		{
			m_mgrToDoCtrls.PreparePopupMenu(*pSubMenu, ID_TRAYICON_SHOWDUETASKS1);
			
			NM_TRAYICON* pNMTI = (NM_TRAYICON*)pNMHDR;

			// in order to ensure that multiple password dialogs cannot 
			// appear we must make sure that all the command handling is
			// done before we return from here
			UINT nCmdID = ::TrackPopupMenu(*pSubMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, 
											pNMTI->ptAction.x, pNMTI->ptAction.y, 0, *this, NULL);

			PostMessage(WM_NULL);

			if (nCmdID != (UINT)-1)
				SendMessage(WM_COMMAND, nCmdID);
		}
	}
	
	*pResult = 0;
}

void CToDoListWnd::OnClose() 
{
	if (!m_bEndingSession)
	{
		int nSysTrayOption = Prefs().GetSysTrayOption();
		
		if (nSysTrayOption == STO_ONCLOSE || nSysTrayOption == STO_ONMINCLOSE)
			MinimizeToTray();
		
		else // shutdown but user can cancel
			DoExit();
	}
	// else we've already shutdown
}

void CToDoListWnd::MinimizeToTray()
{
	// end whatever the user is doing
	GetToDoCtrl().Flush();

	// save prev state so we can restore properly
	CPreferences().WriteProfileInt(_T("Pos"), _T("Maximized"), IsZoomed());
	
	if (Prefs().GetAutoSaveOnSwitchApp())
	{
		// save all
		SaveAll(TDLS_FLUSH | TDLS_AUTOSAVE);
	}
	
	// hide main window
	Gui::MinToTray(*this); // courtesy of floyd
	m_bVisible = FALSE;
	
	// hide find dialog
	ShowFindDialog(FALSE);
}

void CToDoListWnd::ShowFindDialog(BOOL bShow)
{
	if (bShow)
	{
		if (m_bVisible && m_findDlg.GetSafeHwnd() && IsWindowVisible())
			m_findDlg.Show(TRUE);
	}
	else // hide
	{
		if (m_findDlg.GetSafeHwnd())
		{
			m_bFindShowing = m_findDlg.IsWindowVisible();
			m_findDlg.Show(FALSE);
		}
		else
			m_bFindShowing = FALSE;
	}
}

void CToDoListWnd::OnTrayiconClose() 
{
	DoExit();
}

LRESULT CToDoListWnd::OnToDoCtrlNotifyListChange(WPARAM /*wp*/, LPARAM lp)
{
	// decide whether the filter controls need updating
	switch (lp)
	{
	case TDCA_ALLOCTO:
	case TDCA_ALLOCBY:
	case TDCA_STATUS:
	case TDCA_CATEGORY:
	case TDCA_VERSION:
	case TDCA_TAGS:
		RefreshFilterControls();
		break;
	}
	
	return 0L;
}

LRESULT CToDoListWnd::OnToDoCtrlNotifyViewChange(WPARAM wp, LPARAM lp)
{
	if (GetTDCCount())
	{
		if (lp != (LPARAM)wp)
		{
			CFocusWatcher::UpdateFocus();
			m_filterBar.RefreshFilterControls(GetToDoCtrl());
		}
		else
		{
			int breakpoint = 0;
		}
	}

	return 0L;
}

LRESULT CToDoListWnd::OnToDoCtrlNotifyTimeTrack(WPARAM /*wp*/, LPARAM lp)
{
	BOOL bTrack = lp;

	if (bTrack && Prefs().GetExclusiveTimeTracking())
	{
		// end time tracking on every other tasklist
		int nSel = GetSelToDoCtrl();
		ASSERT (nSel != -1);

		for (int nCtrl = 0; nCtrl < GetTDCCount(); nCtrl++)
		{
			if (nCtrl != nSel)
				GetToDoCtrl(nCtrl).EndTimeTracking();
		}
	}

	return 0L;
}
	
LRESULT CToDoListWnd::OnToDoCtrlNotifyRecreateRecurringTask(WPARAM wp, LPARAM lp)
{
	DWORD dwTaskID = wp, dwNewTaskID = lp;

	// is there a reminder that we need to copy for the new task
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	TDCREMINDER rem;

	int nRem = m_reminders.FindReminder(dwTaskID, &tdc);

	if (nRem != -1)
	{
		// get the existing reminder
		m_reminders.GetReminder(nRem, rem);

		// init for new task
		rem.bEnabled = TRUE;
		rem.dDaysSnooze = 0.0;
		rem.dwTaskID = dwNewTaskID;

		// add for the new task ID
		m_reminders.SetReminder(rem);

		// delete the original only if the task id has changed
		if (dwNewTaskID != dwTaskID)		
			m_reminders.RemoveReminder(dwTaskID, rem.pTDC);
	}

	return 0L;
}

LRESULT CToDoListWnd::OnToDoCtrlNotifyMod(WPARAM wp, LPARAM lp)
{
	int nTDC = m_mgrToDoCtrls.FindToDoCtrl((HWND)wp);

	if (nTDC == -1)
	{
		// could be a notification from a TDC not yet added
		return 0L;
	}

	BOOL bWasMod = m_mgrToDoCtrls.GetModifiedStatus(nTDC);
	m_mgrToDoCtrls.SetModifiedStatus(nTDC, TRUE);

	// update the caption only if the control was not previously modified
	// or the project name changed
	if (!bWasMod)
		UpdateCaption();

	TDC_ATTRIBUTE nAttrib = (TDC_ATTRIBUTE)lp;

	switch (nAttrib)
	{
	case TDCA_PROJNAME:
		{
			// update caption if not already done
			if (bWasMod)
				UpdateCaption();

			// update tab order
			if (Prefs().GetKeepTabsOrdered())
				RefreshTabOrder();
		}
		break;

	// update due items
	case TDCA_DONEDATE:
	case TDCA_DUEDATE:
		OnTimerDueItems(nTDC);
		break;

	// reminders
	case TDCA_DELETE:
		m_reminders.RemoveDeletedTaskReminders(&GetToDoCtrl(nTDC));
		break;
	}

	// status bar
	UpdateStatusbar();

	// refresh toolbar states
	PostMessage(WM_IDLEUPDATECMDUI, TRUE);

	// do we need to update the current todoctrl's
	// custom attributes on the find dialog?
	if (m_findDlg.GetSafeHwnd() && nAttrib == TDCA_CUSTOMATTRIBDEFS)
	{
		UpdateFindDialogCustomAttributes(&GetToDoCtrl());
	}

	return 0L;
}

void CToDoListWnd::UpdateCaption()
{
	int nSel = GetSelToDoCtrl();
	
	CString sProjectName = m_mgrToDoCtrls.UpdateTabItemText(nSel);
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	UINT nIDStatus = 0;
	
	if (m_mgrToDoCtrls.GetReadOnlyStatus(nSel) > 0)
		nIDStatus = IDS_STATUSREADONLY;
	
	else if (tdc.CompareFileFormat() == TDCFF_NEWER)
		nIDStatus = IDS_STATUSNEWERFORMAT;
	
	else if (m_mgrToDoCtrls.GetReadOnlyStatus(nSel) == 0 && 
		m_mgrToDoCtrls.PathSupportsSourceControl(nSel))
	{
		if (tdc.IsCheckedOut())
			nIDStatus = IDS_STATUSCHECKEDOUT;
		else
			nIDStatus = IDS_STATUSCHECKEDIN;
	}
	else if (!Prefs().GetEnableSourceControl() && m_mgrToDoCtrls.IsSourceControlled(nSel))
	{
		nIDStatus = IDS_STATUSSOURCECONTROLLED;
	}
	
	CString sCaption, sCopyright(MAKEINTRESOURCE(IDS_COPYRIGHT));
	CEnString sStatus(nIDStatus);
	
	// add current focus text as required
	if (nIDStatus)
	{
		if (m_bShowStatusBar || m_sCurrentFocus.IsEmpty())
			sCaption.Format(_T("%s [%s] - %s"), sProjectName, sStatus, sCopyright);
		else
			sCaption.Format(_T("%s [%s][%s] - %s"), sProjectName, m_sCurrentFocus, sStatus, sCopyright);
	}
	else
	{
		if (m_bShowStatusBar || m_sCurrentFocus.IsEmpty())
			sCaption.Format(_T("%s - %s"), sProjectName, sCopyright);
		else
			sCaption.Format(_T("%s [%s] - %s"), sProjectName, m_sCurrentFocus, sCopyright);
	}
	
	// prepend task pathname if tasklist not visible
	if (m_nMaxState == TDCMS_MAXCOMMENTS)
	{
		// quote the path to help it stand-out
		CString sTaskPath;
		sTaskPath.Format(_T("\"%s\""), tdc.GetSelectedTaskPath(TRUE, 100));
		
		if (!sTaskPath.IsEmpty())
			sCaption = sTaskPath + " - " + sCaption;
	}
	
	SetWindowText(sCaption);

	// set tray tip too
	UpdateTooltip();
}

void CToDoListWnd::UpdateTooltip()
{
    // base the tooltip on our current caption
    CString sTooltip;
    GetWindowText(sTooltip);

    // move copyright to next line and remove '-'
    sTooltip.Replace(_T(" - "), _T("\n"));

    // prefix with selected task as first line
    CFilteredToDoCtrl& tdc = GetToDoCtrl();
    DWORD dwSelID = tdc.GetSelectedTaskID();

    if (dwSelID)
    {
        CString sSelItem = tdc.GetTaskTitle(dwSelID);

        // maximum length of tooltip is 128 including null
        if (sSelItem.GetLength() > (128 - sTooltip.GetLength() - 6))
        {
            sSelItem = sSelItem.Left(128 - sTooltip.GetLength() - 6);
            sSelItem += _T("...");
        }
        else if (tdc.GetSelectedCount() > 1)
            sSelItem += _T(", ...");

        sTooltip = sSelItem + _T("\n") + sTooltip;
    }

    m_trayIcon.SetTip(sTooltip);
}

BOOL CToDoListWnd::Export2Html(const CTaskFile& tasks, LPCTSTR szFilePath, LPCTSTR szStylesheet) const
{
	CWaitCursor cursor;
	
	if (FileMisc::FileExists(szStylesheet))
	{
		return tasks.TransformToFile(szStylesheet, szFilePath, Prefs().GetHtmlCharSet());
	}
	
	// else default export
	return m_mgrImportExport.ExportTaskListToHtml(&tasks, szFilePath);
}

void CToDoListWnd::OnSaveas() 
{
	int nSel = GetSelToDoCtrl();
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nSel);

	// use tab text as hint to user
	CString sFilePath = m_mgrToDoCtrls.GetFilePath(nSel, FALSE);
	CPreferences prefs;

	// display the dialog
	CFileSaveDialog dialog(IDS_SAVETASKLISTAS_TITLE,
							GetDefaultFileExt(), 
							sFilePath, 
							EOFN_DEFAULTSAVE,
							GetFileFilter(FALSE), 
							this);
	
	// always use the tasklist's own format for initializing the file dialog
	dialog.m_ofn.nFilterIndex = tdc.IsUnicode() ? 2 : 1;
	
	int nRes = dialog.DoModal(&prefs);
	
	if (nRes == IDOK)
	{
		BOOL bWasUnicode = tdc.IsUnicode();
		BOOL bUnicode = (dialog.m_ofn.nFilterIndex == 2);
		tdc.SetUnicode(bUnicode);

		// save this choice as the best hint for the next new tasklist
		CPreferences().WriteProfileInt(PREF_KEY, _T("UnicodeNewTasklists"), bUnicode);

 		if (SaveTaskList(nSel, dialog.GetPathName()) == TDCO_SUCCESS)
		{
			//m_mgrToDoCtrls.ClearWebDetails(nSel); // because it's no longer a remote file
		}
		else // restore previous file format
			tdc.SetUnicode(bWasUnicode);

		UpdateStatusbar();
	}
}

void CToDoListWnd::OnUpdateSaveas(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(tdc.GetTaskCount() || tdc.IsModified());
}

void CToDoListWnd::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	static UINT nActiveMenuID = 0; // prevent reentrancy
	UINT nMenuID = 0;
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (pWnd == &m_tabCtrl)
	{
		// if point.x,y are both -1 then we just use the cursor pos
		// which is what windows appears to do mostly/sometimes
		if (point.x == -1 && point.y == -1)
		{
			CRect rTab;
			
			if (m_tabCtrl.GetItemRect(m_tabCtrl.GetCurSel(), rTab))
			{
				point = rTab.CenterPoint();
				m_tabCtrl.ClientToScreen(&point);
				
				// load popup menu
				nMenuID = TABCTRLCONTEXT;
			}
		}
		else
		{
			// activate clicked tab
			TCHITTESTINFO tcht = { { point.x, point.y }, TCHT_NOWHERE  };
			m_tabCtrl.ScreenToClient(&tcht.pt);
			
			int nTab = m_tabCtrl.HitTest(&tcht);
			
			if (nTab != -1 && !(tcht.flags & TCHT_NOWHERE))
			{
				if (nTab != m_tabCtrl.GetCurSel())
				{
					if (!SelectToDoCtrl(nTab, TRUE))
						return; // user cancelled
				}
				
				m_tabCtrl.SetFocus(); // give user feedback
				
				// load popup menu
				nMenuID = TABCTRLCONTEXT;
			}
		}
	}
	else if (pWnd == (CWnd*)&tdc) // try active todoctrl
	{
		TDC_HITTEST nHit = tdc.HitTest(point);

		switch (nHit)
		{
		case TDCHT_NOWHERE:
			break;

		case TDCHT_TASKLIST:
		case TDCHT_TASK:
			if (tdc.WantTaskContextMenu())
			{
				nMenuID = TASKCONTEXT;

				// if point.x,y are both -1 then we request the current 
				// selection position
				if (point.x == -1 && point.y == -1)
				{
					CRect rSelection;

					if (tdc.GetSelectionBoundingRect(rSelection))
					{
						tdc.ClientToScreen(rSelection);

						point.x = min(rSelection.left + 50, rSelection.CenterPoint().x);
						point.y = rSelection.top + 8;
					}
					else
					{
						nMenuID = 0; // no context menu
					}
				}
			}
			break;

		case TDCHT_COLUMNHEADER:
			nMenuID = HEADERCONTEXT;
			break;
		}
	}
	
	// show the menu
	if (nMenuID && nMenuID != nActiveMenuID)
	{
		CEnMenu menu;
		
		if (menu.LoadMenu(IDR_MISC, NULL, TRUE))
		{
			CMenu* pPopup = menu.GetSubMenu(nMenuID);
			
			if (pPopup)
			{
				// some special handling
				switch (nMenuID)
				{
				case TASKCONTEXT:
					m_nContextColumnID = tdc.ColumnHitTest(point);
					PrepareEditMenu(pPopup);
					break;

				case TABCTRLCONTEXT:
					PrepareSourceControlMenu(pPopup);
					break;
				}
				
				CToolbarHelper::PrepareMenuItems(pPopup, this);
				
				nActiveMenuID = nMenuID;
				pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
				nActiveMenuID = 0;
			}
		}
	}
	else
		CFrameWnd::OnContextMenu(pWnd, point);
}

void CToDoListWnd::OnTrayiconShow() 
{
	Show(FALSE);
}

void CToDoListWnd::OnTrayiconShowDueTasks(UINT nCmdID) 
{
	int nTDC = nCmdID - ID_TRAYICON_SHOWDUETASKS1;
	int nSelTDC = GetSelToDoCtrl();

	// verify password if encrypted tasklist is active
	// unless app is already visible
	if (!m_bVisible || IsIconic() || (nTDC != nSelTDC))
	{
		if (!VerifyToDoCtrlPassword(nTDC))
			return;
	}

	CFilteredToDoCtrl& tdc = GetToDoCtrl(nTDC);

	if (!DoDueTaskNotification(nTDC, PFP_DUETODAY))
	{
		CEnString sMessage(IDS_NODUETODAY, m_mgrToDoCtrls.GetFriendlyProjectName(nTDC));
		MessageBox(sMessage);//, IDS_DUETASKS_TITLE);
	}
}

LRESULT CToDoListWnd::OnHotkey(WPARAM /*wp*/, LPARAM /*lp*/)
{
	Show(TRUE);
	return 0L;
}

BOOL CToDoListWnd::VerifyToDoCtrlPassword() const
{
	return VerifyToDoCtrlPassword(GetSelToDoCtrl());
}

BOOL CToDoListWnd::VerifyToDoCtrlPassword(int nIndex) const
{
	if (m_bPasswordPrompting)
	{
		CEnString sExplanation(IDS_SELECTENCRYPTED, 
								m_mgrToDoCtrls.GetFriendlyProjectName(nIndex));

		return GetToDoCtrl(nIndex).VerifyPassword(sExplanation);
	}
	
	// else
	return TRUE;
}

void CToDoListWnd::Show(BOOL bAllowToggle)
{
	if (GetSelToDoCtrl() == -1)
		return;
	
	if (!m_bVisible || !IsWindowVisible()) // restore from the tray
	{
		SetForegroundWindow();
		
		if (!VerifyToDoCtrlPassword())
			return;

		m_bVisible = TRUE;
		Gui::RestoreFromTray(*this, CPreferences().GetProfileInt(_T("Pos"), _T("Maximized"), FALSE));

		// restore find dialog
		if (m_bFindShowing)
			m_findDlg.Show();
	}
	else if (IsIconic())
	{
		SetForegroundWindow();
		ShowWindow(SW_RESTORE); // this will force a password check
	}
	// if we're already visible then either bring to the foreground 
	// or hide if we're right at the top of the z-order
	else if (!bAllowToggle || Gui::IsObscured(*this) || !Gui::HasFocus(*this, TRUE))
		SetForegroundWindow();

	else if (Prefs().GetSysTrayOption() == STO_NONE)
		ShowWindow(SW_MINIMIZE);

	else // hide to system tray
		MinimizeToTray();
	
	// refresh all tasklists if we are visible
	if (m_bVisible && !IsIconic())
	{
		const CPreferencesDlg& userPrefs = Prefs();
		
		if (userPrefs.GetReadonlyReloadOption() != RO_NO)
			OnTimerReadOnlyStatus();
		
		if (userPrefs.GetTimestampReloadOption() != RO_NO)
			OnTimerTimestampChange();
		
		if (userPrefs.GetEnableSourceControl())
			OnTimerCheckoutStatus();
	}	

	GetToDoCtrl().SetFocusToTasks();
}

#ifdef _DEBUG
void CToDoListWnd::OnDebugEndSession() 
{ 
	SendMessage(WM_QUERYENDSESSION); 
	SendMessage(WM_ENDSESSION, 1, 0); 
}

void CToDoListWnd::OnDebugShowSetupDlg() 
{ 
	CTDLWelcomeWizard dialog;
	dialog.DoModal();
}
#endif

void CToDoListWnd::TranslateUIElements() 
{ 
	// show progress bar
	DOPROGRESS(IDS_UPDATINGDICTIONARY)

	// disable translation of top-level menu names in
	// IDR_MISC, IDR_PLACEHOLDERS, IDR_TREEDRAGDROP
	CLocalizer::IgnoreString(_T("TrayIcon"));
	CLocalizer::IgnoreString(_T("TaskContext"));
	CLocalizer::IgnoreString(_T("TabCtrl"));
	CLocalizer::IgnoreString(_T("TasklistHeader"));
	CLocalizer::IgnoreString(_T("CommentsPopup"));
	CLocalizer::IgnoreString(_T("ToolsDialog"));
	CLocalizer::IgnoreString(_T("TreeDragDrop"));

	// disable light box manager temporarily
	// to avoid the disabling effect whenever 
	// a dialog is created
	if (Prefs().GetEnableLightboxMgr())
		CLightBoxMgr::Release();

	CLocalizer::ForceTranslateAllUIElements(IDS_FIRSTSTRING, IDS_LASTSTRING);

	// force a redraw of whole UI
	if (IsWindowVisible())
	{
		ShowWindow(SW_HIDE);
		ShowWindow(SW_SHOW);
	}
	
	SetForegroundWindow();

	// restore light box manager
	if (Prefs().GetEnableLightboxMgr())
		CLightBoxMgr::Initialize(this, m_theme.crAppBackDark);
}

void CToDoListWnd::OnUpdateRecentFileMenu(CCmdUI* pCmdUI) 
{
	// check that this is not occurring because our CFrameWnd
	// base class is routing this to the first item in a submenu
	if (pCmdUI->m_pMenu && 
		pCmdUI->m_pMenu->GetMenuItemID(pCmdUI->m_nIndex) == (UINT)-1)
		return;

	m_mruList.UpdateMenu(pCmdUI);	
}

BOOL CToDoListWnd::OnOpenRecentFile(UINT nID)
{
	ASSERT(nID >= ID_FILE_MRU_FILE1);
	ASSERT(nID < ID_FILE_MRU_FILE1 + (UINT)m_mruList.GetSize());
	
	int nIndex = nID - ID_FILE_MRU_FILE1;
	
	CString sTaskList = m_mruList[nIndex];
	TDC_FILE nOpen = OpenTaskList(sTaskList);
	
	if (nOpen == TDCO_SUCCESS)
	{
		Resize();
		UpdateWindow();
	}
	else
	{
		HandleLoadTasklistError(nOpen, sTaskList);
		
		if (nOpen != TDCO_CANCELLED)
			m_mruList.Remove(nIndex);
	}

	RefreshTabOrder();
	
	// always return TRUE to say we handled it
	return TRUE;
}

void CToDoListWnd::RefreshTabOrder()
{
	if (Prefs().GetKeepTabsOrdered())
	{
		int nSelOrg = GetSelToDoCtrl();
		int nSel = m_mgrToDoCtrls.SortToDoCtrlsByName();
		
		if (nSel != nSelOrg)
			SelectToDoCtrl(nSel, FALSE);
	}
}

TDC_FILE CToDoListWnd::DelayOpenTaskList(LPCTSTR szFilePath)
{
	ASSERT (Prefs().GetEnableDelayedLoading()); // sanity check

	// decode/prepare filepath
	CString sFilePath(szFilePath);
	TSM_TASKLISTINFO storageInfo;

	TDC_PREPAREPATH nPathType = PrepareFilePath(sFilePath, &storageInfo);

	if (nPathType == TDCPP_NONE)
		return TDCO_NOTEXIST;

	// see if the tasklist is already open
	if (SelectToDoCtrl(sFilePath, TRUE))
		return TDCO_SUCCESS;

	// delay load the file, visible but disabled
	CFilteredToDoCtrl* pTDC = NewToDoCtrl(TRUE, FALSE);

	// if this is a 'special' temp file then assume TDL automatically
	// named it when handling WM_ENDSESSION. 
	BOOL bDelayLoad = !IsEndSessionFilePath(sFilePath);
	COleDateTime dtEarliest;

	if (bDelayLoad)
		bDelayLoad = pTDC->DelayLoad(sFilePath, dtEarliest);

	if (bDelayLoad)
	{
		// now we have to check for whether the tasklist has due tasks 
		// and the user wants notification
		int nNotifyDueBy = Prefs().GetNotifyDueByOnLoad();

		if (nNotifyDueBy != PFP_DONTNOTIFY && CDateHelper::IsDateSet(dtEarliest.m_dt))
		{
			// check the date against when the user wants notifying
			DH_DATE nDate = DHD_TODAY;
			
			switch (nNotifyDueBy)
			{
			case PFP_DUETODAY:		/*nDate = DHD_TODAY;*/		break;
			case PFP_DUETOMORROW:	nDate = DHD_TOMORROW;		break;
			case PFP_DUETHISWEEK:	nDate = DHD_ENDTHISWEEK;	break;
			case PFP_DUENEXTWEEK:	nDate = DHD_ENDNEXTWEEK;	break;
			case PFP_DUETHISMONTH:	nDate = DHD_ENDTHISMONTH;	break;
			case PFP_DUENEXTMONTH:	nDate = DHD_ENDNEXTMONTH;	break;
			default:				ASSERT (0);
			}
			
			COleDateTime dtDueWhen = CDateHelper::GetDate(nDate);
			
			bDelayLoad = (dtDueWhen < dtEarliest);
		}
	}

	// if the delay load failed for any reason we need to delete the tasklist
	// and fallback on the default load mechanism
	if (!bDelayLoad)
	{
		pTDC->DestroyWindow();
		delete pTDC;

		// note: we use the original filepath in case it is 
		// actually storage info
		return OpenTaskList(szFilePath, FALSE);
	}
	
	int nCtrl = m_mgrToDoCtrls.AddToDoCtrl(pTDC, &storageInfo, FALSE); // FALSE == not yet loaded
	
	// update due item status
	if (CDateHelper::IsDateSet(dtEarliest))
	{
		TDCM_DUESTATUS nStatus = TDCM_FUTURE;
		COleDateTime dtToday = COleDateTime::GetCurrentTime();

		if (floor(dtEarliest) < floor(dtToday))
			nStatus = TDCM_PAST;

		else if (floor(dtEarliest) == floor(dtToday))
			nStatus = TDCM_TODAY;

		m_mgrToDoCtrls.SetDueItemStatus(nCtrl, nStatus);
	}
		
	return TDCO_SUCCESS;
}

CString CToDoListWnd::GetEndSessionFilePath()
{
	return FileMisc::GetTempFileName(_T("tde"));
}

BOOL CToDoListWnd::IsEndSessionFilePath(const CString& sFilePath)
{
	if (!FileMisc::FileExists(sFilePath))
		return FALSE;

	if (!FileMisc::HasExtension(sFilePath, _T("tmp")))
		return FALSE;
	
	if (!FileMisc::IsTempFile(sFilePath))
		return FALSE;

	if (FileMisc::GetFileNameFromPath(sFilePath).Find(_T("tde")) != 0)
		return FALSE;

	// passed all the tests
	return TRUE;
}

TDC_ARCHIVE CToDoListWnd::GetAutoArchiveOptions(LPCTSTR szFilePath, CString& sArchivePath, BOOL& bRemoveFlagged) const
{
	TDC_ARCHIVE nRemove = TDC_REMOVENONE;
	bRemoveFlagged = FALSE;

	const CPreferencesDlg& userPrefs = Prefs();
	
	if (userPrefs.GetAutoArchive())
	{
		if (userPrefs.GetRemoveArchivedTasks())
		{
			if (userPrefs.GetRemoveOnlyOnAbsoluteCompletion())
				nRemove = TDC_REMOVEIFSIBLINGSANDSUBTASKSCOMPLETE;
			else
				nRemove = TDC_REMOVEALL;

			bRemoveFlagged = !userPrefs.GetDontRemoveFlagged();
		}
		
		sArchivePath = m_mgrToDoCtrls.GetArchivePath(szFilePath);
	}
	else
		sArchivePath.Empty();

	return nRemove;
}

TDC_FILE CToDoListWnd::OpenTaskList(LPCTSTR szFilePath, BOOL bNotifyDueTasks)
{
	CString sFilePath(szFilePath);
	TDC_PREPAREPATH nType = PrepareFilePath(sFilePath);
	
	if (nType == TDCPP_NONE)
		return TDCO_NOTEXIST;
	
	// see if the tasklist is already open
	int nExist = m_mgrToDoCtrls.FindToDoCtrl(sFilePath);
	
	if (nExist != -1)
	{
		// reload provided there are no existing changes
		// and the timestamp has changed
		if (!m_mgrToDoCtrls.GetModifiedStatus(nExist) &&
			m_mgrToDoCtrls.RefreshLastModified(nExist))
		{
			ReloadTaskList(nExist, bNotifyDueTasks);
		}
		
		// then select
		if (SelectToDoCtrl(nExist, TRUE))
			return TDCO_SUCCESS;
	}
	
	// create a new todoltrl for this tasklist 
	const CPreferencesDlg& userPrefs = Prefs();
	CFilteredToDoCtrl* pTDC = NewToDoCtrl();
	CHoldRedraw hr(pTDC->GetSafeHwnd());
	
	// handles simple and storage tasklists
	// we use szFilePath because it may be storage Info not a true path
	TSM_TASKLISTINFO storageInfo;
	TDC_FILE nOpen = OpenTaskList(pTDC, sFilePath, &storageInfo);
	
	if (nOpen == TDCO_SUCCESS)
	{
		int nTDC = AddToDoCtrl(pTDC, &storageInfo);

		// notify readonly
		CheckNotifyReadonly(nTDC);

		// reload any reminders
		m_reminders.AddToDoCtrl(*pTDC);
		
		// notify user of due tasks if req
		if (bNotifyDueTasks)
			DoDueTaskNotification(nTDC, userPrefs.GetNotifyDueByOnLoad());
		
		// save checkout status
		if (userPrefs.GetAutoCheckOut())
			m_mgrToDoCtrls.SetLastCheckoutStatus(nTDC, pTDC->IsCheckedOut());

		// check for automatic naming when handling WM_ENDSESSION. 
		// so we clear the filename and mark it as modified
		if (IsEndSessionFilePath(sFilePath))
		{
			pTDC->ClearFilePath();
			pTDC->SetModified();
		}
		
		UpdateCaption();
		UpdateStatusbar();
		OnTimerDueItems(nTDC);
		
		// update search
		if (userPrefs.GetRefreshFindOnLoad() && m_findDlg.GetSafeHwnd())
			m_findDlg.RefreshSearch();
	}
	else if (GetTDCCount() >= 1) // only delete if there's another ctrl existing
	{
		pTDC->DestroyWindow();
		delete pTDC;
	}
	else // re-add
	{
		AddToDoCtrl(pTDC);
	}
	
	return nOpen;
}

TDC_FILE CToDoListWnd::OpenTaskList(CFilteredToDoCtrl* pTDC, LPCTSTR szFilePath, TSM_TASKLISTINFO* pInfo)
{
	CString sFilePath(szFilePath);
	CTaskFile tasks;
	TDC_FILE nOpen = TDCO_UNSET;

	TSM_TASKLISTINFO storageInfo;
	TDC_PREPAREPATH nType = PrepareFilePath(sFilePath, &storageInfo);

	// handle bad path
	if ((szFilePath && *szFilePath) && sFilePath.IsEmpty())
		return TDCO_NOTEXIST;

	DOPROGRESS(IDS_LOADINGPROGRESS)

	if (sFilePath.IsEmpty())
	{
		sFilePath = pTDC->GetFilePath(); // ie. reload
	}
	else
	{
		switch (nType)
		{
		case TDCPP_STORAGE:
			{
				CPreferences prefs;

				// retrieve file from storage
				if (m_mgrStorage.RetrieveTasklist(&storageInfo, &tasks, -1, &prefs))
				{
					// handle returned tasks
					if (tasks.GetTaskCount())
					{
						// merge returned tasks with this tasklist
						// TODO

						nOpen = TDCO_SUCCESS;
					}

					// return update storage info
					if (pInfo)
						*pInfo = storageInfo;
				}
				else
					nOpen = TDCO_CANCELLED;
			}
			break;

		case TDCPP_FILE:
			{
				BOOL bSrcControl = m_mgrToDoCtrls.PathSupportsSourceControl(szFilePath);

				pTDC->SetStyle(TDCS_ENABLESOURCECONTROL, bSrcControl);
				pTDC->SetStyle(TDCS_CHECKOUTONLOAD, bSrcControl ? Prefs().GetAutoCheckOut() : FALSE);
			}
			break;

		case TDCPP_NONE:
		default:
			ASSERT(0);
			break;
		}
	}
	
	// has the load already been handled?
	if (nOpen == TDCO_UNSET)
		nOpen = pTDC->Load(sFilePath, tasks);

	if (nOpen == TDCO_SUCCESS)
	{
		// update readonly status
		m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(*pTDC);

		const CPreferencesDlg& userPrefs = Prefs();

		// certain operations cannot be performed on 'storage' tasklists
		if (nType == TDCPP_FILE)
		{
			// archive completed tasks?
			if (!pTDC->IsReadOnly())
			{
				CString sArchivePath;
				BOOL bRemoveFlagged;
				TDC_ARCHIVE nRemove = GetAutoArchiveOptions(szFilePath, sArchivePath, bRemoveFlagged);
			
				if (!sArchivePath.IsEmpty())
					pTDC->ArchiveDoneTasks(sArchivePath, nRemove, bRemoveFlagged);
			}

			if (userPrefs.GetAddFilesToMRU())
				m_mruList.Add(sFilePath);
		}

		if (userPrefs.GetExpandTasksOnLoad())
			pTDC->ExpandTasks(TDCEC_ALL);
		
		// update find dialog with this ToDoCtrl's custom attributes
		UpdateFindDialogCustomAttributes(pTDC);
	}
	else
		pTDC->SetModified(FALSE);

	return nOpen;
}

void CToDoListWnd::CheckNotifyReadonly(int nIndex) const
{
	ASSERT(nIndex != -1);

	const CPreferencesDlg& userPrefs = Prefs();

	if (nIndex >= 0 && userPrefs.GetNotifyReadOnly())
	{
		CEnString sMessage;
		CString sDisplayPath = m_mgrToDoCtrls.GetDisplayPath(nIndex);
		CString sFilePath = m_mgrToDoCtrls.GetFilePath(nIndex);
		const CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);
		
		if (CDriveInfo::IsReadonlyPath(sFilePath) > 0)
			sMessage.Format(IDS_OPENREADONLY, sDisplayPath);
		
		else if (!userPrefs.GetEnableSourceControl() && m_mgrToDoCtrls.IsSourceControlled(nIndex))
			sMessage.Format(IDS_OPENSOURCECONTROLLED, sDisplayPath);
		
		else if (tdc.CompareFileFormat() == TDCFF_NEWER)
			sMessage.Format(IDS_OPENNEWER, sDisplayPath);
		
		if (!sMessage.IsEmpty())
			MessageBox(sMessage, IDS_OPENTASKLIST_TITLE);
	}
}

void CToDoListWnd::UpdateFindDialogCustomAttributes(const CFilteredToDoCtrl* pTDC)
{
	if (pTDC == NULL && GetTDCCount() == 0)
		return; // nothing to do

	CTDCCustomAttribDefinitionArray aTDCAttribDefs, aAllAttribDefs;

	// all tasklists
	int nTDC = GetTDCCount();

	while (nTDC--)
	{
		const CFilteredToDoCtrl& tdc = GetToDoCtrl(nTDC);
		tdc.GetCustomAttributeDefs(aTDCAttribDefs);

		CTDCCustomAttributeHelper::AppendUniqueAttributes(aTDCAttribDefs, aAllAttribDefs);
	}
	
	// active tasklist
	if (pTDC == NULL)
	{
		ASSERT(GetTDCCount() > 0);
		pTDC = &GetToDoCtrl();
	}

	ASSERT (pTDC);
	pTDC->GetCustomAttributeDefs(aTDCAttribDefs);

	// do the update
	m_findDlg.SetCustomAttributes(aTDCAttribDefs, aAllAttribDefs);
}

BOOL CToDoListWnd::DoDueTaskNotification(int nDueBy)
{
	return DoDueTaskNotification(GetSelToDoCtrl(), nDueBy);
}

BOOL CToDoListWnd::DoDueTaskNotification(int nTDC, int nDueBy)
{
	// check userPrefs
	if (nDueBy == -1)
		return TRUE; // nothing to do
	
	if (nTDC != -1 && !VerifyTaskListOpen(nTDC, FALSE))
		return TRUE; // no error. user cancelled

	CFilteredToDoCtrl& tdc = m_mgrToDoCtrls.GetToDoCtrl(nTDC);

	const CPreferencesDlg& userPrefs = Prefs();
	
	// preferences
	BOOL bParentTitleCommentsOnly = userPrefs.GetExportParentTitleCommentsOnly();
	BOOL bDueTaskTitlesOnly = userPrefs.GetDueTaskTitlesOnly();
	CString sStylesheet = userPrefs.GetDueTaskStylesheet();
	BOOL bTransform = FileMisc::FileExists(sStylesheet);
	BOOL bHtmlNotify = userPrefs.GetDisplayDueTasksInHtml();
	
	DWORD dwFlags = TDCGTF_FILENAME;
	
	if (bHtmlNotify)
		dwFlags |= TDCGTF_HTMLCOMMENTS;

	if (bTransform)
		dwFlags |= TDCGTF_TRANSFORM;

	// due task notification preference overrides Export preference
	if (bDueTaskTitlesOnly)
		dwFlags |= TDCGTF_TITLESONLY;

	else if (bParentTitleCommentsOnly)
		dwFlags |= TDCGTF_PARENTTITLECOMMENTSONLY;

	TDC_GETTASKS nFilter = TDCGT_DUE;
	UINT nIDDueBy = IDS_DUETODAY;
	
	switch (nDueBy)
	{
	case PFP_DUETODAY:
		break; // done
		
	case PFP_DUETOMORROW:
		nIDDueBy = IDS_DUETOMORROW;
		nFilter = TDCGT_DUETOMORROW;
		break;
		
	case PFP_DUETHISWEEK:
		nIDDueBy = IDS_DUETHISWEEK;
		nFilter = TDCGT_DUETHISWEEK;
		break;
		
	case PFP_DUENEXTWEEK:
		nIDDueBy = IDS_DUENEXTWEEK;
		nFilter = TDCGT_DUENEXTWEEK;
		break;
		
	case PFP_DUETHISMONTH:
		nIDDueBy = IDS_DUETHISMONTH;
		nFilter = TDCGT_DUETHISMONTH;
		break;
		
	case PFP_DUENEXTMONTH:
		nIDDueBy = IDS_DUENEXTMONTH;
		nFilter = TDCGT_DUENEXTMONTH;
		break;
		
	default:
		ASSERT (0);
		return FALSE;
	}
	
	TDCGETTASKS filter(nFilter, dwFlags);
	filter.sAllocTo = userPrefs.GetDueTaskPerson();

	CTaskFile tasks;

	if (!tdc.GetTasks(tasks, filter))
		return FALSE;
	
	// set an appropriate title
	tasks.SetReportAttributes(CEnString(nIDDueBy));
	tasks.SetCharSet(userPrefs.GetHtmlCharSet());

	// save intermediate tasklist to file as required
	LogIntermediateTaskList(tasks, tdc.GetFilePath());

	// nasty hack to prevent exporters adding space for notes
	BOOL bSpaceForNotes = userPrefs.GetExportSpaceForNotes();

	if (bSpaceForNotes)
		CPreferences().WriteProfileInt(PREF_KEY, _T("ExportSpaceForNotes"), FALSE);
	
	// different file for each
	CString sTempFile;

	sTempFile.Format(_T("ToDoList.due.%d"), nTDC);
	sTempFile = FileMisc::GetTempFileName(sTempFile, bHtmlNotify ? _T("html") : _T("txt"));
			
	BOOL bRes = FALSE;
	
	if (bHtmlNotify) // display in browser?
	{
		if (bTransform)
			bRes = tasks.TransformToFile(sStylesheet, sTempFile, userPrefs.GetHtmlCharSet());
		else
			bRes = m_mgrImportExport.ExportTaskListToHtml(&tasks, sTempFile);
	}
	else
		bRes = m_mgrImportExport.ExportTaskListToText(&tasks, sTempFile);

	if (bRes)
	{
		Show(FALSE);
		m_mgrToDoCtrls.ShowDueTaskNotification(nTDC, sTempFile, bHtmlNotify);
	}

	// undo hack
	if (bSpaceForNotes)
		CPreferences().WriteProfileInt(PREF_KEY, _T("ExportSpaceForNotes"), TRUE);
	
	return TRUE;
}

CString CToDoListWnd::GetTitle() 
{
	static CString sTitle(_T("ToDoList 6.8.10 Feature Release"));
	CLocalizer::IgnoreString(sTitle);

	return sTitle;
}

void CToDoListWnd::OnAbout() 
{
	CString sVersion;
	sVersion.Format(_T("<b>%s</b> (c) AbstractSpoon 2003-14"), GetTitle());
	CLocalizer::IgnoreString(sVersion);

	CAboutDlg dialog(IDR_MAINFRAME, 
					ABS_LISTCOPYRIGHT, 
					sVersion,
					CEnString(IDS_ABOUTHEADING), 
					CEnString(IDS_ABOUTCONTRIBUTION), 
					// format the link into the license so that it is not translated
					CEnString(IDS_LICENSE, _T("\"http://www.opensource.org/licenses/eclipse-1.0.php\"")), 
					1, 1, 12, 1, 250);
	
	dialog.DoModal();
}

void CToDoListWnd::OnPreferences() 
{
	DoPreferences();
}

void CToDoListWnd::DoPreferences(int nInitPage) 
{
	// take a copy of current userPrefs to check changes against
	const CPreferencesDlg curPrefs; 
	
	// kill timers
	SetTimer(TIMER_READONLYSTATUS, FALSE);
	SetTimer(TIMER_TIMESTAMPCHANGE, FALSE);
	SetTimer(TIMER_CHECKOUTSTATUS, FALSE);
	SetTimer(TIMER_AUTOSAVE, FALSE);
	SetTimer(TIMER_TIMETRACKING, FALSE);
	SetTimer(TIMER_AUTOMINIMIZE, FALSE);

	// restore translation of dynamic menu items shortcut prefs
	EnableDynamicMenuTranslation(TRUE);
	
	ASSERT(m_pPrefs);
	UINT nRet = m_pPrefs->DoModal(nInitPage);
	
	// updates userPrefs
	RedrawWindow();
	ResetPrefs();

	const CPreferencesDlg& userPrefs = Prefs();
	
	if (nRet == IDOK)
	{
		SetUITheme(userPrefs.GetUITheme());

		// lightbox mgr
		if (curPrefs.GetEnableLightboxMgr() != userPrefs.GetEnableLightboxMgr())
		{
			if (userPrefs.GetEnableLightboxMgr())
				CLightBoxMgr::Initialize(this, m_theme.crAppBackDark);
			else
				CLightBoxMgr::Release();
		}

		// mark all todoctrls as needing refreshing
		m_mgrToDoCtrls.SetAllNeedPreferenceUpdate(TRUE); 
		
		// delete fonts if they appear to have changed
		// and recreate in UpdateToDoCtrlPrefs
		CString sFaceName;
		int nFontSize;
		
		if (!userPrefs.GetTreeFont(sFaceName, nFontSize) || !GraphicsMisc::SameFont(m_fontTree, sFaceName, nFontSize))
			GraphicsMisc::VerifyDeleteObject(m_fontTree);
		
		if (!userPrefs.GetCommentsFont(sFaceName, nFontSize) || !GraphicsMisc::SameFont(m_fontComments, sFaceName, nFontSize))
			GraphicsMisc::VerifyDeleteObject(m_fontComments);
		
		BOOL bResizeDlg = FALSE;
		
		// topmost
		BOOL bTopMost = (userPrefs.GetAlwaysOnTop() && !IsZoomed());
		
		SetWindowPos(bTopMost ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
		
		// tray icon
		m_trayIcon.ShowTrayIcon(userPrefs.GetUseSysTray());
		
		// support for .tdl
		if (curPrefs.GetEnableTDLExtension() != userPrefs.GetEnableTDLExtension())
		{
			CFileRegister filereg(_T("tdl"), _T("tdl_Tasklist"));
			
			if (userPrefs.GetEnableTDLExtension())
				filereg.RegisterFileType(_T("Tasklist"), 0);
			else
				filereg.UnRegisterFileType();
		}

		// support for tdl web protocol
		if (curPrefs.GetEnableTDLProtocol() != userPrefs.GetEnableTDLProtocol())
			EnableTDLProtocol(userPrefs.GetEnableTDLProtocol());

		// language
		CString sLangFile = userPrefs.GetLanguageFile();
		BOOL bAdd2Dict = userPrefs.GetEnableAdd2Dictionary();

		if (UpdateLanguageTranslationAndRestart(sLangFile, bAdd2Dict, curPrefs))
		{
			DoExit(TRUE);
			return;
		}

		// default task attributes
		UpdateDefaultTaskAttributes(userPrefs);

		// source control
		BOOL bSourceControl = userPrefs.GetEnableSourceControl();
		
		if (curPrefs.GetEnableSourceControl() != bSourceControl ||
			curPrefs.GetSourceControlLanOnly() != userPrefs.GetSourceControlLanOnly())
		{
			// update all open files to ensure they're in the right state
			int nCtrl = GetTDCCount();
			
			while (nCtrl--)
			{
				CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
				
				// check files in if we're disabling sc and this file is
				// checked out. however although we 
				// are checking in, the file cannot be edited by the user
				// until they remove the file from under source control
				if (!bSourceControl && tdc.IsCheckedOut())
				{
					if (tdc.IsModified())
						tdc.Save();
					
					tdc.CheckIn();
				}
				// else checkout if we're enabling and auto-checkout is also enabled
				else if (bSourceControl)
				{
					// there can be two reasons for wanting to check out a file
					// either the autocheckout preference is set or its a local
					// file which is not checked out but has been modified and source
					// control now covers all files in which case we save it first
					BOOL bPathSupports = m_mgrToDoCtrls.PathSupportsSourceControl(nCtrl);
					BOOL bNeedsSave = bPathSupports && !tdc.IsCheckedOut() && tdc.IsModified();
					BOOL bWantCheckOut = bNeedsSave || (bPathSupports && userPrefs.GetAutoCheckOut());
					
					if (bNeedsSave)
						tdc.Save(); // save silently
					
					tdc.SetStyle(TDCS_ENABLESOURCECONTROL, bPathSupports);
					tdc.SetStyle(TDCS_CHECKOUTONLOAD, bPathSupports && userPrefs.GetAutoCheckOut());
					
					if (bWantCheckOut && !tdc.IsCheckedOut())
						tdc.CheckOut();
				}

				// re-sync
				m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nCtrl);				
				m_mgrToDoCtrls.RefreshModifiedStatus(nCtrl);
				m_mgrToDoCtrls.RefreshLastModified(nCtrl);
				m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
			}
		}
		
		m_toolbar.GetToolBarCtrl().HideButton(ID_TOOLS_TOGGLECHECKIN, !bSourceControl);

		// check box in front of task title.
		// this is tricky because the checkbox won't display if the completion
		// column is visible. ie. the completion column take precedence.
		// so if the user has just turned on the checkbox in front of
		// the task title then we need to turn off the completion column
		// on all those tasklists currently showing it. But we only need to do
		// this on those tasklists which are managing their own columns else
		// it will be handled when we update the preferences in due course.
		if (userPrefs.GetTreeCheckboxes() && !curPrefs.GetTreeCheckboxes())
		{
			int nCtrl = GetTDCCount();
				
			while (nCtrl--)
			{
				if (m_mgrToDoCtrls.HasOwnColumns(nCtrl))
				{
					CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);

					if (tdc.IsColumnShowing(TDCC_DONE))
					{
						CTDCColumnIDArray aColumns;
						int nCol = tdc.GetVisibleColumns(aColumns);

						while (nCol--)
						{
							if (aColumns[nCol] == TDCC_DONE)
							{
								aColumns.RemoveAt(nCol);
								break;
							}
						}

						tdc.SetVisibleColumns(aColumns);
					}
				}
			}	
		}

		// same again for task icons
		if (userPrefs.GetTreeTaskIcons() && !curPrefs.GetTreeTaskIcons())
		{
			int nCtrl = GetTDCCount();
				
			while (nCtrl--)
			{
				if (m_mgrToDoCtrls.HasOwnColumns(nCtrl))
				{
					CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);

					if (tdc.IsColumnShowing(TDCC_ICON))
					{
						CTDCColumnIDArray aColumns;
						int nCol = tdc.GetVisibleColumns(aColumns);

						while (nCol--)
						{
							if (aColumns[nCol] == TDCC_ICON)
							{
								aColumns.RemoveAt(nCol);
								break;
							}
						}

						tdc.SetVisibleColumns(aColumns);
					}
				}
			}	
		}

		// menu icons
		UINT nPrevID = MapNewTaskPos(curPrefs.GetNewTaskPos(), FALSE);
		m_mgrMenuIcons.ChangeImageID(nPrevID, GetNewTaskCmdID());

		nPrevID = MapNewTaskPos(curPrefs.GetNewSubtaskPos(), TRUE);
		m_mgrMenuIcons.ChangeImageID(nPrevID, GetNewSubtaskCmdID());
		
		// reload menu 
		LoadMenubar();
		
		// tab bar
		bResizeDlg |= (curPrefs.GetAutoHideTabbar() != userPrefs.GetAutoHideTabbar());
		
		if (curPrefs.GetStackTabbarItems() != userPrefs.GetStackTabbarItems())
		{
			BOOL bStackTabbar = userPrefs.GetStackTabbarItems();
			
			bResizeDlg = TRUE;
			m_tabCtrl.ModifyStyle(bStackTabbar ? 0 : TCS_MULTILINE, bStackTabbar ? TCS_MULTILINE : 0);
		}
		else
			m_tabCtrl.Invalidate(); // handle priority colour changes
			
		// visible filter controls
		if (m_bShowFilterBar)
			bResizeDlg = TRUE;

		BOOL bEnableMultiSel = userPrefs.GetMultiSelFilters();
		BOOL bPrevMultiSel = curPrefs.GetMultiSelFilters();

		if (bPrevMultiSel != bEnableMultiSel)
		{
			m_filterBar.EnableMultiSelection(bEnableMultiSel);

			// if it was was previously multisel (but not now) then
			// refresh the filter because we may have gone from
			// multiple selection down to only one
			OnViewRefreshfilter();
		}

		m_filterBar.ShowDefaultFilters(userPrefs.GetShowDefaultFilters());

		// title filter option
		PUIP_MATCHTITLE nTitleOption = userPrefs.GetTitleFilterOption();
		PUIP_MATCHTITLE nPrevTitleOption = curPrefs.GetTitleFilterOption();

		if (nPrevTitleOption != nTitleOption)
			OnViewRefreshfilter();

		// inherited parent task attributes for new tasks
		CTDCAttributeArray aParentAttrib;
		BOOL bUpdateAttrib;

		userPrefs.GetParentAttribsUsed(aParentAttrib, bUpdateAttrib);
		CFilteredToDoCtrl::SetInheritedParentAttributes(aParentAttrib, bUpdateAttrib);
				
		// hotkey
		UpdateGlobalHotkey();
		
		// time periods
		CTimeHelper::SetHoursInOneDay(userPrefs.GetHoursInOneDay());
		CTimeHelper::SetDaysInOneWeek(userPrefs.GetDaysInOneWeek());
		CDateHelper::SetWeekendDays(userPrefs.GetWeekendDays());
		
		RefreshTabOrder();
		
		// time tracking
		if (curPrefs.GetTrackNonActiveTasklists() != userPrefs.GetTrackNonActiveTasklists())
			RefreshPauseTimeTracking();
		
		UpdateCaption();
		UpdateTabSwitchTooltip();

		// colours
		if (m_findDlg.GetSafeHwnd())
			m_findDlg.RefreshUserPreferences();
		
		// active tasklist userPrefs
		UpdateToDoCtrlPreferences();

		// then refresh filter bar for any new default cats, statuses, etc
		m_filterBar.RefreshFilterControls(GetToDoCtrl());
		
		if (bResizeDlg)
			Resize();

		// Stickies Support
		CString sStickiesPath;

		if (userPrefs.GetUseStickies(sStickiesPath))
			VERIFY(m_reminders.UseStickies(TRUE, sStickiesPath));
		else
			m_reminders.UseStickies(FALSE);

		// Recently modified period
		CFilteredToDoCtrl::SetRecentlyModifiedPeriod(userPrefs.GetRecentlyModifiedPeriod());
		
		// don't ask me for the full details on this but it seems as
		// though the CSysImageList class is waiting to process a 
		// message before we can switch image sizes so we put it
		// right at the end after everything is done.
		Misc::ProcessMsgLoop();
		AppendTools2Toolbar(TRUE);
	}
	
	// finally set or terminate the various status check timers
	SetTimer(TIMER_READONLYSTATUS, (userPrefs.GetReadonlyReloadOption() != RO_NO));
	SetTimer(TIMER_TIMESTAMPCHANGE, (userPrefs.GetTimestampReloadOption() != RO_NO));
	SetTimer(TIMER_AUTOSAVE, userPrefs.GetAutoSaveFrequency());
	SetTimer(TIMER_CHECKOUTSTATUS, (userPrefs.GetCheckoutOnCheckin() || userPrefs.GetAutoCheckinFrequency()));
	SetTimer(TIMER_TIMETRACKING, TRUE);
	SetTimer(TIMER_AUTOMINIMIZE, userPrefs.GetAutoMinimizeFrequency());

	// re-disable dynamic menu translation
	EnableDynamicMenuTranslation(FALSE);
}

BOOL CToDoListWnd::UpdateLanguageTranslationAndRestart(const CString& sLangFile, BOOL bAdd2Dict, 
													   const CPreferencesDlg& curPrefs)
{
	BOOL bDefLang = (CTDLLanguageComboBox::GetDefaultLanguage() == sLangFile);
		
	if (curPrefs.GetLanguageFile() != sLangFile)
	{
		if (bDefLang || FileMisc::FileExists(sLangFile))
		{
			// if the language file exists and has changed then inform the user that they to restart
			// Note: restarting will also handle bAdd2Dict
			if (MessageBox(IDS_RESTARTTOCHANGELANGUAGE, 0, MB_YESNO) == IDYES)
			{
				return TRUE;
			}
		}
	}
	// else if the the user wants to enable/disable 'Add2Dictionary'
	else if (bAdd2Dict != curPrefs.GetEnableAdd2Dictionary())
	{
		if (bAdd2Dict && !bDefLang)
		{
			CLocalizer::SetTranslationOption(ITTTO_ADD2DICTIONARY);
			TranslateUIElements();
		}
		else // disable 'Add2Dictionary'
			CLocalizer::SetTranslationOption(ITTTO_TRANSLATEONLY);
	}

	// no need to restart
	return FALSE;
}

void CToDoListWnd::UpdateDefaultTaskAttributes(const CPreferencesDlg& prefs)
{
	m_tdiDefault.sTitle = CEnString(IDS_TASK);
	m_tdiDefault.color = prefs.GetDefaultColor();
	m_tdiDefault.dateStart.m_dt = prefs.GetAutoDefaultStartDate() ? -1 : 0;
	m_tdiDefault.sAllocBy = prefs.GetDefaultAllocBy();
	m_tdiDefault.sStatus = prefs.GetDefaultStatus();
	m_tdiDefault.dTimeEstimate = prefs.GetDefaultTimeEst(m_tdiDefault.nTimeEstUnits);
	m_tdiDefault.dTimeSpent = prefs.GetDefaultTimeSpent(m_tdiDefault.nTimeSpentUnits);
	m_tdiDefault.nTimeSpentUnits = m_tdiDefault.nTimeEstUnits; // to match
	m_tdiDefault.sCreatedBy = prefs.GetDefaultCreatedBy();
	m_tdiDefault.dCost = prefs.GetDefaultCost();
	m_tdiDefault.sCommentsTypeID = prefs.GetDefaultCommentsFormat();
	m_tdiDefault.nPriority = prefs.GetDefaultPriority();
	m_tdiDefault.nRisk = prefs.GetDefaultRisk();
	
	prefs.GetDefaultCategories(m_tdiDefault.aCategories);
	prefs.GetDefaultAllocTo(m_tdiDefault.aAllocTo);
	prefs.GetDefaultTags(m_tdiDefault.aTags);
	
	m_mgrImportExport.SetDefaultTaskAttributes(m_tdiDefault);
}

BOOL CToDoListWnd::LoadMenubar()
{
	m_menubar.DestroyMenu();
	
	if (!m_menubar.LoadMenu(IDR_MAINFRAME))
		return FALSE;

	SetMenu(&m_menubar);
	m_hMenuDefault = m_menubar;
	
#ifdef _DEBUG
	// add menu option to simulate WM_QUERYENDSESSION
	m_menubar.InsertMenu((UINT)-1, MFT_STRING, ID_DEBUGENDSESSION, _T("EndSession"));
	CLocalizer::EnableTranslation(ID_DEBUGENDSESSION, FALSE);

	// and option to display setup dialog on demand
	m_menubar.InsertMenu((UINT)-1, MFT_STRING, ID_DEBUGSHOWSETUPDLG, _T("Show Setup"));
	CLocalizer::EnableTranslation(ID_DEBUGSHOWSETUPDLG, FALSE);
#endif

	if (Prefs().GetShowTasklistCloseButton()) 
		m_menubar.AddMDIButton(MEB_CLOSE, ID_CLOSE);

	if (CThemed::IsThemeActive())
		m_menubar.SetBackgroundColor(m_theme.crMenuBack);

	DrawMenuBar();

	// disable translation of dynamic menus
	EnableDynamicMenuTranslation(FALSE);

	return TRUE;
}

void CToDoListWnd::EnableDynamicMenuTranslation(BOOL bEnable)
{
	CLocalizer::EnableTranslation(ID_FILE_MRU_FIRST, ID_FILE_MRU_LAST, bEnable);
	CLocalizer::EnableTranslation(ID_WINDOW1, ID_WINDOW16, bEnable);
	CLocalizer::EnableTranslation(ID_TOOLS_USERTOOL1, ID_TOOLS_USERTOOL16, bEnable);
	CLocalizer::EnableTranslation(ID_FILE_OPEN_USERSTORAGE1, ID_FILE_OPEN_USERSTORAGE16, bEnable);
	CLocalizer::EnableTranslation(ID_FILE_SAVE_USERSTORAGE1, ID_FILE_SAVE_USERSTORAGE16, bEnable);
	CLocalizer::EnableTranslation(ID_TRAYICON_SHOWDUETASKS1, ID_TRAYICON_SHOWDUETASKS20, bEnable);
}

void CToDoListWnd::UpdateGlobalHotkey()
{
	static DWORD dwPrevHotkey = 0;
	DWORD dwHotkey = Prefs().GetGlobalHotkey();
	
	if (dwPrevHotkey == dwHotkey)
		return;
	
	if (dwHotkey == 0) // disabled
		::UnregisterHotKey(*this, 1);
	else
	{
		// map modifiers to those wanted by RegisterHotKey
		DWORD dwPrefMods = HIWORD(dwHotkey);
		DWORD dwVKey = LOWORD(dwHotkey);
		
		DWORD dwMods = (dwPrefMods & HOTKEYF_ALT) ? MOD_ALT : 0;
		dwMods |= (dwPrefMods & HOTKEYF_CONTROL) ? MOD_CONTROL : 0;
		dwMods |= (dwPrefMods & HOTKEYF_SHIFT) ? MOD_SHIFT : 0;
		
		RegisterHotKey(*this, 1, dwMods, dwVKey);
	}

	dwPrevHotkey = dwHotkey;
}

void CToDoListWnd::RefreshPauseTimeTracking()
{
	// time tracking
	int nCtrl = GetTDCCount();
	int nSel = GetSelToDoCtrl();
	BOOL bTrackActiveOnly = !Prefs().GetTrackNonActiveTasklists();
	
	while (nCtrl--)
	{
		BOOL bSel = (nCtrl == nSel);
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
		
		tdc.PauseTimeTracking(bSel ? FALSE : bTrackActiveOnly);
	}
}

BOOL CToDoListWnd::ProcessStartupOptions(const TDCSTARTUP& startup)
{
	// 1. check if we can handle a task link
	if (startup.HasFlag(TLD_TASKLINK))
	{
		CStringArray aFiles;

		if (startup.GetFilePaths(aFiles))
		{
			CString sFile;
			DWORD dwTaskID = 0;

			CFilteredToDoCtrl::ParseTaskLink(aFiles[0], dwTaskID, sFile);

			return DoTaskLink(sFile, dwTaskID);
		}

		// else
		return FALSE;
	}

	// 2. execute a command
	if (startup.HasCommandID())
	{
		SendMessage(WM_COMMAND, MAKEWPARAM(startup.GetCommandID(), 0), 0);
		return TRUE;
	}

	// 3. try open/import file
	if (startup.HasFilePath())
	{
		int nFirstSel = -1;
		BOOL bSuccess = FALSE;

		CStringArray aFilePaths;
		int nNumFiles = startup.GetFilePaths(aFilePaths);

		for (int nFile = 0; nFile < nNumFiles; nFile++)
		{
			const CString& sFilePath = aFilePaths[nFile];
			
			if (startup.HasFlag(TLD_IMPORTFILE))
			{
				if (ImportFile(sFilePath, TRUE))
				{
					bSuccess = TRUE;
				}
			}
			else
			{
				BOOL bCanDelayLoad = Prefs().GetEnableDelayedLoading();

				// open the first tasklist fully and the rest delayed
				if (!bSuccess || !Prefs().GetEnableDelayedLoading())
				{
					if (OpenTaskList(sFilePath, FALSE) == TDCO_SUCCESS)
					{
						bSuccess = TRUE;
					}
				}
				else if (DelayOpenTaskList(sFilePath) == TDCO_SUCCESS)
				{
					bSuccess = TRUE;
				}
			}

			// snapshot the first success for subsequent selection
			if (bSuccess && (nFirstSel == -1))
				nFirstSel = GetSelToDoCtrl();
		}
		
		// exit on failure
		if (!bSuccess)
			return FALSE;

		// set selection to first tasklist loaded
		ASSERT((nFirstSel != -1) && (nFirstSel < GetTDCCount()));

		SelectToDoCtrl(nFirstSel, FALSE);
	}
	
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	BOOL bRes = FALSE;
	
	if (startup.HasFlag(TLD_NEWTASK))
	{
		CEnString sNewTask;
		BOOL bEditTask = FALSE;
		
		// we edit the task name if no name was supplied
		if (!startup.GetNewTask(sNewTask))
		{
			sNewTask.LoadString(IDS_TASK);
			bEditTask = TRUE;
		}
		
		// do we have a parent task ?
		if (tdc.SelectTask(startup.GetParentTaskID()))
		{
			bRes = CreateNewTask(sNewTask, TDC_INSERTATTOPOFSELTASK, FALSE);
		}
		// or a sibling task ?
		else if (tdc.SelectTask(startup.GetSiblingTaskID()))
		{
			bRes = CreateNewTask(sNewTask, TDC_INSERTAFTERSELTASK, FALSE);
		}	
		else
		{
			bRes = CreateNewTask(sNewTask, TDC_INSERTATTOP, FALSE);
		}
	
		// creation date
		double dDate;

		if (startup.GetCreationDate(dDate))
			tdc.SetSelectedTaskDate(TDCD_CREATE, dDate);
		
		// edit task title?
		if (bRes && bEditTask)
			PostMessage(WM_COMMAND, ID_EDIT_TASKTEXT);
	}
	else if (startup.GetTaskID())
	{
		bRes = tdc.SelectTask(startup.GetTaskID());
	}
	else // works on the currently selected item(s)
	{
		bRes = (tdc.GetSelectedCount() > 0);
	}

	// rest of task attributes
	if (bRes)
	{
		CStringArray aItems;
		CString sItem;
		int nItem;
		double dItem;
		
		if (startup.GetComments(sItem))
			tdc.SetSelectedTaskComments(sItem, sItem);
		
		if (startup.GetExternalID(sItem))
			tdc.SetSelectedTaskExtID(sItem);
		
		if (startup.GetVersion(sItem))
			tdc.SetSelectedTaskVersion(sItem);
		
		if (startup.GetAllocTo(aItems))
			tdc.SetSelectedTaskAllocTo(aItems);
		
		if (startup.GetAllocBy(sItem))
			tdc.SetSelectedTaskAllocBy(sItem);
		
		if (startup.GetCategories(aItems))
			tdc.SetSelectedTaskCategories(aItems);
		
		if (startup.GetTags(aItems))
			tdc.SetSelectedTaskTags(aItems);
		
		if (startup.GetStatus(sItem))
			tdc.SetSelectedTaskStatus(sItem);
		
		if (startup.GetFileRef(sItem))
			tdc.SetSelectedTaskFileRef(sItem);

		if (startup.GetPriority(nItem))
			tdc.SetSelectedTaskPriority(nItem);

		if (startup.GetRisk(nItem))
			tdc.SetSelectedTaskRisk(nItem);

		if (startup.GetPercentDone(nItem))
			tdc.SetSelectedTaskPercentDone(nItem);

		if (startup.GetCost(dItem))
			tdc.SetSelectedTaskCost(dItem);

		if (startup.GetTimeEst(dItem))
			tdc.SetSelectedTaskTimeEstimate(dItem); // in hours

		if (startup.GetTimeSpent(dItem))
			tdc.SetSelectedTaskTimeSpent(dItem); // in hours

		if (startup.GetStartDate(dItem))
			tdc.SetSelectedTaskDate(TDCD_START, dItem);

		if (startup.GetDueDate(dItem))
			tdc.SetSelectedTaskDate(TDCD_DUE, dItem);

		if (startup.GetDoneDate(dItem))
			tdc.SetSelectedTaskDate(TDCD_DONE, dItem);
	}

	return bRes;
}

BOOL CToDoListWnd::OnCopyData(CWnd* /*pWnd*/, COPYDATASTRUCT* pCopyDataStruct)
{
	BOOL bRes = FALSE;

	switch (pCopyDataStruct->dwData)
	{
	case TDL_STARTUP:
		{
			ASSERT(pCopyDataStruct->cbData == sizeof(TDCSTARTUP));

			const TDCSTARTUP* pStartup = (TDCSTARTUP*)(pCopyDataStruct->lpData);

			if (pStartup)
				bRes = ProcessStartupOptions(*pStartup);
		}
		break;
	}

	return bRes; 
}

BOOL CToDoListWnd::ImportFile(LPCTSTR szFilePath, BOOL bSilent)
{
	int nImporter = m_mgrImportExport.FindImporter(szFilePath);

	if (nImporter == -1)
		return FALSE;

	CTaskFile tasks;
		
	m_mgrImportExport.ImportTaskList(szFilePath, &tasks, nImporter, bSilent);
		
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
		
	if (tdc.InsertTasks(tasks, TDC_INSERTATTOP))
		UpdateCaption();

	return TRUE;
}

void CToDoListWnd::OnEditCopy() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	tdc.Flush();
	tdc.CopySelectedTask();
}

void CToDoListWnd::OnEditCut() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	tdc.Flush();
	tdc.CutSelectedTask();
}

void CToDoListWnd::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && nSelCount);	
}

void CToDoListWnd::OnEditPasteSub() 
{
	CWaitCursor wait;
	GetToDoCtrl().PasteTasks(TDCP_ONSELTASK);
}

void CToDoListWnd::OnUpdateEditPasteSub(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.CanPaste() && nSelCount == 1);	
}

void CToDoListWnd::OnEditPasteAfter() 
{
	CWaitCursor wait;
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	if (nSelCount == 0)
		tdc.PasteTasks(TDCP_ATBOTTOM);
	else
		tdc.PasteTasks(TDCP_BELOWSELTASK);
}

void CToDoListWnd::OnUpdateEditPasteAfter(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	// modify the text appropriately if the tasklist is empty
	if (nSelCount == 0)
		pCmdUI->SetText(CEnString(IDS_PASTETOPLEVELTASK));
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.CanPaste());	
}

void CToDoListWnd::OnEditPasteAsRef() 
{
	CWaitCursor wait;
	GetToDoCtrl().PasteTasks(TDCP_ONSELTASK, TRUE);
}

void CToDoListWnd::OnUpdateEditPasteAsRef(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	//int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.CanPaste()/* && nSelCount == 1*/);	
}

void CToDoListWnd::OnEditCopyastext() 
{
	CopySelectedTasksToClipboard(TDCTC_ASTEXT);
}

void CToDoListWnd::OnEditCopyashtml() 
{
	CopySelectedTasksToClipboard(TDCTC_ASHTML);
}

void CToDoListWnd::CopySelectedTasksToClipboard(TDC_TASKS2CLIPBOARD nAsFormat)
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.Flush();
	
	BOOL bParentTitleCommentsOnly = Prefs().GetExportParentTitleCommentsOnly();
	DWORD dwFlags = (bParentTitleCommentsOnly ? TDCGTF_PARENTTITLECOMMENTSONLY : 0);
	
	CTaskFile tasks;
	CString sTasks;
	tdc.GetSelectedTasks(tasks, TDCGETTASKS(TDCGT_ALL, dwFlags));
	
	switch (nAsFormat)
	{	
	case TDCTC_ASHTML:
		sTasks = m_mgrImportExport.ExportTaskListToHtml(&tasks);
		break;
		
	case TDCTC_ASTEXT:
		sTasks = m_mgrImportExport.ExportTaskListToText(&tasks);
		break;
		
	case TDCTC_ASLINK:
		sTasks.Format(_T("tdl://%ld"), tdc.GetSelectedTaskID());
		break;
		
	case TDCTC_ASDEPENDS:
		sTasks.Format(_T("%ld"), tdc.GetSelectedTaskID());
		break;
		
	case TDCTC_ASLINKFULL:
		sTasks.Format(_T("tdl://%s?%ld"), 
						tdc.GetFilePath(),
						tdc.GetSelectedTaskID());
		sTasks.Replace(_T(" "), _T("%20"));
		break;
		
	case TDCTC_ASDEPENDSFULL:
		sTasks.Format(_T("%s?%ld"), 
						tdc.GetFilePath(),
						tdc.GetSelectedTaskID());
		break;

	case TDCTC_ASPATH:
		sTasks = tdc.GetSelectedTaskPath(TRUE);
		break;
	}

	Misc::CopyTexttoClipboard(sTasks, *this);
}

void CToDoListWnd::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetSelectedCount() > 0);
}

BOOL CToDoListWnd::CanCreateNewTask(TDC_INSERTWHERE nInsertWhere) const
{
	return GetToDoCtrl().CanCreateNewTask(nInsertWhere);
}

void CToDoListWnd::OnUpdateNewtaskAttopSelected(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTATTOPOFSELTASKPARENT));
}

void CToDoListWnd::OnUpdateNewtaskAtbottomSelected(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTATBOTTOMOFSELTASKPARENT));
}

void CToDoListWnd::OnUpdateNewtaskAfterselectedtask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTAFTERSELTASK));
}

void CToDoListWnd::OnUpdateNewtaskBeforeselectedtask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTBEFORESELTASK));
}

void CToDoListWnd::OnUpdateNewsubtaskAttop(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTATTOPOFSELTASK));
}

void CToDoListWnd::OnUpdateNewsubtaskAtBottom(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTATBOTTOMOFSELTASK));
}

void CToDoListWnd::OnUpdateNewtaskAttop(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTATTOP));
}

void CToDoListWnd::OnUpdateNewtaskAtbottom(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(CanCreateNewTask(TDC_INSERTATBOTTOM));
}

void CToDoListWnd::OnMaximizeTasklist() 
{
	// toggle max state on or off
	switch (m_nMaxState)
	{
	case TDCMS_MAXTASKLIST:
		// turn off maximize tasklist by restoring previous max state
		m_nMaxState = m_nPrevMaxState;
		m_nPrevMaxState = TDCMS_NORMAL; // reset
		break;

	case TDCMS_MAXCOMMENTS:
		// turn on maximize tasklist and save previous max state
		m_nMaxState = TDCMS_MAXTASKLIST;
		m_nPrevMaxState = TDCMS_MAXCOMMENTS;
		break;

	case TDCMS_NORMAL:
		// turn on maximize tasklist
		m_nMaxState = TDCMS_MAXTASKLIST;
		m_nPrevMaxState = TDCMS_NORMAL; // reset
		break;
	}
	
	// update active tasklist
	GetToDoCtrl().SetMaximizeState(m_nMaxState);
	Invalidate();

	// and caption
	UpdateCaption();
}

void CToDoListWnd::OnUpdateMaximizeTasklist(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_nMaxState == TDCMS_MAXTASKLIST ? 1 : 0);
}

void CToDoListWnd::OnMaximizeComments() 
{
	// toggle max state on or off
	switch (m_nMaxState)
	{
	case TDCMS_MAXCOMMENTS:
		// toggle off maximize comments by restoring previous max state
		m_nMaxState = m_nPrevMaxState;
		m_nPrevMaxState = TDCMS_NORMAL; // reset
		break;

	case TDCMS_MAXTASKLIST:
		// turn on maximize comments and save previous max state
		m_nMaxState = TDCMS_MAXCOMMENTS;
		m_nPrevMaxState = TDCMS_MAXTASKLIST;
		break;

	case TDCMS_NORMAL:
		// turn on maximize comments
		m_nMaxState = TDCMS_MAXCOMMENTS;
		m_nPrevMaxState = TDCMS_NORMAL; // reset
		break;
	}
	
	// update active tasklist
	GetToDoCtrl().SetMaximizeState(m_nMaxState);
	Invalidate();

	// and caption
	UpdateCaption();
}

void CToDoListWnd::OnUpdateMaximizeComments(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_nMaxState == TDCMS_MAXCOMMENTS ? 1 : 0);
}

void CToDoListWnd::OnReload() 
{
	int nSel = GetSelToDoCtrl();
	
	if (m_mgrToDoCtrls.GetModifiedStatus(nSel))
	{ 
		if (IDYES != MessageBox(IDS_CONFIRMRELOAD, IDS_CONFIRMRELOAD_TITLE, MB_YESNOCANCEL | MB_DEFBUTTON2))
		{
			return;
		}
	}
	
	// else reload
	ReloadTaskList(nSel);
	RefreshTabOrder();
}

BOOL CToDoListWnd::ReloadTaskList(int nIndex, BOOL bNotifyDueTasks, BOOL bNotifyError)
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);
	
	TDC_FILE nRes = OpenTaskList(&tdc);
	
	if (nRes == TDCO_SUCCESS)
	{
		const CPreferencesDlg& userPrefs = Prefs();
		
		// update file status
		if (userPrefs.GetAutoCheckOut())
			m_mgrToDoCtrls.SetLastCheckoutStatus(nIndex, tdc.IsCheckedOut());
		
		m_mgrToDoCtrls.RefreshLastModified(nIndex);
		m_mgrToDoCtrls.SetModifiedStatus(nIndex, FALSE);
		m_mgrToDoCtrls.UpdateTabItemText(nIndex);
		
		// notify user of due tasks if req
		if (bNotifyDueTasks)
			DoDueTaskNotification(nIndex, userPrefs.GetNotifyDueByOnLoad());
		
		UpdateCaption();
		UpdateStatusbar();
	}
	else if (bNotifyError)
	{
		HandleLoadTasklistError(nRes, tdc.GetFilePath());
	}

	return (nRes == TDCO_SUCCESS);
}

void CToDoListWnd::OnUpdateReload(CCmdUI* pCmdUI) 
{
	int nSel = GetSelToDoCtrl();
	
	pCmdUI->Enable(!m_mgrToDoCtrls.GetFilePath(nSel).IsEmpty());
}

void CToDoListWnd::OnSize(UINT nType, int cx, int cy) 
{
	CFrameWnd::OnSize(nType, cx, cy);
	
	// ensure m_cbQuickFind is positioned correctly
	int nPos = m_toolbar.CommandToIndex(ID_EDIT_FINDTASKS) + 2;

	CRect rNewPos, rOrgPos;
	m_toolbar.GetItemRect(nPos, rNewPos);
	m_toolbar.ClientToScreen(rNewPos);
	m_cbQuickFind.CWnd::GetWindowRect(rOrgPos);

	// check if it needs to be moved
	if (rNewPos.TopLeft() != rOrgPos.TopLeft())
	{
		m_toolbar.ScreenToClient(rNewPos);
		rNewPos.bottom = rNewPos.top + 200;
		m_cbQuickFind.MoveWindow(rNewPos);
	}

	// topmost?
	BOOL bMaximized = (nType == SIZE_MAXIMIZED);
	
	if (nType != SIZE_MINIMIZED)
		Resize(cx, cy, bMaximized);
	
	// if not maximized then set topmost if that's the preference
	BOOL bTopMost = (Prefs().GetAlwaysOnTop() && !bMaximized) ? 1 : 0;
	
	// do nothing if no change
	BOOL bIsTopMost = (GetExStyle() & WS_EX_TOPMOST) ? 1 : 0;
	
	if (bTopMost != bIsTopMost)
		SetWindowPos(bTopMost ? &wndTopMost : &wndNoTopMost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

BOOL CToDoListWnd::CalcToDoCtrlRect(CRect& rect, int cx, int cy, BOOL bMaximized)
{
	if (!cx && !cy)
	{
		CRect rClient;
		GetClientRect(rClient);
		
		cx = rClient.right;
		cy = rClient.bottom;
		bMaximized = IsZoomed();
		
		// check again 
		if (!cx && !cy)
			return FALSE;
	}
	
	CRect rTaskList(0, BEVEL, cx - BEVEL, cy);
	
	// toolbar
	if (m_bShowToolbar) 
 		rTaskList.top += m_toolbar.GetHeight() + TB_VOFFSET;
	
	// resize tabctrl
	CDeferWndMove dwm(0); // dummy
	
	CPoint ptOrg(0, rTaskList.top);
	int nTabHeight = ReposTabBar(dwm, ptOrg, cx, TRUE);
	
	if (nTabHeight)
		rTaskList.top += nTabHeight + 1; // hide the bottom of the tab ctrl
	
	// filter controls
	int nInset = (CThemed().IsNonClientThemed() ? BORDER : BEVEL);
	int nFilterWidth = cx - 2 * nInset;
	int nFilterHeight = m_bShowFilterBar ? m_filterBar.CalcHeight(nFilterWidth) : 0;
	
	if (nFilterHeight)
		rTaskList.top += nFilterHeight;// + 4;
	
	// statusbar
	if (m_bShowStatusBar)
	{
		CRect rStatus;
		m_statusBar.GetWindowRect(rStatus);
		ScreenToClient(rStatus);
		rTaskList.bottom = rStatus.top - BORDER;
	}
	else
		rTaskList.bottom = cy - BORDER;
	
	// shrink slightly so that edit controls do not merge with window border
	rTaskList.DeflateRect(nInset, nInset, nInset, nInset);
	rect = rTaskList;
	
	return TRUE;
}

void CToDoListWnd::Resize(int cx, int cy, BOOL bMaximized)
{
	static int nLastCx = 0, nLastCy = 0;

	if (!cx && !cy)
	{
		CRect rClient;
		GetClientRect(rClient);
		
		cx = rClient.right;
		cy = rClient.bottom;
		bMaximized = IsZoomed();
		
		// check again 
		if (!cx && !cy)
			return;
	}

	if (cx == nLastCx && cy == nLastCy && !GetTDCCount())
		return;

	nLastCx = cx;
	nLastCy = cy;
	
	// resize in one go
	CDlgUnits dlu(*this);
	CDeferWndMove dwm(6);
	CRect rTaskList(0, BEVEL, cx - BEVEL, cy);
	
	// toolbar
	if (m_bShowToolbar) // showing toolbar
		rTaskList.top += m_toolbar.Resize(cx, CPoint(0, TB_VOFFSET)) + TB_VOFFSET;
	
	// resize tabctrl
	CPoint ptOrg(0, rTaskList.top);
	int nTabHeight = ReposTabBar(dwm, ptOrg, cx);
	
	if (nTabHeight)
		rTaskList.top += nTabHeight + 1; // hide the bottom of the tab ctrl
	
	// filter controls
	int nInset = (CThemed().IsNonClientThemed() ? BORDER : BEVEL);
	int nFilterWidth = cx - 2 * nInset;
	int nFilterHeight = m_bShowFilterBar ? m_filterBar.CalcHeight(nFilterWidth) : 0;
	
	dwm.MoveWindow(&m_filterBar, nInset, rTaskList.top, nFilterWidth, nFilterHeight);
	
	if (nFilterHeight)
		rTaskList.top += nFilterHeight;// + 4;
	
	// statusbar has already been automatically resized unless it's invisible
	CRect rStatus(0, cy, cx, cy);

	if (m_bShowStatusBar)
	{
		m_statusBar.GetWindowRect(rStatus);
		ScreenToClient(rStatus);
	}
	else
		dwm.MoveWindow(&m_statusBar, rStatus, FALSE);
	
	// finally the active todoctrl
	if (GetTDCCount())
	{
		if (m_bShowStatusBar)
			rTaskList.bottom = rStatus.top - BORDER;
		else
			rTaskList.bottom = rStatus.bottom - BORDER;
		
		// shrink slightly so that edit controls do not merge with window border
		rTaskList.DeflateRect(nInset, nInset, nInset, nInset);

		dwm.MoveWindow(&GetToDoCtrl(), rTaskList);

#ifdef _DEBUG
		CRect rect;
		CalcToDoCtrlRect(rect, cx, cy, IsZoomed());
		ASSERT(rect == rTaskList);
#endif

	}
}

BOOL CToDoListWnd::WantTasklistTabbarVisible() const 
{ 
	BOOL bWantTabbar = (GetTDCCount() > 1 || !Prefs().GetAutoHideTabbar()); 
	bWantTabbar &= m_bShowTasklistBar;

	return bWantTabbar;
}

int CToDoListWnd::ReposTabBar(CDeferWndMove& dwm, const CPoint& ptOrg, int nWidth, BOOL bCalcOnly)
{
	CRect rTabs(0, 0, nWidth, 0);
	m_tabCtrl.AdjustRect(TRUE, rTabs);
	int nTabHeight = rTabs.Height() - 4;
	
	rTabs = dwm.OffsetCtrl(this, IDC_TABCONTROL); // not actually a move
	rTabs.right = nWidth + 1;
	rTabs.bottom = rTabs.top + nTabHeight;
	rTabs.OffsetRect(0, ptOrg.y - rTabs.top + 1); // add a pixel between tabbar and toolbar
	
	BOOL bNeedTabCtrl = WantTasklistTabbarVisible();
	
	if (!bCalcOnly)
	{
		dwm.MoveWindow(&m_tabCtrl, rTabs);
		
		// hide and disable tabctrl if not needed
		m_tabCtrl.ShowWindow(bNeedTabCtrl ? SW_SHOW : SW_HIDE);
		m_tabCtrl.EnableWindow(bNeedTabCtrl);

		if (bNeedTabCtrl)
			UpdateTabSwitchTooltip();
	}
	
	return bNeedTabCtrl ? rTabs.Height() : 0;
}

void CToDoListWnd::OnPrint() 
{
	DoPrint();
}

void CToDoListWnd::DoPrint(BOOL bPreview)
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelTDC = GetSelToDoCtrl();

	// pass the project name as the title field
	CString sTitle = m_mgrToDoCtrls.GetFriendlyProjectName(nSelTDC);

	// export to html and then print in IE
	CTDLPrintDialog dialog(sTitle, bPreview, tdc.GetView());
	
	if (dialog.DoModal() != IDOK)
		return;

	RedrawWindow();
	
	// always use the same file
	CString sTempFile = FileMisc::GetTempFileName(_T("ToDoList.print"), _T("html"));
	
	// stylesheets don't seem to like the way we do html comments
	CString sStylesheet = dialog.GetStylesheet();
	BOOL bTransform = FileMisc::FileExists(sStylesheet);

	sTitle = dialog.GetTitle();
	
	// export
	DOPROGRESS(bPreview ? IDS_PPREVIEWPROGRESS : IDS_PRINTPROGRESS)

	CTaskFile tasks;
	GetTasks(tdc, TRUE, bTransform, dialog.GetTaskSelection(), tasks, NULL);

	// add title and date, and style 
	COleDateTime date;

	if (dialog.GetWantDate())
		date = COleDateTime::GetCurrentTime();

	tasks.SetReportAttributes(sTitle, date);

	// add export style
	if (!bTransform)
	{
		TDLPD_STYLE nStyle = dialog.GetExportStyle();
		tasks.SetMetaData(TDL_EXPORTSTYLE, Misc::Format(nStyle));
	}
	
	// save intermediate tasklist to file as required
	LogIntermediateTaskList(tasks, tdc.GetFilePath());

	if (!Export2Html(tasks, sTempFile, sStylesheet))
		return;
	
	// print from browser
	CRect rHidden(-20, -20, -10, -10); // create IE off screen
	
	if (m_IE.GetSafeHwnd() || m_IE.Create(NULL, WS_CHILD | WS_VISIBLE, rHidden, this, (UINT)IDC_STATIC))
	{
		double dFileSize = FileMisc::GetFileSize(sTempFile);
		BOOL bPrintBkgnd = Prefs().GetColorTaskBackground();

		if (bPreview)
			m_IE.PrintPreview(sTempFile, bPrintBkgnd);
		else
			m_IE.Print(sTempFile, bPrintBkgnd);
	}
	else // try sending to browser
	{
		int nRes = (int)::ShellExecute(*this, bPreview ? _T("print") : NULL, sTempFile, NULL, NULL, SW_HIDE);
								
		if (nRes < 32)
			MessageBox(IDS_PRINTFAILED, IDS_PRINTFAILED_TITLE, MB_OK);
	}
}

void CToDoListWnd::OnUpdatePrint(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetTaskCount());
}

int CToDoListWnd::AddToDoCtrl(CFilteredToDoCtrl* pTDC, TSM_TASKLISTINFO* pInfo, BOOL bResizeDlg)
{
	// add tdc first to ensure tab controls has some
	// items before we query it for its size
	int nSel = m_mgrToDoCtrls.AddToDoCtrl(pTDC, pInfo);
	
	// make sure size is right
	CRect rTDC;
	
	if (CalcToDoCtrlRect(rTDC))
		pTDC->MoveWindow(rTDC);
	
	SelectToDoCtrl(nSel, FALSE);
	pTDC->SetFocusToTasks();
	
	// make sure the tab control is correctly sized
	if (bResizeDlg)
		Resize();
	
	// if this is the only control then set or terminate the various status 
	// check timers
	if (GetTDCCount() == 1)
	{
		const CPreferencesDlg& userPrefs = Prefs();
		
		SetTimer(TIMER_READONLYSTATUS, userPrefs.GetReadonlyReloadOption() != RO_NO);
		SetTimer(TIMER_TIMESTAMPCHANGE, userPrefs.GetTimestampReloadOption() != RO_NO);
		SetTimer(TIMER_AUTOSAVE, userPrefs.GetAutoSaveFrequency());
		SetTimer(TIMER_CHECKOUTSTATUS, 
				userPrefs.GetCheckoutOnCheckin() ||	userPrefs.GetAutoCheckinFrequency());
	}
	
	// make sure everything looks okay
	Invalidate();
	UpdateWindow();
	
	return nSel;
}

void CToDoListWnd::SetTimer(UINT nTimerID, BOOL bOn)
{
	if (bOn)
	{
		UINT nPeriod = 0;
		
		switch (nTimerID)
		{
		case TIMER_READONLYSTATUS:
			nPeriod = INTERVAL_READONLYSTATUS;
			break;
			
		case TIMER_TIMESTAMPCHANGE:
			nPeriod = INTERVAL_TIMESTAMPCHANGE;
			break;
			
		case TIMER_AUTOSAVE:
			nPeriod = (Prefs().GetAutoSaveFrequency() * ONE_MINUTE);
			break;
			
		case TIMER_CHECKOUTSTATUS:
			nPeriod = INTERVAL_CHECKOUTSTATUS;
			break;
			
		case TIMER_DUEITEMS:
			nPeriod = INTERVAL_DUEITEMS;
			break;
			
		case TIMER_TIMETRACKING:
			nPeriod = INTERVAL_TIMETRACKING;
			break;
			
		case TIMER_AUTOMINIMIZE:
			nPeriod = (Prefs().GetAutoMinimizeFrequency() * ONE_MINUTE);
			break;
		}
		
		if (nPeriod)
		{
			UINT nID = CFrameWnd::SetTimer(nTimerID, nPeriod, NULL);
			ASSERT (nID);
		}
	}
	else
		KillTimer(nTimerID);
}

void CToDoListWnd::OnTimer(UINT nIDEvent) 
{
	CFrameWnd::OnTimer(nIDEvent);
	
	// if we are disabled (== modal dialog visible) then do not respond
	if (!IsWindowEnabled())
		return;
	
	// don't check whilst in the middle of saving or closing
	if (m_bSaving || m_bClosing)
		return;
	
	// if no controls are active kill the timers
	if (!GetTDCCount())
	{
		SetTimer(TIMER_READONLYSTATUS, FALSE);
		SetTimer(TIMER_TIMESTAMPCHANGE, FALSE);
		SetTimer(TIMER_AUTOSAVE, FALSE);
		SetTimer(TIMER_CHECKOUTSTATUS, FALSE);
		SetTimer(TIMER_DUEITEMS, FALSE);
		SetTimer(TIMER_TIMETRACKING, FALSE);
		SetTimer(TIMER_AUTOMINIMIZE, FALSE);
		return;
	}
	
	switch (nIDEvent)
	{
	case TIMER_READONLYSTATUS:
		OnTimerReadOnlyStatus();
		break;
		
	case TIMER_TIMESTAMPCHANGE:
		OnTimerTimestampChange();
		break;
		
	case TIMER_AUTOSAVE:
		OnTimerAutoSave();
		break;
		
	case TIMER_CHECKOUTSTATUS:
		OnTimerCheckoutStatus();
		break;
		
	case TIMER_DUEITEMS:
		OnTimerDueItems();
		break;
		
	case TIMER_TIMETRACKING:
		OnTimerTimeTracking();
		break;
		
	case TIMER_AUTOMINIMIZE:
		OnTimerAutoMinimize();
		break;
	}
}

BOOL CToDoListWnd::IsActivelyTimeTracking() const
{
	// cycle thru tasklists until we find a time tracker
	int nCtrl = GetTDCCount();
	
	while (nCtrl--)
	{
		if (GetToDoCtrl(nCtrl).IsActivelyTimeTracking())
			return TRUE;
	}

	// else
	return FALSE;
}

void CToDoListWnd::OnTimerTimeTracking()
{
	AF_NOREENTRANT // macro helper
		
	static BOOL bWasTimeTracking = FALSE;
	BOOL bNowTimeTracking = IsActivelyTimeTracking();
		
	if (bWasTimeTracking != bNowTimeTracking)
	{
		UINT nIDTrayIcon = (bNowTimeTracking ? IDI_TRAYTRACK_STD : IDI_TRAY_STD);
		m_trayIcon.SetIcon(nIDTrayIcon);

		// set the main window icon also as this helps the user know what's going on
		HICON hIcon = GraphicsMisc::LoadIcon(nIDTrayIcon);
		SetIcon(hIcon, FALSE);
	}
	
	bWasTimeTracking = bNowTimeTracking;
}

void CToDoListWnd::OnTimerDueItems(int nCtrl)
{
	AF_NOREENTRANT // macro helper
		
	int nFrom = (nCtrl == -1) ? 0 : nCtrl;
	int nTo = (nCtrl == -1) ? GetTDCCount() - 1 : nCtrl;
	BOOL bRepaint = FALSE;
	
	for (nCtrl = nFrom; nCtrl <= nTo; nCtrl++)
	{
		// first we search for overdue items on each tasklist and if that
		// fails to find anything we then search for items due today
		// but only if the tasklist is fully loaded
		if (m_mgrToDoCtrls.IsLoaded(nCtrl))
		{
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
			TDCM_DUESTATUS nStatus = TDCM_NONE;
			
			if (tdc.HasOverdueTasks()) // takes priority
				nStatus = TDCM_PAST;

			else if (tdc.HasDueTodayTasks())
				nStatus = TDCM_TODAY;
			
			if (nStatus != m_mgrToDoCtrls.GetDueItemStatus(nCtrl))
			{
				m_mgrToDoCtrls.SetDueItemStatus(nCtrl, nStatus);
				bRepaint = TRUE;
			}
		}
	}

	if (bRepaint)
		m_tabCtrl.Invalidate(FALSE);
}

void CToDoListWnd::OnTimerReadOnlyStatus(int nCtrl)
{
	AF_NOREENTRANT // macro helper
		
	const CPreferencesDlg& userPrefs = Prefs();
	
	// work out whether we should check remote files or not
	BOOL bCheckRemoteFiles = (nCtrl != -1);
	
	if (!bCheckRemoteFiles)
	{
		static int nElapsed = 0;
		UINT nRemoteFileCheckInterval = userPrefs.GetRemoteFileCheckFrequency() * 1000; // in ms
		
		nElapsed %= nRemoteFileCheckInterval;
		bCheckRemoteFiles = !nElapsed;
		
		nElapsed += INTERVAL_READONLYSTATUS;
	}
	
	int nReloadOption = userPrefs.GetReadonlyReloadOption();
	
	ASSERT (nReloadOption != RO_NO);
	
	// process files
	CString sFileList;
	int nFrom = (nCtrl == -1) ? 0 : nCtrl;
	int nTo = (nCtrl == -1) ? GetTDCCount() - 1 : nCtrl;
	
	for (nCtrl = nFrom; nCtrl <= nTo; nCtrl++)
	{
		// don't check delay-loaded tasklists
		if (!m_mgrToDoCtrls.IsLoaded(nCtrl))
			continue;
		
		// don't check removeable drives
		int nType = m_mgrToDoCtrls.GetFilePathType(nCtrl);
		
        if (nType == TDCM_UNDEF || nType == TDCM_REMOVABLE)
			continue;
		
		// check remote files?
		if (!bCheckRemoteFiles && nType == TDCM_REMOTE)
			continue;
				
		if (m_mgrToDoCtrls.RefreshReadOnlyStatus(nCtrl))
		{
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
		
			BOOL bReadOnly = m_mgrToDoCtrls.GetReadOnlyStatus(nCtrl);
			BOOL bReload = FALSE;
			
			if (nReloadOption == RO_ASK)
			{
				CString sFilePath = tdc.GetFilePath();
				CEnString sMessage(bReadOnly ? IDS_WRITABLETOREADONLY : IDS_READONLYTOWRITABLE, sFilePath);
				
				if (!bReadOnly) // might been modified
					sMessage += CEnString(IDS_WANTRELOAD);

				UINT nRet = MessageBox(sMessage, IDS_STATUSCHANGE_TITLE, !bReadOnly ? MB_YESNOCANCEL : MB_OK);
				
				bReload = (nRet == IDYES || nRet == IDOK);
			}
			else
				bReload = !bReadOnly; // now writable
			
			if (bReload && ReloadTaskList(nCtrl, FALSE, (nReloadOption == RO_ASK)))
			{
				// notify the user if we haven't already
				if (nReloadOption == RO_NOTIFY)
				{
					sFileList += tdc.GetFriendlyProjectName();
					sFileList += "\n";
				}
			}
			else // update the UI
			{
				if (nCtrl == m_tabCtrl.GetCurSel())
					UpdateCaption();
				
				m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nCtrl);
				m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
			}
		}
	}
	
	// do we need to notify the user?
	if (!sFileList.IsEmpty())
	{
		CEnString sMessage(IDS_TASKLISTSRELOADED, sFileList);
		m_trayIcon.ShowBalloon(sMessage, CEnString(IDS_TIMESTAMPCHANGE_BALLOONTITLE), NIIF_INFO);
	}
}

void CToDoListWnd::OnTimerTimestampChange(int nCtrl)
{
	AF_NOREENTRANT // macro helper
		
	const CPreferencesDlg& userPrefs = Prefs();
	int nReloadOption = userPrefs.GetTimestampReloadOption();
	
	ASSERT (nReloadOption != RO_NO);
	
	// work out whether we should check remote files or not
	BOOL bCheckRemoteFiles = (nCtrl != -1);
	
	if (!bCheckRemoteFiles)
	{
		static int nElapsed = 0;
		UINT nRemoteFileCheckInterval = userPrefs.GetRemoteFileCheckFrequency() * 1000; // in ms
		
		nElapsed %= nRemoteFileCheckInterval;
		bCheckRemoteFiles = !nElapsed;
		
		nElapsed += INTERVAL_TIMESTAMPCHANGE;
	}
	
	// process files
	CString sFileList;
	int nFrom = (nCtrl == -1) ? 0 : nCtrl;
	int nTo = (nCtrl == -1) ? GetTDCCount() - 1 : nCtrl;
	
	for (nCtrl = nFrom; nCtrl <= nTo; nCtrl++)
	{
		// don't check delay-loaded tasklists
		if (!m_mgrToDoCtrls.IsLoaded(nCtrl))
			continue;

		// don't check removeable drives
		int nType = m_mgrToDoCtrls.GetFilePathType(nCtrl);
		
        if (nType == TDCM_UNDEF || nType == TDCM_REMOVABLE)
			continue;
		
		// check remote files?
		if (!bCheckRemoteFiles && nType == TDCM_REMOTE)
			continue;
		
		if (m_mgrToDoCtrls.RefreshLastModified(nCtrl))
		{
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);

			BOOL bReload = TRUE; // default
			
			if (nReloadOption == RO_ASK)
			{
				CString sFilePath = tdc.GetFilePath();
			
				CEnString sMessage(IDS_MODIFIEDELSEWHERE, sFilePath);
				sMessage += CEnString(IDS_WANTRELOAD);
				
				bReload = (MessageBox(sMessage, IDS_TIMESTAMPCHANGE_TITLE, MB_YESNOCANCEL) == IDYES);
			}
			
			if (bReload && ReloadTaskList(nCtrl, FALSE, (nReloadOption == RO_ASK)))
			{
				// notify the user if we haven't already
				if (nReloadOption == RO_NOTIFY)
				{
					sFileList += tdc.GetFriendlyProjectName();
					sFileList += "\n";
				}
			}
			else
			{
				// update UI
				if (nCtrl == m_tabCtrl.GetCurSel())
					UpdateCaption();
				
				m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nCtrl);
				m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
			}
		}
	}
	
	// do we need to notify the user?
	if (!sFileList.IsEmpty())
	{
		CEnString sMessage(IDS_TASKLISTSRELOADED, sFileList);
		m_trayIcon.ShowBalloon(sMessage, CEnString(IDS_TIMESTAMPCHANGE_BALLOONTITLE), NIIF_INFO);
	}
}

void CToDoListWnd::OnTimerAutoSave()
{
	AF_NOREENTRANT // macro helper
		
	// don't save if the user is editing a task label
	if (!GetToDoCtrl().IsTaskLabelEditing())
		SaveAll(TDLS_AUTOSAVE);
}

void CToDoListWnd::OnTimerAutoMinimize()
{
	AF_NOREENTRANT // macro helper

	if (!IsWindowVisible() || IsIconic())
		return;

	LASTINPUTINFO lii = { sizeof(LASTINPUTINFO), 0 };
	
	if (GetLastInputInfo(&lii))
	{
		double dElapsed = (GetTickCount() - lii.dwTime);
		dElapsed /= ONE_MINUTE; // convert to minutes
		
		// check
		if (dElapsed > (double)Prefs().GetAutoMinimizeFrequency())
			ShowWindow(SW_MINIMIZE);
	}
}

void CToDoListWnd::OnTimerCheckoutStatus(int nCtrl)
{
	AF_NOREENTRANT // macro helper
		
	const CPreferencesDlg& userPrefs = Prefs();
	
	// work out whether we should check remote files or not
	BOOL bCheckRemoteFiles = (nCtrl != -1);
	
	if (!bCheckRemoteFiles)
	{
		static int nElapsed = 0;
		UINT nRemoteFileCheckInterval = userPrefs.GetRemoteFileCheckFrequency() * 1000; // in ms
		
		nElapsed %= nRemoteFileCheckInterval;
		bCheckRemoteFiles = !nElapsed;
		
		nElapsed += INTERVAL_CHECKOUTSTATUS;
	}
	
	// process files
	CString sFileList;
	int nFrom = (nCtrl == -1) ? 0 : nCtrl;
	int nTo = (nCtrl == -1) ? GetTDCCount() - 1 : nCtrl;
	
	for (nCtrl = nFrom; nCtrl <= nTo; nCtrl++)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
		
		if (!m_mgrToDoCtrls.PathSupportsSourceControl(nCtrl))
            continue;
		
		// try to check out only if the previous attempt failed
		if (!tdc.IsCheckedOut() && userPrefs.GetCheckoutOnCheckin())
		{
			// we only try to check if the previous checkout failed
			if (!m_mgrToDoCtrls.GetLastCheckoutStatus(nCtrl))
			{
				if (tdc.CheckOut())
				{
					// update timestamp 
					m_mgrToDoCtrls.RefreshLastModified(nCtrl);
					m_mgrToDoCtrls.SetLastCheckoutStatus(nCtrl, TRUE);
					
					if (nCtrl == m_tabCtrl.GetCurSel())
						UpdateCaption();
					
					m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nCtrl);
					m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
					
					// notify the user if the trayicon is visible
					sFileList += tdc.GetFriendlyProjectName();
					sFileList += "\n";
				}
				else // make sure we try again later
					m_mgrToDoCtrls.SetLastCheckoutStatus(nCtrl, FALSE);
			}
		}
		// only checkin if sufficient time has elapsed since last mod
		// and there are no mods outstanding
		else if (tdc.IsCheckedOut() && userPrefs.GetAutoCheckinFrequency())
		{
			if (!tdc.IsModified())
			{
				double dElapsed = COleDateTime::GetCurrentTime() - tdc.GetLastTaskModified();
				dElapsed *= 24 * 60; // convert to minutes
				
				if (dElapsed > (double)userPrefs.GetAutoCheckinFrequency())
				{
					if (tdc.CheckIn())	
					{
						m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nCtrl);
						m_mgrToDoCtrls.RefreshLastModified(nCtrl);
						m_mgrToDoCtrls.SetLastCheckoutStatus(nCtrl, TRUE);
						m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
						
						UpdateCaption();
					}
				}
			}
		}
	}
	// do we need to notify the user?
	if (!sFileList.IsEmpty())
	{
		CEnString sMessage(IDS_TASKLISTSCHECKEDOUT, sFileList);
		m_trayIcon.ShowBalloon(sMessage, _T("Source Control Change(s)"), NIIF_INFO);
	}
}

void CToDoListWnd::OnNeedTooltipText(NMHDR* pNMHDR, LRESULT* pResult)
{
	static CString sTipText;
	sTipText.Empty();

	switch (pNMHDR->idFrom)
	{
	case ID_TOOLS_USERTOOL1:
	case ID_TOOLS_USERTOOL2:
	case ID_TOOLS_USERTOOL3:
	case ID_TOOLS_USERTOOL4:
	case ID_TOOLS_USERTOOL5:
	case ID_TOOLS_USERTOOL6:
	case ID_TOOLS_USERTOOL7:
	case ID_TOOLS_USERTOOL8:
	case ID_TOOLS_USERTOOL9:
	case ID_TOOLS_USERTOOL10:
	case ID_TOOLS_USERTOOL11:
	case ID_TOOLS_USERTOOL12:
	case ID_TOOLS_USERTOOL13:
	case ID_TOOLS_USERTOOL14:
	case ID_TOOLS_USERTOOL15:
	case ID_TOOLS_USERTOOL16:
		{
			USERTOOL ut;

			if (Prefs().GetUserTool(pNMHDR->idFrom - ID_TOOLS_USERTOOL1, ut))
				sTipText = ut.sToolName;
		}
		break;

	default:
		// tab control popups
		if (pNMHDR->idFrom >= 0 && pNMHDR->idFrom < (UINT)m_mgrToDoCtrls.GetCount())
		{
			sTipText = m_mgrToDoCtrls.GetTabItemTooltip(pNMHDR->idFrom);
		}
		break;
	}

	if (!sTipText.IsEmpty())
	{
		TOOLTIPTEXT* pTTT = (TOOLTIPTEXT*)pNMHDR;
		pTTT->lpszText = (LPTSTR)(LPCTSTR)sTipText;
	}

	*pResult = 0;
}

void CToDoListWnd::OnUpdateUserTool(CCmdUI* pCmdUI) 
{
	if (pCmdUI->m_pMenu && pCmdUI->m_nID == ID_TOOLS_USERTOOL1) // only handle first item
	{
		CUserToolArray aTools;
		Prefs().GetUserTools(aTools);
		
		CToolsHelper th(Prefs().GetEnableTDLExtension(), ID_TOOLS_USERTOOL1);
		th.UpdateMenu(pCmdUI, aTools, m_mgrMenuIcons);
	}
	else if (m_bShowToolbar) 
	{
		int nTool = pCmdUI->m_nID - ID_TOOLS_USERTOOL1;
		ASSERT (nTool >= 0 && nTool < 16);

		USERTOOL ut;
		
		if (Prefs().GetUserTool(nTool, ut))
			pCmdUI->Enable(TRUE);
	}
}

void CToDoListWnd::OnUserTool(UINT nCmdID) 
{
	int nTool = nCmdID - ID_TOOLS_USERTOOL1;
	USERTOOL ut;
	
	ASSERT (nTool >= 0 && nTool < 16);

	const CPreferencesDlg& prefs = Prefs();
	
	if (prefs.GetUserTool(nTool, ut))
	{
		// Save all tasklists before executing the user tool
		if (prefs.GetAutoSaveOnRunTools())
		{
			if (SaveAll(TDLS_FLUSH) == TDCO_CANCELLED)
				return;
		}

		USERTOOLARGS args;
		PopulateToolArgs(args);

		CToolsHelper th(prefs.GetEnableTDLExtension(), ID_TOOLS_USERTOOL1);
		th.RunTool(ut, args, this);
	}
}

void CToDoListWnd::AddUserStorageToMenu(CMenu* pMenu) 
{
	if (pMenu)
	{
		const UINT MENUSTARTID = pMenu->GetMenuItemID(0);

		// delete existing entries
		int nStore = 16;

		while (nStore--)
		{
			pMenu->DeleteMenu(nStore, MF_BYPOSITION);
		}
		
		// if we have any tools to add we do it here
		int nNumStorage = min(m_mgrStorage.GetNumStorage(), 16);

		if (nNumStorage)
		{
			UINT nFlags = (MF_BYPOSITION | MF_STRING);

			for (int nStore = 0; nStore < nNumStorage; nStore++)
			{
				CString sMenuItem, sText = m_mgrStorage.GetStorageMenuText(nStore);
								
				if (nStore < 9)
					sMenuItem.Format(_T("&%d %s"), nStore + 1, sText);
				else
					sMenuItem = sText;
				
				// special case: if this is a 'Save' menu item
				// then the we disable it if it's the same as
				// the current tasklist's storage
				UINT nExtraFlags = 0;
/*
				if (MENUSTARTID == ID_FILE_SAVE_USERSTORAGE1)
				{
					int nTDC = GetSelToDoCtrl();
					TSM_TASKLISTINFO storageInfo;

					if (m_mgrToDoCtrls.GetStorageDetails(nTDC, storageInfo))
					{
						if (m_mgrStorage.FindStorage(storageInfo.sStorageID) == nStore)
							nExtraFlags = (MF_GRAYED | MF_CHECKED);
					}
				}
*/				
				pMenu->InsertMenu(nStore, nFlags | nExtraFlags, MENUSTARTID + nStore, sMenuItem);

				// add icon if available
				HICON hIcon = m_mgrStorage.GetStorageIcon(nStore);

				if (hIcon)
					m_mgrMenuIcons.AddImage(MENUSTARTID + nStore, hIcon);
			}
		}
		else // if nothing to add just re-add placeholder
		{
			pMenu->InsertMenu(0, MF_BYPOSITION | MF_STRING | MF_GRAYED, MENUSTARTID, _T("3rd Party Storage"));
		}
	}
}

void CToDoListWnd::OnFileOpenFromUserStorage(UINT nCmdID) 
{
	int nStorage = nCmdID - ID_FILE_OPEN_USERSTORAGE1;
	
	ASSERT (nStorage >= 0 && nStorage < 16);

	TSM_TASKLISTINFO storageInfo;
	CPreferences prefs;
	CTaskFile tasks;

	if (m_mgrStorage.RetrieveTasklist(&storageInfo, &tasks, nStorage, &prefs))
	{
		if (tasks.GetTaskCount())
		{
			VERIFY(CreateNewTaskList(FALSE));
			
			CFilteredToDoCtrl& tdc = GetToDoCtrl(); 
			VERIFY(tdc.InsertTasks(tasks, TDC_INSERTATTOP));

			// attach the returned storage info to this tasklist
			m_mgrToDoCtrls.SetStorageDetails(GetSelToDoCtrl(), storageInfo);
		}
		else if (storageInfo.szLocalFileName[0])
		{
			TDC_FILE nOpen = OpenTaskList(storageInfo.szLocalFileName, TRUE);
			
			if (nOpen == TDCO_SUCCESS)
			{
				// attach the returned storage info to this tasklist
				int nTDC = m_mgrToDoCtrls.FindToDoCtrl(storageInfo.szLocalFileName);
				ASSERT(nTDC == GetSelToDoCtrl());
				
				m_mgrToDoCtrls.SetStorageDetails(nTDC, storageInfo);
			}
			else
				HandleLoadTasklistError(nOpen, storageInfo.szDisplayName);
		}
		else
		{
			// TODO
		}
		
		UpdateCaption();
		UpdateStatusbar();
		Resize();
		UpdateWindow();
	}
}

void CToDoListWnd::OnFileSaveToUserStorage(UINT nCmdID) 
{
	int nStorage = (nCmdID - ID_FILE_SAVE_USERSTORAGE1);
	
	ASSERT (nStorage >= 0 && nStorage < 16);

	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	CString sLocalPath, sTdcPath = tdc.GetFilePath();

	// retrieve any existing storage info for this tasklist
	int nTDC = GetSelToDoCtrl();
	TSM_TASKLISTINFO storageInfo;

	if (m_mgrToDoCtrls.GetStorageDetails(nTDC, storageInfo))
	{
		sLocalPath = storageInfo.szLocalFileName;

		// clear the storage info ID if we are changing
		// the destination
		if (nStorage != m_mgrStorage.FindStorage(storageInfo.sStorageID))
			storageInfo.Reset();
	}

	if (sLocalPath.IsEmpty())
	{
		sLocalPath = sTdcPath = tdc.GetFilePath();

		if (sLocalPath.IsEmpty())
			sLocalPath = FileMisc::GetTempFileName(m_mgrToDoCtrls.GetFriendlyProjectName(nTDC), _T("tdl"));
	}

	CTaskFile tasks;
	VERIFY (tdc.Save(tasks, sLocalPath) == TDCO_SUCCESS);

	// restore previous task path
	if (sTdcPath != sLocalPath)
	{
		tdc.SetFilePath(sTdcPath);
	}
	else // prevent this save triggering a reload
	{
		m_mgrToDoCtrls.RefreshLastModified(nTDC);
	}

	_tcsncpy(storageInfo.szLocalFileName, sLocalPath, _MAX_PATH);
		
	CPreferences prefs;

	if (m_mgrStorage.StoreTasklist(&storageInfo, &tasks, nStorage, &prefs))
	{
		m_mgrToDoCtrls.SetStorageDetails(nTDC, storageInfo);
		
		UpdateCaption();
		UpdateStatusbar();
		Resize();
		UpdateWindow();
	}

	// else assume that the plugin handled any problems
}

void CToDoListWnd::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu) 
{
	// test for top-level menus
	if (bSysMenu)
		return;

	if (m_menubar.GetSubMenu(FILEALL) == pPopupMenu)
	{
		// insert Min to sys tray if appropriate 
		BOOL bHasMinToTray = (::GetMenuString(*pPopupMenu, ID_MINIMIZETOTRAY, NULL, 0, MF_BYCOMMAND) != 0);
		
		if (Prefs().GetSysTrayOption() == STO_ONCLOSE || Prefs().GetSysTrayOption() == STO_ONMINCLOSE)
		{
			if (!bHasMinToTray)
				pPopupMenu->InsertMenu(ID_EXIT, MF_BYCOMMAND, ID_MINIMIZETOTRAY, CEnString(ID_MINIMIZETOTRAY));
		}
		else if (bHasMinToTray) // then remove
		{
			pPopupMenu->DeleteMenu(ID_MINIMIZETOTRAY, MF_BYCOMMAND);
		}
	}
	else if (m_menubar.GetSubMenu(EDITTASK) == pPopupMenu)
	{
		// remove relevant commands from the edit menu
		PrepareEditMenu(pPopupMenu);
	}
	else if (m_menubar.GetSubMenu(SORTTASK) == pPopupMenu)
	{
		// remove relevant commands from the sort menu
		PrepareSortMenu(pPopupMenu);
	}
	else if (m_menubar.GetSubMenu(TOOLS) == pPopupMenu)
	{
		// remove relevant commands from the sort menu
		PrepareSourceControlMenu(pPopupMenu);
	}
	else // all other sub-menus
	{
		// test for 'Open From...'
		if (pPopupMenu->GetMenuItemID(0) == ID_FILE_OPEN_USERSTORAGE1)
		{
			AddUserStorageToMenu(pPopupMenu);
		}
		// test for 'save To...'
		else if (pPopupMenu->GetMenuItemID(0) == ID_FILE_SAVE_USERSTORAGE1)
		{
			AddUserStorageToMenu(pPopupMenu);
		}
	}

	CFrameWnd::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
}

void CToDoListWnd::OnViewToolbar() 
{
	m_bShowToolbar = !m_bShowToolbar;
	m_toolbar.ShowWindow(m_bShowToolbar ? SW_SHOW : SW_HIDE);
	m_toolbar.EnableWindow(m_bShowToolbar);

	Resize();
	Invalidate(TRUE);
}

void CToDoListWnd::OnUpdateViewToolbar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowToolbar);
}

void CToDoListWnd::AppendTools2Toolbar(BOOL bAppend)
{
	CToolsHelper th(Prefs().GetEnableTDLExtension(), ID_TOOLS_USERTOOL1);
	
	if (bAppend)
	{
		// then re-add
		CUserToolArray aTools;
		Prefs().GetUserTools(aTools);
		
		th.AppendToolsToToolbar(aTools, m_toolbar, ID_PREFERENCES);

		// refresh tooltips
		m_tbHelper.Release();
		m_tbHelper.Initialize(&m_toolbar, this);
	}
	else // remove
	{
		th.RemoveToolsFromToolbar(m_toolbar, ID_PREFERENCES);
	}

	// resize toolbar to accept the additional buttons
	Resize();
}

void CToDoListWnd::OnSort() 
{
	GetToDoCtrl().Resort(TRUE);
}

void CToDoListWnd::OnWindowPosChanged(WINDOWPOS FAR* lpwndpos) 
{
	CFrameWnd::OnWindowPosChanged(lpwndpos);
}

void CToDoListWnd::OnArchiveCompletedtasks() 
{
	CWaitCursor cursor;
	int nSelTDC = GetSelToDoCtrl();
	
	if (m_mgrToDoCtrls.ArchiveDoneTasks(nSelTDC))
	{
		// auto-reload archive if it's loaded
		CString sArchivePath = m_mgrToDoCtrls.GetArchivePath(nSelTDC);
		int nArchiveTDC = m_mgrToDoCtrls.FindToDoCtrl(sArchivePath);

		if (nArchiveTDC != -1 && m_mgrToDoCtrls.IsLoaded(nArchiveTDC))
			ReloadTaskList(nArchiveTDC, FALSE, FALSE);
	
		UpdateCaption();
	}
}

void CToDoListWnd::OnUpdateArchiveCompletedtasks(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && !m_mgrToDoCtrls.GetArchivePath(tdc.GetFilePath()).IsEmpty());
}

void CToDoListWnd::OnArchiveSelectedTasks() 
{
	CWaitCursor cursor;
	int nSelTDC = GetSelToDoCtrl();
	
	if (m_mgrToDoCtrls.ArchiveSelectedTasks(nSelTDC))
	{
		// auto-reload archive if it's loaded
		CString sArchivePath = m_mgrToDoCtrls.GetArchivePath(nSelTDC);
		int nArchiveTDC = m_mgrToDoCtrls.FindToDoCtrl(sArchivePath);

		if (nArchiveTDC != -1 && m_mgrToDoCtrls.IsLoaded(nArchiveTDC))
			ReloadTaskList(nArchiveTDC, FALSE, FALSE);
	
		UpdateCaption();
	}
}

void CToDoListWnd::OnUpdateArchiveSelectedCompletedTasks(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && !m_mgrToDoCtrls.GetArchivePath(tdc.GetFilePath()).IsEmpty());
}

void CToDoListWnd::OnMovetaskdown() 
{
	GetToDoCtrl().MoveSelectedTask(TDCM_DOWN);	
}

void CToDoListWnd::OnUpdateMovetaskdown(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanMoveSelectedTask(TDCM_DOWN));	
}

void CToDoListWnd::OnMovetaskup() 
{
	GetToDoCtrl().MoveSelectedTask(TDCM_UP);	
}

void CToDoListWnd::OnUpdateMovetaskup(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanMoveSelectedTask(TDCM_UP));	
}

void CToDoListWnd::OnMovetaskright() 
{
	GetToDoCtrl().MoveSelectedTask(TDCM_RIGHT);	
}

void CToDoListWnd::OnUpdateMovetaskright(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanMoveSelectedTask(TDCM_RIGHT));	
}

void CToDoListWnd::OnMovetaskleft() 
{
	GetToDoCtrl().MoveSelectedTask(TDCM_LEFT);	
}

void CToDoListWnd::OnUpdateMovetaskleft(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanMoveSelectedTask(TDCM_LEFT));	
}

CFilteredToDoCtrl& CToDoListWnd::GetToDoCtrl()
{
	return GetToDoCtrl(GetSelToDoCtrl());
}

CFilteredToDoCtrl& CToDoListWnd::GetToDoCtrl(int nIndex)
{
	return m_mgrToDoCtrls.GetToDoCtrl(nIndex);
}

const CFilteredToDoCtrl& CToDoListWnd::GetToDoCtrl() const
{
	return GetToDoCtrl(GetSelToDoCtrl());
}

const CFilteredToDoCtrl& CToDoListWnd::GetToDoCtrl(int nIndex) const
{
	return m_mgrToDoCtrls.GetToDoCtrl(nIndex);
}

CFilteredToDoCtrl* CToDoListWnd::NewToDoCtrl(BOOL bVisible, BOOL bEnabled)
{
	CFilteredToDoCtrl* pTDC = NULL;
	
	// if the active tasklist is untitled and unmodified then reuse it
	if (GetTDCCount())
	{
		int nSel = GetSelToDoCtrl();
		CFilteredToDoCtrl& tdc = GetToDoCtrl();
		
		// make sure that we don't accidently reuse a just edited tasklist
		tdc.Flush(); 
		
		if (m_mgrToDoCtrls.IsPristine(nSel))
		{
			pTDC = &tdc;
			m_mgrToDoCtrls.RemoveToDoCtrl(nSel, FALSE);
			
			// make sure we reset the selection to a valid index
			if (GetTDCCount())
			{
				// try leaving the selection as-is
				if (nSel >= GetTDCCount())
					nSel--; // try the preceding item
				
				SelectToDoCtrl(nSel, FALSE);
			}
			
			return pTDC;
		}
	}
	
	// else
	pTDC = new CFilteredToDoCtrl(m_mgrContent, Prefs().GetDefaultCommentsFormat());
	
	if (pTDC && CreateToDoCtrl(pTDC, bVisible, bEnabled))
		return pTDC;
	
	// else
	delete pTDC;
	return NULL;
}

BOOL CToDoListWnd::CreateToDoCtrl(CFilteredToDoCtrl* pTDC, BOOL bVisible, BOOL bEnabled)
{
	// create somewhere out in space
	CRect rCtrl(-32010, -32010, -32000, -32000);

	if (pTDC->Create(rCtrl, this, IDC_TODOLIST, bVisible, bEnabled))
	{
		// set font to our font
		CDialogHelper::SetFont(pTDC, m_fontMain, FALSE);
		
		if (!m_ilCheckboxes.GetSafeHandle())
			InitCheckboxImageList();
		
		UpdateToDoCtrlPreferences(pTDC);

		if (CThemed::IsThemeActive())
			pTDC->SetUITheme(m_theme);

		for (int nExt = 0; nExt < m_mgrUIExtensions.GetNumUIExtensions(); nExt++)
			pTDC->AddView(m_mgrUIExtensions.GetUIExtension(nExt));
		
		return TRUE;
	}
	
	return FALSE;
}

BOOL CToDoListWnd::InitCheckboxImageList()
{
	if (m_ilCheckboxes.GetSafeHandle())
		return TRUE;
	
	const int nStates[] = { -1, CBS_UNCHECKEDNORMAL, CBS_CHECKEDNORMAL };//, CBS_MIXEDNORMAL };
	const int nNumStates = sizeof(nStates) / sizeof(int);
	
	CThemed th;
	
	if (th.Open(this, _T("BUTTON")) && th.AreControlsThemed())
	{
		th.BuildImageList(m_ilCheckboxes, BP_CHECKBOX, nStates, nNumStates);
	}
	
	// unthemed + fallback
	if (!m_ilCheckboxes.GetSafeHandle())
	{
		CBitmap bitmap;
		bitmap.LoadBitmap(IDB_CHECKBOXES);
		
		BITMAP BM;
		bitmap.GetBitmap(&BM);
		
		if (m_ilCheckboxes.Create(BM.bmWidth / nNumStates, BM.bmHeight, ILC_COLOR32 | ILC_MASK, 0, 1))
			m_ilCheckboxes.Add(&bitmap, 255);
	}
	
	return (NULL != m_ilCheckboxes.GetSafeHandle());
}

void CToDoListWnd::OnMBtnClickTabcontrol(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NMTCMBTNCLK* pTCHDR = (NMTCMBTNCLK*)pNMHDR;
	
	// check valid tab
	if (pTCHDR->iTab >= 0)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(pTCHDR->iTab);
		tdc.Flush();
		
		CloseToDoCtrl(pTCHDR->iTab);
		
		if (!GetTDCCount())
			CreateNewTaskList(FALSE);
	}
	*pResult = 0;
}

void CToDoListWnd::OnSelchangeTabcontrol(NMHDR* /*pNMHDR*/, LRESULT* pResult) 
{
	// show the incoming selection
	int nCurSel = GetSelToDoCtrl();

	// check password if necessary
	if (m_nLastSelItem != -1 && !VerifyToDoCtrlPassword())
	{
		m_tabCtrl.SetCurSel(m_nLastSelItem);
		return;
	}

	int nDueBy = Prefs().GetNotifyDueByOnSwitch();
	
	if (nCurSel != -1)
	{
		// make sure it's loaded
		if (!VerifyTaskListOpen(nCurSel, (nDueBy == -1)))
		{
			// restore the previous tab
			m_tabCtrl.SetCurSel(m_nLastSelItem);
			return;
		}

		CFilteredToDoCtrl& tdc = GetToDoCtrl(nCurSel);
		UpdateToDoCtrlPreferences(&tdc);

		// update the filter selection
 		RefreshFilterControls();
 		
		// update status bar
		UpdateStatusbar();
		UpdateCaption();

		// make sure size is right
		CRect rTDC;
		
		if (CalcToDoCtrlRect(rTDC))
			tdc.MoveWindow(rTDC, FALSE);

		// refresh view state
		tdc.SetMaximizeState(m_nMaxState);

		tdc.EnableWindow(TRUE);
		tdc.ShowWindow(SW_SHOW);
		tdc.PauseTimeTracking(FALSE); // always
	}
	
	// hide the outgoing selection
	if (m_nLastSelItem != -1)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(m_nLastSelItem);

		tdc.ShowWindow(SW_HIDE);
		tdc.EnableWindow(FALSE);
		tdc.PauseTimeTracking(!Prefs().GetTrackNonActiveTasklists());

		m_nLastSelItem = -1; // reset
	}
	
	if (nCurSel != -1)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nCurSel);

		// update find dialog with this ToDoCtrl's custom attributes
		UpdateFindDialogCustomAttributes(&tdc);

		// leave focus setting till last else the 'old' tasklist flashes
		tdc.SetFocusToTasks();

		// notify user of due tasks if req
		DoDueTaskNotification(nCurSel, nDueBy);

		UpdateAeroFeatures();
	}

	InitMenuIconManager();
	
	*pResult = 0;
}

void CToDoListWnd::RefreshFilterControls()
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	// if this tasklist's filter is an unnamed custom one
	RefreshFilterBarCustomFilters();

	// get existing filter bar size so we can determine if a 
	// resize if required
	CRect rFilter;

	m_filterBar.GetClientRect(rFilter);
	m_filterBar.RefreshFilterControls(tdc);

	CRect rClient;
	GetClientRect(rClient);

	if (m_filterBar.CalcHeight(rClient.Width()) != rFilter.Height())
		Resize();
}

void CToDoListWnd::OnSelchangingTabcontrol(NMHDR* /*pNMHDR*/, LRESULT* pResult) 
{
	// cache the outgoing selection
	m_nLastSelItem = GetSelToDoCtrl();
	
	// and flush
	if (m_nLastSelItem != -1)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(m_nLastSelItem);
		tdc.Flush();

		// and save
		if (Prefs().GetAutoSaveOnSwitchTasklist() && !tdc.GetFilePath().IsEmpty() && tdc.IsModified())
		{
			tdc.Save();

			m_mgrToDoCtrls.SetModifiedStatus(m_nLastSelItem, FALSE); 
			m_mgrToDoCtrls.RefreshLastModified(m_nLastSelItem); 
			m_mgrToDoCtrls.RefreshReadOnlyStatus(m_nLastSelItem); 
			m_mgrToDoCtrls.RefreshPathType(m_nLastSelItem); 
		}
	}
	
	*pResult = 0;
}

TDC_FILE CToDoListWnd::ConfirmSaveTaskList(int nIndex, DWORD dwFlags)
{
	BOOL bClosingWindows = Misc::HasFlag(dwFlags, TDLS_CLOSINGWINDOWS);
	BOOL bClosingTaskList = Misc::HasFlag(dwFlags, TDLS_CLOSINGTASKLISTS) || bClosingWindows; // sanity check
	TDC_FILE nSave = TDCO_SUCCESS;
	
	// save changes
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);
	
	if (tdc.IsModified())
	{
		BOOL bFirstTimeSave = tdc.GetFilePath().IsEmpty();

		// if we are closing Windows, we don't bother asking
		// we just save and get out as fast as poss
		if (bClosingWindows)
		{
			// if it's a first time save we just save to a temp file
			if (bFirstTimeSave)
				tdc.Save(GetEndSessionFilePath());
			else
				tdc.Save();

			return TDCO_SUCCESS;
		}
		// else we obey the user's preferences
		else if (bClosingTaskList && (bFirstTimeSave || Prefs().GetConfirmSaveOnExit()))
		{
			// make sure app is visible
			Show(FALSE);

			// save tasklist
			CString sName = m_mgrToDoCtrls.GetFriendlyProjectName(nIndex);
			CEnString sMessage(IDS_SAVEBEFORECLOSE, sName);
			
			// don't allow user to cancel if closing down
			int nRet = MessageBox(sMessage, IDS_CONFIRMSAVE_TITLE, bClosingWindows ? MB_YESNO : MB_YESNOCANCEL);
			
			if (nRet == IDYES)
			{
				// note: we omit the auto save parameter here because we want the user to
				// be notified of any problems
				nSave = SaveTaskList(nIndex);

				// if the save failed (as opposed to cancelled) then we must
				// propagate this upwards
				if (nSave != TDCO_SUCCESS && nSave != TDCO_CANCELLED)
					return nSave;

				// user can still cancel save dialog even if closing down
				// but not if closing windows
				else if (nSave == TDCO_CANCELLED)
					nRet = bClosingWindows ? IDNO : IDCANCEL;
			}
			
			ASSERT (!(bClosingWindows && nRet == IDCANCEL)); // sanity check
			
			if (nRet == IDCANCEL)
				return TDCO_CANCELLED;
			else
			{
				tdc.SetModified(FALSE); // so we don't get prompted again
				m_mgrToDoCtrls.SetModifiedStatus(nIndex, FALSE);
			}
		}
		else
			nSave = SaveTaskList(nIndex, NULL, Misc::HasFlag(dwFlags, TDLS_AUTOSAVE));
	}
	
	return nSave; // user did not cancel
}

BOOL CToDoListWnd::CloseToDoCtrl(int nIndex)
{
	ASSERT (nIndex >= 0);
	ASSERT (nIndex < GetTDCCount());

	CFilteredToDoCtrl& tdcSel = GetToDoCtrl();
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);

	tdc.Flush(TRUE);
	
	if (ConfirmSaveTaskList(nIndex, TDLS_CLOSINGTASKLISTS) != TDCO_SUCCESS)
		return FALSE;
	
	// remove any find results associated with this tasklist
	if (m_findDlg.GetSafeHwnd())
		m_findDlg.DeleteResults(&tdc);
	
	CWaitCursor cursor;

	// save off current reminders
	m_reminders.CloseToDoCtrl(tdc);

	int nNewSel = m_mgrToDoCtrls.RemoveToDoCtrl(nIndex, TRUE);
	
	if (nNewSel != -1)
	{
		// if we're closing TDL then the main window will not
		// be visible at this point so we don't have to worry about
		// encrypted tasklists becoming visible. however if as a result
		// of this closure an encrypted tasklist would become visible
		// then we need to prompt for a password and if this fails
		// we must create another tasklist to hide the encrypted one.
		// unless the tasklist being closed was not active and the 
		// new selection hasn't actually changed
		BOOL bCheckPassword = !m_bClosing && (&GetToDoCtrl(nNewSel) != &tdcSel);

		if (!SelectToDoCtrl(nNewSel, bCheckPassword))
		{
			CreateNewTaskList(FALSE);
			RefreshTabOrder();
		}

		if (!m_bClosing)
			Resize();
	}
	
	return TRUE;
}

void CToDoListWnd::OnCloseTasklist() 
{
	int nSel = GetSelToDoCtrl();
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nSel);

	// make sure there are no edits pending
	tdc.Flush(TRUE); 
	
	// check if its a pristine tasklist and the last tasklist and 
	// if so only close it if the default comments type has changed
	if (m_mgrToDoCtrls.IsPristine(nSel) && GetTDCCount() == 1)
		return;
	
	CloseToDoCtrl(nSel);
	
	// if empty then create a new dummy item		
	if (!GetTDCCount())
		CreateNewTaskList(FALSE);
	else
		Resize();
}

BOOL CToDoListWnd::SelectToDoCtrl(LPCTSTR szFilePath, BOOL bCheckPassword, int nNotifyDueTasksBy)
{
	int nCtrl = m_mgrToDoCtrls.FindToDoCtrl(szFilePath);
	
	if (nCtrl != -1)
	{
		SelectToDoCtrl(nCtrl, bCheckPassword, nNotifyDueTasksBy);
		return TRUE;
	}
	
	return FALSE;
}

int CToDoListWnd::GetSelToDoCtrl() const 
{ 
	if (m_tabCtrl.GetSafeHwnd()) 
		return m_tabCtrl.GetCurSel(); 
	else
		return -1;
}

BOOL CToDoListWnd::VerifyTaskListOpen(int nIndex, BOOL bWantNotifyDueTasks)
{
	if (!m_mgrToDoCtrls.IsLoaded(nIndex))
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);
		TSM_TASKLISTINFO storageInfo;
		CString sFilePath = tdc.GetFilePath();

		if (m_mgrToDoCtrls.GetStorageDetails(nIndex, storageInfo))
			sFilePath = storageInfo.EncodeInfo();

		if (OpenTaskList(&tdc, sFilePath, &storageInfo) == TDCO_SUCCESS)
		{
			// make sure hidden windows stay hidden
			if (nIndex != GetSelToDoCtrl())
				tdc.ShowWindow(SW_HIDE);

			// notify readonly
			Resize();
			CheckNotifyReadonly(nIndex);

			m_mgrToDoCtrls.SetLoaded(nIndex);
			m_mgrToDoCtrls.UpdateTabItemText(nIndex);

			// update storage info
			m_mgrToDoCtrls.SetStorageDetails(nIndex, storageInfo);

			if (bWantNotifyDueTasks)
				DoDueTaskNotification(nIndex, Prefs().GetNotifyDueByOnLoad());

			return TRUE;
		}

		return FALSE;
	}

	return TRUE;
}

BOOL CToDoListWnd::SelectToDoCtrl(int nIndex, BOOL bCheckPassword, int nNotifyDueTasksBy)
{
	ASSERT (nIndex >= 0);
	ASSERT (nIndex < GetTDCCount());
	
	// load and show the chosen item
	// we don't need to do a 'open' due task notification if the caller
	// has asked for a notification anyway
	if (!m_bClosing)
	{
		// if the tasklist is not loaded and we verify its loading
		// then we know that the password (if there is one) has been 
		// verified and doesn't need checking again
		if (!m_mgrToDoCtrls.IsLoaded(nIndex) )
		{
			if (!VerifyTaskListOpen(nIndex, nNotifyDueTasksBy == -1))
			{
				// TODO
				return FALSE;
			}
			else
				bCheckPassword = FALSE;
		}
	}

	// validate password first if necessary
	if (bCheckPassword && !VerifyToDoCtrlPassword(nIndex))
		return FALSE;
	
	int nCurSel = GetSelToDoCtrl(); // cache this

	// resize tdc first
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nIndex);
	CRect rTDC;
	
	if (CalcToDoCtrlRect(rTDC))
		tdc.MoveWindow(rTDC);
	
	m_tabCtrl.SetCurSel(nIndex); // this changes the selected CToDoCtrl
	m_tabCtrl.UpdateWindow();
	
	if (!m_bClosing)
		UpdateToDoCtrlPreferences();
	
	const CPreferencesDlg& userPrefs = Prefs();

	tdc.EnableWindow(TRUE);
	tdc.SetFocusToTasks();
	tdc.ShowWindow(SW_SHOW);
	tdc.PauseTimeTracking(FALSE); // always
	tdc.SetMaximizeState(m_nMaxState);

	// if the tasklist is encrypted and todolist always prompts for password
	// then disable Flip3D and Aero Peek
	UpdateAeroFeatures();

	// before hiding the previous selection
	if (nCurSel != -1 && nCurSel != nIndex)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nCurSel);
		
		tdc.ShowWindow(SW_HIDE);
		tdc.EnableWindow(FALSE);
		tdc.PauseTimeTracking(!userPrefs.GetTrackNonActiveTasklists());
	}
	
	if (!m_bClosing)
	{
		if (userPrefs.GetReadonlyReloadOption() != RO_NO)
			OnTimerReadOnlyStatus(nIndex);
		
		if (userPrefs.GetTimestampReloadOption() != RO_NO)
			OnTimerTimestampChange(nIndex);
		
		if (userPrefs.GetEnableSourceControl())
			OnTimerCheckoutStatus(nIndex);
		
		UpdateCaption();
		UpdateStatusbar();
		
		// update the filter selection
		RefreshFilterControls();

		// and the menu icon manager
		InitMenuIconManager();

		// and current directory
		UpdateCwd();

		DoDueTaskNotification(nNotifyDueTasksBy);
	}

	return TRUE;
}

void CToDoListWnd::UpdateAeroFeatures()
{
#ifdef _DEBUG
	BOOL bEnable = !GetToDoCtrl().IsEncrypted();
#else
	BOOL bEnable = (!m_bPasswordPrompting || !GetToDoCtrl().IsEncrypted());
#endif

	// Disable peek and other dynamic views if the active tasklist is encrypted
	GraphicsMisc::EnableFlip3D(*this, bEnable);

	if (!GraphicsMisc::EnableAeroPeek(*this, bEnable))
		GraphicsMisc::ForceIconicRepresentation(*this, !bEnable);
}

void CToDoListWnd::UpdateToDoCtrlPreferences()
{
	// check if this has already been done since the last userPrefs change
	int nSel = GetSelToDoCtrl();
	
	if (m_mgrToDoCtrls.GetNeedsPreferenceUpdate(nSel))
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nSel);

		UpdateToDoCtrlPreferences(&tdc);

		// we do column visibility a bit different because 
		// the manager knows whether the columns have been fiddled 
		// with or not
		CTDCColumnIDArray aColumns;
		m_mgrToDoCtrls.RefreshColumns(nSel, aColumns);

		// and filter bar relies on this tdc's visible columns
		m_filterBar.SetVisibleFilters(aColumns);
		
		// reset flag
		m_mgrToDoCtrls.SetNeedsPreferenceUpdate(nSel, FALSE);
	}
}

void CToDoListWnd::UpdateToDoCtrlPreferences(CFilteredToDoCtrl* pTDC)
{
	const CPreferencesDlg& userPrefs = Prefs();

	CFilteredToDoCtrl& tdc = *pTDC;
	tdc.NotifyBeginPreferencesUpdate();
	
	CTDCStylesMap styles;
	styles.InitHashTable(TDCS_LAST);
	
	styles[TDCS_ALLOWPARENTTIMETRACKING] = userPrefs.GetAllowParentTimeTracking();
	styles[TDCS_ALLOWREFERENCEEDITING] = userPrefs.GetAllowReferenceEditing();
	styles[TDCS_ALWAYSHIDELISTPARENTS] = userPrefs.GetAlwaysHideListParents();
	styles[TDCS_AUTOADJUSTDEPENDENCYDATES] = userPrefs.GetAutoAdjustDependentsDates();
	styles[TDCS_AUTOCALCPERCENTDONE] = userPrefs.GetAutoCalcPercentDone();
	styles[TDCS_AUTOCALCTIMEESTIMATES] = userPrefs.GetAutoCalcTimeEstimates();
	styles[TDCS_AUTOREPOSCTRLS] = userPrefs.GetAutoReposCtrls();
	styles[TDCS_AVERAGEPERCENTSUBCOMPLETION] = userPrefs.GetAveragePercentSubCompletion();
	styles[TDCS_CALCREMAININGTIMEBYDUEDATE] = (userPrefs.GetTimeRemainingCalculation() == PTCP_REMAININGTTIMEISDUEDATE);
	styles[TDCS_CALCREMAININGTIMEBYPERCENT] = (userPrefs.GetTimeRemainingCalculation() == PTCP_REMAININGTTIMEISPERCENTAGE);
	styles[TDCS_CALCREMAININGTIMEBYSPENT] = (userPrefs.GetTimeRemainingCalculation() == PTCP_REMAININGTTIMEISSPENT);
	styles[TDCS_COLORTEXTBYATTRIBUTE] = (userPrefs.GetTextColorOption() == COLOROPT_ATTRIB);
	styles[TDCS_COLORTEXTBYNONE] = (userPrefs.GetTextColorOption() == COLOROPT_NONE);
	styles[TDCS_COLORTEXTBYPRIORITY] = (userPrefs.GetTextColorOption() == COLOROPT_PRIORITY);
	styles[TDCS_COLUMNHEADERSORTING] = userPrefs.GetEnableColumnHeaderSorting();
	styles[TDCS_COMMENTSUSETREEFONT] = userPrefs.GetCommentsUseTreeFont();
	styles[TDCS_CONFIRMDELETE] = userPrefs.GetConfirmDelete();
	styles[TDCS_DISPLAYHMSTIMEFORMAT] = userPrefs.GetUseHMSTimeFormat();
	styles[TDCS_DONEHAVELOWESTPRIORITY] = userPrefs.GetDoneTasksHaveLowestPriority();
	styles[TDCS_DONEHAVELOWESTRISK] = userPrefs.GetDoneTasksHaveLowestRisk();
	styles[TDCS_DUEHAVEHIGHESTPRIORITY] = userPrefs.GetDueTasksHaveHighestPriority();
	styles[TDCS_FOCUSTREEONENTER] = userPrefs.GetFocusTreeOnEnter();
	styles[TDCS_FULLROWSELECTION] = userPrefs.GetFullRowSelection();
	styles[TDCS_HIDEDONETIMEFIELD] = userPrefs.GetHideDoneTimeField();
	styles[TDCS_HIDEDUETIMEFIELD] = userPrefs.GetHideDueTimeField();
	styles[TDCS_HIDEPERCENTFORDONETASKS] = userPrefs.GetHidePercentForDoneTasks();
	styles[TDCS_HIDEPRIORITYNUMBER] = userPrefs.GetHidePriorityNumber();
	styles[TDCS_HIDESTARTDUEFORDONETASKS] = userPrefs.GetHideStartDueForDoneTasks();
	styles[TDCS_HIDESTARTTIMEFIELD] = userPrefs.GetHideStartTimeField();
	styles[TDCS_HIDEZEROPERCENTDONE] = userPrefs.GetHideZeroPercentDone();
	styles[TDCS_HIDEZEROTIMECOST] = userPrefs.GetHideZeroTimeCost();
	styles[TDCS_INCLUDEDONEINAVERAGECALC] = userPrefs.GetIncludeDoneInAverageCalc();
	styles[TDCS_INCLUDEDONEINPRIORITYCALC] = userPrefs.GetIncludeDoneInPriorityRiskCalc();
	styles[TDCS_INCLUDEDONEINRISKCALC] = userPrefs.GetIncludeDoneInPriorityRiskCalc();
	styles[TDCS_INCLUDEUSERINCHECKOUT] = userPrefs.GetIncludeUserNameInCheckout();
	styles[TDCS_LOGTASKTIMESEPARATELY] = userPrefs.GetLogTaskTimeSeparately();
	styles[TDCS_LOGTIMETRACKING] = userPrefs.GetLogTimeTracking();
	styles[TDCS_NODUEDATEISDUETODAY] = userPrefs.GetNoDueDateIsDueToday();
	styles[TDCS_PAUSETIMETRACKINGONSCRNSAVER] = !userPrefs.GetTrackOnScreenSaver();
	styles[TDCS_REFILTERONMODIFY] = userPrefs.GetReFilterOnModify();
	styles[TDCS_RESORTONMODIFY] = userPrefs.GetReSortOnModify();
	styles[TDCS_RESTOREFILTERS] = userPrefs.GetRestoreTasklistFilters();
	styles[TDCS_RIGHTSIDECOLUMNS] = userPrefs.GetShowColumnsOnRight();
	styles[TDCS_ROUNDTIMEFRACTIONS] = userPrefs.GetRoundTimeFractions();
	styles[TDCS_SHAREDCOMMENTSHEIGHT] = userPrefs.GetSharedCommentsHeight();
	styles[TDCS_SHOWCOMMENTSALWAYS] = userPrefs.GetShowCommentsAlways();
	styles[TDCS_SHOWCOMMENTSINLIST] = userPrefs.GetShowComments();
	styles[TDCS_SHOWCTRLSASCOLUMNS] = userPrefs.GetShowCtrlsAsColumns();
	styles[TDCS_SHOWDATESINISO] = userPrefs.GetDisplayDatesInISO();
	styles[TDCS_SHOWDEFAULTTASKICONS] = userPrefs.GetShowDefaultTaskIcons();
	styles[TDCS_SHOWFIRSTCOMMENTLINEINLIST] = userPrefs.GetDisplayFirstCommentLine();
	styles[TDCS_SHOWINFOTIPS] = userPrefs.GetShowInfoTips();
	styles[TDCS_SHOWNONFILEREFSASTEXT] = userPrefs.GetShowNonFilesAsText();
	styles[TDCS_SHOWPARENTSASFOLDERS] = userPrefs.GetShowParentsAsFolders();
	styles[TDCS_SHOWPATHINHEADER] = userPrefs.GetShowPathInHeader();
	styles[TDCS_SHOWPERCENTASPROGRESSBAR] = userPrefs.GetShowPercentAsProgressbar();
	styles[TDCS_SHOWPROJECTNAME] = m_bShowProjectName;
	styles[TDCS_SHOWSUBTASKCOMPLETION] = userPrefs.GetShowSubtaskCompletion();
	styles[TDCS_SHOWTREELISTBAR] = m_bShowTreeListBar;
	styles[TDCS_SHOWWEEKDAYINDATES] = userPrefs.GetShowWeekdayInDates();
	styles[TDCS_SORTDONETASKSATBOTTOM] = userPrefs.GetSortDoneTasksAtBottom();
	styles[TDCS_SORTVISIBLETASKSONLY] = FALSE;//userPrefs.GetSortVisibleOnly();
	styles[TDCS_STRIKETHOUGHDONETASKS] = userPrefs.GetStrikethroughDone();
	styles[TDCS_TASKCOLORISBACKGROUND] = userPrefs.GetColorTaskBackground();
	styles[TDCS_TRACKSELECTEDTASKONLY] = !userPrefs.GetTrackNonSelectedTasks();
	styles[TDCS_TREATSUBCOMPLETEDASDONE] = userPrefs.GetTreatSubCompletedAsDone();
	styles[TDCS_TREECHECKBOXES] = userPrefs.GetTreeCheckboxes();
	styles[TDCS_TREETASKICONS] = userPrefs.GetTreeTaskIcons();
	styles[TDCS_USEEARLIESTDUEDATE] = (userPrefs.GetDueDateCalculation() == PTCP_EARLIESTDUEDATE);
	styles[TDCS_USEEARLIESTSTARTDATE] = (userPrefs.GetStartDateCalculation() == PTCP_EARLIESTSTARTDATE);
	styles[TDCS_USEHIGHESTPRIORITY] = userPrefs.GetUseHighestPriority();
	styles[TDCS_USEHIGHESTRISK] = userPrefs.GetUseHighestRisk();
	styles[TDCS_USELATESTDUEDATE] = (userPrefs.GetDueDateCalculation() == PTCP_LATESTDUEDATE);
	styles[TDCS_USELATESTSTARTDATE] = (userPrefs.GetStartDateCalculation() == PTCP_LATESTSTARTDATE);
	styles[TDCS_USEPERCENTDONEINTIMEEST] = userPrefs.GetUsePercentDoneInTimeEst();
	styles[TDCS_USES3RDPARTYSOURCECONTROL] = userPrefs.GetUsing3rdPartySourceControl();
	styles[TDCS_WARNADDDELETEARCHIVE] = userPrefs.GetWarnAddDeleteArchive();
	styles[TDCS_WEIGHTPERCENTCALCBYNUMSUB] = userPrefs.GetWeightPercentCompletionByNumSubtasks();

	// source control
	BOOL bSrcControl = m_mgrToDoCtrls.PathSupportsSourceControl(tdc.GetFilePath());
	
	styles[TDCS_ENABLESOURCECONTROL] = bSrcControl;
	styles[TDCS_CHECKOUTONLOAD] = bSrcControl && userPrefs.GetAutoCheckOut();
	
	// set the styles in one hit
	tdc.SetStyles(styles);

	// layout
	tdc.SetLayoutPositions((TDC_UILOCATION)userPrefs.GetControlsPos(), 
							(TDC_UILOCATION)userPrefs.GetCommentsPos(), 
							TRUE);
	
	// info tips
	tdc.SetMaxInfotipCommentsLength(userPrefs.GetMaxInfoTipCommentsLength());
	
	// update default task preferences
	tdc.SetDefaultTaskAttributes(m_tdiDefault);

	// default string lists
	CStringArray aItems;
	
	if (userPrefs.GetDefaultListItems(PTDP_CATEGORY, aItems))
		aItems.Append(m_tdiDefault.aCategories);

	tdc.SetDefaultCategoryNames(aItems, userPrefs.GetCategoryListIsReadonly());

	if (userPrefs.GetDefaultListItems(PTDP_ALLOCTO, aItems))
		aItems.Append(m_tdiDefault.aAllocTo);
	
	tdc.SetDefaultAllocToNames(aItems, userPrefs.GetAllocToListIsReadonly());
	
	userPrefs.GetDefaultListItems(PTDP_STATUS, aItems);
	tdc.SetDefaultStatusNames(aItems, userPrefs.GetStatusListIsReadonly());
	
	userPrefs.GetDefaultListItems(PTDP_ALLOCBY, aItems);
	tdc.SetDefaultAllocByNames(aItems, userPrefs.GetAllocByListIsReadonly());
		
	// fonts
	if (!m_fontTree.GetSafeHandle() || !m_fontComments.GetSafeHandle())
	{
		CString sFaceName;
		int nFontSize;
		
		if (!m_fontTree.GetSafeHandle() && userPrefs.GetTreeFont(sFaceName, nFontSize))
			m_fontTree.Attach(GraphicsMisc::CreateFont(sFaceName, nFontSize));
		
		if (!m_fontComments.GetSafeHandle() && userPrefs.GetCommentsFont(sFaceName, nFontSize))
			m_fontComments.Attach(GraphicsMisc::CreateFont(sFaceName, nFontSize));
	}
	
	tdc.SetTreeFont(m_fontTree);
	tdc.SetCommentsFont(m_fontComments);
	
	// colours
	tdc.SetGridlineColor(userPrefs.GetGridlineColor());
	tdc.SetCompletedTaskColor(userPrefs.GetDoneTaskColor());
	tdc.SetAlternateLineColor(userPrefs.GetAlternateLineColor());
	tdc.SetFlaggedTaskColor(userPrefs.GetFlaggedTaskColor());
	tdc.SetReferenceTaskColor(userPrefs.GetReferenceTaskColor());
	tdc.SetPriorityColors(m_aPriorityColors);

	COLORREF color, crToday;
	userPrefs.GetStartedTaskColors(color, crToday);
	tdc.SetStartedTaskColors(color, crToday);

	userPrefs.GetDueTaskColors(color, crToday);
	tdc.SetDueTaskColors(color, crToday);
	
	CTDCColorMap mapColors;
	CAttribColorArray aColors;

	TDC_ATTRIBUTE nAttrib = TDCA_NONE;
	int nColor = userPrefs.GetAttributeColors(nAttrib, aColors);
	
	while (nColor--)
	{
		ATTRIBCOLOR& ac = aColors[nColor];
		mapColors[ac.sAttrib] = ac.color;
	}
	tdc.SetAttributeColors(nAttrib, mapColors);

	// drag drop
	tdc.SetSubtaskDragDropPos(userPrefs.GetNewSubtaskPos() == PUIP_TOP);
	
	// misc
	tdc.SetMaxColumnWidth(userPrefs.GetMaxColumnWidth());
	tdc.SetCheckImageList(m_ilCheckboxes);
	tdc.SetPercentDoneIncrement(userPrefs.GetPercentDoneIncrement());

	CString sStatus;
	userPrefs.GetCompletionStatus(sStatus);
	tdc.SetCompletionStatus(sStatus);
	
	tdc.Flush(); // clear any outstanding issues

	// we're done
	tdc.NotifyEndPreferencesUpdate();
}

void CToDoListWnd::UpdateTabSwitchTooltip()
{
	CToolTipCtrl* pTT = m_tabCtrl.GetToolTips();
	
	if (pTT)
	{
		// get the string for tab switching
		CString sShortcut = m_mgrShortcuts.GetShortcutTextByCmd(ID_VIEW_NEXT);
		
		if (sShortcut.IsEmpty())
			sShortcut = m_mgrShortcuts.GetShortcutTextByCmd(ID_VIEW_PREV);
		
		pTT->DelTool(&m_tabCtrl); // always
		
		if (!sShortcut.IsEmpty())
		{
			CEnString sTip(IDS_TABSWITCHTOOLTIP, sShortcut);
			pTT->AddTool(&m_tabCtrl, sTip);
		}
	}
}

void CToDoListWnd::OnSaveall() 
{
	SaveAll(TDLS_INCLUDEUNSAVED | TDLS_FLUSH);
}

void CToDoListWnd::OnUpdateSaveall(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_mgrToDoCtrls.AnyIsModified());
}

void CToDoListWnd::OnCloseall() 
{
	// save first
	TDC_FILE nSaveAll = SaveAll(TDLS_INCLUDEUNSAVED | TDLS_CLOSINGTASKLISTS | TDLS_FLUSH);

	if (nSaveAll != TDCO_SUCCESS)
		return;

	// remove tasklists
	int nCtrl = GetTDCCount();
	
	while (nCtrl--)
		m_mgrToDoCtrls.RemoveToDoCtrl(nCtrl, TRUE);

	// if empty then create a new dummy item		
	if (!GetTDCCount())
		CreateNewTaskList(FALSE);
	else
		Resize();
}

void CToDoListWnd::OnUpdateCloseall(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetTDCCount());
}

BOOL CToDoListWnd::DoQueryEndSession(BOOL bQuery, BOOL bEnding)
{
	HWND hWnd = GetSafeHwnd();

	// what we do here depends on whether we're on Vista or not
	// we test for this by trying to load the new API functions
	if (bQuery)
	{
		CEnString sReason(IDS_SHUTDOWNBLOCKREASON);

		// if Vista and handling WM_QUERYENDSESSION
		// we register our reason and return TRUE to
		// get more time to clean up in WM_ENDSESSION
		if (Misc::ShutdownBlockReasonCreate(hWnd, sReason))
			return TRUE;

		// else we're XP so we return TRUE to let shutdown continue
		return TRUE;
	}

	// else do a proper shutdown
	m_bEndingSession = TRUE;

	DoExit(FALSE, bEnding);
		
	// cleanup our shutdown reason if not handled in DoExit
	Misc::ShutdownBlockReasonDestroy(hWnd);

	// and return anything because it's ignored
	return TRUE;
}

BOOL CToDoListWnd::OnQueryEndSession() 
{
	if (!CFrameWnd::OnQueryEndSession())
		return FALSE;
	
	return DoQueryEndSession(TRUE, FALSE);
}

void CToDoListWnd::OnEndSession(BOOL bEnding) 
{
	CFrameWnd::OnEndSession(bEnding);

	DoQueryEndSession(FALSE, bEnding);
}

void CToDoListWnd::OnExit()
{
	DoExit();
}

void CToDoListWnd::OnMinimizeToTray()
{
	MinimizeToTray();
}

BOOL CToDoListWnd::DoExit(BOOL bRestart, BOOL bClosingWindows) 
{
	ASSERT (!(bClosingWindows && bRestart));

    // save all first to ensure new tasklists get reloaded on startup
	DWORD dwSaveFlags = TDLS_INCLUDEUNSAVED | TDLS_CLOSINGTASKLISTS | TDLS_FLUSH;

	if (bClosingWindows)
		dwSaveFlags |= TDLS_CLOSINGWINDOWS;

	TDC_FILE nSaveAll = SaveAll(dwSaveFlags);

	if (nSaveAll != TDCO_SUCCESS)
        return FALSE; // user cancelled
	
	// save settings before we close the tasklists
	// to snapshot the currently open tasklists
	SaveSettings(); 

	m_bClosing = TRUE;
	
	// hide the window as soon as possible so users do not
	// see the machinations of closing down
	if (m_bFindShowing)
		m_findDlg.ShowWindow(SW_HIDE);

	ShowWindow(SW_HIDE);
	
	// remove tasklists
	int nCtrl = GetTDCCount();
		
	while (nCtrl--)
		VERIFY(CloseToDoCtrl(nCtrl)); // shouldn't fail now
	
	// if there are any still open then the user must have cancelled else destroy the window
	ASSERT (GetTDCCount() == 0);
	
	if (GetTDCCount() == 0)
	{
		// this will save any left over settings 
		// when it goes out of scope
		{
			CPreferences prefs; 

			m_mgrImportExport.Release();
			m_tbHelper.Release();
			m_mgrShortcuts.Release(&prefs);
			m_mgrImportExport.Release();
			m_mgrUIExtensions.Release();
			m_mgrStorage.Release();
			
			CFocusWatcher::Release();
			CMouseWheelMgr::Release();
			CEditShortcutMgr::Release();
		}

		// cleanup our shutdown reason
		Misc::ShutdownBlockReasonDestroy(*this);

		DestroyWindow();
		
		if (bRestart)
		{
			CString sParams = AfxGetApp()->m_lpCmdLine;
			::ShellExecute(NULL, NULL, FileMisc::GetModuleFileName(), sParams, NULL, SW_SHOW);
		}

		return TRUE;
	}

	// cancel
	m_bClosing = FALSE;
	return FALSE;
}

void CToDoListWnd::OnImportTasklist() 
{
	CString sImportPath;
	TDLID_IMPORTTO nImportTo = TDIT_NEWTASKLIST;
	int nImporter = -1;
	
	do
	{
		CTDLImportDialog dialog(m_mgrImportExport);

		if (dialog.DoModal() == IDOK)
		{
			// check file can be opened
			nImportTo = dialog.GetImportTo();
			nImporter = dialog.GetImporterIndex();

			if (dialog.GetImportFromClipboard())
			{
				sImportPath = FileMisc::GetTempFileName(_T("ToDoList.import"), _T("txt"));
				FileMisc::SaveFile(sImportPath, dialog.GetImportClipboardText());
			}
			else
				sImportPath = dialog.GetImportFilePath();

			// check file accessibility
			if (sImportPath.IsEmpty() || FileMisc::CanOpenFile(sImportPath, TRUE))
				break;

			// else
			MessageBox(CEnString(IDS_IMPORTFILE_CANTOPEN, sImportPath), IDS_IMPORTTASKLIST_TITLE);
			sImportPath.Empty();
		}
		else // cancel
			return;
	}
	while (sImportPath.IsEmpty());

	// load/import tasks
	DOPROGRESS(IDS_IMPORTPROGRESS)

	// do the import
	CTaskFile tasks;
	BOOL bCancel = !m_mgrImportExport.ImportTaskList(sImportPath, &tasks, nImporter);

	if (bCancel)
		return;

	if (!tasks.GetTaskCount())
	{
		// notify user
		MessageBox(IDS_NOTASKSIMPORTED);
	}
	else
	{
		if (nImportTo == TDIT_NEWTASKLIST)
			VERIFY(CreateNewTaskList(FALSE));
		
		CFilteredToDoCtrl& tdc = GetToDoCtrl(); 
		TDC_INSERTWHERE nWhere = (nImportTo == TDIT_SELECTEDTASK) ? TDC_INSERTATTOPOFSELTASK : TDC_INSERTATTOP;
		
		VERIFY(tdc.InsertTasks(tasks, nWhere));

		// if a new tasklist then set the project name
		if (nImportTo == TDIT_NEWTASKLIST)
			tdc.SetProjectName(tasks.GetProjectName());

		m_mgrToDoCtrls.RefreshModifiedStatus(GetSelToDoCtrl());
		UpdateCaption();
	}
}

void CToDoListWnd::OnSetPriority(UINT nCmdID) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (!tdc.IsReadOnly() && tdc.HasSelection())
	{
		if (nCmdID == ID_EDIT_SETPRIORITYNONE) 
			tdc.SetSelectedTaskPriority(-2);
		else
			tdc.SetSelectedTaskPriority(nCmdID - ID_EDIT_SETPRIORITY0);
	}
}

void CToDoListWnd::OnUpdateSetPriority(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && nSelCount);
	
	int nPriority = pCmdUI->m_nID - ID_EDIT_SETPRIORITY0;
	
	if (pCmdUI->m_nID == ID_EDIT_SETPRIORITYNONE)
		nPriority = -2;
	
	pCmdUI->SetCheck(nPriority == tdc.GetSelectedTaskPriority());
}

void CToDoListWnd::OnEditSetfileref() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (!tdc.IsReadOnly() && tdc.HasSelection())
	{
		CPreferences prefs;
		CFileOpenDialog dialog(IDS_SETFILEREF_TITLE, 
								NULL, 
								tdc.GetSelectedTaskFileRef(), 
								EOFN_DEFAULTOPEN, 
								CEnString(IDS_ALLFILEFILTER));
		
		if (dialog.DoModal(&prefs) == IDOK)
			tdc.SetSelectedTaskFileRef(dialog.GetPathName());
	}
}

void CToDoListWnd::OnUpdateEditSetfileref(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.HasSelection());
}

void CToDoListWnd::OnEditOpenfileref() 
{
	GetToDoCtrl().GotoSelectedTaskFileRef();
}

void CToDoListWnd::OnUpdateEditOpenfileref(CCmdUI* pCmdUI) 
{
	CEnString sFileRef = GetToDoCtrl().GetSelectedTaskFileRef();
	
	if (sFileRef.IsEmpty())
		pCmdUI->Enable(FALSE);
	else
	{
		pCmdUI->Enable(TRUE);

		// restrict file length to 250 pixels
		CClientDC dc(this);
		sFileRef.FormatDC(&dc, 250, ES_PATH);
		pCmdUI->SetText(sFileRef);
	}
}

LRESULT CToDoListWnd::OnPreferencesDefaultListChange(WPARAM wp, LPARAM lp)
{
	// decode params
	int nList = LOWORD(wp);
	BOOL bAdd = HIWORD(wp);
	LPCTSTR szItem = (LPCTSTR)lp;

	switch (nList)
	{
	case PTDP_ALLOCBY:
		break;

	case PTDP_ALLOCTO:
		break;

	case PTDP_STATUS:
		break;

	case PTDP_CATEGORY:
		break;
	}

	return 0L;
}

void CToDoListWnd::PopulateToolArgs(USERTOOLARGS& args) const
{
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();
		
	args.sTasklist = tdc.GetFilePath();
	args.sTaskTitle = tdc.GetSelectedTaskTitle();
	args.sTaskExtID = tdc.GetSelectedTaskExtID();
	args.sTaskComments = tdc.GetSelectedTaskComments();
	args.sTaskFileLink = tdc.GetSelectedTaskFileRef();
	args.sTaskAllocBy = tdc.GetSelectedTaskAllocBy();
	
	CDWordArray aIDs;
	DWORD dwTemp;
	
	if (tdc.GetSelectedTaskIDs(aIDs, dwTemp, FALSE))
		args.sTaskIDs = Misc::FormatArray(aIDs, _T("|"));
	
	CStringArray aAllocTo;
	
	if (tdc.GetSelectedTaskAllocTo(aAllocTo))
		args.sTaskAllocTo = Misc::FormatArray(aAllocTo, _T("|"));
}

LRESULT CToDoListWnd::OnPreferencesTestTool(WPARAM /*wp*/, LPARAM lp)
{
	USERTOOL* pTool = (USERTOOL*)lp;
	
	if (pTool)
	{
		USERTOOLARGS args;
		PopulateToolArgs(args);

		CToolsHelper th(Prefs().GetEnableTDLExtension(), ID_TOOLS_USERTOOL1);
		th.TestTool(*pTool, args, m_pPrefs);
	}
	
	return 0;
}

LRESULT CToDoListWnd::OnPreferencesClearMRU(WPARAM /*wp*/, LPARAM /*lp*/)
{
	m_mruList.RemoveAll();
	m_mruList.WriteList(CPreferences());
	
	return 0;
}

LRESULT CToDoListWnd::OnPreferencesCleanupDictionary(WPARAM /*wp*/, LPARAM lp)
{
	LPCTSTR szLangFile = (LPCTSTR)lp;
	ASSERT(szLangFile && *szLangFile);

	if (szLangFile && *szLangFile)
	{
		DOPROGRESS(IDS_CLEANINGDICTPROGRESS)

		CString sMasterDict = FileMisc::GetFolderFromFilePath(szLangFile);
		sMasterDict += _T("\\YourLanguage.csv");

		return CLocalizer::CleanupDictionary(sMasterDict);
	}
	
	return 0;
}

void CToDoListWnd::PrepareSortMenu(CMenu* pMenu)
{
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();
		
	if (Prefs().GetShowEditMenuAsColumns())
	{
		int nCountLastSep = 0;
		
		for (int nItem = 0; nItem < (int)pMenu->GetMenuItemCount(); nItem++)
		{
			BOOL bDelete = FALSE;
			BOOL bIsSeparator = FALSE;

			UINT nMenuID = pMenu->GetMenuItemID(nItem);
			
			switch (nMenuID)
			{
			case ID_SORT_BYCOST:		bDelete = !tdc.IsColumnShowing(TDCC_COST); break;
			case ID_SORT_BYFLAG:		bDelete = !tdc.IsColumnShowing(TDCC_FLAG); break; 
			case ID_SORT_BYDONEDATE:	bDelete = !tdc.IsColumnShowing(TDCC_DONEDATE); break; 
			case ID_SORT_BYPRIORITY:	bDelete = !tdc.IsColumnShowing(TDCC_PRIORITY); break; 
			case ID_SORT_BYPERCENT:		bDelete = !tdc.IsColumnShowing(TDCC_PERCENT); break; 
			case ID_SORT_BYSTARTDATE:	bDelete = !tdc.IsColumnShowing(TDCC_STARTDATE); break; 
			case ID_SORT_BYDUEDATE:		bDelete = !tdc.IsColumnShowing(TDCC_DUEDATE); break; 
			case ID_SORT_BYTIMEEST:		bDelete = !tdc.IsColumnShowing(TDCC_TIMEEST); break; 
			case ID_SORT_BYID:			bDelete = !tdc.IsColumnShowing(TDCC_ID); break; 
			case ID_SORT_BYDONE:		bDelete = !tdc.IsColumnShowing(TDCC_DONE); break; 
			case ID_SORT_BYALLOCBY:		bDelete = !tdc.IsColumnShowing(TDCC_ALLOCBY); break; 
			case ID_SORT_BYSTATUS:		bDelete = !tdc.IsColumnShowing(TDCC_STATUS); break; 
			case ID_SORT_BYCATEGORY:	bDelete = !tdc.IsColumnShowing(TDCC_CATEGORY); break; 
			case ID_SORT_BYTIMESPENT:	bDelete = !tdc.IsColumnShowing(TDCC_TIMESPENT); break; 
			case ID_SORT_BYALLOCTO:		bDelete = !tdc.IsColumnShowing(TDCC_ALLOCTO); break; 
			case ID_SORT_BYCREATIONDATE:bDelete = !tdc.IsColumnShowing(TDCC_CREATIONDATE); break; 
			case ID_SORT_BYCREATEDBY:	bDelete = !tdc.IsColumnShowing(TDCC_CREATEDBY); break; 
			case ID_SORT_BYMODIFYDATE:	bDelete = !tdc.IsColumnShowing(TDCC_LASTMOD); break; 
			case ID_SORT_BYRISK:		bDelete = !tdc.IsColumnShowing(TDCC_RISK); break; 
			case ID_SORT_BYEXTERNALID:	bDelete = !tdc.IsColumnShowing(TDCC_EXTERNALID); break; 
			case ID_SORT_BYVERSION:		bDelete = !tdc.IsColumnShowing(TDCC_VERSION); break; 
			case ID_SORT_BYRECURRENCE:	bDelete = !tdc.IsColumnShowing(TDCC_RECURRENCE); break; 
			case ID_SORT_BYREMAINING:	bDelete = !tdc.IsColumnShowing(TDCC_REMAINING); break; 
			case ID_SORT_BYRECENTEDIT:	bDelete = !tdc.IsColumnShowing(TDCC_RECENTEDIT); break; 
			case ID_SORT_BYICON:		bDelete = !tdc.IsColumnShowing(TDCC_ICON); break; 
			case ID_SORT_BYFILEREF:		bDelete = !tdc.IsColumnShowing(TDCC_FILEREF); break; 
			case ID_SORT_BYTIMETRACKING:bDelete = !tdc.IsColumnShowing(TDCC_TRACKTIME); break; 
			case ID_SORT_BYPATH:		bDelete = !tdc.IsColumnShowing(TDCC_PATH); break; 
			case ID_SORT_BYTAG:			bDelete = !tdc.IsColumnShowing(TDCC_TAGS); break; 
			case ID_SORT_BYDEPENDENCY:	bDelete = !tdc.IsColumnShowing(TDCC_DEPENDENCY); break; 
	//		case ID_SORT_BYCOLOR: bDelete = (Prefs().GetTextColorOption() != COLOROPT_DEFAULT); break; 

			case ID_SEPARATOR: 
				bIsSeparator = TRUE;
				bDelete = (nCountLastSep == 0);
				nCountLastSep = 0;
				break;

			default: bDelete = FALSE; break; 
			}

			// delete the item else increment the count since the last separator
			if (bDelete)
			{
				pMenu->DeleteMenu(nItem, MF_BYPOSITION);
				nItem--;
			}
			else if (!bIsSeparator)
				nCountLastSep++;
		}
	}

	// custom sort columns

	// first delete all custom columns and the related separator
	int nPosUnsorted = -1;

	for (int nItem = 0; nItem < (int)pMenu->GetMenuItemCount(); nItem++)
	{
		UINT nMenuID = pMenu->GetMenuItemID(nItem);

		if (nMenuID >= ID_SORT_BYCUSTOMCOLUMN_FIRST && nMenuID <= ID_SORT_BYCUSTOMCOLUMN_LAST)
		{
			pMenu->DeleteMenu(nItem, MF_BYPOSITION);
			nItem--;
		}
	}

	// separator is just before the separator before 'unsorted entry'
	int nInsert = CEnMenu::GetMenuItemPos(pMenu->GetSafeHmenu(), ID_SORT_NONE) - 1;
	ASSERT(nInsert >= 0);

	// delete separator if exist
	if (nInsert > 0 && pMenu->GetMenuItemID(nInsert - 1) == 0)
	{
		nInsert--;
		pMenu->DeleteMenu(nInsert, MF_BYPOSITION);
	}

	// then re-add
	CTDCCustomAttribDefinitionArray aAttribDefs;

	if (tdc.GetCustomAttributeDefs(aAttribDefs))
	{
		// re-add separator on demand
		BOOL bWantSep = TRUE;

		for (int nCol = 0; nCol < aAttribDefs.GetSize(); nCol++)
		{
			const TDCCUSTOMATTRIBUTEDEFINITION& attribDef = aAttribDefs[nCol];

			if (attribDef.bEnabled && attribDef.SupportsFeature(TDCCAF_SORT))
			{
				if (bWantSep)
				{
					bWantSep = FALSE;
					pMenu->InsertMenu(nInsert, MF_BYPOSITION);
					nInsert++;
				}

				UINT nMenuID = (attribDef.GetColumnID() - TDCC_CUSTOMCOLUMN_FIRST) + ID_SORT_BYCUSTOMCOLUMN_FIRST;
				CEnString sColumn(IDS_CUSTOMCOLUMN, attribDef.sLabel);

				pMenu->InsertMenu(nInsert, MF_BYPOSITION, nMenuID, sColumn);
				nInsert++;
			}
		}
	}
}

void CToDoListWnd::PrepareSourceControlMenu(CMenu* pMenu)
{
	if (Prefs().GetEnableSourceControl())
		return;

	int nCountLastSep = 0;
	
	for (int nItem = 0; nItem < (int)pMenu->GetMenuItemCount(); nItem++)
	{
		BOOL bDelete = FALSE;
		BOOL bIsSeparator = FALSE;

		UINT nMenuID = pMenu->GetMenuItemID(nItem);
		
		switch (nMenuID)
		{
		case -1: // its a popup so recursively handle it
			{
				CMenu* pPopup = pMenu->GetSubMenu(nItem);
				PrepareEditMenu(pPopup);
				
				// if the popup is now empty remove it too
				bDelete = !pPopup->GetMenuItemCount();
			}
			break;
			
        case ID_TOOLS_CHECKIN:
        case ID_TOOLS_CHECKOUT:
			bDelete = TRUE;
			break;
			
		case ID_SEPARATOR: 
			bIsSeparator = TRUE;
			bDelete = (nCountLastSep == 0);
			nCountLastSep = 0;
			break;

		default: bDelete = FALSE; break; 
		}

		// delete the item else increment the count since the last separator
		if (bDelete)
		{
			pMenu->DeleteMenu(nItem, MF_BYPOSITION);
			nItem--;
		}
		else if (!bIsSeparator)
			nCountLastSep++;
	}
}

void CToDoListWnd::PrepareEditMenu(CMenu* pMenu)
{
	if (!Prefs().GetShowEditMenuAsColumns())
		return;

	const CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	int nCountLastSep = 0;
	
	for (int nItem = 0; nItem < (int)pMenu->GetMenuItemCount(); nItem++)
	{
		BOOL bDelete = FALSE;
		BOOL bIsSeparator = FALSE;

		UINT nMenuID = pMenu->GetMenuItemID(nItem);
		
		switch (nMenuID)
		{
		case -1: // its a popup so recursively handle it
			{
				CMenu* pPopup = pMenu->GetSubMenu(nItem);

				if (pPopup)
				{
					PrepareEditMenu(pPopup);
				
					// if the popup is now empty remove it too
					bDelete = !pPopup->GetMenuItemCount();
				}
			}
			break;
			
		case ID_EDIT_TASKCOLOR:
		case ID_EDIT_CLEARTASKCOLOR:
			bDelete = (Prefs().GetTextColorOption() != COLOROPT_DEFAULT);
			break;
			
        case ID_EDIT_DECTASKPRIORITY:
        case ID_EDIT_INCTASKPRIORITY:
		case ID_EDIT_SETPRIORITYNONE: 
        case ID_EDIT_SETPRIORITY0:
        case ID_EDIT_SETPRIORITY1:
        case ID_EDIT_SETPRIORITY2:
        case ID_EDIT_SETPRIORITY3:
        case ID_EDIT_SETPRIORITY4:
        case ID_EDIT_SETPRIORITY5:
        case ID_EDIT_SETPRIORITY6:
        case ID_EDIT_SETPRIORITY7:
        case ID_EDIT_SETPRIORITY8:
        case ID_EDIT_SETPRIORITY9:
        case ID_EDIT_SETPRIORITY10:
			bDelete = !tdc.IsColumnShowing(TDCC_PRIORITY);
			break;
			
		case ID_EDIT_OFFSETDATES:
			bDelete = !(tdc.IsColumnShowing(TDCC_STARTDATE) ||
						tdc.IsColumnShowing(TDCC_DUEDATE) || 
						tdc.IsColumnShowing(TDCC_DONEDATE));
			break;
			
        case ID_EDIT_CLOCK_TASK:
			bDelete = !(tdc.IsColumnShowing(TDCC_TRACKTIME) ||
						tdc.IsColumnShowing(TDCC_TIMESPENT));
			break;

        case ID_SHOWTIMELOGFILE:
			bDelete = !((tdc.IsColumnShowing(TDCC_TRACKTIME) ||
						tdc.IsColumnShowing(TDCC_TIMESPENT)) &&
						Prefs().GetLogTimeTracking());
			break;
			
        case ID_EDIT_DECTASKPERCENTDONE:	bDelete = !tdc.IsColumnShowing(TDCC_PERCENT); break;
        case ID_EDIT_INCTASKPERCENTDONE:	bDelete = !tdc.IsColumnShowing(TDCC_PERCENT); break;
        case ID_EDIT_OPENFILEREF:			bDelete = !tdc.IsColumnShowing(TDCC_FILEREF); break;
		case ID_EDIT_SETFILEREF:			bDelete = !tdc.IsColumnShowing(TDCC_FILEREF); break;
        case ID_EDIT_FLAGTASK:				bDelete = !tdc.IsColumnShowing(TDCC_FLAG); break;
        case ID_EDIT_RECURRENCE:			bDelete = !tdc.IsColumnShowing(TDCC_RECURRENCE); break;
        case ID_EDIT_GOTODEPENDENCY:		bDelete = !tdc.IsColumnShowing(TDCC_DEPENDENCY); break;

        case ID_EDIT_SETTASKICON:			
        case ID_EDIT_CLEARTASKICON:	
			bDelete = !(tdc.IsColumnShowing(TDCC_ICON) || Prefs().GetTreeTaskIcons()); 
			break;

		case ID_SEPARATOR: 
			bIsSeparator = TRUE;
			bDelete = (nCountLastSep == 0);
			nCountLastSep = 0;
			break;

		default: bDelete = FALSE; break; 
		}

		// delete the item else increment the count since the last separator
		if (bDelete)
		{
			pMenu->DeleteMenu(nItem, MF_BYPOSITION);
			nItem--;
		}
		else if (!bIsSeparator)
			nCountLastSep++;
	}

	// make sure last item is not a separator
	int nLastItem = (int)pMenu->GetMenuItemCount() - 1;

	if (pMenu->GetMenuItemID(nLastItem) == 0)
		pMenu->DeleteMenu(nLastItem, MF_BYPOSITION);

}

void CToDoListWnd::OnViewNext() 
{
	if (GetTDCCount() < 2)
		return;
	
	int nNext = GetSelToDoCtrl() + 1;
	
	if (nNext >= GetTDCCount())
		nNext = 0;
	
	SelectToDoCtrl(nNext, TRUE, Prefs().GetNotifyDueByOnSwitch());
}

void CToDoListWnd::OnUpdateViewNext(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetTDCCount() > 1);
}

void CToDoListWnd::OnViewPrev() 
{
	if (GetTDCCount() < 2)
		return;
	
	int nPrev = GetSelToDoCtrl() - 1;
	
	if (nPrev < 0)
		nPrev = GetTDCCount() - 1;
	
	SelectToDoCtrl(nPrev, TRUE, Prefs().GetNotifyDueByOnSwitch());
}

void CToDoListWnd::OnUpdateViewPrev(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetTDCCount() > 1);
}

void CToDoListWnd::OnSysCommand(UINT nID, LPARAM lParam) 
{
	switch (nID)
	{
	case SC_MINIMIZE:
		// we don't minimize if we're going to be hiding to then system tray
		{
			m_hwndLastFocus = ::GetFocus();

			int nSysTrayOption = Prefs().GetSysTrayOption();
			
			if (nSysTrayOption == STO_ONMINIMIZE || nSysTrayOption == STO_ONMINCLOSE)
				MinimizeToTray();
			else
			{
				// SPECIAL FIX: Apparently when Ultramon hooks the minimize
				// button it ends up sending us a close message!
				ShowWindow(SW_MINIMIZE);
			}
			return; // eat it
		}

	case SC_HOTKEY:
		Show(FALSE);
		return;
		
	case SC_CLOSE:
		// don't allow closing whilst reloading tasklists
		if (m_bReloading)
			return;
		break;

	case SC_RESTORE:
	case SC_MAXIMIZE:
		PostMessage(WM_APPRESTOREFOCUS, 0L, (LPARAM)m_hwndLastFocus);
		break;
	}

	CFrameWnd::OnSysCommand(nID, lParam);
}


void CToDoListWnd::OnUpdateImport(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!GetToDoCtrl().IsReadOnly());
}

UINT CToDoListWnd::MapNewTaskPos(int nPos, BOOL bSubtask)
{
	if (!bSubtask) // task
	{
		switch (nPos)
		{
		case PUIP_TOP:		return ID_NEWTASK_ATTOP;
		case PUIP_BOTTOM:	return ID_NEWTASK_ATBOTTOM;
		case PUIP_BELOW:	return ID_NEWTASK_AFTERSELECTEDTASK;
			
		case PUIP_ABOVE: 
		default:			return ID_NEWTASK_BEFORESELECTEDTASK;
		}
	}
	else // subtask
	{
		if (nPos == PUIP_BOTTOM)
			return ID_NEWSUBTASK_ATBOTTOM;
		else
			return ID_NEWSUBTASK_ATTOP;
	}
}

UINT CToDoListWnd::GetNewTaskCmdID()
{
	return MapNewTaskPos(Prefs().GetNewTaskPos(), FALSE);
}

UINT CToDoListWnd::GetNewSubtaskCmdID()
{
	return MapNewTaskPos(Prefs().GetNewSubtaskPos(), TRUE);
}

void CToDoListWnd::OnNewtask() 
{
	// convert to users choice
	SendMessage(WM_COMMAND, GetNewTaskCmdID());
}

void CToDoListWnd::OnUpdateNewtask(CCmdUI* pCmdUI) 
{
	switch (GetNewTaskCmdID())
	{
	case ID_NEWTASK_ATTOPSELECTED:
		OnUpdateNewtaskAttopSelected(pCmdUI);
		break;

	case ID_NEWTASK_ATBOTTOMSELECTED:	
		OnUpdateNewtaskAtbottomSelected(pCmdUI);
		break;
	
	case ID_NEWTASK_AFTERSELECTEDTASK:
		OnUpdateNewtaskAfterselectedtask(pCmdUI);
		break;

	case ID_NEWTASK_BEFORESELECTEDTASK:
		OnUpdateNewtaskBeforeselectedtask(pCmdUI);
		break;

	case ID_NEWTASK_ATTOP:
		OnUpdateNewtaskAttop(pCmdUI);
		break;

	case ID_NEWTASK_ATBOTTOM:
		OnUpdateNewtaskAtbottom(pCmdUI);
		break;
	}
}

void CToDoListWnd::OnNewsubtask() 
{
	// convert to users choice
	SendMessage(WM_COMMAND, GetNewSubtaskCmdID());
}

void CToDoListWnd::OnUpdateNewsubtask(CCmdUI* pCmdUI) 
{
	switch (GetNewSubtaskCmdID())
	{
	case ID_NEWSUBTASK_ATTOP:
		OnUpdateNewsubtaskAttop(pCmdUI);
		break;

	case ID_NEWSUBTASK_ATBOTTOM:
		OnUpdateNewsubtaskAtBottom(pCmdUI);
		break;
	}
}

void CToDoListWnd::OnToolsCheckout() 
{
	int nSel = GetSelToDoCtrl();

	// sanity check
	if (!m_mgrToDoCtrls.CanCheckOut(nSel))
		return;
	
	CAutoFlag af(m_bSaving, TRUE);

	CString sCheckedOutTo;
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nSel);
	
	if (tdc.CheckOut(sCheckedOutTo))
	{
		m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nSel);
		m_mgrToDoCtrls.SetLastCheckoutStatus(nSel, TRUE);
		m_mgrToDoCtrls.RefreshLastModified(nSel);
		
		UpdateCaption();

		// update menu icon mgr
		m_mgrMenuIcons.ChangeImageID(ID_TOOLS_CHECKOUT, ID_TOOLS_CHECKIN);
	}
	else // failed
	{
		m_mgrToDoCtrls.SetLastCheckoutStatus(nSel, FALSE);
		
		// notify user
		CEnString sMessage;
		
		if (!sCheckedOutTo.IsEmpty())
		{
			sMessage.Format(IDS_CHECKEDOUTBYOTHER, tdc.GetFilePath(), sCheckedOutTo);
		}
		else
		{
			// if sCheckedOutTo is empty then the error is most likely because
			// the file has been deleted or cannot be opened for editing
			sMessage.Format(IDS_CHECKOUTFAILED, tdc.GetFilePath());
		}
		
		MessageBox(sMessage, IDS_CHECKOUT_TITLE, MB_OK | MB_ICONEXCLAMATION);
	}
}

void CToDoListWnd::OnUpdateToolsCheckout(CCmdUI* pCmdUI) 
{
	int nSel = GetSelToDoCtrl();
	
	pCmdUI->Enable(m_mgrToDoCtrls.CanCheckOut(nSel));
}

void CToDoListWnd::OnToolsToggleCheckin() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (tdc.IsCheckedOut())
		OnToolsCheckin();
	else
		OnToolsCheckout();
}

void CToDoListWnd::OnUpdateToolsToggleCheckin(CCmdUI* pCmdUI) 
{
	int nSel = GetSelToDoCtrl();
	BOOL bEnable = m_mgrToDoCtrls.CanCheckInOut(nSel);
	
	pCmdUI->Enable(bEnable);
	pCmdUI->SetCheck(bEnable && GetToDoCtrl().IsCheckedOut() ? 1 : 0);
}

void CToDoListWnd::OnToolsCheckin() 
{
	int nSel = GetSelToDoCtrl();

	// sanity check
	if (!m_mgrToDoCtrls.CanCheckIn(nSel))
		return;
	
	CAutoFlag af(m_bSaving, TRUE);
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nSel);

	ASSERT (!tdc.GetFilePath().IsEmpty());
	
	tdc.Flush();
	
	// save modifications
	TDC_FILE nSave = TDCO_SUCCESS;

	if (tdc.IsModified())
	{
		if (Prefs().GetConfirmSaveOnExit())
		{
			CString sName = m_mgrToDoCtrls.GetFriendlyProjectName(nSel);
			CEnString sMessage(IDS_SAVEBEFORECHECKIN, sName);
			
			int nRet = MessageBox(sMessage, IDS_CHECKIN_TITLE, MB_YESNOCANCEL);
			
			switch (nRet)
			{
			case IDYES:
				nSave = SaveTaskList(nSel);
				break;
				
			case IDNO:
				ReloadTaskList(nSel, FALSE, FALSE);
				break;
				
			case IDCANCEL:
				return;
			}
		}
		else
			SaveTaskList(nSel); 
	}
	
	if (nSave == TDCO_SUCCESS && tdc.CheckIn())	
	{
		m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nSel);
		m_mgrToDoCtrls.RefreshLastModified(nSel);
		UpdateCaption();

		// update menu icon mgr
		m_mgrMenuIcons.ChangeImageID(ID_TOOLS_CHECKIN, ID_TOOLS_CHECKOUT);
	}

	m_mgrToDoCtrls.SetLastCheckoutStatus(nSel, TRUE); // so we won't try to immediately check it out again
}

void CToDoListWnd::OnUpdateToolsCheckin(CCmdUI* pCmdUI) 
{
	int nSel = GetSelToDoCtrl();
	
	pCmdUI->Enable(m_mgrToDoCtrls.CanCheckIn(nSel));
}

void CToDoListWnd::OnExport() 
{
	const CPreferencesDlg& userPrefs = Prefs();
	
	int nTDCCount = GetTDCCount(), nSelTDC = GetSelToDoCtrl();
	ASSERT (nTDCCount >= 1);

	CTDLExportDlg dialog(m_mgrImportExport, 
						nTDCCount == 1, 
						GetToDoCtrl().GetView(),
						userPrefs.GetExportVisibleColsOnly(), 
						m_mgrToDoCtrls.GetFilePath(nSelTDC, FALSE), 
						userPrefs.GetAutoExportFolderPath());
	
	// keep showing the dialog until either the user
	// selects a filename which does not match a tasklist
	// or they confirm that they want to overwrite the tasklist
	CString sExportPath;
	BOOL bOverWrite = FALSE;
	int nFormat = -1;

	while (!bOverWrite)
	{
		if (dialog.DoModal() != IDOK)
			return;

		sExportPath = dialog.GetExportPath();
		nFormat = dialog.GetExportFormat();

		// interested in overwriting single files
		if (nTDCCount == 1 || !dialog.GetExportAllTasklists() || dialog.GetExportOneFile())
		{
			// check with user if they are about to override a tasklist
			if (m_mgrToDoCtrls.FindToDoCtrl(sExportPath) != -1)
			{
				UINT nRet = MessageBox(IDS_CONFIRM_EXPORT_OVERWRITE, 0, MB_YESNOCANCEL, sExportPath);

				if (nRet == IDCANCEL)
					return;

				// else
				bOverWrite = (nRet == IDYES);
			}
			else
				bOverWrite = TRUE; // nothing to fear
		}
		else // check all open tasklists
		{
			CString sFilePath, sExt = m_mgrImportExport.GetExporterFileExtension(nFormat);
		
			for (int nCtrl = 0; nCtrl < nTDCCount; nCtrl++)
			{
				CString sPath = m_mgrToDoCtrls.GetFilePath(nCtrl);
				CString sFName;
						
				FileMisc::SplitPath(sPath, NULL, NULL, &sFName);
				FileMisc::MakePath(sFilePath, NULL, sExportPath, sFName, sExt);

				if (m_mgrToDoCtrls.FindToDoCtrl(sFilePath) != -1)
				{
					UINT nRet = MessageBox(IDS_CONFIRM_EXPORT_OVERWRITE, 0, MB_YESNOCANCEL, sFilePath);

					if (nRet == IDCANCEL)
						return;

					// else
					bOverWrite = (nRet == IDYES);
					break;
				}
			}

			// no matches?
			bOverWrite = TRUE; // nothing to fear
		}
	}

	UpdateWindow();

	// export
	DOPROGRESS(IDS_EXPORTPROGRESS)
	
	BOOL bHtmlComments = (nFormat == EXPTOHTML);

	if (nTDCCount == 1 || !dialog.GetExportAllTasklists())
	{
		// set the html image folder to be the output path with
		// an different extension
		CString sImgFolder;
		
		if (bHtmlComments)
		{
			sImgFolder = sExportPath;
			FileMisc::ReplaceExtension(sImgFolder, _T("html_images"));
			FileMisc::DeleteFolderContents(sImgFolder, TRUE);
		}
		
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nSelTDC);
		CTaskFile tasks;

		// Note: don't need to verify password if encrypted tasklist is active
		GetTasks(tdc, bHtmlComments, FALSE, dialog.GetTaskSelection(), tasks, sImgFolder);

		// add project name as report title
		CString sTitle = m_mgrToDoCtrls.GetFriendlyProjectName(nSelTDC);
		tasks.SetReportAttributes(sTitle);
		
		// save intermediate tasklist to file as required
		LogIntermediateTaskList(tasks, tdc.GetFilePath());

		if (m_mgrImportExport.ExportTaskList(&tasks, sExportPath, nFormat, FALSE))
		{
			// and preview
			if (userPrefs.GetPreviewExport())
				FileMisc::Run(*this, sExportPath, NULL, SW_SHOWNORMAL);
		}
	} 
	// multiple tasklists
	else if (dialog.GetExportOneFile())
	{
		CMultiTaskFile taskFiles;
		
		for (int nCtrl = 0; nCtrl < nTDCCount; nCtrl++)
		{
			// verify password
			if (nCtrl != nSelTDC && !VerifyToDoCtrlPassword(nCtrl))
				continue;
			
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
			tdc.LockWindowUpdate();
			
			// make sure it's loaded
			if (VerifyTaskListOpen(nCtrl, FALSE))
			{
				CTaskFile& tasks = taskFiles.GetTaskFile(nCtrl);
				tasks.Reset();
				
				// set the html image folder to be the output path with
				// an different extension
				CString sImgFolder;
				
				if (bHtmlComments)
				{
					sImgFolder = sExportPath;
					FileMisc::ReplaceExtension(sImgFolder, _T("html_images"));
				}
				
				GetTasks(tdc, bHtmlComments, FALSE, dialog.GetTaskSelection(), tasks, sImgFolder);
				
				// add project name as report title
				CString sTitle = m_mgrToDoCtrls.GetFriendlyProjectName(nCtrl);
				tasks.SetReportAttributes(sTitle);

				// save intermediate tasklist to file as required
				LogIntermediateTaskList(tasks, tdc.GetFilePath());
			}
			
			tdc.UnlockWindowUpdate();
		}
		
		Resize();
		
		if (m_mgrImportExport.ExportTaskLists(&taskFiles, sExportPath, nFormat, FALSE))
		{
			// and preview
			if (userPrefs.GetPreviewExport())
				FileMisc::Run(*this, sExportPath, NULL, SW_SHOWNORMAL);
		}
	}
	else // separate files
	{
		CString sExt = m_mgrImportExport.GetExporterFileExtension(nFormat);
		
		for (int nCtrl = 0; nCtrl < nTDCCount; nCtrl++)
		{
			// verify password
			if (nCtrl != nSelTDC && !VerifyToDoCtrlPassword(nCtrl))
				continue;
			
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
			tdc.LockWindowUpdate();
			
			// make sure it's loaded
			if (VerifyTaskListOpen(nCtrl, FALSE))
			{
				// build filepath if required (if exporter has an extension)
				CString sFilePath;
				BOOL bOverWrite = -1;
				
				if (!sExt.IsEmpty())
				{
					CString sPath = m_mgrToDoCtrls.GetFilePath(nCtrl);
					
					// if the file has not been saved before we use the tab text
					// and prompt the user to confirm
					if (sPath.IsEmpty())
					{
						sPath = m_mgrToDoCtrls.GetFilePath(nCtrl, FALSE);
						FileMisc::MakePath(sFilePath, NULL, sExportPath, sPath, sExt);
						
						CFileSaveDialog dlgFile(IDS_EXPORTFILEAS_TITLE,
												sExt, 
												sFilePath, 
												EOFN_DEFAULTSAVE,
												m_mgrImportExport.GetExporterFileFilter(nFormat));
						
						CPreferences prefs;

						if (dlgFile.DoModal(&prefs) == IDOK)
							sFilePath = dlgFile.GetPathName();
						else
							continue; // next tasklist
					}
					else
					{
						CString sFName;

						FileMisc::SplitPath(sPath, NULL, NULL, &sFName);
						FileMisc::MakePath(sFilePath, NULL, sExportPath, sFName, sExt);
					}
				}
				
				// set the html image folder to be the output path with
				// an different extension
				CString sImgFolder;
				
				if (bHtmlComments)
				{
					sImgFolder = sFilePath;
					FileMisc::ReplaceExtension(sImgFolder, _T("html_images"));
				}
				
				CTaskFile tasks;
				GetTasks(tdc, bHtmlComments, FALSE, dialog.GetTaskSelection(), tasks, sImgFolder);
				
				// add project name as report title
				CString sTitle = m_mgrToDoCtrls.GetFriendlyProjectName(nCtrl);
				tasks.SetReportAttributes(sTitle);

				// save intermediate tasklist to file as required
				LogIntermediateTaskList(tasks, tdc.GetFilePath());

				m_mgrImportExport.ExportTaskList(&tasks, sFilePath, nFormat, FALSE);
			}
			
			tdc.UnlockWindowUpdate();
		}
	}
}

int CToDoListWnd::GetTasks(CFilteredToDoCtrl& tdc, BOOL bHtmlComments, BOOL bTransform, 
						  TSD_TASKS nWhatTasks, TDCGETTASKS& filter, DWORD dwSelFlags, 
						  CTaskFile& tasks, LPCTSTR szHtmlImageDir) const
{
	// preferences
	const CPreferencesDlg& userPrefs = Prefs();
	
	// project name
	tasks.SetProjectName(tdc.GetFriendlyProjectName());
	tasks.SetCharSet(userPrefs.GetHtmlCharSet());
	
	// export flags
	filter.dwFlags |= TDCGTF_FILENAME;

	if (userPrefs.GetExportParentTitleCommentsOnly())
		filter.dwFlags |= TDCGTF_PARENTTITLECOMMENTSONLY;

	if (bHtmlComments)
	{
		filter.dwFlags |= TDCGTF_HTMLCOMMENTS;
		tasks.SetHtmlImageFolder(szHtmlImageDir);

		// And delete all existing images in that folder
		if (szHtmlImageDir && *szHtmlImageDir)
			FileMisc::DeleteFolderContents(szHtmlImageDir, TRUE);

		if (bTransform)
		{
			ASSERT(bHtmlComments);
			filter.dwFlags |= TDCGTF_TRANSFORM;
		}
	}

	// get the tasks
	tdc.Flush();
	
	switch (nWhatTasks)
	{
	case TSDT_ALL:
		{
			// if there's a filter present then we toggle it off 
			// grab the tasks and then toggle it back on
			BOOL bNeedToggle = (tdc.HasFilter() || tdc.HasCustomFilter());

			if (bNeedToggle)
			{
				::LockWindowUpdate(tdc.GetSafeHwnd());
				tdc.ToggleFilter();
			}

			tdc.GetTasks(tasks, filter);

			if (bNeedToggle)
			{
				tdc.ToggleFilter();
				::LockWindowUpdate(NULL);
			}
		}
		break;

	case TSDT_FILTERED:
		// if no filter is present then this just gets the whole lot
		tdc.GetFilteredTasks(tasks, filter);
		break;

	case TSDT_SELECTED:
		tdc.GetSelectedTasks(tasks, filter, dwSelFlags);
		break;
	}
	
	// delete the HTML image folder if it is empty
	// this will fail if it is not empty.
	if (bHtmlComments && szHtmlImageDir && *szHtmlImageDir)
		RemoveDirectory(szHtmlImageDir);
	
	return tasks.GetTaskCount();
}

int CToDoListWnd::GetTasks(CFilteredToDoCtrl& tdc, BOOL bHtmlComments, BOOL bTransform, 
							const CTaskSelectionDlg& taskSel, CTaskFile& tasks, LPCTSTR szHtmlImageDir) const
{
	DWORD dwSelFlags = 0;
    TSD_TASKS nWhatTasks = taskSel.GetWantWhatTasks();
	
	if (taskSel.GetWantSelectedTasks())
	{
		if (!taskSel.GetWantSelectedSubtasks()) 
			dwSelFlags |= TDCGSTF_NOTSUBTASKS;

		if (taskSel.GetWantSelectedParentTask())
			dwSelFlags |= TDCGSTF_IMMEDIATEPARENT;
	}

	TDC_GETTASKS nFilter = TDCGT_ALL;
	
	// build filter
	if (taskSel.GetWantCompletedTasks() && !taskSel.GetWantInCompleteTasks())
		nFilter = TDCGT_DONE;
		
	else if (!taskSel.GetWantCompletedTasks() && taskSel.GetWantInCompleteTasks())
		nFilter = TDCGT_NOTDONE;
		
	TDCGETTASKS filter(nFilter);

	// attributes to export
	switch (taskSel.GetAttributeOption())
	{
	case TSDA_ALL:
		break;

	case TSDA_VISIBLE:
		{
			CTDCColumnIDArray aCols;
			tdc.GetVisibleColumns(aCols);

			MapColumnsToAttributes(aCols, filter.aAttribs);

			if (taskSel.GetWantCommentsWithVisible())
				filter.aAttribs.Add(TDCA_COMMENTS);

			filter.aAttribs.Add(TDCA_CUSTOMATTRIB); // always
		}
		break;

	case TSDA_USER:
		taskSel.GetUserAttributes(filter.aAttribs);
		filter.dwFlags |= TDCGTF_USERCOLUMNS;
		break;
	}
	
	// get the tasks
   return GetTasks(tdc, bHtmlComments, bTransform, nWhatTasks, filter, dwSelFlags, tasks, szHtmlImageDir);
}

void CToDoListWnd::OnUpdateExport(CCmdUI* pCmdUI) 
{
	// make sure at least one control has items
	int nCtrl = GetTDCCount();
	
	while (nCtrl--)
	{
		if (GetToDoCtrl().GetTaskCount())
		{
			pCmdUI->Enable(TRUE);
			return;
		}
	}
	
	// else
	pCmdUI->Enable(FALSE);
}

void CToDoListWnd::OnToolsTransformactivetasklist() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	// pass the project name as the title field
	CString sTitle = tdc.GetProjectName();
	CTDLTransformDialog dialog(sTitle, tdc.GetView());
	
	if (dialog.DoModal() != IDOK)
		return;
	
	CString sStylesheet = dialog.GetStylesheet();
	sTitle = dialog.GetTitle();
	
	// output path
	CString sOutputPath(tdc.GetFilePath()); 
	{
		if (!sOutputPath.IsEmpty())
			FileMisc::ReplaceExtension(sOutputPath, _T("html"));

		CPreferences prefs;
		CFileSaveDialog dialog(IDS_SAVETRANSFORM_TITLE,
								_T("html"), 
								sOutputPath, 
								OFN_OVERWRITEPROMPT, 
								CEnString(IDS_TRANSFORMFILEFILTER), 
								this);
		
		if (dialog.DoModal(&prefs) != IDOK)
			return; // user elected not to proceed
		
		sOutputPath = dialog.GetPathName();
	}
	
	// export
	DOPROGRESS(IDS_TRANSFORMPROGRESS)

	// set the html image folder to be the same as the 
	// output path without the extension
	CString sHtmlImgFolder(sOutputPath);
	FileMisc::ReplaceExtension(sHtmlImgFolder, _T("html_images"));
	
	CTaskFile tasks;
	GetTasks(tdc, TRUE, TRUE, dialog.GetTaskSelection(), tasks, sHtmlImgFolder);

	// add title and date 
	COleDateTime date;

	if (dialog.GetWantDate())
		date = COleDateTime::GetCurrentTime();

	tasks.SetReportAttributes(sTitle, date);
	
	// save intermediate tasklist to file as required
	LogIntermediateTaskList(tasks, tdc.GetFilePath());
	
	if (tasks.TransformToFile(sStylesheet, sOutputPath, Prefs().GetHtmlCharSet()))
	{
		// preview
		if (Prefs().GetPreviewExport())
			FileMisc::Run(*this, sOutputPath, NULL, SW_SHOWNORMAL);
	}
}

BOOL CToDoListWnd::LogIntermediateTaskList(CTaskFile& tasks, LPCTSTR szRefPath)
{
	if (FileMisc::IsLoggingEnabled())
	{
		CString sRefName = FileMisc::RemoveExtension(FileMisc::GetFileNameFromPath(szRefPath));
		CString sTempTaskPath = FileMisc::GetTempFileName(sRefName, _T("intermediate.txt")); 

		return tasks.Save(sTempTaskPath);
	}

	// else
	return TRUE;
}

void CToDoListWnd::OnNexttopleveltask() 
{
	GetToDoCtrl().GotoNextTopLevelTask(TDCG_NEXT);
}

void CToDoListWnd::OnPrevtopleveltask() 
{
	GetToDoCtrl().GotoNextTopLevelTask(TDCG_PREV);
}

void CToDoListWnd::OnUpdateNexttopleveltask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanGotoNextTopLevelTask(TDCG_NEXT));
}

void CToDoListWnd::OnUpdatePrevtopleveltask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanGotoNextTopLevelTask(TDCG_PREV));
}

void CToDoListWnd::OnGotoNexttask() 
{
	GetToDoCtrl().GotoNextTask(TDCG_NEXT);
}

void CToDoListWnd::OnGotoPrevtask() 
{
	GetToDoCtrl().GotoNextTask(TDCG_PREV);
}

void CToDoListWnd::OnUpdateGotoPrevtask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanGotoNextTask(TDCG_PREV));
}

void CToDoListWnd::OnUpdateGotoNexttask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanGotoNextTask(TDCG_NEXT));
}
//------------------------------------------------------------------------

BOOL CToDoListWnd::InitFindDialog(BOOL bShow)
{
	if (!m_findDlg.GetSafeHwnd())
	{
		UpdateFindDialogCustomAttributes();

		VERIFY(m_findDlg.Initialize(this));

		if (CThemed::IsThemeActive())
			m_findDlg.SetUITheme(m_theme);

		if (bShow)
			m_findDlg.Show(bShow);
	}

	return (m_findDlg.GetSafeHwnd() != NULL);
}

void CToDoListWnd::OnFindTasks() 
{
	InitFindDialog();

	if (IsWindowVisible())
	{
		// remove results from encrypted tasklists but not from the 
		// active tasklist
		if (!m_findDlg.IsWindowVisible())
		{
			int nSelTDC = GetSelToDoCtrl();
			int nTDC = GetTDCCount();

			while (nTDC--)
			{
				const CFilteredToDoCtrl& tdc = GetToDoCtrl(nTDC);

				if (nTDC != nSelTDC && tdc.IsEncrypted())
					m_findDlg.DeleteResults(&tdc);
			}
		}
		m_findDlg.Show();
	}
	
	m_bFindShowing = TRUE;
}

LRESULT CToDoListWnd::OnFindDlgClose(WPARAM /*wp*/, LPARAM /*lp*/)
{
	m_bFindShowing = FALSE;
	GetToDoCtrl().SetFocusToTasks();

	return 0L;
}

LRESULT CToDoListWnd::OnFindDlgFind(WPARAM /*wp*/, LPARAM /*lp*/)
{
	// set up the search
	BOOL bSearchAll = m_findDlg.GetSearchAllTasklists();
	int nSelTaskList = GetSelToDoCtrl();
	
	int nFrom = bSearchAll ? 0 : nSelTaskList;
	int nTo = bSearchAll ? GetTDCCount() - 1 : nSelTaskList;
	
	// search params
	SEARCHPARAMS params;

	if (m_findDlg.GetSearchParams(params))
	{
		int nSel = GetSelToDoCtrl();
		int bFirst = TRUE;
		
		for (int nCtrl = nFrom; nCtrl <= nTo; nCtrl++)
		{
			// load or verify password unless tasklist is already active
			if (nCtrl != nSel)
			{
				// load if necessary (in which case the password will have been checked)
				if (!m_mgrToDoCtrls.IsLoaded(nCtrl))
				{
					if (!VerifyTaskListOpen(nCtrl, FALSE))
						continue;
				}
				else if (!VerifyToDoCtrlPassword(nCtrl))
					continue;
			}
			
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
			CResultArray aResults;
			CHoldRedraw hr(m_bFindShowing ? m_findDlg : NULL);
			
			if (tdc.FindTasks(params, aResults))
			{
				// use tasklist title from tabctrl
				CString sTitle = m_mgrToDoCtrls.GetTabItemText(nCtrl);
				
				m_findDlg.AddHeaderRow(sTitle, !bFirst);
				
				for (int nResult = 0; nResult < aResults.GetSize(); nResult++)
					AddFindResult(aResults[nResult], &tdc);
				
				bFirst = FALSE;
			}
		}
	}	
	
	// auto-select single results
/*	if (m_findDlg.GetResultCount() == 1)
	{
		CFTDResultsArray results;

		m_findDlg.GetAllResults(results);
		ASSERT (results.GetSize() == 1);
		
		if (OnFindSelectResult(0, (LPARAM)&results[0]))
			m_findDlg.Show(FALSE);	
	}
	else*/
		m_findDlg.SetActiveWindow();
	
	return 0;
}

void CToDoListWnd::AddFindResult(const SEARCHRESULT& result, const CFilteredToDoCtrl* pTDC)
{
	CString sTitle = pTDC->GetTaskTitle(result.dwTaskID);
	//CString sPath = pTDC->GetTaskPath(result.dwID);
	
	m_findDlg.AddResult(result, sTitle, /*sPath,*/ pTDC);
}

LRESULT CToDoListWnd::OnFindSelectResult(WPARAM /*wp*/, LPARAM lp)
{
	// extract Task ID
	FTDRESULT* pResult = (FTDRESULT*)lp;
	ASSERT (pResult->dwTaskID); 
	
	int nCtrl = m_mgrToDoCtrls.FindToDoCtrl(pResult->pTDC);
	ASSERT(nCtrl != -1);
	
	if (m_tabCtrl.GetCurSel() != nCtrl)
	{
		if (!SelectToDoCtrl(nCtrl, TRUE))
			return 0L;
	}
	
	// we can't use pResult->pTDC because it's const
	CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);
	tdc.SetFocusToTasks();
	
	if (tdc.GetSelectedTaskID() != pResult->dwTaskID)
	{
		if (!tdc.SelectTask(pResult->dwTaskID))
		{
			// perhaps the task is filtered out so we toggle the filter
			// and try again
			if (tdc.HasFilter())
			{
				tdc.ToggleFilter();

				// if that also fails, we restore the filter
				if (!tdc.SelectTask(pResult->dwTaskID))
					tdc.ToggleFilter();
			}
		}

		Invalidate();
		UpdateWindow();
	}
	
	return 1L;
}

LRESULT CToDoListWnd::OnFindSelectAll(WPARAM /*wp*/, LPARAM /*lp*/)
{
	if (!m_findDlg.GetResultCount())
		return 0;
	
	CWaitCursor cursor;
	
	for (int nTDC = 0; nTDC < GetTDCCount(); nTDC++)
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl(nTDC);
		tdc.DeselectAll();
		
		// collate the taskIDs
		CDWordArray aTaskIDs;
		m_findDlg.GetResultIDs(&tdc, aTaskIDs);

		// select them in one hit
		if (aTaskIDs.GetSize())
			tdc.MultiSelectItems(aTaskIDs, TSHS_SELECT, (nTDC == GetSelToDoCtrl()));
	}

	// if find dialog is floating then hide it
	if (!m_findDlg.IsDocked())
		m_findDlg.Show(FALSE);
	
	return 0;
}

LRESULT CToDoListWnd::OnFindApplyAsFilter(WPARAM /*wp*/, LPARAM lp)
{
	CString sCustom((LPCTSTR)lp);
	SEARCHPARAMS filter;
	m_findDlg.GetSearchParams(filter);

	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.SetCustomFilter(filter, sCustom);
	tdc.SetFocusToTasks();
	
	RefreshFilterBarCustomFilters();
	m_filterBar.RefreshFilterControls(tdc);

	// if find dialog is floating then hide it
	if (!m_findDlg.IsDocked())
		m_findDlg.Show(FALSE);
	
	return 0;
}

LRESULT CToDoListWnd::OnFindAddSearch(WPARAM /*wp*/, LPARAM /*lp*/)
{
	RefreshFilterBarCustomFilters();
	return 0;
}

LRESULT CToDoListWnd::OnFindDeleteSearch(WPARAM /*wp*/, LPARAM /*lp*/)
{
	RefreshFilterBarCustomFilters();
	return 0;
}

void CToDoListWnd::RefreshFilterBarCustomFilters()
{
	CStringArray aFilters;
	
	m_findDlg.GetSavedSearches(aFilters);

	// check for unnamed filter
	if (m_findDlg.GetSafeHwnd())
	{
		CEnString sUnNamed(IDS_UNNAMEDFILTER);

		if (m_findDlg.GetActiveSearch().IsEmpty() && Misc::Find(aFilters, sUnNamed, FALSE, FALSE) == -1)
			aFilters.Add(sUnNamed);
	}

	m_filterBar.AddCustomFilters(aFilters);
}

//------------------------------------------------------------------------

LRESULT CToDoListWnd::OnDropFile(WPARAM wParam, LPARAM lParam)
{
	TLDT_DATA* pData = (TLDT_DATA*)wParam;
	CWnd* pTarget = (CWnd*)lParam;

	if (pTarget == this) // we're the target
	{
		CString sFile = pData->pFilePaths ? pData->pFilePaths->GetAt(0) : _T("");

		if (FileMisc::HasExtension(sFile, _T("tdl")) || FileMisc::HasExtension(sFile, _T("xml"))) // tasklist
		{
			TDC_FILE nRes = OpenTaskList(sFile);
			HandleLoadTasklistError(nRes, sFile);
		}
	}

	return 0L;
}

void CToDoListWnd::OnViewMovetasklistright() 
{
	m_mgrToDoCtrls.MoveToDoCtrl(GetSelToDoCtrl(), 1);
}

void CToDoListWnd::OnUpdateViewMovetasklistright(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!Prefs().GetKeepTabsOrdered() &&
					m_mgrToDoCtrls.CanMoveToDoCtrl(GetSelToDoCtrl(), 1));
}

void CToDoListWnd::OnViewMovetasklistleft() 
{
	m_mgrToDoCtrls.MoveToDoCtrl(GetSelToDoCtrl(), -1);
}

void CToDoListWnd::OnUpdateViewMovetasklistleft(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!Prefs().GetKeepTabsOrdered() &&
					m_mgrToDoCtrls.CanMoveToDoCtrl(GetSelToDoCtrl(), -1));
}

void CToDoListWnd::OnToolsShowtasksDue(UINT nCmdID) 
{
	int nDueBy = PFP_DUETODAY;
	UINT nIDDueBy = IDS_NODUETODAY;
	
	switch (nCmdID)
	{
	case ID_TOOLS_SHOWTASKS_DUETODAY:
		break; // done
		
	case ID_TOOLS_SHOWTASKS_DUETOMORROW:
		nIDDueBy = IDS_NODUETOMORROW;
		nDueBy = PFP_DUETOMORROW;
		break;
		
	case ID_TOOLS_SHOWTASKS_DUEENDTHISWEEK:
		nIDDueBy = IDS_NODUETHISWEEK;
		nDueBy = PFP_DUETHISWEEK;
		break;
		
	case ID_TOOLS_SHOWTASKS_DUEENDNEXTWEEK:
		nIDDueBy = IDS_NODUENEXTWEEK;
		nDueBy = PFP_DUENEXTWEEK;
		break;
		
	case ID_TOOLS_SHOWTASKS_DUEENDTHISMONTH:
		nIDDueBy = IDS_NODUETHISMONTH;
		nDueBy = PFP_DUETHISMONTH;
		break;
		
	case ID_TOOLS_SHOWTASKS_DUEENDNEXTMONTH:
		nIDDueBy = IDS_NODUENEXTMONTH;
		nDueBy = PFP_DUENEXTMONTH;
		break;
		
	default:
		ASSERT(0);
		return;
	}
	
	if (!DoDueTaskNotification(nDueBy))
	{
		MessageBox(nIDDueBy, 0, MB_OK, m_mgrToDoCtrls.GetFriendlyProjectName(GetSelToDoCtrl()));
	}
}

void CToDoListWnd::ResetPrefs()
{
	delete m_pPrefs;
	m_pPrefs = new CPreferencesDlg(&m_mgrShortcuts, IDR_MAINFRAME, &m_mgrContent, &m_mgrImportExport);
	
	// update
	m_mgrToDoCtrls.SetPrefs(m_pPrefs); 
	
	// grab current colors
	Prefs().GetPriorityColors(m_aPriorityColors);

	m_filterBar.SetPriorityColors(m_aPriorityColors);
}

const CPreferencesDlg& CToDoListWnd::Prefs() const
{
	ASSERT (m_pPrefs);
	return *m_pPrefs;
}

void CToDoListWnd::OnSpellcheckcomments() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.SpellcheckSelectedTask(FALSE);
}

void CToDoListWnd::OnUpdateSpellcheckcomments(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.CanSpellcheckSelectedTaskComments());
}

void CToDoListWnd::OnSpellchecktitle() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.SpellcheckSelectedTask(TRUE);
}

void CToDoListWnd::OnUpdateSpellchecktitle(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(!tdc.GetSelectedTaskTitle().IsEmpty());
}

void CToDoListWnd::OnFileEncrypt() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	if (!tdc.IsReadOnly())
	{
		BOOL bWasEncrypted = tdc.IsEncrypted();
		CString sPassword = tdc.GetPassword();

		// if the tasklist is already encrypted then verify password
		// before allowing change
		if (!bWasEncrypted || VerifyToDoCtrlPassword())
			tdc.EnableEncryption(!tdc.IsEncrypted());

		// make sure we disable encryption on the archive too
		if (bWasEncrypted)
		{
			CString sArchive = m_mgrToDoCtrls.GetArchivePath(GetSelToDoCtrl());

			if (FileMisc::FileExists(sArchive))
			{
				CTaskFile archive(sPassword);

				if (archive.Load(sArchive))
				{
					archive.SetPassword(NULL); // remove password
					archive.Save(sArchive);
				}
			}
		}
	}

	UpdateAeroFeatures();
}

void CToDoListWnd::OnUpdateFileEncrypt(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(tdc.CanEncrypt() && !tdc.IsReadOnly());
	pCmdUI->SetCheck(tdc.IsEncrypted() ? 1 : 0);
}

void CToDoListWnd::OnFileResetversion() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (!tdc.IsReadOnly())
	{
		tdc.ResetFileVersion();
		tdc.SetModified();
		
		UpdateStatusbar();
		UpdateCaption();
	}
}

void CToDoListWnd::OnUpdateFileResetversion(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(!tdc.IsReadOnly());
}

void CToDoListWnd::OnSpellcheckTasklist() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.Spellcheck();
}

void CToDoListWnd::OnUpdateSpellcheckTasklist(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.GetTaskCount());
}

TDC_FILE CToDoListWnd::SaveAll(DWORD dwFlags)
{
	TDC_FILE nSaveAll = TDCO_SUCCESS;
	int nCtrl = GetTDCCount();

	BOOL bIncUnsaved = Misc::HasFlag(dwFlags, TDLS_INCLUDEUNSAVED);
	BOOL bClosingWindows = Misc::HasFlag(dwFlags, TDLS_CLOSINGWINDOWS);
	BOOL bClosingAll = Misc::HasFlag(dwFlags, TDLS_CLOSINGTASKLISTS);		

	// scoped to end status bar progress
	// before calling UpdateStatusbar
	{
		DOPROGRESS(IDS_SAVINGPROGRESS)

		while (nCtrl--)
		{
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nCtrl);

			// bypass unsaved tasklists unless closing Windows
			if (!bClosingWindows && !bIncUnsaved && tdc.GetFilePath().IsEmpty())
				continue;
			
			if (Misc::HasFlag(dwFlags, TDLS_FLUSH))
				tdc.Flush(bClosingAll);		

			TDC_FILE nSave = ConfirmSaveTaskList(nCtrl, dwFlags);

			if (nSave == TDCO_CANCELLED) // user cancelled
				return TDCO_CANCELLED;

			// else cache any failure w/o overwriting previous
			if (nSaveAll == TDCO_SUCCESS)
				nSaveAll = nSave;

			m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
		}
	}
	
	if (!bClosingWindows)
	{
		UpdateCaption();
		UpdateStatusbar();
	}
	
    return nSaveAll;
}

void CToDoListWnd::OnEditTimeTrackTask() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.TimeTrackSelectedTask();
}

void CToDoListWnd::OnUpdateEditTimeTrackTask(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(tdc.CanTimeTrackSelectedTask());
	pCmdUI->SetCheck(tdc.IsSelectedTaskBeingTimeTracked() ? 1 : 0);
}

void CToDoListWnd::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	if (nIDCtl == IDC_TABCONTROL)
	{
		TDCM_DUESTATUS nStatus = m_mgrToDoCtrls.GetDueItemStatus(lpDrawItemStruct->itemID);

		if (nStatus == TDCM_PAST || nStatus == TDCM_TODAY)
		{
			// determine appropriate due colour
			COLORREF crDue, crDueToday;

			GetToDoCtrl(lpDrawItemStruct->itemID).GetDueTaskColors(crDue, crDueToday);

			COLORREF crTag = (nStatus == TDCM_PAST) ? crDue : crDueToday;

			if (crTag != CLR_NONE)
			{
				CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);
				const CRect& rect = lpDrawItemStruct->rcItem;

				// draw a little tag in the top left corner
				for (int nHPos = 0; nHPos < 6; nHPos++)
				{
					for (int nVPos = 0; nVPos < 6 - nHPos; nVPos++)
					{
						pDC->SetPixelV(rect.left + nHPos, rect.top + nVPos, crTag);
					}
				}

				// draw a black line between the two
				pDC->SelectStockObject(BLACK_PEN);
				pDC->MoveTo(rect.left, rect.top + 6);
				pDC->LineTo(rect.left + 7, rect.top - 1);
			}
		}
		return;
	}
	else if (nIDCtl == 0 && lpDrawItemStruct->itemID == ID_CLOSE)
	{
		if (m_menubar.DrawMDIButton(lpDrawItemStruct))
			return;
	}

	CFrameWnd::OnDrawItem(nIDCtl, lpDrawItemStruct);
} 

void CToDoListWnd::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
	if (nIDCtl == 0 && lpMeasureItemStruct->itemID == ID_CLOSE)
	{
		if (m_menubar.MeasureMDIButton(lpMeasureItemStruct))
			return;
	}
	
	CFrameWnd::OnMeasureItem(nIDCtl, lpMeasureItemStruct);
}

void CToDoListWnd::OnViewNextSel() 
{
	GetToDoCtrl().SelectNextTasksInHistory();
}

void CToDoListWnd::OnUpdateViewNextSel(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanSelectNextTasksInHistory());
}

void CToDoListWnd::OnViewPrevSel() 
{
	GetToDoCtrl().SelectPrevTasksInHistory();
}

void CToDoListWnd::OnUpdateViewPrevSel(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanSelectPrevTasksInHistory());
}

void CToDoListWnd::OnSplitTaskIntoPieces(UINT nCmdID) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nNumPieces = 2 + (nCmdID - ID_NEWTASK_SPLITTASKINTO_TWO);
	
	tdc.SplitSelectedTask(nNumPieces);
}

void CToDoListWnd::OnUpdateSplitTaskIntoPieces(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(tdc.CanSplitSelectedTask());
}

void CToDoListWnd::OnViewExpandtask() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_SELECTED, TRUE);
}

void CToDoListWnd::OnUpdateViewExpandtask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_SELECTED, TRUE));
}

void CToDoListWnd::OnViewCollapsetask() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_SELECTED, FALSE);
}

void CToDoListWnd::OnUpdateViewCollapsetask(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_SELECTED, FALSE));
}

void CToDoListWnd::OnViewExpandall() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_ALL, TRUE);
}

void CToDoListWnd::OnUpdateViewExpandall(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_ALL, TRUE));
}

void CToDoListWnd::OnViewCollapseall() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_ALL, FALSE);
}

void CToDoListWnd::OnUpdateViewCollapseall(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_ALL, FALSE));
}

void CToDoListWnd::OnViewExpandDuetasks() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_DUE, TRUE);
}

void CToDoListWnd::OnUpdateViewExpandDuetasks(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_DUE, TRUE));
}

void CToDoListWnd::OnViewCollapseDuetasks() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_DUE, FALSE);
}

void CToDoListWnd::OnUpdateViewCollapseDuetasks(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_DUE, FALSE));
}

void CToDoListWnd::OnViewExpandStartedtasks() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_STARTED, TRUE);
}

void CToDoListWnd::OnUpdateViewExpandStartedtasks(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_STARTED, TRUE));
}

void CToDoListWnd::OnViewCollapseStartedtasks() 
{
	GetToDoCtrl().ExpandTasks(TDCEC_STARTED, FALSE);
}

void CToDoListWnd::OnUpdateViewCollapseStartedtasks(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanExpandTasks(TDCEC_STARTED, FALSE));
}

void CToDoListWnd::OnViewToggletaskexpanded() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	tdc.ExpandTasks(TDCEC_SELECTED, tdc.CanExpandTasks(TDCEC_SELECTED, TRUE));
}

void CToDoListWnd::OnUpdateViewToggletaskexpanded(CCmdUI* pCmdUI) 
{
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();

	pCmdUI->Enable(tdc.CanExpandTasks(TDCEC_SELECTED, TRUE) || tdc.CanExpandTasks(TDCEC_SELECTED, FALSE));
}

void CToDoListWnd::OnWindow(UINT nCmdID) 
{
	int nTDC = (int)(nCmdID - ID_WINDOW1);
	
	if (nTDC < GetTDCCount())
		SelectToDoCtrl(nTDC, (nTDC != GetSelToDoCtrl()), Prefs().GetNotifyDueByOnSwitch());
}

void CToDoListWnd::OnUpdateWindow(CCmdUI* pCmdUI) 
{
	if (pCmdUI->m_pMenu)
	{
		ASSERT (ID_WINDOW1 == pCmdUI->m_nID);
		const UINT MAXWINDOWS = 16;
		int nWnd;
		
		// delete existing tool entries first
		for (nWnd = 0; nWnd < MAXWINDOWS; nWnd++)
			pCmdUI->m_pMenu->DeleteMenu(ID_WINDOW1 + nWnd, MF_BYCOMMAND);
		
		int nSel = GetSelToDoCtrl();
		int nPos = 0, nTDCCount = GetTDCCount();
		ASSERT (nTDCCount);

		nTDCCount = min(nTDCCount, MAXWINDOWS);
		
		for (nWnd = 0; nWnd < nTDCCount; nWnd++)
		{
			CFilteredToDoCtrl& tdc = GetToDoCtrl(nWnd);
			
			CString sMenuItem;
			sMenuItem.Format(_T("&%d (%s)"), (nPos + 1) % 10, tdc.GetFriendlyProjectName());
			
			UINT nFlags = MF_BYPOSITION | MF_STRING | (nSel == nWnd ? MF_CHECKED : MF_UNCHECKED);
			pCmdUI->m_pMenu->InsertMenu(pCmdUI->m_nIndex++, nFlags, ID_WINDOW1 + nWnd, sMenuItem);
			
			nPos++;
		}
		
		// update end menu count
		pCmdUI->m_nIndex--; // point to last menu added
		pCmdUI->m_nIndexMax = pCmdUI->m_pMenu->GetMenuItemCount();
		
		pCmdUI->m_bEnableChanged = TRUE;    // all the added items are enabled
	}
}

#if _MSC_VER >= 1400
void CToDoListWnd::OnActivateApp(BOOL bActive, DWORD dwThreadID)
#else
void CToDoListWnd::OnActivateApp(BOOL bActive, HTASK hTask) 
#endif
{
	// don't activate when in the middle of loading
	if (m_bReloading && !bActive)
		return;

#if _MSC_VER >= 1400
	CFrameWnd::OnActivateApp(bActive, dwThreadID);
#else
	CFrameWnd::OnActivateApp(bActive, hTask);
#endif
	
	// don't do any further processing if closing
    if (m_bClosing)
        return; 

	if (!bActive)
	{
		// save focus to restore when we next get activated
		HWND hFocus = ::GetFocus();

		if (hFocus)
			m_hwndLastFocus = hFocus;

		// save tasklists if required
		if (Prefs().GetAutoSaveOnSwitchApp())
			SaveAll(TDLS_FLUSH | TDLS_AUTOSAVE);
	}
	else
	{
		if (GetTDCCount() && (!m_hwndLastFocus || Prefs().GetAutoFocusTasklist()))
			PostMessage(WM_APPRESTOREFOCUS, 0L, (LPARAM)GetToDoCtrl().GetSafeHwnd());
		
		else if (m_hwndLastFocus)
		{
			// delay the restoration of focus else it gets lost
			PostMessage(WM_APPRESTOREFOCUS, 0L, (LPARAM)m_hwndLastFocus);
		}

		UpdateCwd();
	}
}

LRESULT CToDoListWnd::OnAppRestoreFocus(WPARAM /*wp*/, LPARAM lp)
{
	HWND hWnd = (HWND)lp;

	if (GetTDCCount() && hWnd == GetToDoCtrl().GetSafeHwnd())
		GetToDoCtrl().SetFocusToTasks();
	else
		return (LRESULT)::SetFocus(hWnd);

	return 0L;
}

void CToDoListWnd::UpdateCwd()
{
	// set cwd to active tasklist
	if (GetTDCCount())
	{
		CString sFolder	= FileMisc::GetFolderFromFilePath(m_mgrToDoCtrls.GetFilePath(GetSelToDoCtrl()));

		if (FileMisc::FolderExists(sFolder))
			SetCurrentDirectory(sFolder);
	}
}

BOOL CToDoListWnd::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	UpdateWindow();

	return CFrameWnd::OnCommand(wParam, lParam);
}

void CToDoListWnd::OnEnable(BOOL bEnable) 
{
	CFrameWnd::OnEnable(bEnable);
	
	// save current focus because modal window is being shown
	if (!bEnable)
	{
		HWND hFocus = ::GetFocus();

		if (hFocus)
			m_hwndLastFocus = hFocus;
	}
	// then restore it when we are enabled
	else if (m_hwndLastFocus)
	{
		UpdateWindow();
		PostMessage(WM_APPRESTOREFOCUS, 0L, (LPARAM)m_hwndLastFocus);
	}
}

void CToDoListWnd::OnViewSorttasklisttabs() 
{
	int nSel = m_mgrToDoCtrls.SortToDoCtrlsByName();
	SelectToDoCtrl(nSel, FALSE);
}

void CToDoListWnd::OnUpdateViewSorttasklisttabs(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetTDCCount() > 1 && !Prefs().GetKeepTabsOrdered());
}

void CToDoListWnd::OnEditInctaskpercentdone() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.IncrementSelectedTaskPercentDone(TRUE);
}

void CToDoListWnd::OnUpdateEditInctaskpercentdone(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnEditDectaskpercentdone() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.IncrementSelectedTaskPercentDone(FALSE);
}

void CToDoListWnd::OnUpdateEditDectaskpercentdone(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnEditDectaskpriority() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.IncrementSelectedTaskPriority(FALSE);
}

void CToDoListWnd::OnUpdateEditDectaskpriority(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnEditInctaskpriority() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.IncrementSelectedTaskPriority(TRUE);
}

void CToDoListWnd::OnUpdateEditInctaskpriority(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnEditFlagtask() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.SetSelectedTaskFlag(!tdc.IsSelectedTaskFlagged());
}

void CToDoListWnd::OnUpdateEditFlagtask(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
	pCmdUI->SetCheck(tdc.IsSelectedTaskFlagged() ? 1 : 0);
}

void CToDoListWnd::OnEditGotoDependency() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.GotoSelectedTaskDependency();
}

void CToDoListWnd::OnUpdateEditGotoDependency(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	CStringArray aDepends;

	pCmdUI->Enable(tdc.GetSelectedTaskDependencies(aDepends) > 0);	
}

void CToDoListWnd::OnEditRecurrence() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.EditSelectedTaskRecurrence();	
}

void CToDoListWnd::OnUpdateEditRecurrence(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();

	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnFileOpenarchive() 
{
	CString sArchivePath = m_mgrToDoCtrls.GetArchivePath(GetSelToDoCtrl());
	BOOL bArchiveExists = FileMisc::FileExists(sArchivePath);
	
	if (bArchiveExists)
		OpenTaskList(sArchivePath, FALSE);
}

void CToDoListWnd::OnUpdateFileOpenarchive(CCmdUI* pCmdUI) 
{
	CString sArchivePath = m_mgrToDoCtrls.GetArchivePath(GetSelToDoCtrl());
	BOOL bArchiveExists = FileMisc::FileExists(sArchivePath);
	
	pCmdUI->Enable(bArchiveExists);
}

void CToDoListWnd::PrepareFilter(FTDCFILTER& filter) const
{
	if (filter.nShow != FS_CUSTOM)
	{
		// handle title filter option
		switch (Prefs().GetTitleFilterOption())
		{
		case PUIP_MATCHONTITLECOMMENTS:
			filter.nTitleOption = FT_FILTERONTITLECOMMENTS;
			break;

		case PUIP_MATCHONANYTEXT:
			filter.nTitleOption = FT_FILTERONANYTEXT;
			break;

		case PUIP_MATCHONTITLE:
		default:
			filter.nTitleOption = FT_FILTERONTITLEONLY;
			break;
		}
	}
}

void CToDoListWnd::OnViewShowfilterbar() 
{
	m_bShowFilterBar = !m_bShowFilterBar;
	m_filterBar.ShowWindow(m_bShowFilterBar ? SW_SHOW : SW_HIDE);

	Resize();
	Invalidate(TRUE);
}

void CToDoListWnd::OnUpdateViewShowfilterbar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowFilterBar ? 1 : 0);
}

void CToDoListWnd::OnViewClearfilter() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	if (tdc.HasFilter() || tdc.HasCustomFilter())
	{
		tdc.ClearFilter();
	
		// reenable the filter controls
		//m_filterBar.RemoveCustomFilters();
		m_filterBar.RefreshFilterControls(tdc);
	
		RefreshFilterControls();
		UpdateStatusbar();
	}
}

void CToDoListWnd::OnUpdateViewClearfilter(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(tdc.HasFilter() || tdc.HasCustomFilter());
}

void CToDoListWnd::OnViewTogglefilter()
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	tdc.ToggleFilter();

	RefreshFilterControls();
	UpdateStatusbar();

	// reenable the filter controls
	m_filterBar.SetCustomFilter(tdc.HasCustomFilter(), tdc.GetCustomFilterName());
}

void CToDoListWnd::OnUpdateViewTogglefilter(CCmdUI* pCmdUI)
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	pCmdUI->Enable(tdc.HasFilter() || tdc.HasLastFilter() || tdc.HasCustomFilter());
}

LRESULT CToDoListWnd::OnSelchangeFilter(WPARAM wp, LPARAM lp) 
{
	FTDCFILTER* pFilter = (FTDCFILTER*)wp;
	CString sCustom((LPCTSTR)lp);

	ASSERT(pFilter);

	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	if (pFilter->nShow == FS_CUSTOM)
	{
		SEARCHPARAMS params;

		if (sCustom.IsEmpty())
			m_findDlg.GetSearchParams(params);
		else
			m_findDlg.GetSearchParams(sCustom, params);

		tdc.SetCustomFilter(params, sCustom);
	}
	else
	{
		PrepareFilter(*pFilter);
		tdc.SetFilter(*pFilter);
	}

	m_filterBar.RefreshFilterControls(tdc);

	UpdateStatusbar();

	return 0L;
}

void CToDoListWnd::OnViewFilter() 
{
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();
	CStringArray aCustom;
	m_filterBar.GetCustomFilters(aCustom);
	
	CTDLFilterDlg dialog(Prefs().GetMultiSelFilters());

	if (dialog.DoModal(aCustom, tdc, m_aPriorityColors) == IDOK)
	{
		FTDCFILTER filter;
		CString sCustom;
		
		dialog.GetFilter(filter, sCustom);

		OnSelchangeFilter((WPARAM)&filter, (LPARAM)(LPCTSTR)sCustom);
	}
}

void CToDoListWnd::OnUpdateViewFilter(CCmdUI* pCmdUI) 
{
	UNREFERENCED_PARAMETER(pCmdUI);
	//	pCmdUI->Enable(!m_bShowFilterBar);
}

void CToDoListWnd::OnViewRefreshfilter() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	FTDCFILTER filterTDC, filter;

	tdc.GetFilter(filterTDC);
	m_filterBar.GetFilter(filter);
	PrepareFilter(filter);
	
	// if the filter has changed then set the new one else
	// refresh the current one
	if (filterTDC == filter)
		tdc.RefreshFilter();	
	else
	{
		tdc.SetFilter(filter);
	
		if (Prefs().GetExpandTasksOnLoad())
			tdc.ExpandTasks(TDCEC_ALL);
	}

	UpdateStatusbar();
}

void CToDoListWnd::OnUpdateViewRefreshfilter(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	FTDCFILTER filterTDC, filter;

	tdc.GetFilter(filterTDC);
	m_filterBar.GetFilter(filter);
	
	pCmdUI->Enable(tdc.HasFilter() || (filter != filterTDC) || tdc.HasCustomFilter());
}

void CToDoListWnd::OnTabctrlPreferences() 
{
	DoPreferences(PREFPAGE_UI);
}

void CToDoListWnd::OnTasklistSelectColumns() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSel = GetSelToDoCtrl();

	CTDCColumnIDArray aColumns, aDefault;
	tdc.GetVisibleColumns(aColumns);
	Prefs().GetDefaultColumns(aDefault);

	CTDLColumnSelectionDlg dialog(aColumns, aDefault, TRUE);

	if (dialog.DoModal() == IDOK)
	{
		dialog.GetVisibleColumns(aColumns);

		tdc.SetVisibleColumns(aColumns);
		m_filterBar.SetVisibleFilters(aColumns);

		if (dialog.GetApplyActiveTasklist())
			m_mgrToDoCtrls.SetHasOwnColumns(nSel, TRUE);
		else
		{
			// update preferences
			ASSERT(m_pPrefs);
			m_pPrefs->SetDefaultColumns(aColumns);

			// and flag other tasklists as requiring updates
			m_mgrToDoCtrls.SetAllNeedPreferenceUpdate(TRUE, nSel);
			m_mgrToDoCtrls.SetHasOwnColumns(nSel, FALSE);
		}

		// reload the menu if we dynamically alter it
		if (Prefs().GetShowEditMenuAsColumns())
			LoadMenubar();

		Resize();
	}
}

void CToDoListWnd::OnViewProjectname() 
{
	m_bShowProjectName = !m_bShowProjectName;
	
	// mark all tasklists as needing update
	m_mgrToDoCtrls.SetAllNeedPreferenceUpdate(TRUE);
	
	// then update active tasklist
	GetToDoCtrl().SetStyle(TDCS_SHOWPROJECTNAME, m_bShowProjectName);
	m_mgrToDoCtrls.SetNeedsPreferenceUpdate(GetSelToDoCtrl(), FALSE);
}

void CToDoListWnd::OnUpdateViewProjectname(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowProjectName ? 1 : 0);
}

void CToDoListWnd::OnEditOffsetdates() 
{
	COffsetDatesDlg dialog;
	
	if (dialog.DoModal() == IDOK)
	{
		ODD_UNITS nUnits;
		int nAmount = dialog.GetOffsetAmount(nUnits);
		
		if (!nAmount)
			return;
		
		DWORD dwWhat = dialog.GetOffsetWhat();
		BOOL bSubtasks = dialog.GetOffsetSubtasks();
		
		// translate units
		int nTDCUnits = (nUnits == ODDU_DAYS) ? TDITU_DAYS :
						((nUnits == ODDU_WEEKS) ? TDITU_WEEKS : TDITU_MONTHS);
		
		// do the offsets
		CFilteredToDoCtrl& tdc = GetToDoCtrl();
		
		if (dwWhat & ODD_STARTDATE)
			tdc.OffsetSelectedTaskDate(TDCD_START, nAmount, nTDCUnits, bSubtasks);
		
		if (dwWhat & ODD_DUEDATE)
			tdc.OffsetSelectedTaskDate(TDCD_DUE, nAmount, nTDCUnits, bSubtasks);
		
		if (dwWhat & ODD_DONEDATE)
			tdc.OffsetSelectedTaskDate(TDCD_DONE, nAmount, nTDCUnits, bSubtasks);
	}
}

void CToDoListWnd::OnUpdateEditOffsetdates(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

void CToDoListWnd::OnPrintpreview() 
{
	DoPrint(TRUE);
}

void CToDoListWnd::OnShowTimelogfile() 
{
	CString sLogPath = GetToDoCtrl().GetSelectedTaskTimeLogPath();
	
	if (!sLogPath.IsEmpty())
		FileMisc::Run(*this, sLogPath, NULL, SW_HIDE);
}

void CToDoListWnd::OnUpdateShowTimelogfile(CCmdUI* pCmdUI) 
{
	const CPreferencesDlg& userPrefs = Prefs();
	int nTasks = GetToDoCtrl().GetSelectedCount();
	BOOL bEnable = FALSE;

	if (userPrefs.GetLogTimeTracking() && 
		(nTasks == 1 || !userPrefs.GetLogTaskTimeSeparately()))
	{
		CString sLogPath = GetToDoCtrl().GetSelectedTaskTimeLogPath();
		bEnable = FileMisc::FileExists(sLogPath);
	}
	
	pCmdUI->Enable(bEnable);	
}

void CToDoListWnd::OnAddtimetologfile() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	DWORD dwTaskID = tdc.GetSelectedTaskID();
	CString sTitle = tdc.GetSelectedTaskTitle();

	CTDLAddLoggedTimeDlg dialog(dwTaskID, sTitle);

	if (dialog.DoModal() == IDOK)
		tdc.AddTimeToTaskLogFile(dwTaskID, dialog.GetLoggedTime(), dialog.GetWhen(), dialog.GetAddToTimeSpent());
}

void CToDoListWnd::OnUpdateAddtimetologfile(CCmdUI* pCmdUI) 
{
	BOOL bEnable = (Prefs().GetLogTimeTracking() && GetToDoCtrl().GetSelectedCount() == 1);
	pCmdUI->Enable(bEnable);	
}

LRESULT CToDoListWnd::OnToDoCtrlDoLengthyOperation(WPARAM wParam, LPARAM lParam)
{
	if (wParam) // start op
	{
		m_sbProgress.BeginProgress(m_statusBar, (LPCTSTR)lParam);
	}
	else // end op
	{
		m_sbProgress.EndProgress();
	}
	
	return 0L;
}

BOOL CToDoListWnd::DoTaskLink(const CString& sPath, DWORD dwTaskID)
{
	// handle no file path => active tasklist
	if (sPath.IsEmpty())
	{
		ASSERT(dwTaskID);
		GetToDoCtrl().SelectTask(dwTaskID);

		return TRUE; // handled regardless of result
	}
	else
	{
		// build the full path to the file
		// from the folder of the active tasklist
		int nSelTDC = GetSelToDoCtrl();

		CString sActivePath = m_mgrToDoCtrls.GetFilePath(nSelTDC);
		CString sFolder = FileMisc::GetFolderFromFilePath(sActivePath);

		CString sFile(sPath);
		FileMisc::MakeFullPath(sFile, sFolder);

		// do we have this tasklist ?
		int nTDC = m_mgrToDoCtrls.FindToDoCtrl(sFile);

		if (nTDC != -1)
		{
			if (SelectToDoCtrl(nTDC, (nTDC != nSelTDC)) && dwTaskID)
				GetToDoCtrl().SelectTask(dwTaskID);

			return TRUE; // handled regardless of result
		}
		else if (!Prefs().GetMultiInstance())
		{
			TDC_FILE nRet = OpenTaskList(sFile);
			
			if (nRet == TDCO_SUCCESS)
			{
				if (dwTaskID)
					GetToDoCtrl().SelectTask(dwTaskID);
			}
			else
				HandleLoadTasklistError(nRet, sFile);

			return TRUE; // handled regardless of result
		}
	}

	// not handled
	return FALSE;
}

LRESULT CToDoListWnd::OnToDoCtrlDoTaskLink(WPARAM wParam, LPARAM lParam)
{
	DWORD dwTaskID = wParam;
	CString sFile((LPCTSTR)lParam);
	
	// can we handle it ?
	if (DoTaskLink(sFile, dwTaskID))
		return TRUE;

	// Pass to our app startup code to look 
	// for another instance who can handle it
	CString sCommandline;
		
	sCommandline.Format(_T("%s -l \"%s?%ld\""),
						FileMisc::GetAppFileName(),
						sFile,
						dwTaskID);

	return FileMisc::Run(*this, sCommandline);
}

LRESULT CToDoListWnd::OnTodoCtrlFailedLink(WPARAM /*wParam*/, LPARAM lParam)
{
	LPCTSTR szLink = (LPCTSTR)lParam;

	// if it's an Outlook link then prompt to install
	// the Outlook URL handler
	if (COutlookHelper::IsOutlookUrl(szLink))
	{
		// if the handler installs properly give the url another go
		if (COutlookHelper::QueryInstallUrlHandler(IDS_QUERYINSTALLOUTLOOKHANDLER))
			FileMisc::Run(*this, szLink);

		return TRUE; // we handled it regardless
	}
	else // see if it's a task link
	{
		CString sFile;
		DWORD dwTaskID = 0;

		CFilteredToDoCtrl::ParseTaskLink(szLink, dwTaskID, sFile);

		if (DoTaskLink(sFile, dwTaskID))
			return TRUE; // we handled it
	}

	// all else
	AfxMessageBox(IDS_COMMENTSGOTOERRMSG);
	return 0L;
}

LRESULT CToDoListWnd::OnToDoCtrlTaskIsDone(WPARAM wParam, LPARAM lParam)
{
	ASSERT (lParam);
	CString sFile((LPCTSTR)lParam);
	
	if (!sFile.IsEmpty())
	{
		// build the full path to the file
		if (::PathIsRelative(sFile))
		{
			// append it to the folder containing the active tasklist
			CString sPathName = m_mgrToDoCtrls.GetFilePath(GetSelToDoCtrl());
			CString sDrive, sFolder;

			FileMisc::SplitPath(sPathName, &sDrive, &sFolder);
			FileMisc::MakePath(sFile, sDrive, sFolder, sFile);
		}
		// else its a full path already
		
		int nTDC = m_mgrToDoCtrls.FindToDoCtrl(sFile);

		if (nTDC != -1) // already loaded
			return GetToDoCtrl(nTDC).IsTaskDone(wParam);
		else
		{
			// we must load the tasklist ourselves
			CTaskFile tasks;

			if (tasks.Load(sFile))
			{
				HTASKITEM ht = tasks.FindTask(wParam);
				return ht ? tasks.IsTaskDone(ht) : FALSE;
			}
		}
	}
	
	return 0L;
}

LRESULT CToDoListWnd::OnPowerBroadcast(WPARAM wp, LPARAM /*lp*/)
{
	const CPreferencesDlg& userPrefs = Prefs();

	switch (wp)
	{
	case PBT_APMSUSPEND:
	case PBT_APMSTANDBY:
	case PBT_APMQUERYSUSPEND:
	case PBT_APMQUERYSTANDBY:
		// Terminate all timers
		SetTimer(TIMER_DUEITEMS, FALSE);
		SetTimer(TIMER_READONLYSTATUS, FALSE);
		SetTimer(TIMER_TIMESTAMPCHANGE, FALSE);
		SetTimer(TIMER_CHECKOUTSTATUS, FALSE);
		SetTimer(TIMER_AUTOSAVE, FALSE);
		SetTimer(TIMER_TIMETRACKING, FALSE);
		break;

	case PBT_APMQUERYSUSPENDFAILED:
	case PBT_APMQUERYSTANDBYFAILED:
	case PBT_APMRESUMECRITICAL:
	case PBT_APMRESUMESUSPEND: 
	case PBT_APMRESUMESTANDBY:
		// reset time tracking as required
		if (!userPrefs.GetTrackHibernated())
		{
			int nCtrl = GetTDCCount();
			
			while (nCtrl--)
				GetToDoCtrl(nCtrl).ResetTimeTracking();
		}

		// restart timers
		SetTimer(TIMER_DUEITEMS, TRUE);
		SetTimer(TIMER_READONLYSTATUS, userPrefs.GetReadonlyReloadOption() != RO_NO);
		SetTimer(TIMER_TIMESTAMPCHANGE, userPrefs.GetTimestampReloadOption() != RO_NO);
		SetTimer(TIMER_AUTOSAVE, userPrefs.GetAutoSaveFrequency());
		SetTimer(TIMER_CHECKOUTSTATUS, userPrefs.GetCheckoutOnCheckin() || userPrefs.GetAutoCheckinFrequency());
		SetTimer(TIMER_TIMETRACKING, TRUE);

		// check for updates
		if (Prefs().GetAutoCheckForUpdates())
			CheckForUpdates(FALSE);
		break;
	}

	return TRUE; // allow 
}

LRESULT CToDoListWnd::OnGetFont(WPARAM /*wp*/, LPARAM /*lp*/)
{
	return (LRESULT)m_fontMain.GetSafeHandle();
}

void CToDoListWnd::OnViewStatusBar() 
{
	m_bShowStatusBar = !m_bShowStatusBar;
	m_statusBar.ShowWindow(m_bShowStatusBar ? SW_SHOW : SW_HIDE);
	
	SendMessage(WM_SIZE, SIZE_RESTORED, 0L);
	//Resize();

	if (m_bShowStatusBar)
		UpdateStatusbar();
	else
		UpdateCaption();
}

void CToDoListWnd::OnUpdateViewStatusBar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowStatusBar ? 1 : 0) ;
}

BOOL CToDoListWnd::OnQueryOpen() 
{
	if (CFrameWnd::OnQueryOpen())
	{
		// fail if the active tasklist is encrypted because we have to verify the password
		// and we're not allowed to display a dialog in this message handler
		if (!m_bQueryOpenAllow && GetToDoCtrl().IsEncrypted())
		{
			PostMessage(WM_TDL_RESTORE); 
			return FALSE;
		}
		
		// all others
		return TRUE;
	}
	
	return FALSE;
}

LRESULT CToDoListWnd::OnToDoListRestore(WPARAM /*wp*/, LPARAM /*lp*/)
{
    ASSERT (IsIconic() && GetToDoCtrl().IsEncrypted()); // sanity check
	
    if (IsIconic())
    {
        if (VerifyToDoCtrlPassword())
		{
			CAutoFlag af(m_bQueryOpenAllow, TRUE);
            ShowWindow(SW_RESTORE);
		}
    }

	return 0L;
}

void CToDoListWnd::OnCopyTaskasLink() 
{
	CopySelectedTasksToClipboard(TDCTC_ASLINK);
}

void CToDoListWnd::OnUpdateCopyTaskasLink(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetSelectedCount() == 1);
}

void CToDoListWnd::OnCopyTaskasDependency() 
{
	CopySelectedTasksToClipboard(TDCTC_ASDEPENDS);
}

void CToDoListWnd::OnUpdateCopyTaskasDependency(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetSelectedCount() == 1);
}

void CToDoListWnd::OnCopyTaskasLinkFull() 
{
	CopySelectedTasksToClipboard(TDCTC_ASLINKFULL);
}

void CToDoListWnd::OnUpdateCopyTaskasLinkFull(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetSelectedCount() == 1);
}

void CToDoListWnd::OnCopyTaskasDependencyFull() 
{
	CopySelectedTasksToClipboard(TDCTC_ASDEPENDSFULL);
}

void CToDoListWnd::OnUpdateCopyTaskasDependencyFull(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetSelectedCount() == 1);
}

void CToDoListWnd::OnCopyTaskasPath() 
{
	CopySelectedTasksToClipboard(TDCTC_ASPATH);
}

void CToDoListWnd::OnUpdateCopyTaskasPath(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetSelectedCount() == 1);
}

BOOL CToDoListWnd::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (CFrameWnd::PreCreateWindow(cs))
	{
		cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

		// Check if our class is already defined
		LPCTSTR pszClassName = _T("ToDoListFrame");
		WNDCLASS wndcls;

		if (!::GetClassInfo(AfxGetInstanceHandle(), pszClassName, &wndcls))
		{
			// Get the current requested window class
			VERIFY(GetClassInfo(AfxGetInstanceHandle(), cs.lpszClass, &wndcls));

			// We want to register this info with our name
			wndcls.lpszClassName = pszClassName;

			// Need to preset the icon otherwise the function GetIconWndClass
			// calling us will overwrite our class.
			wndcls.hIcon = GraphicsMisc::LoadIcon(IDR_MAINFRAME);

			// Register our class now and check the outcome
			if (!::RegisterClass(&wndcls))
			{
				ASSERT(0);
				return FALSE;
			}
		}

		// Now use our class 
		cs.lpszClass = pszClassName;
		return TRUE;
	}
	
	// else
	return FALSE;
}

void CToDoListWnd::OnWindowPosChanging(WINDOWPOS FAR* lpwndpos) 
{
	CFrameWnd::OnWindowPosChanging(lpwndpos); 
}

void CToDoListWnd::OnToolsCheckforupdates() 
{
	CheckForUpdates(TRUE);
}

void CToDoListWnd::OnEditInsertdatetime() 
{
	DoInsertDateAndTime(TRUE, TRUE);
}

void CToDoListWnd::OnEditInsertdate() 
{
	DoInsertDateAndTime(TRUE, FALSE);
}

void CToDoListWnd::OnEditInserttime() 
{
	DoInsertDateAndTime(FALSE, TRUE);
}

void CToDoListWnd::DoInsertDateAndTime(BOOL bDate, BOOL bTime) 
{
	COleDateTime date = COleDateTime::GetCurrentTime();
	const CPreferencesDlg& userPrefs = Prefs();

	CString sInsert;

	if (bDate) // date only or date and time
	{
		DWORD dwFmt = (bTime ? DHFD_TIME : 0);

		if (userPrefs.GetShowWeekdayInDates())
			dwFmt |= DHFD_DOW;
						
		if (userPrefs.GetDisplayDatesInISO())
			dwFmt |= DHFD_ISO;
						
		sInsert = CDateHelper::FormatDate(date, dwFmt);
	}
	else // time only
	{
		if (userPrefs.GetDisplayDatesInISO())
			sInsert = CTimeHelper::FormatISOTime(date.GetHour(), date.GetMinute(), date.GetSecond(), TRUE);
		else
			sInsert = CTimeHelper::Format24HourTime(date.GetHour(), date.GetMinute(), date.GetSecond(), TRUE);
	}

	// add trailing space
	sInsert += ' ';

	GetToDoCtrl().PasteText(sInsert);
}

void CToDoListWnd::OnUpdateEditInsertdatetime(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanPasteText());
}

void CToDoListWnd::OnUpdateEditInserttime(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanPasteText());
}

void CToDoListWnd::OnUpdateEditInsertdate(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().CanPasteText());
}

void CToDoListWnd::OnSysColorChange() 
{
	CFrameWnd::OnSysColorChange();
	
	InitMenuIconManager();

	SetUITheme(m_sThemeFile);
}

void CToDoListWnd::UpdateSBPaneAndTooltip(UINT nIDPane, UINT nIDTextFormat, const CString& sValue, UINT nIDTooltip, TDC_COLUMN nTDCC)
{
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();
	const CPreferencesDlg& userPrefs = Prefs();
		
	CEnString sText, sTooltip;

	if (!userPrefs.GetShowCtrlsAsColumns() || tdc.IsColumnShowing(nTDCC))
	{
		sText.Format(nIDTextFormat, sValue);
		sTooltip.LoadString(nIDTooltip);
	}
	else
	{
		sText.Empty();
		sTooltip.Empty();
	}

	int nPane = m_statusBar.CommandToIndex(nIDPane);

	m_statusBar.SetPaneText(nPane, sText);
	m_statusBar.SetPaneTooltipIndex(nPane, sTooltip);
}

void CToDoListWnd::UpdateStatusBarInfo(const CFilteredToDoCtrl& tdc, TDCSTATUSBARINFO& sbi) const
{
	sbi.nSelCount = tdc.GetSelectedCount();
	sbi.dwSelTaskID = tdc.GetSelectedTaskID();
	sbi.dCost = tdc.CalcSelectedTaskCost();

	const CPreferencesDlg& userPrefs = Prefs();

	userPrefs.GetDefaultTimeEst(sbi.nTimeEstUnits);
	sbi.dTimeEst = tdc.CalcSelectedTaskTimeEstimate(sbi.nTimeEstUnits);

	userPrefs.GetDefaultTimeSpent(sbi.nTimeSpentUnits);
	sbi.dTimeSpent = tdc.CalcSelectedTaskTimeSpent(sbi.nTimeSpentUnits);
}

void CToDoListWnd::OnUpdateSBSelectionCount(CCmdUI* /*pCmdUI*/)
{
	if (GetTDCCount())
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl();

		// keep track of previous information to avoid unnecessary processing
		static TDCSTATUSBARINFO sbiPrev;

		TDCSTATUSBARINFO sbi;
		UpdateStatusBarInfo(tdc, sbi);

		if (sbi == sbiPrev)
			return;

		sbiPrev = sbi;

		// number of selected tasks
		CEnString sText;

		if (sbi.nSelCount == 1)
		{
			ASSERT(sbi.dwSelTaskID);
			sText.Format(ID_SB_SELCOUNTONE, sbi.dwSelTaskID);
		}
		else
			sText.Format(ID_SB_SELCOUNT, sbi.nSelCount);

		m_statusBar.SetPaneText(m_statusBar.CommandToIndex(ID_SB_SELCOUNT), sText);

		// times
		const CPreferencesDlg& userPrefs = Prefs();

		// estimate
		if (userPrefs.GetUseHMSTimeFormat())
			sText = CTimeHelper().FormatTimeHMS(sbi.dTimeEst, sbi.nTimeEstUnits);
		else
			sText = CTimeHelper().FormatTime(sbi.dTimeEst, sbi.nTimeEstUnits, 2);

		UpdateSBPaneAndTooltip(ID_SB_SELTIMEEST, ID_SB_SELTIMEEST, sText, IDS_SB_SELTIMEEST_TIP, TDCC_TIMEEST);

		// spent
		if (userPrefs.GetUseHMSTimeFormat())
			sText = CTimeHelper().FormatTimeHMS(sbi.dTimeSpent, sbi.nTimeSpentUnits);
		else
			sText = CTimeHelper().FormatTime(sbi.dTimeSpent, sbi.nTimeSpentUnits, 2);

		UpdateSBPaneAndTooltip(ID_SB_SELTIMESPENT, ID_SB_SELTIMESPENT, sText, IDS_SB_SELTIMESPENT_TIP, TDCC_TIMESPENT);

		// cost
		sText = Misc::Format(sbi.dCost, 2);
		UpdateSBPaneAndTooltip(ID_SB_SELCOST, ID_SB_SELCOST, sText, IDS_SB_SELCOST_TIP, TDCC_COST);

		// set tray tip too
		UpdateTooltip();
	}
}

void CToDoListWnd::OnUpdateSBTaskCount(CCmdUI* /*pCmdUI*/)
{
	if (GetTDCCount())
	{
		CFilteredToDoCtrl& tdc = GetToDoCtrl();

		UINT nVisibleTasks;
		UINT nTotalTasks = tdc.GetTaskCount(&nVisibleTasks);

		CEnString sText;
		sText.Format(IDS_SB_TASKCOUNT, nVisibleTasks, nTotalTasks);
		int nIndex = m_statusBar.CommandToIndex(ID_SB_TASKCOUNT);

		m_statusBar.SetPaneText(nIndex, sText);
		m_statusBar.SetPaneTooltipIndex(nIndex, CEnString(IDS_SB_TASKCOUNT_TIP));
	}
}

void CToDoListWnd::OnEditSelectall() 
{
	GetToDoCtrl().SelectAll();
}

void CToDoListWnd::OnUpdateEditSelectall(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetToDoCtrl().GetTaskCount());	
}

void CToDoListWnd::OnCloseallbutthis() 
{
	int nThis = GetSelToDoCtrl();
	int nCtrl = GetTDCCount();
	
	// remove tasklists
	while (nCtrl--)
	{
		if (nCtrl != nThis)
		{
			if (ConfirmSaveTaskList(nCtrl, TDLS_CLOSINGTASKLISTS) != TDCO_SUCCESS)
				continue; // user cancelled

			m_mgrToDoCtrls.RemoveToDoCtrl(nCtrl, TRUE);
		}
	}
}

void CToDoListWnd::OnUpdateCloseallbutthis(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(GetTDCCount() > 1);
}

void CToDoListWnd::DoSendTasks(BOOL bSelected)
{
	CTDLSendTasksDlg dialog(bSelected, GetToDoCtrl().GetView());

	if (dialog.DoModal() == IDOK)
	{
		// get tasks
		CFilteredToDoCtrl& tdc = GetToDoCtrl();
		CTaskFile tasks;

		GetTasks(tdc, FALSE, FALSE, dialog.GetTaskSelection(), tasks, NULL);

		// package them up
		CString sAttachment, sBody;
		TD_SENDAS nSendAs = dialog.GetSendAs();

		switch (nSendAs)
		{
		case TDSA_TASKLIST:
			{
				CString sFilename, sExt;
				FileMisc::SplitPath(tdc.GetFilePath(), NULL, NULL, &sFilename, &sExt);
				
				sAttachment = FileMisc::GetTempFileName(sFilename, sExt);
				
				if (!tasks.Save(sAttachment))
				{
					// TODO
					return;
				}
				
				sBody.LoadString(IDS_TASKLISTATTACHED);
			}
			break;
			
		case TDSA_BODYTEXT:
			sBody = m_mgrImportExport.ExportTaskListToText(&tasks);
			break;
		}
		
		// form subject
		CString sSubject = tdc.GetFriendlyProjectName();
		
		// recipients
		CStringArray aTo;
		tdc.GetSelectedTaskAllocTo(aTo);

		CString sTo = Misc::FormatArray(aTo, _T(";"));

		// prefix with task name if necessary
		if (dialog.GetTaskSelection().GetWantSelectedTasks() && tdc.GetSelectedCount() == 1)
		{
			sSubject = tdc.GetSelectedTaskTitle() + _T(" - ") + sSubject;
		}

		CSendFileTo().SendMail(*this, sTo, sSubject, sBody, sAttachment);
	}
}

void CToDoListWnd::OnSendTasks() 
{
	DoSendTasks(FALSE);
}

void CToDoListWnd::OnUpdateSendTasks(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.GetTaskCount());
}

void CToDoListWnd::OnSendSelectedTasks() 
{
	DoSendTasks(TRUE);
}

void CToDoListWnd::OnUpdateSendSelectedTasks(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.GetTaskCount());
}

void CToDoListWnd::OnEditUndo() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.UndoLastAction(TRUE);
	UpdateStatusbar();
}

void CToDoListWnd::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.CanUndoLastAction(TRUE));
}

void CToDoListWnd::OnEditRedo() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.UndoLastAction(FALSE);
	UpdateStatusbar();
}

void CToDoListWnd::OnUpdateEditRedo(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.CanUndoLastAction(FALSE));
}

void CToDoListWnd::OnViewCycleTaskViews() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	tdc.SetNextView();
	tdc.SetFocusToTasks();
}

void CToDoListWnd::OnUpdateViewCycleTaskViews(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_nMaxState != TDCMS_MAXCOMMENTS);
}

void CToDoListWnd::OnViewToggleTreeandList() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	FTC_VIEW nView = tdc.GetView();

	switch (nView)
	{
	case FTCV_TASKTREE:
		nView = FTCV_TASKLIST;
		break;

	case FTCV_TASKLIST:
	default:
		nView = FTCV_TASKTREE;
		break;
	}

	tdc.SetView(nView);
	tdc.SetFocusToTasks();
}

void CToDoListWnd::OnUpdateViewToggleTreeandList(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_nMaxState != TDCMS_MAXCOMMENTS);
}

void CToDoListWnd::OnViewToggletasksandcomments() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	if (!tdc.TasksHaveFocus())
		tdc.SetFocusToTasks();
	else
		tdc.SetFocusToComments();
}

void CToDoListWnd::OnUpdateViewToggletasksandcomments(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_nMaxState == TDCMS_NORMAL || 
					(m_nMaxState == TDCMS_MAXTASKLIST && Prefs().GetShowCommentsAlways()));
}

void CToDoListWnd::OnUpdateQuickFind(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_bShowToolbar);
}

void CToDoListWnd::OnQuickFind() 
{
	if (m_bShowToolbar)
		m_cbQuickFind.SetFocus();
}

void CToDoListWnd::OnQuickFindNext() 
{
	if (!m_sQuickFind.IsEmpty())
	{
		if (!GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTNEXT))
			GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTFIRST); // return to start
	}
}

void CToDoListWnd::OnUpdateQuickFindNext(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!m_sQuickFind.IsEmpty());
}

LRESULT CToDoListWnd::OnQuickFindItemAdded(WPARAM /*wp*/, LPARAM /*lp*/)
{
	// keep only the last 20 items
	int nItem = m_cbQuickFind.GetCount();

	while (nItem > 20)
	{
		nItem--;
		m_cbQuickFind.DeleteString(nItem);
	}

	return 0L;
}

void CToDoListWnd::OnQuickFindPrev() 
{
	if (!m_sQuickFind.IsEmpty())
	{
		if (!GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTPREV))
			GetToDoCtrl().SelectTask(m_sQuickFind, TDC_SELECTLAST); // return to end
	}
}

void CToDoListWnd::OnUpdateQuickFindPrev(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!m_sQuickFind.IsEmpty());
}

void CToDoListWnd::OnMove(int x, int y) 
{
	CFrameWnd::OnMove(x, y);
}

void CToDoListWnd::OnEditSettaskicon() 
{
	GetToDoCtrl().EditSelectedTaskIcon();
}

void CToDoListWnd::OnUpdateEditSettaskicon(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly());	
}

LRESULT CToDoListWnd::OnToDoCtrlReminder(WPARAM /*wp*/, LPARAM lp)
{
	AF_NOREENTRANT_RET(0L) // macro helper

	// ignore if we are showing a dialog
	if (!IsWindowEnabled())
		return 0L;

	Show(FALSE);

	CTDLShowReminderDlg dialog;
	TDCREMINDER* pReminder = (TDCREMINDER*)lp;

	int nRet = dialog.DoModal(*pReminder);
	
	switch (nRet)
	{
	case IDSNOOZE:
		{
			double dNow = COleDateTime::GetCurrentTime();

			if (pReminder->bRelative)
			{
				if (pReminder->nRelativeFromWhen == TDCR_DUEDATE)
				{
					// in case the user didn't handle the notification immediately we need
					// to soak up any additional elapsed time in the snooze
					COleDateTime dDue = pReminder->pTDC->GetTaskDate(pReminder->dwTaskID, TDCD_DUE);
					
					pReminder->dDaysSnooze = (dNow - dDue + pReminder->dRelativeDaysLeadIn);
				}
				else // from start
				{
					// in case the user didn't handle the notification immediately we need
					// to soak up any additional elapsed time in the snooze
					COleDateTime dStart = pReminder->pTDC->GetTaskDate(pReminder->dwTaskID, TDCD_START);
					
					pReminder->dDaysSnooze = (dNow - dStart + pReminder->dRelativeDaysLeadIn);
				}
			}
			else // absolute
			{
				// in case the user didn't handle the notification immediately we need
				// to soak up any additional elapsed time in the snooze
				pReminder->dDaysSnooze = dNow - pReminder->dtAbsolute;
			}
				
				// then we add the user's snooze
			pReminder->dDaysSnooze += dialog.GetSnoozeDays();
		}
		return 0L; // don't delete (default)

	case IDGOTOTASK:
		{
			int nTDC = m_mgrToDoCtrls.FindToDoCtrl(pReminder->pTDC);
			ASSERT(nTDC != -1);

			SelectToDoCtrl(nTDC, TRUE);
			GetToDoCtrl().SelectTask(pReminder->dwTaskID);
		}
		// fall thru

	case IDCANCEL:
	default:
		// delete unless it's a recurring task in which case we 
		// disable it so that it can later be copied when the 
		// recurring task is completed
		if (GetToDoCtrl().IsTaskRecurring(pReminder->dwTaskID))
		{
			pReminder->bEnabled = FALSE;
			return 0L; // don't delete
		}
		// else
		return 1L; // delete
	}
}

LRESULT CToDoListWnd::OnToDoCtrlTaskHasReminder(WPARAM wParam, LPARAM lParam)
{
	int nRem = m_reminders.FindReminder(wParam, (CFilteredToDoCtrl*)lParam, FALSE);
	return (nRem != -1);
}

LRESULT CToDoListWnd::OnDoubleClkReminderCol(WPARAM /*wp*/, LPARAM /*lp*/)
{
	OnEditSetReminder();
	return 0L;
}

void CToDoListWnd::OnEditSetReminder() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	CTDLSetReminderDlg dialog;
	
	CString sTitle = tdc.GetSelectedTaskTitle();
	
	CDWordArray aTaskIDs;
	int nNumSel = tdc.GetSelectedTaskIDs(aTaskIDs, TRUE);
	
	if (nNumSel == 0)
		return;
	
	// get the first reminder as a reference
	BOOL bNewReminder = TRUE;
	TDCREMINDER rem;
	
	for (int nTask = 0; nTask < nNumSel; nTask++)
	{
		DWORD dwTaskID = aTaskIDs[nTask];
		int nRem = m_reminders.FindReminder(dwTaskID, &tdc);
		
		if (nRem != -1)
		{
			m_reminders.GetReminder(nRem, rem);
			bNewReminder = FALSE;
			break;
		}
	}
	
	// handle new task
	if (bNewReminder)
	{
		rem.dwTaskID = aTaskIDs[0];
		rem.pTDC = &tdc;
	}
	
	if (dialog.DoModal(rem, bNewReminder) == IDOK)
	{
		// apply reminder to selected tasks
		for (int nTask = 0; nTask < nNumSel; nTask++)
		{
			rem.dwTaskID = aTaskIDs[nTask];
			m_reminders.SetReminder(rem);
		}
		
		tdc.RedrawReminders();
	}
}

void CToDoListWnd::OnUpdateEditSetReminder(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	BOOL bEnable = (tdc.GetSelectedCount() > 0) && !tdc.SelectedTasksAreAllDone();
	pCmdUI->Enable(bEnable);
}

void CToDoListWnd::OnEditClearReminder() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	
	CDWordArray aTaskIDs;
	int nTask = tdc.GetSelectedTaskIDs(aTaskIDs, TRUE);
	
	while (nTask--)
	{
		DWORD dwTaskID = aTaskIDs[nTask];
		m_reminders.RemoveReminder(dwTaskID, &tdc);
	}
	
	tdc.RedrawReminders();
}

void CToDoListWnd::OnUpdateEditClearReminder(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	BOOL bEnable = FALSE;
	
	// check at least one selected item has a reminder
	CDWordArray aTaskIDs;
	int nTask = tdc.GetSelectedTaskIDs(aTaskIDs, TRUE);
	
	while (nTask--)
	{
		DWORD dwTaskID = aTaskIDs[nTask];
		
		if (m_reminders.FindReminder(dwTaskID, &tdc) != -1)
		{
			bEnable = TRUE;
			break;
		}
	}
	
	pCmdUI->Enable(bEnable);
}

void CToDoListWnd::OnEditCleartaskicon() 
{
	GetToDoCtrl().ClearSelectedTaskIcon();
}

void CToDoListWnd::OnUpdateEditCleartaskicon(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	int nSelCount = tdc.GetSelectedCount();
	
	pCmdUI->Enable(nSelCount && !tdc.IsReadOnly() && tdc.SelectedTasksHaveIcons());	
}

void CToDoListWnd::OnSortMulti() 
{
	TDSORTCOLUMNS sort;
	CTDCColumnIDArray aColumns;
	CTDCCustomAttribDefinitionArray aAttribDefs;

	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	tdc.GetSortBy(sort);
	tdc.GetVisibleColumns(aColumns);
	tdc.GetCustomAttributeDefs(aAttribDefs);

	CTDLMultiSortDlg dialog(sort, aColumns, aAttribDefs);

	if (dialog.DoModal() == IDOK)
	{
		dialog.GetSortBy(sort);
		tdc.MultiSort(sort);
	}
}

void CToDoListWnd::OnUpdateSortMulti(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(GetToDoCtrl().IsMultiSorting() ? 1 : 0);
}

void CToDoListWnd::OnToolsRemovefromsourcecontrol() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	if (tdc.RemoveFromSourceControl())
	{
		int nCtrl = GetSelToDoCtrl();

		m_mgrToDoCtrls.UpdateToDoCtrlReadOnlyUIState(nCtrl);
		m_mgrToDoCtrls.UpdateTabItemText(nCtrl);
		m_mgrToDoCtrls.SetModifiedStatus(nCtrl, FALSE);
		m_mgrToDoCtrls.RefreshLastModified(nCtrl);
		m_mgrToDoCtrls.RefreshReadOnlyStatus(nCtrl);
		m_mgrToDoCtrls.RefreshPathType(nCtrl);
	}
}

void CToDoListWnd::OnUpdateToolsRemovefromsourcecontrol(CCmdUI* pCmdUI) 
{
	int nCtrl = GetSelToDoCtrl();

	BOOL bEnable = m_mgrToDoCtrls.IsSourceControlled(nCtrl);
//	bEnable &= !Prefs().GetEnableSourceControl();
//	bEnable &= m_mgrToDoCtrls.PathSupportsSourceControl(nCtrl);

	if (bEnable)
	{
		// make sure no-one has the file checked out
		CFilteredToDoCtrl& tdc = GetToDoCtrl();
		bEnable &= !tdc.IsCheckedOut();
	}

	pCmdUI->Enable(bEnable);
}


void CToDoListWnd::OnViewShowTasklistTabbar() 
{
	m_bShowTasklistBar = !m_bShowTasklistBar; 

	Resize();
	Invalidate(TRUE);
}

void CToDoListWnd::OnUpdateViewShowTasklistTabbar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowTasklistBar ? 1 : 0);
}

void CToDoListWnd::OnViewShowTreeListTabbar() 
{
	m_bShowTreeListBar = !m_bShowTreeListBar; 

	GetToDoCtrl().SetStyle(TDCS_SHOWTREELISTBAR, m_bShowTreeListBar);

	// refresh all the other tasklists
	m_mgrToDoCtrls.SetAllNeedPreferenceUpdate(TRUE, GetSelToDoCtrl());
}

void CToDoListWnd::OnUpdateViewShowTreeListTabbar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_bShowTreeListBar ? 1 : 0);
}

void CToDoListWnd::OnFileChangePassword() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	if (!tdc.IsReadOnly() && tdc.IsEncrypted() && VerifyToDoCtrlPassword())
	{
		tdc.EnableEncryption(FALSE); // clears the password
		tdc.EnableEncryption(TRUE); // forces it to be re-got
	}
}

void CToDoListWnd::OnUpdateFileChangePassword(CCmdUI* pCmdUI) 
{
	const CFilteredToDoCtrl& tdc = GetToDoCtrl();

	pCmdUI->Enable(!tdc.IsReadOnly() && tdc.IsEncrypted());
}

void CToDoListWnd::OnTasklistCustomColumns() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();

	if (!tdc.IsReadOnly())
	{
		CTDLCustomAttributeDlg dialog(tdc, m_theme);

		if (dialog.DoModal() == IDOK)
		{
			CTDCCustomAttribDefinitionArray aAttrib;

			dialog.GetAttributes(aAttrib);
			tdc.SetCustomAttributeDefs(aAttrib);
		}
	}
}

void CToDoListWnd::OnUpdateTasklistCustomcolumns(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(!GetToDoCtrl().IsReadOnly());
}

void CToDoListWnd::OnEditClearAttribute() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	TDC_ATTRIBUTE nAttrib = MapColumnToAttribute(m_nContextColumnID);

	tdc.ClearSelectedTaskAttribute(nAttrib);
}

void CToDoListWnd::OnUpdateEditClearAttribute(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	TDC_ATTRIBUTE nAttrib = MapColumnToAttribute(m_nContextColumnID);

	pCmdUI->Enable(tdc.CanClearSelectedTaskAttribute(nAttrib));
}

void CToDoListWnd::OnEditClearFocusedAttribute() 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	tdc.ClearSelectedTaskFocusedAttribute();
}

void CToDoListWnd::OnUpdateEditClearFocusedAttribute(CCmdUI* pCmdUI) 
{
	CFilteredToDoCtrl& tdc = GetToDoCtrl();
	pCmdUI->Enable(tdc.CanClearSelectedTaskFocusedAttribute());
}
