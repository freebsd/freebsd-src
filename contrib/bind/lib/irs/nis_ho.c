/*
 * Copyright (c) 1996 by Internet Software Consortium.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$Id: nis_ho.c,v 1.10 1997/12/04 04:57:59 halley Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_NIS
static int __bind_irs_nis_unneeded;
#else

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

extern int h_errno;

/* Definitions */

#define	MAXALIASES	35
#define	MAXADDRS	35

#if PACKETSZ > 1024
#define	MAXPACKET	PACKETSZ
#else
#define	MAXPACKET	1024
#endif

struct pvt {
	int		needrewind;
	char *		nis_domain;
	char *		curkey_data;
	int		curkey_len;
	char *		curval_data;
	int		curval_len;
	struct hostent	host;
	char *		h_addr_ptrs[MAXADDRS + 1];
	char *		host_aliases[MAXALIASES + 1];
	char		hostbuf[8*1024];
	u_char		host_addr[16];	/* IPv4 or IPv6 */
};

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static const u_char mapped[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0xff,0xff };
static const u_char tunnelled[] = { 0,0, 0,0, 0,0, 0,0, 0,0, 0,0 };
static /*const*/ char hosts_byname[] = "hosts.byname";
static /*const*/ char hosts_byaddr[] = "hosts.byaddr";

/* Forwards */

static void		ho_close(struct irs_ho *this);
static struct hostent *	ho_byname(struct irs_ho *this, const char *name);
static struct hostent *	ho_byname2(struct irs_ho *this, const char *name,
					int af);
static struct hostent *	ho_byaddr(struct irs_ho *this, const void *addr,
				       int len, int af);
static struct hostent *	ho_next(struct irs_ho *this);
static void		ho_rewind(struct irs_ho *this);
static void		ho_minimize(struct irs_ho *this);

static struct hostent *	makehostent(struct irs_ho *this);
static void		nisfree(struct pvt *, enum do_what);

/* Public */

struct irs_ho *
irs_nis_ho(struct irs_acc *this) {
	struct irs_ho *ho;
	struct pvt *pvt;

	if (!(ho = malloc(sizeof *ho))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(ho, 0x5e, sizeof *ho);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(ho);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->needrewind = 1;
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
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

/* Methods */

static void
ho_close(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	nisfree(pvt, do_all);
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
	struct pvt *pvt = (struct pvt *)this->private;
	int r;
	
	nisfree(pvt, do_val);
	r = yp_match(pvt->nis_domain, hosts_byname, (char *)name, strlen(name),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	return (makehostent(this));
}

static struct hostent *
ho_byaddr(struct irs_ho *this, const void *addr, int len, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	char tmp[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
	const u_char *uaddr = addr;
	int r;
	
	if (af == AF_INET6 && len == IN6ADDRSZ &&
	    (!memcmp(uaddr, mapped, sizeof mapped) ||
	     !memcmp(uaddr, tunnelled, sizeof tunnelled))) {
		/* Unmap. */
		addr = (u_char *)addr + sizeof mapped;
		uaddr += sizeof mapped;
		af = AF_INET;
		len = INADDRSZ;
	}
	if (inet_ntop(af, uaddr, tmp, sizeof tmp) == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	nisfree(pvt, do_val);
	r = yp_match(pvt->nis_domain, hosts_byaddr, tmp, strlen(tmp),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	return (makehostent(this));
}

static struct hostent *
ho_next(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct hostent *rval;
	int r;

	do {
		if (pvt->needrewind) {
			nisfree(pvt, do_all);
			r = yp_first(pvt->nis_domain, hosts_byaddr,
				     &pvt->curkey_data, &pvt->curkey_len,
				     &pvt->curval_data, &pvt->curval_len);
			pvt->needrewind = 0;
		} else {
			char *newkey_data;
			int newkey_len;

			nisfree(pvt, do_val);
			r = yp_next(pvt->nis_domain, hosts_byaddr,
				    pvt->curkey_data, pvt->curkey_len,
				    &newkey_data, &newkey_len,
				    &pvt->curval_data, &pvt->curval_len);
			nisfree(pvt, do_key);
			pvt->curkey_data = newkey_data;
			pvt->curkey_len = newkey_len;
		}
		if (r != 0) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
		rval = makehostent(this);
	} while (rval == NULL);
	return (rval);
}

static void
ho_rewind(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->needrewind = 1;
}

static void
ho_minimize(struct irs_ho *this) {
	/* NOOP */
}

/* Private */

static struct hostent *
makehostent(struct irs_ho *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	static const char spaces[] = " \t";
	char *cp, **q, *p;
	int af, len;

	p = pvt->curval_data;
	if ((cp = strchr(p, '#')) != NULL)
		*cp = '\0';
	if (!(cp = strpbrk(p, spaces)))
		return (NULL);
	*cp++ = '\0';
	if ((_res.options & RES_USE_INET6) &&
	    inet_pton(AF_INET6, p, pvt->host_addr) > 0) {
		af = AF_INET6;
		len = IN6ADDRSZ;
	} else if (inet_pton(AF_INET, p, pvt->host_addr) > 0) {
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
		return (NULL);
	}
	pvt->h_addr_ptrs[0] = (char *)pvt->host_addr;
	pvt->h_addr_ptrs[1] = NULL;
	pvt->host.h_addr_list = pvt->h_addr_ptrs;
	pvt->host.h_length = len;
	pvt->host.h_addrtype = af;
	cp += strspn(cp, spaces);
	pvt->host.h_name = cp;
	q = pvt->host.h_aliases = pvt->host_aliases;
	if ((cp = strpbrk(cp, spaces)) != NULL)
		*cp++ = '\0';
	while (cp && *cp) {
		if (*cp == ' ' || *cp == '\t') {
			cp++;
			continue;
		}
		if (q < &pvt->host_aliases[MAXALIASES])
			*q++ = cp;
		if ((cp = strpbrk(cp, spaces)) != NULL)
			*cp++ = '\0';
	}
	*q = NULL;
	h_errno = NETDB_SUCCESS;
	return (&pvt->host);
}

static void
nisfree(struct pvt *pvt, enum do_what do_what) {
	if ((do_what & do_key) && pvt->curkey_data) {
		free(pvt->curkey_data);
		pvt->curkey_data = NULL;
	}
	if ((do_what & do_val) && pvt->curval_data) {
		free(pvt->curval_data);
		pvt->curval_data = NULL;
	}
}

#endif /*WANT_IRS_NIS*/
