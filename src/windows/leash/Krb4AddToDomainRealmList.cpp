//	File:			Krb4AddToDomainRealmList.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for Krb4AddToDomainRealmList.h. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "Krb4AddToDomainRealmList.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrb4AddToDomainRealmList dialog


CKrb4AddToDomainRealmList::CKrb4AddToDomainRealmList(CWnd* pParent /*=NULL*/)
	: CDialog(CKrb4AddToDomainRealmList::IDD, pParent)
{
	m_newRealm = _T("");
	m_newDomainHost = _T("");
	m_startup = TRUE;


	//{{AFX_DATA_INIT(CKrb4AddToDomainRealmList)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CKrb4AddToDomainRealmList::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4AddToDomainRealmList)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb4AddToDomainRealmList, CDialog)
	//{{AFX_MSG_MAP(CKrb4AddToDomainRealmList)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_DOMAINHOSTNAME, OnChangeEditDomainhostname)
	ON_EN_CHANGE(IDC_EDIT_DOMAINREALMNAME, OnChangeEditDomainrealmname)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrb4AddToDomainRealmList message handlers

void CKrb4AddToDomainRealmList::OnChangeEditDomainhostname()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_DOMAINHOSTNAME, m_newDomainHost);
}

void CKrb4AddToDomainRealmList::OnChangeEditDomainrealmname()
{
	if (!m_startup)
	  GetDlgItemText(IDC_EDIT_DOMAINREALMNAME, m_newRealm);
}

void CKrb4AddToDomainRealmList::OnOK()
{
	//if (m_newRealm.IsEmpty)

	m_newRealm.TrimLeft();
	m_newRealm.TrimRight();
	m_newDomainHost.TrimLeft();
	m_newDomainHost.TrimRight();

	if (m_newRealm.IsEmpty() || m_newDomainHost.IsEmpty())
	{ // stay
		MessageBox("OnOK::Both Realm and Domain-Host fields must be filled in!",
                    "Leash", MB_OK);
	}
	else if (-1 != m_newRealm.Find(' ') || -1 != m_newDomainHost.Find(' '))
	{ // stay
		MessageBox("OnOK::Illegal space found!", "Leash", MB_OK);
	}
	else
	  CDialog::OnOK(); // exit
}

void CKrb4AddToDomainRealmList::OnCancel()
{

	CDialog::OnCancel();
}

void CKrb4AddToDomainRealmList::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);
	m_startup = FALSE;
}
