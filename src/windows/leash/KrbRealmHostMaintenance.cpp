//	**************************************************************************************
//	File:			KrbRealmHostMaintenance.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbRealmHostMaintenance.h. Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "KrbProperties.h"
#include "Krb4Properties.h"
#include "KrbAddRealm.h"
#include "KrbAddHostServer.h"
#include "KrbRealmHostMaintenance.h"
#include "KrbEditRealm.h"
#include "KrbEditHostServer.h"
#include "KrbConfigOptions.h"

#include "lglobals.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CKrbRealmHostMaintenance dialog


IMPLEMENT_DYNCREATE(CKrbRealmHostMaintenance, CPropertyPage)

CKrbRealmHostMaintenance::CKrbRealmHostMaintenance()
 : CPropertyPage(CKrbRealmHostMaintenance::IDD)
{
	m_isRealmListBoxInFocus	= FALSE;
	m_isStart = TRUE;
	m_theAdminServer = _T("");
	m_theAdminServerMarked = _T("");
    m_initDnsKdcLookup = 0;
    m_newDnsKdcLookup = 0;

	m_KDCHostList.initOtherListbox(this, &m_KDCRealmList);
}

CKrbRealmHostMaintenance::~CKrbRealmHostMaintenance()
{
}

void CKrbRealmHostMaintenance::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbRealmHostMaintenance)
	DDX_Control(pDX, IDC_LIST_KDC_REALM, m_KDCRealmList);
	DDX_Control(pDX, IDC_LIST_KDC_HOST, m_KDCHostList);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CKrbRealmHostMaintenance, CPropertyPage)
	//{{AFX_MSG_MAP(CKrbRealmHostMaintenance)
	ON_BN_CLICKED(IDC_BUTTON_REALM_HOST_ADD, OnButtonRealmHostAdd)
	ON_BN_CLICKED(IDC_BUTTON_REALM_EDIT, OnButtonRealmHostEdit)
	ON_BN_CLICKED(ID_BUTTON_REALM_REMOVE, OnButtonRealmHostRemove)
	ON_LBN_SELCHANGE(IDC_LIST_KDC_REALM, OnSelchangeListKdcRealm)
	ON_BN_CLICKED(IDC_BUTTON_ADMINSERVER, OnButtonAdminserver)
	ON_LBN_SETFOCUS(IDC_LIST_KDC_REALM, OnSetfocusListKdcRealm)
	ON_BN_CLICKED(IDC_BUTTON_KDCHOST_ADD, OnButtonKdchostAdd)
	ON_BN_CLICKED(IDC_BUTTON_KDCHOST_REMOVE, OnButtonKdchostRemove)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE_ADMINSERVER, OnButtonRemoveAdminserver)
	ON_LBN_SELCHANGE(IDC_LIST_KDC_HOST, OnSelchangeListKdcHost)
	ON_BN_CLICKED(IDC_BUTTON_KDCHOST_EDIT, OnButtonKdchostEdit)
	ON_LBN_DBLCLK(IDC_LIST_KDC_REALM, OnDblclkListKdcRealm)
	ON_LBN_DBLCLK(IDC_LIST_KDC_HOST, OnDblclkListKdcHost)
	ON_WM_KEYDOWN()
	ON_WM_CANCELMODE()
	ON_BN_CLICKED(IDC_BUTTON_REALMHOST_MAINT_HELP, OnButtonRealmhostMaintHelp)
    ON_BN_CLICKED(IDC_DNS_KDC, OnCheckDnsKdcLookup)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CKrbRealmHostMaintenance message handlers

BOOL CKrbRealmHostMaintenance::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	const char*  rootSection[] = {"realms", NULL};
	const char** rootsec = rootSection;
	char **sections = NULL,
		 **cpp = NULL,
          *value = NULL;

	long retval = pprofile_get_subsection_names(CLeashApp::m_krbv5_profile,
						    rootsec, &sections);

	if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnInitDialog::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return TRUE;
	}

	for (cpp = sections; *cpp; cpp++)
	{
		if (LB_ERR == m_KDCRealmList.AddString(*cpp))
		{
			MessageBox("OnInitDialog::Can't add to Kerberos Realm Listbox",
						"Leash", MB_OK);
			return FALSE;
		}
	}

	pprofile_free_list(sections);

    // Determine the starting value for DNS KDC Lookup Checkbox
    retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
                               "dns_lookup_kdc", 0, 0, &value);
    if (value == 0 && retval == 0)
        retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
                                  "dns_fallback", 0, 0, &value);
    if (value == 0) {
        m_initDnsKdcLookup = m_newDnsKdcLookup = 1;
    } else {
        m_initDnsKdcLookup = m_newDnsKdcLookup = config_boolean_to_int(value);
        pprofile_release_string(value);
    }
    CheckDlgButton(IDC_DNS_KDC, m_initDnsKdcLookup);

    // Compaire Krb Four with what's in the Krb Five Profile Linklist
	// and add to m_KDCRealmList if needed.
	m_KDCRealmList.SetCurSel(0);

	if (!m_KDCRealmList.GetCount())
	{
		GetDlgItem(IDC_BUTTON_REALM_EDIT)->EnableWindow(FALSE);
	}
	else if (1 >= m_KDCRealmList.GetCount())
	{
		GetDlgItem(ID_BUTTON_REALM_REMOVE)->EnableWindow(FALSE);
	}
	else
	{
		GetDlgItem(ID_BUTTON_REALM_REMOVE)->EnableWindow();
		GetDlgItem(IDC_BUTTON_REALM_EDIT)->EnableWindow();
	}


	if (!m_KDCHostList.GetCount())
	{
		GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_KDCHOST_EDIT)->EnableWindow(FALSE);
	}
	else
	{
		GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow();
		GetDlgItem(IDC_BUTTON_KDCHOST_EDIT)->EnableWindow();
	}


	return TRUE;
}

BOOL CKrbRealmHostMaintenance::OnApply()
{
    char theSection[REALM_SZ + 1];
    const char*  adminServer[] = {"realms", theSection, ADMIN_SERVER, NULL};
    const char*  Section[] = {"realms", theSection, "kdc", NULL}; //theSection
    const char** section = Section;
    const char** adminServ = adminServer;

    if (!CLeashApp::m_krbv5_profile) {
        CHAR confname[MAX_PATH];
        if (!CLeashApp::GetProfileFile(confname, sizeof(confname)))
        {
	        const char *filenames[2];
		    filenames[0] = confname;
			filenames[1] = NULL;
			pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
		}
	}

	/*
    // Safety check for empty section (may not be need, but left it in anyway)
	INT maxRealms = m_KDCRealmList.GetCount();
	for (INT realm = 0; realm < maxRealms; realm++)
	{
		m_KDCRealmList.GetText(realm, theSection);
		long retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
			                               section, &values);
        pprofile_free_list(values);

        if (PROF_NO_RELATION == retval)
		{
			if (IDYES == AfxMessageBox("One or more Realms do not have any corresponing Servers!!!\n\nContinue?",
				MB_YESNO))
			  break;
			else
			  return TRUE;
		}

		if (retval && PROF_NO_RELATION != retval)
		{
			MessageBox("OnApply::There is an error, profile will not be saved!!!\nIf this error persist, contact your administrator.",
				       "Error", MB_OK);
			return TRUE;
		}
    }
    */

    long retval = pprofile_flush(CLeashApp::m_krbv5_profile);

    if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnApply::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
	}

#ifndef NO_KRB4
    // Save to Kerberos Four config. file "Krb.con"
    CStdioFile krbCon;
	if (!krbCon.Open(CKrbProperties::m_krbPath, CFile::modeCreate |
											    CFile::modeNoTruncate |
												CFile::modeReadWrite))
	{
		LeashErrorBox("OnApply::Can't open Configuration File",
					  CKrbProperties::m_krbPath);
		return TRUE;
	}

	krbCon.SetLength(0);

	krbCon.WriteString(CKrbConfigOptions::m_newDefaultRealm);
	krbCon.WriteString("\n");

	for (INT maxItems = m_KDCRealmList.GetCount(), item = 0; item < maxItems; item++)
	{
        char **values = NULL,
		 **cpp = NULL,
	         **admin = NULL;

        if (LB_ERR == m_KDCRealmList.GetText(item, theSection))
		  ASSERT(0);

        retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
									  section, &values);

        if (retval && PROF_NO_RELATION != retval)
		{
			MessageBox("OnApply::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
				       "Leash", MB_OK);
		}

	    retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
									  adminServ , &admin);

		if (retval && PROF_NO_RELATION != retval)
		{
			MessageBox("OnApply::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
				       "Leash", MB_OK);
		}

        char* pSemiCl = NULL;
		if (admin)
        {
            if (*admin)
            {
                if ((pSemiCl = strchr(*admin, ':')))
	              *pSemiCl = 0;
            }
        }


        char hostKdc[MAX_HSTNM];
		if (values)
        for (cpp = values; *cpp; cpp++)
		{
			strcpy(hostKdc, *cpp);

            if ((pSemiCl = strchr(hostKdc, ':')))
		      *pSemiCl = 0;

            if (admin)
            {
                if (*admin)
                {
                    if (0 == stricmp(hostKdc, *admin))
                      strcat(hostKdc, " admin server");
                }
            }

            CString kdcHost = theSection;
			kdcHost += " ";
			kdcHost += hostKdc;

			krbCon.WriteString(kdcHost);
			krbCon.WriteString("\n");
		}

	if (values)
          pprofile_free_list(values);

        if (admin)
          pprofile_free_list(admin);
    }

    if ( m_newDnsKdcLookup )
        krbCon.WriteString(".KERBEROS.OPTION. dns\n");

    krbCon. Close();
#endif // NO_KRB4
    return TRUE;
}

void CKrbRealmHostMaintenance::OnCancel()
{
    CHAR fileName[MAX_PATH];
    if (CLeashApp::GetProfileFile(fileName, sizeof(fileName)))
    {
        MessageBox("Can't locate Kerberos Five Config. file!", "Error", MB_OK);
        return;
    }


    long retval = 0;
    if (CLeashApp::m_krbv5_profile)
        pprofile_abandon(CLeashApp::m_krbv5_profile);

    /*
    if (retval)
	{
		MessageBox("OnButtonRealmHostAdd::There is an error, profile will not be abandon!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}
	*/

    const char *filenames[2];
    filenames[0] = fileName;
    filenames[1] = NULL;
	retval = pprofile_init(filenames, &CLeashApp::m_krbv5_profile);

    if (retval)
	{
		MessageBox("OnButtonRealmHostAdd::There is an error, profile will not be initialized!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}


	CPropertyPage::OnCancel();
}

void CKrbRealmHostMaintenance::OnCheckDnsKdcLookup()
{
	const char*  dnsLookupKdc[] = {"libdefaults","dns_lookup_kdc",NULL};

    m_newDnsKdcLookup = (BOOL)IsDlgButtonChecked(IDC_DNS_KDC);

	long retval = pprofile_clear_relation(CLeashApp::m_krbv5_profile,
										  dnsLookupKdc);

    if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnButtonAdminserver::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Error", MB_OK);
		return;
	}

	retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
                                    dnsLookupKdc,
                                    m_newDnsKdcLookup ? "true" : "false");

	if (retval)
	{ // this might not be a good way to handle this type of error
		MessageBox("OnButtonAdminserver::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Error", MB_OK);
		return;
	}
    SetModified(TRUE);
}

void CKrbRealmHostMaintenance::OnButtonRealmHostAdd()
{
    m_KDCRealmList.SetFocus();

    CKrbAddRealm addToRealmHostList;
	if (IDOK == addToRealmHostList.DoModal())
	{
		char theSection[REALM_SZ + 1];
		const char* Section[] = {"realms", theSection, NULL};
		const char** section = Section;


	    if (!CLeashApp::m_krbv5_profile) {
		CHAR confname[MAX_PATH];
		if (!CLeashApp::GetProfileFile(confname, sizeof(confname)))
		{
		    const char *filenames[2];
		    filenames[0] = confname;
		    filenames[1] = NULL;
		    pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
		}
	    }

		CString newRealm; // new section in the profile linklist
		newRealm = addToRealmHostList.GetNewRealm();

		if (LB_ERR != m_KDCRealmList.FindStringExact(-1, newRealm))
		{
			MessageBox("We can't have duplicate Realms!\nYour entry was not saved to list.",
                       "Leash", MB_OK);
			return;
		}

		if (addToRealmHostList.GetNewRealm().IsEmpty())
		  ASSERT(0);

		strcpy(theSection, newRealm);
		long retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
						     section, NULL);

		if (retval)
		{
			MessageBox("OnButtonRealmHostAdd::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
				       "Leash", MB_OK);
			return;
		}

		if (LB_ERR == m_KDCRealmList.AddString(newRealm))
		  ASSERT(0);

		if (LB_ERR == m_KDCRealmList.SetCurSel(m_KDCRealmList.FindStringExact(-1, newRealm)))
		  ASSERT(0);

		MessageBox("You must now add a Kerberos Host Server or Realm you just added will be removed!!!",
				   "Leash", MB_OK);

		m_KDCHostList.ResetContent();
        if (OnButtonKdchostAddInternal())
		{ // Cancel

			long retval = pprofile_rename_section(CLeashApp::m_krbv5_profile,
												   section, NULL);

			if (retval)
			{
				MessageBox("OnButtonRealmHostRemove::There is an error, profile will not be saved!!!\
                            \nIf this error persist, contact your administrator.",
						   "Leash", MB_OK);
				return;
			}

			if (LB_ERR == m_KDCRealmList.DeleteString(m_KDCRealmList.GetCurSel()))
			  ASSERT(0);

			m_KDCRealmList.SetCurSel(0);
		}

		OnSelchangeListKdcRealm();
		SetModified(TRUE);
	}

	if (1 >= m_KDCRealmList.GetCount())
	{
		GetDlgItem(ID_BUTTON_REALM_REMOVE)->EnableWindow(FALSE);
	}
	else
	{
		GetDlgItem(ID_BUTTON_REALM_REMOVE)->EnableWindow();
		GetDlgItem(IDC_BUTTON_REALM_EDIT)->EnableWindow();
	}
}

void CKrbRealmHostMaintenance::OnButtonKdchostAdd()
{
    OnButtonKdchostAddInternal();
}

bool CKrbRealmHostMaintenance::OnButtonKdchostAddInternal()
{
	CString newHost; // new section in the profile linklist
    CKrbAddHostServer addHostServer;
	if (IDOK == addHostServer.DoModal())
	{ // OK
		char theSection[MAX_HSTNM + 1];
		const char* Section[] = {"realms", theSection, "kdc", NULL};
		const char** section = Section;

		if (addHostServer.GetNewHost().IsEmpty())
		  ASSERT(0);

		newHost = addHostServer.GetNewHost();

		if (LB_ERR != m_KDCHostList.FindStringExact(-1, newHost))
		{
			MessageBox("We can't have duplicate Host Servers for the same Realm!\
                        \nYour entry was not saved to list.",
                       "Leash", MB_OK);
			return true;
		}

		m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);
	    long retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
									         section, addHostServer.GetNewHost());

		if (retval)
		{
			MessageBox("OnButtonKdchostAdd::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
					   "Leash", MB_OK);

			return true;
		}

        if (LB_ERR == m_KDCHostList.AddString(newHost))
		  ASSERT(0);

		SetModified(TRUE);
	}
	else
	  return true;

	if (m_KDCHostList.GetCount() > 1)
    {
        m_KDCHostList.SetCurSel(m_KDCHostList.FindStringExact(-1, newHost));
	    m_KDCHostList.SetFocus();
        OnSelchangeListKdcHost();

        GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow();
    }

    if (1 == m_KDCRealmList.GetCount())
	{
        GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow();
		GetDlgItem(IDC_BUTTON_KDCHOST_EDIT)->EnableWindow();
	}

	return false;
}

void CKrbRealmHostMaintenance::OnButtonRealmHostEdit()
{
	INT selItemIndex = m_KDCRealmList.GetCurSel();
	CString selItem;

    m_KDCHostList.SetFocus();
    //m_KDCRealmList.SetFocus();
    //m_KDCHostList.SetCurSel(0);
    m_KDCRealmList.GetText(selItemIndex, selItem);

	CKrbEditRealm editRealmHostList(selItem);

	if (IDOK == editRealmHostList.DoModal())
	{
		char theSection[REALM_SZ + 1];
		const char* Section[] = {"realms", theSection, NULL};
		const char** section = Section;

		CString editedRealm = editRealmHostList.GetEditedItem();

		if (0 != editedRealm.CompareNoCase(selItem) &&
			LB_ERR != m_KDCRealmList.FindStringExact(-1, editedRealm))
		{
			MessageBox("We can't have duplicate Realms!\nYour entry was not saved to list.",
                       "Leash", MB_OK);
			return;
		}

		strcpy(theSection, selItem);

		long retval = pprofile_rename_section(CLeashApp::m_krbv5_profile,
											   section, editRealmHostList.GetEditedItem());

		if (retval)
		{
			MessageBox("OnButtonRealmHostEdit::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
					   "Leash", MB_OK);
			return;
		}

		m_KDCRealmList.DeleteString(selItemIndex);
		m_KDCRealmList.AddString(editedRealm);
		selItemIndex = m_KDCRealmList.FindStringExact(-1, editedRealm);
		m_KDCRealmList.SetCurSel(selItemIndex);

		CKrbConfigOptions::ResetDefaultRealmComboBox();
		SetModified(TRUE);
	}
}

void CKrbRealmHostMaintenance::OnDblclkListKdcRealm()
{
	OnButtonRealmHostEdit();
}

void CKrbRealmHostMaintenance::OnButtonKdchostEdit()
{
	INT selItemIndex = m_KDCHostList.GetCurSel();
	CHAR OLD_VALUE[MAX_HSTNM + 1];
	CString editedHostServer;
	CString _adminServer;

    m_KDCHostList.SetFocus();
    m_KDCHostList.GetText(selItemIndex, OLD_VALUE);

	LPSTR pOLD_VALUE = strchr(OLD_VALUE, ' ');
	if (pOLD_VALUE)
	{
		*pOLD_VALUE = 0;
		_adminServer = pOLD_VALUE + 1;
	}

	CString selItem = OLD_VALUE;
	CKrbEditHostServer editHostServerList(selItem);

	if (IDOK == editHostServerList.DoModal())
	{
		char theSection[REALM_SZ + 1];
		const char*  adminServer[] = {"realms", theSection, ADMIN_SERVER, NULL};
		const char* Section[] = {"realms", theSection, "kdc", NULL};
		const char** section = Section;
		const char** adminServ = adminServer;

		editedHostServer = editHostServerList.GetEditedItem();

		if (0 != editedHostServer.CompareNoCase(selItem) &&
			LB_ERR != m_KDCHostList.FindStringExact(-1, editedHostServer))
		{
			MessageBox("We can't have duplicate Host Servers for the same Realm!\
                        \nYour entry was not saved to list.",
                       "Leash", MB_OK);
			return;
		}

		m_KDCHostList.DeleteString(selItemIndex);
		m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);

		if (!_adminServer.IsEmpty())
		{ // there is a admin_server
			editedHostServer += " ";
			editedHostServer += _adminServer;

			long retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
													 adminServ, OLD_VALUE,	editHostServerList.GetEditedItem());
			if (retval)
			{
				MessageBox("OnButtonKdchostEdit::There is an error, profile will not be saved!!!\
                            \nIf this error persist, contact your administrator.",
						   "Leash", MB_OK);
				return;
			}
		}

		long retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
									        section, OLD_VALUE,	editHostServerList.GetEditedItem());

		if (retval)
		{
			MessageBox("OnButtonKdchostEdit::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
					   "Leash", MB_OK);
			return;
		}

		m_KDCHostList.InsertString(selItemIndex, editedHostServer);
		m_KDCHostList.SetCurSel(selItemIndex);

		OnSelchangeListKdcHost();
		SetModified(TRUE);
	}
}

void CKrbRealmHostMaintenance::OnDblclkListKdcHost()
{
	OnButtonKdchostEdit();
}

void CKrbRealmHostMaintenance::OnButtonRealmHostRemove()
{
	char theSection[REALM_SZ + 1];
	const char* Section[] = {"realms", theSection, NULL};
	const char** section = Section;

    m_KDCRealmList.SetFocus();
    m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);

	CString RealmMsg;
	RealmMsg.Format("Your about to remove a Realm, \"%s\", and all it's dependents from the list!\n\nContinue?",
					theSection);

	if (IDYES != AfxMessageBox(RealmMsg, MB_YESNO))
	  return;

	long retval = pprofile_rename_section(CLeashApp::m_krbv5_profile,
										   section, NULL);

	if (retval)
	{
		MessageBox("OnButtonRealmHostRemove::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}

	INT curSel = m_KDCRealmList.GetCurSel();

	if (LB_ERR == m_KDCRealmList.DeleteString(curSel))
	  ASSERT(0);// Single Sel Listbox

	if (-1 == m_KDCRealmList.SetCurSel(curSel))
	  m_KDCRealmList.SetCurSel(curSel - 1);

	SetModified(TRUE);

	if (!m_KDCRealmList.GetCount())
	{
		GetDlgItem(IDC_BUTTON_REALM_EDIT)->EnableWindow(FALSE);
	}
	if (1 >= m_KDCRealmList.GetCount())
	{
		OnSelchangeListKdcRealm();
		GetDlgItem(ID_BUTTON_REALM_REMOVE)->EnableWindow(FALSE);
	}
	else
	  OnSelchangeListKdcRealm();
}

void CKrbRealmHostMaintenance::OnButtonKdchostRemove()
{
	char theSection[REALM_SZ + 1];
	const char*  adminServer[] = {"realms", theSection, ADMIN_SERVER, NULL};
	const char* Section[] = {"realms", theSection, "kdc", NULL};
	const char** section = Section;
	const char** adminServ = adminServer;
	CHAR OLD_VALUE[MAX_HSTNM + 1];
	CString serverHostMsg;
	CString serverHost;
	CString _adminServer;

    m_KDCHostList.GetText(m_KDCHostList.GetCurSel(), serverHost);
	serverHostMsg.Format("Your about to remove Server \"%s\" from the list!\n\nContinue?",
						  serverHost);

	if (IDYES != AfxMessageBox(serverHostMsg, MB_YESNO))
	  return;

	m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);
	INT curSel = m_KDCHostList.GetCurSel();
	m_KDCHostList.GetText(curSel, OLD_VALUE);

	LPSTR pOLD_VALUE = strchr(OLD_VALUE, ' ');
	if (pOLD_VALUE)
	{
		*pOLD_VALUE = 0;
		_adminServer = pOLD_VALUE + 1;
	}

	long retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
										    section, OLD_VALUE, NULL);
	if (retval)
	{
		MessageBox("OnButtonKdchostRemove::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Leash", MB_OK);
		return;
	}

	if (!_adminServer.IsEmpty())
	{ // there is a admin_server
		retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
										   adminServ, OLD_VALUE, NULL);
		if (retval)
		{
			MessageBox("OnButtonKdchostRemove::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
					   "Error", MB_OK);
			return;
		}
	}

	m_KDCHostList.DeleteString(curSel);

	if (-1 == m_KDCHostList.SetCurSel(curSel))
	  m_KDCHostList.SetCurSel(curSel - 1);

	SetModified(TRUE);

	if (!m_KDCHostList.GetCount())
	{
		GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_KDCHOST_EDIT)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_ADMINSERVER)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_REMOVE_ADMINSERVER)->EnableWindow(FALSE);
	}
	else if (m_KDCHostList.GetCount() <= 1)
	  GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow(FALSE);

    OnSelchangeListKdcHost();
}

BOOL CKrbRealmHostMaintenance::PreTranslateMessage(MSG* pMsg)
{
	if (m_isStart)
	{
		OnSelchangeListKdcRealm();
		m_isStart = FALSE;
	}

	return CPropertyPage::PreTranslateMessage(pMsg);
}

void CKrbRealmHostMaintenance::OnSelchangeListKdcRealm()
{
	char theSection[REALM_SZ + 1];
	const char*  adminServer[] = {"realms", theSection, ADMIN_SERVER, NULL};
	const char*  Section[] = {"realms", theSection, "kdc", NULL}; //theSection
	const char** section = Section;
	const char** adminServ = adminServer;
	char **values = NULL,
		 **adminValue = NULL,
		 **cpp = NULL;

	m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);

	long retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
	                                   section, &values);

	if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnSelchangeListKdcRealm::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Error", MB_OK);
		return;
	}

	m_KDCHostList.ResetContent();

    if ( !retval && values ) {
        retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
                                      adminServ, &adminValue);

        if (retval && PROF_NO_RELATION != retval)
        {
            MessageBox("OnSelchangeListKdcRealm::There is an error, profile will not be saved!!!\
                        \nIf this error persist, contact your administrator.",
                        "Error", MB_OK);
            return;
        }

        m_theAdminServer = _T("");
        m_theAdminServerMarked = _T("");

        for (cpp = values; *cpp; cpp++)
        {
            CString kdcHost = *cpp;

            if (adminValue && 0 == strcmp(*adminValue, *cpp))
            {
                m_theAdminServer = kdcHost;
                kdcHost += " ";
                kdcHost += ADMIN_SERVER;

                m_theAdminServerMarked = kdcHost;
            }

            if (LB_ERR == m_KDCHostList.AddString(kdcHost))
            {
                MessageBox("OnSelchangeListKdcRealm::Can't add Realm to Listbox",
                            "Error", MB_OK);
            }
        }

        pprofile_free_list(values);
    } else {
        GetDlgItem(IDC_BUTTON_REALM_HOST_ADD)->EnableWindow(TRUE);
        GetDlgItem(ID_BUTTON_REALM_REMOVE)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_REALM_EDIT)->EnableWindow(FALSE);
    }
    CKrbConfigOptions::ResetDefaultRealmComboBox();

	GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_KDCHOST_EDIT)->EnableWindow(FALSE);
}

void CKrbRealmHostMaintenance::OnSelchangeListKdcHost()
{
	CString adminServer;
	m_KDCHostList.GetText(m_KDCHostList.GetCurSel(), adminServer);

	if (-1 != adminServer.Find(ADMIN_SERVER))
	{
		GetDlgItem(IDC_BUTTON_ADMINSERVER)->EnableWindow(FALSE);
		GetDlgItem(IDC_BUTTON_REMOVE_ADMINSERVER)->EnableWindow();
	}
	else
	{
		GetDlgItem(IDC_BUTTON_ADMINSERVER)->EnableWindow();
		GetDlgItem(IDC_BUTTON_REMOVE_ADMINSERVER)->EnableWindow(FALSE);
	}

	if (m_KDCHostList.GetCount() > 1)
	  GetDlgItem(IDC_BUTTON_KDCHOST_REMOVE)->EnableWindow();

	GetDlgItem(IDC_BUTTON_KDCHOST_EDIT)->EnableWindow();
}

void CKrbRealmHostMaintenance::OnSetfocusListKdcRealm()
{
	GetDlgItem(IDC_BUTTON_ADMINSERVER)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_REMOVE_ADMINSERVER)->EnableWindow(FALSE);
}

void CKrbRealmHostMaintenance::OnButtonAdminserver()
{
	// Install new admin.server in profile linklist
	char theSection[REALM_SZ + 1];
	const char* Section[] = {"realms", theSection, ADMIN_SERVER, NULL};
	const char** section = Section;

    m_KDCHostList.SetFocus();
    INT index1 = m_KDCHostList.GetCurSel();
	INT index2 = m_KDCHostList.FindStringExact(-1, m_theAdminServerMarked);

	if (-1 != index2)
	{
		m_KDCHostList.DeleteString(index2);
		if (LB_ERR == m_KDCHostList.InsertString(index2, m_theAdminServer))
		{
			MessageBox("OnButtonAdminserver::Can't add to list!!!",
						  "Error, MB_OK");
		}
	}

	CString makeAdmin;
	m_KDCHostList.GetText(index1, makeAdmin);
	m_KDCHostList.DeleteString(index1);
	m_theAdminServer = makeAdmin;
	makeAdmin += " ";
	makeAdmin += ADMIN_SERVER;
	m_theAdminServerMarked = makeAdmin;

	if (LB_ERR == m_KDCHostList.InsertString(index1, makeAdmin))
	{
		MessageBox("OnButtonAdminserver::Can't add to list!!!",
					  "Error, MB_OK");
	}

	m_KDCHostList.SetCurSel(m_KDCHostList.FindStringExact(-1, makeAdmin)); //index2 -1);
	GetDlgItem(IDC_BUTTON_ADMINSERVER)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_REMOVE_ADMINSERVER)->EnableWindow();

	m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);

	long retval = pprofile_clear_relation(CLeashApp::m_krbv5_profile,
										   section);

    if (retval && PROF_NO_RELATION != retval)
	{
		MessageBox("OnButtonAdminserver::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Error", MB_OK);
		return;
	}

	retval = pprofile_add_relation(CLeashApp::m_krbv5_profile,
					                section, m_theAdminServer);

	if (retval)
	{ // this might not be a good way to handle this type of error
		MessageBox("OnButtonAdminserver::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Error", MB_OK);
		return;
	}

	SetModified(TRUE);
}

void CKrbRealmHostMaintenance::OnButtonRemoveAdminserver()
{
	// Remove admin.server from profile linklist
	char theSection[REALM_SZ + 1];
	const char* Section[] = {"realms", theSection, ADMIN_SERVER, NULL};
	const char** section = Section;

    m_KDCHostList.SetFocus();
    m_KDCRealmList.GetText(m_KDCRealmList.GetCurSel(), theSection);

	long retval = pprofile_clear_relation(CLeashApp::m_krbv5_profile,
										   section);

	if (retval)
	{
		MessageBox("OnButtonRemoveAdminserver::There is an error, profile will not be saved!!!\
                    \nIf this error persist, contact your administrator.",
				   "Error", MB_OK);
		return;
	}

    INT index = m_KDCHostList.GetCurSel();
	m_KDCHostList.DeleteString(index);

    if (LB_ERR == m_KDCHostList.InsertString(index, m_theAdminServer))
	{
		MessageBox("OnButtonRemoveAdminserver::Can't add to list!!!",
					  "Error, MB_OK");


	}

	m_theAdminServerMarked = m_theAdminServer;
	m_KDCHostList.SetCurSel(m_KDCHostList.FindStringExact(-1, m_theAdminServer));
	GetDlgItem(IDC_BUTTON_ADMINSERVER)->EnableWindow();
	GetDlgItem(IDC_BUTTON_REMOVE_ADMINSERVER)->EnableWindow(FALSE);

	SetModified(TRUE);
}



void CKrbRealmHostMaintenance::OnButtonRealmhostMaintHelp()
{
	MessageBox("No Help Available!", "Note", MB_OK);
}
