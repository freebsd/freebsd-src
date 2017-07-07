//	File:			Krb4AddToRealmHostList.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for Krb4AddToRealmHostList.h. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "Krb4AddToRealmHostList.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrb4AddToRealmHostList dialog


CKrb4AddToRealmHostList::CKrb4AddToRealmHostList(CWnd* pParent /*=NULL*/)
: CDialog(CKrb4AddToRealmHostList::IDD, pParent)
{
	m_newRealm = _T("");
	m_newHost = _T("");
	m_newAdmin = TRUE;
	m_startup = TRUE;

	//{{AFX_DATA_INIT(CKrb4AddToRealmHostList)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CKrb4AddToRealmHostList::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4AddToRealmHostList)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb4AddToRealmHostList, CDialog)
	//{{AFX_MSG_MAP(CKrb4AddToRealmHostList)
	ON_EN_CHANGE(IDC_EDIT_DEFAULT_REALM, OnChangeEditDefaultRealm)
	ON_EN_CHANGE(IDC_EDIT_REALM_HOSTNAME, OnChangeEditRealmHostname)
	ON_WM_SHOWWINDOW()
	ON_BN_CLICKED(IDC_RADIO_ADMIN_SERVER, OnRadioAdminServer)
	ON_BN_CLICKED(IDC_RADIO_NO_ADMIN_SERVER, OnRadioNoAdminServer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrb4AddToRealmHostList message handlers

void CKrb4AddToRealmHostList::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}

void CKrb4AddToRealmHostList::OnChangeEditDefaultRealm()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_DEFAULT_REALM, m_newRealm);
}

void CKrb4AddToRealmHostList::OnChangeEditRealmHostname()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_REALM_HOSTNAME, m_newHost);
}

void CKrb4AddToRealmHostList::OnRadioAdminServer()
{
	m_newAdmin = TRUE;
}

void CKrb4AddToRealmHostList::OnRadioNoAdminServer()
{
	m_newAdmin = FALSE;
}

void CKrb4AddToRealmHostList::OnOK()
{
	m_newRealm.TrimLeft();
	m_newRealm.TrimRight();
	m_newHost.TrimLeft();
	m_newHost.TrimRight();

	if (m_newRealm.IsEmpty() || m_newHost.IsEmpty())
	{ // stay
		MessageBox("OnOK::Both Realm and Host fields must be filled in!",
                    "Leash", MB_OK);
	}
	else if (-1 != m_newRealm.Find(' ') || -1 != m_newHost.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Leash", MB_OK);
	}

	else
	  CDialog::OnOK(); // exit
}

BOOL CKrb4AddToRealmHostList::OnInitDialog()
{
	CDialog::OnInitDialog();

	CheckRadioButton(IDC_RADIO_ADMIN_SERVER, IDC_RADIO_NO_ADMIN_SERVER, IDC_RADIO_ADMIN_SERVER);

	return TRUE;
}
