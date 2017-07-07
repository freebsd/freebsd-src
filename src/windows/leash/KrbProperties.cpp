//	File:			KrbProperties.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbProperties.h. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	02/01/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "KrbProperties.h"
#include "Krb4Properties.h"

#include "Leash.h"
#include "wshelper.h"
#include "lglobals.h"
#include "reminder.h"

CHAR CKrbProperties::m_krbPath[MAX_PATH];
CHAR CKrbProperties::m_krbrealmPath[MAX_PATH];
BOOL CKrbProperties::KrbPropertiesOn;

///////////////////////////////////////////////////////////////////////
// CKrbProperties

IMPLEMENT_DYNAMIC(CKrbProperties, CPropertySheet)
CKrbProperties::CKrbProperties(UINT nIDCaption, CWnd* pParentWnd,
                               UINT iSelectPage)
:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CKrbProperties::CKrbProperties(LPCTSTR pszCaption, CWnd* pParentWnd,
							   UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	KrbPropertiesOn = FALSE;

#ifdef COMMENT
	// If this will not be fatal, then it does not need to be performed here.
	if (CLeashApp::m_hKrb5DLL)
	{
		char *realm = NULL;
		pkrb5_get_default_realm(CLeashApp::m_krbv5_context, &realm);

		if (!realm)
		{
			MessageBox("CKrbProperties::Unable to determine default Kerberos REALM.\
                        \n Consult your Administrator!",
					   "Error", MB_OK);
			// I don't think this is necessarily fatal.  - jaltman
			// return;
		}
	}
#endif /* COMMENT */

#ifndef NO_KRB4
    CLeashApp::GetKrb4ConFile(m_krbPath,sizeof(m_krbPath));
    CLeashApp::GetKrb4RealmFile(m_krbrealmPath,sizeof(m_krbrealmPath));
#endif

	AddPage(&m_configOptions);
	AddPage(&m_miscConfigOpt);

#ifndef NO_KRB4
	if (CLeashApp::m_hKrb4DLL && !CLeashApp::m_hKrb5DLL)
	{
		AddPage(&m_krb4RealmHostMaintenance);
		AddPage(&m_krb4DomainRealmMaintenance);
	}
	else
#endif
	if (CLeashApp::m_hKrb5DLL)
	{
		AddPage(&m_realmHostMaintenance);
		AddPage(&m_domainRealmMaintenance);
	}

	KrbPropertiesOn = TRUE;
}

CKrbProperties::~CKrbProperties()
{
	KrbPropertiesOn = FALSE;
}

void CKrbProperties::OnHelp()
{
    AfxGetApp()->WinHelp(HID_KERBEROS_PROPERTIES_COMMAND);
}


BEGIN_MESSAGE_MAP(CKrbProperties, CPropertySheet)
	//{{AFX_MSG_MAP(CKrbProperties)
		// NOTE - the ClassWizard will add and remove mapping macros here.
    ON_COMMAND(ID_HELP, OnHelp)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////
// CKrbProperties message handlers
