/*	$Id: eqn.c,v 1.61 2016/01/08 00:50:45 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmandoc.h"
#include "libroff.h"

#define	EQN_NEST_MAX	 128 /* maximum nesting of defines */
#define	STRNEQ(p1, sz1, p2, sz2) \
	((sz1) == (sz2) && 0 == strncmp((p1), (p2), (sz1)))

enum	eqn_tok {
	EQN_TOK_DYAD = 0,
	EQN_TOK_VEC,
	EQN_TOK_UNDER,
	EQN_TOK_BAR,
	EQN_TOK_TILDE,
	EQN_TOK_HAT,
	EQN_TOK_DOT,
	EQN_TOK_DOTDOT,
	EQN_TOK_FWD,
	EQN_TOK_BACK,
	EQN_TOK_DOWN,
	EQN_TOK_UP,
	EQN_TOK_FAT,
	EQN_TOK_ROMAN,
	EQN_TOK_ITALIC,
	EQN_TOK_BOLD,
	EQN_TOK_SIZE,
	EQN_TOK_SUB,
	EQN_TOK_SUP,
	EQN_TOK_SQRT,
	EQN_TOK_OVER,
	EQN_TOK_FROM,
	EQN_TOK_TO,
	EQN_TOK_BRACE_OPEN,
	EQN_TOK_BRACE_CLOSE,
	EQN_TOK_GSIZE,
	EQN_TOK_GFONT,
	EQN_TOK_MARK,
	EQN_TOK_LINEUP,
	EQN_TOK_LEFT,
	EQN_TOK_RIGHT,
	EQN_TOK_PILE,
	EQN_TOK_LPILE,
	EQN_TOK_RPILE,
	EQN_TOK_CPILE,
	EQN_TOK_MATRIX,
	EQN_TOK_CCOL,
	EQN_TOK_LCOL,
	EQN_TOK_RCOL,
	EQN_TOK_DELIM,
	EQN_TOK_DEFINE,
	EQN_TOK_TDEFINE,
	EQN_TOK_NDEFINE,
	EQN_TOK_UNDEF,
	EQN_TOK_EOF,
	EQN_TOK_ABOVE,
	EQN_TOK__MAX
};

static	const char *eqn_toks[EQN_TOK__MAX] = {
	"dyad", /* EQN_TOK_DYAD */
	"vec", /* EQN_TOK_VEC */
	"under", /* EQN_TOK_UNDER */
	"bar", /* EQN_TOK_BAR */
	"tilde", /* EQN_TOK_TILDE */
	"hat", /* EQN_TOK_HAT */
	"dot", /* EQN_TOK_DOT */
	"dotdot", /* EQN_TOK_DOTDOT */
	"fwd", /* EQN_TOK_FWD * */
	"back", /* EQN_TOK_BACK */
	"down", /* EQN_TOK_DOWN */
	"up", /* EQN_TOK_UP */
	"fat", /* EQN_TOK_FAT */
	"roman", /* EQN_TOK_ROMAN */
	"italic", /* EQN_TOK_ITALIC */
	"bold", /* EQN_TOK_BOLD */
	"size", /* EQN_TOK_SIZE */
	"sub", /* EQN_TOK_SUB */
	"sup", /* EQN_TOK_SUP */
	"sqrt", /* EQN_TOK_SQRT */
	"over", /* EQN_TOK_OVER */
	"from", /* EQN_TOK_FROM */
	"to", /* EQN_TOK_TO */
	"{", /* EQN_TOK_BRACE_OPEN */
	"}", /* EQN_TOK_BRACE_CLOSE */
	"gsize", /* EQN_TOK_GSIZE */
	"gfont", /* EQN_TOK_GFONT */
	"mark", /* EQN_TOK_MARK */
	"lineup", /* EQN_TOK_LINEUP */
	"left", /* EQN_TOK_LEFT */
	"right", /* EQN_TOK_RIGHT */
	"pile", /* EQN_TOK_PILE */
	"lpile", /* EQN_TOK_LPILE */
	"rpile", /* EQN_TOK_RPILE */
	"cpile", /* EQN_TOK_CPILE */
	"matrix", /* EQN_TOK_MATRIX */
	"ccol", /* EQN_TOK_CCOL */
	"lcol", /* EQN_TOK_LCOL */
	"rcol", /* EQN_TOK_RCOL */
	"delim", /* EQN_TOK_DELIM */
	"define", /* EQN_TOK_DEFINE */
	"tdefine", /* EQN_TOK_TDEFINE */
	"ndefine", /* EQN_TOK_NDEFINE */
	"undef", /* EQN_TOK_UNDEF */
	NULL, /* EQN_TOK_EOF */
	"above", /* EQN_TOK_ABOVE */
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
	EQNSYM_minus,
	EQNSYM__MAX
};

struct	eqnsym {
	const char	*str;
	const char	*sym;
};

static	const struct eqnsym eqnsyms[EQNSYM__MAX] = {
	{ "alpha", "*a" }, /* EQNSYM_alpha */
	{ "beta", "*b" }, /* EQNSYM_beta */
	{ "chi", "*x" }, /* EQNSYM_chi */
	{ "delta", "*d" }, /* EQNSYM_delta */
	{ "epsilon", "*e" }, /* EQNSYM_epsilon */
	{ "eta", "*y" }, /* EQNSYM_eta */
	{ "gamma", "*g" }, /* EQNSYM_gamma */
	{ "iota", "*i" }, /* EQNSYM_iota */
	{ "kappa", "*k" }, /* EQNSYM_kappa */
	{ "lambda", "*l" }, /* EQNSYM_lambda */
	{ "mu", "*m" }, /* EQNSYM_mu */
	{ "nu", "*n" }, /* EQNSYM_nu */
	{ "omega", "*w" }, /* EQNSYM_omega */
	{ "omicron", "*o" }, /* EQNSYM_omicron */
	{ "phi", "*f" }, /* EQNSYM_phi */
	{ "pi", "*p" }, /* EQNSYM_pi */
	{ "psi", "*q" }, /* EQNSYM_psi */
	{ "rho", "*r" }, /* EQNSYM_rho */
	{ "sigma", "*s" }, /* EQNSYM_sigma */
	{ "tau", "*t" }, /* EQNSYM_tau */
	{ "theta", "*h" }, /* EQNSYM_theta */
	{ "upsilon", "*u" }, /* EQNSYM_upsilon */
	{ "xi", "*c" }, /* EQNSYM_xi */
	{ "zeta", "*z" }, /* EQNSYM_zeta */
	{ "DELTA", "*D" }, /* EQNSYM_DELTA */
	{ "GAMMA", "*G" }, /* EQNSYM_GAMMA */
	{ "LAMBDA", "*L" }, /* EQNSYM_LAMBDA */
	{ "OMEGA", "*W" }, /* EQNSYM_OMEGA */
	{ "PHI", "*F" }, /* EQNSYM_PHI */
	{ "PI", "*P" }, /* EQNSYM_PI */
	{ "PSI", "*Q" }, /* EQNSYM_PSI */
	{ "SIGMA", "*S" }, /* EQNSYM_SIGMA */
	{ "THETA", "*H" }, /* EQNSYM_THETA */
	{ "UPSILON", "*U" }, /* EQNSYM_UPSILON */
	{ "XI", "*C" }, /* EQNSYM_XI */
	{ "inter", "ca" }, /* EQNSYM_inter */
	{ "union", "cu" }, /* EQNSYM_union */
	{ "prod", "product" }, /* EQNSYM_prod */
	{ "int", "integral" }, /* EQNSYM_int */
	{ "sum", "sum" }, /* EQNSYM_sum */
	{ "grad", "gr" }, /* EQNSYM_grad */
	{ "del", "gr" }, /* EQNSYM_del */
	{ "times", "mu" }, /* EQNSYM_times */
	{ "cdot", "pc" }, /* EQNSYM_cdot */
	{ "nothing", "&" }, /* EQNSYM_nothing */
	{ "approx", "~~" }, /* EQNSYM_approx */
	{ "prime", "fm" }, /* EQNSYM_prime */
	{ "half", "12" }, /* EQNSYM_half */
	{ "partial", "pd" }, /* EQNSYM_partial */
	{ "inf", "if" }, /* EQNSYM_inf */
	{ ">>", ">>" }, /* EQNSYM_muchgreat */
	{ "<<", "<<" }, /* EQNSYM_muchless */
	{ "<-", "<-" }, /* EQNSYM_larrow */
	{ "->", "->" }, /* EQNSYM_rarrow */
	{ "+-", "+-" }, /* EQNSYM_pm */
	{ "!=", "!=" }, /* EQNSYM_nequal */
	{ "==", "==" }, /* EQNSYM_equiv */
	{ "<=", "<=" }, /* EQNSYM_lessequal */
	{ ">=", ">=" }, /* EQNSYM_moreequal */
	{ "-", "mi" }, /* EQNSYM_minus */
};

static	struct eqn_box	*eqn_box_alloc(struct eqn_node *, struct eqn_box *);
static	void		 eqn_box_free(struct eqn_box *);
static	struct eqn_box	*eqn_box_makebinary(struct eqn_node *,
				enum eqn_post, struct eqn_box *);
static	void		 eqn_def(struct eqn_node *);
static	struct eqn_def	*eqn_def_find(struct eqn_node *, const char *, size_t);
static	void		 eqn_delim(struct eqn_node *);
static	const char	*eqn_next(struct eqn_node *, char, size_t *, int);
static	const char	*eqn_nextrawtok(struct eqn_node *, size_t *);
static	const char	*eqn_nexttok(struct eqn_node *, size_t *);
static	enum rofferr	 eqn_parse(struct eqn_node *, struct eqn_box *);
static	enum eqn_tok	 eqn_tok_parse(struct eqn_node *, char **);
static	void		 eqn_undef(struct eqn_node *);


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
			return er;
		mandoc_vmsg(MANDOCERR_ARG_SKIP, ep->parse,
		    ln, pos, "EN %s", p);
		return er;
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
	return ROFF_IGN;
}

struct eqn_node *
eqn_alloc(int pos, int line, struct mparse *parse)
{
	struct eqn_node	*p;

	p = mandoc_calloc(1, sizeof(struct eqn_node));

	p->parse = parse;
	p->eqn.ln = line;
	p->eqn.pos = pos;
	p->gsize = EQN_DEFSIZE;

	return p;
}

/*
 * Find the key "key" of the give size within our eqn-defined values.
 */
static struct eqn_def *
eqn_def_find(struct eqn_node *ep, const char *key, size_t sz)
{
	int		 i;

	for (i = 0; i < (int)ep->defsz; i++)
		if (ep->defs[i].keysz && STRNEQ(ep->defs[i].key,
		    ep->defs[i].keysz, key, sz))
			return &ep->defs[i];

	return NULL;
}

/*
 * Get the next token from the input stream using the given quote
 * character.
 * Optionally make any replacements.
 */
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
		mandoc_msg(MANDOCERR_ROFFLOOP, ep->parse,
		    ep->eqn.ln, ep->eqn.pos, NULL);
		return NULL;
	}

	ep->cur = ep->rew;
	start = &ep->data[(int)ep->cur];
	q = 0;

	if ('\0' == *start)
		return NULL;

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
			mandoc_msg(MANDOCERR_ARG_QUOTE, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, NULL);
		next = strchr(start, '\0');
		*sz = (size_t)(next - start);
		ep->cur += *sz;
	}

	/* Quotes aren't expanded for values. */

	if (q || ! repl)
		return start;

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
		lim++;
		goto again;
	}

	return start;
}

/*
 * Get the next delimited token using the default current quote
 * character.
 */
static const char *
eqn_nexttok(struct eqn_node *ep, size_t *sz)
{

	return eqn_next(ep, '"', sz, 1);
}

/*
 * Get next token without replacement.
 */
static const char *
eqn_nextrawtok(struct eqn_node *ep, size_t *sz)
{

	return eqn_next(ep, '"', sz, 0);
}

/*
 * Parse a token from the stream of text.
 * A token consists of one of the recognised eqn(7) strings.
 * Strings are separated by delimiting marks.
 * This returns EQN_TOK_EOF when there are no more tokens.
 * If the token is an unrecognised string literal, then it returns
 * EQN_TOK__MAX and sets the "p" pointer to an allocated, nil-terminated
 * string.
 * This must be later freed with free(3).
 */
static enum eqn_tok
eqn_tok_parse(struct eqn_node *ep, char **p)
{
	const char	*start;
	size_t		 i, sz;
	int		 quoted;

	if (NULL != p)
		*p = NULL;

	quoted = ep->data[ep->cur] == '"';

	if (NULL == (start = eqn_nexttok(ep, &sz)))
		return EQN_TOK_EOF;

	if (quoted) {
		if (p != NULL)
			*p = mandoc_strndup(start, sz);
		return EQN_TOK__MAX;
	}

	for (i = 0; i < EQN_TOK__MAX; i++) {
		if (NULL == eqn_toks[i])
			continue;
		if (STRNEQ(start, sz, eqn_toks[i], strlen(eqn_toks[i])))
			break;
	}

	if (i == EQN_TOK__MAX && NULL != p)
		*p = mandoc_strndup(start, sz);

	return i;
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
	free(bp->top);
	free(bp->bottom);
	free(bp);
}

/*
 * Allocate a box as the last child of the parent node.
 */
static struct eqn_box *
eqn_box_alloc(struct eqn_node *ep, struct eqn_box *parent)
{
	struct eqn_box	*bp;

	bp = mandoc_calloc(1, sizeof(struct eqn_box));
	bp->parent = parent;
	bp->parent->args++;
	bp->expectargs = UINT_MAX;
	bp->size = ep->gsize;

	if (NULL != parent->first) {
		parent->last->next = bp;
		bp->prev = parent->last;
	} else
		parent->first = bp;

	parent->last = bp;
	return bp;
}

/*
 * Reparent the current last node (of the current parent) under a new
 * EQN_SUBEXPR as the first element.
 * Then return the new parent.
 * The new EQN_SUBEXPR will have a two-child limit.
 */
static struct eqn_box *
eqn_box_makebinary(struct eqn_node *ep,
	enum eqn_post pos, struct eqn_box *parent)
{
	struct eqn_box	*b, *newb;

	assert(NULL != parent->last);
	b = parent->last;
	if (parent->last == parent->first)
		parent->first = NULL;
	parent->args--;
	parent->last = b->prev;
	b->prev = NULL;
	newb = eqn_box_alloc(ep, parent);
	newb->pos = pos;
	newb->type = EQN_SUBEXPR;
	newb->expectargs = 2;
	newb->args = 1;
	newb->first = newb->last = b;
	newb->first->next = NULL;
	b->parent = newb;
	return newb;
}

/*
 * Parse the "delim" control statement.
 */
static void
eqn_delim(struct eqn_node *ep)
{
	const char	*start;
	size_t		 sz;

	if ((start = eqn_nextrawtok(ep, &sz)) == NULL)
		mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
		    ep->eqn.ln, ep->eqn.pos, "delim");
	else if (strncmp(start, "off", 3) == 0)
		ep->delim = 0;
	else if (strncmp(start, "on", 2) == 0) {
		if (ep->odelim && ep->cdelim)
			ep->delim = 1;
	} else if (start[1] != '\0') {
		ep->odelim = start[0];
		ep->cdelim = start[1];
		ep->delim = 1;
	}
}

/*
 * Undefine a previously-defined string.
 */
static void
eqn_undef(struct eqn_node *ep)
{
	const char	*start;
	struct eqn_def	*def;
	size_t		 sz;

	if ((start = eqn_nextrawtok(ep, &sz)) == NULL) {
		mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
		    ep->eqn.ln, ep->eqn.pos, "undef");
		return;
	}
	if ((def = eqn_def_find(ep, start, sz)) == NULL)
		return;
	free(def->key);
	free(def->val);
	def->key = def->val = NULL;
	def->keysz = def->valsz = 0;
}

static void
eqn_def(struct eqn_node *ep)
{
	const char	*start;
	size_t		 sz;
	struct eqn_def	*def;
	int		 i;

	if ((start = eqn_nextrawtok(ep, &sz)) == NULL) {
		mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
		    ep->eqn.ln, ep->eqn.pos, "define");
		return;
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
			ep->defs = mandoc_reallocarray(ep->defs,
			    ep->defsz, sizeof(struct eqn_def));
			ep->defs[i].key = ep->defs[i].val = NULL;
		}

		def = ep->defs + i;
		free(def->key);
		def->key = mandoc_strndup(start, sz);
		def->keysz = sz;
	}

	start = eqn_next(ep, ep->data[(int)ep->cur], &sz, 0);
	if (start == NULL) {
		mandoc_vmsg(MANDOCERR_REQ_EMPTY, ep->parse,
		    ep->eqn.ln, ep->eqn.pos, "define %s", def->key);
		free(def->key);
		free(def->val);
		def->key = def->val = NULL;
		def->keysz = def->valsz = 0;
		return;
	}
	free(def->val);
	def->val = mandoc_strndup(start, sz);
	def->valsz = sz;
}

/*
 * Recursively parse an eqn(7) expression.
 */
static enum rofferr
eqn_parse(struct eqn_node *ep, struct eqn_box *parent)
{
	char		 sym[64];
	struct eqn_box	*cur;
	const char	*start;
	char		*p;
	size_t		 i, sz;
	enum eqn_tok	 tok, subtok;
	enum eqn_post	 pos;
	int		 size;

	assert(parent != NULL);

	/*
	 * Empty equation.
	 * Do not add it to the high-level syntax tree.
	 */

	if (ep->data == NULL)
		return ROFF_IGN;

next_tok:
	tok = eqn_tok_parse(ep, &p);

this_tok:
	switch (tok) {
	case (EQN_TOK_UNDEF):
		eqn_undef(ep);
		break;
	case (EQN_TOK_NDEFINE):
	case (EQN_TOK_DEFINE):
		eqn_def(ep);
		break;
	case (EQN_TOK_TDEFINE):
		if (eqn_nextrawtok(ep, NULL) == NULL ||
		    eqn_next(ep, ep->data[(int)ep->cur], NULL, 0) == NULL)
			mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, "tdefine");
		break;
	case (EQN_TOK_DELIM):
		eqn_delim(ep);
		break;
	case (EQN_TOK_GFONT):
		if (eqn_nextrawtok(ep, NULL) == NULL)
			mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
		break;
	case (EQN_TOK_MARK):
	case (EQN_TOK_LINEUP):
		/* Ignore these. */
		break;
	case (EQN_TOK_DYAD):
	case (EQN_TOK_VEC):
	case (EQN_TOK_UNDER):
	case (EQN_TOK_BAR):
	case (EQN_TOK_TILDE):
	case (EQN_TOK_HAT):
	case (EQN_TOK_DOT):
	case (EQN_TOK_DOTDOT):
		if (parent->last == NULL) {
			mandoc_msg(MANDOCERR_EQN_NOBOX, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			cur = eqn_box_alloc(ep, parent);
			cur->type = EQN_TEXT;
			cur->text = mandoc_strdup("");
		}
		parent = eqn_box_makebinary(ep, EQNPOS_NONE, parent);
		parent->type = EQN_LISTONE;
		parent->expectargs = 1;
		switch (tok) {
		case (EQN_TOK_DOTDOT):
			strlcpy(sym, "\\[ad]", sizeof(sym));
			break;
		case (EQN_TOK_VEC):
			strlcpy(sym, "\\[->]", sizeof(sym));
			break;
		case (EQN_TOK_DYAD):
			strlcpy(sym, "\\[<>]", sizeof(sym));
			break;
		case (EQN_TOK_TILDE):
			strlcpy(sym, "\\[a~]", sizeof(sym));
			break;
		case (EQN_TOK_UNDER):
			strlcpy(sym, "\\[ul]", sizeof(sym));
			break;
		case (EQN_TOK_BAR):
			strlcpy(sym, "\\[rl]", sizeof(sym));
			break;
		case (EQN_TOK_DOT):
			strlcpy(sym, "\\[a.]", sizeof(sym));
			break;
		case (EQN_TOK_HAT):
			strlcpy(sym, "\\[ha]", sizeof(sym));
			break;
		default:
			abort();
		}

		switch (tok) {
		case (EQN_TOK_DOTDOT):
		case (EQN_TOK_VEC):
		case (EQN_TOK_DYAD):
		case (EQN_TOK_TILDE):
		case (EQN_TOK_BAR):
		case (EQN_TOK_DOT):
		case (EQN_TOK_HAT):
			parent->top = mandoc_strdup(sym);
			break;
		case (EQN_TOK_UNDER):
			parent->bottom = mandoc_strdup(sym);
			break;
		default:
			abort();
		}
		parent = parent->parent;
		break;
	case (EQN_TOK_FWD):
	case (EQN_TOK_BACK):
	case (EQN_TOK_DOWN):
	case (EQN_TOK_UP):
		subtok = eqn_tok_parse(ep, NULL);
		if (subtok != EQN_TOK__MAX) {
			mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			tok = subtok;
			goto this_tok;
		}
		break;
	case (EQN_TOK_FAT):
	case (EQN_TOK_ROMAN):
	case (EQN_TOK_ITALIC):
	case (EQN_TOK_BOLD):
		while (parent->args == parent->expectargs)
			parent = parent->parent;
		/*
		 * These values apply to the next word or sequence of
		 * words; thus, we mark that we'll have a child with
		 * exactly one of those.
		 */
		parent = eqn_box_alloc(ep, parent);
		parent->type = EQN_LISTONE;
		parent->expectargs = 1;
		switch (tok) {
		case (EQN_TOK_FAT):
			parent->font = EQNFONT_FAT;
			break;
		case (EQN_TOK_ROMAN):
			parent->font = EQNFONT_ROMAN;
			break;
		case (EQN_TOK_ITALIC):
			parent->font = EQNFONT_ITALIC;
			break;
		case (EQN_TOK_BOLD):
			parent->font = EQNFONT_BOLD;
			break;
		default:
			abort();
		}
		break;
	case (EQN_TOK_SIZE):
	case (EQN_TOK_GSIZE):
		/* Accept two values: integral size and a single. */
		if (NULL == (start = eqn_nexttok(ep, &sz))) {
			mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			break;
		}
		size = mandoc_strntoi(start, sz, 10);
		if (-1 == size) {
			mandoc_msg(MANDOCERR_IT_NONUM, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			break;
		}
		if (EQN_TOK_GSIZE == tok) {
			ep->gsize = size;
			break;
		}
		parent = eqn_box_alloc(ep, parent);
		parent->type = EQN_LISTONE;
		parent->expectargs = 1;
		parent->size = size;
		break;
	case (EQN_TOK_FROM):
	case (EQN_TOK_TO):
	case (EQN_TOK_SUB):
	case (EQN_TOK_SUP):
		/*
		 * We have a left-right-associative expression.
		 * Repivot under a positional node, open a child scope
		 * and keep on reading.
		 */
		if (parent->last == NULL) {
			mandoc_msg(MANDOCERR_EQN_NOBOX, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			cur = eqn_box_alloc(ep, parent);
			cur->type = EQN_TEXT;
			cur->text = mandoc_strdup("");
		}
		/* Handle the "subsup" and "fromto" positions. */
		if (EQN_TOK_SUP == tok && parent->pos == EQNPOS_SUB) {
			parent->expectargs = 3;
			parent->pos = EQNPOS_SUBSUP;
			break;
		}
		if (EQN_TOK_TO == tok && parent->pos == EQNPOS_FROM) {
			parent->expectargs = 3;
			parent->pos = EQNPOS_FROMTO;
			break;
		}
		switch (tok) {
		case (EQN_TOK_FROM):
			pos = EQNPOS_FROM;
			break;
		case (EQN_TOK_TO):
			pos = EQNPOS_TO;
			break;
		case (EQN_TOK_SUP):
			pos = EQNPOS_SUP;
			break;
		case (EQN_TOK_SUB):
			pos = EQNPOS_SUB;
			break;
		default:
			abort();
		}
		parent = eqn_box_makebinary(ep, pos, parent);
		break;
	case (EQN_TOK_SQRT):
		while (parent->args == parent->expectargs)
			parent = parent->parent;
		/*
		 * Accept a left-right-associative set of arguments just
		 * like sub and sup and friends but without rebalancing
		 * under a pivot.
		 */
		parent = eqn_box_alloc(ep, parent);
		parent->type = EQN_SUBEXPR;
		parent->pos = EQNPOS_SQRT;
		parent->expectargs = 1;
		break;
	case (EQN_TOK_OVER):
		/*
		 * We have a right-left-associative fraction.
		 * Close out anything that's currently open, then
		 * rebalance and continue reading.
		 */
		if (parent->last == NULL) {
			mandoc_msg(MANDOCERR_EQN_NOBOX, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			cur = eqn_box_alloc(ep, parent);
			cur->type = EQN_TEXT;
			cur->text = mandoc_strdup("");
		}
		while (EQN_SUBEXPR == parent->type)
			parent = parent->parent;
		parent = eqn_box_makebinary(ep, EQNPOS_OVER, parent);
		break;
	case (EQN_TOK_RIGHT):
	case (EQN_TOK_BRACE_CLOSE):
		/*
		 * Close out the existing brace.
		 * FIXME: this is a shitty sentinel: we should really
		 * have a native EQN_BRACE type or whatnot.
		 */
		for (cur = parent; cur != NULL; cur = cur->parent)
			if (cur->type == EQN_LIST &&
			    (tok == EQN_TOK_BRACE_CLOSE ||
			     cur->left != NULL))
				break;
		if (cur == NULL) {
			mandoc_msg(MANDOCERR_BLK_NOTOPEN, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			break;
		}
		parent = cur;
		if (EQN_TOK_RIGHT == tok) {
			if (NULL == (start = eqn_nexttok(ep, &sz))) {
				mandoc_msg(MANDOCERR_REQ_EMPTY,
				    ep->parse, ep->eqn.ln,
				    ep->eqn.pos, eqn_toks[tok]);
				break;
			}
			/* Handling depends on right/left. */
			if (STRNEQ(start, sz, "ceiling", 7)) {
				strlcpy(sym, "\\[rc]", sizeof(sym));
				parent->right = mandoc_strdup(sym);
			} else if (STRNEQ(start, sz, "floor", 5)) {
				strlcpy(sym, "\\[rf]", sizeof(sym));
				parent->right = mandoc_strdup(sym);
			} else
				parent->right = mandoc_strndup(start, sz);
		}
		parent = parent->parent;
		if (tok == EQN_TOK_BRACE_CLOSE &&
		    (parent->type == EQN_PILE ||
		     parent->type == EQN_MATRIX))
			parent = parent->parent;
		/* Close out any "singleton" lists. */
		while (parent->type == EQN_LISTONE &&
		    parent->args == parent->expectargs)
			parent = parent->parent;
		break;
	case (EQN_TOK_BRACE_OPEN):
	case (EQN_TOK_LEFT):
		/*
		 * If we already have something in the stack and we're
		 * in an expression, then rewind til we're not any more
		 * (just like with the text node).
		 */
		while (parent->args == parent->expectargs)
			parent = parent->parent;
		if (EQN_TOK_LEFT == tok &&
		    (start = eqn_nexttok(ep, &sz)) == NULL) {
			mandoc_msg(MANDOCERR_REQ_EMPTY, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			break;
		}
		parent = eqn_box_alloc(ep, parent);
		parent->type = EQN_LIST;
		if (EQN_TOK_LEFT == tok) {
			if (STRNEQ(start, sz, "ceiling", 7)) {
				strlcpy(sym, "\\[lc]", sizeof(sym));
				parent->left = mandoc_strdup(sym);
			} else if (STRNEQ(start, sz, "floor", 5)) {
				strlcpy(sym, "\\[lf]", sizeof(sym));
				parent->left = mandoc_strdup(sym);
			} else
				parent->left = mandoc_strndup(start, sz);
		}
		break;
	case (EQN_TOK_PILE):
	case (EQN_TOK_LPILE):
	case (EQN_TOK_RPILE):
	case (EQN_TOK_CPILE):
	case (EQN_TOK_CCOL):
	case (EQN_TOK_LCOL):
	case (EQN_TOK_RCOL):
		while (parent->args == parent->expectargs)
			parent = parent->parent;
		parent = eqn_box_alloc(ep, parent);
		parent->type = EQN_PILE;
		parent->expectargs = 1;
		break;
	case (EQN_TOK_ABOVE):
		for (cur = parent; cur != NULL; cur = cur->parent)
			if (cur->type == EQN_PILE)
				break;
		if (cur == NULL) {
			mandoc_msg(MANDOCERR_IT_STRAY, ep->parse,
			    ep->eqn.ln, ep->eqn.pos, eqn_toks[tok]);
			break;
		}
		parent = eqn_box_alloc(ep, cur);
		parent->type = EQN_LIST;
		break;
	case (EQN_TOK_MATRIX):
		while (parent->args == parent->expectargs)
			parent = parent->parent;
		parent = eqn_box_alloc(ep, parent);
		parent->type = EQN_MATRIX;
		parent->expectargs = 1;
		break;
	case (EQN_TOK_EOF):
		/*
		 * End of file!
		 * TODO: make sure we're not in an open subexpression.
		 */
		return ROFF_EQN;
	default:
		assert(tok == EQN_TOK__MAX);
		assert(NULL != p);
		/*
		 * If we already have something in the stack and we're
		 * in an expression, then rewind til we're not any more.
		 */
		while (parent->args == parent->expectargs)
			parent = parent->parent;
		cur = eqn_box_alloc(ep, parent);
		cur->type = EQN_TEXT;
		for (i = 0; i < EQNSYM__MAX; i++)
			if (0 == strcmp(eqnsyms[i].str, p)) {
				(void)snprintf(sym, sizeof(sym),
					"\\[%s]", eqnsyms[i].sym);
				cur->text = mandoc_strdup(sym);
				free(p);
				break;
			}

		if (i == EQNSYM__MAX)
			cur->text = p;
		/*
		 * Post-process list status.
		 */
		while (parent->type == EQN_LISTONE &&
		    parent->args == parent->expectargs)
			parent = parent->parent;
		break;
	}
	goto next_tok;
}

enum rofferr
eqn_end(struct eqn_node **epp)
{
	struct eqn_node	*ep;

	ep = *epp;
	*epp = NULL;

	ep->eqn.root = mandoc_calloc(1, sizeof(struct eqn_box));
	ep->eqn.root->expectargs = UINT_MAX;
	return eqn_parse(ep, ep->eqn.root);
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

	free(p->data);
	free(p->defs);
	free(p);
}
