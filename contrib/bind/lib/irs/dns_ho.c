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
static const char rcsid[] = "$Id: dns_ho.c,v 1.39 2002/06/27 03:56:32 marka Exp $";
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
#include <syslog.h>

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

#define MAXPACKET (1024*64)

#define BOUNDS_CHECK(ptr, count) \
	if ((ptr) + (count) > eom) { \
		had_error++; \
		continue; \
	} else (void)0

typedef union {
	HEADER hdr;
	u_char buf[MAXPACKET];
} querybuf;

struct dns_res_target {
	struct dns_res_target *next;
	querybuf qbuf;		/* query buffer */
	u_char *answer;		/* buffer to put answer */
	int anslen;		/* size of answer buffer */
	int qclass, qtype;	/* class and type of query */
	int action;		/* condition whether query is really issued */
	char qname[MAXDNAME +1]; /* domain name */
#if 0
	int n;			/* result length */
#endif
};
enum {RESTGT_DOALWAYS, RESTGT_AFTERFAILURE, RESTGT_IGNORE};
enum {RESQRY_SUCCESS, RESQRY_FAIL};

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
static struct addrinfo * ho_addrinfo(struct irs_ho *this, const char *name,
				     const struct addrinfo *pai);

static void		map_v4v6_hostent(struct hostent *hp, char **bp,
					 char *ep);
static void		addrsort(res_state, char **, int);
static struct hostent *	gethostans(struct irs_ho *this,
				   const u_char *ansbuf, int anslen,
				   const char *qname, int qtype,
				   int af, int size,
				   struct addrinfo **ret_aip,
				   const struct addrinfo *pai);
static int add_hostent(struct pvt *pvt, char *bp, char **hap,
		       struct addrinfo *ai);
static const u_char * ar_head(const u_char *, int, const u_char *,
			      const u_char *, struct pvt *,
			      int (*)(const char *));
static struct addrinfo * a6_expand(const u_char *, const u_char *, int,
				   const u_char *, const u_char *,
				   const struct in6_addr *, int,
				   const struct addrinfo *,
				   struct pvt *, int (*)(const char *), int *);
static const char *dname_subst(const char *, const char *, const char *);
static int		init(struct irs_ho *this);

/* Exports. */

struct irs_ho *
irs_dns_ho(struct irs_acc *this) {
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
ho_byname2(struct irs_ho *this, const char *name, int af)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *hp = NULL;
	int n, size;
	char tmp[NS_MAXDNAME];
	const char *cp;
	struct addrinfo ai;
	struct dns_res_target *q, *q2, *p;
	int querystate = RESQRY_FAIL;

	if (init(this) == -1)
		return (NULL);

	q = memget(sizeof(*q));
	q2 = memget(sizeof(*q2));
	if (q == NULL || q2 == NULL) {
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = ENOMEM;
		goto cleanup;
	}
	memset(q, 0, sizeof(q));
	memset(q2, 0, sizeof(q2));

	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		q->qclass = C_IN;
		q->qtype = T_A;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->action = RESTGT_DOALWAYS;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		q->qclass = C_IN;
		q->qtype = ns_t_a6;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->next = q2;
#ifdef RES_USE_A6
		if ((pvt->res->options & RES_USE_A6) == 0)
			q->action = RESTGT_IGNORE;
		else
#endif
			q->action = RESTGT_DOALWAYS;
		q2->qclass = C_IN;
		q2->qtype = T_AAAA;
		q2->answer = q2->qbuf.buf;
		q2->anslen = sizeof(q2->qbuf);
		q2->action = RESTGT_AFTERFAILURE;
		break;
	default:
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = EAFNOSUPPORT;
		hp = NULL;
		goto cleanup;
	}

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_nquery() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') && (cp = res_hostalias(pvt->res, name,
						      tmp, sizeof tmp)))
		name = cp;

	for (p = q; p; p = p->next) {
		switch(p->action) {
		case RESTGT_DOALWAYS:
			break;
		case RESTGT_AFTERFAILURE:
			if (querystate == RESQRY_SUCCESS)
				continue;
			break;
		case RESTGT_IGNORE:
			continue;
		}

		if ((n = res_nsearch(pvt->res, name, p->qclass, p->qtype,
				     p->answer, p->anslen)) < 0) {
			querystate = RESQRY_FAIL;
			continue;
		}

		memset(&ai, 0, sizeof(ai));
		ai.ai_family = af;
		if ((hp = gethostans(this, p->answer, n, name, p->qtype,
				     af, size, NULL,
				     (const struct addrinfo *)&ai)) != NULL)
			goto cleanup;	/* no more loop is necessary */

		querystate = RESQRY_FAIL;
		continue;
	}

 cleanup:
	if (q != NULL)
		memput(q, sizeof(*q));
	if (q2 != NULL)
		memput(q2, sizeof(*q2));
	return(hp);
}

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af)
{
	struct pvt *pvt = (struct pvt *)this->private;
	const u_char *uaddr = addr;
	char *qp;
	struct hostent *hp = NULL;
	struct addrinfo ai;
	struct dns_res_target *q, *q2, *p;
	int n, size;
	int querystate = RESQRY_FAIL;
	
	if (init(this) == -1)
		return (NULL);

	q = memget(sizeof(*q));
	q2 = memget(sizeof(*q2));
	if (q == NULL || q2 == NULL) {
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = ENOMEM;
		goto cleanup;
	}
	memset(q, 0, sizeof(q));
	memset(q2, 0, sizeof(q2));

	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (!memcmp(uaddr, mapped, sizeof mapped) ||
           (!memcmp(uaddr, tunnelled, sizeof tunnelled) &&
            memcmp(&uaddr[sizeof tunnelled], v6local, sizeof(v6local))))) {
		/* Unmap. */
		addr = (const char *)addr + sizeof mapped;
		uaddr += sizeof mapped;
		af = AF_INET;
		len = INADDRSZ;
	}
	switch (af) {
	case AF_INET:
		size = INADDRSZ;
		q->qclass = C_IN;
		q->qtype = T_PTR;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->action = RESTGT_DOALWAYS;
		break;
	case AF_INET6:
		size = IN6ADDRSZ;
		q->qclass = C_IN;
		q->qtype = T_PTR;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->next = q2;
		q->action = RESTGT_DOALWAYS;
		q2->qclass = C_IN;
		q2->qtype = T_PTR;
		q2->answer = q2->qbuf.buf;
		q2->anslen = sizeof(q2->qbuf);
		if ((pvt->res->options & RES_NO_NIBBLE2) != 0)
			q2->action = RESTGT_IGNORE;
		else
			q2->action = RESTGT_AFTERFAILURE;
		break;
	default:
		errno = EAFNOSUPPORT;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		hp = NULL;
		goto cleanup;
	}
	if (size > len) {
		errno = EINVAL;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		hp = NULL;
		goto cleanup;
	}
	switch (af) {
	case AF_INET:
		qp = q->qname;
		(void) sprintf(qp, "%u.%u.%u.%u.in-addr.arpa",
			       (uaddr[3] & 0xff),
			       (uaddr[2] & 0xff),
			       (uaddr[1] & 0xff),
			       (uaddr[0] & 0xff));
		break;
	case AF_INET6:
		if (q->action != RESTGT_IGNORE) {
			qp = q->qname;
			for (n = IN6ADDRSZ - 1; n >= 0; n--) {
				qp += SPRINTF((qp, "%x.%x.",
					       uaddr[n] & 0xf,
					       (uaddr[n] >> 4) & 0xf));
			}
			strcpy(qp, res_get_nibblesuffix(pvt->res));
		}
		if (q2->action != RESTGT_IGNORE) {
			qp = q2->qname;
			for (n = IN6ADDRSZ - 1; n >= 0; n--) {
				qp += SPRINTF((qp, "%x.%x.",
					       uaddr[n] & 0xf,
					       (uaddr[n] >> 4) & 0xf));
			}
			strcpy(qp, res_get_nibblesuffix2(pvt->res));
		}
		break;
	default:
		abort();
	}

	for (p = q; p; p = p->next) {
		switch(p->action) {
		case RESTGT_DOALWAYS:
			break;
		case RESTGT_AFTERFAILURE:
			if (querystate == RESQRY_SUCCESS)
				continue;
			break;
		case RESTGT_IGNORE:
			continue;
		}

		if ((n = res_nquery(pvt->res, p->qname, p->qclass, p->qtype,
				    p->answer, p->anslen)) < 0) {
			querystate = RESQRY_FAIL;
			continue;
		}

		memset(&ai, 0, sizeof(ai));
		ai.ai_family = af;
		hp = gethostans(this, p->answer, n, p->qname, T_PTR, af, size,
				NULL, (const struct addrinfo *)&ai);
		if (!hp) {
			querystate = RESQRY_FAIL;
			continue;
		}
			
		memcpy(pvt->host_addr, addr, len);
		pvt->h_addr_ptrs[0] = (char *)pvt->host_addr;
		pvt->h_addr_ptrs[1] = NULL;
		if (af == AF_INET && (pvt->res->options & RES_USE_INET6)) {
			map_v4v6_address((char*)pvt->host_addr,
					 (char*)pvt->host_addr);
			pvt->host.h_addrtype = AF_INET6;
			pvt->host.h_length = IN6ADDRSZ;
		}

		RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
		goto cleanup;	/* no more loop is necessary. */
	}
	hp = NULL; /* H_ERRNO was set by subroutines */

 cleanup:
	if (q != NULL)
		memput(q, sizeof(*q));
	if (q2 != NULL)
		memput(q2, sizeof(*q2));
	return(hp);
}

static struct hostent *
ho_next(struct irs_ho *this) {

	UNUSED(this);

	return (NULL);
}

static void
ho_rewind(struct irs_ho *this) {

	UNUSED(this);

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

/* XXX */
extern struct addrinfo *addr2addrinfo __P((const struct addrinfo *,
					   const char *));

static struct addrinfo *
ho_addrinfo(struct irs_ho *this, const char *name, const struct addrinfo *pai)
{
	struct pvt *pvt = (struct pvt *)this->private;
	int n;
	char tmp[NS_MAXDNAME];
	const char *cp;
	struct dns_res_target *q, *q2, *q3, *p;
	struct addrinfo sentinel, *cur;
	int querystate = RESQRY_FAIL;

	if (init(this) == -1)
		return (NULL);

	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	q = memget(sizeof(*q));
	q2 = memget(sizeof(*q2));
	q3 = memget(sizeof(*q3));
	if (q == NULL || q2 == NULL || q3 == NULL) {
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		errno = ENOMEM;
		goto cleanup;
	}
	memset(q, 0, sizeof(q2));
	memset(q2, 0, sizeof(q2));
	memset(q3, 0, sizeof(q3));

	switch (pai->ai_family) {
	case AF_UNSPEC:
		/* prefer IPv6 */
		q->qclass = C_IN;
		q->qtype = ns_t_a6;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->next = q2;
#ifdef RES_USE_A6
		if ((pvt->res->options & RES_USE_A6) == 0)
			q->action = RESTGT_IGNORE;
		else
#endif
			q->action = RESTGT_DOALWAYS;
		q2->qclass = C_IN;
		q2->qtype = T_AAAA;
		q2->answer = q2->qbuf.buf;
		q2->anslen = sizeof(q2->qbuf);
		q2->next = q3;
		/* try AAAA only when A6 query fails */
		q2->action = RESTGT_AFTERFAILURE;
		q3->qclass = C_IN;
		q3->qtype = T_A;
		q3->answer = q3->qbuf.buf;
		q3->anslen = sizeof(q3->qbuf);
		q3->action = RESTGT_DOALWAYS;
		break;
	case AF_INET:
		q->qclass = C_IN;
		q->qtype = T_A;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->action = RESTGT_DOALWAYS;
		break;
	case AF_INET6:
		q->qclass = C_IN;
		q->qtype = ns_t_a6;
		q->answer = q->qbuf.buf;
		q->anslen = sizeof(q->qbuf);
		q->next = q2;
#ifdef RES_USE_A6
		if ((pvt->res->options & RES_USE_A6) == 0)
			q->action = RESTGT_IGNORE;
		else
#endif
			q->action = RESTGT_DOALWAYS;
		q2->qclass = C_IN;
		q2->qtype = T_AAAA;
		q2->answer = q2->qbuf.buf;
		q2->anslen = sizeof(q2->qbuf);
		q2->action = RESTGT_AFTERFAILURE;
		break;
	default:
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY); /* better error? */
		goto cleanup;
	}

	/*
	 * if there aren't any dots, it could be a user-level alias.
	 * this is also done in res_nquery() since we are not the only
	 * function that looks up host names.
	 */
	if (!strchr(name, '.') && (cp = res_hostalias(pvt->res, name,
						      tmp, sizeof tmp)))
		name = cp;

	for (p = q; p; p = p->next) {
		struct addrinfo *ai;

		switch(p->action) {
		case RESTGT_DOALWAYS:
			break;
		case RESTGT_AFTERFAILURE:
			if (querystate == RESQRY_SUCCESS)
				continue;
			break;
		case RESTGT_IGNORE:
			continue;
		}

		if ((n = res_nsearch(pvt->res, name, p->qclass, p->qtype,
				     p->answer, p->anslen)) < 0) {
			querystate = RESQRY_FAIL;
			continue;
		}
		(void)gethostans(this, p->answer, n, name, p->qtype,
				 pai->ai_family, /* XXX: meaningless */
				 0, &ai, pai);
		if (ai) {
			querystate = RESQRY_SUCCESS;
			cur->ai_next = ai;
			while (cur && cur->ai_next)
				cur = cur->ai_next;
		}
		else
			querystate = RESQRY_FAIL;
	}

 cleanup:
	if (q != NULL)
		memput(q, sizeof(*q));
	if (q2 != NULL)
		memput(q2, sizeof(*q2));
	if (q3 != NULL)
		memput(q3, sizeof(*q3));
	return(sentinel.ai_next);
}

static const u_char *
ar_head(cp, count, msg, eom, pvt, name_ok)
	const u_char *cp, *msg, *eom;
	int count;
	struct pvt *pvt;
	int (*name_ok)(const char *);
{
	int n;
	char buf[1024];	/* XXX */

	while (count-- > 0 && cp < eom) {
		n = dn_expand(msg, eom, cp, buf, sizeof(buf));
		if (n < 0 || !maybe_ok(pvt->res, buf, name_ok))
			goto end;
		cp += n;			/* name */
		if (cp + 3 * INT16SZ + INT32SZ >= eom)
			goto end;
 		cp += INT16SZ;			/* type */
 		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = ns_get16(cp);
		cp += n + INT16SZ;		/* len */
	}
	return(cp);

  end:
	return(eom);		/* XXX */
}

/* XXX: too many arguments */
static struct addrinfo *
a6_expand(const u_char *ansbuf, const u_char *a6p,
	  int a6len, const u_char *arp, const u_char *eom,
	  const struct in6_addr *in6, int plen, const struct addrinfo *pai,
	  struct pvt *pvt, int (*name_ok)(const char *), int *errorp)
{
	struct in6_addr a;
	int n, pbyte, plen1, pbyte1, error = 0;
	const u_char *cp;
	struct addrinfo sentinel, *cur;
	char pname[1024], buf[1024]; /* XXX */

	*errorp = NETDB_SUCCESS;
	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	/*
	 * Validate A6 parameters.
	 */
	if (a6len == 0) { /* an A6 record must contain at least 1 byte. */
		error = NO_RECOVERY;
		goto bad;
	}
	/* prefix length check. */
	if ((plen1 = *a6p) > 128) {
		error = NO_RECOVERY;
		goto bad;
	}
	if (plen1 > plen) {
		/*
		 * New length must not be greater than old one.
		 * Ignore the record as specified in RFC 2874
		 * Section 3.1.2.
		 */
		return(NULL); /* just ignore. */
	}
	/* boundary check for new plen and prefix addr */
	pbyte1 = (plen1 & ~7) / 8;
	if ((int)sizeof(struct in6_addr) - pbyte1 > a6len - 1) {
		error = NO_RECOVERY;
		goto bad;
	}

	/*
	 * merge the new prefix portion.
	 * <--- plen(bits) --->
	 * <--- pbyte ---><-b->
	 * 000000000000000pppppxxxxxxxxxxx(= in6, 0: unknown, x: known, p: pad)
	 *           PP++++++++(+ should be merged. P: padding, must be 0)
	 * <-- plen1-->
	 * <-pbyte1->
	 *           ^a6p+1
	 * The result should be:
	 * 0000000000PP++++++++xxxxxxxxxxx(= a)
	 */
	pbyte = (plen & ~7) / 8;
	a = *in6;
	if (pbyte > pbyte1) {
		/* N.B. the case of "pbyte1 == 128" is implicitly excluded. */
		int b = plen % 8; /* = the length of "pp..." above */
		u_char c_hi, c_lo;

		memcpy(&a.s6_addr[pbyte1], a6p + 1, pbyte - pbyte1);
		if (b > 0) {
			c_hi = a6p[pbyte - pbyte1 + 1];
			c_lo = in6->s6_addr[pbyte];
			a.s6_addr[pbyte] =
				(c_hi & (0xff << (8 - b))) |
				((0x00ff >> b) & c_lo);
		}
	}

#if 0				/* for debug */
	if ((pvt->res->options & RES_DEBUG) != 0) {
		u_char ntopbuf[INET6_ADDRSTRLEN];

		inet_ntop(AF_INET6, &a, ntopbuf, sizeof(ntopbuf));
		printf("a6_expand: %s\\%d\n", ntopbuf, plen1);
	}
#endif

	if (plen1 == 0) {
		/* Here is the end of A6 chain. make addrinfo, then return. */
		return(addr2addrinfo(pai, (const char *)&a));
	}

	/*
	 * Expand the new prefix name. Since the prefix name must not be
	 * compressed (RFC 2874 Section 3.1.1), we could use ns_name_ntop()
	 * here if it had a stricter boundary check.
	 */
	cp = a6p + 1 + (sizeof(*in6) - pbyte1);
	n = dn_expand(ansbuf, eom, cp, pname, sizeof(pname));
	if (n < 0 || !maybe_ok(pvt->res, pname, name_ok)) {
		error = NO_RECOVERY;
		goto bad;
	}
	if (cp + n != a6p + a6len) { /* length mismatch */
		error = NO_RECOVERY;
		goto bad;
	}

	/*
	 * we need (more) additional section records, but no one is
	 * available, which possibly means a malformed answer.
	 */
	if (arp == NULL) {
		error = NO_RECOVERY; /* we can't resolve the chain. */
		goto bad;
	}

	/*
	 * Loop thru the rest of the buffer, searching for the next A6 record
	 * that has the same owner name as the prefix name. If found, then
	 * recursively call this function to expand the whole A6 chain.
	 */
	plen = plen1;
	for (cp = arp; cp != NULL && cp < eom; cp += n) {
		int class, type;

		n = dn_expand(ansbuf, eom, cp, buf, sizeof(buf));
		if (n < 0 || !maybe_ok(pvt->res, buf, name_ok)) {
			error = NO_RECOVERY;
			goto bad;
		}
		cp += n;			/* name */
		if (cp + 3 * INT16SZ + INT32SZ > eom) {
			error = NO_RECOVERY;
			goto bad;
		}
		type = ns_get16(cp);
		cp += INT16SZ;			/* type */
		class = ns_get16(cp);
		cp += INT16SZ + INT32SZ;	/* class, TTL */
		n = ns_get16(cp);
		cp += INT16SZ;			/* len */
		if (cp + n > eom) {
			error = NO_RECOVERY;
			goto bad;
		}
		if (class != C_IN || type != ns_t_a6) {
			/* we are only interested in A6 records. skip others */
			continue;
		}

		if (ns_samename(buf, pname) != 1) {
			continue;
		}

		/* Proceed to the next record in the chain. */
		cur->ai_next = a6_expand(ansbuf, cp, n, cp + n, eom,
					 (const struct in6_addr *)&a,
					 plen, pai, pvt, name_ok, &error);
		if (error != NETDB_SUCCESS)
			goto bad;
		while (cur && cur->ai_next)
			cur = cur->ai_next;
	}

	return(sentinel.ai_next);

  bad:
	*errorp = error;
	if (sentinel.ai_next)
		freeaddrinfo(sentinel.ai_next);
	return(NULL);
}

static const char *
dname_subst(const char *qname0, const char *owner0, const char *target) {
	char owner[MAXDNAME];
	static char qname[MAXDNAME];
	const char blabelhead[] = "\\[x"; /* we can assume hex strings */
	int qlen, olen;
	int bufsiz = sizeof(qname);

	/* make local copies, which are canonicalized. */
	if (ns_makecanon(qname0, qname, sizeof(qname)) < 0 ||
	    ns_makecanon(owner0, owner, sizeof(owner)) < 0)
		return(NULL);
	qlen = strlen(qname);
	olen = strlen(owner);
	/* from now on, do not refer to qname0 nor owner0. */

	/*
	 * check if QNAME is a subdomain of OWNER.
	 * XXX: currently, we only handle the following two cases:
	 *      (A) none of the labels are bitlabels, or
	 *      (B) both of the head labels are bitlabels (and the following
	 *          labels are NOT bitlabels).
	 * If we pass the check, then subtract the remaining part from QNAME.
	 * ex. (A) qname=www.foo.com,owner=foo.com => new qname=www.
	 *     (B) qname=\[x3ffe0501/32].foo.com,owner=\[x3ffe/16].foo.com
	 *                                  => new qname=\[x0501/16].
	 */
	if (ns_samedomain(qname, owner)) { /* check (A) */
		/* at this point, qlen must not be smaller than olen */
		qname[qlen - olen] = 0;
		bufsiz -= (qlen - olen);
	} else {		/* check (B) */
		char *parent0, *parent1;
		/* the following 3 have enough size to store 1 bitlabel */
		u_char qlabel[64], olabel[64], newlabel[64];
		int qlabellen, olabellen;

		if (strncmp(qname, blabelhead, 3) != 0 ||
		    strncmp(owner, blabelhead, 3) != 0)
			return(NULL);
		/*
		 * Both two begin with bitlabels. The succeeding parts
		 * must exact match.
		 */
		if ((parent0 = strchr(qname, '.')) == NULL ||
		    (parent1 = strchr(owner, '.')) == NULL)
			return(NULL);

		/* ns_samename allows names to begin with '.' */
		if (ns_samename(parent0, parent1) != 1)
			return(NULL);

		/* cut the upper domain parts off. */
		*(parent0 + 1) = 0;
		*(parent1 + 1) = 0;
		/* convert the textual form into binary one. */
		if (ns_name_pton(qname, qlabel, sizeof(qlabel)) < 0 ||
		    ns_name_pton(owner, olabel, sizeof(olabel)) < 0)
			return(NULL);
		if ((qlabellen = *(qlabel + 1)) == 0)
			qlabellen = 256;
		if ((olabellen = *(olabel + 1)) == 0)
			olabellen = 256;
		if (olabellen > qlabellen)
			return(NULL); /* owner does not contain qname. */
		else {
			int qplen = (qlabellen + 7) / 8;
			int oplen = (olabellen + 7) / 8;
			int sft = olabellen % 8;
			int nllen, n;
			u_char *qp, *op, *np;

			/* skip ELT and Count. */
			qp = qlabel + 2;
			op = olabel + 2;

			/* check if olabel is a "subdomain" of qlabel. */
			if (memcmp(qp, op, oplen - 1) != 0)
				return(NULL);
			if (sft > 0) {
				/* compare trailing bits (between 1 and 7) */
				if ((qp[qplen - 1] & (0xff << sft)) !=
				    op[qplen - 1])
					return(NULL);
			}

			/* OK, get remaining bits from qlabel. */
			np = newlabel;
			if (olabellen == qlabellen) {
				/*
				 * Two names (including bitlabels) are exactly
				 * same. Discard the whole names.
				 * XXX: ns_samename() above should exclude
				 * this case...
				 */
				qname[0] = 0;
				goto maketarget;
			}
			*np++ = 0x41; /* XXX hardcoding */
			*np++ = nllen = (qlabellen - olabellen);
			if (sft == 0) {
				/*
				 * No alignment issue. can just use memcpy.
				 * Note that the "else" part below contains
				 * this case. We separate the two cases just
				 * for efficiency.
				 * We assume that ns_name_pton above ensures
				 * QP does not contain trailing garbages.
				 */
				memcpy(np, qp + oplen, qplen - oplen);
				np += qplen - oplen;
				*np = 0;
			} else {
				/*
				 * copy the lower (8-SFT) bits of QP to the
				 * upper (8-SFT) bits of NP, then copy the
				 * upper SFT bits of QP+1 to the lower SFT bits
				 * of NP, and so on...
				 * if QP is       xxxyyyyy zzzwww..., then
				 *    NP would be yyyyyzzz ...
				 * Again, we assume QP does not contain
				 * trailing garbages.
				 */
				qp += (oplen - 1);
				while (nllen > 0) {
					*np = (*qp << sft) & 0xff;
					if ((nllen -= (8 - sft)) <= 0)
						break; /* done */
					qp++;
					*np |= ((*qp >> sft) & 0xff);
					np++;
					nllen -= sft;
				}
				*++np = 0;
			}

			/*
			 * make a new bitlabel with the remaining bits.
			 * Note that there's no buffer boundary issue, since
			 * qlabel, olabel, and newlabel all have the same size.
			 * ns_name_ntop() must not return 0, since we have
			 * a non-empty bitlabel.
			 */
			if ((n = ns_name_ntop(newlabel, qname, sizeof(qname)))
			     <= 0)
				return(NULL);
			bufsiz -= n;
			if (qname[n - 1] != '.') { /* XXX no trailing dot */
				qname[n - 1] = '.';
				qname[n] = 0;
				bufsiz--;
			}

		}
	}

  maketarget:
	/*
	 * Finally, append the remaining part (maybe empty) to the new target.
	 */
	if (bufsiz < (int)strlen(target)) /* bufsiz takes care of the \0. */
		return(NULL);
	strcat(qname, target);

	return((const char *)qname);
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
	   int af, int size,	/* meaningless for addrinfo cases */
	   struct addrinfo **ret_aip, const struct addrinfo *pai)
{
	struct pvt *pvt = (struct pvt *)this->private;
	int type, class, ancount, qdcount, n, haveanswer, had_error;
	int error = NETDB_SUCCESS, arcount;
	int (*name_ok)(const char *);
	const HEADER *hp;
	const u_char *eom;
	const u_char *eor;
	const u_char *cp;
	const char *tname;
	const char *hname;
	char *bp, *ep, **ap, **hap;
	char tbuf[MAXDNAME+1];
	struct addrinfo sentinel, *cur, ai;
	const u_char *arp = NULL;

	if (pai == NULL) abort();
	if (ret_aip != NULL)
		*ret_aip = NULL;
	memset(&sentinel, 0, sizeof(sentinel));
	cur = &sentinel;

	tname = qname;
	eom = ansbuf + anslen;
	switch (qtype) {
	case ns_t_a6:
	case T_A:
	case T_AAAA:
	case T_ANY:	/* use T_ANY only for T_A/T_AAAA lookup */
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
	hname = pvt->host.h_name = NULL;

	/*
	 * Find first satisfactory answer.
	 */
	if (ansbuf + HFIXEDSZ > eom) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	hp = (const HEADER *)ansbuf;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	arcount = ntohs(hp->arcount);
	bp = pvt->hostbuf;
	ep = pvt->hostbuf + sizeof(pvt->hostbuf);
	cp = ansbuf + HFIXEDSZ;
	if (qdcount != 1) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	n = dn_expand(ansbuf, eom, cp, bp, ep - bp);
	if (n < 0 || !maybe_ok(pvt->res, bp, name_ok)) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	cp += n + QFIXEDSZ;
	if (cp > eom) {
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
		return (NULL);
	}
	if (qtype == T_A || qtype == T_AAAA ||
	    qtype == ns_t_a6 || qtype == T_ANY) {
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
		hname = bp;
		bp += n;
		/* The qname can be abbreviated, but hname is now absolute. */
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
		n = dn_expand(ansbuf, eom, cp, bp, ep - bp);
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
		eor = cp + n;
		if ((qtype == T_A || qtype == T_AAAA || qtype == ns_t_a6 ||
		     qtype == T_ANY) && type == T_CNAME) {
			if (haveanswer) {
				int level = LOG_CRIT;
#ifdef LOG_SECURITY
				level |= LOG_SECURITY;
#endif
				syslog(level,
 "gethostans: possible attempt to exploit buffer overflow while looking up %s",
					*qname ? qname : ".");
			}
			n = dn_expand(ansbuf, eor, cp, tbuf, sizeof tbuf);
			if (n < 0 || !maybe_ok(pvt->res, tbuf, name_ok)) {
				had_error++;
				continue;
			}
			cp += n;
			/* Store alias. */
			if (ap >= &pvt->host_aliases[MAXALIASES-1])
				continue;
			*ap++ = bp;
			n = strlen(bp) + 1;	/* for the \0 */
			bp += n;
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > (ep - bp) || n > MAXHOSTNAMELEN) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			pvt->host.h_name = bp;
			hname = bp;
			bp += n;
			continue;
		}
		if (type == ns_t_dname) {
			const char *t0, *t;

			/*
			 * just replace the query target; do not update the
			 * alias list. (Or should we?)
			 */
			t0 = (qtype == T_PTR) ? tname : hname;

			n = dn_expand(ansbuf, eor, cp, tbuf, sizeof(tbuf));
			if (n < 0 || !maybe_dnok(pvt->res, tbuf)) {
				had_error++;
				continue;
			}
#ifdef RES_USE_DNAME
			if ((pvt ->res->options & RES_USE_DNAME) == 0) {
				cp += n;
				continue;
			}
#endif
			if ((t = dname_subst(t0, bp, tbuf)) == NULL) {
				cp += n;
				continue;
			}
#if 0				/* for debug */
			if ((pvt->res->options & RES_DEBUG) != 0) {
				printf("DNAME owner=%s, target=%s, next=%s\n",
				       bp, tbuf, t);
			}
#endif
			cp += n;

			n = strlen(t) + 1; /* for the \0 */
			if (n > (ep - bp)) {
				had_error++;
				continue;
			}
			strcpy(bp, t);
			if (qtype == T_PTR)
				tname = bp;
			else
				hname = bp;
			bp += n;

			continue;
		}
		if (qtype == T_PTR && type == T_CNAME) {
			n = dn_expand(ansbuf, eor, cp, tbuf, sizeof tbuf);
			if (n < 0 || !maybe_dnok(pvt->res, tbuf)) {
				had_error++;
				continue;
			}
			cp += n;
#ifdef RES_USE_DNAME
			if ((pvt->res->options & RES_USE_DNAME) != 0)
#endif
			{
				/*
				 * We may be able to check this regardless
				 * of the USE_DNAME bit, but we add the check
				 * for now since the DNAME support is
				 * experimental.
				 */
				if (ns_samename(tname, bp) != 1)
					continue;
			}
			/* Get canonical name. */
			n = strlen(tbuf) + 1;	/* for the \0 */
			if (n > (ep - bp)) {
				had_error++;
				continue;
			}
			strcpy(bp, tbuf);
			tname = bp;
			bp += n;
			continue;
		}
		if (qtype == T_ANY) {
			if (!(type == T_A || type == T_AAAA ||
			      type == ns_t_a6)) {
				cp += n;
				continue;
			}
		} else if (type != qtype) {
			cp += n;
			continue;
		}
		switch (type) {
		case T_PTR:
			if (ret_aip != NULL) {
				/* addrinfo never needs T_PTR */
				cp += n;
				continue;
			}
			if (ns_samename(tname, bp) != 1) {
				cp += n;
				continue;
			}
			n = dn_expand(ansbuf, eor, cp, bp, ep - bp);
			if (n < 0 || !maybe_hnok(pvt->res, bp) ||
			    n >= MAXHOSTNAMELEN) {
				had_error++;
				break;
			}
			cp += n;
			if (!haveanswer) {
				pvt->host.h_name = bp;
				hname = bp;
			}
			else if (ap < &pvt->host_aliases[MAXALIASES-1])
				*ap++ = bp;
			else
				n = -1;
			if (n != -1) {
				n = strlen(bp) + 1;	/* for the \0 */
				bp += n;
			}
			break;
		case ns_t_a6: {
			struct in6_addr in6;
			struct addrinfo ai;

#ifdef RES_USE_A6
			if ((pvt->res->options & RES_USE_A6) == 0) {
				cp += n;
				continue;
			}
#endif

			if (ns_samename(hname, bp) != 1) {
				cp += n;
				continue;
			}

			/*
			 * search for the top of the additional section.
			 * once found, keep it for the case where we have
			 * more than one A6 record.
			 * XXX: however, we may not need this part.
			 */
			if (arp == NULL && arcount > 0) {
				int nscount = ntohs(hp->nscount);

				arp = ar_head(cp + n, nscount + ancount - 1,
					      ansbuf, eom, pvt, name_ok);
			}

			/* recursively collect the whole A6 chain */
			ai = *pai; /* XXX: we can't override constant pai */
			ai.ai_family = AF_INET6;
			memset(&in6, 0, sizeof(in6)); /* just for safety */
			cur->ai_next = a6_expand(ansbuf, cp, n, arp, eom,
						 &in6, 128,
						 (const struct addrinfo *)&ai,
						 pvt, name_ok, &error);
			if (error != NETDB_SUCCESS) {
#ifdef DEBUG
				/* in this case, cur->ai_next must be NULL. */
				if (cur->ai_next != NULL)
					abort();
#endif
				had_error++;
				continue;
			}

			/*
			 * We don't bother even if cur->ai_next is NULL unless
			 * the expansion failed by a fatal error. The list
			 * can be NULL if the given A6 is incomplete, but we
			 * may have another complete A6 chain in this answer.
			 * See the last paragraph of RFC 2874 Section 3.1.4. 
 			 */
			if (cur->ai_next == NULL) {
				cp += n;
				continue; /* no error, no answer */
			}
			goto convertinfo;
		} /* FALLTHROUGH */
		case T_A:
		case T_AAAA:
			if (ns_samename(hname, bp) != 1) {
				cp += n;
				continue;
			}
			if (type == T_A && n != INADDRSZ) {
				cp += n;
				continue;
			}
			if (type == T_AAAA && n != IN6ADDRSZ) {
				cp += n;
				continue;
			}

			/* make addrinfo. don't overwrite constant PAI */
			ai = *pai;
			ai.ai_family = (type == T_AAAA) ? AF_INET6 : AF_INET;
			cur->ai_next = addr2addrinfo(
					(const struct addrinfo *)&ai,
					(const char *)cp);
			if (cur->ai_next == NULL)
				had_error++;

		  convertinfo:	/* convert addrinfo into hostent form */
			if (!haveanswer) {
				int nn;

				nn = strlen(bp) + 1;	/* for the \0 */
				if (nn >= MAXHOSTNAMELEN) {
					cp += n;
					had_error++;
					continue;
				}
				pvt->host.h_name = bp;
				hname = bp;
				bp += nn;
			}
			/* Ensure alignment. */
			bp = (char *)(((u_long)bp + (sizeof(align) - 1)) &
				      ~(sizeof(align) - 1));
			/* Avoid overflows. */
			if (bp + n >= &pvt->hostbuf[sizeof pvt->hostbuf]) {
				had_error++;
				continue;
			}
			if (ret_aip) { /* need addrinfo. keep it. */
				while (cur && cur->ai_next)
					cur = cur->ai_next;
			} else if (cur->ai_next) { /* need hostent */
				struct addrinfo *aip = cur->ai_next;

				for (aip = cur->ai_next; aip;
				     aip = aip->ai_next) {
					int m;

					m = add_hostent(pvt, bp, hap, aip);
					if (m < 0) {
						had_error++;
						break;
					}
					if (m == 0)
						continue;
					if (hap < &pvt->h_addr_ptrs[MAXADDRS-1])
						hap++;

					bp += m;
				}

				freeaddrinfo(cur->ai_next);
				cur->ai_next = NULL;
			}
			cp += n;
			break;
		default:
			abort();
		}
		if (!had_error)
			haveanswer++;
	}
	if (haveanswer) {
		if (ret_aip == NULL) {
			*ap = NULL;
			*hap = NULL;

			if (pvt->res->nsort && haveanswer > 1 && qtype == T_A)
				addrsort(pvt->res, pvt->h_addr_ptrs,
					 haveanswer);
			if (pvt->host.h_name == NULL) {
				n = strlen(qname) + 1;	/* for the \0 */
				if (n > (ep - bp) || n >= MAXHOSTNAMELEN)
					goto no_recovery;
				strcpy(bp, qname);
				pvt->host.h_name = bp;
				bp += n;
			}
			if (pvt->res->options & RES_USE_INET6)
				map_v4v6_hostent(&pvt->host, &bp, ep);
			RES_SET_H_ERRNO(pvt->res, NETDB_SUCCESS);
			return (&pvt->host);
		} else {
			if ((pai->ai_flags & AI_CANONNAME) != 0) {
				if (pvt->host.h_name == NULL) {
					sentinel.ai_next->ai_canonname =
						strdup(qname);
				}
				else {
					sentinel.ai_next->ai_canonname =
						strdup(pvt->host.h_name);
				}
			}
			*ret_aip = sentinel.ai_next;
			return(NULL);
		}
	}
 no_recovery:
	if (sentinel.ai_next) {
		/* this should be impossible, but check it for safety */
		freeaddrinfo(sentinel.ai_next);
	}
	if (error == NETDB_SUCCESS)
		RES_SET_H_ERRNO(pvt->res, NO_RECOVERY);
	else
		RES_SET_H_ERRNO(pvt->res, error);
	return(NULL);
}

static int
add_hostent(struct pvt *pvt, char *bp, char **hap, struct addrinfo *ai)
{
	int addrlen;
	char *addrp;
	const char **tap;
	char *obp = bp;

	switch(ai->ai_addr->sa_family) {
	case AF_INET6:
		addrlen = IN6ADDRSZ;
		addrp = (char *)&((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
		break;
	case AF_INET:
		addrlen = INADDRSZ;
		addrp = (char *)&((struct sockaddr_in *)ai->ai_addr)->sin_addr;
		break;
	default:
		return(-1);	/* abort? */
	}

	/* Ensure alignment. */
	bp = (char *)(((u_long)bp + (sizeof(align) - 1)) &
		      ~(sizeof(align) - 1));
	/* Avoid overflows. */
	if (bp + addrlen >= &pvt->hostbuf[sizeof pvt->hostbuf])
		return(-1);
	if (hap >= &pvt->h_addr_ptrs[MAXADDRS-1])
		return(0); /* fail, but not treat it as an error. */

	/* Suppress duplicates. */
	for (tap = (const char **)pvt->h_addr_ptrs;
	     *tap != NULL;
	     tap++)
		if (memcmp(*tap, addrp, addrlen) == 0)
			break;
	if (*tap != NULL)
		return (0);

	memcpy(*hap = bp, addrp, addrlen);
	return((bp + addrlen) - obp);
}

static void
map_v4v6_hostent(struct hostent *hp, char **bpp, char *ep) {
	char **ap;

	if (hp->h_addrtype != AF_INET || hp->h_length != INADDRSZ)
		return;
	hp->h_addrtype = AF_INET6;
	hp->h_length = IN6ADDRSZ;
	for (ap = hp->h_addr_list; *ap; ap++) {
		int i = (u_long)*bpp % sizeof(align);

		if (i != 0)
			i = sizeof(align) - i;

		if ((ep - *bpp) < (i + IN6ADDRSZ)) {
			/* Out of memory.  Truncate address list here. */
			*ap = NULL;
			return;
		}
		*bpp += i;
		map_v4v6_address(*ap, *bpp);
		*ap = *bpp;
		*bpp += IN6ADDRSZ;
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
