/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1991, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)options.c	10.51 (Berkeley) 10/14/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "../vi/vi.h"
#include "pathnames.h"

static int	 	 opts_abbcmp __P((const void *, const void *));
static int	 	 opts_cmp __P((const void *, const void *));
static int	 	 opts_print __P((SCR *, OPTLIST const *));

/*
 * O'Reilly noted options and abbreviations are from "Learning the VI Editor",
 * Fifth Edition, May 1992.  There's no way of knowing what systems they are
 * actually from.
 *
 * HPUX noted options and abbreviations are from "The Ultimate Guide to the
 * VI and EX Text Editors", 1990.
 */
OPTLIST const optlist[] = {
/* O_ALTWERASE	  4.4BSD */
	{"altwerase",	f_altwerase,	OPT_0BOOL,	0},
/* O_AUTOINDENT	    4BSD */
	{"autoindent",	NULL,		OPT_0BOOL,	0},
/* O_AUTOPRINT	    4BSD */
	{"autoprint",	NULL,		OPT_1BOOL,	0},
/* O_AUTOWRITE	    4BSD */
	{"autowrite",	NULL,		OPT_0BOOL,	0},
/* O_BACKUP	  4.4BSD */
	{"backup",	NULL,		OPT_STR,	0},
/* O_BEAUTIFY	    4BSD */
	{"beautify",	NULL,		OPT_0BOOL,	0},
/* O_CDPATH	  4.4BSD */
	{"cdpath",	NULL,		OPT_STR,	0},
/* O_CEDIT	  4.4BSD */
	{"cedit",	NULL,		OPT_STR,	0},
/* O_COLUMNS	  4.4BSD */
	{"columns",	f_columns,	OPT_NUM,	OPT_NOSAVE},
/* O_COMMENT	  4.4BSD */
	{"comment",	NULL,		OPT_0BOOL,	0},
/* O_DIRECTORY	    4BSD */
	{"directory",	NULL,		OPT_STR,	0},
/* O_EDCOMPATIBLE   4BSD */
	{"edcompatible",NULL,		OPT_0BOOL,	0},
/* O_ESCAPETIME	  4.4BSD */
	{"escapetime",	NULL,		OPT_NUM,	0},
/* O_ERRORBELLS	    4BSD */
	{"errorbells",	NULL,		OPT_0BOOL,	0},
/* O_EXRC	System V (undocumented) */
	{"exrc",	NULL,		OPT_0BOOL,	0},
/* O_EXTENDED	  4.4BSD */
	{"extended",	f_recompile,	OPT_0BOOL,	0},
/* O_FILEC	  4.4BSD */
	{"filec",	NULL,		OPT_STR,	0},
/* O_FLASH	    HPUX */
	{"flash",	NULL,		OPT_1BOOL,	0},
/* O_HARDTABS	    4BSD */
	{"hardtabs",	NULL,		OPT_NUM,	0},
/* O_ICLOWER	  4.4BSD */
	{"iclower",	f_recompile,	OPT_0BOOL,	0},
/* O_IGNORECASE	    4BSD */
	{"ignorecase",	f_recompile,	OPT_0BOOL,	0},
/* O_KEYTIME	  4.4BSD */
	{"keytime",	NULL,		OPT_NUM,	0},
/* O_LEFTRIGHT	  4.4BSD */
	{"leftright",	f_reformat,	OPT_0BOOL,	0},
/* O_LINES	  4.4BSD */
	{"lines",	f_lines,	OPT_NUM,	OPT_NOSAVE},
/* O_LISP	    4BSD
 *	XXX
 *	When the lisp option is implemented, delete the OPT_NOSAVE flag,
 *	so that :mkexrc dumps it.
 */
	{"lisp",	f_lisp,		OPT_0BOOL,	OPT_NOSAVE},
/* O_LIST	    4BSD */
	{"list",	f_reformat,	OPT_0BOOL,	0},
/* O_LOCKFILES	  4.4BSD
 *	XXX
 *	Locking isn't reliable enough over NFS to require it, in addition,
 *	it's a serious startup performance problem over some remote links.
 */
	{"lock",	NULL,		OPT_1BOOL,	0},
/* O_MAGIC	    4BSD */
	{"magic",	NULL,		OPT_1BOOL,	0},
/* O_MATCHTIME	  4.4BSD */
	{"matchtime",	NULL,		OPT_NUM,	0},
/* O_MESG	    4BSD */
	{"mesg",	NULL,		OPT_1BOOL,	0},
/* O_MODELINE	    4BSD
 *	!!!
 *	This has been documented in historical systems as both "modeline"
 *	and as "modelines".  Regardless of the name, this option represents
 *	a security problem of mammoth proportions, not to mention a stunning
 *	example of what your intro CS professor referred to as the perils of
 *	mixing code and data.  Don't add it, or I will kill you.
 */
	{"modeline",	NULL,		OPT_0BOOL,	OPT_NOSET},
/* O_MSGCAT	  4.4BSD */
	{"msgcat",	f_msgcat,	OPT_STR,	0},
/* O_NOPRINT	  4.4BSD */
	{"noprint",	f_print,	OPT_STR,	0},
/* O_NUMBER	    4BSD */
	{"number",	f_reformat,	OPT_0BOOL,	0},
/* O_OCTAL	  4.4BSD */
	{"octal",	f_print,	OPT_0BOOL,	0},
/* O_OPEN	    4BSD */
	{"open",	NULL,		OPT_1BOOL,	0},
/* O_OPTIMIZE	    4BSD */
	{"optimize",	NULL,		OPT_1BOOL,	0},
/* O_PARAGRAPHS	    4BSD */
	{"paragraphs",	f_paragraph,	OPT_STR,	0},
/* O_PATH	  4.4BSD */
	{"path",	NULL,		OPT_STR,	0},
/* O_PRINT	  4.4BSD */
	{"print",	f_print,	OPT_STR,	0},
/* O_PROMPT	    4BSD */
	{"prompt",	NULL,		OPT_1BOOL,	0},
/* O_READONLY	    4BSD (undocumented) */
	{"readonly",	f_readonly,	OPT_0BOOL,	OPT_ALWAYS},
/* O_RECDIR	  4.4BSD */
	{"recdir",	NULL,		OPT_STR,	0},
/* O_REDRAW	    4BSD */
	{"redraw",	NULL,		OPT_0BOOL,	0},
/* O_REMAP	    4BSD */
	{"remap",	NULL,		OPT_1BOOL,	0},
/* O_REPORT	    4BSD */
	{"report",	NULL,		OPT_NUM,	0},
/* O_RULER	  4.4BSD */
	{"ruler",	NULL,		OPT_0BOOL,	0},
/* O_SCROLL	    4BSD */
	{"scroll",	NULL,		OPT_NUM,	0},
/* O_SEARCHINCR	  4.4BSD */
	{"searchincr",	NULL,		OPT_0BOOL,	0},
/* O_SECTIONS	    4BSD */
	{"sections",	f_section,	OPT_STR,	0},
/* O_SECURE	  4.4BSD */
	{"secure",	NULL,		OPT_0BOOL,	OPT_NOUNSET},
/* O_SHELL	    4BSD */
	{"shell",	NULL,		OPT_STR,	0},
/* O_SHELLMETA	  4.4BSD */
	{"shellmeta",	NULL,		OPT_STR,	0},
/* O_SHIFTWIDTH	    4BSD */
	{"shiftwidth",	NULL,		OPT_NUM,	OPT_NOZERO},
/* O_SHOWMATCH	    4BSD */
	{"showmatch",	NULL,		OPT_0BOOL,	0},
/* O_SHOWMODE	  4.4BSD */
	{"showmode",	NULL,		OPT_0BOOL,	0},
/* O_SIDESCROLL	  4.4BSD */
	{"sidescroll",	NULL,		OPT_NUM,	OPT_NOZERO},
/* O_SLOWOPEN	    4BSD  */
	{"slowopen",	NULL,		OPT_0BOOL,	0},
/* O_SOURCEANY	    4BSD (undocumented)
 *	!!!
 *	Historic vi, on startup, source'd $HOME/.exrc and ./.exrc, if they
 *	were owned by the user.  The sourceany option was an undocumented
 *	feature of historic vi which permitted the startup source'ing of
 *	.exrc files the user didn't own.  This is an obvious security problem,
 *	and we ignore the option.
 */
	{"sourceany",	NULL,		OPT_0BOOL,	OPT_NOSET},
/* O_TABSTOP	    4BSD */
	{"tabstop",	f_reformat,	OPT_NUM,	OPT_NOZERO},
/* O_TAGLENGTH	    4BSD */
	{"taglength",	NULL,		OPT_NUM,	0},
/* O_TAGS	    4BSD */
	{"tags",	NULL,		OPT_STR,	0},
/* O_TERM	    4BSD
 *	!!!
 *	By default, the historic vi always displayed information about two
 *	options, redraw and term.  Term seems sufficient.
 */
	{"term",	NULL,		OPT_STR,	OPT_ADISP|OPT_NOSAVE},
/* O_TERSE	    4BSD */
	{"terse",	NULL,		OPT_0BOOL,	0},
/* O_TILDEOP      4.4BSD */
	{"tildeop",	NULL,		OPT_0BOOL,	0},
/* O_TIMEOUT	    4BSD (undocumented) */
	{"timeout",	NULL,		OPT_1BOOL,	0},
/* O_TTYWERASE	  4.4BSD */
	{"ttywerase",	f_ttywerase,	OPT_0BOOL,	0},
/* O_VERBOSE	  4.4BSD */
	{"verbose",	NULL,		OPT_0BOOL,	0},
/* O_W1200	    4BSD */
	{"w1200",	f_w1200,	OPT_NUM,	OPT_NDISP|OPT_NOSAVE},
/* O_W300	    4BSD */
	{"w300",	f_w300,		OPT_NUM,	OPT_NDISP|OPT_NOSAVE},
/* O_W9600	    4BSD */
	{"w9600",	f_w9600,	OPT_NUM,	OPT_NDISP|OPT_NOSAVE},
/* O_WARN	    4BSD */
	{"warn",	NULL,		OPT_1BOOL,	0},
/* O_WINDOW	    4BSD */
	{"window",	f_window,	OPT_NUM,	0},
/* O_WINDOWNAME	    4BSD */
	{"windowname",	NULL,		OPT_0BOOL,	0},
/* O_WRAPLEN	  4.4BSD */
	{"wraplen",	NULL,		OPT_NUM,	0},
/* O_WRAPMARGIN	    4BSD */
	{"wrapmargin",	NULL,		OPT_NUM,	0},
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
	{"ed",		O_EDCOMPATIBLE},	/*     4BSD */
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
	{"smd",		O_SHOWMODE},		/*     4BSD */
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
	{"wl",		O_WRAPLEN},		/*   4.4BSD */
	{"wm",		O_WRAPMARGIN},		/*     4BSD */
	{"ws",		O_WRAPSCAN},		/*     4BSD */
	{NULL},
};

/*
 * opts_init --
 *	Initialize some of the options.
 *
 * PUBLIC: int opts_init __P((SCR *, int *));
 */
int
opts_init(sp, oargs)
	SCR *sp;
	int *oargs;
{
	ARGS *argv[2], a, b;
	OPTLIST const *op;
	u_long v;
	int cnt, optindx;
	char *s, b1[1024];

	a.bp = b1;
	b.bp = NULL;
	a.len = b.len = 0;
	argv[0] = &a;
	argv[1] = &b;

	/* Set numeric and string default values. */
#define	OI(indx, str) {							\
	if (str != b1)		/* GCC puts strings in text-space. */	\
		(void)strcpy(b1, str);					\
	a.len = strlen(b1);						\
	if (opts_set(sp, argv, NULL)) {					\
		 optindx = indx;					\
		goto err;						\
	}								\
}
	/*
	 * Indirect global options to global space.  Specifically, set up
	 * terminal, lines, columns first, they're used by other options.
	 * Note, don't set the flags until we've set up the indirection.
	 */
	if (o_set(sp, O_TERM, 0, NULL, GO_TERM))
		goto err;
	F_SET(&sp->opts[O_TERM], OPT_GLOBAL);
	if (o_set(sp, O_LINES, 0, NULL, GO_LINES))
		goto err;
	F_SET(&sp->opts[O_LINES], OPT_GLOBAL);
	if (o_set(sp, O_COLUMNS, 0, NULL, GO_COLUMNS))
		goto err;
	F_SET(&sp->opts[O_COLUMNS], OPT_GLOBAL);
	if (o_set(sp, O_SECURE, 0, NULL, GO_SECURE))
		goto err;
	F_SET(&sp->opts[O_SECURE], OPT_GLOBAL);

	/* Initialize string values. */
	(void)snprintf(b1, sizeof(b1),
	    "cdpath=%s", (s = getenv("CDPATH")) == NULL ? ":" : s);
	OI(O_CDPATH, b1);

	/*
	 * !!!
	 * Vi historically stored temporary files in /var/tmp.  We store them
	 * in /tmp by default, hoping it's a memory based file system.  There
	 * are two ways to change this -- the user can set either the directory
	 * option or the TMPDIR environmental variable.
	 */
	(void)snprintf(b1, sizeof(b1),
	    "directory=%s", (s = getenv("TMPDIR")) == NULL ? _PATH_TMP : s);
	OI(O_DIRECTORY, b1);
	OI(O_ESCAPETIME, "escapetime=6");
	OI(O_KEYTIME, "keytime=6");
	OI(O_MATCHTIME, "matchtime=7");
	(void)snprintf(b1, sizeof(b1), "msgcat=%s", _PATH_MSGCAT);
	OI(O_MSGCAT, b1);
	OI(O_REPORT, "report=5");
	OI(O_PARAGRAPHS, "paragraphs=IPLPPPQPP LIpplpipbp");
	(void)snprintf(b1, sizeof(b1), "path=%s", "");
	OI(O_PATH, b1);
	(void)snprintf(b1, sizeof(b1), "recdir=%s", _PATH_PRESERVE);
	OI(O_RECDIR, b1);
	OI(O_SECTIONS, "sections=NHSHH HUnhsh");
	(void)snprintf(b1, sizeof(b1),
	    "shell=%s", (s = getenv("SHELL")) == NULL ? _PATH_BSHELL : s);
	OI(O_SHELL, b1);
	OI(O_SHELLMETA, "shellmeta=~{[*?$`'\"\\");
	OI(O_SHIFTWIDTH, "shiftwidth=8");
	OI(O_SIDESCROLL, "sidescroll=16");
	OI(O_TABSTOP, "tabstop=8");
	(void)snprintf(b1, sizeof(b1), "tags=%s", _PATH_TAGS);
	OI(O_TAGS, b1);

	/*
	 * XXX
	 * Initialize O_SCROLL here, after term; initializing term should
	 * have created a LINES/COLUMNS value.
	 */
	if ((v = (O_VAL(sp, O_LINES) - 1) / 2) == 0)
		v = 1;
	(void)snprintf(b1, sizeof(b1), "scroll=%ld", v);
	OI(O_SCROLL, b1);

	/*
	 * The default window option values are:
	 *		8 if baud rate <=  600
	 *	       16 if baud rate <= 1200
	 *	LINES - 1 if baud rate  > 1200
	 *
	 * Note, the windows option code will correct any too-large value
	 * or when the O_LINES value is 1.
	 */
	if (sp->gp->scr_baud(sp, &v))
		return (1);
	if (v <= 600)
		v = 8;
	else if (v <= 1200)
		v = 16;
	else
		v = O_VAL(sp, O_LINES) - 1;
	(void)snprintf(b1, sizeof(b1), "window=%lu", v);
	OI(O_WINDOW, b1);

	/*
	 * Set boolean default values, and copy all settings into the default
	 * information.  OS_NOFREE is set, we're copying, not replacing.
	 */
	for (op = optlist, cnt = 0; op->name != NULL; ++op, ++cnt)
		switch (op->type) {
		case OPT_0BOOL:
			break;
		case OPT_1BOOL:
			O_SET(sp, cnt);
			O_D_SET(sp, cnt);
			break;
		case OPT_NUM:
			o_set(sp, cnt, OS_DEF, NULL, O_VAL(sp, cnt));
			break;
		case OPT_STR:
			if (O_STR(sp, cnt) != NULL && o_set(sp, cnt,
			    OS_DEF | OS_NOFREE | OS_STRDUP, O_STR(sp, cnt), 0))
				goto err;
			break;
		default:
			abort();
		}

	/*
	 * !!!
	 * Some options can be initialized by the command name or the
	 * command-line arguments.  They don't set the default values,
	 * it's historic practice.
	 */
	for (; *oargs != -1; ++oargs)
		OI(*oargs, optlist[*oargs].name);
	return (0);
#undef OI

err:	msgq(sp, M_ERR,
	    "031|Unable to set default %s option", optlist[optindx].name);
	return (1);
}

/*
 * opts_set --
 *	Change the values of one or more options.
 *
 * PUBLIC: int opts_set __P((SCR *, ARGS *[], char *));
 */
int
opts_set(sp, argv, usage)
	SCR *sp;
	ARGS *argv[];
	char *usage;
{
	enum optdisp disp;
	enum nresult nret;
	OPTLIST const *op;
	OPTION *spo;
	u_long value, turnoff;
	int ch, equals, nf, nf2, offset, qmark, rval;
	char *endp, *name, *p, *sep, *t;

	disp = NO_DISPLAY;
	for (rval = 0; argv[0]->len != 0; ++argv) {
		/*
		 * The historic vi dumped the options for each occurrence of
		 * "all" in the set list.  Puhleeze.
		 */
		if (!strcmp(argv[0]->bp, "all")) {
			disp = ALL_DISPLAY;
			continue;
		}

		/* Find equals sign or question mark. */
		for (sep = NULL, equals = qmark = 0,
		    p = name = argv[0]->bp; (ch = *p) != '\0'; ++p)
			if (ch == '=' || ch == '?') {
				if (p == name) {
					if (usage != NULL)
						msgq(sp, M_ERR,
						    "032|Usage: %s", usage);
					return (1);
				}
				sep = p;
				if (ch == '=')
					equals = 1;
				else
					qmark = 1;
				break;
			}

		turnoff = 0;
		op = NULL;
		if (sep != NULL)
			*sep++ = '\0';

		/* Search for the name, then name without any leading "no". */
		if ((op = opts_search(name)) == NULL &&
		    name[0] == 'n' && name[1] == 'o') {
			turnoff = 1;
			name += 2;
			op = opts_search(name);
		}
		if (op == NULL) {
			opts_nomatch(sp, name);
			rval = 1;
			continue;
		}

		/* Find current option values. */
		offset = op - optlist;
		spo = sp->opts + offset;

		/*
		 * !!!
		 * Historically, the question mark could be a separate
		 * argument.
		 */
		if (!equals && !qmark &&
		    argv[1]->len == 1 && argv[1]->bp[0] == '?') {
			++argv;
			qmark = 1;
		}

		/* Set name, value. */
		switch (op->type) {
		case OPT_0BOOL:
		case OPT_1BOOL:
			/* Some options may not be reset. */
			if (F_ISSET(op, OPT_NOUNSET) && turnoff) {
				msgq_str(sp, M_ERR, name,
			    "291|set: the %s option may not be turned off");
				rval = 1;
				break;
			}

			/* Some options may not be set. */
			if (F_ISSET(op, OPT_NOSET) && !turnoff) {
				msgq_str(sp, M_ERR, name,
			    "313|set: the %s option may never be turned on");
				rval = 1;
				break;
			}

			if (equals) {
				msgq_str(sp, M_ERR, name,
			    "034|set: [no]%s option doesn't take a value");
				rval = 1;
				break;
			}
			if (qmark) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}

			/*
			 * Do nothing if the value is unchanged, the underlying
			 * functions can be expensive.
			 */
			if (!F_ISSET(op, OPT_ALWAYS))
				if (turnoff) {
					if (!O_ISSET(sp, offset))
						break;
				} else {
					if (O_ISSET(sp, offset))
						break;
				}

			/* Report to subsystems. */
			if (op->func != NULL &&
			    op->func(sp, spo, NULL, &turnoff) ||
			    ex_optchange(sp, offset, NULL, &turnoff) ||
			    v_optchange(sp, offset, NULL, &turnoff) ||
			    sp->gp->scr_optchange(sp, offset, NULL, &turnoff)) {
				rval = 1;
				break;
			}

			/* Set the value. */
			if (turnoff)
				O_CLR(sp, offset);
			else
				O_SET(sp, offset);
			break;
		case OPT_NUM:
			if (turnoff) {
				msgq_str(sp, M_ERR, name,
				    "035|set: %s option isn't a boolean");
				rval = 1;
				break;
			}
			if (qmark || !equals) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}

			if (!isdigit(sep[0]))
				goto badnum;
			if ((nret =
			    nget_uslong(&value, sep, &endp, 10)) != NUM_OK) {
				p = msg_print(sp, name, &nf);
				t = msg_print(sp, sep, &nf2);
				switch (nret) {
				case NUM_ERR:
					msgq(sp, M_SYSERR,
					    "036|set: %s option: %s", p, t);
					break;
				case NUM_OVER:
					msgq(sp, M_ERR,
			    "037|set: %s option: %s: value overflow", p, t);
					break;
				case NUM_OK:
				case NUM_UNDER:
					abort();
				}
				if (nf)
					FREE_SPACE(sp, p, 0);
				if (nf2)
					FREE_SPACE(sp, t, 0);
				rval = 1;
				break;
			}
			if (*endp && !isblank(*endp)) {
badnum:				p = msg_print(sp, name, &nf);
				t = msg_print(sp, sep, &nf2);
				msgq(sp, M_ERR,
		    "038|set: %s option: %s is an illegal number", p, t);
				if (nf)
					FREE_SPACE(sp, p, 0);
				if (nf2)
					FREE_SPACE(sp, t, 0);
				rval = 1;
				break;
			}

			/* Some options may never be set to zero. */
			if (F_ISSET(op, OPT_NOZERO) && value == 0) {
				msgq_str(sp, M_ERR, name,
			    "314|set: the %s option may never be set to 0");
				rval = 1;
				break;
			}

			/*
			 * Do nothing if the value is unchanged, the underlying
			 * functions can be expensive.
			 */
			if (!F_ISSET(op, OPT_ALWAYS) &&
			    O_VAL(sp, offset) == value)
				break;

			/* Report to subsystems. */
			if (op->func != NULL &&
			    op->func(sp, spo, sep, &value) ||
			    ex_optchange(sp, offset, sep, &value) ||
			    v_optchange(sp, offset, sep, &value) ||
			    sp->gp->scr_optchange(sp, offset, sep, &value)) {
				rval = 1;
				break;
			}

			/* Set the value. */
			if (o_set(sp, offset, 0, NULL, value))
				rval = 1;
			break;
		case OPT_STR:
			if (turnoff) {
				msgq_str(sp, M_ERR, name,
				    "039|set: %s option isn't a boolean");
				rval = 1;
				break;
			}
			if (qmark || !equals) {
				if (!disp)
					disp = SELECT_DISPLAY;
				F_SET(spo, OPT_SELECTED);
				break;
			}

			/*
			 * Do nothing if the value is unchanged, the underlying
			 * functions can be expensive.
			 */
			if (!F_ISSET(op, OPT_ALWAYS) &&
			    O_STR(sp, offset) != NULL &&
			    !strcmp(O_STR(sp, offset), sep))
				break;

			/* Report to subsystems. */
			if (op->func != NULL &&
			    op->func(sp, spo, sep, NULL) ||
			    ex_optchange(sp, offset, sep, NULL) ||
			    v_optchange(sp, offset, sep, NULL) ||
			    sp->gp->scr_optchange(sp, offset, sep, NULL)) {
				rval = 1;
				break;
			}

			/* Set the value. */
			if (o_set(sp, offset, OS_STRDUP, sep, 0))
				rval = 1;
			break;
		default:
			abort();
		}
	}
	if (disp != NO_DISPLAY)
		opts_dump(sp, disp);
	return (rval);
}

/*
 * o_set --
 *	Set an option's value.
 *
 * PUBLIC: int o_set __P((SCR *, int, u_int, char *, u_long));
 */
int
o_set(sp, opt, flags, str, val)
	SCR *sp;
	int opt;
	u_int flags;
	char *str;
	u_long val;
{
	OPTION *op;

	/* Set a pointer to the options area. */
	op = F_ISSET(&sp->opts[opt], OPT_GLOBAL) ?
	    &sp->gp->opts[sp->opts[opt].o_cur.val] : &sp->opts[opt];

	/* Copy the string, if requested. */
	if (LF_ISSET(OS_STRDUP) && (str = strdup(str)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}

	/* Free the previous string, if requested, and set the value. */
	if LF_ISSET(OS_DEF)
		if (LF_ISSET(OS_STR | OS_STRDUP)) {
			if (!LF_ISSET(OS_NOFREE) && op->o_def.str != NULL)
				free(op->o_def.str);
			op->o_def.str = str;
		} else
			op->o_def.val = val;
	else
		if (LF_ISSET(OS_STR | OS_STRDUP)) {
			if (!LF_ISSET(OS_NOFREE) && op->o_cur.str != NULL)
				free(op->o_cur.str);
			op->o_cur.str = str;
		} else
			op->o_cur.val = val;
	return (0);
}

/*
 * opts_empty --
 *	Return 1 if the string option is invalid, 0 if it's OK.
 *
 * PUBLIC: int opts_empty __P((SCR *, int, int));
 */
int
opts_empty(sp, off, silent)
	SCR *sp;
	int off, silent;
{
	char *p;

	if ((p = O_STR(sp, off)) == NULL || p[0] == '\0') {
		if (!silent)
			msgq_str(sp, M_ERR, optlist[off].name,
			    "305|No %s edit option specified");
		return (1);
	}
	return (0);
}

/*
 * opts_dump --
 *	List the current values of selected options.
 *
 * PUBLIC: void opts_dump __P((SCR *, enum optdisp));
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
	 *
	 * Find a column width we can live with, testing from 10 columns to 1.
	 */
	for (numcols = 10; numcols > 1; --numcols) {
		colwidth = sp->cols / numcols & ~(STANDARD_TAB - 1);
		if (colwidth >= 10) {
			colwidth =
			    (colwidth + STANDARD_TAB) & ~(STANDARD_TAB - 1);
			numcols = sp->cols / colwidth;
			break;
		}
		colwidth = 0;
	}

	/*
	 * Get the set of options to list, entering them into
	 * the column list or the overflow list.
	 */
	for (b_num = s_num = 0, op = optlist; op->name != NULL; ++op) {
		cnt = op - optlist;

		/* If OPT_NDISP set, it's never displayed. */
		if (F_ISSET(op, OPT_NDISP))
			continue;

		switch (type) {
		case ALL_DISPLAY:		/* Display all. */
			break;
		case CHANGED_DISPLAY:		/* Display changed. */
			/* If OPT_ADISP set, it's always "changed". */
			if (F_ISSET(op, OPT_ADISP))
				break;
			switch (op->type) {
			case OPT_0BOOL:
			case OPT_1BOOL:
			case OPT_NUM:
				if (O_VAL(sp, cnt) == O_D_VAL(sp, cnt))
					continue;
				break;
			case OPT_STR:
				if (O_STR(sp, cnt) == O_D_STR(sp, cnt) ||
				    O_D_STR(sp, cnt) != NULL &&
				    !strcmp(O_STR(sp, cnt), O_D_STR(sp, cnt)))
					continue;
				break;
			}
			break;
		case SELECT_DISPLAY:		/* Display selected. */
			if (!F_ISSET(&sp->opts[cnt], OPT_SELECTED))
				continue;
			break;
		default:
		case NO_DISPLAY:
			abort();
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
			if (O_STR(sp, cnt) != NULL)
				curlen += strlen(O_STR(sp, cnt));
			curlen += 3;
			break;
		}
		/* Offset by 2 so there's a gap. */
		if (curlen <= colwidth - 2)
			s_op[s_num++] = cnt;
		else
			b_op[b_num++] = cnt;
	}

	if (s_num > 0) {
		/* Figure out the number of rows. */
		if (s_num > numcols) {
			numrows = s_num / numcols;
			if (s_num % numcols)
				++numrows;
		} else
			numrows = 1;

		/* Display the options in sorted order. */
		for (row = 0; row < numrows;) {
			for (base = row, col = 0; col < numcols; ++col) {
				cnt = opts_print(sp, &optlist[s_op[base]]);
				if ((base += numrows) >= s_num)
					break;
				(void)ex_printf(sp, "%*s",
				    (int)(colwidth - cnt), "");
			}
			if (++row < numrows || b_num)
				(void)ex_puts(sp, "\n");
		}
	}

	for (row = 0; row < b_num;) {
		(void)opts_print(sp, &optlist[b_op[row]]);
		if (++row < b_num)
			(void)ex_puts(sp, "\n");
	}
	(void)ex_puts(sp, "\n");
}

/*
 * opts_print --
 *	Print out an option.
 */
static int
opts_print(sp, op)
	SCR *sp;
	OPTLIST const *op;
{
	int curlen, offset;

	curlen = 0;
	offset = op - optlist;
	switch (op->type) {
	case OPT_0BOOL:
	case OPT_1BOOL:
		curlen += ex_printf(sp,
		    "%s%s", O_ISSET(sp, offset) ? "" : "no", op->name);
		break;
	case OPT_NUM:
		curlen += ex_printf(sp, "%s=%ld", op->name, O_VAL(sp, offset));
		break;
	case OPT_STR:
		curlen += ex_printf(sp, "%s=\"%s\"", op->name,
		    O_STR(sp, offset) == NULL ? "" : O_STR(sp, offset));
		break;
	}
	return (curlen);
}

/*
 * opts_save --
 *	Write the current configuration to a file.
 *
 * PUBLIC: int opts_save __P((SCR *, FILE *));
 */
int
opts_save(sp, fp)
	SCR *sp;
	FILE *fp;
{
	OPTLIST const *op;
	int ch, cnt;
	char *p;

	for (op = optlist; op->name != NULL; ++op) {
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
			    "set %s=%-3ld\n", op->name, O_VAL(sp, cnt));
			break;
		case OPT_STR:
			if (O_STR(sp, cnt) == NULL)
				break;
			(void)fprintf(fp, "set ");
			for (p = op->name; (ch = *p) != '\0'; ++p) {
				if (isblank(ch) || ch == '\\')
					(void)putc('\\', fp);
				(void)putc(ch, fp);
			}
			(void)putc('=', fp);
			for (p = O_STR(sp, cnt); (ch = *p) != '\0'; ++p) {
				if (isblank(ch) || ch == '\\')
					(void)putc('\\', fp);
				(void)putc(ch, fp);
			}
			(void)putc('\n', fp);
			break;
		}
		if (ferror(fp)) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
	}
	return (0);
}

/* 
 * opts_search --
 *	Search for an option.
 *
 * PUBLIC: OPTLIST const *opts_search __P((char *));
 */
OPTLIST const *
opts_search(name)
	char *name;
{
	OPTLIST const *op, *found;
	OABBREV atmp, *ap;
	OPTLIST otmp;
	size_t len;

	/* Check list of abbreviations. */
	atmp.name = name;
	if ((ap = bsearch(&atmp, abbrev, sizeof(abbrev) / sizeof(OABBREV) - 1,
	    sizeof(OABBREV), opts_abbcmp)) != NULL)
		return (optlist + ap->offset);

	/* Check list of options. */
	otmp.name = name;
	if ((op = bsearch(&otmp, optlist, sizeof(optlist) / sizeof(OPTLIST) - 1,
	    sizeof(OPTLIST), opts_cmp)) != NULL)
		return (op);
		
	/*
	 * Check to see if the name is the prefix of one (and only one)
	 * option.  If so, return the option.
	 */
	len = strlen(name);
	for (found = NULL, op = optlist; op->name != NULL; ++op) {
		if (op->name[0] < name[0])
			continue;
		if (op->name[0] > name[0])
			break;
		if (!memcmp(op->name, name, len)) {
			if (found != NULL)
				return (NULL);
			found = op;
		}
	}
	return (found);
}

/* 
 * opts_nomatch --
 *	Standard nomatch error message for options.
 *
 * PUBLIC: void opts_nomatch __P((SCR *, char *));
 */
void
opts_nomatch(sp, name)
	SCR *sp;
	char *name;
{
	msgq_str(sp, M_ERR, name,
	    "033|set: no %s option: 'set all' gives all option values");
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
 * opts_copy --
 *	Copy a screen's OPTION array.
 *
 * PUBLIC: int opts_copy __P((SCR *, SCR *));
 */
int
opts_copy(orig, sp)
	SCR *orig, *sp;
{
	int cnt, rval;

	/* Copy most everything without change. */
	memcpy(sp->opts, orig->opts, sizeof(orig->opts));

	/* Copy the string edit options. */
	for (cnt = rval = 0; cnt < O_OPTIONCOUNT; ++cnt) {
		if (optlist[cnt].type != OPT_STR ||
		    F_ISSET(&optlist[cnt], OPT_GLOBAL))
			continue;
		/*
		 * If never set, or already failed, NULL out the entries --
		 * have to continue after failure, otherwise would have two
		 * screens referencing the same memory.
		 */
		if (rval || O_STR(sp, cnt) == NULL) {
			o_set(sp, cnt, OS_NOFREE | OS_STR, NULL, 0);
			o_set(sp, cnt, OS_DEF | OS_NOFREE | OS_STR, NULL, 0);
			continue;
		}

		/* Copy the current string. */
		if (o_set(sp, cnt, OS_NOFREE | OS_STRDUP, O_STR(sp, cnt), 0)) {
			o_set(sp, cnt, OS_DEF | OS_NOFREE | OS_STR, NULL, 0);
			goto nomem;
		}

		/* Copy the default string. */
		if (O_D_STR(sp, cnt) != NULL && o_set(sp, cnt,
		    OS_DEF | OS_NOFREE | OS_STRDUP, O_D_STR(sp, cnt), 0)) {
nomem:			msgq(orig, M_SYSERR, NULL);
			rval = 1;
		}
	}
	return (rval);
}

/*
 * opts_free --
 *	Free all option strings
 *
 * PUBLIC: void opts_free __P((SCR *));
 */
void
opts_free(sp)
	SCR *sp;
{
	int cnt;

	for (cnt = 0; cnt < O_OPTIONCOUNT; ++cnt) {
		if (optlist[cnt].type != OPT_STR ||
		    F_ISSET(&optlist[cnt], OPT_GLOBAL))
			continue;
		if (O_STR(sp, cnt) != NULL)
			free(O_STR(sp, cnt));
		if (O_D_STR(sp, cnt) != NULL)
			free(O_D_STR(sp, cnt));
	}
}
