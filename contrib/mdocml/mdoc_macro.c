/*	$Id: mdoc_macro.c,v 1.154 2014/11/29 04:31:35 schwarze Exp $ */
/*
 * Copyright (c) 2008-2012 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2010, 2012, 2013, 2014 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mdoc.h"
#include "mandoc.h"
#include "libmdoc.h"
#include "libmandoc.h"

enum	rew {	/* see rew_dohalt() */
	REWIND_NONE,
	REWIND_THIS,
	REWIND_MORE,
	REWIND_FORCE,
	REWIND_LATER,
	REWIND_ERROR
};

static	void		blk_full(MACRO_PROT_ARGS);
static	void		blk_exp_close(MACRO_PROT_ARGS);
static	void		blk_part_exp(MACRO_PROT_ARGS);
static	void		blk_part_imp(MACRO_PROT_ARGS);
static	void		ctx_synopsis(MACRO_PROT_ARGS);
static	void		in_line_eoln(MACRO_PROT_ARGS);
static	void		in_line_argn(MACRO_PROT_ARGS);
static	void		in_line(MACRO_PROT_ARGS);
static	void		phrase_ta(MACRO_PROT_ARGS);

static	void		dword(struct mdoc *, int, int, const char *,
				 enum mdelim, int);
static	void		append_delims(struct mdoc *, int, int *, char *);
static	enum mdoct	lookup(enum mdoct, const char *);
static	int		macro_or_word(MACRO_PROT_ARGS, int);
static	int		make_pending(struct mdoc_node *, enum mdoct,
				struct mdoc *, int, int);
static	int		parse_rest(struct mdoc *, enum mdoct,
				int, int *, char *);
static	enum mdoct	rew_alt(enum mdoct);
static	enum rew	rew_dohalt(enum mdoct, enum mdoc_type,
				const struct mdoc_node *);
static	void		rew_elem(struct mdoc *, enum mdoct);
static	void		rew_last(struct mdoc *, const struct mdoc_node *);
static	void		rew_sub(enum mdoc_type, struct mdoc *,
				enum mdoct, int, int);

const	struct mdoc_macro __mdoc_macros[MDOC_MAX] = {
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Ap */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dd */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dt */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Os */
	{ blk_full, MDOC_PARSED | MDOC_JOIN }, /* Sh */
	{ blk_full, MDOC_PARSED | MDOC_JOIN }, /* Ss */
	{ in_line_eoln, 0 }, /* Pp */
	{ blk_part_imp, MDOC_PARSED | MDOC_JOIN }, /* D1 */
	{ blk_part_imp, MDOC_PARSED | MDOC_JOIN }, /* Dl */
	{ blk_full, MDOC_EXPLICIT }, /* Bd */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_JOIN }, /* Ed */
	{ blk_full, MDOC_EXPLICIT }, /* Bl */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_JOIN }, /* El */
	{ blk_full, MDOC_PARSED | MDOC_JOIN }, /* It */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ad */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* An */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ar */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Cd */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Cm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Dv */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Er */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ev */
	{ in_line_eoln, 0 }, /* Ex */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fa */
	{ in_line_eoln, 0 }, /* Fd */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fl */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fn */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ft */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Ic */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* In */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Li */
	{ blk_full, MDOC_JOIN }, /* Nd */
	{ ctx_synopsis, MDOC_CALLABLE | MDOC_PARSED }, /* Nm */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Op */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ot */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Pa */
	{ in_line_eoln, 0 }, /* Rv */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* St */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Va */
	{ ctx_synopsis, MDOC_CALLABLE | MDOC_PARSED }, /* Vt */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Xr */
	{ in_line_eoln, MDOC_JOIN }, /* %A */
	{ in_line_eoln, MDOC_JOIN }, /* %B */
	{ in_line_eoln, MDOC_JOIN }, /* %D */
	{ in_line_eoln, MDOC_JOIN }, /* %I */
	{ in_line_eoln, MDOC_JOIN }, /* %J */
	{ in_line_eoln, 0 }, /* %N */
	{ in_line_eoln, MDOC_JOIN }, /* %O */
	{ in_line_eoln, 0 }, /* %P */
	{ in_line_eoln, MDOC_JOIN }, /* %R */
	{ in_line_eoln, MDOC_JOIN }, /* %T */
	{ in_line_eoln, 0 }, /* %V */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Ac */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Ao */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Aq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* At */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Bc */
	{ blk_full, MDOC_EXPLICIT }, /* Bf */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Bo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Bq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bsx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bx */
	{ in_line_eoln, 0 }, /* Db */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Dc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Do */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Dq */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Ec */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_JOIN }, /* Ef */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Em */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Eo */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Fx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ms */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* No */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_IGNDELIM | MDOC_JOIN }, /* Ns */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Nx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ox */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Pc */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_IGNDELIM }, /* Pf */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Po */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Pq */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Qc */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Ql */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Qo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Qq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_JOIN }, /* Re */
	{ blk_full, MDOC_EXPLICIT }, /* Rs */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Sc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* So */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Sq */
	{ in_line_argn, 0 }, /* Sm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Sx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Sy */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Tn */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Ux */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Xc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Xo */
	{ blk_full, MDOC_EXPLICIT | MDOC_CALLABLE }, /* Fo */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Fc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Oo */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Oc */
	{ blk_full, MDOC_EXPLICIT }, /* Bk */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_JOIN }, /* Ek */
	{ in_line_eoln, 0 }, /* Bt */
	{ in_line_eoln, 0 }, /* Hf */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Fr */
	{ in_line_eoln, 0 }, /* Ud */
	{ in_line, 0 }, /* Lb */
	{ in_line_eoln, 0 }, /* Lp */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Lk */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Mt */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Brq */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED |
			MDOC_EXPLICIT | MDOC_JOIN }, /* Bro */
	{ blk_exp_close, MDOC_CALLABLE | MDOC_PARSED |
			 MDOC_EXPLICIT | MDOC_JOIN }, /* Brc */
	{ in_line_eoln, MDOC_JOIN }, /* %C */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Es */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* En */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Dx */
	{ in_line_eoln, MDOC_JOIN }, /* %Q */
	{ in_line_eoln, 0 }, /* br */
	{ in_line_eoln, 0 }, /* sp */
	{ in_line_eoln, 0 }, /* %U */
	{ phrase_ta, MDOC_CALLABLE | MDOC_PARSED | MDOC_JOIN }, /* Ta */
	{ in_line_eoln, MDOC_PROLOGUE }, /* ll */
};

const	struct mdoc_macro * const mdoc_macros = __mdoc_macros;


/*
 * This is called at the end of parsing.  It must traverse up the tree,
 * closing out open [implicit] scopes.  Obviously, open explicit scopes
 * are errors.
 */
void
mdoc_macroend(struct mdoc *mdoc)
{
	struct mdoc_node *n;

	/* Scan for open explicit scopes. */

	n = mdoc->last->flags & MDOC_VALID ?
	    mdoc->last->parent : mdoc->last;

	for ( ; n; n = n->parent)
		if (n->type == MDOC_BLOCK &&
		    mdoc_macros[n->tok].flags & MDOC_EXPLICIT)
			mandoc_msg(MANDOCERR_BLK_NOEND, mdoc->parse,
			    n->line, n->pos, mdoc_macronames[n->tok]);

	/* Rewind to the first. */

	rew_last(mdoc, mdoc->first);
}

/*
 * Look up the macro at *p called by "from",
 * or as a line macro if from == MDOC_MAX.
 */
static enum mdoct
lookup(enum mdoct from, const char *p)
{
	enum mdoct	 res;

	if (from == MDOC_MAX || mdoc_macros[from].flags & MDOC_PARSED) {
		res = mdoc_hash_find(p);
		if (res != MDOC_MAX && mdoc_macros[res].flags & MDOC_CALLABLE)
			return(res);
	}
	return(MDOC_MAX);
}

static void
rew_last(struct mdoc *mdoc, const struct mdoc_node *to)
{
	struct mdoc_node *n, *np;

	assert(to);
	mdoc->next = MDOC_NEXT_SIBLING;
	while (mdoc->last != to) {
		/*
		 * Save the parent here, because we may delete the
		 * mdoc->last node in the post-validation phase and reset
		 * it to mdoc->last->parent, causing a step in the closing
		 * out to be lost.
		 */
		np = mdoc->last->parent;
		mdoc_valid_post(mdoc);
		n = mdoc->last;
		mdoc->last = np;
		assert(mdoc->last);
		mdoc->last->last = n;
	}
	mdoc_valid_post(mdoc);
}

/*
 * For a block closing macro, return the corresponding opening one.
 * Otherwise, return the macro itself.
 */
static enum mdoct
rew_alt(enum mdoct tok)
{
	switch (tok) {
	case MDOC_Ac:
		return(MDOC_Ao);
	case MDOC_Bc:
		return(MDOC_Bo);
	case MDOC_Brc:
		return(MDOC_Bro);
	case MDOC_Dc:
		return(MDOC_Do);
	case MDOC_Ec:
		return(MDOC_Eo);
	case MDOC_Ed:
		return(MDOC_Bd);
	case MDOC_Ef:
		return(MDOC_Bf);
	case MDOC_Ek:
		return(MDOC_Bk);
	case MDOC_El:
		return(MDOC_Bl);
	case MDOC_Fc:
		return(MDOC_Fo);
	case MDOC_Oc:
		return(MDOC_Oo);
	case MDOC_Pc:
		return(MDOC_Po);
	case MDOC_Qc:
		return(MDOC_Qo);
	case MDOC_Re:
		return(MDOC_Rs);
	case MDOC_Sc:
		return(MDOC_So);
	case MDOC_Xc:
		return(MDOC_Xo);
	default:
		return(tok);
	}
	/* NOTREACHED */
}

/*
 * Rewinding to tok, how do we have to handle *p?
 * REWIND_NONE: *p would delimit tok, but no tok scope is open
 *   inside *p, so there is no need to rewind anything at all.
 * REWIND_THIS: *p matches tok, so rewind *p and nothing else.
 * REWIND_MORE: *p is implicit, rewind it and keep searching for tok.
 * REWIND_FORCE: *p is explicit, but tok is full, force rewinding *p.
 * REWIND_LATER: *p is explicit and still open, postpone rewinding.
 * REWIND_ERROR: No tok block is open at all.
 */
static enum rew
rew_dohalt(enum mdoct tok, enum mdoc_type type,
		const struct mdoc_node *p)
{

	/*
	 * No matching token, no delimiting block, no broken block.
	 * This can happen when full implicit macros are called for
	 * the first time but try to rewind their previous
	 * instance anyway.
	 */
	if (MDOC_ROOT == p->type)
		return(MDOC_BLOCK == type &&
		    MDOC_EXPLICIT & mdoc_macros[tok].flags ?
		    REWIND_ERROR : REWIND_NONE);

	/*
	 * When starting to rewind, skip plain text
	 * and nodes that have already been rewound.
	 */
	if (MDOC_TEXT == p->type || MDOC_VALID & p->flags)
		return(REWIND_MORE);

	/*
	 * The easiest case:  Found a matching token.
	 * This applies to both blocks and elements.
	 */
	tok = rew_alt(tok);
	if (tok == p->tok)
		return(p->end ? REWIND_NONE :
		    type == p->type ? REWIND_THIS : REWIND_MORE);

	/*
	 * While elements do require rewinding for themselves,
	 * they never affect rewinding of other nodes.
	 */
	if (MDOC_ELEM == p->type)
		return(REWIND_MORE);

	/*
	 * Blocks delimited by our target token get REWIND_MORE.
	 * Blocks delimiting our target token get REWIND_NONE.
	 */
	switch (tok) {
	case MDOC_Bl:
		if (MDOC_It == p->tok)
			return(REWIND_MORE);
		break;
	case MDOC_It:
		if (MDOC_BODY == p->type && MDOC_Bl == p->tok)
			return(REWIND_NONE);
		break;
	/*
	 * XXX Badly nested block handling still fails badly
	 * when one block is breaking two blocks of the same type.
	 * This is an incomplete and extremely ugly workaround,
	 * required to let the OpenBSD tree build.
	 */
	case MDOC_Oo:
		if (MDOC_Op == p->tok)
			return(REWIND_MORE);
		break;
	case MDOC_Nm:
		return(REWIND_NONE);
	case MDOC_Nd:
		/* FALLTHROUGH */
	case MDOC_Ss:
		if (MDOC_BODY == p->type && MDOC_Sh == p->tok)
			return(REWIND_NONE);
		/* FALLTHROUGH */
	case MDOC_Sh:
		if (MDOC_ROOT == p->parent->type)
			return(REWIND_THIS);
		if (MDOC_Nd == p->tok || MDOC_Ss == p->tok ||
		    MDOC_Sh == p->tok)
			return(REWIND_MORE);
		break;
	default:
		break;
	}

	/*
	 * Default block rewinding rules.
	 * In particular, always skip block end markers,
	 * and let all blocks rewind Nm children.
	 * Do not warn again when closing a block,
	 * since closing the body already warned.
	 */
	if (ENDBODY_NOT != p->end || MDOC_Nm == p->tok ||
	    MDOC_BLOCK == type || (MDOC_BLOCK == p->type &&
	    ! (MDOC_EXPLICIT & mdoc_macros[tok].flags)))
		return(REWIND_MORE);

	/*
	 * By default, closing out full blocks
	 * forces closing of broken explicit blocks,
	 * while closing out partial blocks
	 * allows delayed rewinding by default.
	 */
	return (&blk_full == mdoc_macros[tok].fp ?
	    REWIND_FORCE : REWIND_LATER);
}

static void
rew_elem(struct mdoc *mdoc, enum mdoct tok)
{
	struct mdoc_node *n;

	n = mdoc->last;
	if (MDOC_ELEM != n->type)
		n = n->parent;
	assert(MDOC_ELEM == n->type);
	assert(tok == n->tok);
	rew_last(mdoc, n);
}

/*
 * We are trying to close a block identified by tok,
 * but the child block *broken is still open.
 * Thus, postpone closing the tok block
 * until the rew_sub call closing *broken.
 */
static int
make_pending(struct mdoc_node *broken, enum mdoct tok,
		struct mdoc *mdoc, int line, int ppos)
{
	struct mdoc_node *breaker;

	/*
	 * Iterate backwards, searching for the block matching tok,
	 * that is, the block breaking the *broken block.
	 */
	for (breaker = broken->parent; breaker; breaker = breaker->parent) {

		/*
		 * If the *broken block had already been broken before
		 * and we encounter its breaker, make the tok block
		 * pending on the inner breaker.
		 * Graphically, "[A breaker=[B broken=[C->B B] tok=A] C]"
		 * becomes "[A broken=[B [C->B B] tok=A] C]"
		 * and finally "[A [B->A [C->B B] A] C]".
		 */
		if (breaker == broken->pending) {
			broken = breaker;
			continue;
		}

		if (REWIND_THIS != rew_dohalt(tok, MDOC_BLOCK, breaker))
			continue;
		if (MDOC_BODY == broken->type)
			broken = broken->parent;

		/*
		 * Found the breaker.
		 * If another, outer breaker is already pending on
		 * the *broken block, we must not clobber the link
		 * to the outer breaker, but make it pending on the
		 * new, now inner breaker.
		 * Graphically, "[A breaker=[B broken=[C->A A] tok=B] C]"
		 * becomes "[A breaker=[B->A broken=[C A] tok=B] C]"
		 * and finally "[A [B->A [C->B A] B] C]".
		 */
		if (broken->pending) {
			struct mdoc_node *taker;

			/*
			 * If the breaker had also been broken before,
			 * it cannot take on the outer breaker itself,
			 * but must hand it on to its own breakers.
			 * Graphically, this is the following situation:
			 * "[A [B breaker=[C->B B] broken=[D->A A] tok=C] D]"
			 * "[A taker=[B->A breaker=[C->B B] [D->C A] C] D]"
			 */
			taker = breaker;
			while (taker->pending)
				taker = taker->pending;
			taker->pending = broken->pending;
		}
		broken->pending = breaker;
		mandoc_vmsg(MANDOCERR_BLK_NEST, mdoc->parse, line, ppos,
		    "%s breaks %s", mdoc_macronames[tok],
		    mdoc_macronames[broken->tok]);
		return(1);
	}

	/*
	 * Found no matching block for tok.
	 * Are you trying to close a block that is not open?
	 */
	return(0);
}

static void
rew_sub(enum mdoc_type t, struct mdoc *mdoc,
		enum mdoct tok, int line, int ppos)
{
	struct mdoc_node *n;

	n = mdoc->last;
	while (n) {
		switch (rew_dohalt(tok, t, n)) {
		case REWIND_NONE:
			return;
		case REWIND_THIS:
			n->lastline = line -
			    (mdoc->flags & MDOC_NEWLINE &&
			     ! (mdoc_macros[tok].flags & MDOC_EXPLICIT));
			break;
		case REWIND_FORCE:
			mandoc_vmsg(MANDOCERR_BLK_BROKEN, mdoc->parse,
			    line, ppos, "%s breaks %s",
			    mdoc_macronames[tok],
			    mdoc_macronames[n->tok]);
			/* FALLTHROUGH */
		case REWIND_MORE:
			n->lastline = line -
			    (mdoc->flags & MDOC_NEWLINE ? 1 : 0);
			n = n->parent;
			continue;
		case REWIND_LATER:
			if (make_pending(n, tok, mdoc, line, ppos) ||
			    t != MDOC_BLOCK)
				return;
			/* FALLTHROUGH */
		case REWIND_ERROR:
			mandoc_msg(MANDOCERR_BLK_NOTOPEN,
			    mdoc->parse, line, ppos,
			    mdoc_macronames[tok]);
			return;
		}
		break;
	}
	assert(n);
	rew_last(mdoc, n);

	/*
	 * The current block extends an enclosing block.
	 * Now that the current block ends, close the enclosing block, too.
	 */
	while ((n = n->pending) != NULL) {
		rew_last(mdoc, n);
		if (n->type == MDOC_HEAD)
			mdoc_body_alloc(mdoc, n->line, n->pos, n->tok);
	}
}

/*
 * Allocate a word and check whether it's punctuation or not.
 * Punctuation consists of those tokens found in mdoc_isdelim().
 */
static void
dword(struct mdoc *mdoc, int line, int col, const char *p,
		enum mdelim d, int may_append)
{

	if (d == DELIM_MAX)
		d = mdoc_isdelim(p);

	if (may_append &&
	    ! (mdoc->flags & (MDOC_SYNOPSIS | MDOC_KEEP | MDOC_SMOFF)) &&
	    d == DELIM_NONE && mdoc->last->type == MDOC_TEXT &&
	    mdoc_isdelim(mdoc->last->string) == DELIM_NONE) {
		mdoc_word_append(mdoc, p);
		return;
	}

	mdoc_word_alloc(mdoc, line, col, p);

	/*
	 * If the word consists of a bare delimiter,
	 * flag the new node accordingly,
	 * unless doing so was vetoed by the invoking macro.
	 * Always clear the veto, it is only valid for one word.
	 */

	if (d == DELIM_OPEN)
		mdoc->last->flags |= MDOC_DELIMO;
	else if (d == DELIM_CLOSE &&
	    ! (mdoc->flags & MDOC_NODELIMC) &&
	    mdoc->last->parent->tok != MDOC_Fd)
		mdoc->last->flags |= MDOC_DELIMC;
	mdoc->flags &= ~MDOC_NODELIMC;
}

static void
append_delims(struct mdoc *mdoc, int line, int *pos, char *buf)
{
	char		*p;
	int		 la;

	if (buf[*pos] == '\0')
		return;

	for (;;) {
		la = *pos;
		if (mdoc_args(mdoc, line, pos, buf, MDOC_MAX, &p) == ARGS_EOLN)
			break;
		dword(mdoc, line, la, p, DELIM_MAX, 1);

		/*
		 * If we encounter end-of-sentence symbols, then trigger
		 * the double-space.
		 *
		 * XXX: it's easy to allow this to propagate outward to
		 * the last symbol, such that `. )' will cause the
		 * correct double-spacing.  However, (1) groff isn't
		 * smart enough to do this and (2) it would require
		 * knowing which symbols break this behaviour, for
		 * example, `.  ;' shouldn't propagate the double-space.
		 */

		if (mandoc_eos(p, strlen(p)))
			mdoc->last->flags |= MDOC_EOS;
	}
}

/*
 * Parse one word.
 * If it is a macro, call it and return 1.
 * Otherwise, allocate it and return 0.
 */
static int
macro_or_word(MACRO_PROT_ARGS, int parsed)
{
	char		*p;
	enum mdoct	 ntok;

	p = buf + ppos;
	ntok = MDOC_MAX;
	if (mdoc->flags & MDOC_PHRASELIT)
		/* nothing */;
	else if (*p == '"')
		p++;
	else if (parsed)
		ntok = lookup(tok, p);

	if (ntok == MDOC_MAX) {
		dword(mdoc, line, ppos, p, DELIM_MAX, tok == MDOC_MAX ||
		    mdoc_macros[tok].flags & MDOC_JOIN);
		return(0);
	} else {
		if (mdoc_macros[tok].fp == in_line_eoln)
			rew_elem(mdoc, tok);
		mdoc_macro(mdoc, ntok, line, ppos, pos, buf);
		if (tok == MDOC_MAX)
			append_delims(mdoc, line, pos, buf);
		return(1);
	}
}

/*
 * Close out block partial/full explicit.
 */
static void
blk_exp_close(MACRO_PROT_ARGS)
{
	struct mdoc_node *body;		/* Our own body. */
	struct mdoc_node *endbody;	/* Our own end marker. */
	struct mdoc_node *later;	/* A sub-block starting later. */
	struct mdoc_node *n;		/* For searching backwards. */

	int		 j, lastarg, maxargs, flushed, nl;
	enum margserr	 ac;
	enum mdoct	 atok, ntok;
	char		*p;

	nl = MDOC_NEWLINE & mdoc->flags;

	switch (tok) {
	case MDOC_Ec:
		maxargs = 1;
		break;
	case MDOC_Ek:
		mdoc->flags &= ~MDOC_KEEP;
		/* FALLTHROUGH */
	default:
		maxargs = 0;
		break;
	}

	/*
	 * Search backwards for beginnings of blocks,
	 * both of our own and of pending sub-blocks.
	 */

	atok = rew_alt(tok);
	body = endbody = later = NULL;
	for (n = mdoc->last; n; n = n->parent) {
		if (n->flags & MDOC_VALID)
			continue;

		/* Remember the start of our own body. */

		if (n->type == MDOC_BODY && atok == n->tok) {
			if (n->end == ENDBODY_NOT)
				body = n;
			continue;
		}

		if (n->type != MDOC_BLOCK || n->tok == MDOC_Nm)
			continue;
		if (atok == n->tok) {
			assert(body);

			/*
			 * Found the start of our own block.
			 * When there is no pending sub block,
			 * just proceed to closing out.
			 */

			if (later == NULL)
				break;

			/*
			 * When there is a pending sub block,
			 * postpone closing out the current block
			 * until the rew_sub() closing out the sub-block.
			 */

			make_pending(later, tok, mdoc, line, ppos);

			/*
			 * Mark the place where the formatting - but not
			 * the scope - of the current block ends.
			 */

			mdoc_endbody_alloc(mdoc, line, ppos,
			    atok, body, ENDBODY_SPACE);

			/*
			 * If a block closing macro taking arguments
			 * breaks another block, put the arguments
			 * into the end marker and remeber the
			 * end marker in order to close it out.
			 */

			if (maxargs) {
				endbody = mdoc->last;
				mdoc->next = MDOC_NEXT_CHILD;
			}
			break;
		}

		/*
		 * When finding an open sub block, remember the last
		 * open explicit block, or, in case there are only
		 * implicit ones, the first open implicit block.
		 */

		if (later &&
		    mdoc_macros[later->tok].flags & MDOC_EXPLICIT)
			continue;
		if (n->tok != MDOC_It)
			later = n;
	}
	rew_sub(MDOC_BODY, mdoc, tok, line, ppos);

	if ( ! (mdoc_macros[tok].flags & MDOC_PARSED)) {
		if (buf[*pos] != '\0')
			mandoc_vmsg(MANDOCERR_ARG_SKIP,
			    mdoc->parse, line, ppos,
			    "%s %s", mdoc_macronames[tok],
			    buf + *pos);
		rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);
		return;
	}

	if (maxargs && endbody == NULL) {
		if (n == NULL) {
			/*
			 * Stray .Ec without previous .Eo:
			 * Break the output line, ignore any arguments.
			 */
			mdoc_elem_alloc(mdoc, line, ppos, MDOC_br, NULL);
			rew_elem(mdoc, MDOC_br);
		} else
			mdoc_tail_alloc(mdoc, line, ppos, atok);
	}

	flushed = n == NULL;
	for (j = 0; ; j++) {
		lastarg = *pos;

		if (j == maxargs && ! flushed) {
			if (endbody == NULL)
				rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);
			else
				rew_last(mdoc, endbody);
			flushed = 1;
		}

		ac = mdoc_args(mdoc, line, pos, buf, tok, &p);
		if (ac == ARGS_PUNCT || ac == ARGS_EOLN)
			break;

		ntok = ac == ARGS_QWORD ? MDOC_MAX : lookup(tok, p);

		if (ntok == MDOC_MAX) {
			dword(mdoc, line, lastarg, p, DELIM_MAX,
			    MDOC_JOIN & mdoc_macros[tok].flags);
			continue;
		}

		if ( ! flushed) {
			if (endbody == NULL)
				rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);
			else
				rew_last(mdoc, endbody);
			flushed = 1;
		}
		mdoc->flags &= ~MDOC_NEWLINE;
		mdoc_macro(mdoc, ntok, line, lastarg, pos, buf);
		break;
	}

	if ( ! flushed) {
		if (endbody == NULL)
			rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);
		else
			rew_last(mdoc, endbody);
	}
	if (nl)
		append_delims(mdoc, line, pos, buf);
}

static void
in_line(MACRO_PROT_ARGS)
{
	int		 la, scope, cnt, firstarg, mayopen, nc, nl;
	enum mdoct	 ntok;
	enum margserr	 ac;
	enum mdelim	 d;
	struct mdoc_arg	*arg;
	char		*p;

	nl = MDOC_NEWLINE & mdoc->flags;

	/*
	 * Whether we allow ignored elements (those without content,
	 * usually because of reserved words) to squeak by.
	 */

	switch (tok) {
	case MDOC_An:
		/* FALLTHROUGH */
	case MDOC_Ar:
		/* FALLTHROUGH */
	case MDOC_Fl:
		/* FALLTHROUGH */
	case MDOC_Mt:
		/* FALLTHROUGH */
	case MDOC_Nm:
		/* FALLTHROUGH */
	case MDOC_Pa:
		nc = 1;
		break;
	default:
		nc = 0;
		break;
	}

	mdoc_argv(mdoc, line, tok, &arg, pos, buf);

	d = DELIM_NONE;
	firstarg = 1;
	mayopen = 1;
	for (cnt = scope = 0;; ) {
		la = *pos;
		ac = mdoc_args(mdoc, line, pos, buf, tok, &p);

		/*
		 * At the end of a macro line,
		 * opening delimiters do not suppress spacing.
		 */

		if (ac == ARGS_EOLN) {
			if (d == DELIM_OPEN)
				mdoc->last->flags &= ~MDOC_DELIMO;
			break;
		}

		/*
		 * The rest of the macro line is only punctuation,
		 * to be handled by append_delims().
		 * If there were no other arguments,
		 * do not allow the first one to suppress spacing,
		 * even if it turns out to be a closing one.
		 */

		if (ac == ARGS_PUNCT) {
			if (cnt == 0 && nc == 0)
				mdoc->flags |= MDOC_NODELIMC;
			break;
		}

		ntok = (ac == ARGS_QWORD || (tok == MDOC_Fn && !cnt)) ?
		    MDOC_MAX : lookup(tok, p);

		/*
		 * In this case, we've located a submacro and must
		 * execute it.  Close out scope, if open.  If no
		 * elements have been generated, either create one (nc)
		 * or raise a warning.
		 */

		if (ntok != MDOC_MAX) {
			if (scope)
				rew_elem(mdoc, tok);
			if (nc && ! cnt) {
				mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
				rew_last(mdoc, mdoc->last);
			} else if ( ! nc && ! cnt) {
				mdoc_argv_free(arg);
				mandoc_msg(MANDOCERR_MACRO_EMPTY,
				    mdoc->parse, line, ppos,
				    mdoc_macronames[tok]);
			}
			mdoc_macro(mdoc, ntok, line, la, pos, buf);
			if (nl)
				append_delims(mdoc, line, pos, buf);
			return;
		}

		/*
		 * Non-quote-enclosed punctuation.  Set up our scope, if
		 * a word; rewind the scope, if a delimiter; then append
		 * the word.
		 */

		d = ac == ARGS_QWORD ? DELIM_NONE : mdoc_isdelim(p);

		if (DELIM_NONE != d) {
			/*
			 * If we encounter closing punctuation, no word
			 * has been emitted, no scope is open, and we're
			 * allowed to have an empty element, then start
			 * a new scope.
			 */
			if ((d == DELIM_CLOSE ||
			     (d == DELIM_MIDDLE && tok == MDOC_Fl)) &&
			    !cnt && !scope && nc && mayopen) {
				mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
				scope = 1;
				cnt++;
				if (tok == MDOC_Nm)
					mayopen = 0;
			}
			/*
			 * Close out our scope, if one is open, before
			 * any punctuation.
			 */
			if (scope)
				rew_elem(mdoc, tok);
			scope = 0;
			if (tok == MDOC_Fn)
				mayopen = 0;
		} else if (mayopen && !scope) {
			mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
			scope = 1;
			cnt++;
		}

		dword(mdoc, line, la, p, d,
		    MDOC_JOIN & mdoc_macros[tok].flags);

		/*
		 * If the first argument is a closing delimiter,
		 * do not suppress spacing before it.
		 */

		if (firstarg && d == DELIM_CLOSE && !nc)
			mdoc->last->flags &= ~MDOC_DELIMC;
		firstarg = 0;

		/*
		 * `Fl' macros have their scope re-opened with each new
		 * word so that the `-' can be added to each one without
		 * having to parse out spaces.
		 */
		if (scope && tok == MDOC_Fl) {
			rew_elem(mdoc, tok);
			scope = 0;
		}
	}

	if (scope)
		rew_elem(mdoc, tok);

	/*
	 * If no elements have been collected and we're allowed to have
	 * empties (nc), open a scope and close it out.  Otherwise,
	 * raise a warning.
	 */

	if ( ! cnt) {
		if (nc) {
			mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
			rew_last(mdoc, mdoc->last);
		} else {
			mdoc_argv_free(arg);
			mandoc_msg(MANDOCERR_MACRO_EMPTY, mdoc->parse,
			    line, ppos, mdoc_macronames[tok]);
		}
	}
	if (nl)
		append_delims(mdoc, line, pos, buf);
}

static void
blk_full(MACRO_PROT_ARGS)
{
	int		  la, nl, parsed;
	struct mdoc_arg	 *arg;
	struct mdoc_node *head; /* save of head macro */
	struct mdoc_node *body; /* save of body macro */
	struct mdoc_node *n;
	enum margserr	  ac, lac;
	char		 *p;

	nl = MDOC_NEWLINE & mdoc->flags;

	/* Skip items outside lists. */

	if (tok == MDOC_It) {
		for (n = mdoc->last; n; n = n->parent)
			if (n->tok == MDOC_Bl &&
			    ! (n->flags & MDOC_VALID))
				break;
		if (n == NULL) {
			mandoc_vmsg(MANDOCERR_IT_STRAY, mdoc->parse,
			    line, ppos, "It %s", buf + *pos);
			mdoc_elem_alloc(mdoc, line, ppos, MDOC_br, NULL);
			rew_elem(mdoc, MDOC_br);
			return;
		}
	}

	/* Close out prior implicit scope. */

	if ( ! (mdoc_macros[tok].flags & MDOC_EXPLICIT)) {
		rew_sub(MDOC_BODY, mdoc, tok, line, ppos);
		rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);
	}

	/*
	 * This routine accommodates implicitly- and explicitly-scoped
	 * macro openings.  Implicit ones first close out prior scope
	 * (seen above).  Delay opening the head until necessary to
	 * allow leading punctuation to print.  Special consideration
	 * for `It -column', which has phrase-part syntax instead of
	 * regular child nodes.
	 */

	mdoc_argv(mdoc, line, tok, &arg, pos, buf);
	mdoc_block_alloc(mdoc, line, ppos, tok, arg);
	head = body = NULL;

	/*
	 * Exception: Heads of `It' macros in `-diag' lists are not
	 * parsed, even though `It' macros in general are parsed.
	 */

	parsed = tok != MDOC_It ||
	    mdoc->last->parent->tok != MDOC_Bl ||
	    mdoc->last->parent->norm->Bl.type != LIST_diag;

	/*
	 * The `Nd' macro has all arguments in its body: it's a hybrid
	 * of block partial-explicit and full-implicit.  Stupid.
	 */

	if (tok == MDOC_Nd) {
		head = mdoc_head_alloc(mdoc, line, ppos, tok);
		rew_sub(MDOC_HEAD, mdoc, tok, line, ppos);
		body = mdoc_body_alloc(mdoc, line, ppos, tok);
	}

	if (tok == MDOC_Bk)
		mdoc->flags |= MDOC_KEEP;

	ac = ARGS_PEND;
	for (;;) {
		la = *pos;
		lac = ac;
		ac = mdoc_args(mdoc, line, pos, buf, tok, &p);
		if (ac == ARGS_PUNCT)
			break;
		if (ac == ARGS_EOLN) {
			if (lac != ARGS_PPHRASE && lac != ARGS_PHRASE)
				break;
			/*
			 * This is necessary: if the last token on a
			 * line is a `Ta' or tab, then we'll get
			 * ARGS_EOLN, so we must be smart enough to
			 * reopen our scope if the last parse was a
			 * phrase or partial phrase.
			 */
			rew_sub(MDOC_BODY, mdoc, tok, line, ppos);
			body = mdoc_body_alloc(mdoc, line, ppos, tok);
			break;
		}

		/*
		 * Emit leading punctuation (i.e., punctuation before
		 * the MDOC_HEAD) for non-phrase types.
		 */

		if (head == NULL &&
		    ac != ARGS_PEND &&
		    ac != ARGS_PHRASE &&
		    ac != ARGS_PPHRASE &&
		    ac != ARGS_QWORD &&
		    mdoc_isdelim(p) == DELIM_OPEN) {
			dword(mdoc, line, la, p, DELIM_OPEN, 0);
			continue;
		}

		/* Open a head if one hasn't been opened. */

		if (head == NULL)
			head = mdoc_head_alloc(mdoc, line, ppos, tok);

		if (ac == ARGS_PHRASE ||
		    ac == ARGS_PEND ||
		    ac == ARGS_PPHRASE) {

			/*
			 * If we haven't opened a body yet, rewind the
			 * head; if we have, rewind that instead.
			 */

			rew_sub(body ? MDOC_BODY : MDOC_HEAD,
			    mdoc, tok, line, ppos);
			body = mdoc_body_alloc(mdoc, line, ppos, tok);

			/*
			 * Process phrases: set whether we're in a
			 * partial-phrase (this effects line handling)
			 * then call down into the phrase parser.
			 */

			if (ac == ARGS_PPHRASE)
				mdoc->flags |= MDOC_PPHRASE;
			if (ac == ARGS_PEND && lac == ARGS_PPHRASE)
				mdoc->flags |= MDOC_PPHRASE;
			parse_rest(mdoc, MDOC_MAX, line, &la, buf);
			mdoc->flags &= ~MDOC_PPHRASE;
			continue;
		}

		if (macro_or_word(mdoc, tok, line, la, pos, buf, parsed))
			break;
	}

	if (head == NULL)
		head = mdoc_head_alloc(mdoc, line, ppos, tok);
	if (nl)
		append_delims(mdoc, line, pos, buf);
	if (body != NULL)
		goto out;

	/*
	 * If there is an open (i.e., unvalidated) sub-block requiring
	 * explicit close-out, postpone switching the current block from
	 * head to body until the rew_sub() call closing out that
	 * sub-block.
	 */
	for (n = mdoc->last; n && n != head; n = n->parent) {
		if (n->type == MDOC_BLOCK &&
		    mdoc_macros[n->tok].flags & MDOC_EXPLICIT &&
		    ! (n->flags & MDOC_VALID)) {
			n->pending = head;
			return;
		}
	}

	/* Close out scopes to remain in a consistent state. */

	rew_sub(MDOC_HEAD, mdoc, tok, line, ppos);
	mdoc_body_alloc(mdoc, line, ppos, tok);
out:
	if (mdoc->flags & MDOC_FREECOL) {
		rew_sub(MDOC_BODY, mdoc, tok, line, ppos);
		rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);
		mdoc->flags &= ~MDOC_FREECOL;
	}
}

static void
blk_part_imp(MACRO_PROT_ARGS)
{
	int		  la, nl;
	enum margserr	  ac;
	char		 *p;
	struct mdoc_node *blk; /* saved block context */
	struct mdoc_node *body; /* saved body context */
	struct mdoc_node *n;

	nl = MDOC_NEWLINE & mdoc->flags;

	/*
	 * A macro that spans to the end of the line.  This is generally
	 * (but not necessarily) called as the first macro.  The block
	 * has a head as the immediate child, which is always empty,
	 * followed by zero or more opening punctuation nodes, then the
	 * body (which may be empty, depending on the macro), then zero
	 * or more closing punctuation nodes.
	 */

	blk = mdoc_block_alloc(mdoc, line, ppos, tok, NULL);
	mdoc_head_alloc(mdoc, line, ppos, tok);
	rew_sub(MDOC_HEAD, mdoc, tok, line, ppos);

	/*
	 * Open the body scope "on-demand", that is, after we've
	 * processed all our the leading delimiters (open parenthesis,
	 * etc.).
	 */

	for (body = NULL; ; ) {
		la = *pos;
		ac = mdoc_args(mdoc, line, pos, buf, tok, &p);
		if (ac == ARGS_EOLN || ac == ARGS_PUNCT)
			break;

		if (body == NULL && ac != ARGS_QWORD &&
		    mdoc_isdelim(p) == DELIM_OPEN) {
			dword(mdoc, line, la, p, DELIM_OPEN, 0);
			continue;
		}

		if (body == NULL)
			body = mdoc_body_alloc(mdoc, line, ppos, tok);

		if (macro_or_word(mdoc, tok, line, la, pos, buf, 1))
			break;
	}
	if (body == NULL)
		body = mdoc_body_alloc(mdoc, line, ppos, tok);

	/*
	 * If there is an open sub-block requiring explicit close-out,
	 * postpone closing out the current block
	 * until the rew_sub() call closing out the sub-block.
	 */

	for (n = mdoc->last; n && n != body && n != blk->parent;
	     n = n->parent) {
		if (n->type == MDOC_BLOCK &&
		    mdoc_macros[n->tok].flags & MDOC_EXPLICIT &&
		    ! (n->flags & MDOC_VALID)) {
			make_pending(n, tok, mdoc, line, ppos);
			mdoc_endbody_alloc(mdoc, line, ppos,
			    tok, body, ENDBODY_NOSPACE);
			return;
		}
	}
	assert(n == body);
	rew_sub(MDOC_BODY, mdoc, tok, line, ppos);
	if (nl)
		append_delims(mdoc, line, pos, buf);
	rew_sub(MDOC_BLOCK, mdoc, tok, line, ppos);

	/* Move trailing .Ns out of scope. */

	for (n = body->child; n && n->next; n = n->next)
		/* Do nothing. */ ;
	if (n && n->tok == MDOC_Ns)
		mdoc_node_relink(mdoc, n);
}

static void
blk_part_exp(MACRO_PROT_ARGS)
{
	int		  la, nl;
	enum margserr	  ac;
	struct mdoc_node *head; /* keep track of head */
	struct mdoc_node *body; /* keep track of body */
	char		 *p;

	nl = MDOC_NEWLINE & mdoc->flags;

	/*
	 * The opening of an explicit macro having zero or more leading
	 * punctuation nodes; a head with optional single element (the
	 * case of `Eo'); and a body that may be empty.
	 */

	mdoc_block_alloc(mdoc, line, ppos, tok, NULL);
	for (head = body = NULL; ; ) {
		la = *pos;
		ac = mdoc_args(mdoc, line, pos, buf, tok, &p);
		if (ac == ARGS_PUNCT || ac == ARGS_EOLN)
			break;

		/* Flush out leading punctuation. */

		if (head == NULL && ac != ARGS_QWORD &&
		    mdoc_isdelim(p) == DELIM_OPEN) {
			assert(NULL == body);
			dword(mdoc, line, la, p, DELIM_OPEN, 0);
			continue;
		}

		if (head == NULL) {
			assert(body == NULL);
			head = mdoc_head_alloc(mdoc, line, ppos, tok);
		}

		/*
		 * `Eo' gobbles any data into the head, but most other
		 * macros just immediately close out and begin the body.
		 */

		if (body == NULL) {
			assert(head);
			/* No check whether it's a macro! */
			if (tok == MDOC_Eo)
				dword(mdoc, line, la, p, DELIM_MAX, 0);
			rew_sub(MDOC_HEAD, mdoc, tok, line, ppos);
			body = mdoc_body_alloc(mdoc, line, ppos, tok);
			if (tok == MDOC_Eo)
				continue;
		}
		assert(head != NULL && body != NULL);

		if (macro_or_word(mdoc, tok, line, la, pos, buf, 1))
			break;
	}

	/* Clean-up to leave in a consistent state. */

	if (head == NULL)
		mdoc_head_alloc(mdoc, line, ppos, tok);

	if (body == NULL) {
		rew_sub(MDOC_HEAD, mdoc, tok, line, ppos);
		mdoc_body_alloc(mdoc, line, ppos, tok);
	}
	if (nl)
		append_delims(mdoc, line, pos, buf);
}

static void
in_line_argn(MACRO_PROT_ARGS)
{
	int		 la, flushed, j, maxargs, nl;
	enum margserr	 ac;
	struct mdoc_arg	*arg;
	char		*p;
	enum mdoct	 ntok;

	nl = MDOC_NEWLINE & mdoc->flags;

	/*
	 * A line macro that has a fixed number of arguments (maxargs).
	 * Only open the scope once the first non-leading-punctuation is
	 * found (unless MDOC_IGNDELIM is noted, like in `Pf'), then
	 * keep it open until the maximum number of arguments are
	 * exhausted.
	 */

	switch (tok) {
	case MDOC_Ap:
		/* FALLTHROUGH */
	case MDOC_Ns:
		/* FALLTHROUGH */
	case MDOC_Ux:
		maxargs = 0;
		break;
	case MDOC_Bx:
		/* FALLTHROUGH */
	case MDOC_Es:
		/* FALLTHROUGH */
	case MDOC_Xr:
		maxargs = 2;
		break;
	default:
		maxargs = 1;
		break;
	}

	mdoc_argv(mdoc, line, tok, &arg, pos, buf);

	for (flushed = j = 0; ; ) {
		la = *pos;
		ac = mdoc_args(mdoc, line, pos, buf, tok, &p);
		if (ac == ARGS_PUNCT || ac == ARGS_EOLN)
			break;

		if ( ! (mdoc_macros[tok].flags & MDOC_IGNDELIM) &&
		    ac != ARGS_QWORD && j == 0 &&
		    mdoc_isdelim(p) == DELIM_OPEN) {
			dword(mdoc, line, la, p, DELIM_OPEN, 0);
			continue;
		} else if (j == 0)
		       mdoc_elem_alloc(mdoc, line, ppos, tok, arg);

		if (j == maxargs && ! flushed) {
			rew_elem(mdoc, tok);
			flushed = 1;
		}

		ntok = ac == ARGS_QWORD ? MDOC_MAX : lookup(tok, p);

		if (ntok != MDOC_MAX) {
			if ( ! flushed)
				rew_elem(mdoc, tok);
			flushed = 1;
			mdoc_macro(mdoc, ntok, line, la, pos, buf);
			j++;
			break;
		}

		if ( ! (mdoc_macros[tok].flags & MDOC_IGNDELIM) &&
		    ac != ARGS_QWORD && ! flushed &&
		    mdoc_isdelim(p) != DELIM_NONE) {
			rew_elem(mdoc, tok);
			flushed = 1;
		}

		dword(mdoc, line, la, p, DELIM_MAX,
		    MDOC_JOIN & mdoc_macros[tok].flags);
		j++;
	}

	if (j == 0)
		mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
	if ( ! flushed)
		rew_elem(mdoc, tok);
	if (nl)
		append_delims(mdoc, line, pos, buf);
}

static void
in_line_eoln(MACRO_PROT_ARGS)
{
	struct mdoc_arg	*arg;

	if (tok == MDOC_Pp)
		rew_sub(MDOC_BLOCK, mdoc, MDOC_Nm, line, ppos);

	mdoc_argv(mdoc, line, tok, &arg, pos, buf);
	mdoc_elem_alloc(mdoc, line, ppos, tok, arg);
	if (parse_rest(mdoc, tok, line, pos, buf))
		return;
	rew_elem(mdoc, tok);
}

/*
 * The simplest argument parser available: Parse the remaining
 * words until the end of the phrase or line and return 0
 * or until the next macro, call that macro, and return 1.
 */
static int
parse_rest(struct mdoc *mdoc, enum mdoct tok, int line, int *pos, char *buf)
{
	int		 la;

	for (;;) {
		la = *pos;
		if (mdoc_args(mdoc, line, pos, buf, tok, NULL) == ARGS_EOLN)
			return(0);
		if (macro_or_word(mdoc, tok, line, la, pos, buf, 1))
			return(1);
	}
}

static void
ctx_synopsis(MACRO_PROT_ARGS)
{

	if (~mdoc->flags & (MDOC_SYNOPSIS | MDOC_NEWLINE))
		in_line(mdoc, tok, line, ppos, pos, buf);
	else if (tok == MDOC_Nm)
		blk_full(mdoc, tok, line, ppos, pos, buf);
	else {
		assert(tok == MDOC_Vt);
		blk_part_imp(mdoc, tok, line, ppos, pos, buf);
	}
}

/*
 * Phrases occur within `Bl -column' entries, separated by `Ta' or tabs.
 * They're unusual because they're basically free-form text until a
 * macro is encountered.
 */
static void
phrase_ta(MACRO_PROT_ARGS)
{
	struct mdoc_node *n;

	/* Make sure we are in a column list or ignore this macro. */

	n = mdoc->last;
	while (n != NULL && n->tok != MDOC_Bl)
		n = n->parent;
	if (n == NULL || n->norm->Bl.type != LIST_column) {
		mandoc_msg(MANDOCERR_TA_STRAY, mdoc->parse,
		    line, ppos, "Ta");
		return;
	}

	/* Advance to the next column. */

	rew_sub(MDOC_BODY, mdoc, MDOC_It, line, ppos);
	mdoc_body_alloc(mdoc, line, ppos, MDOC_It);
	parse_rest(mdoc, MDOC_MAX, line, pos, buf);
}
