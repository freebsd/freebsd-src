/*	$Id: tbl_layout.c,v 1.23 2012/05/27 17:54:54 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2012 Ingo Schwarze <schwarze@openbsd.org>
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

struct	tbl_phrase {
	char		 name;
	enum tbl_cellt	 key;
};

/*
 * FIXME: we can make this parse a lot nicer by, when an error is
 * encountered in a layout key, bailing to the next key (i.e. to the
 * next whitespace then continuing).
 */

#define	KEYS_MAX	 11

static	const struct tbl_phrase keys[KEYS_MAX] = {
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

static	int		 mods(struct tbl_node *, struct tbl_cell *, 
				int, const char *, int *);
static	int		 cell(struct tbl_node *, struct tbl_row *, 
				int, const char *, int *);
static	void		 row(struct tbl_node *, int, const char *, int *);
static	struct tbl_cell *cell_alloc(struct tbl_node *, struct tbl_row *,
				enum tbl_cellt, int vert);

static int
mods(struct tbl_node *tbl, struct tbl_cell *cp, 
		int ln, const char *p, int *pos)
{
	char		 buf[5];
	int		 i;

	/* Not all types accept modifiers. */

	switch (cp->pos) {
	case (TBL_CELL_DOWN):
		/* FALLTHROUGH */
	case (TBL_CELL_HORIZ):
		/* FALLTHROUGH */
	case (TBL_CELL_DHORIZ):
		return(1);
	default:
		break;
	}

mod:
	/* 
	 * XXX: since, at least for now, modifiers are non-conflicting
	 * (are separable by value, regardless of position), we let
	 * modifiers come in any order.  The existing tbl doesn't let
	 * this happen.
	 */
	switch (p[*pos]) {
	case ('\0'):
		/* FALLTHROUGH */
	case (' '):
		/* FALLTHROUGH */
	case ('\t'):
		/* FALLTHROUGH */
	case (','):
		/* FALLTHROUGH */
	case ('.'):
		return(1);
	default:
		break;
	}

	/* Throw away parenthesised expression. */

	if ('(' == p[*pos]) {
		(*pos)++;
		while (p[*pos] && ')' != p[*pos])
			(*pos)++;
		if (')' == p[*pos]) {
			(*pos)++;
			goto mod;
		}
		mandoc_msg(MANDOCERR_TBLLAYOUT, 
				tbl->parse, ln, *pos, NULL);
		return(0);
	}

	/* Parse numerical spacing from modifier string. */

	if (isdigit((unsigned char)p[*pos])) {
		for (i = 0; i < 4; i++) {
			if ( ! isdigit((unsigned char)p[*pos + i]))
				break;
			buf[i] = p[*pos + i];
		}
		buf[i] = '\0';

		/* No greater than 4 digits. */

		if (4 == i) {
			mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse,
					ln, *pos, NULL);
			return(0);
		}

		*pos += i;
		cp->spacing = (size_t)atoi(buf);

		goto mod;
		/* NOTREACHED */
	} 

	/* TODO: GNU has many more extensions. */

	switch (tolower((unsigned char)p[(*pos)++])) {
	case ('z'):
		cp->flags |= TBL_CELL_WIGN;
		goto mod;
	case ('u'):
		cp->flags |= TBL_CELL_UP;
		goto mod;
	case ('e'):
		cp->flags |= TBL_CELL_EQUAL;
		goto mod;
	case ('t'):
		cp->flags |= TBL_CELL_TALIGN;
		goto mod;
	case ('d'):
		cp->flags |= TBL_CELL_BALIGN;
		goto mod;
	case ('w'):  /* XXX for now, ignore minimal column width */
		goto mod;
	case ('f'):
		break;
	case ('r'):
		/* FALLTHROUGH */
	case ('b'):
		/* FALLTHROUGH */
	case ('i'):
		(*pos)--;
		break;
	default:
		mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse,
				ln, *pos - 1, NULL);
		return(0);
	}

	switch (tolower((unsigned char)p[(*pos)++])) {
	case ('3'):
		/* FALLTHROUGH */
	case ('b'):
		cp->flags |= TBL_CELL_BOLD;
		goto mod;
	case ('2'):
		/* FALLTHROUGH */
	case ('i'):
		cp->flags |= TBL_CELL_ITALIC;
		goto mod;
	case ('1'):
		/* FALLTHROUGH */
	case ('r'):
		goto mod;
	default:
		break;
	}

	mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse,
			ln, *pos - 1, NULL);
	return(0);
}

static int
cell(struct tbl_node *tbl, struct tbl_row *rp, 
		int ln, const char *p, int *pos)
{
	int		 vert, i;
	enum tbl_cellt	 c;

	/* Handle vertical lines. */

	for (vert = 0; '|' == p[*pos]; ++*pos)
		vert++;
	while (' ' == p[*pos])
		(*pos)++;

	/* Parse the column position (`c', `l', `r', ...). */

	for (i = 0; i < KEYS_MAX; i++)
		if (tolower((unsigned char)p[*pos]) == keys[i].name)
			break;

	if (KEYS_MAX == i) {
		mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse, 
				ln, *pos, NULL);
		return(0);
	}

	c = keys[i].key;

	/*
	 * If a span cell is found first, raise a warning and abort the
	 * parse.  If a span cell is found and the last layout element
	 * isn't a "normal" layout, bail.
	 *
	 * FIXME: recover from this somehow?
	 */

	if (TBL_CELL_SPAN == c) {
		if (NULL == rp->first) {
			mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse,
					ln, *pos, NULL);
			return(0);
		} else if (rp->last)
			switch (rp->last->pos) {
			case (TBL_CELL_HORIZ):
			case (TBL_CELL_DHORIZ):
				mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse,
						ln, *pos, NULL);
				return(0);
			default:
				break;
			}
	}

	/*
	 * If a vertical spanner is found, we may not be in the first
	 * row.
	 */

	if (TBL_CELL_DOWN == c && rp == tbl->first_row) {
		mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse, ln, *pos, NULL);
		return(0);
	}

	(*pos)++;

	/* Disallow adjacent spacers. */

	if (vert > 2) {
		mandoc_msg(MANDOCERR_TBLLAYOUT, tbl->parse, ln, *pos - 1, NULL);
		return(0);
	}

	/* Allocate cell then parse its modifiers. */

	return(mods(tbl, cell_alloc(tbl, rp, c, vert), ln, p, pos));
}


static void
row(struct tbl_node *tbl, int ln, const char *p, int *pos)
{
	struct tbl_row	*rp;

row:	/*
	 * EBNF describing this section:
	 *
	 * row		::= row_list [:space:]* [.]?[\n]
	 * row_list	::= [:space:]* row_elem row_tail
	 * row_tail	::= [:space:]*[,] row_list |
	 *                  epsilon
	 * row_elem	::= [\t\ ]*[:alpha:]+
	 */

	rp = mandoc_calloc(1, sizeof(struct tbl_row));
	if (tbl->last_row)
		tbl->last_row->next = rp;
	else
		tbl->first_row = rp;
	tbl->last_row = rp;

cell:
	while (isspace((unsigned char)p[*pos]))
		(*pos)++;

	/* Safely exit layout context. */

	if ('.' == p[*pos]) {
		tbl->part = TBL_PART_DATA;
		if (NULL == tbl->first_row) 
			mandoc_msg(MANDOCERR_TBLNOLAYOUT, tbl->parse, 
					ln, *pos, NULL);
		(*pos)++;
		return;
	}

	/* End (and possibly restart) a row. */

	if (',' == p[*pos]) {
		(*pos)++;
		goto row;
	} else if ('\0' == p[*pos])
		return;

	if ( ! cell(tbl, rp, ln, p, pos))
		return;

	goto cell;
	/* NOTREACHED */
}

int
tbl_layout(struct tbl_node *tbl, int ln, const char *p)
{
	int		 pos;

	pos = 0;
	row(tbl, ln, p, &pos);

	/* Always succeed. */
	return(1);
}

static struct tbl_cell *
cell_alloc(struct tbl_node *tbl, struct tbl_row *rp, enum tbl_cellt pos,
		int vert)
{
	struct tbl_cell	*p, *pp;
	struct tbl_head	*h, *hp;

	p = mandoc_calloc(1, sizeof(struct tbl_cell));

	if (NULL != (pp = rp->last)) {
		pp->next = p;
		h = pp->head->next;
	} else {
		rp->first = p;
		h = tbl->first_head;
	}
	rp->last = p;

	p->pos = pos;
	p->vert = vert;

	/* Re-use header. */

	if (h) {
		p->head = h;
		return(p);
	}

	hp = mandoc_calloc(1, sizeof(struct tbl_head));
	hp->ident = tbl->opts.cols++;
	hp->vert = vert;

	if (tbl->last_head) {
		hp->prev = tbl->last_head;
		tbl->last_head->next = hp;
	} else
		tbl->first_head = hp;
	tbl->last_head = hp;

	p->head = hp;
	return(p);
}
