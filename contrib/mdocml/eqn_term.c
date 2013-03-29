/*	$Id: eqn_term.c,v 1.4 2011/07/24 10:09:03 kristaps Exp $ */
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

	p->flags |= TERMP_NONOSPACE;
	eqn_box(p, ep->root);
	term_word(p, " ");
	p->flags &= ~TERMP_NONOSPACE;
}

static void
eqn_box(struct termp *p, const struct eqn_box *bp)
{

	if (EQNFONT_NONE != bp->font)
		term_fontpush(p, fontmap[(int)bp->font]);
	if (bp->left)
		term_word(p, bp->left);
	if (EQN_SUBEXPR == bp->type)
		term_word(p, "(");

	if (bp->text)
		term_word(p, bp->text);

	if (bp->first)
		eqn_box(p, bp->first);

	if (EQN_SUBEXPR == bp->type)
		term_word(p, ")");
	if (bp->right)
		term_word(p, bp->right);
	if (EQNFONT_NONE != bp->font) 
		term_fontpop(p);

	if (bp->next)
		eqn_box(p, bp->next);
}
