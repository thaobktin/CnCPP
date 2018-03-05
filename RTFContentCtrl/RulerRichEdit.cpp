/* ==========================================================================
File :			RuleRichEdit.cpp

  Class :			CRulerRichEdit
  
	Author :		Johan Rosengren, Abstrakt Mekanik AB
	Iain Clarke
	
	  Date :			2004-04-17
	  
		Purpose :		"CRulerRichEdit" is derived from "CWnd". 
		
		  Description :	The class, in addition to the normal "CWnd", 
		  handles horizontal scrollbar messages - forcing an 
		  update of the parent (to synchronize the ruler). The 
		  change notification is called for the same reason. 
		  "WM_GETDLGCODE" is handled, we want all keys in a 
		  dialog box instantiation.
		  
			Usage :			This class is only useful as a child of the 
			"CRulerRichEditCtrl".
			
========================================================================*/

#include "stdafx.h"
#include "resource.h"
#include "RulerRichEdit.h"
#include "createfilelinkdlg.h"

#include "..\shared\RichEdithelper.h"
#include "..\shared\autoflag.h"
#include "..\shared\enbitmap.h"
#include "..\shared\misc.h"
#include "..\shared\filemisc.h"
#include "..\shared\subclass.h"
#include "..\shared\HookMgr.h"
#include "..\shared\outlookhelper.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFindReplaceDialogEx


// for some reason (that I cannot divine) dialog keyboard handling 
// is not working so we install a keyboard hook and handle

class CFindReplaceDialogEx : public CFindReplaceDialog, public CHookMgr<CFindReplaceDialogEx>  
{
   friend class CHookMgr<CFindReplaceDialogEx>;

public:
	virtual ~CFindReplaceDialogEx() {}
	static CFindReplaceDialogEx& Instance() { return CHookMgr<CFindReplaceDialogEx>::GetInstance(); }

protected:
	CFindReplaceDialogEx() {}

	virtual void PostNcDestroy() { ReleaseHooks(); }
	virtual BOOL OnInitDialog();
	virtual BOOL OnKeyboard(UINT uVirtKey, UINT /*uFlags*/);
};

BOOL CFindReplaceDialogEx::OnInitDialog()
{
	CFindReplaceDialog::OnInitDialog();

	InitHooks(HM_KEYBOARD);
	return TRUE;
}

BOOL CFindReplaceDialogEx::OnKeyboard(UINT uVirtKey, UINT uFlags)
{
	// only handle messages belonging to us
	HWND hFocus = ::GetFocus();

	if (::IsChild(*this, hFocus) && Misc::KeyIsPressed(uVirtKey))
	{
		MSG msg = { hFocus, WM_KEYDOWN, uVirtKey, uFlags, 0, {0, 0} };

		if (::IsDialogMessage(*this, &msg))
			return TRUE;
	}
	
	// all else
	return FALSE;
}

CFindReplaceDialog* CRulerRichEdit::NewFindReplaceDlg()
{
	// because we reuse the same dialog we must clear the terminating flag
	CFindReplaceDialog& dialog = CFindReplaceDialogEx::Instance();
	dialog.m_fr.Flags &= ~FR_DIALOGTERM;

	return &dialog;
}

/////////////////////////////////////////////////////////////////////////////
// CRulerRichEdit

CRulerRichEdit::CRulerRichEdit() : m_bPasteSimple(FALSE), m_bIMEComposing(FALSE), m_nFileLinkOption(REP_ASIMAGE)
{
	EnableSelectOnFocus(FALSE);
}

CRulerRichEdit::~CRulerRichEdit()
{
}

BEGIN_MESSAGE_MAP(CRulerRichEdit, CUrlRichEditCtrl)
//{{AFX_MSG_MAP(CRulerRichEdit)
	ON_WM_HSCROLL()
	ON_WM_GETDLGCODE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CREATE()
	//}}AFX_MSG_MAP
	ON_MESSAGE(WM_DROPFILES, OnDropFiles)
	ON_MESSAGE(WM_IME_STARTCOMPOSITION, OnIMEStartComposition)
	ON_MESSAGE(WM_IME_ENDCOMPOSITION, OnIMEEndComposition)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRulerRichEdit message handlers

void CRulerRichEdit::SetFileLinkOption(RE_PASTE nLinkOption, BOOL bDefault) 
{ 
	m_nFileLinkOption = nLinkOption; 
	m_bLinkOptionIsDefault = bDefault;
}

int CRulerRichEdit::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	return CUrlRichEditCtrl::OnCreate(lpCreateStruct);
}

void CRulerRichEdit::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	
	CUrlRichEditCtrl::OnHScroll( nSBCode, nPos, pScrollBar );
	
	if ( nSBCode == SB_THUMBTRACK )
	{
		SCROLLINFO	si;
		ZeroMemory( &si, sizeof( SCROLLINFO ) );
		si.cbSize = sizeof( SCROLLINFO );
		GetScrollInfo( SB_HORZ, &si );
		
		si.nPos = nPos;
		SetScrollInfo( SB_HORZ, &si );
		
		// notify parent
		GetParent()->SendMessage(WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(), EN_HSCROLL), (LPARAM)GetSafeHwnd());
	}
}

LRESULT CRulerRichEdit::OnDropFiles(WPARAM wp, LPARAM /*lp*/) 
{
	CWaitCursor cursor;
	CStringArray aFiles;
	
	int nNumFiles = FileMisc::GetDropFilePaths((HDROP)wp, aFiles);

	::CloseClipboard();

	if (nNumFiles > 0)
	{
		if (!m_bLinkOptionIsDefault)
		{
			CCreateFileLinkDlg dialog(aFiles[0], m_nFileLinkOption, FALSE);

			if (dialog.DoModal() != IDOK)
				return 0L;

			m_nFileLinkOption = dialog.GetLinkOption();
			m_bLinkOptionIsDefault = dialog.GetMakeLinkOptionDefault();
		}

		return CRichEditHelper::PasteFiles(*this, aFiles, m_nFileLinkOption);
	}
	else if (nNumFiles == -1) // error
	{
		AfxMessageBox(IDS_PASTE_ERROR, MB_OK | MB_ICONERROR);
	}

	// else
	return 0L;
}

HRESULT CRulerRichEdit::GetDragDropEffect(BOOL fDrag, DWORD grfKeyState, LPDWORD pdwEffect)
{
	if (!fDrag) // allowable dest effects
	{
		BOOL bEnable = !(GetStyle() & ES_READONLY) && IsWindowEnabled();
		
		if (bEnable)
		{
			BOOL bFileDrop = (!COutlookHelper::IsOutlookObject(m_lpDragObject) &&
								Misc::HasFlag(*pdwEffect, DROPEFFECT_LINK));
			
			if (bFileDrop)
			{
				DWORD dwEffect = DROPEFFECT_NONE;
				BOOL bShift = Misc::HasFlag(grfKeyState, MK_SHIFT);

				// if SHIFT is down then show this as a copy because we are embedding
				dwEffect = (DROPEFFECT_MOVE | (bShift ? DROPEFFECT_COPY : 0));
				
				TrackDragCursor();
				
				// make sure allowed type
				if ((dwEffect & *pdwEffect) == dwEffect) 
					*pdwEffect = dwEffect;

				return S_OK;
			}
		}
	}

	// all else
	return CUrlRichEditCtrl::GetDragDropEffect(fDrag, grfKeyState, pdwEffect);
}

CLIPFORMAT CRulerRichEdit::GetAcceptableClipFormat(LPDATAOBJECT lpDataOb, CLIPFORMAT format) 
{ 
	if (m_bPasteSimple)
#ifndef _UNICODE
		return CF_TEXT;
#else
		return CF_UNICODETEXT;
#endif
	
	static CLIPFORMAT cfRtf = (CLIPFORMAT)::RegisterClipboardFormat(CF_RTF);
	static CLIPFORMAT cfRtfObj = (CLIPFORMAT)::RegisterClipboardFormat(CF_RETEXTOBJ); 
	
	CLIPFORMAT formats[] = 
	{ 
		COutlookHelper::CF_OUTLOOK,
		CF_HDROP,
		cfRtf,
		cfRtfObj, 
		CF_BITMAP,

#ifndef _UNICODE
		CF_TEXT,
#else
		CF_UNICODETEXT,
#endif
		CF_METAFILEPICT,    
		CF_SYLK,            
		CF_DIF,             
		CF_TIFF,            
		CF_OEMTEXT,         
		CF_DIB,             
		CF_PALETTE,         
		CF_PENDATA,         
		CF_RIFF,            
		CF_WAVE,            
		CF_ENHMETAFILE
	};
	
	const long nNumFmts = sizeof(formats) / sizeof(CLIPFORMAT);
	
	COleDataObject dataobj;
    dataobj.Attach(lpDataOb, FALSE);
    
	for (int nFmt = 0; nFmt < nNumFmts; nFmt++)
	{
		if (format && format == formats[nFmt])
			return format;
		
		if (dataobj.IsDataAvailable(formats[nFmt]))
			return formats[nFmt];
	}
	
	// all else
	return CF_HDROP; 
}

UINT CRulerRichEdit::OnGetDlgCode() 
{
	return DLGC_WANTALLKEYS;
}

void CRulerRichEdit::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));
	
	CUrlRichEditCtrl::OnLButtonDblClk(nFlags, point);
}

void CRulerRichEdit::Paste(BOOL bSimple)
{
	if (!bSimple)
		CUrlRichEditCtrl::Paste();
	else
	{
		CAutoFlag af(m_bPasteSimple, TRUE);
		CUrlRichEditCtrl::Paste();
	}
}

LRESULT CRulerRichEdit::OnIMEStartComposition(WPARAM /*wp*/, LPARAM /*lp*/)
{
	// There's a bug where I'm not receiving the 'end composition' 
	// notification resulting in text changes not triggering a change
	// notification to the app.
	//m_bIMEComposing = TRUE;

	return Default();
}

LRESULT CRulerRichEdit::OnIMEEndComposition(WPARAM /*wp*/, LPARAM /*lp*/)
{
	m_bIMEComposing = FALSE;

	return Default();
}

