/*	$Id: out.c,v 1.77 2018/12/13 11:55:47 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011,2014,2015,2017,2018 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "tbl.h"
#include "out.h"

struct	tbl_colgroup {
	struct tbl_colgroup	*next;
	size_t			 wanted;
	int			 startcol;
	int			 endcol;
};

static	size_t	tblcalc_data(struct rofftbl *, struct roffcol *,
			const struct tbl_opts *, const struct tbl_dat *,
			size_t);
static	size_t	tblcalc_literal(struct rofftbl *, struct roffcol *,
			const struct tbl_dat *, size_t);
static	size_t	tblcalc_number(struct rofftbl *, struct roffcol *,
			const struct tbl_opts *, const struct tbl_dat *);


/*
 * Parse the *src string and store a scaling unit into *dst.
 * If the string doesn't specify the unit, use the default.
 * If no default is specified, fail.
 * Return a pointer to the byte after the last byte used,
 * or NULL on total failure.
 */
const char *
a2roffsu(const char *src, struct roffsu *dst, enum roffscale def)
{
	char		*endptr;

	dst->unit = def == SCALE_MAX ? SCALE_BU : def;
	dst->scale = strtod(src, &endptr);
	if (endptr == src)
		return NULL;

	switch (*endptr++) {
	case 'c':
		dst->unit = SCALE_CM;
		break;
	case 'i':
		dst->unit = SCALE_IN;
		break;
	case 'f':
		dst->unit = SCALE_FS;
		break;
	case 'M':
		dst->unit = SCALE_MM;
		break;
	case 'm':
		dst->unit = SCALE_EM;
		break;
	case 'n':
		dst->unit = SCALE_EN;
		break;
	case 'P':
		dst->unit = SCALE_PC;
		break;
	case 'p':
		dst->unit = SCALE_PT;
		break;
	case 'u':
		dst->unit = SCALE_BU;
		break;
	case 'v':
		dst->unit = SCALE_VS;
		break;
	default:
		endptr--;
		if (SCALE_MAX == def)
			return NULL;
		dst->unit = def;
		break;
	}
	return endptr;
}

/*
 * Calculate the abstract widths and decimal positions of columns in a
 * table.  This routine allocates the columns structures then runs over
 * all rows and cells in the table.  The function pointers in "tbl" are
 * used for the actual width calculations.
 */
void
tblcalc(struct rofftbl *tbl, const struct tbl_span *sp_first,
    size_t offset, size_t rmargin)
{
	struct roffsu		 su;
	const struct tbl_opts	*opts;
	const struct tbl_span	*sp;
	const struct tbl_dat	*dp;
	struct roffcol		*col;
	struct tbl_colgroup	*first_group, **gp, *g;
	size_t			*colwidth;
	size_t			 ewidth, min1, min2, wanted, width, xwidth;
	int			 done, icol, maxcol, necol, nxcol, quirkcol;

	/*
	 * Allocate the master column specifiers.  These will hold the
	 * widths and decimal positions for all cells in the column.  It
	 * must be freed and nullified by the caller.
	 */

	assert(tbl->cols == NULL);
	tbl->cols = mandoc_calloc((size_t)sp_first->opts->cols,
	    sizeof(struct roffcol));
	opts = sp_first->opts;

	maxcol = -1;
	first_group = NULL;
	for (sp = sp_first; sp != NULL; sp = sp->next) {
		if (sp->pos != TBL_SPAN_DATA)
			continue;

		/*
		 * Account for the data cells in the layout, matching it
		 * to data cells in the data section.
		 */

		gp = &first_group;
		for (dp = sp->first; dp != NULL; dp = dp->next) {
			icol = dp->layout->col;
			while (icol > maxcol)
				tbl->cols[++maxcol].spacing = SIZE_MAX;
			col = tbl->cols + icol;
			col->flags |= dp->layout->flags;
			if (dp->layout->flags & TBL_CELL_WIGN)
				continue;

			/* Handle explicit width specifications. */

			if (dp->layout->wstr != NULL &&
			    dp->layout->width == 0 &&
			    a2roffsu(dp->layout->wstr, &su, SCALE_EN)
			    != NULL)
				dp->layout->width =
				    (*tbl->sulen)(&su, tbl->arg);
			if (col->width < dp->layout->width)
				col->width = dp->layout->width;
			if (dp->layout->spacing != SIZE_MAX &&
			    (col->spacing == SIZE_MAX ||
			     col->spacing < dp->layout->spacing))
				col->spacing = dp->layout->spacing;

			/*
			 * Calculate an automatic width.
			 * Except for spanning cells, apply it.
			 */

			width = tblcalc_data(tbl,
			    dp->hspans == 0 ? col : NULL,
			    opts, dp,
			    dp->block == 0 ? 0 :
			    dp->layout->width ? dp->layout->width :
			    rmargin ? (rmargin + sp->opts->cols / 2)
			    / (sp->opts->cols + 1) : 0);
			if (dp->hspans == 0)
				continue;

			/*
			 * Build an ordered, singly linked list
			 * of all groups of columns joined by spans,
			 * recording the minimum width for each group.
			 */

			while (*gp != NULL && ((*gp)->startcol < icol ||
			    (*gp)->endcol < icol + dp->hspans))
				gp = &(*gp)->next;
			if (*gp == NULL || (*gp)->startcol > icol ||
                            (*gp)->endcol > icol + dp->hspans) {
				g = mandoc_malloc(sizeof(*g));
				g->next = *gp;
				g->wanted = width;
				g->startcol = icol;
				g->endcol = icol + dp->hspans;
				*gp = g;
			} else if ((*gp)->wanted < width)
				(*gp)->wanted = width;
		}
	}

	/*
	 * Column spacings are needed for span width calculations,
	 * so set the default values now.
	 */

	for (icol = 0; icol <= maxcol; icol++)
		if (tbl->cols[icol].spacing == SIZE_MAX || icol == maxcol)
			tbl->cols[icol].spacing = 3;

	/*
	 * Replace the minimum widths with the missing widths,
	 * and dismiss groups that are already wide enough.
	 */

	gp = &first_group;
	while ((g = *gp) != NULL) {
		done = 0;
		for (icol = g->startcol; icol <= g->endcol; icol++) {
			width = tbl->cols[icol].width;
			if (icol < g->endcol)
				width += tbl->cols[icol].spacing;
			if (g->wanted <= width) {
				done = 1;
				break;
			} else
				(*gp)->wanted -= width;
		}
		if (done) {
			*gp = g->next;
			free(g);
		} else
			gp = &(*gp)->next;
	}

	colwidth = mandoc_reallocarray(NULL, maxcol + 1, sizeof(*colwidth));
	while (first_group != NULL) {

		/*
		 * Rebuild the array of the widths of all columns
		 * participating in spans that require expansion.
		 */

		for (icol = 0; icol <= maxcol; icol++)
			colwidth[icol] = SIZE_MAX;
		for (g = first_group; g != NULL; g = g->next)
			for (icol = g->startcol; icol <= g->endcol; icol++)
				colwidth[icol] = tbl->cols[icol].width;

		/*
		 * Find the smallest and second smallest column width
		 * among the columns which may need expamsion.
		 */

		min1 = min2 = SIZE_MAX;
		for (icol = 0; icol <= maxcol; icol++) {
			if (min1 > colwidth[icol]) {
				min2 = min1;
				min1 = colwidth[icol];
			} else if (min1 < colwidth[icol] &&
			    min2 > colwidth[icol])
				min2 = colwidth[icol];
		}

		/*
		 * Find the minimum wanted width
		 * for any one of the narrowest columns,
		 * and mark the columns wanting that width.
		 */

		wanted = min2;
		for (g = first_group; g != NULL; g = g->next) {
			necol = 0;
			for (icol = g->startcol; icol <= g->endcol; icol++)
				if (tbl->cols[icol].width == min1)
					necol++;
			if (necol == 0)
				continue;
			width = min1 + (g->wanted - 1) / necol + 1;
			if (width > min2)
				width = min2;
			if (wanted > width)
				wanted = width;
			for (icol = g->startcol; icol <= g->endcol; icol++)
				if (colwidth[icol] == min1 ||
				    (colwidth[icol] < min2 &&
				     colwidth[icol] > width))
					colwidth[icol] = width;
		}

		/* Record the effect of the widening on the group list. */

		gp = &first_group;
		while ((g = *gp) != NULL) {
			done = 0;
			for (icol = g->startcol; icol <= g->endcol; icol++) {
				if (colwidth[icol] != wanted ||
				    tbl->cols[icol].width == wanted)
					continue;
				if (g->wanted <= wanted - min1) {
					done = 1;
					break;
				}
				g->wanted -= wanted - min1;
			}
			if (done) {
				*gp = g->next;
				free(g);
			} else
				gp = &(*gp)->next;
		}

		/* Record the effect of the widening on the columns. */

		for (icol = 0; icol <= maxcol; icol++)
			if (colwidth[icol] == wanted)
				tbl->cols[icol].width = wanted;
	}
	free(colwidth);

	/*
	 * Align numbers with text.
	 * Count columns to equalize and columns to maximize.
	 * Find maximum width of the columns to equalize.
	 * Find total width of the columns *not* to maximize.
	 */

	necol = nxcol = 0;
	ewidth = xwidth = 0;
	for (icol = 0; icol <= maxcol; icol++) {
		col = tbl->cols + icol;
		if (col->width > col->nwidth)
			col->decimal += (col->width - col->nwidth) / 2;
		else
			col->width = col->nwidth;
		if (col->flags & TBL_CELL_EQUAL) {
			necol++;
			if (ewidth < col->width)
				ewidth = col->width;
		}
		if (col->flags & TBL_CELL_WMAX)
			nxcol++;
		else
			xwidth += col->width;
	}

	/*
	 * Equalize columns, if requested for any of them.
	 * Update total width of the columns not to maximize.
	 */

	if (necol) {
		for (icol = 0; icol <= maxcol; icol++) {
			col = tbl->cols + icol;
			if ( ! (col->flags & TBL_CELL_EQUAL))
				continue;
			if (col->width == ewidth)
				continue;
			if (nxcol && rmargin)
				xwidth += ewidth - col->width;
			col->width = ewidth;
		}
	}

	/*
	 * If there are any columns to maximize, find the total
	 * available width, deducting 3n margins between columns.
	 * Distribute the available width evenly.
	 */

	if (nxcol && rmargin) {
		xwidth += 3*maxcol +
		    (opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX) ?
		     2 : !!opts->lvert + !!opts->rvert);
		if (rmargin <= offset + xwidth)
			return;
		xwidth = rmargin - offset - xwidth;

		/*
		 * Emulate a bug in GNU tbl width calculation that
		 * manifests itself for large numbers of x-columns.
		 * Emulating it for 5 x-columns gives identical
		 * behaviour for up to 6 x-columns.
		 */

		if (nxcol == 5) {
			quirkcol = xwidth % nxcol + 2;
			if (quirkcol != 3 && quirkcol != 4)
				quirkcol = -1;
		} else
			quirkcol = -1;

		necol = 0;
		ewidth = 0;
		for (icol = 0; icol <= maxcol; icol++) {
			col = tbl->cols + icol;
			if ( ! (col->flags & TBL_CELL_WMAX))
				continue;
			col->width = (double)xwidth * ++necol / nxcol
			    - ewidth + 0.4995;
			if (necol == quirkcol)
				col->width--;
			ewidth += col->width;
		}
	}
}

static size_t
tblcalc_data(struct rofftbl *tbl, struct roffcol *col,
    const struct tbl_opts *opts, const struct tbl_dat *dp, size_t mw)
{
	size_t		 sz;

	/* Branch down into data sub-types. */

	switch (dp->layout->pos) {
	case TBL_CELL_HORIZ:
	case TBL_CELL_DHORIZ:
		sz = (*tbl->len)(1, tbl->arg);
		if (col != NULL && col->width < sz)
			col->width = sz;
		return sz;
	case TBL_CELL_LONG:
	case TBL_CELL_CENTRE:
	case TBL_CELL_LEFT:
	case TBL_CELL_RIGHT:
		return tblcalc_literal(tbl, col, dp, mw);
	case TBL_CELL_NUMBER:
		return tblcalc_number(tbl, col, opts, dp);
	case TBL_CELL_DOWN:
		return 0;
	default:
		abort();
	}
}

static size_t
tblcalc_literal(struct rofftbl *tbl, struct roffcol *col,
    const struct tbl_dat *dp, size_t mw)
{
	const char	*str;	/* Beginning of the first line. */
	const char	*beg;	/* Beginning of the current line. */
	char		*end;	/* End of the current line. */
	size_t		 lsz;	/* Length of the current line. */
	size_t		 wsz;	/* Length of the current word. */
	size_t		 msz;   /* Length of the longest line. */

	if (dp->string == NULL || *dp->string == '\0')
		return 0;
	str = mw ? mandoc_strdup(dp->string) : dp->string;
	msz = lsz = 0;
	for (beg = str; beg != NULL && *beg != '\0'; beg = end) {
		end = mw ? strchr(beg, ' ') : NULL;
		if (end != NULL) {
			*end++ = '\0';
			while (*end == ' ')
				end++;
		}
		wsz = (*tbl->slen)(beg, tbl->arg);
		if (mw && lsz && lsz + 1 + wsz <= mw)
			lsz += 1 + wsz;
		else
			lsz = wsz;
		if (msz < lsz)
			msz = lsz;
	}
	if (mw)
		free((void *)str);
	if (col != NULL && col->width < msz)
		col->width = msz;
	return msz;
}

static size_t
tblcalc_number(struct rofftbl *tbl, struct roffcol *col,
		const struct tbl_opts *opts, const struct tbl_dat *dp)
{
	const char	*cp, *lastdigit, *lastpoint;
	size_t		 intsz, totsz;
	char		 buf[2];

	if (dp->string == NULL || *dp->string == '\0')
		return 0;

	totsz = (*tbl->slen)(dp->string, tbl->arg);
	if (col == NULL)
		return totsz;

	/*
	 * Find the last digit and
	 * the last decimal point that is adjacent to a digit.
	 * The alignment indicator "\&" overrides everything.
	 */

	lastdigit = lastpoint = NULL;
	for (cp = dp->string; cp[0] != '\0'; cp++) {
		if (cp[0] == '\\' && cp[1] == '&') {
			lastdigit = lastpoint = cp;
			break;
		} else if (cp[0] == opts->decimal &&
		    (isdigit((unsigned char)cp[1]) ||
		     (cp > dp->string && isdigit((unsigned char)cp[-1]))))
			lastpoint = cp;
		else if (isdigit((unsigned char)cp[0]))
			lastdigit = cp;
	}

	/* Not a number, treat as a literal string. */

	if (lastdigit == NULL) {
		if (col != NULL && col->width < totsz)
			col->width = totsz;
		return totsz;
	}

	/* Measure the width of the integer part. */

	if (lastpoint == NULL)
		lastpoint = lastdigit + 1;
	intsz = 0;
	buf[1] = '\0';
	for (cp = dp->string; cp < lastpoint; cp++) {
		buf[0] = cp[0];
		intsz += (*tbl->slen)(buf, tbl->arg);
	}

	/*
         * If this number has more integer digits than all numbers
         * seen on earlier lines, shift them all to the right.
	 * If it has fewer, shift this number to the right.
	 */

	if (intsz > col->decimal) {
		col->nwidth += intsz - col->decimal;
		col->decimal = intsz;
	} else
		totsz += col->decimal - intsz;

	/* Update the maximum total width seen so far. */

	if (totsz > col->nwidth)
		col->nwidth = totsz;
	return totsz;
}
