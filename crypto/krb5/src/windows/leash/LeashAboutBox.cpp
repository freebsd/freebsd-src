//*****************************************************************************
// File:	LeashAboutBox.cpp
// By:		Arthur David Leather
// Created:	12/02/98
// Copyright:	@1998 Massachusetts Institute of Technology - All rights
//              reserved.
// Description:	CPP file for LeashAboutBox.h. Contains variables and functions
//		for the Leash About Box Dialog Box
//
// History:
//
// MM/DD/YY	Inits	Description of Change
// 12/02/98	ADL	Original
//*****************************************************************************


#include "stdafx.h"
#include "leash.h"
#include "LeashAboutBox.h"
#include "reminder.h"
#include "lglobals.h"
#include "psapi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CLeashAboutBox dialog


CLeashAboutBox::CLeashAboutBox(CWnd* pParent /*=NULL*/)
	: CDialog(CLeashAboutBox::IDD, pParent)
        , m_bListModules(FALSE)
{
    m_missingFileError = FALSE;

    //{{AFX_DATA_INIT(CLeashAboutBox)
    m_fileItem = _T("");
    //}}AFX_DATA_INIT
}


void CLeashAboutBox::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CLeashAboutBox)
    DDX_Control(pDX, IDC_PROPERTIES, m_propertiesButton);
    DDX_Control(pDX, IDC_LEASH_MODULES, m_radio_LeashDLLs);
    DDX_Control(pDX, IDC_LEASH_MODULE_LB, m_LB_DLLsLoaded);
    DDX_LBString(pDX, IDC_LEASH_MODULE_LB, m_fileItem);
    //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CLeashAboutBox, CDialog)
    //{{AFX_MSG_MAP(CLeashAboutBox)
    ON_WM_HSCROLL()
    ON_LBN_SELCHANGE(IDC_LEASH_MODULE_LB, OnSelchangeLeashModuleLb)
    ON_BN_CLICKED(IDC_ALL_MODULES, OnAllModules)
    ON_BN_CLICKED(IDC_LEASH_MODULES, OnLeashModules)
    ON_LBN_DBLCLK(IDC_LEASH_MODULE_LB, OnDblclkLeashModuleLb)
    ON_BN_CLICKED(IDC_PROPERTIES, OnProperties)
    ON_LBN_SETFOCUS(IDC_LEASH_MODULE_LB, OnSetfocusLeashModuleLb)
    ON_BN_CLICKED(IDC_NOT_LOADED_MODULES, OnNotLoadedModules)
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()
;
/////////////////////////////////////////////////////////////////////////////
// CLeashAboutBox message handlers

void CLeashAboutBox::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL CLeashAboutBox::GetModules95(DWORD processID, BOOL allModules)
{
    char szModNames[1024];
    MODULEENTRY32 me32 = {0};
    HANDLE hProcessSnap = NULL;

    hProcessSnap = pCreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
    if (hProcessSnap == (HANDLE)-1)
        return FALSE;

    me32.dwSize = sizeof(MODULEENTRY32);
    if (pModule32First(hProcessSnap, &me32))
    {
        do
        {
            lstrcpy(szModNames, me32.szExePath);
            strupr(szModNames);

            if (!allModules)
            {
                if (!strstr(szModNames, "SYSTEM"))
                    m_LB_DLLsLoaded.AddString(me32.szExePath);
            }
            else
                m_LB_DLLsLoaded.AddString(me32.szExePath);
        }
        while (pModule32Next(hProcessSnap, &me32));
    }

    return TRUE;
}

void CLeashAboutBox::GetModulesNT(DWORD processID, BOOL allModules)
{
    char checkName[1024];
    HMODULE hMods[1024];
    HANDLE hProcess;
    DWORD cbNeeded;
    unsigned int i;

    // Get a list of all the modules in this process.
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                           FALSE, processID);

    if (pEnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
        {
            char szModName[2048];

            // Get the full path to the module's file.
            if (pGetModuleFileNameEx(hProcess, hMods[i], szModName,
                                     sizeof(szModName)))
            {
                lstrcpy(checkName, szModName);
                strupr(checkName);

                if (!allModules)
                {
                    if (!strstr(checkName, "SYSTEM32"))
                        m_LB_DLLsLoaded.AddString(szModName);
                }
                else
                    m_LB_DLLsLoaded.AddString(szModName);
            }
        }
    }

    CloseHandle(hProcess);
}

void CLeashAboutBox::HighlightFirstItem()
{
    UINT numModules = m_LB_DLLsLoaded.GetCount();
    CHAR numModulesBuffer [25];
    _itoa(numModules, numModulesBuffer, 10);

    if (numModules)
    {
        m_LB_DLLsLoaded.SetCurSel(0);
        m_propertiesButton.EnableWindow();
    }
    else
        m_propertiesButton.EnableWindow(FALSE);

    GetDlgItem(IDC_STATIC_NO_OF_MODULES)->SetWindowText(numModulesBuffer);
}

DWORD
CLeashAboutBox::SetVersionInfo(
    UINT id_version,
    UINT id_copyright
    )
{
    TCHAR filename[1024];
    DWORD dwVersionHandle;
    LPVOID pVersionInfo = 0;
    DWORD retval = 0;
    LPDWORD pLangInfo = 0;
    LPTSTR szVersion = 0;
    LPTSTR szCopyright = 0;
    UINT len = 0;
    TCHAR sname_version[] = TEXT("FileVersion");
    TCHAR sname_copyright[] = TEXT("LegalCopyright");
    TCHAR szVerQ[(sizeof("\\StringFileInfo\\12345678\\") +
                  max(sizeof(sname_version) / sizeof(TCHAR),
                      sizeof(sname_copyright) / sizeof(TCHAR)))];
    TCHAR * cp = szVerQ;

    if (!GetModuleFileName(NULL, filename, sizeof(filename)))
        return GetLastError();

    DWORD size = GetFileVersionInfoSize(filename, &dwVersionHandle);

    if (!size)
        return GetLastError();

    pVersionInfo = malloc(size);
    if (!pVersionInfo)
        return ERROR_NOT_ENOUGH_MEMORY;

    if (!GetFileVersionInfo(filename, dwVersionHandle, size, pVersionInfo))
    {
        retval = GetLastError();
        goto cleanup;
    }

    if (!VerQueryValue(pVersionInfo, TEXT("\\VarFileInfo\\Translation"),
                       (LPVOID*)&pLangInfo, &len))
    {
        retval = GetLastError();
        goto cleanup;
    }


    cp += wsprintf(szVerQ,
                   TEXT("\\StringFileInfo\\%04x%04x\\"),
                   LOWORD(*pLangInfo), HIWORD(*pLangInfo));

    lstrcpy(cp, sname_version);
    if (!VerQueryValue(pVersionInfo, szVerQ, (LPVOID*)&szVersion, &len))
    {
        retval = GetLastError() || ERROR_NOT_ENOUGH_MEMORY;
        goto cleanup;
    }
    TCHAR version[100];
    _sntprintf(version, sizeof(version), TEXT("MIT Kerberos Version %s"), szVersion);
    version[sizeof(version) - 1] = 0;
    GetDlgItem(id_version)->SetWindowText(version);

    lstrcpy(cp, sname_copyright);
    if (!VerQueryValue(pVersionInfo, szVerQ, (LPVOID*)&szCopyright, &len))
    {
        retval = GetLastError() || ERROR_NOT_ENOUGH_MEMORY;
        goto cleanup;
    }
    GetDlgItem(id_copyright)->SetWindowText(szCopyright);

 cleanup:
    if (pVersionInfo)
        free(pVersionInfo);
    return retval;
}

BOOL CLeashAboutBox::OnInitDialog()
{
    CDialog::OnInitDialog();

    // XXX - we need to add some sensible behavior on error.
    // We need to get the version info and display it...
    SetVersionInfo(IDC_ABOUT_VERSION, IDC_ABOUT_COPYRIGHT);

    if (!CLeashApp::m_hToolHelp32 && !CLeashApp::m_hPsapi)
        m_missingFileError = TRUE;

    if (m_bListModules) {
        m_radio_LeashDLLs.SetCheck(TRUE);
        OnLeashModules();

        HighlightFirstItem();

        if (!CLeashApp::m_hPsapi)
            GetDlgItem(IDC_PROPERTIES)->EnableWindow(FALSE);
    } else {
        m_radio_LeashDLLs.ShowWindow(SW_HIDE);
        GetDlgItem(IDC_NOT_LOADED_MODULES)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_ALL_MODULES)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_PROPERTIES)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_STATIC_MODULES_LOADED)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_STATIC_NO_OF_MODULES)->ShowWindow(SW_HIDE);
        m_LB_DLLsLoaded.ShowWindow(SW_HIDE);
        // shrink window, move 'OK' button
        const int hideDiff = 150;
        RECT okRect;
        CWnd* pOK = GetDlgItem(IDOK);
        pOK->GetWindowRect(&okRect);
        ScreenToClient(&okRect);
        pOK->SetWindowPos(0, okRect.left, okRect.top - hideDiff,
                          0, 0, SWP_NOZORDER | SWP_NOSIZE);
        RECT dlgRect;
        GetWindowRect( &dlgRect );

        SetWindowPos(0,0,0,
                     dlgRect.right-dlgRect.left,
                     dlgRect.bottom-dlgRect.top - hideDiff,
                     SWP_NOZORDER|SWP_NOMOVE);
    }
    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

void CLeashAboutBox::OnSelchangeLeashModuleLb()
{
}

void CLeashAboutBox::OnAllModules()
{
    if (!CLeashApp::m_hToolHelp32 && !CLeashApp::m_hPsapi)
        return; //error

    m_LB_DLLsLoaded.ResetContent();

    if (!CLeashApp::m_hPsapi)
        GetModules95(GetCurrentProcessId());
    //m_LB_DLLsLoaded.AddString("Doesn't work in Windows 95");
    else
        GetModulesNT(GetCurrentProcessId());

    HighlightFirstItem();
}

void CLeashAboutBox::OnLeashModules()
{
    if (!CLeashApp::m_hToolHelp32 && !CLeashApp::m_hPsapi)
        return; // error

    m_LB_DLLsLoaded.ResetContent();

    if (!CLeashApp::m_hPsapi)
        GetModules95(GetCurrentProcessId(), FALSE);
    //m_LB_DLLsLoaded.AddString("Doesn't work in Windows 95");
    else
        GetModulesNT(GetCurrentProcessId(), FALSE);

    HighlightFirstItem();
}

void CLeashAboutBox::OnNotLoadedModules()
{
    m_LB_DLLsLoaded.ResetContent();

    if (!CLeashApp::m_hKrb5DLL)
        m_LB_DLLsLoaded.AddString(KERB5DLL);

    HighlightFirstItem();
}

void CLeashAboutBox::OnDblclkLeashModuleLb()
{
    m_LB_DLLsLoaded.GetText(m_LB_DLLsLoaded.GetCurSel(), m_fileItem);

    SHELLEXECUTEINFO sei;
    ZeroMemory(&sei,sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.lpFile = m_fileItem;
    sei.lpVerb = "properties";
    sei.fMask  = SEE_MASK_INVOKEIDLIST;

    if (!ShellExecuteEx(&sei))
    {
        MessageBox("Can't find selected file or Properties dialog", "Error",
                   MB_OK);
    }
}

void CLeashAboutBox::OnProperties()
{
    OnDblclkLeashModuleLb();
}

void CLeashAboutBox::OnSetfocusLeashModuleLb()
{
    if (m_LB_DLLsLoaded.GetCount())
        m_propertiesButton.EnableWindow(TRUE);
}

BOOL CLeashAboutBox::PreTranslateMessage(MSG* pMsg)
{
    if (m_missingFileError)
    {
        ::MessageBox(NULL, "OnInitDialog::We can't find file\"PSAPI.DLL\" "
                     "or \"KERNEL32.DLL\"!!!\n"
                     "About Box will not work properly.",
                     "Error", MB_OK);

        m_missingFileError = FALSE;
    }
    return CDialog::PreTranslateMessage(pMsg);
}
