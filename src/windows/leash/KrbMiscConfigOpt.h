//*****************************************************************************
// File:	KrbMiscConfigOpt.h
// By:		Paul B. Hill
// Created:	08/12/1999
// Copyright:	@1999 Massachusetts Institute of Technology - All rights
//		reserved.
// Description: H file for KrbMiscConfigOpt.cpp.  Contains variables
//		and functions for Kerberos Properties.
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 08/12/99	PBH	Original
//*****************************************************************************


#if !defined(AFX_MISCCONFIGOPT_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
#define AFX_MISCONFIGOPT_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif


#include "resource.h"


///////////////////////////////////////////////////////////////////////
// CKrbMiscConfigOptions dialog

class CKrbMiscConfigOpt : public CPropertyPage
{
// Construction
private:
	DECLARE_DYNCREATE(CKrbMiscConfigOpt)
	BOOL m_startupPage2;
    BOOL m_noLifeTime;

	static UINT m_DefaultLifeTime;
	static CString m_initDefaultLifeTimeMin;
	static CString m_newDefaultLifeTimeMin;
	static CString m_initDefaultLifeTimeHr;
	static CString m_newDefaultLifeTimeHr;
	static CString m_initDefaultLifeTimeDay;
	static CString m_newDefaultLifeTimeDay;

	static UINT m_DefaultRenewTill;
	static CString m_initDefaultRenewTillMin;
	static CString m_newDefaultRenewTillMin;
	static CString m_initDefaultRenewTillHr;
	static CString m_newDefaultRenewTillHr;
	static CString m_initDefaultRenewTillDay;
	static CString m_newDefaultRenewTillDay;

	static UINT m_DefaultLifeMin;
	static CString m_initDefaultLifeMinMin;
	static CString m_newDefaultLifeMinMin;
	static CString m_initDefaultLifeMinHr;
	static CString m_newDefaultLifeMinHr;
	static CString m_initDefaultLifeMinDay;
	static CString m_newDefaultLifeMinDay;

	static UINT m_DefaultLifeMax;
	static CString m_initDefaultLifeMaxMin;
	static CString m_newDefaultLifeMaxMin;
	static CString m_initDefaultLifeMaxHr;
	static CString m_newDefaultLifeMaxHr;
	static CString m_initDefaultLifeMaxDay;
	static CString m_newDefaultLifeMaxDay;

	static UINT m_DefaultRenewMin;
	static CString m_initDefaultRenewMinMin;
	static CString m_newDefaultRenewMinMin;
	static CString m_initDefaultRenewMinHr;
	static CString m_newDefaultRenewMinHr;
	static CString m_initDefaultRenewMinDay;
	static CString m_newDefaultRenewMinDay;

	static UINT m_DefaultRenewMax;
	static CString m_initDefaultRenewMaxMin;
	static CString m_newDefaultRenewMaxMin;
	static CString m_initDefaultRenewMaxHr;
	static CString m_newDefaultRenewMaxHr;
	static CString m_initDefaultRenewMaxDay;
	static CString m_newDefaultRenewMaxDay;

	static void ResetDefaultLifeTimeEditBox();
	static void ResetDefaultRenewTillEditBox();
	static void ResetDefaultLifeMinEditBox();
	static void ResetDefaultLifeMaxEditBox();
	static void ResetDefaultRenewMinEditBox();
	static void ResetDefaultRenewMaxEditBox();

    BOOL m_initUseKrb4;
    BOOL m_newUseKrb4;
    BOOL m_initKinitPreserve;
    BOOL m_newKinitPreserve;

public:
	CKrbMiscConfigOpt();
	~CKrbMiscConfigOpt();

// Dialog Data
	//{{AFX_DATA(CKrbMiscConfigOpt)
	enum { IDD = IDD_KRB_PROP_MISC };
	static CEdit m_krbLifeTimeDayEditbox;
	static CEdit m_krbLifeTimeMinEditbox;
	static CEdit m_krbLifeTimeHrEditbox;
	static CEdit m_krbRenewTillDayEditbox;
	static CEdit m_krbRenewTillMinEditbox;
	static CEdit m_krbRenewTillHrEditbox;
	static CEdit m_krbRenewMaxDayEditbox;
	static CEdit m_krbRenewMinDayEditbox;
	static CEdit m_krbLifeMinDayEditbox;
	static CEdit m_krbLifeMinMinEditbox;
	static CEdit m_krbLifeMinHrEditbox;
	static CEdit m_krbLifeMaxDayEditbox;
	static CEdit m_krbLifeMaxMinEditbox;
	static CEdit m_krbLifeMaxHrEditbox;
	static CEdit m_krbRenewMinMinEditbox;
	static CEdit m_krbRenewMinHrEditbox;
	static CEdit m_krbRenewMaxMinEditbox;
	static CEdit m_krbRenewMaxHrEditbox;
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
	//{{AFX_MSG(CKrbMiscConfigOpt)
	virtual BOOL OnInitDialog();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnEditKillfocusEditDefaultLifeTime();
	afx_msg void OnResetDefaultLifeTimeEditBox();
	afx_msg void OnSelchangeEditDefaultLifeTime();
	afx_msg void OnEditKillfocusEditDefaultRenewTill();
	afx_msg void OnResetDefaultRenewTillEditBox();
	afx_msg void OnSelchangeEditDefaultRenewTill();
	afx_msg void OnEditKillfocusEditDefaultLifeMin();
	afx_msg void OnResetDefaultLifeMinEditBox();
	afx_msg void OnSelchangeEditDefaultLifeMin();
	afx_msg void OnEditKillfocusEditDefaultLifeMax();
	afx_msg void OnResetDefaultLifeMaxEditBox();
	afx_msg void OnSelchangeEditDefaultLifeMax();
	afx_msg void OnEditKillfocusEditDefaultRenewMin();
	afx_msg void OnResetDefaultRenewMinEditBox();
	afx_msg void OnSelchangeEditDefaultRenewMin();
	afx_msg void OnEditKillfocusEditDefaultRenewMax();
	afx_msg void OnResetDefaultRenewMaxEditBox();
	afx_msg void OnSelchangeEditDefaultRenewMax();
    afx_msg void OnCheckUseKrb4();
    afx_msg void OnCheckKinitPreserve();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MISCONFIGOPT_H__CD702F99_7495_11D0_8FDC_00C04FC2A0C2__INCLUDED_)
