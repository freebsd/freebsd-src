/* $Id: mdoc_validate.c,v 1.389 2021/07/18 11:41:23 schwarze Exp $ */
/*
 * Copyright (c) 2010-2020 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010 Joerg Sonnenberger <joerg@netbsd.org>
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
 * Validation module for mdoc(7) syntax trees used by mandoc(1).
 */
#include "config.h"

#include <sys/types.h>
#ifndef OSNAME
#include <sys/utsname.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "mandoc_xr.h"
#include "roff.h"
#include "mdoc.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "libmdoc.h"
#include "tag.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */

#define	POST_ARGS struct roff_man *mdoc

enum	check_ineq {
	CHECK_LT,
	CHECK_GT,
	CHECK_EQ
};

typedef	void	(*v_post)(POST_ARGS);

static	int	 build_list(struct roff_man *, int);
static	void	 check_argv(struct roff_man *,
			struct roff_node *, struct mdoc_argv *);
static	void	 check_args(struct roff_man *, struct roff_node *);
static	void	 check_text(struct roff_man *, int, int, char *);
static	void	 check_text_em(struct roff_man *, int, int, char *);
static	void	 check_toptext(struct roff_man *, int, int, const char *);
static	int	 child_an(const struct roff_node *);
static	size_t		macro2len(enum roff_tok);
static	void	 rewrite_macro2len(struct roff_man *, char **);
static	int	 similar(const char *, const char *);

static	void	 post_abort(POST_ARGS) __attribute__((__noreturn__));
static	void	 post_an(POST_ARGS);
static	void	 post_an_norm(POST_ARGS);
static	void	 post_at(POST_ARGS);
static	void	 post_bd(POST_ARGS);
static	void	 post_bf(POST_ARGS);
static	void	 post_bk(POST_ARGS);
static	void	 post_bl(POST_ARGS);
static	void	 post_bl_block(POST_ARGS);
static	void	 post_bl_head(POST_ARGS);
static	void	 post_bl_norm(POST_ARGS);
static	void	 post_bx(POST_ARGS);
static	void	 post_defaults(POST_ARGS);
static	void	 post_display(POST_ARGS);
static	void	 post_dd(POST_ARGS);
static	void	 post_delim(POST_ARGS);
static	void	 post_delim_nb(POST_ARGS);
static	void	 post_dt(POST_ARGS);
static	void	 post_em(POST_ARGS);
static	void	 post_en(POST_ARGS);
static	void	 post_er(POST_ARGS);
static	void	 post_es(POST_ARGS);
static	void	 post_eoln(POST_ARGS);
static	void	 post_ex(POST_ARGS);
static	void	 post_fa(POST_ARGS);
static	void	 post_fl(POST_ARGS);
static	void	 post_fn(POST_ARGS);
static	void	 post_fname(POST_ARGS);
static	void	 post_fo(POST_ARGS);
static	void	 post_hyph(POST_ARGS);
static	void	 post_it(POST_ARGS);
static	void	 post_lb(POST_ARGS);
static	void	 post_nd(POST_ARGS);
static	void	 post_nm(POST_ARGS);
static	void	 post_ns(POST_ARGS);
static	void	 post_obsolete(POST_ARGS);
static	void	 post_os(POST_ARGS);
static	void	 post_par(POST_ARGS);
static	void	 post_prevpar(POST_ARGS);
static	void	 post_root(POST_ARGS);
static	void	 post_rs(POST_ARGS);
static	void	 post_rv(POST_ARGS);
static	void	 post_section(POST_ARGS);
static	void	 post_sh(POST_ARGS);
static	void	 post_sh_head(POST_ARGS);
static	void	 post_sh_name(POST_ARGS);
static	void	 post_sh_see_also(POST_ARGS);
static	void	 post_sh_authors(POST_ARGS);
static	void	 post_sm(POST_ARGS);
static	void	 post_st(POST_ARGS);
static	void	 post_std(POST_ARGS);
static	void	 post_sx(POST_ARGS);
static	void	 post_tag(POST_ARGS);
static	void	 post_tg(POST_ARGS);
static	void	 post_useless(POST_ARGS);
static	void	 post_xr(POST_ARGS);
static	void	 post_xx(POST_ARGS);

static	const v_post mdoc_valids[MDOC_MAX - MDOC_Dd] = {
	post_dd,	/* Dd */
	post_dt,	/* Dt */
	post_os,	/* Os */
	post_sh,	/* Sh */
	post_section,	/* Ss */
	post_par,	/* Pp */
	post_display,	/* D1 */
	post_display,	/* Dl */
	post_display,	/* Bd */
	NULL,		/* Ed */
	post_bl,	/* Bl */
	NULL,		/* El */
	post_it,	/* It */
	post_delim_nb,	/* Ad */
	post_an,	/* An */
	NULL,		/* Ap */
	post_defaults,	/* Ar */
	NULL,		/* Cd */
	post_tag,	/* Cm */
	post_tag,	/* Dv */
	post_er,	/* Er */
	post_tag,	/* Ev */
	post_ex,	/* Ex */
	post_fa,	/* Fa */
	NULL,		/* Fd */
	post_fl,	/* Fl */
	post_fn,	/* Fn */
	post_delim_nb,	/* Ft */
	post_tag,	/* Ic */
	post_delim_nb,	/* In */
	post_tag,	/* Li */
	post_nd,	/* Nd */
	post_nm,	/* Nm */
	post_delim_nb,	/* Op */
	post_abort,	/* Ot */
	post_defaults,	/* Pa */
	post_rv,	/* Rv */
	post_st,	/* St */
	post_tag,	/* Va */
	post_delim_nb,	/* Vt */
	post_xr,	/* Xr */
	NULL,		/* %A */
	post_hyph,	/* %B */ /* FIXME: can be used outside Rs/Re. */
	NULL,		/* %D */
	NULL,		/* %I */
	NULL,		/* %J */
	post_hyph,	/* %N */
	post_hyph,	/* %O */
	NULL,		/* %P */
	post_hyph,	/* %R */
	post_hyph,	/* %T */ /* FIXME: can be used outside Rs/Re. */
	NULL,		/* %V */
	NULL,		/* Ac */
	NULL,		/* Ao */
	post_delim_nb,	/* Aq */
	post_at,	/* At */
	NULL,		/* Bc */
	post_bf,	/* Bf */
	NULL,		/* Bo */
	NULL,		/* Bq */
	post_xx,	/* Bsx */
	post_bx,	/* Bx */
	post_obsolete,	/* Db */
	NULL,		/* Dc */
	NULL,		/* Do */
	NULL,		/* Dq */
	NULL,		/* Ec */
	NULL,		/* Ef */
	post_em,	/* Em */
	NULL,		/* Eo */
	post_xx,	/* Fx */
	post_tag,	/* Ms */
	post_tag,	/* No */
	post_ns,	/* Ns */
	post_xx,	/* Nx */
	post_xx,	/* Ox */
	NULL,		/* Pc */
	NULL,		/* Pf */
	NULL,		/* Po */
	post_delim_nb,	/* Pq */
	NULL,		/* Qc */
	post_delim_nb,	/* Ql */
	NULL,		/* Qo */
	post_delim_nb,	/* Qq */
	NULL,		/* Re */
	post_rs,	/* Rs */
	NULL,		/* Sc */
	NULL,		/* So */
	post_delim_nb,	/* Sq */
	post_sm,	/* Sm */
	post_sx,	/* Sx */
	post_em,	/* Sy */
	post_useless,	/* Tn */
	post_xx,	/* Ux */
	NULL,		/* Xc */
	NULL,		/* Xo */
	post_fo,	/* Fo */
	NULL,		/* Fc */
	NULL,		/* Oo */
	NULL,		/* Oc */
	post_bk,	/* Bk */
	NULL,		/* Ek */
	post_eoln,	/* Bt */
	post_obsolete,	/* Hf */
	post_obsolete,	/* Fr */
	post_eoln,	/* Ud */
	post_lb,	/* Lb */
	post_abort,	/* Lp */
	post_delim_nb,	/* Lk */
	post_defaults,	/* Mt */
	post_delim_nb,	/* Brq */
	NULL,		/* Bro */
	NULL,		/* Brc */
	NULL,		/* %C */
	post_es,	/* Es */
	post_en,	/* En */
	post_xx,	/* Dx */
	NULL,		/* %Q */
	NULL,		/* %U */
	NULL,		/* Ta */
	post_tg,	/* Tg */
};

#define	RSORD_MAX 14 /* Number of `Rs' blocks. */

static	const enum roff_tok rsord[RSORD_MAX] = {
	MDOC__A,
	MDOC__T,
	MDOC__B,
	MDOC__I,
	MDOC__J,
	MDOC__R,
	MDOC__N,
	MDOC__V,
	MDOC__U,
	MDOC__P,
	MDOC__Q,
	MDOC__C,
	MDOC__D,
	MDOC__O
};

static	const char * const secnames[SEC__MAX] = {
	NULL,
	"NAME",
	"LIBRARY",
	"SYNOPSIS",
	"DESCRIPTION",
	"CONTEXT",
	"IMPLEMENTATION NOTES",
	"RETURN VALUES",
	"ENVIRONMENT",
	"FILES",
	"EXIT STATUS",
	"EXAMPLES",
	"DIAGNOSTICS",
	"COMPATIBILITY",
	"ERRORS",
	"SEE ALSO",
	"STANDARDS",
	"HISTORY",
	"AUTHORS",
	"CAVEATS",
	"BUGS",
	"SECURITY CONSIDERATIONS",
	NULL
};

static	int	  fn_prio = TAG_STRONG;


/* Validate the subtree rooted at mdoc->last. */
void
mdoc_validate(struct roff_man *mdoc)
{
	struct roff_node *n, *np;
	const v_post *p;

	/*
	 * Translate obsolete macros to modern macros first
	 * such that later code does not need to look
	 * for the obsolete versions.
	 */

	n = mdoc->last;
	switch (n->tok) {
	case MDOC_Lp:
		n->tok = MDOC_Pp;
		break;
	case MDOC_Ot:
		post_obsolete(mdoc);
		n->tok = MDOC_Ft;
		break;
	default:
		break;
	}

	/*
	 * Iterate over all children, recursing into each one
	 * in turn, depth-first.
	 */

	mdoc->last = mdoc->last->child;
	while (mdoc->last != NULL) {
		mdoc_validate(mdoc);
		if (mdoc->last == n)
			mdoc->last = mdoc->last->child;
		else
			mdoc->last = mdoc->last->next;
	}

	/* Finally validate the macro itself. */

	mdoc->last = n;
	mdoc->next = ROFF_NEXT_SIBLING;
	switch (n->type) {
	case ROFFT_TEXT:
		np = n->parent;
		if (n->sec != SEC_SYNOPSIS ||
		    (np->tok != MDOC_Cd && np->tok != MDOC_Fd))
			check_text(mdoc, n->line, n->pos, n->string);
		if ((n->flags & NODE_NOFILL) == 0 &&
		    (np->tok != MDOC_It || np->type != ROFFT_HEAD ||
		     np->parent->parent->norm->Bl.type != LIST_diag))
			check_text_em(mdoc, n->line, n->pos, n->string);
		if (np->tok == MDOC_It || (np->type == ROFFT_BODY &&
		    (np->tok == MDOC_Sh || np->tok == MDOC_Ss)))
			check_toptext(mdoc, n->line, n->pos, n->string);
		break;
	case ROFFT_COMMENT:
	case ROFFT_EQN:
	case ROFFT_TBL:
		break;
	case ROFFT_ROOT:
		post_root(mdoc);
		break;
	default:
		check_args(mdoc, mdoc->last);

		/*
		 * Closing delimiters are not special at the
		 * beginning of a block, opening delimiters
		 * are not special at the end.
		 */

		if (n->child != NULL)
			n->child->flags &= ~NODE_DELIMC;
		if (n->last != NULL)
			n->last->flags &= ~NODE_DELIMO;

		/* Call the macro's postprocessor. */

		if (n->tok < ROFF_MAX) {
			roff_validate(mdoc);
			break;
		}

		assert(n->tok >= MDOC_Dd && n->tok < MDOC_MAX);
		p = mdoc_valids + (n->tok - MDOC_Dd);
		if (*p)
			(*p)(mdoc);
		if (mdoc->last == n)
			mdoc_state(mdoc, n);
		break;
	}
}

static void
check_args(struct roff_man *mdoc, struct roff_node *n)
{
	int		 i;

	if (NULL == n->args)
		return;

	assert(n->args->argc);
	for (i = 0; i < (int)n->args->argc; i++)
		check_argv(mdoc, n, &n->args->argv[i]);
}

static void
check_argv(struct roff_man *mdoc, struct roff_node *n, struct mdoc_argv *v)
{
	int		 i;

	for (i = 0; i < (int)v->sz; i++)
		check_text(mdoc, v->line, v->pos, v->value[i]);
}

static void
check_text(struct roff_man *mdoc, int ln, int pos, char *p)
{
	char		*cp;

	if (mdoc->last->flags & NODE_NOFILL)
		return;

	for (cp = p; NULL != (p = strchr(p, '\t')); p++)
		mandoc_msg(MANDOCERR_FI_TAB, ln, pos + (int)(p - cp), NULL);
}

static void
check_text_em(struct roff_man *mdoc, int ln, int pos, char *p)
{
	const struct roff_node	*np, *nn;
	char			*cp;

	np = mdoc->last->prev;
	nn = mdoc->last->next;

	/* Look for em-dashes wrongly encoded as "--". */

	for (cp = p; *cp != '\0'; cp++) {
		if (cp[0] != '-' || cp[1] != '-')
			continue;
		cp++;

		/* Skip input sequences of more than two '-'. */

		if (cp[1] == '-') {
			while (cp[1] == '-')
				cp++;
			continue;
		}

		/* Skip "--" directly attached to something else. */

		if ((cp - p > 1 && cp[-2] != ' ') ||
		    (cp[1] != '\0' && cp[1] != ' '))
			continue;

		/* Require a letter right before or right afterwards. */

		if ((cp - p > 2 ?
		     isalpha((unsigned char)cp[-3]) :
		     np != NULL &&
		     np->type == ROFFT_TEXT &&
		     *np->string != '\0' &&
		     isalpha((unsigned char)np->string[
		       strlen(np->string) - 1])) ||
		    (cp[1] != '\0' && cp[2] != '\0' ?
		     isalpha((unsigned char)cp[2]) :
		     nn != NULL &&
		     nn->type == ROFFT_TEXT &&
		     isalpha((unsigned char)*nn->string))) {
			mandoc_msg(MANDOCERR_DASHDASH,
			    ln, pos + (int)(cp - p) - 1, NULL);
			break;
		}
	}
}

static void
check_toptext(struct roff_man *mdoc, int ln, int pos, const char *p)
{
	const char	*cp, *cpr;

	if (*p == '\0')
		return;

	if ((cp = strstr(p, "OpenBSD")) != NULL)
		mandoc_msg(MANDOCERR_BX, ln, pos + (int)(cp - p), "Ox");
	if ((cp = strstr(p, "NetBSD")) != NULL)
		mandoc_msg(MANDOCERR_BX, ln, pos + (int)(cp - p), "Nx");
	if ((cp = strstr(p, "FreeBSD")) != NULL)
		mandoc_msg(MANDOCERR_BX, ln, pos + (int)(cp - p), "Fx");
	if ((cp = strstr(p, "DragonFly")) != NULL)
		mandoc_msg(MANDOCERR_BX, ln, pos + (int)(cp - p), "Dx");

	cp = p;
	while ((cp = strstr(cp + 1, "()")) != NULL) {
		for (cpr = cp - 1; cpr >= p; cpr--)
			if (*cpr != '_' && !isalnum((unsigned char)*cpr))
				break;
		if ((cpr < p || *cpr == ' ') && cpr + 1 < cp) {
			cpr++;
			mandoc_msg(MANDOCERR_FUNC, ln, pos + (int)(cpr - p),
			    "%.*s()", (int)(cp - cpr), cpr);
		}
	}
}

static void
post_abort(POST_ARGS)
{
	abort();
}

static void
post_delim(POST_ARGS)
{
	const struct roff_node	*nch;
	const char		*lc;
	enum mdelim		 delim;
	enum roff_tok		 tok;

	tok = mdoc->last->tok;
	nch = mdoc->last->last;
	if (nch == NULL || nch->type != ROFFT_TEXT)
		return;
	lc = strchr(nch->string, '\0') - 1;
	if (lc < nch->string)
		return;
	delim = mdoc_isdelim(lc);
	if (delim == DELIM_NONE || delim == DELIM_OPEN)
		return;
	if (*lc == ')' && (tok == MDOC_Nd || tok == MDOC_Sh ||
	    tok == MDOC_Ss || tok == MDOC_Fo))
		return;

	mandoc_msg(MANDOCERR_DELIM, nch->line,
	    nch->pos + (int)(lc - nch->string), "%s%s %s", roff_name[tok],
	    nch == mdoc->last->child ? "" : " ...", nch->string);
}

static void
post_delim_nb(POST_ARGS)
{
	const struct roff_node	*nch;
	const char		*lc, *cp;
	int			 nw;
	enum mdelim		 delim;
	enum roff_tok		 tok;

	/*
	 * Find candidates: at least two bytes,
	 * the last one a closing or middle delimiter.
	 */

	tok = mdoc->last->tok;
	nch = mdoc->last->last;
	if (nch == NULL || nch->type != ROFFT_TEXT)
		return;
	lc = strchr(nch->string, '\0') - 1;
	if (lc <= nch->string)
		return;
	delim = mdoc_isdelim(lc);
	if (delim == DELIM_NONE || delim == DELIM_OPEN)
		return;

	/*
	 * Reduce false positives by allowing various cases.
	 */

	/* Escaped delimiters. */
	if (lc > nch->string + 1 && lc[-2] == '\\' &&
	    (lc[-1] == '&' || lc[-1] == 'e'))
		return;

	/* Specific byte sequences. */
	switch (*lc) {
	case ')':
		for (cp = lc; cp >= nch->string; cp--)
			if (*cp == '(')
				return;
		break;
	case '.':
		if (lc > nch->string + 1 && lc[-2] == '.' && lc[-1] == '.')
			return;
		if (lc[-1] == '.')
			return;
		break;
	case ';':
		if (tok == MDOC_Vt)
			return;
		break;
	case '?':
		if (lc[-1] == '?')
			return;
		break;
	case ']':
		for (cp = lc; cp >= nch->string; cp--)
			if (*cp == '[')
				return;
		break;
	case '|':
		if (lc == nch->string + 1 && lc[-1] == '|')
			return;
	default:
		break;
	}

	/* Exactly two non-alphanumeric bytes. */
	if (lc == nch->string + 1 && !isalnum((unsigned char)lc[-1]))
		return;

	/* At least three alphabetic words with a sentence ending. */
	if (strchr("!.:?", *lc) != NULL && (tok == MDOC_Em ||
	    tok == MDOC_Li || tok == MDOC_Pq || tok == MDOC_Sy)) {
		nw = 0;
		for (cp = lc - 1; cp >= nch->string; cp--) {
			if (*cp == ' ') {
				nw++;
				if (cp > nch->string && cp[-1] == ',')
					cp--;
			} else if (isalpha((unsigned int)*cp)) {
				if (nw > 1)
					return;
			} else
				break;
		}
	}

	mandoc_msg(MANDOCERR_DELIM_NB, nch->line,
	    nch->pos + (int)(lc - nch->string), "%s%s %s", roff_name[tok],
	    nch == mdoc->last->child ? "" : " ...", nch->string);
}

static void
post_bl_norm(POST_ARGS)
{
	struct roff_node *n;
	struct mdoc_argv *argv, *wa;
	int		  i;
	enum mdocargt	  mdoclt;
	enum mdoc_list	  lt;

	n = mdoc->last->parent;
	n->norm->Bl.type = LIST__NONE;

	/*
	 * First figure out which kind of list to use: bind ourselves to
	 * the first mentioned list type and warn about any remaining
	 * ones.  If we find no list type, we default to LIST_item.
	 */

	wa = (n->args == NULL) ? NULL : n->args->argv;
	mdoclt = MDOC_ARG_MAX;
	for (i = 0; n->args && i < (int)n->args->argc; i++) {
		argv = n->args->argv + i;
		lt = LIST__NONE;
		switch (argv->arg) {
		/* Set list types. */
		case MDOC_Bullet:
			lt = LIST_bullet;
			break;
		case MDOC_Dash:
			lt = LIST_dash;
			break;
		case MDOC_Enum:
			lt = LIST_enum;
			break;
		case MDOC_Hyphen:
			lt = LIST_hyphen;
			break;
		case MDOC_Item:
			lt = LIST_item;
			break;
		case MDOC_Tag:
			lt = LIST_tag;
			break;
		case MDOC_Diag:
			lt = LIST_diag;
			break;
		case MDOC_Hang:
			lt = LIST_hang;
			break;
		case MDOC_Ohang:
			lt = LIST_ohang;
			break;
		case MDOC_Inset:
			lt = LIST_inset;
			break;
		case MDOC_Column:
			lt = LIST_column;
			break;
		/* Set list arguments. */
		case MDOC_Compact:
			if (n->norm->Bl.comp)
				mandoc_msg(MANDOCERR_ARG_REP,
				    argv->line, argv->pos, "Bl -compact");
			n->norm->Bl.comp = 1;
			break;
		case MDOC_Width:
			wa = argv;
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    argv->line, argv->pos, "Bl -width");
				n->norm->Bl.width = "0n";
				break;
			}
			if (NULL != n->norm->Bl.width)
				mandoc_msg(MANDOCERR_ARG_REP,
				    argv->line, argv->pos,
				    "Bl -width %s", argv->value[0]);
			rewrite_macro2len(mdoc, argv->value);
			n->norm->Bl.width = argv->value[0];
			break;
		case MDOC_Offset:
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    argv->line, argv->pos, "Bl -offset");
				break;
			}
			if (NULL != n->norm->Bl.offs)
				mandoc_msg(MANDOCERR_ARG_REP,
				    argv->line, argv->pos,
				    "Bl -offset %s", argv->value[0]);
			rewrite_macro2len(mdoc, argv->value);
			n->norm->Bl.offs = argv->value[0];
			break;
		default:
			continue;
		}
		if (LIST__NONE == lt)
			continue;
		mdoclt = argv->arg;

		/* Check: multiple list types. */

		if (LIST__NONE != n->norm->Bl.type) {
			mandoc_msg(MANDOCERR_BL_REP, n->line, n->pos,
			    "Bl -%s", mdoc_argnames[argv->arg]);
			continue;
		}

		/* The list type should come first. */

		if (n->norm->Bl.width ||
		    n->norm->Bl.offs ||
		    n->norm->Bl.comp)
			mandoc_msg(MANDOCERR_BL_LATETYPE,
			    n->line, n->pos, "Bl -%s",
			    mdoc_argnames[n->args->argv[0].arg]);

		n->norm->Bl.type = lt;
		if (LIST_column == lt) {
			n->norm->Bl.ncols = argv->sz;
			n->norm->Bl.cols = (void *)argv->value;
		}
	}

	/* Allow lists to default to LIST_item. */

	if (LIST__NONE == n->norm->Bl.type) {
		mandoc_msg(MANDOCERR_BL_NOTYPE, n->line, n->pos, "Bl");
		n->norm->Bl.type = LIST_item;
		mdoclt = MDOC_Item;
	}

	/*
	 * Validate the width field.  Some list types don't need width
	 * types and should be warned about them.  Others should have it
	 * and must also be warned.  Yet others have a default and need
	 * no warning.
	 */

	switch (n->norm->Bl.type) {
	case LIST_tag:
		if (n->norm->Bl.width == NULL)
			mandoc_msg(MANDOCERR_BL_NOWIDTH,
			    n->line, n->pos, "Bl -tag");
		break;
	case LIST_column:
	case LIST_diag:
	case LIST_ohang:
	case LIST_inset:
	case LIST_item:
		if (n->norm->Bl.width != NULL)
			mandoc_msg(MANDOCERR_BL_SKIPW, wa->line, wa->pos,
			    "Bl -%s", mdoc_argnames[mdoclt]);
		n->norm->Bl.width = NULL;
		break;
	case LIST_bullet:
	case LIST_dash:
	case LIST_hyphen:
		if (n->norm->Bl.width == NULL)
			n->norm->Bl.width = "2n";
		break;
	case LIST_enum:
		if (n->norm->Bl.width == NULL)
			n->norm->Bl.width = "3n";
		break;
	default:
		break;
	}
}

static void
post_bd(POST_ARGS)
{
	struct roff_node *n;
	struct mdoc_argv *argv;
	int		  i;
	enum mdoc_disp	  dt;

	n = mdoc->last;
	for (i = 0; n->args && i < (int)n->args->argc; i++) {
		argv = n->args->argv + i;
		dt = DISP__NONE;

		switch (argv->arg) {
		case MDOC_Centred:
			dt = DISP_centered;
			break;
		case MDOC_Ragged:
			dt = DISP_ragged;
			break;
		case MDOC_Unfilled:
			dt = DISP_unfilled;
			break;
		case MDOC_Filled:
			dt = DISP_filled;
			break;
		case MDOC_Literal:
			dt = DISP_literal;
			break;
		case MDOC_File:
			mandoc_msg(MANDOCERR_BD_FILE, n->line, n->pos, NULL);
			break;
		case MDOC_Offset:
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    argv->line, argv->pos, "Bd -offset");
				break;
			}
			if (NULL != n->norm->Bd.offs)
				mandoc_msg(MANDOCERR_ARG_REP,
				    argv->line, argv->pos,
				    "Bd -offset %s", argv->value[0]);
			rewrite_macro2len(mdoc, argv->value);
			n->norm->Bd.offs = argv->value[0];
			break;
		case MDOC_Compact:
			if (n->norm->Bd.comp)
				mandoc_msg(MANDOCERR_ARG_REP,
				    argv->line, argv->pos, "Bd -compact");
			n->norm->Bd.comp = 1;
			break;
		default:
			abort();
		}
		if (DISP__NONE == dt)
			continue;

		if (DISP__NONE == n->norm->Bd.type)
			n->norm->Bd.type = dt;
		else
			mandoc_msg(MANDOCERR_BD_REP, n->line, n->pos,
			    "Bd -%s", mdoc_argnames[argv->arg]);
	}

	if (DISP__NONE == n->norm->Bd.type) {
		mandoc_msg(MANDOCERR_BD_NOTYPE, n->line, n->pos, "Bd");
		n->norm->Bd.type = DISP_ragged;
	}
}

/*
 * Stand-alone line macros.
 */

static void
post_an_norm(POST_ARGS)
{
	struct roff_node *n;
	struct mdoc_argv *argv;
	size_t	 i;

	n = mdoc->last;
	if (n->args == NULL)
		return;

	for (i = 1; i < n->args->argc; i++) {
		argv = n->args->argv + i;
		mandoc_msg(MANDOCERR_AN_REP, argv->line, argv->pos,
		    "An -%s", mdoc_argnames[argv->arg]);
	}

	argv = n->args->argv;
	if (argv->arg == MDOC_Split)
		n->norm->An.auth = AUTH_split;
	else if (argv->arg == MDOC_Nosplit)
		n->norm->An.auth = AUTH_nosplit;
	else
		abort();
}

static void
post_eoln(POST_ARGS)
{
	struct roff_node	*n;

	post_useless(mdoc);
	n = mdoc->last;
	if (n->child != NULL)
		mandoc_msg(MANDOCERR_ARG_SKIP, n->line,
		    n->pos, "%s %s", roff_name[n->tok], n->child->string);

	while (n->child != NULL)
		roff_node_delete(mdoc, n->child);

	roff_word_alloc(mdoc, n->line, n->pos, n->tok == MDOC_Bt ?
	    "is currently in beta test." : "currently under development.");
	mdoc->last->flags |= NODE_EOS | NODE_NOSRC;
	mdoc->last = n;
}

static int
build_list(struct roff_man *mdoc, int tok)
{
	struct roff_node	*n;
	int			 ic;

	n = mdoc->last->next;
	for (ic = 1;; ic++) {
		roff_elem_alloc(mdoc, n->line, n->pos, tok);
		mdoc->last->flags |= NODE_NOSRC;
		roff_node_relink(mdoc, n);
		n = mdoc->last = mdoc->last->parent;
		mdoc->next = ROFF_NEXT_SIBLING;
		if (n->next == NULL)
			return ic;
		if (ic > 1 || n->next->next != NULL) {
			roff_word_alloc(mdoc, n->line, n->pos, ",");
			mdoc->last->flags |= NODE_DELIMC | NODE_NOSRC;
		}
		n = mdoc->last->next;
		if (n->next == NULL) {
			roff_word_alloc(mdoc, n->line, n->pos, "and");
			mdoc->last->flags |= NODE_NOSRC;
		}
	}
}

static void
post_ex(POST_ARGS)
{
	struct roff_node	*n;
	int			 ic;

	post_std(mdoc);

	n = mdoc->last;
	mdoc->next = ROFF_NEXT_CHILD;
	roff_word_alloc(mdoc, n->line, n->pos, "The");
	mdoc->last->flags |= NODE_NOSRC;

	if (mdoc->last->next != NULL)
		ic = build_list(mdoc, MDOC_Nm);
	else if (mdoc->meta.name != NULL) {
		roff_elem_alloc(mdoc, n->line, n->pos, MDOC_Nm);
		mdoc->last->flags |= NODE_NOSRC;
		roff_word_alloc(mdoc, n->line, n->pos, mdoc->meta.name);
		mdoc->last->flags |= NODE_NOSRC;
		mdoc->last = mdoc->last->parent;
		mdoc->next = ROFF_NEXT_SIBLING;
		ic = 1;
	} else {
		mandoc_msg(MANDOCERR_EX_NONAME, n->line, n->pos, "Ex");
		ic = 0;
	}

	roff_word_alloc(mdoc, n->line, n->pos,
	    ic > 1 ? "utilities exit\\~0" : "utility exits\\~0");
	mdoc->last->flags |= NODE_NOSRC;
	roff_word_alloc(mdoc, n->line, n->pos,
	    "on success, and\\~>0 if an error occurs.");
	mdoc->last->flags |= NODE_EOS | NODE_NOSRC;
	mdoc->last = n;
}

static void
post_lb(POST_ARGS)
{
	struct roff_node	*n;
	const char		*p;

	post_delim_nb(mdoc);

	n = mdoc->last;
	assert(n->child->type == ROFFT_TEXT);
	mdoc->next = ROFF_NEXT_CHILD;

	if ((p = mdoc_a2lib(n->child->string)) != NULL) {
		n->child->flags |= NODE_NOPRT;
		roff_word_alloc(mdoc, n->line, n->pos, p);
		mdoc->last->flags = NODE_NOSRC;
		mdoc->last = n;
		return;
	}

	mandoc_msg(MANDOCERR_LB_BAD, n->child->line,
	    n->child->pos, "Lb %s", n->child->string);

	roff_word_alloc(mdoc, n->line, n->pos, "library");
	mdoc->last->flags = NODE_NOSRC;
	roff_word_alloc(mdoc, n->line, n->pos, "\\(lq");
	mdoc->last->flags = NODE_DELIMO | NODE_NOSRC;
	mdoc->last = mdoc->last->next;
	roff_word_alloc(mdoc, n->line, n->pos, "\\(rq");
	mdoc->last->flags = NODE_DELIMC | NODE_NOSRC;
	mdoc->last = n;
}

static void
post_rv(POST_ARGS)
{
	struct roff_node	*n;
	int			 ic;

	post_std(mdoc);

	n = mdoc->last;
	mdoc->next = ROFF_NEXT_CHILD;
	if (n->child != NULL) {
		roff_word_alloc(mdoc, n->line, n->pos, "The");
		mdoc->last->flags |= NODE_NOSRC;
		ic = build_list(mdoc, MDOC_Fn);
		roff_word_alloc(mdoc, n->line, n->pos,
		    ic > 1 ? "functions return" : "function returns");
		mdoc->last->flags |= NODE_NOSRC;
		roff_word_alloc(mdoc, n->line, n->pos,
		    "the value\\~0 if successful;");
	} else
		roff_word_alloc(mdoc, n->line, n->pos, "Upon successful "
		    "completion, the value\\~0 is returned;");
	mdoc->last->flags |= NODE_NOSRC;

	roff_word_alloc(mdoc, n->line, n->pos, "otherwise "
	    "the value\\~\\-1 is returned and the global variable");
	mdoc->last->flags |= NODE_NOSRC;
	roff_elem_alloc(mdoc, n->line, n->pos, MDOC_Va);
	mdoc->last->flags |= NODE_NOSRC;
	roff_word_alloc(mdoc, n->line, n->pos, "errno");
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last = mdoc->last->parent;
	mdoc->next = ROFF_NEXT_SIBLING;
	roff_word_alloc(mdoc, n->line, n->pos,
	    "is set to indicate the error.");
	mdoc->last->flags |= NODE_EOS | NODE_NOSRC;
	mdoc->last = n;
}

static void
post_std(POST_ARGS)
{
	struct roff_node *n;

	post_delim(mdoc);

	n = mdoc->last;
	if (n->args && n->args->argc == 1)
		if (n->args->argv[0].arg == MDOC_Std)
			return;

	mandoc_msg(MANDOCERR_ARG_STD, n->line, n->pos,
	    "%s", roff_name[n->tok]);
}

static void
post_st(POST_ARGS)
{
	struct roff_node	 *n, *nch;
	const char		 *p;

	n = mdoc->last;
	nch = n->child;
	assert(nch->type == ROFFT_TEXT);

	if ((p = mdoc_a2st(nch->string)) == NULL) {
		mandoc_msg(MANDOCERR_ST_BAD,
		    nch->line, nch->pos, "St %s", nch->string);
		roff_node_delete(mdoc, n);
		return;
	}

	nch->flags |= NODE_NOPRT;
	mdoc->next = ROFF_NEXT_CHILD;
	roff_word_alloc(mdoc, nch->line, nch->pos, p);
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last= n;
}

static void
post_tg(POST_ARGS)
{
	struct roff_node *n;	/* The .Tg node. */
	struct roff_node *nch;	/* The first child of the .Tg node. */
	struct roff_node *nn;   /* The next node after the .Tg node. */
	struct roff_node *np;	/* The parent of the next node. */
	struct roff_node *nt;	/* The TEXT node containing the tag. */
	size_t		  len;	/* The number of bytes in the tag. */

	/* Find the next node. */
	n = mdoc->last;
	for (nn = n; nn != NULL; nn = nn->parent) {
		if (nn->next != NULL) {
			nn = nn->next;
			break;
		}
	}

	/* Find the tag. */
	nt = nch = n->child;
	if (nch == NULL && nn != NULL && nn->child != NULL &&
	    nn->child->type == ROFFT_TEXT)
		nt = nn->child;

	/* Validate the tag. */
	if (nt == NULL || *nt->string == '\0')
		mandoc_msg(MANDOCERR_MACRO_EMPTY, n->line, n->pos, "Tg");
	if (nt == NULL) {
		roff_node_delete(mdoc, n);
		return;
	}
	len = strcspn(nt->string, " \t\\");
	if (nt->string[len] != '\0')
		mandoc_msg(MANDOCERR_TG_SPC, nt->line,
		    nt->pos + len, "Tg %s", nt->string);

	/* Keep only the first argument. */
	if (nch != NULL && nch->next != NULL) {
		mandoc_msg(MANDOCERR_ARG_EXCESS, nch->next->line,
		    nch->next->pos, "Tg ... %s", nch->next->string);
		while (nch->next != NULL)
			roff_node_delete(mdoc, nch->next);
	}

	/* Drop the macro if the first argument is invalid. */
	if (len == 0 || nt->string[len] != '\0') {
		roff_node_delete(mdoc, n);
		return;
	}

	/* By default, tag the .Tg node itself. */
	if (nn == NULL || nn->flags & NODE_ID)
		nn = n;

	/* Explicit tagging of specific macros. */
	switch (nn->tok) {
	case MDOC_Sh:
	case MDOC_Ss:
	case MDOC_Fo:
		nn = nn->head->child == NULL ? n : nn->head;
		break;
	case MDOC_It:
		np = nn->parent;
		while (np->tok != MDOC_Bl)
			np = np->parent;
		switch (np->norm->Bl.type) {
		case LIST_column:
			break;
		case LIST_diag:
		case LIST_hang:
		case LIST_inset:
		case LIST_ohang:
		case LIST_tag:
			nn = nn->head;
			break;
		case LIST_bullet:
		case LIST_dash:
		case LIST_enum:
		case LIST_hyphen:
		case LIST_item:
			nn = nn->body->child == NULL ? n : nn->body;
			break;
		default:
			abort();
		}
		break;
	case MDOC_Bd:
	case MDOC_Bl:
	case MDOC_D1:
	case MDOC_Dl:
		nn = nn->body->child == NULL ? n : nn->body;
		break;
	case MDOC_Pp:
		break;
	case MDOC_Cm:
	case MDOC_Dv:
	case MDOC_Em:
	case MDOC_Er:
	case MDOC_Ev:
	case MDOC_Fl:
	case MDOC_Fn:
	case MDOC_Ic:
	case MDOC_Li:
	case MDOC_Ms:
	case MDOC_No:
	case MDOC_Sy:
		if (nn->child == NULL)
			nn = n;
		break;
	default:
		nn = n;
		break;
	}
	tag_put(nt->string, TAG_MANUAL, nn);
	if (nn != n)
		n->flags |= NODE_NOPRT;
}

static void
post_obsolete(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	if (n->type == ROFFT_ELEM || n->type == ROFFT_BLOCK)
		mandoc_msg(MANDOCERR_MACRO_OBS, n->line, n->pos,
		    "%s", roff_name[n->tok]);
}

static void
post_useless(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	mandoc_msg(MANDOCERR_MACRO_USELESS, n->line, n->pos,
	    "%s", roff_name[n->tok]);
}

/*
 * Block macros.
 */

static void
post_bf(POST_ARGS)
{
	struct roff_node *np, *nch;

	/*
	 * Unlike other data pointers, these are "housed" by the HEAD
	 * element, which contains the goods.
	 */

	np = mdoc->last;
	if (np->type != ROFFT_HEAD)
		return;

	assert(np->parent->type == ROFFT_BLOCK);
	assert(np->parent->tok == MDOC_Bf);

	/* Check the number of arguments. */

	nch = np->child;
	if (np->parent->args == NULL) {
		if (nch == NULL) {
			mandoc_msg(MANDOCERR_BF_NOFONT,
			    np->line, np->pos, "Bf");
			return;
		}
		nch = nch->next;
	}
	if (nch != NULL)
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    nch->line, nch->pos, "Bf ... %s", nch->string);

	/* Extract argument into data. */

	if (np->parent->args != NULL) {
		switch (np->parent->args->argv[0].arg) {
		case MDOC_Emphasis:
			np->norm->Bf.font = FONT_Em;
			break;
		case MDOC_Literal:
			np->norm->Bf.font = FONT_Li;
			break;
		case MDOC_Symbolic:
			np->norm->Bf.font = FONT_Sy;
			break;
		default:
			abort();
		}
		return;
	}

	/* Extract parameter into data. */

	if ( ! strcmp(np->child->string, "Em"))
		np->norm->Bf.font = FONT_Em;
	else if ( ! strcmp(np->child->string, "Li"))
		np->norm->Bf.font = FONT_Li;
	else if ( ! strcmp(np->child->string, "Sy"))
		np->norm->Bf.font = FONT_Sy;
	else
		mandoc_msg(MANDOCERR_BF_BADFONT, np->child->line,
		    np->child->pos, "Bf %s", np->child->string);
}

static void
post_fname(POST_ARGS)
{
	struct roff_node	*n, *nch;
	const char		*cp;
	size_t			 pos;

	n = mdoc->last;
	nch = n->child;
	cp = nch->string;
	if (*cp == '(') {
		if (cp[strlen(cp + 1)] == ')')
			return;
		pos = 0;
	} else {
		pos = strcspn(cp, "()");
		if (cp[pos] == '\0') {
			if (n->sec == SEC_DESCRIPTION ||
			    n->sec == SEC_CUSTOM)
				tag_put(NULL, fn_prio++, n);
			return;
		}
	}
	mandoc_msg(MANDOCERR_FN_PAREN, nch->line, nch->pos + pos, "%s", cp);
}

static void
post_fn(POST_ARGS)
{
	post_fname(mdoc);
	post_fa(mdoc);
}

static void
post_fo(POST_ARGS)
{
	const struct roff_node	*n;

	n = mdoc->last;

	if (n->type != ROFFT_HEAD)
		return;

	if (n->child == NULL) {
		mandoc_msg(MANDOCERR_FO_NOHEAD, n->line, n->pos, "Fo");
		return;
	}
	if (n->child != n->last) {
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    n->child->next->line, n->child->next->pos,
		    "Fo ... %s", n->child->next->string);
		while (n->child != n->last)
			roff_node_delete(mdoc, n->last);
	} else
		post_delim(mdoc);

	post_fname(mdoc);
}

static void
post_fa(POST_ARGS)
{
	const struct roff_node *n;
	const char *cp;

	for (n = mdoc->last->child; n != NULL; n = n->next) {
		for (cp = n->string; *cp != '\0'; cp++) {
			/* Ignore callbacks and alterations. */
			if (*cp == '(' || *cp == '{')
				break;
			if (*cp != ',')
				continue;
			mandoc_msg(MANDOCERR_FA_COMMA, n->line,
			    n->pos + (int)(cp - n->string), "%s", n->string);
			break;
		}
	}
	post_delim_nb(mdoc);
}

static void
post_nm(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;

	if (n->sec == SEC_NAME && n->child != NULL &&
	    n->child->type == ROFFT_TEXT && mdoc->meta.msec != NULL)
		mandoc_xr_add(mdoc->meta.msec, n->child->string, -1, -1);

	if (n->last != NULL && n->last->tok == MDOC_Pp)
		roff_node_relink(mdoc, n->last);

	if (mdoc->meta.name == NULL)
		deroff(&mdoc->meta.name, n);

	if (mdoc->meta.name == NULL ||
	    (mdoc->lastsec == SEC_NAME && n->child == NULL))
		mandoc_msg(MANDOCERR_NM_NONAME, n->line, n->pos, "Nm");

	switch (n->type) {
	case ROFFT_ELEM:
		post_delim_nb(mdoc);
		break;
	case ROFFT_HEAD:
		post_delim(mdoc);
		break;
	default:
		return;
	}

	if ((n->child != NULL && n->child->type == ROFFT_TEXT) ||
	    mdoc->meta.name == NULL)
		return;

	mdoc->next = ROFF_NEXT_CHILD;
	roff_word_alloc(mdoc, n->line, n->pos, mdoc->meta.name);
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last = n;
}

static void
post_nd(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;

	if (n->type != ROFFT_BODY)
		return;

	if (n->sec != SEC_NAME)
		mandoc_msg(MANDOCERR_ND_LATE, n->line, n->pos, "Nd");

	if (n->child == NULL)
		mandoc_msg(MANDOCERR_ND_EMPTY, n->line, n->pos, "Nd");
	else
		post_delim(mdoc);

	post_hyph(mdoc);
}

static void
post_display(POST_ARGS)
{
	struct roff_node *n, *np;

	n = mdoc->last;
	switch (n->type) {
	case ROFFT_BODY:
		if (n->end != ENDBODY_NOT) {
			if (n->tok == MDOC_Bd &&
			    n->body->parent->args == NULL)
				roff_node_delete(mdoc, n);
		} else if (n->child == NULL)
			mandoc_msg(MANDOCERR_BLK_EMPTY, n->line, n->pos,
			    "%s", roff_name[n->tok]);
		else if (n->tok == MDOC_D1)
			post_hyph(mdoc);
		break;
	case ROFFT_BLOCK:
		if (n->tok == MDOC_Bd) {
			if (n->args == NULL) {
				mandoc_msg(MANDOCERR_BD_NOARG,
				    n->line, n->pos, "Bd");
				mdoc->next = ROFF_NEXT_SIBLING;
				while (n->body->child != NULL)
					roff_node_relink(mdoc,
					    n->body->child);
				roff_node_delete(mdoc, n);
				break;
			}
			post_bd(mdoc);
			post_prevpar(mdoc);
		}
		for (np = n->parent; np != NULL; np = np->parent) {
			if (np->type == ROFFT_BLOCK && np->tok == MDOC_Bd) {
				mandoc_msg(MANDOCERR_BD_NEST, n->line,
				    n->pos, "%s in Bd", roff_name[n->tok]);
				break;
			}
		}
		break;
	default:
		break;
	}
}

static void
post_defaults(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	if (n->child != NULL) {
		post_delim_nb(mdoc);
		return;
	}
	mdoc->next = ROFF_NEXT_CHILD;
	switch (n->tok) {
	case MDOC_Ar:
		roff_word_alloc(mdoc, n->line, n->pos, "file");
		mdoc->last->flags |= NODE_NOSRC;
		roff_word_alloc(mdoc, n->line, n->pos, "...");
		break;
	case MDOC_Pa:
	case MDOC_Mt:
		roff_word_alloc(mdoc, n->line, n->pos, "~");
		break;
	default:
		abort();
	}
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last = n;
}

static void
post_at(POST_ARGS)
{
	struct roff_node	*n, *nch;
	const char		*att;

	n = mdoc->last;
	nch = n->child;

	/*
	 * If we have a child, look it up in the standard keys.  If a
	 * key exist, use that instead of the child; if it doesn't,
	 * prefix "AT&T UNIX " to the existing data.
	 */

	att = NULL;
	if (nch != NULL && ((att = mdoc_a2att(nch->string)) == NULL))
		mandoc_msg(MANDOCERR_AT_BAD,
		    nch->line, nch->pos, "At %s", nch->string);

	mdoc->next = ROFF_NEXT_CHILD;
	if (att != NULL) {
		roff_word_alloc(mdoc, nch->line, nch->pos, att);
		nch->flags |= NODE_NOPRT;
	} else
		roff_word_alloc(mdoc, n->line, n->pos, "AT&T UNIX");
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last = n;
}

static void
post_an(POST_ARGS)
{
	struct roff_node *np, *nch;

	post_an_norm(mdoc);

	np = mdoc->last;
	nch = np->child;
	if (np->norm->An.auth == AUTH__NONE) {
		if (nch == NULL)
			mandoc_msg(MANDOCERR_MACRO_EMPTY,
			    np->line, np->pos, "An");
		else
			post_delim_nb(mdoc);
	} else if (nch != NULL)
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    nch->line, nch->pos, "An ... %s", nch->string);
}

static void
post_em(POST_ARGS)
{
	post_tag(mdoc);
	tag_put(NULL, TAG_FALLBACK, mdoc->last);
}

static void
post_en(POST_ARGS)
{
	post_obsolete(mdoc);
	if (mdoc->last->type == ROFFT_BLOCK)
		mdoc->last->norm->Es = mdoc->last_es;
}

static void
post_er(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	if (n->sec == SEC_ERRORS &&
	    (n->parent->tok == MDOC_It ||
	     (n->parent->tok == MDOC_Bq &&
	      n->parent->parent->parent->tok == MDOC_It)))
		tag_put(NULL, TAG_STRONG, n);
	post_delim_nb(mdoc);
}

static void
post_tag(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	if ((n->prev == NULL ||
	     (n->prev->type == ROFFT_TEXT &&
	      strcmp(n->prev->string, "|") == 0)) &&
	    (n->parent->tok == MDOC_It ||
	     (n->parent->tok == MDOC_Xo &&
	      n->parent->parent->prev == NULL &&
	      n->parent->parent->parent->tok == MDOC_It)))
		tag_put(NULL, TAG_STRONG, n);
	post_delim_nb(mdoc);
}

static void
post_es(POST_ARGS)
{
	post_obsolete(mdoc);
	mdoc->last_es = mdoc->last;
}

static void
post_fl(POST_ARGS)
{
	struct roff_node	*n;
	char			*cp;

	/*
	 * Transform ".Fl Fl long" to ".Fl \-long",
	 * resulting for example in better HTML output.
	 */

	n = mdoc->last;
	if (n->prev != NULL && n->prev->tok == MDOC_Fl &&
	    n->prev->child == NULL && n->child != NULL &&
	    (n->flags & NODE_LINE) == 0) {
		mandoc_asprintf(&cp, "\\-%s", n->child->string);
		free(n->child->string);
		n->child->string = cp;
		roff_node_delete(mdoc, n->prev);
	}
	post_tag(mdoc);
}

static void
post_xx(POST_ARGS)
{
	struct roff_node	*n;
	const char		*os;
	char			*v;

	post_delim_nb(mdoc);

	n = mdoc->last;
	switch (n->tok) {
	case MDOC_Bsx:
		os = "BSD/OS";
		break;
	case MDOC_Dx:
		os = "DragonFly";
		break;
	case MDOC_Fx:
		os = "FreeBSD";
		break;
	case MDOC_Nx:
		os = "NetBSD";
		if (n->child == NULL)
			break;
		v = n->child->string;
		if ((v[0] != '0' && v[0] != '1') || v[1] != '.' ||
		    v[2] < '0' || v[2] > '9' ||
		    v[3] < 'a' || v[3] > 'z' || v[4] != '\0')
			break;
		n->child->flags |= NODE_NOPRT;
		mdoc->next = ROFF_NEXT_CHILD;
		roff_word_alloc(mdoc, n->child->line, n->child->pos, v);
		v = mdoc->last->string;
		v[3] = toupper((unsigned char)v[3]);
		mdoc->last->flags |= NODE_NOSRC;
		mdoc->last = n;
		break;
	case MDOC_Ox:
		os = "OpenBSD";
		break;
	case MDOC_Ux:
		os = "UNIX";
		break;
	default:
		abort();
	}
	mdoc->next = ROFF_NEXT_CHILD;
	roff_word_alloc(mdoc, n->line, n->pos, os);
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last = n;
}

static void
post_it(POST_ARGS)
{
	struct roff_node *nbl, *nit, *nch;
	int		  i, cols;
	enum mdoc_list	  lt;

	post_prevpar(mdoc);

	nit = mdoc->last;
	if (nit->type != ROFFT_BLOCK)
		return;

	nbl = nit->parent->parent;
	lt = nbl->norm->Bl.type;

	switch (lt) {
	case LIST_tag:
	case LIST_hang:
	case LIST_ohang:
	case LIST_inset:
	case LIST_diag:
		if (nit->head->child == NULL)
			mandoc_msg(MANDOCERR_IT_NOHEAD,
			    nit->line, nit->pos, "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		break;
	case LIST_bullet:
	case LIST_dash:
	case LIST_enum:
	case LIST_hyphen:
		if (nit->body == NULL || nit->body->child == NULL)
			mandoc_msg(MANDOCERR_IT_NOBODY,
			    nit->line, nit->pos, "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		/* FALLTHROUGH */
	case LIST_item:
		if ((nch = nit->head->child) != NULL)
			mandoc_msg(MANDOCERR_ARG_SKIP,
			    nit->line, nit->pos, "It %s",
			    nch->type == ROFFT_TEXT ? nch->string :
			    roff_name[nch->tok]);
		break;
	case LIST_column:
		cols = (int)nbl->norm->Bl.ncols;

		assert(nit->head->child == NULL);

		if (nit->head->next->child == NULL &&
		    nit->head->next->next == NULL) {
			mandoc_msg(MANDOCERR_MACRO_EMPTY,
			    nit->line, nit->pos, "It");
			roff_node_delete(mdoc, nit);
			break;
		}

		i = 0;
		for (nch = nit->child; nch != NULL; nch = nch->next) {
			if (nch->type != ROFFT_BODY)
				continue;
			if (i++ && nch->flags & NODE_LINE)
				mandoc_msg(MANDOCERR_TA_LINE,
				    nch->line, nch->pos, "Ta");
		}
		if (i < cols || i > cols + 1)
			mandoc_msg(MANDOCERR_BL_COL, nit->line, nit->pos,
			    "%d columns, %d cells", cols, i);
		else if (nit->head->next->child != NULL &&
		    nit->head->next->child->flags & NODE_LINE)
			mandoc_msg(MANDOCERR_IT_NOARG,
			    nit->line, nit->pos, "Bl -column It");
		break;
	default:
		abort();
	}
}

static void
post_bl_block(POST_ARGS)
{
	struct roff_node *n, *ni, *nc;

	post_prevpar(mdoc);

	n = mdoc->last;
	for (ni = n->body->child; ni != NULL; ni = ni->next) {
		if (ni->body == NULL)
			continue;
		nc = ni->body->last;
		while (nc != NULL) {
			switch (nc->tok) {
			case MDOC_Pp:
			case ROFF_br:
				break;
			default:
				nc = NULL;
				continue;
			}
			if (ni->next == NULL) {
				mandoc_msg(MANDOCERR_PAR_MOVE, nc->line,
				    nc->pos, "%s", roff_name[nc->tok]);
				roff_node_relink(mdoc, nc);
			} else if (n->norm->Bl.comp == 0 &&
			    n->norm->Bl.type != LIST_column) {
				mandoc_msg(MANDOCERR_PAR_SKIP,
				    nc->line, nc->pos,
				    "%s before It", roff_name[nc->tok]);
				roff_node_delete(mdoc, nc);
			} else
				break;
			nc = ni->body->last;
		}
	}
}

/*
 * If the argument of -offset or -width is a macro,
 * replace it with the associated default width.
 */
static void
rewrite_macro2len(struct roff_man *mdoc, char **arg)
{
	size_t		  width;
	enum roff_tok	  tok;

	if (*arg == NULL)
		return;
	else if ( ! strcmp(*arg, "Ds"))
		width = 6;
	else if ((tok = roffhash_find(mdoc->mdocmac, *arg, 0)) == TOKEN_NONE)
		return;
	else
		width = macro2len(tok);

	free(*arg);
	mandoc_asprintf(arg, "%zun", width);
}

static void
post_bl_head(POST_ARGS)
{
	struct roff_node *nbl, *nh, *nch, *nnext;
	struct mdoc_argv *argv;
	int		  i, j;

	post_bl_norm(mdoc);

	nh = mdoc->last;
	if (nh->norm->Bl.type != LIST_column) {
		if ((nch = nh->child) == NULL)
			return;
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    nch->line, nch->pos, "Bl ... %s", nch->string);
		while (nch != NULL) {
			roff_node_delete(mdoc, nch);
			nch = nh->child;
		}
		return;
	}

	/*
	 * Append old-style lists, where the column width specifiers
	 * trail as macro parameters, to the new-style ("normal-form")
	 * lists where they're argument values following -column.
	 */

	if (nh->child == NULL)
		return;

	nbl = nh->parent;
	for (j = 0; j < (int)nbl->args->argc; j++)
		if (nbl->args->argv[j].arg == MDOC_Column)
			break;

	assert(j < (int)nbl->args->argc);

	/*
	 * Accommodate for new-style groff column syntax.  Shuffle the
	 * child nodes, all of which must be TEXT, as arguments for the
	 * column field.  Then, delete the head children.
	 */

	argv = nbl->args->argv + j;
	i = argv->sz;
	for (nch = nh->child; nch != NULL; nch = nch->next)
		argv->sz++;
	argv->value = mandoc_reallocarray(argv->value,
	    argv->sz, sizeof(char *));

	nh->norm->Bl.ncols = argv->sz;
	nh->norm->Bl.cols = (void *)argv->value;

	for (nch = nh->child; nch != NULL; nch = nnext) {
		argv->value[i++] = nch->string;
		nch->string = NULL;
		nnext = nch->next;
		roff_node_delete(NULL, nch);
	}
	nh->child = NULL;
}

static void
post_bl(POST_ARGS)
{
	struct roff_node	*nbody;           /* of the Bl */
	struct roff_node	*nchild, *nnext;  /* of the Bl body */
	const char		*prev_Er;
	int			 order;

	nbody = mdoc->last;
	switch (nbody->type) {
	case ROFFT_BLOCK:
		post_bl_block(mdoc);
		return;
	case ROFFT_HEAD:
		post_bl_head(mdoc);
		return;
	case ROFFT_BODY:
		break;
	default:
		return;
	}
	if (nbody->end != ENDBODY_NOT)
		return;

	/*
	 * Up to the first item, move nodes before the list,
	 * but leave transparent nodes where they are
	 * if they precede an item.
	 * The next non-transparent node is kept in nchild.
	 * It only needs to be updated after a non-transparent
	 * node was moved out, and at the very beginning
	 * when no node at all was moved yet.
	 */

	nchild = mdoc->last;
	for (;;) {
		if (nchild == mdoc->last)
			nchild = roff_node_child(nbody);
		if (nchild == NULL) {
			mdoc->last = nbody;
			mandoc_msg(MANDOCERR_BLK_EMPTY,
			    nbody->line, nbody->pos, "Bl");
			return;
		}
		if (nchild->tok == MDOC_It) {
			mdoc->last = nbody;
			break;
		}
		mandoc_msg(MANDOCERR_BL_MOVE, nbody->child->line,
		    nbody->child->pos, "%s", roff_name[nbody->child->tok]);
		if (nbody->parent->prev == NULL) {
			mdoc->last = nbody->parent->parent;
			mdoc->next = ROFF_NEXT_CHILD;
		} else {
			mdoc->last = nbody->parent->prev;
			mdoc->next = ROFF_NEXT_SIBLING;
		}
		roff_node_relink(mdoc, nbody->child);
	}

	/*
	 * We have reached the first item,
	 * so moving nodes out is no longer possible.
	 * But in .Bl -column, the first rows may be implicit,
	 * that is, they may not start with .It macros.
	 * Such rows may be followed by nodes generated on the
	 * roff level, for example .TS.
	 * Wrap such roff nodes into an implicit row.
	 */

	while (nchild != NULL) {
		if (nchild->tok == MDOC_It) {
			nchild = roff_node_next(nchild);
			continue;
		}
		nnext = nchild->next;
		mdoc->last = nchild->prev;
		mdoc->next = ROFF_NEXT_SIBLING;
		roff_block_alloc(mdoc, nchild->line, nchild->pos, MDOC_It);
		roff_head_alloc(mdoc, nchild->line, nchild->pos, MDOC_It);
		mdoc->next = ROFF_NEXT_SIBLING;
		roff_body_alloc(mdoc, nchild->line, nchild->pos, MDOC_It);
		while (nchild->tok != MDOC_It) {
			roff_node_relink(mdoc, nchild);
			if (nnext == NULL)
				break;
			nchild = nnext;
			nnext = nchild->next;
			mdoc->next = ROFF_NEXT_SIBLING;
		}
		mdoc->last = nbody;
	}

	if (mdoc->meta.os_e != MANDOC_OS_NETBSD)
		return;

	prev_Er = NULL;
	for (nchild = nbody->child; nchild != NULL; nchild = nchild->next) {
		if (nchild->tok != MDOC_It)
			continue;
		if ((nnext = nchild->head->child) == NULL)
			continue;
		if (nnext->type == ROFFT_BLOCK)
			nnext = nnext->body->child;
		if (nnext == NULL || nnext->tok != MDOC_Er)
			continue;
		nnext = nnext->child;
		if (prev_Er != NULL) {
			order = strcmp(prev_Er, nnext->string);
			if (order > 0)
				mandoc_msg(MANDOCERR_ER_ORDER,
				    nnext->line, nnext->pos,
				    "Er %s %s (NetBSD)",
				    prev_Er, nnext->string);
			else if (order == 0)
				mandoc_msg(MANDOCERR_ER_REP,
				    nnext->line, nnext->pos,
				    "Er %s (NetBSD)", prev_Er);
		}
		prev_Er = nnext->string;
	}
}

static void
post_bk(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;

	if (n->type == ROFFT_BLOCK && n->body->child == NULL) {
		mandoc_msg(MANDOCERR_BLK_EMPTY, n->line, n->pos, "Bk");
		roff_node_delete(mdoc, n);
	}
}

static void
post_sm(POST_ARGS)
{
	struct roff_node	*nch;

	nch = mdoc->last->child;

	if (nch == NULL) {
		mdoc->flags ^= MDOC_SMOFF;
		return;
	}

	assert(nch->type == ROFFT_TEXT);

	if ( ! strcmp(nch->string, "on")) {
		mdoc->flags &= ~MDOC_SMOFF;
		return;
	}
	if ( ! strcmp(nch->string, "off")) {
		mdoc->flags |= MDOC_SMOFF;
		return;
	}

	mandoc_msg(MANDOCERR_SM_BAD, nch->line, nch->pos,
	    "%s %s", roff_name[mdoc->last->tok], nch->string);
	roff_node_relink(mdoc, nch);
	return;
}

static void
post_root(POST_ARGS)
{
	struct roff_node *n;

	/* Add missing prologue data. */

	if (mdoc->meta.date == NULL)
		mdoc->meta.date = mandoc_normdate(NULL, NULL);

	if (mdoc->meta.title == NULL) {
		mandoc_msg(MANDOCERR_DT_NOTITLE, 0, 0, "EOF");
		mdoc->meta.title = mandoc_strdup("UNTITLED");
	}

	if (mdoc->meta.vol == NULL)
		mdoc->meta.vol = mandoc_strdup("LOCAL");

	if (mdoc->meta.os == NULL) {
		mandoc_msg(MANDOCERR_OS_MISSING, 0, 0, NULL);
		mdoc->meta.os = mandoc_strdup("");
	} else if (mdoc->meta.os_e &&
	    (mdoc->meta.rcsids & (1 << mdoc->meta.os_e)) == 0)
		mandoc_msg(MANDOCERR_RCS_MISSING, 0, 0,
		    mdoc->meta.os_e == MANDOC_OS_OPENBSD ?
		    "(OpenBSD)" : "(NetBSD)");

	if (mdoc->meta.arch != NULL &&
	    arch_valid(mdoc->meta.arch, mdoc->meta.os_e) == 0) {
		n = mdoc->meta.first->child;
		while (n->tok != MDOC_Dt ||
		    n->child == NULL ||
		    n->child->next == NULL ||
		    n->child->next->next == NULL)
			n = n->next;
		n = n->child->next->next;
		mandoc_msg(MANDOCERR_ARCH_BAD, n->line, n->pos,
		    "Dt ... %s %s", mdoc->meta.arch,
		    mdoc->meta.os_e == MANDOC_OS_OPENBSD ?
		    "(OpenBSD)" : "(NetBSD)");
	}

	/* Check that we begin with a proper `Sh'. */

	n = mdoc->meta.first->child;
	while (n != NULL &&
	    (n->type == ROFFT_COMMENT ||
	     (n->tok >= MDOC_Dd &&
	      mdoc_macro(n->tok)->flags & MDOC_PROLOGUE)))
		n = n->next;

	if (n == NULL)
		mandoc_msg(MANDOCERR_DOC_EMPTY, 0, 0, NULL);
	else if (n->tok != MDOC_Sh)
		mandoc_msg(MANDOCERR_SEC_BEFORE, n->line, n->pos,
		    "%s", roff_name[n->tok]);
}

static void
post_rs(POST_ARGS)
{
	struct roff_node *np, *nch, *next, *prev;
	int		  i, j;

	np = mdoc->last;

	if (np->type != ROFFT_BODY)
		return;

	if (np->child == NULL) {
		mandoc_msg(MANDOCERR_RS_EMPTY, np->line, np->pos, "Rs");
		return;
	}

	/*
	 * The full `Rs' block needs special handling to order the
	 * sub-elements according to `rsord'.  Pick through each element
	 * and correctly order it.  This is an insertion sort.
	 */

	next = NULL;
	for (nch = np->child->next; nch != NULL; nch = next) {
		/* Determine order number of this child. */
		for (i = 0; i < RSORD_MAX; i++)
			if (rsord[i] == nch->tok)
				break;

		if (i == RSORD_MAX) {
			mandoc_msg(MANDOCERR_RS_BAD, nch->line, nch->pos,
			    "%s", roff_name[nch->tok]);
			i = -1;
		} else if (nch->tok == MDOC__J || nch->tok == MDOC__B)
			np->norm->Rs.quote_T++;

		/*
		 * Remove this child from the chain.  This somewhat
		 * repeats roff_node_unlink(), but since we're
		 * just re-ordering, there's no need for the
		 * full unlink process.
		 */

		if ((next = nch->next) != NULL)
			next->prev = nch->prev;

		if ((prev = nch->prev) != NULL)
			prev->next = nch->next;

		nch->prev = nch->next = NULL;

		/*
		 * Scan back until we reach a node that's
		 * to be ordered before this child.
		 */

		for ( ; prev ; prev = prev->prev) {
			/* Determine order of `prev'. */
			for (j = 0; j < RSORD_MAX; j++)
				if (rsord[j] == prev->tok)
					break;
			if (j == RSORD_MAX)
				j = -1;

			if (j <= i)
				break;
		}

		/*
		 * Set this child back into its correct place
		 * in front of the `prev' node.
		 */

		nch->prev = prev;

		if (prev == NULL) {
			np->child->prev = nch;
			nch->next = np->child;
			np->child = nch;
		} else {
			if (prev->next)
				prev->next->prev = nch;
			nch->next = prev->next;
			prev->next = nch;
		}
	}
}

/*
 * For some arguments of some macros,
 * convert all breakable hyphens into ASCII_HYPH.
 */
static void
post_hyph(POST_ARGS)
{
	struct roff_node	*n, *nch;
	char			*cp;

	n = mdoc->last;
	for (nch = n->child; nch != NULL; nch = nch->next) {
		if (nch->type != ROFFT_TEXT)
			continue;
		cp = nch->string;
		if (*cp == '\0')
			continue;
		while (*(++cp) != '\0')
			if (*cp == '-' &&
			    isalpha((unsigned char)cp[-1]) &&
			    isalpha((unsigned char)cp[1])) {
				if (n->tag == NULL && n->flags & NODE_ID)
					n->tag = mandoc_strdup(nch->string);
				*cp = ASCII_HYPH;
			}
	}
}

static void
post_ns(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;
	if (n->flags & NODE_LINE ||
	    (n->next != NULL && n->next->flags & NODE_DELIMC))
		mandoc_msg(MANDOCERR_NS_SKIP, n->line, n->pos, NULL);
}

static void
post_sx(POST_ARGS)
{
	post_delim(mdoc);
	post_hyph(mdoc);
}

static void
post_sh(POST_ARGS)
{
	post_section(mdoc);

	switch (mdoc->last->type) {
	case ROFFT_HEAD:
		post_sh_head(mdoc);
		break;
	case ROFFT_BODY:
		switch (mdoc->lastsec)  {
		case SEC_NAME:
			post_sh_name(mdoc);
			break;
		case SEC_SEE_ALSO:
			post_sh_see_also(mdoc);
			break;
		case SEC_AUTHORS:
			post_sh_authors(mdoc);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

static void
post_sh_name(POST_ARGS)
{
	struct roff_node *n;
	int hasnm, hasnd;

	hasnm = hasnd = 0;

	for (n = mdoc->last->child; n != NULL; n = n->next) {
		switch (n->tok) {
		case MDOC_Nm:
			if (hasnm && n->child != NULL)
				mandoc_msg(MANDOCERR_NAMESEC_PUNCT,
				    n->line, n->pos,
				    "Nm %s", n->child->string);
			hasnm = 1;
			continue;
		case MDOC_Nd:
			hasnd = 1;
			if (n->next != NULL)
				mandoc_msg(MANDOCERR_NAMESEC_ND,
				    n->line, n->pos, NULL);
			break;
		case TOKEN_NONE:
			if (n->type == ROFFT_TEXT &&
			    n->string[0] == ',' && n->string[1] == '\0' &&
			    n->next != NULL && n->next->tok == MDOC_Nm) {
				n = n->next;
				continue;
			}
			/* FALLTHROUGH */
		default:
			mandoc_msg(MANDOCERR_NAMESEC_BAD,
			    n->line, n->pos, "%s", roff_name[n->tok]);
			continue;
		}
		break;
	}

	if ( ! hasnm)
		mandoc_msg(MANDOCERR_NAMESEC_NONM,
		    mdoc->last->line, mdoc->last->pos, NULL);
	if ( ! hasnd)
		mandoc_msg(MANDOCERR_NAMESEC_NOND,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

static void
post_sh_see_also(POST_ARGS)
{
	const struct roff_node	*n;
	const char		*name, *sec;
	const char		*lastname, *lastsec, *lastpunct;
	int			 cmp;

	n = mdoc->last->child;
	lastname = lastsec = lastpunct = NULL;
	while (n != NULL) {
		if (n->tok != MDOC_Xr ||
		    n->child == NULL ||
		    n->child->next == NULL)
			break;

		/* Process one .Xr node. */

		name = n->child->string;
		sec = n->child->next->string;
		if (lastsec != NULL) {
			if (lastpunct[0] != ',' || lastpunct[1] != '\0')
				mandoc_msg(MANDOCERR_XR_PUNCT, n->line,
				    n->pos, "%s before %s(%s)",
				    lastpunct, name, sec);
			cmp = strcmp(lastsec, sec);
			if (cmp > 0)
				mandoc_msg(MANDOCERR_XR_ORDER, n->line,
				    n->pos, "%s(%s) after %s(%s)",
				    name, sec, lastname, lastsec);
			else if (cmp == 0 &&
			    strcasecmp(lastname, name) > 0)
				mandoc_msg(MANDOCERR_XR_ORDER, n->line,
				    n->pos, "%s after %s", name, lastname);
		}
		lastname = name;
		lastsec = sec;

		/* Process the following node. */

		n = n->next;
		if (n == NULL)
			break;
		if (n->tok == MDOC_Xr) {
			lastpunct = "none";
			continue;
		}
		if (n->type != ROFFT_TEXT)
			break;
		for (name = n->string; *name != '\0'; name++)
			if (isalpha((const unsigned char)*name))
				return;
		lastpunct = n->string;
		if (n->next == NULL || n->next->tok == MDOC_Rs)
			mandoc_msg(MANDOCERR_XR_PUNCT, n->line,
			    n->pos, "%s after %s(%s)",
			    lastpunct, lastname, lastsec);
		n = n->next;
	}
}

static int
child_an(const struct roff_node *n)
{

	for (n = n->child; n != NULL; n = n->next)
		if ((n->tok == MDOC_An && n->child != NULL) || child_an(n))
			return 1;
	return 0;
}

static void
post_sh_authors(POST_ARGS)
{

	if ( ! child_an(mdoc->last))
		mandoc_msg(MANDOCERR_AN_MISSING,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

/*
 * Return an upper bound for the string distance (allowing
 * transpositions).  Not a full Levenshtein implementation
 * because Levenshtein is quadratic in the string length
 * and this function is called for every standard name,
 * so the check for each custom name would be cubic.
 * The following crude heuristics is linear, resulting
 * in quadratic behaviour for checking one custom name,
 * which does not cause measurable slowdown.
 */
static int
similar(const char *s1, const char *s2)
{
	const int	maxdist = 3;
	int		dist = 0;

	while (s1[0] != '\0' && s2[0] != '\0') {
		if (s1[0] == s2[0]) {
			s1++;
			s2++;
			continue;
		}
		if (++dist > maxdist)
			return INT_MAX;
		if (s1[1] == s2[1]) {  /* replacement */
			s1++;
			s2++;
		} else if (s1[0] == s2[1] && s1[1] == s2[0]) {
			s1 += 2;	/* transposition */
			s2 += 2;
		} else if (s1[0] == s2[1])  /* insertion */
			s2++;
		else if (s1[1] == s2[0])  /* deletion */
			s1++;
		else
			return INT_MAX;
	}
	dist += strlen(s1) + strlen(s2);
	return dist > maxdist ? INT_MAX : dist;
}

static void
post_sh_head(POST_ARGS)
{
	struct roff_node	*nch;
	const char		*goodsec;
	const char *const	*testsec;
	int			 dist, mindist;
	enum roff_sec		 sec;

	/*
	 * Process a new section.  Sections are either "named" or
	 * "custom".  Custom sections are user-defined, while named ones
	 * follow a conventional order and may only appear in certain
	 * manual sections.
	 */

	sec = mdoc->last->sec;

	/* The NAME should be first. */

	if (sec != SEC_NAME && mdoc->lastnamed == SEC_NONE)
		mandoc_msg(MANDOCERR_NAMESEC_FIRST,
		    mdoc->last->line, mdoc->last->pos, "Sh %s",
		    sec != SEC_CUSTOM ? secnames[sec] :
		    (nch = mdoc->last->child) == NULL ? "" :
		    nch->type == ROFFT_TEXT ? nch->string :
		    roff_name[nch->tok]);

	/* The SYNOPSIS gets special attention in other areas. */

	if (sec == SEC_SYNOPSIS) {
		roff_setreg(mdoc->roff, "nS", 1, '=');
		mdoc->flags |= MDOC_SYNOPSIS;
	} else {
		roff_setreg(mdoc->roff, "nS", 0, '=');
		mdoc->flags &= ~MDOC_SYNOPSIS;
	}
	if (sec == SEC_DESCRIPTION)
		fn_prio = TAG_STRONG;

	/* Mark our last section. */

	mdoc->lastsec = sec;

	/* We don't care about custom sections after this. */

	if (sec == SEC_CUSTOM) {
		if ((nch = mdoc->last->child) == NULL ||
		    nch->type != ROFFT_TEXT || nch->next != NULL)
			return;
		goodsec = NULL;
		mindist = INT_MAX;
		for (testsec = secnames + 1; *testsec != NULL; testsec++) {
			dist = similar(nch->string, *testsec);
			if (dist < mindist) {
				goodsec = *testsec;
				mindist = dist;
			}
		}
		if (goodsec != NULL)
			mandoc_msg(MANDOCERR_SEC_TYPO, nch->line, nch->pos,
			    "Sh %s instead of %s", nch->string, goodsec);
		return;
	}

	/*
	 * Check whether our non-custom section is being repeated or is
	 * out of order.
	 */

	if (sec == mdoc->lastnamed)
		mandoc_msg(MANDOCERR_SEC_REP, mdoc->last->line,
		    mdoc->last->pos, "Sh %s", secnames[sec]);

	if (sec < mdoc->lastnamed)
		mandoc_msg(MANDOCERR_SEC_ORDER, mdoc->last->line,
		    mdoc->last->pos, "Sh %s", secnames[sec]);

	/* Mark the last named section. */

	mdoc->lastnamed = sec;

	/* Check particular section/manual conventions. */

	if (mdoc->meta.msec == NULL)
		return;

	goodsec = NULL;
	switch (sec) {
	case SEC_ERRORS:
		if (*mdoc->meta.msec == '4')
			break;
		goodsec = "2, 3, 4, 9";
		/* FALLTHROUGH */
	case SEC_RETURN_VALUES:
	case SEC_LIBRARY:
		if (*mdoc->meta.msec == '2')
			break;
		if (*mdoc->meta.msec == '3')
			break;
		if (NULL == goodsec)
			goodsec = "2, 3, 9";
		/* FALLTHROUGH */
	case SEC_CONTEXT:
		if (*mdoc->meta.msec == '9')
			break;
		if (NULL == goodsec)
			goodsec = "9";
		mandoc_msg(MANDOCERR_SEC_MSEC,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s for %s only", secnames[sec], goodsec);
		break;
	default:
		break;
	}
}

static void
post_xr(POST_ARGS)
{
	struct roff_node *n, *nch;

	n = mdoc->last;
	nch = n->child;
	if (nch->next == NULL) {
		mandoc_msg(MANDOCERR_XR_NOSEC,
		    n->line, n->pos, "Xr %s", nch->string);
	} else {
		assert(nch->next == n->last);
		if(mandoc_xr_add(nch->next->string, nch->string,
		    nch->line, nch->pos))
			mandoc_msg(MANDOCERR_XR_SELF,
			    nch->line, nch->pos, "Xr %s %s",
			    nch->string, nch->next->string);
	}
	post_delim_nb(mdoc);
}

static void
post_section(POST_ARGS)
{
	struct roff_node *n, *nch;
	char		 *cp, *tag;

	n = mdoc->last;
	switch (n->type) {
	case ROFFT_BLOCK:
		post_prevpar(mdoc);
		return;
	case ROFFT_HEAD:
		tag = NULL;
		deroff(&tag, n);
		if (tag != NULL) {
			for (cp = tag; *cp != '\0'; cp++)
				if (*cp == ' ')
					*cp = '_';
			if ((nch = n->child) != NULL &&
			    nch->type == ROFFT_TEXT &&
			    strcmp(nch->string, tag) == 0)
				tag_put(NULL, TAG_STRONG, n);
			else
				tag_put(tag, TAG_FALLBACK, n);
			free(tag);
		}
		post_delim(mdoc);
		post_hyph(mdoc);
		return;
	case ROFFT_BODY:
		break;
	default:
		return;
	}
	if ((nch = n->child) != NULL &&
	    (nch->tok == MDOC_Pp || nch->tok == ROFF_br ||
	     nch->tok == ROFF_sp)) {
		mandoc_msg(MANDOCERR_PAR_SKIP, nch->line, nch->pos,
		    "%s after %s", roff_name[nch->tok],
		    roff_name[n->tok]);
		roff_node_delete(mdoc, nch);
	}
	if ((nch = n->last) != NULL &&
	    (nch->tok == MDOC_Pp || nch->tok == ROFF_br)) {
		mandoc_msg(MANDOCERR_PAR_SKIP, nch->line, nch->pos,
		    "%s at the end of %s", roff_name[nch->tok],
		    roff_name[n->tok]);
		roff_node_delete(mdoc, nch);
	}
}

static void
post_prevpar(POST_ARGS)
{
	struct roff_node *n, *np;

	n = mdoc->last;
	if (n->type != ROFFT_ELEM && n->type != ROFFT_BLOCK)
		return;
	if ((np = roff_node_prev(n)) == NULL)
		return;

	/*
	 * Don't allow `Pp' prior to a paragraph-type
	 * block: `Pp' or non-compact `Bd' or `Bl'.
	 */

	if (np->tok != MDOC_Pp && np->tok != ROFF_br)
		return;
	if (n->tok == MDOC_Bl && n->norm->Bl.comp)
		return;
	if (n->tok == MDOC_Bd && n->norm->Bd.comp)
		return;
	if (n->tok == MDOC_It && n->parent->norm->Bl.comp)
		return;

	mandoc_msg(MANDOCERR_PAR_SKIP, np->line, np->pos,
	    "%s before %s", roff_name[np->tok], roff_name[n->tok]);
	roff_node_delete(mdoc, np);
}

static void
post_par(POST_ARGS)
{
	struct roff_node *np;

	fn_prio = TAG_STRONG;
	post_prevpar(mdoc);

	np = mdoc->last;
	if (np->child != NULL)
		mandoc_msg(MANDOCERR_ARG_SKIP, np->line, np->pos,
		    "%s %s", roff_name[np->tok], np->child->string);
}

static void
post_dd(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	n->flags |= NODE_NOPRT;

	if (mdoc->meta.date != NULL) {
		mandoc_msg(MANDOCERR_PROLOG_REP, n->line, n->pos, "Dd");
		free(mdoc->meta.date);
	} else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, n->line, n->pos, "Dd");
	else if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER,
		    n->line, n->pos, "Dd after Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER,
		    n->line, n->pos, "Dd after Os");

	if (mdoc->quick && n != NULL)
		mdoc->meta.date = mandoc_strdup("");
	else
		mdoc->meta.date = mandoc_normdate(n->child, n);
}

static void
post_dt(POST_ARGS)
{
	struct roff_node *nn, *n;
	const char	 *cp;
	char		 *p;

	n = mdoc->last;
	n->flags |= NODE_NOPRT;

	if (mdoc->flags & MDOC_PBODY) {
		mandoc_msg(MANDOCERR_DT_LATE, n->line, n->pos, "Dt");
		return;
	}

	if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, n->line, n->pos, "Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER,
		    n->line, n->pos, "Dt after Os");

	free(mdoc->meta.title);
	free(mdoc->meta.msec);
	free(mdoc->meta.vol);
	free(mdoc->meta.arch);

	mdoc->meta.title = NULL;
	mdoc->meta.msec = NULL;
	mdoc->meta.vol = NULL;
	mdoc->meta.arch = NULL;

	/* Mandatory first argument: title. */

	nn = n->child;
	if (nn == NULL || *nn->string == '\0') {
		mandoc_msg(MANDOCERR_DT_NOTITLE, n->line, n->pos, "Dt");
		mdoc->meta.title = mandoc_strdup("UNTITLED");
	} else {
		mdoc->meta.title = mandoc_strdup(nn->string);

		/* Check that all characters are uppercase. */

		for (p = nn->string; *p != '\0'; p++)
			if (islower((unsigned char)*p)) {
				mandoc_msg(MANDOCERR_TITLE_CASE, nn->line,
				    nn->pos + (int)(p - nn->string),
				    "Dt %s", nn->string);
				break;
			}
	}

	/* Mandatory second argument: section. */

	if (nn != NULL)
		nn = nn->next;

	if (nn == NULL) {
		mandoc_msg(MANDOCERR_MSEC_MISSING, n->line, n->pos,
		    "Dt %s", mdoc->meta.title);
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		return;  /* msec and arch remain NULL. */
	}

	mdoc->meta.msec = mandoc_strdup(nn->string);

	/* Infer volume title from section number. */

	cp = mandoc_a2msec(nn->string);
	if (cp == NULL) {
		mandoc_msg(MANDOCERR_MSEC_BAD,
		    nn->line, nn->pos, "Dt ... %s", nn->string);
		mdoc->meta.vol = mandoc_strdup(nn->string);
	} else {
		mdoc->meta.vol = mandoc_strdup(cp);
		if (mdoc->filesec != '\0' &&
		    mdoc->filesec != *nn->string &&
		    *nn->string >= '1' && *nn->string <= '9')
			mandoc_msg(MANDOCERR_MSEC_FILE, nn->line, nn->pos,
			    "*.%c vs Dt ... %c", mdoc->filesec, *nn->string);
	}

	/* Optional third argument: architecture. */

	if ((nn = nn->next) == NULL)
		return;

	for (p = nn->string; *p != '\0'; p++)
		*p = tolower((unsigned char)*p);
	mdoc->meta.arch = mandoc_strdup(nn->string);

	/* Ignore fourth and later arguments. */

	if ((nn = nn->next) != NULL)
		mandoc_msg(MANDOCERR_ARG_EXCESS,
		    nn->line, nn->pos, "Dt ... %s", nn->string);
}

static void
post_bx(POST_ARGS)
{
	struct roff_node	*n, *nch;
	const char		*macro;

	post_delim_nb(mdoc);

	n = mdoc->last;
	nch = n->child;

	if (nch != NULL) {
		macro = !strcmp(nch->string, "Open") ? "Ox" :
		    !strcmp(nch->string, "Net") ? "Nx" :
		    !strcmp(nch->string, "Free") ? "Fx" :
		    !strcmp(nch->string, "DragonFly") ? "Dx" : NULL;
		if (macro != NULL)
			mandoc_msg(MANDOCERR_BX,
			    n->line, n->pos, "%s", macro);
		mdoc->last = nch;
		nch = nch->next;
		mdoc->next = ROFF_NEXT_SIBLING;
		roff_elem_alloc(mdoc, n->line, n->pos, MDOC_Ns);
		mdoc->last->flags |= NODE_NOSRC;
		mdoc->next = ROFF_NEXT_SIBLING;
	} else
		mdoc->next = ROFF_NEXT_CHILD;
	roff_word_alloc(mdoc, n->line, n->pos, "BSD");
	mdoc->last->flags |= NODE_NOSRC;

	if (nch == NULL) {
		mdoc->last = n;
		return;
	}

	roff_elem_alloc(mdoc, n->line, n->pos, MDOC_Ns);
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->next = ROFF_NEXT_SIBLING;
	roff_word_alloc(mdoc, n->line, n->pos, "-");
	mdoc->last->flags |= NODE_NOSRC;
	roff_elem_alloc(mdoc, n->line, n->pos, MDOC_Ns);
	mdoc->last->flags |= NODE_NOSRC;
	mdoc->last = n;

	/*
	 * Make `Bx's second argument always start with an uppercase
	 * letter.  Groff checks if it's an "accepted" term, but we just
	 * uppercase blindly.
	 */

	*nch->string = (char)toupper((unsigned char)*nch->string);
}

static void
post_os(POST_ARGS)
{
#ifndef OSNAME
	struct utsname	  utsname;
	static char	 *defbuf;
#endif
	struct roff_node *n;

	n = mdoc->last;
	n->flags |= NODE_NOPRT;

	if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, n->line, n->pos, "Os");
	else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, n->line, n->pos, "Os");

	post_delim(mdoc);

	/*
	 * Set the operating system by way of the `Os' macro.
	 * The order of precedence is:
	 * 1. the argument of the `Os' macro, unless empty
	 * 2. the -Ios=foo command line argument, if provided
	 * 3. -DOSNAME="\"foo\"", if provided during compilation
	 * 4. "sysname release" from uname(3)
	 */

	free(mdoc->meta.os);
	mdoc->meta.os = NULL;
	deroff(&mdoc->meta.os, n);
	if (mdoc->meta.os)
		goto out;

	if (mdoc->os_s != NULL) {
		mdoc->meta.os = mandoc_strdup(mdoc->os_s);
		goto out;
	}

#ifdef OSNAME
	mdoc->meta.os = mandoc_strdup(OSNAME);
#else /*!OSNAME */
	if (defbuf == NULL) {
		if (uname(&utsname) == -1) {
			mandoc_msg(MANDOCERR_OS_UNAME, n->line, n->pos, "Os");
			defbuf = mandoc_strdup("UNKNOWN");
		} else
			mandoc_asprintf(&defbuf, "%s %s",
			    utsname.sysname, utsname.release);
	}
	mdoc->meta.os = mandoc_strdup(defbuf);
#endif /*!OSNAME*/

out:
	if (mdoc->meta.os_e == MANDOC_OS_OTHER) {
		if (strstr(mdoc->meta.os, "OpenBSD") != NULL)
			mdoc->meta.os_e = MANDOC_OS_OPENBSD;
		else if (strstr(mdoc->meta.os, "NetBSD") != NULL)
			mdoc->meta.os_e = MANDOC_OS_NETBSD;
	}

	/*
	 * This is the earliest point where we can check
	 * Mdocdate conventions because we don't know
	 * the operating system earlier.
	 */

	if (n->child != NULL)
		mandoc_msg(MANDOCERR_OS_ARG, n->child->line, n->child->pos,
		    "Os %s (%s)", n->child->string,
		    mdoc->meta.os_e == MANDOC_OS_OPENBSD ?
		    "OpenBSD" : "NetBSD");

	while (n->tok != MDOC_Dd)
		if ((n = n->prev) == NULL)
			return;
	if ((n = n->child) == NULL)
		return;
	if (strncmp(n->string, "$" "Mdocdate", 9)) {
		if (mdoc->meta.os_e == MANDOC_OS_OPENBSD)
			mandoc_msg(MANDOCERR_MDOCDATE_MISSING, n->line,
			    n->pos, "Dd %s (OpenBSD)", n->string);
	} else {
		if (mdoc->meta.os_e == MANDOC_OS_NETBSD)
			mandoc_msg(MANDOCERR_MDOCDATE, n->line,
			    n->pos, "Dd %s (NetBSD)", n->string);
	}
}

enum roff_sec
mdoc_a2sec(const char *p)
{
	int		 i;

	for (i = 0; i < (int)SEC__MAX; i++)
		if (secnames[i] && 0 == strcmp(p, secnames[i]))
			return (enum roff_sec)i;

	return SEC_CUSTOM;
}

static size_t
macro2len(enum roff_tok macro)
{

	switch (macro) {
	case MDOC_Ad:
		return 12;
	case MDOC_Ao:
		return 12;
	case MDOC_An:
		return 12;
	case MDOC_Aq:
		return 12;
	case MDOC_Ar:
		return 12;
	case MDOC_Bo:
		return 12;
	case MDOC_Bq:
		return 12;
	case MDOC_Cd:
		return 12;
	case MDOC_Cm:
		return 10;
	case MDOC_Do:
		return 10;
	case MDOC_Dq:
		return 12;
	case MDOC_Dv:
		return 12;
	case MDOC_Eo:
		return 12;
	case MDOC_Em:
		return 10;
	case MDOC_Er:
		return 17;
	case MDOC_Ev:
		return 15;
	case MDOC_Fa:
		return 12;
	case MDOC_Fl:
		return 10;
	case MDOC_Fo:
		return 16;
	case MDOC_Fn:
		return 16;
	case MDOC_Ic:
		return 10;
	case MDOC_Li:
		return 16;
	case MDOC_Ms:
		return 6;
	case MDOC_Nm:
		return 10;
	case MDOC_No:
		return 12;
	case MDOC_Oo:
		return 10;
	case MDOC_Op:
		return 14;
	case MDOC_Pa:
		return 32;
	case MDOC_Pf:
		return 12;
	case MDOC_Po:
		return 12;
	case MDOC_Pq:
		return 12;
	case MDOC_Ql:
		return 16;
	case MDOC_Qo:
		return 12;
	case MDOC_So:
		return 12;
	case MDOC_Sq:
		return 12;
	case MDOC_Sy:
		return 6;
	case MDOC_Sx:
		return 16;
	case MDOC_Tn:
		return 10;
	case MDOC_Va:
		return 12;
	case MDOC_Vt:
		return 12;
	case MDOC_Xr:
		return 10;
	default:
		break;
	};
	return 0;
}
