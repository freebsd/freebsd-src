//	File:			Krb4AddToDomainRealmList.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Krb4AddToDomainRealmList.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_KRB4ADDTODOMAINREALMLIST_H__F4D41683_96A4_11D2_94E2_0000861B8A3C__INCLUDED_)
#define AFX_KRB4ADDTODOMAINREALMLIST_H__F4D41683_96A4_11D2_94E2_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Krb4AddToDomainRealmList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrb4AddToDomainRealmList dialog

class CKrb4AddToDomainRealmList : public CDialog
{
// Construction
private:
	CString m_newRealm;
	CString m_newDomainHost;
	BOOL m_newAdmin;
	BOOL m_startup;

public:
	CKrb4AddToDomainRealmList(CWnd* pParent = NULL);   // standard constructor

	CString GetNewRealm() {return m_newRealm;}
	CString GetNewDomainHost() {return m_newDomainHost;}

// Dialog Data
	//{{AFX_DATA(CKrb4AddToDomainRealmList)
	enum { IDD = IDD_KRB4_ADD_DOMAINREALMNAME };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb4AddToDomainRealmList)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb4AddToDomainRealmList)
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnChangeEditDomainhostname();
	afx_msg void OnChangeEditDomainrealmname();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRB4ADDTODOMAINREALMLIST_H__F4D41683_96A4_11D2_94E2_0000861B8A3C__INCLUDED_)
