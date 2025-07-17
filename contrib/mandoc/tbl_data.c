/*	$Id: tbl_data.c,v 1.59 2021/09/10 13:24:38 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011,2015,2017-2019,2021 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "tbl.h"
#include "libmandoc.h"
#include "tbl_int.h"

static	void		 getdata(struct tbl_node *, struct tbl_span *,
				int, const char *, int *);
static	struct tbl_span	*newspan(struct tbl_node *, int,
				struct tbl_row *);


static void
getdata(struct tbl_node *tbl, struct tbl_span *dp,
		int ln, const char *p, int *pos)
{
	struct tbl_dat	*dat, *pdat;
	struct tbl_cell	*cp;
	struct tbl_span	*pdp;
	const char	*ccp;
	int		 startpos, endpos;

	/*
	 * Determine the length of the string in the cell
	 * and advance the parse point to the end of the cell.
	 */

	startpos = *pos;
	ccp = p + startpos;
	while (*ccp != '\0' && *ccp != tbl->opts.tab)
		if (*ccp++ == '\\')
			mandoc_escape(&ccp, NULL, NULL);
	*pos = ccp - p;

	/* Advance to the next layout cell, skipping spanners. */

	cp = dp->last == NULL ? dp->layout->first : dp->last->layout->next;
	while (cp != NULL && cp->pos == TBL_CELL_SPAN)
		cp = cp->next;

	/*
	 * If the current layout row is out of cells, allocate
	 * a new cell if another row of the table has at least
	 * this number of columns, or discard the input if we
	 * are beyond the last column of the table as a whole.
	 */

	if (cp == NULL) {
		if (dp->layout->last->col + 1 < dp->opts->cols) {
			cp = mandoc_calloc(1, sizeof(*cp));
			cp->pos = TBL_CELL_LEFT;
			cp->font = ESCAPE_FONTROMAN;
			cp->spacing = SIZE_MAX;
			dp->layout->last->next = cp;
			cp->col = dp->layout->last->col + 1;
			dp->layout->last = cp;
		} else {
			mandoc_msg(MANDOCERR_TBLDATA_EXTRA,
			    ln, startpos, "%s", p + startpos);
			while (p[*pos] != '\0')
				(*pos)++;
			return;
		}
	}

	dat = mandoc_malloc(sizeof(*dat));
	dat->layout = cp;
	dat->next = NULL;
	dat->string = NULL;
	dat->hspans = 0;
	dat->vspans = 0;
	dat->block = 0;
	dat->pos = TBL_DATA_NONE;

	/*
	 * Increment the number of vertical spans in a data cell above,
	 * if this cell vertically extends one or more cells above.
	 * The iteration must be done over data rows,
	 * not over layout rows, because one layout row
	 * can be reused for more than one data row.
	 */

	if (cp->pos == TBL_CELL_DOWN ||
	    (*pos - startpos == 2 &&
	     p[startpos] == '\\' && p[startpos + 1] == '^')) {
		pdp = dp;
		while ((pdp = pdp->prev) != NULL) {
			pdat = pdp->first;
			while (pdat != NULL &&
			    pdat->layout->col < dat->layout->col)
				pdat = pdat->next;
			if (pdat == NULL)
				break;
			if (pdat->layout->pos != TBL_CELL_DOWN &&
			    strcmp(pdat->string, "\\^") != 0) {
				pdat->vspans++;
				break;
			}
		}
	}

	/*
	 * Count the number of horizontal spans to the right of this cell.
	 * This is purely a matter of the layout, independent of the data.
	 */

	for (cp = cp->next; cp != NULL; cp = cp->next)
		if (cp->pos == TBL_CELL_SPAN)
			dat->hspans++;
		else
			break;

	/* Append the new data cell to the data row. */

	if (dp->last == NULL)
		dp->first = dat;
	else
		dp->last->next = dat;
	dp->last = dat;

	/* Strip leading and trailing spaces, if requested. */

	endpos = *pos;
	if (dp->opts->opts & TBL_OPT_NOSPACE) {
		while (p[startpos] == ' ')
			startpos++;
		while (endpos > startpos && p[endpos - 1] == ' ')
			endpos--;
	}

	/*
	 * Check for a continued-data scope opening.  This consists of a
	 * trailing `T{' at the end of the line.  Subsequent lines,
	 * until a standalone `T}', are included in our cell.
	 */

	if (endpos - startpos == 2 &&
	    p[startpos] == 'T' && p[startpos + 1] == '{') {
		tbl->part = TBL_PART_CDATA;
		return;
	}

	dat->string = mandoc_strndup(p + startpos, endpos - startpos);

	if (p[*pos] != '\0')
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
		    ln, startpos, "%s", dat->string);
}

void
tbl_cdata(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	struct tbl_dat	*dat;
	size_t		 sz;

	dat = tbl->last_span->last;

	if (p[pos] == 'T' && p[pos + 1] == '}') {
		pos += 2;
		if (tbl->opts.opts & TBL_OPT_NOSPACE)
			while (p[pos] == ' ')
				pos++;
		if (p[pos] == tbl->opts.tab) {
			tbl->part = TBL_PART_DATA;
			pos++;
			while (p[pos] != '\0')
				getdata(tbl, tbl->last_span, ln, p, &pos);
			return;
		} else if (p[pos] == '\0') {
			tbl->part = TBL_PART_DATA;
			return;
		}

		/* Fallthrough: T} is part of a word. */
	}

	dat->pos = TBL_DATA_DATA;
	dat->block = 1;

	if (dat->string != NULL) {
		sz = strlen(p + pos) + strlen(dat->string) + 2;
		dat->string = mandoc_realloc(dat->string, sz);
		(void)strlcat(dat->string, " ", sz);
		(void)strlcat(dat->string, p + pos, sz);
	} else
		dat->string = mandoc_strdup(p + pos);

	if (dat->layout->pos == TBL_CELL_DOWN)
		mandoc_msg(MANDOCERR_TBLDATA_SPAN,
		    ln, pos, "%s", dat->string);
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
	struct tbl_row	*rp;
	struct tbl_cell	*cp;
	struct tbl_span	*sp;

	for (sp = tbl->last_span; sp != NULL; sp = sp->prev)
		if (sp->pos == TBL_SPAN_DATA)
			break;
	rp = sp == NULL ? tbl->first_row :
	    sp->layout->next == NULL ? sp->layout : sp->layout->next;
	assert(rp != NULL);

	if (p[1] == '\0') {
		switch (p[0]) {
		case '.':
			/*
			 * Empty request lines must be handled here
			 * and cannot be discarded in roff_parseln()
			 * because in the layout section, they
			 * are significant and end the layout.
			 */
			return;
		case '_':
			sp = newspan(tbl, ln, rp);
			sp->pos = TBL_SPAN_HORIZ;
			return;
		case '=':
			sp = newspan(tbl, ln, rp);
			sp->pos = TBL_SPAN_DHORIZ;
			return;
		default:
			break;
		}
	}

	/*
	 * If the layout row contains nothing but horizontal lines,
	 * allocate an empty span for it and assign the current span
	 * to the next layout row accepting data.
	 */

	while (rp->next != NULL) {
		if (rp->last->col + 1 < tbl->opts.cols)
			break;
		for (cp = rp->first; cp != NULL; cp = cp->next)
			if (cp->pos != TBL_CELL_HORIZ &&
			    cp->pos != TBL_CELL_DHORIZ)
				break;
		if (cp != NULL)
			break;
		sp = newspan(tbl, ln, rp);
		sp->pos = TBL_SPAN_DATA;
		rp = rp->next;
	}

	/* Process a real data row. */

	sp = newspan(tbl, ln, rp);
	sp->pos = TBL_SPAN_DATA;
	while (p[pos] != '\0')
		getdata(tbl, sp, ln, p, &pos);
}
