/*
 * @doc RESOLVE
 *
 * @module res_init.c |
 *
 * Contains the implementation for res_init, res_getopts, res_setopts
 * and supplementary internal functions. If you are adding support for a
 * new TCP/IP stack of resolver configuration information this is where
 * it will go.
 * @xref <f res_init> <f res_setopts> <f res_getopts> <f WhichOS> <f getRegKey>
 *
 * WSHelper DNS/Hesiod Library for WINSOCK
 *
 */

/*-
 * Copyright (c) 1985, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_init.c  6.15 (Berkeley) 2/24/91";
#endif /* LIBC_SCCS and not lint */

#include <windows.h>
#include <winsock.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windns.h>   //DNS api's

#include <shellapi.h>


#include <mitwhich.h>

#include "resource.h"

char debstr[80];

#define index strchr

#ifndef MAKELONG
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#endif

#define TCPIP_PATH "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters"
#define HKEY_MIT_PRIVATE HKEY_CLASSES_ROOT
#define WSH_MIT_PRIVATE_DOMAIN_SUBKEY TCPIP_PATH"\\Domain"
#define WSH_MIT_PRIVATE_NAMESERVER_SUBKEY TCPIP_PATH"\\NameServer"

DWORD WhichOS( DWORD *check);

static int const getRegKeyEx(const HKEY key, const char *subkey, const char *value, char *buf, unsigned int len);

int WINAPI wsh_getdomainname(char* name, int size);

static HMODULE this_module();


/*
 * Resolver state default settings
 */
// @struct _res | a structure of this type holds the state information for the
// resolver options
struct state _res = {
    RES_TIMEOUT,                    /* @field retransmition time interval */
    4,                              /* @field number of times to retransmit */
    RES_DEFAULT,                    /* @field options flags */
    1,                              /* @field number of name servers */
};

#ifndef _MSC_VER

#define _upcase(c) (((c) <= 'Z' && (c) >= 'A') ? (c) + 'a' - 'A' : (c))
#define _chricmp(a, b) (_upcase(a) - _upcase(b))

int
#ifdef __cplusplus
inline
#endif
_strnicmp( register const char *a, register const char *b, register size_t n)
{
    register int cmp = 0; /* equal */
    while( n-- && !(cmp = _chricmp(*a, *b)) && (a++, *b++) /* *a == *b anyways */ );
    return cmp;
};

#endif


/*
	This function retrieves the default domain name and search order. It will look to see if an
	environment variable LOCALDOMAIN is defined. Otherwise, the domain associated with the local host
	is used. Otherwise, it will try to find the domain name from the registry

	\retval		The return value is 0 if the operation was successful.
				Otherwise the value -1 is returned.

*/
int
WINAPI
res_init()
{
    register char *cp, **pp;

    register int n;

    int haveenv = 0;	/* have an environment variable for local domain */
    int havedomain = 0; /* 0 or 1 do we have a value for the domain */

    LONG result1 = -1995;

#define WSH_SPACES " \t,;="

    _res.nsaddr.sin_addr.s_addr = INADDR_ANY;
    _res.nsaddr.sin_family = AF_INET;
    _res.nsaddr.sin_port = htons(NAMESERVER_PORT);
    _res.nscount = 1;


    /* Allow user to override the local domain definition */
    if ((cp = getenv("LOCALDOMAIN")) != NULL) {
        strncpy(_res.defdname, cp, sizeof(_res.defdname));
        haveenv++;
        havedomain++;
    };

    if (!havedomain) {
        if (!wsh_getdomainname(_res.defdname, sizeof(_res.defdname)))
            havedomain++;
    }



    if( 0 != havedomain){
        // return early, we've done our job
        /* find components of local domain that might be searched */

            pp = _res.dnsrch;
            *pp++ = _res.defdname;
            for (cp = _res.defdname, n = 0; *cp; cp++)
                if (*cp == '.')
                    n++;
            cp = _res.defdname;
            for (; n >= LOCALDOMAINPARTS && pp < _res.dnsrch + MAXDFLSRCH;
                 n--) {
                cp = index(cp, '.');
                *pp++ = ++cp;
            }
            *pp++ = 0;
    }

   _res.options |= RES_INIT;
        return(0);
}


/*
 res_setopts -- unsupported
*/

void
WINAPI
res_setopts(long opts)
{
}



/*
	res_getopts -- unsupported
*/

long
WINAPI
res_getopts()
{
    return -1;
}

/* --------------------------------------------------------------------------*/
/* Excerpt from IPTYPES.H */
#define MAX_HOSTNAME_LEN                128 // arb.
#define MAX_DOMAIN_NAME_LEN             128 // arb.
#define MAX_SCOPE_ID_LEN                256 // arb.



/*

  @doc MISC

  @func DWORD | WhichOS | This function will attempt to
  determine which Operating System and subsystem is being used by the
  application. It should function under Win16, Windows NT amd Windows
  95 at least.  It does call WSAStartup() and WSACleanup(). This
  function does have side effects on some global variables.  See the
  comments below.

  @parm DWORD *| check | a pointer to a DWORD, a value indicating
  which operating system and/or subsystem is being used will be stored
  in this parameter upon return.

  @rdesc a NULL will indicate that we could not determine what OS is
  being used. The high word contains:


  @flag MS_OS_WIN     (1) | The application is running under Windows or WFWG
  @flag	MS_OS_95      (2) | The application is running under Windows 95
  @flag	MS_OS_NT      (3) | The application is running under Windows NT
  @flag	MS_OS_2000    (4) | The application is running under Windows 2000
  @flag	MS_OS_XP      (5) | The application is running under Windows XP
  @flag	MS_OS_2003    (6) | The application is running under Windows 2003
  @flag	MS_OS_NT_UNKNOWN (7) | The application is running under Windows NT family beyond 2003
  @flag	MS_OS_UNKNOWN (0) | It looks like Windows but not any version that
                            we know of.

  <nl>these are defined in mitwhich.h<nl>

The low word contains one of the following, which is derived from the winsock implementation: <nl>

  @flag MS_NT_32 (1) | The MS 32 bit Winsock stack for NT is being used
  @flag MS_NT_16 (2) | The MS 16 bit Winsock stack under NT is being used
  @flag	MS_95_32 (3) | The MS 32 bit Winsock stack under 95 is being used
  @flag	MS_95_16 (4) | The MS 16 bit Winsock stack under 95 is being used
  @flag	NOVELL_LWP_16       (5)  | The Novell 16 Winsock stack is being used
  @flag UNKNOWN_16_UNDER_32 (-2) | We don't know the stack.
  @flag UNKNOWN_16_UNDER_16 (-3) | We don't know the stack.
  @flag UNKNOWN_32_UNDER_32 (-4) | We don't know the stack.
  @flag UNKNOWN_32_UNDER_16 (-5) | We don't know the stack.

*/
DWORD
WhichOS(
    DWORD *check
    )
{
    WORD wVersionRequested;
    WSADATA wsaData; // should be a global?
    int err;

    int checkStack = 0;
    int checkOS = 0;
    static DWORD dwCheck = 0xFFFFFFFF;

    if ( dwCheck != 0xFFFFFFFF ) {
        if ( check )
            *check = dwCheck;
        return dwCheck;
    }

    // first get the information from WSAStartup because it may give
    // more consistent information than Microsoft APIs.

    wVersionRequested = 0x0101;

    err = WSAStartup( wVersionRequested, &wsaData );

    if( err != 0 ){
        MessageBox( NULL,
                    "It looks like a useable winsock.dll\n"
                    "could not be located by the wshelp*.dll\n"
                    "Please check your system configuration.",
                    "Problem in wshelper.dll", MB_OK );
        check = 0;
        return(0);
    }

    WSACleanup();

    if( _res.options & RES_DEBUG ){
        wsprintf( debstr, wsaData.szDescription );
        OutputDebugString( debstr );
    }

    if( (0 == checkStack) && (0 == stricmp( wsaData.szDescription, NT_32 ))){
        // OK we appear to be running under NT in the 32 bit subsystem
        // so we must be a 32 bit application.
        // This also implies that we can get the TCPIP parameters out
        // of the NT registry.
        checkStack = MS_NT_32;
    }

    if( (0 == checkStack) && (0 == stricmp( wsaData.szDescription, NT_16 ))){
        // this implies we're running under NT in the 16 bit subsystem
        // so we must be a 16 bit application
        // This means we have to go through some strange gyrations to read the
        // TCPIP parameters out of the NT 32 bit registry.
        checkStack = MS_NT_16;
        checkOS = MS_OS_NT;
    }

    if( (0 == checkStack) && (0 == stricmp( wsaData.szDescription, W95_32 ))){
	// get the TCPIP parameters out of the Win95 registry
        checkStack = MS_95_32;
        checkOS = MS_OS_95; // ??
    }

    if( (0 == checkStack) && (0 == stricmp( wsaData.szDescription, W95_16 ))){
        // go through the pain of getting the TCPIP parameters out of the Win95
        // 32 bit registry
        checkStack = MS_95_16;
        checkOS = MS_OS_95;
    }

    if( (0 == checkStack) && (0 == stricmp( wsaData.szDescription, LWP_16 ))){
        // get the information out of the %NDIR%\TCP\RESOLV.CFG file
        checkStack = NOVELL_LWP_16;
        checkOS = MS_OS_WIN;
    }

    if( 0 == checkStack ){
        // at this time we don't easily know how to support this stack
        checkStack = STACK_UNKNOWN;
    }

#if !defined(_WIN32)
    // Note, if this is the 32 bit DLL we can't use the following
    // functions to determine the OS because they are
    // obsolete. However, we should be able to use them in the 16 bit
    // DLL.
    {
        DWORD dwVersion = 0;
        DWORD dwFlags = 0;

        dwFlags = GetWinFlags();
        if( _res.options & RES_DEBUG ){
            wsprintf( debstr, "dwFlags = %x ", dwFlags );
            OutputDebugString( debstr );
        }

        dwVersion = GetVersion();

        if( _res.options & RES_DEBUG ){
            wsprintf( debstr, "dwVersion = %8lx ", dwVersion );
            OutputDebugString( debstr );
        }

        if( 95 == (DWORD)(HIBYTE(LOWORD(dwVersion))) ){
            // OK, we're a 16 bit app running on 95?
            checkOS = MS_OS_95;
        }

        if( dwFlags & 0x4000 ){
            // This means that this is a 16 bit application running
            // under WOW layer on NT.

            // So, we're going to get the TCPIP parameters out of the
            // 32 bit registry, but we don't know which set of
            // registry entries yet.

            // Since we see these version numbers and we're under WOW
            // we must be under NT 4.0 but we don't necessarily know
            // the stack
            checkOS = MS_OS_NT;
        }


        if( checkOS == 0 ){
            // We are a 16 bit application running on a 16 bit operating system
            checkOS = MS_OS_WIN; // assumption, but we're not under 95 and not under NT, it looks like
            if( checkStack == STACK_UNKNOWN ){
                checkStack = UNKNOWN_16_UNDER_16;
            }
        }
    }
#endif // !_WIN32

#if defined(_WIN32)
    // This must be a 32 bit application so we are either under NT,
    // Win95, or WIN32s
    {
        OSVERSIONINFO osvi;

        memset( &osvi, 0, sizeof(OSVERSIONINFO));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx( &osvi );

        if( osvi.dwPlatformId == VER_PLATFORM_WIN32s ){
            if( checkStack == STACK_UNKNOWN ){
                checkStack = UNKNOWN_16_UNDER_16;
            }
            checkOS = MS_OS_WIN;
            wsprintf( debstr, "Microsoft Win32s %d.%d (Build %d)\n",
                      osvi.dwMajorVersion,
                      osvi.dwMinorVersion,
                      osvi.dwBuildNumber & 0xFFFF );
        }

        if( osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS ){
            if( checkStack == STACK_UNKNOWN ){
                checkStack = UNKNOWN_32_UNDER_32;
            }
            checkOS = MS_OS_95;
            wsprintf( debstr, "Microsoft Windows 95 %d.%d (Build %d)\n",
                      osvi.dwMajorVersion,
                      osvi.dwMinorVersion,
                      osvi.dwBuildNumber & 0xFFFF );
        }

        if( osvi.dwPlatformId == VER_PLATFORM_WIN32_NT ){
            if( checkStack == STACK_UNKNOWN ){
                checkStack = UNKNOWN_32_UNDER_32;
            }
            if ( osvi.dwMajorVersion <= 4 )
                checkOS = MS_OS_NT;
            else if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
                checkOS = MS_OS_2000;
            else if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
                checkOS = MS_OS_XP;
            else if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
                checkOS = MS_OS_2003;
            else
                checkOS = MS_OS_NT_UNKNOWN;
            wsprintf( debstr, "Microsoft Windows NT family %d.%d (Build %d)\n",
                      osvi.dwMajorVersion,
                      osvi.dwMinorVersion,
                      osvi.dwBuildNumber & 0xFFFF );
        }

        if( _res.options & RES_DEBUG ){
            OutputDebugString( debstr );
        }
    }

#endif // _WIN32

    // At this point we should know the OS.
    // We should also know the subsystem but not always the stack.

    dwCheck = MAKELONG(checkOS, checkStack);
    if ( check )
        *check = dwCheck;
    return( dwCheck );
}


static
BOOL
get_nt5_adapter_param(
    char* param,
    WORD skip,
    char* buf,
    unsigned int   len
    )
{
    static char linkage[BUFSIZ*4];
    char* p;
    char* q;
    HKEY hAdapters;

    char* DEVICE_STR = "\\Device\\";
    SIZE_T DEVICE_LEN = strlen(DEVICE_STR);

#define TCPIP_PATH_ADAPTERS "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters\\Interfaces"
#define TCPIP_PATH_LINKAGE "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Linkage"

    if (!getRegKeyEx(HKEY_LOCAL_MACHINE, TCPIP_PATH_LINKAGE, "Bind", linkage, sizeof(linkage)))
        return FALSE;

    p = linkage;

    RegOpenKeyEx(HKEY_LOCAL_MACHINE, TCPIP_PATH_ADAPTERS, 0,
                 KEY_QUERY_VALUE | KEY_ENUMERATE_SUB_KEYS,
                 &hAdapters);

    while (*p) {
        q = strstr(p, DEVICE_STR);
        if (!q) {
            while (*p) p++;
            p++;
            continue;
        }
        q += DEVICE_LEN;
        p = q;
        while (*p) p++;
        p++;
        buf[0] = '\0';
        if (getRegKeyEx(hAdapters, q, param, buf, len) && !buf[0]) {
            if (!skip) {
                RegCloseKey(hAdapters);
                return TRUE;
            }
            else
                skip--;
        }
    }
    RegCloseKey(hAdapters);

    // Bottom out by looking at default parameters
    {
        char Tcpip_path[_MAX_PATH];

        if(!LoadString(this_module(), IDS_TCPIP_PATH_NT,
                       Tcpip_path, sizeof(Tcpip_path)))
            strcpy(Tcpip_path, NT_TCP_PATH);
        return getRegKeyEx(HKEY_LOCAL_MACHINE, Tcpip_path, param, buf, len);
    }
    return FALSE;
}



static
BOOL
_getdomainname(
    char* name,
    int size
    )
{
    char buf[BUFSIZ];

    char* dhcp_param = "DhcpDomain";
    char* param = "Domain";
    BOOL ok = FALSE;
    char* rbuf;
    unsigned int rlen;

    if (!name || (size <= 0))
        return FALSE;

    rbuf = (size >= sizeof(buf))?name:buf;
    rlen = (size >= sizeof(buf))?size:sizeof(buf);


    ok = get_nt5_adapter_param(dhcp_param, 0, rbuf, rlen);
    if (!ok || !rbuf[0])
        ok = get_nt5_adapter_param(param, 0, rbuf, rlen);

    if (ok && rbuf[0]) {
        if (size < (lstrlen(rbuf) + 1))
            return FALSE;
        if (rbuf != name)
            strncpy(name, rbuf, size);
        return TRUE;
    }
    return FALSE;
}

/*
	Gets the base part of the hostname
	defined in wshelper\res_init.c

	\param[in, out]	name pointer to a buffer that receives a null-terminated string containing the computer name
	\param[in]		size specifies the size of the buffer, in chars (must be large
                    enough to hold NULL-terminated host name)

	\retval			return 0 ifsuccess,  -1 on error.

*/
int WINAPI
wsh_gethostname(char* name, int size)
{
    if (name){
       // Get and display the name of the computer.

        if( GetComputerName(name, &size) )
        {
            while (*name && (*name != '.'))
            {
                *name = tolower(*name);
                name++;
            }
            if (*name == '.') *name = 0;
                return 0;
        }
    }
    return -1;
}

/*
	Gets the machine's domain name

	\param[in, out]	name pointer to a buffer that receives a null-terminated string containing the domain name
	\param[in]		size specifies the size of the buffer, in chars (must be large
                    enough to hold NULL-terminated domain name)

	\retval			return 0 ifsuccess,  -1 on error.


*/
int WINAPI
wsh_getdomainname(char* name, int size)
{
    DNS_STATUS status;

    PDNS_RECORD pDnsRecord;
    DNS_FREE_TYPE freetype ;

    DWORD length;
    char hostName[BUFSIZ];

    length = BUFSIZ;
    freetype =  DnsFreeRecordListDeep;


   // Get and display the name of the computer.

   if( GetComputerName(hostName, &length) )
   {

        status = DnsQuery_A(hostName,                 //pointer to OwnerName
                        DNS_TYPE_A,                      //Type of the record to be queried
                        DNS_QUERY_BYPASS_CACHE|DNS_QUERY_NO_LOCAL_NAME,     // Bypasses the resolver cache on the lookup.
                        NULL,                   //contains DNS server IP address
                        &pDnsRecord,                //Resource record comprising the response
                        NULL);                     //reserved for future use

        if (status)
            return -1;
        else
        {
            char* cp;
            cp = index(pDnsRecord->pName, '.');
            if (cp)
            {
                cp++;
                strncpy(name, cp, size);
                name[size-1] = '\0';
                DnsRecordListFree(pDnsRecord, freetype);
                return(0);
            }
            DnsRecordListFree(pDnsRecord, freetype);

        }
   }

    /* try to get local domain from the registry */
    if (_getdomainname(name, size))
        return 0;
    else
        return -1;
}








// @func int | getRegKeyEx | This function is only used when the library is
//                           running under a known 32-bit Microsoft Operating
//                           system

// @parm const HKEY | key | Specifies a a currently open key or any
//  of the following predefined reserved handle values:
//	HKEY_CLASSES_ROOT
//	KEY_CURRENT_USER
//	HKEY_LOCAL_MACHINE
//	HKEY_USERS
//
// @parm const char * | subkey | Specifies a pointer to a null-terminated
//  string containing the name of the subkey to open. If this parameter is NULL
//  or a pointer to an empty string, the function will open a new handle
//  of the key identified by the key parameter.
//
// @parm const char * | value | Specifiea a pointer to a null-terminated
//  string containing the name of the value to be queried.
//
// @parm char * | buf | Specifies a pointer to a buffer that recieves the
//  key's data. This parameter can be NULL if the data is not required.
//
// @parm unsigned int | len | Specifies the size of buffer 'buf'.
//
// @rdesc Returns an int  that can mean:
//
// FALSE - if the subkey cannot be queried or possibly opened.
// TRUE  - if the subkey can be queried but it is not of type: REG_EXPAND_SZ
// If the subkey can be queried, and its type is REG_EXPAND_SZ, and it can
// be expanded the return value is the number of characters stored in the
// buf parameter. If the number of characters is greater than the size of the
// of the destination buffer, the return value should be the size of the
// buffer required to hold the value.

static
int const
getRegKeyEx(
    const HKEY key,
    const char *subkey,
    const char *value,
    char *buf,
    unsigned int len
    )
{
    HKEY hkTcpipParameters;
    LONG err;
    DWORD type, cb;
    char *env_buf;


    if (RegOpenKey(key, subkey, &hkTcpipParameters) == ERROR_SUCCESS) {
        cb = len;
        err = RegQueryValueEx(hkTcpipParameters, value, 0, &type, buf, &cb);
        RegCloseKey(hkTcpipParameters);
        if( err == ERROR_SUCCESS ){
            if( type == REG_EXPAND_SZ ){
                if( env_buf = malloc( cb ) ){
                    err = ExpandEnvironmentStrings( strcpy( env_buf, buf ), buf, len );
                    free( env_buf );
                    return err;
                } else {
                    return FALSE;
                }
            }
            return TRUE; // subkey could be queried but it was not of type REG_EXPAND_SZ
        } else {
            return FALSE; // subkey exists but could not be queried
        }
    }
    else

// #endif // WIN32

        return FALSE; // subkey could not be opened
}

#ifdef __cplusplus
inline
#endif

#include "wsh-int.h"

static
HMODULE
this_module()
{
    static HMODULE hModWSHelp = 0;
    if (!hModWSHelp)
    {
        // Note: these must match the DEF file entries
#if defined(_WIN64)
        hModWSHelp = GetModuleHandle( "WSHELP64" );
#else
        hModWSHelp = GetModuleHandle( "WSHELP32" );
#endif
    }
    return hModWSHelp;
}

static
int
try_registry(
    HKEY  hBaseKey,
    const char * name,
    DWORD * value
    )
{
    HKEY hKey;
    LONG err;
    DWORD size;

    err = RegOpenKeyEx(hBaseKey,
                       "Software\\MIT\\WsHelper",
                       0,
                       KEY_QUERY_VALUE,
                       &hKey);
    if (err)
        return 0;
    size = sizeof(value);
    err = RegQueryValueEx(hKey, name, 0, 0, value, &size);
    RegCloseKey(hKey);
    return !err;
}

void
res_init_startup()
{
    DWORD debug_on = 0;


    if (try_registry(HKEY_CURRENT_USER, "DebugOn", &debug_on) ||
        try_registry(HKEY_LOCAL_MACHINE, "DebugOn", &debug_on))
    {
        if (debug_on)
            _res.options |= RES_DEBUG;
    }
}

void
res_init_cleanup()
{

}
