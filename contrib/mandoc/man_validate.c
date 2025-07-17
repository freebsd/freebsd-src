/* $Id: man_validate.c,v 1.159 2023/10/24 20:53:12 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2012-2020, 2023 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Validation module for man(7) syntax trees used by mandoc(1).
 */
#include "config.h"

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "mandoc_xr.h"
#include "roff.h"
#include "man.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "libman.h"
#include "tag.h"

#define	CHKARGS	  struct roff_man *man, struct roff_node *n

typedef	void	(*v_check)(CHKARGS);

static	void	  check_par(CHKARGS);
static	void	  check_part(CHKARGS);
static	void	  check_root(CHKARGS);
static	void	  check_tag(struct roff_node *, struct roff_node *);
static	void	  check_text(CHKARGS);

static	void	  post_AT(CHKARGS);
static	void	  post_EE(CHKARGS);
static	void	  post_EX(CHKARGS);
static	void	  post_IP(CHKARGS);
static	void	  post_MR(CHKARGS);
static	void	  post_OP(CHKARGS);
static	void	  post_SH(CHKARGS);
static	void	  post_TH(CHKARGS);
static	void	  post_TP(CHKARGS);
static	void	  post_UC(CHKARGS);
static	void	  post_UR(CHKARGS);
static	void	  post_in(CHKARGS);

static	const v_check man_valids[MAN_MAX - MAN_TH] = {
	post_TH,    /* TH */
	post_SH,    /* SH */
	post_SH,    /* SS */
	post_TP,    /* TP */
	post_TP,    /* TQ */
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
	NULL,       /* RE */
	check_part, /* RS */
	NULL,       /* DT */
	post_UC,    /* UC */
	NULL,       /* PD */
	post_AT,    /* AT */
	post_in,    /* in */
	NULL,       /* SY */
	NULL,       /* YS */
	post_OP,    /* OP */
	post_EX,    /* EX */
	post_EE,    /* EE */
	post_UR,    /* UR */
	NULL,       /* UE */
	post_UR,    /* MT */
	NULL,       /* ME */
	post_MR,    /* MR */
};


/* Validate the subtree rooted at man->last. */
void
man_validate(struct roff_man *man)
{
	struct roff_node *n;
	const v_check	 *cp;

	/*
	 * Iterate over all children, recursing into each one
	 * in turn, depth-first.
	 */

	n = man->last;
	man->last = man->last->child;
	while (man->last != NULL) {
		man_validate(man);
		if (man->last == n)
			man->last = man->last->child;
		else
			man->last = man->last->next;
	}

	/* Finally validate the macro itself. */

	man->last = n;
	man->next = ROFF_NEXT_SIBLING;
	switch (n->type) {
	case ROFFT_TEXT:
		check_text(man, n);
		break;
	case ROFFT_ROOT:
		check_root(man, n);
		break;
	case ROFFT_COMMENT:
	case ROFFT_EQN:
	case ROFFT_TBL:
		break;
	default:
		if (n->tok < ROFF_MAX) {
			roff_validate(man);
			break;
		}
		assert(n->tok >= MAN_TH && n->tok < MAN_MAX);
		cp = man_valids + (n->tok - MAN_TH);
		if (*cp)
			(*cp)(man, n);
		if (man->last == n)
			n->flags |= NODE_VALID;
		break;
	}
}

static void
check_root(CHKARGS)
{
	assert((man->flags & (MAN_BLINE | MAN_ELINE)) == 0);

	if (n->last == NULL || n->last->type == ROFFT_COMMENT)
		mandoc_msg(MANDOCERR_DOC_EMPTY, n->line, n->pos, NULL);
	else
		man->meta.hasbody = 1;

	if (NULL == man->meta.title) {
		mandoc_msg(MANDOCERR_TH_NOTITLE, n->line, n->pos, NULL);

		/*
		 * If a title hasn't been set, do so now (by
		 * implication, date and section also aren't set).
		 */

		man->meta.title = mandoc_strdup("");
		man->meta.msec = mandoc_strdup("");
		man->meta.date = mandoc_normdate(NULL, NULL);
	}

	if (man->meta.os_e &&
	    (man->meta.rcsids & (1 << man->meta.os_e)) == 0)
		mandoc_msg(MANDOCERR_RCS_MISSING, 0, 0,
		    man->meta.os_e == MANDOC_OS_OPENBSD ?
		    "(OpenBSD)" : "(NetBSD)");
}

/*
 * Skip leading whitespace, dashes, backslashes, and font escapes,
 * then create a tag if the first following byte is a letter.
 * Priority is high unless whitespace is present.
 */
static void
check_tag(struct roff_node *n, struct roff_node *nt)
{
	const char	*cp, *arg;
	int		 prio, sz;

	if (nt == NULL || nt->type != ROFFT_TEXT)
		return;

	cp = nt->string;
	prio = TAG_STRONG;
	for (;;) {
		switch (*cp) {
		case ' ':
		case '\t':
			prio = TAG_WEAK;
			/* FALLTHROUGH */
		case '-':
			cp++;
			break;
		case '\\':
			cp++;
			switch (mandoc_escape(&cp, &arg, &sz)) {
			case ESCAPE_FONT:
			case ESCAPE_FONTBOLD:
			case ESCAPE_FONTITALIC:
			case ESCAPE_FONTBI:
			case ESCAPE_FONTROMAN:
			case ESCAPE_FONTCR:
			case ESCAPE_FONTCB:
			case ESCAPE_FONTCI:
			case ESCAPE_FONTPREV:
			case ESCAPE_IGNORE:
				break;
			case ESCAPE_SPECIAL:
				if (sz != 1)
					return;
				switch (*arg) {
				case '-':
				case 'e':
					break;
				default:
					return;
				}
				break;
			default:
				return;
			}
			break;
		default:
			if (isalpha((unsigned char)*cp))
				tag_put(cp, prio, n);
			return;
		}
	}
}

static void
check_text(CHKARGS)
{
	char		*cp, *p;

	if (n->flags & NODE_NOFILL)
		return;

	cp = n->string;
	for (p = cp; NULL != (p = strchr(p, '\t')); p++)
		mandoc_msg(MANDOCERR_FI_TAB,
		    n->line, n->pos + (int)(p - cp), NULL);
}

static void
post_EE(CHKARGS)
{
	if ((n->flags & NODE_NOFILL) == 0)
		mandoc_msg(MANDOCERR_FI_SKIP, n->line, n->pos, "EE");
}

static void
post_EX(CHKARGS)
{
	if (n->flags & NODE_NOFILL)
		mandoc_msg(MANDOCERR_NF_SKIP, n->line, n->pos, "EX");
}

static void
post_OP(CHKARGS)
{

	if (n->child == NULL)
		mandoc_msg(MANDOCERR_OP_EMPTY, n->line, n->pos, "OP");
	else if (n->child->next != NULL && n->child->next->next != NULL) {
		n = n->child->next->next;
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    n->line, n->pos, "OP ... %s", n->string);
	}
}

static void
post_SH(CHKARGS)
{
	struct roff_node	*nc;
	char			*cp, *tag;

	nc = n->child;
	switch (n->type) {
	case ROFFT_HEAD:
		tag = NULL;
		deroff(&tag, n);
		if (tag != NULL) {
			for (cp = tag; *cp != '\0'; cp++)
				if (*cp == ' ')
					*cp = '_';
			if (nc != NULL && nc->type == ROFFT_TEXT &&
			    strcmp(nc->string, tag) == 0)
				tag_put(NULL, TAG_STRONG, n);
			else
				tag_put(tag, TAG_FALLBACK, n);
			free(tag);
		}
		return;
	case ROFFT_BODY:
		if (nc != NULL)
			break;
		return;
	default:
		return;
	}

	if ((nc->tok == MAN_LP || nc->tok == MAN_PP || nc->tok == MAN_P) &&
	    nc->body->child != NULL) {
		while (nc->body->last != NULL) {
			man->next = ROFF_NEXT_CHILD;
			roff_node_relink(man, nc->body->last);
			man->last = n;
		}
	}

	if (nc->tok == MAN_LP || nc->tok == MAN_PP || nc->tok == MAN_P ||
	    nc->tok == ROFF_sp || nc->tok == ROFF_br) {
		mandoc_msg(MANDOCERR_PAR_SKIP, nc->line, nc->pos,
		    "%s after %s", roff_name[nc->tok], roff_name[n->tok]);
		roff_node_delete(man, nc);
	}

	/*
	 * Trailing PP is empty, so it is deleted by check_par().
	 * Trailing sp is significant.
	 */

	if ((nc = n->last) != NULL && nc->tok == ROFF_br) {
		mandoc_msg(MANDOCERR_PAR_SKIP,
		    nc->line, nc->pos, "%s at the end of %s",
		    roff_name[nc->tok], roff_name[n->tok]);
		roff_node_delete(man, nc);
	}
}

static void
post_UR(CHKARGS)
{
	if (n->type == ROFFT_HEAD && n->child == NULL)
		mandoc_msg(MANDOCERR_UR_NOHEAD, n->line, n->pos,
		    "%s", roff_name[n->tok]);
}

static void
check_part(CHKARGS)
{
	if (n->type == ROFFT_BODY && n->child == NULL)
		mandoc_msg(MANDOCERR_BLK_EMPTY, n->line, n->pos,
		    "%s", roff_name[n->tok]);
}

static void
check_par(CHKARGS)
{

	switch (n->type) {
	case ROFFT_BLOCK:
		if (n->body->child == NULL)
			roff_node_delete(man, n);
		break;
	case ROFFT_BODY:
		if (n->child != NULL &&
		    (n->child->tok == ROFF_sp || n->child->tok == ROFF_br)) {
			mandoc_msg(MANDOCERR_PAR_SKIP,
			    n->child->line, n->child->pos,
			    "%s after %s", roff_name[n->child->tok],
			    roff_name[n->tok]);
			roff_node_delete(man, n->child);
		}
		if (n->child == NULL)
			mandoc_msg(MANDOCERR_PAR_SKIP, n->line, n->pos,
			    "%s empty", roff_name[n->tok]);
		break;
	case ROFFT_HEAD:
		if (n->child != NULL)
			mandoc_msg(MANDOCERR_ARG_SKIP,
			    n->line, n->pos, "%s %s%s",
			    roff_name[n->tok], n->child->string,
			    n->child->next != NULL ? " ..." : "");
		break;
	default:
		break;
	}
}

static void
post_IP(CHKARGS)
{
	switch (n->type) {
	case ROFFT_BLOCK:
		if (n->head->child == NULL && n->body->child == NULL)
			roff_node_delete(man, n);
		break;
	case ROFFT_HEAD:
		check_tag(n, n->child);
		break;
	case ROFFT_BODY:
		if (n->parent->head->child == NULL && n->child == NULL)
			mandoc_msg(MANDOCERR_PAR_SKIP, n->line, n->pos,
			    "%s empty", roff_name[n->tok]);
		break;
	default:
		break;
	}
}

/*
 * The first next-line element in the head is the tag.
 * If that's a font macro, use its first child instead.
 */
static void
post_TP(CHKARGS)
{
	struct roff_node *nt;

	if (n->type != ROFFT_HEAD || (nt = n->child) == NULL)
		return;

	while ((nt->flags & NODE_LINE) == 0)
		if ((nt = nt->next) == NULL)
			return;

	switch (nt->tok) {
	case MAN_B:
	case MAN_BI:
	case MAN_BR:
	case MAN_I:
	case MAN_IB:
	case MAN_IR:
		nt = nt->child;
		break;
	default:
		break;
	}
	check_tag(n, nt);
}

static void
post_TH(CHKARGS)
{
	struct roff_node *nb;
	const char	*p;

	free(man->meta.title);
	free(man->meta.vol);
	free(man->meta.os);
	free(man->meta.msec);
	free(man->meta.date);

	man->meta.title = man->meta.vol = man->meta.date =
	    man->meta.msec = man->meta.os = NULL;

	nb = n;

	/* ->TITLE<- MSEC DATE OS VOL */

	n = n->child;
	if (n != NULL && n->string != NULL) {
		for (p = n->string; *p != '\0'; p++) {
			/* Only warn about this once... */
			if (isalpha((unsigned char)*p) &&
			    ! isupper((unsigned char)*p)) {
				mandoc_msg(MANDOCERR_TITLE_CASE, n->line,
				    n->pos + (int)(p - n->string),
				    "TH %s", n->string);
				break;
			}
		}
		man->meta.title = mandoc_strdup(n->string);
	} else {
		man->meta.title = mandoc_strdup("");
		mandoc_msg(MANDOCERR_TH_NOTITLE, nb->line, nb->pos, "TH");
	}

	/* TITLE ->MSEC<- DATE OS VOL */

	if (n != NULL)
		n = n->next;
	if (n != NULL && n->string != NULL) {
		man->meta.msec = mandoc_strdup(n->string);
		if (man->filesec != '\0' &&
		    man->filesec != *n->string &&
		    *n->string >= '1' && *n->string <= '9')
			mandoc_msg(MANDOCERR_MSEC_FILE, n->line, n->pos,
			    "*.%c vs TH ... %c", man->filesec, *n->string);
	} else {
		man->meta.msec = mandoc_strdup("");
		mandoc_msg(MANDOCERR_MSEC_MISSING,
		    nb->line, nb->pos, "TH %s", man->meta.title);
	}

	/* TITLE MSEC ->DATE<- OS VOL */

	if (n != NULL)
		n = n->next;
	if (man->quick && n != NULL)
		man->meta.date = mandoc_strdup("");
	else
		man->meta.date = mandoc_normdate(n, nb);

	/* TITLE MSEC DATE ->OS<- VOL */

	if (n && (n = n->next))
		man->meta.os = mandoc_strdup(n->string);
	else if (man->os_s != NULL)
		man->meta.os = mandoc_strdup(man->os_s);
	if (man->meta.os_e == MANDOC_OS_OTHER && man->meta.os != NULL) {
		if (strstr(man->meta.os, "OpenBSD") != NULL)
			man->meta.os_e = MANDOC_OS_OPENBSD;
		else if (strstr(man->meta.os, "NetBSD") != NULL)
			man->meta.os_e = MANDOC_OS_NETBSD;
	}

	/* TITLE MSEC DATE OS ->VOL<- */
	/* If missing, use the default VOL name for MSEC. */

	if (n && (n = n->next))
		man->meta.vol = mandoc_strdup(n->string);
	else if ('\0' != man->meta.msec[0] &&
	    (NULL != (p = mandoc_a2msec(man->meta.msec))))
		man->meta.vol = mandoc_strdup(p);

	if (n != NULL && (n = n->next) != NULL)
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    n->line, n->pos, "TH ... %s", n->string);

	/*
	 * Remove the `TH' node after we've processed it for our
	 * meta-data.
	 */
	roff_node_delete(man, man->last);
}

static void
post_MR(CHKARGS)
{
	struct roff_node *nch;

	if ((nch = n->child) == NULL) {
		mandoc_msg(MANDOCERR_NM_NONAME, n->line, n->pos, "MR");
		return;
	}
	if (nch->next == NULL) {
		mandoc_msg(MANDOCERR_XR_NOSEC,
		    n->line, n->pos, "MR %s", nch->string);
		return;
	}
	if (mandoc_xr_add(nch->next->string, nch->string, nch->line, nch->pos))
		mandoc_msg(MANDOCERR_XR_SELF, nch->line, nch->pos,
		    "MR %s %s", nch->string, nch->next->string);
	if ((nch = nch->next->next) == NULL || nch->next == NULL)
		return;

	mandoc_msg(MANDOCERR_ARG_EXCESS, nch->next->line, nch->next->pos,
	    "MR ... %s", nch->next->string);
	while (nch->next != NULL)
		roff_node_delete(man, nch->next);
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

	if (n == NULL || n->type != ROFFT_TEXT)
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

	free(man->meta.os);
	man->meta.os = mandoc_strdup(p);
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

	struct roff_node *nn;
	const char	*p, *s;

	n = n->child;

	if (n == NULL || n->type != ROFFT_TEXT)
		p = unix_versions[0];
	else {
		s = n->string;
		if (0 == strcmp(s, "3"))
			p = unix_versions[0];
		else if (0 == strcmp(s, "4"))
			p = unix_versions[1];
		else if (0 == strcmp(s, "5")) {
			nn = n->next;
			if (nn != NULL &&
			    nn->type == ROFFT_TEXT &&
			    nn->string[0] != '\0')
				p = unix_versions[3];
			else
				p = unix_versions[2];
		} else
			p = unix_versions[0];
	}

	free(man->meta.os);
	man->meta.os = mandoc_strdup(p);
}

static void
post_in(CHKARGS)
{
	char	*s;

	if (n->parent->tok != MAN_TP ||
	    n->parent->type != ROFFT_HEAD ||
	    n->child == NULL ||
	    *n->child->string == '+' ||
	    *n->child->string == '-')
		return;
	mandoc_asprintf(&s, "+%s", n->child->string);
	free(n->child->string);
	n->child->string = s;
}
