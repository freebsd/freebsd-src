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
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: lcl_ho.c,v 1.26 2001/05/29 05:49:04 marka Exp $";
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
#include <isc/memcluster.h>

#include "port_after.h"

#include "irs_p.h"
#include "dns_p.h"
#include "lcl_p.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) sprintf x
#endif

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
	struct __res_state  *res;
	void		(*free_res)(void *);
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
static struct __res_state * ho_res_get(struct irs_ho *this);
static void		ho_res_set(struct irs_ho *this,
				   struct __res_state *res,
				   void (*free_res)(void *));
static struct addrinfo * ho_addrinfo(struct irs_ho *this, const char *name,
				     const struct addrinfo *pai);

static size_t		ns_namelen(const char *);
static int		init(struct irs_ho *this);

/* Portability. */

#ifndef SEEK_SET
# define SEEK_SET 0
#endif

/* Public. */

struct irs_ho *
irs_lcl_ho(struct irs_acc *this) {
	struct irs_ho *ho;
	struct pvt *pvt;

	UNUSED(this);

	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	if (!(ho = memget(sizeof *ho))) {
		memput(pvt, sizeof *pvt);
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
	ho->res_get = ho_res_get;
	ho->res_set = ho_res_set;
	ho->addrinfo = ho_addrinfo;
	return (ho);
}

/* Methods. */

static void
ho_close(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	ho_minimize(this);
	if (pvt->fp)
		(void) fclose(pvt->fp);
	if (pvt->res && pvt->free_res)
		(*pvt->free_res)(pvt->res);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct hostent *
ho_byname(struct irs_ho *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *hp;

	if (init(this) == -1)
		return (NULL);

	if (pvt->res->options & RES_USE_INET6) {
		hp = ho_byname2(this, name, AF_INET6);
		if (hp)
			return (hp);
	}
	return (ho_byname2(this, name, AF_INET));
}

static struct hostent *
ho_byname2(struct irs_ho *this, const char *name, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *hp;
	char **hap;
	size_t n;
	
	if (init(this) == -1)
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
		RES_SET_H_ERRNO(pvt->res, HOST_NOT_FOUND);
		return (NULL);
	}
	RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
	return (hp);
}

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	const u_char *uaddr = addr;
	struct hostent *hp;
	int size;
	
	if (init(this) == -1)
		return (NULL);

	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (!memcmp(uaddr, mapped, sizeof mapped) ||
	     !memcmp(uaddr, tunnelled, sizeof tunnelled))) {
		/* Unmap. */
		addr = (const u_char *)addr + sizeof mapped;
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
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		return (NULL);
	}
	if (size > len) {
		errno = EINVAL;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
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
		RES_SET_H_ERRNO(pvt->res, HOST_NOT_FOUND);
		return (NULL);
	}
	RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
	return (hp);
}

static struct hostent *
ho_next(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *cp, **q, *p;
	char *bufp, *ndbuf, *dbuf = NULL;
	int c, af, len, bufsiz, offset;

	if (init(this) == -1)
		return (NULL);

	if (!pvt->fp)
		ho_rewind(this);
	if (!pvt->fp) {
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		return (NULL);
	}
	bufp = pvt->hostbuf;
	bufsiz = sizeof pvt->hostbuf;
	offset = 0;
 again:
	if (!(p = fgets(bufp + offset, bufsiz - offset, pvt->fp))) {
		RES_SET_H_ERRNO(pvt->res, HOST_NOT_FOUND);
		if (dbuf)
			free(dbuf);
		return (NULL);
	}
	if (!strchr(p, '\n') && !feof(pvt->fp)) {
#define GROWBUF 1024
		/* allocate space for longer line */
		if (dbuf == NULL) {
			if ((ndbuf = malloc(bufsiz + GROWBUF)) != NULL)
				strcpy(ndbuf, bufp);
		} else
			ndbuf = realloc(dbuf, bufsiz + GROWBUF);
		if (ndbuf) {
			dbuf = ndbuf;
			bufp = dbuf;
			bufsiz += GROWBUF;
			offset = strlen(dbuf);
		} else {
			/* allocation failed; skip this long line */
			while ((c = getc(pvt->fp)) != EOF)
				if (c == '\n')
					break;
			if (c != EOF)
				ungetc(c, pvt->fp);
		}
		goto again;
	}

	p -= offset;
	offset = 0;

	if (*p == '#')
		goto again;
	if ((cp = strpbrk(p, "#\n")) != NULL)
		*cp = '\0';
	if (!(cp = strpbrk(p, " \t")))
		goto again;
	*cp++ = '\0';
	if (inet_pton(AF_INET6, p, pvt->host_addr) > 0) {
		af = AF_INET6;
		len = IN6ADDRSZ;
	} else if (inet_aton(p, (struct in_addr *)pvt->host_addr) > 0) {
		if (pvt->res->options & RES_USE_INET6) {
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
	if (dbuf)
		free(dbuf);
	RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
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
	if (pvt->res)
		res_nclose(pvt->res);
} 

static struct __res_state *
ho_res_get(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		ho_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
ho_res_set(struct irs_ho *this, struct __res_state *res,
		void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->res && pvt->free_res) {
		res_nclose(pvt->res);
		(*pvt->free_res)(pvt->res);
	}

	pvt->res = res;
	pvt->free_res = free_res;
}

struct lcl_res_target {
	struct lcl_res_target *next;
	int family;
};

/* XXX */
extern struct addrinfo *hostent2addrinfo __P((struct hostent *,
					      const struct addrinfo *pai));

static struct addrinfo *
ho_addrinfo(struct irs_ho *this, const char *name, const struct addrinfo *pai)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *hp;
	struct lcl_res_target q, q2, *p;
	struct addrinfo sentinel, *cur;

	memset(&q, 0, sizeof(q2));
	memset(&q2, 0, sizeof(q2));
	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	switch(pai->ai_family) {
	case AF_UNSPEC:		/* INET6 then INET4 */
		q.family = AF_INET6;
		q.next = &q2;
		q2.family = AF_INET;
		break;
	case AF_INET6:
		q.family = AF_INET6;
		break;
	case AF_INET:
		q.family = AF_INET;
		break;
	default:
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY); /* ??? */
		return(NULL);
	}

	for (p = &q; p; p = p->next) {
		struct addrinfo *ai;

		hp = (*this->byname2)(this, name, p->family);
		if (hp == NULL) {
			/* byname2 should've set an appropriate error */
			continue;
		}
		if ((hp->h_name == NULL) || (hp->h_name[0] == 0) ||
		    (hp->h_addr_list[0] == NULL)) {
			RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
			continue;
		}

		ai = hostent2addrinfo(hp, pai);
		if (ai) {
			cur->ai_next = ai;
			while (cur && cur->ai_next)
				cur = cur->ai_next;
		}
	}

	if (sentinel.ai_next == NULL)
		RES_SET_H_ERRNO(pvt->res, HOST_NOT_FOUND);

	return(sentinel.ai_next);
}

/* Private. */

static size_t
ns_namelen(const char *s) {
	int i;

	for (i = strlen(s); i > 0 && s[i-1] == '.'; i--)
		(void)NULL;
	return ((size_t) i);
}

static int
init(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (!pvt->res && !ho_res_get(this))
		return (-1);
	if (((pvt->res->options & RES_INIT) == 0) &&
	    res_ninit(pvt->res) == -1)
		return (-1);
	return (0);
}
