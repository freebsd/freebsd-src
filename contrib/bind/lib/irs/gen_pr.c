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

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: gen_pr.c,v 1.8 1997/12/04 04:57:51 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "gen_p.h"

/* Types */

struct pvt {
	struct irs_rule *	rules;
	struct irs_rule *	rule;
};

/* Forward */

static void			pr_close(struct irs_pr*);
static struct protoent *	pr_next(struct irs_pr *);
static struct protoent *	pr_byname(struct irs_pr *, const char *);
static struct protoent * 	pr_bynumber(struct irs_pr *, int);
static void 			pr_rewind(struct irs_pr *);
static void			pr_minimize(struct irs_pr *);

/* Public */

struct irs_pr *
irs_gen_pr(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_pr *pr;
	struct pvt *pvt;

	if (!(pr = (struct irs_pr *)malloc(sizeof *pr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pr, 0x5e, sizeof *pr);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(pr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->rules = accpvt->map_rules[irs_pr];
	pvt->rule = pvt->rules;
	pr->private = pvt;
	pr->close = pr_close;
	pr->next = pr_next;
	pr->byname = pr_byname;
	pr->bynumber = pr_bynumber;
	pr->rewind = pr_rewind;
	pr->minimize = pr_minimize;
	return (pr);
}

/* Methods */

static void
pr_close(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	free(pvt);
	free(this);
}

static struct protoent *
pr_next(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct protoent *rval;
	struct irs_pr *pr;

	while (pvt->rule) {
		pr = pvt->rule->inst->pr;
		rval = (*pr->next)(pr);
		if (rval)
			return (rval);
		if (!(pvt->rules->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			pr = pvt->rule->inst->pr;
			(*pr->rewind)(pr);
		}
	}
	return (NULL);
}

static struct protoent *
pr_byname(struct irs_pr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct protoent *rval;
	struct irs_pr *pr;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		pr = rule->inst->pr;
		rval = (*pr->byname)(pr, name);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static struct protoent *
pr_bynumber(struct irs_pr *this, int proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct protoent *rval;
	struct irs_pr *pr;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		pr = rule->inst->pr;
		rval = (*pr->bynumber)(pr, proto);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static void
pr_rewind(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_pr *pr;

	pvt->rule = pvt->rules;
	if (pvt->rule) {
		pr = pvt->rule->inst->pr;
		(*pr->rewind)(pr);
	}
}

static void
pr_minimize(struct irs_pr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_pr *pr = rule->inst->pr;

		(*pr->minimize)(pr);
	}
}
