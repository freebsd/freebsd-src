/* Microsoft Developer Support Copyright (c) 1993 Microsoft Corporation. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "messages.h"
#include "log.h"

#define PERR(bSuccess, api) {if(!(bSuccess)) printf("%s: Error %d from %s \
    on line %d\n", __FILE__, GetLastError(), api, __LINE__);}


/*********************************************************************
* FUNCTION: addSourceToRegistry(void)                                *
*                                                                    *
* PURPOSE: Add a source name key, message DLL name value, and        *
*          message type supported value to the registry              *
*                                                                    *
* INPUT: source name, path of message DLL                            *
*                                                                    *
* RETURNS: none                                                      *
*********************************************************************/

void addSourceToRegistry(LPSTR pszAppname, LPSTR pszMsgDLL)
{
  HKEY hk;                      /* registry key handle */
  DWORD dwData;
  BOOL bSuccess;

  /* When an application uses the RegisterEventSource or OpenEventLog
     function to get a handle of an event log, the event loggging service
     searches for the specified source name in the registry. You can add a
     new source name to the registry by opening a new registry subkey
     under the Application key and adding registry values to the new
     subkey. */

  /* Create a new key for our application */
  bSuccess = RegCreateKey(HKEY_LOCAL_MACHINE,
      "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\NTP", &hk);
  PERR(bSuccess == ERROR_SUCCESS, "RegCreateKey");

  /* Add the Event-ID message-file name to the subkey. */
  bSuccess = RegSetValueEx(hk,  /* subkey handle         */
      "EventMessageFile",       /* value name            */
      0,                        /* must be zero          */
      REG_EXPAND_SZ,            /* value type            */
      (LPBYTE) pszMsgDLL,       /* address of value data */
      strlen(pszMsgDLL) + 1);   /* length of value data  */
  PERR(bSuccess == ERROR_SUCCESS, "RegSetValueEx");

  /* Set the supported types flags and addit to the subkey. */
  dwData = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE |
      EVENTLOG_INFORMATION_TYPE;
  bSuccess = RegSetValueEx(hk,  /* subkey handle                */
      "TypesSupported",         /* value name                   */
      0,                        /* must be zero                 */
      REG_DWORD,                /* value type                   */
      (LPBYTE) &dwData,         /* address of value data        */
      sizeof(DWORD));           /* length of value data         */
  PERR(bSuccess == ERROR_SUCCESS, "RegSetValueEx");
  RegCloseKey(hk);
  return;
}

/*********************************************************************
* FUNCTION: reportAnEvent(DWORD dwIdEvent, WORD cStrings,            *
*                         LPTSTR *ppszStrings);                      *
*                                                                    *
* PURPOSE: add the event to the event log                            *
*                                                                    *
* INPUT: the event ID to report in the log, the number of insert     *
*        strings, and an array of null-terminated insert strings     *
*                                                                    *
* RETURNS: none                                                      *
*********************************************************************/

void reportAnIEvent(DWORD dwIdEvent, WORD cStrings, LPTSTR *pszStrings)
{
  HANDLE hAppLog;
  BOOL bSuccess;

  /* Get a handle to the Application event log */
  hAppLog = RegisterEventSource(NULL,   /* use local machine      */
      "NTP");                   /* source name                 */
  PERR(hAppLog, "RegisterEventSource");

  /* Now report the event, which will add this event to the event log */
  bSuccess = ReportEvent(hAppLog,       /* event-log handle            */
      EVENTLOG_INFORMATION_TYPE,      /* event type                  */
      0,                        /* category zero               */
      dwIdEvent,                /* event ID                    */
      NULL,                     /* no user SID                 */
      cStrings,                 /* number of substitution strings     */
      0,                        /* no binary data              */
      pszStrings,               /* string array                */
      NULL);                    /* address of data             */
  PERR(bSuccess, "ReportEvent");
  DeregisterEventSource(hAppLog);
  return;
}

void reportAnWEvent(DWORD dwIdEvent, WORD cStrings, LPTSTR *pszStrings)
{
  HANDLE hAppLog;
  BOOL bSuccess;

  /* Get a handle to the Application event log */
  hAppLog = RegisterEventSource(NULL,   /* use local machine      */
      "NTP");                   /* source name                 */
  PERR(hAppLog, "RegisterEventSource");

  /* Now report the event, which will add this event to the event log */
  bSuccess = ReportEvent(hAppLog,       /* event-log handle            */
      EVENTLOG_WARNING_TYPE,      /* event type                  */
      0,                        /* category zero               */
      dwIdEvent,                /* event ID                    */
      NULL,                     /* no user SID                 */
      cStrings,                 /* number of substitution strings     */
      0,                        /* no binary data              */
      pszStrings,               /* string array                */
      NULL);                    /* address of data             */
  PERR(bSuccess, "ReportEvent");
  DeregisterEventSource(hAppLog);
  return;
}

void reportAnEEvent(DWORD dwIdEvent, WORD cStrings, LPTSTR *pszStrings)
{
  HANDLE hAppLog;
  BOOL bSuccess;

  /* Get a handle to the Application event log */
  hAppLog = RegisterEventSource(NULL,   /* use local machine      */
      "NTP");                   /* source name                 */
  PERR(hAppLog, "RegisterEventSource");

  /* Now report the event, which will add this event to the event log */
  bSuccess = ReportEvent(hAppLog,       /* event-log handle            */
      EVENTLOG_ERROR_TYPE,      /* event type                  */
      0,                        /* category zero               */
      dwIdEvent,                /* event ID                    */
      NULL,                     /* no user SID                 */
      cStrings,                 /* number of substitution strings     */
      0,                        /* no binary data              */
      pszStrings,               /* string array                */
      NULL);                    /* address of data             */
  PERR(bSuccess, "ReportEvent");
  DeregisterEventSource(hAppLog);
  return;
}
