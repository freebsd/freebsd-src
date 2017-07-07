//	File:			KrbAddRealm.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbAddRealm.h. Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "KrbAddRealm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrbAddRealm dialog


CKrbAddRealm::CKrbAddRealm(CWnd* pParent /*=NULL*/)
: CDialog(CKrbAddRealm::IDD, pParent)
{
	m_newRealm = _T("");
	m_startup = TRUE;

	//{{AFX_DATA_INIT(CKrbAddRealm)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CKrbAddRealm::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbAddRealm)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrbAddRealm, CDialog)
	//{{AFX_MSG_MAP(CKrbAddRealm)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_REALM, OnChangeEditRealm)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrbAddRealm message handlers

void CKrbAddRealm::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}

void CKrbAddRealm::OnChangeEditRealm()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_REALM, m_newRealm);
}

void CKrbAddRealm::OnOK()
{
	m_newRealm.TrimLeft();
	m_newRealm.TrimRight();

	if (m_newRealm.IsEmpty())
	{ // stay
		MessageBox("OnOK:: Kerberos Realm must be filled in!",
                   "Leash", MB_OK);
	}
	else if (-1 != m_newRealm.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Leash", MB_OK);
	}
	else
	  CDialog::OnOK(); // exit
}
