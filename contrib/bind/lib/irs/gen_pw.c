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
static char rcsid[] = "$Id: gen_pw.c,v 1.10 1997/12/04 04:57:51 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_PW
static int __bind_irs_pw_unneeded;
#else

#include <sys/types.h>

#include <errno.h>
#include <pwd.h>
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

static void			pw_close(struct irs_pw *);
static struct passwd *		pw_next(struct irs_pw *);
static struct passwd *		pw_byname(struct irs_pw *, const char *);
static struct passwd *		pw_byuid(struct irs_pw *, uid_t);
static void			pw_rewind(struct irs_pw *);
static void			pw_minimize(struct irs_pw *);

/* Public */

struct irs_pw *
irs_gen_pw(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_pw *pw;
	struct pvt *pvt;

	if (!(pw = (struct irs_pw *)malloc(sizeof *pw))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(pw, 0x5e, sizeof *pw);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(pw);
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
	return (pw);
}

/* Methods */

static void
pw_close(struct irs_pw *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	free(pvt);
	free(this);
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

#endif /* WANT_IRS_PW */
