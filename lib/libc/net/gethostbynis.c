/*-
 * Copyright (c) 1994, Garrett Wollman
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND
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
static char sccsid[] = "@(#)$Id: gethostbynis.c,v 1.2 1996/03/16 21:25:58 wpaul Exp $";
static char rcsid[] = "$Id: gethostbynis.c,v 1.2 1996/03/16 21:25:58 wpaul Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#define	MAXALIASES	35
#define	MAXADDRS	35

#ifdef YP
static char *host_aliases[MAXALIASES];
static char hostaddr[MAXADDRS];
static char *host_addrs[2];
#endif /* YP */

static struct hostent *
_gethostbynis(name, map)
	char *name, *map;
{
#ifdef YP
	register char *cp, **q;
	char *result;
	int resultlen;
	static struct hostent h;
	static char *domain = (char *)NULL;
	static char ypbuf[YPMAXRECORD];

	if (domain == (char *)NULL)
		if (yp_get_default_domain (&domain))
			return ((struct hostent *)NULL);

	if (yp_match(domain, map, name, strlen(name), &result, &resultlen))
		return ((struct hostent *)NULL);

	/* avoid potential memory leak */
	bcopy((char *)result, (char *)&ypbuf, resultlen);
	free(result);
	result = (char *)&ypbuf;

	if ((cp = index(result, '\n')))
		*cp = '\0';

	cp = strpbrk(result, " \t");
	*cp++ = '\0';
	h.h_addr_list = host_addrs;
	h.h_addr = hostaddr;
	*((u_long *)h.h_addr) = inet_addr(result);
	h.h_length = sizeof(u_long);
	h.h_addrtype = AF_INET;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	h.h_name = cp;
	q = h.h_aliases = host_aliases;
	cp = strpbrk(cp, " \t");
	if (cp != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &host_aliases[MAXALIASES - 1])
			*q++ = cp;
		cp = strpbrk(cp, " \t");
		if (cp != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	return (&h);
#else
	return (NULL);
#endif /* YP */
}

struct hostent *
_gethostbynisname(name)
	char *name;
{
	return _gethostbynis(name, "hosts.byname");
}

struct hostent *
_gethostbynisaddr(addr, len, type)
	char *addr;
	int len;
	int type;
{
	return _gethostbynis(inet_ntoa(*(struct in_addr *)addr),"hosts.byaddr");
}
