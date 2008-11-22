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
static const char rcsid[] = "$Id: gen_ng.c,v 1.1.206.1 2004/03/09 08:33:35 marka Exp $";
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
	char *			curgroup;
};

/* Forward */

static void		ng_close(struct irs_ng *);
static int		ng_next(struct irs_ng *, const char **,
				const char **, const char **);
static int 		ng_test(struct irs_ng *, const char *,
				const char *, const char *,
				const char *);
static void 		ng_rewind(struct irs_ng *, const char *);
static void		ng_minimize(struct irs_ng *);

/* Public */

struct irs_ng *
irs_gen_ng(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
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
	pvt->rules = accpvt->map_rules[irs_ng];
	pvt->rule = pvt->rules;
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
	
	ng_minimize(this);
	if (pvt->curgroup)
		free(pvt->curgroup);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static int
ng_next(struct irs_ng *this, const char **host, const char **user,
	const char **domain)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_ng *ng;
	
	while (pvt->rule) {
		ng = pvt->rule->inst->ng;
		if ((*ng->next)(ng, host, user, domain) == 1)
			return (1);
		if (!(pvt->rule->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			ng = pvt->rule->inst->ng;
			(*ng->rewind)(ng, pvt->curgroup);
		}
	}
	return (0);
}

static int
ng_test(struct irs_ng *this, const char *name,
	const char *user, const char *host, const char *domain)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct irs_ng *ng;
	int rval;
	
	rval = 0;
	for (rule = pvt->rules; rule; rule = rule->next) {
		ng = rule->inst->ng;
		rval = (*ng->test)(ng, name, user, host, domain);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static void
ng_rewind(struct irs_ng *this, const char *group) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_ng *ng;
	
	pvt->rule = pvt->rules;
	if (pvt->rule) {
		if (pvt->curgroup)
			free(pvt->curgroup);
		pvt->curgroup = strdup(group);
		ng = pvt->rule->inst->ng;
		(*ng->rewind)(ng, pvt->curgroup);
	}
}

static void
ng_minimize(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_ng *ng = rule->inst->ng;

		(*ng->minimize)(ng);
	}
}
