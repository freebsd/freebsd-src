//	**************************************************************************************
//	File:			LeashMessageBox.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    H file for LeashMessageBox.cpp. Contains variables and functions
//					for the Leash Special Message Dialog Box
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_LEASHMESSAGEBOX_H__865865B6_56F6_11D2_945F_0000861B8A3C__INCLUDED_)
#define AFX_LEASHMESSAGEBOX_H__865865B6_56F6_11D2_945F_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LeashMessageBox.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLeashMessageBox dialog

#include "windows.h"

class CLeashMessageBox : public CDialog
{
private:
	static DWORD m_dwTime;
	static void CALLBACK MessageBoxTimer(HWND hwnd, UINT uiMsg, UINT_PTR idEvent, DWORD dwTime);

	// Construction
public:
	CLeashMessageBox(CWnd* pParent = NULL,            const CString msgText = "Place your message here!!!",
					 DWORD dwTime = 0);
	~CLeashMessageBox();

// Dialog Data
	//{{AFX_DATA(CLeashMessageBox)
	enum { IDD = IDD_MESSAGE_BOX };
	CString	m_messageText;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLeashMessageBox)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLeashMessageBox)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LEASHMESSAGEBOX_H__865865B6_56F6_11D2_945F_0000861B8A3C__INCLUDED_)
