/*	$Id: tbl_data.c,v 1.41 2015/10/06 18:32:20 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "libmandoc.h"
#include "libroff.h"

static	void		 getdata(struct tbl_node *, struct tbl_span *,
				int, const char *, int *);
static	struct tbl_span	*newspan(struct tbl_node *, int,
				struct tbl_row *);


static void
getdata(struct tbl_node *tbl, struct tbl_span *dp,
		int ln, const char *p, int *pos)
{
	struct tbl_dat	*dat;
	struct tbl_cell	*cp;
	int		 sv;

	/* Advance to the next layout cell, skipping spanners. */

	cp = dp->last == NULL ? dp->layout->first : dp->last->layout->next;
	while (cp != NULL && cp->pos == TBL_CELL_SPAN)
		cp = cp->next;

	/*
	 * Stop processing when we reach the end of the available layout
	 * cells.  This means that we have extra input.
	 */

	if (cp == NULL) {
		mandoc_msg(MANDOCERR_TBLDATA_EXTRA, tbl->parse,
		    ln, *pos, p + *pos);
		/* Skip to the end... */
		while (p[*pos])
			(*pos)++;
		return;
	}

	dat = mandoc_calloc(1, sizeof(*dat));
	dat->layout = cp;
	dat->pos = TBL_DATA_NONE;
	dat->spans = 0;
	for (cp = cp->next; cp != NULL; cp = cp->next)
		if (cp->pos == TBL_CELL_SPAN)
			dat->spans++;
		else
			break;

	if (dp->last == NULL)
		dp->first = dat;
	else
		dp->last->next = dat;
	dp->last = dat;

	sv = *pos;
	while (p[*pos] && p[*pos] != tbl->opts.tab)
		(*pos)++;

	/*
	 * Check for a continued-data scope opening.  This consists of a
	 * trailing `T{' at the end of the line.  Subsequent lines,
	 * until a standalone `T}', are included in our cell.
	 */

	if (*pos - sv == 2 && p[sv] == 'T' && p[sv + 1] == '{') {
		tbl->part = TBL_PART_CDATA;
		return;
	}

	dat->string = mandoc_strndup(p + sv, *pos - sv);

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

	if ((dat->layout->pos == TBL_CELL_HORIZ ||
	    dat->layout->pos == TBL_CELL_DHORIZ ||
	    dat->layout->pos == TBL_CELL_DOWN) &&
	    dat->pos == TBL_DATA_DATA && *dat->string != '\0')
		mandoc_msg(MANDOCERR_TBLDATA_SPAN,
		    tbl->parse, ln, sv, dat->string);
}

int
tbl_cdata(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	struct tbl_dat	*dat;
	size_t		 sz;

	dat = tbl->last_span->last;

	if (p[pos] == 'T' && p[pos + 1] == '}') {
		pos += 2;
		if (p[pos] == tbl->opts.tab) {
			tbl->part = TBL_PART_DATA;
			pos++;
			while (p[pos] != '\0')
				getdata(tbl, tbl->last_span, ln, p, &pos);
			return 1;
		} else if (p[pos] == '\0') {
			tbl->part = TBL_PART_DATA;
			return 1;
		}

		/* Fallthrough: T} is part of a word. */
	}

	dat->pos = TBL_DATA_DATA;

	if (dat->string != NULL) {
		sz = strlen(p + pos) + strlen(dat->string) + 2;
		dat->string = mandoc_realloc(dat->string, sz);
		(void)strlcat(dat->string, " ", sz);
		(void)strlcat(dat->string, p + pos, sz);
	} else
		dat->string = mandoc_strdup(p + pos);

	if (dat->layout->pos == TBL_CELL_DOWN)
		mandoc_msg(MANDOCERR_TBLDATA_SPAN, tbl->parse,
		    ln, pos, dat->string);

	return 0;
}

static struct tbl_span *
newspan(struct tbl_node *tbl, int line, struct tbl_row *rp)
{
	struct tbl_span	*dp;

	dp = mandoc_calloc(1, sizeof(*dp));
	dp->line = line;
	dp->opts = &tbl->opts;
	dp->layout = rp;
	dp->prev = tbl->last_span;

	if (dp->prev == NULL) {
		tbl->first_span = dp;
		tbl->current_span = NULL;
	} else
		dp->prev->next = dp;
	tbl->last_span = dp;

	return dp;
}

void
tbl_data(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	struct tbl_span	*dp;
	struct tbl_row	*rp;

	/*
	 * Choose a layout row: take the one following the last parsed
	 * span's.  If that doesn't exist, use the last parsed span's.
	 * If there's no last parsed span, use the first row.  Lastly,
	 * if the last span was a horizontal line, use the same layout
	 * (it doesn't "consume" the layout).
	 */

	if (tbl->last_span != NULL) {
		if (tbl->last_span->pos == TBL_SPAN_DATA) {
			for (rp = tbl->last_span->layout->next;
			     rp != NULL && rp->first != NULL;
			     rp = rp->next) {
				switch (rp->first->pos) {
				case TBL_CELL_HORIZ:
					dp = newspan(tbl, ln, rp);
					dp->pos = TBL_SPAN_HORIZ;
					continue;
				case TBL_CELL_DHORIZ:
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

		if (rp == NULL)
			rp = tbl->last_span->layout;
	} else
		rp = tbl->first_row;

	assert(rp);

	dp = newspan(tbl, ln, rp);

	if ( ! strcmp(p, "_")) {
		dp->pos = TBL_SPAN_HORIZ;
		return;
	} else if ( ! strcmp(p, "=")) {
		dp->pos = TBL_SPAN_DHORIZ;
		return;
	}

	dp->pos = TBL_SPAN_DATA;

	while (p[pos] != '\0')
		getdata(tbl, dp, ln, p, &pos);
}
