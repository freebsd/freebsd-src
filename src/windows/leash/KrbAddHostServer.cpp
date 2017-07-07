// KrbAddHostServer.cpp : implementation file
//

#include "stdafx.h"
#include "leash.h"
#include "KrbAddHostServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrbAddHostServer dialog


CKrbAddHostServer::CKrbAddHostServer(CWnd* pParent /*=NULL*/)
	: CDialog(CKrbAddHostServer::IDD, pParent)
{
	m_newHost = _T("");
	m_startup = TRUE;

	//{{AFX_DATA_INIT(CKrbAddHostServer)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CKrbAddHostServer::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbAddHostServer)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrbAddHostServer, CDialog)
	//{{AFX_MSG_MAP(CKrbAddHostServer)
	ON_EN_CHANGE(IDC_EDIT_KDC_HOST, OnChangeEditKdcHost)
	ON_WM_SHOWWINDOW()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrbAddHostServer message handlers

void CKrbAddHostServer::OnOK()
{
	m_newHost.TrimLeft();
	m_newHost.TrimRight();

	if (m_newHost.IsEmpty())
	{ // stay
		MessageBox("OnOK:: Server Hosting a KDC must be filled in!",
                    "Error", MB_OK);
	}
	else if (-1 != m_newHost.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Error", MB_OK);
	}
	else
	  CDialog::OnOK(); // exit
}

void CKrbAddHostServer::OnChangeEditKdcHost()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_KDC_HOST, m_newHost);
}

void CKrbAddHostServer::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}
