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
static char sccsid[] = "@(#)$Id: getnetbynis.c,v 1.4 1995/10/22 14:39:06 phk Exp $";
static char rcsid[] = "$Id: getnetbynis.c,v 1.4 1995/10/22 14:39:06 phk Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <arpa/nameser.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#define	MAXALIASES	35
#define	MAXADDRS	35

#ifdef YP
static char *host_aliases[MAXALIASES];
#endif /* YP */

static struct netent *
_getnetbynis(name, map)
	char *name, *map;
{
#ifdef YP
	register char *cp, **q;
	static char *result;
	int resultlen;
	static struct netent h;
	static char *domain = (char *)NULL;
	static char ypbuf[YPMAXRECORD];

	if (domain == (char *)NULL)
		if (yp_get_default_domain (&domain))
			return (NULL);

	if (yp_match(domain, map, name, strlen(name), &result, &resultlen))
		return (NULL);

	bcopy((char *)result, (char *)&ypbuf, resultlen);
	free(result);
	result = (char *)&ypbuf;

	if ((cp = index(result, '\n')))
		*cp = '\0';

	cp = strpbrk(result, " \t");
	*cp++ = '\0';
	h.n_name = result;

	while (*cp == ' ' || *cp == '\t')
		cp++;

	h.n_net = inet_network(cp);
	h.n_addrtype = AF_INET;

	q = h.n_aliases = host_aliases;
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
#endif
}

struct netent *
_getnetbynisname(name)
	char *name;
{
	return _getnetbynis(name, "networks.byname");
}

struct netent *
_getnetbynisaddr(addr, type)
	long addr;
	int type;
{
	char *str, *cp;
	unsigned long net2;
	int nn;
	unsigned int netbr[4];
	char buf[MAXDNAME];

	if (type != AF_INET)
		return (NULL);

        for (nn = 4, net2 = addr; net2; net2 >>= 8) {
                netbr[--nn] = net2 & 0xff;
	}

	switch (nn) {
	case 3:		/* Class A */
		sprintf(buf, "%u", netbr[3]);
		break;
        case 2:		/* Class B */
		sprintf(buf, "%u.%u", netbr[2], netbr[3]);
		break;
        case 1:		/* Class C */
		sprintf(buf, "%u.%u.%u", netbr[1], netbr[2], netbr[3]);
                break;
        case 0:		/* Class D - E */
		sprintf(buf, "%u.%u.%u.%u", netbr[0], netbr[1],
			netbr[2], netbr[3]);
		break;
	}

	str = (char *)&buf;
	cp = str + (strlen(str) - 2);

	while(!strcmp(cp, ".0")) {
		*cp = '\0';
		cp = str + (strlen(str) - 2);
	}

	return _getnetbynis(str, "networks.byaddr");
}
