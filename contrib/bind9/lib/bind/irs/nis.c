/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
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
static const char rcsid[] = "$Id: nis.c,v 1.1.206.1 2004/03/09 08:33:38 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifdef WANT_IRS_NIS

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
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
#include "hesiod.h"
#include "nis_p.h"

/* Forward */

static void		nis_close(struct irs_acc *);
static struct __res_state * nis_res_get(struct irs_acc *);
static void		nis_res_set(struct irs_acc *, struct __res_state *,
				void (*)(void *));

/* Public */

struct irs_acc *
irs_nis_acc(const char *options) {
	struct nis_p *nis;
	struct irs_acc *acc;
	char *domain;

	UNUSED(options);

	if (yp_get_default_domain(&domain) != 0)
		return (NULL);
	if (!(nis = memget(sizeof *nis))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(nis, 0, sizeof *nis);
	if (!(acc = memget(sizeof *acc))) {
		memput(nis, sizeof *nis);
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	acc->private = nis;
	nis->domain = strdup(domain);
#ifdef WANT_IRS_GR
	acc->gr_map = irs_nis_gr;
#else
	acc->gr_map = NULL;
#endif
#ifdef WANT_IRS_PW
	acc->pw_map = irs_nis_pw;
#else
	acc->pw_map = NULL;
#endif
	acc->sv_map = irs_nis_sv;
	acc->pr_map = irs_nis_pr;
	acc->ho_map = irs_nis_ho;
	acc->nw_map = irs_nis_nw;
	acc->ng_map = irs_nis_ng;
	acc->res_get = nis_res_get;
	acc->res_set = nis_res_set;
	acc->close = nis_close;
	return (acc);
}

/* Methods */

static struct __res_state *
nis_res_get(struct irs_acc *this) {
	struct nis_p *nis = (struct nis_p *)this->private;

	if (nis->res == NULL) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (res == NULL)
			return (NULL);
		memset(res, 0, sizeof *res);
		nis_res_set(this, res, free);
	}

	if ((nis->res->options & RES_INIT) == 0 &&
	    res_ninit(nis->res) < 0)
		return (NULL);

	return (nis->res);
}

static void
nis_res_set(struct irs_acc *this, struct __res_state *res,
		void (*free_res)(void *)) {
	struct nis_p *nis = (struct nis_p *)this->private;

	if (nis->res && nis->free_res) {
		res_nclose(nis->res);
		(*nis->free_res)(nis->res);
	}

	nis->res = res;
	nis->free_res = free_res;
}

static void
nis_close(struct irs_acc *this) {
	struct nis_p *nis = (struct nis_p *)this->private;

	if (nis->res && nis->free_res)
		(*nis->free_res)(nis->res);
	free(nis->domain);
	memput(nis, sizeof *nis);
	memput(this, sizeof *this);
}

#endif /*WANT_IRS_NIS*/
