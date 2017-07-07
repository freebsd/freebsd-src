//	**************************************************************************************
//	File:			Krb4EditDomainRealmList.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:    CPP file for Krb4EditDomainRealmList.h. Contains variables and functions
//					for Kerberos Four Properites
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "Krb4Properties.h"
#include "Krb4EditDomainRealmList.h"
#include "lglobals.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrb4EditDomainRealmList dialog


CKrb4EditDomainRealmList::CKrb4EditDomainRealmList(LPSTR editItem, CWnd* pParent)
	: CDialog(CKrb4EditDomainRealmList::IDD, pParent)
{
    m_startup = TRUE;
	m_editItem = _T("");

	// Parse the passed in item
	LPSTR pEditItem = editItem;
	LPSTR findSpace = strchr(editItem, ' ');
	if (findSpace)
	  *findSpace = 0;
	else
	{
////@#+This hack doesn't seem right
#ifndef NO_KRB4

		 LeashErrorBox("This is a defective entry in file",
					   CKrb4ConfigFileLocation::m_newKrbrealmFile);
#endif
		 ASSERT(0);
		 m_initDomainHost = m_newDomainHost = editItem;
		 m_initRealm = m_newRealm = _T("");
		 return;
	}

	m_initDomainHost = m_newDomainHost = editItem;  // first token

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

	m_initRealm = m_newRealm = pEditItem; // second token

	//{{AFX_DATA_INIT(CKrb4EditDomainRealmList)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

void CKrb4EditDomainRealmList::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4EditDomainRealmList)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb4EditDomainRealmList, CDialog)
	//{{AFX_MSG_MAP(CKrb4EditDomainRealmList)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_REALMNAME, OnChangeEditDefaultRealm)
	ON_EN_CHANGE(IDC_EDIT_DOMAINHOST, OnChangeEditRealmHostname)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrb4EditDomainRealmList message handlers


void CKrb4EditDomainRealmList::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}

BOOL CKrb4EditDomainRealmList::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_EDIT_REALMNAME, m_newRealm);
	SetDlgItemText(IDC_EDIT_DOMAINHOST, m_newDomainHost);

	return TRUE;
}

void CKrb4EditDomainRealmList::OnChangeEditDefaultRealm()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_REALMNAME, m_newRealm);
}

void CKrb4EditDomainRealmList::OnChangeEditRealmHostname()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_DOMAINHOST, m_newDomainHost);
}

void CKrb4EditDomainRealmList::OnOK()
{
	m_newRealm.TrimLeft();
	m_newRealm.TrimRight();
	m_newDomainHost.TrimLeft();
	m_newDomainHost.TrimRight();

	if (m_newRealm.IsEmpty() || m_newDomainHost.IsEmpty())
	{ // stay
		MessageBox("OnOK::Both Domain-Host and Realm fields must be filled in!",
                    "Leash", MB_OK);
	}
	else if (-1 != m_newRealm.Find(' ') || -1 != m_newDomainHost.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Leash", MB_OK);
	}

	else
	  CDialog::OnOK(); // exit

	m_editItem = m_newDomainHost + " " + m_newRealm;
}
