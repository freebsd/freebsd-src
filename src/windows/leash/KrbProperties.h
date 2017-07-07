//	**************************************************************************************
//	File:			KrbProperties.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for KrbProperties.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	02/01/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_KRB_PROPERTY_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
#define AFX_KRB_PROPERTY_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// KrbProperties.h : header file
//

#include "KrbConfigOptions.h"
#include "KrbRealmHostMaintenance.h"
#include "KrbDomainRealmMaintenance.h"
#ifndef NO_KRB4
#include "Krb4DomainRealmMaintenance.h"
#include "Krb4RealmHostMaintenance.h"
#endif
#include "KrbMiscConfigOpt.h"

//////////////////////////////////////////////////////////////////////
// CKrbProperties

class CKrbProperties : public CPropertySheet
{
private:
	DECLARE_DYNAMIC(CKrbProperties)

public:
	//CKrbConfigFileLocation m_fileLocation;
	CKrbConfigOptions m_configOptions;
#ifndef NO_KRB4
	CKrb4RealmHostMaintenance m_krb4RealmHostMaintenance;
#endif
	CKrbRealmHostMaintenance m_realmHostMaintenance;
#ifndef NO_KRB4
	CKrb4DomainRealmMaintenance m_krb4DomainRealmMaintenance;
#endif
	CKrbDomainRealmMaintenance m_domainRealmMaintenance;
    CKrbMiscConfigOpt m_miscConfigOpt;

	static BOOL KrbPropertiesOn;
	static BOOL applyButtonEnabled;
	static CHAR m_krbPath[MAX_PATH];
	static CHAR m_krbrealmPath[MAX_PATH];

// Construction
public:
	CKrbProperties(UINT nIDCaption, CWnd* pParentWnd = NULL,
	           UINT iSelectPage = 0);
	CKrbProperties(LPCTSTR pszCaption, CWnd* pParentWnd = NULL,
	           UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbProperties)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CKrbProperties();

	// Generated message map functions
protected:
	//{{AFX_MSG(CKrbProperties)
		// NOTE - the ClassWizard will add and remove member functions here.
    afx_msg void OnHelp();
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRB_PROPERTY_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
