/*
 *	@doc HESIOD
 *
 * @module hesservb.c |
 *
 *
 *	  Contains the definition for hes_getservbyname,
 *
 *	  WSHelper DNS/Hesiod Library for WINSOCK
 *
 */

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getservbyname.c	5.3 (Berkeley) 5/19/86";
#endif /* LIBC_SCCS and not lint */

#include <hesiod.h>
#include <windows.h>
#include <winsock.h>
#include <windns.h>

#include <string.h>

#include <stdio.h>
#include <ctype.h>

#define cistrcmp stricmp

#define LISTSIZE 15


/*
	This function will query a Hesiod server for a servent structure given
	a service name and protocol. This is a replacement for the Winsock
	getservbyname function which normally just uses a local services
	file. This allows a site to use a centralized database for adding new
	services.

	defined in hesservb.c

	\param[in]	name	pointer to the official name of the service, eg "POP3".
	\param[in]	proto	pointer to the protocol to use when contacting the service, e.g. "TCP"

	\retval				NULL if there was an error or a pointer to a servent structure. The caller must
						never attempt to modify this structure or to free any of its components.
						Furthermore, only one copy of this structure is allocated per call per thread, so the application should copy any information it needs before
						issuing another hes_getservbyname call

*/

extern DWORD dwHesServIndex;
struct servent *
WINAPI
hes_getservbyname(char *name, char *proto)
{
    struct servent *p;
    register char **cp;
    register char** hesinfo;
    register int i = 0;

    char buf[DNS_MAX_NAME_BUFFER_LENGTH];
    char* l;

    hesinfo = hes_resolve(name, "service");
    cp = hesinfo;
    if (cp == NULL)
	return(NULL);
    p = (struct servent*)(TlsGetValue(dwHesServIndex));
    if (p == NULL) {
	LPVOID lpvData = (LPVOID) LocalAlloc(LPTR, sizeof(struct servent));
	if (lpvData != NULL) {
	    TlsSetValue(dwHesServIndex, lpvData);
	    p = (struct servent*)lpvData;
	} else
	    return NULL;
    }
    if (!p->s_name)
        p->s_name = LocalAlloc(LPTR, DNS_MAX_LABEL_BUFFER_LENGTH);
    if (!p->s_proto)
        p->s_proto = LocalAlloc(LPTR, DNS_MAX_LABEL_BUFFER_LENGTH);
    if (!p->s_aliases)
        p->s_aliases = LocalAlloc(LPTR, LISTSIZE*sizeof(LPSTR));

    for (;*cp; cp++) {
	register char *servicename, *protoname, *port;
        strcpy(buf, *cp);
        l = buf;
	while(*l && (*l == ' ' || *l == '\t')) l++;
	servicename = l;
	while(*l && *l != ' ' && *l != '\t' && *l != ';') l++;
	if (*l == '\0') continue; /* malformed entry */
	*l++ = '\0';
	while(*l && (*l == ' ' || *l == '\t')) l++;
	protoname = l;
	while(*l && *l != ' ' && *l != ';') l++;
	if (*l == '\0') continue; /* malformed entry */
	*l++ = '\0';
	if (cistrcmp(proto, protoname)) continue; /* wrong port */
	while(*l && (*l == ' ' || *l == '\t' || *l == ';')) l++;
	if (*l == '\0') continue; /* malformed entry */
	port = l;
	while(*l && (*l != ' ' && *l != '\t' && *l != ';')) l++;
	if (*l) *l++ = '\0';
	if (*l != '\0') {
	    do {
		char* tmp = l;
		while(*l && !isspace(*l)) l++;
		if (*l) *l++ = 0;
                if (p->s_aliases[i])
                    p->s_aliases[i] = LocalAlloc(LPTR, strlen(tmp));
                strcpy(p->s_aliases[i], tmp);
                i++;
	    } while(*l);
	}
	p->s_aliases[i] = NULL;
        for (; i<LISTSIZE; i++)
        {
            if (p->s_aliases[i]){
                LocalFree(p->s_aliases[i]);
                p->s_aliases[i] = NULL;
            }
        }
	strcpy(p->s_name, servicename);
	p->s_port = htons((u_short)atoi(port));
	strcpy(p->s_proto, protoname);
        if (hesinfo)
            hes_free(hesinfo);
	return (p);
    }
    return(NULL);
}
