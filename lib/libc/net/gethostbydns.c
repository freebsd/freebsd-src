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
static char rcsid[] = "$Id: gethostbydns.c,v 1.2 1994/09/25 17:45:37 pst Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

#define	BYADDR		0
#define	BYNAME		1

#define	MAXALIASES	35
#define	MAXADDRS	35

#define	MULTI_PTRS_ARE_ALIASES 1	/* XXX - experimental */

static const char AskedForGot[] =
	"gethostby*.gethostanswer: asked for \"%s\", got \"%s\"";

static char *h_addr_ptrs[MAXADDRS + 1];

static struct hostent host;
static char *host_aliases[MAXALIASES];
static char hostbuf[8*1024];
static struct in_addr host_addr;
static FILE *hostf = NULL;
static char hostaddr[MAXADDRS];
static char *host_addrs[2];
static int stayopen = 0;

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
	    for (j = 0 ; j < _res.nsort; j++)
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

static struct hostent *
gethostanswer(answer, anslen, qname, qclass, qtype)
	const querybuf *answer;
	int anslen;
	const char *qname;
	int qclass, qtype;
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

	host.h_name = NULL;
	eom = answer->buf + anslen;
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
	if ((n = dn_expand(answer->buf, eom, cp, bp, buflen)) < 0) {
		h_errno = NO_RECOVERY;
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (qtype == T_A) {
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
	while (ancount-- > 0 && cp < eom && !had_error) {
		n = dn_expand(answer->buf, eom, cp, bp, buflen);
		if (n < 0) {
			had_error++;
			continue;
		}
		cp += n;			/* name */
		type = _getshort(cp);
 		cp += INT16SZ;			/* type */
		class = _getshort(cp);
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = _getshort(cp);
		cp += INT16SZ;			/* len */
		if (class != qclass) {
			/* XXX - debug? syslog? */
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		if (qtype == T_A && type == T_CNAME) {
			if (ap >= &host_aliases[MAXALIASES-1])
				continue;
			n = dn_expand(answer->buf, eom, cp, tbuf, sizeof tbuf);
			if (n < 0) {
				had_error++;
				continue;
			}
			cp += n;
			if (host.h_name && strcasecmp(host.h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
		"gethostby*.getanswer: asked for \"%s\", got CNAME for \"%s\"",
				       host.h_name, bp);
				continue;	/* XXX - had_error++ ? */
			}
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
		if (type != qtype) {
			syslog(LOG_NOTICE|LOG_AUTH,
		     "gethostby*.getanswer: asked for type %d(%s), got %d(%s)",
			       qtype, qname, type, bp);
			cp += n;
			continue;		/* XXX - had_error++ ? */
		}
		switch (type) {
		case T_PTR:
			if (strcasecmp(qname, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, qname, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			n = dn_expand(answer->buf, eom, cp, bp, buflen);
			if (n < 0) {
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
			return (&host);
#endif
		case T_A:
			if (strcasecmp(host.h_name, bp) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				       AskedForGot, host.h_name, bp);
				cp += n;
				continue;	/* XXX - had_error++ ? */
			}
			if (haveanswer) {
				if (n != host.h_length) {
					cp += n;
					continue;
				}
			} else {
				register int nn;

				host.h_length = n;
				host.h_addrtype = (class == C_IN)
						  ? AF_INET
						  : AF_UNSPEC;
				host.h_name = bp;
				nn = strlen(bp) + 1;	/* for the \0 */
				bp += nn;
				buflen -= nn;
			}

			bp += sizeof(align) - ((u_long)bp % sizeof(align));

			if (bp + n >= &hostbuf[sizeof hostbuf]) {
				if (_res.options & RES_DEBUG)
					printf("size (%d) too big\n", n);
				had_error++;
				continue;
			}
			if (hap >= &h_addr_ptrs[MAXADDRS-1]) {
				if (_res.options & RES_DEBUG && !toobig++)
					printf("Too many addresses (%d)\n",
					       MAXADDRS);
				cp += n;
				continue;
			}
			bcopy(cp, *hap++ = bp, n);
			bp += n;
			cp += n;
			break;
		default:
			abort();
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
		if (_res.nsort && haveanswer > 1 &&
		    qclass == C_IN && qtype == T_A)
			addrsort(h_addr_ptrs, haveanswer);
# endif /*RESOLVSORT*/
		if (!host.h_name) {
			n = strlen(qname) + 1;	/* for the \0 */
			strcpy(bp, qname);
			host.h_name = bp;
		}
		return (&host);
	} else {
		h_errno = TRY_AGAIN;
		return (NULL);
	}
}

struct hostent *
_gethostbydnsname(name)
	const char *name;
{
	querybuf buf;
	register const char *cp;
	int n;

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
				if (!inet_aton(name, &host_addr)) {
					h_errno = HOST_NOT_FOUND;
					return (NULL);
				}
				host.h_name = (char *)name;
				host.h_aliases = host_aliases;
				host_aliases[0] = NULL;
				host.h_addrtype = AF_INET;
				host.h_length = INT32SZ;
				h_addr_ptrs[0] = (char *)&host_addr;
				h_addr_ptrs[1] = NULL;
				host.h_addr_list = h_addr_ptrs;
				return (&host);
			}
			if (!isdigit(*cp) && *cp != '.') 
				break;
		}

	if ((n = res_search(name, C_IN, T_A, buf.buf, sizeof(buf))) < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_search failed\n");
		return (NULL);
	}
	return (gethostanswer(&buf, n, name, C_IN, T_A));
}

struct hostent *
_gethostbydnsaddr(addr, len, type)
	const char *addr;
	int len, type;
{
	int n;
	querybuf buf;
	register struct hostent *hp;
	char qbuf[MAXDNAME+1];
	int o_res_options = _res.options;
	
	if (type != AF_INET)
		return (NULL);
	(void)sprintf(qbuf, "%u.%u.%u.%u.in-addr.arpa",
		((unsigned)addr[3] & 0xff),
		((unsigned)addr[2] & 0xff),
		((unsigned)addr[1] & 0xff),
		((unsigned)addr[0] & 0xff));
	_res.options |= RES_RECURSE;
	n = res_query(qbuf, C_IN, T_PTR, (u_char *)buf.buf, sizeof buf.buf);
	_res.options = o_res_options;
	if (n < 0) {
		if (_res.options & RES_DEBUG)
			printf("res_query failed\n");
		return (NULL);
	}
	if (!(hp = gethostanswer(&buf, n, qbuf, C_IN, T_PTR)))
		return (NULL);
	hp->h_addrtype = type;
	hp->h_length = len;
	h_addr_ptrs[0] = (char *)&host_addr;
	h_addr_ptrs[1] = NULL;
	host_addr = *(struct in_addr *)addr;
#if BSD < 43 && !defined(h_addr)	/* new-style hostent structure */
	hp->h_addr = h_addr_ptrs[0];
#endif
	return (hp);
}

void
_sethostdnsent(stayopen)
	int stayopen;
{
	if (stayopen)
		_res.options |= RES_STAYOPEN | RES_USEVC;
}

void
_endhostdnsent()
{
	_res.options &= ~(RES_STAYOPEN | RES_USEVC);
	_res_close();
}
