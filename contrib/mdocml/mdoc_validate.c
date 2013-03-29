/*	$Id: mdoc_validate.c,v 1.182 2012/03/23 05:50:25 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2011 Ingo Schwarze <schwarze@openbsd.org>
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

#ifndef	OSNAME
#include <sys/utsname.h>
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mdoc.h"
#include "mandoc.h"
#include "libmdoc.h"
#include "libmandoc.h"

/* FIXME: .Bl -diag can't have non-text children in HEAD. */

#define	PRE_ARGS  struct mdoc *mdoc, struct mdoc_node *n
#define	POST_ARGS struct mdoc *mdoc

#define	NUMSIZ	  32
#define	DATESIZE  32

enum	check_ineq {
	CHECK_LT,
	CHECK_GT,
	CHECK_EQ
};

enum	check_lvl {
	CHECK_WARN,
	CHECK_ERROR,
};

typedef	int	(*v_pre)(PRE_ARGS);
typedef	int	(*v_post)(POST_ARGS);

struct	valids {
	v_pre	*pre;
	v_post	*post;
};

static	int	 check_count(struct mdoc *, enum mdoc_type, 
			enum check_lvl, enum check_ineq, int);
static	int	 check_parent(PRE_ARGS, enum mdoct, enum mdoc_type);
static	void	 check_text(struct mdoc *, int, int, char *);
static	void	 check_argv(struct mdoc *, 
			struct mdoc_node *, struct mdoc_argv *);
static	void	 check_args(struct mdoc *, struct mdoc_node *);
static	int	 concat(char *, const struct mdoc_node *, size_t);
static	enum mdoc_sec	a2sec(const char *);
static	size_t		macro2len(enum mdoct);

static	int	 ebool(POST_ARGS);
static	int	 berr_ge1(POST_ARGS);
static	int	 bwarn_ge1(POST_ARGS);
static	int	 ewarn_eq0(POST_ARGS);
static	int	 ewarn_eq1(POST_ARGS);
static	int	 ewarn_ge1(POST_ARGS);
static	int	 ewarn_le1(POST_ARGS);
static	int	 hwarn_eq0(POST_ARGS);
static	int	 hwarn_eq1(POST_ARGS);
static	int	 hwarn_ge1(POST_ARGS);
static	int	 hwarn_le1(POST_ARGS);

static	int	 post_an(POST_ARGS);
static	int	 post_at(POST_ARGS);
static	int	 post_bf(POST_ARGS);
static	int	 post_bl(POST_ARGS);
static	int	 post_bl_block(POST_ARGS);
static	int	 post_bl_block_width(POST_ARGS);
static	int	 post_bl_block_tag(POST_ARGS);
static	int	 post_bl_head(POST_ARGS);
static	int	 post_bx(POST_ARGS);
static	int	 post_dd(POST_ARGS);
static	int	 post_dt(POST_ARGS);
static	int	 post_defaults(POST_ARGS);
static	int	 post_literal(POST_ARGS);
static	int	 post_eoln(POST_ARGS);
static	int	 post_it(POST_ARGS);
static	int	 post_lb(POST_ARGS);
static	int	 post_nm(POST_ARGS);
static	int	 post_ns(POST_ARGS);
static	int	 post_os(POST_ARGS);
static	int	 post_ignpar(POST_ARGS);
static	int	 post_prol(POST_ARGS);
static	int	 post_root(POST_ARGS);
static	int	 post_rs(POST_ARGS);
static	int	 post_sh(POST_ARGS);
static	int	 post_sh_body(POST_ARGS);
static	int	 post_sh_head(POST_ARGS);
static	int	 post_st(POST_ARGS);
static	int	 post_std(POST_ARGS);
static	int	 post_vt(POST_ARGS);
static	int	 pre_an(PRE_ARGS);
static	int	 pre_bd(PRE_ARGS);
static	int	 pre_bl(PRE_ARGS);
static	int	 pre_dd(PRE_ARGS);
static	int	 pre_display(PRE_ARGS);
static	int	 pre_dt(PRE_ARGS);
static	int	 pre_it(PRE_ARGS);
static	int	 pre_literal(PRE_ARGS);
static	int	 pre_os(PRE_ARGS);
static	int	 pre_par(PRE_ARGS);
static	int	 pre_sh(PRE_ARGS);
static	int	 pre_ss(PRE_ARGS);
static	int	 pre_std(PRE_ARGS);

static	v_post	 posts_an[] = { post_an, NULL };
static	v_post	 posts_at[] = { post_at, post_defaults, NULL };
static	v_post	 posts_bd[] = { post_literal, hwarn_eq0, bwarn_ge1, NULL };
static	v_post	 posts_bf[] = { hwarn_le1, post_bf, NULL };
static	v_post	 posts_bk[] = { hwarn_eq0, bwarn_ge1, NULL };
static	v_post	 posts_bl[] = { bwarn_ge1, post_bl, NULL };
static	v_post	 posts_bx[] = { post_bx, NULL };
static	v_post	 posts_bool[] = { ebool, NULL };
static	v_post	 posts_eoln[] = { post_eoln, NULL };
static	v_post	 posts_defaults[] = { post_defaults, NULL };
static	v_post	 posts_dd[] = { post_dd, post_prol, NULL };
static	v_post	 posts_dl[] = { post_literal, bwarn_ge1, NULL };
static	v_post	 posts_dt[] = { post_dt, post_prol, NULL };
static	v_post	 posts_fo[] = { hwarn_eq1, bwarn_ge1, NULL };
static	v_post	 posts_it[] = { post_it, NULL };
static	v_post	 posts_lb[] = { post_lb, NULL };
static	v_post	 posts_nd[] = { berr_ge1, NULL };
static	v_post	 posts_nm[] = { post_nm, NULL };
static	v_post	 posts_notext[] = { ewarn_eq0, NULL };
static	v_post	 posts_ns[] = { post_ns, NULL };
static	v_post	 posts_os[] = { post_os, post_prol, NULL };
static	v_post	 posts_rs[] = { post_rs, NULL };
static	v_post	 posts_sh[] = { post_ignpar, hwarn_ge1, post_sh, NULL };
static	v_post	 posts_sp[] = { ewarn_le1, NULL };
static	v_post	 posts_ss[] = { post_ignpar, hwarn_ge1, NULL };
static	v_post	 posts_st[] = { post_st, NULL };
static	v_post	 posts_std[] = { post_std, NULL };
static	v_post	 posts_text[] = { ewarn_ge1, NULL };
static	v_post	 posts_text1[] = { ewarn_eq1, NULL };
static	v_post	 posts_vt[] = { post_vt, NULL };
static	v_post	 posts_wline[] = { bwarn_ge1, NULL };
static	v_pre	 pres_an[] = { pre_an, NULL };
static	v_pre	 pres_bd[] = { pre_display, pre_bd, pre_literal, pre_par, NULL };
static	v_pre	 pres_bl[] = { pre_bl, pre_par, NULL };
static	v_pre	 pres_d1[] = { pre_display, NULL };
static	v_pre	 pres_dl[] = { pre_literal, pre_display, NULL };
static	v_pre	 pres_dd[] = { pre_dd, NULL };
static	v_pre	 pres_dt[] = { pre_dt, NULL };
static	v_pre	 pres_er[] = { NULL, NULL };
static	v_pre	 pres_fd[] = { NULL, NULL };
static	v_pre	 pres_it[] = { pre_it, pre_par, NULL };
static	v_pre	 pres_os[] = { pre_os, NULL };
static	v_pre	 pres_pp[] = { pre_par, NULL };
static	v_pre	 pres_sh[] = { pre_sh, NULL };
static	v_pre	 pres_ss[] = { pre_ss, NULL };
static	v_pre	 pres_std[] = { pre_std, NULL };

static	const struct valids mdoc_valids[MDOC_MAX] = {
	{ NULL, NULL },				/* Ap */
	{ pres_dd, posts_dd },			/* Dd */
	{ pres_dt, posts_dt },			/* Dt */
	{ pres_os, posts_os },			/* Os */
	{ pres_sh, posts_sh },			/* Sh */ 
	{ pres_ss, posts_ss },			/* Ss */ 
	{ pres_pp, posts_notext },		/* Pp */ 
	{ pres_d1, posts_wline },		/* D1 */
	{ pres_dl, posts_dl },			/* Dl */
	{ pres_bd, posts_bd },			/* Bd */
	{ NULL, NULL },				/* Ed */
	{ pres_bl, posts_bl },			/* Bl */ 
	{ NULL, NULL },				/* El */
	{ pres_it, posts_it },			/* It */
	{ NULL, NULL },				/* Ad */ 
	{ pres_an, posts_an },			/* An */ 
	{ NULL, posts_defaults },		/* Ar */
	{ NULL, NULL },				/* Cd */ 
	{ NULL, NULL },				/* Cm */
	{ NULL, NULL },				/* Dv */ 
	{ pres_er, NULL },			/* Er */ 
	{ NULL, NULL },				/* Ev */ 
	{ pres_std, posts_std },		/* Ex */ 
	{ NULL, NULL },				/* Fa */ 
	{ pres_fd, posts_text },		/* Fd */
	{ NULL, NULL },				/* Fl */
	{ NULL, NULL },				/* Fn */ 
	{ NULL, NULL },				/* Ft */ 
	{ NULL, NULL },				/* Ic */ 
	{ NULL, posts_text1 },			/* In */ 
	{ NULL, posts_defaults },		/* Li */
	{ NULL, posts_nd },			/* Nd */
	{ NULL, posts_nm },			/* Nm */
	{ NULL, NULL },				/* Op */
	{ NULL, NULL },				/* Ot */
	{ NULL, posts_defaults },		/* Pa */
	{ pres_std, posts_std },		/* Rv */
	{ NULL, posts_st },			/* St */ 
	{ NULL, NULL },				/* Va */
	{ NULL, posts_vt },			/* Vt */ 
	{ NULL, posts_text },			/* Xr */ 
	{ NULL, posts_text },			/* %A */
	{ NULL, posts_text },			/* %B */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, posts_text },			/* %D */
	{ NULL, posts_text },			/* %I */
	{ NULL, posts_text },			/* %J */
	{ NULL, posts_text },			/* %N */
	{ NULL, posts_text },			/* %O */
	{ NULL, posts_text },			/* %P */
	{ NULL, posts_text },			/* %R */
	{ NULL, posts_text },			/* %T */ /* FIXME: can be used outside Rs/Re. */
	{ NULL, posts_text },			/* %V */
	{ NULL, NULL },				/* Ac */
	{ NULL, NULL },				/* Ao */
	{ NULL, NULL },				/* Aq */
	{ NULL, posts_at },			/* At */ 
	{ NULL, NULL },				/* Bc */
	{ NULL, posts_bf },			/* Bf */
	{ NULL, NULL },				/* Bo */
	{ NULL, NULL },				/* Bq */
	{ NULL, NULL },				/* Bsx */
	{ NULL, posts_bx },			/* Bx */
	{ NULL, posts_bool },			/* Db */
	{ NULL, NULL },				/* Dc */
	{ NULL, NULL },				/* Do */
	{ NULL, NULL },				/* Dq */
	{ NULL, NULL },				/* Ec */
	{ NULL, NULL },				/* Ef */ 
	{ NULL, NULL },				/* Em */ 
	{ NULL, NULL },				/* Eo */
	{ NULL, NULL },				/* Fx */
	{ NULL, NULL },				/* Ms */ 
	{ NULL, posts_notext },			/* No */
	{ NULL, posts_ns },			/* Ns */
	{ NULL, NULL },				/* Nx */
	{ NULL, NULL },				/* Ox */
	{ NULL, NULL },				/* Pc */
	{ NULL, posts_text1 },			/* Pf */
	{ NULL, NULL },				/* Po */
	{ NULL, NULL },				/* Pq */
	{ NULL, NULL },				/* Qc */
	{ NULL, NULL },				/* Ql */
	{ NULL, NULL },				/* Qo */
	{ NULL, NULL },				/* Qq */
	{ NULL, NULL },				/* Re */
	{ NULL, posts_rs },			/* Rs */
	{ NULL, NULL },				/* Sc */
	{ NULL, NULL },				/* So */
	{ NULL, NULL },				/* Sq */
	{ NULL, posts_bool },			/* Sm */ 
	{ NULL, NULL },				/* Sx */
	{ NULL, NULL },				/* Sy */
	{ NULL, NULL },				/* Tn */
	{ NULL, NULL },				/* Ux */
	{ NULL, NULL },				/* Xc */
	{ NULL, NULL },				/* Xo */
	{ NULL, posts_fo },			/* Fo */ 
	{ NULL, NULL },				/* Fc */ 
	{ NULL, NULL },				/* Oo */
	{ NULL, NULL },				/* Oc */
	{ NULL, posts_bk },			/* Bk */
	{ NULL, NULL },				/* Ek */
	{ NULL, posts_eoln },			/* Bt */
	{ NULL, NULL },				/* Hf */
	{ NULL, NULL },				/* Fr */
	{ NULL, posts_eoln },			/* Ud */
	{ NULL, posts_lb },			/* Lb */
	{ NULL, posts_notext },			/* Lp */ 
	{ NULL, NULL },				/* Lk */ 
	{ NULL, posts_defaults },		/* Mt */ 
	{ NULL, NULL },				/* Brq */ 
	{ NULL, NULL },				/* Bro */ 
	{ NULL, NULL },				/* Brc */ 
	{ NULL, posts_text },			/* %C */
	{ NULL, NULL },				/* Es */
	{ NULL, NULL },				/* En */
	{ NULL, NULL },				/* Dx */
	{ NULL, posts_text },			/* %Q */
	{ NULL, posts_notext },			/* br */
	{ pres_pp, posts_sp },			/* sp */
	{ NULL, posts_text1 },			/* %U */
	{ NULL, NULL },				/* Ta */
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
	MDOC__P,
	MDOC__Q,
	MDOC__D,
	MDOC__O,
	MDOC__C,
	MDOC__U
};

static	const char * const secnames[SEC__MAX] = {
	NULL,
	"NAME",
	"LIBRARY",
	"SYNOPSIS",
	"DESCRIPTION",
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

int
mdoc_valid_pre(struct mdoc *mdoc, struct mdoc_node *n)
{
	v_pre		*p;
	int		 line, pos;
	char		*tp;

	switch (n->type) {
	case (MDOC_TEXT):
		tp = n->string;
		line = n->line;
		pos = n->pos;
		check_text(mdoc, line, pos, tp);
		/* FALLTHROUGH */
	case (MDOC_TBL):
		/* FALLTHROUGH */
	case (MDOC_EQN):
		/* FALLTHROUGH */
	case (MDOC_ROOT):
		return(1);
	default:
		break;
	}

	check_args(mdoc, n);

	if (NULL == mdoc_valids[n->tok].pre)
		return(1);
	for (p = mdoc_valids[n->tok].pre; *p; p++)
		if ( ! (*p)(mdoc, n)) 
			return(0);
	return(1);
}


int
mdoc_valid_post(struct mdoc *mdoc)
{
	v_post		*p;

	if (MDOC_VALID & mdoc->last->flags)
		return(1);
	mdoc->last->flags |= MDOC_VALID;

	switch (mdoc->last->type) {
	case (MDOC_TEXT):
		/* FALLTHROUGH */
	case (MDOC_EQN):
		/* FALLTHROUGH */
	case (MDOC_TBL):
		return(1);
	case (MDOC_ROOT):
		return(post_root(mdoc));
	default:
		break;
	}

	if (NULL == mdoc_valids[mdoc->last->tok].post)
		return(1);
	for (p = mdoc_valids[mdoc->last->tok].post; *p; p++)
		if ( ! (*p)(mdoc)) 
			return(0);

	return(1);
}

static int
check_count(struct mdoc *m, enum mdoc_type type, 
		enum check_lvl lvl, enum check_ineq ineq, int val)
{
	const char	*p;
	enum mandocerr	 t;

	if (m->last->type != type)
		return(1);
	
	switch (ineq) {
	case (CHECK_LT):
		p = "less than ";
		if (m->last->nchild < val)
			return(1);
		break;
	case (CHECK_GT):
		p = "more than ";
		if (m->last->nchild > val)
			return(1);
		break;
	case (CHECK_EQ):
		p = "";
		if (val == m->last->nchild)
			return(1);
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	t = lvl == CHECK_WARN ? MANDOCERR_ARGCWARN : MANDOCERR_ARGCOUNT;
	mandoc_vmsg(t, m->parse, m->last->line, m->last->pos,
			"want %s%d children (have %d)",
			p, val, m->last->nchild);
	return(1);
}

static int
berr_ge1(POST_ARGS)
{

	return(check_count(mdoc, MDOC_BODY, CHECK_ERROR, CHECK_GT, 0));
}

static int
bwarn_ge1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_BODY, CHECK_WARN, CHECK_GT, 0));
}

static int
ewarn_eq0(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 0));
}

static int
ewarn_eq1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 1));
}

static int
ewarn_ge1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_GT, 0));
}

static int
ewarn_le1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_LT, 2));
}

static int
hwarn_eq0(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_EQ, 0));
}

static int
hwarn_eq1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_EQ, 1));
}

static int
hwarn_ge1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_GT, 0));
}

static int
hwarn_le1(POST_ARGS)
{
	return(check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_LT, 2));
}

static void
check_args(struct mdoc *m, struct mdoc_node *n)
{
	int		 i;

	if (NULL == n->args)
		return;

	assert(n->args->argc);
	for (i = 0; i < (int)n->args->argc; i++)
		check_argv(m, n, &n->args->argv[i]);
}

static void
check_argv(struct mdoc *m, struct mdoc_node *n, struct mdoc_argv *v)
{
	int		 i;

	for (i = 0; i < (int)v->sz; i++)
		check_text(m, v->line, v->pos, v->value[i]);

	/* FIXME: move to post_std(). */

	if (MDOC_Std == v->arg)
		if ( ! (v->sz || m->meta.name))
			mdoc_nmsg(m, n, MANDOCERR_NONAME);
}

static void
check_text(struct mdoc *m, int ln, int pos, char *p)
{
	char		*cp;

	if (MDOC_LITERAL & m->flags)
		return;

	for (cp = p; NULL != (p = strchr(p, '\t')); p++)
		mdoc_pmsg(m, ln, pos + (int)(p - cp), MANDOCERR_BADTAB);
}

static int
check_parent(PRE_ARGS, enum mdoct tok, enum mdoc_type t)
{

	assert(n->parent);
	if ((MDOC_ROOT == t || tok == n->parent->tok) &&
			(t == n->parent->type))
		return(1);

	mandoc_vmsg(MANDOCERR_SYNTCHILD, mdoc->parse, n->line, 
			n->pos, "want parent %s", MDOC_ROOT == t ? 
			"<root>" : mdoc_macronames[tok]);
	return(0);
}


static int
pre_display(PRE_ARGS)
{
	struct mdoc_node *node;

	if (MDOC_BLOCK != n->type)
		return(1);

	for (node = mdoc->last->parent; node; node = node->parent) 
		if (MDOC_BLOCK == node->type)
			if (MDOC_Bd == node->tok)
				break;

	if (node)
		mdoc_nmsg(mdoc, n, MANDOCERR_NESTEDDISP);

	return(1);
}


static int
pre_bl(PRE_ARGS)
{
	int		  i, comp, dup;
	const char	 *offs, *width;
	enum mdoc_list	  lt;
	struct mdoc_node *np;

	if (MDOC_BLOCK != n->type) {
		if (ENDBODY_NOT != n->end) {
			assert(n->pending);
			np = n->pending->parent;
		} else
			np = n->parent;

		assert(np);
		assert(MDOC_BLOCK == np->type);
		assert(MDOC_Bl == np->tok);
		return(1);
	}

	/* 
	 * First figure out which kind of list to use: bind ourselves to
	 * the first mentioned list type and warn about any remaining
	 * ones.  If we find no list type, we default to LIST_item.
	 */

	/* LINTED */
	for (i = 0; n->args && i < (int)n->args->argc; i++) {
		lt = LIST__NONE;
		dup = comp = 0;
		width = offs = NULL;
		switch (n->args->argv[i].arg) {
		/* Set list types. */
		case (MDOC_Bullet):
			lt = LIST_bullet;
			break;
		case (MDOC_Dash):
			lt = LIST_dash;
			break;
		case (MDOC_Enum):
			lt = LIST_enum;
			break;
		case (MDOC_Hyphen):
			lt = LIST_hyphen;
			break;
		case (MDOC_Item):
			lt = LIST_item;
			break;
		case (MDOC_Tag):
			lt = LIST_tag;
			break;
		case (MDOC_Diag):
			lt = LIST_diag;
			break;
		case (MDOC_Hang):
			lt = LIST_hang;
			break;
		case (MDOC_Ohang):
			lt = LIST_ohang;
			break;
		case (MDOC_Inset):
			lt = LIST_inset;
			break;
		case (MDOC_Column):
			lt = LIST_column;
			break;
		/* Set list arguments. */
		case (MDOC_Compact):
			dup = n->norm->Bl.comp;
			comp = 1;
			break;
		case (MDOC_Width):
			/* NB: this can be empty! */
			if (n->args->argv[i].sz) {
				width = n->args->argv[i].value[0];
				dup = (NULL != n->norm->Bl.width);
				break;
			}
			mdoc_nmsg(mdoc, n, MANDOCERR_IGNARGV);
			break;
		case (MDOC_Offset):
			/* NB: this can be empty! */
			if (n->args->argv[i].sz) {
				offs = n->args->argv[i].value[0];
				dup = (NULL != n->norm->Bl.offs);
				break;
			}
			mdoc_nmsg(mdoc, n, MANDOCERR_IGNARGV);
			break;
		default:
			continue;
		}

		/* Check: duplicate auxiliary arguments. */

		if (dup)
			mdoc_nmsg(mdoc, n, MANDOCERR_ARGVREP);

		if (comp && ! dup)
			n->norm->Bl.comp = comp;
		if (offs && ! dup)
			n->norm->Bl.offs = offs;
		if (width && ! dup)
			n->norm->Bl.width = width;

		/* Check: multiple list types. */

		if (LIST__NONE != lt && n->norm->Bl.type != LIST__NONE)
			mdoc_nmsg(mdoc, n, MANDOCERR_LISTREP);

		/* Assign list type. */

		if (LIST__NONE != lt && n->norm->Bl.type == LIST__NONE) {
			n->norm->Bl.type = lt;
			/* Set column information, too. */
			if (LIST_column == lt) {
				n->norm->Bl.ncols = 
					n->args->argv[i].sz;
				n->norm->Bl.cols = (void *)
					n->args->argv[i].value;
			}
		}

		/* The list type should come first. */

		if (n->norm->Bl.type == LIST__NONE)
			if (n->norm->Bl.width || 
					n->norm->Bl.offs || 
					n->norm->Bl.comp)
				mdoc_nmsg(mdoc, n, MANDOCERR_LISTFIRST);

		continue;
	}

	/* Allow lists to default to LIST_item. */

	if (LIST__NONE == n->norm->Bl.type) {
		mdoc_nmsg(mdoc, n, MANDOCERR_LISTTYPE);
		n->norm->Bl.type = LIST_item;
	}

	/* 
	 * Validate the width field.  Some list types don't need width
	 * types and should be warned about them.  Others should have it
	 * and must also be warned.
	 */

	switch (n->norm->Bl.type) {
	case (LIST_tag):
		if (n->norm->Bl.width)
			break;
		mdoc_nmsg(mdoc, n, MANDOCERR_NOWIDTHARG);
		break;
	case (LIST_column):
		/* FALLTHROUGH */
	case (LIST_diag):
		/* FALLTHROUGH */
	case (LIST_ohang):
		/* FALLTHROUGH */
	case (LIST_inset):
		/* FALLTHROUGH */
	case (LIST_item):
		if (n->norm->Bl.width)
			mdoc_nmsg(mdoc, n, MANDOCERR_IGNARGV);
		break;
	default:
		break;
	}

	return(1);
}


static int
pre_bd(PRE_ARGS)
{
	int		  i, dup, comp;
	enum mdoc_disp 	  dt;
	const char	 *offs;
	struct mdoc_node *np;

	if (MDOC_BLOCK != n->type) {
		if (ENDBODY_NOT != n->end) {
			assert(n->pending);
			np = n->pending->parent;
		} else
			np = n->parent;

		assert(np);
		assert(MDOC_BLOCK == np->type);
		assert(MDOC_Bd == np->tok);
		return(1);
	}

	/* LINTED */
	for (i = 0; n->args && i < (int)n->args->argc; i++) {
		dt = DISP__NONE;
		dup = comp = 0;
		offs = NULL;

		switch (n->args->argv[i].arg) {
		case (MDOC_Centred):
			dt = DISP_centred;
			break;
		case (MDOC_Ragged):
			dt = DISP_ragged;
			break;
		case (MDOC_Unfilled):
			dt = DISP_unfilled;
			break;
		case (MDOC_Filled):
			dt = DISP_filled;
			break;
		case (MDOC_Literal):
			dt = DISP_literal;
			break;
		case (MDOC_File):
			mdoc_nmsg(mdoc, n, MANDOCERR_BADDISP);
			return(0);
		case (MDOC_Offset):
			/* NB: this can be empty! */
			if (n->args->argv[i].sz) {
				offs = n->args->argv[i].value[0];
				dup = (NULL != n->norm->Bd.offs);
				break;
			}
			mdoc_nmsg(mdoc, n, MANDOCERR_IGNARGV);
			break;
		case (MDOC_Compact):
			comp = 1;
			dup = n->norm->Bd.comp;
			break;
		default:
			abort();
			/* NOTREACHED */
		}

		/* Check whether we have duplicates. */

		if (dup)
			mdoc_nmsg(mdoc, n, MANDOCERR_ARGVREP);

		/* Make our auxiliary assignments. */

		if (offs && ! dup)
			n->norm->Bd.offs = offs;
		if (comp && ! dup)
			n->norm->Bd.comp = comp;

		/* Check whether a type has already been assigned. */

		if (DISP__NONE != dt && n->norm->Bd.type != DISP__NONE)
			mdoc_nmsg(mdoc, n, MANDOCERR_DISPREP);

		/* Make our type assignment. */

		if (DISP__NONE != dt && n->norm->Bd.type == DISP__NONE)
			n->norm->Bd.type = dt;
	}

	if (DISP__NONE == n->norm->Bd.type) {
		mdoc_nmsg(mdoc, n, MANDOCERR_DISPTYPE);
		n->norm->Bd.type = DISP_ragged;
	}

	return(1);
}


static int
pre_ss(PRE_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);
	return(check_parent(mdoc, n, MDOC_Sh, MDOC_BODY));
}


static int
pre_sh(PRE_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);

	roff_regunset(mdoc->roff, REG_nS);
	return(check_parent(mdoc, n, MDOC_MAX, MDOC_ROOT));
}


static int
pre_it(PRE_ARGS)
{

	if (MDOC_BLOCK != n->type)
		return(1);

	return(check_parent(mdoc, n, MDOC_Bl, MDOC_BODY));
}


static int
pre_an(PRE_ARGS)
{
	int		 i;

	if (NULL == n->args)
		return(1);
	
	for (i = 1; i < (int)n->args->argc; i++)
		mdoc_pmsg(mdoc, n->args->argv[i].line, 
			n->args->argv[i].pos, MANDOCERR_IGNARGV);

	if (MDOC_Split == n->args->argv[0].arg)
		n->norm->An.auth = AUTH_split;
	else if (MDOC_Nosplit == n->args->argv[0].arg)
		n->norm->An.auth = AUTH_nosplit;
	else
		abort();

	return(1);
}

static int
pre_std(PRE_ARGS)
{

	if (n->args && 1 == n->args->argc)
		if (MDOC_Std == n->args->argv[0].arg)
			return(1);

	mdoc_nmsg(mdoc, n, MANDOCERR_NOARGV);
	return(1);
}

static int
pre_dt(PRE_ARGS)
{

	if (NULL == mdoc->meta.date || mdoc->meta.os)
		mdoc_nmsg(mdoc, n, MANDOCERR_PROLOGOOO);

	if (mdoc->meta.title)
		mdoc_nmsg(mdoc, n, MANDOCERR_PROLOGREP);

	return(1);
}

static int
pre_os(PRE_ARGS)
{

	if (NULL == mdoc->meta.title || NULL == mdoc->meta.date)
		mdoc_nmsg(mdoc, n, MANDOCERR_PROLOGOOO);

	if (mdoc->meta.os)
		mdoc_nmsg(mdoc, n, MANDOCERR_PROLOGREP);

	return(1);
}

static int
pre_dd(PRE_ARGS)
{

	if (mdoc->meta.title || mdoc->meta.os)
		mdoc_nmsg(mdoc, n, MANDOCERR_PROLOGOOO);

	if (mdoc->meta.date)
		mdoc_nmsg(mdoc, n, MANDOCERR_PROLOGREP);

	return(1);
}


static int
post_bf(POST_ARGS)
{
	struct mdoc_node *np;
	enum mdocargt	  arg;

	/*
	 * Unlike other data pointers, these are "housed" by the HEAD
	 * element, which contains the goods.
	 */

	if (MDOC_HEAD != mdoc->last->type) {
		if (ENDBODY_NOT != mdoc->last->end) {
			assert(mdoc->last->pending);
			np = mdoc->last->pending->parent->head;
		} else if (MDOC_BLOCK != mdoc->last->type) {
			np = mdoc->last->parent->head;
		} else 
			np = mdoc->last->head;

		assert(np);
		assert(MDOC_HEAD == np->type);
		assert(MDOC_Bf == np->tok);
		return(1);
	}

	np = mdoc->last;
	assert(MDOC_BLOCK == np->parent->type);
	assert(MDOC_Bf == np->parent->tok);

	/* 
	 * Cannot have both argument and parameter.
	 * If neither is specified, let it through with a warning. 
	 */

	if (np->parent->args && np->child) {
		mdoc_nmsg(mdoc, np, MANDOCERR_SYNTARGVCOUNT);
		return(0);
	} else if (NULL == np->parent->args && NULL == np->child) {
		mdoc_nmsg(mdoc, np, MANDOCERR_FONTTYPE);
		return(1);
	}

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
		return(1);
	}

	/* Extract parameter into data. */

	if (0 == strcmp(np->child->string, "Em"))
		np->norm->Bf.font = FONT_Em;
	else if (0 == strcmp(np->child->string, "Li"))
		np->norm->Bf.font = FONT_Li;
	else if (0 == strcmp(np->child->string, "Sy"))
		np->norm->Bf.font = FONT_Sy;
	else 
		mdoc_nmsg(mdoc, np, MANDOCERR_FONTTYPE);

	return(1);
}

static int
post_lb(POST_ARGS)
{
	const char	*p;
	char		*buf;
	size_t		 sz;

	check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 1);

	assert(mdoc->last->child);
	assert(MDOC_TEXT == mdoc->last->child->type);

	p = mdoc_a2lib(mdoc->last->child->string);

	/* If lookup ok, replace with table value. */

	if (p) {
		free(mdoc->last->child->string);
		mdoc->last->child->string = mandoc_strdup(p);
		return(1);
	}

	/* If not, use "library ``xxxx''. */

	sz = strlen(mdoc->last->child->string) +
		2 + strlen("\\(lqlibrary\\(rq");
	buf = mandoc_malloc(sz);
	snprintf(buf, sz, "library \\(lq%s\\(rq", 
			mdoc->last->child->string);
	free(mdoc->last->child->string);
	mdoc->last->child->string = buf;
	return(1);
}

static int
post_eoln(POST_ARGS)
{

	if (mdoc->last->child)
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_ARGSLOST);
	return(1);
}


static int
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
		return(1);
	
	for (n = mdoc->last->child; n; n = n->next)
		if (MDOC_TEXT != n->type) 
			mdoc_nmsg(mdoc, n, MANDOCERR_CHILD);

	return(1);
}


static int
post_nm(POST_ARGS)
{
	char		 buf[BUFSIZ];
	int		 c;

	/* If no child specified, make sure we have the meta name. */

	if (NULL == mdoc->last->child && NULL == mdoc->meta.name) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_NONAME);
		return(1);
	} else if (mdoc->meta.name)
		return(1);

	/* If no meta name, set it from the child. */

	buf[0] = '\0';
	if (-1 == (c = concat(buf, mdoc->last->child, BUFSIZ))) {
		mdoc_nmsg(mdoc, mdoc->last->child, MANDOCERR_MEM);
		return(0);
	}

	assert(c);
	mdoc->meta.name = mandoc_strdup(buf);
	return(1);
}

static int
post_literal(POST_ARGS)
{
	
	/*
	 * The `Dl' (note "el" not "one") and `Bd' macros unset the
	 * MDOC_LITERAL flag as they leave.  Note that `Bd' only sets
	 * this in literal mode, but it doesn't hurt to just switch it
	 * off in general since displays can't be nested.
	 */

	if (MDOC_BODY == mdoc->last->type)
		mdoc->flags &= ~MDOC_LITERAL;

	return(1);
}

static int
post_defaults(POST_ARGS)
{
	struct mdoc_node *nn;

	/*
	 * The `Ar' defaults to "file ..." if no value is provided as an
	 * argument; the `Mt' and `Pa' macros use "~"; the `Li' just
	 * gets an empty string.
	 */

	if (mdoc->last->child)
		return(1);
	
	nn = mdoc->last;
	mdoc->next = MDOC_NEXT_CHILD;

	switch (nn->tok) {
	case (MDOC_Ar):
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "file"))
			return(0);
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "..."))
			return(0);
		break;
	case (MDOC_At):
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "AT&T"))
			return(0);
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "UNIX"))
			return(0);
		break;
	case (MDOC_Li):
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, ""))
			return(0);
		break;
	case (MDOC_Pa):
		/* FALLTHROUGH */
	case (MDOC_Mt):
		if ( ! mdoc_word_alloc(mdoc, nn->line, nn->pos, "~"))
			return(0);
		break;
	default:
		abort();
		/* NOTREACHED */
	} 

	mdoc->last = nn;
	return(1);
}

static int
post_at(POST_ARGS)
{
	const char	 *p, *q;
	char		 *buf;
	size_t		  sz;

	/*
	 * If we have a child, look it up in the standard keys.  If a
	 * key exist, use that instead of the child; if it doesn't,
	 * prefix "AT&T UNIX " to the existing data.
	 */
	
	if (NULL == mdoc->last->child)
		return(1);

	assert(MDOC_TEXT == mdoc->last->child->type);
	p = mdoc_a2att(mdoc->last->child->string);

	if (p) {
		free(mdoc->last->child->string);
		mdoc->last->child->string = mandoc_strdup(p);
	} else {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_BADATT);
		p = "AT&T UNIX ";
		q = mdoc->last->child->string;
		sz = strlen(p) + strlen(q) + 1;
		buf = mandoc_malloc(sz);
		strlcpy(buf, p, sz);
		strlcat(buf, q, sz);
		free(mdoc->last->child->string);
		mdoc->last->child->string = buf;
	}

	return(1);
}

static int
post_an(POST_ARGS)
{
	struct mdoc_node *np;

	np = mdoc->last;
	if (AUTH__NONE == np->norm->An.auth) {
		if (0 == np->child)
			check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_GT, 0);
	} else if (np->child)
		check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 0);

	return(1);
}


static int
post_it(POST_ARGS)
{
	int		  i, cols;
	enum mdoc_list	  lt;
	struct mdoc_node *n, *c;
	enum mandocerr	  er;

	if (MDOC_BLOCK != mdoc->last->type)
		return(1);

	n = mdoc->last->parent->parent;
	lt = n->norm->Bl.type;

	if (LIST__NONE == lt) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_LISTTYPE);
		return(1);
	}

	switch (lt) {
	case (LIST_tag):
		if (mdoc->last->head->child)
			break;
		/* FIXME: give this a dummy value. */
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_NOARGS);
		break;
	case (LIST_hang):
		/* FALLTHROUGH */
	case (LIST_ohang):
		/* FALLTHROUGH */
	case (LIST_inset):
		/* FALLTHROUGH */
	case (LIST_diag):
		if (NULL == mdoc->last->head->child)
			mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_NOARGS);
		break;
	case (LIST_bullet):
		/* FALLTHROUGH */
	case (LIST_dash):
		/* FALLTHROUGH */
	case (LIST_enum):
		/* FALLTHROUGH */
	case (LIST_hyphen):
		if (NULL == mdoc->last->body->child)
			mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_NOBODY);
		/* FALLTHROUGH */
	case (LIST_item):
		if (mdoc->last->head->child)
			mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_ARGSLOST);
		break;
	case (LIST_column):
		cols = (int)n->norm->Bl.ncols;

		assert(NULL == mdoc->last->head->child);

		if (NULL == mdoc->last->body->child)
			mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_NOBODY);

		for (i = 0, c = mdoc->last->child; c; c = c->next)
			if (MDOC_BODY == c->type)
				i++;

		if (i < cols)
			er = MANDOCERR_ARGCOUNT;
		else if (i == cols || i == cols + 1)
			break;
		else
			er = MANDOCERR_SYNTARGCOUNT;

		mandoc_vmsg(er, mdoc->parse, mdoc->last->line, 
				mdoc->last->pos, 
				"columns == %d (have %d)", cols, i);
		return(MANDOCERR_ARGCOUNT == er);
	default:
		break;
	}

	return(1);
}

static int
post_bl_block(POST_ARGS) 
{
	struct mdoc_node *n;

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
		if ( ! post_bl_block_tag(mdoc))
			return(0);
	} else if (NULL != n->norm->Bl.width) {
		if ( ! post_bl_block_width(mdoc))
			return(0);
	} else 
		return(1);

	assert(n->norm->Bl.width);
	return(1);
}

static int
post_bl_block_width(POST_ARGS)
{
	size_t		  width;
	int		  i;
	enum mdoct	  tok;
	struct mdoc_node *n;
	char		  buf[NUMSIZ];

	n = mdoc->last;

	/*
	 * Calculate the real width of a list from the -width string,
	 * which may contain a macro (with a known default width), a
	 * literal string, or a scaling width.
	 *
	 * If the value to -width is a macro, then we re-write it to be
	 * the macro's width as set in share/tmac/mdoc/doc-common.
	 */

	if (0 == strcmp(n->norm->Bl.width, "Ds"))
		width = 6;
	else if (MDOC_MAX == (tok = mdoc_hash_find(n->norm->Bl.width)))
		return(1);
	else if (0 == (width = macro2len(tok)))  {
		mdoc_nmsg(mdoc, n, MANDOCERR_BADWIDTH);
		return(1);
	}

	/* The value already exists: free and reallocate it. */

	assert(n->args);

	for (i = 0; i < (int)n->args->argc; i++) 
		if (MDOC_Width == n->args->argv[i].arg)
			break;

	assert(i < (int)n->args->argc);

	snprintf(buf, NUMSIZ, "%un", (unsigned int)width);
	free(n->args->argv[i].value[0]);
	n->args->argv[i].value[0] = mandoc_strdup(buf);

	/* Set our width! */
	n->norm->Bl.width = n->args->argv[i].value[0];
	return(1);
}

static int
post_bl_block_tag(POST_ARGS)
{
	struct mdoc_node *n, *nn;
	size_t		  sz, ssz;
	int		  i;
	char		  buf[NUMSIZ];

	/*
	 * Calculate the -width for a `Bl -tag' list if it hasn't been
	 * provided.  Uses the first head macro.  NOTE AGAIN: this is
	 * ONLY if the -width argument has NOT been provided.  See
	 * post_bl_block_width() for converting the -width string.
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

	snprintf(buf, NUMSIZ, "%un", (unsigned int)sz);

	/*
	 * We have to dynamically add this to the macro's argument list.
	 * We're guaranteed that a MDOC_Width doesn't already exist.
	 */

	assert(n->args);
	i = (int)(n->args->argc)++;

	n->args->argv = mandoc_realloc(n->args->argv, 
			n->args->argc * sizeof(struct mdoc_argv));

	n->args->argv[i].arg = MDOC_Width;
	n->args->argv[i].line = n->line;
	n->args->argv[i].pos = n->pos;
	n->args->argv[i].sz = 1;
	n->args->argv[i].value = mandoc_malloc(sizeof(char *));
	n->args->argv[i].value[0] = mandoc_strdup(buf);

	/* Set our width! */
	n->norm->Bl.width = n->args->argv[i].value[0];
	return(1);
}


static int
post_bl_head(POST_ARGS) 
{
	struct mdoc_node *np, *nn, *nnp;
	int		  i, j;

	if (LIST_column != mdoc->last->norm->Bl.type)
		/* FIXME: this should be ERROR class... */
		return(hwarn_eq0(mdoc));

	/*
	 * Convert old-style lists, where the column width specifiers
	 * trail as macro parameters, to the new-style ("normal-form")
	 * lists where they're argument values following -column.
	 */

	/* First, disallow both types and allow normal-form. */

	/* 
	 * TODO: technically, we can accept both and just merge the two
	 * lists, but I'll leave that for another day.
	 */

	if (mdoc->last->norm->Bl.ncols && mdoc->last->nchild) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_COLUMNS);
		return(0);
	} else if (NULL == mdoc->last->child)
		return(1);

	np = mdoc->last->parent;
	assert(np->args);

	for (j = 0; j < (int)np->args->argc; j++) 
		if (MDOC_Column == np->args->argv[j].arg)
			break;

	assert(j < (int)np->args->argc);
	assert(0 == np->args->argv[j].sz);

	/*
	 * Accommodate for new-style groff column syntax.  Shuffle the
	 * child nodes, all of which must be TEXT, as arguments for the
	 * column field.  Then, delete the head children.
	 */

	np->args->argv[j].sz = (size_t)mdoc->last->nchild;
	np->args->argv[j].value = mandoc_malloc
		((size_t)mdoc->last->nchild * sizeof(char *));

	mdoc->last->norm->Bl.ncols = np->args->argv[j].sz;
	mdoc->last->norm->Bl.cols = (void *)np->args->argv[j].value;

	for (i = 0, nn = mdoc->last->child; nn; i++) {
		np->args->argv[j].value[i] = nn->string;
		nn->string = NULL;
		nnp = nn;
		nn = nn->next;
		mdoc_node_delete(NULL, nnp);
	}

	mdoc->last->nchild = 0;
	mdoc->last->child = NULL;

	return(1);
}

static int
post_bl(POST_ARGS)
{
	struct mdoc_node	*n;

	if (MDOC_HEAD == mdoc->last->type) 
		return(post_bl_head(mdoc));
	if (MDOC_BLOCK == mdoc->last->type)
		return(post_bl_block(mdoc));
	if (MDOC_BODY != mdoc->last->type)
		return(1);

	for (n = mdoc->last->child; n; n = n->next) {
		switch (n->tok) {
		case (MDOC_Lp):
			/* FALLTHROUGH */
		case (MDOC_Pp):
			mdoc_nmsg(mdoc, n, MANDOCERR_CHILD);
			/* FALLTHROUGH */
		case (MDOC_It):
			/* FALLTHROUGH */
		case (MDOC_Sm):
			continue;
		default:
			break;
		}

		mdoc_nmsg(mdoc, n, MANDOCERR_SYNTCHILD);
		return(0);
	}

	return(1);
}

static int
ebool(struct mdoc *mdoc)
{

	if (NULL == mdoc->last->child) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_MACROEMPTY);
		mdoc_node_delete(mdoc, mdoc->last);
		return(1);
	}
	check_count(mdoc, MDOC_ELEM, CHECK_WARN, CHECK_EQ, 1);

	assert(MDOC_TEXT == mdoc->last->child->type);

	if (0 == strcmp(mdoc->last->child->string, "on"))
		return(1);
	if (0 == strcmp(mdoc->last->child->string, "off"))
		return(1);

	mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_BADBOOL);
	return(1);
}

static int
post_root(POST_ARGS)
{
	int		  erc;
	struct mdoc_node *n;

	erc = 0;

	/* Check that we have a finished prologue. */

	if ( ! (MDOC_PBODY & mdoc->flags)) {
		erc++;
		mdoc_nmsg(mdoc, mdoc->first, MANDOCERR_NODOCPROLOG);
	}

	n = mdoc->first;
	assert(n);
	
	/* Check that we begin with a proper `Sh'. */

	if (NULL == n->child) {
		erc++;
		mdoc_nmsg(mdoc, n, MANDOCERR_NODOCBODY);
	} else if (MDOC_BLOCK != n->child->type || 
			MDOC_Sh != n->child->tok) {
		erc++;
		/* Can this be lifted?  See rxdebug.1 for example. */
		mdoc_nmsg(mdoc, n, MANDOCERR_NODOCBODY);
	}

	return(erc ? 0 : 1);
}

static int
post_st(POST_ARGS)
{
	struct mdoc_node	 *ch;
	const char		 *p;

	if (NULL == (ch = mdoc->last->child)) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_MACROEMPTY);
		mdoc_node_delete(mdoc, mdoc->last);
		return(1);
	}

	assert(MDOC_TEXT == ch->type);

	if (NULL == (p = mdoc_a2st(ch->string))) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_BADSTANDARD);
		mdoc_node_delete(mdoc, mdoc->last);
	} else {
		free(ch->string);
		ch->string = mandoc_strdup(p);
	}

	return(1);
}

static int
post_rs(POST_ARGS)
{
	struct mdoc_node *nn, *next, *prev;
	int		  i, j;

	switch (mdoc->last->type) {
	case (MDOC_HEAD):
		check_count(mdoc, MDOC_HEAD, CHECK_WARN, CHECK_EQ, 0);
		return(1);
	case (MDOC_BODY):
		if (mdoc->last->child)
			break;
		check_count(mdoc, MDOC_BODY, CHECK_WARN, CHECK_GT, 0);
		return(1);
	default:
		return(1);
	}

	/*
	 * Make sure only certain types of nodes are allowed within the
	 * the `Rs' body.  Delete offending nodes and raise a warning.
	 * Do this before re-ordering for the sake of clarity.
	 */

	next = NULL;
	for (nn = mdoc->last->child; nn; nn = next) {
		for (i = 0; i < RSORD_MAX; i++)
			if (nn->tok == rsord[i])
				break;

		if (i < RSORD_MAX) {
			if (MDOC__J == rsord[i] || MDOC__B == rsord[i])
				mdoc->last->norm->Rs.quote_T++;
			next = nn->next;
			continue;
		}

		next = nn->next;
		mdoc_nmsg(mdoc, nn, MANDOCERR_CHILD);
		mdoc_node_delete(mdoc, nn);
	}

	/*
	 * Nothing to sort if only invalid nodes were found
	 * inside the `Rs' body.
	 */

	if (NULL == mdoc->last->child)
		return(1);

	/*
	 * The full `Rs' block needs special handling to order the
	 * sub-elements according to `rsord'.  Pick through each element
	 * and correctly order it.  This is a insertion sort.
	 */

	next = NULL;
	for (nn = mdoc->last->child->next; nn; nn = next) {
		/* Determine order of `nn'. */
		for (i = 0; i < RSORD_MAX; i++)
			if (rsord[i] == nn->tok)
				break;

		/* 
		 * Remove `nn' from the chain.  This somewhat
		 * repeats mdoc_node_unlink(), but since we're
		 * just re-ordering, there's no need for the
		 * full unlink process.
		 */
		
		if (NULL != (next = nn->next))
			next->prev = nn->prev;

		if (NULL != (prev = nn->prev))
			prev->next = nn->next;

		nn->prev = nn->next = NULL;

		/* 
		 * Scan back until we reach a node that's
		 * ordered before `nn'.
		 */

		for ( ; prev ; prev = prev->prev) {
			/* Determine order of `prev'. */
			for (j = 0; j < RSORD_MAX; j++)
				if (rsord[j] == prev->tok)
					break;

			if (j <= i)
				break;
		}

		/*
		 * Set `nn' back into its correct place in front
		 * of the `prev' node.
		 */

		nn->prev = prev;

		if (prev) {
			if (prev->next)
				prev->next->prev = nn;
			nn->next = prev->next;
			prev->next = nn;
		} else {
			mdoc->last->child->prev = nn;
			nn->next = mdoc->last->child;
			mdoc->last->child = nn;
		}
	}

	return(1);
}

static int
post_ns(POST_ARGS)
{

	if (MDOC_LINE & mdoc->last->flags)
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_IGNNS);
	return(1);
}

static int
post_sh(POST_ARGS)
{

	if (MDOC_HEAD == mdoc->last->type)
		return(post_sh_head(mdoc));
	if (MDOC_BODY == mdoc->last->type)
		return(post_sh_body(mdoc));

	return(1);
}

static int
post_sh_body(POST_ARGS)
{
	struct mdoc_node *n;

	if (SEC_NAME != mdoc->lastsec)
		return(1);

	/*
	 * Warn if the NAME section doesn't contain the `Nm' and `Nd'
	 * macros (can have multiple `Nm' and one `Nd').  Note that the
	 * children of the BODY declaration can also be "text".
	 */

	if (NULL == (n = mdoc->last->child)) {
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_BADNAMESEC);
		return(1);
	}

	for ( ; n && n->next; n = n->next) {
		if (MDOC_ELEM == n->type && MDOC_Nm == n->tok)
			continue;
		if (MDOC_TEXT == n->type)
			continue;
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_BADNAMESEC);
	}

	assert(n);
	if (MDOC_BLOCK == n->type && MDOC_Nd == n->tok)
		return(1);

	mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_BADNAMESEC);
	return(1);
}

static int
post_sh_head(POST_ARGS)
{
	char		 buf[BUFSIZ];
	struct mdoc_node *n;
	enum mdoc_sec	 sec;
	int		 c;

	/*
	 * Process a new section.  Sections are either "named" or
	 * "custom".  Custom sections are user-defined, while named ones
	 * follow a conventional order and may only appear in certain
	 * manual sections.
	 */

	sec = SEC_CUSTOM;
	buf[0] = '\0';
	if (-1 == (c = concat(buf, mdoc->last->child, BUFSIZ))) {
		mdoc_nmsg(mdoc, mdoc->last->child, MANDOCERR_MEM);
		return(0);
	} else if (1 == c)
		sec = a2sec(buf);

	/* The NAME should be first. */

	if (SEC_NAME != sec && SEC_NONE == mdoc->lastnamed)
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_NAMESECFIRST);

	/* The SYNOPSIS gets special attention in other areas. */

	if (SEC_SYNOPSIS == sec)
		mdoc->flags |= MDOC_SYNOPSIS;
	else
		mdoc->flags &= ~MDOC_SYNOPSIS;

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

	if (SEC_CUSTOM == sec)
		return(1);

	/*
	 * Check whether our non-custom section is being repeated or is
	 * out of order.
	 */

	if (sec == mdoc->lastnamed)
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_SECREP);

	if (sec < mdoc->lastnamed)
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_SECOOO);

	/* Mark the last named section. */

	mdoc->lastnamed = sec;

	/* Check particular section/manual conventions. */

	assert(mdoc->meta.msec);

	switch (sec) {
	case (SEC_RETURN_VALUES):
		/* FALLTHROUGH */
	case (SEC_ERRORS):
		/* FALLTHROUGH */
	case (SEC_LIBRARY):
		if (*mdoc->meta.msec == '2')
			break;
		if (*mdoc->meta.msec == '3')
			break;
		if (*mdoc->meta.msec == '9')
			break;
		mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_SECMSEC);
		break;
	default:
		break;
	}

	return(1);
}

static int
post_ignpar(POST_ARGS)
{
	struct mdoc_node *np;

	if (MDOC_BODY != mdoc->last->type)
		return(1);

	if (NULL != (np = mdoc->last->child))
		if (MDOC_Pp == np->tok || MDOC_Lp == np->tok) {
			mdoc_nmsg(mdoc, np, MANDOCERR_IGNPAR);
			mdoc_node_delete(mdoc, np);
		}

	if (NULL != (np = mdoc->last->last))
		if (MDOC_Pp == np->tok || MDOC_Lp == np->tok) {
			mdoc_nmsg(mdoc, np, MANDOCERR_IGNPAR);
			mdoc_node_delete(mdoc, np);
		}

	return(1);
}

static int
pre_par(PRE_ARGS)
{

	if (NULL == mdoc->last)
		return(1);
	if (MDOC_ELEM != n->type && MDOC_BLOCK != n->type)
		return(1);

	/* 
	 * Don't allow prior `Lp' or `Pp' prior to a paragraph-type
	 * block:  `Lp', `Pp', or non-compact `Bd' or `Bl'.
	 */

	if (MDOC_Pp != mdoc->last->tok && MDOC_Lp != mdoc->last->tok)
		return(1);
	if (MDOC_Bl == n->tok && n->norm->Bl.comp)
		return(1);
	if (MDOC_Bd == n->tok && n->norm->Bd.comp)
		return(1);
	if (MDOC_It == n->tok && n->parent->norm->Bl.comp)
		return(1);

	mdoc_nmsg(mdoc, mdoc->last, MANDOCERR_IGNPAR);
	mdoc_node_delete(mdoc, mdoc->last);
	return(1);
}

static int
pre_literal(PRE_ARGS)
{

	if (MDOC_BODY != n->type)
		return(1);

	/*
	 * The `Dl' (note "el" not "one") and `Bd -literal' and `Bd
	 * -unfilled' macros set MDOC_LITERAL on entrance to the body.
	 */

	switch (n->tok) {
	case (MDOC_Dl):
		mdoc->flags |= MDOC_LITERAL;
		break;
	case (MDOC_Bd):
		if (DISP_literal == n->norm->Bd.type)
			mdoc->flags |= MDOC_LITERAL;
		if (DISP_unfilled == n->norm->Bd.type)
			mdoc->flags |= MDOC_LITERAL;
		break;
	default:
		abort();
		/* NOTREACHED */
	}
	
	return(1);
}

static int
post_dd(POST_ARGS)
{
	char		  buf[DATESIZE];
	struct mdoc_node *n;
	int		  c;

	if (mdoc->meta.date)
		free(mdoc->meta.date);

	n = mdoc->last;
	if (NULL == n->child || '\0' == n->child->string[0]) {
		mdoc->meta.date = mandoc_normdate
			(mdoc->parse, NULL, n->line, n->pos);
		return(1);
	}

	buf[0] = '\0';
	if (-1 == (c = concat(buf, n->child, DATESIZE))) {
		mdoc_nmsg(mdoc, n->child, MANDOCERR_MEM);
		return(0);
	}

	assert(c);
	mdoc->meta.date = mandoc_normdate
		(mdoc->parse, buf, n->line, n->pos);

	return(1);
}

static int
post_dt(POST_ARGS)
{
	struct mdoc_node *nn, *n;
	const char	 *cp;
	char		 *p;

	n = mdoc->last;

	if (mdoc->meta.title)
		free(mdoc->meta.title);
	if (mdoc->meta.vol)
		free(mdoc->meta.vol);
	if (mdoc->meta.arch)
		free(mdoc->meta.arch);

	mdoc->meta.title = mdoc->meta.vol = mdoc->meta.arch = NULL;

	/* First make all characters uppercase. */

	if (NULL != (nn = n->child))
		for (p = nn->string; *p; p++) {
			if (toupper((unsigned char)*p) == *p)
				continue;

			/* 
			 * FIXME: don't be lazy: have this make all
			 * characters be uppercase and just warn once.
			 */
			mdoc_nmsg(mdoc, nn, MANDOCERR_UPPERCASE);
			break;
		}

	/* Handles: `.Dt' 
	 *   --> title = unknown, volume = local, msec = 0, arch = NULL
	 */

	if (NULL == (nn = n->child)) {
		/* XXX: make these macro values. */
		/* FIXME: warn about missing values. */
		mdoc->meta.title = mandoc_strdup("UNKNOWN");
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		mdoc->meta.msec = mandoc_strdup("1");
		return(1);
	}

	/* Handles: `.Dt TITLE' 
	 *   --> title = TITLE, volume = local, msec = 0, arch = NULL
	 */

	mdoc->meta.title = mandoc_strdup
		('\0' == nn->string[0] ? "UNKNOWN" : nn->string);

	if (NULL == (nn = nn->next)) {
		/* FIXME: warn about missing msec. */
		/* XXX: make this a macro value. */
		mdoc->meta.vol = mandoc_strdup("LOCAL");
		mdoc->meta.msec = mandoc_strdup("1");
		return(1);
	}

	/* Handles: `.Dt TITLE SEC'
	 *   --> title = TITLE, volume = SEC is msec ? 
	 *           format(msec) : SEC,
	 *       msec = SEC is msec ? atoi(msec) : 0,
	 *       arch = NULL
	 */

	cp = mandoc_a2msec(nn->string);
	if (cp) {
		mdoc->meta.vol = mandoc_strdup(cp);
		mdoc->meta.msec = mandoc_strdup(nn->string);
	} else {
		mdoc_nmsg(mdoc, n, MANDOCERR_BADMSEC);
		mdoc->meta.vol = mandoc_strdup(nn->string);
		mdoc->meta.msec = mandoc_strdup(nn->string);
	} 

	if (NULL == (nn = nn->next))
		return(1);

	/* Handles: `.Dt TITLE SEC VOL'
	 *   --> title = TITLE, volume = VOL is vol ?
	 *       format(VOL) : 
	 *           VOL is arch ? format(arch) : 
	 *               VOL
	 */

	cp = mdoc_a2vol(nn->string);
	if (cp) {
		free(mdoc->meta.vol);
		mdoc->meta.vol = mandoc_strdup(cp);
	} else {
		/* FIXME: warn about bad arch. */
		cp = mdoc_a2arch(nn->string);
		if (NULL == cp) {
			free(mdoc->meta.vol);
			mdoc->meta.vol = mandoc_strdup(nn->string);
		} else 
			mdoc->meta.arch = mandoc_strdup(cp);
	}	

	/* Ignore any subsequent parameters... */
	/* FIXME: warn about subsequent parameters. */

	return(1);
}

static int
post_prol(POST_ARGS)
{
	/*
	 * Remove prologue macros from the document after they're
	 * processed.  The final document uses mdoc_meta for these
	 * values and discards the originals.
	 */

	mdoc_node_delete(mdoc, mdoc->last);
	if (mdoc->meta.title && mdoc->meta.date && mdoc->meta.os)
		mdoc->flags |= MDOC_PBODY;

	return(1);
}

static int
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
		*n->string = (char)toupper
			((unsigned char)*n->string);

	return(1);
}

static int
post_os(POST_ARGS)
{
	struct mdoc_node *n;
	char		  buf[BUFSIZ];
	int		  c;
#ifndef OSNAME
	struct utsname	  utsname;
#endif

	n = mdoc->last;

	/*
	 * Set the operating system by way of the `Os' macro.  Note that
	 * if an argument isn't provided and -DOSNAME="\"foo\"" is
	 * provided during compilation, this value will be used instead
	 * of filling in "sysname release" from uname().
 	 */

	if (mdoc->meta.os)
		free(mdoc->meta.os);

	buf[0] = '\0';
	if (-1 == (c = concat(buf, n->child, BUFSIZ))) {
		mdoc_nmsg(mdoc, n->child, MANDOCERR_MEM);
		return(0);
	}

	assert(c);

	/* XXX: yes, these can all be dynamically-adjusted buffers, but
	 * it's really not worth the extra hackery.
	 */

	if ('\0' == buf[0]) {
#ifdef OSNAME
		if (strlcat(buf, OSNAME, BUFSIZ) >= BUFSIZ) {
			mdoc_nmsg(mdoc, n, MANDOCERR_MEM);
			return(0);
		}
#else /*!OSNAME */
		if (-1 == uname(&utsname)) {
			mdoc_nmsg(mdoc, n, MANDOCERR_UNAME);
                        mdoc->meta.os = mandoc_strdup("UNKNOWN");
                        return(post_prol(mdoc));
                }

		if (strlcat(buf, utsname.sysname, BUFSIZ) >= BUFSIZ) {
			mdoc_nmsg(mdoc, n, MANDOCERR_MEM);
			return(0);
		}
		if (strlcat(buf, " ", BUFSIZ) >= BUFSIZ) {
			mdoc_nmsg(mdoc, n, MANDOCERR_MEM);
			return(0);
		}
		if (0 == strcmp(utsname.sysname, "FreeBSD"))
			strtok(utsname.release, "-");
		if (strlcat(buf, utsname.release, BUFSIZ) >= BUFSIZ) {
			mdoc_nmsg(mdoc, n, MANDOCERR_MEM);
			return(0);
		}
#endif /*!OSNAME*/
	}

	mdoc->meta.os = mandoc_strdup(buf);
	return(1);
}

static int
post_std(POST_ARGS)
{
	struct mdoc_node *nn, *n;

	n = mdoc->last;

	/*
	 * Macros accepting `-std' as an argument have the name of the
	 * current document (`Nm') filled in as the argument if it's not
	 * provided.
	 */

	if (n->child)
		return(1);

	if (NULL == mdoc->meta.name)
		return(1);
	
	nn = n;
	mdoc->next = MDOC_NEXT_CHILD;

	if ( ! mdoc_word_alloc(mdoc, n->line, n->pos, mdoc->meta.name))
		return(0);

	mdoc->last = nn;
	return(1);
}

/*
 * Concatenate a node, stopping at the first non-text.
 * Concatenation is separated by a single whitespace.  
 * Returns -1 on fatal (string overrun) error, 0 if child nodes were
 * encountered, 1 otherwise.
 */
static int
concat(char *p, const struct mdoc_node *n, size_t sz)
{

	for ( ; NULL != n; n = n->next) {
		if (MDOC_TEXT != n->type) 
			return(0);
		if ('\0' != p[0] && strlcat(p, " ", sz) >= sz)
			return(-1);
		if (strlcat(p, n->string, sz) >= sz)
			return(-1);
		concat(p, n->child, sz);
	}

	return(1);
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
	case(MDOC_Ad):
		return(12);
	case(MDOC_Ao):
		return(12);
	case(MDOC_An):
		return(12);
	case(MDOC_Aq):
		return(12);
	case(MDOC_Ar):
		return(12);
	case(MDOC_Bo):
		return(12);
	case(MDOC_Bq):
		return(12);
	case(MDOC_Cd):
		return(12);
	case(MDOC_Cm):
		return(10);
	case(MDOC_Do):
		return(10);
	case(MDOC_Dq):
		return(12);
	case(MDOC_Dv):
		return(12);
	case(MDOC_Eo):
		return(12);
	case(MDOC_Em):
		return(10);
	case(MDOC_Er):
		return(17);
	case(MDOC_Ev):
		return(15);
	case(MDOC_Fa):
		return(12);
	case(MDOC_Fl):
		return(10);
	case(MDOC_Fo):
		return(16);
	case(MDOC_Fn):
		return(16);
	case(MDOC_Ic):
		return(10);
	case(MDOC_Li):
		return(16);
	case(MDOC_Ms):
		return(6);
	case(MDOC_Nm):
		return(10);
	case(MDOC_No):
		return(12);
	case(MDOC_Oo):
		return(10);
	case(MDOC_Op):
		return(14);
	case(MDOC_Pa):
		return(32);
	case(MDOC_Pf):
		return(12);
	case(MDOC_Po):
		return(12);
	case(MDOC_Pq):
		return(12);
	case(MDOC_Ql):
		return(16);
	case(MDOC_Qo):
		return(12);
	case(MDOC_So):
		return(12);
	case(MDOC_Sq):
		return(12);
	case(MDOC_Sy):
		return(6);
	case(MDOC_Sx):
		return(16);
	case(MDOC_Tn):
		return(10);
	case(MDOC_Va):
		return(12);
	case(MDOC_Vt):
		return(12);
	case(MDOC_Xr):
		return(10);
	default:
		break;
	};
	return(0);
}
