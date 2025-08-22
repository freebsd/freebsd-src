/* $Id: tbl_layout.c,v 1.52 2025/07/16 14:33:08 schwarze Exp $ */
/*
 * Copyright (c) 2012, 2014, 2015, 2017, 2020, 2021, 2025
 *               Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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

struct	tbl_phrase {
	char		 name;
	enum tbl_cellt	 key;
};

static	const struct tbl_phrase keys[] = {
	{ 'c',		 TBL_CELL_CENTRE },
	{ 'r',		 TBL_CELL_RIGHT },
	{ 'l',		 TBL_CELL_LEFT },
	{ 'n',		 TBL_CELL_NUMBER },
	{ 's',		 TBL_CELL_SPAN },
	{ 'a',		 TBL_CELL_LONG },
	{ '^',		 TBL_CELL_DOWN },
	{ '-',		 TBL_CELL_HORIZ },
	{ '_',		 TBL_CELL_HORIZ },
	{ '=',		 TBL_CELL_DHORIZ }
};

#define KEYS_MAX ((int)(sizeof(keys)/sizeof(keys[0])))

static	void		 mods(struct tbl_node *, struct tbl_cell *,
				int, const char *, int *);
static	void		 cell(struct tbl_node *, struct tbl_row *,
				int, const char *, int *);
static	struct tbl_cell *cell_alloc(struct tbl_node *, struct tbl_row *,
				enum tbl_cellt);


static void
mods(struct tbl_node *tbl, struct tbl_cell *cp,
		int ln, const char *p, int *pos)
{
	char		*endptr;
	unsigned long	 spacing;  /* Column spacing in EN units. */
	int		 isz;      /* Width in basic units. */
	enum mandoc_esc	 fontesc;

mod:
	while (p[*pos] == ' ' || p[*pos] == '\t')
		(*pos)++;

	/* Row delimiters and cell specifiers end modifier lists. */

	if (strchr(".,-=^_ACLNRSaclnrs", p[*pos]) != NULL)
		return;

	/* Throw away parenthesised expression. */

	if ('(' == p[*pos]) {
		(*pos)++;
		while (p[*pos] && ')' != p[*pos])
			(*pos)++;
		if (')' == p[*pos]) {
			(*pos)++;
			goto mod;
		}
		mandoc_msg(MANDOCERR_TBLLAYOUT_PAR, ln, *pos, NULL);
		return;
	}

	/* Parse numerical spacing from modifier string. */

	if (isdigit((unsigned char)p[*pos])) {
		if ((spacing = strtoul(p + *pos, &endptr, 10)) > 9)
			mandoc_msg(MANDOCERR_TBLLAYOUT_SPC, ln, *pos,
			    "%lu", spacing);
		else
			cp->spacing = spacing;
		*pos = endptr - p;
		goto mod;
	}

	switch (tolower((unsigned char)p[(*pos)++])) {
	case 'b':
		cp->font = ESCAPE_FONTBOLD;
		goto mod;
	case 'd':
		cp->flags |= TBL_CELL_BALIGN;
		goto mod;
	case 'e':
		cp->flags |= TBL_CELL_EQUAL;
		goto mod;
	case 'f':
		break;
	case 'i':
		cp->font = ESCAPE_FONTITALIC;
		goto mod;
	case 'm':
		mandoc_msg(MANDOCERR_TBLLAYOUT_MOD, ln, *pos, "m");
		goto mod;
	case 'p':
	case 'v':
		if (p[*pos] == '-' || p[*pos] == '+')
			(*pos)++;
		while (isdigit((unsigned char)p[*pos]))
			(*pos)++;
		goto mod;
	case 't':
		cp->flags |= TBL_CELL_TALIGN;
		goto mod;
	case 'u':
		cp->flags |= TBL_CELL_UP;
		goto mod;
	case 'w':
		if (p[*pos] == '(') {
			(*pos)++;
			isz = 0;
			if (roff_evalnum(ln, p, pos, &isz, 'n', 1) == 0 ||
			    p[*pos] != ')')
				mandoc_msg(MANDOCERR_TBLLAYOUT_WIDTH,
				    ln, *pos, "%s", p + *pos);
			else {
				cp->width = isz;
				(*pos)++;
			}
		} else {
			cp->width = 0;
			while (isdigit((unsigned char)p[*pos])) {
				cp->width *= 10;
				cp->width += p[(*pos)++] - '0';
			}
			cp->width *= 24;
			if (cp->width == 0)
				mandoc_msg(MANDOCERR_TBLLAYOUT_WIDTH,
				    ln, *pos, "%s", p + *pos);
		}
		goto mod;
	case 'x':
		cp->flags |= TBL_CELL_WMAX;
		goto mod;
	case 'z':
		cp->flags |= TBL_CELL_WIGN;
		goto mod;
	case '|':
		if (cp->vert < 2)
			cp->vert++;
		else
			mandoc_msg(MANDOCERR_TBLLAYOUT_VERT,
			    ln, *pos - 1, NULL);
		goto mod;
	default:
		mandoc_msg(MANDOCERR_TBLLAYOUT_CHAR,
		    ln, *pos - 1, "%c", p[*pos - 1]);
		goto mod;
	}

	while (p[*pos] == ' ' || p[*pos] == '\t')
		(*pos)++;

	/* Ignore parenthised font names for now. */

	if (p[*pos] == '(')
		goto mod;

	isz = 0;
	if (p[*pos] != '\0')
		isz++;
	if (strchr(" \t.", p[*pos + isz]) == NULL)
		isz++;
	
	fontesc = mandoc_font(p + *pos, isz);

	switch (fontesc) {
	case ESCAPE_FONTPREV:
	case ESCAPE_ERROR:
		mandoc_msg(MANDOCERR_FT_BAD,
		    ln, *pos, "TS %s", p + *pos - 1);
		break;
	default:
		cp->font = fontesc;
		break;
	}
	*pos += isz;
	goto mod;
}

static void
cell(struct tbl_node *tbl, struct tbl_row *rp,
		int ln, const char *p, int *pos)
{
	int		 i;
	enum tbl_cellt	 c;

	/* Handle leading vertical lines */

	while (p[*pos] == ' ' || p[*pos] == '\t' || p[*pos] == '|') {
		if (p[*pos] == '|') {
			if (rp->vert < 2)
				rp->vert++;
			else
				mandoc_msg(MANDOCERR_TBLLAYOUT_VERT,
				    ln, *pos, NULL);
		}
		(*pos)++;
	}

again:
	while (p[*pos] == ' ' || p[*pos] == '\t')
		(*pos)++;

	if (p[*pos] == '.' || p[*pos] == '\0')
		return;

	/* Parse the column position (`c', `l', `r', ...). */

	for (i = 0; i < KEYS_MAX; i++)
		if (tolower((unsigned char)p[*pos]) == keys[i].name)
			break;

	if (i == KEYS_MAX) {
		mandoc_msg(MANDOCERR_TBLLAYOUT_CHAR,
		    ln, *pos, "%c", p[*pos]);
		(*pos)++;
		goto again;
	}
	c = keys[i].key;

	/* Special cases of spanners. */

	if (c == TBL_CELL_SPAN) {
		if (rp->last == NULL)
			mandoc_msg(MANDOCERR_TBLLAYOUT_SPAN, ln, *pos, NULL);
		else if (rp->last->pos == TBL_CELL_HORIZ ||
		    rp->last->pos == TBL_CELL_DHORIZ)
			c = rp->last->pos;
	} else if (c == TBL_CELL_DOWN && rp == tbl->first_row)
		mandoc_msg(MANDOCERR_TBLLAYOUT_DOWN, ln, *pos, NULL);

	(*pos)++;

	/* Allocate cell then parse its modifiers. */

	mods(tbl, cell_alloc(tbl, rp, c), ln, p, pos);
}

void
tbl_layout(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	struct tbl_row	*rp;

	rp = NULL;
	for (;;) {
		/* Skip whitespace before and after each cell. */

		while (p[pos] == ' ' || p[pos] == '\t')
			pos++;

		switch (p[pos]) {
		case ',':  /* Next row on this input line. */
			pos++;
			rp = NULL;
			continue;
		case '\0':  /* Next row on next input line. */
			return;
		case '.':  /* End of layout. */
			pos++;
			tbl->part = TBL_PART_DATA;

			/*
			 * When the layout is completely empty,
			 * default to one left-justified column.
			 */

			if (tbl->first_row == NULL) {
				tbl->first_row = tbl->last_row =
				    mandoc_calloc(1, sizeof(*rp));
			}
			if (tbl->first_row->first == NULL) {
				mandoc_msg(MANDOCERR_TBLLAYOUT_NONE,
				    ln, pos, NULL);
				cell_alloc(tbl, tbl->first_row,
				    TBL_CELL_LEFT);
				if (tbl->opts.lvert < tbl->first_row->vert)
					tbl->opts.lvert = tbl->first_row->vert;
				return;
			}

			/*
			 * Search for the widest line
			 * along the left and right margins.
			 */

			for (rp = tbl->first_row; rp; rp = rp->next) {
				if (tbl->opts.lvert < rp->vert)
					tbl->opts.lvert = rp->vert;
				if (rp->last != NULL &&
				    rp->last->col + 1 == tbl->opts.cols &&
				    tbl->opts.rvert < rp->last->vert)
					tbl->opts.rvert = rp->last->vert;

				/* If the last line is empty, drop it. */

				if (rp->next != NULL &&
				    rp->next->first == NULL) {
					free(rp->next);
					rp->next = NULL;
					tbl->last_row = rp;
				}
			}
			return;
		default:  /* Cell. */
			break;
		}

		/*
		 * If the last line had at least one cell,
		 * start a new one; otherwise, continue it.
		 */

		if (rp == NULL) {
			if (tbl->last_row == NULL ||
			    tbl->last_row->first != NULL) {
				rp = mandoc_calloc(1, sizeof(*rp));
				if (tbl->last_row)
					tbl->last_row->next = rp;
				else
					tbl->first_row = rp;
				tbl->last_row = rp;
			} else
				rp = tbl->last_row;
		}
		cell(tbl, rp, ln, p, &pos);
	}
}

static struct tbl_cell *
cell_alloc(struct tbl_node *tbl, struct tbl_row *rp, enum tbl_cellt pos)
{
	struct tbl_cell	*p, *pp;

	p = mandoc_calloc(1, sizeof(*p));
	p->spacing = SIZE_MAX;
	p->font = ESCAPE_FONTROMAN;
	p->pos = pos;

	if ((pp = rp->last) != NULL) {
		pp->next = p;
		p->col = pp->col + 1;
	} else
		rp->first = p;
	rp->last = p;

	if (tbl->opts.cols <= p->col)
		tbl->opts.cols = p->col + 1;

	return p;
}
