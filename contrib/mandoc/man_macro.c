/* $Id: man_macro.c,v 1.150 2023/11/13 19:13:01 schwarze Exp $ */
/*
 * Copyright (c) 2012-2015,2017-2020,2022 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2008, 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2013 Franco Fichtner <franco@lastsummer.de>
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

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if DEBUG_MEMORY
#include "mandoc_dbg.h"
#endif
#include "mandoc.h"
#include "roff.h"
#include "man.h"
#include "libmandoc.h"
#include "roff_int.h"
#include "libman.h"

static	void		 blk_close(MACRO_PROT_ARGS);
static	void		 blk_exp(MACRO_PROT_ARGS);
static	void		 blk_imp(MACRO_PROT_ARGS);
static	void		 in_line_eoln(MACRO_PROT_ARGS);
static	int		 man_args(struct roff_man *, int,
				int *, char *, char **);
static	void		 rew_scope(struct roff_man *, enum roff_tok);

static const struct man_macro man_macros[MAN_MAX - MAN_TH] = {
	{ in_line_eoln, MAN_XSCOPE }, /* TH */
	{ blk_imp, MAN_XSCOPE | MAN_BSCOPED }, /* SH */
	{ blk_imp, MAN_XSCOPE | MAN_BSCOPED }, /* SS */
	{ blk_imp, MAN_XSCOPE | MAN_BSCOPED }, /* TP */
	{ blk_imp, MAN_XSCOPE | MAN_BSCOPED }, /* TQ */
	{ blk_imp, MAN_XSCOPE }, /* LP */
	{ blk_imp, MAN_XSCOPE }, /* PP */
	{ blk_imp, MAN_XSCOPE }, /* P */
	{ blk_imp, MAN_XSCOPE }, /* IP */
	{ blk_imp, MAN_XSCOPE }, /* HP */
	{ in_line_eoln, MAN_NSCOPED | MAN_ESCOPED | MAN_JOIN }, /* SM */
	{ in_line_eoln, MAN_NSCOPED | MAN_ESCOPED | MAN_JOIN }, /* SB */
	{ in_line_eoln, 0 }, /* BI */
	{ in_line_eoln, 0 }, /* IB */
	{ in_line_eoln, 0 }, /* BR */
	{ in_line_eoln, 0 }, /* RB */
	{ in_line_eoln, MAN_NSCOPED | MAN_ESCOPED | MAN_JOIN }, /* R */
	{ in_line_eoln, MAN_NSCOPED | MAN_ESCOPED | MAN_JOIN }, /* B */
	{ in_line_eoln, MAN_NSCOPED | MAN_ESCOPED | MAN_JOIN }, /* I */
	{ in_line_eoln, 0 }, /* IR */
	{ in_line_eoln, 0 }, /* RI */
	{ blk_close, MAN_XSCOPE }, /* RE */
	{ blk_exp, MAN_XSCOPE }, /* RS */
	{ in_line_eoln, MAN_NSCOPED }, /* DT */
	{ in_line_eoln, MAN_NSCOPED }, /* UC */
	{ in_line_eoln, MAN_NSCOPED }, /* PD */
	{ in_line_eoln, MAN_NSCOPED }, /* AT */
	{ in_line_eoln, MAN_NSCOPED }, /* in */
	{ blk_imp, MAN_XSCOPE }, /* SY */
	{ blk_close, MAN_XSCOPE }, /* YS */
	{ in_line_eoln, 0 }, /* OP */
	{ in_line_eoln, MAN_XSCOPE }, /* EX */
	{ in_line_eoln, MAN_XSCOPE }, /* EE */
	{ blk_exp, MAN_XSCOPE }, /* UR */
	{ blk_close, MAN_XSCOPE }, /* UE */
	{ blk_exp, MAN_XSCOPE }, /* MT */
	{ blk_close, MAN_XSCOPE }, /* ME */
	{ in_line_eoln, 0 }, /* MR */
};


const struct man_macro *
man_macro(enum roff_tok tok)
{
	assert(tok >= MAN_TH && tok <= MAN_MAX);
	return man_macros + (tok - MAN_TH);
}

void
man_unscope(struct roff_man *man, const struct roff_node *to)
{
	struct roff_node *n;

	to = to->parent;
	n = man->last;
	while (n != to) {

		/* Reached the end of the document? */

		if (to == NULL && ! (n->flags & NODE_VALID)) {
			if (man->flags & (MAN_BLINE | MAN_ELINE) &&
			    man_macro(n->tok)->flags &
			     (MAN_BSCOPED | MAN_NSCOPED)) {
				mandoc_msg(MANDOCERR_BLK_LINE,
				    n->line, n->pos,
				    "EOF breaks %s", roff_name[n->tok]);
				if (man->flags & MAN_ELINE) {
					if (n->parent->type == ROFFT_ROOT ||
					    (man_macro(n->parent->tok)->flags &
					    MAN_ESCOPED) == 0)
						man->flags &= ~MAN_ELINE;
				} else {
					assert(n->type == ROFFT_HEAD);
					n = n->parent;
					man->flags &= ~MAN_BLINE;
				}
				man->last = n;
				n = n->parent;
				roff_node_delete(man, man->last);
				continue;
			}
			if (n->type == ROFFT_BLOCK &&
			    man_macro(n->tok)->fp == blk_exp)
				mandoc_msg(MANDOCERR_BLK_NOEND,
				    n->line, n->pos, "%s",
				    roff_name[n->tok]);
		}

		/*
		 * We might delete the man->last node
		 * in the post-validation phase.
		 * Save a pointer to the parent such that
		 * we know where to continue the iteration.
		 */

		man->last = n;
		n = n->parent;
		man->last->flags |= NODE_VALID;
	}

	/*
	 * If we ended up at the parent of the node we were
	 * supposed to rewind to, that means the target node
	 * got deleted, so add the next node we parse as a child
	 * of the parent instead of as a sibling of the target.
	 */

	man->next = (man->last == to) ?
	    ROFF_NEXT_CHILD : ROFF_NEXT_SIBLING;
}

/*
 * Rewinding entails ascending the parse tree until a coherent point,
 * for example, the `SH' macro will close out any intervening `SS'
 * scopes.  When a scope is closed, it must be validated and actioned.
 */
static void
rew_scope(struct roff_man *man, enum roff_tok tok)
{
	struct roff_node *n;

	/* Preserve empty paragraphs before RS. */

	n = man->last;
	if (tok == MAN_RS && n->child == NULL &&
	    (n->tok == MAN_P || n->tok == MAN_PP || n->tok == MAN_LP))
		return;

	for (;;) {
		if (n->type == ROFFT_ROOT)
			return;
		if (n->flags & NODE_VALID) {
			n = n->parent;
			continue;
		}
		if (n->type != ROFFT_BLOCK) {
			if (n->parent->type == ROFFT_ROOT) {
				man_unscope(man, n);
				return;
			} else {
				n = n->parent;
				continue;
			}
		}
		if (tok != MAN_SH && (n->tok == MAN_SH ||
		    (tok != MAN_SS && (n->tok == MAN_SS ||
		     man_macro(n->tok)->fp == blk_exp))))
			return;
		man_unscope(man, n);
		n = man->last;
	}
}


/*
 * Close out a generic explicit macro.
 */
void
blk_close(MACRO_PROT_ARGS)
{
	enum roff_tok		 ctok, ntok;
	const struct roff_node	*nn;
	char			*p, *ep;
	int			 cline, cpos, la, nrew, target;

	nrew = 1;
	switch (tok) {
	case MAN_RE:
		ntok = MAN_RS;
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		for (nn = man->last->parent; nn; nn = nn->parent)
			if (nn->tok == ntok && nn->type == ROFFT_BLOCK)
				nrew++;
		target = strtol(p, &ep, 10);
		if (*ep != '\0')
			mandoc_msg(MANDOCERR_ARG_EXCESS, line,
			    la + (buf[la] == '"') + (int)(ep - p),
			    "RE ... %s", ep);
		free(p);
		if (target == 0)
			target = 1;
		nrew -= target;
		if (nrew < 1) {
			mandoc_msg(MANDOCERR_RE_NOTOPEN,
			    line, ppos, "RE %d", target);
			return;
		}
		break;
	case MAN_YS:
		ntok = MAN_SY;
		break;
	case MAN_UE:
		ntok = MAN_UR;
		break;
	case MAN_ME:
		ntok = MAN_MT;
		break;
	default:
		abort();
	}

	for (nn = man->last->parent; nn; nn = nn->parent)
		if (nn->tok == ntok && nn->type == ROFFT_BLOCK && ! --nrew)
			break;

	if (nn == NULL) {
		mandoc_msg(MANDOCERR_BLK_NOTOPEN,
		    line, ppos, "%s", roff_name[tok]);
		rew_scope(man, MAN_PP);
		if (tok == MAN_RE) {
			roff_elem_alloc(man, line, ppos, ROFF_br);
			man->last->flags |= NODE_LINE |
			    NODE_VALID | NODE_ENDED;
			man->next = ROFF_NEXT_SIBLING;
		}
		return;
	}

	cline = man->last->line;
	cpos = man->last->pos;
	ctok = man->last->tok;
	man_unscope(man, nn);

	if (tok == MAN_RE && nn->head->aux > 0)
		roff_setreg(man->roff, "an-margin", nn->head->aux, '-');

	/* Trailing text. */

	if (buf[*pos] != '\0') {
		roff_word_alloc(man, line, ppos, buf + *pos);
		man->last->flags |= NODE_DELIMC;
		if (mandoc_eos(man->last->string, strlen(man->last->string)))
			man->last->flags |= NODE_EOS;
	}

	/* Move a trailing paragraph behind the block. */

	if (ctok == MAN_LP || ctok == MAN_PP || ctok == MAN_P) {
		*pos = strlen(buf);
		blk_imp(man, ctok, cline, cpos, pos, buf);
	}

	/* Synopsis blocks need an explicit end marker for spacing. */

	if (tok == MAN_YS && man->last == nn) {
		roff_elem_alloc(man, line, ppos, tok);
		man_unscope(man, man->last);
	}
}

void
blk_exp(MACRO_PROT_ARGS)
{
	struct roff_node *head;
	char		*p;
	int		 la;

	if (tok == MAN_RS) {
		rew_scope(man, tok);
		man->flags |= ROFF_NONOFILL;
	}
	roff_block_alloc(man, line, ppos, tok);
	head = roff_head_alloc(man, line, ppos, tok);

	la = *pos;
	if (man_args(man, line, pos, buf, &p)) {
		roff_word_alloc(man, line, la, p);
		if (tok == MAN_RS) {
			if (roff_getreg(man->roff, "an-margin") == 0)
				roff_setreg(man->roff, "an-margin",
				    5 * 24, '=');
			if ((head->aux = strtod(p, NULL) * 24.0) > 0)
				roff_setreg(man->roff, "an-margin",
				    head->aux, '+');
		}
		free(p);
	}

	if (buf[*pos] != '\0')
		mandoc_msg(MANDOCERR_ARG_EXCESS, line, *pos,
		    "%s ... %s", roff_name[tok], buf + *pos);

	man_unscope(man, head);
	roff_body_alloc(man, line, ppos, tok);
	man->flags &= ~ROFF_NONOFILL;
}

/*
 * Parse an implicit-block macro.  These contain a ROFFT_HEAD and a
 * ROFFT_BODY contained within a ROFFT_BLOCK.  Rules for closing out other
 * scopes, such as `SH' closing out an `SS', are defined in the rew
 * routines.
 */
void
blk_imp(MACRO_PROT_ARGS)
{
	int		 la;
	char		*p;
	struct roff_node *n;

	rew_scope(man, tok);
	man->flags |= ROFF_NONOFILL;
	if (tok == MAN_SH || tok == MAN_SS)
		man->flags &= ~ROFF_NOFILL;
	roff_block_alloc(man, line, ppos, tok);
	n = roff_head_alloc(man, line, ppos, tok);

	/* Add line arguments. */

	for (;;) {
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		roff_word_alloc(man, line, la, p);
		free(p);
	}

	/*
	 * For macros having optional next-line scope,
	 * keep the head open if there were no arguments.
	 * For `TP' and `TQ', always keep the head open.
	 */

	if (man_macro(tok)->flags & MAN_BSCOPED &&
	    (tok == MAN_TP || tok == MAN_TQ || n == man->last)) {
		man->flags |= MAN_BLINE;
		return;
	}

	/* Close out the head and open the body. */

	man_unscope(man, n);
	roff_body_alloc(man, line, ppos, tok);
	man->flags &= ~ROFF_NONOFILL;
}

void
in_line_eoln(MACRO_PROT_ARGS)
{
	int		 la;
	char		*p;
	struct roff_node *n;

	roff_elem_alloc(man, line, ppos, tok);
	n = man->last;

	if (tok == MAN_EX)
		man->flags |= ROFF_NOFILL;
	else if (tok == MAN_EE)
		man->flags &= ~ROFF_NOFILL;

#if DEBUG_MEMORY
	if (tok == MAN_TH)
		mandoc_dbg_name(buf);
#endif

	for (;;) {
		if (buf[*pos] != '\0' && man->last != n && tok == MAN_PD) {
			mandoc_msg(MANDOCERR_ARG_EXCESS, line, *pos,
			    "%s ... %s", roff_name[tok], buf + *pos);
			break;
		}
		la = *pos;
		if ( ! man_args(man, line, pos, buf, &p))
			break;
		if (man_macro(tok)->flags & MAN_JOIN &&
		    man->last->type == ROFFT_TEXT)
			roff_word_append(man, p);
		else
			roff_word_alloc(man, line, la, p);
		free(p);
	}

	/*
	 * Append NODE_EOS in case the last snipped argument
	 * ends with a dot, e.g. `.IR syslog (3).'
	 */

	if (n != man->last &&
	    mandoc_eos(man->last->string, strlen(man->last->string)))
		man->last->flags |= NODE_EOS;

	/*
	 * If no arguments are specified and this is MAN_ESCOPED (i.e.,
	 * next-line scoped), then set our mode to indicate that we're
	 * waiting for terms to load into our context.
	 */

	if (n == man->last && man_macro(tok)->flags & MAN_ESCOPED) {
		man->flags |= MAN_ELINE;
		return;
	}

	assert(man->last->type != ROFFT_ROOT);
	man->next = ROFF_NEXT_SIBLING;

	/* Rewind our element scope. */

	for ( ; man->last; man->last = man->last->parent) {
		man->last->flags |= NODE_VALID;
		if (man->last == n)
			break;
	}

	/* Rewind next-line scoped ancestors, if any. */

	if (man_macro(tok)->flags & MAN_ESCOPED)
		man_descope(man, line, ppos, NULL);
}

void
man_endparse(struct roff_man *man)
{
	man_unscope(man, man->meta.first);
}

static int
man_args(struct roff_man *man, int line, int *pos, char *buf, char **v)
{
	char	 *start;

	assert(*pos);
	*v = start = buf + *pos;
	assert(' ' != *start);

	if ('\0' == *start)
		return 0;

	*v = roff_getarg(man->roff, v, line, pos);
	return 1;
}
