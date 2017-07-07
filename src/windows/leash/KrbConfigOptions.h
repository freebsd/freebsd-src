//	**************************************************************************************
//	File:			KrbConfigOptions.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for KrbProperties.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	2/01/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_CONFIGOPTIONS_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
#define AFX_CONFIGOPTIONS_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// Krb4Properties.h : header file
//

#include "Resource.h"


///////////////////////////////////////////////////////////////////////
// CKrbConfigOptions dialog

class CKrbConfigOptions : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrbConfigOptions)
	BOOL m_startupPage2;
	BOOL m_noKrbFileError;
	BOOL m_noKrbhostWarning;
	static BOOL m_profileError;
	static BOOL m_dupEntriesError;
	BOOL m_noRealm;
	CString m_initDefaultRealm;
	static CString m_newDefaultRealm; ///// also used for CKrb4DomainRealmMaintenance
	static CString m_hostServer;

	static void ResetDefaultRealmComboBox();

public:
	CKrbConfigOptions();
	~CKrbConfigOptions();

// Dialog Data
	//{{AFX_DATA(CKrbConfigOptions)
	enum { IDD = IDD_KRB_PROP_CONTENT };
	static CComboBox m_krbRealmEditbox;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CKrbConfigOptions)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual VOID DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnApply();

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CKrbConfigOptions)
	virtual BOOL OnInitDialog();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnEditchangeEditDefaultRealm();
	afx_msg void OnSelchangeEditDefaultRealm();
	afx_msg void OnButtonKrbHelp();
	afx_msg void OnButtonKrbrealmHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CONFIGOPTIONS_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
