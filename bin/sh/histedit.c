/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
#if 0
static char sccsid[] = "@(#)histedit.c	8.2 (Berkeley) 5/4/95";
#endif
static const char rcsid[] =
  "$FreeBSD: src/bin/sh/histedit.c,v 1.13.2.1 2000/06/14 13:42:25 cracauer Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/*
 * Editline and history functions (and glue).
 */
#include "shell.h"
#include "parser.h"
#include "var.h"
#include "options.h"
#include "main.h"
#include "output.h"
#include "mystring.h"
#ifndef NO_HISTORY
#include "myhistedit.h"
#include "error.h"
#include "eval.h"
#include "memalloc.h"

#define MAXHISTLOOPS	4	/* max recursions through fc */
#define DEFEDITOR	"ed"	/* default editor *should* be $EDITOR */

History *hist;	/* history cookie */
EditLine *el;	/* editline cookie */
int displayhist;
static FILE *el_in, *el_out;

STATIC char *fc_replace __P((const char *, char *, char *));

/*
 * Set history and editing status.  Called whenever the status may
 * have changed (figures out what to do).
 */
void
histedit()
{

#define editing (Eflag || Vflag)

	if (iflag) {
		if (!hist) {
			/*
			 * turn history on
			 */
			INTOFF;
			hist = history_init();
			INTON;

			if (hist != NULL)
				sethistsize(histsizeval());
			else
				out2str("sh: can't initialize history\n");
		}
		if (editing && !el && isatty(0)) { /* && isatty(2) ??? */
			/*
			 * turn editing on
			 */
			INTOFF;
			if (el_in == NULL)
				el_in = fdopen(0, "r");
			if (el_out == NULL)
				el_out = fdopen(2, "w");
			if (el_in == NULL || el_out == NULL)
				goto bad;
			el = el_init(arg0, el_in, el_out);
			if (el != NULL) {
				if (hist)
					el_set(el, EL_HIST, history, hist);
				el_set(el, EL_PROMPT, getprompt);
			} else {
bad:
				out2str("sh: can't initialize editing\n");
			}
			INTON;
		} else if (!editing && el) {
			INTOFF;
			el_end(el);
			el = NULL;
			INTON;
		}
		if (el) {
			if (Vflag)
				el_set(el, EL_EDITOR, "vi");
			else if (Eflag)
				el_set(el, EL_EDITOR, "emacs");
		}
	} else {
		INTOFF;
		if (el) {	/* no editing if not interactive */
			el_end(el);
			el = NULL;
		}
		if (hist) {
			history_end(hist);
			hist = NULL;
		}
		INTON;
	}
}


void
sethistsize(hs)
	const char *hs;
{
	int histsize;

	if (hist != NULL) {
		if (hs == NULL || *hs == '\0' ||
		   (histsize = atoi(hs)) < 0)
			histsize = 100;
		history(hist, H_EVENT, histsize);
	}
}

/*
 *  This command is provided since POSIX decided to standardize
 *  the Korn shell fc command.  Oh well...
 */
int
histcmd(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	char *editor = NULL;
	const HistEvent *he;
	int lflg = 0, nflg = 0, rflg = 0, sflg = 0;
	int i;
	char *firststr, *laststr;
	int first, last, direction;
	char *pat = NULL, *repl;	/* ksh "fc old=new" crap */
	static int active = 0;
	struct jmploc jmploc;
	struct jmploc *volatile savehandler;
	char editfile[MAXPATHLEN + 1];
	FILE *efp;
#ifdef __GNUC__
	/* Avoid longjmp clobbering */
	(void) &editor;
	(void) &lflg;
	(void) &nflg;
	(void) &rflg;
	(void) &sflg;
	(void) &firststr;
	(void) &laststr;
	(void) &pat;
	(void) &repl;
	(void) &efp;
	(void) &argc;
	(void) &argv;
#endif

	if (hist == NULL)
		error("history not active");

	if (argc == 1)
		error("missing history argument");

	optreset = 1; optind = 1; /* initialize getopt */
	while (not_fcnumber(argv[optind]) &&
	      (ch = getopt(argc, argv, ":e:lnrs")) != -1)
		switch ((char)ch) {
		case 'e':
			editor = shoptarg;
			break;
		case 'l':
			lflg = 1;
			break;
		case 'n':
			nflg = 1;
			break;
		case 'r':
			rflg = 1;
			break;
		case 's':
			sflg = 1;
			break;
		case ':':
			error("option -%c expects argument", optopt);
		case '?':
		default:
			error("unknown option: -%c", optopt);
		}
	argc -= optind, argv += optind;

	/*
	 * If executing...
	 */
	if (lflg == 0 || editor || sflg) {
		lflg = 0;	/* ignore */
		editfile[0] = '\0';
		/*
		 * Catch interrupts to reset active counter and
		 * cleanup temp files.
		 */
		if (setjmp(jmploc.loc)) {
			active = 0;
			if (*editfile)
				unlink(editfile);
			handler = savehandler;
			longjmp(handler->loc, 1);
		}
		savehandler = handler;
		handler = &jmploc;
		if (++active > MAXHISTLOOPS) {
			active = 0;
			displayhist = 0;
			error("called recursively too many times");
		}
		/*
		 * Set editor.
		 */
		if (sflg == 0) {
			if (editor == NULL &&
			    (editor = bltinlookup("FCEDIT", 1)) == NULL &&
			    (editor = bltinlookup("EDITOR", 1)) == NULL)
				editor = DEFEDITOR;
			if (editor[0] == '-' && editor[1] == '\0') {
				sflg = 1;	/* no edit */
				editor = NULL;
			}
		}
	}

	/*
	 * If executing, parse [old=new] now
	 */
	if (lflg == 0 && argc > 0 &&
	     ((repl = strchr(argv[0], '=')) != NULL)) {
		pat = argv[0];
		*repl++ = '\0';
		argc--, argv++;
	}
	/*
	 * determine [first] and [last]
	 */
	switch (argc) {
	case 0:
		firststr = lflg ? "-16" : "-1";
		laststr = "-1";
		break;
	case 1:
		firststr = argv[0];
		laststr = lflg ? "-1" : argv[0];
		break;
	case 2:
		firststr = argv[0];
		laststr = argv[1];
		break;
	default:
		error("too many args");
	}
	/*
	 * Turn into event numbers.
	 */
	first = str_to_event(firststr, 0);
	last = str_to_event(laststr, 1);

	if (rflg) {
		i = last;
		last = first;
		first = i;
	}
	/*
	 * XXX - this should not depend on the event numbers
	 * always increasing.  Add sequence numbers or offset
	 * to the history element in next (diskbased) release.
	 */
	direction = first < last ? H_PREV : H_NEXT;

	/*
	 * If editing, grab a temp file.
	 */
	if (editor) {
		int fd;
		INTOFF;		/* easier */
		sprintf(editfile, "%s/_shXXXXXX", _PATH_TMP);
		if ((fd = mkstemp(editfile)) < 0)
			error("can't create temporary file %s", editfile);
		if ((efp = fdopen(fd, "w")) == NULL) {
			close(fd);
			error("can't allocate stdio buffer for temp");
		}
	}

	/*
	 * Loop through selected history events.  If listing or executing,
	 * do it now.  Otherwise, put into temp file and call the editor
	 * after.
	 *
	 * The history interface needs rethinking, as the following
	 * convolutions will demonstrate.
	 */
	history(hist, H_FIRST);
	he = history(hist, H_NEXT_EVENT, first);
	for (;he != NULL; he = history(hist, direction)) {
		if (lflg) {
			if (!nflg)
				out1fmt("%5d ", he->num);
			out1str(he->str);
		} else {
			char *s = pat ?
			   fc_replace(he->str, pat, repl) : (char *)he->str;

			if (sflg) {
				if (displayhist) {
					out2str(s);
				}
				evalstring(s);
				if (displayhist && hist) {
					/*
					 *  XXX what about recursive and
					 *  relative histnums.
					 */
					history(hist, H_ENTER, s);
				}
			} else
				fputs(s, efp);
		}
		/*
		 * At end?  (if we were to loose last, we'd sure be
		 * messed up).
		 */
		if (he->num == last)
			break;
	}
	if (editor) {
		char *editcmd;

		fclose(efp);
		editcmd = stalloc(strlen(editor) + strlen(editfile) + 2);
		sprintf(editcmd, "%s %s", editor, editfile);
		evalstring(editcmd);	/* XXX - should use no JC command */
		INTON;
		readcmdfile(editfile);	/* XXX - should read back - quick tst */
		unlink(editfile);
	}

	if (lflg == 0 && active > 0)
		--active;
	if (displayhist)
		displayhist = 0;
	return 0;
}

STATIC char *
fc_replace(s, p, r)
	const char *s;
	char *p, *r;
{
	char *dest;
	int plen = strlen(p);

	STARTSTACKSTR(dest);
	while (*s) {
		if (*s == *p && strncmp(s, p, plen) == 0) {
			while (*r)
				STPUTC(*r++, dest);
			s += plen;
			*p = '\0';	/* so no more matches */
		} else
			STPUTC(*s++, dest);
	}
	STACKSTRNUL(dest);
	dest = grabstackstr(dest);

	return (dest);
}

int
not_fcnumber(s)
	char *s;
{
	if (s == NULL)
		return (0);
	if (*s == '-')
		s++;
	return (!is_number(s));
}

int
str_to_event(str, last)
	char *str;
	int last;
{
	const HistEvent *he;
	char *s = str;
	int relative = 0;
	int i;

	he = history(hist, H_FIRST);
	switch (*s) {
	case '-':
		relative = 1;
		/*FALLTHROUGH*/
	case '+':
		s++;
	}
	if (is_number(s)) {
		i = atoi(s);
		if (relative) {
			while (he != NULL && i--) {
				he = history(hist, H_NEXT);
			}
			if (he == NULL)
				he = history(hist, H_LAST);
		} else {
			he = history(hist, H_NEXT_EVENT, i);
			if (he == NULL) {
				/*
				 * the notion of first and last is
				 * backwards to that of the history package
				 */
				he = history(hist, last ? H_FIRST : H_LAST);
			}
		}
		if (he == NULL)
			error("history number %s not found (internal error)",
			       str);
	} else {
		/*
		 * pattern
		 */
		he = history(hist, H_PREV_STR, str);
		if (he == NULL)
			error("history pattern not found: %s", str);
	}
	return (he->num);
}
#else
#include "error.h"

int
histcmd(argc, argv)
	int argc;
	char **argv;
{

	error("not compiled with history support");
	/*NOTREACHED*/
	return (0);
}
#endif
