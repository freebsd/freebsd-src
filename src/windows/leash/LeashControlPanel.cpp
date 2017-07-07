// LeashControlPanel.cpp : implementation file
//

#include "stdafx.h"
#include "leash.h"
#include "LeashControlPanel.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLeashControlPanel dialog


CLeashControlPanel::CLeashControlPanel(CWnd* pParent /*=NULL*/)
	: CDialog(CLeashControlPanel::IDD, pParent)
{
	//{{AFX_DATA_INIT(CLeashControlPanel)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CLeashControlPanel::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLeashControlPanel)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLeashControlPanel, CDialog)
	//{{AFX_MSG_MAP(CLeashControlPanel)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLeashControlPanel message handlers
