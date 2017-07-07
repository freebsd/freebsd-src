//****************************************************************************
// File:	Krb5Properties.cpp
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright:	1998 Massachusetts Institute of Technology - All rights
//		reserved.
// Description:	CPP file for Krb5Properties.h. Contains variables and functions
//		for Kerberos Five Properties
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 12/02/98	ADL	Original
//*****************************************************************************

#include "stdafx.h"
#include "leash.h"
#include "LeashFileDialog.h"
#include "Krb5Properties.h"
#include "win-mac.h"
#include "lglobals.h"
#include "LeashView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CKrb5ConfigFileLocation dialog

IMPLEMENT_DYNCREATE(CKrb5ConfigFileLocation, CPropertyPage)

CKrb5ConfigFileLocation::CKrb5ConfigFileLocation()
    : CPropertyPage(CKrb5ConfigFileLocation::IDD)
{
    m_initConfigFile = _T("");
    m_initTicketFile = _T("");
    m_newConfigFile = _T("");
    m_newTicketFile = _T("");
    m_startupPage1 = TRUE;

    //{{AFX_DATA_INIT(CKrb5ConfigFileLocation)
    //}}AFX_DATA_INIT
}

void CKrb5ConfigFileLocation::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CKrb5ConfigFileLocation)
    DDX_Control(pDX, IDC_EDIT_KRB5_TXT_FILE, m_ticketEditBox);
    //}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CKrb5ConfigFileLocation, CDialog)
    //{{AFX_MSG_MAP(CKrb5ConfigFileLocation)
    ON_BN_CLICKED(IDC_BUTTON_KRB5INI_BROWSE, OnButtonKrb5iniBrowse)
    ON_BN_CLICKED(IDC_BUTTON_KRB5_TICKETFILE_BROWSE, OnButtonKrb5TicketfileBrowse)
    ON_EN_CHANGE(IDC_EDIT_KRB5_TXT_FILE, OnChangeEditKrb5TxtFile)
    ON_EN_CHANGE(IDC_EDIT_KRB5INI_LOCATION, OnChangeEditKrb5iniLocation)
    ON_WM_SHOWWINDOW()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CKrb5ConfigFileLocation::OnApply()
{
    BOOL tooManySlashes = FALSE;
    BOOL foundError = FALSE;

    if( getenv("RENEW_TILL") !=  NULL)
    {
        MessageBox("The ticket renewable time is being controlled by the environment"
                   "variable RENEW_TILL instead of the registry. Leash cannot modify"
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if( getenv("RENEWABLE") !=  NULL)
    {
        MessageBox("Ticket renewability is being controlled by the environment"
                   "variable RENEWABLE instead of the registry. Leash cannot modify"
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if( getenv("FORWARDABLE") !=  NULL)
    {
        MessageBox("Ticket forwarding is being controlled by the environment"
                   "variable FORWARDABLE instead of the registry. Leash cannot modify"
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if( getenv("PROXIABLE") !=  NULL)
    {
        MessageBox("Ticket proxying is being controlled by the environment"
                   "variable PROXIABLE instead of the registry. Leash cannot modify"
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return(FALSE);
    }

    if( getenv("NOADDRESSES") !=  NULL)
    {
        MessageBox("Addressless tickets are being controlled by the environment"
                   "variable NOADDRESSES instead of the registry. Leash cannot modify"
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return(FALSE);
    }


    // KRB5.INI file
    if (!CLeashApp::m_krbv5_profile ||
	0 != m_newConfigFile.CompareNoCase(m_initConfigFile))
    { // Different path for Krb5.ini

        if (IsDlgButtonChecked(IDC_CHECK_CONFIRM_KRB5_EXISTS))
        {
            // Check for extra slashes at end of path
            LPSTR pSlash = strrchr(m_newConfigFile.GetBuffer(0), '\\');
            if (pSlash && *(pSlash - 1) == '\\')
            { // don't commit changes
                tooManySlashes = TRUE;
            }
            else if (pSlash && *(pSlash + 1) == '\0')
            { // commit changes, but take out slash at the end of path
                *pSlash = 0;
            }

            m_newConfigFile.ReleaseBuffer(-1);

            // Check for invalid path
            Directory directory(m_newConfigFile);
            if (tooManySlashes || !directory.IsValidFile())
            { // don't commit changes
                foundError = TRUE;

                if (tooManySlashes)
                    LeashErrorBox("OnApply::Too Many Slashes At End of "
                                  "Selected Directory",
                                  m_newConfigFile);
                else
                    LeashErrorBox("OnApply::Selected file doesn't exist",
                                  m_newConfigFile);

                SetDlgItemText(IDC_EDIT_KRB5INI_LOCATION, m_initConfigFile);
            }
            else
            {
                // more error checking
                CHAR confname[MAX_PATH];

                const char *filenames[2];
                filenames[0] = m_newConfigFile;
                filenames[1] = NULL;

                const char*  rootSection[] = {"realms", NULL};
                const char** rootsec = rootSection;
                char **sections = NULL;

                long retval = pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
                if (!retval)
                    retval = pprofile_get_subsection_names(CLeashApp::m_krbv5_profile,
                                                           rootsec, &sections
                                                           );
                if (retval || !*sections )
                {
                    foundError = TRUE;
                    MessageBox("Your file selection is either corrupt or not a Kerberos Five Config. file",
                               "Leash", MB_OK);

                    pprofile_free_list(sections);

                    // Restore old 'valid' config. file
                    if (CLeashApp::GetProfileFile(confname, sizeof(confname)))
                    {
                        foundError = TRUE;
                        MessageBox("Can't locate Kerberos Five Config. file!",
                                   "Error", MB_OK);
                        return TRUE;
                    }

                    filenames[0] = confname;
                    filenames[1] = NULL;

                    retval = pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
                    if (!retval)
                        retval = pprofile_get_subsection_names(CLeashApp::m_krbv5_profile,
                                                               rootsec, &sections);
                    if (retval || !*sections)
                    {
                        foundError = TRUE;
                        MessageBox("OnApply::There is a problem with your "
                                   "Kerberos Five Config. file!\n"
                                   "Contact your Administrator.",
                                   "Leash", MB_OK);
                    }

                    pprofile_free_list(sections);
                    SetDlgItemText(IDC_EDIT_KRB5INI_LOCATION, m_initConfigFile);

                    pprofile_release(CLeashApp::m_krbv5_profile);
                    return TRUE;
                }

                pprofile_free_list(sections);
	    }
        }

        // Commit changes
        if (!foundError)
        {
            if (SetRegistryVariable("config", m_newConfigFile,
                                    "Software\\MIT\\Kerberos5"))
            {
                MessageBox("Failed to set \"Krb.conf\"!", "Error", MB_OK);
            }

            m_initConfigFile = m_newConfigFile;
            SetModified(TRUE);
        }
    }

    // Credential cache (ticket) file
    // Ticket file
    if (0 != m_initTicketFile.CompareNoCase(m_newTicketFile))
    {
        if (getenv("KRB5_ENV_CCNAME"))
        {
            // Just in case they set (somehow) KRB5_ENV_CCNAME while this box is up
            MessageBox("OnApply::Ticket file is set in your System's"
                       "Environment!\nYou must first remove it.",
                       "Error", MB_OK);

            return TRUE;
        }

        // Commit changes
        if (SetRegistryVariable("ccname", m_newTicketFile,
                                "Software\\MIT\\Kerberos5"))
        {
            MessageBox("Failed to set \"ccname\"!", "Error", MB_OK);
        }
        if ( CLeashApp::m_krbv5_context )
            pkrb5_cc_set_default_name(CLeashApp::m_krbv5_context,m_newTicketFile);

        m_initTicketFile = m_newTicketFile;
    }

    return TRUE;
}


BOOL CKrb5ConfigFileLocation::OnInitDialog()
{
    CDialog::OnInitDialog();

    CHAR confname[MAX_PATH];
    CHAR ticketName[MAX_PATH];

    CheckDlgButton(IDC_CHECK_CONFIRM_KRB5_EXISTS, TRUE);

    // Config. file (Krb5.ini)
    if (CLeashApp::GetProfileFile(confname, sizeof(confname)))
    {
        MessageBox("Can't locate Kerberos Five config. file!", "Error", MB_OK);
        return TRUE;
    }

    m_initConfigFile = m_newConfigFile = confname;
    SetDlgItemText(IDC_EDIT_KRB5INI_LOCATION, m_initConfigFile);

    if (pLeash_get_lock_file_locations() || getenv("KRB5_CONFIG"))
    {
        GetDlgItem(IDC_EDIT_KRB5INI_LOCATION)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_KRB5INI_BROWSE)->EnableWindow(FALSE);
        GetDlgItem(IDC_CHECK_CONFIRM_KRB5_EXISTS)->EnableWindow(FALSE);
    }
    else if ( !(getenv("KRB5_CONFIG")) )
    {
        GetDlgItem(IDC_STATIC_INIFILES)->ShowWindow(FALSE);
    }


    // Set TICKET.KRB file Editbox
    *ticketName = NULL;
    if (CLeashApp::m_krbv5_context)
    {
        const char *pticketName = pkrb5_cc_default_name(CLeashApp::m_krbv5_context);

        if (pticketName)
            strcpy(ticketName, pticketName);
    }

    if (!*ticketName)
    {
        MessageBox("OnInitDialog::Can't locate Kerberos Five ticket file!",
                   "Error", MB_OK);
        return TRUE;
    }
    else
    {
        m_initTicketFile = m_newTicketFile = ticketName;
        SetDlgItemText(IDC_EDIT_KRB5_TXT_FILE, m_initTicketFile);
    }

    if (getenv("KRB5CCNAME"))
        GetDlgItem(IDC_EDIT_KRB5_TXT_FILE)->EnableWindow(FALSE);
    else
        GetDlgItem(IDC_STATIC_TICKETFILE)->ShowWindow(FALSE);

    return TRUE;
}

void CKrb5ConfigFileLocation::OnButtonKrb5iniBrowse()
{
    CLeashFileDialog dlgFile(TRUE, NULL, "*.*",
                             "Kerbereos Five Config. File (.ini)");
    dlgFile.m_ofn.lpstrTitle = "Select the Kerberos Five Config. File";
    while (TRUE)
    {
        if (IDOK == dlgFile.DoModal())
        {
            m_newConfigFile = dlgFile.GetPathName();
            SetDlgItemText(IDC_EDIT_KRB5INI_LOCATION, m_newConfigFile);
            break;
        }
        else
            break;
    }
}

void CKrb5ConfigFileLocation::OnButtonKrb5TicketfileBrowse()
{
    CString ticket_path = "*.*";
    CLeashFileDialog dlgFile(TRUE, NULL, ticket_path,
                             "Kerbereos Five Ticket File (Krb5cc)");
    dlgFile.m_ofn.lpstrTitle = "Select Credential Cache (Ticket) File";

    if (IDOK == dlgFile.DoModal())
    {
        m_newTicketFile = dlgFile.GetPathName();
        SetDlgItemText(IDC_EDIT_KRB5_TXT_FILE, m_newTicketFile);
    }
}

void CKrb5ConfigFileLocation::OnChangeEditKrb5iniLocation()
{
    if (!m_startupPage1)
    {
        GetDlgItemText(IDC_EDIT_KRB5INI_LOCATION, m_newConfigFile);
        SetModified(TRUE);
    }
}

void CKrb5ConfigFileLocation::OnChangeEditKrb5TxtFile()
{
    if (!m_startupPage1)
    {
        GetDlgItemText(IDC_EDIT_KRB5_TXT_FILE, m_newTicketFile);
        SetModified(TRUE);
    }
}

void CKrb5ConfigFileLocation::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CDialog::OnShowWindow(bShow, nStatus);
    m_startupPage1 = FALSE;
}


/////////////////////////////////////////////////////////////////////////////
// CKrb5ConfigOptions dialog

IMPLEMENT_DYNCREATE(CKrb5ConfigOptions, CPropertyPage)

CKrb5ConfigOptions::CKrb5ConfigOptions()
	: CPropertyPage(CKrb5ConfigOptions::IDD)
{
    m_initForwardable = 0;
    m_newForwardable = 0;
    m_initProxiable = 0;
    m_newProxiable = 0;
    m_initRenewable = 0;
    m_newRenewable = 0;
    m_initNoAddress = 0;
    m_newNoAddress = 0;
    m_initIPAddress = 0;
#ifdef SET_PUBLIC_IP
    m_newIPAddress = 0;
#endif /* SET_PUBLIC_IP */

    //{{AFX_DATA_INIT(CKrb5ConfigOptions)
    // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
}


void CKrb5ConfigOptions::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    //{{AFX_DATA_MAP(CKrb5ConfigOptions)
    // NOTE: the ClassWizard will add DDX and DDV calls here
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CKrb5ConfigOptions, CDialog)
    //{{AFX_MSG_MAP(CKrb5ConfigOptions)
    ON_BN_CLICKED(IDC_CHECK_FORWARDABLE, OnCheckForwardable)
    ON_BN_CLICKED(IDC_CHECK_PROXIABLE, OnCheckProxiable)
    ON_BN_CLICKED(IDC_CHECK_RENEWABLE, OnCheckRenewable)
    ON_BN_CLICKED(IDC_CHECK_NO_ADDRESS, OnCheckNoAddress)
    ON_WM_HELPINFO()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()


BOOL CKrb5ConfigOptions::OnApply()
{
#ifdef SET_PUBLIC_IP
    SendDlgItemMessage( IDC_IPADDRESS_PUBLIC,
                        IPM_GETADDRESS,
                        0,
                        (LPARAM)(LPDWORD)&m_newIPAddress
                      );
#endif /* SET_PUBLIC_IP */

    if ((m_initForwardable == m_newForwardable) &&
        (m_initProxiable == m_newProxiable) &&
        (m_initRenewable == m_newRenewable) &&
        (m_initNoAddress == m_newNoAddress)
#ifdef SET_PUBLIC_IP
         && (m_initIPAddress == m_newIPAddress)
#endif /* SET_PUBLIC_IP */
         )
        return TRUE;

    CWinApp *pApp = NULL;
    pApp = AfxGetApp();
    if (!pApp)
    {
        MessageBox("There is a problem finding Leash application "
                   "information!",
                   "Error", MB_OK);
        return FALSE;
    }

    if ( m_newNoAddress == FALSE ) {
        CHAR confname[MAX_PATH];
        if (!CLeashApp::GetProfileFile(confname, sizeof(confname)))
        {
            const char *filenames[2];
            char *value=NULL;
            long retval, noaddresses = 1;
            filenames[0] = confname;
            filenames[1] = NULL;
            retval = pprofile_init(filenames, &CLeashApp::m_krbv5_profile);
            if (!retval) {
                retval = pprofile_get_string(CLeashApp::m_krbv5_profile, "libdefaults","noaddresses", 0, "true", &value);
                if ( value ) {
                    noaddresses = config_boolean_to_int(value);
                    pprofile_release_string(value);
                }
                pprofile_release(CLeashApp::m_krbv5_profile);
            }

            if ( noaddresses )
            {
                MessageBox("The No Addresses setting cannot be disabled unless the setting\n"
                           "    noaddresses=false\n"
                           "is added to the [libdefaults] section of the KRB5.INI file.",
                            "Error", MB_OK);
                return FALSE;

            }
        }
    }

    pLeash_set_default_forwardable(m_newForwardable);
    pLeash_set_default_proxiable(m_newProxiable);
    pLeash_set_default_renewable(m_newRenewable);
    pLeash_set_default_noaddresses(m_newNoAddress);
#ifdef SET_PUBLIC_IP
    pLeash_set_default_publicip(m_newIPAddress);
#endif /* SET_PUBLIC_IP */

    CLeashView::m_forwardableTicket = m_initForwardable = m_newForwardable;
    CLeashView::m_proxiableTicket = m_initProxiable = m_newProxiable;
    CLeashView::m_renewableTicket = m_initRenewable = m_newRenewable;
    CLeashView::m_noaddressTicket = m_initNoAddress = m_newNoAddress;
#ifdef SET_PUBLIC_IP
    CLeashView::m_publicIPAddress = m_initIPAddress = m_newIPAddress;
#endif /* SET_PUBLIC_IP */
    return TRUE;
}

BOOL CKrb5ConfigOptions::OnInitDialog()
{
    CDialog::OnInitDialog();

    CWinApp *pApp = NULL;
    pApp = AfxGetApp();
    if (!pApp)
    {
        MessageBox("There is a problem finding Leash application "
                   "information!",
                   "Error", MB_OK);
    }
    else
    {
        m_initForwardable = pLeash_get_default_forwardable();
        m_initProxiable = pLeash_get_default_proxiable();
        m_initRenewable = pLeash_get_default_renewable();
        m_initNoAddress = pLeash_get_default_noaddresses();
        m_initIPAddress = pLeash_get_default_publicip();
    }

    CheckDlgButton(IDC_CHECK_FORWARDABLE, m_initForwardable);
    m_newForwardable = m_initForwardable;

    CheckDlgButton(IDC_CHECK_PROXIABLE, m_initProxiable);
    m_newProxiable = m_initProxiable;

    CheckDlgButton(IDC_CHECK_RENEWABLE, m_initRenewable);
    m_newRenewable = m_initRenewable;

    CheckDlgButton(IDC_CHECK_NO_ADDRESS, m_initNoAddress);
    m_newNoAddress = m_initNoAddress;

    if ( m_initNoAddress ) {
        // Disable the control - jaltman

        SendDlgItemMessage( IDC_IPADDRESS_PUBLIC,
                            IPM_CLEARADDRESS,
                            0,
                            0
                            );
    }
    else {
        SendDlgItemMessage( IDC_IPADDRESS_PUBLIC,
                            IPM_SETADDRESS,
                            0,
                            (LPARAM)m_initIPAddress
                            );
    }
#ifdef SET_PUBLIC_IP
    m_newIPAddress = m_initIPAddress;
#endif /* SET_PUBLIC_IP */

    return TRUE;  // return TRUE unless you set the focus to a control
                  // EXCEPTION: OCX Property Pages should return FALSE
}

void CKrb5ConfigOptions::OnCheckForwardable()
{
    m_newForwardable = (BOOL)IsDlgButtonChecked(IDC_CHECK_FORWARDABLE);
    SetModified(TRUE);
}

void CKrb5ConfigOptions::OnCheckProxiable()
{
    m_newProxiable = (BOOL)IsDlgButtonChecked(IDC_CHECK_PROXIABLE);
    SetModified(TRUE);
}

void CKrb5ConfigOptions::OnCheckRenewable()
{
    m_newRenewable = (BOOL)IsDlgButtonChecked(IDC_CHECK_RENEWABLE);
    SetModified(TRUE);
}

void CKrb5ConfigOptions::OnCheckNoAddress()
{
    m_newNoAddress = (BOOL)IsDlgButtonChecked(IDC_CHECK_NO_ADDRESS);
    SetModified(TRUE);

    if ( m_newNoAddress ) {
        // Disable the control - jaltman

        SendDlgItemMessage( IDC_IPADDRESS_PUBLIC,
                            IPM_CLEARADDRESS,
                            0,
                            0
                            );
    } else {
        // Enable the IP Address Control - jaltman

        SendDlgItemMessage( IDC_IPADDRESS_PUBLIC,
                            IPM_SETADDRESS,
                            0,
                            (LPARAM)m_initIPAddress
                            );
    }
}

///////////////////////////////////////////////////////////////////////
// CKrb5Properties

IMPLEMENT_DYNAMIC(CKrb5Properties, CPropertySheet)

CKrb5Properties::CKrb5Properties(UINT nIDCaption, CWnd* pParentWnd,
                                 UINT iSelectPage)
    :CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CKrb5Properties::CKrb5Properties(LPCTSTR pszCaption, CWnd* pParentWnd,
                                 UINT iSelectPage)
    :CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
    AddPage(&m_fileLocation);
    AddPage(&m_configOptions);
}

CKrb5Properties::~CKrb5Properties()
{
}

void CKrb5Properties::OnHelp()
{
#ifdef CALL_HTMLHELP
    AfxGetApp()->HtmlHelp(HID_KRB5_PROPERTIES_COMMAND);
#else
    AfxGetApp()->WinHelp(HID_KRB5_PROPERTIES_COMMAND);
#endif
}



BEGIN_MESSAGE_MAP(CKrb5Properties, CPropertySheet)
    //{{AFX_MSG_MAP(CKrb5Properties)
    // NOTE - the ClassWizard will add and remove mapping macros here.
    ON_COMMAND(ID_HELP, OnHelp)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()
