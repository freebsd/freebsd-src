//	**************************************************************************************
//	File:			Krb5Properties.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for Krb5Properties.cpp. Contains variables and functions
//					for Kerberos Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_KRB5PROPERTIES_H__9011A0B3_6E92_11D2_9454_0000861B8A3C__INCLUDED_)
#define AFX_KRB5PROPERTIES_H__9011A0B3_6E92_11D2_9454_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// Krb5Properties.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrb5ConfigOptions dialog

class CKrb5ConfigFileLocation : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrb5ConfigFileLocation)
	CString m_initConfigFile;
	CString m_initTicketFile;
	CString m_newConfigFile;
	CString m_newTicketFile;
	BOOL m_startupPage1;

public:
	CKrb5ConfigFileLocation();   // standard constructor

// Dialog Data
	//{{AFX_DATA(CKrb5ConfigFileLocation)
	enum { IDD = IDD_KRB5_PROP_LOCATION };
	CEdit	m_ticketEditBox;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb5ConfigFileLocation)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnApply();

 // Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb5ConfigFileLocation)
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonKrb5iniBrowse();
	afx_msg void OnButtonKrb5TicketfileBrowse();
	afx_msg void OnChangeEditKrb5TxtFile();
	afx_msg void OnChangeEditKrb5iniLocation();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////
// CKrb5ConfigOptions dialog

class CKrb5ConfigOptions : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrb5ConfigOptions)
	INT m_initForwardable;
	INT m_newForwardable;
	INT m_initProxiable;
	INT m_newProxiable;
	INT m_initRenewable;
	INT m_newRenewable;
	INT m_initNoAddress;
	INT m_newNoAddress;
    DWORD m_initIPAddress;
#ifdef SET_PUBLIC_IP
    DWORD m_newIPAddress;
#endif /* SET_PUBLIC_IP */

public:
	CKrb5ConfigOptions();   // standard constructor

// Dialog Data
	//{{AFX_DATA(CKrb5ConfigOptions)
	enum { IDD = IDD_KRB5_PROP_CONTENT };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb5ConfigOptions)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

	virtual BOOL OnApply();

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrb5ConfigOptions)
	virtual BOOL OnInitDialog();
	afx_msg void OnCheckForwardable();
	afx_msg void OnCheckProxiable();
	afx_msg void OnCheckRenewable();
	afx_msg void OnCheckNoAddress();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

class CKrb5Properties : public CPropertySheet
{
private:
	DECLARE_DYNAMIC(CKrb5Properties)

public:
	CKrb5ConfigFileLocation m_fileLocation;
	CKrb5ConfigOptions m_configOptions;

// Construction
public:
	CKrb5Properties(UINT nIDCaption, CWnd* pParentWnd = NULL,
	           UINT iSelectPage = 0);
	CKrb5Properties(LPCTSTR pszCaption, CWnd* pParentWnd = NULL,
	           UINT iSelectPage = 0);

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrb5Properties)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CKrb5Properties();

	// Generated message map functions
protected:
	//{{AFX_MSG(CKrb5Properties)
		// NOTE - the ClassWizard will add and remove member functions here.
    afx_msg void OnHelp();
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRB5PROPERTIES_H__9011A0B3_6E92_11D2_9454_0000861B8A3C__INCLUDED_)
