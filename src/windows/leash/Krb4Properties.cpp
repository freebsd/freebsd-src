//	**************************************************************************************
//	File:			Krb4Properties.cpp
//	By:				Arthur David Leather
//	Created:		12/02/98
//	Copyright		@1998 Massachusetts Institute of Technology - All rights reserved.
//	Description:	CPP file for KrbProperties.h. Contains variables and functions
//					for Kerberos Four Properties
//
//	History:
//
//	MM/DD/YY	Inits	Description of Change
//	12/02/98	ADL		Original
//	**************************************************************************************



#include "stdafx.h"
#include "Leash.h"
#include "Krb4Properties.h"
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
// CKrb4ConfigFileLocation property page

IMPLEMENT_DYNCREATE(CKrb4ConfigFileLocation, CPropertyPage)

CString CKrb4ConfigFileLocation::m_newKrbFile;
CString CKrb4ConfigFileLocation::m_newKrbrealmFile;

CKrb4ConfigFileLocation::CKrb4ConfigFileLocation() : CPropertyPage(CKrb4ConfigFileLocation::IDD)
{
    m_newTicketFile = _T("");
    m_newKrbFile = _T("");
    m_newKrbrealmFile = _T("");
    m_initKrbFile = _T("");
    m_initKrbrealmFile = _T("");
    m_initTicketFile = _T("");
    m_noKrbrealmFileStartupWarning = FALSE;
    m_noKrbFileStartupWarning = FALSE;

    m_startupPage1 = TRUE;

	//{{AFX_DATA_INIT(CKrb4ConfigFileLocation)
	//}}AFX_DATA_INIT
}

CKrb4ConfigFileLocation::~CKrb4ConfigFileLocation()
{
}

BOOL CKrb4ConfigFileLocation::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	INT krbCreate = 0;
	INT krbrealmCreate = 0;
    CHAR krb_path[MAX_PATH];
	CHAR krbrealm_path[MAX_PATH];
    CHAR ticketName[MAX_PATH];
	unsigned int krb_path_sz = sizeof(krb_path);
    unsigned int krbrealm_path_sz = sizeof(krbrealm_path);
    CString strMessage;


	// Set KRB.CON
	memset(krb_path, '\0', sizeof(krb_path));
	if (!pkrb_get_krbconf2(krb_path, &krb_path_sz))
	{ // Error has happened
		m_noKrbFileStartupWarning = TRUE;
	}
	else
	{ // normal find
		m_initKrbFile = krb_path;
	    m_newKrbFile = m_initKrbFile;
        SetDlgItemText(IDC_EDIT_KRB_LOC, m_initKrbFile);
    }

    // Set KRBREALM.CON
    memset(krbrealm_path, '\0', sizeof(krbrealm_path));
    if (!pkrb_get_krbrealm2(krbrealm_path, &krbrealm_path_sz))
	{
        // Error has happened
		m_noKrbrealmFileStartupWarning = TRUE;
	}
	else
	{
        // normal find
		m_initKrbrealmFile = krbrealm_path;
        m_newKrbrealmFile = m_initKrbrealmFile;
        SetDlgItemText(IDC_EDIT_KRBREALM_LOC, m_initKrbrealmFile);
    }

	if (pLeash_get_lock_file_locations() ||
        getenv("KRB4_KRB.REALMS") || getenv("KRB4_KRB.CONF") || getenv("KRB4_CONFIG"))
    {
        GetDlgItem(IDC_EDIT_KRB_LOC)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_KRBREALM_LOC)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_KRB_BROWSE)->EnableWindow(FALSE);
        GetDlgItem(IDC_BUTTON_KRBREALM_BROWSE)->EnableWindow(FALSE);
    }
    else if ( !(getenv("KRB4_KRB.REALMS") || getenv("KRB4_KRB.CONF") || getenv("KRB4_CONFIG")) )
    {
        GetDlgItem(IDC_STATIC_CONFILES)->ShowWindow(FALSE);
    }


    // Set TICKET.KRB file Editbox
    *ticketName = NULL;
	pkrb_set_tkt_string(0);

    char *pticketName = ptkt_string();
    if (pticketName)
        strcpy(ticketName, pticketName);

    if (!*ticketName)
	{
		LeashErrorBox("OnInitDialog::Can't locate ticket file", TICKET_FILE);
	}
	else
	{
        m_initTicketFile = m_newTicketFile = ticketName;
		m_ticketEditBox.ReplaceSel(m_initTicketFile);
	}

	if (getenv("KRBTKFILE"))
        GetDlgItem(IDC_EDIT_TICKET_FILE)->EnableWindow(FALSE);
    else
        GetDlgItem(IDC_STATIC_TXT)->ShowWindow(FALSE);

    return FALSE;
}

BOOL CKrb4ConfigFileLocation::OnApply()
{
	// Krb.con
    if (0 != m_initKrbFile.CompareNoCase(m_newKrbFile))
    {
        // Commit changes
        if (SetRegistryVariable("krb.conf", m_newKrbFile,
            "Software\\MIT\\Kerberos4"))
        {
            MessageBox("Failed to set \"Krb.conf\"!", "Error", MB_OK);
        }

        m_initKrbFile = m_newKrbFile;
    }

    // Krbrealms.con
    if (0 != m_initKrbrealmFile.CompareNoCase(m_newKrbrealmFile))
    {
        // Commit changes
        if (SetRegistryVariable("krb.realms", m_newKrbrealmFile,
            "Software\\MIT\\Kerberos4"))
        {
            MessageBox("Failed to set \"krb.realms\"!", "Error", MB_OK);
        }

        m_initKrbrealmFile = m_newKrbrealmFile;
    }

    // Ticket file
	if (0 != m_initTicketFile.CompareNoCase(m_newTicketFile))
	{
        if (getenv("KRBTKFILE"))
        {
            // Just in case they set (somehow) KRBTKFILE while this box is up
            MessageBox("OnApply::Ticket file is set in your System's\
                        Environment!\nYou must first remove it.",
                        "Error", MB_OK);

            return TRUE;
        }

        // Commit changes
        if (SetRegistryVariable("ticketfile", m_newTicketFile,
            "Software\\MIT\\Kerberos4"))
        {
            MessageBox("Failed to set \"ticketfile\"!", "Error", MB_OK);
        }

        m_initTicketFile = m_newTicketFile;
	}

    return TRUE;
}

VOID CKrb4ConfigFileLocation::OnOK()
{
	CPropertyPage::OnOK();
}

VOID CKrb4ConfigFileLocation::DoDataExchange(CDataExchange* pDX)
{
	TRACE("Entering CKrb4ConfigFileLocation::DoDataExchange -- %d\n",
	      pDX->m_bSaveAndValidate);
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CKrb4ConfigFileLocation)
	DDX_Control(pDX, IDC_EDIT_TICKET_FILE, m_ticketEditBox);
	//}}AFX_DATA_MAP
}


VOID CKrb4ConfigFileLocation::OnButtonKrbBrowse()
{
	CString msg;
	msg.Format("Select %s Location", KRB_FILE);

    CString krb_path = "*.*";
	CLeashFileDialog dlgFile(TRUE, NULL, krb_path, "Kerbereos Four Config. File (.con)");
    dlgFile.m_ofn.lpstrTitle = msg;

    if (IDOK == dlgFile.DoModal())
	{
		//m_newKrbFile = dlgFile.GetSelectedFileName();
        m_newKrbFile= dlgFile.GetPathName();
		SetDlgItemText(IDC_EDIT_KRB_LOC, m_newKrbFile);
        SetModified(TRUE);
    }
}

VOID CKrb4ConfigFileLocation::OnButtonKrbrealmBrowse()
{
	CString msg;
	msg.Format("Select %s Location", KRBREALM_FILE);

    CString krbrealm_path = "*.*";
    CLeashFileDialog dlgFile(TRUE, NULL, krbrealm_path, "Kerbereos Four Config. File (.con)");
    dlgFile.m_ofn.lpstrTitle = msg;

    if (IDOK == dlgFile.DoModal())
	{
		//m_krbrealmFile = dlgFile.GetSelectedFileName();
		m_newKrbrealmFile = dlgFile.GetPathName();
		SetDlgItemText(IDC_EDIT_KRB_KRBREALM_LOC, m_newKrbrealmFile);
        SetModified(TRUE);
    }
}

/*
VOID CKrb4ConfigFileLocation::OnButtonTicketfileBrowse()
{
	CString ticketPath = *.*";
	CLeashFileDialog dlgFile(TRUE, NULL, ticketPath, "Kerberos Four Ticket File (.con)");
	CString msg;
	msg.Format("Select Location/Ticket File (Default file = %s)", TICKET_FILE);
	dlgFile.m_ofn.lpstrTitle = msg;
	while (TRUE)
	{
		if (IDOK == dlgFile.DoModal())
		{
			m_newTicketFile = dlgFile.GetPathName();
			SetDlgItemText(IDC_EDIT_TICKET_FILE, m_newTicketFile);
			SetModified(TRUE);
			break;
		}
		else
		  break;
	}
}
*/

void CKrb4ConfigFileLocation::OnChangeEditKrbLoc()
{
	if (!m_startupPage1)
	{
		GetDlgItemText(IDC_EDIT_KRB_LOC, m_newKrbFile);
		SetModified(TRUE);
	}
}

void CKrb4ConfigFileLocation::OnChangeEditKrbrealmLoc()
{
	if (!m_startupPage1)
	{
		GetDlgItemText(IDC_EDIT_KRBREALM_LOC, m_newKrbrealmFile);
		SetModified(TRUE);
	}
}

void CKrb4ConfigFileLocation::OnChangeEditTicketFile()
{
	if (!m_startupPage1)
	{
		GetDlgItemText(IDC_EDIT_TICKET_FILE, m_newTicketFile);
		SetModified(TRUE);
	}
}

VOID CKrb4ConfigFileLocation::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CPropertyPage::OnShowWindow(bShow, nStatus);
}

VOID CKrb4ConfigFileLocation::OnCancel()
{
	CPropertyPage::OnCancel();
}

void CKrb4ConfigFileLocation::OnHelp()
{
#ifdef CALL_HTMLHELP
    AfxGetApp()->HtmlHelp(HID_KRB4_PROPERTIES_COMMAND);
#else
    AfxGetApp()->WinHelp(HID_KRB4_PROPERTIES_COMMAND);
#endif
}

BOOL CKrb4ConfigFileLocation::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class
	CString wmsg;
	if (m_startupPage1)
	{
        if (m_noKrbFileStartupWarning)
		{
			wmsg.Format("OnInitDialog::Can't locate configuration file: %s.",
							  KRB_FILE);
			MessageBox(wmsg, "Leash", MB_OK);
            m_noKrbFileStartupWarning  = FALSE;
		}

        if (m_noKrbrealmFileStartupWarning)
		{
			wmsg.Format("OnInitDialog::Can't locate configuration file: %s.",
							  KRBREALM_FILE);
			MessageBox(wmsg, "Leash", MB_OK);
            m_noKrbrealmFileStartupWarning = FALSE;
        }
    }

	m_startupPage1 = FALSE;
    return CPropertyPage::PreTranslateMessage(pMsg);
}


BEGIN_MESSAGE_MAP(CKrb4ConfigFileLocation, CPropertyPage)
	//{{AFX_MSG_MAP(CKrb4ConfigFileLocation)
	ON_BN_CLICKED(IDC_BUTTON_KRB_BROWSE, OnButtonKrbBrowse)
	ON_BN_CLICKED(IDC_BUTTON_KRBREALM_BROWSE, OnButtonKrbrealmBrowse)
	ON_WM_SHOWWINDOW()
	ON_EN_CHANGE(IDC_EDIT_TICKET_FILE, OnChangeEditTicketFile)
    ON_COMMAND(ID_HELP, OnHelp)
	ON_EN_CHANGE(IDC_EDIT_KRB_LOC, OnChangeEditKrbLoc)
	ON_EN_CHANGE(IDC_EDIT_KRBREALM_LOC, OnChangeEditKrbrealmLoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


///////////////////////////////////////////////////////////////////////
// CKrb4Properties

IMPLEMENT_DYNAMIC(CKrb4Properties, CPropertySheet)
CKrb4Properties::CKrb4Properties(UINT nIDCaption, CWnd* pParentWnd,
                                 UINT iSelectPage)
:CPropertySheet(nIDCaption, pParentWnd, iSelectPage)
{
}

CKrb4Properties::CKrb4Properties(LPCTSTR pszCaption, CWnd* pParentWnd,
								 UINT iSelectPage)
:CPropertySheet(pszCaption, pParentWnd, iSelectPage)
{
	AddPage(&m_fileLocation);
}

CKrb4Properties::~CKrb4Properties()
{
}


BEGIN_MESSAGE_MAP(CKrb4Properties, CPropertySheet)
	//{{AFX_MSG_MAP(CKrb4Properties)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

///////////////////////////////////////////////////////////////////////
// CKrb4Properties message handlers
