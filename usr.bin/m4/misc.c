/*  File   : misc.c
    Author : Ozan Yigit
    Updated: 26-Mar-1993
    Purpose: Miscellaneous support code for PD M4.
*/

#include "mdef.h"
#include "extr.h"
#include "ourlims.h"

#ifdef	DUFFCP

/*  This version of the ANSI standard function memcpy()
    uses Duff's Device (tm Tom Duff)  to unroll the copying loop:
	while (count-- > 0) *to++ = *from++;
*/
void memcpy(to, from, count)
    register char *from, *to;
    register int count;
    {
	if (count > 0) {
	    register int loops = (count+8-1) >> 3;	/* div 8 round up */

	    switch (count & (8-1)) {	/* mod 8 */
	        case 0: do {	*to++ = *from++;
		case 7:		*to++ = *from++;
		case 6:		*to++ = *from++;
		case 5:		*to++ = *from++;
		case 4:		*to++ = *from++;
		case 3:		*to++ = *from++;
		case 2:		*to++ = *from++;
		case 1:		*to++ = *from++;
			} while (--loops > 0);
	    }
	}
    }

#endif


/*  strsave(s)
    return a new malloc()ed copy of s -- same as V.3's strdup().
*/
char *strsave(s)
    char *s;
    {
	register int n = strlen(s)+1;
	char *p = malloc(n);

	if (p) memcpy(p, s, n);
	return p;
    }


/*  indx(s1, s2)
    if s1 can be decomposed as alpha || s2 || omega, return the length
    of the shortest such alpha, otherwise return -1.
*/
int indx(s1, s2)
    char *s1;
    char *s2;
    {
	register char *t;
	register char *m;
	register char *p;

	for (p = s1; *p; p++) {
	    for (t = p, m = s2; *m && *m == *t; m++, t++);
	    if (!*m) return p-s1;
	}
	return -1;
    }


char pbmsg[] = "m4: too many characters pushed back";

/*  Xputback(c)
    push character c back onto the input stream.
    This is now macro putback() in misc.h
*/
void Xputback(c)
    char c;
    {
	if (bp < endpbb) *bp++ = c; else error(pbmsg);
    }


/*  pbstr(s)
    push string s back onto the input stream.
    putback() has been unfolded here to improve performance.
    Example:
	s = <ABC>
	bp = <more stuff>
    After the call:
	bp = <more stuffCBA>
    It would be more efficient if we ran the pushback buffer in the
    opposite direction
*/
void pbstr(s)
    register char *s;
    {
	register char *es;
	register char *zp;

	zp = bp;
	for (es = s; *es; ) es++;	/* now es points to terminating NUL */
	bp += es-s;			/* advance bp as far as it should go */
	if (bp >= endpbb) error("m4: too many characters to push back");
	while (es > s) *zp++ = *--es;
    }


/*  pbqtd(s)
    pushes string s back "quoted", doing whatever has to be done to it to
    make sure that the result will evaluate to the original value.  As it
    happens, we have only to add lquote and rquote.
*/
void pbqtd(s)
    register char *s;
    {
	register char *es;
	register char *zp;

	zp = bp;
	for (es = s; *es; ) es++;	/* now es points to terminating NUL */
	bp += 2+es-s;			/* advance bp as far as it should go */
	if (bp >= endpbb) error("m4: too many characters to push back");
	*zp++ = rquote;
	while (es > s) *zp++ = *--es;
	*zp++ = lquote;
    }


/*  pbnum(n)
    convert a number to a (decimal) string and push it back.
    The original definition did not work for MININT; this does.
*/
void pbnum(n)
    int n;
    {
	register int num;

	num = n > 0 ? -n : n;	/* MININT <= num <= 0 */
	do {
	    putback('0' - (num % 10));
	} while ((num /= 10) < 0);
	if (n < 0) putback('-');
    }


/*  pbrad(n, r, m)
    converts a number n to base r ([-36..-2] U [2..36]), with at least
    m digits.  If r == 10 and m == 1, this is exactly the same as pbnum.
    However, this uses the function int2str() from R.A.O'Keefe's public
    domain string library, and puts the results of that back.
    The Unix System V Release 3 version of m4 accepts radix 1;
    THIS VERSION OF M4 DOES NOT ACCEPT RADIX 1 OR -1,
    nor do we accept radix < -36 or radix > 36.  At the moment such bad
    radices quietly produce nothing.  The V.3 treatment of radix 1 is
	push back abs(n) "1"s, then
	if n < 0, push back one "-".
    Until I come across something which uses it, I can't bring myself to
    implement this.

    I have, however, found a use for radix 0.  Unsurprisingly, it is
    related to radix 0 in Edinburgh Prolog.
	eval('c1c2...cn', 0, m)
    pushes back max(m-n,0) blanks and the characters c1...cn.  This can
    adjust to any byte size as long as UCHAR_MAX = (1 << CHAR_BIT) - 1.
    In particular, eval(c, 0) where 0 < c <= UCHAR_MAX, pushes back the
    character with code c.  Note that this has to agree with eval(); so
    both of them have to use the same byte ordering.
*/
void pbrad(n, r, m)
    long int n;
    int r, m;
    {
	char buffer[34];
	char *p;
	int L;

	if (r == 0) {
	    unsigned long int x = (unsigned long)n;
	    int n;

	    for (n = 0; x; x >>= CHAR_BIT, n++) buffer[n] = x & UCHAR_MAX;
	    for (L = n; --L >= 0; ) putback(buffer[L]);
	    for (L = m-n; --L >= 0; ) putback(' ');
	    return;
	}
	L = m - (int2str(p = buffer, -r, n)-buffer);
	if (buffer[0] == '-') L++, p++;
	if (L > 0) {
	    pbstr(p);
	    while (--L >= 0) putback('0');
	    if (p != buffer) putback('-');
	} else {
	    pbstr(buffer);
	}
    }


char csmsg[] = "m4: string space overflow";

/*  chrsave(c)
    put the character c in the string space.
*/
void Xchrsave(c)
    char c;
    {
#if 0
	if (sp < 0) putc(c, active); else
#endif
	if (ep < endest) *ep++ = c; else
	error(csmsg);
    }


/*  getdiv(ind)
    read in a diversion file and then delete it.
*/
void getdiv(ind)
    int ind;
    {
	register int c;
	register FILE *dfil;
	register FILE *afil;

	afil = active;
	if (outfile[ind] == afil)
	    error("m4: undivert: diversion still active.");
	(void) fclose(outfile[ind]);
	outfile[ind] = NULL;
	m4temp[UNIQUE] = '0' + ind;
	if ((dfil = fopen(m4temp, "r")) == NULL)
	    error("m4: cannot undivert.");
	while ((c = getc(dfil)) != EOF) putc(c, afil);
	(void) fclose(dfil);

#if vms
	if (remove(m4temp)) error("m4: cannot unlink.");
#else
	if (unlink(m4temp) == -1) error("m4: cannot unlink.");
#endif
    }


/*  killdiv()
    delete all the diversion files which have been created.
*/
void killdiv()
    {
	register int n;

	for (n = 0; n < MAXOUT; n++) {
	    if (outfile[n] != NULL) {
		(void) fclose(outfile[n]);
		m4temp[UNIQUE] = '0' + n;
#if unix
		(void) unlink(m4temp);
#else
		(void) remove(m4temp);
#endif
	    }
	}
    }


/*  error(s)
    close all files, report a fatal error, and quit, letting the caller know.
*/
void error(s)
    char *s;
    {
	killdiv();
	fprintf(stderr, "%s\n", s);
	exit(1);
    }


/*  Interrupt handling
*/
static char *msg = "\ninterrupted.";

#ifdef	__STDC__
void onintr(int signo)
#else
onintr()
#endif
    {
	error(msg);
    }


void usage()
    {
	fprintf(stderr, "Usage: m4 [-e] [-[BHST]int] [-Dname[=val]] [-Uname]\n");
	exit(1);
    }

#ifdef GETOPT
/* Henry Spencer's getopt() - get option letter from argv */

char *optarg;			/* Global argument pointer. */
int optind = 0;			/* Global argv index. */

static char *scan = NULL;	/* Private scan pointer. */

#ifndef	__STDC__
extern	char *index();
#define strchr index
#endif

int getopt(argc, argv, optstring)
    int argc;
    char **argv;
    char *optstring;
    {
	register char c;
	register char *place;

	optarg = NULL;

	if (scan == NULL || *scan == '\0') {
	    if (optind == 0) optind++;
	    if (optind >= argc
	     || argv[optind][0] != '-'
	     || argv[optind][1] == '\0')
		return EOF;
	    if (strcmp(argv[optind], "--") == 0) {
		optind++;
		return EOF;
	    }
	    scan = argv[optind]+1;
	    optind++;
	}
	c = *scan++;
	place = strchr(optstring, c);

	if (place == NULL || c == ':') {
	    fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
	    return '?';
	}
	place++;
	if (*place == ':') {
	    if (*scan != '\0') {
		optarg = scan;
		scan = NULL;
	    } else {
		optarg = argv[optind];
		optind++;
	    }
	}
	return c;
    }
#endif

