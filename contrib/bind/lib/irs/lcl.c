/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: lcl.c,v 1.16 2000/02/28 07:52:16 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <netinet/in.h> 
#include <arpa/nameser.h>
#include <resolv.h>

#include <isc/memcluster.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

/* Forward. */

static void		lcl_close(struct irs_acc *);
static struct __res_state *	lcl_res_get(struct irs_acc *);
static void		lcl_res_set(struct irs_acc *, struct __res_state *,
				void (*)(void *));

/* Public */

struct irs_acc *
irs_lcl_acc(const char *options) {
	struct irs_acc *acc;
	struct lcl_p *lcl;

	if (!(acc = memget(sizeof *acc))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	if (!(lcl = memget(sizeof *lcl))) {
		errno = ENOMEM;
		free(acc);
		return (NULL);
	}
	memset(lcl, 0x5e, sizeof *lcl);
	lcl->res = NULL;
	lcl->free_res = NULL;
	acc->private = lcl;
#ifdef WANT_IRS_GR
	acc->gr_map = irs_lcl_gr;
#else
	acc->gr_map = NULL;
#endif
#ifdef WANT_IRS_PW
	acc->pw_map = irs_lcl_pw;
#else
	acc->pw_map = NULL;
#endif
	acc->sv_map = irs_lcl_sv;
	acc->pr_map = irs_lcl_pr;
	acc->ho_map = irs_lcl_ho;
	acc->nw_map = irs_lcl_nw;
	acc->ng_map = irs_lcl_ng;
	acc->res_get = lcl_res_get;
	acc->res_set = lcl_res_set;
	acc->close = lcl_close;
	return (acc);
}

/* Methods */
static struct __res_state *
lcl_res_get(struct irs_acc *this) {
	struct lcl_p *lcl = (struct lcl_p *)this->private;

	if (lcl->res == NULL) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (res == NULL)
			return (NULL);
		memset(res, 0, sizeof *res);
		lcl_res_set(this, res, free);
	}

	if ((lcl->res->options & RES_INIT) == 0 &&
	    res_ninit(lcl->res) < 0)
		return (NULL);

	return (lcl->res);
}

static void
lcl_res_set(struct irs_acc *this, struct __res_state *res,
		void (*free_res)(void *)) {
	struct lcl_p *lcl = (struct lcl_p *)this->private;

	if (lcl->res && lcl->free_res) {
		res_nclose(lcl->res);
		(*lcl->free_res)(lcl->res);
	}

	lcl->res = res;
	lcl->free_res = free_res;
}

static void
lcl_close(struct irs_acc *this) {
	struct lcl_p *lcl = (struct lcl_p *)this->private;

	if (lcl) {
		if (lcl->free_res)
			(*lcl->free_res)(lcl->res);
		memput(lcl, sizeof *lcl);
	}
	memput(this, sizeof *this);
}
