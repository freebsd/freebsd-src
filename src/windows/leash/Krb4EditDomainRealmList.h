//	**************************************************************************************
//	File:			Krb4EditDomainRealmList.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    H file for Krb4EditDomainRealmList.cpp. Contains variables and functions
//					for Kerberos Four Properites
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_KRB4EDITDOMAINREALMLIST_H__F4D41684_96A4_11D2_94E2_0000861B8A3C__INCLUDED_)
#define AFX_KRB4EDITDOMAINREALMLIST_H__F4D41684_96A4_11D2_94E2_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CKrb4EditDomainRealmList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrb4EditDomainRealmList dialog

class CKrb4EditDomainRealmList : public CDialog
{
// Construction
private:
	CString m_editItem;
	CString m_initRealm;
	CString m_newRealm;
	CString m_initDomainHost;
	CString m_newDomainHost;
	BOOL m_startup;


public:
	CKrb4EditDomainRealmList(LPSTR editItem, CWnd* pParent = NULL);
	CString GetEditedItem() {return m_editItem;}
	CString GetRealm() {return m_newRealm;}
	CString GetDomainHost() {return m_newDomainHost;}

// Dialog Data
	//{{AFX_DATA(CKrb4EditDomainRealmList)
	enum { IDD = IDD_KRB4_EDIT_DOMAINREALMNAME };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb4EditDomainRealmList)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb4EditDomainRealmList)
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditDefaultRealm();
	afx_msg void OnChangeEditRealmHostname();
	virtual void OnOK();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRB4EDITDOMAINREALMLIST_H__F4D41684_96A4_11D2_94E2_0000861B8A3C__INCLUDED_)
