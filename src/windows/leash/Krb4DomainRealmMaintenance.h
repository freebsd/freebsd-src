//	**************************************************************************************
//	File:			Krb4DomainRealmMaintenance.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Krb4DomainRealmMaintenance.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_REALMNAMEMAINTENANCE_H__9CA36918_8FC0_11D2_94CC_0000861B8A3C__INCLUDED_)
#define AFX_REALMNAMEMAINTENANCE_H__9CA36918_8FC0_11D2_94CC_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RealmNameMaintenance.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrb4DomainRealmMaintenance dialog

class CKrb4DomainRealmMaintenance : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrb4DomainRealmMaintenance)
	CHAR lineBuf[MAXLINE];
	INT m_defectiveLines;

public:
	CKrb4DomainRealmMaintenance();   // standard constructor
	virtual ~CKrb4DomainRealmMaintenance();

// Dialog Data
	//{{AFX_DATA(CKrb4DomainRealmMaintenance)
	enum { IDD = IDD_KRB4_DOMAINREALM_MAINT };
	CDragListBox m_realmDomainList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb4DomainRealmMaintenance)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb4DomainRealmMaintenance)
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	afx_msg void OnButtonRealmHostAdd();
	afx_msg void OnButtonRealmHostRemove();
	afx_msg void OnButtonRealmHostEdit();
	afx_msg void OnSelchangeListDomainrealm();
	afx_msg void OnDblclkListDomainrealm();
	afx_msg void OnButtonHostmaintHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REALMNAMEMAINTENANCE_H__9CA36918_8FC0_11D2_94CC_0000861B8A3C__INCLUDED_)
