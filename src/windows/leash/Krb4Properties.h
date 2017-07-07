//	**************************************************************************************
//	File:			Krb4Properties.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for KrbProperties.cpp. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_PROPERTY_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
#define AFX_PROPERTY_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// Krb4Properties.h : header file
//

#include "Resource.h"
//#include "Krb4RealmHostMaintenance.h"
//#include "Krb4DomainRealmMaintenance.h"

///////////////////////////////////////////////////////////////////////
// CKrb4ConfigFileLocation dialog

class CKrb4ConfigFileLocation : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrb4ConfigFileLocation)
	CString m_ticketFile;
	CString m_newTicketFile;
	static CString m_newKrbFile;
    static CString m_newKrbrealmFile; // static for the CKrb4EditDomainRealmList class
    CString m_initKrbFile;
    CString m_initKrbrealmFile;
    CString m_initTicketFile;

	BOOL m_noKrbFileStartupWarning;
    BOOL m_noKrbrealmFileStartupWarning;
	BOOL m_startupPage1;

public:
	CKrb4ConfigFileLocation();
	~CKrb4ConfigFileLocation();

// Dialog Data
	//{{AFX_DATA(CKrb4ConfigFileLocation)
	enum { IDD = IDD_KRB4_PROP_LOCATION };
	CEdit	m_ticketEditBox;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CKrb4ConfigFileLocation)
	public:
	virtual VOID OnCancel();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual VOID DoDataExchange(CDataExchange* pDX); // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual VOID OnOK();
	virtual BOOL OnApply();

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CKrb4ConfigFileLocation)
	virtual BOOL OnInitDialog();
	afx_msg VOID OnButtonKrbBrowse();
	afx_msg VOID OnButtonKrbrealmBrowse();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnChangeEditTicketFile();
    afx_msg void OnHelp();
	afx_msg void OnChangeEditKrbLoc();
	afx_msg void OnChangeEditKrbrealmLoc();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


//////////////////////////////////////////////////////////////////////
// CKrb4Properties

class CKrb4Properties : public CPropertySheet
{
private:
	DECLARE_DYNAMIC(CKrb4Properties)

public:
	CKrb4ConfigFileLocation m_fileLocation;

	static BOOL applyButtonEnabled;

// Construction
public:
	CKrb4Properties(UINT nIDCaption, CWnd* pParentWnd = NULL,
	           UINT iSelectPage = 0);
	CKrb4Properties(LPCTSTR pszCaption, CWnd* pParentWnd = NULL,
	           UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb4Properties)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CKrb4Properties();

	// Generated message map functions
protected:
	//{{AFX_MSG(CKrb4Properties)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROPERTY_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
