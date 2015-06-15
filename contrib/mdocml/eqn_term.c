/*	$Id: eqn_term.c,v 1.8 2015/01/01 15:36:08 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"
#include "term.h"

static	const enum termfont fontmap[EQNFONT__MAX] = {
	TERMFONT_NONE, /* EQNFONT_NONE */
	TERMFONT_NONE, /* EQNFONT_ROMAN */
	TERMFONT_BOLD, /* EQNFONT_BOLD */
	TERMFONT_BOLD, /* EQNFONT_FAT */
	TERMFONT_UNDER /* EQNFONT_ITALIC */
};

static void	eqn_box(struct termp *, const struct eqn_box *);


void
term_eqn(struct termp *p, const struct eqn *ep)
{

	eqn_box(p, ep->root);
	p->flags &= ~TERMP_NOSPACE;
}

static void
eqn_box(struct termp *p, const struct eqn_box *bp)
{
	const struct eqn_box *child;

	if (bp->type == EQN_LIST ||
	    (bp->type == EQN_PILE && (bp->prev || bp->next)) ||
	    (bp->parent != NULL && bp->parent->pos == EQNPOS_SQRT)) {
		if (bp->parent->type == EQN_SUBEXPR && bp->prev != NULL)
			p->flags |= TERMP_NOSPACE;
		term_word(p, bp->left != NULL ? bp->left : "(");
		p->flags |= TERMP_NOSPACE;
	}
	if (bp->font != EQNFONT_NONE)
		term_fontpush(p, fontmap[(int)bp->font]);

	if (bp->text != NULL)
		term_word(p, bp->text);

	if (bp->pos == EQNPOS_SQRT) {
		term_word(p, "sqrt");
		p->flags |= TERMP_NOSPACE;
		eqn_box(p, bp->first);
	} else if (bp->type == EQN_SUBEXPR) {
		child = bp->first;
		eqn_box(p, child);
		p->flags |= TERMP_NOSPACE;
		term_word(p, bp->pos == EQNPOS_OVER ? "/" :
		    (bp->pos == EQNPOS_SUP ||
		     bp->pos == EQNPOS_TO) ? "^" : "_");
		p->flags |= TERMP_NOSPACE;
		child = child->next;
		if (child != NULL) {
			eqn_box(p, child);
			if (bp->pos == EQNPOS_FROMTO ||
			    bp->pos == EQNPOS_SUBSUP) {
				p->flags |= TERMP_NOSPACE;
				term_word(p, "^");
				p->flags |= TERMP_NOSPACE;
				child = child->next;
				if (child != NULL)
					eqn_box(p, child);
			}
		}
	} else {
		child = bp->first;
		if (bp->type == EQN_MATRIX && child->type == EQN_LIST)
			child = child->first;
		while (child != NULL) {
			eqn_box(p,
			    bp->type == EQN_PILE &&
			    child->type == EQN_LIST &&
			    child->args == 1 ?
			    child->first : child);
			child = child->next;
		}
	}

	if (bp->font != EQNFONT_NONE)
		term_fontpop(p);
	if (bp->type == EQN_LIST ||
	    (bp->type == EQN_PILE && (bp->prev || bp->next)) ||
	    (bp->parent != NULL && bp->parent->pos == EQNPOS_SQRT)) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, bp->right != NULL ? bp->right : ")");
		if (bp->parent->type == EQN_SUBEXPR && bp->next != NULL)
			p->flags |= TERMP_NOSPACE;
	}

	if (bp->top != NULL) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, bp->top);
	}
	if (bp->bottom != NULL) {
		p->flags |= TERMP_NOSPACE;
		term_word(p, "_");
	}
}
