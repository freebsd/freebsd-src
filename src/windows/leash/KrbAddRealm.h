//	**************************************************************************************
//	File:			KrbAddRealm.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for KrbAddRealm.cpp Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_ADDTOREALMHOSTLIST_H__26A1E1F3_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
#define AFX_ADDTOREALMHOSTLIST_H__26A1E1F3_9117_11D2_94D0_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AddToRealmHostList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrbAddRealm dialog

class CKrbAddRealm : public CDialog
{
// Construction
	CString m_newRealm;
	BOOL m_startup;

public:
	CKrbAddRealm(CWnd* pParent = NULL);   // standard constructor
	CString GetNewRealm() {return m_newRealm;}

// Dialog Data
	//{{AFX_DATA(CKrbAddRealm)
	enum { IDD = IDD_KRB_ADD_REALM };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbAddRealm)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrbAddRealm)
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	virtual void OnOK();
	afx_msg void OnChangeEditRealm();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADDTOREALMHOSTLIST_H__26A1E1F3_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
