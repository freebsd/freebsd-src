/* timesync stuff for leash - 7/28/94 - evanr */

#include <windows.h>
#include "leashdll.h"

#include <time.h>
#include <sys\timeb.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>

#include <stdio.h>
#include "leasherr.h"
#include "leashids.h"

int ProcessTimeSync(char *, int, char *);

#define TM_OFFSET 2208988800

/* timezone.h has a winsock.h conflict */
struct timezone {
    int     tz_minuteswest;
    int     tz_dsttime;
};

/************************************/
/* settimeofday():                  */
/************************************/
int
settimeofday(
    struct timeval *tv,
    struct timezone *tz
    )
{
    SYSTEMTIME systime;
    struct tm *newtime;

    newtime = gmtime((time_t *)&(tv->tv_sec));
    systime.wYear = 1900+newtime->tm_year;
    systime.wMonth = 1+newtime->tm_mon;
    systime.wDay = newtime->tm_mday;
    systime.wHour = newtime->tm_hour;
    systime.wMinute = newtime->tm_min;
    systime.wSecond = newtime->tm_sec;
    systime.wMilliseconds = 0;
    return SetSystemTime(&systime);
}

/************************************/
/* gettimeofday():                  */
/************************************/
int
gettimeofday(
    struct timeval *tv,
    struct timezone *tz
    )
{
    struct _timeb tb;
    _tzset();
    _ftime(&tb);
    if (tv) {
	tv->tv_sec = tb.time;
	tv->tv_usec = tb.millitm * 1000;
    }
    if (tz) {
	tz->tz_minuteswest = tb.timezone;
	tz->tz_dsttime = tb.dstflag;
    }
    return 0;
}


LONG
get_time_server_name(
    char *timeServerName,
    const char *valueName
    )
{
    HMODULE     hmLeash;
    char        hostname[128];
    char        value[80];
    DWORD       dwType;
    DWORD       dwCount;
    int         check = 0;
    HKEY    	hKey;
    HKEY        rKey1;
    HKEY        rKey2;
    LONG        lResult;
    BOOL 	bEnv;

    memset(value, '\0', sizeof(value));
    memset(hostname, '\0', sizeof(hostname));

    GetEnvironmentVariable("TIMEHOST", hostname, sizeof(hostname));
    bEnv = (GetLastError() == ERROR_ENVVAR_NOT_FOUND);

    if (!(bEnv && hostname[0]))
    {
        // Check registry for TIMEHOST
        rKey1 = HKEY_CURRENT_USER;
        rKey2 = HKEY_LOCAL_MACHINE;

        for (check = 0; check < 2; check++)
        {
            if (ERROR_SUCCESS == RegOpenKeyEx(check == 0 ? rKey1 : rKey2,
                                              "Software\\MIT\\Leash32\\Settings",
                                              0, KEY_QUERY_VALUE, &hKey))
            {
                memset(value, '\0', sizeof(value));
                lResult = RegQueryValueEx(hKey, (LPTSTR)valueName, NULL,
                                          &dwType, NULL, &dwCount);
                if (lResult == ERROR_SUCCESS)
                {
                    lResult = RegQueryValueEx(hKey, (LPTSTR)valueName,
                                              NULL, &dwType,
                                              (LPTSTR)value, &dwCount);
                    if (lResult == ERROR_SUCCESS && *value)
                    {
                        // found
                        strcpy(hostname, value);
                        break;
                    }
                }
            }
        }

        if (!*hostname)
        {
            // Check resource string for TIMEHOST
            if ((hmLeash = GetModuleHandle(LEASH_DLL)) != NULL)
            {
                if (!LoadString(hmLeash, LSH_TIME_HOST, hostname,
                                sizeof(hostname)))
                    memset(hostname, '\0', sizeof(hostname));
            }
        }
        if (!*hostname)
        {
            // OK, _Default_ it will be! :)
            strcpy(hostname, "time");
        }
    }
    strcpy(timeServerName, hostname);
    return 0;
}

/************************************/
/* Leash_timesync():                */
/************************************/
LONG Leash_timesync(int MessageP)
{
    char                tmpstr[2048];
    char                hostname[128];
    int                 Port;
    int                 rc;
    struct servent      *sp;
    WORD                wVersionRequested;
    WSADATA             wsaData;
    char                name[80];

    if (pkrb5_init_context == NULL)
        return(0);

    wVersionRequested = 0x0101;
    memset(name, '\0', sizeof(name));
    memset(hostname, '\0', sizeof(hostname));
    memset(tmpstr, '\0', sizeof(tmpstr));

    if ((rc = WSAStartup(wVersionRequested, &wsaData)))
    {
        wsprintf(tmpstr, "Couldn't initialize WinSock to synchronize time\n\rError Number: %d", rc);
        WSACleanup();
        return(LSH_BADWINSOCK);
    }

    sp = getservbyname("time", "udp");
    if (sp == 0)
        Port = htons(IPPORT_TIMESERVER);
    else
        Port = sp->s_port;

    get_time_server_name(hostname, TIMEHOST);

    rc = ProcessTimeSync(hostname, Port, tmpstr);

#ifdef USE_MESSAGE_BOX
    if(MessageP != 0)
    {
        if (rc && !*tmpstr)
        {
            strcpy(tmpstr, "Unable to synchronize time!\n\n");
            if (*hostname)
            {
                char                tmpstr1[2048];

                memset(tmpstr1, '\0', sizeof(tmpstr1));
                sprintf(tmpstr1, "Unreachable server: %s\n", hostname);
                strcat(tmpstr, tmpstr1);
            }
        }

	MessageBox(NULL, tmpstr, "Time Server",
                   MB_ICONERROR | MB_OK);
    }
#endif /* USE_MESSAGE_BOX */
    WSACleanup();
    return(rc);
}


/************************************/
/* ProcessTimeSync():               */
/************************************/
int ProcessTimeSync(char *hostname, int Port, char *tmpstr)
{
    char                buffer[512];
    int                 cc;
    long                *nettime;
    int                 s;
    long                hosttime;
    struct hostent      *host;
    struct              timeval tv;
    struct              timezone tz;
    u_long              argp;
    struct sockaddr_in  sin;
    int                 i;

    if ((host = gethostbyname(hostname)) == NULL)
        return(LSH_BADTIMESERV);

    sin.sin_port = (short)Port;
    sin.sin_family = host->h_addrtype;
    memcpy((struct sockaddr *)&sin.sin_addr, host->h_addr, host->h_length);
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        return(LSH_NOSOCKET);
    }

    argp = 1;
    if (ioctlsocket(s, FIONBIO, &argp) != 0)
    {
        closesocket(s);
        return(LSH_NOCONNECT);
    }

    if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        closesocket(s);
        return(LSH_NOCONNECT);
    }
    send(s, buffer, 40, 0);
    if (gettimeofday (&tv, &tz) < 0)
    {
        closesocket(s);
        return(LSH_GETTIMEOFDAY);
    }

    for (i = 0; i < 4; i++)
    {
        if ((cc = recv(s, buffer, 512, 0)) > 0)
            break;
        Sleep(500);
    }
    if (i == 4)
    {
        closesocket(s);
        return(LSH_RECVTIME);
    }

    if (cc != 4)
    {
        closesocket(s);
        return(LSH_RECVBYTES);
    }

    nettime = (long *)buffer;
    hosttime = (long) ntohl (*nettime) - TM_OFFSET;
    (&tv)->tv_sec = hosttime;
    if (settimeofday(&tv, &tz) < 0)
    {
        closesocket(s);
        return(LSH_SETTIMEOFDAY);
    }

    sprintf(tmpstr, "The time has been synchronized with the server:   %s\n\n", hostname);
    strcat(tmpstr, "To be able to use the Kerberos server, it was necessary to \nset the system time to:  ") ;
    strcat(tmpstr, ctime((time_t *)&hosttime));
    strcat(tmpstr, "\n");
    closesocket(s);
    return(0);
}
