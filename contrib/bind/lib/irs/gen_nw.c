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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: gen_nw.c,v 1.13 1999/10/13 16:39:29 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "gen_p.h"

/* Types */

struct pvt {
	struct irs_rule *	rules;
	struct irs_rule *	rule;
	struct __res_state *	res;
	void 			(*free_res)(void *);
};

/* Forward */

static void		nw_close(struct irs_nw*);
static struct nwent *	nw_next(struct irs_nw *);
static struct nwent *	nw_byname(struct irs_nw *, const char *, int);
static struct nwent *	nw_byaddr(struct irs_nw *, void *, int, int);
static void    		nw_rewind(struct irs_nw *);
static void		nw_minimize(struct irs_nw *);
static struct __res_state * nw_res_get(struct irs_nw *this);
static void		nw_res_set(struct irs_nw *this,
				   struct __res_state *res,
				   void (*free_res)(void *));

static int		init(struct irs_nw *this);

/* Public */

struct irs_nw *
irs_gen_nw(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_nw *nw;
	struct pvt *pvt;

	if (!(pvt = memget(sizeof *pvt))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	if (!(nw = memget(sizeof *nw))) {
		memput(pvt, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(nw, 0x5e, sizeof *nw);
	pvt->rules = accpvt->map_rules[irs_nw];
	pvt->rule = pvt->rules;
	nw->private = pvt;
	nw->close = nw_close;
	nw->next = nw_next;
	nw->byname = nw_byname;
	nw->byaddr = nw_byaddr;
	nw->rewind = nw_rewind;
	nw->minimize = nw_minimize;
	nw->res_get = nw_res_get;
	nw->res_set = nw_res_set;
	return (nw);
}

/* Methods */

static void
nw_close(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	nw_minimize(this);

	if (pvt->res && pvt->free_res)
		(*pvt->free_res)(pvt->res);

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct nwent *
nw_next(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct nwent *rval;
	struct irs_nw *nw;

	if (init(this) == -1)
		return(NULL);

	while (pvt->rule) {
		nw = pvt->rule->inst->nw;
		rval = (*nw->next)(nw);
		if (rval)
			return (rval);
		if (!(pvt->rules->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			nw = pvt->rule->inst->nw;
			(*nw->rewind)(nw);
		}
	}
	return (NULL);
}

static struct nwent *
nw_byname(struct irs_nw *this, const char *name, int type) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct nwent *rval;
	struct irs_nw *nw;
	
	if (init(this) == -1)
		return(NULL);

	for (rule = pvt->rules; rule; rule = rule->next) {
		nw = rule->inst->nw;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		rval = (*nw->byname)(nw, name, type);
		if (rval != NULL)
			return (rval);
		if (pvt->res->res_h_errno != TRY_AGAIN &&
		    !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (NULL);
}

static struct nwent *
nw_byaddr(struct irs_nw *this, void *net, int length, int type) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct nwent *rval;
	struct irs_nw *nw;
	
	if (init(this) == -1)
		return(NULL);

	for (rule = pvt->rules; rule; rule = rule->next) {
		nw = rule->inst->nw;
		RES_SET_H_ERRNO(pvt->res, NETDB_INTERNAL);
		rval = (*nw->byaddr)(nw, net, length, type);
		if (rval != NULL)
			return (rval);
		if (pvt->res->res_h_errno != TRY_AGAIN &&
		    !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (NULL);
}

static void
nw_rewind(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_nw *nw;

	pvt->rule = pvt->rules;
	if (pvt->rule) {
		nw = pvt->rule->inst->nw;
		(*nw->rewind)(nw);
	}
}

static void
nw_minimize(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	if (pvt->res)
		res_nclose(pvt->res);
	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_nw *nw = rule->inst->nw;

		(*nw->minimize)(nw);
	}
}

static struct __res_state *
nw_res_get(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		nw_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
nw_res_set(struct irs_nw *this, struct __res_state *res,
		void (*free_res)(void *)) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	if (pvt->res && pvt->free_res) {
		res_nclose(pvt->res);
		(*pvt->free_res)(pvt->res);
	}

	pvt->res = res;
	pvt->free_res = free_res;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_nw *nw = rule->inst->nw;

		(*nw->res_set)(nw, pvt->res, NULL);
	}
}

static int
init(struct irs_nw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (!pvt->res && !nw_res_get(this))
		return (-1);
	if (((pvt->res->options & RES_INIT) == 0) &&
	    res_ninit(pvt->res) == -1)
		return (-1);
	return (0);
}
