/*	$Id: eqn_html.c,v 1.11 2017/01/17 01:47:51 schwarze Exp $ */
/*
 * Copyright (c) 2011, 2014 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include "html.h"

static void
eqn_box(struct html *p, const struct eqn_box *bp)
{
	struct tag	*post, *row, *cell, *t;
	const struct eqn_box *child, *parent;
	size_t		 i, j, rows;

	if (NULL == bp)
		return;

	post = NULL;

	/*
	 * Special handling for a matrix, which is presented to us in
	 * column order, but must be printed in row-order.
	 */
	if (EQN_MATRIX == bp->type) {
		if (NULL == bp->first)
			goto out;
		if (EQN_LIST != bp->first->type) {
			eqn_box(p, bp->first);
			goto out;
		}
		if (NULL == (parent = bp->first->first))
			goto out;
		/* Estimate the number of rows, first. */
		if (NULL == (child = parent->first))
			goto out;
		for (rows = 0; NULL != child; rows++)
			child = child->next;
		/* Print row-by-row. */
		post = print_otag(p, TAG_MTABLE, "");
		for (i = 0; i < rows; i++) {
			parent = bp->first->first;
			row = print_otag(p, TAG_MTR, "");
			while (NULL != parent) {
				child = parent->first;
				for (j = 0; j < i; j++) {
					if (NULL == child)
						break;
					child = child->next;
				}
				cell = print_otag(p, TAG_MTD, "");
				/*
				 * If we have no data for this
				 * particular cell, then print a
				 * placeholder and continue--don't puke.
				 */
				if (NULL != child)
					eqn_box(p, child->first);
				print_tagq(p, cell);
				parent = parent->next;
			}
			print_tagq(p, row);
		}
		goto out;
	}

	switch (bp->pos) {
	case (EQNPOS_TO):
		post = print_otag(p, TAG_MOVER, "");
		break;
	case (EQNPOS_SUP):
		post = print_otag(p, TAG_MSUP, "");
		break;
	case (EQNPOS_FROM):
		post = print_otag(p, TAG_MUNDER, "");
		break;
	case (EQNPOS_SUB):
		post = print_otag(p, TAG_MSUB, "");
		break;
	case (EQNPOS_OVER):
		post = print_otag(p, TAG_MFRAC, "");
		break;
	case (EQNPOS_FROMTO):
		post = print_otag(p, TAG_MUNDEROVER, "");
		break;
	case (EQNPOS_SUBSUP):
		post = print_otag(p, TAG_MSUBSUP, "");
		break;
	case (EQNPOS_SQRT):
		post = print_otag(p, TAG_MSQRT, "");
		break;
	default:
		break;
	}

	if (bp->top || bp->bottom) {
		assert(NULL == post);
		if (bp->top && NULL == bp->bottom)
			post = print_otag(p, TAG_MOVER, "");
		else if (bp->top && bp->bottom)
			post = print_otag(p, TAG_MUNDEROVER, "");
		else if (bp->bottom)
			post = print_otag(p, TAG_MUNDER, "");
	}

	if (EQN_PILE == bp->type) {
		assert(NULL == post);
		if (bp->first != NULL && bp->first->type == EQN_LIST)
			post = print_otag(p, TAG_MTABLE, "");
	} else if (bp->type == EQN_LIST &&
	    bp->parent && bp->parent->type == EQN_PILE) {
		assert(NULL == post);
		post = print_otag(p, TAG_MTR, "");
		print_otag(p, TAG_MTD, "");
	}

	if (NULL != bp->text) {
		assert(NULL == post);
		post = print_otag(p, TAG_MI, "");
		print_text(p, bp->text);
	} else if (NULL == post) {
		if (NULL != bp->left || NULL != bp->right)
			post = print_otag(p, TAG_MFENCED, "??",
			    "open", bp->left == NULL ? "" : bp->left,
			    "close", bp->right == NULL ? "" : bp->right);
		if (NULL == post)
			post = print_otag(p, TAG_MROW, "");
		else
			print_otag(p, TAG_MROW, "");
	}

	eqn_box(p, bp->first);

out:
	if (NULL != bp->bottom) {
		t = print_otag(p, TAG_MO, "");
		print_text(p, bp->bottom);
		print_tagq(p, t);
	}
	if (NULL != bp->top) {
		t = print_otag(p, TAG_MO, "");
		print_text(p, bp->top);
		print_tagq(p, t);
	}

	if (NULL != post)
		print_tagq(p, post);

	eqn_box(p, bp->next);
}

void
print_eqn(struct html *p, const struct eqn *ep)
{
	struct tag	*t;

	t = print_otag(p, TAG_MATH, "c", "eqn");

	p->flags |= HTML_NONOSPACE;
	eqn_box(p, ep->root);
	p->flags &= ~HTML_NONOSPACE;

	print_tagq(p, t);
}
