/*	$Id: out.c,v 1.62 2015/10/12 00:08:16 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2011, 2014, 2015 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "out.h"

static	void	tblcalc_data(struct rofftbl *, struct roffcol *,
			const struct tbl_opts *, const struct tbl_dat *);
static	void	tblcalc_literal(struct rofftbl *, struct roffcol *,
			const struct tbl_dat *);
static	void	tblcalc_number(struct rofftbl *, struct roffcol *,
			const struct tbl_opts *, const struct tbl_dat *);


/*
 * Parse the *src string and store a scaling unit into *dst.
 * If the string doesn't specify the unit, use the default.
 * If no default is specified, fail.
 * Return 2 on complete success, 1 when a conversion was done,
 * but there was trailing garbage, and 0 on total failure.
 */
int
a2roffsu(const char *src, struct roffsu *dst, enum roffscale def)
{
	char		*endptr;

	dst->unit = def == SCALE_MAX ? SCALE_BU : def;
	dst->scale = strtod(src, &endptr);
	if (endptr == src)
		return 0;

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
	case '\0':
		endptr--;
		/* FALLTHROUGH */
	default:
		if (SCALE_MAX == def)
			return 0;
		dst->unit = def;
		break;
	}

	return *endptr == '\0' ? 2 : 1;
}

/*
 * Calculate the abstract widths and decimal positions of columns in a
 * table.  This routine allocates the columns structures then runs over
 * all rows and cells in the table.  The function pointers in "tbl" are
 * used for the actual width calculations.
 */
void
tblcalc(struct rofftbl *tbl, const struct tbl_span *sp,
	size_t totalwidth)
{
	const struct tbl_opts	*opts;
	const struct tbl_dat	*dp;
	struct roffcol		*col;
	size_t			 ewidth, xwidth;
	int			 spans;
	int			 icol, maxcol, necol, nxcol, quirkcol;

	/*
	 * Allocate the master column specifiers.  These will hold the
	 * widths and decimal positions for all cells in the column.  It
	 * must be freed and nullified by the caller.
	 */

	assert(NULL == tbl->cols);
	tbl->cols = mandoc_calloc((size_t)sp->opts->cols,
	    sizeof(struct roffcol));
	opts = sp->opts;

	for (maxcol = -1; sp; sp = sp->next) {
		if (TBL_SPAN_DATA != sp->pos)
			continue;
		spans = 1;
		/*
		 * Account for the data cells in the layout, matching it
		 * to data cells in the data section.
		 */
		for (dp = sp->first; dp; dp = dp->next) {
			/* Do not used spanned cells in the calculation. */
			if (0 < --spans)
				continue;
			spans = dp->spans;
			if (1 < spans)
				continue;
			icol = dp->layout->col;
			if (maxcol < icol)
				maxcol = icol;
			col = tbl->cols + icol;
			col->flags |= dp->layout->flags;
			if (dp->layout->flags & TBL_CELL_WIGN)
				continue;
			tblcalc_data(tbl, col, opts, dp);
		}
	}

	/*
	 * Count columns to equalize and columns to maximize.
	 * Find maximum width of the columns to equalize.
	 * Find total width of the columns *not* to maximize.
	 */

	necol = nxcol = 0;
	ewidth = xwidth = 0;
	for (icol = 0; icol <= maxcol; icol++) {
		col = tbl->cols + icol;
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
			if (nxcol && totalwidth)
				xwidth += ewidth - col->width;
			col->width = ewidth;
		}
	}

	/*
	 * If there are any columns to maximize, find the total
	 * available width, deducting 3n margins between columns.
	 * Distribute the available width evenly.
	 */

	if (nxcol && totalwidth) {
		xwidth = totalwidth - xwidth - 3*maxcol -
		    (opts->opts & (TBL_OPT_BOX | TBL_OPT_DBOX) ?
		     2 : !!opts->lvert + !!opts->rvert);

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

static void
tblcalc_data(struct rofftbl *tbl, struct roffcol *col,
		const struct tbl_opts *opts, const struct tbl_dat *dp)
{
	size_t		 sz;

	/* Branch down into data sub-types. */

	switch (dp->layout->pos) {
	case TBL_CELL_HORIZ:
	case TBL_CELL_DHORIZ:
		sz = (*tbl->len)(1, tbl->arg);
		if (col->width < sz)
			col->width = sz;
		break;
	case TBL_CELL_LONG:
	case TBL_CELL_CENTRE:
	case TBL_CELL_LEFT:
	case TBL_CELL_RIGHT:
		tblcalc_literal(tbl, col, dp);
		break;
	case TBL_CELL_NUMBER:
		tblcalc_number(tbl, col, opts, dp);
		break;
	case TBL_CELL_DOWN:
		break;
	default:
		abort();
	}
}

static void
tblcalc_literal(struct rofftbl *tbl, struct roffcol *col,
		const struct tbl_dat *dp)
{
	size_t		 sz;
	const char	*str;

	str = dp->string ? dp->string : "";
	sz = (*tbl->slen)(str, tbl->arg);

	if (col->width < sz)
		col->width = sz;
}

static void
tblcalc_number(struct rofftbl *tbl, struct roffcol *col,
		const struct tbl_opts *opts, const struct tbl_dat *dp)
{
	int		 i;
	size_t		 sz, psz, ssz, d;
	const char	*str;
	char		*cp;
	char		 buf[2];

	/*
	 * First calculate number width and decimal place (last + 1 for
	 * non-decimal numbers).  If the stored decimal is subsequent to
	 * ours, make our size longer by that difference
	 * (right-"shifting"); similarly, if ours is subsequent the
	 * stored, then extend the stored size by the difference.
	 * Finally, re-assign the stored values.
	 */

	str = dp->string ? dp->string : "";
	sz = (*tbl->slen)(str, tbl->arg);

	/* FIXME: TBL_DATA_HORIZ et al.? */

	buf[0] = opts->decimal;
	buf[1] = '\0';

	psz = (*tbl->slen)(buf, tbl->arg);

	if (NULL != (cp = strrchr(str, opts->decimal))) {
		buf[1] = '\0';
		for (ssz = 0, i = 0; cp != &str[i]; i++) {
			buf[0] = str[i];
			ssz += (*tbl->slen)(buf, tbl->arg);
		}
		d = ssz + psz;
	} else
		d = sz + psz;

	/* Adjust the settings for this column. */

	if (col->decimal > d) {
		sz += col->decimal - d;
		d = col->decimal;
	} else
		col->width += d - col->decimal;

	if (sz > col->width)
		col->width = sz;
	if (d > col->decimal)
		col->decimal = d;
}
