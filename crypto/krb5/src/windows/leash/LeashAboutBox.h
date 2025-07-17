//*****************************************************************************
// File:	LeashAboutBox.cpp
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright:	@1998 Massachusetts Institute of Technology - All rights
//              reserved.
// Description:	H file for LeashAboutBox.cpp. Contains variables and functions
//		for the Leash About Box Dialog Box
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 12/02/98	ADL	Original
//*****************************************************************************


#if !defined(AFX_LEASHABOUTBOX_H__B49E3501_4801_11D2_8F7D_0000861B8A3C__INCLUDED_)
#define AFX_LEASHABOUTBOX_H__B49E3501_4801_11D2_8F7D_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LeashAboutBox.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLeashAboutBox dialog

class CLeashAboutBox : public CDialog
{
    BOOL m_missingFileError;
    DWORD SetVersionInfo(UINT id_ver, UINT id_copyright);
    BOOL GetModules95(DWORD processID, BOOL allModules = TRUE);
    void GetModulesNT(DWORD processID, BOOL allModules = TRUE);
    void HighlightFirstItem();

// Construction
public:
    CLeashAboutBox(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
    //{{AFX_DATA(CLeashAboutBox)
    enum { IDD = IDD_LEASH_ABOUTBOX };
    CButton	m_propertiesButton;
    CButton	m_radio_LeashDLLs;
    CListBox	m_LB_DLLsLoaded;
    CString	m_fileItem;
    BOOL        m_bListModules;
    //}}AFX_DATA


// Overrides
    // ClassWizard generated virtual function overrides
    //{{AFX_VIRTUAL(CLeashAboutBox)
public:
    virtual BOOL PreTranslateMessage(MSG* pMsg);
protected:
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
    //}}AFX_VIRTUAL

// Implementation
protected:

    // Generated message map functions
    //{{AFX_MSG(CLeashAboutBox)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    virtual BOOL OnInitDialog();
    afx_msg void OnSelchangeLeashModuleLb();
    afx_msg void OnAllModules();
    afx_msg void OnLeashModules();
    afx_msg void OnDblclkLeashModuleLb();
    afx_msg void OnProperties();
    afx_msg void OnSetfocusLeashModuleLb();
    afx_msg void OnNotLoadedModules();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LEASHABOUTBOX_H__B49E3501_4801_11D2_8F7D_0000861B8A3C__INCLUDED_)
