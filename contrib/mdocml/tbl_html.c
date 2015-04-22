/*	$Id: tbl_html.c,v 1.16 2015/01/30 17:32:16 schwarze Exp $ */
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
#include "config.h"

#include <sys/types.h>

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


static size_t
html_tbl_len(size_t sz, void *arg)
{

	return(sz);
}

static size_t
html_tbl_strlen(const char *p, void *arg)
{

	return(strlen(p));
}

static void
html_tblopen(struct html *h, const struct tbl_span *sp)
{
	struct htmlpair	 tag;
	struct roffsu	 su;
	struct roffcol	*col;
	int		 ic;

	if (h->tbl.cols == NULL) {
		h->tbl.len = html_tbl_len;
		h->tbl.slen = html_tbl_strlen;
		tblcalc(&h->tbl, sp, 0);
	}

	assert(NULL == h->tblt);
	PAIR_CLASS_INIT(&tag, "tbl");
	h->tblt = print_otag(h, TAG_TABLE, 1, &tag);

	for (ic = 0; ic < sp->opts->cols; ic++) {
		bufinit(h);
		col = h->tbl.cols + ic;
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
	const struct tbl_dat *dp;
	struct htmlpair	 tag;
	struct tag	*tt;
	int		 ic;

	/* Inhibit printing of spaces: we do padding ourselves. */

	if (h->tblt == NULL)
		html_tblopen(h, sp);

	assert(h->tblt);

	h->flags |= HTML_NONOSPACE;
	h->flags |= HTML_NOSPACE;

	tt = print_otag(h, TAG_TR, 0, NULL);

	switch (sp->pos) {
	case TBL_SPAN_HORIZ:
		/* FALLTHROUGH */
	case TBL_SPAN_DHORIZ:
		PAIR_INIT(&tag, ATTR_COLSPAN, "0");
		print_otag(h, TAG_TD, 1, &tag);
		break;
	default:
		dp = sp->first;
		for (ic = 0; ic < sp->opts->cols; ic++) {
			print_stagq(h, tt);
			print_otag(h, TAG_TD, 0, NULL);

			if (dp == NULL || dp->layout->col > ic)
				continue;
			if (dp->layout->pos != TBL_CELL_DOWN)
				if (dp->string != NULL)
					print_text(h, dp->string);
			dp = dp->next;
		}
		break;
	}

	print_tagq(h, tt);

	h->flags &= ~HTML_NONOSPACE;

	if (sp->next == NULL) {
		assert(h->tbl.cols);
		free(h->tbl.cols);
		h->tbl.cols = NULL;
		print_tblclose(h);
	}

}
