/*	$Id: man_validate.c,v 1.80 2012/01/03 15:16:24 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010 Ingo Schwarze <schwarze@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "man.h"
#include "mandoc.h"
#include "libman.h"
#include "libmandoc.h"

#define	CHKARGS	  struct man *m, struct man_node *n

typedef	int	(*v_check)(CHKARGS);

struct	man_valid {
	v_check	 *pres;
	v_check	 *posts;
};

static	int	  check_eq0(CHKARGS);
static	int	  check_eq2(CHKARGS);
static	int	  check_le1(CHKARGS);
static	int	  check_ge2(CHKARGS);
static	int	  check_le5(CHKARGS);
static	int	  check_par(CHKARGS);
static	int	  check_part(CHKARGS);
static	int	  check_root(CHKARGS);
static	void	  check_text(CHKARGS);

static	int	  post_AT(CHKARGS);
static	int	  post_vs(CHKARGS);
static	int	  post_fi(CHKARGS);
static	int	  post_ft(CHKARGS);
static	int	  post_nf(CHKARGS);
static	int	  post_sec(CHKARGS);
static	int	  post_TH(CHKARGS);
static	int	  post_UC(CHKARGS);
static	int	  pre_sec(CHKARGS);

static	v_check	  posts_at[] = { post_AT, NULL };
static	v_check	  posts_br[] = { post_vs, check_eq0, NULL };
static	v_check	  posts_eq0[] = { check_eq0, NULL };
static	v_check	  posts_eq2[] = { check_eq2, NULL };
static	v_check	  posts_fi[] = { check_eq0, post_fi, NULL };
static	v_check	  posts_ft[] = { post_ft, NULL };
static	v_check	  posts_nf[] = { check_eq0, post_nf, NULL };
static	v_check	  posts_par[] = { check_par, NULL };
static	v_check	  posts_part[] = { check_part, NULL };
static	v_check	  posts_sec[] = { post_sec, NULL };
static	v_check	  posts_sp[] = { post_vs, check_le1, NULL };
static	v_check	  posts_th[] = { check_ge2, check_le5, post_TH, NULL };
static	v_check	  posts_uc[] = { post_UC, NULL };
static	v_check	  pres_sec[] = { pre_sec, NULL };

static	const struct man_valid man_valids[MAN_MAX] = {
	{ NULL, posts_br }, /* br */
	{ NULL, posts_th }, /* TH */
	{ pres_sec, posts_sec }, /* SH */
	{ pres_sec, posts_sec }, /* SS */
	{ NULL, NULL }, /* TP */
	{ NULL, posts_par }, /* LP */
	{ NULL, posts_par }, /* PP */
	{ NULL, posts_par }, /* P */
	{ NULL, NULL }, /* IP */
	{ NULL, NULL }, /* HP */
	{ NULL, NULL }, /* SM */
	{ NULL, NULL }, /* SB */
	{ NULL, NULL }, /* BI */
	{ NULL, NULL }, /* IB */
	{ NULL, NULL }, /* BR */
	{ NULL, NULL }, /* RB */
	{ NULL, NULL }, /* R */
	{ NULL, NULL }, /* B */
	{ NULL, NULL }, /* I */
	{ NULL, NULL }, /* IR */
	{ NULL, NULL }, /* RI */
	{ NULL, posts_eq0 }, /* na */
	{ NULL, posts_sp }, /* sp */
	{ NULL, posts_nf }, /* nf */
	{ NULL, posts_fi }, /* fi */
	{ NULL, NULL }, /* RE */
	{ NULL, posts_part }, /* RS */
	{ NULL, NULL }, /* DT */
	{ NULL, posts_uc }, /* UC */
	{ NULL, NULL }, /* PD */
	{ NULL, posts_at }, /* AT */
	{ NULL, NULL }, /* in */
	{ NULL, posts_ft }, /* ft */
	{ NULL, posts_eq2 }, /* OP */
};


int
man_valid_pre(struct man *m, struct man_node *n)
{
	v_check		*cp;

	switch (n->type) {
	case (MAN_TEXT):
		/* FALLTHROUGH */
	case (MAN_ROOT):
		/* FALLTHROUGH */
	case (MAN_EQN):
		/* FALLTHROUGH */
	case (MAN_TBL):
		return(1);
	default:
		break;
	}

	if (NULL == (cp = man_valids[n->tok].pres))
		return(1);
	for ( ; *cp; cp++)
		if ( ! (*cp)(m, n)) 
			return(0);
	return(1);
}


int
man_valid_post(struct man *m)
{
	v_check		*cp;

	if (MAN_VALID & m->last->flags)
		return(1);
	m->last->flags |= MAN_VALID;

	switch (m->last->type) {
	case (MAN_TEXT): 
		check_text(m, m->last);
		return(1);
	case (MAN_ROOT):
		return(check_root(m, m->last));
	case (MAN_EQN):
		/* FALLTHROUGH */
	case (MAN_TBL):
		return(1);
	default:
		break;
	}

	if (NULL == (cp = man_valids[m->last->tok].posts))
		return(1);
	for ( ; *cp; cp++)
		if ( ! (*cp)(m, m->last))
			return(0);

	return(1);
}


static int
check_root(CHKARGS) 
{

	if (MAN_BLINE & m->flags)
		man_nmsg(m, n, MANDOCERR_SCOPEEXIT);
	else if (MAN_ELINE & m->flags)
		man_nmsg(m, n, MANDOCERR_SCOPEEXIT);

	m->flags &= ~MAN_BLINE;
	m->flags &= ~MAN_ELINE;

	if (NULL == m->first->child) {
		man_nmsg(m, n, MANDOCERR_NODOCBODY);
		return(0);
	} else if (NULL == m->meta.title) {
		man_nmsg(m, n, MANDOCERR_NOTITLE);

		/*
		 * If a title hasn't been set, do so now (by
		 * implication, date and section also aren't set).
		 */

	        m->meta.title = mandoc_strdup("unknown");
		m->meta.msec = mandoc_strdup("1");
		m->meta.date = mandoc_normdate
			(m->parse, NULL, n->line, n->pos);
	}

	return(1);
}

static void
check_text(CHKARGS)
{
	char		*cp, *p;

	if (MAN_LITERAL & m->flags)
		return;

	cp = n->string;
	for (p = cp; NULL != (p = strchr(p, '\t')); p++)
		man_pmsg(m, n->line, (int)(p - cp), MANDOCERR_BADTAB);
}

#define	INEQ_DEFINE(x, ineq, name) \
static int \
check_##name(CHKARGS) \
{ \
	if (n->nchild ineq (x)) \
		return(1); \
	mandoc_vmsg(MANDOCERR_ARGCOUNT, m->parse, n->line, n->pos, \
			"line arguments %s %d (have %d)", \
			#ineq, (x), n->nchild); \
	return(1); \
}

INEQ_DEFINE(0, ==, eq0)
INEQ_DEFINE(2, ==, eq2)
INEQ_DEFINE(1, <=, le1)
INEQ_DEFINE(2, >=, ge2)
INEQ_DEFINE(5, <=, le5)

static int
post_ft(CHKARGS)
{
	char	*cp;
	int	 ok;

	if (0 == n->nchild)
		return(1);

	ok = 0;
	cp = n->child->string;
	switch (*cp) {
	case ('1'):
		/* FALLTHROUGH */
	case ('2'):
		/* FALLTHROUGH */
	case ('3'):
		/* FALLTHROUGH */
	case ('4'):
		/* FALLTHROUGH */
	case ('I'):
		/* FALLTHROUGH */
	case ('P'):
		/* FALLTHROUGH */
	case ('R'):
		if ('\0' == cp[1])
			ok = 1;
		break;
	case ('B'):
		if ('\0' == cp[1] || ('I' == cp[1] && '\0' == cp[2]))
			ok = 1;
		break;
	case ('C'):
		if ('W' == cp[1] && '\0' == cp[2])
			ok = 1;
		break;
	default:
		break;
	}

	if (0 == ok) {
		mandoc_vmsg
			(MANDOCERR_BADFONT, m->parse,
			 n->line, n->pos, "%s", cp);
		*cp = '\0';
	}

	if (1 < n->nchild)
		mandoc_vmsg
			(MANDOCERR_ARGCOUNT, m->parse, n->line, 
			 n->pos, "want one child (have %d)", 
			 n->nchild);

	return(1);
}

static int
pre_sec(CHKARGS)
{

	if (MAN_BLOCK == n->type)
		m->flags &= ~MAN_LITERAL;
	return(1);
}

static int
post_sec(CHKARGS)
{

	if ( ! (MAN_HEAD == n->type && 0 == n->nchild)) 
		return(1);

	man_nmsg(m, n, MANDOCERR_SYNTARGCOUNT);
	return(0);
}

static int
check_part(CHKARGS)
{

	if (MAN_BODY == n->type && 0 == n->nchild)
		mandoc_msg(MANDOCERR_ARGCWARN, m->parse, n->line, 
				n->pos, "want children (have none)");

	return(1);
}


static int
check_par(CHKARGS)
{

	switch (n->type) {
	case (MAN_BLOCK):
		if (0 == n->body->nchild)
			man_node_delete(m, n);
		break;
	case (MAN_BODY):
		if (0 == n->nchild)
			man_nmsg(m, n, MANDOCERR_IGNPAR);
		break;
	case (MAN_HEAD):
		if (n->nchild)
			man_nmsg(m, n, MANDOCERR_ARGSLOST);
		break;
	default:
		break;
	}

	return(1);
}


static int
post_TH(CHKARGS)
{
	const char	*p;
	int		 line, pos;

	if (m->meta.title)
		free(m->meta.title);
	if (m->meta.vol)
		free(m->meta.vol);
	if (m->meta.source)
		free(m->meta.source);
	if (m->meta.msec)
		free(m->meta.msec);
	if (m->meta.date)
		free(m->meta.date);

	line = n->line;
	pos = n->pos;
	m->meta.title = m->meta.vol = m->meta.date =
		m->meta.msec = m->meta.source = NULL;

	/* ->TITLE<- MSEC DATE SOURCE VOL */

	n = n->child;
	if (n && n->string) {
		for (p = n->string; '\0' != *p; p++) {
			/* Only warn about this once... */
			if (isalpha((unsigned char)*p) && 
					! isupper((unsigned char)*p)) {
				man_nmsg(m, n, MANDOCERR_UPPERCASE);
				break;
			}
		}
		m->meta.title = mandoc_strdup(n->string);
	} else
		m->meta.title = mandoc_strdup("");

	/* TITLE ->MSEC<- DATE SOURCE VOL */

	if (n)
		n = n->next;
	if (n && n->string)
		m->meta.msec = mandoc_strdup(n->string);
	else
		m->meta.msec = mandoc_strdup("");

	/* TITLE MSEC ->DATE<- SOURCE VOL */

	if (n)
		n = n->next;
	if (n && n->string && '\0' != n->string[0]) {
		pos = n->pos;
		m->meta.date = mandoc_normdate
		    (m->parse, n->string, line, pos);
	} else
		m->meta.date = mandoc_strdup("");

	/* TITLE MSEC DATE ->SOURCE<- VOL */

	if (n && (n = n->next))
		m->meta.source = mandoc_strdup(n->string);

	/* TITLE MSEC DATE SOURCE ->VOL<- */
	/* If missing, use the default VOL name for MSEC. */

	if (n && (n = n->next))
		m->meta.vol = mandoc_strdup(n->string);
	else if ('\0' != m->meta.msec[0] &&
	    (NULL != (p = mandoc_a2msec(m->meta.msec))))
		m->meta.vol = mandoc_strdup(p);

	/*
	 * Remove the `TH' node after we've processed it for our
	 * meta-data.
	 */
	man_node_delete(m, m->last);
	return(1);
}

static int
post_nf(CHKARGS)
{

	if (MAN_LITERAL & m->flags)
		man_nmsg(m, n, MANDOCERR_SCOPEREP);

	m->flags |= MAN_LITERAL;
	return(1);
}

static int
post_fi(CHKARGS)
{

	if ( ! (MAN_LITERAL & m->flags))
		man_nmsg(m, n, MANDOCERR_WNOSCOPE);

	m->flags &= ~MAN_LITERAL;
	return(1);
}

static int
post_UC(CHKARGS)
{
	static const char * const bsd_versions[] = {
	    "3rd Berkeley Distribution",
	    "4th Berkeley Distribution",
	    "4.2 Berkeley Distribution",
	    "4.3 Berkeley Distribution",
	    "4.4 Berkeley Distribution",
	};

	const char	*p, *s;

	n = n->child;

	if (NULL == n || MAN_TEXT != n->type)
		p = bsd_versions[0];
	else {
		s = n->string;
		if (0 == strcmp(s, "3"))
			p = bsd_versions[0];
		else if (0 == strcmp(s, "4"))
			p = bsd_versions[1];
		else if (0 == strcmp(s, "5"))
			p = bsd_versions[2];
		else if (0 == strcmp(s, "6"))
			p = bsd_versions[3];
		else if (0 == strcmp(s, "7"))
			p = bsd_versions[4];
		else
			p = bsd_versions[0];
	}

	if (m->meta.source)
		free(m->meta.source);

	m->meta.source = mandoc_strdup(p);
	return(1);
}

static int
post_AT(CHKARGS)
{
	static const char * const unix_versions[] = {
	    "7th Edition",
	    "System III",
	    "System V",
	    "System V Release 2",
	};

	const char	*p, *s;
	struct man_node	*nn;

	n = n->child;

	if (NULL == n || MAN_TEXT != n->type)
		p = unix_versions[0];
	else {
		s = n->string;
		if (0 == strcmp(s, "3"))
			p = unix_versions[0];
		else if (0 == strcmp(s, "4"))
			p = unix_versions[1];
		else if (0 == strcmp(s, "5")) {
			nn = n->next;
			if (nn && MAN_TEXT == nn->type && nn->string[0])
				p = unix_versions[3];
			else
				p = unix_versions[2];
		} else
			p = unix_versions[0];
	}

	if (m->meta.source)
		free(m->meta.source);

	m->meta.source = mandoc_strdup(p);
	return(1);
}

static int
post_vs(CHKARGS)
{

	/* 
	 * Don't warn about this because it occurs in pod2man and would
	 * cause considerable (unfixable) warnage.
	 */
	if (NULL == n->prev && MAN_ROOT == n->parent->type)
		man_node_delete(m, n);

	return(1);
}
