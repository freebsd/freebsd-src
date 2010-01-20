/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$Id: nis_ng.c,v 1.3.18.1 2005/04/27 05:01:03 sra Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_NIS
static int __bind_irs_nis_unneeded;
#else

#include <sys/types.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <isc/assertions.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#ifdef T_NULL
#undef T_NULL			/* Silence re-definition warning of T_NULL. */
#endif
#include <arpa/nameser.h>
#include <resolv.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "nis_p.h"

/* Definitions */

struct tmpgrp {
	const char *	name;
	const char *	host;
	const char *	user;
	const char *	domain;
	struct tmpgrp *	next;
};

struct pvt {
	char *		nis_domain;
	struct tmpgrp *	tmp;
	struct tmpgrp *	cur;
	char *		tmpgroup;
};

enum do_what { do_none = 0x0, do_key = 0x1, do_val = 0x2, do_all = 0x3 };

static /*const*/ char netgroup_map[]	= "netgroup";

/* Forward */

static void 		ng_close(struct irs_ng *);
static int		ng_next(struct irs_ng *, const char **,
				const char **, const char **);
static int		ng_test(struct irs_ng *,
 				const char *, const char *,
				const char *, const char *);
static void		ng_rewind(struct irs_ng *, const char *);
static void		ng_minimize(struct irs_ng *);

static void		add_group_to_list(struct pvt *, const char *, int);
static void		add_tuple_to_list(struct pvt *, const char *, char *);
static void		tmpfree(struct pvt *);

/* Public */

struct irs_ng *
irs_nis_ng(struct irs_acc *this) {
	struct irs_ng *ng;
	struct pvt *pvt;

	if (!(ng = memget(sizeof *ng))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(ng, 0x5e, sizeof *ng);
	if (!(pvt = memget(sizeof *pvt))) {
		memput(ng, sizeof *ng);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->nis_domain = ((struct nis_p *)this->private)->domain;
	ng->private = pvt;
	ng->close = ng_close;
	ng->next = ng_next;
	ng->test = ng_test;
	ng->rewind = ng_rewind;
	ng->minimize = ng_minimize;
	return (ng);
}

/* Methods */

static void
ng_close(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	tmpfree(pvt);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static int
ng_next(struct irs_ng *this, const char **host, const char **user, const char **domain) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->cur)
		return (0);
	*host = pvt->cur->host;
	*user = pvt->cur->user;
	*domain = pvt->cur->domain;
	pvt->cur = pvt->cur->next;
	return (1);
}

static int
ng_test(struct irs_ng *this, const char *name,
	const char *host, const char *user, const char *domain)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct tmpgrp *cur;

	tmpfree(pvt);
	add_group_to_list(pvt, name, strlen(name));
	for (cur = pvt->tmp; cur; cur = cur->next) {
		if ((!host || !cur->host || !strcmp(host, cur->host)) &&
		    (!user || !cur->user || !strcmp(user, cur->user)) &&
		    (!domain || !cur->domain || !strcmp(domain, cur->domain)))
			break;
	}
	tmpfree(pvt);
	return ((cur == NULL) ? 0 : 1);
}

static void
ng_rewind(struct irs_ng *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;

	/* Either hand back or free the existing list. */
	if (pvt->tmpgroup) {
		if (pvt->tmp && !strcmp(pvt->tmpgroup, name))
			goto reset;
		tmpfree(pvt);
	}
	pvt->tmpgroup = strdup(name);
	add_group_to_list(pvt, name, strlen(name));
 reset:
	pvt->cur = pvt->tmp;
}

static void
ng_minimize(struct irs_ng *this) {
	UNUSED(this);
	/* NOOP */
}

/* Private */

static void
add_group_to_list(struct pvt *pvt, const char *name, int len) {
	char *vdata, *cp, *np;
	struct tmpgrp *tmp;
	int vlen, r;
	char *nametmp;

	/* Don't add the same group to the list more than once. */
	for (tmp = pvt->tmp; tmp; tmp = tmp->next)
		if (!strcmp(tmp->name, name))
			return;

	DE_CONST(name, nametmp);
	r = yp_match(pvt->nis_domain, netgroup_map, nametmp, len,
		     &vdata, &vlen);
	if (r == 0) {
		cp = vdata;
		if (*cp && cp[strlen(cp)-1] == '\n')
                  cp[strlen(cp)-1] = '\0';
		for ( ; cp; cp = np) {
			np = strchr(cp, ' ');
			if (np)
				*np++ = '\0';
			if (*cp == '(')
				add_tuple_to_list(pvt, name, cp);
			else
				add_group_to_list(pvt, cp, strlen(cp));
		}
		free(vdata);
	}
}

static void
add_tuple_to_list(struct pvt *pvt, const char *name, char *cp) {
	struct tmpgrp *tmp;
	char *tp, *np;

	INSIST(*cp++ == '(');

	tmp = malloc(sizeof *tmp + strlen(name) + sizeof '\0' +
		     strlen(cp) - sizeof ')');
	if (!tmp)
		return;
	memset(tmp, 0, sizeof *tmp);
	tp = ((char *)tmp) + sizeof *tmp;

	/* Name */
	strcpy(tp, name);
	tmp->name = tp;
	tp += strlen(tp) + 1;

	/* Host */
	if (!(np = strchr(cp, ',')))
		goto cleanup;
	*np++ = '\0';
	strcpy(tp, cp);
	tmp->host = tp;
	tp += strlen(tp) + 1;
	cp = np;

	/* User */
	if (!(np = strchr(cp, ',')))
		goto cleanup;
	*np++ = '\0';
	strcpy(tp, cp);
	tmp->user = tp;
	tp += strlen(tp) + 1;
	cp = np;

	/* Domain */
	if (!(np = strchr(cp, ')')))
		goto cleanup;
	*np++ = '\0';
	strcpy(tp, cp);
	tmp->domain = tp;

	/*
	 * Empty string in file means wildcard, but
	 * NULL string in return value means wildcard.
	 */
	if (!*tmp->host)
		tmp->host = NULL;
	if (!*tmp->user)
		tmp->user = NULL;
	if (!*tmp->domain)
		tmp->domain = NULL;

	/* Add to list (LIFO). */
	tmp->next = pvt->tmp;
	pvt->tmp = tmp;
	return;

 cleanup:
	free(tmp);
}

static void
tmpfree(struct pvt *pvt) {
	struct tmpgrp *cur, *next;

	if (pvt->tmpgroup) {
		free(pvt->tmpgroup);
		pvt->tmpgroup = NULL;
	}
	for (cur = pvt->tmp; cur; cur = next) {
		next = cur->next;
		free(cur);
	}
	pvt->tmp = NULL;
}

#endif /*WANT_IRS_NIS*/

/*! \file */
