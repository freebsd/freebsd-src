/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: nis_sv.c,v 1.14 1999/01/18 07:46:59 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_NIS
static int __bind_irs_nis_unneeded;
#else

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

/* Definitions */

struct pvt {
	int		needrewind;
	char *		nis_domain;
	char *		curkey_data;
	int		curkey_len;
	char *		curval_data;
	int		curval_len;
	char		line[BUFSIZ+1];
	struct servent	serv;
	char *		svbuf;
};

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static /*const*/ char services_byname[] = "services.byname";

/* Forward */

static void			sv_close(struct irs_sv*);
static struct servent *		sv_next(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *, const char *,
					  const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);

static struct servent *		makeservent(struct irs_sv *this);
static void			nisfree(struct pvt *, enum do_what);

/* Public */

struct irs_sv *
irs_nis_sv(struct irs_acc *this) {
	struct irs_sv *sv;
	struct pvt *pvt;
	
	if (!(sv = memget(sizeof *sv))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(sv, 0x5e, sizeof *sv);
	if (!(pvt = memget(sizeof *pvt))) {
		memput(sv, sizeof *sv);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->needrewind = 1;
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
	sv->private = pvt;
	sv->close = sv_close;
	sv->next = sv_next;
	sv->byname = sv_byname;
	sv->byport = sv_byport;
	sv->rewind = sv_rewind;
	sv->minimize = sv_minimize;
	sv->res_get = NULL;
	sv->res_set = NULL;
	return (sv);
}

/* Methods */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	nisfree(pvt, do_all);
	if (pvt->serv.s_aliases)
		free(pvt->serv.s_aliases);
	if (pvt->svbuf)
		free(pvt->svbuf);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
	struct servent *serv;
	char **sap;

	sv_rewind(this);
	while ((serv = sv_next(this)) != NULL) {
		if (proto != NULL && strcmp(proto, serv->s_proto))
			continue;
		if (!strcmp(name, serv->s_name))
			break;
		for (sap = serv->s_aliases; sap && *sap; sap++)
			if (!strcmp(name, *sap))
				break;
	}
	return (serv);
}

static struct servent *
sv_byport(struct irs_sv *this, int port, const char *proto) {
	struct servent *serv;

	sv_rewind(this);
	while ((serv = sv_next(this)) != NULL) {
		if (proto != NULL && strcmp(proto, serv->s_proto))
			continue;
		if (serv->s_port == port)
			break;
	}
	return (serv);
}

static void
sv_rewind(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	pvt->needrewind = 1;
}

static struct servent *
sv_next(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct servent *rval;
	int r;

	do {
		if (pvt->needrewind) {
			nisfree(pvt, do_all);
			r = yp_first(pvt->nis_domain, services_byname,
				     &pvt->curkey_data, &pvt->curkey_len,
				     &pvt->curval_data, &pvt->curval_len);
			pvt->needrewind = 0;
		} else {
			char *newkey_data;
			int newkey_len;

			nisfree(pvt, do_val);
			r = yp_next(pvt->nis_domain, services_byname,
				    pvt->curkey_data, pvt->curkey_len,
				    &newkey_data, &newkey_len,
				    &pvt->curval_data, &pvt->curval_len);
			nisfree(pvt, do_key);
			pvt->curkey_data = newkey_data;
			pvt->curkey_len = newkey_len;
		}
		if (r != 0) {
			errno = ENOENT;
			return (NULL);
		}
		rval = makeservent(this);
	} while (rval == NULL);
	return (rval);
}

static void
sv_minimize(struct irs_sv *this) {
	/* NOOP */
}

/* Private */

static struct servent *
makeservent(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	static const char spaces[] = " \t";
	char *p, **t;
	int n, m;

	if (pvt->svbuf)
		free(pvt->svbuf);
	pvt->svbuf = pvt->curval_data;
	pvt->curval_data = NULL;

	if (pvt->serv.s_aliases) {
		free(pvt->serv.s_aliases);
		pvt->serv.s_aliases = NULL;
	}

	if ((p = strpbrk(pvt->svbuf, "#\n")))
		*p = '\0';

	p = pvt->svbuf;

	pvt->serv.s_name = p;
	p += strcspn(p, spaces);
	if (!*p)
		goto cleanup;
	*p++ = '\0';
	p += strspn(p, spaces);

	pvt->serv.s_port = htons((u_short) atoi(p));
	pvt->serv.s_proto = NULL;
	
	while (*p && !isspace(*p))
		if (*p++ == '/')
			pvt->serv.s_proto = p;
	if (!pvt->serv.s_proto)
		goto cleanup;
	if (*p) {
		*p++ = '\0';
		p += strspn(p, spaces);
	}

	n = m = 0;
	while (*p) {
		if ((n + 1) >= m || !pvt->serv.s_aliases) {
			m += 10;
			t = realloc(pvt->serv.s_aliases, m * sizeof(char *));
			if (!t) {
				errno = ENOMEM;
				goto cleanup;
			}
			pvt->serv.s_aliases = t;
		}
		pvt->serv.s_aliases[n++] = p;
		p += strcspn(p, spaces);
		if (!*p)
			break;
		*p++ = '\0';
		p += strspn(p, spaces);
	}
	if (!pvt->serv.s_aliases)
		pvt->serv.s_aliases = malloc(sizeof(char *));
	if (!pvt->serv.s_aliases)
		goto cleanup;
	pvt->serv.s_aliases[n] = NULL;
	return (&pvt->serv);
	
 cleanup:
	if (pvt->serv.s_aliases) {
		free(pvt->serv.s_aliases);
		pvt->serv.s_aliases = NULL;
	}
	if (pvt->svbuf) {
		free(pvt->svbuf);
		pvt->svbuf = NULL;
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
