/*  File   : serv.c
    Author : Ozan Yigit
    Updated: 4 May 1992
    Defines: Principal built-in macros for PD M4.
*/

#include "mdef.h"
#include "extr.h"
#include "ourlims.h"

#define	ucArgv(n) ((unsigned char *)argv[n])

/*  26-Mar-1993		Made m4trim() 8-bit clean.
*/

/*  expand(<DS FN A1 ... An>)
	     0  1  2      n+1		-- initial indices in argv[]
	    -1  0  1      n		-- after adjusting argv++, argc--
    This expands a user-defined macro;  FN is the name of the macro, DS
    is its definition string, and A1 ... An are its arguments.
*/
void expand(argv, argc)
    char **argv;
    int argc;
    {
	register char *t;
	register char *p;
	register int n;

#ifdef	DEBUG
	fprintf(stderr, "expand(%s,%d)\n", argv[1], argc);
#endif
	argc--;			/* discount definition string (-1th arg) */
	t = *argv++;		/* definition string as a whole */
	for (p = t; *p++; ) ;
	p -= 2;			/* points to last character of definition */
	while (p > t) {		/* if definition is empty, fails at once  */
	    if (*--p != ARGFLAG) {
		putback(p[1]);
	    } else {
		switch (p[1]) {
		    case '#':
			pbnum(argc-1);
			break;
		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
			if ((n = p[1]-'0') < argc) pbstr(argv[n]);
			break;
		    case '*':		/* push all arguments back */
			for (n = argc-1; n > 1; n--) {
			    pbstr(argv[n]);
			    putback(',');
			}
			pbstr(argv[1]);
			break;
		    case '@':		/* push arguments back quoted */
			for (n = argc-1; n > 1; n--) {
			    pbqtd(argv[n]);
			    putback(',');
			}
			pbqtd(argv[1]);
			break;
		    case '$':		/* $$ => $ */
			break;
		    default:
			putback(p[1]);
			putback(p[0]);
			break;
		}
		p--;
	    }
	}
	if (p == t) putback(*p);		/* do last character */
    }


static char nuldefmsg[] = "m4: defining null name.";
static char recdefmsg[] = "m4: macro defined as itself.";

/*  dodefine(Name, Definition)
    install Definition as the only definition of Name in the hash table.
 */
void dodefine(name, defn)
    register char *name;
    register char *defn;
    {
	register ndptr p;

	if (!name || !*name) error(nuldefmsg);
	if (strcmp(name, defn) == 0) error(recdefmsg);
#ifdef	DEBUG
	fprintf(stderr, "define(%s,--)\n", name);
#endif
	if ((p = lookup(name)) == nil) {
	    p = addent(name);
	} else
	if (p->defn != null) {		/* what if p->type & STATIC ? */
	    free(p->defn);
	}
	p->defn = !defn || !*defn ? null : strsave(defn);
	p->type = MACRTYPE;
    }


/*  dopushdef(Name, Definition)
    install Definition as the *first* definition of Name in the hash table,
    but do not remove any existing definitions.  The new definition will
    hide any old ones until a popdef() removes it.
*/
void dopushdef(name, defn)
    register char *name;
    register char *defn;
    {
	register ndptr p;

	if (!name || !*name) error(nuldefmsg);
	if (strcmp(name, defn) == 0) error(recdefmsg);
#ifdef	DEBUG
	fprintf(stderr, "pushdef(%s,--)\n", name);
#endif
	p = addent(name);
	p->defn = !defn || !*defn ? null : strsave(defn);
	p->type = MACRTYPE;
    }


/*  dodefn(Name)
    push back a *quoted* copy of Name's definition.
*/
void dodefn(name)
    char *name;
    {
	register ndptr p;

	if ((p = lookup(name)) != nil && p->defn != null) pbqtd(p->defn);
    }


/*  dodump(<? dump>)		dump all definition in the hash table
    dodump(<? dump F1 ... Fn>)	dump the definitions of F1 ... Fn in that order
    The requested definitions are written to stderr.  What happens to names
    which have a built-in (numeric) definition?
*/
void dodump(argv, argc)
    register char **argv;
    register int argc;
    {
	register int n;
	ndptr p;
	static char dumpfmt[] = "define(`%s',\t`%s')\n";

	if (argc > 2) {
	    for (n = 2; n < argc; n++)
		if ((p = lookup(argv[n])) != nil)
		    fprintf(stderr, dumpfmt, p->name, p->defn);
	} else {
	    for (n = 0; n < HASHSIZE; n++)
		for (p = hashtab[n]; p != nil; p = p->nxtptr)
		    fprintf(stderr, dumpfmt, p->name, p->defn);
	}
    }


/*  doifelse(<? ifelse {x y ifx=y}... [else]>)
	      0 1       2 3 4         [2 when we get to it]
*/
void doifelse(argv, argc)
    register char **argv;
    register int argc;
    {
	for (; argc >= 5; argv += 3, argc -= 3)
	    if (strcmp(argv[2], argv[3]) == 0) {
		pbstr(argv[4]);
		return;
	    }
	if (argc >= 3) pbstr(argv[2]);
    }


/*  doinclude(FileName)
    include a given file.
*/
int doincl(FileName)
    char *FileName;
    {
	if (ilevel+1 == MAXINP) error("m4: too many include files.");
#ifdef	DEBUG
	fprintf(stderr, "include(%s)\n", FileName);
#endif
	if ((infile[ilevel+1] = fopen(FileName, "r")) != NULL) {
#ifndef	NO__FILE
	    dopushdef("__FILE__", FileName);
#endif
	    bbstack[ilevel+1] = bb;
	    bb = bp;
	    ilevel++;
	    return 1;
	} else {
	    return 0;
	}
    }


#ifdef EXTENDED
/*  dopaste(FileName)
    copy a given file to the output stream without any macro processing.
*/
int dopaste(FileName)
    char *FileName;
    {
	register FILE *pf;
	register FILE *afil = active;
	register int c;

	if ((pf = fopen(FileName, "r")) != NULL) {
	    while ((c = getc(pf)) != EOF) putc(c, afil);
	    (void) fclose(pf);
	    return 1;
	} else {
	    return 0;
	}
    }
#endif


/*  dochq(<? changequote [left [right [verbatim]]]>)
	   0 1            2     3      4
    change the quote characters; to single characters only.
    Empty arguments result in no change for that parameter.
    Missing arguments result in defaults:
	changequote		=> ` ' ^V
	changequote(q)		=> q q ^V
	changequote(l,r)	=> l r ^V
	changequote(l,r,v)	=> l r v
    There isn't any way of switching the verbatim-quote off,
    but if you make it the same as the right quote it won't
    be able to do anything (we check for R, L, V in that order).
*/
void dochq(argv, argc)
    register char **argv;
    register int argc;
    {
	if (argc > 2) {
	    if (*argv[2]) lquote = *argv[2];
	    if (argc > 3) {
		if (*argv[3]) rquote = *argv[3];
		    if (argc > 4 && *argv[4]) vquote = *argv[4];
	    } else {
		rquote = lquote;
	    }
	} else {
	    lquote = LQUOTE;
	    rquote = RQUOTE;
	    vquote = VQUOTE;
	}
    }


/*  dochc(<? changecomment [left [right]]>)
           0 1		    2     3
    change the comment delimiters; to single characters only.
*/
void dochc(argv, argc)
    register char **argv;
    register int argc;
    {
	if (argc > 2) {
	    if (*argv[2]) scommt = *argv[2];
	    if (argc > 3) {
		if (*argv[3]) ecommt = *argv[3];
	    } else {
		ecommt = ECOMMT;
	    }
	} else {
	    scommt = '\0';	/* assuming no nulls in input */
	    ecommt = '\0';
	}
    }


/*  dodivert - divert the output to a temporary file
*/
void dodiv(n)
    register int n;
    {
	if (n < 0 || n >= MAXOUT) n = 0;	/* bitbucket */
	if (outfile[n] == NULL) {
	    m4temp[UNIQUE] = '0' + n;
	    if ((outfile[n] = fopen(m4temp, "w")) == NULL)
		error("m4: cannot divert.");
	}
	oindex = n;
	active = outfile[n];
    }


/*  doundivert - undivert a specified output, or all
 *              other outputs, in numerical order.
*/
void doundiv(argv, argc)
    register char **argv;
    register int argc;
    {
	register int ind;
	register int n;

	if (argc > 2) {
	    for (ind = 2; ind < argc; ind++) {
		n = expr(argv[ind]);
		if (n > 0 && n < MAXOUT && outfile[n] != NULL) getdiv(n);
	    }
	} else {
	    for (n = 1; n < MAXOUT; n++)
		if (outfile[n] != NULL) getdiv(n);
	}
    }


/*  dosub(<? substr {offset} [{length}]>)
    The System V Interface Definition does not say what happens when the
    offset or length are out of range.  I have chosen to force them into
    range, with the result that unlike the former version of this code,
    dosub cannot be tricked into SIGSEGV.

    BUG:  This is not 8-bit clean yet.
*/
void dosub(argv, argc)
    char **argv;
    int argc;
    {
	register int nc;		/* number of characters */
	register char *ap = argv[2];	/* target string */
	register int al = strlen(ap);	/* its length */
	register int df = expr(argv[3]);/* offset */

	if (df < 0) df = 0; else	/* force df back into the range */
	if (df > al) df = al;		/* 0 <= df <= al */
	al -= df;			/* now al limits nc */

	if (argc >= 5) {		/* nc is provided */
	    nc = expr(argv[4]);
	    if (nc < 0) nc = 0; else	/* force nc back into the range */
	    if (nc > al) nc = al;	/* 0 <= nc <= strlen(ap)-df */
	} else {
	    nc = al;			/* default is all rest of ap */
	}
	ap += df + nc;
	while (--nc >= 0) putback(*--ap);
    }


/* map(dest, src, from, to)
    map every character of src that is specified in from 
    into "to" and replace in dest. (source "src" remains untouched)

    This is a standard implementation of Icon's map(s,from,to) function.
    Within mapvec, we replace every character of "from" with the
    corresponding character in "to".  If "to" is shorter than "from",
    then the corresponding entries are null, which means that those
    characters disappear altogether.  Furthermore, imagine a call like
    map(dest, "sourcestring", "srtin", "rn..*"). In this case, `s' maps
    to `r', `r' maps to `n' and `n' maps to `*'. Thus, `s' ultimately
    maps to `*'. In order to achieve this effect in an efficient manner
    (i.e. without multiple passes over the destination string), we loop
    over mapvec, starting with the initial source character.  If the
    character value (dch) in this location is different from the source
    character (sch), sch becomes dch, once again to index into mapvec,
    until the character value stabilizes (i.e. sch = dch, in other words
    mapvec[n] == n).  Even if the entry in the mapvec is null for an
    ordinary character, it will stabilize, since mapvec[0] == 0 at all
    times.  At the end, we restore mapvec* back to normal where
    mapvec[n] == n for 0 <= n <= 127.  This strategy, along with the
    restoration of mapvec, is about 5 times faster than any algorithm
    that makes multiple passes over the destination string.
*/

void map(d, s, f, t)
    char *d, *s, *f, *t;
    {
	register unsigned char *dest = (unsigned char *)d;
	register unsigned char *src  = (unsigned char *)s;
	         unsigned char *from = (unsigned char *)f;
	register unsigned char *to   = (unsigned char *)t;
	register unsigned char *tmp;
	register unsigned char sch, dch;
	static   unsigned char mapvec[1+UCHAR_MAX] = {1};

	if (mapvec[0]) {
	    register int i;
	    for (i = 0; i <= UCHAR_MAX; i++) mapvec[i] = i;
	}
	if (src && *src) {
	    /* create a mapping between "from" and "to" */
	    if (to && *to)
		for (tmp = from; sch = *tmp++; ) mapvec[sch] = *to++;
	    else
		for (tmp = from; sch = *tmp++; ) mapvec[sch] = '\0';

	    while (sch = *src++) {
		while ((dch = mapvec[sch]) != sch) sch = dch;
		if (*dest = dch) dest++;
	    }
	    /* restore all the changed characters */
	    for (tmp = from; sch = *tmp++; ) mapvec[sch] = sch;
	}
	*dest = '\0';
    }


#ifdef	EXTENDED

/*  m4trim(<? m4trim [string [leading [trailing [middle [rep]]]]]>)
	    0 1       2       3        4         5       6
    
    (1) Any prefix consisting of characters in the "leading" set is removed.
	The default is " \t\n".
    (2) Any suffix consisting of characters in the "trailing" set is removed.
	The default is to be the same as leading.
    (3) Any block of consecutive characters in the "middle" set is replaced
	by the rep string.  The default for middle is " \t\n", and the
	default for rep is the first character of middle.
*/
void m4trim(argv, argc)
    char **argv;
    int argc;
    {
	static unsigned char repbuf[2] = " ";
	static unsigned char layout[] = " \t\n\r\f";
	unsigned char *string   = argc > 2 ? ucArgv(2) : repbuf+1;
	unsigned char *leading  = argc > 3 ? ucArgv(3) : layout;
	unsigned char *trailing = argc > 4 ? ucArgv(4) : leading;
	unsigned char *middle   = argc > 5 ? ucArgv(5) : trailing;
	unsigned char *rep      = argc > 6 ? ucArgv(6) :
						 (repbuf[0] = *middle, repbuf);
	static unsigned char sets[1+UCHAR_MAX];
#	define PREF 1
#	define SUFF 2
#	define MIDL 4
	register int i, n;

	for (i = UCHAR_MAX; i >= 0; ) sets[i--] = 0;
	while (*leading)  sets[*leading++]  |= PREF;
	while (*trailing) sets[*trailing++] |= SUFF;
	while (*middle)   sets[*middle++]   |= MIDL;

	while (*string && sets[*string]&PREF) string++;
	n = strlen((char *)string);
	while (n > 0 && sets[string[n-1]]&SUFF) n--;
	while (n > 0) {
	    i = string[--n];
	    if (sets[i]&MIDL) {
		pbstr((char*)rep);
		while (n > 0 && sets[string[n-1]]&MIDL) n--;
	    } else {
		putback(i);
	    }
	}
    }


/*  defquote(MacroName	# The name of the "quoter" macro to be defined.
	[, Opener	# default: "'".  The characters to place at the
			# beginning of the result.
	[, Separator	# default: ",".  The characters to place between
			# successive arguments.
	[, Closer	# default: same as Opener.  The characters to
			# place at the end of the result.
	[, Escape	# default: `'  The escape character to put in
			# front of things that need escaping.
	[, Default	# default: simple.  Possible values are
			# [lL].* = letter, corresponds to PLAIN1.
			# [dD].* = digit,  corresponds to PLAIN2.
			# [sS].* = simple, corresponds to SIMPLE.
			# [eE].* = escaped,corresponds to SCAPED.
			# .*,              corresponds to FANCY
	[, Letters	# default: `'.  The characters of type "L".
	[, Digits	# default: `'.  The characters of type "D".
	[, Simple	# default: `'.  The characters of type "S".
	[, Escaped	# default: `'.  The characters of type "E".
	{, Fancy	# default: none.  Each has the form `C'`Repr'
			# saying that the character C is to be represented
			# as Repr.  Can be used for trigraphs, \n, &c.
	}]]]]]]]]])

    Examples:
	defquote(DOUBLEQT, ")
	defquote(SINGLEQT, ')
    After these definitions,
	DOUBLEQT(a, " b", c)	=> "a,"" b"",c"
	SINGLEQT("Don't`, 'he said.") => '"Don''t, he said."'
    Other examples defining quote styles for several languages will be
    provided later.

    A quoter is represented in M4 by a special identifying number and a
    pointer to a Quoter record.  I expect that there will be few quoters
    but that they will need to go fairly fast.

*/

#define	PLAIN1	0
#define	PLAIN2	1
#define	SIMPLE	2
#define	SCAPED	3
#define	FANCY	4

struct Quoter
    {
	char *opener;
	char *separator;
	char *closer;
	char *escape;
	char *fancy[1+UCHAR_MAX];
	char class[1+UCHAR_MAX];
     };

void freeQuoter(q)
    struct Quoter *q;
    {
	int i;

	free(q->opener);
	free(q->separator);
	free(q->closer);
	free(q->escape);
	for (i = UCHAR_MAX; i >= 0; i--)
	    if (q->fancy[i]) free(q->fancy[i]);
	free((char *)q);
    }

/*  dodefqt(<
	0	?
	1	defquote
	2	MacroName
      [	3	Opener
      [ 4	Separator
      [ 5	Closer
      [ 6	Escape
      [ 7	Default
      [ 8	Letters
      [ 9	Digits
      [10	Simple
      [11	Escaped
      [11+i	Fancy[i]	]]]]]]]]]]>)
*/

void dodefqt(argv, argc)
    char **argv;
    int argc;
    {
	struct Quoter q, *r;
	register int i;
	register unsigned char *s;
	register int c;
	ndptr p;

	if (!(argc > 2 && *argv[2])) error(nuldefmsg);
	switch (argc > 7 ? argv[7][0] : '\0') {
	    case 'l': case 'L':	c = PLAIN1; break;
	    case 'd': case 'D': c = PLAIN2; break;
	    case 'e': case 'E': c = SCAPED; break;
	    case 'f': case 'F': c = FANCY;  break;
	    default:            c = SIMPLE;
	}
	for (i = UCHAR_MAX; --i >= 0; ) q.class[i] = c;
	for (i = UCHAR_MAX; --i >= 0; ) q.fancy[i] = 0;
	q.opener = strsave(argc > 3 ? argv[3] : "");
	q.separator = strsave(argc > 4 ? argv[4] : ",");
	q.closer = strsave(argc > 5 ? argv[5] : q.opener);
	q.escape = strsave(argc > 6 ? argv[6] : "");
	if (argc > 8)
	    for (s = (unsigned char *)argv[8]; c = *s++; )
		q.class[c] = PLAIN1;
	if (argc > 9)
	    for (s = (unsigned char *)argv[9]; c = *s++; )
		q.class[c] = PLAIN2;
	if (argc > 10)
	    for (s = (unsigned char *)argv[10]; c = *s++; )
		q.class[c] = SIMPLE;
	if (argc > 11)
	    for (s = (unsigned char *)argv[11]; c = *s++; )
		q.class[c] = SCAPED;
	for (i = 12; i < argc; i++) {
	    s = (unsigned char *)argv[i];
	    c = *s++;
	    q.fancy[c] = strsave((char *)s);
	    q.class[c] = FANCY;
	}
	/*  Now we have to make sure that the closing quote works.  */
	if ((c = q.closer[0]) && q.class[c] <= SIMPLE) {
	    if (q.escape[0]) {
		q.class[c] = SCAPED;
	    } else {
		char buf[3];
		buf[0] = c, buf[1] = c, buf[2] = '\0';
		q.fancy[c] = strsave(buf);
	    	q.class[c] = FANCY;
	    }
	}
	/*  We also have to make sure that the escape (if any) works.  */
	if ((c = q.escape[0]) && q.class[c] <= SIMPLE) {
	    q.class[c] = SCAPED;
	}
	r = (struct Quoter *)malloc(sizeof *r);
	if (r == NULL) error("m4: no more memory");
	*r = q;
        p = addent(argv[2]);
        p->defn = (char *)r;
        p->type = QUTRTYPE;
    }


/*  doqutr(<DB MN A1 ... An>)
	     0  1  2      n+1 argc
    argv[0] points to the struct Quoter.
    argv[1] points to the name of this quoting macro
    argv[2..argc-1] point to the arguments.
    This applies a user-defined quoting macro.  For example, we could
    define a macro to produce Prolog identifiers:
	defquote(plid, ', , ', , simple,
	    abcdefghijklmnopqrstuvwxyz,
	    ABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789)

    After doing that,
	plid(foo)		=> foo
	plid(*)			=> '*'
	plid(Don't)		=> 'Don''t'
	plid(foo,)		=> 'foo'
*/
void doqutr(argv, argc)
    char **argv;
    int argc;
    /* DEFINITION-BLOCK MacroName Arg1 ... Argn
       0                1         2        n-1   argc
    */
    {
	struct Quoter *r = (struct Quoter *)argv[0];
	char *p;
	register unsigned char *b, *e;
	int i;
	register int c;

	for (;;) {			/* does not actually loop */
	    if (argc != 3) break;
	    b = ucArgv(2);
	    e = b + strlen((char*)b);
	    if (e == b) break;
	    if (r->class[*b++] != PLAIN1) break;
	    while (b != e && r->class[*b] <= PLAIN2) b++;
	    if (b != e) break;
	    pbstr(argv[2]);		
	    return;
	}

	p = r->closer;
	if (argc < 3) {
	    pbstr(p);
	} else
	for (i = argc-1; i >= 2; i--) {
	    pbstr(p);
	    b = ucArgv(i);
	    e = b+strlen((char *)b);
	    while (e != b)
		switch (r->class[c = *--e]) {
		    case FANCY:
			p = r->fancy[c];
			if (p) {
			    pbstr(p);
			} else {
			    pbrad(c, 8, 1);
			    pbstr(r->escape);
			}
			break;
		    case SCAPED:
			putback(c);
			pbstr(r->escape);
			break;
		    default:
			putback(c);
			break;
	        }
	    p = r->separator;
	}
	pbstr(r->opener);
    }

#endif
