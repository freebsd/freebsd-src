//	**************************************************************************************
//	File:			KrbEditHostServer.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	H file for KrbEditHostServer.cpp. Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************



#if !defined(AFX_EDITHOST_H__26A1E1F7_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
#define AFX_EDITHOST_H__26A1E1F7_9117_11D2_94D0_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// EditRealmHostList.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrbEditHostServer dialog

class CKrbEditHostServer : public CDialog
{
// Construction
private:
	CString m_newHost;
	BOOL m_startup;

public:
	CKrbEditHostServer(CString& editItem, CWnd* pParent = NULL);
	CString GetEditedItem() {return m_newHost;}

// Dialog Data
	//{{AFX_DATA(CKrbEditHostServer)
	enum { IDD = IDD_KRB_EDIT_KDC_HOSTSERVER };
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbEditHostServer)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrbEditHostServer)
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnChangeEditKdcHost();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_EDITHOST_H__26A1E1F7_9117_11D2_94D0_0000861B8A3C__INCLUDED_)
