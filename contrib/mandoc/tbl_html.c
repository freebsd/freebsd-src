/*	$Id: tbl_html.c,v 1.38 2021/09/09 16:52:52 schwarze Exp $ */
/*
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014,2015,2017,2018,2021 Ingo Schwarze <schwarze@openbsd.org>
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
#include "roff.h"
#include "tbl.h"
#include "out.h"
#include "html.h"

static	void	 html_tblopen(struct html *, const struct tbl_span *);
static	size_t	 html_tbl_len(size_t, void *);
static	size_t	 html_tbl_strlen(const char *, void *);
static	size_t	 html_tbl_sulen(const struct roffsu *, void *);


static size_t
html_tbl_len(size_t sz, void *arg)
{
	return sz;
}

static size_t
html_tbl_strlen(const char *p, void *arg)
{
	return strlen(p);
}

static size_t
html_tbl_sulen(const struct roffsu *su, void *arg)
{
	if (su->scale < 0.0)
		return 0;

	switch (su->unit) {
	case SCALE_FS:  /* 2^16 basic units */
		return su->scale * 65536.0 / 24.0;
	case SCALE_IN:  /* 10 characters per inch */
		return su->scale * 10.0;
	case SCALE_CM:  /* 2.54 cm per inch */
		return su->scale * 10.0 / 2.54;
	case SCALE_PC:  /* 6 pica per inch */
	case SCALE_VS:
		return su->scale * 10.0 / 6.0;
	case SCALE_EN:
	case SCALE_EM:
		return su->scale;
	case SCALE_PT:  /* 12 points per pica */
		return su->scale * 10.0 / 6.0 / 12.0;
	case SCALE_BU:  /* 24 basic units per character */
		return su->scale / 24.0;
	case SCALE_MM:  /* 1/1000 inch */
		return su->scale / 100.0;
	default:
		abort();
	}
}

static void
html_tblopen(struct html *h, const struct tbl_span *sp)
{
	html_close_paragraph(h);
	if (h->tbl.cols == NULL) {
		h->tbl.len = html_tbl_len;
		h->tbl.slen = html_tbl_strlen;
		h->tbl.sulen = html_tbl_sulen;
		tblcalc(&h->tbl, sp, 0, 0);
	}
	assert(NULL == h->tblt);
	h->tblt = print_otag(h, TAG_TABLE, "c?ss", "tbl",
	    "border",
		sp->opts->opts & TBL_OPT_ALLBOX ? "1" : NULL,
	    "border-style",
		sp->opts->opts & TBL_OPT_DBOX ? "double" :
		sp->opts->opts & TBL_OPT_BOX ? "solid" : NULL,
	    "border-top-style",
		sp->pos == TBL_SPAN_DHORIZ ? "double" :
		sp->pos == TBL_SPAN_HORIZ ? "solid" : NULL);
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
	const struct tbl_dat	*dp;
	const struct tbl_cell	*cp;
	const struct tbl_span	*psp;
	const struct roffcol	*col;
	struct tag		*tt;
	const char		*hspans, *vspans, *halign, *valign;
	const char		*bborder, *lborder, *rborder;
	const char		*ccp;
	char			 hbuf[4], vbuf[4];
	size_t			 sz;
	enum mandoc_esc		 save_font;
	int			 i;

	if (h->tblt == NULL)
		html_tblopen(h, sp);

	/*
	 * Horizontal lines spanning the whole table
	 * are handled by previous or following table rows.
	 */

	if (sp->pos != TBL_SPAN_DATA)
		return;

	/* Inhibit printing of spaces: we do padding ourselves. */

	h->flags |= HTML_NONOSPACE;
	h->flags |= HTML_NOSPACE;

	/* Draw a vertical line left of this row? */

	switch (sp->layout->vert) {
	case 2:
		lborder = "double";
		break;
	case 1:
		lborder = "solid";
		break;
	default:
		lborder = NULL;
		break;
	}

	/* Draw a horizontal line below this row? */

	bborder = NULL;
	if ((psp = sp->next) != NULL) {
		switch (psp->pos) {
		case TBL_SPAN_DHORIZ:
			bborder = "double";
			break;
		case TBL_SPAN_HORIZ:
			bborder = "solid";
			break;
		default:
			break;
		}
	}

	tt = print_otag(h, TAG_TR, "ss",
	    "border-left-style", lborder,
	    "border-bottom-style", bborder);

	for (dp = sp->first; dp != NULL; dp = dp->next) {
		print_stagq(h, tt);

		/*
		 * Do not generate <td> elements for continuations
		 * of spanned cells.  Larger <td> elements covering
		 * this space were already generated earlier.
		 */

		cp = dp->layout;
		if (cp->pos == TBL_CELL_SPAN || cp->pos == TBL_CELL_DOWN ||
		    (dp->string != NULL && strcmp(dp->string, "\\^") == 0))
			continue;

		/* Determine the attribute values. */

		if (dp->hspans > 0) {
			(void)snprintf(hbuf, sizeof(hbuf),
			    "%d", dp->hspans + 1);
			hspans = hbuf;
		} else
			hspans = NULL;
		if (dp->vspans > 0) {
			(void)snprintf(vbuf, sizeof(vbuf),
			    "%d", dp->vspans + 1);
			vspans = vbuf;
		} else
			vspans = NULL;

		switch (cp->pos) {
		case TBL_CELL_CENTRE:
			halign = "center";
			break;
		case TBL_CELL_RIGHT:
		case TBL_CELL_NUMBER:
			halign = "right";
			break;
		default:
			halign = NULL;
			break;
		}
		if (cp->flags & TBL_CELL_TALIGN)
			valign = "top";
		else if (cp->flags & TBL_CELL_BALIGN)
			valign = "bottom";
		else
			valign = NULL;

		for (i = dp->hspans; i > 0; i--)
			cp = cp->next;
		switch (cp->vert) {
		case 2:
			rborder = "double";
			break;
		case 1:
			rborder = "solid";
			break;
		default:
			rborder = NULL;
			break;
		}

		/* Print the element and the attributes. */

		print_otag(h, TAG_TD, "??sss",
		    "colspan", hspans, "rowspan", vspans,
		    "vertical-align", valign,
		    "text-align", halign,
		    "border-right-style", rborder);
		if (dp->layout->pos == TBL_CELL_HORIZ ||
		    dp->layout->pos == TBL_CELL_DHORIZ ||
		    dp->pos == TBL_DATA_HORIZ ||
		    dp->pos == TBL_DATA_DHORIZ)
			print_otag(h, TAG_HR, "");
		else if (dp->string != NULL) {
			save_font = h->metac;
			html_setfont(h, dp->layout->font);
			if (dp->layout->pos == TBL_CELL_LONG)
				print_text(h, "\\[u2003]");  /* em space */
			print_text(h, dp->string);
			if (dp->layout->pos == TBL_CELL_NUMBER) {
				col = h->tbl.cols + dp->layout->col;
				if (col->decimal < col->nwidth) {
					if ((ccp = strrchr(dp->string,
					    sp->opts->decimal)) == NULL) {
						/* Punctuation space. */
						print_text(h, "\\[u2008]");
						ccp = strchr(dp->string, '\0');
					} else
						ccp++;
					sz = col->nwidth - col->decimal;
					while (--sz > 0) {
						if (*ccp == '\0')
							/* Figure space. */
							print_text(h,
							    "\\[u2007]");
						else
							ccp++;
					}
				}
			}
			html_setfont(h, save_font);
		}
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
