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
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * main.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"
#include "pathnames.h"

ndptr hashtab[HASHSIZE];	/* hash table for macros etc.  */
unsigned char buf[BUFSIZE];              /* push-back buffer            */
unsigned char *bufbase = buf;            /* the base for current ilevel */
unsigned char *bbase[MAXINP];            /* the base for each ilevel    */
unsigned char *bp = buf;                 /* first available character   */
unsigned char *endpbb = buf+BUFSIZE;     /* end of push-back buffer     */
stae mstack[STACKMAX+1]; 	/* stack of m4 machine         */
char strspace[STRSPMAX+1];	/* string space for evaluation */
char *ep = strspace;		/* first free char in strspace */
char *endest= strspace+STRSPMAX;/* end of string space	       */
int sp; 			/* current m4  stack pointer   */
int fp; 			/* m4 call frame pointer       */
FILE *infile[MAXINP];		/* input file stack (0=stdin)  */
FILE *outfile[MAXOUT];		/* diversion array(0=bitbucket)*/
FILE *active;			/* active output file pointer  */
char *m4temp;			/* filename for diversions     */
char *m4dir;			/* directory for diversions    */
int ilevel = 0; 		/* input file stack pointer    */
int oindex = 0; 		/* diversion index..	       */
char *null = "";                /* as it says.. just a null..  */
char *m4wraps = "";             /* m4wrap string default..     */
char lquote = LQUOTE;		/* left quote character  (`)   */
char rquote = RQUOTE;		/* right quote character (')   */
char scommt = SCOMMT;		/* start character for comment */
char ecommt = ECOMMT;		/* end character for comment   */

struct keyblk keywrds[] = {	/* m4 keywords to be installed */
	"include",      INCLTYPE,
	"sinclude",     SINCTYPE,
	"define",       DEFITYPE,
	"defn",         DEFNTYPE,
	"divert",       DIVRTYPE,
	"expr",         EXPRTYPE,
	"eval",         EXPRTYPE,
	"substr",       SUBSTYPE,
	"ifelse",       IFELTYPE,
	"ifdef",        IFDFTYPE,
	"len",          LENGTYPE,
	"incr",         INCRTYPE,
	"decr",         DECRTYPE,
	"dnl",          DNLNTYPE,
	"changequote",  CHNQTYPE,
	"changecom",    CHNCTYPE,
	"index",        INDXTYPE,
#ifdef EXTENDED
	"paste",        PASTTYPE,
	"spaste",       SPASTYPE,
#endif
	"popdef",       POPDTYPE,
	"pushdef",      PUSDTYPE,
	"dumpdef",      DUMPTYPE,
	"shift",        SHIFTYPE,
	"translit",     TRNLTYPE,
	"undefine",     UNDFTYPE,
	"undivert",     UNDVTYPE,
	"divnum",       DIVNTYPE,
	"maketemp",     MKTMTYPE,
	"errprint",     ERRPTYPE,
	"m4wrap",       M4WRTYPE,
	"m4exit",       EXITTYPE,
	"syscmd",       SYSCTYPE,
	"sysval",       SYSVTYPE,

#ifdef unix
	"unix",         MACRTYPE,
#else
#ifdef vms
	"vms",          MACRTYPE,
#endif
#endif
};

#define MAXKEYS	(sizeof(keywrds)/sizeof(struct keyblk))

void macro();
void initkwds();

int
main(argc,argv)
	int argc;
	char *argv[];
{
	register int c;
	register int n;
	char *p;
	register FILE *ifp;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, onintr);

	initkwds();

	while ((c = getopt(argc, argv, "tD:U:o:")) != -1)
		switch(c) {

		case 'D':               /* define something..*/
			for (p = optarg; *p; p++)
				if (*p == '=')
					break;
			if (*p)
				*p++ = EOS;
			dodefine(optarg, p);
			break;
		case 'U':               /* undefine...       */
			remhash(optarg, TOP);
			break;
		case 'o':		/* specific output   */
		case '?':
			usage();
		}

        argc -= optind;
        argv += optind;

	active = stdout;		/* default active output     */
					/* filename for diversions   */
	m4dir = mkdtemp(xstrdup(_PATH_DIVDIRNAME));
	(void) asprintf(&m4temp, "%s/%s", m4dir, _PATH_DIVNAME);

	bbase[0] = bufbase;
        if (!argc) {
 		sp = -1;		/* stack pointer initialized */
		fp = 0; 		/* frame pointer initialized */
		infile[0] = stdin;	/* default input (naturally) */
		macro();
	} else
		for (; argc--; ++argv) {
			p = *argv;
			if (p[0] == '-' && p[1] == '\0')
				ifp = stdin;
			else if ((ifp = fopen(p, "r")) == NULL)
				err(1, "%s", p);
			sp = -1;
			fp = 0;
			infile[0] = ifp;
			macro();
			if (ifp != stdin)
				(void)fclose(ifp);
		}

	if (*m4wraps) { 		/* anything for rundown ??   */
		ilevel = 0;		/* in case m4wrap includes.. */
		bufbase = bp = buf;	/* use the entire buffer   */
		putback(EOF);		/* eof is a must !!	     */
		pbstr(m4wraps); 	/* user-defined wrapup act   */
		macro();		/* last will and testament   */
	}

	if (active != stdout)
		active = stdout;	/* reset output just in case */
	for (n = 1; n < MAXOUT; n++)	/* default wrap-up: undivert */
		if (outfile[n] != NULL)
			getdiv(n);
					/* remove bitbucket if used  */
	if (outfile[0] != NULL) {
		(void) fclose(outfile[0]);
		m4temp[UNIQUE] = '0';
#ifdef vms
		(void) remove(m4temp);
#else
		(void) unlink(m4temp);
		(void) rmdir(m4dir);
#endif
	}

	return 0;
}

ndptr inspect();

/*
 * macro - the work horse..
 */
void
macro() {
	char token[MAXTOK];
	register char *s;
	register int t, l;
	register ndptr p;
	register int  nlpar;

	cycle {
		if ((t = gpbc()) == '_' || (t != EOF && isalpha(t))) {
			putback(t);
			if ((p = inspect(s = token)) == nil) {
				if (sp < 0)
					while (*s)
						putc(*s++, active);
				else
					while (*s)
						chrsave(*s++);
			}
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
				pushs(p->defn);	      /* defn string */
				pushs(p->name);	      /* macro name  */
				pushs(ep);	      /* start next..*/

				putback(l = gpbc());
				if (l != LPAREN)  {   /* add bracks  */
					putback(RPAREN);
					putback(LPAREN);
				}
			}
		}
		else if (t == EOF) {
			if (sp > -1)
				errx(1, "unexpected end of input");
			if (ilevel <= 0)
				break;			/* all done thanks.. */
			--ilevel;
			(void) fclose(infile[ilevel+1]);
			bufbase = bbase[ilevel];
			continue;
		}
	/*
	 * non-alpha single-char token seen..
	 * [the order of else if .. stmts is important.]
	 */
		else if (t == lquote) { 		/* strip quotes */
			nlpar = 1;
			do {
				if ((l = gpbc()) == rquote)
					nlpar--;
				else if (l == lquote)
					nlpar++;
				else if (l == EOF)
					errx(1, "missing right quote");
				if (nlpar > 0) {
					if (sp < 0)
						putc(l, active);
					else
						chrsave(l);
				}
			}
			while (nlpar != 0);
		}

		else if (sp < 0) {		/* not in a macro at all */
			if (t == scommt) {	/* comment handling here */
				putc(t, active);
				while ((t = gpbc()) != ecommt)
					putc(t, active);
			}
			putc(t, active);	/* output directly..	 */
		}

		else switch(t) {

		case LPAREN:
			if (PARLEV > 0)
				chrsave(t);
			while ((l = gpbc()) != EOF && isspace(l))
				;		/* skip blank, tab, nl.. */
			putback(l);
			PARLEV++;
			break;

		case RPAREN:
			if (--PARLEV > 0)
				chrsave(t);
			else {			/* end of argument list */
				chrsave(EOS);

				if (sp == STACKMAX)
					errx(1, "internal stack overflow");

				if (CALTYP == MACRTYPE)
					expand((char **) mstack+fp+1, sp-fp);
				else
					eval((char **) mstack+fp+1, sp-fp, CALTYP);

				ep = PREVEP;	/* flush strspace */
				sp = PREVSP;	/* previous sp..  */
				fp = PREVFP;	/* rewind stack...*/
			}
			break;

		case COMMA:
			if (PARLEV == 1) {
				chrsave(EOS);		/* new argument   */
				while ((l = gpbc()) != EOF && isspace(l))
					;
				putback(l);
				pushs(ep);
			} else
				chrsave(t);
			break;

		default:
			chrsave(t);			/* stack the char */
			break;
		}
	}
}

/*
 * build an input token..
 * consider only those starting with _ or A-Za-z. This is a
 * combo with lookup to speed things up.
 */
ndptr
inspect(tp)
register char *tp;
{
	register int c;
	register char *name = tp;
	register char *etp = tp+MAXTOK;
	register ndptr p;
	register unsigned long h = 0;

	while ((c = gpbc()) != EOF && (isalnum(c) || c == '_') && tp < etp)
		h = (h << 5) + h + (*tp++ = c);
	putback(c);
	if (tp == etp)
		errx(1, "token too long");

	*tp = EOS;

	for (p = hashtab[h%HASHSIZE]; p != nil; p = p->nxtptr)
		if (STREQ(name, p->name))
			break;
	return p;
}

/*
 * initkwds - initialise m4 keywords as fast as possible.
 * This very similar to install, but without certain overheads,
 * such as calling lookup. Malloc is not used for storing the
 * keyword strings, since we simply use the static  pointers
 * within keywrds block.
 */
void
initkwds() {
	register int i;
	register int h;
	register ndptr p;

	for (i = 0; i < MAXKEYS; i++) {
		h = hash(keywrds[i].knam);
		p = (ndptr) xalloc(sizeof(struct ndblock));
		p->nxtptr = hashtab[h];
		hashtab[h] = p;
		p->name = keywrds[i].knam;
		p->defn = null;
		p->type = keywrds[i].ktyp | STATIC;
	}
}
