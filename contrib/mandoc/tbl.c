/*	$Id: tbl.c,v 1.47 2025/01/05 18:14:39 schwarze Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc_aux.h"
#include "mandoc.h"
#include "tbl.h"
#include "libmandoc.h"
#include "tbl_parse.h"
#include "tbl_int.h"


void
tbl_read(struct tbl_node *tbl, int ln, const char *p, int pos)
{
	const char	*cp;
	int		 active;

	/*
	 * In the options section, proceed to the layout section
	 * after a semicolon, or right away if there is no semicolon.
	 * Ignore semicolons in arguments.
	 */

	if (tbl->part == TBL_PART_OPTS) {
		tbl->part = TBL_PART_LAYOUT;
		active = 1;
		for (cp = p + pos; *cp != '\0'; cp++) {
			switch (*cp) {
			case '(':
				active = 0;
				continue;
			case ')':
				active = 1;
				continue;
			case ';':
				if (active)
					break;
				continue;
			default:
				continue;
			}
			break;
		}
		if (*cp == ';') {
			tbl_option(tbl, ln, p, &pos);
			if (p[pos] == '\0')
				return;
		}
	}

	/* Process the other section types.  */

	switch (tbl->part) {
	case TBL_PART_LAYOUT:
		tbl_layout(tbl, ln, p, pos);
		break;
	case TBL_PART_CDATA:
		tbl_cdata(tbl, ln, p, pos);
		break;
	default:
		tbl_data(tbl, ln, p, pos);
		break;
	}
}

struct tbl_node *
tbl_alloc(int pos, int line, struct tbl_node *last_tbl)
{
	struct tbl_node	*tbl;

	tbl = mandoc_calloc(1, sizeof(*tbl));
	if (last_tbl != NULL)
		last_tbl->next = tbl;
	tbl->line = line;
	tbl->pos = pos;
	tbl->part = TBL_PART_OPTS;
	tbl->opts.tab = '\t';
	tbl->opts.decimal = '.';
	return tbl;
}

void
tbl_free(struct tbl_node *tbl)
{
	struct tbl_node	*old_tbl;
	struct tbl_row	*rp;
	struct tbl_cell	*cp;
	struct tbl_span	*sp;
	struct tbl_dat	*dp;

	while (tbl != NULL) {
		while ((rp = tbl->first_row) != NULL) {
			tbl->first_row = rp->next;
			while (rp->first != NULL) {
				cp = rp->first;
				rp->first = cp->next;
				free(cp);
			}
			free(rp);
		}
		while ((sp = tbl->first_span) != NULL) {
			tbl->first_span = sp->next;
			while (sp->first != NULL) {
				dp = sp->first;
				sp->first = dp->next;
				free(dp->string);
				free(dp);
			}
			free(sp);
		}
		old_tbl = tbl;
		tbl = tbl->next;
		free(old_tbl);
	}
}

void
tbl_restart(int line, int pos, struct tbl_node *tbl)
{
	if (tbl->part == TBL_PART_CDATA)
		mandoc_msg(MANDOCERR_TBLDATA_BLK, line, pos, "T&");

	tbl->part = TBL_PART_LAYOUT;
	tbl->line = line;
	tbl->pos = pos;
}

struct tbl_span *
tbl_span(struct tbl_node *tbl)
{
	struct tbl_span	 *span;

	span = tbl->current_span ? tbl->current_span->next
				 : tbl->first_span;
	if (span != NULL)
		tbl->current_span = span;
	return span;
}

int
tbl_end(struct tbl_node *tbl, int still_open)
{
	struct tbl_span *sp;

	if (still_open)
		mandoc_msg(MANDOCERR_BLK_NOEND, tbl->line, tbl->pos, "TS");
	else if (tbl->part == TBL_PART_CDATA)
		mandoc_msg(MANDOCERR_TBLDATA_BLK, tbl->line, tbl->pos, "TE");

	sp = tbl->first_span;
	while (sp != NULL && sp->first == NULL)
		sp = sp->next;
	if (sp == NULL) {
		mandoc_msg(MANDOCERR_TBLDATA_NONE, tbl->line, tbl->pos, NULL);
		return 0;
	}
	return 1;
}
