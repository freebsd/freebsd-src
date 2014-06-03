/*	$Id: tbl_term.c,v 1.25 2013/05/31 21:37:17 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2012 Ingo Schwarze <schwarze@openbsd.org>
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

static	size_t	term_tbl_len(size_t, void *);
static	size_t	term_tbl_strlen(const char *, void *);
static	void	tbl_char(struct termp *, char, size_t);
static	void	tbl_data(struct termp *, const struct tbl_opts *,
			const struct tbl_dat *, 
			const struct roffcol *);
static	size_t	tbl_rulewidth(struct termp *, const struct tbl_head *);
static	void	tbl_hframe(struct termp *, const struct tbl_span *, int);
static	void	tbl_literal(struct termp *, const struct tbl_dat *, 
			const struct roffcol *);
static	void	tbl_number(struct termp *, const struct tbl_opts *, 
			const struct tbl_dat *, 
			const struct roffcol *);
static	void	tbl_hrule(struct termp *, const struct tbl_span *);
static	void	tbl_vrule(struct termp *, const struct tbl_head *);


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
	const struct tbl_head	*hp;
	const struct tbl_dat	*dp;
	struct roffcol		*col;
	int			 spans;
	size_t		   	 rmargin, maxrmargin;

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

	if (TBL_SPAN_FIRST & sp->flags) {
		term_flushln(tp);

		tp->tbl.len = term_tbl_len;
		tp->tbl.slen = term_tbl_strlen;
		tp->tbl.arg = tp;

		tblcalc(&tp->tbl, sp);
	}

	/* Horizontal frame at the start of boxed tables. */

	if (TBL_SPAN_FIRST & sp->flags) {
		if (TBL_OPT_DBOX & sp->opts->opts)
			tbl_hframe(tp, sp, 1);
		if (TBL_OPT_DBOX & sp->opts->opts ||
		    TBL_OPT_BOX  & sp->opts->opts)
			tbl_hframe(tp, sp, 0);
	}

	/* Vertical frame at the start of each row. */

	if (TBL_OPT_BOX & sp->opts->opts || TBL_OPT_DBOX & sp->opts->opts)
		term_word(tp, TBL_SPAN_HORIZ == sp->pos ||
			TBL_SPAN_DHORIZ == sp->pos ? "+" : "|");

	/*
	 * Now print the actual data itself depending on the span type.
	 * Spanner spans get a horizontal rule; data spanners have their
	 * data printed by matching data to header.
	 */

	switch (sp->pos) {
	case (TBL_SPAN_HORIZ):
		/* FALLTHROUGH */
	case (TBL_SPAN_DHORIZ):
		tbl_hrule(tp, sp);
		break;
	case (TBL_SPAN_DATA):
		/* Iterate over template headers. */
		dp = sp->first;
		spans = 0;
		for (hp = sp->head; hp; hp = hp->next) {

			/* 
			 * If the current data header is invoked during
			 * a spanner ("spans" > 0), don't emit anything
			 * at all.
			 */

			if (--spans >= 0)
				continue;

			/* Separate columns. */

			if (NULL != hp->prev)
				tbl_vrule(tp, hp);

			col = &tp->tbl.cols[hp->ident];
			tbl_data(tp, sp->opts, dp, col);

			/* 
			 * Go to the next data cell and assign the
			 * number of subsequent spans, if applicable.
			 */

			if (dp) {
				spans = dp->spans;
				dp = dp->next;
			}
		}
		break;
	}

	/* Vertical frame at the end of each row. */

	if (TBL_OPT_BOX & sp->opts->opts || TBL_OPT_DBOX & sp->opts->opts)
		term_word(tp, TBL_SPAN_HORIZ == sp->pos ||
			TBL_SPAN_DHORIZ == sp->pos ? "+" : " |");
	term_flushln(tp);

	/*
	 * If we're the last row, clean up after ourselves: clear the
	 * existing table configuration and set it to NULL.
	 */

	if (TBL_SPAN_LAST & sp->flags) {
		if (TBL_OPT_DBOX & sp->opts->opts ||
		    TBL_OPT_BOX  & sp->opts->opts) {
			tbl_hframe(tp, sp, 0);
			tp->skipvsp = 1;
		}
		if (TBL_OPT_DBOX & sp->opts->opts) {
			tbl_hframe(tp, sp, 1);
			tp->skipvsp = 2;
		}
		assert(tp->tbl.cols);
		free(tp->tbl.cols);
		tp->tbl.cols = NULL;
	}

	tp->flags &= ~TERMP_NONOSPACE;
	tp->rmargin = rmargin;
	tp->maxrmargin = maxrmargin;

}

/*
 * Horizontal rules extend across the entire table.
 * Calculate the width by iterating over columns.
 */
static size_t
tbl_rulewidth(struct termp *tp, const struct tbl_head *hp)
{
	size_t		 width;

	width = tp->tbl.cols[hp->ident].width;

	/* Account for leading blanks. */
	if (hp->prev)
		width += 2 - hp->vert;

	/* Account for trailing blank. */
	width++;

	return(width);
}

/*
 * Rules inside the table can be single or double
 * and have crossings with vertical rules marked with pluses.
 */
static void
tbl_hrule(struct termp *tp, const struct tbl_span *sp)
{
	const struct tbl_head *hp;
	char		 c;

	c = '-';
	if (TBL_SPAN_DHORIZ == sp->pos)
		c = '=';

	for (hp = sp->head; hp; hp = hp->next) {
		if (hp->prev && hp->vert)
			tbl_char(tp, '+', hp->vert);
		tbl_char(tp, c, tbl_rulewidth(tp, hp));
	}
}

/*
 * Rules above and below the table are always single
 * and have an additional plus at the beginning and end.
 * For double frames, this function is called twice,
 * and the outer one does not have crossings.
 */
static void
tbl_hframe(struct termp *tp, const struct tbl_span *sp, int outer)
{
	const struct tbl_head *hp;

	term_word(tp, "+");
	for (hp = sp->head; hp; hp = hp->next) {
		if (hp->prev && hp->vert)
			tbl_char(tp, (outer ? '-' : '+'), hp->vert);
		tbl_char(tp, '-', tbl_rulewidth(tp, hp));
	}
	term_word(tp, "+");
	term_flushln(tp);
}

static void
tbl_data(struct termp *tp, const struct tbl_opts *opts,
		const struct tbl_dat *dp, 
		const struct roffcol *col)
{

	if (NULL == dp) {
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	}
	assert(dp->layout);

	switch (dp->pos) {
	case (TBL_DATA_NONE):
		tbl_char(tp, ASCII_NBRSP, col->width);
		return;
	case (TBL_DATA_HORIZ):
		/* FALLTHROUGH */
	case (TBL_DATA_NHORIZ):
		tbl_char(tp, '-', col->width);
		return;
	case (TBL_DATA_NDHORIZ):
		/* FALLTHROUGH */
	case (TBL_DATA_DHORIZ):
		tbl_char(tp, '=', col->width);
		return;
	default:
		break;
	}
	
	switch (dp->layout->pos) {
	case (TBL_CELL_HORIZ):
		tbl_char(tp, '-', col->width);
		break;
	case (TBL_CELL_DHORIZ):
		tbl_char(tp, '=', col->width);
		break;
	case (TBL_CELL_LONG):
		/* FALLTHROUGH */
	case (TBL_CELL_CENTRE):
		/* FALLTHROUGH */
	case (TBL_CELL_LEFT):
		/* FALLTHROUGH */
	case (TBL_CELL_RIGHT):
		tbl_literal(tp, dp, col);
		break;
	case (TBL_CELL_NUMBER):
		tbl_number(tp, opts, dp, col);
		break;
	case (TBL_CELL_DOWN):
		tbl_char(tp, ASCII_NBRSP, col->width);
		break;
	default:
		abort();
		/* NOTREACHED */
	}
}

static void
tbl_vrule(struct termp *tp, const struct tbl_head *hp)
{

	tbl_char(tp, ASCII_NBRSP, 1);
	if (0 < hp->vert)
		tbl_char(tp, '|', hp->vert);
	if (2 > hp->vert)
		tbl_char(tp, ASCII_NBRSP, 2 - hp->vert);
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
	struct tbl_head		*hp;
	size_t			 width, len, padl, padr;
	int			 spans;

	assert(dp->string);
	len = term_strlen(tp, dp->string);

	hp = dp->layout->head->next;
	width = col->width;
	for (spans = dp->spans; spans--; hp = hp->next)
		width += tp->tbl.cols[hp->ident].width + 3;

	padr = width > len ? width - len : 0;
	padl = 0;

	switch (dp->layout->pos) {
	case (TBL_CELL_LONG):
		padl = term_len(tp, 1);
		padr = padr > padl ? padr - padl : 0;
		break;
	case (TBL_CELL_CENTRE):
		if (2 > padr)
			break;
		padl = padr / 2;
		padr -= padl;
		break;
	case (TBL_CELL_RIGHT):
		padl = padr;
		padr = 0;
		break;
	default:
		break;
	}

	tbl_char(tp, ASCII_NBRSP, padl);
	term_word(tp, dp->string);
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

	if (NULL != (cp = strrchr(dp->string, opts->decimal))) {
		buf[1] = '\0';
		for (ssz = 0, i = 0; cp != &dp->string[i]; i++) {
			buf[0] = dp->string[i];
			ssz += term_strlen(tp, buf);
		}
		d = ssz + psz;
	} else
		d = sz + psz;

	padl = col->decimal - d;

	tbl_char(tp, ASCII_NBRSP, padl);
	term_word(tp, dp->string);
	if (col->width > sz + padl)
		tbl_char(tp, ASCII_NBRSP, col->width - sz - padl);
}

