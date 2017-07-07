#ifdef __NMAKE__

# NMAKE portion.
# Build with : nmake /f custom.cpp
# Clean with : nmake /f custom.cpp clean

# Builds custom.dll

OUTPATH = .

# program name macros
CC = cl /nologo

LINK = link /nologo

RM = del

DLLFILE = $(OUTPATH)\custom.dll

DLLEXPORTS =\
    -EXPORT:EnableAllowTgtSessionKey \
    -EXPORT:RevertAllowTgtSessionKey \
    -EXPORT:AbortMsiImmediate \
    -EXPORT:UninstallNsisInstallation \
    -EXPORT:KillRunningProcesses \
    -EXPORT:ListRunningProcesses \
    -EXPORT:InstallNetProvider \
    -EXPORT:UninstallNetProvider

$(DLLFILE): $(OUTPATH)\custom.obj
    $(LINK) /OUT:$@ /DLL $** $(DLLEXPORTS)

$(OUTPATH)\custom.obj: custom.cpp custom.h
    $(CC) /c /Fo$@ custom.cpp

all: $(DLLFILE)

clean:
    $(RM) $(DLLFILE)
    $(RM) $(OUTPATH)\custom.obj
    $(RM) $(OUTPATH)\custom.exp

!IFDEF __C_TEXT__
#else
/*

Copyright 2004,2005 by the Massachusetts Institute of Technology

All rights reserved.

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the Massachusetts
Institute of Technology (M.I.T.) not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.

M.I.T. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/**************************************************************
* custom.cpp : Dll implementing custom action to install Kerberos for Windows
*
*         The functions in this file are for use as entry points
*         for calls from MSI only. The specific MSI parameters
*         are noted in the comments section of each of the
*         functions.
*
* rcsid: $Id$
**************************************************************/

#pragma unmanaged

// Only works for Win2k and above
#define _WIN32_WINNT 0x500
#include "custom.h"
#include <shellapi.h>

// linker stuff
#pragma comment(lib, "msi")
#pragma comment(lib, "advapi32")
#pragma comment(lib, "shell32")
#pragma comment(lib, "user32")

void ShowMsiError( MSIHANDLE hInstall, DWORD errcode, DWORD param ){
	MSIHANDLE hRecord;

	hRecord = MsiCreateRecord(3);
	MsiRecordClearData(hRecord);
	MsiRecordSetInteger(hRecord, 1, errcode);
	MsiRecordSetInteger(hRecord, 2, param);

	MsiProcessMessage( hInstall, INSTALLMESSAGE_ERROR, hRecord );
	
	MsiCloseHandle( hRecord );
}

static void ShowMsiErrorEx(MSIHANDLE hInstall, DWORD errcode, LPTSTR str,
                           DWORD param )
{
    MSIHANDLE hRecord;

    hRecord = MsiCreateRecord(3);
    MsiRecordClearData(hRecord);
    MsiRecordSetInteger(hRecord, 1, errcode);
    MsiRecordSetString(hRecord, 2, str);
    MsiRecordSetInteger(hRecord, 3, param);

    MsiProcessMessage(hInstall, INSTALLMESSAGE_ERROR, hRecord);

    MsiCloseHandle(hRecord);
}

#define LSA_KERBEROS_KEY "SYSTEM\\CurrentControlSet\\Control\\Lsa\\Kerberos"
#define LSA_KERBEROS_PARM_KEY "SYSTEM\\CurrentControlSet\\Control\\Lsa\\Kerberos\\Parameters"
#define KFW_CLIENT_KEY "SOFTWARE\\MIT\\Kerberos\\Client\\"
#define SESSKEY_VALUE_NAME "AllowTGTSessionKey"

#define SESSBACKUP_VALUE_NAME "AllowTGTSessionKeyBackup"
#define SESSXPBACKUP_VALUE_NAME "AllowTGTSessionKeyBackupXP"


/* Set the AllowTGTSessionKey registry keys on install.  Called as a deferred custom action. */
MSIDLLEXPORT EnableAllowTgtSessionKey( MSIHANDLE hInstall ) {
    return SetAllowTgtSessionKey( hInstall, TRUE );
}

/* Unset the AllowTGTSessionKey registry keys on uninstall.  Called as a deferred custom action. */
MSIDLLEXPORT RevertAllowTgtSessionKey( MSIHANDLE hInstall ) {
    return SetAllowTgtSessionKey( hInstall, FALSE );
}

UINT SetAllowTgtSessionKey( MSIHANDLE hInstall, BOOL pInstall ) {
    TCHAR tchVersionString[1024];
    TCHAR tchVersionKey[2048];
    DWORD size;
    DWORD type;
    DWORD value;
    HKEY hkKfwClient = NULL;
    HKEY hkLsaKerberos = NULL;
    HKEY hkLsaKerberosParm = NULL;
    UINT rv;
    DWORD phase = 0;

    // construct the backup key path
    size = sizeof(tchVersionString) / sizeof(TCHAR);
    rv = MsiGetProperty( hInstall, _T("CustomActionData"), tchVersionString, &size );
    if(rv != ERROR_SUCCESS) {
        if(pInstall) {
            ShowMsiError( hInstall, ERR_CUSTACTDATA, rv );
            return rv;
        } else {
            return ERROR_SUCCESS;
        }
    }

    _tcscpy( tchVersionKey, _T( KFW_CLIENT_KEY ) );
    _tcscat( tchVersionKey, tchVersionString );

    phase = 1;

    rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, tchVersionKey, 0, ((pInstall)?KEY_WRITE:KEY_READ), &hkKfwClient );
    if(rv != ERROR_SUCCESS)
        goto cleanup;

    phase = 2;

    rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, _T( LSA_KERBEROS_KEY ), 0, KEY_READ | KEY_WRITE, &hkLsaKerberos );
    if(rv != ERROR_SUCCESS) 
        goto cleanup;

    phase = 3;

    rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, _T( LSA_KERBEROS_PARM_KEY ), 0, KEY_READ | KEY_WRITE, &hkLsaKerberosParm );
    if(rv != ERROR_SUCCESS) {
        hkLsaKerberosParm = NULL;
    }

    if(pInstall) {
        // backup the existing values
        if(hkLsaKerberosParm) {
            phase = 4;

            size = sizeof(value);
            rv = RegQueryValueEx( hkLsaKerberosParm, _T( SESSKEY_VALUE_NAME ), NULL, &type, (LPBYTE) &value, &size );
            if(rv != ERROR_SUCCESS)
                value = 0;

            phase = 5;
            rv = RegSetValueEx( hkKfwClient, _T( SESSBACKUP_VALUE_NAME ), 0, REG_DWORD, (LPBYTE) &value, sizeof(value));
            if(rv != ERROR_SUCCESS)
                goto cleanup;
        }

        phase = 6;
        size = sizeof(value);
        rv = RegQueryValueEx( hkLsaKerberos, _T( SESSKEY_VALUE_NAME ), NULL, &type, (LPBYTE) &value, &size );
        if(rv != ERROR_SUCCESS)
            value = 0;

        phase = 7;
        rv = RegSetValueEx( hkKfwClient, _T( SESSXPBACKUP_VALUE_NAME ), 0, REG_DWORD, (LPBYTE) &value, sizeof(value));
        if(rv != ERROR_SUCCESS)
            goto cleanup;

        // and now write the actual values
        phase = 8;
        value = 1;
        rv = RegSetValueEx( hkLsaKerberos, _T( SESSKEY_VALUE_NAME ), 0, REG_DWORD, (LPBYTE) &value, sizeof(value));
        if(rv != ERROR_SUCCESS)
            goto cleanup;

        if(hkLsaKerberosParm) {
            phase = 9;
            value = 1;
            rv = RegSetValueEx( hkLsaKerberosParm, _T( SESSKEY_VALUE_NAME ), 0, REG_DWORD, (LPBYTE) &value, sizeof(value));
            if(rv != ERROR_SUCCESS)
                goto cleanup;
        }

    } else { // uninstalling
        // Don't fail no matter what goes wrong.  This is also a rollback action.
        if(hkLsaKerberosParm) {
            size = sizeof(value);
            rv = RegQueryValueEx( hkKfwClient, _T( SESSBACKUP_VALUE_NAME ), NULL, &type, (LPBYTE) &value, &size );
            if(rv != ERROR_SUCCESS)
                value = 0;

            RegSetValueEx( hkLsaKerberosParm, _T( SESSKEY_VALUE_NAME ), 0, REG_DWORD, (LPBYTE) &value, sizeof(value));
        }

        size = sizeof(value);
        rv = RegQueryValueEx( hkKfwClient, _T( SESSXPBACKUP_VALUE_NAME ), NULL, &type, (LPBYTE) &value, &size );
        if(rv != ERROR_SUCCESS)
            value = 0;

        RegSetValueEx( hkLsaKerberos, _T( SESSKEY_VALUE_NAME ), 0, REG_DWORD, (LPBYTE) &value, sizeof(value));

        RegDeleteValue( hkKfwClient, _T( SESSXPBACKUP_VALUE_NAME ) );
        RegDeleteValue( hkKfwClient, _T( SESSBACKUP_VALUE_NAME ) );
    }

    // all done
    rv = ERROR_SUCCESS;

cleanup:
    if(rv != ERROR_SUCCESS && pInstall) {
        ShowMsiError(hInstall, 4005, phase);
    }
    if(hkKfwClient) RegCloseKey( hkKfwClient );
    if(hkLsaKerberos) RegCloseKey( hkLsaKerberos );
    if(hkLsaKerberosParm) RegCloseKey( hkLsaKerberosParm );

    return rv;
}

/* Abort the installation (called as an immediate custom action) */
MSIDLLEXPORT AbortMsiImmediate( MSIHANDLE hInstall ) {
    DWORD rv;
	DWORD dwSize = 0;
	LPTSTR sReason = NULL;
	LPTSTR sFormatted = NULL;
	MSIHANDLE hRecord = NULL;
	LPTSTR cAbortReason = _T("ABORTREASON");

	rv = MsiGetProperty( hInstall, cAbortReason, _T(""), &dwSize );
	if(rv != ERROR_MORE_DATA) goto _cleanup;

	sReason = new TCHAR[ ++dwSize ];

	rv = MsiGetProperty( hInstall, cAbortReason, sReason, &dwSize );

	if(rv != ERROR_SUCCESS) goto _cleanup;

    hRecord = MsiCreateRecord(3);
	MsiRecordClearData(hRecord);
	MsiRecordSetString(hRecord, 0, sReason);

	dwSize = 0;

	rv = MsiFormatRecord(hInstall, hRecord, "", &dwSize);
	if(rv != ERROR_MORE_DATA) goto _cleanup;

	sFormatted = new TCHAR[ ++dwSize ];

	rv = MsiFormatRecord(hInstall, hRecord, sFormatted, &dwSize);

	if(rv != ERROR_SUCCESS) goto _cleanup;

	MsiCloseHandle(hRecord);

	hRecord = MsiCreateRecord(3);
	MsiRecordClearData(hRecord);
	MsiRecordSetInteger(hRecord, 1, ERR_ABORT);
	MsiRecordSetString(hRecord,2, sFormatted);
	MsiProcessMessage(hInstall, INSTALLMESSAGE_ERROR, hRecord);

_cleanup:
	if(sFormatted) delete sFormatted;
	if(hRecord) MsiCloseHandle( hRecord );
	if(sReason) delete sReason;

	return ~ERROR_SUCCESS;
}

/* Kill specified processes that are running on the system */
/* Uses the custom table KillProcess.  Called as an immediate action. */

#define MAX_KILL_PROCESSES 255
#define FIELD_SIZE 256

struct _KillProc {
    TCHAR * image;
    TCHAR * desc;
    BOOL    found;
    DWORD   pid;
};

#define RV_BAIL if(rv != ERROR_SUCCESS) goto _cleanup

MSIDLLEXPORT KillRunningProcesses( MSIHANDLE hInstall ) {
    return KillRunningProcessesSlave( hInstall, TRUE );
}

/* When listing running processes, we populate the ListBox table with
   values associated with the property 'KillableProcesses'.  If we
   actually find any processes worth killing, then we also set the
   'FoundProcceses' property to '1'.  Otherwise we set it to ''.
*/

MSIDLLEXPORT ListRunningProcesses( MSIHANDLE hInstall ) {
    return KillRunningProcessesSlave( hInstall, FALSE );
}

UINT KillRunningProcessesSlave( MSIHANDLE hInstall, BOOL bKill )
{
    UINT rv = ERROR_SUCCESS;
    _KillProc * kpList;
    int nKpList = 0;
    int i;
    int rowNum = 1;
    DWORD size;
    BOOL found = FALSE;

    MSIHANDLE hDatabase = NULL;
    MSIHANDLE hView = NULL;
    MSIHANDLE hViewInsert = NULL;
    MSIHANDLE hRecord = NULL;
    MSIHANDLE hRecordInsert = NULL;

    HANDLE hSnapshot = NULL;

    PROCESSENTRY32 pe;

    kpList = new _KillProc[MAX_KILL_PROCESSES];
    memset(kpList, 0, sizeof(*kpList) * MAX_KILL_PROCESSES);

    hDatabase = MsiGetActiveDatabase( hInstall );
    if( hDatabase == NULL ) {
        rv = GetLastError();
        goto _cleanup;
    }

    // If we are only going to list out the processes, delete all the existing
    // entries first.

    if(!bKill) {

        rv = MsiDatabaseOpenView( hDatabase,
            _T( "DELETE FROM `ListBox` WHERE `ListBox`.`Property` = 'KillableProcesses'" ),
            &hView); RV_BAIL;

        rv = MsiViewExecute( hView, NULL ); RV_BAIL;

        MsiCloseHandle( hView );

        hView = NULL;
        
        rv = MsiDatabaseOpenView( hDatabase,
              _T( "SELECT * FROM `ListBox` WHERE `Property` = 'KillableProcesses'" ),
            &hViewInsert); RV_BAIL;

        MsiViewExecute(hViewInsert, NULL);

        hRecordInsert = MsiCreateRecord(4);

        if(hRecordInsert == NULL) {
            rv = GetLastError();
            goto _cleanup;
        }
    }

    // Open a view
    rv = MsiDatabaseOpenView( hDatabase, 
        _T( "SELECT `Image`,`Desc` FROM `KillProcess`" ),
        &hView); RV_BAIL;

    rv = MsiViewExecute( hView, NULL ); RV_BAIL;

    do {
        rv = MsiViewFetch( hView, &hRecord );
        if(rv != ERROR_SUCCESS) {
            if(hRecord) 
                MsiCloseHandle(hRecord);
            hRecord = NULL;
            break;
        }

        kpList[nKpList].image = new TCHAR[ FIELD_SIZE ]; kpList[nKpList].image[0] = _T('\0');
        kpList[nKpList].desc = new TCHAR[ FIELD_SIZE ];  kpList[nKpList].desc[0] = _T('\0');
        nKpList++;

        size = FIELD_SIZE;
        rv = MsiRecordGetString(hRecord, 1, kpList[nKpList-1].image, &size); RV_BAIL;

        size = FIELD_SIZE;
        rv = MsiRecordGetString(hRecord, 2, kpList[nKpList-1].desc, &size); RV_BAIL;

        MsiCloseHandle(hRecord);
    } while(nKpList < MAX_KILL_PROCESSES);

    hRecord = NULL;

    // now we have all the processes in the array.  Check if they are running.
    
    hSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if(hSnapshot == INVALID_HANDLE_VALUE) {
        rv = GetLastError();
        goto _cleanup;
    }

    pe.dwSize = sizeof( PROCESSENTRY32 );

    if(!Process32First( hSnapshot, &pe )) {
        // technically we should at least find the MSI process, but we let this pass
        rv = ERROR_SUCCESS;
        goto _cleanup;
    }

    do {
        for(i=0; i<nKpList; i++) {
            if(!_tcsicmp( kpList[i].image, pe.szExeFile )) {
                // got one
                if(bKill) {
                    // try to kill the process
                    HANDLE hProcess = NULL;

                    // If we encounter an error, instead of bailing
                    // out, we continue on to the next process.  We
                    // may not have permission to kill all the
                    // processes we want to kill anyway.  If there are
                    // any files that we want to replace that is in
                    // use, Windows Installer will schedule a reboot.
                    // Also, it's not like we have an exhaustive list
                    // of all the programs that use Kerberos anyway.

                    hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if(hProcess == NULL) {
                        rv = GetLastError();
                        break;
                    }

                    if(!TerminateProcess(hProcess, 0)) {
                        rv = GetLastError();
                        CloseHandle(hProcess);
                        break;
                    }

                    CloseHandle(hProcess);

                } else {
                    TCHAR buf[256];

                    // we are supposed to just list out the processes
                    rv = MsiRecordClearData( hRecordInsert ); RV_BAIL;
                    rv = MsiRecordSetString( hRecordInsert, 1, _T("KillableProcesses"));
                    rv = MsiRecordSetInteger( hRecordInsert, 2, rowNum++ ); RV_BAIL;
                    _itot( rowNum, buf, 10 );
                    rv = MsiRecordSetString( hRecordInsert, 3, buf ); RV_BAIL;
                    if(_tcslen(kpList[i].desc)) {
                        rv = MsiRecordSetString( hRecordInsert, 4, kpList[i].desc ); RV_BAIL;
                    } else {
                        rv = MsiRecordSetString( hRecordInsert, 4, kpList[i].image ); RV_BAIL;
                    }
                    MsiViewModify(hViewInsert, MSIMODIFY_INSERT_TEMPORARY, hRecordInsert); RV_BAIL;

                    found = TRUE;
                }
                break;
            }
        }
   } while( Process32Next( hSnapshot, &pe ) );

    if(!bKill) {
        // set the 'FoundProcceses' property
        if(found) {
            MsiSetProperty( hInstall, _T("FoundProcesses"), _T("1"));
        } else {
            MsiSetProperty( hInstall, _T("FoundProcesses"), _T(""));
        }
    }

    // Finally:
    rv = ERROR_SUCCESS;

_cleanup:

    if(hRecordInsert) MsiCloseHandle(hRecordInsert);
    if(hViewInsert) MsiCloseHandle(hView);

    if(hSnapshot && hSnapshot != INVALID_HANDLE_VALUE) CloseHandle(hSnapshot);

    while(nKpList) {
        nKpList--;
        delete kpList[nKpList].image;
        delete kpList[nKpList].desc;
    }
    delete kpList;

    if(hRecord) MsiCloseHandle(hRecord);
    if(hView) MsiCloseHandle(hView);

    if(hDatabase) MsiCloseHandle(hDatabase);

    if(rv != ERROR_SUCCESS) {
        ShowMsiError(hInstall, ERR_PROC_LIST, rv);
    }

    return rv;
}

static bool IsNSISInstalled()
{
    HKEY nsisKfwKey = NULL;
    // Note: check Wow6432 node if 64 bit build
    HRESULT res = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                               "SOFTWARE\\Microsoft\\Windows\\CurrentVersion"
                               "\\Uninstall\\Kerberos for Windows",
                               0,
                               KEY_READ | KEY_WOW64_32KEY,
                               &nsisKfwKey);
    if (res != ERROR_SUCCESS)
        return FALSE;

    RegCloseKey(nsisKfwKey);
    return TRUE;
}

static HANDLE NSISUninstallShellExecute(LPTSTR pathUninstall)
{
    SHELLEXECUTEINFO   sei;
    ZeroMemory ( &sei, sizeof(sei) );

    sei.cbSize          = sizeof(sei);
    sei.hwnd            = GetForegroundWindow();
    sei.fMask           = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI |
                          SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb          = _T("runas"); // run as administrator
    sei.lpFile          = pathUninstall;
    sei.lpParameters    = _T("");
    sei.nShow           = SW_SHOWNORMAL;

    if (!ShellExecuteEx(&sei)) {
        // FAILED! TODO: report details?
    }
    return sei.hProcess;
}

static HANDLE NSISUninstallCreateProcess(LPTSTR pathUninstall)
{
    STARTUPINFO sInfo;
    PROCESS_INFORMATION pInfo;
    pInfo.hProcess = NULL;
    pInfo.hThread = NULL;

    // Create a process for the uninstaller
    sInfo.cb = sizeof(sInfo);
    sInfo.lpReserved = NULL;
    sInfo.lpDesktop = _T("");
    sInfo.lpTitle = _T("NSIS Uninstaller for Kerberos for Windows");
    sInfo.dwX = 0;
    sInfo.dwY = 0;
    sInfo.dwXSize = 0;
    sInfo.dwYSize = 0;
    sInfo.dwXCountChars = 0;
    sInfo.dwYCountChars = 0;
    sInfo.dwFillAttribute = 0;
    sInfo.dwFlags = 0;
    sInfo.wShowWindow = 0;
    sInfo.cbReserved2 = 0;
    sInfo.lpReserved2 = 0;
    sInfo.hStdInput = 0;
    sInfo.hStdOutput = 0;
    sInfo.hStdError = 0;

    if (!CreateProcess(pathUninstall,
                       _T("Uninstall /S"),
                       NULL,
                       NULL,
                       FALSE,
                       CREATE_SUSPENDED,
                       NULL,
                       NULL,
                       &sInfo,
                       &pInfo)) {
        // failure; could grab info, but we should be able to recover by
        // using NSISUninstallShellExecute...
    } else {
        // success
        // start up the thread
        ResumeThread(pInfo.hThread);
        // done with thread handle
        CloseHandle(pInfo.hThread);
    }
    return pInfo.hProcess;
}


/* Uninstall NSIS */
MSIDLLEXPORT UninstallNsisInstallation( MSIHANDLE hInstall )
{
    DWORD rv = ERROR_SUCCESS;
    DWORD lastError;
    // lookup the NSISUNINSTALL property value
    LPTSTR cNsisUninstall = _T("UPGRADENSIS");
    LPTSTR strPathUninst = NULL;
    DWORD dwSize = 0;
    HANDLE hProcess = NULL;
    HANDLE hIo = NULL;
    HANDLE hJob = NULL;

    rv = MsiGetProperty( hInstall, cNsisUninstall, _T(""), &dwSize );
    if(rv != ERROR_MORE_DATA) goto _cleanup;

    strPathUninst = new TCHAR[ ++dwSize ];

    rv = MsiGetProperty(hInstall, cNsisUninstall, strPathUninst, &dwSize);
    if(rv != ERROR_SUCCESS) goto _cleanup;

    hProcess = NSISUninstallCreateProcess(strPathUninst);
    if (hProcess == NULL) // expected when run on UAC-limited account
        hProcess = NSISUninstallShellExecute(strPathUninst);

    if (hProcess == NULL) {
        // still no uninstall process? ick...
        lastError = GetLastError();
        rv = 40;
        goto _cleanup;
    }
    // note that it is not suffiecient to wait for the initial process to
    // finish; there is a whole process tree that we need to wait for.  sigh.
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT acp;
    acp.CompletionKey = 0;
    hJob = CreateJobObject(NULL, _T("NSISUninstallObject"));
    if(!hJob) {
        rv = 41;
        goto _cleanup;
    }

    hIo = CreateIoCompletionPort(INVALID_HANDLE_VALUE,0,0,0);
    if(!hIo) {
        rv = 42;
        goto _cleanup;
    }

    acp.CompletionPort = hIo;

    SetInformationJobObject(hJob,
                            JobObjectAssociateCompletionPortInformation,
                            &acp,
                            sizeof(acp));

    AssignProcessToJobObject(hJob, hProcess);

    DWORD msgId;
    ULONG_PTR unusedCompletionKey;
    LPOVERLAPPED unusedOverlapped;
    for (;;) {
        if (!GetQueuedCompletionStatus(hIo,
                                       &msgId,
                                       &unusedCompletionKey,
                                       &unusedOverlapped,
                                       INFINITE)) {
            Sleep(1000);
        } else if (msgId == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
            break;
        }
    }

_cleanup:
    if (hProcess) CloseHandle(hProcess);
    if (hIo) CloseHandle(hIo);
    if (hJob) CloseHandle(hJob);

    if (IsNSISInstalled()) {
        // uninstall failed: maybe user cancelled uninstall, or something else
        // went wrong...
        if (rv == ERROR_SUCCESS)
            rv = 43;
    } else {
        // Maybe something went wrong, but it doesn't matter as long as nsis
        // is gone now...
        rv = ERROR_SUCCESS;
    }

    if (rv == 40) {
        // CreateProcess() / ShellExecute() errors get extra data
        ShowMsiErrorEx(hInstall, ERR_NSS_FAILED_CP, strPathUninst, lastError);
    } else if (rv != ERROR_SUCCESS) {
        ShowMsiError(hInstall, ERR_NSS_FAILED, rv);
    }

    if (strPathUninst) delete strPathUninst;
    return rv;
}

/* Check and add or remove networkprovider key value
        str : target string
        str2: string to add/remove
        bInst: == 1 if string should be added to target if not already there, 
	otherwise remove string from target if present.
*/
int npi_CheckAndAddRemove( LPTSTR str, LPTSTR str2, int bInst ) {

    LPTSTR target, charset, match;
    int ret=0;

    target = new TCHAR[lstrlen(str)+3];
    lstrcpy(target,_T(","));
    lstrcat(target,str);
    lstrcat(target,_T(","));
    charset = new TCHAR[lstrlen(str2)+3];
    lstrcpy(charset,_T(","));
    lstrcat(charset,str2);
    lstrcat(charset,_T(","));

    match = _tcsstr(target, charset);

    if ((match) && (bInst)) {
        ret = INP_ERR_PRESENT;
        goto cleanup;
    }

    if ((!match) && (!bInst)) {
        ret = INP_ERR_ABSENT;
        goto cleanup;
    }

    if (bInst) // && !match
    {
       lstrcat(str, _T(","));
       lstrcat(str, str2);
       ret = INP_ERR_ADDED;
       goto cleanup;
    }

    // if (!bInst) && (match)
    {
       lstrcpy(str+(match-target),match+lstrlen(str2)+2);
       str[lstrlen(str)-1]=_T('\0');
       ret = INP_ERR_REMOVED;
       goto cleanup;
    }

cleanup:

    delete target;
    delete charset;
    return ret;
}

/* Sets the registry keys required for the functioning of the network provider */

DWORD InstNetProvider(MSIHANDLE hInstall, int bInst) {
    LPTSTR strOrder;
    HKEY hkOrder;
    LONG rv;
    DWORD dwSize;
    HANDLE hProcHeap;

    strOrder = (LPTSTR) 0;

    CHECK(rv = RegOpenKeyEx( HKEY_LOCAL_MACHINE, STR_KEY_ORDER, 0, KEY_READ | KEY_WRITE, &hkOrder ));

    dwSize = 0;
    CHECK(rv = RegQueryValueEx( hkOrder, STR_VAL_ORDER, NULL, NULL, NULL, &dwSize ) );

    strOrder = new TCHAR[ (dwSize + STR_SERVICE_LEN + 4) * sizeof(TCHAR) ];

    CHECK(rv = RegQueryValueEx( hkOrder, STR_VAL_ORDER, NULL, NULL, (LPBYTE) strOrder, &dwSize));

    strOrder[dwSize] = '\0';	/* reg strings are not always nul terminated */

    npi_CheckAndAddRemove( strOrder, STR_SERVICE , bInst);

    dwSize = (lstrlen( strOrder ) + 1) * sizeof(TCHAR);

    CHECK(rv = RegSetValueEx( hkOrder, STR_VAL_ORDER, NULL, REG_SZ, (LPBYTE) strOrder, dwSize ));

    /* everything else should be set by the MSI tables */
    rv = ERROR_SUCCESS;
_cleanup:

    if( rv != ERROR_SUCCESS ) {
        ShowMsiError( hInstall, ERR_NPI_FAILED, rv );
    }

    if(strOrder) delete strOrder;

    return rv;
}

MSIDLLEXPORT InstallNetProvider( MSIHANDLE hInstall ) {
    return InstNetProvider( hInstall, 1 );
}

MSIDLLEXPORT UninstallNetProvider( MSIHANDLE hInstall) {
    return InstNetProvider( hInstall, 0 );
}

#endif
#ifdef __NMAKE__
!ENDIF
#endif
