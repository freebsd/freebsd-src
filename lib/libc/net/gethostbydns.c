/*
 * ++Copyright++ 1985, 1988, 1993
 * -
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
static char fromrcsid[] = "From: Id: gethnamaddr.c,v 8.23 1998/04/07 04:59:46 vixie Exp $";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <resolv.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <nsswitch.h>

#include "netdb_private.h"
#include "res_config.h"

#define SPRINTF(x) ((size_t)sprintf x)

static const char AskedForGot[] =
		"gethostby*.gethostanswer: asked for \"%s\", got \"%s\"";

#ifdef RESOLVSORT
static void addrsort(char **, int);
#endif

#ifdef DEBUG
static void dprintf(char *, int) __printflike(1, 0);
#endif

#define MAXPACKET	(64*1024)

typedef union {
    HEADER hdr;
    u_char buf[MAXPACKET];
} querybuf;

typedef union {
    int32_t al;
    char ac;
} align;

int _dns_ttl_;

#ifdef DEBUG
static void
dprintf(msg, num)
	char *msg;
	int num;
{
	if (_res.options & RES_DEBUG) {
		int save = errno;

		printf(msg, num);
		errno = save;
	}
}
#else
# define dprintf(msg, num) /*nada*/
#endif

#define BOUNDED_INCR(x) \
	do { \
		cp += x; \
		if (cp > eom) { \
			h_errno = NO_RECOVERY; \
			return -1; \
		} \
	} while (0)

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			h_errno = NO_RECOVERY; \
			return -1; \
		} \
	} while (0)

static int
gethostanswer(const querybuf *answer, int anslen, const char *qname, int qtype,
    struct hostent *he, struct hostent_data *hed)
{
	const HEADER *hp;
	const u_char *cp;
	int n;
	const u_char *eom, *erdata;
	char *bp, *ep, **ap, **hap;
	int type, class, ancount, qdcount;
	int haveanswer, had_error;
	int toobig = 0;
	char tbuf[MAXDNAME];
	const char *tname;
	int (*name_ok)(const char *);

	tname = qname;
	he->h_name = NULL;
	eom = answer->buf + anslen;
	switch (qtype) {
	case T_A:
	case T_AAAA:
		name_ok = res_hnok;
		break;
	case T_PTR:
		name_ok = res_dnok;
		break;
	default:
		h_errno = NO_RECOVERY;
		return -1;	/* XXX should be abort(); */
	}
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = hed->hostbuf;
	ep = hed->hostbuf + sizeof hed->hostbuf;
	cp = answer->buf;
	BOUNDED_INCR(HFIXEDSZ);
	if (qdcount != 1) {
		h_errno = NO_RECOVERY;
		return -1;
	}
	n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
	if ((n < 0) || !(*name_ok)(bp)) {
		h_errno = NO_RECOVERY;
		return -1;
	}
	BOUNDED_INCR(n + QFIXEDSZ);
	if (qtype == T_A || qtype == T_AAAA) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = strlen(bp) + 1;		/* for the \0 */
		if (n >= MAXHOSTNAMELEN) {
			h_errno = NO_RECOVERY;
			return -1;
		}
		he->h_name = bp;
		bp += n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = he->h_name;
	}
	ap = hed->host_aliases;
	*ap = NULL;
	he->h_aliases = hed->host_aliases;
	hap = hed->h_addr_ptrs;
	*hap = NULL;
	he->h_addr_list = hed->h_addr_ptrs;
	haveanswer = 0;
	had_error = 0;
	_dns_ttl_ = -1;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
		if ((n < 0) || !(*name_ok)(bp)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		BOUNDS_CHECK(cp, 3 * INT16SZ + INT32SZ);
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ;			/* class */
		if (qtype == T_A  && type == T_A)
			_dns_ttl_ = _getlong(cp);
		cp += INT32SZ;			/* TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		BOUNDS_CHECK(cp, n);
		erdata = cp + n;
		if (class != C_IN) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME) {
			if (ap >= &hed->host_aliases[_MAXALIASES-1])
				continue;
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if ((n < 0) || !(*name_ok)(tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return -1;
			}
			/* Store alias. */
			*ap++ = bp;
			n = strlen(bp) + 1;	/* for the \0 */
			if (n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			bp += n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			he->h_name = bp;
			bp += n;
			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if (n < 0 || !res_dnok(tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return -1;
			}
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			tname = bp;
			bp += n;
			continue;
		}
		if (type != qtype) {
			if (type != T_SIG)
				syslog(LOG_NOTICE|LOG_AUTH,
	"gethostby*.gethostanswer: asked for \"%s %s %s\", got type \"%s\"",
				       qname, p_class(C_IN), p_type(qtype),
				       p_type(type));
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		switch (type) {
		case T_PTR:
			if (strcasecmp(tname, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, qname, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			n = dn_expand(answer->buf, eom, cp, bp, ep - bp);
			if ((n < 0) || !res_hnok(bp)) {
				had_error++;
				break;
			}
#if MULTI_PTRS_ARE_ALIASES
			cp += n;
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return -1;
			}
			if (!haveanswer)
				he->h_name = bp;
			else if (ap < &hed->host_aliases[_MAXALIASES-1])
				*ap++ = bp;
			else
				n = -1;
			if (n != -1) {
				n = strlen(bp) + 1;	/* for the \0 */
				if (n >= MAXHOSTNAMELEN) {
					had_error++;
					break;
				}
				bp += n;
			}
			break;
#else
			he->h_name = bp;
			if (_res.options & RES_USE_INET6) {
				n = strlen(bp) + 1;	/* for the \0 */
				if (n >= MAXHOSTNAMELEN) {
					had_error++;
					break;
				}
				bp += n;
				_map_v4v6_hostent(he, &bp, ep);
			}
			h_errno = NETDB_SUCCESS;
			return 0;
#endif
		case T_A:
		case T_AAAA:
			if (strcasecmp(he->h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, he->h_name, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (n != he->h_length) {
				cp += n;
				continue;
			}
			if (!haveanswer) {
				int nn;

				he->h_name = bp;
				nn = strlen(bp) + 1;	/* for the \0 */
				bp += nn;
			}

			bp += sizeof(align) - ((u_long)bp % sizeof(align));

			if (bp + n >= ep) {
				dprintf("size (%d) too big\n", n);
				had_error++;
				continue;
			}
			if (hap >= &hed->h_addr_ptrs[_MAXADDRS-1]) {
				if (!toobig++)
					dprintf("Too many addresses (%d)\n",
						_MAXADDRS);
				cp += n;
				continue;
			}
			bcopy(cp, *hap++ = bp, n);
			bp += n;
			cp += n;
			if (cp != erdata) {
				h_errno = NO_RECOVERY;
				return -1;
			}
			break;
		default:
			dprintf("Impossible condition (type=%d)\n", type);
			h_errno = NO_RECOVERY;
			return -1;
			/* BIND has abort() here, too risky on bad data */
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
		*ap = NULL;
		*hap = NULL;
# if defined(RESOLVSORT)
		/*
		 * Note: we sort even if host can take only one address
		 * in its return structures - should give it the "best"
		 * address in that case, not some random one
		 */
		if (_res.nsort && haveanswer > 1 && qtype == T_A)
			addrsort(hed->h_addr_ptrs, haveanswer);
# endif /*RESOLVSORT*/
		if (!he->h_name) {
			n = strlen(qname) + 1;	/* for the \0 */
			if (n > ep - bp || n >= MAXHOSTNAMELEN)
				goto no_recovery;
			strcpy(bp, qname);
			he->h_name = bp;
			bp += n;
		}
		if (_res.options & RES_USE_INET6)
			_map_v4v6_hostent(he, &bp, ep);
		h_errno = NETDB_SUCCESS;
		return 0;
	}
 no_recovery:
	h_errno = NO_RECOVERY;
	return -1;
}

/* XXX: for async DNS resolver in ypserv */
struct hostent *
__dns_getanswer(const char *answer, int anslen, const char *qname, int qtype)
{
	struct hostdata *hd;
	int error;

	if ((hd = __hostdata_init()) == NULL) {
		h_errno = NETDB_INTERNAL;
		return NULL;
	}
	switch (qtype) {
	case T_AAAA:
		hd->host.h_addrtype = AF_INET6;
		hd->host.h_length = IN6ADDRSZ;
		break;
	case T_A:
	default:
		hd->host.h_addrtype = AF_INET;
		hd->host.h_length = INADDRSZ;
		break;
	}

	error = gethostanswer((const querybuf *)answer, anslen, qname, qtype,
	    &hd->host, &hd->data);
	return (error == 0) ? &hd->host : NULL;
}

int
_dns_gethostbyname(void *rval, void *cb_data, va_list ap)
{
	const char *name;
	int af;
	struct hostent *he;
	struct hostent_data *hed;
	querybuf *buf;
	int n, size, type, error;

	name = va_arg(ap, const char *);
	af = va_arg(ap, int);
	he = va_arg(ap, struct hostent *);
	hed = va_arg(ap, struct hostent_data *);

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
		h_errno = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return NS_UNAVAIL;
	}

	he->h_addrtype = af;
	he->h_length = size;

	if ((buf = malloc(sizeof(*buf))) == NULL) {
		h_errno = NETDB_INTERNAL;
		return NS_NOTFOUND;
	}
	n = res_search(name, C_IN, type, buf->buf, sizeof(buf->buf));
	if (n < 0) {
		free(buf);
		dprintf("res_search failed (%d)\n", n);
		return (0);
	} else if (n > sizeof(buf->buf)) {
		free(buf);
		dprintf("static buffer is too small (%d)\n", n);
		return (0);
	}
	error = gethostanswer(buf, n, name, type, he, hed);
	free(buf);
	return (error == 0) ? NS_SUCCESS : NS_NOTFOUND;
}

int
_dns_gethostbyaddr(void *rval, void *cb_data, va_list ap)
{
	const char *addr;	/* XXX should have been def'd as u_char! */
	int len, af;
	struct hostent *he;
	struct hostent_data *hed;
	const u_char *uaddr;
	static const u_char mapped[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0xff,0xff };
	static const u_char tunnelled[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 };
	int n, size, error;
	querybuf *buf;
	char qbuf[MAXDNAME+1], *qp;
#ifdef SUNSECURITY
	struct hostdata rhd;
	struct hostent *rhe;
	char **haddr;
	u_long old_options;
	char hname2[MAXDNAME+1];
#endif /*SUNSECURITY*/

	addr = va_arg(ap, const char *);
	uaddr = (const u_char *)addr;
	len = va_arg(ap, int);
	af = va_arg(ap, int);
	he = va_arg(ap, struct hostent *);
	hed = va_arg(ap, struct hostent_data *);

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return NS_UNAVAIL;
	}
	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (!bcmp(uaddr, mapped, sizeof mapped) ||
	     !bcmp(uaddr, tunnelled, sizeof tunnelled))) {
		/* Unmap. */
		addr += sizeof mapped;
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
		return NS_UNAVAIL;
	}
	if (size != len) {
		errno = EINVAL;
		h_errno = NETDB_INTERNAL;
		return NS_UNAVAIL;
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
		strlcat(qbuf, "ip6.arpa", sizeof(qbuf));
		break;
	default:
		abort();
	}
	if ((buf = malloc(sizeof(*buf))) == NULL) {
		h_errno = NETDB_INTERNAL;
		return NS_NOTFOUND;
	}
	n = res_query(qbuf, C_IN, T_PTR, (u_char *)buf->buf, sizeof buf->buf);
	if (n < 0) {
		free(buf);
		dprintf("res_query failed (%d)\n", n);
		return NS_UNAVAIL;
	}
	if (n > sizeof buf->buf) {
		free(buf);
		dprintf("static buffer is too small (%d)\n", n);
		return NS_UNAVAIL;
	}
	if ((error = gethostanswer(buf, n, qbuf, T_PTR, he, hed)) != 0) {
		free(buf);
		return NS_NOTFOUND;   /* h_errno was set by gethostanswer() */
	}
	free(buf);
#ifdef SUNSECURITY
	if (af == AF_INET) {
	    /*
	     * turn off search as the name should be absolute,
	     * 'localhost' should be matched by defnames
	     */
	    strncpy(hname2, he->h_name, MAXDNAME);
	    hname2[MAXDNAME] = '\0';
	    old_options = _res.options;
	    _res.options &= ~RES_DNSRCH;
	    _res.options |= RES_DEFNAMES;
	    memset(&rhd, 0, sizeof rhd);
	    if (!(rhe = gethostbyname_r(hname2, &rhd.host, &rhd.data))) {
		syslog(LOG_NOTICE|LOG_AUTH,
		       "gethostbyaddr: No A record for %s (verifying [%s])",
		       hname2, inet_ntoa(*((struct in_addr *)addr)));
		_res.options = old_options;
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	    }
	    _res.options = old_options;
	    for (haddr = rhe->h_addr_list; *haddr; haddr++)
		if (!memcmp(*haddr, addr, INADDRSZ))
			break;
	    if (!*haddr) {
		syslog(LOG_NOTICE|LOG_AUTH,
		       "gethostbyaddr: A record of %s != PTR record [%s]",
		       hname2, inet_ntoa(*((struct in_addr *)addr)));
		h_errno = HOST_NOT_FOUND;
		return NS_NOTFOUND;
	    }
	}
#endif /*SUNSECURITY*/
	he->h_addrtype = af;
	he->h_length = len;
	bcopy(addr, hed->host_addr, len);
	hed->h_addr_ptrs[0] = (char *)hed->host_addr;
	hed->h_addr_ptrs[1] = NULL;
	if (af == AF_INET && (_res.options & RES_USE_INET6)) {
		_map_v4v6_address((char*)hed->host_addr, (char*)hed->host_addr);
		he->h_addrtype = AF_INET6;
		he->h_length = IN6ADDRSZ;
	}
	h_errno = NETDB_SUCCESS;
	return (error == 0) ? NS_SUCCESS : NS_NOTFOUND;
}

#ifdef RESOLVSORT
static void
addrsort(ap, num)
	char **ap;
	int num;
{
	int i, j;
	char **p;
	short aval[_MAXADDRS];
	int needsort = 0;

	p = ap;
	for (i = 0; i < num; i++, p++) {
	    for (j = 0 ; (unsigned)j < _res.nsort; j++)
		if (_res.sort_list[j].addr.s_addr == 
		    (((struct in_addr *)(*p))->s_addr & _res.sort_list[j].mask))
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
#endif
void
_sethostdnsent(stayopen)
	int stayopen;
{
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return;
	if (stayopen)
		_res.options |= RES_STAYOPEN | RES_USEVC;
}

void
_endhostdnsent()
{
	_res.options &= ~(RES_STAYOPEN | RES_USEVC);
	res_close();
}
