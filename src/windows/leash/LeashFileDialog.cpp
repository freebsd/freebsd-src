//	**************************************************************************************
//	File:			LeashFileDialog.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    CPP file for LeashFileDialog.h. Contains variables and functions
//					for the Leash File Dialog Box
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "LeashFileDialog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLeashFileDialog

IMPLEMENT_DYNAMIC(CLeashFileDialog, CFileDialog)



CLeashFileDialog::CLeashFileDialog(BOOL bOpenFileDialog, LPCTSTR lpszDefExt, LPCTSTR lpszFileName,
		LPCTSTR lpszFilter, DWORD dwFlags, CWnd* pParentWnd) :
		CFileDialog(bOpenFileDialog, lpszDefExt, lpszFileName, dwFlags, lpszFilter, pParentWnd)
{
	m_ofn.Flags |= OFN_ENABLETEMPLATE;
	m_ofn.lpTemplateName = MAKEINTRESOURCE(IDD_FILESPECIAL);
	m_ofn.lpstrFilter = lpszFilter;
	m_ofn.lpstrFileTitle = m_lpstrFileTitle;
	m_ofn.nMaxFileTitle = MAX_PATH;
	*m_lpstrFileTitle = 0;
	BOOL m_startup = TRUE;
}


BEGIN_MESSAGE_MAP(CLeashFileDialog, CFileDialog)
	//{{AFX_MSG_MAP(CLeashFileDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CLeashFileDialog::OnInitDialog()
{
	BOOL bRet = CFileDialog::OnInitDialog();
	if (bRet == TRUE)
	{
		GetParent()->GetDlgItem(IDOK)->SetWindowText("&OK");
		//GetParent()->GetDlgItem(IDOK)->EnableWindow(FALSE);
	}

	return bRet;
}

void CLeashFileDialog::OnFileNameChange( )
{
	if (!m_startup)
	{ //' keeps the OK button disabled until a real select is made
		CString testString = GetFileName();
		if (-1 == testString.Find('*'))
		  GetParent()->GetDlgItem(IDOK)->EnableWindow();
	}
	  else
	    m_startup = FALSE;
}
