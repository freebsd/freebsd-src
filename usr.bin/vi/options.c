/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)options.c	8.36 (Berkeley) 12/29/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "pathnames.h"

static int	 	 opts_abbcmp __P((const void *, const void *));
static int	 	 opts_cmp __P((const void *, const void *));
static OPTLIST const	*opts_prefix __P((char *));
static int	 	 opts_print __P((SCR *, OPTLIST const *, OPTION *));

/*
 * O'Reilly noted options and abbreviations are from "Learning the VI Editor",
 * Fifth Edition, May 1992.  There's no way of knowing what systems they are
 * actually from.
 *
 * HPUX noted options and abbreviations are from "The Ultimate Guide to the
 * VI and EX Text Editors", 1990.
 */
static OPTLIST const optlist[] = {
/* O_ALTWERASE	  4.4BSD */
	{"altwerase",	f_altwerase,	OPT_0BOOL,	0},
/* O_AUTOINDENT	    4BSD */
	{"autoindent",	NULL,		OPT_0BOOL,	0},
/* O_AUTOPRINT	    4BSD */
	{"autoprint",	NULL,		OPT_1BOOL,	0},
/* O_AUTOWRITE	    4BSD */
	{"autowrite",	NULL,		OPT_0BOOL,	0},
/* O_BEAUTIFY	    4BSD */
	{"beautify",	NULL,		OPT_0BOOL,	0},
/* O_COLUMNS	  4.4BSD */
	{"columns",	f_columns,	OPT_NUM,	OPT_NOSAVE},
/* O_COMMENT	  4.4BSD */
	{"comment",	NULL,		OPT_0BOOL,	0},
/* O_DIGRAPH	  XXX: Elvis */
	{"digraph",	NULL,		OPT_0BOOL,	0},
/* O_DIRECTORY	    4BSD */
	{"directory",	NULL,		OPT_STR,	0},
/* O_EDCOMPATIBLE   4BSD */
	{"edcompatible",NULL,		OPT_0BOOL,	0},
/* O_ERRORBELLS	    4BSD */
	{"errorbells",	NULL,		OPT_0BOOL,	0},
/* O_EXRC	System V (undocumented) */
	{"exrc",	NULL,		OPT_0BOOL,	0},
/* O_EXTENDED	  4.4BSD */
	{"extended",	NULL,		OPT_0BOOL,	0},
/* O_FLASH	    HPUX */
	{"flash",	NULL,		OPT_1BOOL,	0},
/* O_HARDTABS	    4BSD */
	{"hardtabs",	NULL,		OPT_NUM,	0},
/* O_IGNORECASE	    4BSD */
	{"ignorecase",	NULL,		OPT_0BOOL,	0},
/* O_KEYTIME	  4.4BSD */
	{"keytime",	f_keytime,	OPT_NUM,	0},
/* O_LEFTRIGHT	  4.4BSD */
	{"leftright",	f_leftright,	OPT_0BOOL,	0},
/* O_LINES	  4.4BSD */
	{"lines",	f_lines,	OPT_NUM,	OPT_NOSAVE},
/* O_LISP	    4BSD */
	{"lisp",	f_lisp,		OPT_0BOOL,	0},
/* O_LIST	    4BSD */
	{"list",	f_list,		OPT_0BOOL,	0},
/* O_MAGIC	    4BSD */
	{"magic",	NULL,		OPT_1BOOL,	0},
/* O_MATCHTIME	  4.4BSD */
	{"matchtime",	f_matchtime,	OPT_NUM,	0},
/* O_MESG	    4BSD */
	{"mesg",	f_mesg,		OPT_1BOOL,	0},
/* O_MODELINE	    4BSD */
	{"modeline",	f_modeline,	OPT_0BOOL,	0},
/* O_NUMBER	    4BSD */
	{"number",	f_number,	OPT_0BOOL,	0},
/* O_OPEN	    4BSD */
	{"open",	NULL,		OPT_1BOOL,	0},
/* O_OPTIMIZE	    4BSD */
	{"optimize",	f_optimize,	OPT_1BOOL,	0},
/* O_PARAGRAPHS	    4BSD */
	{"paragraphs",	f_paragraph,	OPT_STR,	0},
/* O_PROMPT	    4BSD */
	{"prompt",	NULL,		OPT_1BOOL,	0},
/* O_READONLY	    4BSD (undocumented) */
	{"readonly",	f_readonly,	OPT_0BOOL,	0},
/* O_RECDIR	  4.4BSD */
	{"recdir",	NULL,		OPT_STR,	0},
/* O_REDRAW	    4BSD */
	{"redraw",	NULL,		OPT_0BOOL,	0},
/* O_REMAP	    4BSD */
	{"remap",	NULL,		OPT_1BOOL,	0},
/* O_REPORT	    4BSD */
	{"report",	NULL,		OPT_NUM,	OPT_NOSTR},
/* O_RULER	  4.4BSD */
	{"ruler",	f_ruler,	OPT_0BOOL,	0},
/* O_SCROLL	    4BSD */
	{"scroll",	NULL,		OPT_NUM,	0},
/* O_SECTIONS	    4BSD */
	{"sections",	f_section,	OPT_STR,	0},
/* O_SHELL	    4BSD */
	{"shell",	NULL,		OPT_STR,	0},
/* O_SHIFTWIDTH	    4BSD */
	{"shiftwidth",	f_shiftwidth,	OPT_NUM,	0},
/* O_SHOWDIRTY	  4.4BSD */
	{"showdirty",	NULL,		OPT_0BOOL,	0},
/* O_SHOWMATCH	    4BSD */
	{"showmatch",	NULL,		OPT_0BOOL,	0},
/* O_SHOWMODE	  4.4BSD */
	{"showmode",	NULL,		OPT_0BOOL,	0},
/* O_SIDESCROLL	  4.4BSD */
	{"sidescroll",	f_sidescroll,	OPT_NUM,	0},
/* O_SLOWOPEN	    4BSD  */
	{"slowopen",	NULL,		OPT_0BOOL,	0},
/* O_SOURCEANY	    4BSD (undocumented) */
	{"sourceany",	f_sourceany,	OPT_0BOOL,	0},
/* O_TABSTOP	    4BSD */
	{"tabstop",	f_tabstop,	OPT_NUM,	0},
/* O_TAGLENGTH	    4BSD */
	{"taglength",	NULL,		OPT_NUM,	OPT_NOSTR},
/* O_TAGS	    4BSD */
	{"tags",	f_tags,		OPT_STR,	0},
/* O_TERM	    4BSD */
	{"term",	f_term,		OPT_STR,	OPT_NOSAVE},
/* O_TERSE	    4BSD */
	{"terse",	NULL,		OPT_0BOOL,	0},
/* O_TIMEOUT	    4BSD (undocumented) */
	{"timeout",	NULL,		OPT_1BOOL,	0},
/* O_TTYWERASE	  4.4BSD */
	{"ttywerase",	f_ttywerase,	OPT_0BOOL,	0},
/* O_VERBOSE	  4.4BSD */
	{"verbose",	NULL,		OPT_0BOOL,	0},
/* O_W1200	    4BSD */
	{"w1200",	f_w1200,	OPT_NUM,	OPT_NEVER},
/* O_W300	    4BSD */
	{"w300",	f_w300,		OPT_NUM,	OPT_NEVER},
/* O_W9600	    4BSD */
	{"w9600",	f_w9600,	OPT_NUM,	OPT_NEVER},
/* O_WARN	    4BSD */
	{"warn",	NULL,		OPT_1BOOL,	0},
/* O_WINDOW	    4BSD */
	{"window",	f_window,	OPT_NUM,	0},
/* O_WRAPMARGIN	    4BSD */
	{"wrapmargin",	f_wrapmargin,	OPT_NUM,	OPT_NOSTR},
/* O_WRAPSCAN	    4BSD */
	{"wrapscan",	NULL,		OPT_1BOOL,	0},
/* O_WRITEANY	    4BSD */
	{"writeany",	NULL,		OPT_0BOOL,	0},
	{NULL},
};

typedef struct abbrev {
        char *name;
        int offset;
} OABBREV;

static OABBREV const abbrev[] = {
	{"ai",		O_AUTOINDENT},		/*     4BSD */
	{"ap",		O_AUTOPRINT},		/*     4BSD */
	{"aw",		O_AUTOWRITE},		/*     4BSD */
	{"bf",		O_BEAUTIFY},		/*     4BSD */
	{"co",		O_COLUMNS},		/*   4.4BSD */
	{"dir",		O_DIRECTORY},		/*     4BSD */
	{"eb",		O_ERRORBELLS},		/*     4BSD */
	{"ed",		O_EDCOMPATIBLE},	/*     4BSD (undocumented) */
	{"ex",		O_EXRC},		/* System V (undocumented) */
	{"ht",		O_HARDTABS},		/*     4BSD */
	{"ic",		O_IGNORECASE},		/*     4BSD */
	{"li",		O_LINES},		/*   4.4BSD */
	{"modelines",	O_MODELINE},		/*     HPUX */
	{"nu",		O_NUMBER},		/*     4BSD */
	{"opt",		O_OPTIMIZE},		/*     4BSD */
	{"para",	O_PARAGRAPHS},		/*     4BSD */
	{"re",		O_REDRAW},		/* O'Reilly */
	{"ro",		O_READONLY},		/*     4BSD (undocumented) */
	{"scr",		O_SCROLL},		/*     4BSD (undocumented) */
	{"sect",	O_SECTIONS},		/* O'Reilly */
	{"sh",		O_SHELL},		/*     4BSD */
	{"slow",	O_SLOWOPEN},		/*     4BSD */
	{"sm",		O_SHOWMATCH},		/*     4BSD */
	{"sw",		O_SHIFTWIDTH},		/*     4BSD */
	{"tag",		O_TAGS},		/*     4BSD (undocumented) */
	{"tl",		O_TAGLENGTH},		/*     4BSD */
	{"to",		O_TIMEOUT},		/*     4BSD (undocumented) */
	{"ts",		O_TABSTOP},		/*     4BSD */
	{"tty",		O_TERM},		/*     4BSD (undocumented) */
	{"ttytype",	O_TERM},		/*     4BSD (undocumented) */
	{"w",		O_WINDOW},		/* O'Reilly */
	{"wa",		O_WRITEANY},		/*     4BSD */
	{"wi",		O_WINDOW},		/*     4BSD (undocumented) */
	{"wm",		O_WRAPMARGIN},		/*     4BSD */
	{"ws",		O_WRAPSCAN},		/*     4BSD */
	{NULL},
};

/*
 * opts_init --
 *	Initialize some of the options.  Since the user isn't really
 *	"setting" these variables, don't set their OPT_SET bits.
 */
int
opts_init(sp)
	SCR *sp;
{
	ARGS *argv[2], a, b;
	OPTLIST const *op;
	u_long v;
	int cnt;
	char *s, b1[1024];

	a.bp = b1;
	a.len = 0;
	b.bp = NULL;
	b.len = 0;
	argv[0] = &a;
	argv[1] = &b;

#define	SET_DEF(opt, str) {						\
	if (str != b1)		/* GCC puts strings in text-space. */	\
		(void)strcpy(b1, str);					\
	a.len = strlen(b1);						\
	if (opts_set(sp, argv)) {					\
		msgq(sp, M_ERR,						\
		    "Unable to set default %s option", optlist[opt]);	\
		return (1);						\
	}								\
	F_CLR(&sp->opts[opt], OPT_SET);					\
}
	/* Set default values. */
	for (op = optlist, cnt = 0; op->name != NULL; ++op, ++cnt)
		if (op->type == OPT_0BOOL)
			O_CLR(sp, cnt);
		else if (op->type == OPT_1BOOL)
			O_SET(sp, cnt);
			
	/*
	 * !!!
	 * Vi historically stored temporary files in /var/tmp.  We store them
	 * in /tmp by default, hoping it's a memory based file system.  There
	 * are two ways to change this -- the user can set either the directory
	 * option or the TMPDIR environmental variable.
	 */
	(void)snprintf(b1, sizeof(b1), "directory=%s",
	    (s = getenv("TMPDIR")) == NULL ? _PATH_TMP : s);
	SET_DEF(O_DIRECTORY, b1);
	SET_DEF(O_KEYTIME, "keytime=6");
	SET_DEF(O_MATCHTIME, "matchtime=7");
	SET_DEF(O_REPORT, "report=5");
	SET_DEF(O_PARAGRAPHS, "paragraphs=IPLPPPQPP LIpplpipbp");
	(void)snprintf(b1, sizeof(b1), "recdir=%s", _PATH_PRESERVE);
	SET_DEF(O_RECDIR, b1);
	(void)snprintf(b1, sizeof(b1), "scroll=%ld", O_VAL(sp, O_LINES) / 2);
	SET_DEF(O_SCROLL, b1);
	SET_DEF(O_SECTIONS, "sections=NHSHH HUnhsh");
	(void)snprintf(b1, sizeof(b1),
	    "shell=%s", (s = getenv("SHELL")) == NULL ? _PATH_BSHELL : s);
	SET_DEF(O_SHELL, b1);
	SET_DEF(O_SHIFTWIDTH, "shiftwidth=8");
	SET_DEF(O_SIDESCROLL, "sidescroll=16");
	SET_DEF(O_TABSTOP, "tabstop=8");
	(void)snprintf(b1, sizeof(b1), "tags=%s", _PATH_TAGS);
	SET_DEF(O_TAGS, b1);
	(void)snprintf(b1, sizeof(b1),
	    "term=%s", (s = getenv("TERM")) == NULL ? "unknown" : s);
	SET_DEF(O_TERM, b1);

	/*
	 * The default window option values are:
	 *		8 if baud rate <=  600
	 *	       16 if baud rate <= 1200
	 *	LINES - 1 if baud rate  > 1200
	 */
	v = baud_from_bval(sp);
	if (v <= 600)
		v = 8;
	else if (v <= 1200)
		v = 16;
	else
		v = O_VAL(sp, O_LINES) - 1;
	(void)snprintf(b1, sizeof(b1), "window=%lu", v);
	SET_DEF(O_WINDOW, b1);

	SET_DEF(O_WRAPMARGIN, "wrapmargin=0");

	/*
	 * By default, the historic vi always displayed information
	 * about two options, redraw and term.  Term seems sufficient.
	 */
	F_SET(&sp->opts[O_TERM], OPT_SET);
	return (0);
}

/*
 * opts_set --
 *	Change the values of one or more options.
 */
int
opts_set(sp, argv)
	SCR *sp;
	ARGS *argv[];
{
	enum optdisp disp;
	OABBREV atmp, *ap;
	OPTLIST const *op;
	OPTLIST otmp;
	OPTION *spo;
	u_long value, turnoff;
	int ch, offset, rval;
	char *endp, *equals, *name, *p;
	
	disp = NO_DISPLAY;
	for (rval = 0; (*argv)->len != 0; ++argv) {
		/*
		 * The historic vi dumped the options for each occurrence of
		 * "all" in the set list.  Puhleeze.
		 */
		if (!strcmp(argv[0]->bp, "all")) {
			disp = ALL_DISPLAY;
			continue;
		}
			
		/* Find equals sign or end of set, skipping backquoted chars. */
		for (p = name = argv[0]->bp, equals = NULL; ch = *p; ++p)
			switch(ch) {
			case '=':
				equals = p;
				break;
			case '\\':
				/* Historic vi just used the backslash. */
				if (p[1] == '\0')
					break;
				++p;
				break;
			}

		turnoff = 0;
		op = NULL;
		if (equals)
			*equals++ = '\0';

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(OABBREV) - 1,
		    sizeof(OABBREV), opts_abbcmp)) != NULL) {
			op = optlist + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		if ((op = bsearch(&otmp, optlist,
		    sizeof(optlist) / sizeof(OPTLIST) - 1,
		    sizeof(OPTLIST), opts_cmp)) != NULL)
			goto found;

		/* Try the name without any leading "no". */
		if (name[0] == 'n' && name[1] == 'o') {
			turnoff = 1;
			name += 2;
		} else
			goto prefix;

		/* Check list of abbreviations. */
		atmp.name = name;
		if ((ap = bsearch(&atmp, abbrev,
		    sizeof(abbrev) / sizeof(OABBREV) - 1,
		    sizeof(OABBREV), opts_abbcmp)) != NULL) {
			op = optlist + ap->offset;
			goto found;
		}

		/* Check list of options. */
		otmp.name = name;
		if ((op = bsearch(&otmp, optlist,
		    sizeof(optlist) / sizeof(OPTLIST) - 1,
		    sizeof(OPTLIST), opts_cmp)) != NULL)
			goto found;

		/* Check for prefix match. */
prefix:		op = opts_prefix(name);

found:		if (op == NULL) {
			msgq(sp, M_ERR,
			    "no %s option: 'set all' gives all option values",
			    name);
			continue;
		}

		/* Find current option values. */
		offset = op - optlist;
		spo = sp->opts + offset;

		/* Set name, value. */
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (equals) {
				msgq(sp, M_ERR,
				    "set: [no]%s option doesn't take a value",
				    name);
				break;
			}
			if (op->func != NULL) {
				if (op->func(sp, spo, NULL, turnoff)) {
					rval = 1;
					break;
				}
			} else if (turnoff)
				O_CLR(sp, offset);
			else
				O_SET(sp, offset);
			goto change;
		case OPT_NUM:
			/*
			 * !!!
			 * Extension to historic vi.  If the OPT_NOSTR flag is
			 * set, a numeric option may be turned off by using a
			 * "no" prefix, e.g. "nowrapmargin".  (We assume that
			 * setting the value to 0 turns a numeric option off.)
			 */
			if (turnoff) {
				if (F_ISSET(op, OPT_NOSTR)) {
					value = 0;
					goto nostr;
				}
				msgq(sp, M_ERR,
				    "set: %s option isn't a boolean", name);
				break;
			}
			if (!equals) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}
			value = strtol(equals, &endp, 10);
			if (*endp && !isblank(*endp)) {
				msgq(sp, M_ERR,
				    "set %s: illegal number %s", name, equals);
				break;
			}
nostr:			if (op->func != NULL) {
				if (op->func(sp, spo, equals, value)) {
					rval = 1;
					break;
				}
			} else
				O_VAL(sp, offset) = value;
			goto change;
		case OPT_STR:
			if (turnoff) {
				msgq(sp, M_ERR,
				    "set: %s option isn't a boolean", name);
				break;
			}
			if (!equals) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}
			if (op->func != NULL) {
				if (op->func(sp, spo, equals, (u_long)0)) {
					rval = 1;
					break;
				}
			} else {
				if (F_ISSET(&sp->opts[offset], OPT_ALLOCATED))
					free(O_STR(sp, offset));
				if ((O_STR(sp, offset) =
				    strdup(equals)) == NULL) {
					msgq(sp, M_SYSERR, NULL);
					rval = 1;
					break;
				} else
					F_SET(&sp->opts[offset], OPT_ALLOCATED);
			}
change:			if (sp->s_optchange != NULL)
				(void)sp->s_optchange(sp, offset);
			F_SET(&sp->opts[offset], OPT_SET);
			break;
		default:
			abort();
		}
	}
	if (disp)
		opts_dump(sp, disp);
	return (rval);
}

/*
 * opts_dump --
 *	List the current values of selected options.
 */
void
opts_dump(sp, type)
	SCR *sp;
	enum optdisp type;
{
	OPTLIST const *op;
	int base, b_num, cnt, col, colwidth, curlen, s_num;
	int numcols, numrows, row;
	int b_op[O_OPTIONCOUNT], s_op[O_OPTIONCOUNT];
	char nbuf[20];

	/*
	 * Options are output in two groups -- those that fit in a column and
	 * those that don't.  Output is done on 6 character "tab" boundaries
	 * for no particular reason.  (Since we don't output tab characters,
	 * we can ignore the terminal's tab settings.)  Ignore the user's tab
	 * setting because we have no idea how reasonable it is.
	 */
#define	BOUND	6

	/* Find a column width we can live with. */
	for (cnt = 6; cnt > 1; --cnt) {
		colwidth = (sp->cols - 1) / cnt & ~(BOUND - 1);
		if (colwidth >= 10) {
			colwidth = (colwidth + BOUND) & ~(BOUND - 1);
			break;
		}
		colwidth = 0;
	}

	/* 
	 * Two passes.  First, get the set of options to list, entering them
	 * into the column list or the overflow list.  No error checking,
	 * since we know that at least one option (O_TERM) has the OPT_SET bit
	 * set.
	 */
	for (b_num = s_num = 0, op = optlist; op->name; ++op) {
		cnt = op - optlist;

		/* If OPT_NEVER set, it's never displayed. */
		if (F_ISSET(op, OPT_NEVER))
			continue;

		switch (type) {
		case ALL_DISPLAY:		/* Display all. */
			break;
		case CHANGED_DISPLAY:		/* Display changed. */
			if (!F_ISSET(&sp->opts[cnt], OPT_SET))
				continue;
			break;
		case SELECT_DISPLAY:		/* Display selected. */
			if (!F_ISSET(&sp->opts[cnt], OPT_SELECTED))
				continue;
			break;
		default:
		case NO_DISPLAY:
			abort();
			/* NOTREACHED */
		}
		F_CLR(&sp->opts[cnt], OPT_SELECTED);

		curlen = strlen(op->name);
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (!O_ISSET(sp, cnt))
				curlen += 2;
			break;
		case OPT_NUM:
			(void)snprintf(nbuf,
			    sizeof(nbuf), "%ld", O_VAL(sp, cnt));
			curlen += strlen(nbuf);
			break;
		case OPT_STR:
			curlen += strlen(O_STR(sp, cnt)) + 3;
			break;
		}
		/* Offset by two so there's a gap. */
		if (curlen < colwidth - 2)
			s_op[s_num++] = cnt;
		else
			b_op[b_num++] = cnt;
	}

	numcols = (sp->cols - 1) / colwidth;
	if (s_num > numcols) {
		numrows = s_num / numcols;
		if (s_num % numcols)
			++numrows;
	} else
		numrows = 1;

	for (row = 0; row < numrows;) {
		for (base = row, col = 0; col < numcols; ++col) {
			cnt = opts_print(sp,
			    &optlist[s_op[base]], &sp->opts[s_op[base]]);
			if ((base += numrows) >= s_num)
				break;
			(void)ex_printf(EXCOOKIE,
			    "%*s", (int)(colwidth - cnt), "");
		}
		if (++row < numrows || b_num)
			(void)ex_printf(EXCOOKIE, "\n");
	}

	for (row = 0; row < b_num;) {
		(void)opts_print(sp, &optlist[b_op[row]], &sp->opts[b_op[row]]);
		if (++row < b_num)
			(void)ex_printf(EXCOOKIE, "\n");
	}
	(void)ex_printf(EXCOOKIE, "\n");
}

/*
 * opts_print --
 *	Print out an option.
 */
static int
opts_print(sp, op, spo)
	SCR *sp;
	OPTLIST const *op;
	OPTION *spo;
{
	int curlen, offset;

	curlen = 0;
	offset = op - optlist;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		curlen += ex_printf(EXCOOKIE,
		    "%s%s", O_ISSET(sp, offset) ? "" : "no", op->name);
		break;
	case OPT_NUM:
		curlen += ex_printf(EXCOOKIE,
		     "%s=%ld", op->name, O_VAL(sp, offset));
		break;
	case OPT_STR:
		curlen += ex_printf(EXCOOKIE,
		    "%s=\"%s\"", op->name, O_STR(sp, offset));
		break;
	}
	return (curlen);
}

/*
 * opts_save --
 *	Write the current configuration to a file.
 */
int
opts_save(sp, fp)
	SCR *sp;
	FILE *fp;
{
	OPTION *spo;
	OPTLIST const *op;
	int ch, cnt;
	char *p;

	for (spo = sp->opts, op = optlist; op->name; ++op) {
		if (F_ISSET(op, OPT_NOSAVE))
			continue;
		cnt = op - optlist;
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			if (O_ISSET(sp, cnt))
				(void)fprintf(fp, "set %s\n", op->name);
			else
				(void)fprintf(fp, "set no%s\n", op->name);
			break;
		case OPT_NUM:
			(void)fprintf(fp,
			    "set %s=%-3d\n", op->name, O_VAL(sp, cnt));
			break;
		case OPT_STR:
			(void)fprintf(fp, "set ");
			for (p = op->name; (ch = *p) != '\0'; ++p) {
				if (isblank(ch))
					(void)putc('\\', fp);
				(void)putc(ch, fp);
			}
			(void)putc('=', fp);
			for (p = O_STR(sp, cnt); (ch = *p) != '\0'; ++p) {
				if (isblank(ch))
					(void)putc('\\', fp);
				(void)putc(ch, fp);
			}
			(void)putc('\n', fp);
			break;
		}
		if (ferror(fp)) {
			msgq(sp, M_ERR, "I/O error: %s", strerror(errno));
			return (1);
		}
	}
	return (0);
}

/*
 * opts_prefix --
 *	Check to see if the name is the prefix of one (and only one)
 *	option.  If so, return the option.
 */
static OPTLIST const *
opts_prefix(name)
	char *name;
{
	OPTLIST const *op, *save_op;
	size_t len;

	save_op = NULL;
	len = strlen(name);
	for (op = optlist; op->name != NULL; ++op) {
		if (op->name[0] < name[0])
			continue;
		if (op->name[0] > name[0])
			break;
		if (!memcmp(op->name, name, len)) {
			if (save_op != NULL)
				return (NULL);
			save_op = op;
		}
	}
	return (save_op);
}

static int
opts_abbcmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OABBREV *)a)->name, ((OABBREV *)b)->name));
}

static int
opts_cmp(a, b)
        const void *a, *b;
{
        return(strcmp(((OPTLIST *)a)->name, ((OPTLIST *)b)->name));
}

/*
 * opts_free --
 *	Free all option strings
 */
void
opts_free(sp)
	SCR *sp;
{
	int cnt;
	char *p;

	for (cnt = 0; cnt < O_OPTIONCOUNT; ++cnt)
		if (F_ISSET(&sp->opts[cnt], OPT_ALLOCATED)) {
			p = O_STR(sp, cnt);
			FREE(p, strlen(p) + 1);
		}
}

/*
 * opts_copy --
 *	Copy a screen's OPTION array.
 */
int
opts_copy(orig, sp)
	SCR *orig, *sp;
{
	OPTION *op;
	int cnt;

	/* Copy most everything without change. */
	memmove(sp->opts, orig->opts, sizeof(orig->opts));

	/*
	 * Allocate copies of the strings -- keep trying to reallocate
	 * after ENOMEM failure, otherwise end up with more than one
	 * screen referencing the original memory.
	 */
	for (op = sp->opts, cnt = 0; cnt < O_OPTIONCOUNT; ++cnt, ++op)
		if (F_ISSET(&sp->opts[cnt], OPT_ALLOCATED) &&
		    (O_STR(sp, cnt) = strdup(O_STR(sp, cnt))) == NULL) {
			msgq(orig, M_SYSERR, NULL);
			return (1);
		}
	return (0);
}
