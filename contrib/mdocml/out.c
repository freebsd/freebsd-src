/*	$Id: out.c,v 1.43 2011/09/20 23:05:49 schwarze Exp $ */
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

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "out.h"

static	void	tblcalc_data(struct rofftbl *, struct roffcol *,
			const struct tbl *, const struct tbl_dat *);
static	void	tblcalc_literal(struct rofftbl *, struct roffcol *,
			const struct tbl_dat *);
static	void	tblcalc_number(struct rofftbl *, struct roffcol *,
			const struct tbl *, const struct tbl_dat *);

/* 
 * Convert a `scaling unit' to a consistent form, or fail.  Scaling
 * units are documented in groff.7, mdoc.7, man.7.
 */
int
a2roffsu(const char *src, struct roffsu *dst, enum roffscale def)
{
	char		 buf[BUFSIZ], hasd;
	int		 i;
	enum roffscale	 unit;

	if ('\0' == *src)
		return(0);

	i = hasd = 0;

	switch (*src) {
	case ('+'):
		src++;
		break;
	case ('-'):
		buf[i++] = *src++;
		break;
	default:
		break;
	}

	if ('\0' == *src)
		return(0);

	while (i < BUFSIZ) {
		if ( ! isdigit((unsigned char)*src)) {
			if ('.' != *src)
				break;
			else if (hasd)
				break;
			else
				hasd = 1;
		}
		buf[i++] = *src++;
	}

	if (BUFSIZ == i || (*src && *(src + 1)))
		return(0);

	buf[i] = '\0';

	switch (*src) {
	case ('c'):
		unit = SCALE_CM;
		break;
	case ('i'):
		unit = SCALE_IN;
		break;
	case ('P'):
		unit = SCALE_PC;
		break;
	case ('p'):
		unit = SCALE_PT;
		break;
	case ('f'):
		unit = SCALE_FS;
		break;
	case ('v'):
		unit = SCALE_VS;
		break;
	case ('m'):
		unit = SCALE_EM;
		break;
	case ('\0'):
		if (SCALE_MAX == def)
			return(0);
		unit = SCALE_BU;
		break;
	case ('u'):
		unit = SCALE_BU;
		break;
	case ('M'):
		unit = SCALE_MM;
		break;
	case ('n'):
		unit = SCALE_EN;
		break;
	default:
		return(0);
	}

	/* FIXME: do this in the caller. */
	if ((dst->scale = atof(buf)) < 0)
		dst->scale = 0;
	dst->unit = unit;
	return(1);
}

/*
 * Calculate the abstract widths and decimal positions of columns in a
 * table.  This routine allocates the columns structures then runs over
 * all rows and cells in the table.  The function pointers in "tbl" are
 * used for the actual width calculations.
 */
void
tblcalc(struct rofftbl *tbl, const struct tbl_span *sp)
{
	const struct tbl_dat	*dp;
	const struct tbl_head	*hp;
	struct roffcol		*col;
	int			 spans;

	/*
	 * Allocate the master column specifiers.  These will hold the
	 * widths and decimal positions for all cells in the column.  It
	 * must be freed and nullified by the caller.
	 */

	assert(NULL == tbl->cols);
	tbl->cols = mandoc_calloc
		((size_t)sp->tbl->cols, sizeof(struct roffcol));

	hp = sp->head;

	for ( ; sp; sp = sp->next) {
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
			assert(dp->layout);
			col = &tbl->cols[dp->layout->head->ident];
			tblcalc_data(tbl, col, sp->tbl, dp);
		}
	}

	/* 
	 * Calculate width of the spanners.  These get one space for a
	 * vertical line, two for a double-vertical line. 
	 */

	for ( ; hp; hp = hp->next) {
		col = &tbl->cols[hp->ident];
		switch (hp->pos) {
		case (TBL_HEAD_VERT):
			col->width = (*tbl->len)(1, tbl->arg);
			break;
		case (TBL_HEAD_DVERT):
			col->width = (*tbl->len)(2, tbl->arg);
			break;
		default:
			break;
		}
	}
}

static void
tblcalc_data(struct rofftbl *tbl, struct roffcol *col,
		const struct tbl *tp, const struct tbl_dat *dp)
{
	size_t		 sz;

	/* Branch down into data sub-types. */

	switch (dp->layout->pos) {
	case (TBL_CELL_HORIZ):
		/* FALLTHROUGH */
	case (TBL_CELL_DHORIZ):
		sz = (*tbl->len)(1, tbl->arg);
		if (col->width < sz)
			col->width = sz;
		break;
	case (TBL_CELL_LONG):
		/* FALLTHROUGH */
	case (TBL_CELL_CENTRE):
		/* FALLTHROUGH */
	case (TBL_CELL_LEFT):
		/* FALLTHROUGH */
	case (TBL_CELL_RIGHT):
		tblcalc_literal(tbl, col, dp);
		break;
	case (TBL_CELL_NUMBER):
		tblcalc_number(tbl, col, tp, dp);
		break;
	case (TBL_CELL_DOWN):
		break;
	default:
		abort();
		/* NOTREACHED */
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
		const struct tbl *tp, const struct tbl_dat *dp)
{
	int 		 i;
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

	buf[0] = tp->decimal;
	buf[1] = '\0';

	psz = (*tbl->slen)(buf, tbl->arg);

	if (NULL != (cp = strrchr(str, tp->decimal))) {
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
