/* ed.c: This file contains the main control and user-interface routines
   for the ed line editor. */
/*-
 * Copyright (c) 1993 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Andrew Moore, Talke Studio.
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
/*-
 * Kernighan/Plauger, "Software Tools in Pascal," (c) 1981 by
 * Addison-Wesley Publishing Company, Inc.  Reprinted with permission of
 * the publisher.
 */

#ifndef lint
char copyright1[] =
"@(#) Copyright (c) 1993 The Regents of the University of California.\n\
 All rights reserved.\n";
char copyright2[] =
"@(#) Kernighan/Plauger, Software Tools in Pascal, (c) 1981 by\n\
 Addison-Wesley Publishing Company, Inc.  Reprinted with permission of\n\
 the publisher.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)ed.c	5.5 (Berkeley) 3/28/93";
#endif /* not lint */

/*
 * CREDITS
 *	The buf.c algorithm is attributed to Rodney Ruddock of
 *	the University of Guelph, Guelph, Ontario.
 *
 *	The cbc.c encryption code is adapted from
 *	the bdes program by Matt Bishop of Dartmouth College,
 *	Hanover, NH.
 *
 *	Addison-Wesley Publishing Company generously granted
 *	permission to distribute this program over Internet.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <pwd.h>
#include <sys/ioctl.h>

#include "ed.h"

#ifdef _POSIX_SOURCE
sigjmp_buf env;
#else
jmp_buf env;
#endif

/* static buffers */
char *shcmd;			/* shell command buffer */
int shcmdsz;			/* shell command buffer size */
int shcmdi;			/* shell command buffer index */
char *cvbuf;			/* global command buffer */
int cvbufsz;			/* global command buffer size */
char *lhbuf;			/* lhs buffer */
int lhbufsz;			/* lhs buffer size */
char *rhbuf;			/* rhs buffer */
int rhbufsz;			/* rhs buffer size */
int rhbufi;			/* rhs buffer index */
char *rbuf;			/* regsub buffer */
int rbufsz;			/* regsub buffer size */
char *sbuf;			/* file i/o buffer */
int sbufsz;			/* file i/o buffer size */
char *ibuf;			/* ed command-line buffer */
int ibufsz;			/* ed command-line buffer size */
char *ibufp;			/* pointer to ed command-line buffer */

/* global flags */
int isbinary;			/* if set, buffer contains ASCII NULs */
int modified;			/* if set, buffer modified since last write */
int garrulous = 0;		/* if set, print all error messages */
int scripted = 0;		/* if set, suppress diagnostics */
int des = 0;			/* if set, use crypt(3) for i/o */
int mutex = 0;			/* if set, signals set "sigflags" */
int sigflags = 0;		/* if set, signals received while mutex set */
int sigactive = 0;		/* if set, signal handlers are enabled */
int red = 0;			/* if set, restrict shell/directory access */

char dfn[MAXFNAME + 1] = "";	/* default filename */
long curln;			/* current address */
long lastln;			/* last address */
int lineno;			/* script line number */
char *prompt;			/* command-line prompt */
char *dps = "*";		/* default command-line prompt */

char *usage = "usage: %s [-] [-sx] [-p string] [name]\n";

extern char errmsg[];
extern int optind;
extern char *optarg;

/* ed: line editor */
main(argc, argv)
	int argc;
	char **argv;
{
	int c, n;
	long status = 0;

	red = (n = strlen(argv[0])) > 2 && argv[0][n - 3] == 'r';
top:
	while ((c = getopt(argc, argv, "p:sx")) != EOF)
		switch(c) {
		case 'p':				/* set prompt */
			prompt = optarg;
			break;
		case 's':				/* run script */
			scripted = 1;
			break;
		case 'x':				/* use crypt */
#ifdef DES
			des = getkey();
#else
			fprintf(stderr, "crypt unavailable\n?\n");
#endif
			break;

		default:
			fprintf(stderr, usage, argv[0]);
			exit(1);
		}
	argv += optind;
	argc -= optind;
	if (argc && **argv == '-') {
		scripted = 1;
		if (argc > 1) {
			optind = 1;
			goto top;
		}
		argv++;
		argc--;
	}
	/* assert: reliable signals! */
#ifdef SIGWINCH
	dowinch(SIGWINCH);
	if (isatty(0)) signal(SIGWINCH, dowinch);
#endif
	signal(SIGHUP, onhup);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, onintr);
#ifdef _POSIX_SOURCE
	if (status = sigsetjmp(env, 1))
#else
	if (status = setjmp(env))
#endif
	{
		fputs("\n?\n", stderr);
		sprintf(errmsg, "interrupt");
	} else {
		init_buf();
		sigactive = 1;			/* enable signal handlers */
		if (argc && **argv && ckfn(*argv)) {
			if (doread(0, *argv) < 0 && !isatty(0))
				quit(2);
			else if (**argv != '!')
				strcpy(dfn, *argv);
		} else if (argc) {
			fputs("?\n", stderr);
			if (**argv == '\0')
				sprintf(errmsg, "invalid filename");
			if (!isatty(0))
				quit(2);
		}
	}
	for (;;) {
		if (status < 0 && garrulous)
			fprintf(stderr, "%s\n", errmsg);
		if (prompt) {
			printf("%s", prompt);
			fflush(stdout);
		}
		if ((n = getline()) < 0) {
			status = ERR;
			continue;
		} else if (n == 0) {
			if (modified && !scripted) {
				fputs("?\n", stderr);
				sprintf(errmsg, "warning: file modified");
				if (!isatty(0)) {
					fprintf(stderr, garrulous ? "script, line %d: %s\n"
						: "", lineno, errmsg);
					quit(2);
				}
				clearerr(stdin);
				modified = 0;
				status = EMOD;
				continue;
			} else
				quit(0);
		} else if (ibuf[n - 1] != '\n') {
			/* discard line */
			sprintf(errmsg, "unexpected end-of-file");
			clearerr(stdin);
			status = ERR;
			continue;
		}
		if ((n = getlist()) >= 0 && (status = ckglob()) != 0) {
			if (status > 0 && (status = doglob(status)) >= 0) {
				curln = status;
				continue;
			}
		} else if ((status = n) >= 0 && (status = docmd(0)) >= 0) {
			if (!status || status
			 && (status = doprint(curln, curln, status)) >= 0)
				continue;
		}
		switch (status) {
		case EOF:
			quit(0);
		case EMOD:
			modified = 0;
			fputs("?\n", stderr);		/* give warning */
			sprintf(errmsg, "warning: file modified");
			if (!isatty(0)) {
				fprintf(stderr, garrulous ? "script, line %d: %s\n" : "", lineno, errmsg);
				quit(2);
			}
			break;
		case FATAL:
			if (!isatty(0))
				fprintf(stderr, garrulous ? "script, line %d: %s\n" : "", lineno, errmsg);
			else
				fprintf(stderr, garrulous ? "%s\n" : "", errmsg);
			quit(3);
		default:
			fputs("?\n", stderr);
			if (!isatty(0)) {
				fprintf(stderr, garrulous ? "script, line %d: %s\n" : "", lineno, errmsg);
				quit(2);
			}
			break;
		}
	}
	/*NOTREACHED*/
}


long line1, line2, nlines;

/* getlist: get line numbers from the command buffer until an illegal
   address is seen.  return range status */
getlist()
{
	long num;

	nlines = line2 = 0;
	while ((num = getone()) >= 0) {
		line1 = line2;
		line2 = num;
		nlines++;
		if (*ibufp != ',' && *ibufp != ';')
			break;
		else if (*ibufp++ == ';')
			curln = num;
	}
	nlines = min(nlines, 2);
	if (nlines == 0)
		line2 = curln;
	if (nlines <= 1)
		line1 = line2;
	return  (num == ERR) ? ERR : nlines;
}


/*  getone: return the next line number in the command buffer */
long
getone()
{
	int c;
	long i, num;

	if ((num = getnum(1)) < 0)
		return num;
	for (;;) {
		c = isspace(*ibufp);
		skipblanks();
		c = c && isdigit(*ibufp);
		if (!c && *ibufp != '+' && *ibufp != '-' && *ibufp != '^')
			break;
		c = c ? '+' : *ibufp++;
		if ((i = getnum(0)) < 0) {
			sprintf(errmsg, "invalid address");
			return  i;
		}
		if (c == '+')
			num += i;
		else	num -= i;
	}
	if (num > lastln || num < 0) {
		sprintf(errmsg, "invalid address");
		return ERR;
	}
	return num;
}


/* getnum:  return a relative line number from the command buffer */
long
getnum(first)
	int first;
{
	pattern_t *pat;
	char c;

	skipblanks();
	if (isdigit(*ibufp))
		return strtol(ibufp, &ibufp, 10);
	switch(c = *ibufp) {
	case '.':
		ibufp++;
		return first ? curln : ERR;
	case '$':
		ibufp++;
		return first ? lastln : ERR;
	case '/':
	case '?':
		if ((pat = optpat()) == NULL)
			return ERR;
		else if (*ibufp == c)
			ibufp++;
		return first ? patscan(pat, (c == '/') ? 1 : 0) : ERR;
	case '^':
	case '-':
	case '+':
		return first ? curln : 1;
	case '\'':
		ibufp++;
		return first ? getmark(*ibufp++) : ERR;
	case '%':
	case ',':
	case ';':
		if (first) {
			ibufp++;
			line2 = (c == ';') ? curln : 1;
			nlines++;
			return lastln;
		}
		return 1;
	default:
		return  first ? EOF : 1;
	}
}


/* gflags */
#define GLB 001		/* global command */
#define GPR 002		/* print after command */
#define GLS 004		/* list after command */
#define GNP 010		/* enumerate after command */
#define GSG 020		/* global substitute */


/* VRFYCMD: verify the command suffix in the command buffer */
#define VRFYCMD() { \
	int done = 0; \
	do { \
		switch(*ibufp) { \
		case 'p': \
			gflag |= GPR, ibufp++; \
			break; \
		case 'l': \
			gflag |= GLS, ibufp++; \
			break; \
		case 'n': \
			gflag |= GNP, ibufp++; \
			break; \
		default: \
			done++; \
		} \
	} while (!done); \
	if (*ibufp++ != '\n') { \
		sprintf(errmsg, "invalid command suffix"); \
		return ERR; \
	} \
}


/* ckglob:  set lines matching a pattern in the command buffer; return
   global status  */
ckglob()
{
	pattern_t *pat;
	char c, delim;
	char *s;
	int nomatch;
	long n;
	line_t *lp;
	int gflag = 0;			/* print suffix of interactive cmd */

	if ((c = *ibufp) == 'V' || c == 'G')
		gflag = GLB;
	else if (c != 'g' && c != 'v')
		return 0;
	if (ckrange(1, lastln) < 0)
		return ERR;
	else if ((delim = *++ibufp) == ' ' || delim == '\n') {
		sprintf(errmsg, "invalid pattern delimiter");
		return ERR;
	} else if ((pat = optpat()) == NULL)
		return ERR;
	else if (*ibufp == delim)
		ibufp++;
	if (gflag)
		VRFYCMD();			/* get print suffix */
	for (lp = getlp(n = 1); n <= lastln; n++, lp = lp->next) {
		if ((s = gettxt(lp)) == NULL)
			return ERR;
		lp->len &= ~ACTV;			/* zero ACTV  bit */
		if (isbinary)
			s = nultonl(s, lp->len & ~ACTV);
		if (line1 <= n && n <= line2
		 && (!(nomatch = regexec(pat, s, 0, NULL, 0))
		 && (c == 'g'  || c == 'G')
		 || nomatch && (c == 'v' || c == 'V')))
			lp->len |= ACTV;
	}
	return gflag | GSG;
}


/* doglob: apply command list in the command buffer to the active
   lines in a range; return command status */
long
doglob(gflag)
	int gflag;
{
	static char *ocmd = NULL;
	static int ocmdsz = 0;

	line_t *lp = NULL;
	long lc;
	int status;
	int n;
	int interact = gflag & ~GSG;		/* GLB & gflag ? */
	char *cmd = NULL;

#ifdef BACKWARDS
	if (!interact)
		if (!strcmp(ibufp, "\n"))
			cmd = "p\n";		/* null cmd-list == `p' */
		else if ((cmd = getcmdv(&n, 0)) == NULL)
			return ERR;
#else
	if (!interact && (cmd = getcmdv(&n, 0)) == NULL)
		return ERR;
#endif
	ureset();
	for (;;) {
		for (lp = getlp(lc = 1); lc <= lastln; lc++, lp = lp->next)
			if (lp->len & ACTV)		/* active line */
				break;
		if (lc > lastln)
			break;
		lp->len ^= ACTV;			/* zero ACTV bit */
		curln = lc;
		if (interact) {
			/* print curln and get a command in global syntax */
			if (doprint(curln, curln, 0) < 0)
				return ERR;
			while ((n = getline()) > 0
			    && ibuf[n - 1] != '\n')
				clearerr(stdin);
			if (n < 0)
				return ERR;
			else if (n == 0) {
				sprintf(errmsg, "unexpected end-of-file");
				return ERR;
			} else if (n == 1 && !strcmp(ibuf, "\n"))
				continue;
			else if (n == 2 && !strcmp(ibuf, "&\n")) {
				if (cmd == NULL) {
					sprintf(errmsg, "no previous command");
					return ERR;
				} else cmd = ocmd;
			} else if ((cmd = getcmdv(&n, 0)) == NULL)
				return ERR;
			else {
				CKBUF(ocmd, ocmdsz, n + 1, ERR);
				memcpy(ocmd, cmd, n + 1);
				cmd = ocmd;
			}

		}
		ibufp = cmd;
		for (; *ibufp;)
			if ((status = getlist()) < 0
			 || (status = docmd(1)) < 0
			 || (status > 0
			 && (status = doprint(curln, curln, status)) < 0))
				return status;
	}
	return ((interact & ~GLB ) && doprint(curln, curln, interact) < 0) ? ERR : curln;
}


#ifdef BACKWARDS
/* GETLINE3: get a legal address from the command buffer */
#define GETLINE3(num) \
{ \
	long ol1, ol2; \
\
	ol1 = line1, ol2 = line2; \
	if (getlist() < 0) \
		return ERR; \
	else if (nlines == 0) { \
		sprintf(errmsg, "destination expected"); \
		return ERR; \
	} else if (line2 < 0 || lastln < line2) { \
		sprintf(errmsg, "invalid address"); \
		return ERR; \
	} \
	num = line2; \
	line1 = ol1, line2 = ol2; \
}
#else	/* BACKWARDS */
/* GETLINE3: get a legal address from the command buffer */
#define GETLINE3(num) \
{ \
	long ol1, ol2; \
\
	ol1 = line1, ol2 = line2; \
	if (getlist() < 0) \
		return ERR; \
	if (line2 < 0 || lastln < line2) { \
		sprintf(errmsg, "invalid address"); \
		return ERR; \
	} \
	num = line2; \
	line1 = ol1, line2 = ol2; \
}
#endif

/* sgflags */
#define SGG 001		/* complement previous global substitute suffix */
#define SGP 002		/* complement previous print suffix */
#define SGR 004		/* use last regex instead of last pat */
#define SGF 010		/* newline found */

long ucurln = -1;	/* if >= 0, undo enabled */
long ulastln = -1;	/* if >= 0, undo enabled */
int patlock = 0;	/* if set, pattern not released by optpat() */

long rows = 22;		/* scroll length: ws_row - 2 */

/* docmd: execute the next command in command buffer; return print
   request, if any */
docmd(glob)
	int glob;
{
	static pattern_t *pat = NULL;
	static int sgflag = 0;

	pattern_t *tpat;
	char *fnp;
	int gflag = 0;
	int sflags = 0;
	long num = 0;
	int n = 0;
	int c;

	skipblanks();
	switch(c = *ibufp++) {
	case 'a':
		VRFYCMD();
		if (!glob) ureset();
		if (append(line2, glob) < 0)
			return ERR;
		break;
	case 'c':
		if (ckrange(curln, curln) < 0)
			return ERR;
		VRFYCMD();
		if (!glob) ureset();
		if (lndelete(line1, line2) < 0 || append(curln, glob) < 0)
			return ERR;
		break;
	case 'd':
		if (ckrange(curln, curln) < 0)
			return ERR;
		VRFYCMD();
		if (!glob) ureset();
		if (lndelete(line1, line2) < 0)
			return ERR;
		else if (nextln(curln, lastln) != 0)
			curln = nextln(curln, lastln);
		modified = 1;
		break;
	case 'e':
		if (modified && !scripted)
			return EMOD;
		/* fall through */
	case 'E':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		} else if (!isspace(*ibufp)) {
			sprintf(errmsg, "unexpected command suffix");
			return ERR;
		} else if ((fnp = getfn()) == NULL)
			return ERR;
		VRFYCMD();
		if (lndelete(1, lastln) < 0)
			return ERR;
		ureset();
		if (sbclose() < 0)
			return ERR;
		else if (sbopen() < 0)
			return FATAL;
		if (*fnp && *fnp != '!') strcpy(dfn, fnp);
#ifdef BACKWARDS
		if (*fnp == '\0' && *dfn == '\0') {
			sprintf(errmsg, "no current filename");
			return ERR;
		}
#endif
		if (doread(0, *fnp ? fnp : dfn) < 0)
			return ERR;
		ureset();
		modified = 0;
		ucurln = ulastln = -1;
		break;
	case 'f':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		} else if (!isspace(*ibufp)) {
			sprintf(errmsg, "unexpected command suffix");
			return ERR;
		} else if ((fnp = getfn()) == NULL)
			return ERR;
		else if (*fnp == '!') {
			sprintf(errmsg, "invalid redirection");
			return ERR;
		}
		VRFYCMD();
		if (*fnp) strcpy(dfn, fnp);
		printf("%s\n", esctos(dfn));
		break;
	case 'g':
	case 'G':
		sprintf(errmsg, "cannot nest global commands");
		return ERR;
	case 'h':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
		VRFYCMD();
		if (*errmsg) fprintf(stderr, "%s\n", errmsg);
		break;
	case 'H':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
		VRFYCMD();
		if ((garrulous = 1 - garrulous) && *errmsg)
			fprintf(stderr, "%s\n", errmsg);
		break;
	case 'i':
		if (line2 == 0) {
			sprintf(errmsg, "invalid address");
			return ERR;
		}
		VRFYCMD();
		if (!glob) ureset();
		if (append(prevln(line2, lastln), glob) < 0)
			return ERR;
		break;
	case 'j':
		if (ckrange(curln, curln + 1) < 0)
			return ERR;
		VRFYCMD();
		if (!glob) ureset();
		if (line1 != line2 && join(line1, line2) < 0)
			return ERR;
		break;
	case 'k':
		c = *ibufp++;
		if (line2 == 0) {
			sprintf(errmsg, "invalid address");
			return ERR;
		} 
		VRFYCMD();
		if (putmark(c, getlp(line2)) < 0)
			return ERR;
		break;
	case 'l':
		if (ckrange(curln, curln) < 0)
			return ERR;
		VRFYCMD();
		if (doprint(line1, line2, gflag | GLS) < 0)
			return ERR;
		gflag = 0;
		break;
	case 'm':
		if (ckrange(curln, curln) < 0)
			return ERR;
		GETLINE3(num);
		if (line1 <= num && num < line2) {
			sprintf(errmsg, "invalid destination");
			return ERR;
		}
		VRFYCMD();
		if (!glob) ureset();
		if (move(num, glob) < 0)
			return ERR;
		else
			modified = 1;
		break;
	case 'n':
		if (ckrange(curln, curln) < 0)
			return ERR;
		VRFYCMD();
		if (doprint(line1, line2, gflag | GNP) < 0)
			return ERR;
		gflag = 0;
		break;
	case 'p':
		if (ckrange(curln, curln) < 0)
			return ERR;
		VRFYCMD();
		if (doprint(line1, line2, gflag | GPR) < 0)
			return ERR;
		gflag = 0;
		break;
	case 'P':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
		VRFYCMD();
		prompt = prompt ? NULL : optarg ? optarg : dps;
		break;
	case 'q':
	case 'Q':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
		VRFYCMD();
		gflag =  (modified && !scripted && c == 'q') ? EMOD : EOF;
		break;
	case 'r':
		if (!isspace(*ibufp)) {
			sprintf(errmsg, "unexpected command suffix");
			return ERR;
		} else if (nlines == 0)
			line2 = lastln;
		if ((fnp = getfn()) == NULL)
			return ERR;
		VRFYCMD();
		if (!glob) ureset();
		if (*dfn == '\0' && *fnp != '!') strcpy(dfn, fnp);
#ifdef BACKWARDS
		if (*fnp == '\0' && *dfn == '\0') {
			sprintf(errmsg, "no current filename");
			return ERR;
		}
#endif
		if ((num = doread(line2, *fnp ? fnp : dfn)) < 0)
			return ERR;
		else if (num && num != lastln)
			modified = 1;
		break;
	case 's':
		do {
			switch(*ibufp) {
			case '\n':
				sflags |=SGF;
				break;
			case 'g':
				sflags |= SGG;
				ibufp++;
				break;
			case 'p':
				sflags |= SGP;
				ibufp++;
				break;
			case 'r':
				sflags |= SGR;
				ibufp++;
				break;
			default:
				if (sflags) {
					sprintf(errmsg, "invalid command suffix");
					return ERR;
				}
			}
		} while (sflags && *ibufp != '\n');
		if (sflags && !pat) {
			sprintf(errmsg, "no previous substitution");
			return ERR;
		} else if (!(sflags & SGF))
			sgflag &= 0xff;
		if (*ibufp != '\n' && *(ibufp + 1) == '\n') {
			sprintf(errmsg, "invalid pattern delimiter");
			return ERR;
		}
		tpat = pat;
		spl1();
		if ((!sflags || (sflags & SGR))
		 && (tpat = optpat()) == NULL)
			return ERR;
		else if (tpat != pat) {
			if (pat) {
				 regfree(pat);
				 free(pat);
			 }
			pat = tpat;
			patlock = 1;		/* reserve pattern */
		} else if (pat == NULL) {
			/* NOTREACHED */
			sprintf(errmsg, "no previous substitution");
			return ERR;
		}
		spl0();
		if (!sflags && (sgflag = getrhs(glob)) < 0)
			return ERR;
		else if (glob)
			sgflag |= GLB;
		else
			sgflag &= ~GLB;
		if (sflags & SGG)
			sgflag ^= GSG;
		if (sflags & SGP)
			sgflag ^= GPR, sgflag &= ~(GLS | GNP);
		do {
			switch(*ibufp) {
			case 'p':
				sgflag |= GPR, ibufp++;
				break;
			case 'l':
				sgflag |= GLS, ibufp++;
				break;
			case 'n':
				sgflag |= GNP, ibufp++;
				break;
			default:
				n++;
			}
		} while (!n);
		if (ckrange(curln, curln) < 0)
			return ERR;
		VRFYCMD();
		if (!glob) ureset();
		if ((n = subst(pat, sgflag)) < 0)
			return ERR;
		else if (n)
			modified = 1;
		break;
	case 't':
		if (ckrange(curln, curln) < 0)
			return ERR;
		GETLINE3(num);
		VRFYCMD();
		if (!glob) ureset();
		if (transfer(num) < 0)
			return ERR;
		modified = 1;
		break;
	case 'u':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
		VRFYCMD();
		if (undo(glob) < 0)
			return ERR;
		break;
	case 'v':
	case 'V':
		sprintf(errmsg, "cannot nest global commands");
		return ERR;
	case 'w':
	case 'W':
		if ((n = *ibufp) == 'q' || n == 'Q') {
			gflag = EOF;
			ibufp++;
		}
		if (!isspace(*ibufp)) {
			sprintf(errmsg, "unexpected command suffix");
			return ERR;
		} else if ((fnp = getfn()) == NULL)
			return ERR;
		if (nlines == 0 && !lastln)
			line1 = line2 = 0;
		else if (ckrange(1, lastln) < 0)
			return ERR;
		VRFYCMD();
		if (*dfn == '\0' && *fnp != '!') strcpy(dfn, fnp);
#ifdef BACKWARDS
		if (*fnp == '\0' && *dfn == '\0') {
			sprintf(errmsg, "no current filename");
			return ERR;
		}
#endif
		if ((num = dowrite(line1, line2, *fnp ? fnp : dfn, (c == 'W') ? "a" : "w")) < 0)
			return ERR;
		else if (num == lastln)
			modified = 0;
		else if (modified && !scripted && n == 'q')
			gflag = EMOD;
		break;
	case 'x':
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
		VRFYCMD();
#ifdef DES
		des = getkey();
#else
		sprintf(errmsg, "crypt unavailable");
		return ERR;
#endif
		break;
	case 'z':
#ifdef BACKWARDS
		if (ckrange(line1 = 1, curln + 1) < 0)
#else
		if (ckrange(line1 = 1, curln + !glob) < 0)
#endif
			return ERR;
		else if ('0' < *ibufp && *ibufp <= '9')
			rows = strtol(ibufp, &ibufp, 10);
		VRFYCMD();
		if (doprint(line2, min(lastln, line2 + rows - 1), gflag) < 0)
			return ERR;
		gflag = 0;
		break;
	case '=':
		VRFYCMD();
		printf("%d\n", nlines ? line2 : lastln);
		break;
	case '!':
#ifndef VI_BANG
		if (nlines > 0) {
			sprintf(errmsg, "unexpected address");
			return ERR;
		}
#endif
		if ((sflags = getshcmd()) < 0)
			return ERR;
		VRFYCMD();
		if (sflags) printf("%s\n", shcmd + 1);
#ifdef VI_BANG
		if (nlines == 0) {
#endif
			system(shcmd + 1);
			if (!scripted) printf("!\n");
			break;
#ifdef VI_BANG
		}
		if (!lastln && !line1 && !line2) {
			if (!glob) ureset();
		} else if (ckrange(curln, curln) < 0)
			return ERR;
		else {
			if (!glob) ureset();
			if (lndelete(line1, line2) < 0)
				return ERR;
			line2 = curln;
			modified = 1;
		}
		if ((num = doread(line2, shcmd)) < 0)
			return ERR;
		else if (num && num != lastln)
			modified = 1;
		break;
#endif
	case '\n':
#ifdef BACKWARDS
		if (ckrange(line1 = 1, curln + 1) < 0
#else
		if (ckrange(line1 = 1, curln + !glob) < 0
#endif
		 || doprint(line2, line2, 0) < 0)
			return ERR;
		break;
	default:
		sprintf(errmsg, "unknown command");
		return ERR;
	}
	return gflag;
}


/* ckrange: return status of line number range check */
ckrange(def1, def2)
	long def1, def2;
{
	if (nlines == 0) {
		line1 = def1;
		line2 = def2;
	}
	if (line1 > line2 || 1 > line1 || line2 > lastln) {
		sprintf(errmsg, "invalid address");
		return ERR;
	}
	return 0;
}


/* patscan: return the number of the next line matching a pattern in a
   given direction.  wrap around begin/end of line queue if necessary */
long
patscan(pat, dir)
	pattern_t *pat;
	int dir;
{
	char *s;
	long n = curln;
	line_t *lp;

	do {
		if (n = dir ? nextln(n, lastln) : prevln(n, lastln)) {
			if ((s = gettxt(lp = getlp(n))) == NULL)
				return ERR;
			if (isbinary)
				s = nultonl(s, lp->len & ~ACTV);
			if (!regexec(pat, s, 0, NULL, 0))
				return n;
		}
	} while (n != curln);
	sprintf(errmsg, "no match");
	return  ERR;
}


/* getfn: return pointer to copy of filename in the command buffer */
char *
getfn()
{
	static char *file = NULL;
	static int filesz = 0;

	int n;

	if (*ibufp != '\n') {
		skipblanks();
		if (*ibufp == '\n') {
			sprintf(errmsg, "invalid filename");
			return NULL;
		} else if ((ibufp = getcmdv(&n, 1)) == NULL)
			return NULL;
#ifdef VI_BANG
		else if (*ibufp == '!') {
			ibufp++;
			if ((n = getshcmd()) < 0)
				return NULL;
			if (n) printf("%s\n", shcmd + 1);
			return shcmd;
		}
#endif
		else if (n - 1 > MAXFNAME) {
			sprintf(errmsg, "filename too long");
			return  NULL;
		}
	}
#ifndef BACKWARDS
	else if (*dfn == '\0') {
		sprintf(errmsg, "no current filename");
		return  NULL;
	}
#endif
	CKBUF(file, filesz, MAXFNAME + 1, NULL);
	for (n = 0; *ibufp != '\n';)
		file[n++] = *ibufp++;
	file[n] = '\0';
	return ckfn(file);
}


/* getrhs: extract substitution template from the command buffer */
getrhs(glob)
	int glob;
{
	char delim;

	if ((delim = *ibufp) == '\n') {
		rhbufi = 0;
		return GPR;
	} else if (makesub(glob) == NULL)
		return  ERR;
	else if (*ibufp == '\n')
		return GPR;
	else if (*ibufp == delim)
		ibufp++;
	if ('1' <= *ibufp && *ibufp <= '9')
		return (int) strtol(ibufp, &ibufp, 10) << 8;
	else if (*ibufp == 'g') {
		ibufp++;
		return GSG;
	}
	return  0;
}


/* makesub: return pointer to copy of substitution template in the command
   buffer */
char *
makesub(glob)
	int glob;
{
	int n = 0;
	int i = 0;
	char delim = *ibufp++;
	char c;

	if (*ibufp == '%' && *(ibufp + 1) == delim) {
		ibufp++;
		if (!rhbuf) sprintf(errmsg, "no previous substitution");
		return rhbuf;
	}
	while (*ibufp != delim) {
		CKBUF(rhbuf, rhbufsz, i + 2, NULL);
		if ((c = rhbuf[i++] = *ibufp++) == '\n' && *ibufp == '\0') {
			i--, ibufp--;
			break;
		} else if (c != '\\')
			;
		else if ((rhbuf[i++] = *ibufp++) != '\n')
			;
		else if (!glob) {
			while ((n = getline()) == 0
			    || n > 0 && ibuf[n - 1] != '\n')
				clearerr(stdin);
			if (n < 0)
				return NULL;
		} else
			/*NOTREACHED*/
			;
	}
	CKBUF(rhbuf, rhbufsz, i + 1, NULL);
	rhbuf[rhbufi = i] = '\0';
	return  rhbuf;
}


/* getshcmd: read a shell command up a maximum size from stdin; return
   substitution status */
int
getshcmd()
{
	static char *buf = NULL;
	static int n = 0;

	char *s;			/* substitution char pointer */
	int i = 0;
	int j = 0;

	if (red) {
		sprintf(errmsg, "shell access restricted");
		return ERR;
	} else if ((s = ibufp = getcmdv(&j, 1)) == NULL)
		return ERR;
	CKBUF(buf, n, j + 1, ERR);
	buf[i++] = '!';			/* prefix command w/ bang */
	while (*ibufp != '\n')
		switch (*ibufp) {
		default:
			CKBUF(buf, n, i + 2, ERR);
			buf[i++] = *ibufp;
			if (*ibufp++ == '\\')
				buf[i++] = *ibufp++;
			break;
		case '!':
			if (s != ibufp) {
				CKBUF(buf, n, i + 1, ERR);
				buf[i++] = *ibufp++;
			}
#ifdef BACKWARDS
			else if (shcmd == NULL || *(shcmd + 1) == '\0')
#else
			else if (shcmd == NULL)
#endif
			{
				sprintf(errmsg, "no previous command");
				return ERR;
			} else {
				CKBUF(buf, n, i + shcmdi, ERR);
				for (s = shcmd + 1; s < shcmd + shcmdi;)
					buf[i++] = *s++;
				s = ibufp++;
			}
			break;
		case '%':
			if (*dfn  == '\0') {
				sprintf(errmsg, "no current filename");
				return ERR;
			}
			j = strlen(s = esctos(dfn));
			CKBUF(buf, n, i + j, ERR);
			while (j--)
				buf[i++] = *s++;
			s = ibufp++;
			break;
		}
	CKBUF(shcmd, shcmdsz, i + 1, ERR);
	memcpy(shcmd, buf, i);
	shcmd[shcmdi = i] = '\0';
	return *s == '!' || *s == '%';
}


/* append: insert text from stdin to after line n; stop when either a
   single period is read or EOF; return status */
append(n, glob)
	long n;
	int  glob;
{
	int l;
	char *lp = ibuf;
	char *eot;
	undo_t *up = NULL;

	for (curln = n;;) {
		if (!glob) {
			if ((l = getline()) < 0)
				return ERR;
			else if (l == 0 || ibuf[l - 1] != '\n') {
				clearerr(stdin);
				return  l ? EOF : 0;
			}
			lp = ibuf;
		} else if (*(lp = ibufp) == '\0')
			return 0;
		else {
			while (*ibufp++ != '\n')
				;
			l = ibufp - lp;
		}
		if (l == 2 && lp[0] == '.' && lp[1] == '\n') {
			return 0;
		}
		eot = lp + l;
		spl1();
		do {
			if ((lp = puttxt(lp)) == NULL) {
				spl0();
				return ERR;
			} else if (up)
				up->t = getlp(curln);
			else if ((up = upush(UADD, curln, curln)) == NULL) {
				spl0();
				return ERR;
			}
		} while (lp != eot);
		spl0();
		modified = 1;
	}
}


/* subst: change all text matching a pattern in a range of lines according to
   a substitution template; return status  */
subst(pat, gflag)
	pattern_t *pat;
	int gflag;
{
	undo_t *up;
	char *txt;
	char *eot;
	long lc;
	int nsubs = 0;
	line_t *lp;
	int len;

	curln = prevln(line1, lastln);
	for (lc = 0; lc <= line2 - line1; lc++) {
		lp = getlp(curln = nextln(curln, lastln));
		if ((len = regsub(pat, lp, gflag)) < 0)
			return ERR;
		else if (len) {
			up = NULL;
			if (lndelete(curln, curln) < 0)
				return ERR;
			txt = rbuf;
			eot = rbuf + len;
			spl1();
			do {
				if ((txt = puttxt(txt)) == NULL) {
					spl0();
					return ERR;
				} else if (up)
					up->t = getlp(curln);
				else if ((up = upush(UADD, curln, curln)) == NULL) {
					spl0();
					return ERR;
				}
			} while (txt != eot);
			spl0();
			nsubs++;
		}
	}
	if  (nsubs == 0 && !(gflag & GLB)) {
		sprintf(errmsg, "no match");
		return ERR;
	} else if ((gflag & (GPR | GLS | GNP))
	 && doprint(curln, curln, gflag) < 0)
		return ERR;
	return 1;
}


/* regsub: replace text matched by a pattern according to a substitution
   template; return pointer to the modified text */
regsub(pat, lp, gflag)
	pattern_t *pat;
	line_t *lp;
	int gflag;
{
	int off = 0;
	int kth = gflag >> 8;		/* substitute kth match only */
	int chngd = 0;
	int matchno = 0;
	int len;
	int i = 0;
	regmatch_t rm[SE_MAX];
	char *txt;
	char *eot;

	if ((txt = gettxt(lp)) == NULL)
		return ERR;
	len = lp->len & ~ACTV;
	eot = txt + len;
	if (isbinary) txt = nultonl(txt, len);
	if (!regexec(pat, txt, SE_MAX, rm, 0)) {
		do {
			if (!kth || kth == ++matchno) {
				chngd++;
				i = rm[0].rm_so;
				CKBUF(rbuf, rbufsz, off + i, ERR);
				if (isbinary) txt = nltonul(txt, rm[0].rm_eo);
				memcpy(rbuf + off, txt, i);
				if ((off = catsub(txt, rm, off += i)) < 0)
					return ERR;
			} else {
				i = rm[0].rm_eo;
				CKBUF(rbuf, rbufsz, off + i, ERR);
				if (isbinary) txt = nltonul(txt, i);
				memcpy(rbuf + off, txt, i);
				off += i;
			}
			txt += rm[0].rm_eo;
		} while (*txt && (!chngd || (gflag & GSG) && rm[0].rm_eo)
		      && !regexec(pat, txt, SE_MAX, rm, REG_NOTBOL));
		i = eot - txt;
		CKBUF(rbuf, rbufsz, off + i + 2, ERR);
		if (i > 0 && !rm[0].rm_eo && (gflag & GSG)) {
			sprintf(errmsg, "infinite substitution loop");
			return  ERR;
		}
		if (isbinary) txt = nltonul(txt, i);
		memcpy(rbuf + off, txt, i);
		memcpy(rbuf + off + i, "\n", 2);
	}
	return chngd ? off + i + 1 : 0;
}


/* join: replace a range of lines with the joined text of those lines */
join(from, to)
	long from;
	long to;
{
	static char *buf = NULL;
	static int n;

	char *s;
	int len = 0;
	int size = 0;
	line_t *bp, *ep;

	ep = getlp(nextln(to, lastln));
	for (bp = getlp(from); bp != ep; bp = bp->next, size += len) {
		if ((s = gettxt(bp)) == NULL)
			return ERR;
		len = bp->len & ~ACTV;
		CKBUF(buf, n, size + len, ERR);
		memcpy(buf + size, s, len);
	}
	CKBUF(buf, n, size + 2, ERR);
	memcpy(buf + size, "\n", 2);
	if (lndelete(from, to) < 0)
		return ERR;
	curln = from - 1;
	spl1();
	if (puttxt(buf) == NULL
	 || upush(UADD, curln, curln) == NULL) {
		spl0();
		return ERR;
	}
	spl0();
	modified = 1;
	return 0;
}


/* move: move a range of lines */
move(num, glob)
	long num;
	int glob;
{
	line_t *b1, *a1, *b2, *a2, *lp;
	long n = nextln(line2, lastln);
	long p = prevln(line1, lastln);
	int done = (num == line1 - 1 || num == line2);

	spl1();
	if (done) {
		a2 = getlp(n);
		b2 = getlp(p);
		curln = line2;
	} else if (upush(UMOV, p, n) == NULL
	 || upush(UMOV, num, nextln(num, lastln)) == NULL) {
		spl0();
		return ERR;
	} else {
		a1 = getlp(n);
		if (num < line1)
			b1 = getlp(p), b2 = getlp(num);	/* this getlp last! */
		else	b2 = getlp(num), b1 = getlp(p);	/* this getlp last! */
		a2 = b2->next;
		requeue(b2, b1->next);
		requeue(a1->prev, a2);
		requeue(b1, a1);
		curln = num + ((num < line1) ? line2 - line1 + 1 : 0);
	}
	if (glob)
		for (lp = b2->next; lp != a2; lp = lp->next)
			lp->len &= ~ACTV;		/* zero ACTV  bit */
	spl0();
	return 0;
}


/* transfer: copy a range of lines; return status */
transfer(num)
	long num;
{
	line_t *lp;
	long nl, nt, lc;
	long mid = (num < line2) ? num : line2;
	undo_t *up = NULL;

	curln = num;
	for (nt = 0, nl = line1; nl <= mid; nl++, nt++) {
		spl1();
		if ((lp = lpdup(getlp(nl))) == NULL) {
			spl0();
			return ERR;
		}
		lpqueue(lp);
		if (up)
			up->t = lp;
		else if ((up = upush(UADD, curln, curln)) == NULL) {
			spl0();
			return ERR;
		}
		spl0();
	}
	for (nl += nt, lc = line2 + nt; nl <= lc; nl += 2, lc++) {
		spl1();
		if ((lp = lpdup(getlp(nl))) == NULL) {
			spl0();
			return ERR;
		}
		lpqueue(lp);
		if (up)
			up->t = lp;
		else if ((up = upush(UADD, curln, curln)) == NULL) {
			spl0();
			return ERR;
		}
		spl0();
	}
	return 0;
}


/* lndelete: delete a range of lines */
lndelete(from, to)
	long from, to;
{
	line_t *before, *after;

	spl1();
	if (upush(UDEL, from, to) == NULL) {
		spl0();
		return ERR;
	}
	after = getlp(nextln(to, lastln));
	before = getlp(prevln(from, lastln));		/* this getlp last! */
	requeue(before, after);
	lastln -= to - from + 1;
	curln = prevln(from, lastln);
	spl0();
	return 0;
}


/* catsub: modify text according to a substitution template;
   return offset to end of modified text */
catsub(boln, rm, off)
	char *boln;
	regmatch_t *rm;
	int off;
{
	int j = 0;
	int k = 0;
	char *sub = rhbuf;

	for (; sub - rhbuf < rhbufi; sub++)
		if (*sub == '&') {
			j = rm[0].rm_so;
			k = rm[0].rm_eo;
			CKBUF(rbuf, rbufsz, off + k - j, ERR);
			while (j < k)
				rbuf[off++] = boln[j++];
		} else if (*sub == '\\' && '1' <= *++sub && *sub <= '9'
		      && rm[*sub - '0'].rm_so >= 0
		      && rm[*sub - '0'].rm_eo >= 0) {
			j = rm[*sub - '0'].rm_so;
			k = rm[*sub - '0'].rm_eo;
			CKBUF(rbuf, rbufsz, off + k - j, ERR);
			while (j < k)
				rbuf[off++] = boln[j++];
		} else {
			CKBUF(rbuf, rbufsz, off + 1, ERR);
			rbuf[off++] = *sub;
		}
	CKBUF(rbuf, rbufsz, off + 1, ERR);
	rbuf[off] = '\0';
	return off;
}

/* doprint: print a range of lines to stdout */
doprint(from, to, gflag)
	long from;
	long to;
	int gflag;
{
	line_t *bp;
	line_t *ep;
	char *s;

	if (!from) {
		sprintf(errmsg, "invalid address");
		return ERR;
	}
	ep = getlp(nextln(to, lastln));
	for (bp = getlp(from); bp != ep; bp = bp->next) {
		if ((s = gettxt(bp)) == NULL)
			return ERR;
		putstr(s, bp->len & ~ACTV, curln = from++, gflag);
	}
	return 0;
}


int cols = 72;		/* wrap column: ws_col - 8 */

/* putstr: print text to stdout */
void
putstr(s, l, n, gflag)
	char *s;
	int l;
	long n;
	int gflag;
{
	int col = 0;

	if (gflag & GNP) {
		printf("%ld\t", n);
		col = 8;
	}
	for (; l--; s++) {
		if ((gflag & GLS) && ++col > cols) {
			fputs("\\\n", stdout);
			col = 1;
		}
		if (gflag & GLS) {
			switch (*s) {
			case '\b':
				fputs("\\b", stdout);
				break;
			case '\f':
				fputs("\\f", stdout);
				break;
			case '\n':
				fputs("\\n", stdout);
				break;
			case '\r':
				fputs("\\r", stdout);
				break;
			case '\t':
				fputs("\\t", stdout);
				break;
			case '\v':
				fputs("\\v", stdout);
				break;
			default:
				if (*s < 32 || 126 < *s) {
					putchar('\\');
					putchar((((unsigned char) *s & 0300) >> 6) + '0');
					putchar((((unsigned char) *s & 070) >> 3) + '0');
					putchar(((unsigned char) *s & 07) + '0');
					col += 2;
				} else if (*s == '\\')
					fputs("\\\\", stdout);
				else {
					putchar(*s);
					col--;
				}
			}
			col++;
		} else
			putchar(*s);
	}
#ifndef BACKWARDS
	if (gflag & GLS)
		putchar('$');
#endif
	putchar('\n');
}


int newline_added;		/* set if newline appended to input file */

/* doread: read a text file into the editor buffer; return line count */
long
doread(n, fn)
	long n;
	char *fn;
{
	FILE *fp;
	line_t *lp = getlp(n);
	unsigned long size = 0;
	undo_t *up = NULL;
	int len;

	isbinary = newline_added = 0;
	if ((fp = (*fn == '!') ? popen(fn + 1, "r") : fopen(esctos(fn), "r")) == NULL) {
		fprintf(stderr, "%s: %s\n", fn, strerror(errno));
		sprintf(errmsg, "cannot open input file");
		return ERR;
	} else if (des)
		desinit();
	for (curln = n; (len = sgetline(fp)) > 0; size += len) {
		spl1();
		if (puttxt(sbuf) == NULL) {
			spl0();
			return ERR;
		}
		lp = lp->next;
		if (up)
			up->t = lp;
		else if ((up = upush(UADD, curln, curln)) == NULL) {
			spl0();
			return ERR;
		}
		spl0();
	}
	if (((*fn == '!') ?  pclose(fp) : fclose(fp)) < 0) {
		fprintf(stderr, "%s: %s\n", fn, strerror(errno));
		sprintf(errmsg, "cannot close input file");
		return ERR;
	}
	if (newline_added && !isbinary)
		fputs("newline appended\n", stderr);
	if (des) size += 8 - size % 8;
	fprintf(stderr, !scripted ? "%lu\n" : "", size);
	return  (len < 0) ? ERR : curln - n;
}


/* dowrite: write the text of a range of lines to a file; return line count */
long
dowrite(n, m, fn, mode)
	long n;
	long m;
	char *fn;
	char *mode;
{
	FILE *fp;
	line_t *lp;
	unsigned long size = 0;
	long lc = n ? m - n + 1 : 0;
	char *s = NULL;
	int len;
	int ct;

	if ((fp = ((*fn == '!') ? popen(fn + 1, "w") : fopen(esctos(fn), mode))) == NULL) {
		fprintf(stderr, "%s: %s\n", fn, strerror(errno));
		sprintf(errmsg, "cannot open output file");
		return ERR;
	} else if (des)
		desinit();
	if (n && !des)
		for (lp = getlp(n); n <= m; n++, lp = lp->next) {
			if ((s = gettxt(lp)) == NULL)
				return ERR;
			len = lp->len & ~ACTV;
			if (n != lastln || !isbinary || !newline_added)
				s[len++] = '\n';
			if ((ct = fwrite(s, sizeof(char), len, fp)) < 0 || ct != len) {
				fprintf(stderr, "%s: %s\n", fn, strerror(errno));
				sprintf(errmsg, "cannot write file");
				return ERR;
			}
			size += len;
		}
	else if (n)
		for (lp = getlp(n); n <= m; n++, lp = lp->next) {
			if ((s = gettxt(lp)) == NULL)
				return ERR;
			len = lp->len & ~ACTV;
			while (len--) {
				if (desputc(*s++, fp) == EOF && ferror(fp)) {
					fprintf(stderr, "%s: %s\n", fn, strerror(errno));
					sprintf(errmsg, "cannot write file");
					return ERR;
				}
			}
			if (n != lastln || !isbinary || !newline_added) {
				if (desputc('\n', fp) < 0) {
					fprintf(stderr, "%s: %s\n", fn, strerror(errno));
					sprintf(errmsg, "cannot write file");
					return ERR;
				}
				size++;			/* for '\n' */
			}
			size += (lp->len & ~ACTV);
		}
	if (des) {
		desflush(fp);				/* flush buffer */
		size += 8 - size % 8;			/* adjust DES size */
	}
	if (((*fn == '!') ?  pclose(fp) : fclose(fp)) < 0) {
		fprintf(stderr, "%s: %s\n", fn, strerror(errno));
		sprintf(errmsg, "cannot close output file");
		return ERR;
	}
	fprintf(stderr, !scripted ? "%lu\n" : "", size);
	return  lc;
}


#define USIZE 100				/* undo stack size */
undo_t *ustack = NULL;				/* undo stack */
long usize = 0;					/* stack size variable */
long u_p = 0;					/* undo stack pointer */

/* upush: return pointer to intialized undo node */
undo_t *
upush(type, from, to)
	int type;
	long from;
	long to;
{
	undo_t *t;

#if defined(sun) || defined(NO_REALLOC_NULL)
	if (ustack == NULL
	 && (ustack = (undo_t *) malloc((usize = USIZE) * sizeof(undo_t))) == NULL) {
		fprintf(stderr, "%s\n", strerror(errno));
		sprintf(errmsg, "out of memory");
		return NULL;
	}
#endif
	t = ustack;
	if (u_p < usize
	 || (t = (undo_t *) realloc(ustack, (usize += USIZE) * sizeof(undo_t))) != NULL) {
		ustack = t;
		ustack[u_p].type = type;
		ustack[u_p].t = getlp(to);
		ustack[u_p].h = getlp(from);
		return ustack + u_p++;
	}
	/* out of memory - release undo stack */
	fprintf(stderr, "%s\n", strerror(errno));
	sprintf(errmsg, "out of memory");
	ureset();
	free(ustack);
	ustack = NULL;
	usize = 0;
	return NULL;
}


/* USWAP: swap undo nodes */
#define USWAP(x,y) { \
	undo_t utmp; \
	utmp = x, x = y, y = utmp; \
}


/* undo: undo last change to the editor buffer */
undo(glob)
	int glob;
{
	long n;
	long ocurln = curln;
	long olastln = lastln;
	line_t *lp, *np;

	if (ucurln == -1 || ulastln == -1) {
		sprintf(errmsg, "nothing to undo");
		return ERR;
	} else if (u_p)
		modified = 1;
	getlp(0);				/* this getlp last! */
	spl1();
	for (n = u_p; n-- > 0;) {
		switch(ustack[n].type) {
		case UADD:
			requeue(ustack[n].h->prev, ustack[n].t->next);
			break;
		case UDEL:
			requeue(ustack[n].h->prev, ustack[n].h);
			requeue(ustack[n].t, ustack[n].t->next);
			break;
		case UMOV:
		case VMOV:
			requeue(ustack[n - 1].h, ustack[n].h->next);
			requeue(ustack[n].t->prev, ustack[n - 1].t);
			requeue(ustack[n].h, ustack[n].t);
			n--;
			break;
		default:
			/*NOTREACHED*/
			;
		}
		ustack[n].type ^= 1;
	}
	/* reverse undo order */
	for (n = u_p; n-- > (u_p + 1)/ 2;)
		USWAP(ustack[n], ustack[u_p - 1 - n]);
	if (glob)
		for (lp = np = getlp(0); (lp = lp->next) != np;)
			lp->len &= ~ACTV;		/* zero ACTV bit */
	curln = ucurln, ucurln = ocurln;
	lastln = ulastln, ulastln = olastln;
	spl0();
	return 0;
}


/* ureset: clear the undo stack */
void
ureset()
{
	line_t *lp, *ep, *tl;

	while (u_p--)
		if (ustack[u_p].type == UDEL) {
			ep = ustack[u_p].t->next;
			for (lp = ustack[u_p].h; lp != ep; lp = tl) {
				clrmark(lp);
				tl = lp->next;
				free(lp);
			}
		}
	u_p = 0;
	ucurln = curln;
	ulastln = lastln;
}


#define MAXMARK 26			/* max number of marks */

line_t	*mark[MAXMARK];			/* line markers */
int markno;				/* line marker count */

/* getmark: return address of a marked line */
long
getmark(n)
	int n;
{ 	
	if (!islower(n)) {
		sprintf(errmsg, "invalid mark character");
		return ERR;
	}
	return getaddr(mark[n - 'a']);
}


/* putmark: set a line node mark */
int
putmark(n, lp)
	int n;
	line_t *lp;
{
	if (!islower(n)) {
		sprintf(errmsg, "invalid mark character");
		return ERR;
	} else if (mark[n - 'a'] == NULL)
		markno++;
	mark[n - 'a'] = lp;
	return 0;
}


/* clrmark: clear line node marks */
void
clrmark(lp)
	line_t *lp;
{
	int i;

	if (markno)
		for (i = 0; i < MAXMARK; i++)
			if (mark[i] == lp) {
				mark[i] = NULL;
				markno--;
			}
}


/* sgetline: read a line of text up a maximum size from a file; return
   line length */
sgetline(fp)
	FILE *fp;
{
	register int c;
	register int i = 0;

	while (((c = des ? desgetc(fp) : getc(fp)) != EOF || !feof(fp) && !ferror(fp)) && c != '\n') {
		CKBUF(sbuf, sbufsz, i + 1, ERR);
		if (!(sbuf[i++] = c)) isbinary = 1;
	}
	CKBUF(sbuf, sbufsz, i + 2, ERR);
	if (c == '\n')
		sbuf[i++] = c;
	else if (feof(fp) && i) {
		sbuf[i++] = '\n';
		newline_added = 1;
	} else if (ferror(fp)) {
		fprintf(stderr, "%s\n", strerror(errno));
		sprintf(errmsg, "cannot read input file");
		return ERR;
	}
	sbuf[i] = '\0';
	return (isbinary && newline_added && i) ? --i : i;
}


/* getline: read a line of text up a maximum size from stdin; return
   line length */
getline()
{
	register int i = 0;
	register int oi = 0;
	char c;

	/* Read one character at a time to avoid i/o contention with shell
	   escapes invoked by nonterminal input, e.g.,
	   ed - <<EOF
	   !cat
	   hello, world
	   EOF */
	for (;;)
		switch (read(0, &c, 1)) {
		default:
			oi = 0;
			CKBUF(ibuf, ibufsz, i + 2, ERR);
			if (!(ibuf[i++] = c)) isbinary = 1;
			if (c != '\n')
				continue;
			lineno++;		/* script line no. */
			ibuf[i] = '\0';
			ibufp = ibuf;
			return i;
		case 0:
			if (i != oi) {
				oi = i;
				continue;
			} else if (i)
				ibuf[i] = '\0';
			ibufp = ibuf;
			return i;
		case -1:
			fprintf(stderr, "%s\n", strerror(errno));
			sprintf(errmsg, "cannot read standard input");
			clearerr(stdin);
			ibufp = NULL;
			return ERR;
		}
}


/* getcmdv: get a command vector */
char *
getcmdv(sizep, nonl)
	int *sizep;
	int nonl;
{
	int l, n;
	char *t = ibufp;

	while (*t++ != '\n')
		;
	if ((l = t - ibufp) < 2 || !oddesc(ibufp, ibufp + l - 1)) {
		*sizep = l;
		return ibufp;
	}
	*sizep = -1;
	CKBUF(cvbuf, cvbufsz, l, NULL);
	memcpy(cvbuf, ibufp, l);
	*(cvbuf + --l - 1) = '\n'; 	/* strip trailing esc */
	if (nonl) l--; 			/* strip newline */
	for (;;) {
		if ((n = getline()) < 0)
			return NULL;
		else if (n == 0 || ibuf[n - 1] != '\n') {
			sprintf(errmsg, "unexpected end-of-file");
			return NULL;
		}
		CKBUF(cvbuf, cvbufsz, l + n, NULL);
		memcpy(cvbuf + l, ibuf, n);
		l += n;
		if (n < 2 || !oddesc(cvbuf, cvbuf + l - 1))
			break;
		*(cvbuf + --l - 1) = '\n'; 	/* strip trailing esc */
		if (nonl) l--; 			/* strip newline */
	}
	CKBUF(cvbuf, cvbufsz, l + 1, NULL);
	cvbuf[l] = '\0';
	*sizep = l;
	return cvbuf;
}


/* lpdup: return a pointer to a copy of a line node */
line_t *
lpdup(lp)
	line_t *lp;
{
	line_t *np;

	if ((np = (line_t *) malloc(sizeof(line_t))) == NULL) {
		fprintf(stderr, "%s\n", strerror(errno));
		sprintf(errmsg, "out of memory");
		return NULL;
	}
	np->seek = lp->seek;
	np->len = (lp->len & ~ACTV);	/* zero ACTV bit */
	return np;
}


/* oddesc:  return the parity of escapes preceding a character in a
   string */
oddesc(s, t)
	char *s;
	char *t;
{
    return (s == t || *(t - 1) != '\\') ? 0 : !oddesc(s, t - 1);
}


/* esctos: return copy of escaped string */
char *
esctos(s)
	char *s;
{
	static char *file = NULL;
	static int filesz = 0;

	int i = 0;

	CKBUF(file, filesz, MAXFNAME + 1, NULL);
	/* assert: no trailing escape */
	while (file[i++] = (*s == '\\') ? *++s : *s)
		s++;
	return file;
}


void
onhup(signo)
	int signo;
{
	if (mutex)
		sigflags |= (1 << signo);
	else	dohup(signo);
}


void
onintr(signo)
	int signo;
{
	if (mutex)
		sigflags |= (1 << signo);
	else	dointr(signo);
}


void
dohup(signo)
	int signo;
{
	char *hup = NULL;		/* hup filename */
	char *s;
	int n;

	if (!sigactive)
		quit(1);
	sigflags &= ~(1 << signo);
	if (lastln && dowrite(1, lastln, "ed.hup", "w") < 0
	 && (s = getenv("HOME")) != NULL
	 && (n = strlen(s)) + 8 <= MAXFNAME	/* "ed.hup" + '/' */
	 && (hup = (char *) malloc(n + 10)) != NULL) {
		strcpy(hup, s);
		if (hup[n - 1] != '/')
			hup[n] = '/', hup[n+1] = '\0';
		strcat(hup, "ed.hup");
		dowrite(1, lastln, hup, "w");
	}
	quit(2);
}


void
dointr(signo)
	int signo;
{
	if (!sigactive)
		quit(1);
	sigflags &= ~(1 << signo);
#ifdef _POSIX_SOURCE
	siglongjmp(env, -1);
#else
	longjmp(env, -1);
#endif
}


struct winsize ws;		/* window size structure */

void
dowinch(signo)
	int signo;
{
	sigflags &= ~(1 << signo);
	if (ioctl(0, TIOCGWINSZ, (char *) &ws) >= 0) {
		if (ws.ws_row > 2) rows = ws.ws_row - 2;
		if (ws.ws_col > 8) cols = ws.ws_col - 8;
	}
}


/* ckfn: return a legal filename */
char *
ckfn(s)
	char *s;
{
	if (red && (*s == '!' || !strcmp(s, "..") || strchr(s, '/'))) {
		sprintf(errmsg, "shell access restricted");
		return NULL;
	}
	return s;
}
