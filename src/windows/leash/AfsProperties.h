#if !defined(AFX_AFSPROPERTIES_H__FD135601_2FCB_11D3_96A2_0000861B8A3C__INCLUDED_)
#define AFX_AFSPROPERTIES_H__FD135601_2FCB_11D3_96A2_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// AfsProperties.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CAfsProperties dialog

class CAfsProperties : public CDialog
{
// Construction
private:
    UINT m_newAfsStatus;
    UINT m_oldAfsStatus;
    CWinApp *m_pApp;

public:
	CAfsProperties(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CAfsProperties)
	enum { IDD = IDD_AFS_PROPERTIES };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAfsProperties)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CAfsProperties)
	virtual BOOL OnInitDialog();
	afx_msg void OnButtonAfsProperties();
	virtual void OnOK();
	afx_msg void OnRadioAfsEnabled();
	afx_msg void OnRadioAfsDisabled();
    afx_msg void OnHelp();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_AFSPROPERTIES_H__FD135601_2FCB_11D3_96A2_0000861B8A3C__INCLUDED_)
