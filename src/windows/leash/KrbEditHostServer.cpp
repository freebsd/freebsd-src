//	**************************************************************************************
//	File:			KrbEditHostServer.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbEditHostServer.h. Contains variables and functions
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
#include "KrbEditHostServer.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrbEditHostServer dialog

CKrbEditHostServer::CKrbEditHostServer(CString& editItem, CWnd* pParent)
	: CDialog(CKrbEditHostServer::IDD, pParent)
{
	m_startup = TRUE;
	m_newHost = editItem;

	//{{AFX_DATA_INIT(CKrbEditHostServer)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CKrbEditHostServer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbEditHostServer)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrbEditHostServer, CDialog)
	//{{AFX_MSG_MAP(CKrbEditHostServer)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_KDC_HOST, OnChangeEditKdcHost)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrbEditHostServer message handlers

BOOL CKrbEditHostServer::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_EDIT_KDC_HOST, m_newHost);
	return TRUE;
}

void CKrbEditHostServer::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}

void CKrbEditHostServer::OnChangeEditKdcHost()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_KDC_HOST, m_newHost);
}

void CKrbEditHostServer::OnOK()
{
	m_newHost.TrimLeft();
	m_newHost.TrimRight();

	if (m_newHost.IsEmpty())
	{ // stay
		MessageBox("OnOK::The Server field must be filled in!",
                    "Error", MB_OK);
	}
	else if (-1 != m_newHost.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Error", MB_OK);
	}
	else
	  CDialog::OnOK(); // exit
}
