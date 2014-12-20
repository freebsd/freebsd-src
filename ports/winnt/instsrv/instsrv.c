/*
 *  File: instsrv.c
 *  Purpose: To install a new service and to insert registry entries.
 *
 */
#ifndef __RPCASYNC_H__
#define __RPCASYNC_H__	/* Skip asynch rpc inclusion */
#endif

#include <windows.h>
#include <stdio.h>

#define PERR(api) printf("\n%s: Error %d from %s on line %d",  \
    __FILE__, GetLastError(), api, __LINE__);

#define MSG_FOR_ACCESS_DENIED "You aren't authorized to do this - please see your system Administrator"
#define MSG_1_FOR_BAD_PATH "The fully qualified path name to the .exe must be given, and"
#define MSG_2_FOR_BAD_PATH "  the drive letter must be for a fixed disk (e.g., not a net drive)"

SC_HANDLE schService;
SC_HANDLE schSCManager;
int ok2;

VOID DisplayHelp(VOID);

/* --------------------------------------------------------------------------------------- */

int InstallService(LPCTSTR serviceName, LPCTSTR displayName, LPCTSTR serviceExe)
{
  LPCTSTR lpszBinaryPathName = serviceExe;
  TCHAR lpszRootPathName[] ="?:\\";

  if ( (':' != *(lpszBinaryPathName+1)) || ('\\' != *(lpszBinaryPathName+2)) )
  { printf("\n%s",MSG_1_FOR_BAD_PATH);
    printf("\n%s\n",MSG_2_FOR_BAD_PATH);
    return 1;
  }

  #define DRIVE_TYPE_INDETERMINATE 0
  #define ROOT_DIR_DOESNT_EXIST    1

  *lpszRootPathName = *(lpszBinaryPathName+0) ;

  switch (  GetDriveType(lpszRootPathName)  )
  {
    case DRIVE_FIXED :
    { // OK
      break;
    }
    case  ROOT_DIR_DOESNT_EXIST :
    { printf("\n%s",MSG_1_FOR_BAD_PATH);
      printf("\n  the root directory where the .exe is specified to be must exist, and");
      printf("\n%s\n",MSG_2_FOR_BAD_PATH);
      return 1;
    }
    case  DRIVE_TYPE_INDETERMINATE :
    case  DRIVE_REMOVABLE          :
    case  DRIVE_REMOTE             :
    case  DRIVE_CDROM              :
    case  DRIVE_RAMDISK            :
    { printf("\n%s",MSG_1_FOR_BAD_PATH);
      printf("\n%s\n",MSG_2_FOR_BAD_PATH);
      return 1;
    }
    default :
    { printf("\n%s",MSG_1_FOR_BAD_PATH);
      printf("\n%s\n",MSG_2_FOR_BAD_PATH);
      return 1;
    }
  }

  if (INVALID_HANDLE_VALUE == CreateFile(lpszBinaryPathName,
                                         GENERIC_READ,
                                         FILE_SHARE_READ,
                                         NULL,
                                         OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL,
                                         NULL))
  { 
    printf("\n%s",MSG_1_FOR_BAD_PATH);
    printf("\n  the file must exist, and");
    printf("\n%s\n",MSG_2_FOR_BAD_PATH);
    return 1;
  }

  schService = CreateService(
        schSCManager,               // SCManager database
        serviceName,                // name of service
        displayName,                // name to display
        SERVICE_ALL_ACCESS,         // desired access
        SERVICE_WIN32_OWN_PROCESS,  // service type
        SERVICE_AUTO_START,         // start type
        SERVICE_ERROR_NORMAL,       // error control type
        lpszBinaryPathName,         // service's binary
        NULL,                       // no load ordering group
        NULL,                       // no tag identifier
        NULL,                       // no dependencies
        NULL,                       // Local System account
        NULL);                      // null password

  if (NULL == schService)
  { switch (GetLastError())
    {
      case ERROR_ACCESS_DENIED :
      { printf("\n%s",MSG_FOR_ACCESS_DENIED);
        break;
      }
      case ERROR_SERVICE_EXISTS :
      { printf("\nThe %s service is already installed",serviceName);
        printf("\nRemove it first if you need to re-install a new version\n");
        break;
      }
      default :
      { PERR("CreateService");
      }
    }
    return 1;
  }
  else

  CloseServiceHandle(schService);
  return 0;
}

/* --------------------------------------------------------------------------------------- */

int RemoveService(LPCTSTR serviceName)
{
  {
    #define                                     SZ_ENUM_BUF 4096
    ENUM_SERVICE_STATUS        essServiceStatus[SZ_ENUM_BUF];
    DWORD   dwBufSize = sizeof(essServiceStatus);
    DWORD   dwBytesNeeded      = 0;
    DWORD   dwServicesReturned = 0;
    DWORD   dwResumeHandle     = 0;
    DWORD   dwI                = 0;
    BOOLEAN bFound = FALSE;

    if (!EnumServicesStatus(schSCManager,
                            SERVICE_WIN32,
                            SERVICE_ACTIVE,
                            (LPENUM_SERVICE_STATUS)&essServiceStatus,
                            dwBufSize,
                            &dwBytesNeeded,
                            &dwServicesReturned,
                            &dwResumeHandle))
    { switch (GetLastError())
      {
        case ERROR_ACCESS_DENIED :
        { printf("\n%s",MSG_FOR_ACCESS_DENIED);
          break;
        }
        default :
        { PERR("EnumServicesStatus");
        }
      }
      return 1;
    }

    for (dwI=0; dwI<dwServicesReturned; dwI++)
    { if(0 == _stricmp(essServiceStatus[dwI].lpServiceName,serviceName))
      { bFound = TRUE;
        break;
      }
    }

    if (bFound)
    { printf("\nThe %s service cannot be removed until it has been stopped.",serviceName);
      printf("\nTo stop the %s service, use the Stop button in the Control",serviceName);
      printf("\n  Panel Services applet\n");
      return 1;
    }
  }

  schService = OpenService(schSCManager,
                           serviceName,
                           SERVICE_ALL_ACCESS);
  if (NULL == schService)
  { switch (GetLastError())
    {
      case ERROR_ACCESS_DENIED :
      { printf("\n%s",MSG_FOR_ACCESS_DENIED);
        break;
      }
      case ERROR_SERVICE_DOES_NOT_EXIST :
      { printf("\nThe %s service is not installed, so cannot be removed\n",serviceName);
        break;
      }
      default :
      { PERR("OpenService");
      }
    }
    return 1;
  }

  if (DeleteService(schService))
  { printf("\nDelete of Service \"Network Time Protocol\" was SUCCESSFUL\n");
   return 0;
  }
  else
  { switch (GetLastError())
    {
      case ERROR_ACCESS_DENIED :
      { printf("\n%s",MSG_FOR_ACCESS_DENIED);
        break;
      }
      default :
      { PERR("DeleteService");
      }
    }
   return 1;
  }
}

/* --------------------------------------------------------------------------------------- */

int addSourceToRegistry(LPSTR pszAppname, LPSTR pszMsgDLL)
{
  HKEY hk;                      /* registry key handle */
  DWORD dwData;
  BOOL bSuccess;
  char   regarray[200];
  char *lpregarray = regarray;

  /* When an application uses the RegisterEventSource or OpenEventLog
     function to get a handle of an event log, the event loggging service
     searches for the specified source name in the registry. You can add a
     new source name to the registry by opening a new registry subkey
     under the Application key and adding registry values to the new
     subkey. */

  strcpy(lpregarray, "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\");
  strcat(lpregarray, pszAppname);
  /* Create a new key for our application */
  bSuccess = RegCreateKey(HKEY_LOCAL_MACHINE, lpregarray, &hk);
   if(bSuccess != ERROR_SUCCESS)
    {
      PERR("RegCreateKey");
      return 1;
    }
    
  /* Add the Event-ID message-file name to the subkey. */
  bSuccess = RegSetValueEx(hk,  /* subkey handle         */
      "EventMessageFile",       /* value name            */
      0,                        /* must be zero          */
      REG_EXPAND_SZ,            /* value type            */
      (LPBYTE) pszMsgDLL,       /* address of value data */
      strlen(pszMsgDLL) + 1);   /* length of value data  */
 if(bSuccess != ERROR_SUCCESS)
    {
      PERR("RegSetValueEx");
      return 1;
    }
  
  /* Set the supported types flags and addit to the subkey. */
  dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
      EVENTLOG_INFORMATION_TYPE;
  bSuccess = RegSetValueEx(hk,  /* subkey handle                */
      "TypesSupported",         /* value name                   */
      0,                        /* must be zero                 */
      REG_DWORD,                /* value type                   */
      (LPBYTE) &dwData,         /* address of value data        */
      sizeof(DWORD));           /* length of value data         */
  if(bSuccess != ERROR_SUCCESS)
    {
      PERR("RegSetValueEx");
      return 1;
    }
  RegCloseKey(hk);
  return 0;
}

/* --------------------------------------------------------------------------------------- */

int addKeysToRegistry()

{
  HKEY hk;                      /* registry key handle */
  BOOL bSuccess;
  char   myarray[200];
  char *lpmyarray = myarray;
  int arsize = 0;

  /* now add the depends on service key */
 
  /* Create a new key for our application */
  bSuccess = RegCreateKey(HKEY_LOCAL_MACHINE,
      "SYSTEM\\CurrentControlSet\\Services\\NTP", &hk);
  if(bSuccess != ERROR_SUCCESS)
    {
      PERR("RegCreateKey");
      return 1;
    }

  strcpy(lpmyarray,"TcpIp");
  lpmyarray = lpmyarray + 6;
  arsize = arsize + 6;
  strcpy(lpmyarray,"Afd");
  lpmyarray = lpmyarray + 4;
  arsize = arsize + 4;
  arsize = arsize + 2;
  strcpy(lpmyarray,"\0\0");
  
  bSuccess = RegSetValueEx(hk,  /* subkey handle         */
      "DependOnService",        /* value name            */
      0,                        /* must be zero          */
      REG_MULTI_SZ,             /* value type            */
      (LPBYTE) &myarray,        /* address of value data */
      arsize);                  /* length of value data  */
   if(bSuccess != ERROR_SUCCESS)
    {
      PERR("RegSetValueEx");
      return 1;
    }

  RegCloseKey(hk);
  return 0;
}

/* --------------------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
  #define           SZ_NAME_BUF  270  // 256 is max, add a little
  UCHAR   ucNameBuf[SZ_NAME_BUF] = "NTP";
  LPTSTR  lpszServName = (LPTSTR)&ucNameBuf;

  UCHAR   ucDNameBuf[SZ_NAME_BUF] = "Network Time Protocol";
  LPTSTR  lpszDispName = (LPTSTR)&ucDNameBuf;


  UCHAR   ucExeNBuf[SZ_NAME_BUF] = "";
  LPTSTR  lpszExeName  = (LPTSTR)&ucExeNBuf;

  BOOL    bRemovingService = FALSE;
  char *p;

  int ok = 0;  
  
  // check if Win32s, if so, display notice and terminate
      if( GetVersion() & 0x80000000 )
      {
        MessageBox( NULL,
           "This application cannot run on Windows 3.1.\n"
           "This application will now terminate.",
           "NAMED",
           MB_OK | MB_ICONSTOP | MB_SETFOREGROUND );
        return( 1 );
      }
  if (argc == 2)
     bRemovingService = (!stricmp(argv[1], "remove"));

  if(!bRemovingService)
   {

  
  if (argc != 2)
  {
    DisplayHelp();
    return(1);
  }

  p=argv[1];
  if (    ('/' == *p)
       || ('-' == *p) )
  {
    DisplayHelp();
    return(1);
  }
        
  
   }

  if (strlen(argv[1]) > 256)
    {
      printf("\nThe service name cannot be longer than 256 characters\n");
      return(1);
    }



  bRemovingService = (!stricmp(argv[1], "remove"));
  schSCManager = OpenSCManager(
                      NULL,                   // machine (NULL == local)
                      NULL,                   // database (NULL == default)
                      SC_MANAGER_ALL_ACCESS); // access required

  if (NULL == schSCManager)
  { switch (GetLastError())
    {
      case ERROR_ACCESS_DENIED :
      { printf("\n%s",MSG_FOR_ACCESS_DENIED);
        break;
      }
      default :
      { PERR("OpenSCManager");
      }
    }
    return (0);
  }
   
  if (bRemovingService)
  {
   ok = RemoveService(lpszServName);
  }
  else
  {
   /* get the exe name */
   strcpy(lpszExeName,argv[1]);
   ok = InstallService(lpszServName, lpszDispName, lpszExeName);
  }

  CloseServiceHandle(schSCManager);

  if (!bRemovingService)
    {
  if (ok == 0)
   { /* Set the Event-ID message-file name. */
    ok = addSourceToRegistry("NTP", lpszExeName);
    if (ok == 0)
      ok = addKeysToRegistry();
    else return ok;

    if (ok == 0)
    {
      printf("\nThe \"Network Time Protocol\" service was successfully created.\n");
      printf("\nDon't forget!!! You must now go to the Control Panel and");
      printf("\n  use the Services applet to change the account name and");
      printf("\n  password that the NTP Service will use when");
      printf("\n  it starts.");
      printf("\nTo do this: use the Startup button in the Services applet,");
      printf("\n  and (for example) specify the desired account and");
      printf("\n  correct password.");
      printf("\nAlso, use the Services applet to ensure this newly installed");
      printf("\n  service starts automatically on bootup.\n");
     return 0;
    }
   }
  else return ok;
  }
 return 0;
}

/* --------------------------------------------------------------------------------------- */

VOID DisplayHelp(VOID)
{
    printf("Installs or removes the NTP service.\n");
    printf("To install the NTP service,\n");
    printf("type INSTSRV <path> \n");
    printf("Where:\n");
    printf("    path    Absolute path to the NTP service, name.exe.  You must\n");
    printf("            use a fully qualified path and the drive letter must be for a\n");
    printf("            fixed, local drive.\n\n");
    printf("For example, INSTSRV i:\\winnt\\system32\\ntpd.exe\n");
    printf("To remove the NTP service,\n");
    printf("type INSTSRV remove \n");

}

/* EOF */
