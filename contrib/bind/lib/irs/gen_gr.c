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
static const char rcsid[] = "$Id: gen_gr.c,v 1.22 2000/07/11 05:51:56 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#ifndef WANT_IRS_GR
static int __bind_irs_gr_unneeded;
#else

#include <sys/types.h>

#include <isc/assertions.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include <isc/memcluster.h>
#include <irs.h>

#include "port_after.h"

#include "irs_p.h"
#include "gen_p.h"

/* Definitions */

struct pvt {
	struct irs_rule *	rules;
	struct irs_rule *	rule;
	struct irs_gr *		gr;
	/*
	 * Need space to store the entries read from the group file.
	 * The members list also needs space per member, and the
	 * strings making up the user names must be allocated
	 * somewhere.  Rather than doing lots of small allocations,
	 * we keep one buffer and resize it as needed.
	 */
	struct group		group;
	size_t			nmemb;    /* Malloc'd max index of gr_mem[]. */
	char *			membuf;
	size_t			membufsize;
	struct __res_state *	res;
	void			(*free_res)(void *);
};

/* Forward */

static void		gr_close(struct irs_gr *);
static struct group *	gr_next(struct irs_gr *);
static struct group *	gr_byname(struct irs_gr *, const char *);
static struct group *	gr_bygid(struct irs_gr *, gid_t);
static void		gr_rewind(struct irs_gr *);
static int		gr_list(struct irs_gr *, const char *,
				gid_t, gid_t *, int *);
static void		gr_minimize(struct irs_gr *);
static struct __res_state * gr_res_get(struct irs_gr *);
static void		gr_res_set(struct irs_gr *,
				      struct __res_state *,
				      void (*)(void *));

static void		grmerge(struct irs_gr *gr, const struct group *src,
				int preserve);

static int		countvec(char **vec);
static int		isnew(char **old, char *new);
static int		countnew(char **old, char **new);
static size_t		sizenew(char **old, char **new);
static int		newgid(int, gid_t *, gid_t);

/* Public */

struct irs_gr *
irs_gen_gr(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_gr *gr;
	struct pvt *pvt;

	if (!(gr = memget(sizeof *gr))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(gr, 0x5e, sizeof *gr);
	if (!(pvt = memget(sizeof *pvt))) {
		memput(gr, sizeof *gr);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->rules = accpvt->map_rules[irs_gr];
	pvt->rule = pvt->rules;
	gr->private = pvt;
	gr->close = gr_close;
	gr->next = gr_next;
	gr->byname = gr_byname;
	gr->bygid = gr_bygid;
	gr->rewind = gr_rewind;
	gr->list = gr_list;
	gr->minimize = gr_minimize;
	gr->res_get = gr_res_get;
	gr->res_set = gr_res_set;
	return (gr);
}

/* Methods. */

static void
gr_close(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}

static struct group *
gr_next(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct group *rval;
	struct irs_gr *gr;

	while (pvt->rule) {
		gr = pvt->rule->inst->gr;
		rval = (*gr->next)(gr);
		if (rval)
			return (rval);
		if (!(pvt->rule->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			gr = pvt->rule->inst->gr;
			(*gr->rewind)(gr);
		}
	}
	return (NULL);
}

static struct group *
gr_byname(struct irs_gr *this, const char *name) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct group *tval;
	struct irs_gr *gr;
	int dirty;

	dirty = 0;
	for (rule = pvt->rules; rule; rule = rule->next) {
		gr = rule->inst->gr;
		tval = (*gr->byname)(gr, name);
		if (tval) {
			grmerge(this, tval, dirty++);
			if (!(rule->flags & IRS_MERGE))
				break;
		} else {
			if (!(rule->flags & IRS_CONTINUE))
				break;
		}
	}
	if (dirty)
		return (&pvt->group);
	return (NULL);
}

static struct group *
gr_bygid(struct irs_gr *this, gid_t gid) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct group *tval;
	struct irs_gr *gr;
	int dirty;

	dirty = 0;
	for (rule = pvt->rules; rule; rule = rule->next) {
		gr = rule->inst->gr;
		tval = (*gr->bygid)(gr, gid);
		if (tval) {
			grmerge(this, tval, dirty++);
			if (!(rule->flags & IRS_MERGE))
				break;
		} else {
			if (!(rule->flags & IRS_CONTINUE))
				break;
		}
	}
	if (dirty)
		return (&pvt->group);
	return (NULL);
}

static void
gr_rewind(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_gr *gr;

	pvt->rule = pvt->rules;
	if (pvt->rule) {
		gr = pvt->rule->inst->gr;
		(*gr->rewind)(gr);
	}
}

static int
gr_list(struct irs_gr *this, const char *name,
	gid_t basegid, gid_t *groups, int *ngroups)
{
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct irs_gr *gr;
	int t_ngroups, maxgroups;
	gid_t *t_groups;
	int n, t, rval = 0;

	maxgroups = *ngroups;
	*ngroups = 0;
	t_groups = (gid_t *)malloc(maxgroups * sizeof(gid_t));
	if (!t_groups) {
		errno = ENOMEM;
		return (-1);
	}

	for (rule = pvt->rules; rule; rule = rule->next) {
		t_ngroups = maxgroups;
		gr = rule->inst->gr;
		t = (*gr->list)(gr, name, basegid, t_groups, &t_ngroups);
		for (n = 0; n < t_ngroups; n++) {
			if (newgid(*ngroups, groups, t_groups[n])) {
				if (*ngroups == maxgroups) {
					rval = -1;
					goto done;
				}
				groups[(*ngroups)++] = t_groups[n];
			}
		}
		if (t == 0) {
			if (!(rule->flags & IRS_MERGE))
				break;
		} else {
			if (!(rule->flags & IRS_CONTINUE))
				break;
		}
	}
 done:
	free(t_groups);
	return (rval);
}

static void
gr_minimize(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_gr *gr = rule->inst->gr;

		(*gr->minimize)(gr);
	}
}

static struct __res_state *
gr_res_get(struct irs_gr *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (!pvt->res) {
		struct __res_state *res;
		res = (struct __res_state *)malloc(sizeof *res);
		if (!res) {
			errno = ENOMEM;
			return (NULL);
		}
		memset(res, 0, sizeof *res);
		gr_res_set(this, res, free);
	}

	return (pvt->res);
}

static void
gr_res_set(struct irs_gr *this, struct __res_state *res,
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
		struct irs_gr *gr = rule->inst->gr;

		if (gr->res_set)
			(*gr->res_set)(gr, pvt->res, NULL);
	}
}

/* Private. */

static void
grmerge(struct irs_gr *this, const struct group *src, int preserve) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *cp, **m, **p;
	int n, ndst, nnew, memadj;

	if (!preserve) {
		pvt->group.gr_gid = src->gr_gid;
		if (pvt->nmemb < 1) {
			m = malloc(sizeof *m);
			if (!m) {
				/* No harm done, no work done. */
				return;
			}
			pvt->group.gr_mem = m;
			pvt->nmemb = 1;
		}
		pvt->group.gr_mem[0] = NULL;
	}
	ndst = countvec(pvt->group.gr_mem);
	nnew = countnew(pvt->group.gr_mem, src->gr_mem);

	/*
	 * Make sure destination member array is large enough.
	 * p points to new portion.
	 */
	n = ndst + nnew + 1;
	if ((size_t)n > pvt->nmemb) {
		m = realloc(pvt->group.gr_mem, n * sizeof *m);
		if (!m) {
			/* No harm done, no work done. */
			return;
		}
		pvt->group.gr_mem = m;
		pvt->nmemb = n;
	}
	p = pvt->group.gr_mem + ndst;

	/*
	 * Enlarge destination membuf; cp points at new portion.
	 */
	n = sizenew(pvt->group.gr_mem, src->gr_mem);
	INSIST((nnew == 0) == (n == 0));
	if (!preserve) {
		n += strlen(src->gr_name) + 1;
		n += strlen(src->gr_passwd) + 1;
	}
	if (n == 0) {
		/* No work to do. */
		return;
	}
	cp = realloc(pvt->membuf, pvt->membufsize + n);
	if (!cp) {
		/* No harm done, no work done. */
		return;
	}
	memadj = cp - pvt->membuf;
	pvt->membuf = cp;
	cp += pvt->membufsize;
	pvt->membufsize += n;

	/*
	 * Add new elements.
	 */
	for (m = src->gr_mem; *m; m++)
		if (isnew(pvt->group.gr_mem, *m)) {
			*p++ = cp;
			*p = NULL;
			strcpy(cp, *m);
			cp += strlen(cp) + 1;
		}
	if (preserve) {
		pvt->group.gr_name += memadj;
		pvt->group.gr_passwd += memadj;
	} else {
		pvt->group.gr_name = cp;
		strcpy(cp, src->gr_name);
		cp += strlen(src->gr_name) + 1;
		pvt->group.gr_passwd = cp;
		strcpy(cp, src->gr_passwd);
		cp += strlen(src->gr_passwd) + 1;
	}
	INSIST(cp >= pvt->membuf && cp <= &pvt->membuf[pvt->membufsize]);
}

static int
countvec(char **vec) {
	int n = 0;

	while (*vec++)
		n++;
	return (n);
}

static int
isnew(char **old, char *new) {
	for (; *old; old++)
		if (strcmp(*old, new) == 0)
			return (0);
	return (1);
}

static int
countnew(char **old, char **new) {
	int n = 0;

	for (; *new; new++)
		n += isnew(old, *new);
	return (n);
}

static size_t
sizenew(char **old, char **new) {
	size_t n = 0;

	for (; *new; new++)
		if (isnew(old, *new))
			n += strlen(*new) + 1;
	return (n);
}

static int
newgid(int ngroups, gid_t *groups, gid_t group) {
	ngroups--, groups++;
	for (; ngroups-- > 0; groups++)
		if (*groups == group)
			return (0);
	return (1);
}

#endif /* WANT_IRS_GR */
