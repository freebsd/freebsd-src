//	**************************************************************************************
//	File:			Krb4RealmHostMaintenance.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Krb4RealmHostMaintenance.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_REAMLHOSTMAINT_H__2FE711C3_8E9A_11D2_94C5_0000861B8A3C__INCLUDED_)
#define AFX_REAMLHOSTMAINT_H__2FE711C3_8E9A_11D2_94C5_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// RemoveHostNameList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrb4RealmHostMaintenance dialog

#define MAXLINE 256

class CKrb4RealmHostMaintenance : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrb4RealmHostMaintenance)
	CHAR lineBuf[MAXLINE];
	INT  m_defectiveLines;
    BOOL m_initDnsKdcLookup;
    BOOL m_newDnsKdcLookup;

	void ResetDefaultRealmComboBox();

public:
	//CKrb4RealmHostMaintenance(CWnd* pParent = NULL);   // standard constructor
	CKrb4RealmHostMaintenance();
	virtual ~CKrb4RealmHostMaintenance();

// Dialog Data
	//{{AFX_DATA(CKrb4RealmHostMaintenance)
	enum { IDD = IDD_KRB4_REALMHOST_MAINT2 };
	CDragListBox	m_RealmHostList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb4RealmHostMaintenance)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb4RealmHostMaintenance)
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual void OnOK();
	virtual void OnCancel();
	afx_msg void OnButtonRealmHostAdd();
	afx_msg void OnButtonRealmHostEdit();
	afx_msg void OnButtonRealmHostRemove();
	afx_msg void OnSelchangeListRemoveHost();
	afx_msg void OnDblclkListRemoveHost();
	afx_msg void OnButtonRealmhostMaintHelp2();
    afx_msg void OnCheckDnsKdcLookup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REAMLHOSTMAINT_H__2FE711C3_8E9A_11D2_94C5_0000861B8A3C__INCLUDED_)
