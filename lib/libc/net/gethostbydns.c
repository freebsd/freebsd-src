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
static char rcsid[] = "$Id: gethostbydns.c,v 1.13 1996/12/22 22:05:43 wpaul Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <resolv.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>

#include "res_config.h"

extern void _res_close __P((void));

#define SPRINTF(x) ((size_t)sprintf x)

#define	MAXALIASES	35
#define	MAXADDRS	35

static const char AskedForGot[] =
		"gethostby*.gethostanswer: asked for \"%s\", got \"%s\"";

static char *h_addr_ptrs[MAXADDRS + 1];
static struct hostent *gethostbyname_ipv4 __P((const char *));

static struct hostent host;
static char *host_aliases[MAXALIASES];
static char hostbuf[8*1024];
static u_char host_addr[16];	/* IPv4 or IPv6 */

#ifdef RESOLVSORT
static void addrsort __P((char **, int));
#endif

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

typedef union {
    HEADER hdr;
    u_char buf[MAXPACKET];
} querybuf;

typedef union {
    int32_t al;
    char ac;
} align;

extern int h_errno;
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

static struct hostent *
gethostanswer(answer, anslen, qname, qtype)
	const querybuf *answer;
	int anslen;
	const char *qname;
	int qtype;
{
	register const HEADER *hp;
	register const u_char *cp;
	register int n;
	const u_char *eom;
	char *bp, **ap, **hap;
	int type, class, buflen, ancount, qdcount;
	int haveanswer, had_error;
	int toobig = 0;
	char tbuf[MAXDNAME+1];
	const char *tname;
	int (*name_ok) __P((const char *));

	tname = qname;
	host.h_name = NULL;
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
		return (NULL);	/* XXX should be abort(); */
	}
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = hostbuf;
	buflen = sizeof hostbuf;
	cp = answer->buf + HFIXEDSZ;
	if (qdcount != 1) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	n = dn_expand(answer->buf, eom, cp, bp, buflen);
	if ((n < 0) || !(*name_ok)(bp)) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (qtype == T_A || qtype == T_AAAA) {
		/* res_send() has already verified that the query name is the
		 * same as the one we sent; this just gets the expanded name
		 * (i.e., with the succeeding search-domain tacked on).
		 */
		n = strlen(bp) + 1;		/* for the \0 */
		host.h_name = bp;
		bp += n;
		buflen -= n;
		/* The qname can be abbreviated, but h_name is now absolute. */
		qname = host.h_name;
	}
	ap = host_aliases;
	*ap = NULL;
	host.h_aliases = host_aliases;
	hap = h_addr_ptrs;
	*hap = NULL;
	host.h_addr_list = h_addr_ptrs;
	haveanswer = 0;
	had_error = 0;
	_dns_ttl_ = -1;
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, buflen);
		if ((n < 0) || !(*name_ok)(bp)) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ;			/* class */
		if (qtype == T_A  && type == T_A)
			_dns_ttl_ = _getlong(cp);
		cp += INT32SZ;			/* TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		if (class != C_IN) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if ((qtype == T_A || qtype == T_AAAA) && type == T_CNAME) {
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if ((n < 0) || !(*name_ok)(tbuf)) {
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
			if (n > buflen) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			host.h_name = bp;
			bp += n;
			buflen -= n;
			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if ((n < 0) || !res_hnok(tbuf)) {
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
			n = dn_expand(answer->buf, eom, cp, bp, buflen);
			if ((n < 0) || !res_hnok(bp)) {
				had_error++;
				break;
			}
#if MULTI_PTRS_ARE_ALIASES
			cp += n;
			if (!haveanswer)
				host.h_name = bp;
			else if (ap < &host_aliases[MAXALIASES-1])
				*ap++ = bp;
			else
				n = -1;
			if (n != -1) {
				n = strlen(bp) + 1;	/* for the \0 */
				bp += n;
				buflen -= n;
			}
			break;
#else
			host.h_name = bp;
			if (_res.options & RES_USE_INET6) {
				n = strlen(bp) + 1;	/* for the \0 */
				bp += n;
				buflen -= n;
				_map_v4v6_hostent(&host, &bp, &buflen);
			}
			h_errno = NETDB_SUCCESS;
			return (&host);
#endif
		case T_A:
		case T_AAAA:
			if (strcasecmp(host.h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, host.h_name, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (n != host.h_length) {
				cp += n;
				continue;
			}
			if (!haveanswer) {
				register int nn;

				host.h_name = bp;
				nn = strlen(bp) + 1;	/* for the \0 */
				bp += nn;
				buflen -= nn;
			}

			bp += sizeof(align) - ((u_long)bp % sizeof(align));

			if (bp + n >= &hostbuf[sizeof hostbuf]) {
				dprintf("size (%d) too big\n", n);
				had_error++;
				continue;
			}
			if (hap >= &h_addr_ptrs[MAXADDRS-1]) {
				if (!toobig++)
					dprintf("Too many addresses (%d)\n",
						MAXADDRS);
				cp += n;
				continue;
			}
			bcopy(cp, *hap++ = bp, n);
			bp += n;
			buflen -= n;
			cp += n;
			break;
		default:
			dprintf("Impossible condition (type=%d)\n", type);
			h_errno = NO_RECOVERY;
			return (NULL);
		} /*switch*/
		if (!had_error)
			haveanswer++;
	} /*while*/
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
			addrsort(h_addr_ptrs, haveanswer);
# endif /*RESOLVSORT*/
		if (!host.h_name) {
			n = strlen(qname) + 1;	/* for the \0 */
			if (n > buflen)
				goto try_again;
			strcpy(bp, qname);
			host.h_name = bp;
			bp += n;
			buflen -= n;
		}
		if (_res.options & RES_USE_INET6)
			_map_v4v6_hostent(&host, &bp, &buflen);
		h_errno = NETDB_SUCCESS;
		return (&host);
	}
 try_again:
	h_errno = TRY_AGAIN;
	return (NULL);
}

struct hostent *
__dns_getanswer(answer, anslen, qname, qtype)
	const char *answer;
	int anslen;
	const char *qname;
	int qtype;
{
	switch(qtype) {
	case T_AAAA:
		host.h_addrtype = AF_INET6;
		host.h_length = IN6ADDRSZ;
		break;
	case T_A:
	default:
		host.h_addrtype = AF_INET;
		host.h_length = INADDRSZ;
		break;
		return(NULL);
	}

	return(gethostanswer((const querybuf *)answer, anslen, qname, qtype));
}

struct hostent *
_gethostbydnsname(name, af)
	const char *name;
	int af;
{
	querybuf buf;
	register const char *cp;
	char *bp;
	int n, size, type, len;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}

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
		return (NULL);
	}

	host.h_addrtype = af;
	host.h_length = size;

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_query() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') && (cp = __hostalias(name)))
		name = cp;

	/*
	 * disallow names consisting only of digits/dots, unless
	 * they end in a dot.
	 */
	if (isdigit(name[0]))
		for (cp = name;; ++cp) {
			if (!*cp) {
				if (*--cp == '.')
					break;
				/*
				 * All-numeric, no dot at the end.
				 * Fake up a hostent as if we'd actually
				 * done a lookup.
				 */
				if (inet_pton(af, name, host_addr) <= 0) {
					h_errno = HOST_NOT_FOUND;
					return (NULL);
				}
				strncpy(hostbuf, name, MAXDNAME);
				hostbuf[MAXDNAME] = '\0';
				bp = hostbuf + MAXDNAME;
				len = sizeof hostbuf - MAXDNAME;
				host.h_name = hostbuf;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				h_addr_ptrs[0] = (char *)host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				if (_res.options & RES_USE_INET6)
					_map_v4v6_hostent(&host, &bp, &len);
				h_errno = NETDB_SUCCESS;
				return (&host);
			}
			if (!isdigit(*cp) && *cp != '.') 
				break;
		}

	if ((n = res_search(name, C_IN, type, buf.buf, sizeof(buf))) < 0) {
		dprintf("res_search failed (%d)\n", n);
		return (NULL);
	}
	return (gethostanswer(&buf, n, name, type));
}

struct hostent *
_gethostbydnsaddr(addr, len, af)
	const char *addr;	/* XXX should have been def'd as u_char! */
	int len, af;
{
	const u_char *uaddr = (const u_char *)addr;
	static const u_char mapped[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0xff,0xff };
	static const u_char tunnelled[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 };
	int n, size;
	querybuf buf;
	register struct hostent *hp;
	char qbuf[MAXDNAME+1], *qp;
#ifdef SUNSECURITY
	register struct hostent *rhp;
	char **haddr;
	u_long old_options;
	char hname2[MAXDNAME+1];
#endif /*SUNSECURITY*/
	
	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
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
		return (NULL);
	}
	if (size != len) {
		errno = EINVAL;
		h_errno = NETDB_INTERNAL;
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
	n = res_query(qbuf, C_IN, T_PTR, (u_char *)buf.buf, sizeof buf.buf);
	if (n < 0) {
		dprintf("res_query failed (%d)\n", n);
		return (NULL);
	}
	if (!(hp = gethostanswer(&buf, n, qbuf, T_PTR)))
		return (NULL);	/* h_errno was set by gethostanswer() */
#ifdef SUNSECURITY
	if (af == AF_INET) {
	    /*
	     * turn off search as the name should be absolute,
	     * 'localhost' should be matched by defnames
	     */
	    strncpy(hname2, hp->h_name, MAXDNAME);
	    hname2[MAXDNAME] = '\0';
	    old_options = _res.options;
	    _res.options &= ~RES_DNSRCH;
	    _res.options |= RES_DEFNAMES;
	    if (!(rhp = gethostbyname(hname2))) {
		syslog(LOG_NOTICE|LOG_AUTH,
		       "gethostbyaddr: No A record for %s (verifying [%s])",
		       hname2, inet_ntoa(*((struct in_addr *)addr)));
		_res.options = old_options;
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	    }
	    _res.options = old_options;
	    for (haddr = rhp->h_addr_list; *haddr; haddr++)
		if (!memcmp(*haddr, addr, INADDRSZ))
			break;
	    if (!*haddr) {
		syslog(LOG_NOTICE|LOG_AUTH,
		       "gethostbyaddr: A record of %s != PTR record [%s]",
		       hname2, inet_ntoa(*((struct in_addr *)addr)));
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	    }
	}
#endif /*SUNSECURITY*/
	hp->h_addrtype = af;
	hp->h_length = len;
	bcopy(addr, host_addr, len);
	h_addr_ptrs[0] = (char *)host_addr;
	h_addr_ptrs[1] = NULL;
	if (af == AF_INET && (_res.options & RES_USE_INET6)) {
		_map_v4v6_address((char*)host_addr, (char*)host_addr);
		hp->h_addrtype = AF_INET6;
		hp->h_length = IN6ADDRSZ;
	}
	h_errno = NETDB_SUCCESS;
	return (hp);
}

#ifdef RESOLVSORT
static void
addrsort(ap, num)
	char **ap;
	int num;
{
	int i, j;
	char **p;
	short aval[MAXADDRS];
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
	_res_close();
}
