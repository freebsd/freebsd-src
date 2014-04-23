/*	$Id: tbl_data.c,v 1.27 2013/06/01 04:56:50 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "libroff.h"

static	int		 data(struct tbl_node *, struct tbl_span *, 
				int, const char *, int *);
static	struct tbl_span	*newspan(struct tbl_node *, int, 
				struct tbl_row *);

static int
data(struct tbl_node *tbl, struct tbl_span *dp, 
		int ln, const char *p, int *pos)
{
	struct tbl_dat	*dat;
	struct tbl_cell	*cp;
	int		 sv, spans;

	cp = NULL;
	if (dp->last && dp->last->layout)
		cp = dp->last->layout->next;
	else if (NULL == dp->last)
		cp = dp->layout->first;

	/* 
	 * Skip over spanners, since
	 * we want to match data with data layout cells in the header.
	 */

	while (cp && TBL_CELL_SPAN == cp->pos)
		cp = cp->next;

	/*
	 * Stop processing when we reach the end of the available layout
	 * cells.  This means that we have extra input.
	 */

	if (NULL == cp) {
		mandoc_msg(MANDOCERR_TBLEXTRADAT, 
				tbl->parse, ln, *pos, NULL);
		/* Skip to the end... */
		while (p[*pos])
			(*pos)++;
		return(1);
	}

	dat = mandoc_calloc(1, sizeof(struct tbl_dat));
	dat->layout = cp;
	dat->pos = TBL_DATA_NONE;

	assert(TBL_CELL_SPAN != cp->pos);

	for (spans = 0, cp = cp->next; cp; cp = cp->next)
		if (TBL_CELL_SPAN == cp->pos)
			spans++;
		else
			break;
	
	dat->spans = spans;

	if (dp->last) {
		dp->last->next = dat;
		dp->last = dat;
	} else
		dp->last = dp->first = dat;

	sv = *pos;
	while (p[*pos] && p[*pos] != tbl->opts.tab)
		(*pos)++;

	/*
	 * Check for a continued-data scope opening.  This consists of a
	 * trailing `T{' at the end of the line.  Subsequent lines,
	 * until a standalone `T}', are included in our cell.
	 */

	if (*pos - sv == 2 && 'T' == p[sv] && '{' == p[sv + 1]) {
		tbl->part = TBL_PART_CDATA;
		return(1);
	}

	assert(*pos - sv >= 0);

	dat->string = mandoc_malloc((size_t)(*pos - sv + 1));
	memcpy(dat->string, &p[sv], (size_t)(*pos - sv));
	dat->string[*pos - sv] = '\0';

	if (p[*pos])
		(*pos)++;

	if ( ! strcmp(dat->string, "_"))
		dat->pos = TBL_DATA_HORIZ;
	else if ( ! strcmp(dat->string, "="))
		dat->pos = TBL_DATA_DHORIZ;
	else if ( ! strcmp(dat->string, "\\_"))
		dat->pos = TBL_DATA_NHORIZ;
	else if ( ! strcmp(dat->string, "\\="))
		dat->pos = TBL_DATA_NDHORIZ;
	else
		dat->pos = TBL_DATA_DATA;

	if (TBL_CELL_HORIZ == dat->layout->pos ||
			TBL_CELL_DHORIZ == dat->layout->pos ||
			TBL_CELL_DOWN == dat->layout->pos)
		if (TBL_DATA_DATA == dat->pos && '\0' != *dat->string)
			mandoc_msg(MANDOCERR_TBLIGNDATA, 
					tbl->parse, ln, sv, NULL);

	return(1);
}

/* ARGSUSED */
int
tbl_cdata(struct tbl_node *tbl, int ln, const char *p)
{
	struct tbl_dat	*dat;
	size_t	 	 sz;
	int		 pos;

	pos = 0;

	dat = tbl->last_span->last;

	if (p[pos] == 'T' && p[pos + 1] == '}') {
		pos += 2;
		if (p[pos] == tbl->opts.tab) {
			tbl->part = TBL_PART_DATA;
			pos++;
			return(data(tbl, tbl->last_span, ln, p, &pos));
		} else if ('\0' == p[pos]) {
			tbl->part = TBL_PART_DATA;
			return(1);
		}

		/* Fallthrough: T} is part of a word. */
	}

	dat->pos = TBL_DATA_DATA;

	if (dat->string) {
		sz = strlen(p) + strlen(dat->string) + 2;
		dat->string = mandoc_realloc(dat->string, sz);
		strlcat(dat->string, " ", sz);
		strlcat(dat->string, p, sz);
	} else
		dat->string = mandoc_strdup(p);

	if (TBL_CELL_DOWN == dat->layout->pos) 
		mandoc_msg(MANDOCERR_TBLIGNDATA, 
				tbl->parse, ln, pos, NULL);

	return(0);
}

static struct tbl_span *
newspan(struct tbl_node *tbl, int line, struct tbl_row *rp)
{
	struct tbl_span	*dp;

	dp = mandoc_calloc(1, sizeof(struct tbl_span));
	dp->line = line;
	dp->opts = &tbl->opts;
	dp->layout = rp;
	dp->head = tbl->first_head;

	if (tbl->last_span) {
		tbl->last_span->next = dp;
		tbl->last_span = dp;
	} else {
		tbl->last_span = tbl->first_span = dp;
		tbl->current_span = NULL;
		dp->flags |= TBL_SPAN_FIRST;
	}

	return(dp);
}

int
tbl_data(struct tbl_node *tbl, int ln, const char *p)
{
	struct tbl_span	*dp;
	struct tbl_row	*rp;
	int		 pos;

	pos = 0;

	if ('\0' == p[pos]) {
		mandoc_msg(MANDOCERR_TBL, tbl->parse, ln, pos, NULL);
		return(0);
	}

	/* 
	 * Choose a layout row: take the one following the last parsed
	 * span's.  If that doesn't exist, use the last parsed span's.
	 * If there's no last parsed span, use the first row.  Lastly,
	 * if the last span was a horizontal line, use the same layout
	 * (it doesn't "consume" the layout).
	 */

	if (tbl->last_span) {
		assert(tbl->last_span->layout);
		if (tbl->last_span->pos == TBL_SPAN_DATA) {
			for (rp = tbl->last_span->layout->next;
					rp && rp->first; rp = rp->next) {
				switch (rp->first->pos) {
				case (TBL_CELL_HORIZ):
					dp = newspan(tbl, ln, rp);
					dp->pos = TBL_SPAN_HORIZ;
					continue;
				case (TBL_CELL_DHORIZ):
					dp = newspan(tbl, ln, rp);
					dp->pos = TBL_SPAN_DHORIZ;
					continue;
				default:
					break;
				}
				break;
			}
		} else
			rp = tbl->last_span->layout;

		if (NULL == rp)
			rp = tbl->last_span->layout;
	} else
		rp = tbl->first_row;

	assert(rp);

	dp = newspan(tbl, ln, rp);

	if ( ! strcmp(p, "_")) {
		dp->pos = TBL_SPAN_HORIZ;
		return(1);
	} else if ( ! strcmp(p, "=")) {
		dp->pos = TBL_SPAN_DHORIZ;
		return(1);
	}

	dp->pos = TBL_SPAN_DATA;

	/* This returns 0 when TBL_PART_CDATA is entered. */

	while ('\0' != p[pos])
		if ( ! data(tbl, dp, ln, p, &pos))
			return(0);

	return(1);
}
