/*	$Id: mdoc_validate.c,v 1.283 2015/02/23 13:55:55 schwarze Exp $ */
/*
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2015 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2010 Joerg Sonnenberger <joerg@netbsd.org>
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

#include "mdoc.h"
#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmdoc.h"
#include "libmandoc.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */

#define	PRE_ARGS  struct mdoc *mdoc, struct mdoc_node *n
#define	POST_ARGS struct mdoc *mdoc

enum	check_ineq {
	CHECK_LT,
	CHECK_GT,
	CHECK_EQ
};

typedef	void	(*v_pre)(PRE_ARGS);
typedef	void	(*v_post)(POST_ARGS);

struct	valids {
	v_pre	 pre;
	v_post	 post;
};

static	void	 check_text(struct mdoc *, int, int, char *);
static	void	 check_argv(struct mdoc *,
			struct mdoc_node *, struct mdoc_argv *);
static	void	 check_args(struct mdoc *, struct mdoc_node *);
static	int	 child_an(const struct mdoc_node *);
static	enum mdoc_sec	a2sec(const char *);
static	size_t		macro2len(enum mdoct);
static	void	 rewrite_macro2len(char **);

static	void	 post_an(POST_ARGS);
static	void	 post_at(POST_ARGS);
static	void	 post_bf(POST_ARGS);
static	void	 post_bk(POST_ARGS);
static	void	 post_bl(POST_ARGS);
static	void	 post_bl_block(POST_ARGS);
static	void	 post_bl_block_tag(POST_ARGS);
static	void	 post_bl_head(POST_ARGS);
static	void	 post_bx(POST_ARGS);
static	void	 post_d1(POST_ARGS);
static	void	 post_defaults(POST_ARGS);
static	void	 post_dd(POST_ARGS);
static	void	 post_dt(POST_ARGS);
static	void	 post_en(POST_ARGS);
static	void	 post_es(POST_ARGS);
static	void	 post_eoln(POST_ARGS);
static	void	 post_ex(POST_ARGS);
static	void	 post_fa(POST_ARGS);
static	void	 post_fn(POST_ARGS);
static	void	 post_fname(POST_ARGS);
static	void	 post_fo(POST_ARGS);
static	void	 post_hyph(POST_ARGS);
static	void	 post_ignpar(POST_ARGS);
static	void	 post_it(POST_ARGS);
static	void	 post_lb(POST_ARGS);
static	void	 post_literal(POST_ARGS);
static	void	 post_nd(POST_ARGS);
static	void	 post_nm(POST_ARGS);
static	void	 post_ns(POST_ARGS);
static	void	 post_os(POST_ARGS);
static	void	 post_par(POST_ARGS);
static	void	 post_root(POST_ARGS);
static	void	 post_rs(POST_ARGS);
static	void	 post_sh(POST_ARGS);
static	void	 post_sh_head(POST_ARGS);
static	void	 post_sh_name(POST_ARGS);
static	void	 post_sh_see_also(POST_ARGS);
static	void	 post_sh_authors(POST_ARGS);
static	void	 post_sm(POST_ARGS);
static	void	 post_st(POST_ARGS);
static	void	 post_vt(POST_ARGS);

static	void	 pre_an(PRE_ARGS);
static	void	 pre_bd(PRE_ARGS);
static	void	 pre_bl(PRE_ARGS);
static	void	 pre_dd(PRE_ARGS);
static	void	 pre_display(PRE_ARGS);
static	void	 pre_dt(PRE_ARGS);
static	void	 pre_literal(PRE_ARGS);
static	void	 pre_obsolete(PRE_ARGS);
static	void	 pre_os(PRE_ARGS);
static	void	 pre_par(PRE_ARGS);
static	void	 pre_std(PRE_ARGS);

static	const struct valids mdoc_valids[MDOC_MAX] = {
	{ NULL, NULL },				/* Ap */
	{ pre_dd, post_dd },			/* Dd */
	{ pre_dt, post_dt },			/* Dt */
	{ pre_os, post_os },			/* Os */
	{ NULL, post_sh },			/* Sh */
	{ NULL, post_ignpar },			/* Ss */
	{ pre_par, post_par },			/* Pp */
	{ pre_display, post_d1 },		/* D1 */
	{ pre_literal, post_literal },		/* Dl */
	{ pre_bd, post_literal },		/* Bd */
	{ NULL, NULL },				/* Ed */
	{ pre_bl, post_bl },			/* Bl */
	{ NULL, NULL },				/* El */
	{ pre_par, post_it },			/* It */
	{ NULL, NULL },				/* Ad */
	{ pre_an, post_an },			/* An */
	{ NULL, post_defaults },		/* Ar */
	{ NULL, NULL },				/* Cd */
	{ NULL, NULL },				/* Cm */
	{ NULL, NULL },				/* Dv */
	{ NULL, NULL },				/* Er */
	{ NULL, NULL },				/* Ev */
	{ pre_std, post_ex },			/* Ex */
	{ NULL, post_fa },			/* Fa */
	{ NULL, NULL },				/* Fd */
	{ NULL, NULL },				/* Fl */
	{ NULL, post_fn },			/* Fn */
	{ NULL, NULL },				/* Ft */
	{ NULL, NULL },				/* Ic */
	{ NULL, NULL },				/* In */
	{ NULL, post_defaults },		/* Li */
	{ NULL, post_nd },			/* Nd */
	{ NULL, post_nm },			/* Nm */
	{ NULL, NULL },				/* Op */
	{ pre_obsolete, NULL },			/* Ot */
	{ NULL, post_defaults },		/* Pa */
	{ pre_std, NULL },			/* Rv */
	{ NULL, post_st },			/* St */
	{ NULL, NULL },				/* Va */
	{ NULL, post_vt },			/* Vt */
	{ NULL, NULL },				/* Xr */
	{ NULL, NULL },				/* %A */
	{ NULL, post_hyph },			/* %B */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, NULL },				/* %D */
	{ NULL, NULL },				/* %I */
	{ NULL, NULL },				/* %J */
	{ NULL, post_hyph },			/* %N */
	{ NULL, post_hyph },			/* %O */
	{ NULL, NULL },				/* %P */
	{ NULL, post_hyph },			/* %R */
	{ NULL, post_hyph },			/* %T */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, NULL },				/* %V */
	{ NULL, NULL },				/* Ac */
	{ NULL, NULL },				/* Ao */
	{ NULL, NULL },				/* Aq */
	{ NULL, post_at },			/* At */
	{ NULL, NULL },				/* Bc */
	{ NULL, post_bf },			/* Bf */
	{ NULL, NULL },				/* Bo */
	{ NULL, NULL },				/* Bq */
	{ NULL, NULL },				/* Bsx */
	{ NULL, post_bx },			/* Bx */
	{ pre_obsolete, NULL },			/* Db */
	{ NULL, NULL },				/* Dc */
	{ NULL, NULL },				/* Do */
	{ NULL, NULL },				/* Dq */
	{ NULL, NULL },				/* Ec */
	{ NULL, NULL },				/* Ef */
	{ NULL, NULL },				/* Em */
	{ NULL, NULL },				/* Eo */
	{ NULL, NULL },				/* Fx */
	{ NULL, NULL },				/* Ms */
	{ NULL, NULL },				/* No */
	{ NULL, post_ns },			/* Ns */
	{ NULL, NULL },				/* Nx */
	{ NULL, NULL },				/* Ox */
	{ NULL, NULL },				/* Pc */
	{ NULL, NULL },				/* Pf */
	{ NULL, NULL },				/* Po */
	{ NULL, NULL },				/* Pq */
	{ NULL, NULL },				/* Qc */
	{ NULL, NULL },				/* Ql */
	{ NULL, NULL },				/* Qo */
	{ NULL, NULL },				/* Qq */
	{ NULL, NULL },				/* Re */
	{ NULL, post_rs },			/* Rs */
	{ NULL, NULL },				/* Sc */
	{ NULL, NULL },				/* So */
	{ NULL, NULL },				/* Sq */
	{ NULL, post_sm },			/* Sm */
	{ NULL, post_hyph },			/* Sx */
	{ NULL, NULL },				/* Sy */
	{ NULL, NULL },				/* Tn */
	{ NULL, NULL },				/* Ux */
	{ NULL, NULL },				/* Xc */
	{ NULL, NULL },				/* Xo */
	{ NULL, post_fo },			/* Fo */
	{ NULL, NULL },				/* Fc */
	{ NULL, NULL },				/* Oo */
	{ NULL, NULL },				/* Oc */
	{ NULL, post_bk },			/* Bk */
	{ NULL, NULL },				/* Ek */
	{ NULL, post_eoln },			/* Bt */
	{ NULL, NULL },				/* Hf */
	{ pre_obsolete, NULL },			/* Fr */
	{ NULL, post_eoln },			/* Ud */
	{ NULL, post_lb },			/* Lb */
	{ pre_par, post_par },			/* Lp */
	{ NULL, NULL },				/* Lk */
	{ NULL, post_defaults },		/* Mt */
	{ NULL, NULL },				/* Brq */
	{ NULL, NULL },				/* Bro */
	{ NULL, NULL },				/* Brc */
	{ NULL, NULL },				/* %C */
	{ pre_obsolete, post_es },		/* Es */
	{ pre_obsolete, post_en },		/* En */
	{ NULL, NULL },				/* Dx */
	{ NULL, NULL },				/* %Q */
	{ NULL, post_par },			/* br */
	{ NULL, post_par },			/* sp */
	{ NULL, NULL },				/* %U */
	{ NULL, NULL },				/* Ta */
	{ NULL, NULL },				/* ll */
};

#define	RSORD_MAX 14 /* Number of `Rs' blocks. */

static	const enum mdoct rsord[RSORD_MAX] = {
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


void
mdoc_valid_pre(struct mdoc *mdoc, struct mdoc_node *n)
{
	v_pre	 p;

	switch (n->type) {
	case MDOC_TEXT:
		if (n->sec != SEC_SYNOPSIS || n->parent->tok != MDOC_Fd)
			check_text(mdoc, n->line, n->pos, n->string);
		/* FALLTHROUGH */
	case MDOC_TBL:
		/* FALLTHROUGH */
	case MDOC_EQN:
		/* FALLTHROUGH */
	case MDOC_ROOT:
		return;
	default:
		break;
	}

	check_args(mdoc, n);
	p = mdoc_valids[n->tok].pre;
	if (*p)
		(*p)(mdoc, n);
}

void
mdoc_valid_post(struct mdoc *mdoc)
{
	struct mdoc_node *n;
	v_post p;

	n = mdoc->last;
	if (n->flags & MDOC_VALID)
		return;
	n->flags |= MDOC_VALID | MDOC_ENDED;

	switch (n->type) {
	case MDOC_TEXT:
		/* FALLTHROUGH */
	case MDOC_EQN:
		/* FALLTHROUGH */
	case MDOC_TBL:
		break;
	case MDOC_ROOT:
		post_root(mdoc);
		break;
	default:

		/*
		 * Closing delimiters are not special at the
		 * beginning of a block, opening delimiters
		 * are not special at the end.
		 */

		if (n->child != NULL)
			n->child->flags &= ~MDOC_DELIMC;
		if (n->last != NULL)
			n->last->flags &= ~MDOC_DELIMO;

		/* Call the macro's postprocessor. */

		p = mdoc_valids[n->tok].post;
		if (*p)
			(*p)(mdoc);
		break;
	}
}

static void
check_args(struct mdoc *mdoc, struct mdoc_node *n)
{
	int		 i;

	if (NULL == n->args)
		return;

	assert(n->args->argc);
	for (i = 0; i < (int)n->args->argc; i++)
		check_argv(mdoc, n, &n->args->argv[i]);
}

static void
check_argv(struct mdoc *mdoc, struct mdoc_node *n, struct mdoc_argv *v)
{
	int		 i;

	for (i = 0; i < (int)v->sz; i++)
		check_text(mdoc, v->line, v->pos, v->value[i]);
}

static void
check_text(struct mdoc *mdoc, int ln, int pos, char *p)
{
	char		*cp;

	if (MDOC_LITERAL & mdoc->flags)
		return;

	for (cp = p; NULL != (p = strchr(p, '\t')); p++)
		mandoc_msg(MANDOCERR_FI_TAB, mdoc->parse,
		    ln, pos + (int)(p - cp), NULL);
}

static void
pre_display(PRE_ARGS)
{
	struct mdoc_node *node;

	if (MDOC_BLOCK != n->type)
		return;

	for (node = mdoc->last->parent; node; node = node->parent)
		if (MDOC_BLOCK == node->type)
			if (MDOC_Bd == node->tok)
				break;

	if (node)
		mandoc_vmsg(MANDOCERR_BD_NEST,
		    mdoc->parse, n->line, n->pos,
		    "%s in Bd", mdoc_macronames[n->tok]);
}

static void
pre_bl(PRE_ARGS)
{
	struct mdoc_argv *argv, *wa;
	int		  i;
	enum mdocargt	  mdoclt;
	enum mdoc_list	  lt;

	if (n->type != MDOC_BLOCK)
		return;

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
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -compact");
			n->norm->Bl.comp = 1;
			break;
		case MDOC_Width:
			wa = argv;
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -width");
				n->norm->Bl.width = "0n";
				break;
			}
			if (NULL != n->norm->Bl.width)
				mandoc_vmsg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -width %s",
				    argv->value[0]);
			rewrite_macro2len(argv->value);
			n->norm->Bl.width = argv->value[0];
			break;
		case MDOC_Offset:
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -offset");
				break;
			}
			if (NULL != n->norm->Bl.offs)
				mandoc_vmsg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bl -offset %s",
				    argv->value[0]);
			rewrite_macro2len(argv->value);
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
			mandoc_vmsg(MANDOCERR_BL_REP,
			    mdoc->parse, n->line, n->pos,
			    "Bl -%s", mdoc_argnames[argv->arg]);
			continue;
		}

		/* The list type should come first. */

		if (n->norm->Bl.width ||
		    n->norm->Bl.offs ||
		    n->norm->Bl.comp)
			mandoc_vmsg(MANDOCERR_BL_LATETYPE,
			    mdoc->parse, n->line, n->pos, "Bl -%s",
			    mdoc_argnames[n->args->argv[0].arg]);

		n->norm->Bl.type = lt;
		if (LIST_column == lt) {
			n->norm->Bl.ncols = argv->sz;
			n->norm->Bl.cols = (void *)argv->value;
		}
	}

	/* Allow lists to default to LIST_item. */

	if (LIST__NONE == n->norm->Bl.type) {
		mandoc_msg(MANDOCERR_BL_NOTYPE, mdoc->parse,
		    n->line, n->pos, "Bl");
		n->norm->Bl.type = LIST_item;
	}

	/*
	 * Validate the width field.  Some list types don't need width
	 * types and should be warned about them.  Others should have it
	 * and must also be warned.  Yet others have a default and need
	 * no warning.
	 */

	switch (n->norm->Bl.type) {
	case LIST_tag:
		if (NULL == n->norm->Bl.width)
			mandoc_msg(MANDOCERR_BL_NOWIDTH, mdoc->parse,
			    n->line, n->pos, "Bl -tag");
		break;
	case LIST_column:
		/* FALLTHROUGH */
	case LIST_diag:
		/* FALLTHROUGH */
	case LIST_ohang:
		/* FALLTHROUGH */
	case LIST_inset:
		/* FALLTHROUGH */
	case LIST_item:
		if (n->norm->Bl.width)
			mandoc_vmsg(MANDOCERR_BL_SKIPW, mdoc->parse,
			    wa->line, wa->pos, "Bl -%s",
			    mdoc_argnames[mdoclt]);
		break;
	case LIST_bullet:
		/* FALLTHROUGH */
	case LIST_dash:
		/* FALLTHROUGH */
	case LIST_hyphen:
		if (NULL == n->norm->Bl.width)
			n->norm->Bl.width = "2n";
		break;
	case LIST_enum:
		if (NULL == n->norm->Bl.width)
			n->norm->Bl.width = "3n";
		break;
	default:
		break;
	}
	pre_par(mdoc, n);
}

static void
pre_bd(PRE_ARGS)
{
	struct mdoc_argv *argv;
	int		  i;
	enum mdoc_disp	  dt;

	pre_literal(mdoc, n);

	if (n->type != MDOC_BLOCK)
		return;

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
			mandoc_msg(MANDOCERR_BD_FILE, mdoc->parse,
			    n->line, n->pos, NULL);
			break;
		case MDOC_Offset:
			if (0 == argv->sz) {
				mandoc_msg(MANDOCERR_ARG_EMPTY,
				    mdoc->parse, argv->line,
				    argv->pos, "Bd -offset");
				break;
			}
			if (NULL != n->norm->Bd.offs)
				mandoc_vmsg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bd -offset %s",
				    argv->value[0]);
			rewrite_macro2len(argv->value);
			n->norm->Bd.offs = argv->value[0];
			break;
		case MDOC_Compact:
			if (n->norm->Bd.comp)
				mandoc_msg(MANDOCERR_ARG_REP,
				    mdoc->parse, argv->line,
				    argv->pos, "Bd -compact");
			n->norm->Bd.comp = 1;
			break;
		default:
			abort();
			/* NOTREACHED */
		}
		if (DISP__NONE == dt)
			continue;

		if (DISP__NONE == n->norm->Bd.type)
			n->norm->Bd.type = dt;
		else
			mandoc_vmsg(MANDOCERR_BD_REP,
			    mdoc->parse, n->line, n->pos,
			    "Bd -%s", mdoc_argnames[argv->arg]);
	}

	if (DISP__NONE == n->norm->Bd.type) {
		mandoc_msg(MANDOCERR_BD_NOTYPE, mdoc->parse,
		    n->line, n->pos, "Bd");
		n->norm->Bd.type = DISP_ragged;
	}
	pre_par(mdoc, n);
}

static void
pre_an(PRE_ARGS)
{
	struct mdoc_argv *argv;
	size_t	 i;

	if (n->args == NULL)
		return;

	for (i = 1; i < n->args->argc; i++) {
		argv = n->args->argv + i;
		mandoc_vmsg(MANDOCERR_AN_REP,
		    mdoc->parse, argv->line, argv->pos,
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
pre_std(PRE_ARGS)
{

	if (n->args && 1 == n->args->argc)
		if (MDOC_Std == n->args->argv[0].arg)
			return;

	mandoc_msg(MANDOCERR_ARG_STD, mdoc->parse,
	    n->line, n->pos, mdoc_macronames[n->tok]);
}

static void
pre_obsolete(PRE_ARGS)
{

	if (MDOC_ELEM == n->type || MDOC_BLOCK == n->type)
		mandoc_msg(MANDOCERR_MACRO_OBS, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
}

static void
pre_dt(PRE_ARGS)
{

	if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dt after Os");
}

static void
pre_os(PRE_ARGS)
{

	if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Os");
	else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, mdoc->parse,
		    n->line, n->pos, "Os");
}

static void
pre_dd(PRE_ARGS)
{

	if (mdoc->meta.date != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Dd");
	else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, mdoc->parse,
		    n->line, n->pos, "Dd");
	else if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dd after Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dd after Os");
}

static void
post_bf(POST_ARGS)
{
	struct mdoc_node *np, *nch;
	enum mdocargt	  arg;

	/*
	 * Unlike other data pointers, these are "housed" by the HEAD
	 * element, which contains the goods.
	 */

	np = mdoc->last;
	if (MDOC_HEAD != np->type)
		return;

	assert(MDOC_BLOCK == np->parent->type);
	assert(MDOC_Bf == np->parent->tok);

	/* Check the number of arguments. */

	nch = np->child;
	if (NULL == np->parent->args) {
		if (NULL == nch) {
			mandoc_msg(MANDOCERR_BF_NOFONT, mdoc->parse,
			    np->line, np->pos, "Bf");
			return;
		}
		nch = nch->next;
	}
	if (NULL != nch)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nch->line, nch->pos, "Bf ... %s", nch->string);

	/* Extract argument into data. */

	if (np->parent->args) {
		arg = np->parent->args->argv[0].arg;
		if (MDOC_Emphasis == arg)
			np->norm->Bf.font = FONT_Em;
		else if (MDOC_Literal == arg)
			np->norm->Bf.font = FONT_Li;
		else if (MDOC_Symbolic == arg)
			np->norm->Bf.font = FONT_Sy;
		else
			abort();
		return;
	}

	/* Extract parameter into data. */

	if (0 == strcmp(np->child->string, "Em"))
		np->norm->Bf.font = FONT_Em;
	else if (0 == strcmp(np->child->string, "Li"))
		np->norm->Bf.font = FONT_Li;
	else if (0 == strcmp(np->child->string, "Sy"))
		np->norm->Bf.font = FONT_Sy;
	else
		mandoc_vmsg(MANDOCERR_BF_BADFONT, mdoc->parse,
		    np->child->line, np->child->pos,
		    "Bf %s", np->child->string);
}

static void
post_lb(POST_ARGS)
{
	struct mdoc_node	*n;
	const char		*stdlibname;
	char			*libname;

	n = mdoc->last->child;
	assert(MDOC_TEXT == n->type);

	if (NULL == (stdlibname = mdoc_a2lib(n->string)))
		mandoc_asprintf(&libname,
		    "library \\(Lq%s\\(Rq", n->string);
	else
		libname = mandoc_strdup(stdlibname);

	free(n->string);
	n->string = libname;
}

static void
post_eoln(POST_ARGS)
{
	const struct mdoc_node *n;

	n = mdoc->last;
	if (n->child)
		mandoc_vmsg(MANDOCERR_ARG_SKIP,
		    mdoc->parse, n->line, n->pos,
		    "%s %s", mdoc_macronames[n->tok],
		    n->child->string);
}

static void
post_fname(POST_ARGS)
{
	const struct mdoc_node	*n;
	const char		*cp;
	size_t			 pos;

	n = mdoc->last->child;
	pos = strcspn(n->string, "()");
	cp = n->string + pos;
	if ( ! (cp[0] == '\0' || (cp[0] == '(' && cp[1] == '*')))
		mandoc_msg(MANDOCERR_FN_PAREN, mdoc->parse,
		    n->line, n->pos + pos, n->string);
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
	const struct mdoc_node	*n;

	n = mdoc->last;

	if (n->type != MDOC_HEAD)
		return;

	if (n->child == NULL) {
		mandoc_msg(MANDOCERR_FO_NOHEAD, mdoc->parse,
		    n->line, n->pos, "Fo");
		return;
	}
	if (n->child != n->last) {
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    n->child->next->line, n->child->next->pos,
		    "Fo ... %s", n->child->next->string);
		while (n->child != n->last)
			mdoc_node_delete(mdoc, n->last);
	}

	post_fname(mdoc);
}

static void
post_fa(POST_ARGS)
{
	const struct mdoc_node *n;
	const char *cp;

	for (n = mdoc->last->child; n != NULL; n = n->next) {
		for (cp = n->string; *cp != '\0'; cp++) {
			/* Ignore callbacks and alterations. */
			if (*cp == '(' || *cp == '{')
				break;
			if (*cp != ',')
				continue;
			mandoc_msg(MANDOCERR_FA_COMMA, mdoc->parse,
			    n->line, n->pos + (cp - n->string),
			    n->string);
			break;
		}
	}
}

static void
post_vt(POST_ARGS)
{
	const struct mdoc_node *n;

	/*
	 * The Vt macro comes in both ELEM and BLOCK form, both of which
	 * have different syntaxes (yet more context-sensitive
	 * behaviour).  ELEM types must have a child, which is already
	 * guaranteed by the in_line parsing routine; BLOCK types,
	 * specifically the BODY, should only have TEXT children.
	 */

	if (MDOC_BODY != mdoc->last->type)
		return;

	for (n = mdoc->last->child; n; n = n->next)
		if (MDOC_TEXT != n->type)
			mandoc_msg(MANDOCERR_VT_CHILD, mdoc->parse,
			    n->line, n->pos, mdoc_macronames[n->tok]);
}

static void
post_nm(POST_ARGS)
{
	struct mdoc_node	*n;

	n = mdoc->last;

	if (n->last != NULL &&
	    (n->last->tok == MDOC_Pp ||
	     n->last->tok == MDOC_Lp))
		mdoc_node_relink(mdoc, n->last);

	if (NULL != mdoc->meta.name)
		return;

	mdoc_deroff(&mdoc->meta.name, n);

	if (NULL == mdoc->meta.name)
		mandoc_msg(MANDOCERR_NM_NONAME, mdoc->parse,
		    n->line, n->pos, "Nm");
}

static void
post_nd(POST_ARGS)
{
	struct mdoc_node	*n;

	n = mdoc->last;

	if (n->type != MDOC_BODY)
		return;

	if (n->child == NULL)
		mandoc_msg(MANDOCERR_ND_EMPTY, mdoc->parse,
		    n->line, n->pos, "Nd");

	post_hyph(mdoc);
}

static void
post_d1(POST_ARGS)
{
	struct mdoc_node	*n;

	n = mdoc->last;

	if (n->type != MDOC_BODY)
		return;

	if (n->child == NULL)
		mandoc_msg(MANDOCERR_BLK_EMPTY, mdoc->parse,
		    n->line, n->pos, "D1");

	post_hyph(mdoc);
}

static void
post_literal(POST_ARGS)
{
	struct mdoc_node	*n;

	n = mdoc->last;

	if (n->type != MDOC_BODY)
		return;

	if (n->child == NULL)
		mandoc_msg(MANDOCERR_BLK_EMPTY, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);

	if (n->tok == MDOC_Bd &&
	    n->norm->Bd.type != DISP_literal &&
	    n->norm->Bd.type != DISP_unfilled)
		return;

	mdoc->flags &= ~MDOC_LITERAL;
}

static void
post_defaults(POST_ARGS)
{
	struct mdoc_node *nn;

	/*
	 * The `Ar' defaults to "file ..." if no value is provided as an
	 * argument; the `Mt' and `Pa' macros use "~"; the `Li' just
	 * gets an empty string.
	 */

	if (mdoc->last->child)
		return;

	nn = mdoc->last;
	mdoc->next = MDOC_NEXT_CHILD;

	switch (nn->tok) {
	case MDOC_Ar:
		mdoc_word_alloc(mdoc, nn->line, nn->pos, "file");
		mdoc_word_alloc(mdoc, nn->line, nn->pos, "...");
		break;
	case MDOC_Pa:
		/* FALLTHROUGH */
	case MDOC_Mt:
		mdoc_word_alloc(mdoc, nn->line, nn->pos, "~");
		break;
	default:
		abort();
		/* NOTREACHED */
	}
	mdoc->last = nn;
}

static void
post_at(POST_ARGS)
{
	struct mdoc_node	*n;
	const char		*std_att;
	char			*att;

	n = mdoc->last;
	if (n->child == NULL) {
		mdoc->next = MDOC_NEXT_CHILD;
		mdoc_word_alloc(mdoc, n->line, n->pos, "AT&T UNIX");
		mdoc->last = n;
		return;
	}

	/*
	 * If we have a child, look it up in the standard keys.  If a
	 * key exist, use that instead of the child; if it doesn't,
	 * prefix "AT&T UNIX " to the existing data.
	 */

	n = n->child;
	assert(MDOC_TEXT == n->type);
	if (NULL == (std_att = mdoc_a2att(n->string))) {
		mandoc_vmsg(MANDOCERR_AT_BAD, mdoc->parse,
		    n->line, n->pos, "At %s", n->string);
		mandoc_asprintf(&att, "AT&T UNIX %s", n->string);
	} else
		att = mandoc_strdup(std_att);

	free(n->string);
	n->string = att;
}

static void
post_an(POST_ARGS)
{
	struct mdoc_node *np, *nch;

	np = mdoc->last;
	nch = np->child;
	if (np->norm->An.auth == AUTH__NONE) {
		if (nch == NULL)
			mandoc_msg(MANDOCERR_MACRO_EMPTY, mdoc->parse,
			    np->line, np->pos, "An");
	} else if (nch != NULL)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nch->line, nch->pos, "An ... %s", nch->string);
}

static void
post_en(POST_ARGS)
{

	if (MDOC_BLOCK == mdoc->last->type)
		mdoc->last->norm->Es = mdoc->last_es;
}

static void
post_es(POST_ARGS)
{

	mdoc->last_es = mdoc->last;
}

static void
post_it(POST_ARGS)
{
	int		  i, cols;
	enum mdoc_list	  lt;
	struct mdoc_node *nbl, *nit, *nch;

	nit = mdoc->last;
	if (nit->type != MDOC_BLOCK)
		return;

	nbl = nit->parent->parent;
	lt = nbl->norm->Bl.type;

	switch (lt) {
	case LIST_tag:
		/* FALLTHROUGH */
	case LIST_hang:
		/* FALLTHROUGH */
	case LIST_ohang:
		/* FALLTHROUGH */
	case LIST_inset:
		/* FALLTHROUGH */
	case LIST_diag:
		if (nit->head->child == NULL)
			mandoc_vmsg(MANDOCERR_IT_NOHEAD,
			    mdoc->parse, nit->line, nit->pos,
			    "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		break;
	case LIST_bullet:
		/* FALLTHROUGH */
	case LIST_dash:
		/* FALLTHROUGH */
	case LIST_enum:
		/* FALLTHROUGH */
	case LIST_hyphen:
		if (nit->body == NULL || nit->body->child == NULL)
			mandoc_vmsg(MANDOCERR_IT_NOBODY,
			    mdoc->parse, nit->line, nit->pos,
			    "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		/* FALLTHROUGH */
	case LIST_item:
		if (nit->head->child != NULL)
			mandoc_vmsg(MANDOCERR_ARG_SKIP,
			    mdoc->parse, nit->line, nit->pos,
			    "It %s", nit->head->child->string);
		break;
	case LIST_column:
		cols = (int)nbl->norm->Bl.ncols;

		assert(nit->head->child == NULL);

		for (i = 0, nch = nit->child; nch; nch = nch->next)
			if (nch->type == MDOC_BODY)
				i++;

		if (i < cols || i > cols + 1)
			mandoc_vmsg(MANDOCERR_BL_COL,
			    mdoc->parse, nit->line, nit->pos,
			    "%d columns, %d cells", cols, i);
		break;
	default:
		abort();
	}
}

static void
post_bl_block(POST_ARGS)
{
	struct mdoc_node *n, *ni, *nc;

	/*
	 * These are fairly complicated, so we've broken them into two
	 * functions.  post_bl_block_tag() is called when a -tag is
	 * specified, but no -width (it must be guessed).  The second
	 * when a -width is specified (macro indicators must be
	 * rewritten into real lengths).
	 */

	n = mdoc->last;

	if (LIST_tag == n->norm->Bl.type &&
	    NULL == n->norm->Bl.width) {
		post_bl_block_tag(mdoc);
		assert(n->norm->Bl.width);
	}

	for (ni = n->body->child; ni; ni = ni->next) {
		if (NULL == ni->body)
			continue;
		nc = ni->body->last;
		while (NULL != nc) {
			switch (nc->tok) {
			case MDOC_Pp:
				/* FALLTHROUGH */
			case MDOC_Lp:
				/* FALLTHROUGH */
			case MDOC_br:
				break;
			default:
				nc = NULL;
				continue;
			}
			if (NULL == ni->next) {
				mandoc_msg(MANDOCERR_PAR_MOVE,
				    mdoc->parse, nc->line, nc->pos,
				    mdoc_macronames[nc->tok]);
				mdoc_node_relink(mdoc, nc);
			} else if (0 == n->norm->Bl.comp &&
			    LIST_column != n->norm->Bl.type) {
				mandoc_vmsg(MANDOCERR_PAR_SKIP,
				    mdoc->parse, nc->line, nc->pos,
				    "%s before It",
				    mdoc_macronames[nc->tok]);
				mdoc_node_delete(mdoc, nc);
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
void
rewrite_macro2len(char **arg)
{
	size_t		  width;
	enum mdoct	  tok;

	if (*arg == NULL)
		return;
	else if ( ! strcmp(*arg, "Ds"))
		width = 6;
	else if ((tok = mdoc_hash_find(*arg)) == MDOC_MAX)
		return;
	else
		width = macro2len(tok);

	free(*arg);
	mandoc_asprintf(arg, "%zun", width);
}

static void
post_bl_block_tag(POST_ARGS)
{
	struct mdoc_node *n, *nn;
	size_t		  sz, ssz;
	int		  i;
	char		  buf[24];

	/*
	 * Calculate the -width for a `Bl -tag' list if it hasn't been
	 * provided.  Uses the first head macro.  NOTE AGAIN: this is
	 * ONLY if the -width argument has NOT been provided.  See
	 * rewrite_macro2len() for converting the -width string.
	 */

	sz = 10;
	n = mdoc->last;

	for (nn = n->body->child; nn; nn = nn->next) {
		if (MDOC_It != nn->tok)
			continue;

		assert(MDOC_BLOCK == nn->type);
		nn = nn->head->child;

		if (nn == NULL)
			break;

		if (MDOC_TEXT == nn->type) {
			sz = strlen(nn->string) + 1;
			break;
		}

		if (0 != (ssz = macro2len(nn->tok)))
			sz = ssz;

		break;
	}

	/* Defaults to ten ens. */

	(void)snprintf(buf, sizeof(buf), "%un", (unsigned int)sz);

	/*
	 * We have to dynamically add this to the macro's argument list.
	 * We're guaranteed that a MDOC_Width doesn't already exist.
	 */

	assert(n->args);
	i = (int)(n->args->argc)++;

	n->args->argv = mandoc_reallocarray(n->args->argv,
	    n->args->argc, sizeof(struct mdoc_argv));

	n->args->argv[i].arg = MDOC_Width;
	n->args->argv[i].line = n->line;
	n->args->argv[i].pos = n->pos;
	n->args->argv[i].sz = 1;
	n->args->argv[i].value = mandoc_malloc(sizeof(char *));
	n->args->argv[i].value[0] = mandoc_strdup(buf);

	/* Set our width! */
	n->norm->Bl.width = n->args->argv[i].value[0];
}

static void
post_bl_head(POST_ARGS)
{
	struct mdoc_node *nbl, *nh, *nch, *nnext;
	struct mdoc_argv *argv;
	int		  i, j;

	nh = mdoc->last;

	if (nh->norm->Bl.type != LIST_column) {
		if ((nch = nh->child) == NULL)
			return;
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nch->line, nch->pos, "Bl ... %s", nch->string);
		while (nch != NULL) {
			mdoc_node_delete(mdoc, nch);
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
	argv->sz += nh->nchild;
	argv->value = mandoc_reallocarray(argv->value,
	    argv->sz, sizeof(char *));

	nh->norm->Bl.ncols = argv->sz;
	nh->norm->Bl.cols = (void *)argv->value;

	for (nch = nh->child; nch != NULL; nch = nnext) {
		argv->value[i++] = nch->string;
		nch->string = NULL;
		nnext = nch->next;
		mdoc_node_delete(NULL, nch);
	}
	nh->nchild = 0;
	nh->child = NULL;
}

static void
post_bl(POST_ARGS)
{
	struct mdoc_node	*nparent, *nprev; /* of the Bl block */
	struct mdoc_node	*nblock, *nbody;  /* of the Bl */
	struct mdoc_node	*nchild, *nnext;  /* of the Bl body */

	nbody = mdoc->last;
	switch (nbody->type) {
	case MDOC_BLOCK:
		post_bl_block(mdoc);
		return;
	case MDOC_HEAD:
		post_bl_head(mdoc);
		return;
	case MDOC_BODY:
		break;
	default:
		return;
	}

	nchild = nbody->child;
	if (nchild == NULL) {
		mandoc_msg(MANDOCERR_BLK_EMPTY, mdoc->parse,
		    nbody->line, nbody->pos, "Bl");
		return;
	}
	while (nchild != NULL) {
		if (nchild->tok == MDOC_It ||
		    (nchild->tok == MDOC_Sm &&
		     nchild->next != NULL &&
		     nchild->next->tok == MDOC_It)) {
			nchild = nchild->next;
			continue;
		}

		mandoc_msg(MANDOCERR_BL_MOVE, mdoc->parse,
		    nchild->line, nchild->pos,
		    mdoc_macronames[nchild->tok]);

		/*
		 * Move the node out of the Bl block.
		 * First, collect all required node pointers.
		 */

		nblock  = nbody->parent;
		nprev   = nblock->prev;
		nparent = nblock->parent;
		nnext   = nchild->next;

		/*
		 * Unlink this child.
		 */

		assert(NULL == nchild->prev);
		if (0 == --nbody->nchild) {
			nbody->child = NULL;
			nbody->last  = NULL;
			assert(NULL == nnext);
		} else {
			nbody->child = nnext;
			nnext->prev = NULL;
		}

		/*
		 * Relink this child.
		 */

		nchild->parent = nparent;
		nchild->prev   = nprev;
		nchild->next   = nblock;

		nblock->prev = nchild;
		nparent->nchild++;
		if (NULL == nprev)
			nparent->child = nchild;
		else
			nprev->next = nchild;

		nchild = nnext;
	}
}

static void
post_bk(POST_ARGS)
{
	struct mdoc_node	*n;

	n = mdoc->last;

	if (n->type == MDOC_BLOCK && n->body->child == NULL) {
		mandoc_msg(MANDOCERR_BLK_EMPTY,
		    mdoc->parse, n->line, n->pos, "Bk");
		mdoc_node_delete(mdoc, n);
	}
}

static void
post_sm(struct mdoc *mdoc)
{
	struct mdoc_node	*nch;

	nch = mdoc->last->child;

	if (nch == NULL) {
		mdoc->flags ^= MDOC_SMOFF;
		return;
	}

	assert(nch->type == MDOC_TEXT);

	if ( ! strcmp(nch->string, "on")) {
		mdoc->flags &= ~MDOC_SMOFF;
		return;
	}
	if ( ! strcmp(nch->string, "off")) {
		mdoc->flags |= MDOC_SMOFF;
		return;
	}

	mandoc_vmsg(MANDOCERR_SM_BAD,
	    mdoc->parse, nch->line, nch->pos,
	    "%s %s", mdoc_macronames[mdoc->last->tok], nch->string);
	mdoc_node_relink(mdoc, nch);
	return;
}

static void
post_root(POST_ARGS)
{
	struct mdoc_node *n;

	/* Add missing prologue data. */

	if (mdoc->meta.date == NULL)
		mdoc->meta.date = mdoc->quick ?
		    mandoc_strdup("") :
		    mandoc_normdate(mdoc->parse, NULL, 0, 0);

	if (mdoc->meta.title == NULL) {
		mandoc_msg(MANDOCERR_DT_NOTITLE,
		    mdoc->parse, 0, 0, "EOF");
		mdoc->meta.title = mandoc_strdup("UNTITLED");
	}

	if (mdoc->meta.vol == NULL)
		mdoc->meta.vol = mandoc_strdup("LOCAL");

	if (mdoc->meta.os == NULL) {
		mandoc_msg(MANDOCERR_OS_MISSING,
		    mdoc->parse, 0, 0, NULL);
		mdoc->meta.os = mandoc_strdup("");
	}

	/* Check that we begin with a proper `Sh'. */

	n = mdoc->first->child;
	while (n != NULL && mdoc_macros[n->tok].flags & MDOC_PROLOGUE)
		n = n->next;

	if (n == NULL)
		mandoc_msg(MANDOCERR_DOC_EMPTY, mdoc->parse, 0, 0, NULL);
	else if (n->tok != MDOC_Sh)
		mandoc_msg(MANDOCERR_SEC_BEFORE, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
}

static void
post_st(POST_ARGS)
{
	struct mdoc_node	 *n, *nch;
	const char		 *p;

	n = mdoc->last;
	nch = n->child;

	assert(MDOC_TEXT == nch->type);

	if (NULL == (p = mdoc_a2st(nch->string))) {
		mandoc_vmsg(MANDOCERR_ST_BAD, mdoc->parse,
		    nch->line, nch->pos, "St %s", nch->string);
		mdoc_node_delete(mdoc, n);
	} else {
		free(nch->string);
		nch->string = mandoc_strdup(p);
	}
}

static void
post_rs(POST_ARGS)
{
	struct mdoc_node *np, *nch, *next, *prev;
	int		  i, j;

	np = mdoc->last;

	if (np->type != MDOC_BODY)
		return;

	if (np->child == NULL) {
		mandoc_msg(MANDOCERR_RS_EMPTY, mdoc->parse,
		    np->line, np->pos, "Rs");
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
			mandoc_msg(MANDOCERR_RS_BAD,
			    mdoc->parse, nch->line, nch->pos,
			    mdoc_macronames[nch->tok]);
			i = -1;
		} else if (nch->tok == MDOC__J || nch->tok == MDOC__B)
			np->norm->Rs.quote_T++;

		/*
		 * Remove this child from the chain.  This somewhat
		 * repeats mdoc_node_unlink(), but since we're
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
	struct mdoc_node	*nch;
	char			*cp;

	for (nch = mdoc->last->child; nch != NULL; nch = nch->next) {
		if (nch->type != MDOC_TEXT)
			continue;
		cp = nch->string;
		if (*cp == '\0')
			continue;
		while (*(++cp) != '\0')
			if (*cp == '-' &&
			    isalpha((unsigned char)cp[-1]) &&
			    isalpha((unsigned char)cp[1]))
				*cp = ASCII_HYPH;
	}
}

static void
post_ns(POST_ARGS)
{

	if (MDOC_LINE & mdoc->last->flags)
		mandoc_msg(MANDOCERR_NS_SKIP, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

static void
post_sh(POST_ARGS)
{

	post_ignpar(mdoc);

	switch (mdoc->last->type) {
	case MDOC_HEAD:
		post_sh_head(mdoc);
		break;
	case MDOC_BODY:
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
	struct mdoc_node *n;
	int hasnm, hasnd;

	hasnm = hasnd = 0;

	for (n = mdoc->last->child; n != NULL; n = n->next) {
		switch (n->tok) {
		case MDOC_Nm:
			hasnm = 1;
			break;
		case MDOC_Nd:
			hasnd = 1;
			if (n->next != NULL)
				mandoc_msg(MANDOCERR_NAMESEC_ND,
				    mdoc->parse, n->line, n->pos, NULL);
			break;
		case MDOC_MAX:
			if (hasnm)
				break;
			/* FALLTHROUGH */
		default:
			mandoc_msg(MANDOCERR_NAMESEC_BAD, mdoc->parse,
			    n->line, n->pos, mdoc_macronames[n->tok]);
			break;
		}
	}

	if ( ! hasnm)
		mandoc_msg(MANDOCERR_NAMESEC_NONM, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
	if ( ! hasnd)
		mandoc_msg(MANDOCERR_NAMESEC_NOND, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

static void
post_sh_see_also(POST_ARGS)
{
	const struct mdoc_node	*n;
	const char		*name, *sec;
	const char		*lastname, *lastsec, *lastpunct;
	int			 cmp;

	n = mdoc->last->child;
	lastname = lastsec = lastpunct = NULL;
	while (n != NULL) {
		if (n->tok != MDOC_Xr || n->nchild < 2)
			break;

		/* Process one .Xr node. */

		name = n->child->string;
		sec = n->child->next->string;
		if (lastsec != NULL) {
			if (lastpunct[0] != ',' || lastpunct[1] != '\0')
				mandoc_vmsg(MANDOCERR_XR_PUNCT,
				    mdoc->parse, n->line, n->pos,
				    "%s before %s(%s)", lastpunct,
				    name, sec);
			cmp = strcmp(lastsec, sec);
			if (cmp > 0)
				mandoc_vmsg(MANDOCERR_XR_ORDER,
				    mdoc->parse, n->line, n->pos,
				    "%s(%s) after %s(%s)", name,
				    sec, lastname, lastsec);
			else if (cmp == 0 &&
			    strcasecmp(lastname, name) > 0)
				mandoc_vmsg(MANDOCERR_XR_ORDER,
				    mdoc->parse, n->line, n->pos,
				    "%s after %s", name, lastname);
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
		if (n->type != MDOC_TEXT)
			break;
		for (name = n->string; *name != '\0'; name++)
			if (isalpha((const unsigned char)*name))
				return;
		lastpunct = n->string;
		if (n->next == NULL)
			mandoc_vmsg(MANDOCERR_XR_PUNCT, mdoc->parse,
			    n->line, n->pos, "%s after %s(%s)",
			    lastpunct, lastname, lastsec);
		n = n->next;
	}
}

static int
child_an(const struct mdoc_node *n)
{

	for (n = n->child; n != NULL; n = n->next)
		if ((n->tok == MDOC_An && n->nchild) || child_an(n))
			return(1);
	return(0);
}

static void
post_sh_authors(POST_ARGS)
{

	if ( ! child_an(mdoc->last))
		mandoc_msg(MANDOCERR_AN_MISSING, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

static void
post_sh_head(POST_ARGS)
{
	struct mdoc_node *n;
	const char	*goodsec;
	char		*secname;
	enum mdoc_sec	 sec;

	/*
	 * Process a new section.  Sections are either "named" or
	 * "custom".  Custom sections are user-defined, while named ones
	 * follow a conventional order and may only appear in certain
	 * manual sections.
	 */

	secname = NULL;
	sec = SEC_CUSTOM;
	mdoc_deroff(&secname, mdoc->last);
	sec = NULL == secname ? SEC_CUSTOM : a2sec(secname);

	/* The NAME should be first. */

	if (SEC_NAME != sec && SEC_NONE == mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_NAMESEC_FIRST, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secname);

	/* The SYNOPSIS gets special attention in other areas. */

	if (SEC_SYNOPSIS == sec) {
		roff_setreg(mdoc->roff, "nS", 1, '=');
		mdoc->flags |= MDOC_SYNOPSIS;
	} else {
		roff_setreg(mdoc->roff, "nS", 0, '=');
		mdoc->flags &= ~MDOC_SYNOPSIS;
	}

	/* Mark our last section. */

	mdoc->lastsec = sec;

	/*
	 * Set the section attribute for the current HEAD, for its
	 * parent BLOCK, and for the HEAD children; the latter can
	 * only be TEXT nodes, so no recursion is needed.
	 * For other blocks and elements, including .Sh BODY, this is
	 * done when allocating the node data structures, but for .Sh
	 * BLOCK and HEAD, the section is still unknown at that time.
	 */

	mdoc->last->parent->sec = sec;
	mdoc->last->sec = sec;
	for (n = mdoc->last->child; n; n = n->next)
		n->sec = sec;

	/* We don't care about custom sections after this. */

	if (SEC_CUSTOM == sec) {
		free(secname);
		return;
	}

	/*
	 * Check whether our non-custom section is being repeated or is
	 * out of order.
	 */

	if (sec == mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_SEC_REP, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secname);

	if (sec < mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_SEC_ORDER, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secname);

	/* Mark the last named section. */

	mdoc->lastnamed = sec;

	/* Check particular section/manual conventions. */

	if (mdoc->meta.msec == NULL) {
		free(secname);
		return;
	}

	goodsec = NULL;
	switch (sec) {
	case SEC_ERRORS:
		if (*mdoc->meta.msec == '4')
			break;
		goodsec = "2, 3, 4, 9";
		/* FALLTHROUGH */
	case SEC_RETURN_VALUES:
		/* FALLTHROUGH */
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
		mandoc_vmsg(MANDOCERR_SEC_MSEC, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s for %s only", secname, goodsec);
		break;
	default:
		break;
	}
	free(secname);
}

static void
post_ignpar(POST_ARGS)
{
	struct mdoc_node *np;

	switch (mdoc->last->type) {
	case MDOC_HEAD:
		post_hyph(mdoc);
		return;
	case MDOC_BODY:
		break;
	default:
		return;
	}

	if (NULL != (np = mdoc->last->child))
		if (MDOC_Pp == np->tok || MDOC_Lp == np->tok) {
			mandoc_vmsg(MANDOCERR_PAR_SKIP,
			    mdoc->parse, np->line, np->pos,
			    "%s after %s", mdoc_macronames[np->tok],
			    mdoc_macronames[mdoc->last->tok]);
			mdoc_node_delete(mdoc, np);
		}

	if (NULL != (np = mdoc->last->last))
		if (MDOC_Pp == np->tok || MDOC_Lp == np->tok) {
			mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
			    np->line, np->pos, "%s at the end of %s",
			    mdoc_macronames[np->tok],
			    mdoc_macronames[mdoc->last->tok]);
			mdoc_node_delete(mdoc, np);
		}
}

static void
pre_par(PRE_ARGS)
{

	if (NULL == mdoc->last)
		return;
	if (MDOC_ELEM != n->type && MDOC_BLOCK != n->type)
		return;

	/*
	 * Don't allow prior `Lp' or `Pp' prior to a paragraph-type
	 * block:  `Lp', `Pp', or non-compact `Bd' or `Bl'.
	 */

	if (MDOC_Pp != mdoc->last->tok &&
	    MDOC_Lp != mdoc->last->tok &&
	    MDOC_br != mdoc->last->tok)
		return;
	if (MDOC_Bl == n->tok && n->norm->Bl.comp)
		return;
	if (MDOC_Bd == n->tok && n->norm->Bd.comp)
		return;
	if (MDOC_It == n->tok && n->parent->norm->Bl.comp)
		return;

	mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
	    mdoc->last->line, mdoc->last->pos,
	    "%s before %s", mdoc_macronames[mdoc->last->tok],
	    mdoc_macronames[n->tok]);
	mdoc_node_delete(mdoc, mdoc->last);
}

static void
post_par(POST_ARGS)
{
	struct mdoc_node *np;

	np = mdoc->last;

	if (np->tok == MDOC_sp) {
		if (np->nchild > 1)
			mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
			    np->child->next->line, np->child->next->pos,
			    "sp ... %s", np->child->next->string);
	} else if (np->child != NULL)
		mandoc_vmsg(MANDOCERR_ARG_SKIP,
		    mdoc->parse, np->line, np->pos, "%s %s",
		    mdoc_macronames[np->tok], np->child->string);

	if (NULL == (np = mdoc->last->prev)) {
		np = mdoc->last->parent;
		if (MDOC_Sh != np->tok && MDOC_Ss != np->tok)
			return;
	} else if (MDOC_Pp != np->tok && MDOC_Lp != np->tok &&
	    (MDOC_br != mdoc->last->tok ||
	     (MDOC_sp != np->tok && MDOC_br != np->tok)))
		return;

	mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
	    mdoc->last->line, mdoc->last->pos,
	    "%s after %s", mdoc_macronames[mdoc->last->tok],
	    mdoc_macronames[np->tok]);
	mdoc_node_delete(mdoc, mdoc->last);
}

static void
pre_literal(PRE_ARGS)
{

	pre_display(mdoc, n);

	if (MDOC_BODY != n->type)
		return;

	/*
	 * The `Dl' (note "el" not "one") and `Bd -literal' and `Bd
	 * -unfilled' macros set MDOC_LITERAL on entrance to the body.
	 */

	switch (n->tok) {
	case MDOC_Dl:
		mdoc->flags |= MDOC_LITERAL;
		break;
	case MDOC_Bd:
		if (DISP_literal == n->norm->Bd.type)
			mdoc->flags |= MDOC_LITERAL;
		if (DISP_unfilled == n->norm->Bd.type)
			mdoc->flags |= MDOC_LITERAL;
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}

static void
post_dd(POST_ARGS)
{
	struct mdoc_node *n;
	char		 *datestr;

	if (mdoc->meta.date)
		free(mdoc->meta.date);

	n = mdoc->last;
	if (NULL == n->child || '\0' == n->child->string[0]) {
		mdoc->meta.date = mdoc->quick ? mandoc_strdup("") :
		    mandoc_normdate(mdoc->parse, NULL, n->line, n->pos);
		goto out;
	}

	datestr = NULL;
	mdoc_deroff(&datestr, n);
	if (mdoc->quick)
		mdoc->meta.date = datestr;
	else {
		mdoc->meta.date = mandoc_normdate(mdoc->parse,
		    datestr, n->line, n->pos);
		free(datestr);
	}
out:
	mdoc_node_delete(mdoc, n);
}

static void
post_dt(POST_ARGS)
{
	struct mdoc_node *nn, *n;
	const char	 *cp;
	char		 *p;

	n = mdoc->last;

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
		mandoc_msg(MANDOCERR_DT_NOTITLE,
		    mdoc->parse, n->line, n->pos, "Dt");
		mdoc->meta.title = mandoc_strdup("UNTITLED");
	} else {
		mdoc->meta.title = mandoc_strdup(nn->string);

		/* Check that all characters are uppercase. */

		for (p = nn->string; *p != '\0'; p++)
			if (islower((unsigned char)*p)) {
				mandoc_vmsg(MANDOCERR_TITLE_CASE,
				    mdoc->parse, nn->line,
				    nn->pos + (p - nn->string),
				    "Dt %s", nn->string);
				break;
			}
	}

	/* Mandatory second argument: section. */

	if (nn != NULL)
		nn = nn->next;

	if (nn == NULL) {
		mandoc_vmsg(MANDOCERR_MSEC_MISSING,
		    mdoc->parse, n->line, n->pos,
		    "Dt %s", mdoc->meta.title);
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		goto out;  /* msec and arch remain NULL. */
	}

	mdoc->meta.msec = mandoc_strdup(nn->string);

	/* Infer volume title from section number. */

	cp = mandoc_a2msec(nn->string);
	if (cp == NULL) {
		mandoc_vmsg(MANDOCERR_MSEC_BAD, mdoc->parse,
		    nn->line, nn->pos, "Dt ... %s", nn->string);
		mdoc->meta.vol = mandoc_strdup(nn->string);
	} else
		mdoc->meta.vol = mandoc_strdup(cp);

	/* Optional third argument: architecture. */

	if ((nn = nn->next) == NULL)
		goto out;

	for (p = nn->string; *p != '\0'; p++)
		*p = tolower((unsigned char)*p);
	mdoc->meta.arch = mandoc_strdup(nn->string);

	/* Ignore fourth and later arguments. */

	if ((nn = nn->next) != NULL)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nn->line, nn->pos, "Dt ... %s", nn->string);

out:
	mdoc_node_delete(mdoc, n);
}

static void
post_bx(POST_ARGS)
{
	struct mdoc_node	*n;

	/*
	 * Make `Bx's second argument always start with an uppercase
	 * letter.  Groff checks if it's an "accepted" term, but we just
	 * uppercase blindly.
	 */

	n = mdoc->last->child;
	if (n && NULL != (n = n->next))
		*n->string = (char)toupper((unsigned char)*n->string);
}

static void
post_os(POST_ARGS)
{
#ifndef OSNAME
	struct utsname	  utsname;
	static char	 *defbuf;
#endif
	struct mdoc_node *n;

	n = mdoc->last;

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
	mdoc_deroff(&mdoc->meta.os, n);
	if (mdoc->meta.os)
		goto out;

	if (mdoc->defos) {
		mdoc->meta.os = mandoc_strdup(mdoc->defos);
		goto out;
	}

#ifdef OSNAME
	mdoc->meta.os = mandoc_strdup(OSNAME);
#else /*!OSNAME */
	if (NULL == defbuf) {
		if (-1 == uname(&utsname)) {
			mandoc_msg(MANDOCERR_OS_UNAME, mdoc->parse,
			    n->line, n->pos, "Os");
			defbuf = mandoc_strdup("UNKNOWN");
		} else
			mandoc_asprintf(&defbuf, "%s %s",
			    utsname.sysname, utsname.release);
	}
	mdoc->meta.os = mandoc_strdup(defbuf);
#endif /*!OSNAME*/

out:
	mdoc_node_delete(mdoc, n);
}

/*
 * If no argument is provided,
 * fill in the name of the current manual page.
 */
static void
post_ex(POST_ARGS)
{
	struct mdoc_node *n;

	n = mdoc->last;

	if (n->child)
		return;

	if (mdoc->meta.name == NULL) {
		mandoc_msg(MANDOCERR_EX_NONAME, mdoc->parse,
		    n->line, n->pos, "Ex");
		return;
	}

	mdoc->next = MDOC_NEXT_CHILD;
	mdoc_word_alloc(mdoc, n->line, n->pos, mdoc->meta.name);
	mdoc->last = n;
}

static enum mdoc_sec
a2sec(const char *p)
{
	int		 i;

	for (i = 0; i < (int)SEC__MAX; i++)
		if (secnames[i] && 0 == strcmp(p, secnames[i]))
			return((enum mdoc_sec)i);

	return(SEC_CUSTOM);
}

static size_t
macro2len(enum mdoct macro)
{

	switch (macro) {
	case MDOC_Ad:
		return(12);
	case MDOC_Ao:
		return(12);
	case MDOC_An:
		return(12);
	case MDOC_Aq:
		return(12);
	case MDOC_Ar:
		return(12);
	case MDOC_Bo:
		return(12);
	case MDOC_Bq:
		return(12);
	case MDOC_Cd:
		return(12);
	case MDOC_Cm:
		return(10);
	case MDOC_Do:
		return(10);
	case MDOC_Dq:
		return(12);
	case MDOC_Dv:
		return(12);
	case MDOC_Eo:
		return(12);
	case MDOC_Em:
		return(10);
	case MDOC_Er:
		return(17);
	case MDOC_Ev:
		return(15);
	case MDOC_Fa:
		return(12);
	case MDOC_Fl:
		return(10);
	case MDOC_Fo:
		return(16);
	case MDOC_Fn:
		return(16);
	case MDOC_Ic:
		return(10);
	case MDOC_Li:
		return(16);
	case MDOC_Ms:
		return(6);
	case MDOC_Nm:
		return(10);
	case MDOC_No:
		return(12);
	case MDOC_Oo:
		return(10);
	case MDOC_Op:
		return(14);
	case MDOC_Pa:
		return(32);
	case MDOC_Pf:
		return(12);
	case MDOC_Po:
		return(12);
	case MDOC_Pq:
		return(12);
	case MDOC_Ql:
		return(16);
	case MDOC_Qo:
		return(12);
	case MDOC_So:
		return(12);
	case MDOC_Sq:
		return(12);
	case MDOC_Sy:
		return(6);
	case MDOC_Sx:
		return(16);
	case MDOC_Tn:
		return(10);
	case MDOC_Va:
		return(12);
	case MDOC_Vt:
		return(12);
	case MDOC_Xr:
		return(10);
	default:
		break;
	};
	return(0);
}
