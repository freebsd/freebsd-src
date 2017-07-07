//	**************************************************************************************
//	File:			KrbConfigOptions.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbProperties.h. Contains variables and functions
//					for Kerberos Four and Five Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	2/01/98	ADL		Original
//	**************************************************************************************


#include "stdafx.h"
#include "Leash.h"
#include "KrbProperties.h"
#include "KrbConfigOptions.h"
#include "LeashFileDialog.h"
#include "LeashMessageBox.h"
#include "wshelper.h"
#include "lglobals.h"
#include <io.h>
#include <direct.h>
#include "reminder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


///////////////////////////////////////////////////////////////////////
// CKrbConfigOptions property page

CString CKrbConfigOptions::m_newDefaultRealm;
CString CKrbConfigOptions::m_hostServer;
CComboBox CKrbConfigOptions::m_krbRealmEditbox;
BOOL CKrbConfigOptions::m_profileError;
BOOL CKrbConfigOptions::m_dupEntriesError;

IMPLEMENT_DYNCREATE(CKrbConfigOptions, CPropertyPage)

CKrbConfigOptions::CKrbConfigOptions() : CPropertyPage(CKrbConfigOptions::IDD)
{
	m_initDefaultRealm = _T("");
	m_newDefaultRealm = _T("");
	m_startupPage2 = TRUE;
	m_noKrbFileError = FALSE;
	m_noKrbhostWarning = FALSE;
	m_dupEntriesError = FALSE;
	m_profileError	= FALSE;
	m_noRealm = FALSE;

	//{{AFX_DATA_INIT(CKrbConfigOptions)
	//}}AFX_DATA_INIT
}

CKrbConfigOptions::~CKrbConfigOptions()
{
}

VOID CKrbConfigOptions::DoDataExchange(CDataExchange* pDX)
{
	TRACE("Entering CKrbConfigOptions::DoDataExchange -- %d\n",
	      pDX->m_bSaveAndValidate);
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrbConfigOptions)
	DDX_Control(pDX, IDC_EDIT_DEFAULT_REALM, m_krbRealmEditbox);
	//}}AFX_DATA_MAP
}

static char far * near parse_str(char far*buffer,char far*result)
{
        while (isspace(*buffer))
                buffer++;
        while (!isspace(*buffer))
                *result++=*buffer++;
        *result='\0';
        return buffer;
}

#ifndef NO_KRB4
int krb_get_krbhst(char* h, char* r, int n)
{
	char lbstorage[BUFSIZ];
    char tr[REALM_SZ];
    static FILE *cnffile; /*XXX pbh added static because of MS bug in fgets() */
    static char FAR *linebuf; /*XXX pbh added static because of MS bug in fgets() */
    int i;
    char *p;

    //static char buffer[80];
    //krb_get_krbconf(buffer);
    memset(lbstorage, '\0', BUFSIZ ); /* 4-22-94 */
    linebuf = &lbstorage[0];

    if ((cnffile = fopen(CKrbProperties::m_krbPath,"r")) == NULL) {
        if (n==1) {
            (void) strcpy(h,KRB_HOST);
            return(KSUCCESS);
        } else {
            return(KFAILURE);
        }
    }
    /* linebuf=(char FAR *)malloc(BUFSIZ); */ /*4-22-94*/
    if (fgets(linebuf,BUFSIZ,cnffile)==NULL) {
            /* free(linebuf); */ /* 4-22-94 */

            return(KFAILURE);
    }
    /* bzero( tr, sizeof(tr) ); */   /* pbh 2-24-93 */
    memset(tr, '\0', sizeof(tr) );
    parse_str(linebuf,tr);
    if (*tr=='\0') {
            return (KFAILURE);
    }
    /* run through the file, looking for the nth server for this realm */
    for (i = 1; i <= n;) {
        if (fgets(linebuf, BUFSIZ, cnffile) == NULL) {
            /* free(linebuf); */ /*4-22-94*/
            (void) fclose(cnffile);
            return(KFAILURE);
        }
        /* bzero( tr, sizeof(tr) ); */   /* pbh 2-24-93 */
        memset(tr, '\0', sizeof(tr) );
        p=parse_str(linebuf,tr);
        if (*tr=='\0')
                continue;
        memset(h, '\0', lstrlen(h) );
        parse_str(p,h);
        if (*tr=='\0')
                continue;
        if (!lstrcmp(tr,r))
                i++;
    }
    /* free(linebuf); */ /*4-22-94*/
    (void) fclose(cnffile);
    return(KSUCCESS);
}
#endif

BOOL CKrbConfigOptions::OnInitDialog()
{
    m_initDefaultRealm = _T("");
    m_newDefaultRealm = _T("");
    m_noKrbFileError = FALSE;
    m_noKrbhostWarning = FALSE;
    m_dupEntriesError = FALSE;
    m_profileError	= FALSE;
    m_noRealm = FALSE;

    CPropertyPage::OnInitDialog();

#ifndef NO_KRB4
	if (CLeashApp::m_hKrb4DLL && !CLeashApp::m_hKrb5DLL)
	{  // Krb4 NOT krb5
		// Fill in all edit boxes
		char krbRealm[REALM_SZ + 1];
		char krbhst[MAX_HSTNM + 1];
		CStdioFile krbCon;
		if (!krbCon.Open(CKrbProperties::m_krbPath, CFile::modeRead))
		{
			SetDlgItemText(IDC_EDIT_DEFAULT_REALM, KRB_REALM);
			SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, KRB_MASTER);
			//CheckRadioButton(IDC_RADIO_ADMIN_SERVER, IDC_RADIO_NO_ADMIN_SERVER, IDC_RADIO_NO_ADMIN_SERVER);
			m_initDefaultRealm = m_newDefaultRealm = KRB_REALM;
		}
		else
		{ // place krbRealm in Edit box
			memset(krbRealm, '\0', sizeof(krbRealm));
			if (!krbCon.ReadString(krbRealm, sizeof(krbRealm)) || '\r' == *krbRealm  ||
				'\n' == *krbRealm || '\0' == *krbRealm)
			{
				SetDlgItemText(IDC_EDIT_DEFAULT_REALM, KRB_REALM);
				SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, KRB_MASTER);
				m_initDefaultRealm = m_newDefaultRealm = KRB_REALM;
			}
			else
			{
				*(krbRealm + strlen(krbRealm) - 1) = 0;
				LPSTR pSpace = strchr(krbRealm, ' ');
				if (pSpace)
				  *pSpace = 0;

				m_initDefaultRealm = m_newDefaultRealm = krbRealm;

				memset(krbhst, '\0', sizeof(krbhst));
				krbCon.Close();

				// Check for Host
				// don't use KRB4 - krb_get_krbhst - would have to re-logon, on file location
				// change, to use this function
				if (KFAILURE == pkrb_get_krbhst(krbhst, krbRealm, 1))
				{
					m_noKrbhostWarning = TRUE;
				}
				else
				{ // place hostname in Edit Box
					//SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, krbhst);

					m_hostServer = krbhst;

					// New stuff to put realms in Combo Box
					CStdioFile krbCon;
					if (!krbCon.Open(CKrbProperties::m_krbPath, CFile::modeRead))
					{
						m_noKrbFileError = TRUE;
						m_noRealm = TRUE;
					} else {

						LPSTR space = NULL;
						CHAR lineBuf[REALM_SZ + MAX_HSTNM + 20];
						CHAR localRealm[REALM_SZ + MAX_HSTNM + 20];
						memset(lineBuf, '\0', sizeof(lineBuf));
						memset(localRealm, '\0', sizeof(localRealm));

						if (krbCon.ReadString(localRealm, sizeof(localRealm)))
						  *(localRealm + strlen(localRealm) - 1) = 0;
						else
						  return FALSE;

						space = strchr(localRealm, ' ');
						if (space)
						  *space = 0;

						while (TRUE)
						{
							if (!krbCon.ReadString(lineBuf, sizeof(lineBuf)))
							  break;

							*(lineBuf + sizeof(lineBuf) - 1) = 0;

							if (strlen(lineBuf) == 0)
								continue;

							space = strchr(lineBuf, ' ');
							if (!space) space = strchr(lineBuf, '\t');
							if (space)
							  *space = 0;
							else
							  ASSERT(0);

                            // skip Kerberos Options
                            if ( !strncmp(".KERBEROS.OPTION.",lineBuf,17) )
                                continue;

							if (CB_ERR == m_krbRealmEditbox.FindStringExact(-1, lineBuf))
							{ // no dups
								if (LB_ERR == m_krbRealmEditbox.AddString(lineBuf))
								{
									MessageBox("OnInitDialog::Can't add to Kerberos Realm Combobox",
										   "Leash", MB_OK);
									return FALSE;
								}
							}
						}

						m_krbRealmEditbox.SelectString(-1, krbRealm);

					} // end of 'else'
				} // end of 'place hostname in Edit Box' else statement
			} // end of 'Check for Host' else statement
		} // end of 'place krbRealm in Edit box' else
	}
	else
#endif
	if (CLeashApp::m_hKrb5DLL)
	{ // Krb5 OR krb5 AND krb4
		char *realm = NULL;
		pkrb5_get_default_realm(CLeashApp::m_krbv5_context, &realm);

		if (!realm)
			m_noRealm = TRUE;

		m_initDefaultRealm = m_newDefaultRealm = realm;

	    if ( !CLeashApp::m_krbv5_profile ) {
            CHAR confname[MAX_PATH];
            if (!CLeashApp::GetProfileFile(confname, sizeof(confname)))
            {
                const char *filenames[2];
                filenames[0] = confname;
                filenames[1] = NULL;
                pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
            }
	    }

        CHAR selRealm[REALM_SZ];
        strcpy(selRealm, m_newDefaultRealm);
        const char*  Section[] = {"realms", selRealm, "kdc", NULL};
        const char** section = Section;
        char **values = NULL;
        char * value  = NULL;

        long retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
                                           section, &values);

        if (!retval && values)
            m_hostServer = *values;
        else {
            int dns_in_use = 0;
            // Determine if we are using DNS for KDC lookups
            retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
                                          "dns_lookup_kdc", 0, 0, &value);
            if (value == 0 && retval == 0)
                retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
                                              "dns_fallback", 0, 0, &value);
            if (value == 0) {
                dns_in_use = 1;
            } else {
                dns_in_use = config_boolean_to_int(value);
                pprofile_release_string(value);
            }
            if (dns_in_use)
                m_hostServer = "DNS SRV record lookups will be used to find KDC";
            else {
                m_hostServer = "No KDC information available";
            }
        }
        SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, m_hostServer);

        if ( realm )
            pkrb5_free_default_realm(CLeashApp::m_krbv5_context, realm);
    }

	// Set host and domain names in their Edit Boxes, respectively.
	char hostName[80]="";
	char domainName[80]="";
	int ckHost = wsh_gethostname(hostName, sizeof(hostName));
	int ckdomain = wsh_getdomainname(domainName, sizeof(domainName));
	CString dot_DomainName = ".";
	dot_DomainName += domainName;

	SetDlgItemText(IDC_EDIT_HOSTNAME, ckHost == 0 ? hostName : "");
	SetDlgItemText(IDC_EDIT_DOMAINNAME, ckdomain == 0 ? dot_DomainName : "");

	return m_noRealm;
}

BOOL CKrbConfigOptions::OnApply()
{
	// If no changes were made, quit this function
	if (0 == m_initDefaultRealm.CompareNoCase(m_newDefaultRealm))
	  return TRUE;

	m_newDefaultRealm.TrimLeft();
	m_newDefaultRealm.TrimRight();

	if (m_newDefaultRealm.IsEmpty())
	{
		MessageBox("OnApply::Your Kerberos Realm field must be filled in!",
                    "Leash", MB_OK);
		m_newDefaultRealm = m_initDefaultRealm;
		SetDlgItemText(IDC_EDIT_DEFAULT_REALM, m_newDefaultRealm);
		return TRUE;
	}

	CStdioFile krbCon;
	if (!krbCon.Open(CKrbProperties::m_krbPath, CFile::modeCreate |
			  CFile::modeNoTruncate |
			  CFile::modeRead))
	{
		LeashErrorBox("OnApply::Can't open configuration file",
					  CKrbProperties::m_krbPath);
		return TRUE;
	}

	CStdioFile krbCon2;
	CString krbCon2File = CKrbProperties::m_krbPath;
	krbCon2File += "___";
	if (!krbCon2.Open(krbCon2File, CFile::modeCreate | CFile::modeWrite))
	{
		LeashErrorBox("OnApply:: Can't open configuration file",
					  CKrbProperties::m_krbPath);
		return TRUE;
	}

	CString readWrite;
	krbCon.ReadString(readWrite);
	krbCon2.WriteString(m_newDefaultRealm);
	krbCon2.WriteString("\n");
	while (krbCon.ReadString(readWrite))
	{
		krbCon2.WriteString(readWrite);
		krbCon2.WriteString("\n");
	}

	krbCon.Close();
	krbCon2.Close();
	krbCon2.Remove(CKrbProperties::m_krbPath);
	krbCon2.Rename(krbCon2File, CKrbProperties::m_krbPath);

	if (CLeashApp::m_hKrb5DLL)
	{ // Krb5 OR krb5 AND krb4
	    if ( !CLeashApp::m_krbv5_profile ) {
            CHAR confname[MAX_PATH];
            if (!CLeashApp::GetProfileFile(confname, sizeof(confname)))
            {
                const char *filenames[2];
                filenames[0] = confname;
                filenames[1] = NULL;
                pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
            }
	    }

		const char*  Names[] = {"libdefaults", "default_realm", NULL};
		const char** names = Names;

		long retval = pprofile_update_relation(CLeashApp::m_krbv5_profile,
							names, m_initDefaultRealm, m_newDefaultRealm);

		if (retval)
		{
			MessageBox("OnApply::The previous value cannot be found, the profile will not be saved!!!\
                        \nIf this error persists after restarting Leash, contact your administrator.",
					   "Leash", MB_OK);
			return TRUE;
		}

		// Save to Kerberos Five config. file "Krb5.ini"
	    retval = pprofile_flush(CLeashApp::m_krbv5_profile);
	}

	m_initDefaultRealm = m_newDefaultRealm;
	return TRUE;
}

void CKrbConfigOptions::OnSelchangeEditDefaultRealm()
{
	if (!m_startupPage2)
	{
		GetDlgItemText(IDC_EDIT_DEFAULT_REALM, m_newDefaultRealm);
		SetModified(TRUE);

		if (CLeashApp::m_hKrb5DLL)
		{
			CHAR selRealm[REALM_SZ];
			strcpy(selRealm, m_newDefaultRealm);
			const char*  Section[] = {"realms", selRealm, "kdc", NULL};
			const char** section = Section;
			char **values = NULL;
			char * value  = NULL;

			long retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
											   section, &values);

			if (!retval && values)
			  SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, *values);
			else {
				int dns_in_use = 0;
				// Determine if we are using DNS for KDC lookups
				retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
							                 "dns_lookup_kdc", 0, 0, &value);
				if (value == 0 && retval == 0)
					retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
								                 "dns_fallback", 0, 0, &value);
				if (value == 0) {
			        dns_in_use = 1;
				} else {
					dns_in_use = config_boolean_to_int(value);
					pprofile_release_string(value);
				}
				if (dns_in_use)
					SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, "DNS SRV record lookups will be used to find KDC");
				else
					SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, "No KDC information available");
			}
		}
#ifndef NO_KRB4
		else
		{
			CHAR krbhst[MAX_HSTNM + 1];
			CHAR krbrlm[REALM_SZ + 1];

			strcpy(krbrlm, CKrbConfigOptions::m_newDefaultRealm);
			memset(krbhst, '\0', sizeof(krbhst));

			// Check for Host
			// don't use KRB4 - krb_get_krbhst - would have to re-logon, on file location
			// change, to use this function
			if (KFAILURE == pkrb_get_krbhst(krbhst, krbrlm, 1))
			{
				MessageBox("OnSelchangeEditDefaultRealm::Unable to find the Host Server for your Default Realm!!!\
                            \n 'Apply' your changes and try again.",
					        "Leash", MB_OK);
			    SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, "");
				return;
			}

			m_hostServer = krbhst;
			if (strlen(krbhst))
			  SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, m_hostServer);
		}
#endif
	}
}

void CKrbConfigOptions::OnEditchangeEditDefaultRealm()
{
	if (!m_startupPage2)
	{
		GetDlgItemText(IDC_EDIT_DEFAULT_REALM, m_newDefaultRealm);
		SetModified(TRUE);
	}
}

void CKrbConfigOptions::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CPropertyPage::OnShowWindow(bShow, nStatus);

	if (CLeashApp::m_hKrb5DLL)
	  ResetDefaultRealmComboBox();

	SetDlgItemText(IDC_EDIT_REALM_HOSTNAME, m_hostServer);
}

void CKrbConfigOptions::ResetDefaultRealmComboBox()
{ // Krb5 is loaded
	// Reset Config Tab's Default Realm Combo Editbox
	const char*  rootSection[] = {"realms", NULL};
	const char** rootsec = rootSection;
	char **sections = NULL,
		 **cpp = NULL,
         *value = 0;
    int dns;

    long retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
                                  "dns_lookup_kdc", 0, 0, &value);
    if (value == 0 && retval == 0)
        retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults",
                                      "dns_fallback", 0, 0, &value);
    if (value == 0) {
        dns = 1;
    } else {
        dns = config_boolean_to_int(value);
        pprofile_release_string(value);
    }

    retval = pprofile_get_subsection_names(CLeashApp::m_krbv5_profile,
						    rootsec , &sections);

	if (retval)
	{
        m_hostServer = _T("");

        // This is not a fatal error if DNS KDC Lookup is being used.
        // Determine the starting value for DNS KDC Lookup Checkbox
        if ( dns )
            return;

        m_profileError = TRUE;
	}

	m_krbRealmEditbox.ResetContent();

    if ( !m_profileError ) {
	for (cpp = sections; *cpp; cpp++)
	{
		if (CB_ERR == m_krbRealmEditbox.FindStringExact(-1, *cpp))
		{ // no dups
			if (CB_ERR == m_krbRealmEditbox.AddString(*cpp))
			{
				::MessageBox(NULL, "ResetDefaultRealmComboBox::Can't add to Kerberos Realm Combobox",
							 "Leash", MB_OK);
				return;
			}
		}
		else
		  m_dupEntriesError = TRUE;
	}
    }

    if (!m_newDefaultRealm.IsEmpty()) {

		if (CB_ERR == m_krbRealmEditbox.FindStringExact(-1, m_newDefaultRealm))
		{ // no dups
			m_krbRealmEditbox.AddString(m_newDefaultRealm);
		}
		m_krbRealmEditbox.SelectString(-1, m_newDefaultRealm);

		const char*  Section[] = {"realms", m_newDefaultRealm, "kdc", NULL}; //theSection
		const char** section = Section;
		char **values = NULL;

		retval = pprofile_get_values(CLeashApp::m_krbv5_profile,
					     section, &values);

        if (!retval && values)
            m_hostServer = *values;
        else {
            if (dns)
                m_hostServer = "DNS SRV record lookups will be used to find KDC";
            else {
                m_hostServer = "No KDC information available";
            }
		}
	}
}

BOOL CKrbConfigOptions::PreTranslateMessage(MSG* pMsg)
{
	if (!m_startupPage2)
	{
		if (m_noKrbFileError)
		{
			LeashErrorBox("PreTranslateMessage::Unable to open configuration file",
				!strlen(CKrbProperties::m_krbPath) ? KRB_FILE :
				CKrbProperties::m_krbPath);
			m_noKrbFileError = FALSE;
		}

		if (m_noKrbhostWarning)
		{
			MessageBox("PreTranslateMessage::Unable to locate the Kerberos Host for your Kerberos Realm!",
					   "Leash", MB_OK);
			m_noKrbhostWarning = FALSE;
		}

		if (m_dupEntriesError)
		{
			MessageBox("PreTranslateMessage::Found duplicate entries in the Kerberos 5 Config. File!!!\
                        \nPlease contact your Administrator.",
					   "Leash", MB_OK);

			m_dupEntriesError = FALSE;
		}

		if (m_profileError)
		{
			MessageBox("PreTranslateMessage::Unable to open Kerberos 5 Config. File!!!\
                        \nIf this error persists, contact your administrator.",
				       "Leash", MB_OK);
			m_profileError	= FALSE;
		}

		if (m_noRealm)
		{
			MessageBox("PreTranslateMessage::Unable to determine the Default Realm.\
                        \n Contact your Administrator!",
					   "Leash", MB_OK);

			m_noRealm = FALSE;
		}
	}

	m_startupPage2 = FALSE;
	return CPropertyPage::PreTranslateMessage(pMsg);
}


BEGIN_MESSAGE_MAP(CKrbConfigOptions, CPropertyPage)
	//{{AFX_MSG_MAP(CKrbConfigOptions)
	ON_WM_SHOWWINDOW()
	ON_CBN_EDITCHANGE(IDC_EDIT_DEFAULT_REALM, OnEditchangeEditDefaultRealm)
	ON_CBN_SELCHANGE(IDC_EDIT_DEFAULT_REALM, OnSelchangeEditDefaultRealm)
	ON_BN_CLICKED(IDC_BUTTON_KRB_HELP, OnButtonKrbHelp)
	ON_BN_CLICKED(IDC_BUTTON_KRBREALM_HELP, OnButtonKrbrealmHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



void CKrbConfigOptions::OnButtonKrbHelp()
{
	MessageBox("No Help Available!", "Leash", MB_OK);
}

void CKrbConfigOptions::OnButtonKrbrealmHelp()
{
	MessageBox("No Help Available!", "Leash", MB_OK);
}
