/*
*	@doc RESOLVE
*
*	@module gethna.c  |
*
*	This file contains the function definitions for:
*		rgethostbyname,
*		rgethostbyaddr,
*		rdn_expand,
*		gethinfobyname,
*		getmxbyname,
*		getrecordbyname,
*		rrhost,
*		rgetservbyname,
*	and some other internal functions called by these functions.
*
*
*	WSHelper DNS/Hesiod Library for WINSOCK
*
*/

/*
 * Copyright (c) 1985, 1988 Regents of the University of California.
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
static char sccsid[] = "@(#)gethostnamadr.c	6.48 (Berkeley) 1/10/93";
#endif /* LIBC_SCCS and not lint */

#include <windows.h>
#include <winsock.h>
#include <resolv.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <windns.h>

#ifdef _WIN32
#include <mitwhich.h>
#endif

#define MAXALIASES      35
#define MAXADDRS        35

extern DWORD dwGhnIndex;
extern DWORD dwGhaIndex;

unsigned long WINAPI inet_aton(register const char *, struct in_addr *);


#ifdef _DEBUG
#ifndef DEBUG
#define DEBUG
#endif
#endif


extern int WINAPI hes_error( void );
DNS_STATUS doquery(const char* queryname, struct hostent* host);

/*
	query the dns name space for a host given the host name
	\param[in]	name Pointer to the null-terminated name of the host to resolve. It can be a fully qualified host name such as x.mit.edu
				or it can be a simple host name such as x. If it is a simple host name, the default domain name is
				appended to do the search.
	\retval		a pointer to the structure hostent. a structure allocated by the library. The hostent structure contains
				the results of a successful search for the host specified in the name parameter. The caller must never
				attempt to modify this structure or to free any of its components. Furthermore, only one copy of this
				structure is allocated per call per thread, so the application should copy any information it needs before
				issuing another rgethostbyname.
				NULL if the search has failed

*/
struct hostent *
WINAPI
rgethostbyname(char *name)
{
    struct hostent* host;
    DNS_STATUS status;
    const char *cp;
    char queryname[DNS_MAX_NAME_BUFFER_LENGTH ];
#ifdef DEBUG
    char debstr[80];
#endif
    char** domain;
    struct in_addr host_addr;

    host = (struct hostent*)(TlsGetValue(dwGhnIndex));
    if (host == NULL) {
	LPVOID lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct hostent));
	if (lpvData != NULL) {
	    TlsSetValue(dwGhnIndex, lpvData);
	    host = (struct hostent*)lpvData;
	} else
	    return NULL;
    }

    if (host->h_name == NULL)
	host->h_name = LocalAlloc(LPTR, DNS_MAX_LABEL_BUFFER_LENGTH);
    if (host->h_aliases == NULL)
	host->h_aliases = LocalAlloc(LPTR, 1*sizeof(LPSTR));
    if (host->h_addr_list == NULL)
    {
	host->h_addr_list = LocalAlloc(LPTR, 2*sizeof(LPSTR));
	host->h_addr_list[0] = LocalAlloc(LPTR, DNS_MAX_LABEL_BUFFER_LENGTH);
    }


    /*
     * disallow names consisting only of digits/dots, unless
     * they end in a dot.
     */
    if (isdigit(name[0])) {
        for (cp = name;; ++cp) {
            if (!*cp) {
                if (*--cp == '.')
                    break;
                /*
                 * All-numeric, no dot at the end.
                 * Fake up a hostent as if we'd actually
                 * done a lookup.
                 */
                if (!inet_aton(name, &host_addr)) {
                    return((struct hostent *) NULL);
                }
                strcpy(host->h_name, name);
                host->h_aliases[0] = NULL;
                host->h_addrtype = AF_INET;
                host->h_length = sizeof(u_long);
                memcpy(host->h_addr_list[0], &host_addr, sizeof(host_addr));
				host->h_addr_list[1] = NULL;
                return (host);
            }
            if (!isdigit(*cp) && *cp != '.')
                break;
        }
    }

    strcpy(queryname, name);

    if ((_res.options & RES_INIT) == 0 && res_init() == -1)
        return NULL;
    if (strchr(name, '.') == NULL)
    {
	if (_res.options & RES_DEFNAMES)
	{
	    for (domain = _res.dnsrch; *domain; domain++) {
		strcpy(queryname, name);
		strcat(queryname, ".");
		strcat(queryname, *domain);
		status = doquery(queryname, host);
		if (status == 0)
		    break;
	    }
	}
    }
    else {
	status = doquery(queryname, host);
    }

    if (status) {
#ifdef DEBUG
	if (_res.options & RES_DEBUG)
	{
	    wsprintf(debstr, "res_query failed\n");
	    OutputDebugString(debstr);
	}
#endif
        return  NULL;
    }
    return host;
}


/*
	an internal function used by rgethostbyname that does the actual DnsQuery call and populates the hostent
	structure.

	\param[in]	Name of the owner of the record set being queried
	\param[in, out] populated hostent structure

	\retval		DNS_STATUS value returned by DnsQuery

*/
DNS_STATUS doquery(const char* queryname, struct hostent* host)
{
    DNS_STATUS status;
    PDNS_RECORD pDnsRecord, pDnsIter;
    DNS_FREE_TYPE freetype ;
    struct in_addr host_addr;
    char querynamecp[DNS_MAX_NAME_BUFFER_LENGTH];
    size_t len;

    freetype =  DnsFreeRecordListDeep;
    strcpy(querynamecp, queryname);
    status = DnsQuery_A(queryname,          //pointer to OwnerName
			DNS_TYPE_A,         //Type of the record to be queried
                        DNS_QUERY_STANDARD,
                        NULL,               //contains DNS server IP address
                        &pDnsRecord,        //Resource record comprising the response
                        NULL);              //reserved for future use

    if (status)
	return status;

    /* If the query name includes a trailing separator in order to prevent
     * a local domain search, remove the separator during the file name
     * comparisons. */
    len = strlen(querynamecp);
    if (querynamecp[len-1] == '.')
	querynamecp[len-1] = '\0';

    for (pDnsIter = pDnsRecord; pDnsIter; pDnsIter=pDnsIter->pNext) {
	/* if we get an A record, keep it */
	if (pDnsIter->wType == DNS_TYPE_A && stricmp(querynamecp, pDnsIter->pName)==0)
	    break;

	/* if we get a CNAME, look for a corresponding A record */
	if (pDnsIter->wType == DNS_TYPE_CNAME && stricmp(queryname, pDnsIter->pName)==0) {
	    strcpy(querynamecp, pDnsIter->Data.CNAME.pNameHost);
	}
    }
    if (pDnsIter == NULL)
	return DNS_ERROR_RCODE_NAME_ERROR;

    strcpy(host->h_name, pDnsIter->pName);
    host->h_addrtype = AF_INET;
    host->h_length = sizeof(u_long);
    host->h_aliases[0] = NULL;
    host_addr.S_un.S_addr = (pDnsIter->Data.A.IpAddress);
    memcpy(host->h_addr_list[0], (char*)&host_addr, sizeof(pDnsIter->Data.A.IpAddress));
    host->h_addr_list[1] = NULL;
    DnsRecordListFree(pDnsRecord, freetype);

    return 0;
}


/*
	retrieves the host information corresponding to a network address in the DNS database
	\param[in]	addr Pointer to an address in network byte order
	\param[in]	len  Length of the address, in bytes
	\param[in]  type Type of the address, such as the AF_INET address family type (defined as TCP,
				UDP, and other associated Internet protocols). Address family types and their corresponding
				values are defined in the Winsock2.h header file.
	\retval		returns a pointer to the hostent structure that contains the name and address corresponding
				to the given network address. The structure is allocated by the library.  The caller must never
				attempt to modify this structure or to free any of its components. Furthermore, only one copy of this
				structure is allocated per call per thread, so the application should copy any information it needs before
				issuing another rgethostbyaddr.
				NULL if the search has failed

*/

struct hostent *
WINAPI
rgethostbyaddr(const char *addr, int len, int type)
{
    DNS_STATUS status;
    struct hostent* host;
#ifdef DEBUG
    char debstr[80];
#endif

    PDNS_RECORD pDnsRecord;
    DNS_FREE_TYPE freetype ;
    char qbuf[BUFSIZ];

    if (type != AF_INET)
        return ((struct hostent *) NULL);

    wsprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
         ((unsigned)addr[3] & 0xff),
         ((unsigned)addr[2] & 0xff),
         ((unsigned)addr[1] & 0xff),
         ((unsigned)addr[0] & 0xff));


    freetype =  DnsFreeRecordListDeep;


    status = DnsQuery_A(qbuf,                 //pointer to OwnerName
                        DNS_TYPE_PTR,         //Type of the record to be queried
                        DNS_QUERY_STANDARD,
                        NULL,                   //contains DNS server IP address
                        &pDnsRecord,                //Resource record comprising the response
                        NULL);                     //reserved for future use

    if (status) {
#ifdef DEBUG
        if (_res.options & RES_DEBUG)
        {
            wsprintf(debstr, "res_query failed\n");
            OutputDebugString(debstr);
        }
#endif

        return  NULL;
    }

    host = (struct hostent*)(TlsGetValue(dwGhaIndex));
    if (host == NULL) {
	LPVOID lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct hostent));
	if (lpvData != NULL) {
	    TlsSetValue(dwGhaIndex, lpvData);
	    host = (struct hostent*)lpvData;
	} else
	    return NULL;
    }

    if (host->h_name == NULL)
	host->h_name = LocalAlloc(LPTR, DNS_MAX_LABEL_BUFFER_LENGTH);
    if (host->h_aliases == NULL)
	host->h_aliases = LocalAlloc(LPTR, 1*sizeof(LPSTR));
    if (host->h_addr_list == NULL)
    {
	host->h_addr_list = LocalAlloc(LPTR, 2*sizeof(LPSTR));
	host->h_addr_list[0] = LocalAlloc(LPTR, DNS_MAX_LABEL_BUFFER_LENGTH);
    }

    strcpy(host->h_name, pDnsRecord->Data.Ptr.pNameHost);
    host->h_addrtype = type;
    host->h_length = len;
    host->h_aliases[0] = NULL;
    memcpy(host->h_addr_list[0], addr, sizeof(unsigned long));
    host->h_addr_list[1] = NULL;
    DnsRecordListFree(pDnsRecord, freetype);

    return host;

}


/*

  @doc MISC

  @func LPSTR WINAPI | gethinfobyname | Given the name
  of a host query the nameservers for the T_HINFO information
  associated with the host. unsupported

  @parm LPSTR | name | pointer to the name of the host that the query is about.

  @rdesc NULL or a pointer to the T_HINFO.


*/

LPSTR
WINAPI
gethinfobyname(LPSTR name)
{
    return NULL;

}


/*

  @func struct mxent  * WINAPI | getmxbyname | This
  function will query the nameservers for the MX records associated
  with the given hostname. Note that the return is a pointer to the
  mxent structure so an application making this call can iterate
  through the different records returned and can also reference the
  preference information associated with each hostname returned. unsupported

  @parm LPSTR | name | The name of the host for which we want MX records.

  @rdesc NULL or a pointer to a mxent structure.

 */

struct mxent  *
WINAPI
getmxbyname(LPSTR name)
{
    return NULL;
}


/*

  @func LPSTR WINAPI | getrecordbyname | This function
  will query the nameservers about the given hostname for and DNS
  record type that the application wishes to query. unsupported

  @parm LPSTR | name | a pointer to the hostname

  @parm int | rectype | a DNS record type, e.g. T_MX, T_HINFO, ...

  @rdesc The return is NULL or a pointer to a string containing the
  data returned. It is up to the calling application to parse the
  string appropriately for the rectype queried.

*/

LPSTR
WINAPI
getrecordbyname(LPSTR name, int rectype)
{
    return NULL;
}


/*

  @func DWORD WINAPI | rrhost | This function emulates the
  rhost function that was part of Excelan / Novell's LAN WorkPlace TCP/IP API.
  Given a pointer to an IP hostname it will return the IP address as a 32 bit
  integer.


  @parm LPSTR | lpHost | a pointer to the hostname.

  @rdesc 0 or the IP address as a 32 bit integer.

*/

DWORD WINAPI rrhost( LPSTR lpHost )
{
    return (DWORD) 0;
}


/*
	retrieves service information corresponding to a service name and protocol.

	\param[in]	name Pointer to a null-terminated service name.
	\param[in]  proto pointer to a null-terminated protocol name. getservbyname should match both
				the name and the proto.

	\retval		a pointer to the servent structure containing the name(s) and service number that match the name and proto
				parameters. The structure is allocated by the library.  The caller must never
				attempt to modify this structure or to free any of its components. Furthermore, only one copy of this
				structure is allocated per call per thread, so the application should copy any information it needs before
				issuing another rgetservbyname.
				NULL if the search has failed

*/

struct servent  * WINAPI rgetservbyname(LPCSTR name, LPCSTR proto)
{
    struct servent  * WINAPI hes_getservbyname(LPCSTR name, LPCSTR proto);
    struct servent  *tmpent;

    tmpent = hes_getservbyname(name, proto);
    return (!hes_error()) ? tmpent : getservbyname(name, proto);
}
