/*-
 * Copyright (c) 1985, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)gethostnamadr.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <nsswitch.h>
#include <arpa/nameser.h>	/* XXX */
#include <resolv.h>		/* XXX */
#include "netdb_private.h"

void
_sethosthtent(int f, struct hostent_data *hed)
{
	if (!hed->hostf)
		hed->hostf = fopen(_PATH_HOSTS, "r");
	else
		rewind(hed->hostf);
	hed->stayopen = f;
}

void
_endhosthtent(struct hostent_data *hed)
{
	if (hed->hostf && !hed->stayopen) {
		(void) fclose(hed->hostf);
		hed->hostf = NULL;
	}
}

int
gethostent_r(struct hostent *he, struct hostent_data *hed)
{
	char *p, *bp, *ep;
	char *cp, **q;
	int af, len;
	char hostbuf[BUFSIZ + 1];

	if (!hed->hostf && !(hed->hostf = fopen(_PATH_HOSTS, "r"))) {
		h_errno = NETDB_INTERNAL;
		return -1;
	}
 again:
	if (!(p = fgets(hostbuf, sizeof hostbuf, hed->hostf))) {
		h_errno = HOST_NOT_FOUND;
		return -1;
	}
	if (*p == '#')
		goto again;
	cp = strpbrk(p, "#\n");
	if (cp != NULL)
		*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	if (inet_pton(AF_INET6, p, hed->host_addr) > 0) {
		af = AF_INET6;
		len = IN6ADDRSZ;
	} else if (inet_pton(AF_INET, p, hed->host_addr) > 0) {
		if (_res.options & RES_USE_INET6) {
			_map_v4v6_address((char *)hed->host_addr,
			    (char *)hed->host_addr);
			af = AF_INET6;
			len = IN6ADDRSZ;
		} else {
			af = AF_INET;
			len = INADDRSZ;
		}
	} else {
		goto again;
	}
	hed->h_addr_ptrs[0] = (char *)hed->host_addr;
	hed->h_addr_ptrs[1] = NULL;
	he->h_addr_list = hed->h_addr_ptrs;
	he->h_length = len;
	he->h_addrtype = af;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	bp = hed->hostbuf;
	ep = hed->hostbuf + sizeof hed->hostbuf;
	he->h_name = bp;
	q = he->h_aliases = hed->host_aliases;
	if ((p = strpbrk(cp, " \t")) != NULL)
		*p++ = '\0';
	len = strlen(cp) + 1;
	if (ep - bp < len) {
		h_errno = NETDB_INTERNAL;
		return -1;
	}
	strlcpy(bp, cp, ep - bp);
	bp += len;
	cp = p;
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q >= &hed->host_aliases[_MAXALIASES - 1])
			break;
		if ((p = strpbrk(cp, " \t")) != NULL)
			*p++ = '\0';
		len = strlen(cp) + 1;
		if (ep - bp < len)
			break;
		strlcpy(bp, cp, ep - bp);
		*q++ = bp;
		bp += len;
		cp = p;
	}
	*q = NULL;
	h_errno = NETDB_SUCCESS;
	return 0;
}

struct hostent *
gethostent(void)
{
	struct hostdata *hd;

	if ((hd = __hostdata_init()) == NULL)
		return NULL;
	if (gethostent_r(&hd->host, &hd->data) != 0)
		return NULL;
	return &hd->host;
}

int
_ht_gethostbyname(void *rval, void *cb_data, va_list ap) 
{
	const char *name;
	int af;
	struct hostent *he;
	struct hostent_data *hed;
	char **cp;
	int error;

	name = va_arg(ap, const char *);
	af = va_arg(ap, int);
	he = va_arg(ap, struct hostent *);
	hed = va_arg(ap, struct hostent_data *);

	sethostent_r(0, hed);
	while ((error = gethostent_r(he, hed)) == 0) {
		if (he->h_addrtype != af)
			continue;
		if (strcasecmp(he->h_name, name) == 0)
			break;
		for (cp = he->h_aliases; *cp != 0; cp++)
			if (strcasecmp(*cp, name) == 0)
				goto found;
	}
found:
	endhostent_r(hed);

	return (error == 0) ? NS_SUCCESS : NS_NOTFOUND;
}

int 
_ht_gethostbyaddr(void *rval, void *cb_data, va_list ap)
{
	const char *addr;
	int len, af;
	struct hostent *he;
	struct hostent_data *hed;
	int error;

	addr = va_arg(ap, const char *);
	len = va_arg(ap, int);
	af = va_arg(ap, int);
	he = va_arg(ap, struct hostent *);
	hed = va_arg(ap, struct hostent_data *);

	sethostent_r(0, hed);
	while ((error = gethostent_r(he, hed)) == 0)
		if (he->h_addrtype == af && !bcmp(he->h_addr, addr, len))
			break;
	endhostent_r(hed);

	return (error == 0) ? NS_SUCCESS : NS_NOTFOUND;
}
