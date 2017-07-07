#if !defined(AFX_KRBDOMAINREALMMAINTENANCE_H__6DB290A6_E14D_11D2_95CE_0000861B8A3C__INCLUDED_)
#define AFX_KRBDOMAINREALMMAINTENANCE_H__6DB290A6_E14D_11D2_95CE_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// CKrbDomainRealmMaintenance.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CKrbDomainRealmMaintenance dialog

class CKrbDomainRealmMaintenance : public CPropertyPage
{
// Construction
private:
	BOOL m_dupEntiesError;
	BOOL CheckForDupDomain(CString& newDomainHost);

public:
	CKrbDomainRealmMaintenance(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CKrbDomainRealmMaintenance)
	enum { IDD = IDD_KRB_DOMAINREALM_MAINT };
	CListBox	m_KDCDomainList;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CKrbDomainRealmMaintenance)
	public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CKrbDomainRealmMaintenance)
	virtual void OnCancel();
	virtual BOOL OnApply();
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonHostAdd();
	afx_msg void OnButtonHostEdit();
	afx_msg void OnButtonHostRemove();
	afx_msg void OnDblclkListDomainrealm();
	afx_msg void OnButtonHostmaintHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_KRBDOMAINREALMMAINTENANCE_H__6DB290A6_E14D_11D2_95CE_0000861B8A3C__INCLUDED_)
