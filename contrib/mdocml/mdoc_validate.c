/*	$Id: mdoc_validate.c,v 1.318 2017/02/06 03:44:58 schwarze Exp $ */
/*
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010-2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include "roff.h"
#include "mdoc.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "libmdoc.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */

#define	POST_ARGS struct roff_man *mdoc

enum	check_ineq {
	CHECK_LT,
	CHECK_GT,
	CHECK_EQ
};

typedef	void	(*v_post)(POST_ARGS);

static	int	 build_list(struct roff_man *, int);
static	void	 check_text(struct roff_man *, int, int, char *);
static	void	 check_argv(struct roff_man *,
			struct roff_node *, struct mdoc_argv *);
static	void	 check_args(struct roff_man *, struct roff_node *);
static	int	 child_an(const struct roff_node *);
static	size_t		macro2len(int);
static	void	 rewrite_macro2len(char **);

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
static	void	 post_sh(POST_ARGS);
static	void	 post_sh_head(POST_ARGS);
static	void	 post_sh_name(POST_ARGS);
static	void	 post_sh_see_also(POST_ARGS);
static	void	 post_sh_authors(POST_ARGS);
static	void	 post_sm(POST_ARGS);
static	void	 post_st(POST_ARGS);
static	void	 post_std(POST_ARGS);
static	void	 post_xr(POST_ARGS);
static	void	 post_xx(POST_ARGS);

static	v_post mdoc_valids[MDOC_MAX] = {
	NULL,		/* Ap */
	post_dd,	/* Dd */
	post_dt,	/* Dt */
	post_os,	/* Os */
	post_sh,	/* Sh */
	post_ignpar,	/* Ss */
	post_par,	/* Pp */
	post_display,	/* D1 */
	post_display,	/* Dl */
	post_display,	/* Bd */
	NULL,		/* Ed */
	post_bl,	/* Bl */
	NULL,		/* El */
	post_it,	/* It */
	NULL,		/* Ad */
	post_an,	/* An */
	post_defaults,	/* Ar */
	NULL,		/* Cd */
	NULL,		/* Cm */
	NULL,		/* Dv */
	NULL,		/* Er */
	NULL,		/* Ev */
	post_ex,	/* Ex */
	post_fa,	/* Fa */
	NULL,		/* Fd */
	NULL,		/* Fl */
	post_fn,	/* Fn */
	NULL,		/* Ft */
	NULL,		/* Ic */
	NULL,		/* In */
	post_defaults,	/* Li */
	post_nd,	/* Nd */
	post_nm,	/* Nm */
	NULL,		/* Op */
	post_obsolete,	/* Ot */
	post_defaults,	/* Pa */
	post_rv,	/* Rv */
	post_st,	/* St */
	NULL,		/* Va */
	NULL,		/* Vt */
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
	NULL,		/* Aq */
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
	NULL,		/* Em */
	NULL,		/* Eo */
	post_xx,	/* Fx */
	NULL,		/* Ms */
	NULL,		/* No */
	post_ns,	/* Ns */
	post_xx,	/* Nx */
	post_xx,	/* Ox */
	NULL,		/* Pc */
	NULL,		/* Pf */
	NULL,		/* Po */
	NULL,		/* Pq */
	NULL,		/* Qc */
	NULL,		/* Ql */
	NULL,		/* Qo */
	NULL,		/* Qq */
	NULL,		/* Re */
	post_rs,	/* Rs */
	NULL,		/* Sc */
	NULL,		/* So */
	NULL,		/* Sq */
	post_sm,	/* Sm */
	post_hyph,	/* Sx */
	NULL,		/* Sy */
	NULL,		/* Tn */
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
	NULL,		/* Hf */
	post_obsolete,	/* Fr */
	post_eoln,	/* Ud */
	post_lb,	/* Lb */
	post_par,	/* Lp */
	NULL,		/* Lk */
	post_defaults,	/* Mt */
	NULL,		/* Brq */
	NULL,		/* Bro */
	NULL,		/* Brc */
	NULL,		/* %C */
	post_es,	/* Es */
	post_en,	/* En */
	post_xx,	/* Dx */
	NULL,		/* %Q */
	post_par,	/* br */
	post_par,	/* sp */
	NULL,		/* %U */
	NULL,		/* Ta */
	NULL,		/* ll */
};

#define	RSORD_MAX 14 /* Number of `Rs' blocks. */

static	const int rsord[RSORD_MAX] = {
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
mdoc_node_validate(struct roff_man *mdoc)
{
	struct roff_node *n;
	v_post *p;

	n = mdoc->last;
	mdoc->last = mdoc->last->child;
	while (mdoc->last != NULL) {
		mdoc_node_validate(mdoc);
		if (mdoc->last == n)
			mdoc->last = mdoc->last->child;
		else
			mdoc->last = mdoc->last->next;
	}

	mdoc->last = n;
	mdoc->next = ROFF_NEXT_SIBLING;
	switch (n->type) {
	case ROFFT_TEXT:
		if (n->sec != SEC_SYNOPSIS ||
		    (n->parent->tok != MDOC_Cd && n->parent->tok != MDOC_Fd))
			check_text(mdoc, n->line, n->pos, n->string);
		break;
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

		p = mdoc_valids + n->tok;
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

	if (MDOC_LITERAL & mdoc->flags)
		return;

	for (cp = p; NULL != (p = strchr(p, '\t')); p++)
		mandoc_msg(MANDOCERR_FI_TAB, mdoc->parse,
		    ln, pos + (int)(p - cp), NULL);
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
		if (NULL == n->norm->Bl.width)
			mandoc_msg(MANDOCERR_BL_NOWIDTH, mdoc->parse,
			    n->line, n->pos, "Bl -tag");
		break;
	case LIST_column:
	case LIST_diag:
	case LIST_ohang:
	case LIST_inset:
	case LIST_item:
		if (n->norm->Bl.width)
			mandoc_vmsg(MANDOCERR_BL_SKIPW, mdoc->parse,
			    wa->line, wa->pos, "Bl -%s",
			    mdoc_argnames[mdoclt]);
		break;
	case LIST_bullet:
	case LIST_dash:
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
post_eoln(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;
	if (n->child != NULL)
		mandoc_vmsg(MANDOCERR_ARG_SKIP, mdoc->parse,
		    n->line, n->pos, "%s %s",
		    mdoc_macronames[n->tok], n->child->string);

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
		mdoc_node_relink(mdoc, n);
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
		mandoc_msg(MANDOCERR_EX_NONAME, mdoc->parse,
		    n->line, n->pos, "Ex");
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

	roff_word_alloc(mdoc, n->line, n->pos, "library");
	mdoc->last->flags = NODE_NOSRC;
	roff_word_alloc(mdoc, n->line, n->pos, "\\(Lq");
	mdoc->last->flags = NODE_DELIMO | NODE_NOSRC;
	mdoc->last = mdoc->last->next;
	roff_word_alloc(mdoc, n->line, n->pos, "\\(Rq");
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

	n = mdoc->last;
	if (n->args && n->args->argc == 1)
		if (n->args->argv[0].arg == MDOC_Std)
			return;

	mandoc_msg(MANDOCERR_ARG_STD, mdoc->parse,
	    n->line, n->pos, mdoc_macronames[n->tok]);
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
		mandoc_vmsg(MANDOCERR_ST_BAD, mdoc->parse,
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
post_obsolete(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	if (n->type == ROFFT_ELEM || n->type == ROFFT_BLOCK)
		mandoc_msg(MANDOCERR_MACRO_OBS, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
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
			mandoc_msg(MANDOCERR_BF_NOFONT, mdoc->parse,
			    np->line, np->pos, "Bf");
			return;
		}
		nch = nch->next;
	}
	if (nch != NULL)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
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
		mandoc_vmsg(MANDOCERR_BF_BADFONT, mdoc->parse,
		    np->child->line, np->child->pos,
		    "Bf %s", np->child->string);
}

static void
post_fname(POST_ARGS)
{
	const struct roff_node	*n;
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
	const struct roff_node	*n;

	n = mdoc->last;

	if (n->type != ROFFT_HEAD)
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
			roff_node_delete(mdoc, n->last);
	}

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
			mandoc_msg(MANDOCERR_FA_COMMA, mdoc->parse,
			    n->line, n->pos + (cp - n->string),
			    n->string);
			break;
		}
	}
}

static void
post_nm(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;

	if (n->last != NULL &&
	    (n->last->tok == MDOC_Pp ||
	     n->last->tok == MDOC_Lp))
		mdoc_node_relink(mdoc, n->last);

	if (mdoc->meta.name == NULL)
		deroff(&mdoc->meta.name, n);

	if (mdoc->meta.name == NULL ||
	    (mdoc->lastsec == SEC_NAME && n->child == NULL))
		mandoc_msg(MANDOCERR_NM_NONAME, mdoc->parse,
		    n->line, n->pos, "Nm");

	if ((n->type != ROFFT_ELEM && n->type != ROFFT_HEAD) ||
	    (n->child != NULL && n->child->type == ROFFT_TEXT) ||
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

	if (n->child == NULL)
		mandoc_msg(MANDOCERR_ND_EMPTY, mdoc->parse,
		    n->line, n->pos, "Nd");

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
			mandoc_msg(MANDOCERR_BLK_EMPTY, mdoc->parse,
			    n->line, n->pos, mdoc_macronames[n->tok]);
		else if (n->tok == MDOC_D1)
			post_hyph(mdoc);
		break;
	case ROFFT_BLOCK:
		if (n->tok == MDOC_Bd) {
			if (n->args == NULL) {
				mandoc_msg(MANDOCERR_BD_NOARG,
				    mdoc->parse, n->line, n->pos, "Bd");
				mdoc->next = ROFF_NEXT_SIBLING;
				while (n->body->child != NULL)
					mdoc_node_relink(mdoc,
					    n->body->child);
				roff_node_delete(mdoc, n);
				break;
			}
			post_bd(mdoc);
			post_prevpar(mdoc);
		}
		for (np = n->parent; np != NULL; np = np->parent) {
			if (np->type == ROFFT_BLOCK && np->tok == MDOC_Bd) {
				mandoc_vmsg(MANDOCERR_BD_NEST,
				    mdoc->parse, n->line, n->pos,
				    "%s in Bd", mdoc_macronames[n->tok]);
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
	struct roff_node *nn;

	/*
	 * The `Ar' defaults to "file ..." if no value is provided as an
	 * argument; the `Mt' and `Pa' macros use "~"; the `Li' just
	 * gets an empty string.
	 */

	if (mdoc->last->child != NULL)
		return;

	nn = mdoc->last;

	switch (nn->tok) {
	case MDOC_Ar:
		mdoc->next = ROFF_NEXT_CHILD;
		roff_word_alloc(mdoc, nn->line, nn->pos, "file");
		mdoc->last->flags |= NODE_NOSRC;
		roff_word_alloc(mdoc, nn->line, nn->pos, "...");
		mdoc->last->flags |= NODE_NOSRC;
		break;
	case MDOC_Pa:
	case MDOC_Mt:
		mdoc->next = ROFF_NEXT_CHILD;
		roff_word_alloc(mdoc, nn->line, nn->pos, "~");
		mdoc->last->flags |= NODE_NOSRC;
		break;
	default:
		abort();
	}
	mdoc->last = nn;
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
		mandoc_vmsg(MANDOCERR_AT_BAD, mdoc->parse,
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
			mandoc_msg(MANDOCERR_MACRO_EMPTY, mdoc->parse,
			    np->line, np->pos, "An");
	} else if (nch != NULL)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nch->line, nch->pos, "An ... %s", nch->string);
}

static void
post_en(POST_ARGS)
{

	post_obsolete(mdoc);
	if (mdoc->last->type == ROFFT_BLOCK)
		mdoc->last->norm->Es = mdoc->last_es;
}

static void
post_es(POST_ARGS)
{

	post_obsolete(mdoc);
	mdoc->last_es = mdoc->last;
}

static void
post_xx(POST_ARGS)
{
	struct roff_node	*n;
	const char		*os;

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
			mandoc_vmsg(MANDOCERR_IT_NOHEAD,
			    mdoc->parse, nit->line, nit->pos,
			    "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		break;
	case LIST_bullet:
	case LIST_dash:
	case LIST_enum:
	case LIST_hyphen:
		if (nit->body == NULL || nit->body->child == NULL)
			mandoc_vmsg(MANDOCERR_IT_NOBODY,
			    mdoc->parse, nit->line, nit->pos,
			    "Bl -%s It",
			    mdoc_argnames[nbl->args->argv[0].arg]);
		/* FALLTHROUGH */
	case LIST_item:
		if ((nch = nit->head->child) != NULL)
			mandoc_vmsg(MANDOCERR_ARG_SKIP,
			    mdoc->parse, nit->line, nit->pos,
			    "It %s", nch->string == NULL ?
			    mdoc_macronames[nch->tok] : nch->string);
		break;
	case LIST_column:
		cols = (int)nbl->norm->Bl.ncols;

		assert(nit->head->child == NULL);

		i = 0;
		for (nch = nit->child; nch != NULL; nch = nch->next)
			if (nch->type == ROFFT_BODY)
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
			case MDOC_Lp:
			case MDOC_br:
				break;
			default:
				nc = NULL;
				continue;
			}
			if (ni->next == NULL) {
				mandoc_msg(MANDOCERR_PAR_MOVE,
				    mdoc->parse, nc->line, nc->pos,
				    mdoc_macronames[nc->tok]);
				mdoc_node_relink(mdoc, nc);
			} else if (n->norm->Bl.comp == 0 &&
			    n->norm->Bl.type != LIST_column) {
				mandoc_vmsg(MANDOCERR_PAR_SKIP,
				    mdoc->parse, nc->line, nc->pos,
				    "%s before It",
				    mdoc_macronames[nc->tok]);
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
void
rewrite_macro2len(char **arg)
{
	size_t		  width;
	int		  tok;

	if (*arg == NULL)
		return;
	else if ( ! strcmp(*arg, "Ds"))
		width = 6;
	else if ((tok = mdoc_hash_find(*arg)) == TOKEN_NONE)
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
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
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
	struct roff_node	*nparent, *nprev; /* of the Bl block */
	struct roff_node	*nblock, *nbody;  /* of the Bl */
	struct roff_node	*nchild, *nnext;  /* of the Bl body */

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

	nchild = nbody->child;
	if (nchild == NULL) {
		mandoc_msg(MANDOCERR_BLK_EMPTY, mdoc->parse,
		    nbody->line, nbody->pos, "Bl");
		return;
	}
	while (nchild != NULL) {
		nnext = nchild->next;
		if (nchild->tok == MDOC_It ||
		    (nchild->tok == MDOC_Sm &&
		     nnext != NULL && nnext->tok == MDOC_It)) {
			nchild = nnext;
			continue;
		}

		/*
		 * In .Bl -column, the first rows may be implicit,
		 * that is, they may not start with .It macros.
		 * Such rows may be followed by nodes generated on the
		 * roff level, for example .TS, which cannot be moved
		 * out of the list.  In that case, wrap such roff nodes
		 * into an implicit row.
		 */

		if (nchild->prev != NULL) {
			mdoc->last = nchild;
			mdoc->next = ROFF_NEXT_SIBLING;
			roff_block_alloc(mdoc, nchild->line,
			    nchild->pos, MDOC_It);
			roff_head_alloc(mdoc, nchild->line,
			    nchild->pos, MDOC_It);
			mdoc->next = ROFF_NEXT_SIBLING;
			roff_body_alloc(mdoc, nchild->line,
			    nchild->pos, MDOC_It);
			while (nchild->tok != MDOC_It) {
				mdoc_node_relink(mdoc, nchild);
				if ((nchild = nnext) == NULL)
					break;
				nnext = nchild->next;
				mdoc->next = ROFF_NEXT_SIBLING;
			}
			mdoc->last = nbody;
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

		/*
		 * Unlink this child.
		 */

		nbody->child = nnext;
		if (nnext == NULL)
			nbody->last  = NULL;
		else
			nnext->prev = NULL;

		/*
		 * Relink this child.
		 */

		nchild->parent = nparent;
		nchild->prev   = nprev;
		nchild->next   = nblock;

		nblock->prev = nchild;
		if (nprev == NULL)
			nparent->child = nchild;
		else
			nprev->next = nchild;

		nchild = nnext;
	}
}

static void
post_bk(POST_ARGS)
{
	struct roff_node	*n;

	n = mdoc->last;

	if (n->type == ROFFT_BLOCK && n->body->child == NULL) {
		mandoc_msg(MANDOCERR_BLK_EMPTY,
		    mdoc->parse, n->line, n->pos, "Bk");
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

	mandoc_vmsg(MANDOCERR_SM_BAD,
	    mdoc->parse, nch->line, nch->pos,
	    "%s %s", mdoc_macronames[mdoc->last->tok], nch->string);
	mdoc_node_relink(mdoc, nch);
	return;
}

static void
post_root(POST_ARGS)
{
	struct roff_node *n;

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
	while (n != NULL && n->tok != TOKEN_NONE &&
	    mdoc_macros[n->tok].flags & MDOC_PROLOGUE)
		n = n->next;

	if (n == NULL)
		mandoc_msg(MANDOCERR_DOC_EMPTY, mdoc->parse, 0, 0, NULL);
	else if (n->tok != MDOC_Sh)
		mandoc_msg(MANDOCERR_SEC_BEFORE, mdoc->parse,
		    n->line, n->pos, mdoc_macronames[n->tok]);
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
	struct roff_node	*nch;
	char			*cp;

	for (nch = mdoc->last->child; nch != NULL; nch = nch->next) {
		if (nch->type != ROFFT_TEXT)
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

	if (mdoc->last->flags & NODE_LINE)
		mandoc_msg(MANDOCERR_NS_SKIP, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

static void
post_sh(POST_ARGS)
{

	post_ignpar(mdoc);

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
				mandoc_vmsg(MANDOCERR_NAMESEC_PUNCT,
				    mdoc->parse, n->line, n->pos,
				    "Nm %s", n->child->string);
			hasnm = 1;
			continue;
		case MDOC_Nd:
			hasnd = 1;
			if (n->next != NULL)
				mandoc_msg(MANDOCERR_NAMESEC_ND,
				    mdoc->parse, n->line, n->pos, NULL);
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
			mandoc_msg(MANDOCERR_NAMESEC_BAD, mdoc->parse,
			    n->line, n->pos, mdoc_macronames[n->tok]);
			continue;
		}
		break;
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
		if (n->type != ROFFT_TEXT)
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
		mandoc_msg(MANDOCERR_AN_MISSING, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, NULL);
}

static void
post_sh_head(POST_ARGS)
{
	struct roff_node	*nch;
	const char		*goodsec;
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
		mandoc_vmsg(MANDOCERR_NAMESEC_FIRST, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos, "Sh %s",
		    sec != SEC_CUSTOM ? secnames[sec] :
		    (nch = mdoc->last->child) == NULL ? "" :
		    nch->type == ROFFT_TEXT ? nch->string :
		    mdoc_macronames[nch->tok]);

	/* The SYNOPSIS gets special attention in other areas. */

	if (sec == SEC_SYNOPSIS) {
		roff_setreg(mdoc->roff, "nS", 1, '=');
		mdoc->flags |= MDOC_SYNOPSIS;
	} else {
		roff_setreg(mdoc->roff, "nS", 0, '=');
		mdoc->flags &= ~MDOC_SYNOPSIS;
	}

	/* Mark our last section. */

	mdoc->lastsec = sec;

	/* We don't care about custom sections after this. */

	if (sec == SEC_CUSTOM)
		return;

	/*
	 * Check whether our non-custom section is being repeated or is
	 * out of order.
	 */

	if (sec == mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_SEC_REP, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secnames[sec]);

	if (sec < mdoc->lastnamed)
		mandoc_vmsg(MANDOCERR_SEC_ORDER, mdoc->parse,
		    mdoc->last->line, mdoc->last->pos,
		    "Sh %s", secnames[sec]);

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
		mandoc_vmsg(MANDOCERR_SEC_MSEC, mdoc->parse,
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
		mandoc_vmsg(MANDOCERR_XR_NOSEC, mdoc->parse,
		    n->line, n->pos, "Xr %s", nch->string);
		return;
	}
	assert(nch->next == n->last);
}

static void
post_ignpar(POST_ARGS)
{
	struct roff_node *np;

	switch (mdoc->last->type) {
	case ROFFT_HEAD:
		post_hyph(mdoc);
		return;
	case ROFFT_BODY:
		break;
	default:
		return;
	}

	if ((np = mdoc->last->child) != NULL)
		if (np->tok == MDOC_Pp || np->tok == MDOC_Lp) {
			mandoc_vmsg(MANDOCERR_PAR_SKIP,
			    mdoc->parse, np->line, np->pos,
			    "%s after %s", mdoc_macronames[np->tok],
			    mdoc_macronames[mdoc->last->tok]);
			roff_node_delete(mdoc, np);
		}

	if ((np = mdoc->last->last) != NULL)
		if (np->tok == MDOC_Pp || np->tok == MDOC_Lp) {
			mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
			    np->line, np->pos, "%s at the end of %s",
			    mdoc_macronames[np->tok],
			    mdoc_macronames[mdoc->last->tok]);
			roff_node_delete(mdoc, np);
		}
}

static void
post_prevpar(POST_ARGS)
{
	struct roff_node *n;

	n = mdoc->last;
	if (NULL == n->prev)
		return;
	if (n->type != ROFFT_ELEM && n->type != ROFFT_BLOCK)
		return;

	/*
	 * Don't allow prior `Lp' or `Pp' prior to a paragraph-type
	 * block:  `Lp', `Pp', or non-compact `Bd' or `Bl'.
	 */

	if (n->prev->tok != MDOC_Pp &&
	    n->prev->tok != MDOC_Lp &&
	    n->prev->tok != MDOC_br)
		return;
	if (n->tok == MDOC_Bl && n->norm->Bl.comp)
		return;
	if (n->tok == MDOC_Bd && n->norm->Bd.comp)
		return;
	if (n->tok == MDOC_It && n->parent->norm->Bl.comp)
		return;

	mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
	    n->prev->line, n->prev->pos,
	    "%s before %s", mdoc_macronames[n->prev->tok],
	    mdoc_macronames[n->tok]);
	roff_node_delete(mdoc, n->prev);
}

static void
post_par(POST_ARGS)
{
	struct roff_node *np;

	np = mdoc->last;
	if (np->tok != MDOC_br && np->tok != MDOC_sp)
		post_prevpar(mdoc);

	if (np->tok == MDOC_sp) {
		if (np->child != NULL && np->child->next != NULL)
			mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
			    np->child->next->line, np->child->next->pos,
			    "sp ... %s", np->child->next->string);
	} else if (np->child != NULL)
		mandoc_vmsg(MANDOCERR_ARG_SKIP,
		    mdoc->parse, np->line, np->pos, "%s %s",
		    mdoc_macronames[np->tok], np->child->string);

	if ((np = mdoc->last->prev) == NULL) {
		np = mdoc->last->parent;
		if (np->tok != MDOC_Sh && np->tok != MDOC_Ss)
			return;
	} else if (np->tok != MDOC_Pp && np->tok != MDOC_Lp &&
	    (mdoc->last->tok != MDOC_br ||
	     (np->tok != MDOC_sp && np->tok != MDOC_br)))
		return;

	mandoc_vmsg(MANDOCERR_PAR_SKIP, mdoc->parse,
	    mdoc->last->line, mdoc->last->pos,
	    "%s after %s", mdoc_macronames[mdoc->last->tok],
	    mdoc_macronames[np->tok]);
	roff_node_delete(mdoc, mdoc->last);
}

static void
post_dd(POST_ARGS)
{
	struct roff_node *n;
	char		 *datestr;

	n = mdoc->last;
	n->flags |= NODE_NOPRT;

	if (mdoc->meta.date != NULL) {
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Dd");
		free(mdoc->meta.date);
	} else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, mdoc->parse,
		    n->line, n->pos, "Dd");
	else if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dd after Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
		    n->line, n->pos, "Dd after Os");

	if (n->child == NULL || n->child->string[0] == '\0') {
		mdoc->meta.date = mdoc->quick ? mandoc_strdup("") :
		    mandoc_normdate(mdoc->parse, NULL, n->line, n->pos);
		return;
	}

	datestr = NULL;
	deroff(&datestr, n);
	if (mdoc->quick)
		mdoc->meta.date = datestr;
	else {
		mdoc->meta.date = mandoc_normdate(mdoc->parse,
		    datestr, n->line, n->pos);
		free(datestr);
	}
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
		mandoc_msg(MANDOCERR_DT_LATE, mdoc->parse,
		    n->line, n->pos, "Dt");
		return;
	}

	if (mdoc->meta.title != NULL)
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Dt");
	else if (mdoc->meta.os != NULL)
		mandoc_msg(MANDOCERR_PROLOG_ORDER, mdoc->parse,
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

	/* Mandatory second argument: section. */

	if (nn != NULL)
		nn = nn->next;

	if (nn == NULL) {
		mandoc_vmsg(MANDOCERR_MSEC_MISSING,
		    mdoc->parse, n->line, n->pos,
		    "Dt %s", mdoc->meta.title);
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		return;  /* msec and arch remain NULL. */
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
		return;

	for (p = nn->string; *p != '\0'; p++)
		*p = tolower((unsigned char)*p);
	mdoc->meta.arch = mandoc_strdup(nn->string);

	/* Ignore fourth and later arguments. */

	if ((nn = nn->next) != NULL)
		mandoc_vmsg(MANDOCERR_ARG_EXCESS, mdoc->parse,
		    nn->line, nn->pos, "Dt ... %s", nn->string);
}

static void
post_bx(POST_ARGS)
{
	struct roff_node	*n, *nch;

	n = mdoc->last;
	nch = n->child;

	if (nch != NULL) {
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
		mandoc_msg(MANDOCERR_PROLOG_REP, mdoc->parse,
		    n->line, n->pos, "Os");
	else if (mdoc->flags & MDOC_PBODY)
		mandoc_msg(MANDOCERR_PROLOG_LATE, mdoc->parse,
		    n->line, n->pos, "Os");

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
		return;

	if (mdoc->defos) {
		mdoc->meta.os = mandoc_strdup(mdoc->defos);
		return;
	}

#ifdef OSNAME
	mdoc->meta.os = mandoc_strdup(OSNAME);
#else /*!OSNAME */
	if (defbuf == NULL) {
		if (uname(&utsname) == -1) {
			mandoc_msg(MANDOCERR_OS_UNAME, mdoc->parse,
			    n->line, n->pos, "Os");
			defbuf = mandoc_strdup("UNKNOWN");
		} else
			mandoc_asprintf(&defbuf, "%s %s",
			    utsname.sysname, utsname.release);
	}
	mdoc->meta.os = mandoc_strdup(defbuf);
#endif /*!OSNAME*/
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
macro2len(int macro)
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
