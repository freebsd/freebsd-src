/* $RCSfile: str.c,v $$Revision: 1.3 $$Date: 1997/08/08 20:53:59 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: str.c,v $
 * Revision 1.3  1997/08/08 20:53:59  joerg
 * Fix a buffer overflow condition (that causes a security hole in suidperl).
 *
 * Closes: CERT Advisory CA-97.17 - Vulnerability in suidperl
 * Obtained from: (partly) the fix in CA-97.17
 *
 * Revision 1.2  1995/05/30 05:03:21  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.1.1.1  1994/09/10  06:27:33  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:39  nate
 * PERL!
 *
 * Revision 4.0.1.7  1993/02/05  19:43:47  lwall
 * patch36: the non-std stdio input code wasn't null-proof
 *
 * Revision 4.0.1.6  92/06/11  21:14:21  lwall
 * patch34: quotes containing subscripts containing variables didn't parse right
 *
 * Revision 4.0.1.5  92/06/08  15:40:43  lwall
 * patch20: removed implicit int declarations on functions
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 * patch20: paragraph mode now skips extra newlines automatically
 * patch20: fixed memory leak in doube-quote interpretation
 * patch20: made /\$$foo/ look for literal '$foo'
 * patch20: "$var{$foo'bar}" didn't scan subscript correctly
 * patch20: a splice on non-existent array elements could dump core
 * patch20: running taintperl explicitly now does checks even if $< == $>
 *
 * Revision 4.0.1.4  91/11/05  18:40:51  lwall
 * patch11: $foo .= <BAR> could overrun malloced memory
 * patch11: \$ didn't always make it through double-quoter to regexp routines
 * patch11: prepared for ctype implementations that don't define isascii()
 *
 * Revision 4.0.1.3  91/06/10  01:27:54  lwall
 * patch10: $) and $| incorrectly handled in run-time patterns
 *
 * Revision 4.0.1.2  91/06/07  11:58:13  lwall
 * patch4: new copyright notice
 * patch4: taint check on undefined string could cause core dump
 *
 * Revision 4.0.1.1  91/04/12  09:15:30  lwall
 * patch1: fixed undefined environ problem
 * patch1: substr($ENV{"PATH"},0,0) = "/foo:" didn't modify environment
 * patch1: $foo .= <BAR> could cause core dump for certain lengths of $foo
 *
 * Revision 4.0  91/03/20  01:39:55  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"
#include "perly.h"

static void ucase();
static void lcase();

#ifndef str_get
char *
str_get(str)
STR *str;
{
#ifdef TAINT
    tainted |= str->str_tainted;
#endif
    return str->str_pok ? str->str_ptr : str_2ptr(str);
}
#endif

/* dlb ... guess we have a "crippled cc".
 * dlb the following functions are usually macros.
 */
#ifndef str_true
int
str_true(Str)
STR *Str;
{
	if (Str->str_pok) {
	    if (*Str->str_ptr > '0' ||
	      Str->str_cur > 1 ||
	      (Str->str_cur && *Str->str_ptr != '0'))
		return 1;
	    return 0;
	}
	if (Str->str_nok)
		return (Str->str_u.str_nval != 0.0);
	return 0;
}
#endif /* str_true */

#ifndef str_gnum
double str_gnum(Str)
STR *Str;
{
#ifdef TAINT
	tainted |= Str->str_tainted;
#endif /* TAINT*/
	if (Str->str_nok)
		return Str->str_u.str_nval;
	return str_2num(Str);
}
#endif /* str_gnum */
/* dlb ... end of crutch */

char *
str_grow(str,newlen)
register STR *str;
#ifndef DOSISH
register int newlen;
#else
unsigned long newlen;
#endif
{
    register char *s = str->str_ptr;

#ifdef MSDOS
    if (newlen >= 0x10000) {
	fprintf(stderr, "Allocation too large: %lx\n", newlen);
	exit(1);
    }
#endif /* MSDOS */
    if (str->str_state == SS_INCR) {		/* data before str_ptr? */
	str->str_len += str->str_u.str_useful;
	str->str_ptr -= str->str_u.str_useful;
	str->str_u.str_useful = 0L;
	Move(s, str->str_ptr, str->str_cur+1, char);
	s = str->str_ptr;
	str->str_state = SS_NORM;			/* normal again */
	if (newlen > str->str_len)
	    newlen += 10 * (newlen - str->str_cur); /* avoid copy each time */
    }
    if (newlen > str->str_len) {		/* need more room? */
        if (str->str_len)
	    Renew(s,newlen,char);
        else
	    New(703,s,newlen,char);
	str->str_ptr = s;
        str->str_len = newlen;
    }
    return s;
}

void
str_numset(str,num)
register STR *str;
double num;
{
    if (str->str_pok) {
	str->str_pok = 0;	/* invalidate pointer */
	if (str->str_state == SS_INCR)
	    Str_Grow(str,0);
    }
    str->str_u.str_nval = num;
    str->str_state = SS_NORM;
    str->str_nok = 1;			/* validate number */
#ifdef TAINT
    str->str_tainted = tainted;
#endif
}

char *
str_2ptr(str)
register STR *str;
{
    register char *s;
    int olderrno;

    if (!str)
	return "";
    if (str->str_nok) {
	STR_GROW(str, 30);
	s = str->str_ptr;
	olderrno = errno;	/* some Xenix systems wipe out errno here */
#if defined(scs) && defined(ns32000)
	gcvt(str->str_u.str_nval,20,s);
#else
#ifdef apollo
	if (str->str_u.str_nval == 0.0)
	    (void)strcpy(s,"0");
	else
#endif /*apollo*/
	(void)sprintf(s,"%.20g",str->str_u.str_nval);
#endif /*scs*/
	errno = olderrno;
	while (*s) s++;
#ifdef hcx
	if (s[-1] == '.')
	    s--;
#endif
    }
    else {
	if (str == &str_undef)
	    return No;
	if (dowarn)
	    warn("Use of uninitialized variable");
	STR_GROW(str, 30);
	s = str->str_ptr;
    }
    *s = '\0';
    str->str_cur = s - str->str_ptr;
    str->str_pok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"%p ptr(%s)\n",str,str->str_ptr);
#endif
    return str->str_ptr;
}

double
str_2num(str)
register STR *str;
{
    if (!str)
	return 0.0;
    if (str->str_state == SS_INCR)
	Str_Grow(str,0);       /* just force copy down */
    str->str_state = SS_NORM;
    if (str->str_len && str->str_pok)
	str->str_u.str_nval = atof(str->str_ptr);
    else  {
	if (str == &str_undef)
	    return 0.0;
	if (dowarn)
	    warn("Use of uninitialized variable");
	str->str_u.str_nval = 0.0;
    }
    str->str_nok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"%p num(%g)\n",str,str->str_u.str_nval);
#endif
    return str->str_u.str_nval;
}

/* Note: str_sset() should not be called with a source string that needs
 * be reused, since it may destroy the source string if it is marked
 * as temporary.
 */

void
str_sset(dstr,sstr)
STR *dstr;
register STR *sstr;
{
#ifdef TAINT
    if (sstr)
	tainted |= sstr->str_tainted;
#endif
    if (sstr == dstr || dstr == &str_undef)
	return;
    if (!sstr)
	dstr->str_pok = dstr->str_nok = 0;
    else if (sstr->str_pok) {

	/*
	 * Check to see if we can just swipe the string.  If so, it's a
	 * possible small lose on short strings, but a big win on long ones.
	 * It might even be a win on short strings if dstr->str_ptr
	 * has to be allocated and sstr->str_ptr has to be freed.
	 */

	if (sstr->str_pok & SP_TEMP) {		/* slated for free anyway? */
	    if (dstr->str_ptr) {
		if (dstr->str_state == SS_INCR)
		    dstr->str_ptr -= dstr->str_u.str_useful;
		Safefree(dstr->str_ptr);
	    }
	    dstr->str_ptr = sstr->str_ptr;
	    dstr->str_len = sstr->str_len;
	    dstr->str_cur = sstr->str_cur;
	    dstr->str_state = sstr->str_state;
	    dstr->str_pok = sstr->str_pok & ~SP_TEMP;
#ifdef TAINT
	    dstr->str_tainted = sstr->str_tainted;
#endif
	    sstr->str_ptr = Nullch;
	    sstr->str_len = 0;
	    sstr->str_pok = 0;			/* wipe out any weird flags */
	    sstr->str_state = 0;		/* so sstr frees uneventfully */
	}
	else {					/* have to copy actual string */
	    if (dstr->str_ptr) {
		if (dstr->str_state == SS_INCR) {
			Str_Grow(dstr,0);
		}
	    }
	    str_nset(dstr,sstr->str_ptr,sstr->str_cur);
	}
	/*SUPPRESS 560*/
	if (dstr->str_nok = sstr->str_nok)
	    dstr->str_u.str_nval = sstr->str_u.str_nval;
	else {
#ifdef STRUCTCOPY
	    dstr->str_u = sstr->str_u;
#else
	    dstr->str_u.str_nval = sstr->str_u.str_nval;
#endif
	    if (dstr->str_cur == sizeof(STBP)) {
		char *tmps = dstr->str_ptr;

		if (*tmps == 'S' && bcmp(tmps,"StB",4) == 0) {
		    if (dstr->str_magic && dstr->str_magic->str_rare == 'X') {
			str_free(dstr->str_magic);
			dstr->str_magic = Nullstr;
		    }
		    if (!dstr->str_magic) {
			dstr->str_magic = str_smake(sstr->str_magic);
			dstr->str_magic->str_rare = 'X';
		    }
		}
	    }
	}
    }
    else if (sstr->str_nok)
	str_numset(dstr,sstr->str_u.str_nval);
    else {
	if (dstr->str_state == SS_INCR)
	    Str_Grow(dstr,0);       /* just force copy down */

#ifdef STRUCTCOPY
	dstr->str_u = sstr->str_u;
#else
	dstr->str_u.str_nval = sstr->str_u.str_nval;
#endif
	dstr->str_pok = dstr->str_nok = 0;
    }
}

void
str_nset(str,ptr,len)
register STR *str;
register char *ptr;
register STRLEN len;
{
    if (str == &str_undef)
	return;
    STR_GROW(str, len + 1);
    if (ptr)
	Move(ptr,str->str_ptr,len,char);
    str->str_cur = len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted = tainted;
#endif
}

void
str_set(str,ptr)
register STR *str;
register char *ptr;
{
    register STRLEN len;

    if (str == &str_undef)
	return;
    if (!ptr)
	ptr = "";
    len = strlen(ptr);
    STR_GROW(str, len + 1);
    Move(ptr,str->str_ptr,len+1,char);
    str->str_cur = len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted = tainted;
#endif
}

void
str_chop(str,ptr)	/* like set but assuming ptr is in str */
register STR *str;
register char *ptr;
{
    register STRLEN delta;

    if (!ptr || !(str->str_pok))
	return;
    delta = ptr - str->str_ptr;
    str->str_len -= delta;
    str->str_cur -= delta;
    str->str_ptr += delta;
    if (str->str_state == SS_INCR)
	str->str_u.str_useful += delta;
    else {
	str->str_u.str_useful = delta;
	str->str_state = SS_INCR;
    }
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer (and unstudy str) */
}

void
str_ncat(str,ptr,len)
register STR *str;
register char *ptr;
register STRLEN len;
{
    if (str == &str_undef)
	return;
    if (!(str->str_pok))
	(void)str_2ptr(str);
    STR_GROW(str, str->str_cur + len + 1);
    Move(ptr,str->str_ptr+str->str_cur,len,char);
    str->str_cur += len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted |= tainted;
#endif
}

void
str_scat(dstr,sstr)
STR *dstr;
register STR *sstr;
{
    if (!sstr)
	return;
#ifdef TAINT
    tainted |= sstr->str_tainted;
#endif
    if (!(sstr->str_pok))
	(void)str_2ptr(sstr);
    if (sstr)
	str_ncat(dstr,sstr->str_ptr,sstr->str_cur);
}

void
str_cat(str,ptr)
register STR *str;
register char *ptr;
{
    register STRLEN len;

    if (str == &str_undef)
	return;
    if (!ptr)
	return;
    if (!(str->str_pok))
	(void)str_2ptr(str);
    len = strlen(ptr);
    STR_GROW(str, str->str_cur + len + 1);
    Move(ptr,str->str_ptr+str->str_cur,len+1,char);
    str->str_cur += len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
#ifdef TAINT
    str->str_tainted |= tainted;
#endif
}

char *
str_append_till(str,from,fromend,delim,keeplist)
register STR *str;
register char *from;
register char *fromend;
register int delim;
char *keeplist;
{
    register char *to;
    register STRLEN len;

    if (str == &str_undef)
	return Nullch;
    if (!from)
	return Nullch;
    len = fromend - from;
    STR_GROW(str, str->str_cur + len + 1);
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
    to = str->str_ptr+str->str_cur;
    for (; from < fromend; from++,to++) {
	if (*from == '\\' && from+1 < fromend && delim != '\\') {
	    if (!keeplist) {
		if (from[1] == delim || from[1] == '\\')
		    from++;
		else
		    *to++ = *from++;
	    }
	    else if (from[1] && index(keeplist,from[1]))
		*to++ = *from++;
	    else
		from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    str->str_cur = to - str->str_ptr;
    return from;
}

STR *
#ifdef LEAKTEST
str_new(x,len)
int x;
#else
str_new(len)
#endif
STRLEN len;
{
    register STR *str;

    if (freestrroot) {
	str = freestrroot;
	freestrroot = str->str_magic;
	str->str_magic = Nullstr;
	str->str_state = SS_NORM;
    }
    else {
	Newz(700+x,str,1,STR);
    }
    if (len)
	STR_GROW(str, len + 1);
    return str;
}

void
str_magic(str, stab, how, name, namlen)
register STR *str;
STAB *stab;
int how;
char *name;
STRLEN namlen;
{
    if (str == &str_undef || str->str_magic)
	return;
    str->str_magic = Str_new(75,namlen);
    str = str->str_magic;
    str->str_u.str_stab = stab;
    str->str_rare = how;
    if (name)
	str_nset(str,name,namlen);
}

void
str_insert(bigstr,offset,len,little,littlelen)
STR *bigstr;
STRLEN offset;
STRLEN len;
char *little;
STRLEN littlelen;
{
    register char *big;
    register char *mid;
    register char *midend;
    register char *bigend;
    register int i;

    if (bigstr == &str_undef)
	return;
    bigstr->str_nok = 0;
    bigstr->str_pok = SP_VALID;	/* disable possible screamer */

    i = littlelen - len;
    if (i > 0) {			/* string might grow */
	STR_GROW(bigstr, bigstr->str_cur + i + 1);
	big = bigstr->str_ptr;
	mid = big + offset + len;
	midend = bigend = big + bigstr->str_cur;
	bigend += i;
	*bigend = '\0';
	while (midend > mid)		/* shove everything down */
	    *--bigend = *--midend;
	Move(little,big+offset,littlelen,char);
	bigstr->str_cur += i;
	STABSET(bigstr);
	return;
    }
    else if (i == 0) {
	Move(little,bigstr->str_ptr+offset,len,char);
	STABSET(bigstr);
	return;
    }

    big = bigstr->str_ptr;
    mid = big + offset;
    midend = mid + len;
    bigend = big + bigstr->str_cur;

    if (midend > bigend)
	fatal("panic: str_insert");

    if (mid - big > bigend - midend) {	/* faster to shorten from end */
	if (littlelen) {
	    Move(little, mid, littlelen,char);
	    mid += littlelen;
	}
	i = bigend - midend;
	if (i > 0) {
	    Move(midend, mid, i,char);
	    mid += i;
	}
	*mid = '\0';
	bigstr->str_cur = mid - big;
    }
    /*SUPPRESS 560*/
    else if (i = mid - big) {	/* faster from front */
	midend -= littlelen;
	mid = midend;
	str_chop(bigstr,midend-i);
	big += i;
	while (i--)
	    *--midend = *--big;
	if (littlelen)
	    Move(little, mid, littlelen,char);
    }
    else if (littlelen) {
	midend -= littlelen;
	str_chop(bigstr,midend);
	Move(little,midend,littlelen,char);
    }
    else {
	str_chop(bigstr,midend);
    }
    STABSET(bigstr);
}

/* make str point to what nstr did */

void
str_replace(str,nstr)
register STR *str;
register STR *nstr;
{
    if (str == &str_undef)
	return;
    if (str->str_state == SS_INCR)
	Str_Grow(str,0);	/* just force copy down */
    if (nstr->str_state == SS_INCR)
	Str_Grow(nstr,0);
    if (str->str_ptr)
	Safefree(str->str_ptr);
    str->str_ptr = nstr->str_ptr;
    str->str_len = nstr->str_len;
    str->str_cur = nstr->str_cur;
    str->str_pok = nstr->str_pok;
    str->str_nok = nstr->str_nok;
#ifdef STRUCTCOPY
    str->str_u = nstr->str_u;
#else
    str->str_u.str_nval = nstr->str_u.str_nval;
#endif
#ifdef TAINT
    str->str_tainted = nstr->str_tainted;
#endif
    if (nstr->str_magic)
	str_free(nstr->str_magic);
    Safefree(nstr);
}

void
str_free(str)
register STR *str;
{
    if (!str || str == &str_undef)
	return;
    if (str->str_state) {
	if (str->str_state == SS_FREE)	/* already freed */
	    return;
	if (str->str_state == SS_INCR && !(str->str_pok & 2)) {
	    str->str_ptr -= str->str_u.str_useful;
	    str->str_len += str->str_u.str_useful;
	}
    }
    if (str->str_magic)
	str_free(str->str_magic);
    str->str_magic = freestrroot;
#ifdef LEAKTEST
    if (str->str_len) {
	Safefree(str->str_ptr);
	str->str_ptr = Nullch;
    }
    if ((str->str_pok & SP_INTRP) && str->str_u.str_args)
	arg_free(str->str_u.str_args);
    Safefree(str);
#else /* LEAKTEST */
    if (str->str_len) {
	if (str->str_len > 127) {	/* next user not likely to want more */
	    Safefree(str->str_ptr);	/* so give it back to malloc */
	    str->str_ptr = Nullch;
	    str->str_len = 0;
	}
	else
	    str->str_ptr[0] = '\0';
    }
    if ((str->str_pok & SP_INTRP) && str->str_u.str_args)
	arg_free(str->str_u.str_args);
    str->str_cur = 0;
    str->str_nok = 0;
    str->str_pok = 0;
    str->str_state = SS_FREE;
#ifdef TAINT
    str->str_tainted = 0;
#endif
    freestrroot = str;
#endif /* LEAKTEST */
}

STRLEN
str_len(str)
register STR *str;
{
    if (!str)
	return 0;
    if (!(str->str_pok))
	(void)str_2ptr(str);
    if (str->str_ptr)
	return str->str_cur;
    else
	return 0;
}

int
str_eq(str1,str2)
register STR *str1;
register STR *str2;
{
    if (!str1 || str1 == &str_undef)
	return (str2 == Nullstr || str2 == &str_undef || !str2->str_cur);
    if (!str2 || str2 == &str_undef)
	return !str1->str_cur;

    if (!str1->str_pok)
	(void)str_2ptr(str1);
    if (!str2->str_pok)
	(void)str_2ptr(str2);

    if (str1->str_cur != str2->str_cur)
	return 0;

    return !bcmp(str1->str_ptr, str2->str_ptr, str1->str_cur);
}

int
str_cmp(str1,str2)
register STR *str1;
register STR *str2;
{
    int retval;

    if (!str1 || str1 == &str_undef)
	return (str2 == Nullstr || str2 == &str_undef || !str2->str_cur)?0:-1;
    if (!str2 || str2 == &str_undef)
	return str1->str_cur != 0;

    if (!str1->str_pok)
	(void)str_2ptr(str1);
    if (!str2->str_pok)
	(void)str_2ptr(str2);

    if (str1->str_cur < str2->str_cur) {
	/*SUPPRESS 560*/
	if (retval = memcmp(str1->str_ptr, str2->str_ptr, str1->str_cur))
	    return retval < 0 ? -1 : 1;
	else
	    return -1;
    }
    /*SUPPRESS 560*/
    else if (retval = memcmp(str1->str_ptr, str2->str_ptr, str2->str_cur))
	return retval < 0 ? -1 : 1;
    else if (str1->str_cur == str2->str_cur)
	return 0;
    else
	return 1;
}

char *
str_gets(str,fp,append)
register STR *str;
register FILE *fp;
int append;
{
    register char *bp;		/* we're going to steal some values */
    register int cnt;		/*  from the stdio struct and put EVERYTHING */
    register STDCHAR *ptr;	/*   in the innermost loop into registers */
    register int newline = rschar;/* (assuming >= 6 registers) */
    int i;
    STRLEN bpx;
    int shortbuffered;

    if (str == &str_undef)
	return Nullch;
    if (rspara) {		/* have to do this both before and after */
	do {			/* to make sure file boundaries work right */
	    i = getc(fp);
	    if (i != '\n') {
		ungetc(i,fp);
		break;
	    }
	} while (i != EOF);
    }
#ifdef STDSTDIO		/* Here is some breathtakingly efficient cheating */
    cnt = fp->_cnt;			/* get count into register */
    str->str_nok = 0;			/* invalidate number */
    str->str_pok = 1;			/* validate pointer */
    if (str->str_len - append <= cnt + 1) { /* make sure we have the room */
	if (cnt > 80 && str->str_len > append) {
	    shortbuffered = cnt - str->str_len + append + 1;
	    cnt -= shortbuffered;
	}
	else {
	    shortbuffered = 0;
	    STR_GROW(str, append+cnt+2);/* (remembering cnt can be -1) */
	}
    }
    else
	shortbuffered = 0;
    bp = str->str_ptr + append;		/* move these two too to registers */
    ptr = fp->_ptr;
    for (;;) {
      screamer:
	while (--cnt >= 0) {			/* this */	/* eat */
	    if ((*bp++ = *ptr++) == newline)	/* really */	/* dust */
		goto thats_all_folks;		/* screams */	/* sed :-) */
	}

	if (shortbuffered) {			/* oh well, must extend */
	    cnt = shortbuffered;
	    shortbuffered = 0;
	    bpx = bp - str->str_ptr;	/* prepare for possible relocation */
	    str->str_cur = bpx;
	    STR_GROW(str, str->str_len + append + cnt + 2);
	    bp = str->str_ptr + bpx;	/* reconstitute our pointer */
	    continue;
	}

	fp->_cnt = cnt;			/* deregisterize cnt and ptr */
	fp->_ptr = ptr;
	i = _filbuf(fp);		/* get more characters */
	cnt = fp->_cnt;
	ptr = fp->_ptr;			/* reregisterize cnt and ptr */

	bpx = bp - str->str_ptr;	/* prepare for possible relocation */
	str->str_cur = bpx;
	STR_GROW(str, bpx + cnt + 2);
	bp = str->str_ptr + bpx;	/* reconstitute our pointer */

	if (i == newline) {		/* all done for now? */
	    *bp++ = i;
	    goto thats_all_folks;
	}
	else if (i == EOF)		/* all done for ever? */
	    goto thats_really_all_folks;
	*bp++ = i;			/* now go back to screaming loop */
    }

thats_all_folks:
    if (rslen > 1 && (bp - str->str_ptr < rslen || bcmp(bp - rslen, rs, rslen)))
	goto screamer;	/* go back to the fray */
thats_really_all_folks:
    if (shortbuffered)
	cnt += shortbuffered;
    fp->_cnt = cnt;			/* put these back or we're in trouble */
    fp->_ptr = ptr;
    *bp = '\0';
    str->str_cur = bp - str->str_ptr;	/* set length */

#else /* !STDSTDIO */	/* The big, slow, and stupid way */

    {
	static char buf[8192];
	char * bpe = buf + sizeof(buf) - 3;

screamer:
	bp = buf;
	while ((i = getc(fp)) != EOF && (*bp++ = i) != newline && bp < bpe) ;

	if (append)
	    str_ncat(str, buf, bp - buf);
	else
	    str_nset(str, buf, bp - buf);
	if (i != EOF			/* joy */
	    &&
	    (i != newline
	     ||
	     (rslen > 1
	      &&
	      (str->str_cur < rslen
	       ||
	       bcmp(str->str_ptr + str->str_cur - rslen, rs, rslen)
	      )
	     )
	    )
	   )
	{
	    append = -1;
	    goto screamer;
	}
    }

#endif /* STDSTDIO */

    if (rspara) {
        while (i != EOF) {
	    i = getc(fp);
	    if (i != '\n') {
		ungetc(i,fp);
		break;
	    }
	}
    }
    return str->str_cur - append ? str->str_ptr : Nullch;
}

ARG *
parselist(str)
STR *str;
{
    register CMD *cmd;
    register ARG *arg;
    CMD *oldcurcmd = curcmd;
    int oldperldb = perldb;
    int retval;

    perldb = 0;
    str_sset(linestr,str);
    in_eval++;
    oldoldbufptr = oldbufptr = bufptr = str_get(linestr);
    bufend = bufptr + linestr->str_cur;
    if (++loop_ptr >= loop_max) {
        loop_max += 128;
        Renew(loop_stack, loop_max, struct loop);
    }
    loop_stack[loop_ptr].loop_label = "_EVAL_";
    loop_stack[loop_ptr].loop_sp = 0;
#ifdef DEBUGGING
    if (debug & 4) {
        deb("(Pushing label #%d _EVAL_)\n", loop_ptr);
    }
#endif
    if (setjmp(loop_stack[loop_ptr].loop_env)) {
	in_eval--;
	loop_ptr--;
	perldb = oldperldb;
	fatal("%s\n",stab_val(stabent("@",TRUE))->str_ptr);
    }
#ifdef DEBUGGING
    if (debug & 4) {
	char *tmps = loop_stack[loop_ptr].loop_label;
	deb("(Popping label #%d %s)\n",loop_ptr,
	    tmps ? tmps : "" );
    }
#endif
    loop_ptr--;
    error_count = 0;
    curcmd = &compiling;
    curcmd->c_line = oldcurcmd->c_line;
    retval = yyparse();
    curcmd = oldcurcmd;
    perldb = oldperldb;
    in_eval--;
    if (retval || error_count)
	fatal("Invalid component in string or format");
    cmd = eval_root;
    arg = cmd->c_expr;
    if (cmd->c_type != C_EXPR || cmd->c_next || arg->arg_type != O_LIST)
	fatal("panic: error in parselist %d %x %d", cmd->c_type,
	  cmd->c_next, arg ? arg->arg_type : -1);
    cmd->c_expr = Nullarg;
    cmd_free(cmd);
    eval_root = Nullcmd;
    return arg;
}

void
intrpcompile(src)
STR *src;
{
    register char *s = str_get(src);
    register char *send = s + src->str_cur;
    register STR *str;
    register char *t;
    STR *toparse;
    STRLEN len;
    register int brackets;
    register char *d;
    STAB *stab;
    char *checkpoint;
    int sawcase = 0;

    toparse = Str_new(76,0);
    str = Str_new(77,0);

    str_nset(str,"",0);
    str_nset(toparse,"",0);
    t = s;
    while (s < send) {
	if (*s == '\\' && s[1] && index("$@[{\\]}lLuUE",s[1])) {
	    str_ncat(str, t, s - t);
	    ++s;
	    if (isALPHA(*s)) {
		str_ncat(str, "$c", 2);
		sawcase = (*s != 'E');
	    }
	    else {
		if (*nointrp) {		/* in a regular expression */
		    if (*s == '@')	/* always strip \@ */ /*SUPPRESS 530*/
			;
		    else		/* don't strip \\, \[, \{ etc. */
			str_ncat(str,s-1,1);
		}
		str_ncat(str, "$b", 2);
	    }
	    str_ncat(str, s, 1);
	    ++s;
	    t = s;
	}
	else if (*s == '$' && s+1 < send && *nointrp && index(nointrp,s[1])) {
	    str_ncat(str, t, s - t);
	    str_ncat(str, "$b", 2);
	    str_ncat(str, s, 2);
	    s += 2;
	    t = s;
	}
	else if ((*s == '@' || *s == '$') && s+1 < send) {
	    str_ncat(str,t,s-t);
	    t = s;
	    if (*s == '$' && s[1] == '#' && (isALPHA(s[2]) || s[2] == '_'))
		s++;
	    s = scanident(s,send,tokenbuf,sizeof tokenbuf);
	    if (*t == '@' &&
	      (!(stab = stabent(tokenbuf,FALSE)) ||
		 (*s == '{' ? !stab_xhash(stab) : !stab_xarray(stab)) )) {
		str_ncat(str,"@",1);
		s = ++t;
		continue;	/* grandfather @ from old scripts */
	    }
	    str_ncat(str,"$a",2);
	    str_ncat(toparse,",",1);
	    if (t[1] != '{' && (*s == '['  || *s == '{' /* }} */ ) &&
	      (stab = stabent(tokenbuf,FALSE)) &&
	      ((*s == '[') ? (stab_xarray(stab) != 0) : (stab_xhash(stab) != 0)) ) {
		brackets = 0;
		checkpoint = s;
		do {
		    switch (*s) {
		    case '[':
			brackets++;
			break;
		    case '{':
			brackets++;
			break;
		    case ']':
			brackets--;
			break;
		    case '}':
			brackets--;
			break;
		    case '$':
		    case '%':
		    case '@':
		    case '&':
		    case '*':
			s = scanident(s,send,tokenbuf,sizeof tokenbuf);
			continue;
		    case '\'':
		    case '"':
			/*SUPPRESS 68*/
			s = cpytill(tokenbuf,s+1,send,*s,&len);
			if (s >= send)
			    fatal("Unterminated string");
			break;
		    }
		    s++;
		} while (brackets > 0 && s < send);
		if (s > send)
		    fatal("Unmatched brackets in string");
		if (*nointrp) {		/* we're in a regular expression */
		    d = checkpoint;
		    if (*d == '{' && s[-1] == '}') {	/* maybe {n,m} */
			++d;
			if (isDIGIT(*d)) {	/* matches /^{\d,?\d*}$/ */
			    if (*++d == ',')
				++d;
			    while (isDIGIT(*d))
				d++;
			    if (d == s - 1)
				s = checkpoint;		/* Is {n,m}! Backoff! */
			}
		    }
		    else if (*d == '[' && s[-1] == ']') { /* char class? */
			int weight = 2;		/* let's weigh the evidence */
			char seen[256];
			unsigned char un_char = 0, last_un_char;

			Zero(seen,256,char);
			*--s = '\0';
			if (d[1] == '^')
			    weight += 150;
			else if (d[1] == '$')
			    weight -= 3;
			if (isDIGIT(d[1])) {
			    if (d[2]) {
				if (isDIGIT(d[2]) && !d[3])
				    weight -= 10;
			    }
			    else
				weight -= 100;
			}
			for (d++; d < s; d++) {
			    last_un_char = un_char;
			    un_char = (unsigned char)*d;
			    switch (*d) {
			    case '&':
			    case '$':
				weight -= seen[un_char] * 10;
				if (isALNUM(d[1])) {
				    d = scanident(d,s,tokenbuf,sizeof tokenbuf);
				    if (stabent(tokenbuf,FALSE))
					weight -= 100;
				    else
					weight -= 10;
				}
				else if (*d == '$' && d[1] &&
				  index("[#!%*<>()-=",d[1])) {
				    if (!d[2] || /*{*/ index("])} =",d[2]))
					weight -= 10;
				    else
					weight -= 1;
				}
				break;
			    case '\\':
				un_char = 254;
				if (d[1]) {
				    if (index("wds",d[1]))
					weight += 100;
				    else if (seen['\''] || seen['"'])
					weight += 1;
				    else if (index("rnftb",d[1]))
					weight += 40;
				    else if (isDIGIT(d[1])) {
					weight += 40;
					while (d[1] && isDIGIT(d[1]))
					    d++;
				    }
				}
				else
				    weight += 100;
				break;
			    case '-':
				if (last_un_char < (unsigned char) d[1]
				  || d[1] == '\\') {
				    if (index("aA01! ",last_un_char))
					weight += 30;
				    if (index("zZ79~",d[1]))
					weight += 30;
				}
				else
				    weight -= 1;
			    default:
				if (isALPHA(*d) && d[1] && isALPHA(d[1])) {
				    bufptr = d;
				    if (yylex() != WORD)
					weight -= 150;
				    d = bufptr;
				}
				if (un_char == last_un_char + 1)
				    weight += 5;
				weight -= seen[un_char];
				break;
			    }
			    seen[un_char]++;
			}
#ifdef DEBUGGING
			if (debug & 512)
			    fprintf(stderr,"[%s] weight %d\n",
			      checkpoint+1,weight);
#endif
			*s++ = ']';
			if (weight >= 0)	/* probably a character class */
			    s = checkpoint;
		    }
		}
	    }
	    if (*t == '@')
		str_ncat(toparse, "join($\",", 8);
	    if (t[1] == '{' && s[-1] == '}') {
		str_ncat(toparse, t, 1);
		str_ncat(toparse, t+2, s - t - 3);
	    }
	    else
		str_ncat(toparse, t, s - t);
	    if (*t == '@')
		str_ncat(toparse, ")", 1);
	    t = s;
	}
	else
	    s++;
    }
    str_ncat(str,t,s-t);
    if (sawcase)
	str_ncat(str, "$cE", 3);
    if (toparse->str_ptr && *toparse->str_ptr == ',') {
	*toparse->str_ptr = '(';
	str_ncat(toparse,",$$);",5);
	str->str_u.str_args = parselist(toparse);
	str->str_u.str_args->arg_len--;		/* ignore $$ reference */
    }
    else
	str->str_u.str_args = Nullarg;
    str_free(toparse);
    str->str_pok |= SP_INTRP;
    str->str_nok = 0;
    str_replace(src,str);
}

STR *
interp(str,src,sp)
register STR *str;
STR *src;
int sp;
{
    register char *s;
    register char *t;
    register char *send;
    register STR **elem;
    int docase = 0;
    int l = 0;
    int u = 0;
    int L = 0;
    int U = 0;

    if (str == &str_undef)
	return Nullstr;
    if (!(src->str_pok & SP_INTRP)) {
	int oldsave = savestack->ary_fill;

	(void)savehptr(&curstash);
	curstash = curcmd->c_stash;	/* so stabent knows right package */
	intrpcompile(src);
	restorelist(oldsave);
    }
    s = src->str_ptr;		/* assumed valid since str_pok set */
    t = s;
    send = s + src->str_cur;

    if (src->str_u.str_args) {
	(void)eval(src->str_u.str_args,G_ARRAY,sp);
	/* Assuming we have correct # of args */
	elem = stack->ary_array + sp;
    }

    str_nset(str,"",0);
    while (s < send) {
	if (*s == '$' && s+1 < send) {
	    if (s-t > 0)
		str_ncat(str,t,s-t);
	    switch(*++s) {
	    default:
		fatal("panic: unknown interp cookie\n");
		break;
	    case 'a':
		str_scat(str,*++elem);
		break;
	    case 'b':
		str_ncat(str,++s,1);
		break;
	    case 'c':
		if (docase && str->str_cur >= docase) {
		    char *b = str->str_ptr + --docase;

		    if (L)
			lcase(b, str->str_ptr + str->str_cur);
		    else if (U)
			ucase(b, str->str_ptr + str->str_cur);

		    if (u)	/* note that l & u are independent of L & U */
			ucase(b, b+1);
		    else if (l)
			lcase(b, b+1);
		    l = u = 0;
		}
		docase = str->str_cur + 1;
		switch (*++s) {
		case 'u':
		    u = 1;
		    l = 0;
		    break;
		case 'U':
		    U = 1;
		    L = 0;
		    break;
		case 'l':
		    l = 1;
		    u = 0;
		    break;
		case 'L':
		    L = 1;
		    U = 0;
		    break;
		case 'E':
		    docase = L = U = l = u = 0;
		    break;
		}
		break;
	    }
	    t = ++s;
	}
	else
	    s++;
    }
    if (s-t > 0)
	str_ncat(str,t,s-t);
    return str;
}

static void
ucase(s,send)
register char *s;
register char *send;
{
    while (s < send) {
	if (isLOWER(*s))
	    *s = toupper(*s);
	s++;
    }
}

static void
lcase(s,send)
register char *s;
register char *send;
{
    while (s < send) {
	if (isUPPER(*s))
	    *s = tolower(*s);
	s++;
    }
}

void
str_inc(str)
register STR *str;
{
    register char *d;

    if (!str || str == &str_undef)
	return;
    if (str->str_nok) {
	str->str_u.str_nval += 1.0;
	str->str_pok = 0;
	return;
    }
    if (!str->str_pok || !*str->str_ptr) {
	str->str_u.str_nval = 1.0;
	str->str_nok = 1;
	str->str_pok = 0;
	return;
    }
    d = str->str_ptr;
    while (isALPHA(*d)) d++;
    while (isDIGIT(*d)) d++;
    if (*d) {
        str_numset(str,atof(str->str_ptr) + 1.0);  /* punt */
	return;
    }
    d--;
    while (d >= str->str_ptr) {
	if (isDIGIT(*d)) {
	    if (++*d <= '9')
		return;
	    *(d--) = '0';
	}
	else {
	    ++*d;
	    if (isALPHA(*d))
		return;
	    *(d--) -= 'z' - 'a' + 1;
	}
    }
    /* oh,oh, the number grew */
    STR_GROW(str, str->str_cur + 2);
    str->str_cur++;
    for (d = str->str_ptr + str->str_cur; d > str->str_ptr; d--)
	*d = d[-1];
    if (isDIGIT(d[1]))
	*d = '1';
    else
	*d = d[1];
}

void
str_dec(str)
register STR *str;
{
    if (!str || str == &str_undef)
	return;
    if (str->str_nok) {
	str->str_u.str_nval -= 1.0;
	str->str_pok = 0;
	return;
    }
    if (!str->str_pok) {
	str->str_u.str_nval = -1.0;
	str->str_nok = 1;
	return;
    }
    str_numset(str,atof(str->str_ptr) - 1.0);
}

/* Make a string that will exist for the duration of the expression
 * evaluation.  Actually, it may have to last longer than that, but
 * hopefully cmd_exec won't free it until it has been assigned to a
 * permanent location. */

static long tmps_size = -1;

STR *
str_mortal(oldstr)
STR *oldstr;
{
    register STR *str = Str_new(78,0);

    str_sset(str,oldstr);
    if (++tmps_max > tmps_size) {
	tmps_size = tmps_max;
	if (!(tmps_size & 127)) {
	    if (tmps_size)
		Renew(tmps_list, tmps_size + 128, STR*);
	    else
		New(702,tmps_list, 128, STR*);
	}
    }
    tmps_list[tmps_max] = str;
    if (str->str_pok)
	str->str_pok |= SP_TEMP;
    return str;
}

/* same thing without the copying */

STR *
str_2mortal(str)
register STR *str;
{
    if (!str || str == &str_undef)
	return str;
    if (++tmps_max > tmps_size) {
	tmps_size = tmps_max;
	if (!(tmps_size & 127)) {
	    if (tmps_size)
		Renew(tmps_list, tmps_size + 128, STR*);
	    else
		New(704,tmps_list, 128, STR*);
	}
    }
    tmps_list[tmps_max] = str;
    if (str->str_pok)
	str->str_pok |= SP_TEMP;
    return str;
}

STR *
str_make(s,len)
char *s;
STRLEN len;
{
    register STR *str = Str_new(79,0);

    if (!len)
	len = strlen(s);
    str_nset(str,s,len);
    return str;
}

STR *
str_nmake(n)
double n;
{
    register STR *str = Str_new(80,0);

    str_numset(str,n);
    return str;
}

/* make an exact duplicate of old */

STR *
str_smake(old)
register STR *old;
{
    register STR *new = Str_new(81,0);

    if (!old)
	return Nullstr;
    if (old->str_state == SS_FREE) {
	warn("semi-panic: attempt to dup freed string");
	return Nullstr;
    }
    if (old->str_state == SS_INCR && !(old->str_pok & 2))
	Str_Grow(old,0);
    if (new->str_ptr)
	Safefree(new->str_ptr);
    StructCopy(old,new,STR);
    if (old->str_ptr) {
	new->str_ptr = nsavestr(old->str_ptr,old->str_len);
	new->str_pok &= ~SP_TEMP;
    }
    return new;
}

void
str_reset(s,stash)
register char *s;
HASH *stash;
{
    register HENT *entry;
    register STAB *stab;
    register STR *str;
    register int i;
    register SPAT *spat;
    register int max;

    if (!*s) {		/* reset ?? searches */
	for (spat = stash->tbl_spatroot;
	  spat != Nullspat;
	  spat = spat->spat_next) {
	    spat->spat_flags &= ~SPAT_USED;
	}
	return;
    }

    /* reset variables */

    if (!stash->tbl_array)
	return;
    while (*s) {
	i = *s;
	if (s[1] == '-') {
	    s += 2;
	}
	max = *s++;
	for ( ; i <= max; i++) {
	    for (entry = stash->tbl_array[i];
	      entry;
	      entry = entry->hent_next) {
		stab = (STAB*)entry->hent_val;
		str = stab_val(stab);
		str->str_cur = 0;
		str->str_nok = 0;
#ifdef TAINT
		str->str_tainted = tainted;
#endif
		if (str->str_ptr != Nullch)
		    str->str_ptr[0] = '\0';
		if (stab_xarray(stab)) {
		    aclear(stab_xarray(stab));
		}
		if (stab_xhash(stab)) {
		    hclear(stab_xhash(stab), FALSE);
		    if (stab == envstab)
			environ[0] = Nullch;
		}
	    }
	}
    }
}

#ifdef TAINT
void
taintproper(s)
char *s;
{
#ifdef DEBUGGING
    if (debug & 2048)
	fprintf(stderr,"%s %d %d %d\n",s,tainted,uid, euid);
#endif
    if (tainted && (!euid || euid != uid || egid != gid || taintanyway)) {
	if (!unsafe)
	    fatal("%s", s);
	else if (dowarn)
	    warn("%s", s);
    }
}

void
taintenv()
{
    register STR *envstr;

    envstr = hfetch(stab_hash(envstab),"PATH",4,FALSE);
    if (envstr == &str_undef || envstr->str_tainted) {
	tainted = 1;
	if (envstr->str_tainted == 2)
	    taintproper("Insecure directory in PATH");
	else
	    taintproper("Insecure PATH");
    }
    envstr = hfetch(stab_hash(envstab),"IFS",3,FALSE);
    if (envstr != &str_undef && envstr->str_tainted) {
	tainted = 1;
	taintproper("Insecure IFS");
    }
}
#endif /* TAINT */
