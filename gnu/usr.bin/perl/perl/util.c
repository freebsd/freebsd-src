/* $RCSfile: util.c,v $$Revision: 1.2 $$Date: 1995/05/30 05:03:28 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: util.c,v $
 * Revision 1.2  1995/05/30 05:03:28  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:34  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:40  nate
 * PERL!
 *
 * Revision 4.0.1.6  92/06/11  21:18:47  lwall
 * patch34: boneheaded typo in my_bcopy()
 *
 * Revision 4.0.1.5  92/06/08  16:08:37  lwall
 * patch20: removed implicit int declarations on functions
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 * patch20: fixed confusion between a *var's real name and its effective name
 * patch20: bcopy() and memcpy() now tested for overlap safety
 * patch20: added Atari ST portability
 *
 * Revision 4.0.1.4  91/11/11  16:48:54  lwall
 * patch19: study was busted by 4.018
 * patch19: added little-endian pack/unpack options
 *
 * Revision 4.0.1.3  91/11/05  19:18:26  lwall
 * patch11: safe malloc code now integrated into Perl's malloc when possible
 * patch11: index("little", "longer string") could visit faraway places
 * patch11: warn '-' x 10000 dumped core
 * patch11: forked exec on non-existent program now issues a warning
 *
 * Revision 4.0.1.2  91/06/07  12:10:42  lwall
 * patch4: new copyright notice
 * patch4: made some allowances for "semi-standard" C
 * patch4: index() could blow up searching for null string
 * patch4: taintchecks could improperly modify parent in vfork()
 * patch4: exec would close files even if you cleared close-on-exec flag
 *
 * Revision 4.0.1.1  91/04/12  09:19:25  lwall
 * patch1: random cleanup in cpp namespace
 *
 * Revision 4.0  91/03/20  01:56:39  lwall
 * 4.0 baseline.
 *
 */
/*SUPPRESS 112*/

#include "EXTERN.h"
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

#ifdef I_VFORK
#  include <vfork.h>
#endif

#ifdef I_VARARGS
#  include <varargs.h>
#endif

#ifdef I_FCNTL
#  include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#  include <sys/file.h>
#endif

#define FLUSH

#ifndef safemalloc

static char nomem[] = "Out of memory!\n";

/* paranoid version of malloc */

#ifdef DEBUGGING
static int an = 0;
#endif

/* NOTE:  Do not call the next three routines directly.  Use the macros
 * in handy.h, so that we can easily redefine everything to do tracking of
 * allocated hunks back to the original New to track down any memory leaks.
 */

char *
safemalloc(size)
#ifdef MSDOS
unsigned long size;
#else
MEM_SIZE size;
#endif /* MSDOS */
{
    char *ptr;
#ifndef STANDARD_C
    char *malloc();
#endif /* ! STANDARD_C */

#ifdef MSDOS
	if (size > 0xffff) {
		fprintf(stderr, "Allocation too large: %lx\n", size) FLUSH;
		exit(1);
	}
#endif /* MSDOS */
#ifdef DEBUGGING
    if ((long)size < 0)
	fatal("panic: malloc");
#endif
    ptr = malloc(size?size:1);	/* malloc(0) is NASTY on our system */
#ifdef DEBUGGING
#  if !(defined(I286) || defined(atarist))
    if (debug & 128)
	fprintf(stderr,"0x%x: (%05d) malloc %ld bytes\n",ptr,an++,(long)size);
#  else
    if (debug & 128)
	fprintf(stderr,"0x%lx: (%05d) malloc %ld bytes\n",ptr,an++,(long)size);
#  endif
#endif
    if (ptr != Nullch)
	return ptr;
    else if (nomemok)
	return Nullch;
    else {
	fputs(nomem,stderr) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
#ifdef lint
    return ptr;
#endif
}

/* paranoid version of realloc */

char *
saferealloc(where,size)
char *where;
#ifndef MSDOS
MEM_SIZE size;
#else
unsigned long size;
#endif /* MSDOS */
{
    char *ptr;
#ifndef STANDARD_C
    char *realloc();
#endif /* ! STANDARD_C */

#ifdef MSDOS
	if (size > 0xffff) {
		fprintf(stderr, "Reallocation too large: %lx\n", size) FLUSH;
		exit(1);
	}
#endif /* MSDOS */
    if (!where)
	fatal("Null realloc");
#ifdef DEBUGGING
    if ((long)size < 0)
	fatal("panic: realloc");
#endif
    ptr = realloc(where,size?size:1);	/* realloc(0) is NASTY on our system */
#ifdef DEBUGGING
#  if !(defined(I286) || defined(atarist))
    if (debug & 128) {
	fprintf(stderr,"0x%x: (%05d) rfree\n",where,an++);
	fprintf(stderr,"0x%x: (%05d) realloc %ld bytes\n",ptr,an++,(long)size);
    }
#  else
    if (debug & 128) {
	fprintf(stderr,"0x%lx: (%05d) rfree\n",where,an++);
	fprintf(stderr,"0x%lx: (%05d) realloc %ld bytes\n",ptr,an++,(long)size);
    }
#  endif
#endif
    if (ptr != Nullch)
	return ptr;
    else if (nomemok)
	return Nullch;
    else {
	fputs(nomem,stderr) FLUSH;
	exit(1);
    }
    /*NOTREACHED*/
#ifdef lint
    return ptr;
#endif
}

/* safe version of free */

void
safefree(where)
char *where;
{
#ifdef DEBUGGING
#  if !(defined(I286) || defined(atarist))
    if (debug & 128)
	fprintf(stderr,"0x%x: (%05d) free\n",where,an++);
#  else
    if (debug & 128)
	fprintf(stderr,"0x%lx: (%05d) free\n",where,an++);
#  endif
#endif
    if (where) {
	/*SUPPRESS 701*/
	free(where);
    }
}

#endif /* !safemalloc */

#ifdef LEAKTEST

#define ALIGN sizeof(long)

char *
safexmalloc(x,size)
int x;
MEM_SIZE size;
{
    register char *where;

    where = safemalloc(size + ALIGN);
    xcount[x]++;
    where[0] = x % 100;
    where[1] = x / 100;
    return where + ALIGN;
}

char *
safexrealloc(where,size)
char *where;
MEM_SIZE size;
{
    return saferealloc(where - ALIGN, size + ALIGN) + ALIGN;
}

void
safexfree(where)
char *where;
{
    int x;

    if (!where)
	return;
    where -= ALIGN;
    x = where[0] + 100 * where[1];
    xcount[x]--;
    safefree(where);
}

static void
xstat()
{
    register int i;

    for (i = 0; i < MAXXCOUNT; i++) {
	if (xcount[i] > lastxcount[i]) {
	    fprintf(stderr,"%2d %2d\t%ld\n", i / 100, i % 100, xcount[i]);
	    lastxcount[i] = xcount[i];
	}
    }
}

#endif /* LEAKTEST */

/* copy a string up to some (non-backslashed) delimiter, if any */

char *
cpytill(to,from,fromend,delim,retlen)
register char *to;
register char *from;
register char *fromend;
register int delim;
int *retlen;
{
    char *origto = to;

    for (; from < fromend; from++,to++) {
	if (*from == '\\') {
	    if (from[1] == delim)
		from++;
	    else if (from[1] == '\\')
		*to++ = *from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    *retlen = to - origto;
    return from;
}

/* return ptr to little string in big string, NULL if not found */
/* This routine was donated by Corey Satten. */

char *
instr(big, little)
register char *big;
register char *little;
{
    register char *s, *x;
    register int first;

    if (!little)
	return big;
    first = *little++;
    if (!first)
	return big;
    while (*big) {
	if (*big++ != first)
	    continue;
	for (x=big,s=little; *s; /**/ ) {
	    if (!*x)
		return Nullch;
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (!*s)
	    return big-1;
    }
    return Nullch;
}

/* same as instr but allow embedded nulls */

char *
ninstr(big, bigend, little, lend)
register char *big;
register char *bigend;
char *little;
char *lend;
{
    register char *s, *x;
    register int first = *little;
    register char *littleend = lend;

    if (!first && little > littleend)
	return big;
    if (bigend - big < littleend - little)
	return Nullch;
    bigend -= littleend - little++;
    while (big <= bigend) {
	if (*big++ != first)
	    continue;
	for (x=big,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s >= littleend)
	    return big-1;
    }
    return Nullch;
}

/* reverse of the above--find last substring */

char *
rninstr(big, bigend, little, lend)
register char *big;
char *bigend;
char *little;
char *lend;
{
    register char *bigbeg;
    register char *s, *x;
    register int first = *little;
    register char *littleend = lend;

    if (!first && little > littleend)
	return bigend;
    bigbeg = big;
    big = bigend - (littleend - little++);
    while (big >= bigbeg) {
	if (*big-- != first)
	    continue;
	for (x=big+2,s=little; s < littleend; /**/ ) {
	    if (*s++ != *x++) {
		s--;
		break;
	    }
	}
	if (s >= littleend)
	    return big+1;
    }
    return Nullch;
}

unsigned char fold[] = {
	0,	1,	2,	3,	4,	5,	6,	7,
	8,	9,	10,	11,	12,	13,	14,	15,
	16,	17,	18,	19,	20,	21,	22,	23,
	24,	25,	26,	27,	28,	29,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	40,	41,	42,	43,	44,	45,	46,	47,
	48,	49,	50,	51,	52,	53,	54,	55,
	56,	57,	58,	59,	60,	61,	62,	63,
	64,	'a',	'b',	'c',	'd',	'e',	'f',	'g',
	'h',	'i',	'j',	'k',	'l',	'm',	'n',	'o',
	'p',	'q',	'r',	's',	't',	'u',	'v',	'w',
	'x',	'y',	'z',	91,	92,	93,	94,	95,
	96,	'A',	'B',	'C',	'D',	'E',	'F',	'G',
	'H',	'I',	'J',	'K',	'L',	'M',	'N',	'O',
	'P',	'Q',	'R',	'S',	'T',	'U',	'V',	'W',
	'X',	'Y',	'Z',	123,	124,	125,	126,	127,
	128,	129,	130,	131,	132,	133,	134,	135,
	136,	137,	138,	139,	140,	141,	142,	143,
	144,	145,	146,	147,	148,	149,	150,	151,
	152,	153,	154,	155,	156,	157,	158,	159,
	160,	161,	162,	163,	164,	165,	166,	167,
	168,	169,	170,	171,	172,	173,	174,	175,
	176,	177,	178,	179,	180,	181,	182,	183,
	184,	185,	186,	187,	188,	189,	190,	191,
	192,	193,	194,	195,	196,	197,	198,	199,
	200,	201,	202,	203,	204,	205,	206,	207,
	208,	209,	210,	211,	212,	213,	214,	215,
	216,	217,	218,	219,	220,	221,	222,	223,
	224,	225,	226,	227,	228,	229,	230,	231,
	232,	233,	234,	235,	236,	237,	238,	239,
	240,	241,	242,	243,	244,	245,	246,	247,
	248,	249,	250,	251,	252,	253,	254,	255
};

static unsigned char freq[] = {
	1,	2,	84,	151,	154,	155,	156,	157,
	165,	246,	250,	3,	158,	7,	18,	29,
	40,	51,	62,	73,	85,	96,	107,	118,
	129,	140,	147,	148,	149,	150,	152,	153,
	255,	182,	224,	205,	174,	176,	180,	217,
	233,	232,	236,	187,	235,	228,	234,	226,
	222,	219,	211,	195,	188,	193,	185,	184,
	191,	183,	201,	229,	181,	220,	194,	162,
	163,	208,	186,	202,	200,	218,	198,	179,
	178,	214,	166,	170,	207,	199,	209,	206,
	204,	160,	212,	216,	215,	192,	175,	173,
	243,	172,	161,	190,	203,	189,	164,	230,
	167,	248,	227,	244,	242,	255,	241,	231,
	240,	253,	169,	210,	245,	237,	249,	247,
	239,	168,	252,	251,	254,	238,	223,	221,
	213,	225,	177,	197,	171,	196,	159,	4,
	5,	6,	8,	9,	10,	11,	12,	13,
	14,	15,	16,	17,	19,	20,	21,	22,
	23,	24,	25,	26,	27,	28,	30,	31,
	32,	33,	34,	35,	36,	37,	38,	39,
	41,	42,	43,	44,	45,	46,	47,	48,
	49,	50,	52,	53,	54,	55,	56,	57,
	58,	59,	60,	61,	63,	64,	65,	66,
	67,	68,	69,	70,	71,	72,	74,	75,
	76,	77,	78,	79,	80,	81,	82,	83,
	86,	87,	88,	89,	90,	91,	92,	93,
	94,	95,	97,	98,	99,	100,	101,	102,
	103,	104,	105,	106,	108,	109,	110,	111,
	112,	113,	114,	115,	116,	117,	119,	120,
	121,	122,	123,	124,	125,	126,	127,	128,
	130,	131,	132,	133,	134,	135,	136,	137,
	138,	139,	141,	142,	143,	144,	145,	146
};

void
fbmcompile(str, iflag)
STR *str;
int iflag;
{
    register unsigned char *s;
    register unsigned char *table;
    register unsigned int i;
    register unsigned int len = str->str_cur;
    int rarest = 0;
    unsigned int frequency = 256;

    Str_Grow(str,len+258);
#ifndef lint
    table = (unsigned char*)(str->str_ptr + len + 1);
#else
    table = Null(unsigned char*);
#endif
    s = table - 2;
    for (i = 0; i < 256; i++) {
	table[i] = len;
    }
    i = 0;
#ifndef lint
    while (s >= (unsigned char*)(str->str_ptr))
#endif
    {
	if (table[*s] == len) {
#ifndef pdp11
	    if (iflag)
		table[*s] = table[fold[*s]] = i;
#else
	    if (iflag) {
		int j;
		j = fold[*s];
		table[j] = i;
		table[*s] = i;
	    }
#endif /* pdp11 */
	    else
		table[*s] = i;
	}
	s--,i++;
    }
    str->str_pok |= SP_FBM;		/* deep magic */

#ifndef lint
    s = (unsigned char*)(str->str_ptr);		/* deeper magic */
#else
    s = Null(unsigned char*);
#endif
    if (iflag) {
	register unsigned int tmp, foldtmp;
	str->str_pok |= SP_CASEFOLD;
	for (i = 0; i < len; i++) {
	    tmp=freq[s[i]];
	    foldtmp=freq[fold[s[i]]];
	    if (tmp < frequency && foldtmp < frequency) {
		rarest = i;
		/* choose most frequent among the two */
		frequency = (tmp > foldtmp) ? tmp : foldtmp;
	    }
	}
    }
    else {
	for (i = 0; i < len; i++) {
	    if (freq[s[i]] < frequency) {
		rarest = i;
		frequency = freq[s[i]];
	    }
	}
    }
    str->str_rare = s[rarest];
    str->str_state = rarest;
#ifdef DEBUGGING
    if (debug & 512)
	fprintf(stderr,"rarest char %c at %d\n",str->str_rare, str->str_state);
#endif
}

char *
fbminstr(big, bigend, littlestr)
unsigned char *big;
register unsigned char *bigend;
STR *littlestr;
{
    register unsigned char *s;
    register int tmp;
    register int littlelen;
    register unsigned char *little;
    register unsigned char *table;
    register unsigned char *olds;
    register unsigned char *oldlittle;

#ifndef lint
    if (!(littlestr->str_pok & SP_FBM)) {
	if (!littlestr->str_ptr)
	    return (char*)big;
	return ninstr((char*)big,(char*)bigend,
		littlestr->str_ptr, littlestr->str_ptr + littlestr->str_cur);
    }
#endif

    littlelen = littlestr->str_cur;
#ifndef lint
    if (littlestr->str_pok & SP_TAIL && !multiline) {	/* tail anchored? */
	if (littlelen > bigend - big)
	    return Nullch;
	little = (unsigned char*)littlestr->str_ptr;
	if (littlestr->str_pok & SP_CASEFOLD) {	/* oops, fake it */
	    big = bigend - littlelen;		/* just start near end */
	    if (bigend[-1] == '\n' && little[littlelen-1] != '\n')
		big--;
	}
	else {
	    s = bigend - littlelen;
	    if (*s == *little && bcmp(s,little,littlelen)==0)
		return (char*)s;		/* how sweet it is */
	    else if (bigend[-1] == '\n' && little[littlelen-1] != '\n'
	      && s > big) {
		    s--;
		if (*s == *little && bcmp(s,little,littlelen)==0)
		    return (char*)s;
	    }
	    return Nullch;
	}
    }
    table = (unsigned char*)(littlestr->str_ptr + littlelen + 1);
#else
    table = Null(unsigned char*);
#endif
    if (--littlelen >= bigend - big)
	return Nullch;
    s = big + littlelen;
    oldlittle = little = table - 2;
    if (littlestr->str_pok & SP_CASEFOLD) {	/* case insensitive? */
	if (s < bigend) {
	  top1:
	    /*SUPPRESS 560*/
	    if (tmp = table[*s]) {
#ifdef POINTERRIGOR
		if (bigend - s > tmp) {
		    s += tmp;
		    goto top1;
		}
#else
		if ((s += tmp) < bigend)
		    goto top1;
#endif
		return Nullch;
	    }
	    else {
		tmp = littlelen;	/* less expensive than calling strncmp() */
		olds = s;
		while (tmp--) {
		    if (*--s == *--little || fold[*s] == *little)
			continue;
		    s = olds + 1;	/* here we pay the price for failure */
		    little = oldlittle;
		    if (s < bigend)	/* fake up continue to outer loop */
			goto top1;
		    return Nullch;
		}
#ifndef lint
		return (char *)s;
#endif
	    }
	}
    }
    else {
	if (s < bigend) {
	  top2:
	    /*SUPPRESS 560*/
	    if (tmp = table[*s]) {
#ifdef POINTERRIGOR
		if (bigend - s > tmp) {
		    s += tmp;
		    goto top2;
		}
#else
		if ((s += tmp) < bigend)
		    goto top2;
#endif
		return Nullch;
	    }
	    else {
		tmp = littlelen;	/* less expensive than calling strncmp() */
		olds = s;
		while (tmp--) {
		    if (*--s == *--little)
			continue;
		    s = olds + 1;	/* here we pay the price for failure */
		    little = oldlittle;
		    if (s < bigend)	/* fake up continue to outer loop */
			goto top2;
		    return Nullch;
		}
#ifndef lint
		return (char *)s;
#endif
	    }
	}
    }
    return Nullch;
}

char *
screaminstr(bigstr, littlestr)
STR *bigstr;
STR *littlestr;
{
    register unsigned char *s, *x;
    register unsigned char *big;
    register int pos;
    register int previous;
    register int first;
    register unsigned char *little;
    register unsigned char *bigend;
    register unsigned char *littleend;

    if ((pos = screamfirst[littlestr->str_rare]) < 0)
	return Nullch;
#ifndef lint
    little = (unsigned char *)(littlestr->str_ptr);
#else
    little = Null(unsigned char *);
#endif
    littleend = little + littlestr->str_cur;
    first = *little++;
    previous = littlestr->str_state;
#ifndef lint
    big = (unsigned char *)(bigstr->str_ptr);
#else
    big = Null(unsigned char*);
#endif
    bigend = big + bigstr->str_cur;
    while (pos < previous) {
#ifndef lint
	if (!(pos += screamnext[pos]))
#endif
	    return Nullch;
    }
#ifdef POINTERRIGOR
    if (littlestr->str_pok & SP_CASEFOLD) {	/* case insignificant? */
	do {
	    if (big[pos-previous] != first && big[pos-previous] != fold[first])
		continue;
	    for (x=big+pos+1-previous,s=little; s < littleend; /**/ ) {
		if (x >= bigend)
		    return Nullch;
		if (*s++ != *x++ && fold[*(s-1)] != *(x-1)) {
		    s--;
		    break;
		}
	    }
	    if (s == littleend)
#ifndef lint
		return (char *)(big+pos-previous);
#else
		return Nullch;
#endif
	} while (
#ifndef lint
		pos += screamnext[pos]	/* does this goof up anywhere? */
#else
		pos += screamnext[0]
#endif
	    );
    }
    else {
	do {
	    if (big[pos-previous] != first)
		continue;
	    for (x=big+pos+1-previous,s=little; s < littleend; /**/ ) {
		if (x >= bigend)
		    return Nullch;
		if (*s++ != *x++) {
		    s--;
		    break;
		}
	    }
	    if (s == littleend)
#ifndef lint
		return (char *)(big+pos-previous);
#else
		return Nullch;
#endif
	} while (
#ifndef lint
		pos += screamnext[pos]
#else
		pos += screamnext[0]
#endif
	    );
    }
#else /* !POINTERRIGOR */
    big -= previous;
    if (littlestr->str_pok & SP_CASEFOLD) {	/* case insignificant? */
	do {
	    if (big[pos] != first && big[pos] != fold[first])
		continue;
	    for (x=big+pos+1,s=little; s < littleend; /**/ ) {
		if (x >= bigend)
		    return Nullch;
		if (*s++ != *x++ && fold[*(s-1)] != *(x-1)) {
		    s--;
		    break;
		}
	    }
	    if (s == littleend)
#ifndef lint
		return (char *)(big+pos);
#else
		return Nullch;
#endif
	} while (
#ifndef lint
		pos += screamnext[pos]	/* does this goof up anywhere? */
#else
		pos += screamnext[0]
#endif
	    );
    }
    else {
	do {
	    if (big[pos] != first)
		continue;
	    for (x=big+pos+1,s=little; s < littleend; /**/ ) {
		if (x >= bigend)
		    return Nullch;
		if (*s++ != *x++) {
		    s--;
		    break;
		}
	    }
	    if (s == littleend)
#ifndef lint
		return (char *)(big+pos);
#else
		return Nullch;
#endif
	} while (
#ifndef lint
		pos += screamnext[pos]
#else
		pos += screamnext[0]
#endif
	    );
    }
#endif /* POINTERRIGOR */
    return Nullch;
}

/* copy a string to a safe spot */

char *
savestr(str)
char *str;
{
    register char *newaddr;

    New(902,newaddr,strlen(str)+1,char);
    (void)strcpy(newaddr,str);
    return newaddr;
}

/* same thing but with a known length */

char *
nsavestr(str, len)
char *str;
register int len;
{
    register char *newaddr;

    New(903,newaddr,len+1,char);
    Copy(str,newaddr,len,char);		/* might not be null terminated */
    newaddr[len] = '\0';		/* is now */
    return newaddr;
}

/* grow a static string to at least a certain length */

void
growstr(strptr,curlen,newlen)
char **strptr;
int *curlen;
int newlen;
{
    if (newlen > *curlen) {		/* need more room? */
	if (*curlen)
	    Renew(*strptr,newlen,char);
	else
	    New(905,*strptr,newlen,char);
	*curlen = newlen;
    }
}

#ifndef I_VARARGS
/*VARARGS1*/
char *
mess(pat,a1,a2,a3,a4)
char *pat;
long a1, a2, a3, a4;
{
    char *s;
    int usermess = strEQ(pat,"%s");
    STR *tmpstr;

    s = buf;
    if (usermess) {
	tmpstr = str_mortal(&str_undef);
	str_set(tmpstr, (char*)a1);
	*s++ = tmpstr->str_ptr[tmpstr->str_cur-1];
    }
    else {
	(void)sprintf(s,pat,a1,a2,a3,a4);
	s += strlen(s);
    }

    if (s[-1] != '\n') {
	if (curcmd->c_line) {
	    (void)sprintf(s," at %s line %ld",
	      stab_val(curcmd->c_filestab)->str_ptr, (long)curcmd->c_line);
	    s += strlen(s);
	}
	if (last_in_stab &&
	    stab_io(last_in_stab) &&
	    stab_io(last_in_stab)->lines ) {
	    (void)sprintf(s,", <%s> line %ld",
	      last_in_stab == argvstab ? "" : stab_ename(last_in_stab),
	      (long)stab_io(last_in_stab)->lines);
	    s += strlen(s);
	}
	(void)strcpy(s,".\n");
	if (usermess)
	    str_cat(tmpstr,buf+1);
    }
    if (usermess)
	return tmpstr->str_ptr;
    else
	return buf;
}

/*VARARGS1*/
void fatal(pat,a1,a2,a3,a4)
char *pat;
long a1, a2, a3, a4;
{
    extern FILE *e_fp;
    extern char *e_tmpname;
    char *tmps;
    char *message;

    message = mess(pat,a1,a2,a3,a4);
    if (in_eval) {
	str_set(stab_val(stabent("@",TRUE)),message);
	tmps = "_EVAL_";
	while (loop_ptr >= 0 && (!loop_stack[loop_ptr].loop_label ||
	  strNE(tmps,loop_stack[loop_ptr].loop_label) )) {
#ifdef DEBUGGING
	    if (debug & 4) {
		deb("(Skipping label #%d %s)\n",loop_ptr,
		    loop_stack[loop_ptr].loop_label);
	    }
#endif
	    loop_ptr--;
	}
#ifdef DEBUGGING
	if (debug & 4) {
	    deb("(Found label #%d %s)\n",loop_ptr,
		loop_stack[loop_ptr].loop_label);
	}
#endif
	if (loop_ptr < 0) {
	    in_eval = 0;
	    fatal("Bad label: %s", tmps);
	}
	longjmp(loop_stack[loop_ptr].loop_env, 1);
    }
    fputs(message,stderr);
    (void)fflush(stderr);
    if (e_fp)
	(void)UNLINK(e_tmpname);
    statusvalue >>= 8;
    exit((int)((errno&255)?errno:((statusvalue&255)?statusvalue:255)));
}

/*VARARGS1*/
void warn(pat,a1,a2,a3,a4)
char *pat;
long a1, a2, a3, a4;
{
    char *message;

    message = mess(pat,a1,a2,a3,a4);
    fputs(message,stderr);
#ifdef LEAKTEST
#ifdef DEBUGGING
    if (debug & 4096)
	xstat();
#endif
#endif
    (void)fflush(stderr);
}
#else
/*VARARGS0*/
char *
mess(args)
va_list args;
{
    char *pat;
    char *s;
    STR *tmpstr;
    int usermess;
    size_t l;
#ifndef HAS_VPRINTF
#ifdef CHARVSPRINTF
    char *vsprintf();
#else
    int vsprintf();
#endif
#endif

#ifdef lint
    pat = Nullch;
#else
    pat = va_arg(args, char *);
#endif
    s = buf;
    usermess = strEQ(pat, "%s");
    if (usermess) {
	tmpstr = str_mortal(&str_undef);
	str_set(tmpstr, va_arg(args, char *));
	*s++ = tmpstr->str_ptr[tmpstr->str_cur-1];
    }
    else {
	(void) vsnprintf(s,sizeof buf - (s - buf),pat,args);
	s += strlen(s);
    }

    if (s[-1] != '\n') {
	if (curcmd->c_line) {
	    l = s - buf >= sizeof buf - 1? 1: sizeof buf - (s - buf);
	    (void)snprintf(s,l," at %s line %ld",
	      stab_val(curcmd->c_filestab)->str_ptr, (long)curcmd->c_line);
	    s += strlen(s);
	}
	if (last_in_stab &&
	    stab_io(last_in_stab) &&
	    stab_io(last_in_stab)->lines ) {
	    l = s - buf >= sizeof buf - 1? 1: sizeof buf - (s - buf);
	    (void)snprintf(s,l,", <%s> line %ld",
	      last_in_stab == argvstab ? "" : last_in_stab->str_magic->str_ptr,
	      (long)stab_io(last_in_stab)->lines);
	    s += strlen(s);
	}
	if (s - buf > sizeof buf - 3)
	    (void)strcpy(s,".\n");
	if (usermess)
	    str_cat(tmpstr,buf+1);
    }

    if (usermess)
	return tmpstr->str_ptr;
    else
	return buf;
}

/*VARARGS0*/
void fatal(va_alist)
va_dcl
{
    va_list args;
    extern FILE *e_fp;
    extern char *e_tmpname;
    char *tmps;
    char *message;

#ifndef lint
    va_start(args);
#else
    args = 0;
#endif
    message = mess(args);
    va_end(args);
    if (in_eval) {
	str_set(stab_val(stabent("@",TRUE)),message);
	tmps = "_EVAL_";
	while (loop_ptr >= 0 && (!loop_stack[loop_ptr].loop_label ||
	  strNE(tmps,loop_stack[loop_ptr].loop_label) )) {
#ifdef DEBUGGING
	    if (debug & 4) {
		deb("(Skipping label #%d %s)\n",loop_ptr,
		    loop_stack[loop_ptr].loop_label);
	    }
#endif
	    loop_ptr--;
	}
#ifdef DEBUGGING
	if (debug & 4) {
	    deb("(Found label #%d %s)\n",loop_ptr,
		loop_stack[loop_ptr].loop_label);
	}
#endif
	if (loop_ptr < 0) {
	    in_eval = 0;
	    fatal("Bad label: %s", tmps);
	}
	longjmp(loop_stack[loop_ptr].loop_env, 1);
    }
    fputs(message,stderr);
    (void)fflush(stderr);
    if (e_fp)
	(void)UNLINK(e_tmpname);
    statusvalue >>= 8;
    exit((int)((errno&255)?errno:((statusvalue&255)?statusvalue:255)));
}

/*VARARGS0*/
void warn(va_alist)
va_dcl
{
    va_list args;
    char *message;

#ifndef lint
    va_start(args);
#else
    args = 0;
#endif
    message = mess(args);
    va_end(args);

    fputs(message,stderr);
#ifdef LEAKTEST
#ifdef DEBUGGING
    if (debug & 4096)
	xstat();
#endif
#endif
    (void)fflush(stderr);
}
#endif

void
my_setenv(nam,val)
char *nam, *val;
{
    register int i=envix(nam);		/* where does it go? */

    if (environ == origenviron) {	/* need we copy environment? */
	int j;
	int max;
	char **tmpenv;

	/*SUPPRESS 530*/
	for (max = i; environ[max]; max++) ;
	New(901,tmpenv, max+2, char*);
	for (j=0; j<max; j++)		/* copy environment */
	    tmpenv[j] = savestr(environ[j]);
	tmpenv[max] = Nullch;
	environ = tmpenv;		/* tell exec where it is now */
    }
    if (!val) {
	while (environ[i]) {
	    environ[i] = environ[i+1];
	    i++;
	}
	return;
    }
    if (!environ[i]) {			/* does not exist yet */
	Renew(environ, i+2, char*);	/* just expand it a bit */
	environ[i+1] = Nullch;	/* make sure it's null terminated */
    }
    else
	Safefree(environ[i]);
    New(904, environ[i], strlen(nam) + strlen(val) + 2, char);
#ifndef MSDOS
    (void)sprintf(environ[i],"%s=%s",nam,val);/* all that work just for this */
#else
    /* MS-DOS requires environment variable names to be in uppercase */
    /* [Tom Dinger, 27 August 1990: Well, it doesn't _require_ it, but
     * some utilities and applications may break because they only look
     * for upper case strings. (Fixed strupr() bug here.)]
     */
    strcpy(environ[i],nam); strupr(environ[i]);
    (void)sprintf(environ[i] + strlen(nam),"=%s",val);
#endif /* MSDOS */
}

int
envix(nam)
char *nam;
{
    register int i, len = strlen(nam);

    for (i = 0; environ[i]; i++) {
	if (strnEQ(environ[i],nam,len) && environ[i][len] == '=')
	    break;			/* strnEQ must come first to avoid */
    }					/* potential SEGV's */
    return i;
}

#ifdef EUNICE
int
unlnk(f)	/* unlink all versions of a file */
char *f;
{
    int i;

    for (i = 0; unlink(f) >= 0; i++) ;
    return i ? 0 : -1;
}
#endif

#if !defined(HAS_BCOPY) || !defined(SAFE_BCOPY)
char *
my_bcopy(from,to,len)
register char *from;
register char *to;
register int len;
{
    char *retval = to;

    if (from - to >= 0) {
	while (len--)
	    *to++ = *from++;
    }
    else {
	to += len;
	from += len;
	while (len--)
	    *(--to) = *(--from);
    }
    return retval;
}
#endif

#if !defined(HAS_BZERO) && !defined(HAS_MEMSET)
char *
my_bzero(loc,len)
register char *loc;
register int len;
{
    char *retval = loc;

    while (len--)
	*loc++ = 0;
    return retval;
}
#endif

#ifndef HAS_MEMCMP
int
my_memcmp(s1,s2,len)
register unsigned char *s1;
register unsigned char *s2;
register int len;
{
    register int tmp;

    while (len--) {
	if (tmp = *s1++ - *s2++)
	    return tmp;
    }
    return 0;
}
#endif /* HAS_MEMCMP */

#ifdef I_VARARGS
#ifndef HAS_VPRINTF

#ifdef CHARVSPRINTF
char *
#else
int
#endif
vsprintf(dest, pat, args)
char *dest, *pat, *args;
{
    FILE fakebuf;

    fakebuf._ptr = dest;
    fakebuf._cnt = 32767;
#ifndef _IOSTRG
#define _IOSTRG 0
#endif
    fakebuf._flag = _IOWRT|_IOSTRG;
    _doprnt(pat, args, &fakebuf);	/* what a kludge */
    (void)putc('\0', &fakebuf);
#ifdef CHARVSPRINTF
    return(dest);
#else
    return 0;		/* perl doesn't use return value */
#endif
}

#ifdef DEBUGGING
int
vfprintf(fd, pat, args)
FILE *fd;
char *pat, *args;
{
    _doprnt(pat, args, fd);
    return 0;		/* wrong, but perl doesn't use the return value */
}
#endif
#endif /* HAS_VPRINTF */
#endif /* I_VARARGS */

/*
 * I think my_swap(), htonl() and ntohl() have never been used.
 * perl.h contains last-chance references to my_swap(), my_htonl()
 * and my_ntohl().  I presume these are the intended functions;
 * but htonl() and ntohl() have the wrong names.  There are no
 * functions my_htonl() and my_ntohl() defined anywhere.
 * -DWS
 */
#ifdef MYSWAP
#if BYTEORDER != 0x4321
short
my_swap(s)
short s;
{
#if (BYTEORDER & 1) == 0
    short result;

    result = ((s & 255) << 8) + ((s >> 8) & 255);
    return result;
#else
    return s;
#endif
}

long
htonl(l)
register long l;
{
    union {
	long result;
	char c[sizeof(long)];
    } u;

#if BYTEORDER == 0x1234
    u.c[0] = (l >> 24) & 255;
    u.c[1] = (l >> 16) & 255;
    u.c[2] = (l >> 8) & 255;
    u.c[3] = l & 255;
    return u.result;
#else
#if ((BYTEORDER - 0x1111) & 0x444) || !(BYTEORDER & 0xf)
    fatal("Unknown BYTEORDER\n");
#else
    register int o;
    register int s;

    for (o = BYTEORDER - 0x1111, s = 0; s < (sizeof(long)*8); o >>= 4, s += 8) {
	u.c[o & 0xf] = (l >> s) & 255;
    }
    return u.result;
#endif
#endif
}

long
ntohl(l)
register long l;
{
    union {
	long l;
	char c[sizeof(long)];
    } u;

#if BYTEORDER == 0x1234
    u.c[0] = (l >> 24) & 255;
    u.c[1] = (l >> 16) & 255;
    u.c[2] = (l >> 8) & 255;
    u.c[3] = l & 255;
    return u.l;
#else
#if ((BYTEORDER - 0x1111) & 0x444) || !(BYTEORDER & 0xf)
    fatal("Unknown BYTEORDER\n");
#else
    register int o;
    register int s;

    u.l = l;
    l = 0;
    for (o = BYTEORDER - 0x1111, s = 0; s < (sizeof(long)*8); o >>= 4, s += 8) {
	l |= (u.c[o & 0xf] & 255) << s;
    }
    return l;
#endif
#endif
}

#endif /* BYTEORDER != 0x4321 */
#endif /* MYSWAP */

/*
 * Little-endian byte order functions - 'v' for 'VAX', or 'reVerse'.
 * If these functions are defined,
 * the BYTEORDER is neither 0x1234 nor 0x4321.
 * However, this is not assumed.
 * -DWS
 */

#define HTOV(name,type)						\
	type							\
	name (n)						\
	register type n;					\
	{							\
	    union {						\
		type value;					\
		char c[sizeof(type)];				\
	    } u;						\
	    register int i;					\
	    register int s;					\
	    for (i = 0, s = 0; i < sizeof(u.c); i++, s += 8) {	\
		u.c[i] = (n >> s) & 0xFF;			\
	    }							\
	    return u.value;					\
	}

#define VTOH(name,type)						\
	type							\
	name (n)						\
	register type n;					\
	{							\
	    union {						\
		type value;					\
		char c[sizeof(type)];				\
	    } u;						\
	    register int i;					\
	    register int s;					\
	    u.value = n;					\
	    n = 0;						\
	    for (i = 0, s = 0; i < sizeof(u.c); i++, s += 8) {	\
		n += (u.c[i] & 0xFF) << s;			\
	    }							\
	    return n;						\
	}

#if defined(HAS_HTOVS) && !defined(htovs)
HTOV(htovs,short)
#endif
#if defined(HAS_HTOVL) && !defined(htovl)
HTOV(htovl,long)
#endif
#if defined(HAS_VTOHS) && !defined(vtohs)
VTOH(vtohs,short)
#endif
#if defined(HAS_VTOHL) && !defined(vtohl)
VTOH(vtohl,long)
#endif

#ifndef DOSISH
FILE *
mypopen(cmd,mode)
char	*cmd;
char	*mode;
{
    int p[2];
    register int this, that;
    register int pid;
    STR *str;
    int doexec = strNE(cmd,"-");

    if (pipe(p) < 0)
	return Nullfp;
    this = (*mode == 'w');
    that = !this;
#ifdef TAINT
    if (doexec) {
	taintenv();
	taintproper("Insecure dependency in exec");
    }
#endif
    while ((pid = (doexec?vfork():fork())) < 0) {
	if (errno != EAGAIN) {
	    close(p[this]);
	    if (!doexec)
		fatal("Can't fork");
	    return Nullfp;
	}
	sleep(5);
    }
    if (pid == 0) {
#define THIS that
#define THAT this
	close(p[THAT]);
	if (p[THIS] != (*mode == 'r')) {
	    dup2(p[THIS], *mode == 'r');
	    close(p[THIS]);
	}
	if (doexec) {
#if !defined(HAS_FCNTL) || !defined(F_SETFD)
	    int fd;

#ifndef NOFILE
#define NOFILE 20
#endif
	    for (fd = maxsysfd + 1; fd < NOFILE; fd++)
		close(fd);
#endif
	    do_exec(cmd);	/* may or may not use the shell */
	    warn("Can't exec \"%s\": %s", cmd, strerror(errno));
	    _exit(1);
	}
	/*SUPPRESS 560*/
	if (tmpstab = stabent("$",allstabs))
	    str_numset(STAB_STR(tmpstab),(double)getpid());
	forkprocess = 0;
	hclear(pidstatus, FALSE);	/* we have no children */
	return Nullfp;
#undef THIS
#undef THAT
    }
    do_execfree();	/* free any memory malloced by child on vfork */
    close(p[that]);
    if (p[that] < p[this]) {
	dup2(p[this], p[that]);
	close(p[this]);
	p[this] = p[that];
    }
    str = afetch(fdpid,p[this],TRUE);
    str->str_u.str_useful = pid;
    forkprocess = pid;
    return fdopen(p[this], mode);
}
#else
#ifdef atarist
FILE *popen();
FILE *
mypopen(cmd,mode)
char	*cmd;
char	*mode;
{
    return popen(cmd, mode);
}
#endif

#endif /* !DOSISH */

#ifdef NOTDEF
dumpfds(s)
char *s;
{
    int fd;
    struct stat tmpstatbuf;

    fprintf(stderr,"%s", s);
    for (fd = 0; fd < 32; fd++) {
	if (fstat(fd,&tmpstatbuf) >= 0)
	    fprintf(stderr," %d",fd);
    }
    fprintf(stderr,"\n");
}
#endif

#ifndef HAS_DUP2
dup2(oldfd,newfd)
int oldfd;
int newfd;
{
#if defined(HAS_FCNTL) && defined(F_DUPFD)
    close(newfd);
    fcntl(oldfd, F_DUPFD, newfd);
#else
    int fdtmp[256];
    int fdx = 0;
    int fd;

    if (oldfd == newfd)
	return 0;
    close(newfd);
    while ((fd = dup(oldfd)) != newfd)	/* good enough for low fd's */
	fdtmp[fdx++] = fd;
    while (fdx > 0)
	close(fdtmp[--fdx]);
#endif
}
#endif

#ifndef DOSISH
int
mypclose(ptr)
FILE *ptr;
{
#ifdef VOIDSIG
    void (*hstat)(), (*istat)(), (*qstat)();
#else
    int (*hstat)(), (*istat)(), (*qstat)();
#endif
    int status;
    STR *str;
    int pid;

    str = afetch(fdpid,fileno(ptr),TRUE);
    pid = (int)str->str_u.str_useful;
    astore(fdpid,fileno(ptr),Nullstr);
    fclose(ptr);
#ifdef UTS
    if(kill(pid, 0) < 0) { return(pid); }   /* HOM 12/23/91 */
#endif
    hstat = signal(SIGHUP, SIG_IGN);
    istat = signal(SIGINT, SIG_IGN);
    qstat = signal(SIGQUIT, SIG_IGN);
    pid = wait4pid(pid, &status, 0);
    signal(SIGHUP, hstat);
    signal(SIGINT, istat);
    signal(SIGQUIT, qstat);
    return(pid < 0 ? pid : status);
}

int
wait4pid(pid,statusp,flags)
int pid;
int *statusp;
int flags;
{
#if !defined(HAS_WAIT4) && !defined(HAS_WAITPID)
    int result;
    STR *str;
    char spid[16];
#endif

    if (!pid)
	return -1;
#ifdef HAS_WAIT4
    return wait4((pid==-1)?0:pid,statusp,flags,Null(struct rusage *));
#else
#ifdef HAS_WAITPID
    return waitpid(pid,statusp,flags);
#else
    if (pid > 0) {
	sprintf(spid, "%d", pid);
	str = hfetch(pidstatus,spid,strlen(spid),FALSE);
	if (str != &str_undef) {
	    *statusp = (int)str->str_u.str_useful;
	    hdelete(pidstatus,spid,strlen(spid));
	    return pid;
	}
    }
    else {
	HENT *entry;

	hiterinit(pidstatus);
	if (entry = hiternext(pidstatus)) {
	    pid = atoi(hiterkey(entry,statusp));
	    str = hiterval(pidstatus,entry);
	    *statusp = (int)str->str_u.str_useful;
	    sprintf(spid, "%d", pid);
	    hdelete(pidstatus,spid,strlen(spid));
	    return pid;
	}
    }
    if (flags)
	fatal("Can't do waitpid with flags");
    else {
	while ((result = wait(statusp)) != pid && pid > 0 && result >= 0)
	    pidgone(result,*statusp);
	if (result < 0)
	    *statusp = -1;
    }
    return result;
#endif
#endif
}
#endif /* !DOSISH */

void
/*SUPPRESS 590*/
pidgone(pid,status)
int pid;
int status;
{
#if defined(HAS_WAIT4) || defined(HAS_WAITPID)
#else
    register STR *str;
    char spid[16];

    sprintf(spid, "%d", pid);
    str = hfetch(pidstatus,spid,strlen(spid),TRUE);
    str->str_u.str_useful = status;
#endif
    return;
}

#ifdef atarist
int pclose();
int
mypclose(ptr)
FILE *ptr;
{
    return pclose(ptr);
}
#endif

void
repeatcpy(to,from,len,count)
register char *to;
register char *from;
int len;
register int count;
{
    register int todo;
    register char *frombase = from;

    if (len == 1) {
	todo = *from;
	while (count-- > 0)
	    *to++ = todo;
	return;
    }
    while (count-- > 0) {
	for (todo = len; todo > 0; todo--) {
	    *to++ = *from++;
	}
	from = frombase;
    }
}

#ifndef CASTNEGFLOAT
unsigned long
castulong(f)
double f;
{
    long along;

#if CASTFLAGS & 2
#   define BIGDOUBLE 2147483648.0
    if (f >= BIGDOUBLE)
	return (unsigned long)(f-(long)(f/BIGDOUBLE)*BIGDOUBLE)|0x80000000;
#endif
    if (f >= 0.0)
	return (unsigned long)f;
    along = (long)f;
    return (unsigned long)along;
}
#endif

#ifndef HAS_RENAME
int
same_dirent(a,b)
char *a;
char *b;
{
    char *fa = rindex(a,'/');
    char *fb = rindex(b,'/');
    struct stat tmpstatbuf1;
    struct stat tmpstatbuf2;
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif
    char tmpbuf[MAXPATHLEN+1];

    if (fa)
	fa++;
    else
	fa = a;
    if (fb)
	fb++;
    else
	fb = b;
    if (strNE(a,b))
	return FALSE;
    if (fa == a)
	strcpy(tmpbuf,".");
    else
	strncpy(tmpbuf, a, fa - a);
    if (stat(tmpbuf, &tmpstatbuf1) < 0)
	return FALSE;
    if (fb == b)
	strcpy(tmpbuf,".");
    else
	strncpy(tmpbuf, b, fb - b);
    if (stat(tmpbuf, &tmpstatbuf2) < 0)
	return FALSE;
    return tmpstatbuf1.st_dev == tmpstatbuf2.st_dev &&
	   tmpstatbuf1.st_ino == tmpstatbuf2.st_ino;
}
#endif /* !HAS_RENAME */

unsigned long
scanoct(start, len, retlen)
char *start;
int len;
int *retlen;
{
    register char *s = start;
    register unsigned long retval = 0;

    while (len-- && *s >= '0' && *s <= '7') {
	retval <<= 3;
	retval |= *s++ - '0';
    }
    *retlen = s - start;
    return retval;
}

unsigned long
scanhex(start, len, retlen)
char *start;
int len;
int *retlen;
{
    register char *s = start;
    register unsigned long retval = 0;
    char *tmp;

    while (len-- && *s && (tmp = index(hexdigit, *s))) {
	retval <<= 4;
	retval |= (tmp - hexdigit) & 15;
	s++;
    }
    *retlen = s - start;
    return retval;
}
