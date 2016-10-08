/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*	from OpenSolaris "vfontedpr.c	1.17	93/06/03 SMI" 	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)vfontedpr.c	1.4 (gritter) 10/22/05
 */

#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#ifdef	EUC
#include <wchar.h>
#endif

#if defined (__GLIBC__) && defined (_IO_getc_unlocked)
#undef	getc
#define	getc(f)	_IO_getc_unlocked(f)
#endif

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

/* regexp.c */
/*char	*expmatch();		match a string to an expression */
char	*expmatch(register char *, register char *, register char *);
/*char	*STRNCMP();		a different kind of strncmp */
int	STRNCMP(register char *, register char *, register int);
/*char	*convexp();		convert expression to internal form */
char	*convexp(char *);

/*
 *	The state variables
 */

static boolean	incomm;		/* in a comment of the primary type */
static boolean	instr;		/* in a string constant */
static boolean	inchr;		/* in a string constant */
static boolean	nokeyw = FALSE;	/* no keywords being flagged */
static boolean	doindex = FALSE;/* form an index */
static boolean twocol = FALSE;	/* in two-column mode */
static boolean	filter = FALSE;	/* act as a filter (like eqn) */
static boolean	pass = FALSE;	/* when acting as a filter, pass indicates
				 * whether we are currently processing
				 * input.
				 */
static boolean	prccont;	/* continue last procedure */
static int	comtype;	/* type of comment */
static int	margin;
static int	psptr;		/* the stack index of the current procedure */
static char	pstack[PSMAX][PNAMELEN+1];	/* the procedure name stack */
static int	plstack[PSMAX];	/* the procedure nesting level stack */
static int	blklevel;	/* current nesting level */
static int	prclevel;	/* nesting level at which procedure definitions
				   may be found, -1 if none currently valid
				   (meaningful only if l_prclevel is true) */
static char	*defsfile = LIBDIR "/vgrindefs";	/* name of language definitions file */
static char	pname[BUFSIZ+1];

/*
 *	The language specific globals
 */

static char	*language = "c";/* the language indicator */
static char	*l_keywds[BUFSIZ/2];	/* keyword table address */
static char	*l_prcbeg;	/* regular expr for procedure begin */
static char	*l_combeg;	/* string introducing a comment */
static char	*l_comend;	/* string ending a comment */
static char	*l_acmbeg;	/* string introducing a comment */
static char	*l_acmend;	/* string ending a comment */
static char	*l_blkbeg;	/* string begining of a block */
static char	*l_blkend;	/* string ending a block */
static char	*l_strbeg;	/* delimiter for string constant */
static char	*l_strend;	/* delimiter for string constant */
static char	*l_chrbeg;	/* delimiter for character constant */
static char	*l_chrend;	/* delimiter for character constant */
static char	*l_prcenable;	/* re indicating that procedure definitions
				   can be found in the next lexical level --
				   kludge for lisp-like languages that use
				   something like
					   (defun (proc ...)
					  	(proc ...)
					   )
				   to define procedures */
static char	l_escape;	/* character used to  escape characters */
static boolean	l_toplex;	/* procedures only defined at top lex level */
static boolean l_prclevel;	/* procedure definitions valid only within
				   the nesting level indicated by the px
				   (l_prcenable) capability */

/*
 *  for the benefit of die-hards who aren't convinced that tabs
 *  occur every eight columns
 */
static short tabsize = 8;

static int	mb_cur_max;

/*
 *  global variables also used by expmatch
 */
boolean	escaped;		/* if last character was an escape */
char	*Start;			/* start of the current string */
boolean	l_onecase;		/* upper and lower case are equivalent */
char	*l_idchars;		/* characters legal in identifiers in addition
				   to letters and digits (default "_") */

extern int	STRNCMP(register char *, register char *, register int);
extern char	*convexp(char *);
extern char	*expmatch(register char *, register char *, register char *);
extern int	tgetent(char *, char *, char *);
extern int	tgetnum(char *);
extern int	tgetflag(char *);
extern char	*tgetstr(char *, char **);

static void	putScp(char *);
static void	putKcp(char *, char *, int);
static int	tabs(char *, char *);
static int	width(register char *, register char *);
static void	putcp(register int);
static int	isproc(char *);
static int	iskw(register char *);
static char	*fgetline(char **, size_t *, size_t *, FILE *);

/*
 * The code below emits troff macros and directives that consume part of the
 * troff macro and register space.  See tmac.vgrind for an enumeration of
 * these macros and registers.
 */

int
main(int argc, char **argv)
{
    FILE *in;
    char *fname;
    struct stat stbuf;
    char *buf = NULL;
    size_t size = 0;
    char idbuf[256];	/* enough for all 8 bit chars */
    char strings[2 * BUFSIZ];
    char defs[2 * BUFSIZ];
    int needbp = 0;
    int i;
    char *cp;

    setlocale(LC_CTYPE, "");
    mb_cur_max = MB_CUR_MAX;

    buf = malloc(size = BUFSIZ);

    /*
     * Dump the name by which we were invoked.
     */
    argc--, argv++;

    /*
     * Process arguments.  For the sake of compatibility with older versions
     * of the program, the syntax accepted below is very idiosyncratic.  Some
     * options require space between the option and its argument; others
     * disallow it.  No options may be bundled together.
     *
     * Actually, there is one incompatibility.  Files and options formerly
     * could be arbitrarily intermixed, but this is no longer allowed.  (This
     * possiblity was never documented.)
     */
    while (argc > 0 && *argv[0] == '-') {
	switch (*(cp = argv[0] + 1)) {

	case '\0':				/* - */
	    /* Take input from stdin. */
	    /* 
	     * This option implies the end of the flag arguments.  Leave the
	     * "-" in place for the file processing code to see.
	     */
	    goto flagsdone;

	case '2':				/* -2 */
	    /* Enter two column mode. */
	    twocol = 1;
	    printf("'nr =2 1\n");
	    break;

	case 'd':				/* -d <defs-file> */
	    /* Specify the language description file. */
	    defsfile = argv[1];
	    argc--, argv++;
	    break;

	case 'f':				/* -f */
	    /* Act as a filter like eqn. */
	    filter = 1;
	    /* 
	     * Slide remaining arguments down one position and postpend "-",
	     * to force reading from stdin.
	     */
	    for (i = 0; i < argc - 1; i++)
		argv[i] = argv[i + 1];	
	    argv[argc - 1] = "-";
	    continue;

	case 'h':				/* -h [header] */
	    /* Specify header string. */
	    if (argc == 1) {
		printf("'ds =H\n");
		break;
	    }
	    printf("'ds =H %s\n", argv[1]);
	    argc--, argv++;
	    break;

	case 'l':				/* -l<language> */
	    /* Specify the language. */
	    language = cp + 1;
	    break;

	case 'n':				/* -n */
	    /* Indicate no keywords. */
	    nokeyw = 1;
	    break;

	case 's':				/* -s<size> */
	    /* Specify the font size. */
	    i = 0;
	    cp++;
	    while (*cp)
		i = i * 10 + (*cp++ - '0');
	    printf("'nr vP %d\n", i);
	    break;

	case 't':				/* -t */
	    /* Specify a nondefault tab size. */
	    tabsize = 4;
	    break;

	case 'x':				/* -x */
	    /* Build an index. */
	    doindex = 1;
	    /* This option implies "-n" as well; turn it on. */
	    argv[0] = "-n";
	    continue;
	}

	/* Advance to next argument. */
	argc--, argv++;
    }

flagsdone:

    /*
     * Get the language definition from the defs file.
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
		*cp++ = '\0';
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
    cp = buf;
    l_prcenable = convexp (tgetstr ("px", &cp));
    cp = idbuf;
    l_idchars = tgetstr ("id", &cp);
    /* Set default, for compatibility with old version */
    if (l_idchars == NIL)
	l_idchars = "_";
    l_escape = '\\';
    l_onecase = tgetflag ("oc");
    l_toplex = tgetflag ("tl");
    l_prclevel = tgetflag ("pl");

    /*
     * Emit a call to the initialization macro.  If not in filter mode, emit a
     * call to the vS macro, so that tmac.vgrind can uniformly assume that all
     * program input is bracketed with vS-vE pairs.
     */
    printf("'vI\n");
    if (!filter)
	printf("'vS\n");

    if (doindex) {
	/*
	 * XXX:	Hard-wired spacing information.  This should probably turn
	 *	into the emission of a macro invocation, so that tmac.vgrind
	 *	can make up its own mind about what spacing is appropriate.
	 */
	if (twocol)
	    printf("'ta 2.5i 2.75i 4.0iR\n'in .25i\n");
	else
	    printf("'ta 4i 4.25i 5.5iR\n'in .5i\n");
    }

    while (argc > 0) {
	if (strcmp(argv[0], "-") == 0) {
	    /* Embed an instance of the original stdin. */
	    in = fdopen(fileno(stdin), "r");
	    fname = "";
	} else {
	    /* Open the file for input. */
	    if ((in = fopen(argv[0], "r")) == NULL) {
		perror(argv[0]);
		exit(1);
	    }
	    fname = argv[0];
	}
	argc--, argv++;

	/*
	 * Reinitialize for the current file.
	 */
	incomm = FALSE;
	instr = FALSE;
	inchr = FALSE;
	escaped = FALSE;
	blklevel = 0;
	prclevel = -1;
	for (psptr=0; psptr<PSMAX; psptr++) {
	    pstack[psptr][0] = '\0';
	    plstack[psptr] = 0;
	}
	psptr = -1;
	printf("'-F\n");
	if (!filter) {
	    char *cp;

	    printf(".ds =F %s\n", fname);
	    if (needbp) {
		needbp = 0;
		printf(".()\n");
		printf(".bp\n");
	    }
	    fstat(fileno(in), &stbuf);
	    cp = ctime(&stbuf.st_mtime);
	    cp[16] = '\0';
	    cp[24] = '\0';
	    printf(".ds =M %s %s\n", cp+4, cp+20);
	    printf("'wh 0 vH\n");
	    printf("'wh -1i vF\n");
	}
	if (needbp && filter) {
	    needbp = 0;
	    printf(".()\n");
	    printf(".bp\n");
	}

	/*
	 *	MAIN LOOP!!!
	 */
	while (fgetline(&buf, &size, NULL, in) != NULL) {
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
	    if (prccont && (psptr >= 0))
		printf("'FC %s\n", pstack[psptr]);
#ifdef DEBUG
	    printf ("com %o str %o chr %o ptr %d\n", incomm, instr, inchr, psptr);
#endif
	    margin = 0;
	}

	needbp = 1;
	fclose(in);
    }

    /* Close off the vS-vE pair. */
    if (!filter)
	printf("'vE\n");

    exit(0);
    /* NOTREACHED */
}

#define isidchr(c) (isalnum(c) || ((c) != NIL && strchr(l_idchars, (c)) != NIL))

static void
putScp(char *os)
{
    register char *s = os;		/* pointer to unmatched string */
    char dummy[BUFSIZ];			/* dummy to be used by expmatch */
    char *comptr;			/* end of a comment delimiter */
    char *acmptr;			/* end of a comment delimiter */
    char *strptr;			/* end of a string delimiter */
    char *chrptr;			/* end of a character const delimiter */
    char *blksptr;			/* end of a lexical block start */
    char *blkeptr;			/* end of a lexical block end */

    Start = os;			/* remember the start for expmatch */
    escaped = FALSE;
    if (nokeyw || incomm || instr)
	goto skip;
    if (isproc(s)) {
	printf("'FN %s\n", pname);
	if (psptr < PSMAX-1) {
	    ++psptr;
	    strncpy (pstack[psptr], pname, PNAMELEN);
	    pstack[psptr][PNAMELEN] = '\0';
	    plstack[psptr] = blklevel;
	}
    }
    /*
     * if l_prclevel is set, check to see whether this lexical level
     * is one immediately below which procedure definitions are allowed.
     */
    if (l_prclevel && !incomm && !instr && !inchr) {
	if (expmatch (s, l_prcenable, dummy) != NIL)
	    prclevel = blklevel + 1;
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
			printf ("\\c");
		    printf ("\\c\n'+C\n");
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
			printf ("\\c");
		    printf ("\\c\n'+C\n");
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
		    /* reset prclevel if necessary */
		    if (l_prclevel && prclevel == blklevel)
			prclevel = -1;
		    putKcp (s, blkeptr - 1, FALSE);
		    s = blkeptr;
		    blklevel--;
		    if (psptr >= 0 && plstack[psptr] >= blklevel) {

			/* end of current procedure */
			if (s != os)
			    printf ("\\c");
			printf ("\\c\n'-F\n");
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
		printf("\\c\n'-C\n");
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
putKcp (
    char	*start,		/* start of string to write */
    char	*end,		/* end of string to write */
    boolean	force		/* true if we should force nokeyw */
)
{
    int i;
    int xfld = 0;

    while (start <= end) {
	if (doindex) {
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
	    i = tabs(Start, start) - margin / tabsize;
	    printf ("\\h'|%dn'",
		    i * (tabsize == 4 ? 5 : 10) + 1 - margin % tabsize);
	    continue;
	}

	if (!nokeyw && !force)
	    if (  (*start == '#'   ||  isidchr((int)*start)) 
	       && (start == Start || !isidchr((int)start[-1]))
	       ) {
		i = iskw(start);
		if (i > 0) {
		    printf("\\*(+K");
		    do 
			putcp(*start++);
		    while (--i > 0);
		    printf("\\*(-K");
		    continue;
		}
	    }

	putcp (*start++);
    }
}


static int
tabs(char *s, char *os)
{

    return (width(s, os) / tabsize);
}

static int
width(register char *s, register char *os)
{
	register int i = 0;
	unsigned char c;
	int n;

	while (s < os) {
		if (*s == '\t') {
			i = (i + tabsize) &~ (tabsize-1);
			s++;
			continue;
		}
		c = *(unsigned char *)s;
		if (c < ' ')
			i += 2, s++;
#ifdef	EUC
		else if (c >= 0200) {
			wchar_t	wc;
			if ((n = mbtowc(&wc, s, mb_cur_max)) > 0) {
				s += n;
				if ((n = wcwidth(wc)) > 0)
					i += n;
			} else
				s++;
		}
#endif	/* EUC */
		else
			i++, s++;
	}
	return (i);
}

static void
putcp(register int c)
{

	switch(c) {

	case 0:
		break;

	case '\f':
		break;

	case '{':
		printf("\\*(+K{\\*(-K");
		break;

	case '}':
		printf("\\*(+K}\\*(-K");
		break;

	case '\\':
		printf("\\e");
		break;

	case '_':
		printf("\\*_");
		break;

	case '-':
		printf("\\*-");
		break;

		/*
		 * The following two cases deal with the accent characters.
		 * If they're part of a comment, we assume that they're part
		 * of running text and hand them to troff as regular quote
		 * characters.  Otherwise, we assume they're being used as
		 * special characters (e.g., string delimiters) and arrange
		 * for troff to render them as accents.  This is an imperfect
		 * heuristic that produces slightly better appearance than the
		 * former behavior of unconditionally rendering the characters
		 * as accents.  (See bug 1040343.)
		 */

	case '`':
		if (incomm)
			printf("`");
		else
			printf("\\`");
		break;

	case '\'':
		if (incomm)
			printf("'");
		else
			printf("\\'");
		break;

	case '.':
		printf("\\&.");
		break;

		/*
		 * The following two cases contain special hacking
		 * to make C-style comments line up.  The tests aren't
		 * really adequate; they lead to grotesqueries such
		 * as italicized multiplication and division operators.
		 * However, the obvious test (!incomm) doesn't work,
		 * because incomm isn't set until after we've put out
		 * the comment-begin characters.  The real problem is
		 * that expmatch() doesn't give us enough information.
		 */

	case '*':
		if (instr || inchr)
			printf("*");
		else
			printf("\\f2*\\fP");
		break;

	case '/':
		if (instr || inchr)
			printf("/");
		else
			printf("\\f2\\h'\\w' 'u-\\w'/'u'/\\fP");
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
isproc(char *s)
{
    pname[0] = '\0';
    if (l_prclevel ? (prclevel == blklevel) : (!l_toplex || blklevel == 0))
	if (expmatch (s, l_prcbeg, pname) != NIL) {
	    return (TRUE);
	}
    return (FALSE);
}


/*
 * iskw - check to see if the next word is a keyword
 *	Return its length if it is or 0 if it isn't.
 */

static int
iskw(register char *s)
{
	register char **ss = l_keywds;
	register int i = 1;
	register char *cp = s;

	/* Get token length. */
	while (++cp, isidchr((int)*cp))
		i++;

	while ((cp = *ss++)) {
		if (!STRNCMP(s,cp,i) && !isidchr((int)cp[i]))
			return (i);
	}
	return (0);
}

#define	LSIZE	128

static char *
fgetline(char **line, size_t *linesize, size_t *llen, FILE *fp)
{
	int c;
	size_t n = 0;

	if (*line == NULL || *linesize < LSIZE + n + 1)
		*line = realloc(*line, *linesize = LSIZE + n + 1);
	for (;;) {
		if (n >= *linesize - LSIZE / 2)
			*line = realloc(*line, *linesize += LSIZE);
		c = getc(fp);
		if (c != EOF) {
			(*line)[n++] = c;
			(*line)[n] = '\0';
			if (c == '\n')
				break;
		} else {
			if (n > 0)
				break;
			else
				return NULL;
		}
	}
	if (llen)
		*llen = n;
	return *line;
}
