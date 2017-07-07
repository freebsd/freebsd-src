#if !defined(AFX_KRBADDHOSTSERVER_H__1B6B6ED8_D26D_11D2_95AF_0000861B8A3C__INCLUDED_)
#define AFX_KRBADDHOSTSERVER_H__1B6B6ED8_D26D_11D2_95AF_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// KrbAddHostServer.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrbAddHostServer dialog

class CKrbAddHostServer : public CDialog
{
// Construction
	CString m_newHost;
	BOOL m_startup;

public:
	CKrbAddHostServer(CWnd* pParent = NULL);   // standard constructor
	CString GetNewHost() {return m_newHost;}


// Dialog Data
	//{{AFX_DATA(CKrbAddHostServer)
	enum { IDD = IDD_KRB_ADD_KDC_HOSTSERVER};
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbAddHostServer)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrbAddHostServer)
	virtual void OnOK();
	afx_msg void OnChangeEditKdcHost();
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRBADDHOSTSERVER_H__1B6B6ED8_D26D_11D2_95AF_0000861B8A3C__INCLUDED_)
