//	**************************************************************************************
//	File:			LeashFileDialog.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    H file for LeashFileDialog.cpp. Contains variables and functions
//					for the Leash File Dialog Box
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_LEASHFILEDIALOG_H__E74500E1_6B74_11D2_9448_0000861B8A3C__INCLUDED_)
#define AFX_LEASHFILEDIALOG_H__E74500E1_6B74_11D2_9448_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LeashFileDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLeashFileDialog dialog

class CLeashFileDialog : public CFileDialog
{
	DECLARE_DYNAMIC(CLeashFileDialog)

private:
	CHAR m_lpstrFileTitle[MAX_PATH];
	BOOL m_startup;

public:
	CLeashFileDialog(BOOL bOpenFileDialog, // TRUE for FileOpen, FALSE for FileSaveAs
					 LPCTSTR lpszDefExt = NULL,
					 LPCTSTR lpszFileName = NULL,
					 LPCTSTR lpszFilter = NULL,
                     DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_FILEMUSTEXIST,
					 CWnd* pParentWnd = NULL);

		CString GetSelectedFileName() {return m_lpstrFileTitle;}

protected:
	//{{AFX_MSG(CLeashFileDialog)
	virtual BOOL OnInitDialog();
	virtual void OnFileNameChange( );
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LEASHFILEDIALOG_H__E74500E1_6B74_11D2_9448_0000861B8A3C__INCLUDED_)
