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
static const char rcsid[] = "$Id: gen_pw.c,v 1.14 1999/10/13 16:39:30 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_pw_unneeded;
#else

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <errno.h>
#include <pwd.h>
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

static void			pw_close(struct irs_pw *);
static struct passwd *		pw_next(struct irs_pw *);
static struct passwd *		pw_byname(struct irs_pw *, const char *);
static struct passwd *		pw_byuid(struct irs_pw *, uid_t);
static void			pw_rewind(struct irs_pw *);
static void			pw_minimize(struct irs_pw *);
static struct __res_state *	pw_res_get(struct irs_pw *);
static void			pw_res_set(struct irs_pw *,
					   struct __res_state *,
					   void (*)(void *));

/* Public */

struct irs_pw *
irs_gen_pw(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_pw *pw;
	struct pvt *pvt;

	if (!(pw = memget(sizeof *pw))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pw, 0x5e, sizeof *pw);
	if (!(pvt = memget(sizeof *pvt))) {
		memput(pw, sizeof *pvt);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->rules = accpvt->map_rules[irs_pw];
	pvt->rule = pvt->rules;
	pw->private = pvt;
	pw->close = pw_close;
	pw->next = pw_next;
	pw->byname = pw_byname;
	pw->byuid = pw_byuid;
	pw->rewind = pw_rewind;
	pw->minimize = pw_minimize;
	pw->res_get = pw_res_get;
	pw->res_set = pw_res_set;
	return (pw);
}

/* Methods */

static void
pw_close(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct passwd *
pw_next(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct passwd *rval;
	struct irs_pw *pw;
	
	while (pvt->rule) {
		pw = pvt->rule->inst->pw;
		rval = (*pw->next)(pw);
		if (rval)
			return (rval);
		if (!(pvt->rule->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			pw = pvt->rule->inst->pw;
			(*pw->rewind)(pw);
		}
	}
	return (NULL);
}

static void
pw_rewind(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_pw *pw;
	
	pvt->rule = pvt->rules;
	if (pvt->rule) {
		pw = pvt->rule->inst->pw;
		(*pw->rewind)(pw);
	}
}

static struct passwd *
pw_byname(struct irs_pw *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct passwd *rval;
	struct irs_pw *pw;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		pw = rule->inst->pw;
		rval = (*pw->byname)(pw, name);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static struct passwd *
pw_byuid(struct irs_pw *this, uid_t uid) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct passwd *rval;
	struct irs_pw *pw;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		pw = rule->inst->pw;
		rval = (*pw->byuid)(pw, uid);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}	

static void
pw_minimize(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_pw *pw = rule->inst->pw;

		(*pw->minimize)(pw);
	}
}

static struct __res_state *
pw_res_get(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		pw_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
pw_res_set(struct irs_pw *this, struct __res_state *res,
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
		struct irs_pw *pw = rule->inst->pw;

		if (pw->res_set)
			(*pw->res_set)(pw, pvt->res, NULL);
	}
}

#endif /* WANT_IRS_PW */
