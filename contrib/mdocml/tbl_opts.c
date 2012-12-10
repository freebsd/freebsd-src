/*	$Id: tbl_opts.c,v 1.12 2011/09/18 14:14:15 schwarze Exp $ */
/*
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "libmandoc.h"
#include "libroff.h"

enum	tbl_ident {
	KEY_CENTRE = 0,
	KEY_DELIM,
	KEY_EXPAND,
	KEY_BOX,
	KEY_DBOX,
	KEY_ALLBOX,
	KEY_TAB,
	KEY_LINESIZE,
	KEY_NOKEEP,
	KEY_DPOINT,
	KEY_NOSPACE,
	KEY_FRAME,
	KEY_DFRAME,
	KEY_MAX
};

struct	tbl_phrase {
	const char	*name;
	int		 key;
	enum tbl_ident	 ident;
};

/* Handle Commonwealth/American spellings. */
#define	KEY_MAXKEYS	 14

/* Maximum length of key name string. */
#define	KEY_MAXNAME	 13

/* Maximum length of key number size. */
#define	KEY_MAXNUMSZ	 10

static	const struct tbl_phrase keys[KEY_MAXKEYS] = {
	{ "center",	 TBL_OPT_CENTRE,	KEY_CENTRE},
	{ "centre",	 TBL_OPT_CENTRE,	KEY_CENTRE},
	{ "delim",	 0,	       		KEY_DELIM},
	{ "expand",	 TBL_OPT_EXPAND,	KEY_EXPAND},
	{ "box",	 TBL_OPT_BOX,   	KEY_BOX},
	{ "doublebox",	 TBL_OPT_DBOX,  	KEY_DBOX},
	{ "allbox",	 TBL_OPT_ALLBOX,	KEY_ALLBOX},
	{ "frame",	 TBL_OPT_BOX,		KEY_FRAME},
	{ "doubleframe", TBL_OPT_DBOX,		KEY_DFRAME},
	{ "tab",	 0,			KEY_TAB},
	{ "linesize",	 0,			KEY_LINESIZE},
	{ "nokeep",	 TBL_OPT_NOKEEP,	KEY_NOKEEP},
	{ "decimalpoint", 0,			KEY_DPOINT},
	{ "nospaces",	 TBL_OPT_NOSPACE,	KEY_NOSPACE},
};

static	int		 arg(struct tbl_node *, int, 
				const char *, int *, enum tbl_ident);
static	void		 opt(struct tbl_node *, int, 
				const char *, int *);

static int
arg(struct tbl_node *tbl, int ln, const char *p, int *pos, enum tbl_ident key)
{
	int		 i;
	char		 buf[KEY_MAXNUMSZ];

	while (isspace((unsigned char)p[*pos]))
		(*pos)++;

	/* Arguments always begin with a parenthesis. */

	if ('(' != p[*pos]) {
		mandoc_msg(MANDOCERR_TBL, tbl->parse, 
				ln, *pos, NULL);
		return(0);
	}

	(*pos)++;

	/*
	 * The arguments can be ANY value, so we can't just stop at the
	 * next close parenthesis (the argument can be a closed
	 * parenthesis itself).
	 */

	switch (key) {
	case (KEY_DELIM):
		if ('\0' == p[(*pos)++]) {
			mandoc_msg(MANDOCERR_TBL, tbl->parse,
					ln, *pos - 1, NULL);
			return(0);
		} 

		if ('\0' == p[(*pos)++]) {
			mandoc_msg(MANDOCERR_TBL, tbl->parse,
					ln, *pos - 1, NULL);
			return(0);
		} 
		break;
	case (KEY_TAB):
		if ('\0' != (tbl->opts.tab = p[(*pos)++]))
			break;

		mandoc_msg(MANDOCERR_TBL, tbl->parse,
				ln, *pos - 1, NULL);
		return(0);
	case (KEY_LINESIZE):
		for (i = 0; i < KEY_MAXNUMSZ && p[*pos]; i++, (*pos)++) {
			buf[i] = p[*pos];
			if ( ! isdigit((unsigned char)buf[i]))
				break;
		}

		if (i < KEY_MAXNUMSZ) {
			buf[i] = '\0';
			tbl->opts.linesize = atoi(buf);
			break;
		}

		mandoc_msg(MANDOCERR_TBL, tbl->parse, ln, *pos, NULL);
		return(0);
	case (KEY_DPOINT):
		if ('\0' != (tbl->opts.decimal = p[(*pos)++]))
			break;

		mandoc_msg(MANDOCERR_TBL, tbl->parse, 
				ln, *pos - 1, NULL);
		return(0);
	default:
		abort();
		/* NOTREACHED */
	}

	/* End with a close parenthesis. */

	if (')' == p[(*pos)++])
		return(1);

	mandoc_msg(MANDOCERR_TBL, tbl->parse, ln, *pos - 1, NULL);
	return(0);
}

static void
opt(struct tbl_node *tbl, int ln, const char *p, int *pos)
{
	int		 i, sv;
	char		 buf[KEY_MAXNAME];

	/*
	 * Parse individual options from the stream as surrounded by
	 * this goto.  Each pass through the routine parses out a single
	 * option and registers it.  Option arguments are processed in
	 * the arg() function.
	 */

again:	/*
	 * EBNF describing this section:
	 *
	 * options	::= option_list [:space:]* [;][\n]
	 * option_list	::= option option_tail
	 * option_tail	::= [:space:]+ option_list |
	 * 		::= epsilon
	 * option	::= [:alpha:]+ args
	 * args		::= [:space:]* [(] [:alpha:]+ [)]
	 */

	while (isspace((unsigned char)p[*pos]))
		(*pos)++;

	/* Safe exit point. */

	if (';' == p[*pos])
		return;

	/* Copy up to first non-alpha character. */

	for (sv = *pos, i = 0; i < KEY_MAXNAME; i++, (*pos)++) {
		buf[i] = (char)tolower((unsigned char)p[*pos]);
		if ( ! isalpha((unsigned char)buf[i]))
			break;
	}

	/* Exit if buffer is empty (or overrun). */

	if (KEY_MAXNAME == i || 0 == i) {
		mandoc_msg(MANDOCERR_TBL, tbl->parse, ln, *pos, NULL);
		return;
	}

	buf[i] = '\0';

	while (isspace((unsigned char)p[*pos]))
		(*pos)++;

	/* 
	 * Look through all of the available keys to find one that
	 * matches the input.  FIXME: hashtable this.
	 */

	for (i = 0; i < KEY_MAXKEYS; i++) {
		if (strcmp(buf, keys[i].name))
			continue;

		/*
		 * Note: this is more difficult to recover from, as we
		 * can be anywhere in the option sequence and it's
		 * harder to jump to the next.  Meanwhile, just bail out
		 * of the sequence altogether.
		 */

		if (keys[i].key) 
			tbl->opts.opts |= keys[i].key;
		else if ( ! arg(tbl, ln, p, pos, keys[i].ident))
			return;

		break;
	}

	/* 
	 * Allow us to recover from bad options by continuing to another
	 * parse sequence.
	 */

	if (KEY_MAXKEYS == i)
		mandoc_msg(MANDOCERR_TBLOPT, tbl->parse, ln, sv, NULL);

	goto again;
	/* NOTREACHED */
}

int
tbl_option(struct tbl_node *tbl, int ln, const char *p)
{
	int		 pos;

	/*
	 * Table options are always on just one line, so automatically
	 * switch into the next input mode here.
	 */
	tbl->part = TBL_PART_LAYOUT;

	pos = 0;
	opt(tbl, ln, p, &pos);

	/* Always succeed. */
	return(1);
}
