/*
 * Copyright (c) 1985, 1988, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* from gethostnamadr.c	8.1 (Berkeley) 6/4/93 */
/* BIND Id: gethnamaddr.c,v 8.15 1996/05/22 04:56:30 vixie Exp $ */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$Id: lcl_ho.c,v 1.15 1997/12/04 04:57:56 halley Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "dns_p.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) sprintf x
#endif

extern int h_errno;

/* Definitions. */

#define	MAXALIASES	35
#define	MAXADDRS	35
#define	Max(a,b)	((a) > (b) ? (a) : (b))

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

struct pvt {
	FILE *		fp;
	struct hostent	host;
	char *		h_addr_ptrs[MAXADDRS + 1];
	char *		host_aliases[MAXALIASES];
	char		hostbuf[8*1024];
	u_char		host_addr[16];	/* IPv4 or IPv6 */
};

typedef union {
	int32_t al;
	char ac;
} align;

static const u_char mapped[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0xff,0xff };
static const u_char tunnelled[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 };

/* Forward. */

static void		ho_close(struct irs_ho *this);
static struct hostent *	ho_byname(struct irs_ho *this, const char *name);
static struct hostent *	ho_byname2(struct irs_ho *this, const char *name,
				   int af);
static struct hostent *	ho_byaddr(struct irs_ho *this, const void *addr,
				  int len, int af);
static struct hostent *	ho_next(struct irs_ho *this);
static void		ho_rewind(struct irs_ho *this);
static void		ho_minimize(struct irs_ho *this);

static size_t		ns_namelen(const char *);

/* Portability. */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* Public. */

struct irs_ho *
irs_lcl_ho(struct irs_acc *this) {
	struct irs_ho *ho;
	struct pvt *pvt;

	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	if (!(ho = malloc(sizeof *ho))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(ho, 0x5e, sizeof *ho);
	ho->private = pvt;
	ho->close = ho_close;
	ho->byname = ho_byname;
	ho->byname2 = ho_byname2;
	ho->byaddr = ho_byaddr;
	ho->next = ho_next;
	ho->rewind = ho_rewind;
	ho->minimize = ho_minimize;
	return (ho);
}

/* Methods. */

static void
ho_close(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp)
		(void) fclose(pvt->fp);
	free(pvt);
	free(this);
}

static struct hostent *
ho_byname(struct irs_ho *this, const char *name) {
	struct hostent *hp;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	if (_res.options & RES_USE_INET6) {
		hp = ho_byname2(this, name, AF_INET6);
		if (hp)
			return (hp);
	}
	return (ho_byname2(this, name, AF_INET));
}

static struct hostent *
ho_byname2(struct irs_ho *this, const char *name, int af) {
	struct hostent *hp;
	char **hap;
	size_t n;
	
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	ho_rewind(this);
	n = ns_namelen(name);
	while ((hp = ho_next(this)) != NULL) {
		size_t nn;

		if (hp->h_addrtype != af)
			continue;
		nn = ns_namelen(hp->h_name);
		if (strncasecmp(hp->h_name, name, Max(n, nn)) == 0)
			goto found;
		for (hap = hp->h_aliases; *hap; hap++) {
			nn = ns_namelen(*hap);
			if (strncasecmp(*hap, name, Max(n, nn)) == 0)
				goto found;
		}
	}
 found:
	if (!hp) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	h_errno = NETDB_SUCCESS;
	return (hp);
}

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af) {
	const u_char *uaddr = addr;
	struct hostent *hp;
	int size;
	
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (!memcmp(uaddr, mapped, sizeof mapped) ||
	     !memcmp(uaddr, tunnelled, sizeof tunnelled))) {
		/* Unmap. */
		addr = (u_char *)addr + sizeof mapped;
		uaddr += sizeof mapped;
		af = AF_INET;
		len = INADDRSZ;
	}
	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	if (size > len) {
		errno = EINVAL;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}

	/*
	 * Do the search.
	 */
	ho_rewind(this);
	while ((hp = ho_next(this)) != NULL) {
		char **hap;

		for (hap = hp->h_addr_list; *hap; hap++) {
			const u_char *taddr = (const u_char *)*hap;
			int taf = hp->h_addrtype;
			int tlen = hp->h_length;

			if (taf == AF_INET6 && tlen == IN6ADDRSZ &&
			    (!memcmp(taddr, mapped, sizeof mapped) ||
			     !memcmp(taddr, tunnelled, sizeof tunnelled))) {
				/* Unmap. */
				taddr += sizeof mapped;
				taf = AF_INET;
				tlen = INADDRSZ;
			}
			if (taf == af && tlen == len &&
			    !memcmp(taddr, uaddr, tlen))
				goto found;
		}
	}
 found:
	if (!hp) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	h_errno = NETDB_SUCCESS;
	return (hp);
}

static struct hostent *
ho_next(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *cp, **q, *p;
	int af, len;

	if (!pvt->fp)
		ho_rewind(this);
	if (!pvt->fp) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
 again:
	if (!(p = fgets(pvt->hostbuf, sizeof pvt->hostbuf, pvt->fp))) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	if (*p == '#')
		goto again;
	if (!(cp = strpbrk(p, "#\n")))
		goto again;
	*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	if ((_res.options & RES_USE_INET6) &&
	    inet_pton(AF_INET6, p, pvt->host_addr) > 0) {
		af = AF_INET6;
		len = IN6ADDRSZ;
	} else if (inet_aton(p, (struct in_addr *)pvt->host_addr) > 0) {
		if (_res.options & RES_USE_INET6) {
			map_v4v6_address((char*)pvt->host_addr,
					 (char*)pvt->host_addr);
			af = AF_INET6;
			len = IN6ADDRSZ;
		} else {
			af = AF_INET;
			len = INADDRSZ;
		}
	} else {
		goto again;
	}
	pvt->h_addr_ptrs[0] = (char *)pvt->host_addr;
	pvt->h_addr_ptrs[1] = NULL;
	pvt->host.h_addr_list = pvt->h_addr_ptrs;
	pvt->host.h_length = len;
	pvt->host.h_addrtype = af;
	while (*cp == ' ' || *cp == '\t')
		cp++;
	pvt->host.h_name = cp;
	q = pvt->host.h_aliases = pvt->host_aliases;
	if ((cp = strpbrk(cp, " \t")) != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &pvt->host_aliases[MAXALIASES - 1])
			*q++ = cp;
		if ((cp = strpbrk(cp, " \t")) != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	h_errno = NETDB_SUCCESS;
	return (&pvt->host);
}

static void
ho_rewind(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp) {
		if (fseek(pvt->fp, 0L, SEEK_SET) == 0)
			return;
		(void)fclose(pvt->fp);
	}
	if (!(pvt->fp = fopen(_PATH_HOSTS, "r")))
		return;
	if (fcntl(fileno(pvt->fp), F_SETFD, 1) < 0) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}

static void
ho_minimize(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp != NULL) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}

/* Private. */

static size_t
ns_namelen(const char *s) {
	int i;

	for (i = strlen(s); i > 0 && s[i-1] == '.'; i--)
		(void)NULL;
	return ((size_t) i);
}
