/*  File   : main.c
    Author : Ozan Yigit
    Updated: 4 May 1992
    Defines: M4 macro processor.
*/

#include "mdef.h"
#include "extr.h"
#include "ourlims.h"

char chtype[1 - EOF + UCHAR_MAX];

#define	is_sym1(c) (chtype[(c)-EOF] > 10)
#define is_sym2(c) (chtype[(c)-EOF] >  0)
#define is_blnk(c) ((unsigned)((c)-1) < ' ')

/*
 * m4 - macro processor
 *
 * PD m4 is based on the macro tool distributed with the software 
 * tools (VOS) package, and described in the "SOFTWARE TOOLS" and 
 * "SOFTWARE TOOLS IN PASCAL" books. It has been expanded to include 
 * most of the command set of SysV m4, the standard UN*X macro processor.
 *
 * Since both PD m4 and UN*X m4 are based on SOFTWARE TOOLS macro,
 * there may be certain implementation similarities between
 * the two. The PD m4 was produced without ANY references to m4
 * sources.
 *
 * References:
 *
 *	Software Tools distribution: macro
 *
 *	Kernighan, Brian W. and P. J. Plauger, SOFTWARE
 *	TOOLS IN PASCAL, Addison-Wesley, Mass. 1981
 *
 *	Kernighan, Brian W. and P. J. Plauger, SOFTWARE
 *	TOOLS, Addison-Wesley, Mass. 1976
 *
 *	Kernighan, Brian W. and Dennis M. Ritchie,
 *	THE M4 MACRO PROCESSOR, Unix Programmer's Manual,
 *	Seventh Edition, Vol. 2, Bell Telephone Labs, 1979
 *
 *	System V man page for M4
 *
 * Modification History:
 *
 * Mar 26 1992 RAOK	1.  Eliminated magic numbers 8, 255, 256 in favour
 *			of the standard limits CHAR_BIT, UCHAR_MAX, which 
 *			are in the new header ourlims.h.  This is part of
 *			the "8-bit-clean M4" project.  To the best of my
 *			belief, all of the code should work in EBCDIC,
 *			ASCII, DEC MNCS, ISO 8859/n, or the Mac character
 *			set, as long as chars are unsigned.  There are
 *			still some places where signed bytes can cause
 *			trouble.
 *			
 *			2.  Changed expr() to use long int rather than int.
 *			This is so that we'd get 32-bit arithmetic on a Sun,
 *			Encore, PC, Mac &c.  As part of this, the code for
 *			shifts has been elaborated to yield signed shifts
 *			on all machines.  The charcon() function didn't work
 *			with multi-character literals, although it was meant
 *			to.  Now it does.  pbrad() has been changed so that
 *			eval('abcd',0) => abcd, not dcba, which was useless.
 *			
 *			3.  I finally got sick of the fact that &&, ||, and
 *			?: always evaluate all their arguments.  This is
 *			consistent with UNIX System V Release 3, but I for
 *			one don't see anything to gain by having eval(0&&1/0)
 *			crash when it would simply yield 0 in C.  Now these
 *			operators are more consistent with the C preprocessor.
 *
 * Nov 13 1992 RAOK	Added the quoter facility.  The purpose of this is
 *			to make it easier to generate data for a variety of
 *			programming languages, including sh, awk, Lisp, C.
 *			There are two holes in the implementation:  dumpdef
 *			prints junk and undefine doesn't release everything.
 *			This was mainly intended as a prototype to show that
 *			it could be done.
 *
 * Jun 16 1992 RAOK	Added vquote and gave changequote a 3rd argument.
 *			The idea of this is to make it possible to quote
 *			ANY string, including one with unbalanced ` or '.
 *			I also made eval(c,0) convert decimal->ASCII, so
 *			that eval(39,0) yields ' and eval(96,0) yields `.
 *
 * Apr 28 1992 RAOK	Used gcc to find and fix ANSI clashes, so that
 *			PD M4 could be ported to MS-DOS (Turbo C 3).
 *			Main known remaining problem:  use of mktemp().
 *			Also, command line handling needs to be worked out.
 *
 * Mar 26 1992 RAOK	PD M4 now accepts file names on the command line
 *			just like UNIX M4.  Warning:  macro calls must NOT
 *			cross file boundaries.  UNIX M4 doesn't mind;
 *			(m4 a b c) and (cat a b c | m4) are just the same
 *			except for error messages.  PD M4 will report an
 *			unexpected EOF if a file ends while a macro call or
 *			string is still being parsed.  When there is one
 *			file name argument, or none, you can't tell the
 *			difference, and that's all I need.
 *
 * May 15 1991 RAOK	DIVNAM was a string constant, but was changed!
 *			Fixed that and a couple of other things to make
 *			GCC happy.  (Also made "foo$bar" get through.)
 *
 * Apr 17 1991 RAOK	There was a major mistake.  If you did
 *			define(foo, `1 include(bar) 2') where
 *			file bar held "-bar-" you would naturally
 *			expect "1 -bar- 2" as the output, but you
 *			got "1  2-bar-".  That is, include file
 *			processing was postponed until all macros
 *			had been expanded.  The macro gpbc() was
 *			at fault.  I added bb, bbstack[], and the
 *			code in main.c and serv.c that maintains
 *			them, in order to work around this bug.
 *
 * Apr 12 1991 RAOK	inspect() didn't handle overflow well.
 *			Added the automatically maintained macro
 *			__FILE__, just as in C.  To suppress it,
 *			define NO__FILE.  At some point, $# had
 *			been made to return a value that was off
 *			by one; it now agrees with SysV M4.
 *
 * Aug 13 1990 RAOK	The System V expr() has three arguments:
 *			expression [, radix:10 [, mindigits: 1]]
 *			Brought in my int2str() and wrote pbrad()
 *			to make this work here.  With the wrong #
 *			of args, acts like System V.
 *
 * Aug 11 1990 RAOK	Told expr.c about the Pascal operators
 *			not, div, mod, and, or
 *			so that Pascal constant expressions could
 *			be evaluated.  (It still doesn't handle
 *			floats.)  Fixed a mistake in 'character's.
 *
 * Apr 23 1988 RAOK	Sped it up, mainly by making putback() and
 *			chrsave() into macros.
 *			Finished the -o option (was half done).
 *			Added the System V -e (interactive) option.
 *
 * Jan 28 1986 Oz	Break the whole thing into little
 *			pieces, for easier (?) maintenance.
 *
 * Dec 12 1985 Oz	Optimize the code, try to squeeze
 *			few microseconds out.. [didn't try very hard]
 *
 * Dec 05 1985 Oz	Add getopt interface, define (-D),
 *			undefine (-U) options.
 *
 * Oct 21 1985 Oz	Clean up various bugs, add comment handling.
 *
 * June 7 1985 Oz	Add some of SysV m4 stuff (m4wrap, pushdef,
 *			popdef, decr, shift etc.).
 *
 * June 5 1985 Oz	Initial cut.
 *
 * Implementation Notes:
 *
 * [1]	PD m4 uses a different (and simpler) stack mechanism than the one 
 *	described in Software Tools and Software Tools in Pascal books. 
 *	The triple stack nonsense is replaced with a single stack containing 
 *	the call frames and the arguments. Each frame is back-linked to a 
 * 	previous stack frame, which enables us to rewind the stack after 
 * 	each nested call is completed. Each argument is a character pointer 
 *	to the beginning of the argument string within the string space.
 *	The only exceptions to this are (*) arg 0 and arg 1, which are
 * 	the macro definition and macro name strings, stored dynamically
 *	for the hash table.
 *
 *	    .					   .
 *	|   .	|  <-- sp			|  .  |
 *	+-------+				+-----+
 *	| arg 3 ------------------------------->| str |
 *	+-------+				|  .  |
 *	| arg 2 --------------+ 		   .
 *	+-------+	      |
 *	    *		      |			|     |
 *	+-------+	      | 		+-----+
 *	| plev	|  <-- fp     +---------------->| str |
 *	+-------+				|  .  |
 *	| type	|				   .
 *	+-------+
 *	| prcf	-----------+		plev: paren level
 *	+-------+  	   |		type: call type
 *	|   .	| 	   |		prcf: prev. call frame
 *	    .	   	   |
 *	+-------+	   |
 *	|	<----------+
 *	+-------+
 *
 * [2]	We have three types of null values:
 *
 *		nil  - nodeblock pointer type 0
 *		null - null string ("")
 *		NULL - Stdio-defined NULL
 *
 */

char buf[BUFSIZE];		/* push-back buffer	       */
char *bp = buf; 		/* first available character   */
char *bb = buf;			/* buffer beginning            */
char *endpbb = buf+BUFSIZE;	/* end of push-back buffer     */
stae mstack[STACKMAX+1]; 	/* stack of m4 machine         */
char strspace[STRSPMAX+1];	/* string space for evaluation */
char *ep = strspace;		/* first free char in strspace */
char *endest= strspace+STRSPMAX;/* end of string space	       */
int sp; 			/* current m4  stack pointer   */
int fp; 			/* m4 call frame pointer       */
char *bbstack[MAXINP];		/* stack where bb is saved     */
FILE *infile[MAXINP];		/* input file stack (0=stdin)  */
FILE *outfile[MAXOUT];		/* diversion array(0=bitbucket)*/
FILE *active;			/* active output file pointer  */
int ilevel = 0; 		/* input file stack pointer    */
int oindex = 0; 		/* diversion index..	       */
char *null = "";                /* as it says.. just a null..  */
char *m4wraps = "";             /* m4wrap string default..     */
char lquote = LQUOTE;		/* left quote character  (`)   */
char rquote = RQUOTE;		/* right quote character (')   */
char vquote = VQUOTE;		/* verbatim quote character ^V */
char scommt = SCOMMT;		/* start character for comment */
char ecommt = ECOMMT;		/* end character for comment   */
int strip = 0;			/* throw away comments?        */

/*  Definitions of diversion files.  The last 6 characters MUST be
    "XXXXXX" -- that is a requirement of mktemp().  The character
    '0' is to be replaced by the diversion number; we assume here
    that it is just before the Xs.  If not, you will have to alter
    the definition of UNIQUE.
*/

#if unix
static char DIVNAM[] = "/tmp/m40XXXXXX";
#else
#if vms
static char DIVNAM[] = "sys$login:m40XXXXXX";
#else
static char DIVNAM[] = "M40XXXXXX";	/* was \M4, should it be \\M4? */
#endif
#endif
int UNIQUE = sizeof DIVNAM - 7;	/* where to change m4temp.     */
char *m4temp;			/* filename for diversions     */
extern char *mktemp();


void cantread(s)
    char *s;
    {
	fprintf(stderr, "m4: %s: ", s);
	error("cannot open for input.");
    }


/*  initkwds()
    initialises the hash table to contain all the m4 built-in functions.
    The original version breached module boundaries, but there did not
    seem to be any benefit in that.
*/
static void initkwds()
    {
	register int i;
	static struct { char *name; int type; } keyword[] =
	    {
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
		"m4trim",	TRIMTYPE,
		"defquote",	DEFQTYPE,
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
#if unix || vms
		"syscmd",       SYSCTYPE,
		"sysval",       SYSVTYPE,
#endif
#if unix
		"unix",         MACRTYPE,
#else
#if vms
		"vms",          MACRTYPE,
#endif
#endif
		(char*)0,	0
	    };

	for (i = 0; keyword[i].type != 0; i++)
	    addkywd(keyword[i].name, keyword[i].type);
    }


/*  inspect(Name)
    Build an input token.., considering only those which start with
    [A-Za-z_].  This is fused with lookup() to speed things up.
    name must point to an array of at least MAXTOK characters.
*/
ndptr inspect(name)
    char *name;
    {
	register char *tp = name;
	register char *etp = name+(MAXTOK-1);
	register int c;
	register unsigned long h = 0;
	register ndptr p;

	while (is_sym2(c = gpbc())) {
	    if (tp == etp) error("m4: token too long");
	    *tp++ = c, h = (h << 5) + h + c;
	}
	putback(c);
	*tp = EOS;
	for (p = hashtab[h%HASHSIZE]; p != nil; p = p->nxtptr)
	    if (strcmp(name, p->name) == 0)
		return p;
	return nil;
    }


/*
 * macro - the work horse..
 *
 */
void macro()
    {
	char token[MAXTOK];
	register int t;
	register FILE *op = active;
	static char ovmsg[] = "m4: internal stack overflow";

	for (;;) {
	    t = gpbc();
	    if (is_sym1(t)) {
		register char *s;
		register ndptr p;

		putback(t);
		if ((p = inspect(s = token)) == nil) {
		    if (sp < 0) {
			while (t = *s++) putc(t, op);
		    } else {
			while (t = *s++) chrsave(t);
		    }
		} else {
		    /* real thing.. First build a call frame */
		    if (sp >= STACKMAX-6) error(ovmsg);
		    mstack[1+sp].sfra = fp;		/* previous call frm */
		    mstack[2+sp].sfra = p->type;	/* type of the call  */
		    mstack[3+sp].sfra = 0;		/* parenthesis level */
		    fp = sp+3;				/* new frame pointer */
		    /* now push the string arguments */
		    mstack[4+sp].sstr = p->defn;	/* defn string */
		    mstack[5+sp].sstr = p->name;	/* macro name  */
		    mstack[6+sp].sstr = ep;		/* start next.. */
		    sp += 6;

		    t = gpbc();
		    putback(t);
		    if (t != LPAREN) { putback(RPAREN); putback(LPAREN); }
		}
	    } else
	    if (t == EOF) {
		if (sp >= 0) error("m4: unexpected end of input");
		if (--ilevel < 0) break;		/* all done thanks */
#ifndef	NO__FILE
		remhash("__FILE__", TOP);
#endif
		bb = bbstack[ilevel+1];
		(void) fclose(infile[ilevel+1]);
	    } else
	    /* non-alpha single-char token seen..
		[the order of else if .. stmts is important.] 
	    */
	    if (t == lquote) {				/* strip quotes */
		register int nlpar;

		for (nlpar = 1; ; ) {
		    t = gpbc();
		    if (t == rquote) {
			if (--nlpar == 0) break;
		    } else
		    if (t == lquote) {
			nlpar++;
		    } else {
			if (t == vquote) t = gpbc();
			if (t == EOF) {
			    error("m4: missing right quote");
			}
		    }
		    if (sp < 0) {
			putc(t, op);
		    } else {
			chrsave(t);
		    }
		}
	    } else
	    if (sp < 0) {			/* not in a macro at all */
		if (t != scommt) {		/* not a comment, so */
		    putc(t, op);		/* copy it to output */
		} else
		if (strip) {			/* discard a comment */
		    do {
			t = gpbc();
		    } while (t != ecommt && t != EOF);
		} else {			/* copy comment to output */
		    do {
			putc(t, op);
			t = gpbc();
		    } while (t != ecommt && t != EOF);
		    putc(t, op);
		    /*  A note on comment handling:  this is NOT robust.
		    |   We should do something safe with comments that
		    |   are missing their ecommt termination.
		    */
		}
	    } else
	    switch (t) {
		/*  There is a peculiar detail to notice here.
		    Layout is _always_ discarded after left parentheses,
		    but it is only discarded after commas if they separate
		    arguments.  For example,
		    define(foo,`|$1|$2|')
		    foo( a, b)		=> |a|b|
		    foo(( a ), ( b ))	=> |(a )|(b )|
		    foo((a, x), (b, y))	=> |(a, x)|(b, y)|
		    I find this counter-intuitive, and would expect the code
		    for LPAREN to read something like this:

		    if (PARLEV == 0) {
			(* top level left parenthesis: skip layout *)
			do t = gpbc(); while (is_blnk(t));
			putback(t);
		    } else {
			(* left parenthesis inside an argument *)
			chrsave(t);
		    }
		    PARLEV++;

		    However, it turned out that Oz wrote the actual code
		    very carefully to mimic the behaviour of "real" m4;
		    UNIX m4 really does skip layout after all left parens
		    but only some commas in just this fashion.  Sigh.
		*/
		case LPAREN:
		    if (PARLEV > 0) chrsave(t);
		    do t = gpbc(); while (is_blnk(t));	/* skip layout */
		    putback(t);
		    PARLEV++;
		    break;

		case COMMA:
		    if (PARLEV == 1) {
			chrsave(EOS);		/* new argument   */
			if (sp >= STACKMAX) error(ovmsg);
			do t = gpbc(); while (is_blnk(t)); /* skip layout */
			putback(t);
			mstack[++sp].sstr = ep;
		    } else {
			chrsave(t);
		    }
		    break;

		case RPAREN:
		    if (--PARLEV > 0) {
			chrsave(t);
		    } else {
			char **argv = (char **)(mstack+fp+1);
			int    argc = sp-fp;
#if	unix | vms
			static int sysval;
#endif

			chrsave(EOS);		/* last argument */
			if (sp >= STACKMAX) error(ovmsg);
#ifdef	DEBUG
			fprintf(stderr, "argc = %d\n", argc);
			for (t = 0; t < argc; t++)
			    fprintf(stderr, "argv[%d] = %s\n", t, argv[t]);
#endif
			/*  If argc == 3 and argv[2] is null, then we
			    have a call like `macro_or_builtin()'.  We
			    adjust argc to avoid further checking..
			*/
			if (argc == 3 && !argv[2][0]) argc--;

			switch (CALTYP & ~STATIC) {
			    case MACRTYPE:
				expand(argv, argc);
				break;

			    case DEFITYPE:		/* define(..) */
				for (; argc > 2; argc -= 2, argv += 2)
				    dodefine(argv[2], argc > 3 ? argv[3] : null);
				break;

			    case PUSDTYPE:		/* pushdef(..) */
				for (; argc > 2; argc -= 2, argv += 2)
				    dopushdef(argv[2], argc > 3 ? argv[3] : null);
				break;

			    case DUMPTYPE:
				dodump(argv, argc);
				break;

			    case EXPRTYPE:		/* eval(Expr) */
				{   /* evaluate arithmetic expression */
				    /* eval([val: 0[, radix:10 [,min: 1]]]) */
				    /* excess arguments are ignored */
				    /* eval() with no arguments returns 0 */
				    /* this is based on V.3 behaviour */
				    int min_digits = 1;
				    int radix = 10;
				    long int value = 0;

				    switch (argc) {
					default:
					    /* ignore excess arguments */
					case 5:
					    min_digits = expr(argv[4]);
					case 4:
					    radix = expr(argv[3]);
					case 3:
					    value = expr(argv[2]);
					case 2:
					    break;
				    }
				    pbrad(value, radix, min_digits);
				}
				break;

			    case IFELTYPE:		/* ifelse(X,Y,IFX=Y,Else) */
				doifelse(argv, argc);
				break;

			    case IFDFTYPE:		/* ifdef(Mac,IfDef[,IfNotDef]) */
				/* select one of two alternatives based on the existence */
				/* of another definition */
				if (argc > 3) {
				    if (lookup(argv[2]) != nil) {
					pbstr(argv[3]);
				    } else
				    if (argc > 4) {
					pbstr(argv[4]);
				    }
				}
				break;

			    case LENGTYPE:		/* len(Arg) */
				/* find the length of the argument */
				pbnum(argc > 2 ? strlen(argv[2]) : 0);
				break;

			    case INCRTYPE:		/* incr(Expr) */
				/* increment the value of the argument */
				if (argc > 2) pbnum(expr(argv[2]) + 1);
				break;

			    case DECRTYPE:		/* decr(Expr) */
				/* decrement the value of the argument */
				if (argc > 2) pbnum(expr(argv[2]) - 1);
				break;

#if unix || vms
			    case SYSCTYPE:		/* syscmd(Command) */
				/* execute system command */
				/* Make sure m4 output is NOT interrupted */
				fflush(stdout);
				fflush(stderr);

				if (argc > 2) sysval = system(argv[2]);
				break;

			    case SYSVTYPE:		/* sysval() */
				/* return value of the last system call.  */
				pbnum(sysval);
				break;
#endif

			    case INCLTYPE:		/* include(File) */
				for (t = 2; t < argc; t++)
				    if (!doincl(argv[t])) cantread(argv[t]);
				break;

			    case SINCTYPE:		/* sinclude(File) */
				for (t = 2; t < argc; t++)
				    (void) doincl(argv[t]);
				break;

#ifdef EXTENDED
			    case PASTTYPE:		/* paste(File) */
				for (t = 2; t < argc; t++)
				    if (!dopaste(argv[t])) cantread(argv[t]);
				break;

			    case SPASTYPE:		/* spaste(File) */
				for (t = 2; t < argc; t++)
				    (void) dopaste(argv[t]);
				break;

			    case TRIMTYPE:		/* m4trim(Source,..) */
				if (argc > 2) m4trim(argv, argc);
				break;

			    case DEFQTYPE:		/* defquote(Mac,...) */
				dodefqt(argv, argc);
				break;

			    case QUTRTYPE:		/* <quote>(text...) */
				doqutr(argv, argc);
				break;
#endif

			    case CHNQTYPE:		/* changequote([Left[,Right]]) */
				dochq(argv, argc);
				break;

			    case CHNCTYPE:		/* changecom([Left[,Right]]) */
				dochc(argv, argc);
				break;

			    case SUBSTYPE:		/* substr(Source[,Offset[,Length]]) */
				/* select substring */
				if (argc > 3) dosub(argv, argc);
				break;

			    case SHIFTYPE:		/* shift(~args~) */
				/* push back all arguments except the first one */
				/* (i.e.  skip argv[2]) */
				if (argc > 3) {
				    for (t = argc-1; t > 3; t--) {
					pbqtd(argv[t]);
					putback(',');
				    }
				    pbqtd(argv[3]);
				}
				break;

			    case DIVRTYPE:		/* divert(N) */
				if (argc > 2 && (t = expr(argv[2])) != 0) {
				    dodiv(t);
				} else {
				    active = stdout;
				    oindex = 0;
				}
				op = active;
				break;

			    case UNDVTYPE:		/* undivert(N...) */
				doundiv(argv, argc);
				op = active;
				break;

			    case DIVNTYPE:		/* divnum() */
				/* return the number of current output diversion */
				pbnum(oindex);
				break;

			    case UNDFTYPE:		/* undefine(..) */
				/* undefine a previously defined macro(s) or m4 keyword(s). */
				for (t = 2; t < argc; t++) remhash(argv[t], ALL);
				break;

			    case POPDTYPE:		/* popdef(Mac...) */
				/* remove the topmost definitions of macro(s) or m4 keyword(s). */
				for (t = 2; t < argc; t++) remhash(argv[t], TOP);
				break;

			    case MKTMTYPE:		/* maketemp(Pattern) */
				/* create a temporary file */
				if (argc > 2) pbstr(mktemp(argv[2]));
				break;

			    case TRNLTYPE:		/* translit(Source,Dom,Rng) */
				/* replace all characters in the source string that */
				/* appears in the "from" string with the corresponding */
				/* characters in the "to" string. */

				if (argc > 3) {
				    char temp[MAXTOK];

				    if (argc > 4)
					map(temp, argv[2], argv[3], argv[4]);
				    else
					map(temp, argv[2], argv[3], null);
				    pbstr(temp);
				} else if (argc > 2)
				    pbstr(argv[2]);
				break;

			    case INDXTYPE:		/* index(Source,Target) */
				/* find the index of the second argument string in */
				/* the first argument string. -1 if not present. */
				pbnum(argc > 3 ? indx(argv[2], argv[3]) : -1);
				break;

			    case ERRPTYPE:		/* errprint(W,...,W) */
				/* print the arguments to stderr file */
				for (t = 2; t < argc; t++) fprintf(stderr, "%s ", argv[t]);
				fprintf(stderr, "\n");
				break;

			    case DNLNTYPE:		/* dnl() */
				/* eat upto and including newline */
				while ((t = gpbc()) != '\n' && t != EOF) ;
				break;

			    case M4WRTYPE:		/* m4wrap(AtExit) */
				/* set up for wrap-up/wind-down activity.   */
				/* NB: if there are several calls to m4wrap */
				/* only the last is effective; strange, but */
				/* that's what System V does.               */
				m4wraps = argc > 2 ? strsave(argv[2]) : null;
				break;

			    case EXITTYPE:		/* m4exit(Expr) */
				/* immediate exit from m4.  */
				killdiv();		/* mustn't forget that one! */
				exit(argc > 2 ? expr(argv[2]) : 0);
				break;

			    case DEFNTYPE:		/* defn(Mac) */
				for (t = 2; t < argc; t++)
				    dodefn(argv[t]);
				break;

			    default:
				error("m4: major botch in eval.");
				break;
			}

			ep = PREVEP;		/* flush strspace */
			sp = PREVSP;		/* previous sp..  */
			fp = PREVFP;		/* rewind stack... */
		    }
		    break;

		default:
		    chrsave(t);			/* stack the char */
		    break;
	    }
	}
    }


int main(argc, argv)
    int argc;
    char **argv;
    {
	register int c;
	register int n;
	char *p;

#ifdef	SIGINT
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, onintr);
#endif

	/*  Initialise the chtype[] table.
	    '0' .. '9' -> 1..10
	    'A' .. 'Z' -> 11..37
	    'a' .. 'z' -> 11..37
	    '_' -> 38
	    all other characters -> 0
	*/
	for (c = EOF; c <= UCHAR_MAX; c++) chtype[c - EOF] = 0;
	for (c =  1, p = "0123456789"; *p; p++, c++)
	    chtype[*(unsigned char *)p - EOF] = c;
	for (c = 11, p = "abcdefghijklmnopqrstuvwxyz"; *p; p++, c++)
	    chtype[*(unsigned char *)p - EOF] = c;
	for (c = 11, p = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"; *p; p++, c++)
	    chtype[*(unsigned char *)p - EOF] = c;
	chtype['_' - EOF] = 38;

#ifdef NONZEROPAGES
	/*  If your system does not initialise global variables to  */
	/*  0 bits, do it here.					    */
	for (n = 0; n < HASHSIZE; n++) hashtab[n] = nil;
	for (n = 0; n < MAXOUT; n++) outfile[n] = NULL;
#endif
	initkwds();

	while ((c = getopt(argc, argv, "cetD:U:o:B:H:S:T:")) != EOF) {
	    switch (c) {
#if 0
		case 's':		/* enable #line sync in output */
		    fprintf(stderr, "m4: this version does not support -s\n");
		    exit(2);
#endif

		case 'c':		/* strip comments */
		    strip ^= 1;
		    break;

		case 'e':		/* interactive */
		    (void) signal(SIGINT, SIG_IGN);
		    setbuf(stdout, NULL);
		    break;

		case 'D':               /* define something..*/
		    for (p = optarg; *p && *p != '='; p++) ;
		    if (*p) *p++ = EOS;
		    dodefine(optarg, p);
		    break;

		case 'U':               /* undefine...       */
		    remhash(optarg, TOP);
		    break;

		case 'B': case 'H':	/* System V compatibility */
		case 'S': case 'T':	/* ignore them */
		    break;

		case 'o':		/* specific output   */
		    if (!freopen(optarg, "w", stdout)) {
			perror(optarg);
			exit(1);
		    }
		    break;

		case '?':
		default:
			usage();
	    }
	}

	active = stdout;		/* default active output     */
	m4temp = mktemp(DIVNAM);	/* filename for diversions   */

	sp = -1;			/* stack pointer initialized */
	fp = 0; 			/* frame pointer initialized */

	if (optind == argc) {		/* no more args; read stdin  */
	    infile[0] = stdin;		/* default input (naturally) */
#ifndef	NO__FILE
	    dodefine("__FILE__", "-");	/* Helas */
#endif
	    macro();			/* process that file         */
	} else				/* file names in commandline */
	for (; optind < argc; optind++) {
	    char *name = argv[optind];	/* next file name            */
	    if(name[1] == 0 && name[0] == '-')
		infile[0] = stdin;
	    else
	    	infile[0] = fopen(name, "r");
	    if (!infile[0]) cantread(name);
	    sp = -1;			/* stack pointer initialized */
	    fp = 0; 			/* frame pointer initialized */
	    ilevel = 0;			/* reset input file stack ptr*/
#ifndef	NO__FILE
	    dodefine("__FILE__", name);
#endif
	    macro();
	    fclose(infile[0]);
	}

	if (*m4wraps) { 		/* anything for rundown ??   */
	    ilevel = 0;			/* in case m4wrap includes.. */
	    putback(EOF);		/* eof is a must !!	     */
	    pbstr(m4wraps); 		/* user-defined wrapup act   */
	    macro();			/* last will and testament   */
	}

	if (active != stdout)
	    active = stdout;		/* reset output just in case */

	for (n = 1; n < MAXOUT; n++)	/* default wrap-up: undivert */
	    if (outfile[n] != NULL) getdiv(n);

	if (outfile[0] != NULL) {	/* remove bitbucket if used  */
	    (void) fclose(outfile[0]);
	    m4temp[UNIQUE] = '0';
#if unix
	    (void) unlink(m4temp);
#else
	    (void) remove(m4temp);
#endif
	}
	exit(0);
	return 0;
    }

