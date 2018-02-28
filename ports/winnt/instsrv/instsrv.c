/*
 *  File: instsrv.c
 *  Purpose: To install a new service and to insert registry entries.
 *
 */
#ifndef __RPCASYNC_H__
#define __RPCASYNC_H__  /* Skip asynch rpc inclusion */
#endif

#include <windows.h>
#include <stdio.h>

#if _MSC_VER <= 1800
#define snprintf _snprintf
#endif

#define PERR(api) printf("\n%s: Error %d from %s on line %d",  \
                         __FILE__, GetLastError(), api, __LINE__);

#define MSG_FOR_ACCESS_DENIED "You aren't authorized to do this - please see your system Administrator"
#define MSG_1_FOR_BAD_PATH "The fully qualified path name to the .exe must be given, and"
#define MSG_2_FOR_BAD_PATH "  the drive letter must be for a fixed disk (e.g., not a net drive)"

/* Building an initialised REG_MULTISZ needs a bit of care: The array must be big enough
** to hold the closing double-NUL and should have *exactly* the required size. Since the
** dependencies are fixed here, we take exactly the required 11 bytes:
*/
static const char s_acSvcDeps[11] = "TcpIp\0Afd\0\0";
/* likewise we do with the PPAS API list... */
static const char s_acPpsDlls[30] = "loopback-ppsapi-provider.dll\0\0";

SC_HANDLE schService;
SC_HANDLE schSCManager;
int ok2;

void DisplayHelp(void);

/* --------------------------------------------------------------------------------------- */
static const char *
getServicePath(
    const char * const * const argv,
    const int                  argc) /* MUST be >= 1 !! */
{
    static const char * const s_chars_to_quote = " []()";
    size_t      minsize = argc * 3; /* may need separator/NUL + quotes */
    int         i;
    const char *execPath = argv[0];
    char       *cbuf, *cpos;
    
    /* if just the executable and no dangerous chars, return the exe path. */
    if (argc == 1 && !strpbrk(execPath, s_chars_to_quote))
        return execPath;
    
    /* calculate buffer size and get the buffer */
    for (i = 0; i < argc; ++i)
        minsize += strlen(argv[i]);

    cpos = cbuf = malloc(minsize);
    if (NULL == cbuf) {
        printf("malloc() failed\n");
        exit(2);
    }
    
    /* program name must be quoted in all cases */
    *cpos++ = '"';
    strcpy(cpos, execPath);
    cpos += strlen(cpos);
    *cpos++ = '"';
    
    /* append additional args */
    for (i = 1; i < argc; ++i) {
        *cpos++ = ' ';
        if (strpbrk(argv[i], s_chars_to_quote)) { /* needs quotes? */
            *cpos++ = '"';
            strcpy(cpos, argv[i]);
            cpos += strlen(cpos);
            *cpos++ = '"';
        } else {
            strcpy(cpos, argv[i]);
            cpos += strlen(cpos);
        }
    }
    *cpos = '\0';
    return cbuf;
}

/* --------------------------------------------------------------------------------------- */
BOOL
validateExeName(
        const char *exePath)
{
    char rootPath[] = "?:\\";
    
    /* check for absolute path */
    if ((':' != exePath[1]) || ('\\' != exePath[2])) {
        printf("\n%s", MSG_1_FOR_BAD_PATH);
        printf("\n%s\n", MSG_2_FOR_BAD_PATH);
        return 1;
    }

#define DRIVE_TYPE_INDETERMINATE 0
#define ROOT_DIR_DOESNT_EXIST    1
    
    /* check drive type -- must be local HDD! */
    rootPath[0] = exePath[0];
    switch (GetDriveTypeA(rootPath))
    {
    case DRIVE_FIXED:
        // OK
        break;
        
    case  ROOT_DIR_DOESNT_EXIST:
        printf("\n%s", MSG_1_FOR_BAD_PATH);
        printf("\n  the root directory where the .exe is specified to be must exist, and");
        printf("\n%s\n", MSG_2_FOR_BAD_PATH);
        return 1;
        
    case  DRIVE_TYPE_INDETERMINATE:
    case  DRIVE_REMOVABLE:
    case  DRIVE_REMOTE:
    case  DRIVE_CDROM:
    case  DRIVE_RAMDISK:
        printf("\n%s", MSG_1_FOR_BAD_PATH);
        printf("\n%s\n", MSG_2_FOR_BAD_PATH);
        return 1;
        
    default:
        printf("\n%s", MSG_1_FOR_BAD_PATH);
        printf("\n%s\n", MSG_2_FOR_BAD_PATH);
        return 1;
    }
    
    /* check if file exists. We just drop the handle. This is a one-shot program! */
    if (INVALID_HANDLE_VALUE == CreateFileA(
            exePath, GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
    {
        printf("\n%s", MSG_1_FOR_BAD_PATH);
        printf("\n  the file must exist, and");
        printf("\n%s\n", MSG_2_FOR_BAD_PATH);
        return 1;
    }
    
    return 0;
}
/* --------------------------------------------------------------------------------------- */

int
InstallService(
    LPCSTR serviceName,
    LPCSTR displayName,
    LPCSTR serviceExe ,
    LPCSTR servDepends)
{

    /* create the service now. */
    schService = CreateServiceA(
        schSCManager,               // SCManager database
        serviceName,                // name of service
        displayName,                // name to display
        SERVICE_ALL_ACCESS,         // desired access
        SERVICE_WIN32_OWN_PROCESS,  // service type
        SERVICE_AUTO_START,         // start type
        SERVICE_ERROR_NORMAL,       // error control type
        serviceExe,                 // service's binary
        NULL,                       // no load ordering group
        NULL,                       // no tag identifier
        servDepends,                // possible dependencies
        NULL,                       // Local System account
        NULL);                      // null password

    if (NULL == schService) {
        switch (GetLastError())
        {
        case ERROR_ACCESS_DENIED :
            printf("\n%s",MSG_FOR_ACCESS_DENIED);
            break;
        
        case ERROR_SERVICE_EXISTS :
            printf("\nThe %s service is already installed",serviceName);
            printf("\nRemove it first if you need to re-install a new version\n");
            break;

        default :
            PERR("CreateService");
            break;
        }
        return 1;
    }

    CloseServiceHandle(schService);
    return 0;
}

/* --------------------------------------------------------------------------------------- */

int
RemoveService(
    LPCSTR serviceName)
{
    {
#       define SZ_ENUM_BUF 4096
        ENUM_SERVICE_STATUS essServiceStatus[SZ_ENUM_BUF];
        DWORD   dwBufSize = sizeof(essServiceStatus);
        DWORD   dwBytesNeeded      = 0;
        DWORD   dwServicesReturned = 0;
        DWORD   dwResumeHandle     = 0;
        DWORD   dwI                = 0;
        BOOLEAN bFound = FALSE;

        if (!EnumServicesStatusA(schSCManager, SERVICE_WIN32, SERVICE_ACTIVE,
                                (LPENUM_SERVICE_STATUS)&essServiceStatus,
                                dwBufSize, &dwBytesNeeded, &dwServicesReturned,
                                &dwResumeHandle))
        {
            switch (GetLastError())
            {
            case ERROR_ACCESS_DENIED :
                printf("\n%s", MSG_FOR_ACCESS_DENIED);
                break;

            default :
                PERR("EnumServicesStatus");
                break;
            }
            return 1;
        }

        for (dwI = 0; dwI < dwServicesReturned; ++dwI) {
            if(0 == _stricmp(essServiceStatus[dwI].lpServiceName, serviceName)) {
                bFound = TRUE;
                break;
            }
        }
        
        if (bFound) {
            printf("\nThe %s service cannot be removed until it has been stopped.", serviceName);
            printf("\nTo stop the %s service, use the Stop button in the Control" , serviceName);
            printf("\n  Panel Services applet\n");
            return 1;
        }
    }
    
    schService = OpenServiceA(schSCManager, serviceName, SERVICE_ALL_ACCESS);
    if (NULL == schService) {
        switch (GetLastError())
        {
        case ERROR_ACCESS_DENIED :
            printf("\n%s", MSG_FOR_ACCESS_DENIED);
            break;

        case ERROR_SERVICE_DOES_NOT_EXIST :
            printf("\nThe %s service is not installed, so cannot be removed\n", serviceName);
            break;

        default :
            PERR("OpenService");
            break;
        }
        return 1;
    }
    
    if (DeleteService(schService)) {
        printf("\nDelete of Service \"Network Time Protocol\" was SUCCESSFUL\n");
        return 0;
    }

    switch (GetLastError())
    {
    case ERROR_ACCESS_DENIED :
        printf("\n%s", MSG_FOR_ACCESS_DENIED);
        break;

    default :
        PERR("DeleteService");
        break;
    }
    
    return 1;
}

/* --------------------------------------------------------------------------------------- */

int
addSourceToRegistry(
    const char * pszAppname,
    const char * pszMsgDLL )
{
    HKEY  hk;                      /* registry key handle */
    DWORD dwData;
    BOOL  bSuccess;
    char  regarray[200];
    int   rc;

    /* When an application uses the RegisterEventSource or OpenEventLog function to get a
       handle of an event log, the event logging service searches for the specified
       source name in the registry. You can add a new source name to the registry by
       opening a new registry subkey under the Application key and adding registry values
       to the new subkey.
    */

    rc = _snprintf(regarray, sizeof(regarray), "%s%s",
                  "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\",
                  pszAppname);
    if (rc < 0 || rc >= sizeof(regarray)) {
        fputs("addSourceToRegistry: buffer overrun(app name)", stderr);
        return 1;
    }
    /* Create a new key for our application */
    bSuccess = RegCreateKeyA(HKEY_LOCAL_MACHINE, regarray, &hk);
    if(bSuccess != ERROR_SUCCESS) {
        PERR("RegCreateKey");
        return 1;
    }
    
    /* Add the Event-ID message-file name to the subkey. */
    bSuccess = RegSetValueExA(hk,                               /* subkey handle         */
                              "EventMessageFile",               /* value name            */
                              0,                                /* must be zero          */
                              REG_EXPAND_SZ,                    /* value type            */
                              (LPBYTE)pszMsgDLL,                /* address of value data */
                              (DWORD)(strlen(pszMsgDLL) + 1));  /* length of value data  */
    if(bSuccess != ERROR_SUCCESS) {
        PERR("RegSetValueEx");
        return 1;
    }
  
    /* Set the supported types flags and addit to the subkey. */
    dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    bSuccess = RegSetValueExA(hk,                       /* subkey handle                */
                              "TypesSupported",         /* value name                   */
                              0,                        /* must be zero                 */
                              REG_DWORD,                /* value type                   */
                              (LPBYTE) &dwData,         /* address of value data        */
                              sizeof(DWORD));           /* length of value data         */
    if(bSuccess != ERROR_SUCCESS) {
        PERR("RegSetValueEx");
        return 1;
    }
    
    RegCloseKey(hk);
    return 0;
}

/* --------------------------------------------------------------------------------------- */

int
addKeysToRegistry(void)
{
    HKEY hk;                      /* registry key handle */
    BOOL bSuccess;

    /* now add the depends on service key */
 
    /* Create a new key for our application */
    bSuccess = RegCreateKeyA(HKEY_LOCAL_MACHINE,
                             "SYSTEM\\CurrentControlSet\\Services\\NTP", &hk);
    if(bSuccess != ERROR_SUCCESS) {
        PERR("RegCreateKey");
        return 1;
    }

    bSuccess = RegSetValueExA(hk,                           /* subkey handle         */
                              "PPSProviders",               /* value name            */
                              0,                            /* must be zero          */
                              REG_MULTI_SZ,                 /* value type            */
                              (LPBYTE)s_acPpsDlls,          /* address of value data */
                              (DWORD)sizeof(s_acPpsDlls));  /* length of value data  */
    if (bSuccess != ERROR_SUCCESS) {
        PERR("RegSetValueEx");
        return 1;
    }

    RegCloseKey(hk);
    return 0;
}

/* --------------------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    static const char * const szServiceName = "NTP";
    static const char * const szDisplayName = "Network Time Protocol";

    BOOL  bRemovingService = FALSE;
    char *p;
    int   ok = 0;  
  
    // check if Win32s, if so, display notice and terminate
    if (GetVersion() & 0x80000000) {
        MessageBoxA(NULL,
                    "This application cannot run on Windows 3.1.\n"
                    "This application will now terminate.",
                    "NAMED",
                    MB_OK | MB_ICONSTOP | MB_SETFOREGROUND );
        return 1;
    }

    if (argc >= 2)
            bRemovingService = (!stricmp(argv[1], "remove"));

    if ((bRemovingService && argc != 2) || (!bRemovingService && argc < 2)) {
        DisplayHelp();
        return 1;
    }

    if(!bRemovingService) {
        p = argv[1];
        if (('/' == *p) || ('-' == *p) || validateExeName(p)) {
            DisplayHelp();
            return 1;
        }
    }


    if (strlen(argv[1]) > 256) {
        printf("\nThe service name cannot be longer than 256 characters\n");
        return 1;
    }

    schSCManager = OpenSCManagerA(
        NULL,                   // machine (NULL == local)
        NULL,                   // database (NULL == default)
        SC_MANAGER_ALL_ACCESS); // access required

    if (NULL == schSCManager) {
        switch (GetLastError())
        {
        case ERROR_ACCESS_DENIED :
            printf("\n%s", MSG_FOR_ACCESS_DENIED);
            break;

        default :
            PERR("OpenSCManager");
            break;
        }
        return 0;
    }
   
    if (bRemovingService)
        ok = RemoveService(szServiceName);
    else
        ok = InstallService(szServiceName, szDisplayName,
                            getServicePath(argv+1, argc-1), s_acSvcDeps);

    CloseServiceHandle(schSCManager);

    if (!bRemovingService) {
        if (ok == 0)
            ok = addSourceToRegistry("NTP", argv[1]);/* Set the Event-ID message-file name. */
        if (ok == 0)
            ok = addKeysToRegistry(); /* add other stuff */

        if (ok == 0)
        {
            static const char s_msg[] =
                "\nThe \"Network Time Protocol\" service was successfully created.\n"
                "\nDon't forget!!! You must now go to the Control Panel and"
                "\n  use the Services applet to change the account name and"
                "\n  password that the NTP Service will use when"
                "\n  it starts."
                "\nTo do this: use the Startup button in the Services applet,"
                "\n  and (for example) specify the desired account and"
                "\n  correct password."
                "\nAlso, use the Services applet to ensure this newly installed"
                "\n  service starts automatically on bootup.\n";
            fputs(s_msg, stdout);
        }
    }
    return ok;
}

/* --------------------------------------------------------------------------------------- */

void
DisplayHelp(void)
{
    static const char s_hlpmsg[] =
        "Installs or removes the NTP service.\n"
        "To install the NTP service,\n"
        "type INSTSRV <path> [args]\n"
        "Where:\n"
        "    path    Absolute path to the NTP service. (ntpd.exe)  You must\n"
        "            use a fully qualified path and the drive letter must be for a\n"
        "            fixed, local drive.\n\n"
        "    args    Additional command line arguments for the service\n"
        "For example, INSTSRV i:\\winnt\\system32\\ntpd.exe\n"
        "To remove the NTP service,\n"
        "type INSTSRV remove \n";
    fputs(s_hlpmsg, stdout);
}

/* EOF */
