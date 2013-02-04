/*	$Id: mdoc_macro.c,v 1.115 2012/01/05 00:43:51 schwarze Exp $ */
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

static	int	  	blk_full(MACRO_PROT_ARGS);
static	int	  	blk_exp_close(MACRO_PROT_ARGS);
static	int	  	blk_part_exp(MACRO_PROT_ARGS);
static	int	  	blk_part_imp(MACRO_PROT_ARGS);
static	int	  	ctx_synopsis(MACRO_PROT_ARGS);
static	int	  	in_line_eoln(MACRO_PROT_ARGS);
static	int	  	in_line_argn(MACRO_PROT_ARGS);
static	int	  	in_line(MACRO_PROT_ARGS);
static	int	  	obsolete(MACRO_PROT_ARGS);
static	int	  	phrase_ta(MACRO_PROT_ARGS);

static	int		dword(struct mdoc *, int, int, 
				const char *, enum mdelim);
static	int	  	append_delims(struct mdoc *, 
				int, int *, char *);
static	enum mdoct	lookup(enum mdoct, const char *);
static	enum mdoct	lookup_raw(const char *);
static	int		make_pending(struct mdoc_node *, enum mdoct,
				struct mdoc *, int, int);
static	int	  	phrase(struct mdoc *, int, int, char *);
static	enum mdoct 	rew_alt(enum mdoct);
static	enum rew  	rew_dohalt(enum mdoct, enum mdoc_type, 
				const struct mdoc_node *);
static	int	  	rew_elem(struct mdoc *, enum mdoct);
static	int	  	rew_last(struct mdoc *, 
				const struct mdoc_node *);
static	int	  	rew_sub(enum mdoc_type, struct mdoc *, 
				enum mdoct, int, int);

const	struct mdoc_macro __mdoc_macros[MDOC_MAX] = {
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ap */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dd */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Dt */
	{ in_line_eoln, MDOC_PROLOGUE }, /* Os */
	{ blk_full, MDOC_PARSED }, /* Sh */
	{ blk_full, MDOC_PARSED }, /* Ss */ 
	{ in_line_eoln, 0 }, /* Pp */ 
	{ blk_part_imp, MDOC_PARSED }, /* D1 */
	{ blk_part_imp, MDOC_PARSED }, /* Dl */
	{ blk_full, MDOC_EXPLICIT }, /* Bd */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ed */
	{ blk_full, MDOC_EXPLICIT }, /* Bl */
	{ blk_exp_close, MDOC_EXPLICIT }, /* El */
	{ blk_full, MDOC_PARSED }, /* It */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ad */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* An */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ar */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Cd */
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
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ic */ 
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* In */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Li */
	{ blk_full, 0 }, /* Nd */ 
	{ ctx_synopsis, MDOC_CALLABLE | MDOC_PARSED }, /* Nm */ 
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Op */
	{ obsolete, 0 }, /* Ot */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Pa */
	{ in_line_eoln, 0 }, /* Rv */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* St */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Va */
	{ ctx_synopsis, MDOC_CALLABLE | MDOC_PARSED }, /* Vt */ 
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Xr */
	{ in_line_eoln, 0 }, /* %A */
	{ in_line_eoln, 0 }, /* %B */
	{ in_line_eoln, 0 }, /* %D */
	{ in_line_eoln, 0 }, /* %I */
	{ in_line_eoln, 0 }, /* %J */
	{ in_line_eoln, 0 }, /* %N */
	{ in_line_eoln, 0 }, /* %O */
	{ in_line_eoln, 0 }, /* %P */
	{ in_line_eoln, 0 }, /* %R */
	{ in_line_eoln, 0 }, /* %T */
	{ in_line_eoln, 0 }, /* %V */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Ac */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Ao */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Aq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* At */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Bc */
	{ blk_full, MDOC_EXPLICIT }, /* Bf */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Bo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Bq */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bsx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Bx */
	{ in_line_eoln, 0 }, /* Db */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Dc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Do */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Dq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Ec */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ef */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Em */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Eo */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Fx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Ms */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_IGNDELIM }, /* No */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_IGNDELIM }, /* Ns */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Nx */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ox */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Pc */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED | MDOC_IGNDELIM }, /* Pf */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Po */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Pq */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Qc */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Ql */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Qo */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Qq */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Re */
	{ blk_full, MDOC_EXPLICIT }, /* Rs */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Sc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* So */
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Sq */
	{ in_line_eoln, 0 }, /* Sm */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Sx */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Sy */
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Tn */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Ux */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Xc */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Xo */
	{ blk_full, MDOC_EXPLICIT | MDOC_CALLABLE }, /* Fo */ 
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Fc */ 
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Oo */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Oc */
	{ blk_full, MDOC_EXPLICIT }, /* Bk */
	{ blk_exp_close, MDOC_EXPLICIT }, /* Ek */
	{ in_line_eoln, 0 }, /* Bt */
	{ in_line_eoln, 0 }, /* Hf */
	{ obsolete, 0 }, /* Fr */
	{ in_line_eoln, 0 }, /* Ud */
	{ in_line, 0 }, /* Lb */
	{ in_line_eoln, 0 }, /* Lp */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Lk */ 
	{ in_line, MDOC_CALLABLE | MDOC_PARSED }, /* Mt */ 
	{ blk_part_imp, MDOC_CALLABLE | MDOC_PARSED }, /* Brq */
	{ blk_part_exp, MDOC_CALLABLE | MDOC_PARSED | MDOC_EXPLICIT }, /* Bro */
	{ blk_exp_close, MDOC_EXPLICIT | MDOC_CALLABLE | MDOC_PARSED }, /* Brc */
	{ in_line_eoln, 0 }, /* %C */
	{ obsolete, 0 }, /* Es */
	{ obsolete, 0 }, /* En */
	{ in_line_argn, MDOC_CALLABLE | MDOC_PARSED }, /* Dx */
	{ in_line_eoln, 0 }, /* %Q */
	{ in_line_eoln, 0 }, /* br */
	{ in_line_eoln, 0 }, /* sp */
	{ in_line_eoln, 0 }, /* %U */
	{ phrase_ta, MDOC_CALLABLE | MDOC_PARSED }, /* Ta */
};

const	struct mdoc_macro * const mdoc_macros = __mdoc_macros;


/*
 * This is called at the end of parsing.  It must traverse up the tree,
 * closing out open [implicit] scopes.  Obviously, open explicit scopes
 * are errors.
 */
int
mdoc_macroend(struct mdoc *m)
{
	struct mdoc_node *n;

	/* Scan for open explicit scopes. */

	n = MDOC_VALID & m->last->flags ?  m->last->parent : m->last;

	for ( ; n; n = n->parent)
		if (MDOC_BLOCK == n->type &&
		    MDOC_EXPLICIT & mdoc_macros[n->tok].flags)
			mdoc_nmsg(m, n, MANDOCERR_SCOPEEXIT);

	/* Rewind to the first. */

	return(rew_last(m, m->first));
}


/*
 * Look up a macro from within a subsequent context.
 */
static enum mdoct
lookup(enum mdoct from, const char *p)
{

	if ( ! (MDOC_PARSED & mdoc_macros[from].flags))
		return(MDOC_MAX);
	return(lookup_raw(p));
}


/*
 * Lookup a macro following the initial line macro.
 */
static enum mdoct
lookup_raw(const char *p)
{
	enum mdoct	 res;

	if (MDOC_MAX == (res = mdoc_hash_find(p)))
		return(MDOC_MAX);
	if (MDOC_CALLABLE & mdoc_macros[res].flags)
		return(res);
	return(MDOC_MAX);
}


static int
rew_last(struct mdoc *mdoc, const struct mdoc_node *to)
{
	struct mdoc_node *n, *np;

	assert(to);
	mdoc->next = MDOC_NEXT_SIBLING;

	/* LINTED */
	while (mdoc->last != to) {
		/*
		 * Save the parent here, because we may delete the
		 * m->last node in the post-validation phase and reset
		 * it to m->last->parent, causing a step in the closing
		 * out to be lost.
		 */
		np = mdoc->last->parent;
		if ( ! mdoc_valid_post(mdoc))
			return(0);
		n = mdoc->last;
		mdoc->last = np;
		assert(mdoc->last);
		mdoc->last->last = n;
	}

	return(mdoc_valid_post(mdoc));
}


/*
 * For a block closing macro, return the corresponding opening one.
 * Otherwise, return the macro itself.
 */
static enum mdoct
rew_alt(enum mdoct tok)
{
	switch (tok) {
	case (MDOC_Ac):
		return(MDOC_Ao);
	case (MDOC_Bc):
		return(MDOC_Bo);
	case (MDOC_Brc):
		return(MDOC_Bro);
	case (MDOC_Dc):
		return(MDOC_Do);
	case (MDOC_Ec):
		return(MDOC_Eo);
	case (MDOC_Ed):
		return(MDOC_Bd);
	case (MDOC_Ef):
		return(MDOC_Bf);
	case (MDOC_Ek):
		return(MDOC_Bk);
	case (MDOC_El):
		return(MDOC_Bl);
	case (MDOC_Fc):
		return(MDOC_Fo);
	case (MDOC_Oc):
		return(MDOC_Oo);
	case (MDOC_Pc):
		return(MDOC_Po);
	case (MDOC_Qc):
		return(MDOC_Qo);
	case (MDOC_Re):
		return(MDOC_Rs);
	case (MDOC_Sc):
		return(MDOC_So);
	case (MDOC_Xc):
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
	case (MDOC_Bl):
		if (MDOC_It == p->tok)
			return(REWIND_MORE);
		break;
	case (MDOC_It):
		if (MDOC_BODY == p->type && MDOC_Bl == p->tok)
			return(REWIND_NONE);
		break;
	/*
	 * XXX Badly nested block handling still fails badly
	 * when one block is breaking two blocks of the same type.
	 * This is an incomplete and extremely ugly workaround,
	 * required to let the OpenBSD tree build.
	 */
	case (MDOC_Oo):
		if (MDOC_Op == p->tok)
			return(REWIND_MORE);
		break;
	case (MDOC_Nm):
		return(REWIND_NONE);
	case (MDOC_Nd):
		/* FALLTHROUGH */
	case (MDOC_Ss):
		if (MDOC_BODY == p->type && MDOC_Sh == p->tok)
			return(REWIND_NONE);
		/* FALLTHROUGH */
	case (MDOC_Sh):
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
	 */
	if (ENDBODY_NOT != p->end || MDOC_Nm == p->tok ||
	    (MDOC_BLOCK == p->type &&
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


static int
rew_elem(struct mdoc *mdoc, enum mdoct tok)
{
	struct mdoc_node *n;

	n = mdoc->last;
	if (MDOC_ELEM != n->type)
		n = n->parent;
	assert(MDOC_ELEM == n->type);
	assert(tok == n->tok);

	return(rew_last(mdoc, n));
}


/*
 * We are trying to close a block identified by tok,
 * but the child block *broken is still open.
 * Thus, postpone closing the tok block
 * until the rew_sub call closing *broken.
 */
static int
make_pending(struct mdoc_node *broken, enum mdoct tok,
		struct mdoc *m, int line, int ppos)
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
		mandoc_vmsg(MANDOCERR_SCOPENEST, m->parse, line, ppos,
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


static int
rew_sub(enum mdoc_type t, struct mdoc *m, 
		enum mdoct tok, int line, int ppos)
{
	struct mdoc_node *n;

	n = m->last;
	while (n) {
		switch (rew_dohalt(tok, t, n)) {
		case (REWIND_NONE):
			return(1);
		case (REWIND_THIS):
			break;
		case (REWIND_FORCE):
			mandoc_vmsg(MANDOCERR_SCOPEBROKEN, m->parse, 
					line, ppos, "%s breaks %s", 
					mdoc_macronames[tok],
					mdoc_macronames[n->tok]);
			/* FALLTHROUGH */
		case (REWIND_MORE):
			n = n->parent;
			continue;
		case (REWIND_LATER):
			if (make_pending(n, tok, m, line, ppos) ||
			    MDOC_BLOCK != t)
				return(1);
			/* FALLTHROUGH */
		case (REWIND_ERROR):
			mdoc_pmsg(m, line, ppos, MANDOCERR_NOSCOPE);
			return(1);
		}
		break;
	}

	assert(n);
	if ( ! rew_last(m, n))
		return(0);

	/*
	 * The current block extends an enclosing block.
	 * Now that the current block ends, close the enclosing block, too.
	 */
	while (NULL != (n = n->pending)) {
		if ( ! rew_last(m, n))
			return(0);
		if (MDOC_HEAD == n->type &&
		    ! mdoc_body_alloc(m, n->line, n->pos, n->tok))
			return(0);
	}

	return(1);
}

/*
 * Allocate a word and check whether it's punctuation or not.
 * Punctuation consists of those tokens found in mdoc_isdelim().
 */
static int
dword(struct mdoc *m, int line, 
		int col, const char *p, enum mdelim d)
{
	
	if (DELIM_MAX == d)
		d = mdoc_isdelim(p);

	if ( ! mdoc_word_alloc(m, line, col, p))
		return(0);

	if (DELIM_OPEN == d)
		m->last->flags |= MDOC_DELIMO;

	/*
	 * Closing delimiters only suppress the preceding space
	 * when they follow something, not when they start a new
	 * block or element, and not when they follow `No'.
	 *
	 * XXX	Explicitly special-casing MDOC_No here feels
	 *	like a layering violation.  Find a better way
	 *	and solve this in the code related to `No'!
	 */

	else if (DELIM_CLOSE == d && m->last->prev &&
			m->last->prev->tok != MDOC_No)
		m->last->flags |= MDOC_DELIMC;

	return(1);
}

static int
append_delims(struct mdoc *m, int line, int *pos, char *buf)
{
	int		 la;
	enum margserr	 ac;
	char		*p;

	if ('\0' == buf[*pos])
		return(1);

	for (;;) {
		la = *pos;
		ac = mdoc_zargs(m, line, pos, buf, &p);

		if (ARGS_ERROR == ac)
			return(0);
		else if (ARGS_EOLN == ac)
			break;

		dword(m, line, la, p, DELIM_MAX);

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
		if (mandoc_eos(p, strlen(p), 0))
			m->last->flags |= MDOC_EOS;
	}

	return(1);
}


/*
 * Close out block partial/full explicit.  
 */
static int
blk_exp_close(MACRO_PROT_ARGS)
{
	struct mdoc_node *body;		/* Our own body. */
	struct mdoc_node *later;	/* A sub-block starting later. */
	struct mdoc_node *n;		/* For searching backwards. */

	int	 	 j, lastarg, maxargs, flushed, nl;
	enum margserr	 ac;
	enum mdoct	 atok, ntok;
	char		*p;

	nl = MDOC_NEWLINE & m->flags;

	switch (tok) {
	case (MDOC_Ec):
		maxargs = 1;
		break;
	default:
		maxargs = 0;
		break;
	}

	/*
	 * Search backwards for beginnings of blocks,
	 * both of our own and of pending sub-blocks.
	 */
	atok = rew_alt(tok);
	body = later = NULL;
	for (n = m->last; n; n = n->parent) {
		if (MDOC_VALID & n->flags)
			continue;

		/* Remember the start of our own body. */
		if (MDOC_BODY == n->type && atok == n->tok) {
			if (ENDBODY_NOT == n->end)
				body = n;
			continue;
		}

		if (MDOC_BLOCK != n->type || MDOC_Nm == n->tok)
			continue;
		if (atok == n->tok) {
			assert(body);

			/*
			 * Found the start of our own block.
			 * When there is no pending sub block,
			 * just proceed to closing out.
			 */
			if (NULL == later)
				break;

			/* 
			 * When there is a pending sub block,
			 * postpone closing out the current block
			 * until the rew_sub() closing out the sub-block.
			 */
			make_pending(later, tok, m, line, ppos);

			/*
			 * Mark the place where the formatting - but not
			 * the scope - of the current block ends.
			 */
			if ( ! mdoc_endbody_alloc(m, line, ppos,
			    atok, body, ENDBODY_SPACE))
				return(0);
			break;
		}

		/*
		 * When finding an open sub block, remember the last
		 * open explicit block, or, in case there are only
		 * implicit ones, the first open implicit block.
		 */
		if (later &&
		    MDOC_EXPLICIT & mdoc_macros[later->tok].flags)
			continue;
		if (MDOC_CALLABLE & mdoc_macros[n->tok].flags)
			later = n;
	}

	if ( ! (MDOC_CALLABLE & mdoc_macros[tok].flags)) {
		/* FIXME: do this in validate */
		if (buf[*pos]) 
			mdoc_pmsg(m, line, ppos, MANDOCERR_ARGSLOST);

		if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
			return(0);
		return(rew_sub(MDOC_BLOCK, m, tok, line, ppos));
	}

	if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
		return(0);

	if (NULL == later && maxargs > 0) 
		if ( ! mdoc_tail_alloc(m, line, ppos, rew_alt(tok)))
			return(0);

	for (flushed = j = 0; ; j++) {
		lastarg = *pos;

		if (j == maxargs && ! flushed) {
			if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
				return(0);
			flushed = 1;
		}

		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_PUNCT == ac)
			break;
		if (ARGS_EOLN == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, lastarg, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! flushed) {
			if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
				return(0);
			flushed = 1;
		}
		if ( ! mdoc_macro(m, ntok, line, lastarg, pos, buf))
			return(0);
		break;
	}

	if ( ! flushed && ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
		return(0);

	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line(MACRO_PROT_ARGS)
{
	int		 la, scope, cnt, nc, nl;
	enum margverr	 av;
	enum mdoct	 ntok;
	enum margserr	 ac;
	enum mdelim	 d;
	struct mdoc_arg	*arg;
	char		*p;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * Whether we allow ignored elements (those without content,
	 * usually because of reserved words) to squeak by.
	 */

	switch (tok) {
	case (MDOC_An):
		/* FALLTHROUGH */
	case (MDOC_Ar):
		/* FALLTHROUGH */
	case (MDOC_Fl):
		/* FALLTHROUGH */
	case (MDOC_Mt):
		/* FALLTHROUGH */
	case (MDOC_Nm):
		/* FALLTHROUGH */
	case (MDOC_Pa):
		nc = 1;
		break;
	default:
		nc = 0;
		break;
	}

	for (arg = NULL;; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		} 
		if (ARGV_EOLN == av)
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	for (cnt = scope = 0;; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;
		if (ARGS_PUNCT == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		/* 
		 * In this case, we've located a submacro and must
		 * execute it.  Close out scope, if open.  If no
		 * elements have been generated, either create one (nc)
		 * or raise a warning.
		 */

		if (MDOC_MAX != ntok) {
			if (scope && ! rew_elem(m, tok))
				return(0);
			if (nc && 0 == cnt) {
				if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
					return(0);
				if ( ! rew_last(m, m->last))
					return(0);
			} else if ( ! nc && 0 == cnt) {
				mdoc_argv_free(arg);
				mdoc_pmsg(m, line, ppos, MANDOCERR_MACROEMPTY);
			}

			if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
				return(0);
			if ( ! nl)
				return(1);
			return(append_delims(m, line, pos, buf));
		} 

		/* 
		 * Non-quote-enclosed punctuation.  Set up our scope, if
		 * a word; rewind the scope, if a delimiter; then append
		 * the word. 
		 */

		d = ARGS_QWORD == ac ? DELIM_NONE : mdoc_isdelim(p);

		if (DELIM_NONE != d) {
			/*
			 * If we encounter closing punctuation, no word
			 * has been omitted, no scope is open, and we're
			 * allowed to have an empty element, then start
			 * a new scope.  `Ar', `Fl', and `Li', only do
			 * this once per invocation.  There may be more
			 * of these (all of them?).
			 */
			if (0 == cnt && (nc || MDOC_Li == tok) && 
					DELIM_CLOSE == d && ! scope) {
				if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
					return(0);
				if (MDOC_Ar == tok || MDOC_Li == tok || 
						MDOC_Fl == tok)
					cnt++;
				scope = 1;
			}
			/*
			 * Close out our scope, if one is open, before
			 * any punctuation.
			 */
			if (scope && ! rew_elem(m, tok))
				return(0);
			scope = 0;
		} else if ( ! scope) {
			if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
				return(0);
			scope = 1;
		}

		if (DELIM_NONE == d)
			cnt++;

		if ( ! dword(m, line, la, p, d))
			return(0);

		/*
		 * `Fl' macros have their scope re-opened with each new
		 * word so that the `-' can be added to each one without
		 * having to parse out spaces.
		 */
		if (scope && MDOC_Fl == tok) {
			if ( ! rew_elem(m, tok))
				return(0);
			scope = 0;
		}
	}

	if (scope && ! rew_elem(m, tok))
		return(0);

	/*
	 * If no elements have been collected and we're allowed to have
	 * empties (nc), open a scope and close it out.  Otherwise,
	 * raise a warning.
	 */

	if (nc && 0 == cnt) {
		if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
			return(0);
		if ( ! rew_last(m, m->last))
			return(0);
	} else if ( ! nc && 0 == cnt) {
		mdoc_argv_free(arg);
		mdoc_pmsg(m, line, ppos, MANDOCERR_MACROEMPTY);
	}

	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
blk_full(MACRO_PROT_ARGS)
{
	int		  la, nl, nparsed;
	struct mdoc_arg	 *arg;
	struct mdoc_node *head; /* save of head macro */
	struct mdoc_node *body; /* save of body macro */
	struct mdoc_node *n;
	enum mdoc_type	  mtt;
	enum mdoct	  ntok;
	enum margserr	  ac, lac;
	enum margverr	  av;
	char		 *p;

	nl = MDOC_NEWLINE & m->flags;

	/* Close out prior implicit scope. */

	if ( ! (MDOC_EXPLICIT & mdoc_macros[tok].flags)) {
		if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
			return(0);
		if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
			return(0);
	}

	/*
	 * This routine accommodates implicitly- and explicitly-scoped
	 * macro openings.  Implicit ones first close out prior scope
	 * (seen above).  Delay opening the head until necessary to
	 * allow leading punctuation to print.  Special consideration
	 * for `It -column', which has phrase-part syntax instead of
	 * regular child nodes.
	 */

	for (arg = NULL;; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		} 

		if (ARGV_EOLN == av)
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	if ( ! mdoc_block_alloc(m, line, ppos, tok, arg))
		return(0);

	head = body = NULL;

	/*
	 * Exception: Heads of `It' macros in `-diag' lists are not
	 * parsed, even though `It' macros in general are parsed.
	 */
	nparsed = MDOC_It == tok &&
		MDOC_Bl == m->last->parent->tok &&
		LIST_diag == m->last->parent->norm->Bl.type;

	/*
	 * The `Nd' macro has all arguments in its body: it's a hybrid
	 * of block partial-explicit and full-implicit.  Stupid.
	 */

	if (MDOC_Nd == tok) {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		head = m->last;
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		body = m->last;
	} 

	ac = ARGS_ERROR;

	for ( ; ; ) {
		la = *pos;
		/* Initialise last-phrase-type with ARGS_PEND. */
		lac = ARGS_ERROR == ac ? ARGS_PEND : ac;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_PUNCT == ac)
			break;

		if (ARGS_ERROR == ac)
			return(0);

		if (ARGS_EOLN == ac) {
			if (ARGS_PPHRASE != lac && ARGS_PHRASE != lac)
				break;
			/*
			 * This is necessary: if the last token on a
			 * line is a `Ta' or tab, then we'll get
			 * ARGS_EOLN, so we must be smart enough to
			 * reopen our scope if the last parse was a
			 * phrase or partial phrase.
			 */
			if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
				return(0);
			if ( ! mdoc_body_alloc(m, line, ppos, tok))
				return(0);
			body = m->last;
			break;
		}

		/* 
		 * Emit leading punctuation (i.e., punctuation before
		 * the MDOC_HEAD) for non-phrase types.
		 */

		if (NULL == head && 
				ARGS_PEND != ac &&
				ARGS_PHRASE != ac &&
				ARGS_PPHRASE != ac &&
				ARGS_QWORD != ac &&
				DELIM_OPEN == mdoc_isdelim(p)) {
			if ( ! dword(m, line, la, p, DELIM_OPEN))
				return(0);
			continue;
		}

		/* Open a head if one hasn't been opened. */

		if (NULL == head) {
			if ( ! mdoc_head_alloc(m, line, ppos, tok))
				return(0);
			head = m->last;
		}

		if (ARGS_PHRASE == ac || 
				ARGS_PEND == ac ||
				ARGS_PPHRASE == ac) {
			/*
			 * If we haven't opened a body yet, rewind the
			 * head; if we have, rewind that instead.
			 */

			mtt = body ? MDOC_BODY : MDOC_HEAD;
			if ( ! rew_sub(mtt, m, tok, line, ppos))
				return(0);
			
			/* Then allocate our body context. */

			if ( ! mdoc_body_alloc(m, line, ppos, tok))
				return(0);
			body = m->last;

			/*
			 * Process phrases: set whether we're in a
			 * partial-phrase (this effects line handling)
			 * then call down into the phrase parser.
			 */

			if (ARGS_PPHRASE == ac)
				m->flags |= MDOC_PPHRASE;
			if (ARGS_PEND == ac && ARGS_PPHRASE == lac)
				m->flags |= MDOC_PPHRASE;

			if ( ! phrase(m, line, la, buf))
				return(0);

			m->flags &= ~MDOC_PPHRASE;
			continue;
		}

		ntok = nparsed || ARGS_QWORD == ac ? 
			MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, la, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		break;
	}

	if (NULL == head) {
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);
		head = m->last;
	}
	
	if (nl && ! append_delims(m, line, pos, buf))
		return(0);

	/* If we've already opened our body, exit now. */

	if (NULL != body)
		goto out;

	/*
	 * If there is an open (i.e., unvalidated) sub-block requiring
	 * explicit close-out, postpone switching the current block from
	 * head to body until the rew_sub() call closing out that
	 * sub-block.
	 */
	for (n = m->last; n && n != head; n = n->parent) {
		if (MDOC_BLOCK == n->type && 
				MDOC_EXPLICIT & mdoc_macros[n->tok].flags &&
				! (MDOC_VALID & n->flags)) {
			n->pending = head;
			return(1);
		}
	}

	/* Close out scopes to remain in a consistent state. */

	if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
		return(0);
	if ( ! mdoc_body_alloc(m, line, ppos, tok))
		return(0);

out:
	if ( ! (MDOC_FREECOL & m->flags))
		return(1);

	if ( ! rew_sub(MDOC_BODY, m, tok, line, ppos))
		return(0);
	if ( ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
		return(0);

	m->flags &= ~MDOC_FREECOL;
	return(1);
}


static int
blk_part_imp(MACRO_PROT_ARGS)
{
	int		  la, nl;
	enum mdoct	  ntok;
	enum margserr	  ac;
	char		 *p;
	struct mdoc_node *blk; /* saved block context */
	struct mdoc_node *body; /* saved body context */
	struct mdoc_node *n;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * A macro that spans to the end of the line.  This is generally
	 * (but not necessarily) called as the first macro.  The block
	 * has a head as the immediate child, which is always empty,
	 * followed by zero or more opening punctuation nodes, then the
	 * body (which may be empty, depending on the macro), then zero
	 * or more closing punctuation nodes.
	 */

	if ( ! mdoc_block_alloc(m, line, ppos, tok, NULL))
		return(0);

	blk = m->last;

	if ( ! mdoc_head_alloc(m, line, ppos, tok))
		return(0);
	if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
		return(0);

	/*
	 * Open the body scope "on-demand", that is, after we've
	 * processed all our the leading delimiters (open parenthesis,
	 * etc.).
	 */

	for (body = NULL; ; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;
		if (ARGS_PUNCT == ac)
			break;

		if (NULL == body && ARGS_QWORD != ac &&
				DELIM_OPEN == mdoc_isdelim(p)) {
			if ( ! dword(m, line, la, p, DELIM_OPEN))
				return(0);
			continue;
		} 

		if (NULL == body) {
		       if ( ! mdoc_body_alloc(m, line, ppos, tok))
			       return(0);
			body = m->last;
		}

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, la, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		break;
	}

	/* Clean-ups to leave in a consistent state. */

	if (NULL == body) {
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
		body = m->last;
	}

	for (n = body->child; n && n->next; n = n->next)
		/* Do nothing. */ ;
	
	/* 
	 * End of sentence spacing: if the last node is a text node and
	 * has a trailing period, then mark it as being end-of-sentence.
	 */

	if (n && MDOC_TEXT == n->type && n->string)
		if (mandoc_eos(n->string, strlen(n->string), 1))
			n->flags |= MDOC_EOS;

	/* Up-propagate the end-of-space flag. */

	if (n && (MDOC_EOS & n->flags)) {
		body->flags |= MDOC_EOS;
		body->parent->flags |= MDOC_EOS;
	}

	/*
	 * If there is an open sub-block requiring explicit close-out,
	 * postpone closing out the current block
	 * until the rew_sub() call closing out the sub-block.
	 */
	for (n = m->last; n && n != body && n != blk->parent; n = n->parent) {
		if (MDOC_BLOCK == n->type &&
		    MDOC_EXPLICIT & mdoc_macros[n->tok].flags &&
		    ! (MDOC_VALID & n->flags)) {
			make_pending(n, tok, m, line, ppos);
			if ( ! mdoc_endbody_alloc(m, line, ppos,
			    tok, body, ENDBODY_NOSPACE))
				return(0);
			return(1);
		}
	}

	/* 
	 * If we can't rewind to our body, then our scope has already
	 * been closed by another macro (like `Oc' closing `Op').  This
	 * is ugly behaviour nodding its head to OpenBSD's overwhelming
	 * crufty use of `Op' breakage.
	 */
	if (n != body)
		mandoc_vmsg(MANDOCERR_SCOPENEST, m->parse, line, ppos, 
				"%s broken", mdoc_macronames[tok]);

	if (n && ! rew_sub(MDOC_BODY, m, tok, line, ppos))
		return(0);

	/* Standard appending of delimiters. */

	if (nl && ! append_delims(m, line, pos, buf))
		return(0);

	/* Rewind scope, if applicable. */

	if (n && ! rew_sub(MDOC_BLOCK, m, tok, line, ppos))
		return(0);

	return(1);
}


static int
blk_part_exp(MACRO_PROT_ARGS)
{
	int		  la, nl;
	enum margserr	  ac;
	struct mdoc_node *head; /* keep track of head */
	struct mdoc_node *body; /* keep track of body */
	char		 *p;
	enum mdoct	  ntok;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * The opening of an explicit macro having zero or more leading
	 * punctuation nodes; a head with optional single element (the
	 * case of `Eo'); and a body that may be empty.
	 */

	if ( ! mdoc_block_alloc(m, line, ppos, tok, NULL))
		return(0); 

	for (head = body = NULL; ; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_PUNCT == ac)
			break;
		if (ARGS_EOLN == ac)
			break;

		/* Flush out leading punctuation. */

		if (NULL == head && ARGS_QWORD != ac &&
				DELIM_OPEN == mdoc_isdelim(p)) {
			assert(NULL == body);
			if ( ! dword(m, line, la, p, DELIM_OPEN))
				return(0);
			continue;
		} 

		if (NULL == head) {
			assert(NULL == body);
			if ( ! mdoc_head_alloc(m, line, ppos, tok))
				return(0);
			head = m->last;
		}

		/*
		 * `Eo' gobbles any data into the head, but most other
		 * macros just immediately close out and begin the body.
		 */

		if (NULL == body) {
			assert(head);
			/* No check whether it's a macro! */
			if (MDOC_Eo == tok)
				if ( ! dword(m, line, la, p, DELIM_MAX))
					return(0);

			if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
				return(0);
			if ( ! mdoc_body_alloc(m, line, ppos, tok))
				return(0);
			body = m->last;

			if (MDOC_Eo == tok)
				continue;
		}

		assert(NULL != head && NULL != body);

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, la, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		break;
	}

	/* Clean-up to leave in a consistent state. */

	if (NULL == head)
		if ( ! mdoc_head_alloc(m, line, ppos, tok))
			return(0);

	if (NULL == body) {
		if ( ! rew_sub(MDOC_HEAD, m, tok, line, ppos))
			return(0);
		if ( ! mdoc_body_alloc(m, line, ppos, tok))
			return(0);
	}

	/* Standard appending of delimiters. */

	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


/* ARGSUSED */
static int
in_line_argn(MACRO_PROT_ARGS)
{
	int		 la, flushed, j, maxargs, nl;
	enum margserr	 ac;
	enum margverr	 av;
	struct mdoc_arg	*arg;
	char		*p;
	enum mdoct	 ntok;

	nl = MDOC_NEWLINE & m->flags;

	/*
	 * A line macro that has a fixed number of arguments (maxargs).
	 * Only open the scope once the first non-leading-punctuation is
	 * found (unless MDOC_IGNDELIM is noted, like in `Pf'), then
	 * keep it open until the maximum number of arguments are
	 * exhausted.
	 */

	switch (tok) {
	case (MDOC_Ap):
		/* FALLTHROUGH */
	case (MDOC_No):
		/* FALLTHROUGH */
	case (MDOC_Ns):
		/* FALLTHROUGH */
	case (MDOC_Ux):
		maxargs = 0;
		break;
	case (MDOC_Bx):
		/* FALLTHROUGH */
	case (MDOC_Xr):
		maxargs = 2;
		break;
	default:
		maxargs = 1;
		break;
	}

	for (arg = NULL; ; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		} 

		if (ARGV_EOLN == av)
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	for (flushed = j = 0; ; ) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_PUNCT == ac)
			break;
		if (ARGS_EOLN == ac)
			break;

		if ( ! (MDOC_IGNDELIM & mdoc_macros[tok].flags) && 
				ARGS_QWORD != ac && 0 == j && 
				DELIM_OPEN == mdoc_isdelim(p)) {
			if ( ! dword(m, line, la, p, DELIM_OPEN))
				return(0);
			continue;
		} else if (0 == j)
		       if ( ! mdoc_elem_alloc(m, line, la, tok, arg))
			       return(0);

		if (j == maxargs && ! flushed) {
			if ( ! rew_elem(m, tok))
				return(0);
			flushed = 1;
		}

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX != ntok) {
			if ( ! flushed && ! rew_elem(m, tok))
				return(0);
			flushed = 1;
			if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
				return(0);
			j++;
			break;
		}

		if ( ! (MDOC_IGNDELIM & mdoc_macros[tok].flags) &&
				ARGS_QWORD != ac &&
				! flushed &&
				DELIM_NONE != mdoc_isdelim(p)) {
			if ( ! rew_elem(m, tok))
				return(0);
			flushed = 1;
		}

		if ( ! dword(m, line, la, p, DELIM_MAX))
			return(0);
		j++;
	}

	if (0 == j && ! mdoc_elem_alloc(m, line, la, tok, arg))
	       return(0);

	/* Close out in a consistent state. */

	if ( ! flushed && ! rew_elem(m, tok))
		return(0);
	if ( ! nl)
		return(1);
	return(append_delims(m, line, pos, buf));
}


static int
in_line_eoln(MACRO_PROT_ARGS)
{
	int		 la;
	enum margserr	 ac;
	enum margverr	 av;
	struct mdoc_arg	*arg;
	char		*p;
	enum mdoct	 ntok;

	assert( ! (MDOC_PARSED & mdoc_macros[tok].flags));

	if (tok == MDOC_Pp)
		rew_sub(MDOC_BLOCK, m, MDOC_Nm, line, ppos);

	/* Parse macro arguments. */

	for (arg = NULL; ; ) {
		la = *pos;
		av = mdoc_argv(m, line, tok, &arg, pos, buf);

		if (ARGV_WORD == av) {
			*pos = la;
			break;
		}
		if (ARGV_EOLN == av) 
			break;
		if (ARGV_ARG == av)
			continue;

		mdoc_argv_free(arg);
		return(0);
	}

	/* Open element scope. */

	if ( ! mdoc_elem_alloc(m, line, ppos, tok, arg))
		return(0);

	/* Parse argument terms. */

	for (;;) {
		la = *pos;
		ac = mdoc_args(m, line, pos, buf, tok, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup(tok, p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, la, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! rew_elem(m, tok))
			return(0);
		return(mdoc_macro(m, ntok, line, la, pos, buf));
	}

	/* Close out (no delimiters). */

	return(rew_elem(m, tok));
}


/* ARGSUSED */
static int
ctx_synopsis(MACRO_PROT_ARGS)
{
	int		 nl;

	nl = MDOC_NEWLINE & m->flags;

	/* If we're not in the SYNOPSIS, go straight to in-line. */
	if ( ! (MDOC_SYNOPSIS & m->flags))
		return(in_line(m, tok, line, ppos, pos, buf));

	/* If we're a nested call, same place. */
	if ( ! nl)
		return(in_line(m, tok, line, ppos, pos, buf));

	/*
	 * XXX: this will open a block scope; however, if later we end
	 * up formatting the block scope, then child nodes will inherit
	 * the formatting.  Be careful.
	 */
	if (MDOC_Nm == tok)
		return(blk_full(m, tok, line, ppos, pos, buf));
	assert(MDOC_Vt == tok);
	return(blk_part_imp(m, tok, line, ppos, pos, buf));
}


/* ARGSUSED */
static int
obsolete(MACRO_PROT_ARGS)
{

	mdoc_pmsg(m, line, ppos, MANDOCERR_MACROOBS);
	return(1);
}


/*
 * Phrases occur within `Bl -column' entries, separated by `Ta' or tabs.
 * They're unusual because they're basically free-form text until a
 * macro is encountered.
 */
static int
phrase(struct mdoc *m, int line, int ppos, char *buf)
{
	int		 la, pos;
	enum margserr	 ac;
	enum mdoct	 ntok;
	char		*p;

	for (pos = ppos; ; ) {
		la = pos;

		ac = mdoc_zargs(m, line, &pos, buf, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup_raw(p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, la, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, &pos, buf))
			return(0);
		return(append_delims(m, line, &pos, buf));
	}

	return(1);
}


/* ARGSUSED */
static int
phrase_ta(MACRO_PROT_ARGS)
{
	int		  la;
	enum mdoct	  ntok;
	enum margserr	  ac;
	char		 *p;

	/*
	 * FIXME: this is overly restrictive: if the `Ta' is unexpected,
	 * it should simply error out with ARGSLOST.
	 */

	if ( ! rew_sub(MDOC_BODY, m, MDOC_It, line, ppos))
		return(0);
	if ( ! mdoc_body_alloc(m, line, ppos, MDOC_It))
		return(0);

	for (;;) {
		la = *pos;
		ac = mdoc_zargs(m, line, pos, buf, &p);

		if (ARGS_ERROR == ac)
			return(0);
		if (ARGS_EOLN == ac)
			break;

		ntok = ARGS_QWORD == ac ? MDOC_MAX : lookup_raw(p);

		if (MDOC_MAX == ntok) {
			if ( ! dword(m, line, la, p, DELIM_MAX))
				return(0);
			continue;
		}

		if ( ! mdoc_macro(m, ntok, line, la, pos, buf))
			return(0);
		return(append_delims(m, line, pos, buf));
	}

	return(1);
}
