// RTFContentControl.cpp : implementation file
//

#include "stdafx.h"
#include "RTFContentCtrl.h"
#include "RTFContentControl.h"
//#include "EditWebLinkDlg.h"
#include "CreateFileLinkDlg.h"

#include "..\todolist\tdcmsg.h"
#include "..\todolist\tdlschemadef.h"

#include "..\shared\itasklist.h"
#include "..\shared\enfiledialog.h"
#include "..\shared\autoflag.h"
#include "..\shared\richedithelper.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\uitheme.h"
#include "..\shared\enstring.h"
#include "..\shared\preferences.h"
#include "..\shared\binarydata.h"
#include "..\shared\mswordhelper.h"
#include "..\shared\enmenu.h"
#include "..\shared\toolbarhelper.h"

#include "..\3rdparty\compression.h"
#include "..\3rdparty\zlib\zlib.h"

#include <afxpriv.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRTFContentControl

CRTFContentControl::CRTFContentControl() 
	: 
	m_bAllowNotify(TRUE), 
	m_reSpellCheck(m_rtf),
	m_mgrShortcuts(TRUE)
{
	// add custom protocol to comments field for linking to task IDs
	m_rtf.AddProtocol(TDL_PROTOCOL, TRUE);
}

CRTFContentControl::~CRTFContentControl()
{
}


BEGIN_MESSAGE_MAP(CRTFContentControl, CRulerRichEditCtrl)
	//{{AFX_MSG_MAP(CRTFContentControl)
	ON_WM_CONTEXTMENU()
	ON_WM_CREATE()
	//}}AFX_MSG_
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_COPYFORMATTING, OnEditCopyFormatting)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_COMMAND(ID_EDIT_FILEBROWSE, OnEditFileBrowse)
	ON_COMMAND(ID_EDIT_FIND, OnEditFind)
	ON_COMMAND(ID_EDIT_FINDREPLACE, OnEditFindReplace)
	ON_COMMAND(ID_EDIT_HORZRULE, OnEditHorzRule)
	ON_COMMAND(ID_EDIT_OPENURL, OnEditOpenUrl)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_EDIT_PASTEASREF, OnEditPasteasRef)
	ON_COMMAND(ID_EDIT_PASTEFORMATTING, OnEditPasteFormatting)
	ON_COMMAND(ID_EDIT_PASTESIMPLE, OnEditPasteSimple)
	ON_COMMAND(ID_EDIT_RIGHTALIGN, OnEditRightAlign)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_COMMAND(ID_EDIT_SHOWRULER, OnEditShowRuler)
	ON_COMMAND(ID_EDIT_SHOWTOOLBAR, OnEditShowToolbar)
	ON_COMMAND(ID_EDIT_SPELLCHECK, OnEditSpellcheck)
	ON_COMMAND(ID_EDIT_SUBSCRIPT, OnEditSubscript)
	ON_COMMAND(ID_EDIT_SUPERSCRIPT, OnEditSuperscript)
	ON_COMMAND(ID_PREFERENCES, OnPreferences)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPYFORMATTING, OnUpdateEditCopyFormatting)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_DELETE, OnUpdateEditDelete)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FILEBROWSE, OnUpdateEditFileBrowse)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FIND, OnUpdateEditFind)
	ON_UPDATE_COMMAND_UI(ID_EDIT_FINDREPLACE, OnUpdateEditFindReplace)
	ON_UPDATE_COMMAND_UI(ID_EDIT_HORZRULE, OnUpdateEditHorzRule)
	ON_UPDATE_COMMAND_UI(ID_EDIT_OPENURL, OnUpdateEditOpenUrl)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTEASREF, OnUpdateEditPasteasRef)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTEFORMATTING, OnUpdateEditPasteFormatting)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTESIMPLE, OnUpdateEditPasteSimple)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SELECT_ALL, OnUpdateEditSelectAll)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SHOWRULER, OnUpdateEditShowRuler)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SHOWTOOLBAR, OnUpdateEditShowToolbar)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SPELLCHECK, OnUpdateEditSpellcheck)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SUBSCRIPT, OnUpdateEditSubscript)
	ON_UPDATE_COMMAND_UI(ID_EDIT_SUPERSCRIPT, OnUpdateEditSuperscript)
	ON_EN_CHANGE(RTF_CONTROL, OnChangeText)
	ON_EN_KILLFOCUS(RTF_CONTROL, OnKillFocus)
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_WM_STYLECHANGING()
	ON_REGISTERED_MESSAGE(WM_UREN_CUSTOMURL, OnCustomUrl)
	ON_REGISTERED_MESSAGE(WM_UREN_FAILEDURL, OnFailedUrl)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRTFContentControl message handlers

void CRTFContentControl::OnChangeText() 
{
	if (m_bAllowNotify && !m_rtf.IsIMEComposing())
		GetParent()->SendMessage(WM_TDCN_COMMENTSCHANGE);
}

void CRTFContentControl::OnKillFocus() 
{
	if (m_bAllowNotify)
		GetParent()->SendMessage(WM_TDCN_COMMENTSKILLFOCUS);
}

LRESULT CRTFContentControl::OnSetFont(WPARAM wp, LPARAM lp)
{
	// richedit2.0 sends a EN_CHANGE notification if it contains
	// text when it receives a font change.
	// to us though this is a bogus change so we prevent a notification
	// being sent
	CAutoFlag af(m_bAllowNotify, FALSE);

	return CRulerRichEditCtrl::OnSetFont(wp, lp);
}

// ICustomControl implementation
int CRTFContentControl::GetContent(unsigned char* pContent) const
{
	return GetContent(this, pContent);
}

// hack to get round GetRTF not being const
int CRTFContentControl::GetContent(const CRTFContentControl* pCtrl, unsigned char* pContent)
{
	int nLen = 0;
	
	if (pContent)
	{
		CString sContent;
		
		// cast away constness
		sContent = ((CRTFContentControl*)pCtrl)->GetRTF();
		nLen = sContent.GetLength();
		
		// compress it
		if (!nLen)
			return 0;

		unsigned char* pCompressed = NULL;
		int nLenCompressed = 0;

		if (Compression::Compress((unsigned char*)(LPSTR)(LPCTSTR)sContent, nLen, pCompressed, nLenCompressed) && nLenCompressed)
		{
			CopyMemory(pContent, pCompressed, nLenCompressed);
			nLen = nLenCompressed;
			delete [] pCompressed;
		}
		else
			nLen = 0;
	}
	else
		nLen = ((CRTFContentControl*)pCtrl)->GetRTFLength();
	
	return nLen;
}

bool CRTFContentControl::SetContent(const unsigned char* pContent, int nLength, BOOL bResetSelection)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	unsigned char* pDecompressed = NULL;

	// content may need decompressing 
	// always work in bytes
	if (nLength && strncmp((const char*)pContent, RTFTAG, LENTAG) != 0)
	{
		int nLenDecompressed = 0;

		if (Compression::Decompress(pContent, nLength, pDecompressed, nLenDecompressed))
		{
			pContent = pDecompressed;
			nLength = nLenDecompressed;
		}
		else
			return false;
	}

	// content must begin with rtf tag or be empty
	if (nLength && (nLength < LENTAG || strncmp((const char*)pContent, RTFTAG, LENTAG)))
		return false;

	CAutoFlag af(m_bAllowNotify, FALSE);
	CReSaveCaret* pSave = NULL;

	// save caret position
	if (!bResetSelection)
		pSave = new CReSaveCaret(m_rtf);

	CString sContent;

#ifdef _UNICODE
	CBinaryData(pContent, nLength).Get(sContent);
#else
	memcpy(sContent.GetBufferSetLength(nLength), pContent, nLength);
#endif

	SetRTF(sContent);
	m_rtf.ParseAndFormatText(TRUE);

	delete [] pDecompressed;
	
	// restore caret
	if (!bResetSelection)
		delete pSave;
	else // or reset
		m_rtf.SetSel(0, 0);

	return true; 
}

int CRTFContentControl::GetTextContent(LPTSTR szContent, int nLength) const
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	if (!szContent)
		return m_rtf.GetWindowTextLength();

	ASSERT(nLength > 0);
	
	return m_rtf.GetWindowText(szContent, nLength);
}

bool CRTFContentControl::SetTextContent(LPCTSTR szContent, BOOL bResetSelection)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	CAutoFlag af(m_bAllowNotify, TRUE);
	CReSaveCaret* pSave = NULL;

	// save caret position
	if (!bResetSelection)
		pSave = new CReSaveCaret(m_rtf);

	m_rtf.SendMessage(WM_SETTEXT, 0, (LPARAM)szContent);

	// restore caret
	if (!bResetSelection)
		delete pSave;
	else // or reset
		m_rtf.SetSel(0, 0);

	return true; 
}

void CRTFContentControl::SetUITheme(const UITHEME* pTheme)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
 	if (FileMisc::FileExists(pTheme->szToolbarImage))
 	{
 		m_toolbar.DestroyWindow();
 		m_toolbar.Create(this, pTheme->szToolbarImage, pTheme->crToolbarTransparency);
 	}

	m_toolbar.SetBackgroundColors(pTheme->crToolbarLight, pTheme->crToolbarDark, pTheme->nRenderStyle != UIRS_GLASS, pTheme->nRenderStyle != UIRS_GRADIENT);
	m_ruler.SetBackgroundColor(pTheme->crToolbarLight);
}

HWND CRTFContentControl::GetHwnd() const
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	
	return GetSafeHwnd();
}

LPCTSTR CRTFContentControl::GetTypeID() const
{
	static CString sID;

	Misc::GuidToString(RTF_TYPEID, sID);

	return sID;
}

void CRTFContentControl::SetReadOnly(bool bReadOnly)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	CRulerRichEditCtrl::SetReadOnly((BOOL)bReadOnly);
}

void CRTFContentControl::Release()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	DestroyWindow();
	delete this;
}

void CRTFContentControl::EnableMenuItem(CMenu* pMenu, UINT nCmdID, BOOL bEnable)
{
	pMenu->EnableMenuItem(nCmdID, MF_BYCOMMAND | (bEnable ? MF_ENABLED : MF_GRAYED));
}

void CRTFContentControl::CheckMenuItem(CMenu* pMenu, UINT nCmdID, BOOL bCheck)
{
	pMenu->CheckMenuItem(nCmdID, MF_BYCOMMAND | (bCheck ? MF_CHECKED  : MF_UNCHECKED));
}

BOOL CRTFContentControl::RemoveAdvancedFeatures(CMenu* pMenu)
{
	BOOL bRemoveAdvancedFeatures = ((FileMisc::GetModuleDriveType() == DRIVE_FIXED) && 
									!CMSWordHelper::IsWordInstalled(12));

	if (bRemoveAdvancedFeatures)
	{
		CEnMenu::DeleteMenu(*pMenu, ID_EDIT_SUPERSCRIPT, MF_BYCOMMAND, TRUE);
		CEnMenu::DeleteMenu(*pMenu, ID_EDIT_SUBSCRIPT, MF_BYCOMMAND, TRUE);
		CEnMenu::DeleteMenu(*pMenu, ID_EDIT_INSERTTABLE, MF_BYCOMMAND, TRUE);
		CEnMenu::DeleteMenu(*pMenu, ID_EDIT_HORZRULE, MF_BYCOMMAND, TRUE);
	}

	return bRemoveAdvancedFeatures;
}

void CRTFContentControl::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	if (pWnd == &m_rtf)
	{
		// prepare a simple edit menu
		CMenu menu;

		if (menu.LoadMenu(IDR_POPUP))
		{
			CMenu* pPopup = menu.GetSubMenu(0);

			if (pPopup)
			{
				RemoveAdvancedFeatures(pPopup);

				// check pos
				if (point.x == -1 && point.y == -1)
				{
					point = GetCaretPos();
					::ClientToScreen(m_rtf, &point);
				}

				CToolbarHelper::PrepareMenuItems(pPopup, this);
				pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
			}
		}
	}
}

void CRTFContentControl::InitShortcutManager()
{
	if (!m_mgrShortcuts.Initialize(this))
		return;

	m_mgrShortcuts.AddShortcut(ID_EDIT_COPYFORMATTING,	'C', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_PASTEFORMATTING,	'V', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_PASTESIMPLE,		'P', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
	m_mgrShortcuts.AddShortcut(ID_EDIT_OUTDENT,			'J', HOTKEYF_CONTROL | HOTKEYF_SHIFT); 

	m_mgrShortcuts.AddShortcut(ID_EDIT_COPY,		'C', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_PASTE,		'V', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_CUT,			'X', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_SELECT_ALL,	'A', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_BOLD,		'B', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_ITALIC,		'I', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_UNDERLINE,	'U', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_LEFTALIGN,	'L', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_CENTERALIGN, 'E', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_RIGHTALIGN,	'R', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_STRIKETHRU,	0xBD, HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_GROWFONT,	0xBE, HOTKEYF_CONTROL | HOTKEYF_EXT); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_SHRINKFONT,	0xBC, HOTKEYF_CONTROL | HOTKEYF_EXT); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_FINDREPLACE, 'H', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_FIND,		'F', HOTKEYF_CONTROL); 
	m_mgrShortcuts.AddShortcut(ID_EDIT_INDENT,		'J', HOTKEYF_CONTROL); 
}

void CRTFContentControl::InitMenuIconManager()
{
	if (!m_mgrMenuIcons.Initialize(this))
		return;
	
	m_mgrMenuIcons.ClearImages();
	
	CUIntArray aCmdIDs;
	
	aCmdIDs.Add(ID_EDIT_FONT);
	aCmdIDs.Add(ID_EDIT_BOLD);
	aCmdIDs.Add(ID_EDIT_ITALIC);
	aCmdIDs.Add(ID_EDIT_UNDERLINE);
	aCmdIDs.Add(ID_EDIT_STRIKETHRU);
	aCmdIDs.Add(ID_EDIT_GROWFONT);
	aCmdIDs.Add(ID_EDIT_SHRINKFONT);
	aCmdIDs.Add(ID_EDIT_LEFTALIGN);
	aCmdIDs.Add(ID_EDIT_CENTERALIGN);
	aCmdIDs.Add(ID_EDIT_RIGHTALIGN);
	aCmdIDs.Add(ID_EDIT_JUSTIFY);
	aCmdIDs.Add(ID_EDIT_BULLET);
	aCmdIDs.Add(ID_EDIT_NUMBER);
	aCmdIDs.Add(ID_EDIT_OUTDENT);
	aCmdIDs.Add(ID_EDIT_INDENT);
	aCmdIDs.Add(ID_EDIT_INSERTTABLE);
	aCmdIDs.Add(ID_EDIT_TEXTCOLOR);
	aCmdIDs.Add(ID_EDIT_BACKCOLOR);
	aCmdIDs.Add(ID_EDIT_WORDWRAP);
		
	m_mgrMenuIcons.AddImages(aCmdIDs, IDB_TOOLBAR, 16, RGB(255, 0, 255));
}

BOOL CRTFContentControl::IsTDLClipboardEmpty() const 
{ 
	// try for any clipboard first
	ITaskList* pClipboard = (ITaskList*)GetParent()->SendMessage(WM_TDCM_GETCLIPBOARD, 0, FALSE);
	ITaskList4* pClip4 = GetITLInterface<ITaskList4>(pClipboard, IID_TASKLIST4);

	if (pClip4)
		return (pClipboard->GetFirstTask() == NULL);

	// else try for 'our' clipboard only
	return (!GetParent()->SendMessage(WM_TDCM_HASCLIPBOARD, 0, TRUE)); 
}

BOOL CRTFContentControl::Paste(BOOL bSimple)
{
	if (Misc::ClipboardHasFormat(CF_HDROP, GetSafeHwnd()))
	{
		if (::OpenClipboard(GetSafeHwnd()))
		{
			HANDLE hData = ::GetClipboardData(CF_HDROP);
			ASSERT(hData);

			CStringArray aFiles;
			int nNumFiles = FileMisc::GetDropFilePaths((HDROP)hData, aFiles);

			::CloseClipboard();

			if (nNumFiles > 0)
			{
				if (bSimple)
				{
					return CRichEditHelper::PasteFiles(m_rtf, aFiles, REP_ASFILEURL);
				}
				else
				{
					if (!m_rtf.IsFileLinkOptionDefault())
					{
						CCreateFileLinkDlg dialog(aFiles[0], m_rtf.GetFileLinkOption(), FALSE);

						if (dialog.DoModal() == IDOK)
						{
							RE_PASTE nLinkOption = dialog.GetLinkOption();
							BOOL bDefault = dialog.GetMakeLinkOptionDefault();

							m_rtf.SetFileLinkOption(nLinkOption, bDefault);
						}
						else
							return FALSE; // cancelled
					}

					return CRichEditHelper::PasteFiles(m_rtf, aFiles, m_rtf.GetFileLinkOption());
				}
			}
			else if (nNumFiles == -1) // error
			{
				AfxMessageBox(IDS_PASTE_ERROR, MB_OK | MB_ICONERROR);
				return FALSE;
			}
		}
	}

	// else
	m_rtf.Paste(bSimple);
	return TRUE;
}

BOOL CRTFContentControl::CanPaste()
{
	static CLIPFORMAT formats[] = 
	{ 
		(CLIPFORMAT)::RegisterClipboardFormat(CF_RTF),
		(CLIPFORMAT)::RegisterClipboardFormat(CF_RETEXTOBJ), 
		(CLIPFORMAT)::RegisterClipboardFormat(_T("Embedded Object")),
		(CLIPFORMAT)::RegisterClipboardFormat(_T("Rich Text Format")),
		CF_BITMAP,

#ifndef _UNICODE
		CF_TEXT,
#else
		CF_UNICODETEXT,
#endif
		CF_DIB,
		CF_HDROP
	};

	// for reasons that I'm not entirely clear on even if we 
	// return that CF_HDROP is okay, the richedit itself will
	// veto the drop. So I'm experimenting with handling this ourselves
	if (Misc::ClipboardHasFormat(CF_HDROP, GetSafeHwnd()))
		return TRUE;

	// else try richedit itself
	const long formats_count = sizeof(formats) / sizeof(CLIPFORMAT);
	BOOL bCanPaste = FALSE;
	
	for( long i=0;  i<formats_count;  ++i )
		bCanPaste |= m_rtf.CanPaste( formats[i] );

	if (!bCanPaste)
		bCanPaste = m_rtf.CanPaste(0);
	
	return bCanPaste;
}

int CRTFContentControl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	CAutoFlag af(m_bAllowNotify, FALSE);
	
	if (CRulerRichEditCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_rtf.SetEventMask(m_rtf.GetEventMask() | ENM_CHANGE);
	
	// set max edit length
	m_rtf.LimitText(1024 * 1024 * 1024); // one gigabyte
	
	InitMenuIconManager();
	InitShortcutManager();

	// if MS word is not installed we remove features that
	// our backup RTF2HTML converter cannot handle
	BOOL bRemoveAdvancedFeatures = !CMSWordHelper::IsWordInstalled(12);

	if (bRemoveAdvancedFeatures)
	{
		m_toolbar.GetToolBarCtrl().HideButton(ID_EDIT_INSERTTABLE);
	}

	// helper for toolbar tooltips
	// initialize after hiding table button
	m_tbHelper.Initialize(&m_toolbar, this);

	return 0;
}

bool CRTFContentControl::ProcessMessage(MSG* pMsg) 
{
	if (!IsWindowEnabled())
		return false;

	// process editing shortcuts
	if (m_mgrShortcuts.ProcessMessage(pMsg))
		return true;

	// else custom handling
	switch (pMsg->message)
	{
	case WM_KEYDOWN:
		{
			AFX_MANAGE_STATE(AfxGetStaticModuleState());

			BOOL bCtrl = Misc::KeyIsPressed(VK_CONTROL);
			BOOL bShift = Misc::KeyIsPressed(VK_SHIFT);
			BOOL bAlt = Misc::KeyIsPressed(VK_MENU);
			BOOL bEnabled = !GetReadOnly();

			// most shortcuts are handled by the shortcut manager
			if (bCtrl && !bAlt && !bShift)
			{
				switch (pMsg->wParam)
				{
				case 'z':
				case 'Z':
					return true; // to prevent the richedit performing the undo
						
				case 'y':
				case 'Y':
					return true; // to prevent the richedit performing the redo
				}
			}
			else if (bEnabled && pMsg->wParam == VK_TAB)
			{
				CHARRANGE cr;
				m_rtf.GetSel(cr);
				
				// if nothing is selected then just insert tabs
				if (cr.cpMin == cr.cpMax)
					m_rtf.ReplaceSel(_T("\t"), TRUE);
				else
				{
					if (!bShift)
						DoIndent();
					else
						DoOutdent();
				}
				
				return true;
			}
		}
		break;
	}

	return false;
}

void CRTFContentControl::SavePreferences(IPreferences* pPrefs, LPCTSTR szKey) const
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	pPrefs->WriteProfileInt(szKey, _T("ShowToolbar"), IsToolbarVisible());
	pPrefs->WriteProfileInt(szKey, _T("ShowRuler"), IsRulerVisible());
	pPrefs->WriteProfileInt(szKey, _T("WordWrap"), HasWordWrap());

	pPrefs->WriteProfileInt(szKey, _T("FileLinkOption"), m_rtf.GetFileLinkOption());
	pPrefs->WriteProfileInt(szKey, _T("FileLinkOptionIsDefault"), m_rtf.IsFileLinkOptionDefault());
}

void CRTFContentControl::LoadPreferences(const IPreferences* pPrefs, LPCTSTR szKey)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	BOOL bShowToolbar = pPrefs->GetProfileInt(szKey, _T("ShowToolbar"), m_showToolbar);
	BOOL bShowRuler = pPrefs->GetProfileInt(szKey, _T("ShowRuler"), m_showRuler);
	BOOL bWordWrap = pPrefs->GetProfileInt(szKey, _T("WordWrap"), TRUE);
	
	ShowToolbar(bShowToolbar);
	ShowRuler(bShowRuler);
	SetWordWrap(bWordWrap);

	RE_PASTE nLinkOption = (RE_PASTE)pPrefs->GetProfileInt(szKey, _T("FileLinkOption"), REP_ASIMAGE);
	BOOL bLinkOptionIsDefault = pPrefs->GetProfileInt(szKey, _T("FileLinkOptionIsDefault"), FALSE);

	m_rtf.SetFileLinkOption(nLinkOption, bLinkOptionIsDefault);
}

void CRTFContentControl::OnStyleChanging(int nStyleType, LPSTYLESTRUCT lpStyleStruct)
{
	if (nStyleType == GWL_EXSTYLE && (lpStyleStruct->styleNew & WS_EX_CLIENTEDGE))
		lpStyleStruct->styleNew &= ~WS_EX_CLIENTEDGE;

	CRulerRichEditCtrl::OnStyleChanging(nStyleType, lpStyleStruct);
}

LRESULT CRTFContentControl::OnCustomUrl(WPARAM wp, LPARAM lp)
{
	UNREFERENCED_PARAMETER(wp);
	ASSERT (wp == RTF_CONTROL);

	CString sUrl((LPCTSTR)lp);
	sUrl.MakeLower();

	if (sUrl.Find(TDL_PROTOCOL) != -1 || sUrl.Find(TDL_EXTENSION) != -1)
		return GetParent()->SendMessage(WM_TDCM_TASKLINK, 0, lp);

	return 0;
}

LRESULT CRTFContentControl::OnFailedUrl(WPARAM wp, LPARAM lp)
{
	ASSERT (wp == RTF_CONTROL);
	return GetParent()->SendMessage(WM_TDCM_FAILEDLINK, wp, lp);
}

void CRTFContentControl::OnEditCopy() 
{
	m_rtf.Copy();	
}

void CRTFContentControl::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.HasSelection());
}

void CRTFContentControl::OnEditCopyFormatting() 
{
	CharFormat cf(CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT | 
					CFM_COLOR | CFM_BACKCOLOR | CFM_SUBSCRIPT | CFM_SUPERSCRIPT);

	m_rtf.GetSelectionCharFormat(m_cfCopiedFormat);
}

void CRTFContentControl::OnUpdateEditCopyFormatting(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.GetTextLength() && m_rtf.HasSelection());
}

void CRTFContentControl::OnEditCut() 
{
	m_rtf.Cut();	
}

void CRTFContentControl::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && m_rtf.HasSelection());
}

void CRTFContentControl::OnEditDelete() 
{
	m_rtf.ReplaceSel(_T(""), TRUE);
}

void CRTFContentControl::OnUpdateEditDelete(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && m_rtf.HasSelection());
}

void CRTFContentControl::OnEditFileBrowse() 
{
	int nUrl = m_rtf.GetContextUrl();
	CString sFile;
	
	if (nUrl != -1)
	{
		sFile = m_rtf.GetUrl(nUrl, TRUE);

		if (!FileMisc::FileExists(sFile))
			sFile.Empty();
	}
				
	CFileOpenDialog dialog(CEnString(IDS_BROWSE_TITLE), NULL, sFile, EOFN_DEFAULTOPEN);
	
	if (dialog.DoModal() == IDOK)
	{
		CString sFile = dialog.GetPathName();

		if (!m_rtf.IsFileLinkOptionDefault())
		{
			CCreateFileLinkDlg dialog2(sFile, m_rtf.GetFileLinkOption(), FALSE);

			if (dialog2.DoModal() == IDOK)
			{
				RE_PASTE nLinkOption = dialog2.GetLinkOption();
				BOOL bDefault = dialog2.GetMakeLinkOptionDefault();
				
				m_rtf.SetFileLinkOption(nLinkOption, bDefault);
			}
			else
			{
				return; // user cancelled
			}
		}

		CRichEditHelper::PasteFile(m_rtf, sFile, m_rtf.GetFileLinkOption());
	}
}

void CRTFContentControl::OnUpdateEditFileBrowse(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit());
}

void CRTFContentControl::OnEditFind() 
{
	m_rtf.DoEditFind(IDS_FIND_TITLE);
}

void CRTFContentControl::OnUpdateEditFind(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.GetTextLength());
}

void CRTFContentControl::OnEditFindReplace() 
{
	m_rtf.DoEditReplace(IDS_REPLACE_TITLE);
}

void CRTFContentControl::OnUpdateEditFindReplace(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && m_rtf.GetTextLength());
}

void CRTFContentControl::OnEditHorzRule() 
{
	DoInsertHorzLine();
}

void CRTFContentControl::OnUpdateEditHorzRule(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit()); 
}


void CRTFContentControl::OnEditOpenUrl() 
{
	int nUrl = m_rtf.GetContextUrl();

	if (nUrl != -1)
		m_rtf.GoToUrl(nUrl);
}

void CRTFContentControl::OnUpdateEditOpenUrl(CCmdUI* pCmdUI) 
{
	int nUrl = m_rtf.GetContextUrl();
	pCmdUI->Enable(nUrl != -1);
	
	if (nUrl != -1)
	{
		CEnString sMenu;
		pCmdUI->m_pMenu->GetMenuString(ID_EDIT_OPENURL, sMenu, MF_BYCOMMAND);
		sMenu.Translate();
		
		// restrict url length to 250 pixels
		CEnString sUrl(m_rtf.GetUrl(nUrl, TRUE));
		CClientDC dc(this);
		sUrl.FormatDC(&dc, 250, ES_PATH);
		
		CString sText;
		sText.Format(_T("%s: %s"), sMenu, sUrl);
		pCmdUI->SetText(sText);
	}
}

void CRTFContentControl::OnEditPaste() 
{
	Paste();
}

void CRTFContentControl::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && CanPaste());
}

void CRTFContentControl::OnEditPasteasRef() 
{
	// try to get the clipboard for any tasklist
	ITaskList* pClipboard = (ITaskList*)GetParent()->SendMessage(WM_TDCM_GETCLIPBOARD, 0, FALSE);

	// verify that we can get the corresponding filename
	CString sFileName;
	ITaskList4* pClip4 = GetITLInterface<ITaskList4>(pClipboard, IID_TASKLIST4);

	if (pClip4)
	{
		sFileName = pClip4->GetAttribute(TDL_FILENAME);
		sFileName.Replace(_T(" "), _T("%20"));
	}
	else // get the clipboard for just this tasklist
		pClipboard = (ITaskList*)GetParent()->SendMessage(WM_TDCM_GETCLIPBOARD, 0, TRUE);

	if (pClipboard && pClipboard->GetFirstTask())
	{
		// build single string containing each top level item as a different ref
		CString sRefs, sRef;
		HTASKITEM hClip = pClipboard->GetFirstTask();
		
		while (hClip)
		{
			if (sFileName.IsEmpty())
				sRef.Format(_T(" %s%d"), TDL_PROTOCOL, pClipboard->GetTaskID(hClip));
			else
				sRef.Format(_T(" %s%s?%d"), TDL_PROTOCOL, sFileName, pClipboard->GetTaskID(hClip));

			sRefs += sRef;
			
			hClip = pClipboard->GetNextTask(hClip);
		}

		sRefs += ' ';
		m_rtf.ReplaceSel(sRefs, TRUE);
	}
}

void CRTFContentControl::OnUpdateEditPasteasRef(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && !IsTDLClipboardEmpty());
}

void CRTFContentControl::OnEditPasteFormatting() 
{
	m_rtf.SetSelectionCharFormat(m_cfCopiedFormat);
}

void CRTFContentControl::OnUpdateEditPasteFormatting(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && m_rtf.GetTextLength());
}

void CRTFContentControl::OnEditPasteSimple() 
{
	Paste(TRUE); // TRUE ==  simple
}

void CRTFContentControl::OnUpdateEditPasteSimple(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && CanPaste());
}

void CRTFContentControl::OnEditSelectAll() 
{
	m_rtf.SetSel(0, -1);
}

void CRTFContentControl::OnUpdateEditSelectAll(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.GetTextLength());
}

void CRTFContentControl::OnEditShowRuler() 
{
	ShowRuler(!IsRulerVisible());
}

void CRTFContentControl::OnUpdateEditShowRuler(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(IsRulerVisible() ? 1 : 0);
}

void CRTFContentControl::OnEditShowToolbar() 
{
	ShowToolbar(!IsToolbarVisible());
}

void CRTFContentControl::OnUpdateEditShowToolbar(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(IsToolbarVisible() ? 1 : 0);
}

void CRTFContentControl::OnEditSpellcheck() 
{
	GetParent()->PostMessage(WM_ICC_WANTSPELLCHECK);
}

void CRTFContentControl::OnUpdateEditSpellcheck(CCmdUI* pCmdUI) 
{
	pCmdUI->Enable(m_rtf.CanEdit() && m_rtf.GetTextLength());
}

void CRTFContentControl::OnEditSubscript() 
{
	DoSubscript();	
}

void CRTFContentControl::OnUpdateEditSubscript(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_rtf.SelectionHasEffect(CFM_SUBSCRIPT, CFE_SUBSCRIPT) ? 1 : 0);
	pCmdUI->Enable(m_rtf.CanEdit());
}

void CRTFContentControl::OnEditSuperscript() 
{
	DoSuperscript();	
}

void CRTFContentControl::OnUpdateEditSuperscript(CCmdUI* pCmdUI) 
{
	pCmdUI->SetCheck(m_rtf.SelectionHasEffect(CFM_SUPERSCRIPT, CFE_SUPERSCRIPT) ? 1 : 0);
	pCmdUI->Enable(m_rtf.CanEdit());
}

void CRTFContentControl::OnPreferences() 
{
	CCreateFileLinkDlg dialog(NULL, m_rtf.GetFileLinkOption(), m_rtf.IsFileLinkOptionDefault(), TRUE);
	
	if (dialog.DoModal() == IDOK)
	{
		RE_PASTE nLinkOption = dialog.GetLinkOption();
		BOOL bDefault = dialog.GetMakeLinkOptionDefault();

		m_rtf.SetFileLinkOption(nLinkOption, bDefault);
	}
}
