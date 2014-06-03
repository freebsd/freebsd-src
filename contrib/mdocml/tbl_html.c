/*	$Id: tbl_html.c,v 1.10 2012/05/27 17:54:54 schwarze Exp $ */
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
#include "html.h"

static	void	 html_tblopen(struct html *, const struct tbl_span *);
static	size_t	 html_tbl_len(size_t, void *);
static	size_t	 html_tbl_strlen(const char *, void *);

/* ARGSUSED */
static size_t
html_tbl_len(size_t sz, void *arg)
{
	
	return(sz);
}

/* ARGSUSED */
static size_t
html_tbl_strlen(const char *p, void *arg)
{

	return(strlen(p));
}

static void
html_tblopen(struct html *h, const struct tbl_span *sp)
{
	const struct tbl_head *hp;
	struct htmlpair	 tag;
	struct roffsu	 su;
	struct roffcol	*col;

	if (TBL_SPAN_FIRST & sp->flags) {
		h->tbl.len = html_tbl_len;
		h->tbl.slen = html_tbl_strlen;
		tblcalc(&h->tbl, sp);
	}

	assert(NULL == h->tblt);
	PAIR_CLASS_INIT(&tag, "tbl");
	h->tblt = print_otag(h, TAG_TABLE, 1, &tag);

	for (hp = sp->head; hp; hp = hp->next) {
		bufinit(h);
		col = &h->tbl.cols[hp->ident];
		SCALE_HS_INIT(&su, col->width);
		bufcat_su(h, "width", &su);
		PAIR_STYLE_INIT(&tag, h);
		print_otag(h, TAG_COL, 1, &tag);
	}

	print_otag(h, TAG_TBODY, 0, NULL);
}

void
print_tblclose(struct html *h)
{

	assert(h->tblt);
	print_tagq(h, h->tblt);
	h->tblt = NULL;
}

void
print_tbl(struct html *h, const struct tbl_span *sp)
{
	const struct tbl_head *hp;
	const struct tbl_dat *dp;
	struct htmlpair	 tag;
	struct tag	*tt;

	/* Inhibit printing of spaces: we do padding ourselves. */

	if (NULL == h->tblt)
		html_tblopen(h, sp);

	assert(h->tblt);

	h->flags |= HTML_NONOSPACE;
	h->flags |= HTML_NOSPACE;

	tt = print_otag(h, TAG_TR, 0, NULL);

	switch (sp->pos) {
	case (TBL_SPAN_HORIZ):
		/* FALLTHROUGH */
	case (TBL_SPAN_DHORIZ):
		PAIR_INIT(&tag, ATTR_COLSPAN, "0");
		print_otag(h, TAG_TD, 1, &tag);
		break;
	default:
		dp = sp->first;
		for (hp = sp->head; hp; hp = hp->next) {
			print_stagq(h, tt);
			print_otag(h, TAG_TD, 0, NULL);

			if (NULL == dp)
				break;
			if (TBL_CELL_DOWN != dp->layout->pos)
				if (dp->string)
					print_text(h, dp->string);
			dp = dp->next;
		}
		break;
	}

	print_tagq(h, tt);

	h->flags &= ~HTML_NONOSPACE;

	if (TBL_SPAN_LAST & sp->flags) {
		assert(h->tbl.cols);
		free(h->tbl.cols);
		h->tbl.cols = NULL;
		print_tblclose(h);
	}

}
