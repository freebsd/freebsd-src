// AfsProperties.cpp : implementation file
//

#include "stdafx.h"
#include "leash.h"
#include "AfsProperties.h"

/* This should be set to something other than 0 or 1 (the valid values) */
#define INVALID_AFS_STATUS_VALUE 2
#define IS_INVALID_AFS_STATUS_VALUE(x) ((x != 0) && (x != 1))

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CAfsProperties dialog


CAfsProperties::CAfsProperties(CWnd* pParent /*=NULL*/)
	: CDialog(CAfsProperties::IDD, pParent)
{
    m_newAfsStatus = 0;
    m_oldAfsStatus = 0;

    //{{AFX_DATA_INIT(CAfsProperties)
    // NOTE: the ClassWizard will add member initialization here
    //}}AFX_DATA_INIT
}


void CAfsProperties::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CAfsProperties)
    // NOTE: the ClassWizard will add DDX an3d DDV calls here
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CAfsProperties, CDialog)
    //{{AFX_MSG_MAP(CAfsProperties)
    ON_BN_CLICKED(IDC_BUTTON_AFS_PROPERTIES, OnButtonAfsProperties)
    ON_BN_CLICKED(IDC_RADIO_AFS_ENABLED, OnRadioAfsEnabled)
    ON_BN_CLICKED(IDC_RADIO_AFS_DISABLED, OnRadioAfsDisabled)
    ON_COMMAND(ID_HELP, OnHelp)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAfsProperties message handlers

BOOL
CAfsProperties::OnInitDialog()
{
    CDialog::OnInitDialog();

    // Get State* of Destroy Tickets On Exit
    m_pApp = AfxGetApp();

    m_oldAfsStatus = m_pApp->GetProfileInt("Settings", "AfsStatus",
                                           INVALID_AFS_STATUS_VALUE);
    if (IS_INVALID_AFS_STATUS_VALUE(m_oldAfsStatus))
    {
        // set the default
        m_pApp->WriteProfileInt("Settings", "AfsStatus", 1);
        m_oldAfsStatus = 1;
    }

    m_newAfsStatus = m_oldAfsStatus;

    int enabled = (m_oldAfsStatus != 0);
    if (enabled)
        CheckDlgButton(IDC_RADIO_AFS_ENABLED, TRUE);
    else
        CheckDlgButton(IDC_RADIO_AFS_DISABLED, TRUE);

    return TRUE;
}

void CAfsProperties::OnButtonAfsProperties()
{
    if (32 >= (LRESULT) ShellExecute (NULL, NULL, "AFS_CONFIG.EXE", NULL,
                                      NULL, SW_SHOW))
    {
        MessageBox("Can't find file AFS_CONFIG.EXE", "Error", MB_OK);
    }
}

void CAfsProperties::OnOK()
{
    if (m_oldAfsStatus != m_newAfsStatus)
    {
        if (!m_pApp->WriteProfileInt("Settings", "AfsStatus", m_newAfsStatus))
        {
            MessageBox("There was an error putting your entry into the "
                       "Registry!", "Error", MB_OK);
        }
    }

    CDialog::OnOK();
}

void CAfsProperties::OnRadioAfsEnabled()
{
	m_newAfsStatus = 1;
}

void CAfsProperties::OnRadioAfsDisabled()
{
	m_newAfsStatus = 0;
}

void CAfsProperties::OnHelp()
{
#ifdef CALL_HTMLHELP
    AfxGetApp()->HtmlHelp(HID_AFS_PROPERTIES_COMMAND);
#else
    AfxGetApp()->WinHelp(HID_AFS_PROPERTIES_COMMAND);
#endif
}
