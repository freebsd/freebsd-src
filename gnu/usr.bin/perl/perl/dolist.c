/* $RCSfile: dolist.c,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:32 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: dolist.c,v $
 * Revision 1.1.1.1  1994/09/10  06:27:32  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:36  nate
 * PERL!
 *
 * Revision 4.0.1.5  92/06/08  13:13:27  lwall
 * patch20: g pattern modifer sometimes returned extra values
 * patch20: m/$pattern/g didn't work
 * patch20: pattern modifiers i and o didn't interact right
 * patch20: @ in unpack failed too often
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 * patch20: slice on null list in scalar context returned random value
 * patch20: splice with negative offset didn't work with $[ = 1
 * patch20: fixed some memory leaks in splice
 * patch20: scalar keys %array now counts keys for you
 *
 * Revision 4.0.1.4  91/11/11  16:33:19  lwall
 * patch19: added little-endian pack/unpack options
 * patch19: sort $subname was busted by changes in 4.018
 *
 * Revision 4.0.1.3  91/11/05  17:07:02  lwall
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: /$foo/o optimizer could access deallocated data
 * patch11: certain optimizations of //g in array context returned too many values
 * patch11: regexp with no parens in array context returned wacky $`, $& and $'
 * patch11: $' not set right on some //g
 * patch11: added some support for 64-bit integers
 * patch11: grep of a split lost its values
 * patch11: added sort {} LIST
 * patch11: multiple reallocations now avoided in 1 .. 100000
 *
 * Revision 4.0.1.2  91/06/10  01:22:15  lwall
 * patch10: //g only worked first time through
 *
 * Revision 4.0.1.1  91/06/07  10:58:28  lwall
 * patch4: new copyright notice
 * patch4: added global modifier for pattern matches
 * patch4: // wouldn't use previous pattern if it started with a null character
 * patch4: //o and s///o now optimize themselves fully at runtime
 * patch4: $` was busted inside s///
 * patch4: caller($arg) didn't work except under debugger
 *
 * Revision 4.0  91/03/20  01:08:03  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"

static int sortcmp();
static int sortsub();

#ifdef BUGGY_MSC
 #pragma function(memcmp)
#endif /* BUGGY_MSC */

int
do_match(str,arg,gimme,arglast)
STR *str;
register ARG *arg;
int gimme;
int *arglast;
{
    register STR **st = stack->ary_array;
    register SPAT *spat = arg[2].arg_ptr.arg_spat;
    register char *t;
    register int sp = arglast[0] + 1;
    STR *srchstr = st[sp];
    register char *s = str_get(st[sp]);
    char *strend = s + st[sp]->str_cur;
    STR *tmpstr;
    char *myhint = hint;
    int global;
    int safebase;
    char *truebase = s;
    register REGEXP *rx = spat->spat_regexp;

    hint = Nullch;
    if (!spat) {
	if (gimme == G_ARRAY)
	    return --sp;
	str_set(str,Yes);
	STABSET(str);
	st[sp] = str;
	return sp;
    }
    global = spat->spat_flags & SPAT_GLOBAL;
    safebase = (gimme == G_ARRAY) || global;
    if (!s)
	fatal("panic: do_match");
    if (spat->spat_flags & SPAT_USED) {
#ifdef DEBUGGING
	if (debug & 8)
	    deb("2.SPAT USED\n");
#endif
	if (gimme == G_ARRAY)
	    return --sp;
	str_set(str,No);
	STABSET(str);
	st[sp] = str;
	return sp;
    }
    --sp;
    if (spat->spat_runtime) {
	nointrp = "|)";
	sp = eval(spat->spat_runtime,G_SCALAR,sp);
	st = stack->ary_array;
	t = str_get(tmpstr = st[sp--]);
	nointrp = "";
#ifdef DEBUGGING
	if (debug & 8)
	    deb("2.SPAT /%s/\n",t);
#endif
	if (!global && rx)
	    regfree(rx);
	spat->spat_regexp = Null(REGEXP*);	/* crucial if regcomp aborts */
	spat->spat_regexp = regcomp(t,t+tmpstr->str_cur,
	    spat->spat_flags & SPAT_FOLD);
	if (!spat->spat_regexp->prelen && lastspat)
	    spat = lastspat;
	if (spat->spat_flags & SPAT_KEEP) {
	    if (!(spat->spat_flags & SPAT_FOLD))
		scanconst(spat,spat->spat_regexp->precomp,
		    spat->spat_regexp->prelen);
	    if (spat->spat_runtime)
		arg_free(spat->spat_runtime);	/* it won't change, so */
	    spat->spat_runtime = Nullarg;	/* no point compiling again */
	    hoistmust(spat);
	    if (curcmd->c_expr && (curcmd->c_flags & CF_OPTIMIZE) == CFT_EVAL) {
		curcmd->c_flags &= ~CF_OPTIMIZE;
		opt_arg(curcmd, 1, curcmd->c_type == C_EXPR);
	    }
	}
	if (global) {
	    if (rx) {
	        if (rx->startp[0]) {
		    s = rx->endp[0];
		    if (s == rx->startp[0])
			s++;
		    if (s > strend) {
			regfree(rx);
			rx = spat->spat_regexp;
			goto nope;
		    }
		}
		regfree(rx);
	    }
	}
	else if (!spat->spat_regexp->nparens)
	    gimme = G_SCALAR;			/* accidental array context? */
	rx = spat->spat_regexp;
	if (regexec(rx, s, strend, s, 0,
	  srchstr->str_pok & SP_STUDIED ? srchstr : Nullstr,
	  safebase)) {
	    if (rx->subbase || global)
		curspat = spat;
	    lastspat = spat;
	    goto gotcha;
	}
	else {
	    if (gimme == G_ARRAY)
		return sp;
	    str_sset(str,&str_no);
	    STABSET(str);
	    st[++sp] = str;
	    return sp;
	}
    }
    else {
#ifdef DEBUGGING
	if (debug & 8) {
	    char ch;

	    if (spat->spat_flags & SPAT_ONCE)
		ch = '?';
	    else
		ch = '/';
	    deb("2.SPAT %c%s%c\n",ch,rx->precomp,ch);
	}
#endif
	if (!rx->prelen && lastspat) {
	    spat = lastspat;
	    rx = spat->spat_regexp;
	}
	t = s;
    play_it_again:
	if (global && rx->startp[0]) {
	    t = s = rx->endp[0];
	    if (s == rx->startp[0])
		s++,t++;
	    if (s > strend)
		goto nope;
	}
	if (myhint) {
	    if (myhint < s || myhint > strend)
		fatal("panic: hint in do_match");
	    s = myhint;
	    if (rx->regback >= 0) {
		s -= rx->regback;
		if (s < t)
		    s = t;
	    }
	    else
		s = t;
	}
	else if (spat->spat_short) {
	    if (spat->spat_flags & SPAT_SCANFIRST) {
		if (srchstr->str_pok & SP_STUDIED) {
		    if (screamfirst[spat->spat_short->str_rare] < 0)
			goto nope;
		    else if (!(s = screaminstr(srchstr,spat->spat_short)))
			goto nope;
		    else if (spat->spat_flags & SPAT_ALL)
			goto yup;
		}
#ifndef lint
		else if (!(s = fbminstr((unsigned char*)s,
		  (unsigned char*)strend, spat->spat_short)))
		    goto nope;
#endif
		else if (spat->spat_flags & SPAT_ALL)
		    goto yup;
		if (s && rx->regback >= 0) {
		    ++spat->spat_short->str_u.str_useful;
		    s -= rx->regback;
		    if (s < t)
			s = t;
		}
		else
		    s = t;
	    }
	    else if (!multiline && (*spat->spat_short->str_ptr != *s ||
	      bcmp(spat->spat_short->str_ptr, s, spat->spat_slen) ))
		goto nope;
	    if (--spat->spat_short->str_u.str_useful < 0) {
		str_free(spat->spat_short);
		spat->spat_short = Nullstr;	/* opt is being useless */
	    }
	}
	if (!rx->nparens && !global) {
	    gimme = G_SCALAR;			/* accidental array context? */
	    safebase = FALSE;
	}
	if (regexec(rx, s, strend, truebase, 0,
	  srchstr->str_pok & SP_STUDIED ? srchstr : Nullstr,
	  safebase)) {
	    if (rx->subbase || global)
		curspat = spat;
	    lastspat = spat;
	    if (spat->spat_flags & SPAT_ONCE)
		spat->spat_flags |= SPAT_USED;
	    goto gotcha;
	}
	else {
	    if (global)
		rx->startp[0] = Nullch;
	    if (gimme == G_ARRAY)
		return sp;
	    str_sset(str,&str_no);
	    STABSET(str);
	    st[++sp] = str;
	    return sp;
	}
    }
    /*NOTREACHED*/

  gotcha:
    if (gimme == G_ARRAY) {
	int iters, i, len;

	iters = rx->nparens;
	if (global && !iters)
	    i = 1;
	else
	    i = 0;
	if (sp + iters + i >= stack->ary_max) {
	    astore(stack,sp + iters + i, Nullstr);
	    st = stack->ary_array;		/* possibly realloced */
	}

	for (i = !i; i <= iters; i++) {
	    st[++sp] = str_mortal(&str_no);
	    /*SUPPRESS 560*/
	    if (s = rx->startp[i]) {
		len = rx->endp[i] - s;
		if (len > 0)
		    str_nset(st[sp],s,len);
	    }
	}
	if (global) {
	    truebase = rx->subbeg;
	    goto play_it_again;
	}
	return sp;
    }
    else {
	str_sset(str,&str_yes);
	STABSET(str);
	st[++sp] = str;
	return sp;
    }

yup:
    ++spat->spat_short->str_u.str_useful;
    lastspat = spat;
    if (spat->spat_flags & SPAT_ONCE)
	spat->spat_flags |= SPAT_USED;
    if (global) {
	rx->subbeg = t;
	rx->subend = strend;
	rx->startp[0] = s;
	rx->endp[0] = s + spat->spat_short->str_cur;
	curspat = spat;
	goto gotcha;
    }
    if (sawampersand) {
	char *tmps;

	if (rx->subbase)
	    Safefree(rx->subbase);
	tmps = rx->subbase = nsavestr(t,strend-t);
	rx->subbeg = tmps;
	rx->subend = tmps + (strend-t);
	tmps = rx->startp[0] = tmps + (s - t);
	rx->endp[0] = tmps + spat->spat_short->str_cur;
	curspat = spat;
    }
    str_sset(str,&str_yes);
    STABSET(str);
    st[++sp] = str;
    return sp;

nope:
    rx->startp[0] = Nullch;
    if (spat->spat_short)
	++spat->spat_short->str_u.str_useful;
    if (gimme == G_ARRAY)
	return sp;
    str_sset(str,&str_no);
    STABSET(str);
    st[++sp] = str;
    return sp;
}

#ifdef BUGGY_MSC
 #pragma intrinsic(memcmp)
#endif /* BUGGY_MSC */

int
do_split(str,spat,limit,gimme,arglast)
STR *str;
register SPAT *spat;
register int limit;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    STR **st = ary->ary_array;
    register int sp = arglast[0] + 1;
    register char *s = str_get(st[sp]);
    char *strend = s + st[sp--]->str_cur;
    register STR *dstr;
    register char *m;
    int iters = 0;
    int maxiters = (strend - s) + 10;
    int i;
    char *orig;
    int origlimit = limit;
    int realarray = 0;

    if (!spat || !s)
	fatal("panic: do_split");
    else if (spat->spat_runtime) {
	nointrp = "|)";
	sp = eval(spat->spat_runtime,G_SCALAR,sp);
	st = stack->ary_array;
	m = str_get(dstr = st[sp--]);
	nointrp = "";
	if (*m == ' ' && dstr->str_cur == 1) {
	    str_set(dstr,"\\s+");
	    m = dstr->str_ptr;
	    spat->spat_flags |= SPAT_SKIPWHITE;
	}
	if (spat->spat_regexp) {
	    regfree(spat->spat_regexp);
	    spat->spat_regexp = Null(REGEXP*);	/* avoid possible double free */
	}
	spat->spat_regexp = regcomp(m,m+dstr->str_cur,
	    spat->spat_flags & SPAT_FOLD);
	if (spat->spat_flags & SPAT_KEEP ||
	    (spat->spat_runtime->arg_type == O_ITEM &&
	      (spat->spat_runtime[1].arg_type & A_MASK) == A_SINGLE) ) {
	    arg_free(spat->spat_runtime);	/* it won't change, so */
	    spat->spat_runtime = Nullarg;	/* no point compiling again */
	}
    }
#ifdef DEBUGGING
    if (debug & 8) {
	deb("2.SPAT /%s/\n",spat->spat_regexp->precomp);
    }
#endif
    ary = stab_xarray(spat->spat_repl[1].arg_ptr.arg_stab);
    if (ary && (gimme != G_ARRAY || (spat->spat_flags & SPAT_ONCE))) {
	realarray = 1;
	if (!(ary->ary_flags & ARF_REAL)) {
	    ary->ary_flags |= ARF_REAL;
	    for (i = ary->ary_fill; i >= 0; i--)
		ary->ary_array[i] = Nullstr;	/* don't free mere refs */
	}
	ary->ary_fill = -1;
	sp = -1;	/* temporarily switch stacks */
    }
    else
	ary = stack;
    orig = s;
    if (spat->spat_flags & SPAT_SKIPWHITE) {
	while (isSPACE(*s))
	    s++;
    }
    if (!limit)
	limit = maxiters + 2;
    if (strEQ("\\s+",spat->spat_regexp->precomp)) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && !isSPACE(*m); m++) ;
	    if (m >= strend)
		break;
	    dstr = Str_new(30,m-s);
	    str_nset(dstr,s,m-s);
	    if (!realarray)
		str_2mortal(dstr);
	    (void)astore(ary, ++sp, dstr);
	    /*SUPPRESS 530*/
	    for (s = m + 1; s < strend && isSPACE(*s); s++) ;
	}
    }
    else if (strEQ("^",spat->spat_regexp->precomp)) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && *m != '\n'; m++) ;
	    m++;
	    if (m >= strend)
		break;
	    dstr = Str_new(30,m-s);
	    str_nset(dstr,s,m-s);
	    if (!realarray)
		str_2mortal(dstr);
	    (void)astore(ary, ++sp, dstr);
	    s = m;
	}
    }
    else if (spat->spat_short) {
	i = spat->spat_short->str_cur;
	if (i == 1) {
	    int fold = (spat->spat_flags & SPAT_FOLD);

	    i = *spat->spat_short->str_ptr;
	    if (fold && isUPPER(i))
		i = tolower(i);
	    while (--limit) {
		if (fold) {
		    for ( m = s;
			  m < strend && *m != i &&
			    (!isUPPER(*m) || tolower(*m) != i);
			  m++)			/*SUPPRESS 530*/
			;
		}
		else				/*SUPPRESS 530*/
		    for (m = s; m < strend && *m != i; m++) ;
		if (m >= strend)
		    break;
		dstr = Str_new(30,m-s);
		str_nset(dstr,s,m-s);
		if (!realarray)
		    str_2mortal(dstr);
		(void)astore(ary, ++sp, dstr);
		s = m + 1;
	    }
	}
	else {
#ifndef lint
	    while (s < strend && --limit &&
	      (m=fbminstr((unsigned char*)s, (unsigned char*)strend,
		    spat->spat_short)) )
#endif
	    {
		dstr = Str_new(31,m-s);
		str_nset(dstr,s,m-s);
		if (!realarray)
		    str_2mortal(dstr);
		(void)astore(ary, ++sp, dstr);
		s = m + i;
	    }
	}
    }
    else {
	maxiters += (strend - s) * spat->spat_regexp->nparens;
	while (s < strend && --limit &&
	    regexec(spat->spat_regexp, s, strend, orig, 1, Nullstr, TRUE) ) {
	    if (spat->spat_regexp->subbase
	      && spat->spat_regexp->subbase != orig) {
		m = s;
		s = orig;
		orig = spat->spat_regexp->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = spat->spat_regexp->startp[0];
	    dstr = Str_new(32,m-s);
	    str_nset(dstr,s,m-s);
	    if (!realarray)
		str_2mortal(dstr);
	    (void)astore(ary, ++sp, dstr);
	    if (spat->spat_regexp->nparens) {
		for (i = 1; i <= spat->spat_regexp->nparens; i++) {
		    s = spat->spat_regexp->startp[i];
		    m = spat->spat_regexp->endp[i];
		    dstr = Str_new(33,m-s);
		    str_nset(dstr,s,m-s);
		    if (!realarray)
			str_2mortal(dstr);
		    (void)astore(ary, ++sp, dstr);
		}
	    }
	    s = spat->spat_regexp->endp[0];
	}
    }
    if (realarray)
	iters = sp + 1;
    else
	iters = sp - arglast[0];
    if (iters > maxiters)
	fatal("Split loop");
    if (s < strend || origlimit) {	/* keep field after final delim? */
	dstr = Str_new(34,strend-s);
	str_nset(dstr,s,strend-s);
	if (!realarray)
	    str_2mortal(dstr);
	(void)astore(ary, ++sp, dstr);
	iters++;
    }
    else {
#ifndef I286x
	while (iters > 0 && ary->ary_array[sp]->str_cur == 0)
	    iters--,sp--;
#else
	char *zaps;
	int   zapb;

	if (iters > 0) {
		zaps = str_get(afetch(ary,sp,FALSE));
		zapb = (int) *zaps;
	}

	while (iters > 0 && (!zapb)) {
	    iters--,sp--;
	    if (iters > 0) {
		zaps = str_get(afetch(ary,iters-1,FALSE));
		zapb = (int) *zaps;
	    }
	}
#endif
    }
    if (realarray) {
	ary->ary_fill = sp;
	if (gimme == G_ARRAY) {
	    sp++;
	    astore(stack, arglast[0] + 1 + sp, Nullstr);
	    Copy(ary->ary_array, stack->ary_array + arglast[0] + 1, sp, STR*);
	    return arglast[0] + sp;
	}
    }
    else {
	if (gimme == G_ARRAY)
	    return sp;
    }
    sp = arglast[0] + 1;
    str_numset(str,(double)iters);
    STABSET(str);
    st[sp] = str;
    return sp;
}

int
do_unpack(str,gimme,arglast)
STR *str;
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;
    register int sp = arglast[0] + 1;
    register char *pat = str_get(st[sp++]);
    register char *s = str_get(st[sp]);
    char *strend = s + st[sp--]->str_cur;
    char *strbeg = s;
    register char *patend = pat + st[sp]->str_cur;
    int datumtype;
    register int len;
    register int bits;

    /* These must not be in registers: */
    short ashort;
    int aint;
    long along;
#ifdef QUAD
    quad aquad;
#endif
    unsigned short aushort;
    unsigned int auint;
    unsigned long aulong;
#ifdef QUAD
    unsigned quad auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    int checksum = 0;
    unsigned long culong;
    double cdouble;

    if (gimme != G_ARRAY) {		/* arrange to do first one only */
	/*SUPPRESS 530*/
	for (patend = pat; !isALPHA(*patend) || *patend == 'x'; patend++) ;
	if (index("aAbBhH", *patend) || *pat == '%') {
	    patend++;
	    while (isDIGIT(*patend) || *patend == '*')
		patend++;
	}
	else
	    patend++;
    }
    sp--;
    while (pat < patend) {
      reparse:
	datumtype = *pat++;
	if (pat >= patend)
	    len = 1;
	else if (*pat == '*') {
	    len = strend - strbeg;	/* long enough */
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat))
		len = (len * 10) + (*pat++ - '0');
	}
	else
	    len = (datumtype != '@');
	switch(datumtype) {
	default:
	    break;
	case '%':
	    if (len == 1 && pat[-1] != '1')
		len = 16;
	    checksum = len;
	    culong = 0;
	    cdouble = 0;
	    if (pat < patend)
		goto reparse;
	    break;
	case '@':
	    if (len > strend - strbeg)
		fatal("@ outside of string");
	    s = strbeg + len;
	    break;
	case 'X':
	    if (len > s - strbeg)
		fatal("X outside of string");
	    s -= len;
	    break;
	case 'x':
	    if (len > strend - s)
		fatal("x outside of string");
	    s += len;
	    break;
	case 'A':
	case 'a':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum)
		goto uchar_checksum;
	    str = Str_new(35,len);
	    str_nset(str,s,len);
	    s += len;
	    if (datumtype == 'A') {
		aptr = s;	/* borrow register */
		s = str->str_ptr + len - 1;
		while (s >= str->str_ptr && (!*s || isSPACE(*s)))
		    s--;
		*++s = '\0';
		str->str_cur = s - str->str_ptr;
		s = aptr;	/* unborrow register */
	    }
	    (void)astore(stack, ++sp, str_2mortal(str));
	    break;
	case 'B':
	case 'b':
	    if (pat[-1] == '*' || len > (strend - s) * 8)
		len = (strend - s) * 8;
	    str = Str_new(35, len + 1);
	    str->str_cur = len;
	    str->str_pok = 1;
	    aptr = pat;			/* borrow register */
	    pat = str->str_ptr;
	    if (datumtype == 'b') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)		/*SUPPRESS 595*/
			bits >>= 1;
		    else
			bits = *s++;
		    *pat++ = '0' + (bits & 1);
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)
			bits <<= 1;
		    else
			bits = *s++;
		    *pat++ = '0' + ((bits & 128) != 0);
		}
	    }
	    *pat = '\0';
	    pat = aptr;			/* unborrow register */
	    (void)astore(stack, ++sp, str_2mortal(str));
	    break;
	case 'H':
	case 'h':
	    if (pat[-1] == '*' || len > (strend - s) * 2)
		len = (strend - s) * 2;
	    str = Str_new(35, len + 1);
	    str->str_cur = len;
	    str->str_pok = 1;
	    aptr = pat;			/* borrow register */
	    pat = str->str_ptr;
	    if (datumtype == 'h') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits >>= 4;
		    else
			bits = *s++;
		    *pat++ = hexdigit[bits & 15];
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits <<= 4;
		    else
			bits = *s++;
		    *pat++ = hexdigit[(bits >> 4) & 15];
		}
	    }
	    *pat = '\0';
	    pat = aptr;			/* unborrow register */
	    (void)astore(stack, ++sp, str_2mortal(str));
	    break;
	case 'c':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    culong += aint;
		}
	    }
	    else {
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    str = Str_new(36,0);
		    str_numset(str,(double)aint);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'C':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
	      uchar_checksum:
		while (len-- > 0) {
		    auint = *s++ & 255;
		    culong += auint;
		}
	    }
	    else {
		while (len-- > 0) {
		    auint = *s++ & 255;
		    str = Str_new(37,0);
		    str_numset(str,(double)auint);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 's':
	    along = (strend - s) / sizeof(short);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s,&ashort,1,short);
		    s += sizeof(short);
		    culong += ashort;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s,&ashort,1,short);
		    s += sizeof(short);
		    str = Str_new(38,0);
		    str_numset(str,(double)ashort);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'v':
	case 'n':
	case 'S':
	    along = (strend - s) / sizeof(unsigned short);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s,&aushort,1,unsigned short);
		    s += sizeof(unsigned short);
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    culong += aushort;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s,&aushort,1,unsigned short);
		    s += sizeof(unsigned short);
		    str = Str_new(39,0);
#ifdef HAS_NTOHS
		    if (datumtype == 'n')
			aushort = ntohs(aushort);
#endif
#ifdef HAS_VTOHS
		    if (datumtype == 'v')
			aushort = vtohs(aushort);
#endif
		    str_numset(str,(double)aushort);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'i':
	    along = (strend - s) / sizeof(int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s,&aint,1,int);
		    s += sizeof(int);
		    if (checksum > 32)
			cdouble += (double)aint;
		    else
			culong += aint;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s,&aint,1,int);
		    s += sizeof(int);
		    str = Str_new(40,0);
		    str_numset(str,(double)aint);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'I':
	    along = (strend - s) / sizeof(unsigned int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s,&auint,1,unsigned int);
		    s += sizeof(unsigned int);
		    if (checksum > 32)
			cdouble += (double)auint;
		    else
			culong += auint;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s,&auint,1,unsigned int);
		    s += sizeof(unsigned int);
		    str = Str_new(41,0);
		    str_numset(str,(double)auint);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'l':
	    along = (strend - s) / sizeof(long);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s,&along,1,long);
		    s += sizeof(long);
		    if (checksum > 32)
			cdouble += (double)along;
		    else
			culong += along;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s,&along,1,long);
		    s += sizeof(long);
		    str = Str_new(42,0);
		    str_numset(str,(double)along);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'V':
	case 'N':
	case 'L':
	    along = (strend - s) / sizeof(unsigned long);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s,&aulong,1,unsigned long);
		    s += sizeof(unsigned long);
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    if (checksum > 32)
			cdouble += (double)aulong;
		    else
			culong += aulong;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s,&aulong,1,unsigned long);
		    s += sizeof(unsigned long);
		    str = Str_new(43,0);
#ifdef HAS_NTOHL
		    if (datumtype == 'N')
			aulong = ntohl(aulong);
#endif
#ifdef HAS_VTOHL
		    if (datumtype == 'V')
			aulong = vtohl(aulong);
#endif
		    str_numset(str,(double)aulong);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'p':
	    along = (strend - s) / sizeof(char*);
	    if (len > along)
		len = along;
	    while (len-- > 0) {
		if (sizeof(char*) > strend - s)
		    break;
		else {
		    Copy(s,&aptr,1,char*);
		    s += sizeof(char*);
		}
		str = Str_new(44,0);
		if (aptr)
		    str_set(str,aptr);
		(void)astore(stack, ++sp, str_2mortal(str));
	    }
	    break;
#ifdef QUAD
	case 'q':
	    while (len-- > 0) {
		if (s + sizeof(quad) > strend)
		    aquad = 0;
		else {
		    Copy(s,&aquad,1,quad);
		    s += sizeof(quad);
		}
		str = Str_new(42,0);
		str_numset(str,(double)aquad);
		(void)astore(stack, ++sp, str_2mortal(str));
	    }
	    break;
	case 'Q':
	    while (len-- > 0) {
		if (s + sizeof(unsigned quad) > strend)
		    auquad = 0;
		else {
		    Copy(s,&auquad,1,unsigned quad);
		    s += sizeof(unsigned quad);
		}
		str = Str_new(43,0);
		str_numset(str,(double)auquad);
		(void)astore(stack, ++sp, str_2mortal(str));
	    }
	    break;
#endif
	/* float and double added gnb@melba.bby.oz.au 22/11/89 */
	case 'f':
	case 'F':
	    along = (strend - s) / sizeof(float);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &afloat,1, float);
		    s += sizeof(float);
		    cdouble += afloat;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s, &afloat,1, float);
		    s += sizeof(float);
		    str = Str_new(47, 0);
		    str_numset(str, (double)afloat);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'd':
	case 'D':
	    along = (strend - s) / sizeof(double);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &adouble,1, double);
		    s += sizeof(double);
		    cdouble += adouble;
		}
	    }
	    else {
		while (len-- > 0) {
		    Copy(s, &adouble,1, double);
		    s += sizeof(double);
		    str = Str_new(48, 0);
		    str_numset(str, (double)adouble);
		    (void)astore(stack, ++sp, str_2mortal(str));
		}
	    }
	    break;
	case 'u':
	    along = (strend - s) * 3 / 4;
	    str = Str_new(42,along);
	    while (s < strend && *s > ' ' && *s < 'a') {
		int a,b,c,d;
		char hunk[4];

		hunk[3] = '\0';
		len = (*s++ - ' ') & 077;
		while (len > 0) {
		    if (s < strend && *s >= ' ')
			a = (*s++ - ' ') & 077;
		    else
			a = 0;
		    if (s < strend && *s >= ' ')
			b = (*s++ - ' ') & 077;
		    else
			b = 0;
		    if (s < strend && *s >= ' ')
			c = (*s++ - ' ') & 077;
		    else
			c = 0;
		    if (s < strend && *s >= ' ')
			d = (*s++ - ' ') & 077;
		    else
			d = 0;
		    hunk[0] = a << 2 | b >> 4;
		    hunk[1] = b << 4 | c >> 2;
		    hunk[2] = c << 6 | d;
		    str_ncat(str,hunk, len > 3 ? 3 : len);
		    len -= 3;
		}
		if (*s == '\n')
		    s++;
		else if (s[1] == '\n')		/* possible checksum byte */
		    s += 2;
	    }
	    (void)astore(stack, ++sp, str_2mortal(str));
	    break;
	}
	if (checksum) {
	    str = Str_new(42,0);
	    if (index("fFdD", datumtype) ||
	      (checksum > 32 && index("iIlLN", datumtype)) ) {
		double modf();
		double trouble;

		adouble = 1.0;
		while (checksum >= 16) {
		    checksum -= 16;
		    adouble *= 65536.0;
		}
		while (checksum >= 4) {
		    checksum -= 4;
		    adouble *= 16.0;
		}
		while (checksum--)
		    adouble *= 2.0;
		along = (1 << checksum) - 1;
		while (cdouble < 0.0)
		    cdouble += adouble;
		cdouble = modf(cdouble / adouble, &trouble) * adouble;
		str_numset(str,cdouble);
	    }
	    else {
		if (checksum < 32) {
		    along = (1 << checksum) - 1;
		    culong &= (unsigned long)along;
		}
		str_numset(str,(double)culong);
	    }
	    (void)astore(stack, ++sp, str_2mortal(str));
	    checksum = 0;
	}
    }
    return sp;
}

int
do_slice(stab,str,numarray,lval,gimme,arglast)
STAB *stab;
STR *str;
int numarray;
int lval;
int gimme;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int max = arglast[2];
    register char *tmps;
    register int len;
    register int magic = 0;
    register ARRAY *ary;
    register HASH *hash;
    int oldarybase = arybase;

    if (numarray) {
	if (numarray == 2) {		/* a slice of a LIST */
	    ary = stack;
	    ary->ary_fill = arglast[3];
	    arybase -= max + 1;
	    st[sp] = str;		/* make stack size available */
	    str_numset(str,(double)(sp - 1));
	}
	else
	    ary = stab_array(stab);	/* a slice of an array */
    }
    else {
	if (lval) {
	    if (stab == envstab)
		magic = 'E';
	    else if (stab == sigstab)
		magic = 'S';
#ifdef SOME_DBM
	    else if (stab_hash(stab)->tbl_dbm)
		magic = 'D';
#endif /* SOME_DBM */
	}
	hash = stab_hash(stab);		/* a slice of an associative array */
    }

    if (gimme == G_ARRAY) {
	if (numarray) {
	    while (sp < max) {
		if (st[++sp]) {
		    st[sp-1] = afetch(ary,
		      ((int)str_gnum(st[sp])) - arybase, lval);
		}
		else
		    st[sp-1] = &str_undef;
	    }
	}
	else {
	    while (sp < max) {
		if (st[++sp]) {
		    tmps = str_get(st[sp]);
		    len = st[sp]->str_cur;
		    st[sp-1] = hfetch(hash,tmps,len, lval);
		    if (magic)
			str_magic(st[sp-1],stab,magic,tmps,len);
		}
		else
		    st[sp-1] = &str_undef;
	    }
	}
	sp--;
    }
    else {
	if (sp == max)
	    st[sp] = &str_undef;
	else if (numarray) {
	    if (st[max])
		st[sp] = afetch(ary,
		  ((int)str_gnum(st[max])) - arybase, lval);
	    else
		st[sp] = &str_undef;
	}
	else {
	    if (st[max]) {
		tmps = str_get(st[max]);
		len = st[max]->str_cur;
		st[sp] = hfetch(hash,tmps,len, lval);
		if (magic)
		    str_magic(st[sp],stab,magic,tmps,len);
	    }
	    else
		st[sp] = &str_undef;
	}
    }
    arybase = oldarybase;
    return sp;
}

int
do_splice(ary,gimme,arglast)
register ARRAY *ary;
int gimme;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    int max = arglast[2] + 1;
    register STR **src;
    register STR **dst;
    register int i;
    register int offset;
    register int length;
    int newlen;
    int after;
    int diff;
    STR **tmparyval;

    if (++sp < max) {
	offset = (int)str_gnum(st[sp]);
	if (offset < 0)
	    offset += ary->ary_fill + 1;
	else
	    offset -= arybase;
	if (++sp < max) {
	    length = (int)str_gnum(st[sp++]);
	    if (length < 0)
		length = 0;
	}
	else
	    length = ary->ary_max + 1;		/* close enough to infinity */
    }
    else {
	offset = 0;
	length = ary->ary_max + 1;
    }
    if (offset < 0) {
	length += offset;
	offset = 0;
	if (length < 0)
	    length = 0;
    }
    if (offset > ary->ary_fill + 1)
	offset = ary->ary_fill + 1;
    after = ary->ary_fill + 1 - (offset + length);
    if (after < 0) {				/* not that much array */
	length += after;			/* offset+length now in array */
	after = 0;
	if (!ary->ary_alloc) {
	    afill(ary,0);
	    afill(ary,-1);
	}
    }

    /* At this point, sp .. max-1 is our new LIST */

    newlen = max - sp;
    diff = newlen - length;

    if (diff < 0) {				/* shrinking the area */
	if (newlen) {
	    New(451, tmparyval, newlen, STR*);	/* so remember insertion */
	    Copy(st+sp, tmparyval, newlen, STR*);
	}

	sp = arglast[0] + 1;
	if (gimme == G_ARRAY) {			/* copy return vals to stack */
	    if (sp + length >= stack->ary_max) {
		astore(stack,sp + length, Nullstr);
		st = stack->ary_array;
	    }
	    Copy(ary->ary_array+offset, st+sp, length, STR*);
	    if (ary->ary_flags & ARF_REAL) {
		for (i = length, dst = st+sp; i; i--)
		    str_2mortal(*dst++);	/* free them eventualy */
	    }
	    sp += length - 1;
	}
	else {
	    st[sp] = ary->ary_array[offset+length-1];
	    if (ary->ary_flags & ARF_REAL) {
		str_2mortal(st[sp]);
		for (i = length - 1, dst = &ary->ary_array[offset]; i > 0; i--)
		    str_free(*dst++);	/* free them now */
	    }
	}
	ary->ary_fill += diff;

	/* pull up or down? */

	if (offset < after) {			/* easier to pull up */
	    if (offset) {			/* esp. if nothing to pull */
		src = &ary->ary_array[offset-1];
		dst = src - diff;		/* diff is negative */
		for (i = offset; i > 0; i--)	/* can't trust Copy */
		    *dst-- = *src--;
	    }
	    Zero(ary->ary_array, -diff, STR*);
	    ary->ary_array -= diff;		/* diff is negative */
	    ary->ary_max += diff;
	}
	else {
	    if (after) {			/* anything to pull down? */
		src = ary->ary_array + offset + length;
		dst = src + diff;		/* diff is negative */
		Move(src, dst, after, STR*);
	    }
	    Zero(&ary->ary_array[ary->ary_fill+1], -diff, STR*);
						/* avoid later double free */
	}
	if (newlen) {
	    for (src = tmparyval, dst = ary->ary_array + offset;
	      newlen; newlen--) {
		*dst = Str_new(46,0);
		str_sset(*dst++,*src++);
	    }
	    Safefree(tmparyval);
	}
    }
    else {					/* no, expanding (or same) */
	if (length) {
	    New(452, tmparyval, length, STR*);	/* so remember deletion */
	    Copy(ary->ary_array+offset, tmparyval, length, STR*);
	}

	if (diff > 0) {				/* expanding */

	    /* push up or down? */

	    if (offset < after && diff <= ary->ary_array - ary->ary_alloc) {
		if (offset) {
		    src = ary->ary_array;
		    dst = src - diff;
		    Move(src, dst, offset, STR*);
		}
		ary->ary_array -= diff;		/* diff is positive */
		ary->ary_max += diff;
		ary->ary_fill += diff;
	    }
	    else {
		if (ary->ary_fill + diff >= ary->ary_max)	/* oh, well */
		    astore(ary, ary->ary_fill + diff, Nullstr);
		else
		    ary->ary_fill += diff;
		dst = ary->ary_array + ary->ary_fill;
		for (i = diff; i > 0; i--) {
		    if (*dst)			/* str was hanging around */
			str_free(*dst);		/*  after $#foo */
		    dst--;
		}
		if (after) {
		    dst = ary->ary_array + ary->ary_fill;
		    src = dst - diff;
		    for (i = after; i; i--) {
			*dst-- = *src--;
		    }
		}
	    }
	}

	for (src = st+sp, dst = ary->ary_array + offset; newlen; newlen--) {
	    *dst = Str_new(46,0);
	    str_sset(*dst++,*src++);
	}
	sp = arglast[0] + 1;
	if (gimme == G_ARRAY) {			/* copy return vals to stack */
	    if (length) {
		Copy(tmparyval, st+sp, length, STR*);
		if (ary->ary_flags & ARF_REAL) {
		    for (i = length, dst = st+sp; i; i--)
			str_2mortal(*dst++);	/* free them eventualy */
		}
		Safefree(tmparyval);
	    }
	    sp += length - 1;
	}
	else if (length--) {
	    st[sp] = tmparyval[length];
	    if (ary->ary_flags & ARF_REAL) {
		str_2mortal(st[sp]);
		while (length-- > 0)
		    str_free(tmparyval[length]);
	    }
	    Safefree(tmparyval);
	}
	else
	    st[sp] = &str_undef;
    }
    return sp;
}

int
do_grep(arg,str,gimme,arglast)
register ARG *arg;
STR *str;
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;
    register int dst = arglast[1];
    register int src = dst + 1;
    register int sp = arglast[2];
    register int i = sp - arglast[1];
    int oldsave = savestack->ary_fill;
    SPAT *oldspat = curspat;
    int oldtmps_base = tmps_base;

    savesptr(&stab_val(defstab));
    tmps_base = tmps_max;
    if ((arg[1].arg_type & A_MASK) != A_EXPR) {
	arg[1].arg_type &= A_MASK;
	dehoist(arg,1);
	arg[1].arg_type |= A_DONT;
    }
    arg = arg[1].arg_ptr.arg_arg;
    while (i-- > 0) {
	if (st[src]) {
	    st[src]->str_pok &= ~SP_TEMP;
	    stab_val(defstab) = st[src];
	}
	else
	    stab_val(defstab) = str_mortal(&str_undef);
	(void)eval(arg,G_SCALAR,sp);
	st = stack->ary_array;
	if (str_true(st[sp+1]))
	    st[dst++] = st[src];
	src++;
	curspat = oldspat;
    }
    restorelist(oldsave);
    tmps_base = oldtmps_base;
    if (gimme != G_ARRAY) {
	str_numset(str,(double)(dst - arglast[1]));
	STABSET(str);
	st[arglast[0]+1] = str;
	return arglast[0]+1;
    }
    return arglast[0] + (dst - arglast[1]);
}

int
do_reverse(arglast)
int *arglast;
{
    STR **st = stack->ary_array;
    register STR **up = &st[arglast[1]];
    register STR **down = &st[arglast[2]];
    register int i = arglast[2] - arglast[1];

    while (i-- > 0) {
	*up++ = *down;
	if (i-- > 0)
	    *down-- = *up;
    }
    i = arglast[2] - arglast[1];
    Move(down+1,up,i/2,STR*);
    return arglast[2] - 1;
}

int
do_sreverse(str,arglast)
STR *str;
int *arglast;
{
    STR **st = stack->ary_array;
    register char *up;
    register char *down;
    register int tmp;

    str_sset(str,st[arglast[2]]);
    up = str_get(str);
    if (str->str_cur > 1) {
	down = str->str_ptr + str->str_cur - 1;
	while (down > up) {
	    tmp = *up;
	    *up++ = *down;
	    *down-- = tmp;
	}
    }
    STABSET(str);
    st[arglast[0]+1] = str;
    return arglast[0]+1;
}

static CMD *sortcmd;
static HASH *sortstash = Null(HASH*);
static STAB *firststab = Nullstab;
static STAB *secondstab = Nullstab;

int
do_sort(str,arg,gimme,arglast)
STR *str;
ARG *arg;
int gimme;
int *arglast;
{
    register STR **st = stack->ary_array;
    int sp = arglast[1];
    register STR **up;
    register int max = arglast[2] - sp;
    register int i;
    int sortcmp();
    int sortsub();
    STR *oldfirst;
    STR *oldsecond;
    ARRAY *oldstack;
    HASH *stash;
    STR *sortsubvar;
    static ARRAY *sortstack = Null(ARRAY*);

    if (gimme != G_ARRAY) {
	str_sset(str,&str_undef);
	STABSET(str);
	st[sp] = str;
	return sp;
    }
    up = &st[sp];
    sortsubvar = *up;
    st += sp;		/* temporarily make st point to args */
    for (i = 1; i <= max; i++) {
	/*SUPPRESS 560*/
	if (*up = st[i]) {
	    if (!(*up)->str_pok)
		(void)str_2ptr(*up);
	    else
		(*up)->str_pok &= ~SP_TEMP;
	    up++;
	}
    }
    st -= sp;
    max = up - &st[sp];
    sp--;
    if (max > 1) {
	STAB *stab;

	if (arg[1].arg_type == (A_CMD|A_DONT)) {
	    sortcmd = arg[1].arg_ptr.arg_cmd;
	    stash = curcmd->c_stash;
	}
	else {
	    if ((arg[1].arg_type & A_MASK) == A_WORD)
		stab = arg[1].arg_ptr.arg_stab;
	    else
		stab = stabent(str_get(sortsubvar),TRUE);

	    if (stab) {
		if (!stab_sub(stab) || !(sortcmd = stab_sub(stab)->cmd))
		    fatal("Undefined subroutine \"%s\" in sort",
			stab_ename(stab));
		stash = stab_estash(stab);
	    }
	    else
		sortcmd = Nullcmd;
	}

	if (sortcmd) {
	    int oldtmps_base = tmps_base;

	    if (!sortstack) {
		sortstack = anew(Nullstab);
		astore(sortstack, 0, Nullstr);
		aclear(sortstack);
		sortstack->ary_flags = 0;
	    }
	    oldstack = stack;
	    stack = sortstack;
	    tmps_base = tmps_max;
	    if (sortstash != stash) {
		firststab = stabent("a",TRUE);
		secondstab = stabent("b",TRUE);
		sortstash = stash;
	    }
	    oldfirst = stab_val(firststab);
	    oldsecond = stab_val(secondstab);
#ifndef lint
	    qsort((char*)(st+sp+1),max,sizeof(STR*),sortsub);
#else
	    qsort(Nullch,max,sizeof(STR*),sortsub);
#endif
	    stab_val(firststab) = oldfirst;
	    stab_val(secondstab) = oldsecond;
	    tmps_base = oldtmps_base;
	    stack = oldstack;
	}
#ifndef lint
	else
	    qsort((char*)(st+sp+1),max,sizeof(STR*),sortcmp);
#endif
    }
    return sp+max;
}

static int
sortsub(str1,str2)
STR **str1;
STR **str2;
{
    stab_val(firststab) = *str1;
    stab_val(secondstab) = *str2;
    cmd_exec(sortcmd,G_SCALAR,-1);
    return (int)str_gnum(*stack->ary_array);
}

static int
sortcmp(strp1,strp2)
STR **strp1;
STR **strp2;
{
    register STR *str1 = *strp1;
    register STR *str2 = *strp2;
    int retval;

    if (str1->str_cur < str2->str_cur) {
	/*SUPPRESS 560*/
	if (retval = memcmp(str1->str_ptr, str2->str_ptr, str1->str_cur))
	    return retval;
	else
	    return -1;
    }
    /*SUPPRESS 560*/
    else if (retval = memcmp(str1->str_ptr, str2->str_ptr, str2->str_cur))
	return retval;
    else if (str1->str_cur == str2->str_cur)
	return 0;
    else
	return 1;
}

int
do_range(gimme,arglast)
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;
    register int sp = arglast[0];
    register int i;
    register ARRAY *ary = stack;
    register STR *str;
    int max;

    if (gimme != G_ARRAY)
	fatal("panic: do_range");

    if (st[sp+1]->str_nok || !st[sp+1]->str_pok ||
      (looks_like_number(st[sp+1]) && *st[sp+1]->str_ptr != '0') ) {
	i = (int)str_gnum(st[sp+1]);
	max = (int)str_gnum(st[sp+2]);
	if (max > i)
	    (void)astore(ary, sp + max - i + 1, Nullstr);
	while (i <= max) {
	    (void)astore(ary, ++sp, str = str_mortal(&str_no));
	    str_numset(str,(double)i++);
	}
    }
    else {
	STR *final = str_mortal(st[sp+2]);
	char *tmps = str_get(final);

	str = str_mortal(st[sp+1]);
	while (!str->str_nok && str->str_cur <= final->str_cur &&
	    strNE(str->str_ptr,tmps) ) {
	    (void)astore(ary, ++sp, str);
	    str = str_2mortal(str_smake(str));
	    str_inc(str);
	}
	if (strEQ(str->str_ptr,tmps))
	    (void)astore(ary, ++sp, str);
    }
    return sp;
}

int
do_repeatary(arglast)
int *arglast;
{
    STR **st = stack->ary_array;
    register int sp = arglast[0];
    register int items = arglast[1] - sp;
    register int count = (int) str_gnum(st[arglast[2]]);
    register int i;
    int max;

    max = items * count;
    if (max > 0 && sp + max > stack->ary_max) {
	astore(stack, sp + max, Nullstr);
	st = stack->ary_array;
    }
    if (count > 1) {
	for (i = arglast[1]; i > sp; i--)
	    st[i]->str_pok &= ~SP_TEMP;
	repeatcpy((char*)&st[arglast[1]+1], (char*)&st[sp+1],
	    items * sizeof(STR*), count);
    }
    sp += max;

    return sp;
}

int
do_caller(arg,maxarg,gimme,arglast)
ARG *arg;
int maxarg;
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;
    register int sp = arglast[0];
    register CSV *csv = curcsv;
    STR *str;
    int count = 0;

    if (!csv)
	fatal("There is no caller");
    if (maxarg)
	count = (int) str_gnum(st[sp+1]);
    for (;;) {
	if (!csv)
	    return sp;
	if (DBsub && csv->curcsv && csv->curcsv->sub == stab_sub(DBsub))
	    count++;
	if (!count--)
	    break;
	csv = csv->curcsv;
    }
    if (gimme != G_ARRAY) {
	STR *str = arg->arg_ptr.arg_str;
	str_set(str,csv->curcmd->c_stash->tbl_name);
	STABSET(str);
	st[++sp] = str;
	return sp;
    }

#ifndef lint
    (void)astore(stack,++sp,
      str_2mortal(str_make(csv->curcmd->c_stash->tbl_name,0)) );
    (void)astore(stack,++sp,
      str_2mortal(str_make(stab_val(csv->curcmd->c_filestab)->str_ptr,0)) );
    (void)astore(stack,++sp,
      str_2mortal(str_nmake((double)csv->curcmd->c_line)) );
    if (!maxarg)
	return sp;
    str = Str_new(49,0);
    stab_efullname(str, csv->stab);
    (void)astore(stack,++sp, str_2mortal(str));
    (void)astore(stack,++sp,
      str_2mortal(str_nmake((double)csv->hasargs)) );
    (void)astore(stack,++sp,
      str_2mortal(str_nmake((double)csv->wantarray)) );
    if (csv->hasargs) {
	ARRAY *ary = csv->argarray;

	if (!dbargs)
	    dbargs = stab_xarray(aadd(stabent("DB'args", TRUE)));
	if (dbargs->ary_max < ary->ary_fill)
	    astore(dbargs,ary->ary_fill,Nullstr);
	Copy(ary->ary_array, dbargs->ary_array, ary->ary_fill+1, STR*);
	dbargs->ary_fill = ary->ary_fill;
    }
#else
    (void)astore(stack,++sp,
      str_2mortal(str_make("",0)));
#endif
    return sp;
}

int
do_tms(str,gimme,arglast)
STR *str;
int gimme;
int *arglast;
{
#ifdef MSDOS
    return -1;
#else
    STR **st = stack->ary_array;
    register int sp = arglast[0];

    if (gimme != G_ARRAY) {
	str_sset(str,&str_undef);
	STABSET(str);
	st[++sp] = str;
	return sp;
    }
    (void)times(&timesbuf);

#ifndef HZ
#define HZ 60
#endif

#ifndef lint
    (void)astore(stack,++sp,
      str_2mortal(str_nmake(((double)timesbuf.tms_utime)/HZ)));
    (void)astore(stack,++sp,
      str_2mortal(str_nmake(((double)timesbuf.tms_stime)/HZ)));
    (void)astore(stack,++sp,
      str_2mortal(str_nmake(((double)timesbuf.tms_cutime)/HZ)));
    (void)astore(stack,++sp,
      str_2mortal(str_nmake(((double)timesbuf.tms_cstime)/HZ)));
#else
    (void)astore(stack,++sp,
      str_2mortal(str_nmake(0.0)));
#endif
    return sp;
#endif
}

int
do_time(str,tmbuf,gimme,arglast)
STR *str;
struct tm *tmbuf;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    STR **st = ary->ary_array;
    register int sp = arglast[0];

    if (!tmbuf || gimme != G_ARRAY) {
	str_sset(str,&str_undef);
	STABSET(str);
	st[++sp] = str;
	return sp;
    }
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_sec)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_min)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_hour)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_mday)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_mon)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_year)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_wday)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_yday)));
    (void)astore(ary,++sp,str_2mortal(str_nmake((double)tmbuf->tm_isdst)));
    return sp;
}

int
do_kv(str,hash,kv,gimme,arglast)
STR *str;
HASH *hash;
int kv;
int gimme;
int *arglast;
{
    register ARRAY *ary = stack;
    STR **st = ary->ary_array;
    register int sp = arglast[0];
    int i;
    register HENT *entry;
    char *tmps;
    STR *tmpstr;
    int dokeys = (kv == O_KEYS || kv == O_HASH);
    int dovalues = (kv == O_VALUES || kv == O_HASH);

    if (gimme != G_ARRAY) {
	i = 0;
	(void)hiterinit(hash);
	/*SUPPRESS 560*/
	while (entry = hiternext(hash)) {
	    i++;
	}
	str_numset(str,(double)i);
	STABSET(str);
	st[++sp] = str;
	return sp;
    }
    (void)hiterinit(hash);
    /*SUPPRESS 560*/
    while (entry = hiternext(hash)) {
	if (dokeys) {
	    tmps = hiterkey(entry,&i);
	    if (!i)
		tmps = "";
	    (void)astore(ary,++sp,str_2mortal(str_make(tmps,i)));
	}
	if (dovalues) {
	    tmpstr = Str_new(45,0);
#ifdef DEBUGGING
	    if (debug & 8192) {
		sprintf(buf,"%d%%%d=%d\n",entry->hent_hash,
		    hash->tbl_max+1,entry->hent_hash & hash->tbl_max);
		str_set(tmpstr,buf);
	    }
	    else
#endif
	    str_sset(tmpstr,hiterval(hash,entry));
	    (void)astore(ary,++sp,str_2mortal(tmpstr));
	}
    }
    return sp;
}

int
do_each(str,hash,gimme,arglast)
STR *str;
HASH *hash;
int gimme;
int *arglast;
{
    STR **st = stack->ary_array;
    register int sp = arglast[0];
    static STR *mystrk = Nullstr;
    HENT *entry = hiternext(hash);
    int i;
    char *tmps;

    if (mystrk) {
	str_free(mystrk);
	mystrk = Nullstr;
    }

    if (entry) {
	if (gimme == G_ARRAY) {
	    tmps = hiterkey(entry, &i);
	    if (!i)
		tmps = "";
	    st[++sp] = mystrk = str_make(tmps,i);
	}
	st[++sp] = str;
	str_sset(str,hiterval(hash,entry));
	STABSET(str);
	return sp;
    }
    else
	return sp;
}
