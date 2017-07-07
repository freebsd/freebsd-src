#if !defined(AFX_LEASHCONTROLPANEL_H__940146F3_6857_11D2_943C_0000861B8A3C__INCLUDED_)
#define AFX_LEASHCONTROLPANEL_H__940146F3_6857_11D2_943C_0000861B8A3C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// LeashControlPanel.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CLeashControlPanel dialog

class CLeashControlPanel : public CDialog
{
// Construction
public:
	CLeashControlPanel(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CLeashControlPanel)
	enum { IDD = IDD_LEASH_CONTROL_PANEL };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CLeashControlPanel)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CLeashControlPanel)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_LEASHCONTROLPANEL_H__940146F3_6857_11D2_943C_0000861B8A3C__INCLUDED_)
