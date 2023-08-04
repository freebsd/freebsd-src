/*
Copyright 2005,2006 by the Massachusetts Institute of Technology
Copyright 2007 by Secure Endpoints Inc.

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

#include "kfwlogon.h"

#include <io.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <winsock2.h>
#include <lm.h>
#include <nb30.h>

static HANDLE hDLL;

static HANDLE hInitMutex = NULL;
static BOOL bInit = FALSE;


BOOLEAN APIENTRY DllEntryPoint(HANDLE dll, DWORD reason, PVOID reserved)
{
    hDLL = dll;
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        /* Initialization Mutex */
        hInitMutex = CreateMutex(NULL, FALSE, NULL);
        break;

    case DLL_PROCESS_DETACH:
        CloseHandle(hInitMutex);
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    default:
        /* Everything else succeeds but does nothing. */
        break;
    }

    return TRUE;
}

DWORD APIENTRY NPGetCaps(DWORD index)
{
    switch (index) {
    case WNNC_NET_TYPE:
        /* We aren't a file system; We don't have our own type; use somebody else's. */
        return WNNC_NET_SUN_PC_NFS;
    case WNNC_START:
        /* Say we are already started, even though we might wait after we receive NPLogonNotify */
        return 1;

    default:
        return 0;
    }
}


static BOOL
WINAPI
UnicodeStringToANSI(UNICODE_STRING uInputString, LPSTR lpszOutputString, int nOutStringLen)
{
    CPINFO CodePageInfo;

    GetCPInfo(CP_ACP, &CodePageInfo);

    if (CodePageInfo.MaxCharSize > 1)
        // Only supporting non-Unicode strings
        return FALSE;

    if (uInputString.Buffer && ((LPBYTE) uInputString.Buffer)[1] == '\0')
    {
        // Looks like unicode, better translate it
        // UNICODE_STRING specifies the length of the buffer string in Bytes not WCHARS
        WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) uInputString.Buffer, uInputString.Length/2,
                            lpszOutputString, nOutStringLen-1, NULL, NULL);
        lpszOutputString[min(uInputString.Length/2,nOutStringLen-1)] = '\0';
        return TRUE;
    }

    lpszOutputString[0] = '\0';
    return FALSE;
}  // UnicodeStringToANSI


static BOOL
is_windows_vista(void)
{
   static BOOL fChecked = FALSE;
   static BOOL fIsWinVista = FALSE;

   if (!fChecked)
   {
       OSVERSIONINFO Version;

       memset (&Version, 0x00, sizeof(Version));
       Version.dwOSVersionInfoSize = sizeof(Version);

       if (GetVersionEx (&Version))
       {
           if (Version.dwPlatformId == VER_PLATFORM_WIN32_NT &&
               Version.dwMajorVersion >= 6)
               fIsWinVista = TRUE;
       }
       fChecked = TRUE;
   }

   return fIsWinVista;
}


/* Construct a Logon Script that will cause the LogonEventHandler to be executed
 * under in the logon session
 */

#define RUNDLL32_CMDLINE "rundll32.exe kfwlogon.dll,LogonEventHandler "
VOID
ConfigureLogonScript(LPWSTR *lpLogonScript, char * filename) {
    DWORD dwLogonScriptLen;
    LPWSTR lpScript;
    LPSTR lpTemp;

    if (!lpLogonScript)
	return;
    *lpLogonScript = NULL;

    if (!filename)
	return;

    dwLogonScriptLen = strlen(RUNDLL32_CMDLINE) + strlen(filename) + 2;
    lpTemp = (LPSTR) malloc(dwLogonScriptLen);
    if (!lpTemp)
	return;

    _snprintf(lpTemp, dwLogonScriptLen, "%s%s", RUNDLL32_CMDLINE, filename);

    SetLastError(0);
    dwLogonScriptLen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, lpTemp, -1, NULL, 0);
    DebugEvent("ConfigureLogonScript %s requires %d bytes gle=0x%x", lpTemp, dwLogonScriptLen, GetLastError());

    lpScript = LocalAlloc(LMEM_ZEROINIT, dwLogonScriptLen * 2);
    if (lpScript) {
	if (MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, lpTemp, -1, lpScript, 2 * dwLogonScriptLen))
	    *lpLogonScript = lpScript;
	else {
	    DebugEvent("ConfigureLogonScript - MultiByteToWideChar failed gle = 0x%x", GetLastError());
	    LocalFree(lpScript);
	}
    } else {
	DebugEvent("LocalAlloc failed gle=0x%x", GetLastError());
    }
    free(lpTemp);
}


DWORD APIENTRY NPLogonNotify(
	PLUID lpLogonId,
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	LPWSTR *lpLogonScript)
{
    char uname[MAX_USERNAME_LENGTH+1]="";
    char password[MAX_PASSWORD_LENGTH+1]="";
    char logonDomain[MAX_DOMAIN_LENGTH+1]="";

    MSV1_0_INTERACTIVE_LOGON *IL;

    DWORD code = 0;

    char *reason;
    char *ctemp;

    BOOLEAN interactive = TRUE;
    HWND hwndOwner = (HWND)StationHandle;
    BOOLEAN lowercased_name = TRUE;

    /* Can we load KFW binaries? */
    if ( !KFW_is_available() )
        return 0;

    DebugEvent0("NPLogonNotify start");

    /* Remote Desktop / Terminal Server connections to existing sessions
     * are interactive logons.  Unfortunately, because the session already
     * exists the logon script does not get executed and this prevents
     * us from being able to execute the rundll32 entrypoint
     * LogonEventHandlerA which would process the credential cache this
     * routine will produce.  Therefore, we must cleanup orphaned cache
     * files from this routine.  We will take care of it before doing
     * anything else.
     */
    KFW_cleanup_orphaned_caches();

    /* Are we interactive? */
    if (lpStationName)
        interactive = (wcsicmp(lpStationName, L"WinSta0") == 0);

    if ( !interactive ) {
	char station[64]="station";
        DWORD rv;

        SetLastError(0);
	rv = WideCharToMultiByte(CP_UTF8, 0, lpStationName, -1,
			    station, sizeof(station), NULL, NULL);
        DebugEvent("Skipping NPLogonNotify- LoginId(%d,%d) - Interactive(%d:%s) - gle %d",
                    lpLogonId->HighPart, lpLogonId->LowPart, interactive, rv != 0 ? station : "failure", GetLastError());
        return 0;
    } else
        DebugEvent("NPLogonNotify - LoginId(%d,%d)", lpLogonId->HighPart, lpLogonId->LowPart);

    /* Initialize Logon Script to none */
    *lpLogonScript=NULL;

    /* MSV1_0_INTERACTIVE_LOGON and KERB_INTERACTIVE_LOGON are equivalent for
     * our purposes */

    if ( wcsicmp(lpAuthentInfoType,L"MSV1_0:Interactive") &&
         wcsicmp(lpAuthentInfoType,L"Kerberos:Interactive") )
    {
	char msg[64];
	WideCharToMultiByte(CP_ACP, 0, lpAuthentInfoType, -1,
			    msg, sizeof(msg), NULL, NULL);
	msg[sizeof(msg)-1]='\0';
        DebugEvent("NPLogonNotify - Unsupported Authentication Info Type: %s", msg);
        return 0;
    }

    IL = (MSV1_0_INTERACTIVE_LOGON *) lpAuthentInfo;

    /* Convert from Unicode to ANSI */

    /*TODO: Use SecureZeroMemory to erase passwords */
    if (!UnicodeStringToANSI(IL->UserName, uname, MAX_USERNAME_LENGTH) ||
	!UnicodeStringToANSI(IL->Password, password, MAX_PASSWORD_LENGTH) ||
	!UnicodeStringToANSI(IL->LogonDomainName, logonDomain, MAX_DOMAIN_LENGTH))
	return 0;

    /* Make sure AD-DOMAINS sent from login that is sent to us is stripped */
    ctemp = strchr(uname, '@');
    if (ctemp) *ctemp = 0;

    /* is the name all lowercase? */
    for ( ctemp = uname; *ctemp ; ctemp++) {
        if ( !islower(*ctemp) ) {
            lowercased_name = FALSE;
            break;
        }
    }

    code = KFW_get_cred(uname, password, 0, &reason);
    DebugEvent("NPLogonNotify - KFW_get_cred  uname=[%s] code=[%d]",uname, code);

    /* remove any kerberos 5 tickets currently held by the SYSTEM account
     * for this user
     */
    if (!code) {
	char filename[MAX_PATH+1] = "";
	char acctname[MAX_USERNAME_LENGTH+MAX_DOMAIN_LENGTH+3]="";
	PSID pUserSid = NULL;
	LPTSTR pReferencedDomainName = NULL;
	DWORD dwSidLen = 0, dwDomainLen = 0, count;
	SID_NAME_USE eUse;

	if (_snprintf(acctname, sizeof(acctname), "%s\\%s", logonDomain, uname) < 0) {
	    code = -1;
	    goto cleanup;
	}

	count = GetTempPath(sizeof(filename), filename);
        if (count == 0 || count > (sizeof(filename)-1)) {
            code = -1;
            goto cleanup;
        }

	if (_snprintf(filename, sizeof(filename), "%s\\kfwlogon-%x.%x",
		       filename, lpLogonId->HighPart, lpLogonId->LowPart) < 0)
	{
	    code = -1;
	    goto cleanup;
	}

	KFW_copy_cache_to_system_file(uname, filename);

	/* Need to determine the SID */

	/* First get the size of the required buffers */
	LookupAccountName (NULL,
			   acctname,
			   pUserSid,
			   &dwSidLen,
			   pReferencedDomainName,
			   &dwDomainLen,
			   &eUse);
	if(dwSidLen){
	    pUserSid = (PSID) malloc (dwSidLen);
	    memset(pUserSid,0,dwSidLen);
	}

	if(dwDomainLen){
	    pReferencedDomainName = (LPTSTR) malloc (dwDomainLen * sizeof(TCHAR));
	    memset(pReferencedDomainName,0,dwDomainLen * sizeof(TCHAR));
	}

	//Now get the SID and the domain name
	if (pUserSid && LookupAccountName( NULL,
					   acctname,
					   pUserSid,
					   &dwSidLen,
					   pReferencedDomainName,
					   &dwDomainLen,
					   &eUse))
	{
	    DebugEvent("LookupAccountName obtained user %s sid in domain %s", acctname, pReferencedDomainName);
	    code = KFW_set_ccache_dacl_with_user_sid(filename, pUserSid);

#ifdef USE_WINLOGON_EVENT
	    /* If we are on Vista, setup a LogonScript
	     * that will execute the LogonEventHandler entry point via rundll32.exe
	     */
	    if (is_windows_vista()) {
		ConfigureLogonScript(lpLogonScript, filename);
		if (*lpLogonScript)
		    DebugEvent0("LogonScript assigned");
		else
		    DebugEvent0("No Logon Script");
	    }
#else
	    ConfigureLogonScript(lpLogonScript, filename);
	    if (*lpLogonScript)
		    DebugEvent0("LogonScript assigned");
	    else
		    DebugEvent0("No Logon Script");
#endif
	} else {
	    DebugEvent0("LookupAccountName failed");
	    DeleteFile(filename);
	    code = -1;
	}

      cleanup:
	if (pUserSid)
	    free(pUserSid);
	if (pReferencedDomainName)
	    free(pReferencedDomainName);
    }

    KFW_destroy_tickets_for_principal(uname);

    if (code) {
        char msg[128];
        HANDLE h;
        char *ptbuf[1];

        StringCbPrintf(msg, sizeof(msg), "Kerberos ticket acquisition failed: %s", reason);

        h = RegisterEventSource(NULL, KFW_LOGON_EVENT_NAME);
        ptbuf[0] = msg;
        ReportEvent(h, EVENTLOG_WARNING_TYPE, 0, 1008, NULL, 1, 0, ptbuf, NULL);
        DeregisterEventSource(h);
        SetLastError(code);
    }

    if (code)
	DebugEvent0("NPLogonNotify failure");
    else
	DebugEvent0("NPLogonNotify success");

    return code;
}


DWORD APIENTRY NPPasswordChangeNotify(
	LPCWSTR lpAuthentInfoType,
	LPVOID lpAuthentInfo,
	LPCWSTR lpPreviousAuthentInfoType,
	LPVOID lpPreviousAuthentInfo,
	LPWSTR lpStationName,
	LPVOID StationHandle,
	DWORD dwChangeInfo)
{
    return 0;
}

#include <userenv.h>
#include <Winwlx.h>

#ifdef COMMENT
typedef struct _WLX_NOTIFICATION_INFO {
    ULONG Size;
    ULONG Flags;
    PWSTR UserName;
    PWSTR Domain;
    PWSTR WindowStation;
    HANDLE hToken;
    HDESK hDesktop;
    PFNMSGECALLBACK pStatusCallback;
} WLX_NOTIFICATION_INFO, *PWLX_NOTIFICATION_INFO;
#endif

VOID KFW_Startup_Event( PWLX_NOTIFICATION_INFO pInfo )
{
    DebugEvent0("KFW_Startup_Event");
}

static BOOL
GetSecurityLogonSessionData(HANDLE hToken, PSECURITY_LOGON_SESSION_DATA * ppSessionData)
{
    NTSTATUS Status = 0;
    TOKEN_STATISTICS Stats;
    DWORD   ReqLen;
    BOOL    Success;

    if (!ppSessionData)
        return FALSE;
    *ppSessionData = NULL;


    Success = GetTokenInformation( hToken, TokenStatistics, &Stats, sizeof(TOKEN_STATISTICS), &ReqLen );
    if ( !Success )
        return FALSE;

    Status = LsaGetLogonSessionData( &Stats.AuthenticationId, ppSessionData );
    if ( FAILED(Status) || !ppSessionData )
        return FALSE;

    return TRUE;
}

VOID KFW_Logon_Event( PWLX_NOTIFICATION_INFO pInfo )
{
#ifdef USE_WINLOGON_EVENT
    WCHAR szUserW[128] = L"";
    char  szUserA[128] = "";
    char szPath[MAX_PATH] = "";
    char szLogonId[128] = "";
    DWORD count;
    char filename[MAX_PATH] = "";
    char newfilename[MAX_PATH] = "";
    char commandline[MAX_PATH+256] = "";
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION procinfo;
    HANDLE hf = NULL;

    LUID LogonId = {0, 0};
    PSECURITY_LOGON_SESSION_DATA pLogonSessionData = NULL;

    HKEY hKey1 = NULL, hKey2 = NULL;

    DebugEvent0("KFW_Logon_Event - Start");

    GetSecurityLogonSessionData( pInfo->hToken, &pLogonSessionData );

    if ( pLogonSessionData ) {
        LogonId = pLogonSessionData->LogonId;
        DebugEvent("KFW_Logon_Event - LogonId(%d,%d)", LogonId.HighPart, LogonId.LowPart);

        _snprintf(szLogonId, sizeof(szLogonId), "kfwlogon-%d.%d",LogonId.HighPart, LogonId.LowPart);
        LsaFreeReturnBuffer( pLogonSessionData );
    } else {
        DebugEvent0("KFW_Logon_Event - Unable to determine LogonId");
        return;
    }

    count = GetEnvironmentVariable("TEMP", filename, sizeof(filename));
    if ( count > sizeof(filename) || count == 0 ) {
        GetWindowsDirectory(filename, sizeof(filename));
    }

    if ( strlen(filename) + strlen(szLogonId) + 2 > sizeof(filename) ) {
        DebugEvent0("KFW_Logon_Event - filename too long");
	return;
    }

    strcat(filename, "\\");
    strcat(filename, szLogonId);

    hf = CreateFile(filename, FILE_ALL_ACCESS, 0, NULL, OPEN_EXISTING,
		    FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        DebugEvent0("KFW_Logon_Event - file cannot be opened");
	return;
    }
    CloseHandle(hf);

    if (KFW_set_ccache_dacl(filename, pInfo->hToken)) {
        DebugEvent0("KFW_Logon_Event - unable to set dacl");
	DeleteFile(filename);
	return;
    }

    if (KFW_obtain_user_temp_directory(pInfo->hToken, newfilename, sizeof(newfilename))) {
        DebugEvent0("KFW_Logon_Event - unable to obtain temp directory");
	return;
    }

    if ( strlen(newfilename) + strlen(szLogonId) + 2 > sizeof(newfilename) ) {
        DebugEvent0("KFW_Logon_Event - new filename too long");
	return;
    }

    strcat(newfilename, "\\");
    strcat(newfilename, szLogonId);

    if (!MoveFileEx(filename, newfilename,
		     MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DebugEvent("KFW_Logon_Event - MoveFileEx failed GLE = 0x%x", GetLastError());
	return;
    }

    _snprintf(commandline, sizeof(commandline), "kfwcpcc.exe \"%s\"", newfilename);

    GetStartupInfo(&startupinfo);
    if (CreateProcessAsUser( pInfo->hToken,
                             "kfwcpcc.exe",
                             commandline,
                             NULL,
                             NULL,
                             FALSE,
                             CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
                             NULL,
                             NULL,
                             &startupinfo,
                             &procinfo))
    {
	DebugEvent("KFW_Logon_Event - CommandLine %s", commandline);

	WaitForSingleObject(procinfo.hProcess, 30000);

	CloseHandle(procinfo.hThread);
	CloseHandle(procinfo.hProcess);
    } else {
	DebugEvent0("KFW_Logon_Event - CreateProcessFailed");
    }

    DeleteFile(newfilename);

    DebugEvent0("KFW_Logon_Event - End");
#endif /* USE_WINLOGON_EVENT */
}


/* Documentation on the use of RunDll32 entrypoints can be found
 * at https://support.microsoft.com/kb/164787
 */
void CALLBACK
LogonEventHandlerA(HWND hwnd, HINSTANCE hinst, LPSTR lpszCmdLine, int nCmdShow)
{
    HANDLE hf = NULL;
    char commandline[MAX_PATH+256] = "";
    STARTUPINFO startupinfo;
    PROCESS_INFORMATION procinfo;

    DebugEvent0("LogonEventHandler - Start");

    /* Validate lpszCmdLine as a file */
    hf = CreateFile(lpszCmdLine, GENERIC_READ | DELETE, 0, NULL, OPEN_EXISTING,
		    FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        DebugEvent("LogonEventHandler - \"%s\" cannot be opened", lpszCmdLine);
	return;
    }
    CloseHandle(hf);


    _snprintf(commandline, sizeof(commandline), "kfwcpcc.exe \"%s\"", lpszCmdLine);

    GetStartupInfo(&startupinfo);
    SetLastError(0);
    if (CreateProcess( NULL,
		       commandline,
		       NULL,
		       NULL,
		       FALSE,
		       CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS,
		       NULL,
		       NULL,
		       &startupinfo,
		       &procinfo))
    {
	DebugEvent("KFW_Logon_Event - CommandLine %s", commandline);

	WaitForSingleObject(procinfo.hProcess, 30000);

	CloseHandle(procinfo.hThread);
	CloseHandle(procinfo.hProcess);
    } else {
	DebugEvent("KFW_Logon_Event - CreateProcessFailed \"%s\" GLE 0x%x",
                     commandline, GetLastError());
        DebugEvent("KFW_Logon_Event PATH %s", getenv("PATH"));
    }

    DeleteFile(lpszCmdLine);

    DebugEvent0("KFW_Logon_Event - End");
}
