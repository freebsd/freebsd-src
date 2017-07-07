//**************************************************************************
// File:	Leash.cpp
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright:	1998 Massachusetts Institute of Technology - All rights
//		reserved.
//
// Description:	CPP file for Leash.h. Contains variables and functions
//		for Leash
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 12/02/98	ADL	Original
//**************************************************************************

#include "stdafx.h"
#include "Leash.h"

#include "MainFrm.h"
#include "LeashDoc.h"
#include "LeashView.h"
#include "LeashAboutBox.h"

#include "reminder.h"
#include "mitwhich.h"
#include <leasherr.h>
#include "lglobals.h"
#include "out2con.h"
#include <krb5.h>
#include <com_err.h>

#include <wshelper.h>

#ifndef NO_AFS
#include "afscompat.h"
#endif

#include <errno.h>

#include <afxwin.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" int VScheckVersion(HWND hWnd, HANDLE hThisInstance);

TicketInfoWrapper ticketinfo;

HWND CLeashApp::m_hProgram = 0;
HINSTANCE CLeashApp::m_hLeashDLL = 0;
HINSTANCE CLeashApp::m_hComErr = 0;
////@#+Remove
#ifndef NO_KRB4
HINSTANCE CLeashApp::m_hKrb4DLL = 0;
#endif
HINSTANCE CLeashApp::m_hKrb5DLL = 0;
HINSTANCE CLeashApp::m_hKrb5ProfileDLL= 0;
HINSTANCE CLeashApp::m_hAfsDLL = 0;
HINSTANCE CLeashApp::m_hPsapi = 0;
HINSTANCE CLeashApp::m_hToolHelp32 = 0;
krb5_context CLeashApp::m_krbv5_context = 0;
profile_t CLeashApp::m_krbv5_profile = 0;
HINSTANCE CLeashApp::m_hKrbLSA = 0;
int CLeashApp::m_useRibbon = TRUE;
BOOL CLeashApp::m_bUpdateDisplay = FALSE;

/////////////////////////////////////////////////////////////////////////////
// CLeashApp


BEGIN_MESSAGE_MAP(CLeashApp, CWinApp)
	//{{AFX_MSG_MAP(CLeashApp)
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CLeashApp construction
CLeashApp::CLeashApp()
{
    m_krbv5_context = NULL;
    m_krbv5_profile = NULL;
    // TODO: add construction code here,
    // Place all significant initialization in InitInstance

    // Memory may not be initialized to zeros (in debug)
    memset(&ticketinfo, 0, sizeof(ticketinfo));

    ticketinfo.lockObj = CreateMutex(NULL, FALSE, NULL);

#ifdef USE_HTMLHELP
#if _MSC_VER >= 1300
    EnableHtmlHelp();
#endif
#endif
}

CLeashApp::~CLeashApp()
{
    if ( m_krbv5_context ) {
        pkrb5_free_context(m_krbv5_context);
        m_krbv5_context = NULL;
    }

    if ( m_krbv5_profile ) {
        pprofile_release(m_krbv5_profile);
        m_krbv5_profile = NULL;
    }

#ifdef COMMENT
	/* Do not free the locking objects.  Doing so causes an invalid handle access */
    CloseHandle(ticketinfo.lockObj);
#endif
	AfxFreeLibrary(m_hLeashDLL);
#ifndef NO_KRB4
	AfxFreeLibrary(m_hKrb4DLL);
#endif
	AfxFreeLibrary(m_hKrb5DLL);
	AfxFreeLibrary(m_hKrb5ProfileDLL);
	AfxFreeLibrary(m_hAfsDLL);
	AfxFreeLibrary(m_hPsapi);
    AfxFreeLibrary(m_hToolHelp32);
    AfxFreeLibrary(m_hKrbLSA);
#ifdef DEBUG
    _CrtDumpMemoryLeaks();
#endif
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CLeashApp object

CLeashApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CLeashApp initialization

void CLeashApp::ParseParam (LPCTSTR lpszParam,BOOL bFlag,BOOL bLast)
{
	//CCommandLineInfo::ParseParam(lpszParam, bFlag, bLast) ;
}

extern "C" {
    LRESULT WINAPI LeashWindowProc( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
    {
        switch ( Msg ) {
        case WM_SYSCOMMAND:
            if (SC_CLOSE == (wParam & 0xfff0)) {
                wParam = (wParam & ~0xfff0) | SC_MINIMIZE;
            }
            break;
        }
        return ::DefWindowProc(hWnd, Msg, wParam, lParam);
    }
}

BOOL CLeashApp::InitInstance()
{
#ifdef DEBUG
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
    _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );

    int tmp = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG);
    _CrtSetDbgFlag( tmp | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    AfxOleInit();
    // NOTE: Not used at this time
    /// Set LEASH_DLL to the path where the Leash.exe is
    char modulePath[MAX_PATH];
    krb5_error_code code;
    DWORD result = GetModuleFileName(AfxGetInstanceHandle(), modulePath, MAX_PATH);
    ASSERT(result);

    char* pPath = modulePath + strlen(modulePath) - 1;
    while (*pPath != '\\')
    {
        *pPath = 0;
        pPath--;
    }
    strcat(modulePath, LEASH_HELP_FILE);
    m_helpFile = modulePath;

    ///strcat(dllFile, LEASH_DLL);
    ///m_leashDLL = dllFile;

    BOOL autoInit = FALSE;
    HWND hMsg = GetForegroundWindow();
    if (!InitDLLs())
        return FALSE; //exit program, can't load LEASHDLL
    code = pkrb5_init_context(&m_krbv5_context);
    if (code) {
        // @TODO: report error
        return FALSE;
    }

    // Check for args (switches)
    LPCTSTR exeFile		= __targv[0];
    for (int argi = 1; argi < __argc; argi++) {
        LPCTSTR optionParam =  __targv[argi];

        if (!optionParam)
            continue;

        if (*optionParam  == '-' || *optionParam  == '/')
        {
            if (0 == stricmp(optionParam+1, "kinit") ||
                0 == stricmp(optionParam+1, "i"))
            {
                LSH_DLGINFO_EX ldi;
		char username[64]="";
		char realm[192]="";
		int i=0, j=0;
                if (WaitForSingleObject( ticketinfo.lockObj, INFINITE ) != WAIT_OBJECT_0)
                    throw("Unable to lock ticketinfo");

                LeashKRB5ListDefaultTickets(&ticketinfo.Krb5);

                if ( ticketinfo.Krb5.btickets && ticketinfo.Krb5.principal ) {
                    for (; ticketinfo.Krb5.principal[i] && ticketinfo.Krb5.principal[i] != '@'; i++)
                    {
                        username[i] = ticketinfo.Krb5.principal[i];
                    }
                    username[i] = '\0';
                    if (ticketinfo.Krb5.principal[i]) {
                        for (i++ ; ticketinfo.Krb5.principal[i] ; i++, j++)
                        {
                            realm[j] = ticketinfo.Krb5.principal[i];
                        }
                    }
                    realm[j] = '\0';
                }

                LeashKRB5FreeTicketInfo(&ticketinfo.Krb5);

                ReleaseMutex(ticketinfo.lockObj);

				ldi.size = LSH_DLGINFO_EX_V1_SZ;
				ldi.dlgtype = DLGTYPE_PASSWD;
                ldi.title = "MIT Kerberos: Get Ticket";
                ldi.username = username;
				ldi.realm = realm;
                ldi.dlgtype = DLGTYPE_PASSWD;
                ldi.use_defaults = 1;

                if (!pLeash_kinit_dlg_ex(hMsg, &ldi))
                {
                    MessageBox(hMsg, "There was an error getting tickets!",
                               "Error", MB_OK);
                    return FALSE;
                }
                return TRUE;
            }
            else if (0 == stricmp(optionParam+1, "ms2mit") ||
                     0 == stricmp(optionParam+1, "import") ||
                     0 == stricmp(optionParam+1, "m"))
            {
                if (!pLeash_importable()) {
                    MessageBox(hMsg,
                               "The Microsoft Logon Session does not support importing Ticket Getting Tickets!",
                               "Error", MB_OK);
                    return FALSE;
                }

                if (!pLeash_import())
                {
                    MessageBox(hMsg,
                               "There was an error importing tickets from the Microsoft Logon Session!",
                               "Error", MB_OK);
                    return FALSE;
                }
                return TRUE;
            }
            else if (0 == stricmp(optionParam+1, "destroy") ||
                     0 == stricmp(optionParam+1, "d"))
            {
                if (pLeash_kdestroy())
                {
                    MessageBox(hMsg,
                               "There was an error destroying tickets!",
                               "Error", MB_OK);
                    return FALSE;
                }
                return TRUE;
            }
            else if (0 == stricmp(optionParam+1, "renew") ||
                     0 == stricmp(optionParam+1, "r"))
            {
                if (!pLeash_renew())
                {
                    MessageBox(hMsg,
                               "There was an error renewing tickets!",
                               "Error", MB_OK);
                    return FALSE;
                }
                return TRUE;
            }
            else if (0 == stricmp(optionParam+1, "autoinit") ||
                     0 == stricmp(optionParam+1, "a"))
            {
                autoInit = TRUE;
            }
            else if (0 == stricmp(optionParam+1, "console") ||
                     0 == stricmp(optionParam+1, "c"))
            {
                CreateConsoleEcho();
            }
            else if (0 == stricmp(optionParam+1, "noribbon"))
            {
                m_useRibbon = FALSE;
            }
            else
            {
                MessageBox(hMsg,
                           "'-kinit' or '-i' to perform ticket initialization (and exit)\n"
                            "'-renew' or '-r' to perform ticket renewal (and exit)\n"
                            "'-destroy' or '-d' to perform ticket destruction (and exit)\n"
                            "'-autoinit' or '-a' to perform automatic ticket initialization\n"
                            "'-console' or '-c' to attach a console for debugging\n"
                            "'-ms2mit' or '-import' or '-m' to perform ticket importation (and exit)",
                           "MIT Kerberos Error", MB_OK);
                return FALSE;
            }
        }
        else
        {
            MessageBox(hMsg,
                        "'-kinit' or '-i' to perform ticket initialization (and exit)\n"
                        "'-renew' or '-r' to perform ticket renewal (and exit)\n"
                        "'-destroy' or '-d' to perform ticket destruction (and exit)\n"
                        "'-autoinit' or '-a' to perform automatic ticket initialization\n"
                        "'-ms2mit' or '-import' or '-m' to perform ticket importation (and exit)",
                       "MIT Kerberos Error", MB_OK);
            return FALSE;
        }
    }

    // Insure only one instance of Leash
    if (!FirstInstance())
        return FALSE;

    if (!CWinAppEx::InitInstance())
        return FALSE;

    //register our unique wnd class name to find it later
    WNDCLASS wndcls;
    memset(&wndcls, 0, sizeof(WNDCLASS));
    wndcls.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
    wndcls.lpfnWndProc = ::LeashWindowProc;
    wndcls.hInstance = AfxGetInstanceHandle();
    wndcls.hIcon = LoadIcon(IDR_MAINFRAME);
    wndcls.hCursor = LoadCursor(IDC_ARROW);
    wndcls.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndcls.lpszMenuName = NULL;
    //now the wnd class name to find it
    wndcls.lpszClassName = _T("LEASH.0WNDCLASS");

    //register the new class
    if(!AfxRegisterClass(&wndcls))
    {
        TRACE("Class registration failed\n");
        return FALSE;
    }

    AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#if _MSC_VER < 1300
#ifdef _AFXDLL
    Enable3dControls();			// Call this when using MFC in a shared DLL
#else
    Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif
#endif

    // Registry key under which our settings are stored.
    if (m_pszAppName)
        free((void*)m_pszAppName);
    m_pszAppName = _tcsdup("MIT Kerberos");
    SetRegistryKey(_T("MIT"));

    LoadStdProfileSettings(); // Load standard INI file options (including MRU)

    // Register the application's document templates.  Document templates
    //  serve as the connection between documents, frame windows and views.

    CSingleDocTemplate* pDocTemplate;
    pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(LeashDoc),
        RUNTIME_CLASS(CMainFrame),       // main SDI frame window
        RUNTIME_CLASS(CLeashView));
    AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
    if (!ProcessShellCommand(cmdInfo))
        return FALSE;

    // Check to see if there are any tickets in the cache
    // If not and the Windows Logon Session is Kerberos authenticated attempt an import
    {
        if (WaitForSingleObject( ticketinfo.lockObj, INFINITE ) != WAIT_OBJECT_0)
            throw("Unable to lock ticketinfo");
        LeashKRB5ListDefaultTickets(&ticketinfo.Krb5);
        BOOL b_autoinit = !ticketinfo.Krb5.btickets;
        LeashKRB5FreeTicketInfo(&ticketinfo.Krb5);
        ReleaseMutex(ticketinfo.lockObj);

        DWORD dwMsLsaImport = pLeash_get_default_mslsa_import();

        if ( b_autoinit && dwMsLsaImport && pLeash_importable() ) {
            // We have the option of importing tickets from the MSLSA
            // but should we?  Do the tickets in the MSLSA cache belong
            // to the default realm used by Leash?  If so, import.
            int import = 0;

            if ( dwMsLsaImport == 1 ) {             /* always import */
                import = 1;
            } else if ( dwMsLsaImport == 2 ) {      /* import when realms match */
                krb5_error_code code;
                krb5_ccache mslsa_ccache=0;
                krb5_principal princ = 0;
                char ms_realm[128] = "", *def_realm = 0, *r;
                int i;

                if (code = pkrb5_cc_resolve(CLeashApp::m_krbv5_context, "MSLSA:", &mslsa_ccache))
                    goto cleanup;

                if (code = pkrb5_cc_get_principal(CLeashApp::m_krbv5_context, mslsa_ccache, &princ))
                    goto cleanup;

                for ( r=ms_realm, i=0; i<krb5_princ_realm(CLeashApp::m_krb5v5_context, princ)->length; r++, i++ ) {
                    *r = krb5_princ_realm(CLeashApp::m_krb5v5_context, princ)->data[i];
                }
                *r = '\0';

                if (code = pkrb5_get_default_realm(CLeashApp::m_krbv5_context, &def_realm))
                    goto cleanup;

                import = !strcmp(def_realm, ms_realm);

              cleanup:
                if (def_realm)
                    pkrb5_free_default_realm(CLeashApp::m_krbv5_context, def_realm);

                if (princ)
                    pkrb5_free_principal(CLeashApp::m_krbv5_context, princ);

                if (mslsa_ccache)
                    pkrb5_cc_close(CLeashApp::m_krbv5_context, mslsa_ccache);
            }

            if (import && pLeash_import()) {
                CLeashView::m_importedTickets = 1;
                ::PostMessage(m_pMainWnd->m_hWnd, WM_COMMAND, ID_UPDATE_DISPLAY, 0);
                b_autoinit = FALSE;
            }
        }

        if (autoInit) {
            if ( b_autoinit )
                AfxBeginThread(InitWorker, m_pMainWnd->m_hWnd);

            IpAddrChangeMonitorInit(m_pMainWnd->m_hWnd);
        }
    }

    VScheckVersion(m_pMainWnd->m_hWnd, AfxGetInstanceHandle());

    // The one and only window has been initialized, so show and update it.
    m_pMainWnd->SetWindowText("MIT Kerberos");
    m_pMainWnd->UpdateWindow();
    m_pMainWnd->ShowWindow(SW_SHOW);
    m_pMainWnd->SetForegroundWindow();

    ValidateConfigFiles();

    return TRUE;
}


/////////////////////////////////////////////////////////////////////////////
// CLeashApp commands

// leash functions
DECL_FUNC_PTR(not_an_API_LeashKRB4GetTickets);
DECL_FUNC_PTR(not_an_API_LeashAFSGetToken);
DECL_FUNC_PTR(not_an_API_LeashGetTimeServerName);
DECL_FUNC_PTR(Leash_kdestroy);
DECL_FUNC_PTR(Leash_changepwd_dlg);
DECL_FUNC_PTR(Leash_changepwd_dlg_ex);
DECL_FUNC_PTR(Leash_kinit_dlg);
DECL_FUNC_PTR(Leash_kinit_dlg_ex);
DECL_FUNC_PTR(Leash_timesync);
DECL_FUNC_PTR(Leash_get_default_lifetime);
DECL_FUNC_PTR(Leash_set_default_lifetime);
DECL_FUNC_PTR(Leash_get_default_forwardable);
DECL_FUNC_PTR(Leash_set_default_forwardable);
DECL_FUNC_PTR(Leash_get_default_renew_till);
DECL_FUNC_PTR(Leash_set_default_renew_till);
DECL_FUNC_PTR(Leash_get_default_noaddresses);
DECL_FUNC_PTR(Leash_set_default_noaddresses);
DECL_FUNC_PTR(Leash_get_default_proxiable);
DECL_FUNC_PTR(Leash_set_default_proxiable);
DECL_FUNC_PTR(Leash_get_default_publicip);
DECL_FUNC_PTR(Leash_set_default_publicip);
DECL_FUNC_PTR(Leash_get_default_use_krb4);
DECL_FUNC_PTR(Leash_set_default_use_krb4);
DECL_FUNC_PTR(Leash_get_default_life_min);
DECL_FUNC_PTR(Leash_set_default_life_min);
DECL_FUNC_PTR(Leash_get_default_life_max);
DECL_FUNC_PTR(Leash_set_default_life_max);
DECL_FUNC_PTR(Leash_get_default_renew_min);
DECL_FUNC_PTR(Leash_set_default_renew_min);
DECL_FUNC_PTR(Leash_get_default_renew_max);
DECL_FUNC_PTR(Leash_set_default_renew_max);
DECL_FUNC_PTR(Leash_get_default_renewable);
DECL_FUNC_PTR(Leash_set_default_renewable);
DECL_FUNC_PTR(Leash_get_lock_file_locations);
DECL_FUNC_PTR(Leash_set_lock_file_locations);
DECL_FUNC_PTR(Leash_get_default_uppercaserealm);
DECL_FUNC_PTR(Leash_set_default_uppercaserealm);
DECL_FUNC_PTR(Leash_get_default_mslsa_import);
DECL_FUNC_PTR(Leash_set_default_mslsa_import);
DECL_FUNC_PTR(Leash_get_default_preserve_kinit_settings);
DECL_FUNC_PTR(Leash_set_default_preserve_kinit_settings);
DECL_FUNC_PTR(Leash_import);
DECL_FUNC_PTR(Leash_importable);
DECL_FUNC_PTR(Leash_renew);
DECL_FUNC_PTR(Leash_reset_defaults);

FUNC_INFO leash_fi[] = {
    MAKE_FUNC_INFO(not_an_API_LeashKRB4GetTickets),
    MAKE_FUNC_INFO(not_an_API_LeashAFSGetToken),
    MAKE_FUNC_INFO(not_an_API_LeashGetTimeServerName),
    MAKE_FUNC_INFO(Leash_kdestroy),
    MAKE_FUNC_INFO(Leash_changepwd_dlg),
    MAKE_FUNC_INFO(Leash_changepwd_dlg_ex),
    MAKE_FUNC_INFO(Leash_kinit_dlg),
	MAKE_FUNC_INFO(Leash_kinit_dlg_ex),
    MAKE_FUNC_INFO(Leash_timesync),
    MAKE_FUNC_INFO(Leash_get_default_lifetime),
    MAKE_FUNC_INFO(Leash_set_default_lifetime),
    MAKE_FUNC_INFO(Leash_get_default_renew_till),
    MAKE_FUNC_INFO(Leash_set_default_renew_till),
    MAKE_FUNC_INFO(Leash_get_default_forwardable),
    MAKE_FUNC_INFO(Leash_set_default_forwardable),
    MAKE_FUNC_INFO(Leash_get_default_noaddresses),
    MAKE_FUNC_INFO(Leash_set_default_noaddresses),
    MAKE_FUNC_INFO(Leash_get_default_proxiable),
    MAKE_FUNC_INFO(Leash_set_default_proxiable),
    MAKE_FUNC_INFO(Leash_get_default_publicip),
    MAKE_FUNC_INFO(Leash_set_default_publicip),
    MAKE_FUNC_INFO(Leash_get_default_use_krb4),
    MAKE_FUNC_INFO(Leash_set_default_use_krb4),
    MAKE_FUNC_INFO(Leash_get_default_life_min),
    MAKE_FUNC_INFO(Leash_set_default_life_min),
    MAKE_FUNC_INFO(Leash_get_default_life_max),
    MAKE_FUNC_INFO(Leash_set_default_life_max),
    MAKE_FUNC_INFO(Leash_get_default_renew_min),
    MAKE_FUNC_INFO(Leash_set_default_renew_min),
    MAKE_FUNC_INFO(Leash_get_default_renew_max),
    MAKE_FUNC_INFO(Leash_set_default_renew_max),
    MAKE_FUNC_INFO(Leash_get_default_renewable),
    MAKE_FUNC_INFO(Leash_set_default_renewable),
    MAKE_FUNC_INFO(Leash_get_lock_file_locations),
    MAKE_FUNC_INFO(Leash_set_lock_file_locations),
    MAKE_FUNC_INFO(Leash_get_default_uppercaserealm),
    MAKE_FUNC_INFO(Leash_set_default_uppercaserealm),
    MAKE_FUNC_INFO(Leash_get_default_mslsa_import),
    MAKE_FUNC_INFO(Leash_set_default_mslsa_import),
    MAKE_FUNC_INFO(Leash_get_default_preserve_kinit_settings),
    MAKE_FUNC_INFO(Leash_set_default_preserve_kinit_settings),
    MAKE_FUNC_INFO(Leash_import),
    MAKE_FUNC_INFO(Leash_importable),
    MAKE_FUNC_INFO(Leash_renew),
    MAKE_FUNC_INFO(Leash_reset_defaults),
    END_FUNC_INFO
};

////
#ifndef NO_KRB4
// krb4 functions
DECL_FUNC_PTR(set_krb_debug);
DECL_FUNC_PTR(set_krb_ap_req_debug);
DECL_FUNC_PTR(krb_get_krbconf2);
DECL_FUNC_PTR(krb_get_krbrealm2);
DECL_FUNC_PTR(tkt_string);
DECL_FUNC_PTR(krb_set_tkt_string);
DECL_FUNC_PTR(krb_realmofhost);
DECL_FUNC_PTR(krb_get_lrealm);
DECL_FUNC_PTR(krb_get_krbhst);
DECL_FUNC_PTR(tf_init);
DECL_FUNC_PTR(tf_close);
DECL_FUNC_PTR(krb_get_tf_realm);

FUNC_INFO krb4_fi[] = {
    MAKE_FUNC_INFO(set_krb_debug),
    MAKE_FUNC_INFO(set_krb_ap_req_debug),
    MAKE_FUNC_INFO(krb_get_krbconf2),
    MAKE_FUNC_INFO(krb_get_krbrealm2),
    MAKE_FUNC_INFO(tkt_string),
    MAKE_FUNC_INFO(krb_set_tkt_string),
    MAKE_FUNC_INFO(krb_realmofhost),
    MAKE_FUNC_INFO(krb_get_lrealm),
    MAKE_FUNC_INFO(krb_get_krbhst),
    MAKE_FUNC_INFO(tf_init),
    MAKE_FUNC_INFO(tf_close),
    MAKE_FUNC_INFO(krb_get_tf_realm),
    END_FUNC_INFO
};
#endif

// com_err funcitons
DECL_FUNC_PTR(error_message);
FUNC_INFO ce_fi[] =  {
    MAKE_FUNC_INFO(error_message),
    END_FUNC_INFO
};

// psapi functions
DECL_FUNC_PTR(GetModuleFileNameExA);
DECL_FUNC_PTR(EnumProcessModules);

FUNC_INFO psapi_fi[] = {
    MAKE_FUNC_INFO(GetModuleFileNameExA),
    MAKE_FUNC_INFO(EnumProcessModules),
    END_FUNC_INFO
};

// toolhelp functions
DECL_FUNC_PTR(CreateToolhelp32Snapshot);
DECL_FUNC_PTR(Module32First);
DECL_FUNC_PTR(Module32Next);

FUNC_INFO toolhelp_fi[] = {
    MAKE_FUNC_INFO(CreateToolhelp32Snapshot),
    MAKE_FUNC_INFO(Module32First),
    MAKE_FUNC_INFO(Module32Next),
    END_FUNC_INFO
};

// krb5 functions
DECL_FUNC_PTR(krb5_cc_default_name);
DECL_FUNC_PTR(krb5_cc_set_default_name);
DECL_FUNC_PTR(krb5_get_default_config_files);
DECL_FUNC_PTR(krb5_free_config_files);
DECL_FUNC_PTR(krb5_free_context);
DECL_FUNC_PTR(krb5_get_default_realm);
DECL_FUNC_PTR(krb5_free_default_realm);
DECL_FUNC_PTR(krb5_init_context);
DECL_FUNC_PTR(krb5_cc_default);
DECL_FUNC_PTR(krb5_parse_name);
DECL_FUNC_PTR(krb5_free_principal);
DECL_FUNC_PTR(krb5_cc_close);
DECL_FUNC_PTR(krb5_cc_get_principal);
DECL_FUNC_PTR(krb5_build_principal);
DECL_FUNC_PTR(krb5_c_random_make_octets);
DECL_FUNC_PTR(krb5_get_init_creds_password);
DECL_FUNC_PTR(krb5_free_cred_contents);
DECL_FUNC_PTR(krb5_cc_resolve);
DECL_FUNC_PTR(krb5_unparse_name);
DECL_FUNC_PTR(krb5_free_unparsed_name);
DECL_FUNC_PTR(krb5_cc_destroy);
DECL_FUNC_PTR(krb5_cccol_cursor_new);
DECL_FUNC_PTR(krb5_cccol_cursor_free);
DECL_FUNC_PTR(krb5_cccol_cursor_next);
DECL_FUNC_PTR(krb5_cc_start_seq_get);
DECL_FUNC_PTR(krb5_cc_next_cred);
DECL_FUNC_PTR(krb5_cc_end_seq_get);
DECL_FUNC_PTR(krb5_cc_get_name);
DECL_FUNC_PTR(krb5_cc_set_flags);
DECL_FUNC_PTR(krb5_is_config_principal);
DECL_FUNC_PTR(krb5_free_ticket);
DECL_FUNC_PTR(krb5_decode_ticket);
DECL_FUNC_PTR(krb5_cc_switch);
DECL_FUNC_PTR(krb5_build_principal_ext);
DECL_FUNC_PTR(krb5_get_renewed_creds);
DECL_FUNC_PTR(krb5_cc_initialize);
DECL_FUNC_PTR(krb5_cc_store_cred);
DECL_FUNC_PTR(krb5_cc_get_full_name);
DECL_FUNC_PTR(krb5_free_string);
DECL_FUNC_PTR(krb5_enctype_to_name);
DECL_FUNC_PTR(krb5_cc_get_type);
DECL_FUNC_PTR(krb5int_cc_user_set_default_name);

FUNC_INFO krb5_fi[] = {
    MAKE_FUNC_INFO(krb5_cc_default_name),
    MAKE_FUNC_INFO(krb5_cc_set_default_name),
    MAKE_FUNC_INFO(krb5_get_default_config_files),
    MAKE_FUNC_INFO(krb5_free_config_files),
    MAKE_FUNC_INFO(krb5_free_context),
    MAKE_FUNC_INFO(krb5_get_default_realm),
    MAKE_FUNC_INFO(krb5_free_default_realm),
    MAKE_FUNC_INFO(krb5_init_context),
    MAKE_FUNC_INFO(krb5_cc_default),
    MAKE_FUNC_INFO(krb5_parse_name),
    MAKE_FUNC_INFO(krb5_free_principal),
    MAKE_FUNC_INFO(krb5_cc_close),
    MAKE_FUNC_INFO(krb5_cc_get_principal),
    MAKE_FUNC_INFO(krb5_build_principal),
    MAKE_FUNC_INFO(krb5_c_random_make_octets),
    MAKE_FUNC_INFO(krb5_get_init_creds_password),
    MAKE_FUNC_INFO(krb5_free_cred_contents),
    MAKE_FUNC_INFO(krb5_cc_resolve),
    MAKE_FUNC_INFO(krb5_unparse_name),
    MAKE_FUNC_INFO(krb5_free_unparsed_name),
    MAKE_FUNC_INFO(krb5_cc_destroy),
    MAKE_FUNC_INFO(krb5_cccol_cursor_new),
    MAKE_FUNC_INFO(krb5_cccol_cursor_next),
    MAKE_FUNC_INFO(krb5_cccol_cursor_free),
    MAKE_FUNC_INFO(krb5_cc_start_seq_get),
    MAKE_FUNC_INFO(krb5_cc_next_cred),
    MAKE_FUNC_INFO(krb5_cc_end_seq_get),
    MAKE_FUNC_INFO(krb5_cc_get_name),
    MAKE_FUNC_INFO(krb5_cc_set_flags),
    MAKE_FUNC_INFO(krb5_is_config_principal),
    MAKE_FUNC_INFO(krb5_free_ticket),
    MAKE_FUNC_INFO(krb5_decode_ticket),
    MAKE_FUNC_INFO(krb5_cc_switch),
    MAKE_FUNC_INFO(krb5_build_principal_ext),
    MAKE_FUNC_INFO(krb5_get_renewed_creds),
    MAKE_FUNC_INFO(krb5_cc_initialize),
    MAKE_FUNC_INFO(krb5_cc_store_cred),
    MAKE_FUNC_INFO(krb5_cc_get_full_name),
    MAKE_FUNC_INFO(krb5_free_string),
    MAKE_FUNC_INFO(krb5_enctype_to_name),
    MAKE_FUNC_INFO(krb5_cc_get_type),
    MAKE_FUNC_INFO(krb5int_cc_user_set_default_name),
    END_FUNC_INFO
};

// profile functions
DECL_FUNC_PTR(profile_release);
DECL_FUNC_PTR(profile_init);
DECL_FUNC_PTR(profile_flush);
DECL_FUNC_PTR(profile_rename_section);
DECL_FUNC_PTR(profile_update_relation);
DECL_FUNC_PTR(profile_clear_relation);
DECL_FUNC_PTR(profile_add_relation);
DECL_FUNC_PTR(profile_get_relation_names);
DECL_FUNC_PTR(profile_get_subsection_names);
DECL_FUNC_PTR(profile_get_values);
DECL_FUNC_PTR(profile_free_list);
DECL_FUNC_PTR(profile_abandon);
DECL_FUNC_PTR(profile_get_string);
DECL_FUNC_PTR(profile_release_string);

FUNC_INFO profile_fi[] = {
    MAKE_FUNC_INFO(profile_release),
    MAKE_FUNC_INFO(profile_init),
    MAKE_FUNC_INFO(profile_flush),
    MAKE_FUNC_INFO(profile_rename_section),
    MAKE_FUNC_INFO(profile_update_relation),
    MAKE_FUNC_INFO(profile_clear_relation),
    MAKE_FUNC_INFO(profile_add_relation),
    MAKE_FUNC_INFO(profile_get_relation_names),
    MAKE_FUNC_INFO(profile_get_subsection_names),
    MAKE_FUNC_INFO(profile_get_values),
    MAKE_FUNC_INFO(profile_free_list),
    MAKE_FUNC_INFO(profile_abandon),
    MAKE_FUNC_INFO(profile_get_string),
    MAKE_FUNC_INFO(profile_release_string),
    END_FUNC_INFO
};

// Tries to load the .DLL files.  If it works, we get some functions from them
// and return a TRUE.  If it doesn't work, we return a FALSE.
BOOL CLeashApp::InitDLLs()
{
    m_hLeashDLL = AfxLoadLibrary(LEASHDLL);
#ifndef NO_KRB4
    m_hKrb4DLL = AfxLoadLibrary(KERB4DLL);
#endif
    m_hKrb5DLL = AfxLoadLibrary(KERB5DLL);
    m_hKrb5ProfileDLL = AfxLoadLibrary(KERB5_PPROFILE_DLL);
    m_hComErr = AfxLoadLibrary(COMERR_DLL);

#ifndef NO_AFS
    afscompat_init();
    m_hAfsDLL = AfxLoadLibrary(AFSAuthentDLL());
#endif

#define PSAPIDLL "psapi.dll"
#define TOOLHELPDLL "kernel32.dll"

    m_hPsapi = AfxLoadLibrary(PSAPIDLL);
    m_hToolHelp32 = AfxLoadLibrary(TOOLHELPDLL);

    HWND hwnd = GetForegroundWindow();
    if (!m_hLeashDLL)
    {
        // We couldn't load the m_hLeashDLL.
        m_msgError = "Couldn't load the Leash DLL or one of its dependents.";
        MessageBox(hwnd, m_msgError, "Error", MB_OK);
        return FALSE;
    }

    if (!LoadFuncs(LEASHDLL, leash_fi, 0, 0, 1, 0, 0))
    {
        MessageBox(hwnd,
                   "Functions within the Leash DLL didn't load properly!",
                   "Error", MB_OK);
        return FALSE;
    }

    if (!LoadFuncs(COMERR_DLL, ce_fi, &m_hComErr, 0, 0, 1, 0)) {
        MessageBox(hwnd,
                   "Functions within " COMERR_DLL "didn't load properly!",
                   "Error", MB_OK);
        return FALSE;
    }
////
#ifndef NO_KRB4
    if (m_hKrb4DLL)
    {
        if (!LoadFuncs(KERB4DLL, krb4_fi, 0, 0, 1, 0, 0))
        {
            MessageBox(hwnd,
                       "Unexpected error while loading " KERB4DLL ".\n"
                       "Kerberos 4 functionality will be disabled.\n",
                       "Error", MB_OK);
        }
    }
#endif

    if (m_hKrb5DLL)
    {
        if (!LoadFuncs(KERB5DLL, krb5_fi, 0, 0, 1, 0, 0))
        {
            MessageBox(hwnd,
                       "Unexpected error while loading " KERB5DLL ".\n"
                       "Kerberos 5 functionality will be disabled.\n",
                       "Error", MB_OK);
            AfxFreeLibrary(m_hKrb5DLL);
            m_hKrb5DLL = 0;
        }
        else if (!m_hKrb5ProfileDLL ||
                 !LoadFuncs(KERB5_PPROFILE_DLL, profile_fi, 0, 0, 1, 0, 0))
        {
            MessageBox(hwnd,
                       "Unexpected error while loading " KERB5_PPROFILE_DLL "."
                       "\nKerberos 5 functionality will be disabled.\n",
                       "Error", MB_OK);
            AfxFreeLibrary(m_hKrb5ProfileDLL);
            m_hKrb5ProfileDLL = 0;
            // Use m_hKrb5DLL to undo LoadLibrary in loadfuncs...
            UnloadFuncs(krb5_fi, m_hKrb5DLL);
            AfxFreeLibrary(m_hKrb5DLL);
            m_hKrb5DLL = 0;
        }

    }

    OSVERSIONINFO osvi;
    memset(&osvi, 0, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osvi);

    // XXX: We should really use feature testing, first
    // checking for CreateToolhelp32Snapshot.  If that's
    // not around, we try the psapi stuff.
    //
    // Only load LSA functions if on NT/2000/XP
    if(osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
        // Windows 9x
        AfxFreeLibrary(m_hPsapi);
        m_hPsapi = NULL;
        if (!m_hToolHelp32 ||
            !LoadFuncs(TOOLHELPDLL, toolhelp_fi, 0, 0, 1, 0, 0))
        {
            MessageBox(hwnd, "Could not load " TOOLHELPDLL "!", "Error",
                       MB_OK);
            return FALSE;
        }
    }
    else if(osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        // Windows NT
        AfxFreeLibrary(m_hToolHelp32);
        m_hToolHelp32 = NULL;
        if (!m_hPsapi ||
            !LoadFuncs(PSAPIDLL, psapi_fi, 0, 0, 1, 0, 0))
        {
            MessageBox(hwnd, "Could not load " PSAPIDLL "!", "Error", MB_OK);
            return FALSE;
        }

		m_hKrbLSA  = AfxLoadLibrary(SECUR32DLL);
    }
    else
    {
        MessageBox(hwnd,
                   "Unrecognized Operating System!",
                   "Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}


BOOL CLeashApp::FirstInstance()
{
    CWnd* pWndprev;
    CWnd* pWndchild;

    //find if it exists
    pWndprev = CWnd::FindWindow(_T("LEASH.0WNDCLASS"), NULL);
    if (pWndprev)
    {
        //if it has popups
        pWndchild = pWndprev->GetLastActivePopup();
        //if iconic restore
        if (pWndprev->IsIconic())
            pWndprev->ShowWindow(SW_RESTORE);

        //bring the wnd to foreground
        pWndchild->SetForegroundWindow();

        return FALSE;
    }
    //we could not find prev instance
    else
        return TRUE;
}

void
CLeashApp::ValidateConfigFiles()
{
    CStdioFile krbCon;
    char confname[257];
    char realm[256]="";

    CWinApp * pApp = AfxGetApp();
    if (pApp)
        if (!pApp->GetProfileInt("Settings", "CreateMissingConfig", FALSE_FLAG))
            return;

    if ( m_hKrb5DLL ) {
        int krb_con_open = 0;

        // Create the empty KRB5.INI file
        if (!GetProfileFile(confname,sizeof(confname))) {
            const char *filenames[2];
		    filenames[0] = confname;
		    filenames[1] = NULL;
		    long retval = pprofile_init(filenames, &m_krbv5_profile);
			if (!retval)
				return;
			else if (retval == ENOENT) {
				FILE * f = fopen(confname,"w");
				if (f != NULL) {
					fclose(f);
					retval = pprofile_init(filenames, &m_krbv5_profile);
				}
			}


            if ( !GetKrb4ConFile(confname,sizeof(confname)) ) {
                if (!krbCon.Open(confname, CFile::modeNoTruncate | CFile::modeRead))
                {
                    if (krbCon.Open(confname, CFile::modeCreate | CFile::modeWrite))
                    {
                        krb_con_open = 1;
                    }
                }
            }

            const char*  lookupKdc[] = {"libdefaults", "dns_lookup_kdc", NULL};
            const char*  lookupRealm[] = {"libdefaults", "dns_lookup_realm", NULL};
            const char*  defRealm[] = {"libdefaults", "default_realm", NULL};
            const char*  noAddresses[] = {"libdefaults", "noaddresses", NULL};

            // activate DNS KDC Lookups
            const char** names = lookupKdc;
            retval = pprofile_add_relation(m_krbv5_profile,
                                           names,
                                           "true");

            // activate No Addresses
            names = noAddresses;
            retval = pprofile_add_relation(m_krbv5_profile,
                                           names,
                                           "true");

            // Get Windows 2000/XP/2003 Kerberos config
            if ( m_hKrbLSA && m_hKrb5DLL )
            {
                char domain[256]="";
                HKEY hk=0;
                DWORD dwType, dwSize, dwIndex;

                if ( !RegOpenKeyEx(HKEY_CURRENT_USER,
                                    "Volatile Environment", 0,
                                    KEY_READ, &hk) )
                {
                    dwSize = sizeof(domain);
                    RegQueryValueEx(hk, "USERDNSDOMAIN", 0, 0, (LPBYTE)domain, &dwSize);
                    RegCloseKey(hk);
                }
                else if (!RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                             "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                             0, KEY_READ, &hk))
                {

                    dwSize = sizeof(domain);
                    RegQueryValueEx( hk, "DefaultDomainName",
                                     NULL, &dwType, (unsigned char *)&domain, &dwSize);
                    RegCloseKey(hk);
                }

                char realmkey[256]="SYSTEM\\CurrentControlSet\\Control\\Lsa\\Kerberos\\Domains\\";
                size_t  keylen = strlen(realmkey)-1;

                if ( domain[0] ) {
                    strncpy(realm,domain,256);
                    realm[255] = '\0';
                    if ( krb_con_open ) {
                        krbCon.WriteString(realm);
                        krbCon.WriteString("\n");
                    }
                    strncat(realmkey,domain,256-strlen(realmkey));
                    realmkey[255] = '\0';
                }

                if ( domain[0] &&
                     !RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                   realmkey,
                                   0,
                                   KEY_READ,
                                   &hk)
                     )
                {
                    RegCloseKey(hk);

                    realmkey[keylen] = '\0';
                    RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                                 realmkey,
                                 0,
                                 KEY_READ|KEY_ENUMERATE_SUB_KEYS,
                                 &hk);

                    dwIndex = 0;
                    unsigned char subkey[256];
                    FILETIME ft;
                    dwSize = 256;
                    while ( ERROR_SUCCESS == RegEnumKeyEx(hk,dwIndex++,
                                                          (char *)subkey,
                                                          &dwSize,
                                                          0,
                                                          0,
                                                          0,
                                                          &ft) )
                    {
                        HKEY hksub;

                        if ( !RegOpenKeyEx(hk,
                                   (char *)subkey,
                                   0,
                                   KEY_READ,
                                   &hksub) )
                        {
                            unsigned char * lpszValue = NULL, *p;
                            dwSize = 0;
							dwType = 0;
                            RegQueryValueEx( hksub, "KdcNames",
                                             NULL, &dwType, lpszValue, &dwSize);
                            if ( dwSize > 0 ) {
                                lpszValue = (unsigned char *)malloc(dwSize+1);
                                dwSize += 1;
                                RegQueryValueEx( hksub, "KdcNames",
                                                 NULL, &dwType, lpszValue, &dwSize);

                                p = lpszValue;
                                while ( *p ) {
                                    const char*  realmKdc[] = {"realms", (const char *)subkey, "kdc", NULL};
                                    names = realmKdc;
                                    retval = pprofile_add_relation(m_krbv5_profile,
                                                                    names,
                                                                    (const char *)p);

                                    if ( krb_con_open ) {
                                        krbCon.WriteString((const char *)subkey);
                                        krbCon.WriteString("\t");
                                        krbCon.WriteString((const char *)p);
                                        krbCon.WriteString("\n");
                                    }

                                    p += strlen((char*)p) + 1;
                                }
                                free(lpszValue);
                            }
                            RegCloseKey(hksub);
                        }
                    }
                    RegCloseKey(hk);
                }
            } else {
                // activate DNS Realm Lookups (temporarily)
                names = lookupRealm;
                retval = pprofile_add_relation(m_krbv5_profile,
                                                names,
                                                "true");
            }

            // Save to Kerberos Five config. file "Krb5.ini"
            retval = pprofile_flush(m_krbv5_profile);


            // Use DNS to retrieve the realm (if possible)
            if (!realm[0]) {
                krb5_context ctx = 0;
                krb5_principal me = 0;
                krb5_error_code code = 0;

                code = pkrb5_init_context(&ctx);
                if (code) goto no_k5_realm;

                code = pkrb5_parse_name(ctx, "foo", &me);
                if (code) goto no_k5_realm;

                if ( krb5_princ_realm(ctx,me)->length < sizeof(realm) - 1) {
                    memcpy(realm, krb5_princ_realm(ctx,me)->data,
                            krb5_princ_realm(ctx,me)->length);
                    realm[krb5_princ_realm(ctx,me)->length] = '\0';
                }

                if ( krb_con_open ) {
                    krbCon.WriteString(realm);
                    krbCon.WriteString("\n");
                }

              no_k5_realm:
                if ( me )
                    pkrb5_free_principal(ctx,me);
                if ( ctx )
                    pkrb5_free_context(ctx);
            }

            // disable DNS Realm Lookups
            retval = pprofile_update_relation(m_krbv5_profile,
                                             names,
                                             "true", "false");

            // save the default realm if it was discovered
            if ( realm[0] ) {
                names = defRealm;
                retval = pprofile_add_relation(m_krbv5_profile,
                                               names,
                                               realm);

                // It would be nice to be able to generate a list of KDCs
                // but to do so based upon the contents of DNS would be
                // wrong for several reasons:
                // . it would make static the values inserted into DNS SRV
                //   records
                // . DNS cannot necessarily be trusted
            }

            // Save to Kerberos Five config. file "Krb5.ini"
            retval = pprofile_flush(m_krbv5_profile);

            pprofile_release(m_krbv5_profile);
            m_krbv5_profile = NULL;

            // Close KRB.CON file
            if ( krb_con_open ) {
                krbCon.WriteString(".KERBEROS.OPTION. dns\n");
                krbCon.Close();
            }

            // Create the empty KRBREALM.CON file
            if ( !GetKrb4RealmFile(confname,sizeof(confname)) ) {
                if (!krbCon.Open(confname, CFile::modeNoTruncate | CFile::modeRead))
                {
                    if (krbCon.Open(confname, CFile::modeCreate | CFile::modeWrite))
                    {
                        krbCon.Close();
                    }
                } else
                    krbCon.Close();
            }

        }
////
#ifndef NO_KRB4
    } else if ( m_hKrb4DLL ) {
        if ( !realm[0] ) {
            /* Open ticket file */
            char * file = ptkt_string();
            int k_errno;

            if (file != NULL && file[0]) {
                if ((k_errno = ptf_init(file, R_TKT_FIL)) == KSUCCESS) {
                    /* Close ticket file */
                    (void) ptf_close();

                    k_errno = pkrb_get_tf_realm(file, realm);
                }
                if (k_errno != KSUCCESS) {
                    k_errno = pkrb_get_lrealm(realm, 1);
                }
            }
        }

        if ( !GetKrb4ConFile(confname,sizeof(confname)) ) {
            if (!krbCon.Open(confname, CFile::modeNoTruncate | CFile::modeRead))
            {
                if (krbCon.Open(confname, CFile::modeCreate | CFile::modeWrite))
                {
                    if ( realm[0] )
                        krbCon.WriteString(realm);
                    krbCon.WriteString("\n.KERBEROS.OPTION. dns\n");
                    krbCon.Close();
                }
            } else
				krbCon.Close();
        }

        if ( !GetKrb4RealmFile(confname,sizeof(confname)) ) {
            if (!krbCon.Open(confname, CFile::modeNoTruncate | CFile::modeRead))
            {
                if (krbCon.Open(confname, CFile::modeCreate | CFile::modeWrite))
                {
                    krbCon.Close();
                }
            } else
				krbCon.Close();
        }
#endif
    }
}

////@#+Should this be just deleted or reworked?
BOOL
CLeashApp::GetKrb4ConFile(
    LPSTR confname,
    UINT szConfname
    )
{
    if (m_hKrb5DLL
////
#ifndef NO_KRB4
	&& !m_hKrb4DLL
#endif
	)
	{ // hold krb.con where krb5.ini is located
		CHAR krbConFile[MAX_PATH]="";
	    //strcpy(krbConFile, CLeashApp::m_krbv5_profile->first_file->filename);
        if (GetProfileFile(krbConFile, sizeof(krbConFile)))
        {
		    GetWindowsDirectory(krbConFile,sizeof(krbConFile));
            krbConFile[MAX_PATH-1] = '\0';
			strncat(krbConFile,"\\KRB5.INI",sizeof(krbConFile)-strlen(krbConFile)-1);
            krbConFile[MAX_PATH-1] = '\0';
        }

		LPSTR pFind = strrchr(krbConFile, '\\');
		if (pFind)
		{
			*pFind = 0;
			strncat(krbConFile, "\\",MAX_PATH-1);
            krbConFile[MAX_PATH-1] = '\0';
			strncat(krbConFile, KRB_FILE,MAX_PATH-1);
            krbConFile[MAX_PATH-1] = '\0';
		}
		else
		  ASSERT(0);

		strncpy(confname, krbConFile, szConfname);
        confname[szConfname-1] = '\0';
	}
////
#ifndef NO_KRB4
	else if (m_hKrb4DLL)
	{
        unsigned int size = szConfname;
        memset(confname, '\0', szConfname);
		if (!pkrb_get_krbconf2(confname, &size))
		{ // Error has happened
		    GetWindowsDirectory(confname,szConfname);
            confname[szConfname-1] = '\0';
			strncat(confname, "\\",szConfname);
            confname[szConfname-1] = '\0';
			strncat(confname,KRB_FILE,szConfname);
            confname[szConfname-1] = '\0';
		}
	}
#endif

    return FALSE;
}

BOOL
CLeashApp::GetKrb4RealmFile(
    LPSTR confname,
    UINT szConfname
    )
{
    if (m_hKrb5DLL
////
#ifndef NO_KRB4
	&& !m_hKrb4DLL
#endif
	)
	{ // hold krb.con where krb5.ini is located
		CHAR krbRealmConFile[MAX_PATH];
		//strcpy(krbRealmConFile, CLeashApp::m_krbv5_profile->first_file->filename);
		if (GetProfileFile(krbRealmConFile, sizeof(krbRealmConFile)))
        {
		    GetWindowsDirectory(krbRealmConFile,sizeof(krbRealmConFile));
            krbRealmConFile[MAX_PATH-1] = '\0';
			strncat(krbRealmConFile,"\\KRB5.INI",sizeof(krbRealmConFile)-strlen(krbRealmConFile));
            krbRealmConFile[MAX_PATH-1] = '\0';
        }

		LPSTR pFind = strrchr(krbRealmConFile, '\\');
		if (pFind)
		{
			*pFind = 0;
			strncat(krbRealmConFile, "\\",MAX_PATH-1-strlen(krbRealmConFile));
            krbRealmConFile[MAX_PATH-1] = '\0';
			strncat(krbRealmConFile, KRBREALM_FILE,MAX_PATH-1-strlen(krbRealmConFile));
            krbRealmConFile[MAX_PATH-1] = '\0';
		}
		else
		  ASSERT(0);

		strncpy(confname, krbRealmConFile, szConfname);
        confname[szConfname-1] = '\0';
	}
////
#ifndef NO_KRB4
	else if (m_hKrb4DLL)
	{
        unsigned int size = szConfname;
        memset(confname, '\0', szConfname);
        if (!pkrb_get_krbrealm2(confname, &size))
		{
		    GetWindowsDirectory(confname,szConfname);
            confname[szConfname-1] = '\0';
			strncat(confname, "\\",szConfname-strlen(confname));
            confname[szConfname-1] = '\0';
			strncat(confname,KRBREALM_FILE,szConfname-strlen(confname));
            confname[szConfname-1] = '\0';
            return TRUE;
		}
	}
#endif

    return FALSE;
}

BOOL
CLeashApp::GetProfileFile(
    LPSTR confname,
    UINT szConfname
    )
{
    char **configFile = NULL;
    if (!m_hKrb5DLL)
        return NULL;

    if (pkrb5_get_default_config_files(&configFile))
    {
        GetWindowsDirectory(confname,szConfname);
        confname[szConfname-1] = '\0';
        strncat(confname,"\\KRB5.INI",szConfname-strlen(confname));
        confname[szConfname-1] = '\0';
        return FALSE;
    }

    *confname = 0;

    if (configFile)
    {
        strncpy(confname, *configFile, szConfname);
        confname[szConfname-1] = '\0';
        pkrb5_free_config_files(configFile);
    }

    if (!*confname)
    {
        GetWindowsDirectory(confname,szConfname);
        confname[szConfname-1] = '\0';
        strncat(confname,"\\KRB5.INI",szConfname-strlen(confname));
        confname[szConfname-1] = '\0';
    }

    return FALSE;
}

#define PROBE_USERNAME               "KERBEROS-KDC-PROBE"
#define PROBE_PASSWORD_LEN           16

BOOL
CLeashApp::ProbeKDC(void)
{
    krb5_context ctx=0;
    krb5_ccache  cc=0;
    krb5_principal principal = 0;
    krb5_principal probeprinc = 0;
    krb5_creds     creds;
    krb5_error_code code;
    krb5_data pwdata;
    char   password[PROBE_PASSWORD_LEN+1];
    long   success = FALSE;

    if (!pkrb5_init_context)
        return success;

    memset(&creds, 0, sizeof(creds));

    code = pkrb5_init_context(&ctx);
    if (code)
        goto cleanup;

    code = pkrb5_cc_default(ctx, &cc);
    if (code)
        goto cleanup;

    code = pkrb5_cc_get_principal(ctx, cc, &principal);
    if ( code )
        code = pkrb5_parse_name(ctx, "foo", &principal);
    if ( code )
        goto cleanup;

    code = pkrb5_build_principal( ctx, &probeprinc,
                                  krb5_princ_realm(ctx,principal)->length,
                                  krb5_princ_realm(ctx,principal)->data,
                                  PROBE_USERNAME, NULL, NULL);
    if ( code )
        goto cleanup;

    pwdata.data = password;
    pwdata.length = PROBE_PASSWORD_LEN;
    code = pkrb5_c_random_make_octets(ctx, &pwdata);
    if (code) {
        int i;
        for ( i=0 ; i<PROBE_PASSWORD_LEN ; i )
            password[i] = 'x';
    }
    password[PROBE_PASSWORD_LEN] = '\0';

    code = pkrb5_get_init_creds_password(ctx,
                                         &creds,
                                         probeprinc,
                                         password, // password
                                         NULL, // prompter
                                         0, // prompter data
                                         0, // start time
                                         0, // service name
                                         0  // no options
                                         );

    switch ( code ) {
    case KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN:
    case KRB5KDC_ERR_CLIENT_REVOKED:
    case KRB5KDC_ERR_CLIENT_NOTYET:
    case KRB5KDC_ERR_PREAUTH_FAILED:
    case KRB5KDC_ERR_PREAUTH_REQUIRED:
    case KRB5KDC_ERR_PADATA_TYPE_NOSUPP:
        success = TRUE;
        break;
    }
  cleanup:
    if (creds.client == probeprinc)
        creds.client = 0;
    pkrb5_free_cred_contents(ctx, &creds);
    if (principal)
        pkrb5_free_principal(ctx,principal);
    if (probeprinc)
        pkrb5_free_principal(ctx,probeprinc);
    if (cc)
        pkrb5_cc_close(ctx,cc);
    if (ctx)
        pkrb5_free_context(ctx);
    return success;
}

VOID
CLeashApp::ObtainTicketsViaUserIfNeeded(HWND hWnd)
{
    if (WaitForSingleObject( ticketinfo.lockObj, INFINITE ) != WAIT_OBJECT_0)
        throw("Unable to lock ticketinfo");
    LeashKRB5ListDefaultTickets(&ticketinfo.Krb5);
    int btickets = ticketinfo.Krb5.btickets;
    LeashKRB5FreeTicketInfo(&ticketinfo.Krb5);
    ReleaseMutex(ticketinfo.lockObj);

    if ( !btickets ) {
        if ( pLeash_importable() ) {
            if (pLeash_import())
                CLeashView::m_importedTickets = 1;
        }
        else if ( ProbeKDC() ) {
            LSH_DLGINFO_EX ldi;
            ldi.size = LSH_DLGINFO_EX_V1_SZ;
            ldi.dlgtype = DLGTYPE_PASSWD;
            ldi.title = "MIT Kerberos: Get Ticket";
            ldi.username = NULL;
            ldi.realm = NULL;
            ldi.dlgtype = DLGTYPE_PASSWD;
            ldi.use_defaults = 1;

            pLeash_kinit_dlg_ex(hWnd, &ldi);
        }
    } else {
        if ( CLeashView::m_importedTickets && pLeash_importable() ) {
            if (pLeash_import())
                CLeashView::m_importedTickets = 1;
        }
        else if ( ProbeKDC() && !pLeash_renew() ) {
            LSH_DLGINFO_EX ldi;
            ldi.size = LSH_DLGINFO_EX_V1_SZ;
            ldi.dlgtype = DLGTYPE_PASSWD;
            ldi.title = "MIT Kerberos: Get Ticket";
            ldi.username = NULL;
            ldi.realm = NULL;
            ldi.dlgtype = DLGTYPE_PASSWD;
            ldi.use_defaults = 1;

            pLeash_kinit_dlg_ex(hWnd, &ldi);
        }
    }
    return;
}

// IP Change Monitoring Functions
#include <Iphlpapi.h>


DWORD
CLeashApp::GetNumOfIpAddrs(void)
{
    PMIB_IPADDRTABLE pIpAddrTable = 0;
    ULONG            dwSize;
    DWORD            code;
    DWORD            index;
    DWORD            validAddrs = 0;

    dwSize = 0;
    code = GetIpAddrTable(NULL, &dwSize, 0);
    if (code == ERROR_INSUFFICIENT_BUFFER) {
        pIpAddrTable = (PMIB_IPADDRTABLE) malloc(dwSize);
        code = GetIpAddrTable(pIpAddrTable, &dwSize, 0);
        if ( code == NO_ERROR ) {
            for ( index=0; index < pIpAddrTable->dwNumEntries; index++ ) {
                if (pIpAddrTable->table[index].dwAddr != 0)
                    validAddrs++;
            }
        }
        free(pIpAddrTable);
    }
    return validAddrs;
}

UINT
CLeashApp::IpAddrChangeMonitor(void * hWnd)
{
    DWORD Result;
    DWORD prevNumOfAddrs = GetNumOfIpAddrs();
    DWORD NumOfAddrs;

    if ( !hWnd )
        return 0;

    while ( TRUE ) {
        Result = NotifyAddrChange(NULL,NULL);
        if ( Result != NO_ERROR ) {
            // We do not have permission to open the device
            return 0;
        }

        NumOfAddrs = GetNumOfIpAddrs();
        if ( NumOfAddrs != prevNumOfAddrs ) {
            // wait for the network state to stablize
            Sleep(2000);
            // this call should probably be mutex protected
            ObtainTicketsViaUserIfNeeded((HWND)hWnd);
        }
        prevNumOfAddrs = NumOfAddrs;
    }

    return 0;
}


DWORD
CLeashApp::IpAddrChangeMonitorInit(HWND hWnd)
{
    AfxBeginThread(IpAddrChangeMonitor, hWnd);
    return 0;
}

UINT
CLeashApp::InitWorker(void * hWnd)
{
    if ( ProbeKDC() ) {
        LSH_DLGINFO_EX ldi;
        ldi.size = LSH_DLGINFO_EX_V1_SZ;
        ldi.dlgtype = DLGTYPE_PASSWD;
        ldi.title = "Initialize Ticket";
        ldi.username = NULL;
        ldi.realm = NULL;
        ldi.use_defaults = 1;

        pLeash_kinit_dlg_ex((HWND)hWnd, &ldi);
        ::SendMessage((HWND)hWnd, WM_COMMAND, ID_UPDATE_DISPLAY, 0);
    }
    return 0;
}

#ifdef USE_HTMLHELP
#if _MSC_VER < 1300
void
CLeashApp::WinHelp(DWORD dwData, UINT nCmd)
{
	switch (nCmd)
	{
		case HELP_CONTEXT:
			::HtmlHelp(GetDesktopWindow(), m_helpFile, HH_HELP_CONTEXT, dwData );
			break;
		case HELP_FINDER:
			::HtmlHelp(GetDesktopWindow(), m_helpFile, HH_DISPLAY_TOPIC, 0);
            break;
	}
}
#endif
#endif


BOOL CLeashApp::OnIdle(LONG lCount)
{
    // TODO: Add your specialized code here and/or call the base class
    BOOL retval = CWinAppEx::OnIdle(lCount);
    if ((lCount == 0) && m_bUpdateDisplay) {
        m_bUpdateDisplay = FALSE;
        m_pMainWnd->SendMessage(WM_COMMAND, ID_UPDATE_DISPLAY, 0);
    }
    return retval;
}
