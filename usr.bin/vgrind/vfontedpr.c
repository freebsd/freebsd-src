/*
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1980 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)vfontedpr.c	5.5 (Berkeley) 6/1/90";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include "pathnames.h"

#define boolean int
#define TRUE 1
#define FALSE 0
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

/* regular expression routines */

char	*expmatch();		/* match a string to an expression */
char	*STRNCMP();		/* a different kindof strncmp */
char	*convexp();		/* convert expression to internal form */
char	*tgetstr();

boolean	isproc();


char	*ctime();

/*
 *	The state variables
 */

boolean	incomm;			/* in a comment of the primary type */
boolean	instr;			/* in a string constant */
boolean	inchr;			/* in a string constant */
boolean	nokeyw = FALSE;		/* no keywords being flagged */
boolean	index = FALSE;		/* form an index */
boolean filter = FALSE;		/* act as a filter (like eqn) */
boolean pass = FALSE;		/* when acting as a filter, pass indicates
				 * whether we are currently processing
				 * input.
				 */
boolean prccont;		/* continue last procedure */
int	comtype;		/* type of comment */
int	margin;
int	psptr;			/* the stack index of the current procedure */
char	pstack[PSMAX][PNAMELEN+1];	/* the procedure name stack */
int	plstack[PSMAX];		/* the procedure nesting level stack */
int	blklevel;		/* current nesting level */
char	*defsfile = _PATH_VGRINDEFS;	/* name of language definitions file */
char	pname[BUFSIZ+1];

/*
 *	The language specific globals
 */

char	*language = "c";	/* the language indicator */
char	*l_keywds[BUFSIZ/2];	/* keyword table address */
char	*l_prcbeg;		/* regular expr for procedure begin */
char	*l_combeg;		/* string introducing a comment */
char	*l_comend;		/* string ending a comment */
char	*l_acmbeg;		/* string introducing a comment */
char	*l_acmend;		/* string ending a comment */
char	*l_blkbeg;		/* string begining of a block */
char	*l_blkend;		/* string ending a block */
char    *l_strbeg;		/* delimiter for string constant */
char    *l_strend;		/* delimiter for string constant */
char    *l_chrbeg;		/* delimiter for character constant */
char    *l_chrend;		/* delimiter for character constant */
char	l_escape;		/* character used to  escape characters */
boolean	l_toplex;		/* procedures only defined at top lex level */

/*
 *  global variables also used by expmatch
 */
boolean _escaped;		/* if last character was an escape */
char *_start;			/* start of the current string */
boolean	l_onecase;		/* upper and lower case are equivalent */

#define	ps(x)	printf("%s", x)

main(argc, argv)
    int argc;
    char *argv[];
{
    int lineno;
    char *fname = "";
    char *ptr;
    struct stat stbuf;
    char buf[BUFSIZ];
    char strings[2 * BUFSIZ];
    char defs[2 * BUFSIZ];
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
		argv[argc-1] = "-";
		continue;
	    }

	    /* take input from the standard place */
	    if (!strcmp(argv[0], "-")) {
		argc = 0;
		goto rest;
	    }

	    /* build an index */
	    if (!strcmp(argv[0], "-x")) {
		index++;
		argv[0] = "-n";
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
		defsfile = argv[1];
		argc--, argv++;
		argc--, argv++;
		continue;
	    }

	    /* open the file for input */
	    if (freopen(argv[0], "r", stdin) == NULL) {
		perror(argv[0]);
		exit(1);
	    }
	    if (index)
		printf("'ta 4i 4.25i 5.5iR\n'in .5i\n");
	    fname = argv[0];
	    argc--, argv++;
	}
    rest:

	/*
	 *  get the  language definition from the defs file
	 */
	i = tgetent (defs, language, defsfile);
	if (i == 0) {
	    fprintf (stderr, "no entry for language %s\n", language);
	    exit (0);
	} else  if (i < 0) {
	    fprintf (stderr,  "cannot find vgrindefs file %s\n", defsfile);
	    exit (0);
	}
	cp = strings;
	if (tgetstr ("kw", &cp) == NIL)
	    nokeyw = TRUE;
	else  {
	    char **cpp;

	    cpp = l_keywds;
	    cp = strings;
	    while (*cp) {
		while (*cp == ' ' || *cp =='\t')
		    *cp++ = NULL;
		if (*cp)
		    *cpp++ = cp;
		while (*cp != ' ' && *cp  != '\t' && *cp)
		    cp++;
	    }
	    *cpp = NIL;
	}
	cp = buf;
	l_prcbeg = convexp (tgetstr ("pb", &cp));
	cp = buf;
	l_combeg = convexp (tgetstr ("cb", &cp));
	cp = buf;
	l_comend = convexp (tgetstr ("ce", &cp));
	cp = buf;
	l_acmbeg = convexp (tgetstr ("ab", &cp));
	cp = buf;
	l_acmend = convexp (tgetstr ("ae", &cp));
	cp = buf;
	l_strbeg = convexp (tgetstr ("sb", &cp));
	cp = buf;
	l_strend = convexp (tgetstr ("se", &cp));
	cp = buf;
	l_blkbeg = convexp (tgetstr ("bb", &cp));
	cp = buf;
	l_blkend = convexp (tgetstr ("be", &cp));
	cp = buf;
	l_chrbeg = convexp (tgetstr ("lb", &cp));
	cp = buf;
	l_chrend = convexp (tgetstr ("le", &cp));
	l_escape = '\\';
	l_onecase = tgetflag ("oc");
	l_toplex = tgetflag ("tl");

	/* initialize the program */

	incomm = FALSE;
	instr = FALSE;
	inchr = FALSE;
	_escaped = FALSE;
	blklevel = 0;
	for (psptr=0; psptr<PSMAX; psptr++) {
	    pstack[psptr][0] = NULL;
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

    _start = os;			/* remember the start for expmatch */
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
	    pstack[psptr][PNAMELEN] = NULL;
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

putKcp (start, end, force)
    char	*start;		/* start of string to write */
    char	*end;		/* end of string to write */
    boolean	force;		/* true if we should force nokeyw */
{
    int i;
    int xfld = 0;

    while (start <= end) {
	if (index) {
	    if (*start == ' ' || *start == '\t') {
		if (xfld == 0)	
		    printf("");
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
	    i = tabs(_start, start) - margin / 8;
	    printf("\\h'|%dn'", i * 10 + 1 - margin % 8);
	    continue;
	}

	if (!nokeyw && !force)
	    if ((*start == '#' || isidchr(*start)) 
	    && (start == _start || !isidchr(start[-1]))) {
		i = iskw(start);
		if (i > 0) {
		    ps("\\*(+K");
		    do 
			putcp(*start++);
		    while (--i > 0);
		    ps("\\*(-K");
		    continue;
		}
	    }

	putcp (*start++);
    }
}


tabs(s, os)
    char *s, *os;
{

    return (width(s, os) / 8);
}

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

putcp(c)
	register int c;
{

	switch(c) {

	case 0:
		break;

	case '\f':
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
boolean
isproc(s)
    char *s;
{
    pname[0] = NULL;
    if (!l_toplex || blklevel == 0)
	if (expmatch (s, l_prcbeg, pname) != NIL) {
	    return (TRUE);
	}
    return (FALSE);
}


/*  iskw -	check to see if the next word is a keyword
 */

iskw(s)
	register char *s;
{
	register char **ss = l_keywds;
	register int i = 1;
	register char *cp = s;

	while (++cp, isidchr(*cp))
		i++;
	while (cp = *ss++)
		if (!STRNCMP(s,cp,i) && !isidchr(cp[i]))
			return (i);
	return (0);
}
