/*	$OpenBSD: main.c,v 1.52 2002/02/16 21:27:48 millert Exp $	*/
/*	$NetBSD: main.c,v 1.12 1997/02/08 23:54:49 cgd Exp $	*/

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: main.c,v 1.52 2002/02/16 21:27:48 millert Exp $";
#endif
#endif /* not lint */

/*
 * main.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <sys/types.h>
#include <assert.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <err.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"
#include "pathnames.h"

ndptr hashtab[HASHSIZE];	/* hash table for macros etc.  */
stae *mstack;		 	/* stack of m4 machine         */
char *sstack;		 	/* shadow stack, for string space extension */
static size_t STACKMAX;		/* current maximum size of stack */
int sp; 			/* current m4  stack pointer   */
int fp; 			/* m4 call frame pointer       */
struct input_file infile[MAXINP];/* input file stack (0=stdin)  */
FILE **outfile;			/* diversion array(0=bitbucket)*/
int maxout;
FILE *active;			/* active output file pointer  */
int ilevel = 0; 		/* input file stack pointer    */
int oindex = 0; 		/* diversion index..	       */
char *null = "";                /* as it says.. just a null..  */
char *m4wraps = "";             /* m4wrap string default..     */
char lquote[MAXCCHARS+1] = {LQUOTE};	/* left quote character  (`)   */
char rquote[MAXCCHARS+1] = {RQUOTE};	/* right quote character (')   */
char scommt[MAXCCHARS+1] = {SCOMMT};	/* start character for comment */
char ecommt[MAXCCHARS+1] = {ECOMMT};	/* end character for comment   */

struct keyblk keywrds[] = {	/* m4 keywords to be installed */
	{ "include",      INCLTYPE },
	{ "sinclude",     SINCTYPE },
	{ "define",       DEFITYPE },
	{ "defn",         DEFNTYPE },
	{ "divert",       DIVRTYPE | NOARGS },
	{ "expr",         EXPRTYPE },
	{ "eval",         EXPRTYPE },
	{ "substr",       SUBSTYPE },
	{ "ifelse",       IFELTYPE },
	{ "ifdef",        IFDFTYPE },
	{ "len",          LENGTYPE },
	{ "incr",         INCRTYPE },
	{ "decr",         DECRTYPE },
	{ "dnl",          DNLNTYPE | NOARGS },
	{ "changequote",  CHNQTYPE | NOARGS },
	{ "changecom",    CHNCTYPE | NOARGS },
	{ "index",        INDXTYPE },
#ifdef EXTENDED
	{ "paste",        PASTTYPE },
	{ "spaste",       SPASTYPE },
    	/* Newer extensions, needed to handle gnu-m4 scripts */
	{ "indir",        INDIRTYPE},
	{ "builtin",      BUILTINTYPE},
	{ "patsubst",	  PATSTYPE},
	{ "regexp",	  REGEXPTYPE},
	{ "esyscmd",	  ESYSCMDTYPE},
	{ "__file__",	  FILENAMETYPE | NOARGS},
	{ "__line__",	  LINETYPE | NOARGS},
#endif
	{ "popdef",       POPDTYPE },
	{ "pushdef",      PUSDTYPE },
	{ "dumpdef",      DUMPTYPE | NOARGS },
	{ "shift",        SHIFTYPE | NOARGS },
	{ "translit",     TRNLTYPE },
	{ "undefine",     UNDFTYPE },
	{ "undivert",     UNDVTYPE | NOARGS },
	{ "divnum",       DIVNTYPE | NOARGS },
	{ "maketemp",     MKTMTYPE },
	{ "errprint",     ERRPTYPE | NOARGS },
	{ "m4wrap",       M4WRTYPE | NOARGS },
	{ "m4exit",       EXITTYPE | NOARGS },
	{ "syscmd",       SYSCTYPE },
	{ "sysval",       SYSVTYPE | NOARGS },
	{ "traceon",	  TRACEONTYPE | NOARGS },
	{ "traceoff",	  TRACEOFFTYPE | NOARGS },

#if defined(unix) || defined(__unix__) 
	{ "unix",         SELFTYPE | NOARGS },
#else
#ifdef vms
	{ "vms",          SELFTYPE | NOARGS },
#endif
#endif
};

#define MAXKEYS	(sizeof(keywrds)/sizeof(struct keyblk))

extern int optind;
extern char *optarg;

#define MAXRECORD 50
static struct position {
	char *name;
	unsigned long line;
} quotes[MAXRECORD], paren[MAXRECORD];

static void record(struct position *, int);
static void dump_stack(struct position *, int);

static void macro(void);
static void initkwds(void);
static ndptr inspect(int, char *);
static int do_look_ahead(int, const char *);

static void enlarge_stack(void);

int main(int, char *[]);

int
main(argc,argv)
	int argc;
	char *argv[];
{
	int c;
	int n;
	char *p;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, onintr);

	initkwds();
	initspaces();
	STACKMAX = INITSTACKMAX;

	mstack = (stae *)xalloc(sizeof(stae) * STACKMAX);
	sstack = (char *)xalloc(STACKMAX);

	maxout = 0;
	outfile = NULL;
	resizedivs(MAXOUT);

	while ((c = getopt(argc, argv, "gt:d:D:U:o:I:")) != -1)
		switch(c) {

		case 'D':               /* define something..*/
			for (p = optarg; *p; p++)
				if (*p == '=')
					break;
			if (*p)
				*p++ = EOS;
			dodefine(optarg, p);
			break;
		case 'I':
			addtoincludepath(optarg);
			break;
		case 'U':               /* undefine...       */
			remhash(optarg, TOP);
			break;
		case 'g':
			mimic_gnu = 1;
			break;
		case 'd':
			set_trace_flags(optarg);
			break;
		case 't':
			mark_traced(optarg, 1);
			break;
		case 'o':
			trace_file(optarg);
                        break;
		case '?':
			usage();
		}

        argc -= optind;
        argv += optind;

	active = stdout;		/* default active output     */
	bbase[0] = bufbase;
        if (!argc) {
 		sp = -1;		/* stack pointer initialized */
		fp = 0; 		/* frame pointer initialized */
		set_input(infile+0, stdin, "stdin");
					/* default input (naturally) */
		macro();
	} else
		for (; argc--; ++argv) {
			p = *argv;
			if (p[0] == '-' && p[1] == EOS)
				set_input(infile, stdin, "stdin");
			else if (fopen_trypath(infile, p) == NULL)
				err(1, "%s", p);
			sp = -1;
			fp = 0; 
			macro();
		    	release_input(infile);
		}

	if (*m4wraps) { 		/* anything for rundown ??   */
		ilevel = 0;		/* in case m4wrap includes.. */
		bufbase = bp = buf;	/* use the entire buffer   */
		pbstr(m4wraps); 	/* user-defined wrapup act   */
		macro();		/* last will and testament   */
	}

	if (active != stdout)
		active = stdout;	/* reset output just in case */
	for (n = 1; n < maxout; n++)	/* default wrap-up: undivert */
		if (outfile[n] != NULL)
			getdiv(n);
					/* remove bitbucket if used  */
	if (outfile[0] != NULL) {
		(void) fclose(outfile[0]);
	}

	return 0;
}

/*
 * Look ahead for `token'.
 * (on input `t == token[0]')
 * Used for comment and quoting delimiters.
 * Returns 1 if `token' present; copied to output.
 *         0 if `token' not found; all characters pushed back
 */
static int
do_look_ahead(t, token)
	int	t;
	const char	*token;
{
	int i;

	assert((unsigned char)t == (unsigned char)token[0]);

	for (i = 1; *++token; i++) {
		t = gpbc();
		if (t == EOF || (unsigned char)t != (unsigned char)*token) {
			putback(t);
			while (--i)
				putback(*--token);
			return 0;
		}
	}
	return 1;
}

#define LOOK_AHEAD(t, token) (t != EOF && 		\
    (unsigned char)(t)==(unsigned char)(token)[0] && 	\
    do_look_ahead(t,token))

/*
 * macro - the work horse..
 */
static void
macro()
{
	char token[MAXTOK+1];
	int t, l;
	ndptr p;
	int  nlpar;

	cycle {
		t = gpbc();
		if (t == '_' || isalpha(t)) {
			p = inspect(t, token);
			if (p != nil)
				putback(l = gpbc());
			if (p == nil || (l != LPAREN && 
			    (p->type & NEEDARGS) != 0))
				outputstr(token);
			else {
		/*
		 * real thing.. First build a call frame:
		 */
				pushf(fp);	/* previous call frm */
				pushf(p->type); /* type of the call  */
				pushf(0);	/* parenthesis level */
				fp = sp;	/* new frame pointer */
		/*
		 * now push the string arguments:
		 */
				pushs1(p->defn);	/* defn string */
				pushs1(p->name);	/* macro name  */
				pushs(ep);	      	/* start next..*/

				if (l != LPAREN && PARLEV == 0)  {   
				    /* no bracks  */
					chrsave(EOS);

					if (sp == STACKMAX)
						errx(1, "internal stack overflow");
					eval((const char **) mstack+fp+1, 2, 
					    CALTYP);

					ep = PREVEP;	/* flush strspace */
					sp = PREVSP;	/* previous sp..  */
					fp = PREVFP;	/* rewind stack...*/
				}
			}
		} else if (t == EOF) {
			if (sp > -1) {
				warnx( "unexpected end of input, unclosed parenthesis:");
				dump_stack(paren, PARLEV);
				exit(1);
			}
			if (ilevel <= 0)
				break;			/* all done thanks.. */
			release_input(infile+ilevel--);
			bufbase = bbase[ilevel];
			continue;
		}
	/*
	 * non-alpha token possibly seen..
	 * [the order of else if .. stmts is important.]
	 */
		else if (LOOK_AHEAD(t,lquote)) {	/* strip quotes */
			nlpar = 0;
			record(quotes, nlpar++);
			/*
			 * Opening quote: scan forward until matching
			 * closing quote has been found.
			 */
			do {

				l = gpbc();
				if (LOOK_AHEAD(l,rquote)) {
					if (--nlpar > 0)
						outputstr(rquote);
				} else if (LOOK_AHEAD(l,lquote)) {
					record(quotes, nlpar++);
					outputstr(lquote);
				} else if (l == EOF) {
					if (nlpar == 1)
						warnx("unclosed quote:");
					else
						warnx("%d unclosed quotes:", nlpar);
					dump_stack(quotes, nlpar);
					exit(1);
				} else {
					if (nlpar > 0) {
						if (sp < 0)
							putc(l, active);
						else
							CHRSAVE(l);
					}
				}
			}
			while (nlpar != 0);
		}

		else if (sp < 0 && LOOK_AHEAD(t, scommt)) {
			fputs(scommt, active);

			for(;;) {
				t = gpbc();
				if (LOOK_AHEAD(t, ecommt)) {
					fputs(ecommt, active);
					break;
				}
				if (t == EOF)
					break;
				putc(t, active);
			}
		}

		else if (sp < 0) {		/* not in a macro at all */
			putc(t, active);	/* output directly..	 */
		}

		else switch(t) {

		case LPAREN:
			if (PARLEV > 0)
				chrsave(t);
			while (isspace(l = gpbc()))
				;		/* skip blank, tab, nl.. */
			putback(l);
			record(paren, PARLEV++);
			break;

		case RPAREN:
			if (--PARLEV > 0)
				chrsave(t);
			else {			/* end of argument list */
				chrsave(EOS);

				if (sp == STACKMAX)
					errx(1, "internal stack overflow");

				eval((const char **) mstack+fp+1, sp-fp, 
				    CALTYP);

				ep = PREVEP;	/* flush strspace */
				sp = PREVSP;	/* previous sp..  */
				fp = PREVFP;	/* rewind stack...*/
			}
			break;

		case COMMA:
			if (PARLEV == 1) {
				chrsave(EOS);		/* new argument   */
				while (isspace(l = gpbc()))
					;
				putback(l);
				pushs(ep);
			} else
				chrsave(t);
			break;

		default:
			if (LOOK_AHEAD(t, scommt)) {
				char *p;
				for (p = scommt; *p; p++)
					chrsave(*p);
				for(;;) {
					t = gpbc();
					if (LOOK_AHEAD(t, ecommt)) {
						for (p = ecommt; *p; p++)
							chrsave(*p);
						break;
					}
					if (t == EOF)
					    break;
					CHRSAVE(t);
				}
			} else
				CHRSAVE(t);		/* stack the char */
			break;
		}
	}
}

/* 
 * output string directly, without pushing it for reparses. 
 */
void
outputstr(s)
	const char *s;
{
	if (sp < 0)
		while (*s)
			putc(*s++, active);
	else
		while (*s)
			CHRSAVE(*s++);
}

/*
 * build an input token..
 * consider only those starting with _ or A-Za-z. This is a
 * combo with lookup to speed things up.
 */
static ndptr
inspect(c, tp) 
	int c;
	char *tp;
{
	char *name = tp;
	char *etp = tp+MAXTOK;
	ndptr p;
	unsigned int h;
	
	h = *tp++ = c;

	while ((isalnum(c = gpbc()) || c == '_') && tp < etp)
		h = (h << 5) + h + (*tp++ = c);
	if (c != EOF)
		PUTBACK(c);
	*tp = EOS;
	/* token is too long, it won't match anything, but it can still
	 * be output. */
	if (tp == ep) {
		outputstr(name);
		while (isalnum(c = gpbc()) || c == '_') {
			if (sp < 0)
				putc(c, active);
			else
				CHRSAVE(c);
		}
		*name = EOS;
		return nil;
	}

	for (p = hashtab[h % HASHSIZE]; p != nil; p = p->nxtptr)
		if (h == p->hv && STREQ(name, p->name))
			break;
	return p;
}

/*
 * initkwds - initialise m4 keywords as fast as possible. 
 * This very similar to install, but without certain overheads,
 * such as calling lookup. Malloc is not used for storing the 
 * keyword strings, since we simply use the static pointers
 * within keywrds block.
 */
static void
initkwds()
{
	size_t i;
	unsigned int h;
	ndptr p;

	for (i = 0; i < MAXKEYS; i++) {
		h = hash(keywrds[i].knam);
		p = (ndptr) xalloc(sizeof(struct ndblock));
		p->nxtptr = hashtab[h % HASHSIZE];
		hashtab[h % HASHSIZE] = p;
		p->name = xstrdup(keywrds[i].knam);
		p->defn = null;
		p->hv = h;
		p->type = keywrds[i].ktyp & TYPEMASK;
		if ((keywrds[i].ktyp & NOARGS) == 0)
			p->type |= NEEDARGS;
	}
}

/* Look up a builtin type, even if overridden by the user */
int 
builtin_type(key)
	const char *key;
{
	int i;

	for (i = 0; i != MAXKEYS; i++)
		if (STREQ(keywrds[i].knam, key))
			return keywrds[i].ktyp;
	return -1;
}

char *
builtin_realname(n)
	int n;
{
	int i;

	for (i = 0; i != MAXKEYS; i++)
		if (((keywrds[i].ktyp ^ n) & TYPEMASK) == 0)
			return keywrds[i].knam;
	return NULL;
}

static void
record(t, lev)
	struct position *t;
	int lev;
{
	if (lev < MAXRECORD) {
		t[lev].name = CURRENT_NAME;
		t[lev].line = CURRENT_LINE;
	}
}

static void
dump_stack(t, lev)
	struct position *t;
	int lev;
{
	int i;

	for (i = 0; i < lev; i++) {
		if (i == MAXRECORD) {
			fprintf(stderr, "   ...\n");
			break;
		}
		fprintf(stderr, "   %s at line %lu\n", 
			t[i].name, t[i].line);
	}
}


static void 
enlarge_stack()
{
	STACKMAX *= 2;
	mstack = realloc(mstack, sizeof(stae) * STACKMAX);
	sstack = realloc(sstack, STACKMAX);
	if (mstack == NULL || sstack == NULL)
		errx(1, "Evaluation stack overflow (%lu)", 
		    (unsigned long)STACKMAX);
}
