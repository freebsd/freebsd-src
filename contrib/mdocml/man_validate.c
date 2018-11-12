/*	$OpenBSD$ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2012-2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include "config.h"

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
#include "mandoc_aux.h"
#include "libman.h"
#include "libmandoc.h"

#define	CHKARGS	  struct man *man, struct man_node *n

typedef	void	(*v_check)(CHKARGS);

static	void	  check_par(CHKARGS);
static	void	  check_part(CHKARGS);
static	void	  check_root(CHKARGS);
static	void	  check_text(CHKARGS);

static	void	  post_AT(CHKARGS);
static	void	  post_IP(CHKARGS);
static	void	  post_vs(CHKARGS);
static	void	  post_fi(CHKARGS);
static	void	  post_ft(CHKARGS);
static	void	  post_nf(CHKARGS);
static	void	  post_OP(CHKARGS);
static	void	  post_TH(CHKARGS);
static	void	  post_UC(CHKARGS);
static	void	  post_UR(CHKARGS);

static	v_check man_valids[MAN_MAX] = {
	post_vs,    /* br */
	post_TH,    /* TH */
	NULL,       /* SH */
	NULL,       /* SS */
	NULL,       /* TP */
	check_par,  /* LP */
	check_par,  /* PP */
	check_par,  /* P */
	post_IP,    /* IP */
	NULL,       /* HP */
	NULL,       /* SM */
	NULL,       /* SB */
	NULL,       /* BI */
	NULL,       /* IB */
	NULL,       /* BR */
	NULL,       /* RB */
	NULL,       /* R */
	NULL,       /* B */
	NULL,       /* I */
	NULL,       /* IR */
	NULL,       /* RI */
	post_vs,    /* sp */
	post_nf,    /* nf */
	post_fi,    /* fi */
	NULL,       /* RE */
	check_part, /* RS */
	NULL,       /* DT */
	post_UC,    /* UC */
	NULL,       /* PD */
	post_AT,    /* AT */
	NULL,       /* in */
	post_ft,    /* ft */
	post_OP,    /* OP */
	post_nf,    /* EX */
	post_fi,    /* EE */
	post_UR,    /* UR */
	NULL,       /* UE */
	NULL,       /* ll */
};


void
man_valid_post(struct man *man)
{
	struct man_node	*n;
	v_check		*cp;

	n = man->last;
	if (n->flags & MAN_VALID)
		return;
	n->flags |= MAN_VALID;

	switch (n->type) {
	case MAN_TEXT:
		check_text(man, n);
		break;
	case MAN_ROOT:
		check_root(man, n);
		break;
	case MAN_EQN:
		/* FALLTHROUGH */
	case MAN_TBL:
		break;
	default:
		cp = man_valids + n->tok;
		if (*cp)
			(*cp)(man, n);
		break;
	}
}

static void
check_root(CHKARGS)
{

	assert((man->flags & (MAN_BLINE | MAN_ELINE)) == 0);

	if (NULL == man->first->child)
		mandoc_msg(MANDOCERR_DOC_EMPTY, man->parse,
		    n->line, n->pos, NULL);
	else
		man->meta.hasbody = 1;

	if (NULL == man->meta.title) {
		mandoc_msg(MANDOCERR_TH_NOTITLE, man->parse,
		    n->line, n->pos, NULL);

		/*
		 * If a title hasn't been set, do so now (by
		 * implication, date and section also aren't set).
		 */

		man->meta.title = mandoc_strdup("");
		man->meta.msec = mandoc_strdup("");
		man->meta.date = man->quick ? mandoc_strdup("") :
		    mandoc_normdate(man->parse, NULL, n->line, n->pos);
	}
}

static void
check_text(CHKARGS)
{
	char		*cp, *p;

	if (MAN_LITERAL & man->flags)
		return;

	cp = n->string;
	for (p = cp; NULL != (p = strchr(p, '\t')); p++)
		mandoc_msg(MANDOCERR_FI_TAB, man->parse,
		    n->line, n->pos + (p - cp), NULL);
}

static void
post_OP(CHKARGS)
{

	if (n->nchild == 0)
		mandoc_msg(MANDOCERR_OP_EMPTY, man->parse,
		    n->line, n->pos, "OP");
	else if (n->nchild > 2) {
		n = n->child->next->next;
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, man->parse,
		    n->line, n->pos, "OP ... %s", n->string);
	}
}

static void
post_UR(CHKARGS)
{

	if (n->type == MAN_HEAD && n->child == NULL)
		mandoc_vmsg(MANDOCERR_UR_NOHEAD, man->parse,
		    n->line, n->pos, "UR");
	check_part(man, n);
}

static void
post_ft(CHKARGS)
{
	char	*cp;
	int	 ok;

	if (0 == n->nchild)
		return;

	ok = 0;
	cp = n->child->string;
	switch (*cp) {
	case '1':
		/* FALLTHROUGH */
	case '2':
		/* FALLTHROUGH */
	case '3':
		/* FALLTHROUGH */
	case '4':
		/* FALLTHROUGH */
	case 'I':
		/* FALLTHROUGH */
	case 'P':
		/* FALLTHROUGH */
	case 'R':
		if ('\0' == cp[1])
			ok = 1;
		break;
	case 'B':
		if ('\0' == cp[1] || ('I' == cp[1] && '\0' == cp[2]))
			ok = 1;
		break;
	case 'C':
		if ('W' == cp[1] && '\0' == cp[2])
			ok = 1;
		break;
	default:
		break;
	}

	if (0 == ok) {
		mandoc_vmsg(MANDOCERR_FT_BAD, man->parse,
		    n->line, n->pos, "ft %s", cp);
		*cp = '\0';
	}
}

static void
check_part(CHKARGS)
{

	if (n->type == MAN_BODY && n->child == NULL)
		mandoc_msg(MANDOCERR_BLK_EMPTY, man->parse,
		    n->line, n->pos, man_macronames[n->tok]);
}

static void
check_par(CHKARGS)
{

	switch (n->type) {
	case MAN_BLOCK:
		if (0 == n->body->nchild)
			man_node_delete(man, n);
		break;
	case MAN_BODY:
		if (0 == n->nchild)
			mandoc_vmsg(MANDOCERR_PAR_SKIP,
			    man->parse, n->line, n->pos,
			    "%s empty", man_macronames[n->tok]);
		break;
	case MAN_HEAD:
		if (n->nchild)
			mandoc_vmsg(MANDOCERR_ARG_SKIP,
			    man->parse, n->line, n->pos,
			    "%s %s%s", man_macronames[n->tok],
			    n->child->string,
			    n->nchild > 1 ? " ..." : "");
		break;
	default:
		break;
	}
}

static void
post_IP(CHKARGS)
{

	switch (n->type) {
	case MAN_BLOCK:
		if (0 == n->head->nchild && 0 == n->body->nchild)
			man_node_delete(man, n);
		break;
	case MAN_BODY:
		if (0 == n->parent->head->nchild && 0 == n->nchild)
			mandoc_vmsg(MANDOCERR_PAR_SKIP,
			    man->parse, n->line, n->pos,
			    "%s empty", man_macronames[n->tok]);
		break;
	default:
		break;
	}
}

static void
post_TH(CHKARGS)
{
	struct man_node	*nb;
	const char	*p;

	free(man->meta.title);
	free(man->meta.vol);
	free(man->meta.source);
	free(man->meta.msec);
	free(man->meta.date);

	man->meta.title = man->meta.vol = man->meta.date =
	    man->meta.msec = man->meta.source = NULL;

	nb = n;

	/* ->TITLE<- MSEC DATE SOURCE VOL */

	n = n->child;
	if (n && n->string) {
		for (p = n->string; '\0' != *p; p++) {
			/* Only warn about this once... */
			if (isalpha((unsigned char)*p) &&
			    ! isupper((unsigned char)*p)) {
				mandoc_vmsg(MANDOCERR_TITLE_CASE,
				    man->parse, n->line,
				    n->pos + (p - n->string),
				    "TH %s", n->string);
				break;
			}
		}
		man->meta.title = mandoc_strdup(n->string);
	} else {
		man->meta.title = mandoc_strdup("");
		mandoc_msg(MANDOCERR_TH_NOTITLE, man->parse,
		    nb->line, nb->pos, "TH");
	}

	/* TITLE ->MSEC<- DATE SOURCE VOL */

	if (n)
		n = n->next;
	if (n && n->string)
		man->meta.msec = mandoc_strdup(n->string);
	else {
		man->meta.msec = mandoc_strdup("");
		mandoc_vmsg(MANDOCERR_MSEC_MISSING, man->parse,
		    nb->line, nb->pos, "TH %s", man->meta.title);
	}

	/* TITLE MSEC ->DATE<- SOURCE VOL */

	if (n)
		n = n->next;
	if (n && n->string && '\0' != n->string[0]) {
		man->meta.date = man->quick ?
		    mandoc_strdup(n->string) :
		    mandoc_normdate(man->parse, n->string,
			n->line, n->pos);
	} else {
		man->meta.date = mandoc_strdup("");
		mandoc_msg(MANDOCERR_DATE_MISSING, man->parse,
		    n ? n->line : nb->line,
		    n ? n->pos : nb->pos, "TH");
	}

	/* TITLE MSEC DATE ->SOURCE<- VOL */

	if (n && (n = n->next))
		man->meta.source = mandoc_strdup(n->string);
	else if (man->defos != NULL)
		man->meta.source = mandoc_strdup(man->defos);

	/* TITLE MSEC DATE SOURCE ->VOL<- */
	/* If missing, use the default VOL name for MSEC. */

	if (n && (n = n->next))
		man->meta.vol = mandoc_strdup(n->string);
	else if ('\0' != man->meta.msec[0] &&
	    (NULL != (p = mandoc_a2msec(man->meta.msec))))
		man->meta.vol = mandoc_strdup(p);

	if (n != NULL && (n = n->next) != NULL)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, man->parse,
		    n->line, n->pos, "TH ... %s", n->string);

	/*
	 * Remove the `TH' node after we've processed it for our
	 * meta-data.
	 */
	man_node_delete(man, man->last);
}

static void
post_nf(CHKARGS)
{

	if (man->flags & MAN_LITERAL)
		mandoc_msg(MANDOCERR_NF_SKIP, man->parse,
		    n->line, n->pos, "nf");

	man->flags |= MAN_LITERAL;
}

static void
post_fi(CHKARGS)
{

	if ( ! (MAN_LITERAL & man->flags))
		mandoc_msg(MANDOCERR_FI_SKIP, man->parse,
		    n->line, n->pos, "fi");

	man->flags &= ~MAN_LITERAL;
}

static void
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

	free(man->meta.source);
	man->meta.source = mandoc_strdup(p);
}

static void
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

	free(man->meta.source);
	man->meta.source = mandoc_strdup(p);
}

static void
post_vs(CHKARGS)
{

	if (NULL != n->prev)
		return;

	switch (n->parent->tok) {
	case MAN_SH:
		/* FALLTHROUGH */
	case MAN_SS:
		mandoc_vmsg(MANDOCERR_PAR_SKIP, man->parse, n->line, n->pos,
		    "%s after %s", man_macronames[n->tok],
		    man_macronames[n->parent->tok]);
		/* FALLTHROUGH */
	case MAN_MAX:
		/*
		 * Don't warn about this because it occurs in pod2man
		 * and would cause considerable (unfixable) warnage.
		 */
		man_node_delete(man, n);
		break;
	default:
		break;
	}
}
