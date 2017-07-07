//	**************************************************************************************
//	File:			LeashProperties.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for LeashProperties.cpp. Contains variables and functions
//					for the Leash Properties Dialog Box
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************

#if !defined(AFX_LEASHPROPERTIES_H__7E54E028_726E_11D2_945E_0000861B8A3C__INCLUDED_)
#define AFX_LEASHPROPERTIES_H__7E54E028_726E_11D2_945E_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LeashProperties.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLeashProperties dialog

#define TIMEHOST "TIMEHOST"

class CLeashProperties : public CDialog
{
private:
	static char timeServer[255];
	CHAR sysDir[MAX_PATH];
    BOOL m_initMissingFiles;
    BOOL m_newMissingFiles;
    DWORD dw_initMslsaImport;
    DWORD dw_newMslsaImport;

// Construction
public:
	CLeashProperties(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLeashProperties)
	enum { IDD = IDD_LEASH_PROPERTIES };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLeashProperties)
	public:
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLeashProperties)
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnHelp();
    afx_msg void OnCheckMissingCfg();
    afx_msg void OnRadioMslsaNever();
    afx_msg void OnRadioMslsaAlways();
    afx_msg void OnRadioMslsaMatchingRealm();
    afx_msg void OnButtonResetDefaults();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LEASHPROPERTIES_H__7E54E028_726E_11D2_945E_0000861B8A3C__INCLUDED_)
