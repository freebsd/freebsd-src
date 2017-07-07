//	**************************************************************************************
//	File:			Krb4EditRealmHostList.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for Krb4EditRealmHostList.h. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "Krb4Properties.h"
#include "Krb4EditRealmHostList.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrb4EditRealmHostList dialog

CKrb4EditRealmHostList::CKrb4EditRealmHostList(LPSTR editItem, CWnd* pParent)
	: CDialog(CKrb4EditRealmHostList::IDD, pParent)
{
	m_startup = TRUE;
	m_editItem = _T("");

/*
    // Parse the passed in item
	LPSTR pEditItem = editItem;
	LPSTR findSpace = strchr(editItem, ' ');
	if (findSpace)
	  *findSpace = 0;
	else
	{
		 LeashErrorBox("This is a defective entry in file",
					   CKrb4ConfigFileLocation::m_krbFile);
		 ASSERT(0);
		 m_initRealm = m_newRealm = editItem;
		 m_initHost = m_newHost = _T("");
	}

	m_initRealm = m_newRealm = editItem;  // first token

	pEditItem = strchr(editItem, '\0');
	if (pEditItem)
	{
		pEditItem++;
		findSpace++;
	}
	else
	  ASSERT(0);

	findSpace = strchr(pEditItem, ' ');
	if (findSpace)
	{
		*findSpace = 0;
	}
	else
	{
		m_initAdmin = m_newAdmin = FALSE;
		m_initHost = m_newHost = pEditItem; // second token
		return;
	}

	m_initHost = m_newHost = pEditItem; // second token

	findSpace++;
	pEditItem = findSpace;
	if (pEditItem)
	{
		if (strstr(pEditItem, "admin server"))
		  m_initAdmin = m_newAdmin = TRUE;
		//else
		  //;  It must be something else??? :(
	}
	else
	  ASSERT(0);
*/
	//{{AFX_DATA_INIT(CKrb4EditRealmHostList)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CKrb4EditRealmHostList::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4EditRealmHostList)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb4EditRealmHostList, CDialog)
	//{{AFX_MSG_MAP(CKrb4EditRealmHostList)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_DEFAULT_REALM, OnChangeEditDefaultRealm)
	ON_EN_CHANGE(IDC_EDIT_REALM_HOSTNAME, OnChangeEditRealmHostname)
	ON_BN_CLICKED(IDC_RADIO_ADMIN_SERVER, OnRadioAdminServer)
	ON_BN_CLICKED(IDC_RADIO_NO_ADMIN_SERVER, OnRadioNoAdminServer)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrb4EditRealmHostList message handlers

BOOL CKrb4EditRealmHostList::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_EDIT_DEFAULT_REALM, m_newRealm);
	SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, m_newHost);

	if (m_initAdmin)
	{ // has Admin Server
		CheckRadioButton(IDC_RADIO_ADMIN_SERVER, IDC_RADIO_NO_ADMIN_SERVER, IDC_RADIO_ADMIN_SERVER);
	}
	else
	{ // no Admin Server
		CheckRadioButton(IDC_RADIO_ADMIN_SERVER, IDC_RADIO_NO_ADMIN_SERVER, IDC_RADIO_NO_ADMIN_SERVER);
	}

	//GetDlgItem(IDC_EDIT_DEFAULT_REALM)->EnableWindow();
	//GetDlgItem(IDC_EDIT_DEFAULT_REALM)->SetFocus();

	return TRUE;
}

void CKrb4EditRealmHostList::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}

void CKrb4EditRealmHostList::OnChangeEditDefaultRealm()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_DEFAULT_REALM, m_newRealm);
}

void CKrb4EditRealmHostList::OnChangeEditRealmHostname()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_REALM_HOSTNAME, m_newHost);
}

void CKrb4EditRealmHostList::OnRadioAdminServer()
{
	m_newAdmin = TRUE;
}

void CKrb4EditRealmHostList::OnRadioNoAdminServer()
{
	m_newAdmin = FALSE;
}

void CKrb4EditRealmHostList::OnOK()
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

	m_editItem = m_newRealm + " " + m_newHost;

	if (m_newAdmin)
	{
		m_editItem += " ";
		m_editItem += ADMIN_SERVER;
	}
}
