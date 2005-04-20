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

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: gen_sv.c,v 1.1.206.1 2004/03/09 08:33:35 marka Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <errno.h>
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
	void			(*free_res)(void *);
};

/* Forward */

static void			sv_close(struct irs_sv*);
static struct servent *		sv_next(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *, const char *,
					  const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);
static struct __res_state *	sv_res_get(struct irs_sv *);
static void			sv_res_set(struct irs_sv *,
					      struct __res_state *,
					      void (*)(void *));

/* Public */

struct irs_sv *
irs_gen_sv(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
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
	pvt->rules = accpvt->map_rules[irs_sv];
	pvt->rule = pvt->rules;
	sv->private = pvt;
	sv->close = sv_close;
	sv->next = sv_next;
	sv->byname = sv_byname;
	sv->byport = sv_byport;
	sv->rewind = sv_rewind;
	sv->minimize = sv_minimize;
	sv->res_get = sv_res_get;
	sv->res_set = sv_res_set;
	return (sv);
}

/* Methods */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct servent *
sv_next(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct servent *rval;
	struct irs_sv *sv;
	
	while (pvt->rule) {
		sv = pvt->rule->inst->sv;
		rval = (*sv->next)(sv);
		if (rval)
			return (rval);
		if (!(pvt->rule->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			sv = pvt->rule->inst->sv;
			(*sv->rewind)(sv);
		}
	}
	return (NULL);
}

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct servent *rval;
	struct irs_sv *sv;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		sv = rule->inst->sv;
		rval = (*sv->byname)(sv, name, proto);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static struct servent *
sv_byport(struct irs_sv *this, int port, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct servent *rval;
	struct irs_sv *sv;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		sv = rule->inst->sv;
		rval = (*sv->byport)(sv, port, proto);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static void
sv_rewind(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_sv *sv;

	pvt->rule = pvt->rules;
	if (pvt->rule) {
		sv = pvt->rule->inst->sv;
		(*sv->rewind)(sv);
	}
}

static void
sv_minimize(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_sv *sv = rule->inst->sv;

		(*sv->minimize)(sv);
	}
}

static struct __res_state *
sv_res_get(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		sv_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
sv_res_set(struct irs_sv *this, struct __res_state *res,
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
		struct irs_sv *sv = rule->inst->sv;

		if (sv->res_set)
			(*sv->res_set)(sv, pvt->res, NULL);
	}
}
