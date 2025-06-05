//	**************************************************************************************
//	File:			LeashDebugWindow.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for LeashDebugWindow.cpp. Contains variables and functions
//					for the Leash Debug Window
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_LEASHDEBUGWINDOW_H__DB6F7EE8_570E_11D2_9460_0000861B8A3C__INCLUDED_)
#define AFX_LEASHDEBUGWINDOW_H__DB6F7EE8_570E_11D2_9460_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LeashDebugWindow.h
//

/////////////////////////////////////////////////////////////////////////////
// CLeashDebugWindow dialog

#define WM_GOODBYE WM_USER + 5

class CLeashDebugWindow : public CDialog
{
private:
	BOOL m_CopyButton;
	CFormView* m_pView;
	CString m_debugFilePath;

// Construction
public:
	CLeashDebugWindow(CWnd* pParent = NULL);
	CLeashDebugWindow(CFormView* pView);
	BOOL Create(const LPCSTR debugFilePath);


// Dialog Data
	//{{AFX_DATA(CLeashDebugWindow)
	enum { IDD = IDD_LEASH_DEBUG_WINDOW };
	CStatic	m_debugFile;
	CListBox	m_debugListBox;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLeashDebugWindow)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLeashDebugWindow)
	virtual void OnCancel();
	virtual void OnOK();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnCopyToClipboard();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LEASHDEBUGWINDOW_H__DB6F7EE8_570E_11D2_9460_0000861B8A3C__INCLUDED_)
