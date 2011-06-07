/*
 * Copyright (c) 1980, 1993
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

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)vfontedpr.c	8.1 (Berkeley) 6/6/93";
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pathnames.h"
#include "extern.h"

#define FALSE 0
#define TRUE !(FALSE)
#define NIL 0
#define STANDARD 0
#define ALTERNATE 1

/*
 * Vfontedpr.
 *
 * Dave Presotto 1/12/81 (adapted from an earlier version by Bill Joy)
 *
 */

#define STRLEN 10		/* length of strings introducing things */
#define PNAMELEN 40		/* length of a function/procedure name */
#define PSMAX 20		/* size of procedure name stacking */

static int       iskw(char *);
static boolean   isproc(char *);
static void      putKcp(char *, char *, boolean);
static void      putScp(char *);
static void      putcp(int);
static int       tabs(char *, char *);
static int       width(char *, char *);

/*
 *	The state variables
 */

static boolean  filter = FALSE;	/* act as a filter (like eqn) */
static boolean	inchr;		/* in a string constant */
static boolean	incomm;		/* in a comment of the primary type */
static boolean	idx = FALSE;	/* form an index */
static boolean	instr;		/* in a string constant */
static boolean	nokeyw = FALSE;	/* no keywords being flagged */
static boolean  pass = FALSE;	/*
				 * when acting as a filter, pass indicates
				 * whether we are currently processing
				 * input.
				 */

static int	blklevel;	/* current nesting level */
static int	comtype;	/* type of comment */
static char *	defsfile[2] = { _PATH_VGRINDEFS, 0 };
				/* name of language definitions file */
static int	margin;
static int	plstack[PSMAX];	/* the procedure nesting level stack */
static char	pname[BUFSIZ+1];
static boolean  prccont;	/* continue last procedure */
static int	psptr;		/* the stack index of the current procedure */
static char	pstack[PSMAX][PNAMELEN+1];	/* the procedure name stack */

/*
 *	The language specific globals
 */

char	*l_acmbeg;		/* string introducing a comment */
char	*l_acmend;		/* string ending a comment */
char	*l_blkbeg;		/* string begining of a block */
char	*l_blkend;		/* string ending a block */
char    *l_chrbeg;		/* delimiter for character constant */
char    *l_chrend;		/* delimiter for character constant */
char	*l_combeg;		/* string introducing a comment */
char	*l_comend;		/* string ending a comment */
char	 l_escape;		/* character used to  escape characters */
char	*l_keywds[BUFSIZ/2];	/* keyword table address */
char	*l_nocom;		/* regexp for non-comments */
char	*l_prcbeg;		/* regular expr for procedure begin */
char    *l_strbeg;		/* delimiter for string constant */
char    *l_strend;		/* delimiter for string constant */
boolean	 l_toplex;		/* procedures only defined at top lex level */
const char *language = "c";	/* the language indicator */

#define	ps(x)	printf("%s", x)

int
main(argc, argv)
    int argc;
    char *argv[];
{
    const char *fname = "";
    struct stat stbuf;
    char buf[BUFSIZ];
    char *defs;
    int needbp = 0;

    argc--, argv++;
    do {
	char *cp;
	int i;

	if (argc > 0) {
	    if (!strcmp(argv[0], "-h")) {
		if (argc == 1) {
		    printf("'ds =H\n");
		    argc = 0;
		    goto rest;
		}
		printf("'ds =H %s\n", argv[1]);
		argc--, argv++;
		argc--, argv++;
		if (argc > 0)
		    continue;
		goto rest;
	    }

	    /* act as a filter like eqn */
	    if (!strcmp(argv[0], "-f")) {
		filter++;
		argv[0] = argv[argc-1];
		argv[argc-1] = strdup("-");
		continue;
	    }

	    /* take input from the standard place */
	    if (!strcmp(argv[0], "-")) {
		argc = 0;
		goto rest;
	    }

	    /* build an index */
	    if (!strcmp(argv[0], "-x")) {
		idx++;
		argv[0] = strdup("-n");
	    }

	    /* indicate no keywords */
	    if (!strcmp(argv[0], "-n")) {
		nokeyw++;
		argc--, argv++;
		continue;
	    }

	    /* specify the font size */
	    if (!strncmp(argv[0], "-s", 2)) {
		i = 0;
		cp = argv[0] + 2;
		while (*cp)
		    i = i * 10 + (*cp++ - '0');
		printf("'ps %d\n'vs %d\n", i, i+1);
		argc--, argv++;
		continue;
	    }

	    /* specify the language */
	    if (!strncmp(argv[0], "-l", 2)) {
		language = argv[0]+2;
		argc--, argv++;
		continue;
	    }

	    /* specify the language description file */
	    if (!strncmp(argv[0], "-d", 2)) {
		defsfile[0] = argv[1];
		argc--, argv++;
		argc--, argv++;
		continue;
	    }

	    /* open the file for input */
	    if (freopen(argv[0], "r", stdin) == NULL)
		err(1, "%s", argv[0]);
	    if (idx)
		printf("'ta 4i 4.25i 5.5iR\n'in .5i\n");
	    fname = argv[0];
	    argc--, argv++;
	}
    rest:

	/*
	 *  get the  language definition from the defs file
	 */
	i = cgetent(&defs, defsfile, language);
	if (i == -1) {
	    fprintf (stderr, "no entry for language %s\n", language);
	    exit (0);
	} else  if (i == -2) { fprintf(stderr,
	    "cannot find vgrindefs file %s\n", defsfile[0]);
	    exit (0);
	} else if (i == -3) { fprintf(stderr,
	    "potential reference loop detected in vgrindefs file %s\n",
            defsfile[0]);
	    exit(0);
	}
	if (cgetustr(defs, "kw", &cp) == -1)
	    nokeyw = TRUE;
	else  {
	    char **cpp;

	    cpp = l_keywds;
	    while (*cp) {
		while (*cp == ' ' || *cp =='\t')
		    *cp++ = '\0';
		if (*cp)
		    *cpp++ = cp;
		while (*cp != ' ' && *cp  != '\t' && *cp)
		    cp++;
	    }
	    *cpp = NIL;
	}
	cgetustr(defs, "pb", &cp);
	l_prcbeg = convexp(cp);
	cgetustr(defs, "cb", &cp);
	l_combeg = convexp(cp);
	cgetustr(defs, "ce", &cp);
	l_comend = convexp(cp);
	cgetustr(defs, "ab", &cp);
	l_acmbeg = convexp(cp);
	cgetustr(defs, "ae", &cp);
	l_acmend = convexp(cp);
	cgetustr(defs, "sb", &cp);
	l_strbeg = convexp(cp);
	cgetustr(defs, "se", &cp);
	l_strend = convexp(cp);
	cgetustr(defs, "bb", &cp);
	l_blkbeg = convexp(cp);
	cgetustr(defs, "be", &cp);
	l_blkend = convexp(cp);
	cgetustr(defs, "lb", &cp);
	l_chrbeg = convexp(cp);
	cgetustr(defs, "le", &cp);
	l_chrend = convexp(cp);
	if (cgetustr(defs, "nc", &cp) >= 0)
		l_nocom = convexp(cp);
	l_escape = '\\';
	l_onecase = (cgetcap(defs, "oc", ':') != NULL);
	l_toplex = (cgetcap(defs, "tl", ':') != NULL);

	/* initialize the program */

	incomm = FALSE;
	instr = FALSE;
	inchr = FALSE;
	_escaped = FALSE;
	blklevel = 0;
	for (psptr=0; psptr<PSMAX; psptr++) {
	    pstack[psptr][0] = '\0';
	    plstack[psptr] = 0;
	}
	psptr = -1;
	ps("'-F\n");
	if (!filter) {
	    printf(".ds =F %s\n", fname);
	    ps("'wh 0 vH\n");
	    ps("'wh -1i vF\n");
	}
	if (needbp) {
	    needbp = 0;
	    printf(".()\n");
	    printf(".bp\n");
	}
	if (!filter) {
	    fstat(fileno(stdin), &stbuf);
	    cp = ctime(&stbuf.st_mtime);
	    cp[16] = '\0';
	    cp[24] = '\0';
	    printf(".ds =M %s %s\n", cp+4, cp+20);
	}

	/*
	 *	MAIN LOOP!!!
	 */
	while (fgets(buf, sizeof buf, stdin) != NULL) {
	    if (buf[0] == '\f') {
		printf(".bp\n");
	    }
	    if (buf[0] == '.') {
		printf("%s", buf);
		if (!strncmp (buf+1, "vS", 2))
		    pass = TRUE;
		if (!strncmp (buf+1, "vE", 2))
		    pass = FALSE;
		continue;
	    }
	    prccont = FALSE;
	    if (!filter || pass)
		putScp(buf);
	    else
		printf("%s", buf);
	    if (prccont && (psptr >= 0)) {
		ps("'FC ");
		ps(pstack[psptr]);
		ps("\n");
	    }
#ifdef DEBUG
	    printf ("com %o str %o chr %o ptr %d\n", incomm, instr, inchr, psptr);
#endif
	    margin = 0;
	}
	needbp = 1;
    } while (argc > 0);
    exit(0);
}

#define isidchr(c) (isalnum(c) || (c) == '_')

static void
putScp(os)
    char *os;
{
    register char *s = os;		/* pointer to unmatched string */
    char dummy[BUFSIZ];			/* dummy to be used by expmatch */
    char *comptr;			/* end of a comment delimiter */
    char *acmptr;			/* end of a comment delimiter */
    char *strptr;			/* end of a string delimiter */
    char *chrptr;			/* end of a character const delimiter */
    char *blksptr;			/* end of a lexical block start */
    char *blkeptr;			/* end of a lexical block end */
    char *nocomptr;			/* end of a non-comment delimiter */

    s_start = os;			/* remember the start for expmatch */
    _escaped = FALSE;
    if (nokeyw || incomm || instr)
	goto skip;
    if (isproc(s)) {
	ps("'FN ");
	ps(pname);
        ps("\n");
	if (psptr < PSMAX) {
	    ++psptr;
	    strncpy (pstack[psptr], pname, PNAMELEN);
	    pstack[psptr][PNAMELEN] = '\0';
	    plstack[psptr] = blklevel;
	}
    }
skip:
    do {
	/* check for string, comment, blockstart, etc */
	if (!incomm && !instr && !inchr) {

	    blkeptr = expmatch (s, l_blkend, dummy);
	    blksptr = expmatch (s, l_blkbeg, dummy);
	    comptr = expmatch (s, l_combeg, dummy);
	    acmptr = expmatch (s, l_acmbeg, dummy);
	    strptr = expmatch (s, l_strbeg, dummy);
	    chrptr = expmatch (s, l_chrbeg, dummy);
	    nocomptr = expmatch (s, l_nocom, dummy);

	    /* start of non-comment? */
	    if (nocomptr != NIL)
		if ((nocomptr <= comptr || comptr == NIL)
		  && (nocomptr <= acmptr || acmptr == NIL)) {
		    /* continue after non-comment */
		    putKcp (s, nocomptr-1, FALSE);
		    s = nocomptr;
		    continue;
		}

	    /* start of a comment? */
	    if (comptr != NIL)
		if ((comptr < strptr || strptr == NIL)
		  && (comptr < acmptr || acmptr == NIL)
		  && (comptr < chrptr || chrptr == NIL)
		  && (comptr < blksptr || blksptr == NIL)
		  && (comptr < blkeptr || blkeptr == NIL)) {
		    putKcp (s, comptr-1, FALSE);
		    s = comptr;
		    incomm = TRUE;
		    comtype = STANDARD;
		    if (s != os)
			ps ("\\c");
		    ps ("\\c\n'+C\n");
		    continue;
		}

	    /* start of a comment? */
	    if (acmptr != NIL)
		if ((acmptr < strptr || strptr == NIL)
		  && (acmptr < chrptr || chrptr == NIL)
		  && (acmptr < blksptr || blksptr == NIL)
		  && (acmptr < blkeptr || blkeptr == NIL)) {
		    putKcp (s, acmptr-1, FALSE);
		    s = acmptr;
		    incomm = TRUE;
		    comtype = ALTERNATE;
		    if (s != os)
			ps ("\\c");
		    ps ("\\c\n'+C\n");
		    continue;
		}

	    /* start of a string? */
	    if (strptr != NIL)
		if ((strptr < chrptr || chrptr == NIL)
		  && (strptr < blksptr || blksptr == NIL)
		  && (strptr < blkeptr || blkeptr == NIL)) {
		    putKcp (s, strptr-1, FALSE);
		    s = strptr;
		    instr = TRUE;
		    continue;
		}

	    /* start of a character string? */
	    if (chrptr != NIL)
		if ((chrptr < blksptr || blksptr == NIL)
		  && (chrptr < blkeptr || blkeptr == NIL)) {
		    putKcp (s, chrptr-1, FALSE);
		    s = chrptr;
		    inchr = TRUE;
		    continue;
		}

	    /* end of a lexical block */
	    if (blkeptr != NIL) {
		if (blkeptr < blksptr || blksptr == NIL) {
		    putKcp (s, blkeptr - 1, FALSE);
		    s = blkeptr;
		    if (blklevel > 0 /* sanity */)
			    blklevel--;
		    if (psptr >= 0 && plstack[psptr] >= blklevel) {

			/* end of current procedure */
			if (s != os)
			    ps ("\\c");
			ps ("\\c\n'-F\n");
			blklevel = plstack[psptr];

			/* see if we should print the last proc name */
			if (--psptr >= 0)
			    prccont = TRUE;
			else
			    psptr = -1;
		    }
		    continue;
		}
	    }

	    /* start of a lexical block */
	    if (blksptr != NIL) {
		putKcp (s, blksptr - 1, FALSE);
		s = blksptr;
		blklevel++;
		continue;
	    }

	/* check for end of comment */
	} else if (incomm) {
	    comptr = expmatch (s, l_comend, dummy);
	    acmptr = expmatch (s, l_acmend, dummy);
	    if (((comtype == STANDARD) && (comptr != NIL)) ||
	        ((comtype == ALTERNATE) && (acmptr != NIL))) {
		if (comtype == STANDARD) {
		    putKcp (s, comptr-1, TRUE);
		    s = comptr;
		} else {
		    putKcp (s, acmptr-1, TRUE);
		    s = acmptr;
		}
		incomm = FALSE;
		ps("\\c\n'-C\n");
		continue;
	    } else {
		putKcp (s, s + strlen(s) -1, TRUE);
		s = s + strlen(s);
		continue;
	    }

	/* check for end of string */
	} else if (instr) {
	    if ((strptr = expmatch (s, l_strend, dummy)) != NIL) {
		putKcp (s, strptr-1, TRUE);
		s = strptr;
		instr = FALSE;
		continue;
	    } else {
		putKcp (s, s+strlen(s)-1, TRUE);
		s = s + strlen(s);
		continue;
	    }

	/* check for end of character string */
	} else if (inchr) {
	    if ((chrptr = expmatch (s, l_chrend, dummy)) != NIL) {
		putKcp (s, chrptr-1, TRUE);
		s = chrptr;
		inchr = FALSE;
		continue;
	    } else {
		putKcp (s, s+strlen(s)-1, TRUE);
		s = s + strlen(s);
		continue;
	    }
	}

	/* print out the line */
	putKcp (s, s + strlen(s) -1, FALSE);
	s = s + strlen(s);
    } while (*s);
}

static void
putKcp (start, end, force)
    char	*start;		/* start of string to write */
    char	*end;		/* end of string to write */
    boolean	force;		/* true if we should force nokeyw */
{
    int i;
    int xfld = 0;

    while (start <= end) {
	if (idx) {
	    if (*start == ' ' || *start == '\t') {
		if (xfld == 0)
		    printf("\001");
		printf("\t");
		xfld = 1;
		while (*start == ' ' || *start == '\t')
		    start++;
		continue;
	    }
	}

	/* take care of nice tab stops */
	if (*start == '\t') {
	    while (*start == '\t')
		start++;
	    i = tabs(s_start, start) - margin / 8;
	    printf("\\h'|%dn'", i * 10 + 1 - margin % 8);
	    continue;
	}

	if (!nokeyw && !force)
	    if ((*start == '#' || isidchr(*start))
	    && (start == s_start || !isidchr(start[-1]))) {
		i = iskw(start);
		if (i > 0) {
		    ps("\\*(+K");
		    do
			putcp((unsigned char)*start++);
		    while (--i > 0);
		    ps("\\*(-K");
		    continue;
		}
	    }

	putcp ((unsigned char)*start++);
    }
}


static int
tabs(s, os)
    char *s, *os;
{

    return (width(s, os) / 8);
}

static int
width(s, os)
	register char *s, *os;
{
	register int i = 0;

	while (s < os) {
		if (*s == '\t') {
			i = (i + 8) &~ 7;
			s++;
			continue;
		}
		if (*s < ' ')
			i += 2;
		else
			i++;
		s++;
	}
	return (i);
}

static void
putcp(c)
	register int c;
{

	switch(c) {

	case 0:
		break;

	case '\f':
		break;

	case '\r':
		break;

	case '{':
		ps("\\*(+K{\\*(-K");
		break;

	case '}':
		ps("\\*(+K}\\*(-K");
		break;

	case '\\':
		ps("\\e");
		break;

	case '_':
		ps("\\*_");
		break;

	case '-':
		ps("\\*-");
		break;

	case '`':
		ps("\\`");
		break;

	case '\'':
		ps("\\'");
		break;

	case '.':
		ps("\\&.");
		break;

	case '*':
		ps("\\fI*\\fP");
		break;

	case '/':
		ps("\\fI\\h'\\w' 'u-\\w'/'u'/\\fP");
		break;

	default:
		if (c < 040)
			putchar('^'), c |= '@';
	case '\t':
	case '\n':
		putchar(c);
	}
}

/*
 *	look for a process beginning on this line
 */
static boolean
isproc(s)
    char *s;
{
    pname[0] = '\0';
    if (!l_toplex || blklevel == 0)
	if (expmatch (s, l_prcbeg, pname) != NIL) {
	    return (TRUE);
	}
    return (FALSE);
}


/*  iskw -	check to see if the next word is a keyword
 */

static int
iskw(s)
	register char *s;
{
	register char **ss = l_keywds;
	register int i = 1;
	register char *cp = s;

	while (++cp, isidchr(*cp))
		i++;
	while ((cp = *ss++))
		if (!STRNCMP(s,cp,i) && !isidchr(cp[i]))
			return (i);
	return (0);
}

