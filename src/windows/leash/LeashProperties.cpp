//	**************************************************************************************
//	File:			LeashProperties.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for LeashProperties.h. Contains variables and functions
//					for the Leash Properties Dialog Box
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************

#include "stdafx.h"
#include "leash.h"
#include "LeashProperties.h"
#include "LeashMessageBox.h"
#include <leashinfo.h>
#include "lglobals.h"
#include "reminder.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLeashProperties dialog

char CLeashProperties::timeServer[255] = {NULL};

CLeashProperties::CLeashProperties(CWnd* pParent /*=NULL*/)
	: CDialog(CLeashProperties::IDD, pParent)
{
    m_initMissingFiles = m_newMissingFiles = 0;
    dw_initMslsaImport = dw_newMslsaImport = 0;

	//{{AFX_DATA_INIT(CLeashProperties)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CLeashProperties::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CLeashProperties)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLeashProperties, CDialog)
	//{{AFX_MSG_MAP(CLeashProperties)
	ON_BN_CLICKED(IDC_BUTTON_LEASHINI_HELP2, OnHelp)
    ON_BN_CLICKED(IDC_CHECK_CREATE_MISSING_CFG, OnCheckMissingCfg)
    ON_BN_CLICKED(IDC_RESET_DEFAULTS, OnButtonResetDefaults)
    ON_BN_CLICKED(IDC_RADIO_MSLSA_IMPORT_OFF, OnRadioMslsaNever)
    ON_BN_CLICKED(IDC_RADIO_MSLSA_IMPORT_ON,  OnRadioMslsaAlways)
    ON_BN_CLICKED(IDC_RADIO_MSLSA_IMPORT_MATCH, OnRadioMslsaMatchingRealm)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLeashProperties message handlers

BOOL CLeashProperties::OnInitDialog()
{
	CDialog::OnInitDialog();

    pLeashGetTimeServerName(timeServer, TIMEHOST);
    SetDlgItemText(IDC_EDIT_TIME_SERVER, timeServer);

	if (getenv(TIMEHOST))
        GetDlgItem(IDC_EDIT_TIME_SERVER)->EnableWindow(FALSE);
    else
        GetDlgItem(IDC_STATIC_TIMEHOST)->ShowWindow(FALSE);

    CWinApp * pApp = AfxGetApp();
    if (pApp)
        m_initMissingFiles = m_newMissingFiles =
            pApp->GetProfileInt("Settings", "CreateMissingConfig", FALSE_FLAG);
    CheckDlgButton(IDC_CHECK_CREATE_MISSING_CFG, m_initMissingFiles);

    dw_initMslsaImport = dw_newMslsaImport = pLeash_get_default_mslsa_import();
    switch ( dw_initMslsaImport ) {
    case 0:
        CheckDlgButton(IDC_RADIO_MSLSA_IMPORT_OFF,TRUE);
        break;
    case 1:
        CheckDlgButton(IDC_RADIO_MSLSA_IMPORT_ON,TRUE);
        break;
    case 2:
        CheckDlgButton(IDC_RADIO_MSLSA_IMPORT_MATCH,TRUE);
        break;
    }

    return TRUE;
}

void CLeashProperties::OnOK()
{
	CString timeServer_;
	GetDlgItemText(IDC_EDIT_TIME_SERVER, timeServer_);

	if (getenv(TIMEHOST))
    {
        // Check system for TIMEHOST, just in case it gets set (somehow)
        MessageBox("Can't change the time host unless you remove it from the environment!",
                   "Error", MB_OK);
        return;
    }

    if( getenv("USEKRB4") !=  NULL)
    {
        MessageBox("Kerberos 4 ticket requests are being controlled by the environment"
                   "variable USEKRB4 instead of the registry. Leash cannot modify"
                   "the environment. Use the System control panel instead.",
                    "Leash", MB_OK);
        return;
    }

    if (SetRegistryVariable(TIMEHOST, timeServer_))
	{
		MessageBox("There was an error putting your entry into the Registry!",
                   "Error", MB_OK);
    }

    if ( m_initMissingFiles != m_newMissingFiles ) {
        CWinApp * pApp = AfxGetApp();
        if (pApp)
            pApp->WriteProfileInt("Settings", "CreateMissingConfig",
                                m_newMissingFiles ? TRUE_FLAG : FALSE_FLAG);

        if ( m_newMissingFiles )
            CLeashApp::ValidateConfigFiles();
    }

    if ( dw_initMslsaImport != dw_newMslsaImport ) {
		pLeash_set_default_mslsa_import(dw_newMslsaImport);
	}

	CDialog::OnOK();
}

void CLeashProperties::OnCheckMissingCfg()
{
    m_newMissingFiles = (BOOL)IsDlgButtonChecked(IDC_CHECK_CREATE_MISSING_CFG);
}

void CLeashProperties::OnRadioMslsaNever()
{
    dw_newMslsaImport = 0;
}

void CLeashProperties::OnRadioMslsaAlways()
{
    dw_newMslsaImport = 1;
}

void CLeashProperties::OnRadioMslsaMatchingRealm()
{
    dw_newMslsaImport = 2;
}

void CLeashProperties::OnHelp()
{
#ifdef CALL_HTMLHELP
    AfxGetApp()->HtmlHelp(HID_LEASH_PROPERTIES_COMMAND);
#else
    AfxGetApp()->WinHelp(HID_LEASH_PROPERTIES_COMMAND);
#endif
}

void CLeashProperties::OnButtonResetDefaults()
{
    if (IDYES != AfxMessageBox("You are about to reset all Leash settings to their default values!\n\nContinue?",
                                MB_YESNO))
        return;

    pLeash_reset_defaults();

    HKEY hKey;
    LONG rc;

    rc = RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\MIT\\Leash32\\Settings",
                      0, KEY_WRITE, &hKey);
    if (rc)
        return;

    rc = RegDeleteValue(hKey, "AutoRenewTickets");
    rc = RegDeleteValue(hKey, "CreateMissingConfig");
    rc = RegDeleteValue(hKey, "DebugWindow");
    rc = RegDeleteValue(hKey, "LargeIcons");
    rc = RegDeleteValue(hKey, "TIMEHOST");
    rc = RegDeleteValue(hKey, "AfsStatus");
    rc = RegDeleteValue(hKey, "LowTicketAlarm");

    RegCloseKey(hKey);
}
