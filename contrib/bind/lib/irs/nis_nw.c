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
static const char rcsid[] = "$Id: nis_nw.c,v 1.10 1997/12/04 04:58:00 halley Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_NIS
static int __bind_irs_nis_unneeded;
#else

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

/* Definitions */

#define MAXALIASES 35
#define MAXADDRSIZE 4

struct pvt {
	int		needrewind;
	char *		nis_domain;
	char *		curkey_data;
	int		curkey_len;
	char *		curval_data;
	int		curval_len;

	struct nwent 	nwent;
	char *		nwbuf;

	char *		aliases[MAXALIASES + 1];
	u_char		addr[MAXADDRSIZE];
};

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static /*const*/ char networks_byname[] = "networks.byname";
static /*const*/ char networks_byaddr[] = "networks.byaddr";

/* Forward */

static void 		nw_close(struct irs_nw *);
static struct nwent *	nw_byname(struct irs_nw *, const char *, int);
static struct nwent *	nw_byaddr(struct irs_nw *, void *, int, int);
static struct nwent *	nw_next(struct irs_nw *);
static void		nw_rewind(struct irs_nw *);
static void		nw_minimize(struct irs_nw *);

static struct nwent *	makenwent(struct irs_nw *this);
static void		nisfree(struct pvt *, enum do_what);

/* Public */

struct irs_nw *
irs_nis_nw(struct irs_acc *this) {
	struct irs_nw *nw;
	struct pvt *pvt;

	if (!(nw = (struct irs_nw *)malloc(sizeof *nw))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(nw, 0x5e, sizeof *nw);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(nw);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->needrewind = 1;
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
	nw->private = pvt;
	nw->close = nw_close;
	nw->byname = nw_byname;
	nw->byaddr = nw_byaddr;
	nw->next = nw_next;
	nw->rewind = nw_rewind;
	nw->minimize = nw_minimize;
	return (nw);
}

/* Methods */

static void
nw_close(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;	

	if (pvt->nwbuf)
		free(pvt->nwbuf);
	free(pvt);
	free(this);
}

static struct nwent *
nw_byaddr(struct irs_nw *this, void *net, int length, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	char tmp[sizeof "255.255.255.255/32"], *t;
	int r;

	if (af != AF_INET) {
		h_errno = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return (NULL);
	}
	nisfree(pvt, do_val);
	/* Try it with /CIDR first. */
	if (inet_net_ntop(AF_INET, net, length, tmp, sizeof tmp) == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	r = yp_match(pvt->nis_domain, networks_byaddr, tmp, strlen(tmp),
		     &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		/* Give it a shot without the /CIDR. */
		if ((t = strchr(tmp, '/')) != NULL) {
			*t = '\0';
			r = yp_match(pvt->nis_domain, networks_byaddr,
				     tmp, strlen(tmp),
				     &pvt->curval_data, &pvt->curval_len);
		}
		if (r != 0) {
			h_errno = HOST_NOT_FOUND;
			return (NULL);
		}
	}
	return (makenwent(this));
}

static struct nwent *
nw_byname(struct irs_nw *this, const char *name, int af) {
	struct pvt *pvt = (struct pvt *)this->private;
	int r;
	
	if (af != AF_INET) {
		h_errno = NETDB_INTERNAL;
		errno = EAFNOSUPPORT;
		return (NULL);
	}
	nisfree(pvt, do_val);
	r = yp_match(pvt->nis_domain, networks_byname, (char *)name,
		     strlen(name), &pvt->curval_data, &pvt->curval_len);
	if (r != 0) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	return (makenwent(this));
}

static void
nw_rewind(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->needrewind = 1;
}

static struct nwent *
nw_next(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct nwent *rval;
	int r;

	do {
		if (pvt->needrewind) {
			nisfree(pvt, do_all);
			r = yp_first(pvt->nis_domain, networks_byaddr,
				     &pvt->curkey_data, &pvt->curkey_len,
				     &pvt->curval_data, &pvt->curval_len);
			pvt->needrewind = 0;
		} else {
			char *newkey_data;
			int newkey_len;

			nisfree(pvt, do_val);
			r = yp_next(pvt->nis_domain, networks_byaddr,
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
		rval = makenwent(this);
	} while (rval == NULL);
	return (rval);
}

static void
nw_minimize(struct irs_nw *this) {
	/* NOOP */
}

/* Private */

static struct nwent *
makenwent(struct irs_nw *this) {
	static const char spaces[] = " \t";
	struct pvt *pvt = (struct pvt *)this->private;
	char *t, *cp, **ap;

	if (pvt->nwbuf)
		free(pvt->nwbuf);
	pvt->nwbuf = pvt->curval_data;
	pvt->curval_data = NULL;

	if ((cp = strchr(pvt->nwbuf, '#')) != NULL)
		*cp = '\0';
	cp = pvt->nwbuf;

	/* Name */
	pvt->nwent.n_name = cp;
	cp += strcspn(cp, spaces);
	if (!*cp)
		goto cleanup;
	*cp++ = '\0';
	cp += strspn(cp, spaces);

	/* Network */
	pvt->nwent.n_addrtype = AF_INET;
	t = cp + strcspn(cp, spaces);
	if (*t)
		*t++ = '\0';
	pvt->nwent.n_length = inet_net_pton(AF_INET, cp,
					    pvt->addr, sizeof pvt->addr);
	if (pvt->nwent.n_length < 0)
		goto cleanup;
	pvt->nwent.n_addr = pvt->addr;
	cp = t;

	/* Aliases */
	ap = pvt->nwent.n_aliases = pvt->aliases;
	while (*cp) {
		if (ap >= &pvt->aliases[MAXALIASES])
			break;
		*ap++ = cp;
		cp += strcspn(cp, spaces);
		if (!*cp)
			break;
		*cp++ = '\0';
		cp += strspn(cp, spaces);
	}
	*ap = NULL;

	return (&pvt->nwent);

 cleanup:
	if (pvt->nwbuf) {
		free(pvt->nwbuf);
		pvt->nwbuf = NULL;
	}
	return (NULL);
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
