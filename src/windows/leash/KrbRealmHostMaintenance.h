//	**************************************************************************************
//	File:			KrbRealmHostMaintenance.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for KrbRealmHostMaintenance.cpp. Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_KRBREALMNAMEMAINTENANCE_H__2FE711C3_8E9A_11D2_94C5_0000861B8A3C__INCLUDED_)
#define AFX_KRBREALMNAMEMAINTENANCE_H__2FE711C3_8E9A_11D2_94C5_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


/////////////////////////////////////////////////////////////////////////////
// CKrbRealmHostMaintenance dialog

#include "resource.h"
#include "CLeashDragListBox.h"

#define MAXLINE 256

class CKrbRealmHostMaintenance : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrbRealmHostMaintenance)
	CHAR lineBuf[MAXLINE];
	CString m_theAdminServerMarked;
	CString m_theAdminServer;
	BOOL m_isRealmListBoxInFocus;
	BOOL m_isStart;
    BOOL m_initDnsKdcLookup;
    BOOL m_newDnsKdcLookup;

	bool OnButtonKdchostAddInternal();

	//void ResetDefaultRealmComboBox();

public:
	//CKrbRealmHostMaintenance(CWnd* pParent = NULL);   // standard constructor
	CKrbRealmHostMaintenance();
	virtual ~CKrbRealmHostMaintenance();

// Dialog Data
	//{{AFX_DATA(CKrbRealmHostMaintenance)
	enum { IDD = IDD_KRB_REALMHOST_MAINT };
	CListBox	m_KDCRealmList;
	CLeashDragListBox m_KDCHostList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbRealmHostMaintenance)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL


// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrbRealmHostMaintenance)
	virtual BOOL OnInitDialog();
	virtual BOOL OnApply();
	virtual void OnCancel();
	afx_msg void OnButtonRealmHostAdd();
	afx_msg void OnButtonRealmHostEdit();
	afx_msg void OnButtonRealmHostRemove();
	afx_msg void OnSelchangeListKdcRealm();
	afx_msg void OnButtonAdminserver();
	afx_msg void OnSetfocusListKdcRealm();
	afx_msg void OnButtonKdchostAdd();
	afx_msg void OnButtonKdchostRemove();
	afx_msg void OnButtonRemoveAdminserver();
	afx_msg void OnSelchangeListKdcHost();
	afx_msg void OnButtonKdchostEdit();
	afx_msg void OnDblclkListKdcRealm();
	afx_msg void OnDblclkListKdcHost();
	afx_msg void OnButtonRealmhostMaintHelp();
    afx_msg void OnCheckDnsKdcLookup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRBREALMNAMEMAINTENANCE_H__2FE711C3_8E9A_11D2_94C5_0000861B8A3C__INCLUDED_)
