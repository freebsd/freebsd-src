/* $RCSfile: doarg.c,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:35 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: doarg.c,v $
 * Revision 1.1.1.1  1993/08/23  21:29:35  nate
 * PERL!
 *
 * Revision 4.0.1.8  1993/02/05  19:32:27  lwall
 * patch36: substitution didn't always invalidate numericity
 *
 * Revision 4.0.1.7  92/06/11  21:07:11  lwall
 * patch34: join with null list attempted negative allocation
 * patch34: sprintf("%6.4s", "abcdefg") didn't print "abcd  "
 * 
 * Revision 4.0.1.6  92/06/08  12:34:30  lwall
 * patch20: removed implicit int declarations on funcions
 * patch20: pattern modifiers i and o didn't interact right
 * patch20: join() now pre-extends target string to avoid excessive copying
 * patch20: fixed confusion between a *var's real name and its effective name
 * patch20: subroutines didn't localize $`, $&, $', $1 et al correctly
 * patch20: usersub routines didn't reclaim temp values soon enough
 * patch20: ($<,$>) = ... didn't work on some architectures
 * patch20: added Atari ST portability
 * 
 * Revision 4.0.1.5  91/11/11  16:31:58  lwall
 * patch19: added little-endian pack/unpack options
 * 
 * Revision 4.0.1.4  91/11/05  16:35:06  lwall
 * patch11: /$foo/o optimizer could access deallocated data
 * patch11: minimum match length calculation in regexp is now cumulative
 * patch11: added some support for 64-bit integers
 * patch11: prepared for ctype implementations that don't define isascii()
 * patch11: sprintf() now supports any length of s field
 * patch11: indirect subroutine calls through magic vars (e.g. &$1) didn't work
 * patch11: defined(&$foo) and undef(&$foo) didn't work
 * 
 * Revision 4.0.1.3  91/06/10  01:18:41  lwall
 * patch10: pack(hh,1) dumped core
 * 
 * Revision 4.0.1.2  91/06/07  10:42:17  lwall
 * patch4: new copyright notice
 * patch4: // wouldn't use previous pattern if it started with a null character
 * patch4: //o and s///o now optimize themselves fully at runtime
 * patch4: added global modifier for pattern matches
 * patch4: undef @array disabled "@array" interpolation
 * patch4: chop("") was returning "\0" rather than ""
 * patch4: vector logical operations &, | and ^ sometimes returned null string
 * patch4: syscall couldn't pass numbers with most significant bit set on sparcs
 * 
 * Revision 4.0.1.1  91/04/11  17:40:14  lwall
 * patch1: fixed undefined environ problem
 * patch1: fixed debugger coredump on subroutines
 * 
 * Revision 4.0  91/03/20  01:06:42  lwall
 * 4.0 baseline.
 * 
 */

#include "EXTERN.h"
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

extern unsigned char fold[];

#ifdef BUGGY_MSC
 #pragma function(memcmp)
#endif /* BUGGY_MSC */

static void doencodes();

int
do_subst(str,arg,sp)
STR *str;
ARG *arg;
int sp;
{
    register SPAT *spat;
    SPAT *rspat;
    register STR *dstr;
    register char *s = str_get(str);
    char *strend = s + str->str_cur;
    register char *m;
    char *c;
    register char *d;
    int clen;
    int iters = 0;
    int maxiters = (strend - s) + 10;
    register int i;
    bool once;
    char *orig;
    int safebase;

    rspat = spat = arg[2].arg_ptr.arg_spat;
    if (!spat || !s)
	fatal("panic: do_subst");
    else if (spat->spat_runtime) {
	nointrp = "|)";
	(void)eval(spat->spat_runtime,G_SCALAR,sp);
	m = str_get(dstr = stack->ary_array[sp+1]);
	nointrp = "";
	if (spat->spat_regexp) {
	    regfree(spat->spat_regexp);
	    spat->spat_regexp = Null(REGEXP*);	/* required if regcomp pukes */
	}
	spat->spat_regexp = regcomp(m,m+dstr->str_cur,
	    spat->spat_flags & SPAT_FOLD);
	if (spat->spat_flags & SPAT_KEEP) {
	    if (!(spat->spat_flags & SPAT_FOLD))
		scanconst(spat, m, dstr->str_cur);
	    arg_free(spat->spat_runtime);	/* it won't change, so */
	    spat->spat_runtime = Nullarg;	/* no point compiling again */
	    hoistmust(spat);
            if (curcmd->c_expr && (curcmd->c_flags & CF_OPTIMIZE) == CFT_EVAL) {
                curcmd->c_flags &= ~CF_OPTIMIZE;
                opt_arg(curcmd, 1, curcmd->c_type == C_EXPR);
            }
	}
    }
#ifdef DEBUGGING
    if (debug & 8) {
	deb("2.SPAT /%s/\n",spat->spat_regexp->precomp);
    }
#endif
    safebase = ((!spat->spat_regexp || !spat->spat_regexp->nparens) &&
      !sawampersand);
    if (!spat->spat_regexp->prelen && lastspat)
	spat = lastspat;
    orig = m = s;
    if (hint) {
	if (hint < s || hint > strend)
	    fatal("panic: hint in do_match");
	s = hint;
	hint = Nullch;
	if (spat->spat_regexp->regback >= 0) {
	    s -= spat->spat_regexp->regback;
	    if (s < m)
		s = m;
	}
	else
	    s = m;
    }
    else if (spat->spat_short) {
	if (spat->spat_flags & SPAT_SCANFIRST) {
	    if (str->str_pok & SP_STUDIED) {
		if (screamfirst[spat->spat_short->str_rare] < 0)
		    goto nope;
		else if (!(s = screaminstr(str,spat->spat_short)))
		    goto nope;
	    }
#ifndef lint
	    else if (!(s = fbminstr((unsigned char*)s, (unsigned char*)strend,
	      spat->spat_short)))
		goto nope;
#endif
	    if (s && spat->spat_regexp->regback >= 0) {
		++spat->spat_short->str_u.str_useful;
		s -= spat->spat_regexp->regback;
		if (s < m)
		    s = m;
	    }
	    else
		s = m;
	}
	else if (!multiline && (*spat->spat_short->str_ptr != *s ||
	  bcmp(spat->spat_short->str_ptr, s, spat->spat_slen) ))
	    goto nope;
	if (--spat->spat_short->str_u.str_useful < 0) {
	    str_free(spat->spat_short);
	    spat->spat_short = Nullstr;	/* opt is being useless */
	}
    }
    once = !(rspat->spat_flags & SPAT_GLOBAL);
    if (rspat->spat_flags & SPAT_CONST) {	/* known replacement string? */
	if ((rspat->spat_repl[1].arg_type & A_MASK) == A_SINGLE)
	    dstr = rspat->spat_repl[1].arg_ptr.arg_str;
	else {					/* constant over loop, anyway */
	    (void)eval(rspat->spat_repl,G_SCALAR,sp);
	    dstr = stack->ary_array[sp+1];
	}
	c = str_get(dstr);
	clen = dstr->str_cur;
	if (clen <= spat->spat_regexp->minlen) {
					/* can do inplace substitution */
	    if (regexec(spat->spat_regexp, s, strend, orig, 0,
	      str->str_pok & SP_STUDIED ? str : Nullstr, safebase)) {
		if (spat->spat_regexp->subbase) /* oops, no we can't */
		    goto long_way;
		d = s;
		lastspat = spat;
		str->str_pok = SP_VALID;	/* disable possible screamer */
		if (once) {
		    m = spat->spat_regexp->startp[0];
		    d = spat->spat_regexp->endp[0];
		    s = orig;
		    if (m - s > strend - d) {	/* faster to shorten from end */
			if (clen) {
			    Copy(c, m, clen, char);
			    m += clen;
			}
			i = strend - d;
			if (i > 0) {
			    Move(d, m, i, char);
			    m += i;
			}
			*m = '\0';
			str->str_cur = m - s;
			STABSET(str);
			str_numset(arg->arg_ptr.arg_str, 1.0);
			stack->ary_array[++sp] = arg->arg_ptr.arg_str;
			str->str_nok = 0;
			return sp;
		    }
		    /*SUPPRESS 560*/
		    else if (i = m - s) {	/* faster from front */
			d -= clen;
			m = d;
			str_chop(str,d-i);
			s += i;
			while (i--)
			    *--d = *--s;
			if (clen)
			    Copy(c, m, clen, char);
			STABSET(str);
			str_numset(arg->arg_ptr.arg_str, 1.0);
			stack->ary_array[++sp] = arg->arg_ptr.arg_str;
			str->str_nok = 0;
			return sp;
		    }
		    else if (clen) {
			d -= clen;
			str_chop(str,d);
			Copy(c,d,clen,char);
			STABSET(str);
			str_numset(arg->arg_ptr.arg_str, 1.0);
			stack->ary_array[++sp] = arg->arg_ptr.arg_str;
			str->str_nok = 0;
			return sp;
		    }
		    else {
			str_chop(str,d);
			STABSET(str);
			str_numset(arg->arg_ptr.arg_str, 1.0);
			stack->ary_array[++sp] = arg->arg_ptr.arg_str;
			str->str_nok = 0;
			return sp;
		    }
		    /* NOTREACHED */
		}
		do {
		    if (iters++ > maxiters)
			fatal("Substitution loop");
		    m = spat->spat_regexp->startp[0];
		    /*SUPPRESS 560*/
		    if (i = m - s) {
			if (s != d)
			    Move(s,d,i,char);
			d += i;
		    }
		    if (clen) {
			Copy(c,d,clen,char);
			d += clen;
		    }
		    s = spat->spat_regexp->endp[0];
		} while (regexec(spat->spat_regexp, s, strend, orig, s == m,
		    Nullstr, TRUE));	/* (don't match same null twice) */
		if (s != d) {
		    i = strend - s;
		    str->str_cur = d - str->str_ptr + i;
		    Move(s,d,i+1,char);		/* include the Null */
		}
		STABSET(str);
		str_numset(arg->arg_ptr.arg_str, (double)iters);
		stack->ary_array[++sp] = arg->arg_ptr.arg_str;
		str->str_nok = 0;
		return sp;
	    }
	    str_numset(arg->arg_ptr.arg_str, 0.0);
	    stack->ary_array[++sp] = arg->arg_ptr.arg_str;
	    return sp;
	}
    }
    else
	c = Nullch;
    if (regexec(spat->spat_regexp, s, strend, orig, 0,
      str->str_pok & SP_STUDIED ? str : Nullstr, safebase)) {
    long_way:
	dstr = Str_new(25,str_len(str));
	str_nset(dstr,m,s-m);
	if (spat->spat_regexp->subbase)
	    curspat = spat;
	lastspat = spat;
	do {
	    if (iters++ > maxiters)
		fatal("Substitution loop");
	    if (spat->spat_regexp->subbase
	      && spat->spat_regexp->subbase != orig) {
		m = s;
		s = orig;
		orig = spat->spat_regexp->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = spat->spat_regexp->startp[0];
	    str_ncat(dstr,s,m-s);
	    s = spat->spat_regexp->endp[0];
	    if (c) {
		if (clen)
		    str_ncat(dstr,c,clen);
	    }
	    else {
		char *mysubbase = spat->spat_regexp->subbase;

		spat->spat_regexp->subbase = Nullch;	/* so recursion works */
		(void)eval(rspat->spat_repl,G_SCALAR,sp);
		str_scat(dstr,stack->ary_array[sp+1]);
		if (spat->spat_regexp->subbase)
		    Safefree(spat->spat_regexp->subbase);
		spat->spat_regexp->subbase = mysubbase;
	    }
	    if (once)
		break;
	} while (regexec(spat->spat_regexp, s, strend, orig, s == m, Nullstr,
	    safebase));
	str_ncat(dstr,s,strend - s);
	str_replace(str,dstr);
	STABSET(str);
	str_numset(arg->arg_ptr.arg_str, (double)iters);
	stack->ary_array[++sp] = arg->arg_ptr.arg_str;
	str->str_nok = 0;
	return sp;
    }
    str_numset(arg->arg_ptr.arg_str, 0.0);
    stack->ary_array[++sp] = arg->arg_ptr.arg_str;
    return sp;

nope:
    ++spat->spat_short->str_u.str_useful;
    str_numset(arg->arg_ptr.arg_str, 0.0);
    stack->ary_array[++sp] = arg->arg_ptr.arg_str;
    return sp;
}
#ifdef BUGGY_MSC
 #pragma intrinsic(memcmp)
#endif /* BUGGY_MSC */

int
do_trans(str,arg)
STR *str;
ARG *arg;
{
    register short *tbl;
    register char *s;
    register int matches = 0;
    register int ch;
    register char *send;
    register char *d;
    register int squash = arg[2].arg_len & 1;

    tbl = (short*) arg[2].arg_ptr.arg_cval;
    s = str_get(str);
    send = s + str->str_cur;
    if (!tbl || !s)
	fatal("panic: do_trans");
#ifdef DEBUGGING
    if (debug & 8) {
	deb("2.TBL\n");
    }
#endif
    if (!arg[2].arg_len) {
	while (s < send) {
	    if ((ch = tbl[*s & 0377]) >= 0) {
		matches++;
		*s = ch;
	    }
	    s++;
	}
    }
    else {
	d = s;
	while (s < send) {
	    if ((ch = tbl[*s & 0377]) >= 0) {
		*d = ch;
		if (matches++ && squash) {
		    if (d[-1] == *d)
			matches--;
		    else
			d++;
		}
		else
		    d++;
	    }
	    else if (ch == -1)		/* -1 is unmapped character */
		*d++ = *s;		/* -2 is delete character */
	    s++;
	}
	matches += send - d;	/* account for disappeared chars */
	*d = '\0';
	str->str_cur = d - str->str_ptr;
    }
    STABSET(str);
    return matches;
}

void
do_join(str,arglast)
register STR *str;
int *arglast;
{
    register STR **st = stack->ary_array;
    int sp = arglast[1];
    register int items = arglast[2] - sp;
    register char *delim = str_get(st[sp]);
    register STRLEN len;
    int delimlen = st[sp]->str_cur;

    st += sp + 1;

    len = (items > 0 ? (delimlen * (items - 1) ) : 0);
    if (str->str_len < len + items) {	/* current length is way too short */
	while (items-- > 0) {
	    if (*st)
		len += (*st)->str_cur;
	    st++;
	}
	STR_GROW(str, len + 1);		/* so try to pre-extend */

	items = arglast[2] - sp;
	st -= items;
    }

    if (items-- > 0)
	str_sset(str, *st++);
    else
	str_set(str,"");
    len = delimlen;
    if (len) {
	for (; items > 0; items--,st++) {
	    str_ncat(str,delim,len);
	    str_scat(str,*st);
	}
    }
    else {
	for (; items > 0; items--,st++)
	    str_scat(str,*st);
    }
    STABSET(str);
}

void
do_pack(str,arglast)
register STR *str;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items;
    register char *pat = str_get(st[sp]);
    register char *patend = pat + st[sp]->str_cur;
    register int len;
    int datumtype;
    STR *fromstr;
    /*SUPPRESS 442*/
    static char *null10 = "\0\0\0\0\0\0\0\0\0\0";
    static char *space10 = "          ";

    /* These must not be in registers: */
    char achar;
    short ashort;
    int aint;
    unsigned int auint;
    long along;
    unsigned long aulong;
#ifdef QUAD
    quad aquad;
    unsigned quad auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;

    items = arglast[2] - sp;
    st += ++sp;
    str_nset(str,"",0);
    while (pat < patend) {
#define NEXTFROM (items-- > 0 ? *st++ : &str_no)
	datumtype = *pat++;
	if (*pat == '*') {
	    len = index("@Xxu",datumtype) ? 0 : items;
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat))
		len = (len * 10) + (*pat++ - '0');
	}
	else
	    len = 1;
	switch(datumtype) {
	default:
	    break;
	case '%':
	    fatal("% may only be used in unpack");
	case '@':
	    len -= str->str_cur;
	    if (len > 0)
		goto grow;
	    len = -len;
	    if (len > 0)
		goto shrink;
	    break;
	case 'X':
	  shrink:
	    if (str->str_cur < len)
		fatal("X outside of string");
	    str->str_cur -= len;
	    str->str_ptr[str->str_cur] = '\0';
	    break;
	case 'x':
	  grow:
	    while (len >= 10) {
		str_ncat(str,null10,10);
		len -= 10;
	    }
	    str_ncat(str,null10,len);
	    break;
	case 'A':
	case 'a':
	    fromstr = NEXTFROM;
	    aptr = str_get(fromstr);
	    if (pat[-1] == '*')
		len = fromstr->str_cur;
	    if (fromstr->str_cur > len)
		str_ncat(str,aptr,len);
	    else {
		str_ncat(str,aptr,fromstr->str_cur);
		len -= fromstr->str_cur;
		if (datumtype == 'A') {
		    while (len >= 10) {
			str_ncat(str,space10,10);
			len -= 10;
		    }
		    str_ncat(str,space10,len);
		}
		else {
		    while (len >= 10) {
			str_ncat(str,null10,10);
			len -= 10;
		    }
		    str_ncat(str,null10,len);
		}
	    }
	    break;
	case 'B':
	case 'b':
	    {
		char *savepat = pat;
		int saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		aptr = str_get(fromstr);
		if (pat[-1] == '*')
		    len = fromstr->str_cur;
		pat = aptr;
		aint = str->str_cur;
		str->str_cur += (len+7)/8;
		STR_GROW(str, str->str_cur + 1);
		aptr = str->str_ptr + aint;
		if (len > fromstr->str_cur)
		    len = fromstr->str_cur;
		aint = len;
		items = 0;
		if (datumtype == 'B') {
		    for (len = 0; len++ < aint;) {
			items |= *pat++ & 1;
			if (len & 7)
			    items <<= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (*pat++ & 1)
			    items |= 128;
			if (len & 7)
			    items >>= 1;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 7) {
		    if (datumtype == 'B')
			items <<= 7 - (aint & 7);
		    else
			items >>= 7 - (aint & 7);
		    *aptr++ = items & 0xff;
		}
		pat = str->str_ptr + str->str_cur;
		while (aptr <= pat)
		    *aptr++ = '\0';

		pat = savepat;
		items = saveitems;
	    }
	    break;
	case 'H':
	case 'h':
	    {
		char *savepat = pat;
		int saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		aptr = str_get(fromstr);
		if (pat[-1] == '*')
		    len = fromstr->str_cur;
		pat = aptr;
		aint = str->str_cur;
		str->str_cur += (len+1)/2;
		STR_GROW(str, str->str_cur + 1);
		aptr = str->str_ptr + aint;
		if (len > fromstr->str_cur)
		    len = fromstr->str_cur;
		aint = len;
		items = 0;
		if (datumtype == 'H') {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*pat))
			    items |= ((*pat++ & 15) + 9) & 15;
			else
			    items |= *pat++ & 15;
			if (len & 1)
			    items <<= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		else {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*pat))
			    items |= (((*pat++ & 15) + 9) & 15) << 4;
			else
			    items |= (*pat++ & 15) << 4;
			if (len & 1)
			    items >>= 4;
			else {
			    *aptr++ = items & 0xff;
			    items = 0;
			}
		    }
		}
		if (aint & 1)
		    *aptr++ = items & 0xff;
		pat = str->str_ptr + str->str_cur;
		while (aptr <= pat)
		    *aptr++ = '\0';

		pat = savepat;
		items = saveitems;
	    }
	    break;
	case 'C':
	case 'c':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = (int)str_gnum(fromstr);
		achar = aint;
		str_ncat(str,&achar,sizeof(char));
	    }
	    break;
	/* Float and double added by gnb@melba.bby.oz.au  22/11/89 */
	case 'f':
	case 'F':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		afloat = (float)str_gnum(fromstr);
		str_ncat(str, (char *)&afloat, sizeof (float));
	    }
	    break;
	case 'd':
	case 'D':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		adouble = (double)str_gnum(fromstr);
		str_ncat(str, (char *)&adouble, sizeof (double));
	    }
	    break;
	case 'n':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (short)str_gnum(fromstr);
#ifdef HAS_HTONS
		ashort = htons(ashort);
#endif
		str_ncat(str,(char*)&ashort,sizeof(short));
	    }
	    break;
	case 'v':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (short)str_gnum(fromstr);
#ifdef HAS_HTOVS
		ashort = htovs(ashort);
#endif
		str_ncat(str,(char*)&ashort,sizeof(short));
	    }
	    break;
	case 'S':
	case 's':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (short)str_gnum(fromstr);
		str_ncat(str,(char*)&ashort,sizeof(short));
	    }
	    break;
	case 'I':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = U_I(str_gnum(fromstr));
		str_ncat(str,(char*)&auint,sizeof(unsigned int));
	    }
	    break;
	case 'i':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = (int)str_gnum(fromstr);
		str_ncat(str,(char*)&aint,sizeof(int));
	    }
	    break;
	case 'N':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = U_L(str_gnum(fromstr));
#ifdef HAS_HTONL
		aulong = htonl(aulong);
#endif
		str_ncat(str,(char*)&aulong,sizeof(unsigned long));
	    }
	    break;
	case 'V':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = U_L(str_gnum(fromstr));
#ifdef HAS_HTOVL
		aulong = htovl(aulong);
#endif
		str_ncat(str,(char*)&aulong,sizeof(unsigned long));
	    }
	    break;
	case 'L':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = U_L(str_gnum(fromstr));
		str_ncat(str,(char*)&aulong,sizeof(unsigned long));
	    }
	    break;
	case 'l':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		along = (long)str_gnum(fromstr);
		str_ncat(str,(char*)&along,sizeof(long));
	    }
	    break;
#ifdef QUAD
	case 'Q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auquad = (unsigned quad)str_gnum(fromstr);
		str_ncat(str,(char*)&auquad,sizeof(unsigned quad));
	    }
	    break;
	case 'q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aquad = (quad)str_gnum(fromstr);
		str_ncat(str,(char*)&aquad,sizeof(quad));
	    }
	    break;
#endif /* QUAD */
	case 'p':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aptr = str_get(fromstr);
		str_ncat(str,(char*)&aptr,sizeof(char*));
	    }
	    break;
	case 'u':
	    fromstr = NEXTFROM;
	    aptr = str_get(fromstr);
	    aint = fromstr->str_cur;
	    STR_GROW(str,aint * 4 / 3);
	    if (len <= 1)
		len = 45;
	    else
		len = len / 3 * 3;
	    while (aint > 0) {
		int todo;

		if (aint > len)
		    todo = len;
		else
		    todo = aint;
		doencodes(str, aptr, todo);
		aint -= todo;
		aptr += todo;
	    }
	    break;
	}
    }
    STABSET(str);
}
#undef NEXTFROM

static void
doencodes(str, s, len)
register STR *str;
register char *s;
register int len;
{
    char hunk[5];

    *hunk = len + ' ';
    str_ncat(str, hunk, 1);
    hunk[4] = '\0';
    while (len > 0) {
	hunk[0] = ' ' + (077 & (*s >> 2));
	hunk[1] = ' ' + (077 & ((*s << 4) & 060 | (s[1] >> 4) & 017));
	hunk[2] = ' ' + (077 & ((s[1] << 2) & 074 | (s[2] >> 6) & 03));
	hunk[3] = ' ' + (077 & (s[2] & 077));
	str_ncat(str, hunk, 4);
	s += 3;
	len -= 3;
    }
    for (s = str->str_ptr; *s; s++) {
	if (*s == ' ')
	    *s = '`';
    }
    str_ncat(str, "\n", 1);
}

void
do_sprintf(str,len,sarg)
register STR *str;
register int len;
register STR **sarg;
{
    register char *s;
    register char *t;
    register char *f;
    bool dolong;
#ifdef QUAD
    bool doquad;
#endif /* QUAD */
    char ch;
    static STR *sargnull = &str_no;
    register char *send;
    register STR *arg;
    char *xs;
    int xlen;
    int pre;
    int post;
    double value;

    str_set(str,"");
    len--;			/* don't count pattern string */
    t = s = str_get(*sarg);
    send = s + (*sarg)->str_cur;
    sarg++;
    for ( ; ; len--) {

	/*SUPPRESS 560*/
	if (len <= 0 || !(arg = *sarg++))
	    arg = sargnull;

	/*SUPPRESS 530*/
	for ( ; t < send && *t != '%'; t++) ;
	if (t >= send)
	    break;		/* end of format string, ignore extra args */
	f = t;
	*buf = '\0';
	xs = buf;
#ifdef QUAD
	doquad =
#endif /* QUAD */
	dolong = FALSE;
	pre = post = 0;
	for (t++; t < send; t++) {
	    switch (*t) {
	    default:
		ch = *(++t);
		*t = '\0';
		(void)sprintf(xs,f);
		len++, sarg--;
		xlen = strlen(xs);
		break;
	    case '0': case '1': case '2': case '3': case '4':
	    case '5': case '6': case '7': case '8': case '9': 
	    case '.': case '#': case '-': case '+': case ' ':
		continue;
	    case 'l':
#ifdef QUAD
		if (dolong) {
		    dolong = FALSE;
		    doquad = TRUE;
		} else
#endif
		dolong = TRUE;
		continue;
	    case 'c':
		ch = *(++t);
		*t = '\0';
		xlen = (int)str_gnum(arg);
		if (strEQ(f,"%c")) { /* some printfs fail on null chars */
		    *xs = xlen;
		    xs[1] = '\0';
		    xlen = 1;
		}
		else {
		    (void)sprintf(xs,f,xlen);
		    xlen = strlen(xs);
		}
		break;
	    case 'D':
		dolong = TRUE;
		/* FALL THROUGH */
	    case 'd':
		ch = *(++t);
		*t = '\0';
#ifdef QUAD
		if (doquad)
		    (void)sprintf(buf,s,(quad)str_gnum(arg));
		else
#endif
		if (dolong)
		    (void)sprintf(xs,f,(long)str_gnum(arg));
		else
		    (void)sprintf(xs,f,(int)str_gnum(arg));
		xlen = strlen(xs);
		break;
	    case 'X': case 'O':
		dolong = TRUE;
		/* FALL THROUGH */
	    case 'x': case 'o': case 'u':
		ch = *(++t);
		*t = '\0';
		value = str_gnum(arg);
#ifdef QUAD
		if (doquad)
		    (void)sprintf(buf,s,(unsigned quad)value);
		else
#endif
		if (dolong)
		    (void)sprintf(xs,f,U_L(value));
		else
		    (void)sprintf(xs,f,U_I(value));
		xlen = strlen(xs);
		break;
	    case 'E': case 'e': case 'f': case 'G': case 'g':
		ch = *(++t);
		*t = '\0';
		(void)sprintf(xs,f,str_gnum(arg));
		xlen = strlen(xs);
		break;
	    case 's':
		ch = *(++t);
		*t = '\0';
		xs = str_get(arg);
		xlen = arg->str_cur;
		if (*xs == 'S' && xs[1] == 't' && xs[2] == 'B' && xs[3] == '\0'
		  && xlen == sizeof(STBP)) {
		    STR *tmpstr = Str_new(24,0);

		    stab_efullname(tmpstr, ((STAB*)arg)); /* a stab value! */
		    sprintf(tokenbuf,"*%s",tmpstr->str_ptr);
					/* reformat to non-binary */
		    xs = tokenbuf;
		    xlen = strlen(tokenbuf);
		    str_free(tmpstr);
		}
		if (strEQ(f,"%s")) {	/* some printfs fail on >128 chars */
		    break;		/* so handle simple cases */
		}
		else if (f[1] == '-') {
		    char *mp = index(f, '.');
		    int min = atoi(f+2);

		    if (mp) {
			int max = atoi(mp+1);

			if (xlen > max)
			    xlen = max;
		    }
		    if (xlen < min)
			post = min - xlen;
		    break;
		}
		else if (isDIGIT(f[1])) {
		    char *mp = index(f, '.');
		    int min = atoi(f+1);

		    if (mp) {
			int max = atoi(mp+1);

			if (xlen > max)
			    xlen = max;
		    }
		    if (xlen < min)
			pre = min - xlen;
		    break;
		}
		strcpy(tokenbuf+64,f);	/* sprintf($s,...$s...) */
		*t = ch;
		(void)sprintf(buf,tokenbuf+64,xs);
		xs = buf;
		xlen = strlen(xs);
		break;
	    }
	    /* end of switch, copy results */
	    *t = ch;
	    STR_GROW(str, str->str_cur + (f - s) + xlen + 1 + pre + post);
	    str_ncat(str, s, f - s);
	    if (pre) {
		repeatcpy(str->str_ptr + str->str_cur, " ", 1, pre);
		str->str_cur += pre;
	    }
	    str_ncat(str, xs, xlen);
	    if (post) {
		repeatcpy(str->str_ptr + str->str_cur, " ", 1, post);
		str->str_cur += post;
	    }
	    s = t;
	    break;		/* break from for loop */
	}
    }
    str_ncat(str, s, t - s);
    STABSET(str);
}

STR *
do_push(ary,arglast)
register ARRAY *ary;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items = arglast[2] - sp;
    register STR *str = &str_undef;

    for (st += ++sp; items > 0; items--,st++) {
	str = Str_new(26,0);
	if (*st)
	    str_sset(str,*st);
	(void)apush(ary,str);
    }
    return str;
}

void
do_unshift(ary,arglast)
register ARRAY *ary;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items = arglast[2] - sp;
    register STR *str;
    register int i;

    aunshift(ary,items);
    i = 0;
    for (st += ++sp; i < items; i++,st++) {
	str = Str_new(27,0);
	str_sset(str,*st);
	(void)astore(ary,i,str);
    }
}

int
do_subr(arg,gimme,arglast)
register ARG *arg;
int gimme;
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items = arglast[2] - sp;
    register SUBR *sub;
    SPAT * VOLATILE oldspat = curspat;
    STR *str;
    STAB *stab;
    int oldsave = savestack->ary_fill;
    int oldtmps_base = tmps_base;
    int hasargs = ((arg[2].arg_type & A_MASK) != A_NULL);
    register CSV *csv;

    if ((arg[1].arg_type & A_MASK) == A_WORD)
	stab = arg[1].arg_ptr.arg_stab;
    else {
	STR *tmpstr = STAB_STR(arg[1].arg_ptr.arg_stab);

	if (tmpstr)
	    stab = stabent(str_get(tmpstr),TRUE);
	else
	    stab = Nullstab;
    }
    if (!stab)
	fatal("Undefined subroutine called");
    if (!(sub = stab_sub(stab))) {
	STR *tmpstr = arg[0].arg_ptr.arg_str;

	stab_efullname(tmpstr, stab);
	fatal("Undefined subroutine \"%s\" called",tmpstr->str_ptr);
    }
    if (arg->arg_type == O_DBSUBR && !sub->usersub) {
	str = stab_val(DBsub);
	saveitem(str);
	stab_efullname(str,stab);
	sub = stab_sub(DBsub);
	if (!sub)
	    fatal("No DBsub routine");
    }
    str = Str_new(15, sizeof(CSV));
    str->str_state = SS_SCSV;
    (void)apush(savestack,str);
    csv = (CSV*)str->str_ptr;
    csv->sub = sub;
    csv->stab = stab;
    csv->curcsv = curcsv;
    csv->curcmd = curcmd;
    csv->depth = sub->depth;
    csv->wantarray = gimme;
    csv->hasargs = hasargs;
    curcsv = csv;
    tmps_base = tmps_max;
    if (sub->usersub) {
	csv->hasargs = 0;
	csv->savearray = Null(ARRAY*);;
	csv->argarray = Null(ARRAY*);
	st[sp] = arg->arg_ptr.arg_str;
	if (!hasargs)
	    items = 0;
	sp = (*sub->usersub)(sub->userindex,sp,items);
    }
    else {
	if (hasargs) {
	    csv->savearray = stab_xarray(defstab);
	    csv->argarray = afake(defstab, items, &st[sp+1]);
	    stab_xarray(defstab) = csv->argarray;
	}
	sub->depth++;
	if (sub->depth >= 2) {	/* save temporaries on recursion? */
	    if (sub->depth == 100 && dowarn)
		warn("Deep recursion on subroutine \"%s\"",stab_ename(stab));
	    savelist(sub->tosave->ary_array,sub->tosave->ary_fill);
	}
	sp = cmd_exec(sub->cmd,gimme, --sp);	/* so do it already */
    }

    st = stack->ary_array;
    tmps_base = oldtmps_base;
    for (items = arglast[0] + 1; items <= sp; items++)
	st[items] = str_mortal(st[items]);
	    /* in case restore wipes old str */
    restorelist(oldsave);
    curspat = oldspat;
    return sp;
}

int
do_assign(arg,gimme,arglast)
register ARG *arg;
int gimme;
int *arglast;
{

    register STR **st = stack->ary_array;
    STR **firstrelem = st + arglast[1] + 1;
    STR **firstlelem = st + arglast[0] + 1;
    STR **lastrelem = st + arglast[2];
    STR **lastlelem = st + arglast[1];
    register STR **relem;
    register STR **lelem;

    register STR *str;
    register ARRAY *ary;
    register int makelocal;
    HASH *hash;
    int i;

    makelocal = (arg->arg_flags & AF_LOCAL) != 0;
    localizing = makelocal;
    delaymagic = DM_DELAY;		/* catch simultaneous items */

    /* If there's a common identifier on both sides we have to take
     * special care that assigning the identifier on the left doesn't
     * clobber a value on the right that's used later in the list.
     */
    if (arg->arg_flags & AF_COMMON) {
	for (relem = firstrelem; relem <= lastrelem; relem++) {
	    /*SUPPRESS 560*/
	    if (str = *relem)
		*relem = str_mortal(str);
	}
    }
    relem = firstrelem;
    lelem = firstlelem;
    ary = Null(ARRAY*);
    hash = Null(HASH*);
    while (lelem <= lastlelem) {
	str = *lelem++;
	if (str->str_state >= SS_HASH) {
	    if (str->str_state == SS_ARY) {
		if (makelocal)
		    ary = saveary(str->str_u.str_stab);
		else {
		    ary = stab_array(str->str_u.str_stab);
		    ary->ary_fill = -1;
		}
		i = 0;
		while (relem <= lastrelem) {	/* gobble up all the rest */
		    str = Str_new(28,0);
		    if (*relem)
			str_sset(str,*relem);
		    *(relem++) = str;
		    (void)astore(ary,i++,str);
		}
	    }
	    else if (str->str_state == SS_HASH) {
		char *tmps;
		STR *tmpstr;
		int magic = 0;
		STAB *tmpstab = str->str_u.str_stab;

		if (makelocal)
		    hash = savehash(str->str_u.str_stab);
		else {
		    hash = stab_hash(str->str_u.str_stab);
		    if (tmpstab == envstab) {
			magic = 'E';
			environ[0] = Nullch;
		    }
		    else if (tmpstab == sigstab) {
			magic = 'S';
#ifndef NSIG
#define NSIG 32
#endif
			for (i = 1; i < NSIG; i++)
			    signal(i, SIG_DFL);	/* crunch, crunch, crunch */
		    }
#ifdef SOME_DBM
		    else if (hash->tbl_dbm)
			magic = 'D';
#endif
		    hclear(hash, magic == 'D');	/* wipe any dbm file too */

		}
		while (relem < lastrelem) {	/* gobble up all the rest */
		    if (*relem)
			str = *(relem++);
		    else
			str = &str_no, relem++;
		    tmps = str_get(str);
		    tmpstr = Str_new(29,0);
		    if (*relem)
			str_sset(tmpstr,*relem);	/* value */
		    *(relem++) = tmpstr;
		    (void)hstore(hash,tmps,str->str_cur,tmpstr,0);
		    if (magic) {
			str_magic(tmpstr, tmpstab, magic, tmps, str->str_cur);
			stabset(tmpstr->str_magic, tmpstr);
		    }
		}
	    }
	    else
		fatal("panic: do_assign");
	}
	else {
	    if (makelocal)
		saveitem(str);
	    if (relem <= lastrelem) {
		str_sset(str, *relem);
		*(relem++) = str;
	    }
	    else {
		str_sset(str, &str_undef);
		if (gimme == G_ARRAY) {
		    i = ++lastrelem - firstrelem;
		    relem++;		/* tacky, I suppose */
		    astore(stack,i,str);
		    if (st != stack->ary_array) {
			st = stack->ary_array;
			firstrelem = st + arglast[1] + 1;
			firstlelem = st + arglast[0] + 1;
			lastlelem = st + arglast[1];
			lastrelem = st + i;
			relem = lastrelem + 1;
		    }
		}
	    }
	    STABSET(str);
	}
    }
    if (delaymagic & ~DM_DELAY) {
	if (delaymagic & DM_UID) {
#ifdef HAS_SETREUID
	    (void)setreuid(uid,euid);
#else /* not HAS_SETREUID */
#ifdef HAS_SETRUID
	    if ((delaymagic & DM_UID) == DM_RUID) {
		(void)setruid(uid);
		delaymagic =~ DM_RUID;
	    }
#endif /* HAS_SETRUID */
#ifdef HAS_SETEUID
	    if ((delaymagic & DM_UID) == DM_EUID) {
		(void)seteuid(uid);
		delaymagic =~ DM_EUID;
	    }
#endif /* HAS_SETEUID */
	    if (delaymagic & DM_UID) {
		if (uid != euid)
		    fatal("No setreuid available");
		(void)setuid(uid);
	    }
#endif /* not HAS_SETREUID */
	    uid = (int)getuid();
	    euid = (int)geteuid();
	}
	if (delaymagic & DM_GID) {
#ifdef HAS_SETREGID
	    (void)setregid(gid,egid);
#else /* not HAS_SETREGID */
#ifdef HAS_SETRGID
	    if ((delaymagic & DM_GID) == DM_RGID) {
		(void)setrgid(gid);
		delaymagic =~ DM_RGID;
	    }
#endif /* HAS_SETRGID */
#ifdef HAS_SETEGID
	    if ((delaymagic & DM_GID) == DM_EGID) {
		(void)setegid(gid);
		delaymagic =~ DM_EGID;
	    }
#endif /* HAS_SETEGID */
	    if (delaymagic & DM_GID) {
		if (gid != egid)
		    fatal("No setregid available");
		(void)setgid(gid);
	    }
#endif /* not HAS_SETREGID */
	    gid = (int)getgid();
	    egid = (int)getegid();
	}
    }
    delaymagic = 0;
    localizing = FALSE;
    if (gimme == G_ARRAY) {
	i = lastrelem - firstrelem + 1;
	if (ary || hash)
	    Copy(firstrelem, firstlelem, i, STR*);
	return arglast[0] + i;
    }
    else {
	str_numset(arg->arg_ptr.arg_str,(double)(arglast[2] - arglast[1]));
	*firstlelem = arg->arg_ptr.arg_str;
	return arglast[0] + 1;
    }
}

int					/*SUPPRESS 590*/
do_study(str,arg,gimme,arglast)
STR *str;
ARG *arg;
int gimme;
int *arglast;
{
    register unsigned char *s;
    register int pos = str->str_cur;
    register int ch;
    register int *sfirst;
    register int *snext;
    static int maxscream = -1;
    static STR *lastscream = Nullstr;
    int retval;
    int retarg = arglast[0] + 1;

#ifndef lint
    s = (unsigned char*)(str_get(str));
#else
    s = Null(unsigned char*);
#endif
    if (lastscream)
	lastscream->str_pok &= ~SP_STUDIED;
    lastscream = str;
    if (pos <= 0) {
	retval = 0;
	goto ret;
    }
    if (pos > maxscream) {
	if (maxscream < 0) {
	    maxscream = pos + 80;
	    New(301,screamfirst, 256, int);
	    New(302,screamnext, maxscream, int);
	}
	else {
	    maxscream = pos + pos / 4;
	    Renew(screamnext, maxscream, int);
	}
    }

    sfirst = screamfirst;
    snext = screamnext;

    if (!sfirst || !snext)
	fatal("do_study: out of memory");

    for (ch = 256; ch; --ch)
	*sfirst++ = -1;
    sfirst -= 256;

    while (--pos >= 0) {
	ch = s[pos];
	if (sfirst[ch] >= 0)
	    snext[pos] = sfirst[ch] - pos;
	else
	    snext[pos] = -pos;
	sfirst[ch] = pos;

	/* If there were any case insensitive searches, we must assume they
	 * all are.  This speeds up insensitive searches much more than
	 * it slows down sensitive ones.
	 */
	if (sawi)
	    sfirst[fold[ch]] = pos;
    }

    str->str_pok |= SP_STUDIED;
    retval = 1;
  ret:
    str_numset(arg->arg_ptr.arg_str,(double)retval);
    stack->ary_array[retarg] = arg->arg_ptr.arg_str;
    return retarg;
}

int					/*SUPPRESS 590*/
do_defined(str,arg,gimme,arglast)
STR *str;
register ARG *arg;
int gimme;
int *arglast;
{
    register int type;
    register int retarg = arglast[0] + 1;
    int retval;
    ARRAY *ary;
    HASH *hash;

    if ((arg[1].arg_type & A_MASK) != A_LEXPR)
	fatal("Illegal argument to defined()");
    arg = arg[1].arg_ptr.arg_arg;
    type = arg->arg_type;

    if (type == O_SUBR || type == O_DBSUBR) {
	if ((arg[1].arg_type & A_MASK) == A_WORD)
	    retval = stab_sub(arg[1].arg_ptr.arg_stab) != 0;
	else {
	    STR *tmpstr = STAB_STR(arg[1].arg_ptr.arg_stab);

	    retval = tmpstr && stab_sub(stabent(str_get(tmpstr),TRUE)) != 0;
	}
    }
    else if (type == O_ARRAY || type == O_LARRAY ||
	     type == O_ASLICE || type == O_LASLICE )
	retval = ((ary = stab_xarray(arg[1].arg_ptr.arg_stab)) != 0
	    && ary->ary_max >= 0 );
    else if (type == O_HASH || type == O_LHASH ||
	     type == O_HSLICE || type == O_LHSLICE )
	retval = ((hash = stab_xhash(arg[1].arg_ptr.arg_stab)) != 0
	    && hash->tbl_array);
    else
	retval = FALSE;
    str_numset(str,(double)retval);
    stack->ary_array[retarg] = str;
    return retarg;
}

int						/*SUPPRESS 590*/
do_undef(str,arg,gimme,arglast)
STR *str;
register ARG *arg;
int gimme;
int *arglast;
{
    register int type;
    register STAB *stab;
    int retarg = arglast[0] + 1;

    if ((arg[1].arg_type & A_MASK) != A_LEXPR)
	fatal("Illegal argument to undef()");
    arg = arg[1].arg_ptr.arg_arg;
    type = arg->arg_type;

    if (type == O_ARRAY || type == O_LARRAY) {
	stab = arg[1].arg_ptr.arg_stab;
	afree(stab_xarray(stab));
	stab_xarray(stab) = anew(stab);		/* so "@array" still works */
    }
    else if (type == O_HASH || type == O_LHASH) {
	stab = arg[1].arg_ptr.arg_stab;
	if (stab == envstab)
	    environ[0] = Nullch;
	else if (stab == sigstab) {
	    int i;

	    for (i = 1; i < NSIG; i++)
		signal(i, SIG_DFL);	/* munch, munch, munch */
	}
	(void)hfree(stab_xhash(stab), TRUE);
	stab_xhash(stab) = Null(HASH*);
    }
    else if (type == O_SUBR || type == O_DBSUBR) {
	stab = arg[1].arg_ptr.arg_stab;
	if ((arg[1].arg_type & A_MASK) != A_WORD) {
	    STR *tmpstr = STAB_STR(arg[1].arg_ptr.arg_stab);

	    if (tmpstr)
		stab = stabent(str_get(tmpstr),TRUE);
	    else
		stab = Nullstab;
	}
	if (stab && stab_sub(stab)) {
	    cmd_free(stab_sub(stab)->cmd);
	    stab_sub(stab)->cmd = Nullcmd;
	    afree(stab_sub(stab)->tosave);
	    Safefree(stab_sub(stab));
	    stab_sub(stab) = Null(SUBR*);
	}
    }
    else
	fatal("Can't undefine that kind of object");
    str_numset(str,0.0);
    stack->ary_array[retarg] = str;
    return retarg;
}

int
do_vec(lvalue,astr,arglast)
int lvalue;
STR *astr;
int *arglast;
{
    STR **st = stack->ary_array;
    int sp = arglast[0];
    register STR *str = st[++sp];
    register int offset = (int)str_gnum(st[++sp]);
    register int size = (int)str_gnum(st[++sp]);
    unsigned char *s = (unsigned char*)str_get(str);
    unsigned long retnum;
    int len;

    sp = arglast[1];
    offset *= size;		/* turn into bit offset */
    len = (offset + size + 7) / 8;
    if (offset < 0 || size < 1)
	retnum = 0;
    else if (!lvalue && len > str->str_cur)
	retnum = 0;
    else {
	if (len > str->str_cur) {
	    STR_GROW(str,len);
	    (void)memzero(str->str_ptr + str->str_cur, len - str->str_cur);
	    str->str_cur = len;
	}
	s = (unsigned char*)str_get(str);
	if (size < 8)
	    retnum = (s[offset >> 3] >> (offset & 7)) & ((1 << size) - 1);
	else {
	    offset >>= 3;
	    if (size == 8)
		retnum = s[offset];
	    else if (size == 16)
		retnum = ((unsigned long) s[offset] << 8) + s[offset+1];
	    else if (size == 32)
		retnum = ((unsigned long) s[offset] << 24) +
			((unsigned long) s[offset + 1] << 16) +
			(s[offset + 2] << 8) + s[offset+3];
	}

	if (lvalue) {                      /* it's an lvalue! */
	    struct lstring *lstr = (struct lstring*)astr;

	    astr->str_magic = str;
	    st[sp]->str_rare = 'v';
	    lstr->lstr_offset = offset;
	    lstr->lstr_len = size;
	}
    }

    str_numset(astr,(double)retnum);
    st[sp] = astr;
    return sp;
}

void
do_vecset(mstr,str)
STR *mstr;
STR *str;
{
    struct lstring *lstr = (struct lstring*)str;
    register int offset;
    register int size;
    register unsigned char *s = (unsigned char*)mstr->str_ptr;
    register unsigned long lval = U_L(str_gnum(str));
    int mask;

    mstr->str_rare = 0;
    str->str_magic = Nullstr;
    offset = lstr->lstr_offset;
    size = lstr->lstr_len;
    if (size < 8) {
	mask = (1 << size) - 1;
	size = offset & 7;
	lval &= mask;
	offset >>= 3;
	s[offset] &= ~(mask << size);
	s[offset] |= lval << size;
    }
    else {
	if (size == 8)
	    s[offset] = lval & 255;
	else if (size == 16) {
	    s[offset] = (lval >> 8) & 255;
	    s[offset+1] = lval & 255;
	}
	else if (size == 32) {
	    s[offset] = (lval >> 24) & 255;
	    s[offset+1] = (lval >> 16) & 255;
	    s[offset+2] = (lval >> 8) & 255;
	    s[offset+3] = lval & 255;
	}
    }
}

void
do_chop(astr,str)
register STR *astr;
register STR *str;
{
    register char *tmps;
    register int i;
    ARRAY *ary;
    HASH *hash;
    HENT *entry;

    if (!str)
	return;
    if (str->str_state == SS_ARY) {
	ary = stab_array(str->str_u.str_stab);
	for (i = 0; i <= ary->ary_fill; i++)
	    do_chop(astr,ary->ary_array[i]);
	return;
    }
    if (str->str_state == SS_HASH) {
	hash = stab_hash(str->str_u.str_stab);
	(void)hiterinit(hash);
	/*SUPPRESS 560*/
	while (entry = hiternext(hash))
	    do_chop(astr,hiterval(hash,entry));
	return;
    }
    tmps = str_get(str);
    if (tmps && str->str_cur) {
	tmps += str->str_cur - 1;
	str_nset(astr,tmps,1);	/* remember last char */
	*tmps = '\0';				/* wipe it out */
	str->str_cur = tmps - str->str_ptr;
	str->str_nok = 0;
	STABSET(str);
    }
    else
	str_nset(astr,"",0);
}

void
do_vop(optype,str,left,right)
STR *str;
STR *left;
STR *right;
{
    register char *s;
    register char *l = str_get(left);
    register char *r = str_get(right);
    register int len;

    len = left->str_cur;
    if (len > right->str_cur)
	len = right->str_cur;
    if (str->str_cur > len)
	str->str_cur = len;
    else if (str->str_cur < len) {
	STR_GROW(str,len);
	(void)memzero(str->str_ptr + str->str_cur, len - str->str_cur);
	str->str_cur = len;
    }
    str->str_pok = 1;
    str->str_nok = 0;
    s = str->str_ptr;
    if (!s) {
	str_nset(str,"",0);
	s = str->str_ptr;
    }
    switch (optype) {
    case O_BIT_AND:
	while (len--)
	    *s++ = *l++ & *r++;
	break;
    case O_XOR:
	while (len--)
	    *s++ = *l++ ^ *r++;
	goto mop_up;
    case O_BIT_OR:
	while (len--)
	    *s++ = *l++ | *r++;
      mop_up:
	len = str->str_cur;
	if (right->str_cur > len)
	    str_ncat(str,right->str_ptr+len,right->str_cur - len);
	else if (left->str_cur > len)
	    str_ncat(str,left->str_ptr+len,left->str_cur - len);
	break;
    }
}

int
do_syscall(arglast)
int *arglast;
{
    register STR **st = stack->ary_array;
    register int sp = arglast[1];
    register int items = arglast[2] - sp;
#ifdef atarist
    unsigned long arg[14]; /* yes, we really need that many ! */
#else
    unsigned long arg[8];
#endif
    register int i = 0;
    int retval = -1;

#ifdef HAS_SYSCALL
#ifdef TAINT
    for (st += ++sp; items--; st++)
	tainted |= (*st)->str_tainted;
    st = stack->ary_array;
    sp = arglast[1];
    items = arglast[2] - sp;
#endif
#ifdef TAINT
    taintproper("Insecure dependency in syscall");
#endif
    /* This probably won't work on machines where sizeof(long) != sizeof(int)
     * or where sizeof(long) != sizeof(char*).  But such machines will
     * not likely have syscall implemented either, so who cares?
     */
    while (items--) {
	if (st[++sp]->str_nok || !i)
	    arg[i++] = (unsigned long)str_gnum(st[sp]);
#ifndef lint
	else
	    arg[i++] = (unsigned long)st[sp]->str_ptr;
#endif /* lint */
    }
    sp = arglast[1];
    items = arglast[2] - sp;
    switch (items) {
    case 0:
	fatal("Too few args to syscall");
    case 1:
	retval = syscall(arg[0]);
	break;
    case 2:
	retval = syscall(arg[0],arg[1]);
	break;
    case 3:
	retval = syscall(arg[0],arg[1],arg[2]);
	break;
    case 4:
	retval = syscall(arg[0],arg[1],arg[2],arg[3]);
	break;
    case 5:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4]);
	break;
    case 6:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5]);
	break;
    case 7:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6]);
	break;
    case 8:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7]);
	break;
#ifdef atarist
    case 9:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7], arg[8]);
	break;
    case 10:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7], arg[8], arg[9]);
	break;
    case 11:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7], arg[8], arg[9], arg[10]);
	break;
    case 12:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7], arg[8], arg[9], arg[10], arg[11]);
	break;
    case 13:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7], arg[8], arg[9], arg[10], arg[11], arg[12]);
	break;
    case 14:
	retval = syscall(arg[0],arg[1],arg[2],arg[3],arg[4],arg[5],arg[6],
	  arg[7], arg[8], arg[9], arg[10], arg[11], arg[12], arg[13]);
	break;
#endif /* atarist */
    }
    return retval;
#else
    fatal("syscall() unimplemented");
#endif
}


