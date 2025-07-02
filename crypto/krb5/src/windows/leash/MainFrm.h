//	**************************************************************************************
//	File:			MainFrm.h
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    H file for MainFrm.cpp. Contains variables and functions
//					for Leash
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#if !defined(AFX_MAINFRM_H__6F45AD95_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
#define AFX_MAINFRM_H__6F45AD95_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "LeashFrame.h"
#include "LeashUIApplication.h"

class CMainFrame : public CLeashFrame
{
private:
	int m_winRectLeft;
	int m_winRectTop;
	int m_winRectRight;
	int m_winRectBottom;
    BOOL m_bOwnerCreated;
    CDialog m_MainFrameOwner;
    IUIApplication *pApplication;

protected: // create from serialization only
    // Ribbon bar for the application
    CMFCRibbonBar m_wndRibbonBar;
    // Our own custom application button we can keep hidden.
    CMFCRibbonApplicationButton m_wndApplicationButton;


	CMainFrame();
	DECLARE_DYNCREATE(CMainFrame)

// Attributes
public:
	static int         m_whatSide;
#ifndef NO_STATUS_BAR
    static CMFCStatusBar  m_wndStatusBar;
#endif
	static CMFCToolBar    m_wndToolBar;
	static BOOL		   m_isMinimum;
    static BOOL        m_isBeingResized;
    static CImageList  m_imageList;
    static CImageList  m_disabledImageList;

// Operations
public:
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual void RecalcLayout(BOOL bNotify = TRUE);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	protected:
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CMainFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
    BOOL ShowTaskBarButton(BOOL bVisible);

protected:  // control bar embedded members


// Generated message map functions
protected:
	//{{AFX_MSG(CMainFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnResetWindowSize();
	afx_msg void OnSizing(UINT fwSide, LPRECT pRect);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO FAR* lpMMI);
	afx_msg void OnRibbonResize();
    afx_msg void OnClose(void);
    //afx_msg void OnContextHelp();
    //}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAINFRM_H__6F45AD95_561B_11D0_8FCF_00C04FC2A0C2__INCLUDED_)
