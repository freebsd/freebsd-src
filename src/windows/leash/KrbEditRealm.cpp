//	**************************************************************************************
//	File:			KrbEditRealm.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbEditRealm.h. Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "Krb4Properties.h"
#include "KrbEditRealm.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrbEditRealm dialog

CKrbEditRealm::CKrbEditRealm(CString& editItem, CWnd* pParent)
	: CDialog(CKrbEditRealm::IDD, pParent)
{
	m_startup = TRUE;
	m_newRealm = editItem;


	//{{AFX_DATA_INIT(CKrbEditRealm)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CKrbEditRealm::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbEditRealm)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrbEditRealm, CDialog)
	//{{AFX_MSG_MAP(CKrbEditRealm)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_REALM, OnChangeEditRealm)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrbEditRealm message handlers

BOOL CKrbEditRealm::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_EDIT_REALM, m_newRealm);

	return TRUE;
}

void CKrbEditRealm::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}

void CKrbEditRealm::OnChangeEditRealm()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_REALM, m_newRealm);
}

void CKrbEditRealm::OnOK()
{
	m_newRealm.TrimLeft();
	m_newRealm.TrimRight();

	if (m_newRealm.IsEmpty())
	{ // stay
		MessageBox("OnOK::The Realm field must be filled in!",
                    "Leash", MB_OK);
	}
	else if (-1 != m_newRealm.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Leash", MB_OK);
	}
	else
	  CDialog::OnOK(); // exit
}
