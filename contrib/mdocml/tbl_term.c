/*	$Id: tbl_term.c,v 1.40 2015/03/06 15:48:53 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2012, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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

static	size_t	term_tbl_len(size_t, void *);
static	size_t	term_tbl_strlen(const char *, void *);
static	void	tbl_char(struct termp *, char, size_t);
static	void	tbl_data(struct termp *, const struct tbl_opts *,
			const struct tbl_dat *,
			const struct roffcol *);
static	void	tbl_literal(struct termp *, const struct tbl_dat *,
			const struct roffcol *);
static	void	tbl_number(struct termp *, const struct tbl_opts *,
			const struct tbl_dat *,
			const struct roffcol *);
static	void	tbl_hrule(struct termp *, const struct tbl_span *, int);
static	void	tbl_word(struct termp *, const struct tbl_dat *);


static size_t
term_tbl_strlen(const char *p, void *arg)
{

	return(term_strlen((const struct termp *)arg, p));
}

static size_t
term_tbl_len(size_t sz, void *arg)
{

	return(term_len((const struct termp *)arg, sz));
}

void
term_tbl(struct termp *tp, const struct tbl_span *sp)
{
	const struct tbl_cell	*cp;
	const struct tbl_dat	*dp;
	static size_t		 offset;
	size_t			 rmargin, maxrmargin, tsz;
	int			 ic, horiz, spans, vert;

	rmargin = tp->rmargin;
	maxrmargin = tp->maxrmargin;

	tp->rmargin = tp->maxrmargin = TERM_MAXMARGIN;

	/* Inhibit printing of spaces: we do padding ourselves. */

	tp->flags |= TERMP_NONOSPACE;
	tp->flags |= TERMP_NOSPACE;

	/*
	 * The first time we're invoked for a given table block,
	 * calculate the table widths and decimal positions.
	 */

	if (tp->tbl.cols == NULL) {
		tp->tbl.len = term_tbl_len;
		tp->tbl.slen = term_tbl_strlen;
		tp->tbl.arg = tp;

		tblcalc(&tp->tbl, sp, rmargin - tp->offset);

		/* Center the table as a whole. */

		offset = tp->offset;
		if (sp->opts->opts & TBL_OPT_CENTRE) {
			tsz = sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX)
			    ? 2 : !!sp->opts->lvert + !!sp->opts->rvert;
			for (ic = 0; ic < sp->opts->cols; ic++)
				tsz += tp->tbl.cols[ic].width + 3;
			tsz -= 3;
			if (offset + tsz > rmargin)
				tsz -= 1;
			tp->offset = (offset + rmargin > tsz) ?
			    (offset + rmargin - tsz) / 2 : 0;
		}

		/* Horizontal frame at the start of boxed tables. */

		if (sp->opts->opts & TBL_OPT_DBOX)
			tbl_hrule(tp, sp, 2);
		if (sp->opts->opts & (TBL_OPT_DBOX | TBL_OPT_BOX))
			tbl_hrule(tp, sp, 1);
	}

	/* Vertical frame at the start of each row. */

	horiz = sp->pos == TBL_SPAN_HORIZ || sp->pos == TBL_SPAN_DHORIZ;

	if (sp->layout->vert ||
	    (sp->prev != NULL && sp->prev->layout->vert) ||
	    sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX))
		term_word(tp, horiz ? "+" : "|");
	else if (sp->opts->lvert)
		tbl_char(tp, horiz ? '-' : ASCII_NBRSP, 1);

	/*
	 * Now print the actual data itself depending on the span type.
	 * Match data cells to column numbers.
	 */

	if (sp->pos == TBL_SPAN_DATA) {
		cp = sp->layout->first;
		dp = sp->first;
		spans = 0;
		for (ic = 0; ic < sp->opts->cols; ic++) {

			/*
			 * Remeber whether we need a vertical bar
			 * after this cell.
			 */

			vert = cp == NULL ? 0 : cp->vert;

			/*
			 * Print the data and advance to the next cell.
			 */

			if (spans == 0) {
				tbl_data(tp, sp->opts, dp, tp->tbl.cols + ic);
				if (dp != NULL) {
					spans = dp->spans;
					dp = dp->next;
				}
			} else
				spans--;
			if (cp != NULL)
				cp = cp->next;

			/*
			 * Separate columns, except in the middle
			 * of spans and after the last cell.
			 */

			if (ic + 1 == sp->opts->cols || spans)
				continue;

			tbl_char(tp, ASCII_NBRSP, 1);
			if (vert > 0)
				tbl_char(tp, '|', vert);
			if (vert < 2)
				tbl_char(tp, ASCII_NBRSP, 2 - vert);
		}
	} else if (horiz)
		tbl_hrule(tp, sp, 0);

	/* Vertical frame at the end of each row. */

	if (sp->layout->last->vert ||
	    (sp->prev != NULL && sp->prev->layout->last->vert) ||
	    (sp->opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX)))
		term_word(tp, horiz ? "+" : " |");
	else if (sp->opts->rvert)
		tbl_char(tp, horiz ? '-' : ASCII_NBRSP, 1);
	term_flushln(tp);

	/*
	 * If we're the last row, clean up after ourselves: clear the
	 * existing table configuration and set it to NULL.
	 */

	if (sp->next == NULL) {
		if (sp->opts->opts & (TBL_OPT_DBOX | TBL_OPT_BOX)) {
			tbl_hrule(tp, sp, 1);
			tp->skipvsp = 1;
		}
		if (sp->opts->opts & TBL_OPT_DBOX) {
			tbl_hrule(tp, sp, 2);
			tp->skipvsp = 2;
		}
		assert(tp->tbl.cols);
		free(tp->tbl.cols);
		tp->tbl.cols = NULL;
		tp->offset = offset;
	}

	tp->flags &= ~TERMP_NONOSPACE;
	tp->rmargin = rmargin;
	tp->maxrmargin = maxrmargin;
}

/*
 * Kinds of horizontal rulers:
 * 0: inside the table (single or double line with crossings)
 * 1: inner frame (single line with crossings and ends)
 * 2: outer frame (single line without crossings with ends)
 */
static void
tbl_hrule(struct termp *tp, const struct tbl_span *sp, int kind)
{
	const struct tbl_cell *c1, *c2;
	int	 vert;
	char	 line, cross;

	line = (kind == 0 && TBL_SPAN_DHORIZ == sp->pos) ? '=' : '-';
	cross = (kind < 2) ? '+' : '-';

	if (kind)
		term_word(tp, "+");
	c1 = sp->layout->first;
	c2 = sp->prev == NULL ? NULL : sp->prev->layout->first;
	if (c2 == c1)
		c2 = NULL;
	for (;;) {
		tbl_char(tp, line, tp->tbl.cols[c1->col].width + 1);
		vert = c1->vert;
		if ((c1 = c1->next) == NULL)
			 break;
		if (c2 != NULL) {
			if (vert < c2->vert)
				vert = c2->vert;
			c2 = c2->next;
		}
		if (vert)
			tbl_char(tp, cross, vert);
		if (vert < 2)
			tbl_char(tp, line, 2 - vert);
	}
	if (kind) {
		term_word(tp, "+");
		term_flushln(tp);
	}
}

static void
tbl_data(struct termp *tp, const struct tbl_opts *opts,
	const struct tbl_dat *dp,
	const struct roffcol *col)
{

	if (dp == NULL) {
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	}

	switch (dp->pos) {
	case TBL_DATA_NONE:
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	case TBL_DATA_HORIZ:
		/* FALLTHROUGH */
	case TBL_DATA_NHORIZ:
		tbl_char(tp, '-', col->width);
		return;
	case TBL_DATA_NDHORIZ:
		/* FALLTHROUGH */
	case TBL_DATA_DHORIZ:
		tbl_char(tp, '=', col->width);
		return;
	default:
		break;
	}

	switch (dp->layout->pos) {
	case TBL_CELL_HORIZ:
		tbl_char(tp, '-', col->width);
		break;
	case TBL_CELL_DHORIZ:
		tbl_char(tp, '=', col->width);
		break;
	case TBL_CELL_LONG:
		/* FALLTHROUGH */
	case TBL_CELL_CENTRE:
		/* FALLTHROUGH */
	case TBL_CELL_LEFT:
		/* FALLTHROUGH */
	case TBL_CELL_RIGHT:
		tbl_literal(tp, dp, col);
		break;
	case TBL_CELL_NUMBER:
		tbl_number(tp, opts, dp, col);
		break;
	case TBL_CELL_DOWN:
		tbl_char(tp, ASCII_NBRSP, col->width);
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}

static void
tbl_char(struct termp *tp, char c, size_t len)
{
	size_t		i, sz;
	char		cp[2];

	cp[0] = c;
	cp[1] = '\0';

	sz = term_strlen(tp, cp);

	for (i = 0; i < len; i += sz)
		term_word(tp, cp);
}

static void
tbl_literal(struct termp *tp, const struct tbl_dat *dp,
		const struct roffcol *col)
{
	size_t		 len, padl, padr, width;
	int		 ic, spans;

	assert(dp->string);
	len = term_strlen(tp, dp->string);
	width = col->width;
	ic = dp->layout->col;
	spans = dp->spans;
	while (spans--)
		width += tp->tbl.cols[++ic].width + 3;

	padr = width > len ? width - len : 0;
	padl = 0;

	switch (dp->layout->pos) {
	case TBL_CELL_LONG:
		padl = term_len(tp, 1);
		padr = padr > padl ? padr - padl : 0;
		break;
	case TBL_CELL_CENTRE:
		if (2 > padr)
			break;
		padl = padr / 2;
		padr -= padl;
		break;
	case TBL_CELL_RIGHT:
		padl = padr;
		padr = 0;
		break;
	default:
		break;
	}

	tbl_char(tp, ASCII_NBRSP, padl);
	tbl_word(tp, dp);
	tbl_char(tp, ASCII_NBRSP, padr);
}

static void
tbl_number(struct termp *tp, const struct tbl_opts *opts,
		const struct tbl_dat *dp,
		const struct roffcol *col)
{
	char		*cp;
	char		 buf[2];
	size_t		 sz, psz, ssz, d, padl;
	int		 i;

	/*
	 * See calc_data_number().  Left-pad by taking the offset of our
	 * and the maximum decimal; right-pad by the remaining amount.
	 */

	assert(dp->string);

	sz = term_strlen(tp, dp->string);

	buf[0] = opts->decimal;
	buf[1] = '\0';

	psz = term_strlen(tp, buf);

	if ((cp = strrchr(dp->string, opts->decimal)) != NULL) {
		for (ssz = 0, i = 0; cp != &dp->string[i]; i++) {
			buf[0] = dp->string[i];
			ssz += term_strlen(tp, buf);
		}
		d = ssz + psz;
	} else
		d = sz + psz;

	if (col->decimal > d && col->width > sz) {
		padl = col->decimal - d;
		if (padl + sz > col->width)
			padl = col->width - sz;
		tbl_char(tp, ASCII_NBRSP, padl);
	} else
		padl = 0;
	tbl_word(tp, dp);
	if (col->width > sz + padl)
		tbl_char(tp, ASCII_NBRSP, col->width - sz - padl);
}

static void
tbl_word(struct termp *tp, const struct tbl_dat *dp)
{
	int		 prev_font;

	prev_font = tp->fonti;
	if (dp->layout->flags & TBL_CELL_BOLD)
		term_fontpush(tp, TERMFONT_BOLD);
	else if (dp->layout->flags & TBL_CELL_ITALIC)
		term_fontpush(tp, TERMFONT_UNDER);

	term_word(tp, dp->string);

	term_fontpopq(tp, prev_font);
}
