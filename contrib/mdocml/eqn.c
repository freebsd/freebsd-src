/*	$Id: eqn.c,v 1.38 2011/07/25 15:37:00 kristaps Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "libroff.h"

#define	EQN_NEST_MAX	 128 /* maximum nesting of defines */
#define	EQN_MSG(t, x)	 mandoc_msg((t), (x)->parse, (x)->eqn.ln, (x)->eqn.pos, NULL)

enum	eqn_rest {
	EQN_DESCOPE,
	EQN_ERR,
	EQN_OK,
	EQN_EOF
};

enum	eqn_symt {
	EQNSYM_alpha,
	EQNSYM_beta,
	EQNSYM_chi,
	EQNSYM_delta,
	EQNSYM_epsilon,
	EQNSYM_eta,
	EQNSYM_gamma,
	EQNSYM_iota,
	EQNSYM_kappa,
	EQNSYM_lambda,
	EQNSYM_mu,
	EQNSYM_nu,
	EQNSYM_omega,
	EQNSYM_omicron,
	EQNSYM_phi,
	EQNSYM_pi,
	EQNSYM_ps,
	EQNSYM_rho,
	EQNSYM_sigma,
	EQNSYM_tau,
	EQNSYM_theta,
	EQNSYM_upsilon,
	EQNSYM_xi,
	EQNSYM_zeta,
	EQNSYM_DELTA,
	EQNSYM_GAMMA,
	EQNSYM_LAMBDA,
	EQNSYM_OMEGA,
	EQNSYM_PHI,
	EQNSYM_PI,
	EQNSYM_PSI,
	EQNSYM_SIGMA,
	EQNSYM_THETA,
	EQNSYM_UPSILON,
	EQNSYM_XI,
	EQNSYM_inter,
	EQNSYM_union,
	EQNSYM_prod,
	EQNSYM_int,
	EQNSYM_sum,
	EQNSYM_grad,
	EQNSYM_del,
	EQNSYM_times,
	EQNSYM_cdot,
	EQNSYM_nothing,
	EQNSYM_approx,
	EQNSYM_prime,
	EQNSYM_half,
	EQNSYM_partial,
	EQNSYM_inf,
	EQNSYM_muchgreat,
	EQNSYM_muchless,
	EQNSYM_larrow,
	EQNSYM_rarrow,
	EQNSYM_pm,
	EQNSYM_nequal,
	EQNSYM_equiv,
	EQNSYM_lessequal,
	EQNSYM_moreequal,
	EQNSYM__MAX
};

enum	eqnpartt {
	EQN_DEFINE = 0,
	EQN_NDEFINE,
	EQN_TDEFINE,
	EQN_SET,
	EQN_UNDEF,
	EQN_GFONT,
	EQN_GSIZE,
	EQN_BACK,
	EQN_FWD,
	EQN_UP,
	EQN_DOWN,
	EQN__MAX
};

struct	eqnstr {
	const char	*name;
	size_t		 sz;
};

#define	STRNEQ(p1, sz1, p2, sz2) \
	((sz1) == (sz2) && 0 == strncmp((p1), (p2), (sz1)))
#define	EQNSTREQ(x, p, sz) \
	STRNEQ((x)->name, (x)->sz, (p), (sz))

struct	eqnpart {
	struct eqnstr	 str;
	int		(*fp)(struct eqn_node *);
};

struct	eqnsym {
	struct eqnstr	 str;
	const char	*sym;
};


static	enum eqn_rest	 eqn_box(struct eqn_node *, struct eqn_box *);
static	struct eqn_box	*eqn_box_alloc(struct eqn_node *, 
				struct eqn_box *);
static	void		 eqn_box_free(struct eqn_box *);
static	struct eqn_def	*eqn_def_find(struct eqn_node *, 
				const char *, size_t);
static	int		 eqn_do_gfont(struct eqn_node *);
static	int		 eqn_do_gsize(struct eqn_node *);
static	int		 eqn_do_define(struct eqn_node *);
static	int		 eqn_do_ign1(struct eqn_node *);
static	int		 eqn_do_ign2(struct eqn_node *);
static	int		 eqn_do_tdefine(struct eqn_node *);
static	int		 eqn_do_undef(struct eqn_node *);
static	enum eqn_rest	 eqn_eqn(struct eqn_node *, struct eqn_box *);
static	enum eqn_rest	 eqn_list(struct eqn_node *, struct eqn_box *);
static	enum eqn_rest	 eqn_matrix(struct eqn_node *, struct eqn_box *);
static	const char	*eqn_nexttok(struct eqn_node *, size_t *);
static	const char	*eqn_nextrawtok(struct eqn_node *, size_t *);
static	const char	*eqn_next(struct eqn_node *, 
				char, size_t *, int);
static	void		 eqn_rewind(struct eqn_node *);

static	const struct eqnpart eqnparts[EQN__MAX] = {
	{ { "define", 6 }, eqn_do_define }, /* EQN_DEFINE */
	{ { "ndefine", 7 }, eqn_do_define }, /* EQN_NDEFINE */
	{ { "tdefine", 7 }, eqn_do_tdefine }, /* EQN_TDEFINE */
	{ { "set", 3 }, eqn_do_ign2 }, /* EQN_SET */
	{ { "undef", 5 }, eqn_do_undef }, /* EQN_UNDEF */
	{ { "gfont", 5 }, eqn_do_gfont }, /* EQN_GFONT */
	{ { "gsize", 5 }, eqn_do_gsize }, /* EQN_GSIZE */
	{ { "back", 4 }, eqn_do_ign1 }, /* EQN_BACK */
	{ { "fwd", 3 }, eqn_do_ign1 }, /* EQN_FWD */
	{ { "up", 2 }, eqn_do_ign1 }, /* EQN_UP */
	{ { "down", 4 }, eqn_do_ign1 }, /* EQN_DOWN */
};

static	const struct eqnstr eqnmarks[EQNMARK__MAX] = {
	{ "", 0 }, /* EQNMARK_NONE */
	{ "dot", 3 }, /* EQNMARK_DOT */
	{ "dotdot", 6 }, /* EQNMARK_DOTDOT */
	{ "hat", 3 }, /* EQNMARK_HAT */
	{ "tilde", 5 }, /* EQNMARK_TILDE */
	{ "vec", 3 }, /* EQNMARK_VEC */
	{ "dyad", 4 }, /* EQNMARK_DYAD */
	{ "bar", 3 }, /* EQNMARK_BAR */
	{ "under", 5 }, /* EQNMARK_UNDER */
};

static	const struct eqnstr eqnfonts[EQNFONT__MAX] = {
	{ "", 0 }, /* EQNFONT_NONE */
	{ "roman", 5 }, /* EQNFONT_ROMAN */
	{ "bold", 4 }, /* EQNFONT_BOLD */
	{ "fat", 3 }, /* EQNFONT_FAT */
	{ "italic", 6 }, /* EQNFONT_ITALIC */
};

static	const struct eqnstr eqnposs[EQNPOS__MAX] = {
	{ "", 0 }, /* EQNPOS_NONE */
	{ "over", 4 }, /* EQNPOS_OVER */
	{ "sup", 3 }, /* EQNPOS_SUP */
	{ "sub", 3 }, /* EQNPOS_SUB */
	{ "to", 2 }, /* EQNPOS_TO */
	{ "from", 4 }, /* EQNPOS_FROM */
};

static	const struct eqnstr eqnpiles[EQNPILE__MAX] = {
	{ "", 0 }, /* EQNPILE_NONE */
	{ "pile", 4 }, /* EQNPILE_PILE */
	{ "cpile", 5 }, /* EQNPILE_CPILE */
	{ "rpile", 5 }, /* EQNPILE_RPILE */
	{ "lpile", 5 }, /* EQNPILE_LPILE */
	{ "col", 3 }, /* EQNPILE_COL */
	{ "ccol", 4 }, /* EQNPILE_CCOL */
	{ "rcol", 4 }, /* EQNPILE_RCOL */
	{ "lcol", 4 }, /* EQNPILE_LCOL */
};

static	const struct eqnsym eqnsyms[EQNSYM__MAX] = {
	{ { "alpha", 5 }, "*a" }, /* EQNSYM_alpha */
	{ { "beta", 4 }, "*b" }, /* EQNSYM_beta */
	{ { "chi", 3 }, "*x" }, /* EQNSYM_chi */
	{ { "delta", 5 }, "*d" }, /* EQNSYM_delta */
	{ { "epsilon", 7 }, "*e" }, /* EQNSYM_epsilon */
	{ { "eta", 3 }, "*y" }, /* EQNSYM_eta */
	{ { "gamma", 5 }, "*g" }, /* EQNSYM_gamma */
	{ { "iota", 4 }, "*i" }, /* EQNSYM_iota */
	{ { "kappa", 5 }, "*k" }, /* EQNSYM_kappa */
	{ { "lambda", 6 }, "*l" }, /* EQNSYM_lambda */
	{ { "mu", 2 }, "*m" }, /* EQNSYM_mu */
	{ { "nu", 2 }, "*n" }, /* EQNSYM_nu */
	{ { "omega", 5 }, "*w" }, /* EQNSYM_omega */
	{ { "omicron", 7 }, "*o" }, /* EQNSYM_omicron */
	{ { "phi", 3 }, "*f" }, /* EQNSYM_phi */
	{ { "pi", 2 }, "*p" }, /* EQNSYM_pi */
	{ { "psi", 2 }, "*q" }, /* EQNSYM_psi */
	{ { "rho", 3 }, "*r" }, /* EQNSYM_rho */
	{ { "sigma", 5 }, "*s" }, /* EQNSYM_sigma */
	{ { "tau", 3 }, "*t" }, /* EQNSYM_tau */
	{ { "theta", 5 }, "*h" }, /* EQNSYM_theta */
	{ { "upsilon", 7 }, "*u" }, /* EQNSYM_upsilon */
	{ { "xi", 2 }, "*c" }, /* EQNSYM_xi */
	{ { "zeta", 4 }, "*z" }, /* EQNSYM_zeta */
	{ { "DELTA", 5 }, "*D" }, /* EQNSYM_DELTA */
	{ { "GAMMA", 5 }, "*G" }, /* EQNSYM_GAMMA */
	{ { "LAMBDA", 6 }, "*L" }, /* EQNSYM_LAMBDA */
	{ { "OMEGA", 5 }, "*W" }, /* EQNSYM_OMEGA */
	{ { "PHI", 3 }, "*F" }, /* EQNSYM_PHI */
	{ { "PI", 2 }, "*P" }, /* EQNSYM_PI */
	{ { "PSI", 3 }, "*Q" }, /* EQNSYM_PSI */
	{ { "SIGMA", 5 }, "*S" }, /* EQNSYM_SIGMA */
	{ { "THETA", 5 }, "*H" }, /* EQNSYM_THETA */
	{ { "UPSILON", 7 }, "*U" }, /* EQNSYM_UPSILON */
	{ { "XI", 2 }, "*C" }, /* EQNSYM_XI */
	{ { "inter", 5 }, "ca" }, /* EQNSYM_inter */
	{ { "union", 5 }, "cu" }, /* EQNSYM_union */
	{ { "prod", 4 }, "product" }, /* EQNSYM_prod */
	{ { "int", 3 }, "integral" }, /* EQNSYM_int */
	{ { "sum", 3 }, "sum" }, /* EQNSYM_sum */
	{ { "grad", 4 }, "gr" }, /* EQNSYM_grad */
	{ { "del", 3 }, "gr" }, /* EQNSYM_del */
	{ { "times", 5 }, "mu" }, /* EQNSYM_times */
	{ { "cdot", 4 }, "pc" }, /* EQNSYM_cdot */
	{ { "nothing", 7 }, "&" }, /* EQNSYM_nothing */
	{ { "approx", 6 }, "~~" }, /* EQNSYM_approx */
	{ { "prime", 5 }, "aq" }, /* EQNSYM_prime */
	{ { "half", 4 }, "12" }, /* EQNSYM_half */
	{ { "partial", 7 }, "pd" }, /* EQNSYM_partial */
	{ { "inf", 3 }, "if" }, /* EQNSYM_inf */
	{ { ">>", 2 }, ">>" }, /* EQNSYM_muchgreat */
	{ { "<<", 2 }, "<<" }, /* EQNSYM_muchless */
	{ { "<-", 2 }, "<-" }, /* EQNSYM_larrow */
	{ { "->", 2 }, "->" }, /* EQNSYM_rarrow */
	{ { "+-", 2 }, "+-" }, /* EQNSYM_pm */
	{ { "!=", 2 }, "!=" }, /* EQNSYM_nequal */
	{ { "==", 2 }, "==" }, /* EQNSYM_equiv */
	{ { "<=", 2 }, "<=" }, /* EQNSYM_lessequal */
	{ { ">=", 2 }, ">=" }, /* EQNSYM_moreequal */
};

/* ARGSUSED */
enum rofferr
eqn_read(struct eqn_node **epp, int ln, 
		const char *p, int pos, int *offs)
{
	size_t		 sz;
	struct eqn_node	*ep;
	enum rofferr	 er;

	ep = *epp;

	/*
	 * If we're the terminating mark, unset our equation status and
	 * validate the full equation.
	 */

	if (0 == strncmp(p, ".EN", 3)) {
		er = eqn_end(epp);
		p += 3;
		while (' ' == *p || '\t' == *p)
			p++;
		if ('\0' == *p) 
			return(er);
		mandoc_msg(MANDOCERR_ARGSLOST, ep->parse, ln, pos, NULL);
		return(er);
	}

	/*
	 * Build up the full string, replacing all newlines with regular
	 * whitespace.
	 */

	sz = strlen(p + pos) + 1;
	ep->data = mandoc_realloc(ep->data, ep->sz + sz + 1);

	/* First invocation: nil terminate the string. */

	if (0 == ep->sz)
		*ep->data = '\0';

	ep->sz += sz;
	strlcat(ep->data, p + pos, ep->sz + 1);
	strlcat(ep->data, " ", ep->sz + 1);
	return(ROFF_IGN);
}

struct eqn_node *
eqn_alloc(const char *name, int pos, int line, struct mparse *parse)
{
	struct eqn_node	*p;
	size_t		 sz;
	const char	*end;

	p = mandoc_calloc(1, sizeof(struct eqn_node));

	if (name && '\0' != *name) {
		sz = strlen(name);
		assert(sz);
		do {
			sz--;
			end = name + (int)sz;
		} while (' ' == *end || '\t' == *end);
		p->eqn.name = mandoc_strndup(name, sz + 1);
	}

	p->parse = parse;
	p->eqn.ln = line;
	p->eqn.pos = pos;
	p->gsize = EQN_DEFSIZE;

	return(p);
}

enum rofferr
eqn_end(struct eqn_node **epp)
{
	struct eqn_node	*ep;
	struct eqn_box	*root;
	enum eqn_rest	 c;

	ep = *epp;
	*epp = NULL;

	ep->eqn.root = mandoc_calloc(1, sizeof(struct eqn_box));

	root = ep->eqn.root;
	root->type = EQN_ROOT;

	if (0 == ep->sz)
		return(ROFF_IGN);

	if (EQN_DESCOPE == (c = eqn_eqn(ep, root))) {
		EQN_MSG(MANDOCERR_EQNNSCOPE, ep);
		c = EQN_ERR;
	}

	return(EQN_EOF == c ? ROFF_EQN : ROFF_IGN);
}

static enum eqn_rest
eqn_eqn(struct eqn_node *ep, struct eqn_box *last)
{
	struct eqn_box	*bp;
	enum eqn_rest	 c;

	bp = eqn_box_alloc(ep, last);
	bp->type = EQN_SUBEXPR;

	while (EQN_OK == (c = eqn_box(ep, bp)))
		/* Spin! */ ;

	return(c);
}

static enum eqn_rest
eqn_matrix(struct eqn_node *ep, struct eqn_box *last)
{
	struct eqn_box	*bp;
	const char	*start;
	size_t		 sz;
	enum eqn_rest	 c;

	bp = eqn_box_alloc(ep, last);
	bp->type = EQN_MATRIX;

	if (NULL == (start = eqn_nexttok(ep, &sz))) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(EQN_ERR);
	}
	if ( ! STRNEQ(start, sz, "{", 1)) {
		EQN_MSG(MANDOCERR_EQNSYNT, ep);
		return(EQN_ERR);
	}

	while (EQN_OK == (c = eqn_box(ep, bp)))
		switch (bp->last->pile) {
		case (EQNPILE_LCOL):
			/* FALLTHROUGH */
		case (EQNPILE_CCOL):
			/* FALLTHROUGH */
		case (EQNPILE_RCOL):
			continue;
		default:
			EQN_MSG(MANDOCERR_EQNSYNT, ep);
			return(EQN_ERR);
		};

	if (EQN_DESCOPE != c) {
		if (EQN_EOF == c)
			EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(EQN_ERR);
	}

	eqn_rewind(ep);
	start = eqn_nexttok(ep, &sz);
	assert(start);
	if (STRNEQ(start, sz, "}", 1))
		return(EQN_OK);

	EQN_MSG(MANDOCERR_EQNBADSCOPE, ep);
	return(EQN_ERR);
}

static enum eqn_rest
eqn_list(struct eqn_node *ep, struct eqn_box *last)
{
	struct eqn_box	*bp;
	const char	*start;
	size_t		 sz;
	enum eqn_rest	 c;

	bp = eqn_box_alloc(ep, last);
	bp->type = EQN_LIST;

	if (NULL == (start = eqn_nexttok(ep, &sz))) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(EQN_ERR);
	}
	if ( ! STRNEQ(start, sz, "{", 1)) {
		EQN_MSG(MANDOCERR_EQNSYNT, ep);
		return(EQN_ERR);
	}

	while (EQN_DESCOPE == (c = eqn_eqn(ep, bp))) {
		eqn_rewind(ep);
		start = eqn_nexttok(ep, &sz);
		assert(start);
		if ( ! STRNEQ(start, sz, "above", 5))
			break;
	}

	if (EQN_DESCOPE != c) {
		if (EQN_ERR != c)
			EQN_MSG(MANDOCERR_EQNSCOPE, ep);
		return(EQN_ERR);
	}

	eqn_rewind(ep);
	start = eqn_nexttok(ep, &sz);
	assert(start);
	if (STRNEQ(start, sz, "}", 1))
		return(EQN_OK);

	EQN_MSG(MANDOCERR_EQNBADSCOPE, ep);
	return(EQN_ERR);
}

static enum eqn_rest
eqn_box(struct eqn_node *ep, struct eqn_box *last)
{
	size_t		 sz;
	const char	*start;
	char		*left;
	char		 sym[64];
	enum eqn_rest	 c;
	int		 i, size;
	struct eqn_box	*bp;

	if (NULL == (start = eqn_nexttok(ep, &sz)))
		return(EQN_EOF);

	if (STRNEQ(start, sz, "}", 1))
		return(EQN_DESCOPE);
	else if (STRNEQ(start, sz, "right", 5))
		return(EQN_DESCOPE);
	else if (STRNEQ(start, sz, "above", 5))
		return(EQN_DESCOPE);
	else if (STRNEQ(start, sz, "mark", 4))
		return(EQN_OK);
	else if (STRNEQ(start, sz, "lineup", 6))
		return(EQN_OK);

	for (i = 0; i < (int)EQN__MAX; i++) {
		if ( ! EQNSTREQ(&eqnparts[i].str, start, sz))
			continue;
		return((*eqnparts[i].fp)(ep) ? 
				EQN_OK : EQN_ERR);
	} 

	if (STRNEQ(start, sz, "{", 1)) {
		if (EQN_DESCOPE != (c = eqn_eqn(ep, last))) {
			if (EQN_ERR != c)
				EQN_MSG(MANDOCERR_EQNSCOPE, ep);
			return(EQN_ERR);
		}
		eqn_rewind(ep);
		start = eqn_nexttok(ep, &sz);
		assert(start);
		if (STRNEQ(start, sz, "}", 1))
			return(EQN_OK);
		EQN_MSG(MANDOCERR_EQNBADSCOPE, ep);
		return(EQN_ERR);
	} 

	for (i = 0; i < (int)EQNPILE__MAX; i++) {
		if ( ! EQNSTREQ(&eqnpiles[i], start, sz))
			continue;
		if (EQN_OK == (c = eqn_list(ep, last)))
			last->last->pile = (enum eqn_pilet)i;
		return(c);
	}

	if (STRNEQ(start, sz, "matrix", 6))
		return(eqn_matrix(ep, last));

	if (STRNEQ(start, sz, "left", 4)) {
		if (NULL == (start = eqn_nexttok(ep, &sz))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		}
		left = mandoc_strndup(start, sz);
		c = eqn_eqn(ep, last);
		if (last->last)
			last->last->left = left;
		else
			free(left);
		if (EQN_DESCOPE != c)
			return(c);
		assert(last->last);
		eqn_rewind(ep);
		start = eqn_nexttok(ep, &sz);
		assert(start);
		if ( ! STRNEQ(start, sz, "right", 5))
			return(EQN_DESCOPE);
		if (NULL == (start = eqn_nexttok(ep, &sz))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		}
		last->last->right = mandoc_strndup(start, sz);
		return(EQN_OK);
	}

	for (i = 0; i < (int)EQNPOS__MAX; i++) {
		if ( ! EQNSTREQ(&eqnposs[i], start, sz))
			continue;
		if (NULL == last->last) {
			EQN_MSG(MANDOCERR_EQNSYNT, ep);
			return(EQN_ERR);
		} 
		last->last->pos = (enum eqn_post)i;
		if (EQN_EOF == (c = eqn_box(ep, last))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		}
		return(c);
	}

	for (i = 0; i < (int)EQNMARK__MAX; i++) {
		if ( ! EQNSTREQ(&eqnmarks[i], start, sz))
			continue;
		if (NULL == last->last) {
			EQN_MSG(MANDOCERR_EQNSYNT, ep);
			return(EQN_ERR);
		} 
		last->last->mark = (enum eqn_markt)i;
		if (EQN_EOF == (c = eqn_box(ep, last))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		}
		return(c);
	}

	for (i = 0; i < (int)EQNFONT__MAX; i++) {
		if ( ! EQNSTREQ(&eqnfonts[i], start, sz))
			continue;
		if (EQN_EOF == (c = eqn_box(ep, last))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		} else if (EQN_OK == c)
			last->last->font = (enum eqn_fontt)i;
		return(c);
	}

	if (STRNEQ(start, sz, "size", 4)) {
		if (NULL == (start = eqn_nexttok(ep, &sz))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		}
		size = mandoc_strntoi(start, sz, 10);
		if (EQN_EOF == (c = eqn_box(ep, last))) {
			EQN_MSG(MANDOCERR_EQNEOF, ep);
			return(EQN_ERR);
		} else if (EQN_OK != c)
			return(c);
		last->last->size = size;
	}

	bp = eqn_box_alloc(ep, last);
	bp->type = EQN_TEXT;
	for (i = 0; i < (int)EQNSYM__MAX; i++)
		if (EQNSTREQ(&eqnsyms[i].str, start, sz)) {
			sym[63] = '\0';
			snprintf(sym, 62, "\\[%s]", eqnsyms[i].sym);
			bp->text = mandoc_strdup(sym);
			return(EQN_OK);
		}

	bp->text = mandoc_strndup(start, sz);
	return(EQN_OK);
}

void
eqn_free(struct eqn_node *p)
{
	int		 i;

	eqn_box_free(p->eqn.root);

	for (i = 0; i < (int)p->defsz; i++) {
		free(p->defs[i].key);
		free(p->defs[i].val);
	}

	free(p->eqn.name);
	free(p->data);
	free(p->defs);
	free(p);
}

static struct eqn_box *
eqn_box_alloc(struct eqn_node *ep, struct eqn_box *parent)
{
	struct eqn_box	*bp;

	bp = mandoc_calloc(1, sizeof(struct eqn_box));
	bp->parent = parent;
	bp->size = ep->gsize;

	if (NULL == parent->first)
		parent->first = bp;
	else
		parent->last->next = bp;

	parent->last = bp;
	return(bp);
}

static void
eqn_box_free(struct eqn_box *bp)
{

	if (bp->first)
		eqn_box_free(bp->first);
	if (bp->next)
		eqn_box_free(bp->next);

	free(bp->text);
	free(bp->left);
	free(bp->right);
	free(bp);
}

static const char *
eqn_nextrawtok(struct eqn_node *ep, size_t *sz)
{

	return(eqn_next(ep, '"', sz, 0));
}

static const char *
eqn_nexttok(struct eqn_node *ep, size_t *sz)
{

	return(eqn_next(ep, '"', sz, 1));
}

static void
eqn_rewind(struct eqn_node *ep)
{

	ep->cur = ep->rew;
}

static const char *
eqn_next(struct eqn_node *ep, char quote, size_t *sz, int repl)
{
	char		*start, *next;
	int		 q, diff, lim;
	size_t		 ssz, dummy;
	struct eqn_def	*def;

	if (NULL == sz)
		sz = &dummy;

	lim = 0;
	ep->rew = ep->cur;
again:
	/* Prevent self-definitions. */

	if (lim >= EQN_NEST_MAX) {
		EQN_MSG(MANDOCERR_ROFFLOOP, ep);
		return(NULL);
	}

	ep->cur = ep->rew;
	start = &ep->data[(int)ep->cur];
	q = 0;

	if ('\0' == *start)
		return(NULL);

	if (quote == *start) {
		ep->cur++;
		q = 1;
	}

	start = &ep->data[(int)ep->cur];

	if ( ! q) {
		if ('{' == *start || '}' == *start)
			ssz = 1;
		else
			ssz = strcspn(start + 1, " ^~\"{}\t") + 1;
		next = start + (int)ssz;
		if ('\0' == *next)
			next = NULL;
	} else
		next = strchr(start, quote);

	if (NULL != next) {
		*sz = (size_t)(next - start);
		ep->cur += *sz;
		if (q)
			ep->cur++;
		while (' ' == ep->data[(int)ep->cur] ||
				'\t' == ep->data[(int)ep->cur] ||
				'^' == ep->data[(int)ep->cur] ||
				'~' == ep->data[(int)ep->cur])
			ep->cur++;
	} else {
		if (q)
			EQN_MSG(MANDOCERR_BADQUOTE, ep);
		next = strchr(start, '\0');
		*sz = (size_t)(next - start);
		ep->cur += *sz;
	}

	/* Quotes aren't expanded for values. */

	if (q || ! repl)
		return(start);

	if (NULL != (def = eqn_def_find(ep, start, *sz))) {
		diff = def->valsz - *sz;

		if (def->valsz > *sz) {
			ep->sz += diff;
			ep->data = mandoc_realloc(ep->data, ep->sz + 1);
			ep->data[ep->sz] = '\0';
			start = &ep->data[(int)ep->rew];
		}

		diff = def->valsz - *sz;
		memmove(start + *sz + diff, start + *sz, 
				(strlen(start) - *sz) + 1);
		memcpy(start, def->val, def->valsz);
		goto again;
	}

	return(start);
}

static int
eqn_do_ign1(struct eqn_node *ep)
{

	if (NULL == eqn_nextrawtok(ep, NULL))
		EQN_MSG(MANDOCERR_EQNEOF, ep);
	else
		return(1);

	return(0);
}

static int
eqn_do_ign2(struct eqn_node *ep)
{

	if (NULL == eqn_nextrawtok(ep, NULL))
		EQN_MSG(MANDOCERR_EQNEOF, ep);
	else if (NULL == eqn_nextrawtok(ep, NULL))
		EQN_MSG(MANDOCERR_EQNEOF, ep);
	else
		return(1);

	return(0);
}

static int
eqn_do_tdefine(struct eqn_node *ep)
{

	if (NULL == eqn_nextrawtok(ep, NULL))
		EQN_MSG(MANDOCERR_EQNEOF, ep);
	else if (NULL == eqn_next(ep, ep->data[(int)ep->cur], NULL, 0))
		EQN_MSG(MANDOCERR_EQNEOF, ep);
	else
		return(1);

	return(0);
}

static int
eqn_do_define(struct eqn_node *ep)
{
	const char	*start;
	size_t		 sz;
	struct eqn_def	*def;
	int		 i;

	if (NULL == (start = eqn_nextrawtok(ep, &sz))) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(0);
	}

	/* 
	 * Search for a key that already exists. 
	 * Create a new key if none is found.
	 */

	if (NULL == (def = eqn_def_find(ep, start, sz))) {
		/* Find holes in string array. */
		for (i = 0; i < (int)ep->defsz; i++)
			if (0 == ep->defs[i].keysz)
				break;

		if (i == (int)ep->defsz) {
			ep->defsz++;
			ep->defs = mandoc_realloc
				(ep->defs, ep->defsz * 
				 sizeof(struct eqn_def));
			ep->defs[i].key = ep->defs[i].val = NULL;
		}

		ep->defs[i].keysz = sz;
		ep->defs[i].key = mandoc_realloc
			(ep->defs[i].key, sz + 1);

		memcpy(ep->defs[i].key, start, sz);
		ep->defs[i].key[(int)sz] = '\0';
		def = &ep->defs[i];
	}

	start = eqn_next(ep, ep->data[(int)ep->cur], &sz, 0);

	if (NULL == start) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(0);
	}

	def->valsz = sz;
	def->val = mandoc_realloc(def->val, sz + 1);
	memcpy(def->val, start, sz);
	def->val[(int)sz] = '\0';
	return(1);
}

static int
eqn_do_gfont(struct eqn_node *ep)
{

	if (NULL == eqn_nextrawtok(ep, NULL)) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(0);
	} 
	return(1);
}

static int
eqn_do_gsize(struct eqn_node *ep)
{
	const char	*start;
	size_t		 sz;

	if (NULL == (start = eqn_nextrawtok(ep, &sz))) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(0);
	} 
	ep->gsize = mandoc_strntoi(start, sz, 10);
	return(1);
}

static int
eqn_do_undef(struct eqn_node *ep)
{
	const char	*start;
	struct eqn_def	*def;
	size_t		 sz;

	if (NULL == (start = eqn_nextrawtok(ep, &sz))) {
		EQN_MSG(MANDOCERR_EQNEOF, ep);
		return(0);
	} else if (NULL != (def = eqn_def_find(ep, start, sz)))
		def->keysz = 0;

	return(1);
}

static struct eqn_def *
eqn_def_find(struct eqn_node *ep, const char *key, size_t sz)
{
	int		 i;

	for (i = 0; i < (int)ep->defsz; i++) 
		if (ep->defs[i].keysz && STRNEQ(ep->defs[i].key, 
					ep->defs[i].keysz, key, sz))
			return(&ep->defs[i]);

	return(NULL);
}
