//	**************************************************************************************
//	File:			KrbEditRealm.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Krb4EditRealmHostList.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************



#if !defined(AFX_EDITREALMHOSTLIST_H__26A1E1F7_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
#define AFX_EDITREALMHOSTLIST_H__26A1E1F7_9117_11D2_94D0_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EditRealmHostList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrbEditRealm dialog

class CKrbEditRealm : public CDialog
{
// Construction
private:
	//CString m_editItem;
	//CString m_initRealm;
	CString m_newRealm;
	//CString m_initHost;
	//CString m_newHost;
	//BOOL m_initAdmin;
	//BOOL m_newAdmin;
	BOOL m_startup;

public:
	CKrbEditRealm(CString& editItem, CWnd* pParent = NULL);
	CString GetEditedItem() {return m_newRealm;}

// Dialog Data
	//{{AFX_DATA(CKrbEditRealm)
	enum { IDD = IDD_KRB_EDIT_REALM };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbEditRealm)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrbEditRealm)
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnChangeEditRealm();
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITREALMHOSTLIST_H__26A1E1F7_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
