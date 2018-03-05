// EnBitmap.h: interface for the CEnBitmap class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_ENBITMAP_H__1FDE0A4E_8AB4_11D6_95AD_EFA89432A428__INCLUDED_)
#define AFX_ENBITMAP_H__1FDE0A4E_8AB4_11D6_95AD_EFA89432A428__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "colordef.h"
#include <afxtempl.h>

/////////////////////////////////////////////////////////////////////////////////////
// base class for image processing

class C32BitImageProcessor
{
public:
	C32BitImageProcessor(BOOL bEnableWeighting = FALSE);
	virtual ~C32BitImageProcessor();

	virtual CSize CalcDestSize(CSize sizeSrc);
	virtual BOOL ProcessPixels(RGBX* pSrcPixels, CSize sizeSrc, RGBX* pDestPixels, CSize sizeDest, 
								COLORREF crMask = -1);

	void CalcWeightedColor(RGBX* pPixels, CSize size, double dX, double dY, RGBX& rgbResult);

protected:
	BOOL m_bWeightingEnabled;
};

typedef CArray<C32BitImageProcessor*, C32BitImageProcessor*> C32BIPArray;

//////////////////////////////////////////////////////////////////////////////////////////

enum EB_IMAGETYPE
{
	FT_BMP,
	FT_ICO,
	FT_JPG,
	FT_GIF,

#ifdef PNG_SUPPORT
	FT_PNG,
#endif

	FT_UNKNOWN
};

class CEnBitmap : public CBitmap  
{
public:
	CEnBitmap(COLORREF crBkgnd = RGB(255, 255, 255));
	CEnBitmap(int cx, int cy, COLORREF crBkgnd = RGB(255, 255, 255));
	virtual ~CEnBitmap();

	BOOL LoadImage(LPCTSTR szImagePath, COLORREF crBack = -1);
	BOOL LoadImage(UINT uIDRes, LPCTSTR szResourceType, HMODULE hInst = NULL, COLORREF crBack = -1); 
	BOOL CopyImage(HBITMAP hBitmap);
	BOOL CopyImage(CBitmap* pBitmap);
	BOOL Copy(HIMAGELIST hImageList);

	static BOOL CopyImageFileToClipboard(LPCTSTR szImagePath, COLORREF crBack = -1);
	BOOL CopyToClipboard(HWND hWnd = NULL) const;

	BOOL ProcessImage(C32BitImageProcessor* pProcessor, COLORREF crMask = -1);
	BOOL ProcessImage(C32BIPArray& aProcessors, COLORREF crMask = -1); // ordered list of processors

	// helpers
	static HBITMAP LoadImageFile(LPCTSTR szImagePath, COLORREF crBack = -1, int cx = 0, int cy = 0);
	static HBITMAP LoadImageResource(UINT uIDRes, LPCTSTR szResourceType, HMODULE hInst = NULL, COLORREF crBack = -1, int cx = 0, int cy = 0); 
	static BOOL GetResource(LPCTSTR lpName, LPCTSTR lpType, HMODULE hInst, void* pResource, int& nBufSize);
	static IPicture* LoadFromBuffer(BYTE* pBuff, int nSize);
	static EB_IMAGETYPE GetFileType(LPCTSTR szImagePath);

protected:
	COLORREF m_crBkgnd;
	CSize m_sizeDest;

protected:
	RGBX* GetDIBits32();
	BOOL PrepareBitmapInfo32(BITMAPINFO& bi, HBITMAP hBitmap = NULL);

	static BOOL Fill(RGBX* pPixels, CSize size, COLORREF color);
	static HBITMAP ExtractBitmap(IPicture* pPicture, COLORREF crBack, int cx, int cy);
	static HBITMAP ExtractBitmap(HICON hIcon, COLORREF crBack, int cx, int cy);
	static HBITMAP ExtractBitmap(const CImageList& il, COLORREF crBack, int cx, int cy);

#ifdef PNG_SUPPORT
	static HBITMAP ExtractBitmap(HBITMAP hbmGdip, COLORREF crBack, int cx, int cy);
#endif
};

#endif // !defined(AFX_ENBITMAP_H__1FDE0A4E_8AB4_11D6_95AD_EFA89432A428__INCLUDED_)
