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
static const char rcsid[] = "$Id: dns_ho.c,v 1.28 2000/04/20 07:47:54 vixie Exp $";
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
#include <stdlib.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "dns_p.h"

#ifdef SPRINTF_CHAR
# define SPRINTF(x) strlen(sprintf/**/x)
#else
# define SPRINTF(x) sprintf x
#endif

/* Definitions. */

#define	MAXALIASES	35
#define	MAXADDRS	35

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

#define BOUNDS_CHECK(ptr, count) \
	if ((ptr) + (count) > eom) { \
		had_error++; \
		continue; \
	} else (void)0

struct pvt {
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
/* Note: the IPv6 loopback address is in the "tunnel" space */
static const u_char v6local[] = { 0,0, 0,1 }; /* last 4 bytes of IPv6 addr */

/* Forwards. */

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

static void		map_v4v6_hostent(struct hostent *hp, char **bp,
					 int *len);
static void		addrsort(res_state, char **, int);
static struct hostent *	gethostans(struct irs_ho *this,
				   const u_char *ansbuf, int anslen,
				   const char *qname, int qtype,
				   int af, int size);
static int		init(struct irs_ho *this);

/* Exports. */

struct irs_ho *
irs_dns_ho(struct irs_acc *this) {
	struct irs_ho *ho;
	struct pvt *pvt;

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
	return (ho);
}

/* Methods. */

static void
ho_close(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	ho_minimize(this);
	if (pvt->res && pvt->free_res)
		(*pvt->free_res)(pvt->res);
	if (pvt)
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
	int n, size, type;
	u_char buf[MAXPACKET];
	char tmp[NS_MAXDNAME];
	const char *cp;

	if (init(this) == -1)
		return (NULL);

	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		type = T_A;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		type = T_AAAA;
		break;
	default:
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = EAFNOSUPPORT;
		return (NULL);
	}

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_nquery() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') && (cp = res_hostalias(pvt->res, name,
						      tmp, sizeof tmp)))
		name = cp;

	if ((n = res_nsearch(pvt->res, name, C_IN, type,
			     buf, sizeof buf)) < 0)
		return (NULL);
	return (gethostans(this, buf, n, name, type, af, size));
}

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	const u_char *uaddr = addr;
	char qbuf[MAXDNAME+1], *qp;
	u_char buf[MAXPACKET];
	struct hostent *hp;
	int n, size;
	
	if (init(this) == -1)
		return (NULL);

	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (!memcmp(uaddr, mapped, sizeof mapped) ||
           (!memcmp(uaddr, tunnelled, sizeof tunnelled) &&
            memcmp(&uaddr[sizeof tunnelled], v6local, sizeof(v6local))))) {
		/* Unmap. */
		addr = (char *)addr + sizeof mapped;
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
	switch (af) {
	case AF_INET:
		(void) sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
			       (uaddr[3] & 0xff),
			       (uaddr[2] & 0xff),
			       (uaddr[1] & 0xff),
			       (uaddr[0] & 0xff));
		break;
	case AF_INET6:
		qp = qbuf;
		for (n = IN6ADDRSZ - 1; n >= 0; n--) {
			qp += SPRINTF((qp, "%x.%x.",
				       uaddr[n] & 0xf,
				       (uaddr[n] >> 4) & 0xf));
		}
		strcpy(qp, "ip6.int");
		break;
	default:
		abort();
	}
	n = res_nquery(pvt->res, qbuf, C_IN, T_PTR, buf, sizeof buf);
	if (n < 0)
		return (NULL);
	hp = gethostans(this, buf, n, qbuf, T_PTR, af, size);
	if (!hp)
		return (NULL);	/* H_ERRNO was set by gethostans() */
        memcpy(pvt->host_addr, addr, len);
	pvt->h_addr_ptrs[0] = (char *)pvt->host_addr;
	pvt->h_addr_ptrs[1] = NULL;
	if (af == AF_INET && (pvt->res->options & RES_USE_INET6)) {
		map_v4v6_address((char*)pvt->host_addr, (char*)pvt->host_addr);
		pvt->host.h_addrtype = AF_INET6;
		pvt->host.h_length = IN6ADDRSZ;
	}
	RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
	return (hp);
}

static struct hostent *
ho_next(struct irs_ho *this) {
	return (NULL);
}

static void
ho_rewind(struct irs_ho *this) {
	/* NOOP */
}

static void
ho_minimize(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

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

/* Private. */

static struct hostent *
gethostans(struct irs_ho *this,
	   const u_char *ansbuf, int anslen, const char *qname, int qtype,
	   int af, int size)
{
	struct pvt *pvt = (struct pvt *)this->private;
	int type, class, buflen, ancount, qdcount, n, haveanswer, had_error;
	int (*name_ok)(const char *);
	const HEADER *hp;
	const u_char *eom;
	const u_char *cp;
	const char *tname, **tap;
	char *bp, **ap, **hap;
	char tbuf[MAXDNAME+1];

	tname = qname;
	eom = ansbuf + anslen;
	switch (qtype) {
	case T_A:
	case T_AAAA:
		name_ok = res_hnok;
		break;
	case T_PTR:
		name_ok = res_dnok;
		break;
	default:
		abort();
	}

	pvt->host.h_addrtype = af;
	pvt->host.h_length = size;
	pvt->host.h_name = NULL;

	/*
	 * Find first satisfactory answer.
	 */
	if (ansbuf + HFIXEDSZ > eom) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	hp = (HEADER *)ansbuf;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = pvt->hostbuf;
	buflen = sizeof pvt->hostbuf;
	cp = ansbuf + HFIXEDSZ;
	if (qdcount != 1) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	n = dn_expand(ansbuf, eom, cp, bp, buflen);
	if (n < 0 || !maybe_ok(pvt->res, bp, name_ok)) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (cp > eom) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	if (qtype == T_A || qtype == T_AAAA) {
		/* res_nsend() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = strlen(bp) + 1;		/* for the \0 */
		if (n > MAXHOSTNAMELEN) {
			RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
			return (NULL);
		}
		pvt->host.h_name = bp;
		bp += n;
		buflen -= n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = pvt->host.h_name;
	}
	ap = pvt->host_aliases;
	*ap = NULL;
	pvt->host.h_aliases = pvt->host_aliases;
	hap = pvt->h_addr_ptrs;
	*hap = NULL;
	pvt->host.h_addr_list = pvt->h_addr_ptrs;
	haveanswer = 0;
	had_error = 0;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(ansbuf, eom, cp, bp, buflen);
		if (n < 0 || !maybe_ok(pvt->res, bp, name_ok)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
		type = ns_get16(cp);
 		cp += INT16SZ;			/* type */
		class = ns_get16(cp);
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = ns_get16(cp);
		cp += INT16SZ;			/* len */
		BOUNDS_CHECK(cp, n);
		if (class != C_IN) {
			cp += n;
			continue;
		}
		if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME) {
			if (ap >= &pvt->host_aliases[MAXALIASES-1])
				continue;
			n = dn_expand(ansbuf, eom, cp, tbuf, sizeof tbuf);
			if (n < 0 || !maybe_ok(pvt->res, tbuf, name_ok)) {
				had_error++;
				continue;
			}
			cp += n;
			/* Store alias. */
			*ap++ = bp;
			n = strlen(bp) + 1;	/* for the \0 */
			bp += n;
			buflen -= n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > buflen || n > MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			pvt->host.h_name = bp;
			bp += n;
			buflen -= n;
			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(ansbuf, eom, cp, tbuf, sizeof tbuf);
			if (n < 0 || !maybe_dnok(pvt->res, tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > buflen) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			tname = bp;
			bp += n;
			buflen -= n;
			continue;
		}
		if (type != qtype) {
			cp += n;
			continue;
		}
		switch (type) {
		case T_PTR:
			if (ns_samename(tname, bp) != 1) {
				cp += n;
				continue;
			}
			n = dn_expand(ansbuf, eom, cp, bp, buflen);
			if (n < 0 || !maybe_hnok(pvt->res, bp) ||
			    n >= MAXHOSTNAMELEN) {
				had_error++;
				break;
			}
			cp += n;
			if (!haveanswer)
				pvt->host.h_name = bp;
			else if (ap < &pvt->host_aliases[MAXALIASES-1])
				*ap++ = bp;
			else
				n = -1;
			if (n != -1) {
				n = strlen(bp) + 1;	/* for the \0 */
				bp += n;
				buflen -= n;
			}
			break;
		case T_A:
		case T_AAAA:
			if (ns_samename(pvt->host.h_name, bp) != 1) {
				cp += n;
				continue;
			}
			if (n != pvt->host.h_length) {
				cp += n;
				continue;
			}
			if (!haveanswer) {
				int nn;

				nn = strlen(bp) + 1;	/* for the \0 */
				if (nn >= MAXHOSTNAMELEN) {
					cp += n;
					had_error++;
					continue;
				}
				pvt->host.h_name = bp;
				bp += nn;
				buflen -= nn;
			}
			/* Ensure alignment. */
			bp += sizeof(align) - ((u_long)bp % sizeof(align));
			/* Avoid overflows. */
			if (bp + n >= &pvt->hostbuf[sizeof pvt->hostbuf]) {
				had_error++;
				continue;
			}
			if (hap >= &pvt->h_addr_ptrs[MAXADDRS-1]) {
				cp += n;
				continue;
			}
			/* Suppress duplicates. */
			for (tap = (const char **)pvt->h_addr_ptrs;
			     *tap != NULL;
			     tap++)
				if (memcmp(*tap, cp, n) == 0)
					break;
			if (*tap != NULL) {
				cp += n;
				continue;
			}
			/* Store address. */
			memcpy(*hap++ = bp, cp, n);
			*hap = NULL;
			bp += n;
			cp += n;
			break;
		default:
			abort();
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;

		if (pvt->res->nsort && haveanswer > 1 && qtype == T_A)
			addrsort(pvt->res, pvt->h_addr_ptrs, haveanswer);
		if (!pvt->host.h_name) {
			n = strlen(qname) + 1;	/* for the \0 */
			if (n > buflen || n >= MAXHOSTNAMELEN)
				goto no_recovery;
			strcpy(bp, qname);
			pvt->host.h_name = bp;
			bp += n;
			buflen -= n;
		}
		if (pvt->res->options & RES_USE_INET6)
			map_v4v6_hostent(&pvt->host, &bp, &buflen);
		RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
		return (&pvt->host);
	}
 no_recovery:
	RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
	return (NULL);
}

static void
map_v4v6_hostent(struct hostent *hp, char **bpp, int *lenp) {
	char **ap;

	if (hp->h_addrtype != AF_INET || hp->h_length != INADDRSZ)
		return;
	hp->h_addrtype = AF_INET6;
	hp->h_length = IN6ADDRSZ;
	for (ap = hp->h_addr_list; *ap; ap++) {
		int i = sizeof(align) - ((u_long)*bpp % sizeof(align));

		if (*lenp < (i + IN6ADDRSZ)) {
			/* Out of memory.  Truncate address list here. */
			*ap = NULL;
			return;
		}
		*bpp += i;
		*lenp -= i;
		map_v4v6_address(*ap, *bpp);
		*ap = *bpp;
		*bpp += IN6ADDRSZ;
		*lenp -= IN6ADDRSZ;
	}
}

static void
addrsort(res_state statp, char **ap, int num) {
	int i, j, needsort = 0, aval[MAXADDRS];
	char **p;

	p = ap;
	for (i = 0; i < num; i++, p++) {
		for (j = 0 ; (unsigned)j < statp->nsort; j++)
			if (statp->sort_list[j].addr.s_addr == 
			    (((struct in_addr *)(*p))->s_addr &
			     statp->sort_list[j].mask))
				break;
		aval[i] = j;
		if (needsort == 0 && i > 0 && j < aval[i-1])
			needsort = i;
	}
	if (!needsort)
		return;

	while (needsort < num) {
		for (j = needsort - 1; j >= 0; j--) {
			if (aval[j] > aval[j+1]) {
				char *hp;

				i = aval[j];
				aval[j] = aval[j+1];
				aval[j+1] = i;

				hp = ap[j];
				ap[j] = ap[j+1];
				ap[j+1] = hp;

			} else
				break;
		}
		needsort++;
	}
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
