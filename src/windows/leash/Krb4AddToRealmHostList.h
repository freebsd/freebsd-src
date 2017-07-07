//	**************************************************************************************
//	File:			Krb4AddToRealmHostList.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Krb4AddToRealmHostList.cpp Contains variables and functions
//					for Kerberos Four Properties
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
// CKrb4AddToRealmHostList dialog

class CKrb4AddToRealmHostList : public CDialog
{
// Construction
	CString m_newRealm;
	CString m_newHost;
	BOOL m_newAdmin;
	BOOL m_startup;

public:
	CKrb4AddToRealmHostList(CWnd* pParent = NULL);   // standard constructor

	CString GetNewRealm() {return m_newRealm;}
	CString GetNewHost() {return m_newHost;}
	BOOL GetNewAdmin() {return m_newAdmin;}

// Dialog Data
	//{{AFX_DATA(CKrb4AddToRealmHostList)
	enum { IDD = IDD_KRB4_ADD_REALM };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb4AddToRealmHostList)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb4AddToRealmHostList)
	afx_msg void OnChangeEditDefaultRealm();
	afx_msg void OnChangeEditRealmHostname();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnRadioAdminServer();
	afx_msg void OnRadioNoAdminServer();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_ADDTOREALMHOSTLIST_H__26A1E1F3_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
