/*    doop.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "'So that was the job I felt I had to do when I started,' thought Sam."
 */

#include "EXTERN.h"
#define PERL_IN_DOOP_C
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

STATIC I32
S_do_trans_simple(pTHX_ SV *sv)
{
    U8 *s;
    U8 *d;
    U8 *send;
    U8 *dstart;
    I32 matches = 0;
    I32 grows = PL_op->op_private & OPpTRANS_GROWS;
    STRLEN len;
    short *tbl;
    I32 ch;

    tbl = (short*)cPVOP->op_pv;
    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_simple");

    s = (U8*)SvPV(sv, len);
    send = s + len;

    /* First, take care of non-UTF8 input strings, because they're easy */
    if (!SvUTF8(sv)) {
	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		matches++;
		*s++ = ch;
	    }
	    else
		s++;
	}
	SvSETMAGIC(sv);
        return matches;
    }

    /* Allow for expansion: $_="a".chr(400); tr/a/\xFE/, FE needs encoding */
    if (grows)
	New(0, d, len*2+1, U8);
    else
	d = s;
    dstart = d;
    while (s < send) {
        STRLEN ulen;
        UV c;

        /* Need to check this, otherwise 128..255 won't match */
	c = utf8_to_uv(s, send - s, &ulen, 0);
        if (c < 0x100 && (ch = tbl[c]) >= 0) {
            matches++;
            if (UTF8_IS_ASCII(ch))
                *d++ = ch;
            else
                d = uv_to_utf8(d,ch);
            s += ulen;
        }
	else { /* No match -> copy */
	    Copy(s, d, ulen, U8);
	    d += ulen;
	    s += ulen;
        }
    }
    if (grows) {
	sv_setpvn(sv, (char*)dstart, d - dstart);
	Safefree(dstart);
    }
    else {
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    SvUTF8_on(sv);
    SvSETMAGIC(sv);
    return matches;
}

STATIC I32
S_do_trans_count(pTHX_ SV *sv)/* SPC - OK */
{
    U8 *s;
    U8 *send;
    I32 matches = 0;
    STRLEN len;
    short *tbl;

    tbl = (short*)cPVOP->op_pv;
    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_count");

    s = (U8*)SvPV(sv, len);
    send = s + len;

    if (!SvUTF8(sv))
	while (s < send) {
            if (tbl[*s++] >= 0)
                matches++;
	}
    else
	while (s < send) {
	    UV c;
	    STRLEN ulen;
	    c = utf8_to_uv(s, send - s, &ulen, 0);
	    if (c < 0x100 && tbl[c] >= 0)
		matches++;
	    s += ulen;
	}

    return matches;
}

STATIC I32
S_do_trans_complex(pTHX_ SV *sv)/* SPC - NOT OK */
{
    U8 *s;
    U8 *send;
    U8 *d;
    U8 *dstart;
    I32 isutf8;
    I32 matches = 0;
    I32 grows = PL_op->op_private & OPpTRANS_GROWS;
    STRLEN len;
    short *tbl;
    I32 ch;

    tbl = (short*)cPVOP->op_pv;
    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans_complex");

    s = (U8*)SvPV(sv, len);
    isutf8 = SvUTF8(sv);
    send = s + len;

    if (!isutf8) {
	dstart = d = s;
	if (PL_op->op_private & OPpTRANS_SQUASH) {
	    U8* p = send;
	    while (s < send) {
		if ((ch = tbl[*s]) >= 0) {
		    *d = ch;
		    matches++;
		    if (p != d - 1 || *p != *d)
			p = d++;
		}
		else if (ch == -1)	/* -1 is unmapped character */
		    *d++ = *s;	
		else if (ch == -2)	/* -2 is delete character */
		    matches++;
		s++;
	    }
	}
	else {
	    while (s < send) {
	        if ((ch = tbl[*s]) >= 0) {
		    matches++;
		    *d++ = ch;
		}
		else if (ch == -1)	/* -1 is unmapped character */
		    *d++ = *s;
		else if (ch == -2)      /* -2 is delete character */
		    matches++;
		s++;
	    }
	}
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    else { /* isutf8 */
	if (grows)
	    New(0, d, len*2+1, U8);
	else
	    d = s;
	dstart = d;

#ifdef MACOS_TRADITIONAL
#define comp CoMP   /* "comp" is a keyword in some compilers ... */
#endif

	if (PL_op->op_private & OPpTRANS_SQUASH) {
	    U8* p = send;
	    UV pch = 0xfeedface;
	    while (s < send) {
		STRLEN len;
	        UV comp = utf8_to_uv_simple(s, &len);

		if (comp > 0xff) {	/* always unmapped */	
		    Copy(s, d, len, U8);
		    d += len;
		}
		else if ((ch = tbl[comp]) >= 0) {
		    matches++;
		    if (ch != pch) {
		        d = uv_to_utf8(d, ch);
		        pch = ch;
		    }
		    s += len;
		    continue;
		}
		else if (ch == -1) {	/* -1 is unmapped character */
		    Copy(s, d, len, U8);
		    d += len;
		}
		else if (ch == -2)      /* -2 is delete character */
		    matches++;
		s += len;
		pch = 0xfeedface;
	    }
	}
	else {
	    while (s < send) {
		STRLEN len;
	        UV comp = utf8_to_uv_simple(s, &len);
		if (comp > 0xff) {	/* always unmapped */
		    Copy(s, d, len, U8);
		    d += len;
		}
		else if ((ch = tbl[comp]) >= 0) {
		    d = uv_to_utf8(d, ch);
		    matches++;
		}
		else if (ch == -1) {	/* -1 is unmapped character */
		    Copy(s, d, len, U8);
		    d += len;
		}
		else if (ch == -2)      /* -2 is delete character */
		    matches++;
		s += len;
	    }
	}
	if (grows) {
	    sv_setpvn(sv, (char*)dstart, d - dstart);
	    Safefree(dstart);
	}
	else {
	    *d = '\0';
	    SvCUR_set(sv, d - dstart);
	}
	SvUTF8_on(sv);
    }
    SvSETMAGIC(sv);
    return matches;
}

STATIC I32
S_do_trans_simple_utf8(pTHX_ SV *sv)/* SPC - OK */
{
    U8 *s;
    U8 *send;
    U8 *d;
    U8 *start;
    U8 *dstart, *dend;
    I32 matches = 0;
    I32 grows = PL_op->op_private & OPpTRANS_GROWS;
    STRLEN len;

    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV extra = none + 1;
    UV final;
    UV uv;
    I32 isutf8;
    U8 hibit = 0;

    s = (U8*)SvPV(sv, len);
    isutf8 = SvUTF8(sv);
    if (!isutf8) {
	U8 *t = s, *e = s + len;
	while (t < e)
	    if ((hibit = UTF8_IS_CONTINUED(*t++)))
		break;
	if (hibit)
	    s = bytes_to_utf8(s, &len);
    }
    send = s + len;
    start = s;

    svp = hv_fetch(hv, "FINAL", 5, FALSE);
    if (svp)
	final = SvUV(*svp);

    if (grows) {
	/* d needs to be bigger than s, in case e.g. upgrading is required */
	New(0, d, len*3+UTF8_MAXLEN, U8);
	dend = d + len * 3;
	dstart = d;
    }
    else {
	dstart = d = s;
	dend = d + len;
    }

    while (s < send) {
	if ((uv = swash_fetch(rv, s)) < none) {
	    s += UTF8SKIP(s);
	    matches++;
	    d = uv_to_utf8(d, uv);
	}
	else if (uv == none) {
	    int i = UTF8SKIP(s);
	    Copy(s, d, i, U8);
	    d += i;
	    s += i;
	}
	else if (uv == extra) {
	    int i = UTF8SKIP(s);
	    s += i;
	    matches++;
	    d = uv_to_utf8(d, final);
	}
	else
	    s += UTF8SKIP(s);

	if (d > dend) {
	    STRLEN clen = d - dstart;
	    STRLEN nlen = dend - dstart + len + UTF8_MAXLEN;
	    if (!grows)
		Perl_croak(aTHX_ "panic: do_trans_complex_utf8");
	    Renew(dstart, nlen+UTF8_MAXLEN, U8);
	    d = dstart + clen;
	    dend = dstart + nlen;
	}
    }
    if (grows || hibit) {
	sv_setpvn(sv, (char*)dstart, d - dstart);
	Safefree(dstart);
	if (grows && hibit)
	    Safefree(start);
    }
    else {
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    SvSETMAGIC(sv);
    SvUTF8_on(sv);
    if (!isutf8 && !(PL_hints & HINT_UTF8))
	sv_utf8_downgrade(sv, TRUE);

    return matches;
}

STATIC I32
S_do_trans_count_utf8(pTHX_ SV *sv)/* SPC - OK */
{
    U8 *s;
    U8 *start, *send;
    I32 matches = 0;
    STRLEN len;

    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV uv;
    U8 hibit = 0;

    s = (U8*)SvPV(sv, len);
    if (!SvUTF8(sv)) {
	U8 *t = s, *e = s + len;
	while (t < e)
	    if ((hibit = !UTF8_IS_ASCII(*t++)))
		break;
	if (hibit)
	    start = s = bytes_to_utf8(s, &len);
    }
    send = s + len;

    while (s < send) {
	if ((uv = swash_fetch(rv, s)) < none)
	    matches++;
	s += UTF8SKIP(s);
    }
    if (hibit)
        Safefree(start);

    return matches;
}

STATIC I32
S_do_trans_complex_utf8(pTHX_ SV *sv) /* SPC - NOT OK */
{
    U8 *s;
    U8 *start, *send;
    U8 *d;
    I32 matches = 0;
    I32 squash   = PL_op->op_private & OPpTRANS_SQUASH;
    I32 del      = PL_op->op_private & OPpTRANS_DELETE;
    I32 grows    = PL_op->op_private & OPpTRANS_GROWS;
    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV extra = none + 1;
    UV final;
    UV uv;
    STRLEN len;
    U8 *dstart, *dend;
    I32 isutf8;
    U8 hibit = 0;

    s = (U8*)SvPV(sv, len);
    isutf8 = SvUTF8(sv);
    if (!isutf8) {
	U8 *t = s, *e = s + len;
	while (t < e)
	    if ((hibit = !UTF8_IS_ASCII(*t++)))
		break;
	if (hibit)
	    s = bytes_to_utf8(s, &len);
    }
    send = s + len;
    start = s;

    svp = hv_fetch(hv, "FINAL", 5, FALSE);
    if (svp)
	final = SvUV(*svp);

    if (grows) {
	/* d needs to be bigger than s, in case e.g. upgrading is required */
	New(0, d, len*3+UTF8_MAXLEN, U8);
	dend = d + len * 3;
	dstart = d;
    }
    else {
	dstart = d = s;
	dend = d + len;
    }

    if (squash) {
	UV puv = 0xfeedface;
	while (s < send) {
	    uv = swash_fetch(rv, s);
	    
	    if (d > dend) {
	        STRLEN clen = d - dstart;
		STRLEN nlen = dend - dstart + len + UTF8_MAXLEN;
		if (!grows)
		    Perl_croak(aTHX_ "panic: do_trans_complex_utf8");
		Renew(dstart, nlen+UTF8_MAXLEN, U8);
		d = dstart + clen;
		dend = dstart + nlen;
	    }
	    if (uv < none) {
		matches++;
		if (uv != puv) {
		    d = uv_to_utf8(d, uv);
		    puv = uv;
		}
		s += UTF8SKIP(s);
		continue;
	    }
	    else if (uv == none) {	/* "none" is unmapped character */
		int i = UTF8SKIP(s);
		Copy(s, d, i, U8);
		d += i;
		s += i;
		puv = 0xfeedface;
		continue;
	    }
	    else if (uv == extra && !del) {
		matches++;
		if (uv != puv) {
		    d = uv_to_utf8(d, final);
		    puv = final;
		}
		s += UTF8SKIP(s);
		continue;
	    }
	    matches++;			/* "none+1" is delete character */
	    s += UTF8SKIP(s);
	}
    }
    else {
	while (s < send) {
	    uv = swash_fetch(rv, s);
	    if (d > dend) {
	        STRLEN clen = d - dstart;
		STRLEN nlen = dend - dstart + len + UTF8_MAXLEN;
		if (!grows)
		    Perl_croak(aTHX_ "panic: do_trans_complex_utf8");
		Renew(dstart, nlen+UTF8_MAXLEN, U8);
		d = dstart + clen;
		dend = dstart + nlen;
	    }
	    if (uv < none) {
		matches++;
		d = uv_to_utf8(d, uv);
		s += UTF8SKIP(s);
		continue;
	    }
	    else if (uv == none) {	/* "none" is unmapped character */
		int i = UTF8SKIP(s);
		Copy(s, d, i, U8);
		d += i;
		s += i;
		continue;
	    }
	    else if (uv == extra && !del) {
		matches++;
		d = uv_to_utf8(d, final);
		s += UTF8SKIP(s);
		continue;
	    }
	    matches++;			/* "none+1" is delete character */
	    s += UTF8SKIP(s);
	}
    }
    if (grows || hibit) {
	sv_setpvn(sv, (char*)dstart, d - dstart);
	Safefree(dstart);
	if (grows && hibit)
	    Safefree(start);
    }
    else {
	*d = '\0';
	SvCUR_set(sv, d - dstart);
    }
    SvUTF8_on(sv);
    if (!isutf8 && !(PL_hints & HINT_UTF8))
	sv_utf8_downgrade(sv, TRUE);
    SvSETMAGIC(sv);

    return matches;
}

I32
Perl_do_trans(pTHX_ SV *sv)
{
    STRLEN len;
    I32 hasutf = (PL_op->op_private &
                    (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF));

    if (SvREADONLY(sv) && !(PL_op->op_private & OPpTRANS_IDENTICAL))
	Perl_croak(aTHX_ PL_no_modify);

    (void)SvPV(sv, len);
    if (!len)
	return 0;
    if (!SvPOKp(sv))
	(void)SvPV_force(sv, len);
    if (!(PL_op->op_private & OPpTRANS_IDENTICAL))
	(void)SvPOK_only_UTF8(sv);

    DEBUG_t( Perl_deb(aTHX_ "2.TBL\n"));

    switch (PL_op->op_private & ~hasutf & 63) {
    case 0:
	if (hasutf)
	    return do_trans_simple_utf8(sv);
	else
	    return do_trans_simple(sv);

    case OPpTRANS_IDENTICAL:
	if (hasutf)
	    return do_trans_count_utf8(sv);
	else
	    return do_trans_count(sv);

    default:
	if (hasutf)
	    return do_trans_complex_utf8(sv);
	else
	    return do_trans_complex(sv);
    }
}

void
Perl_do_join(pTHX_ register SV *sv, SV *del, register SV **mark, register SV **sp)
{
    SV **oldmark = mark;
    register I32 items = sp - mark;
    register STRLEN len;
    STRLEN delimlen;
    register char *delim = SvPV(del, delimlen);
    STRLEN tmplen;

    mark++;
    len = (items > 0 ? (delimlen * (items - 1) ) : 0);
    (void)SvUPGRADE(sv, SVt_PV);
    if (SvLEN(sv) < len + items) {	/* current length is way too short */
	while (items-- > 0) {
	    if (*mark && !SvGAMAGIC(*mark) && SvOK(*mark)) {
		SvPV(*mark, tmplen);
		len += tmplen;
	    }
	    mark++;
	}
	SvGROW(sv, len + 1);		/* so try to pre-extend */

	mark = oldmark;
	items = sp - mark;
	++mark;
    }

    if (items-- > 0) {
	sv_setpv(sv, "");
	if (*mark)
	    sv_catsv(sv, *mark);
	mark++;
    }
    else
	sv_setpv(sv,"");
    if (delimlen) {
	for (; items > 0; items--,mark++) {
	    sv_catsv(sv,del);
	    sv_catsv(sv,*mark);
	}
    }
    else {
	for (; items > 0; items--,mark++)
	    sv_catsv(sv,*mark);
    }
    SvSETMAGIC(sv);
}

void
Perl_do_sprintf(pTHX_ SV *sv, I32 len, SV **sarg)
{
    STRLEN patlen;
    char *pat = SvPV(*sarg, patlen);
    bool do_taint = FALSE;

    sv_vsetpvfn(sv, pat, patlen, Null(va_list*), sarg + 1, len - 1, &do_taint);
    SvSETMAGIC(sv);
    if (do_taint)
	SvTAINTED_on(sv);
}

/* currently converts input to bytes if possible, but doesn't sweat failure */
UV
Perl_do_vecget(pTHX_ SV *sv, I32 offset, I32 size)
{
    STRLEN srclen, len;
    unsigned char *s = (unsigned char *) SvPV(sv, srclen);
    UV retnum = 0;

    if (offset < 0)
	return retnum;
    if (size < 1 || (size & (size-1))) /* size < 1 or not a power of two */
	Perl_croak(aTHX_ "Illegal number of bits in vec");

    if (SvUTF8(sv))
	(void) Perl_sv_utf8_downgrade(aTHX_ sv, TRUE);

    offset *= size;	/* turn into bit offset */
    len = (offset + size + 7) / 8;	/* required number of bytes */
    if (len > srclen) {
	if (size <= 8)
	    retnum = 0;
	else {
	    offset >>= 3;	/* turn into byte offset */
	    if (size == 16) {
		if (offset >= srclen)
		    retnum = 0;
		else
		    retnum = (UV) s[offset] <<  8;
	    }
	    else if (size == 32) {
		if (offset >= srclen)
		    retnum = 0;
		else if (offset + 1 >= srclen)
		    retnum =
			((UV) s[offset    ] << 24);
		else if (offset + 2 >= srclen)
		    retnum =
			((UV) s[offset    ] << 24) +
			((UV) s[offset + 1] << 16);
		else
		    retnum =
			((UV) s[offset    ] << 24) +
			((UV) s[offset + 1] << 16) +
			(     s[offset + 2] <<  8);
	    }
#ifdef UV_IS_QUAD
	    else if (size == 64) {
		if (ckWARN(WARN_PORTABLE))
		    Perl_warner(aTHX_ WARN_PORTABLE,
				"Bit vector size > 32 non-portable");
		if (offset >= srclen)
		    retnum = 0;
		else if (offset + 1 >= srclen)
		    retnum =
			(UV) s[offset     ] << 56;
		else if (offset + 2 >= srclen)
		    retnum =
			((UV) s[offset    ] << 56) +
			((UV) s[offset + 1] << 48);
		else if (offset + 3 >= srclen)
		    retnum =
			((UV) s[offset    ] << 56) +
			((UV) s[offset + 1] << 48) +
			((UV) s[offset + 2] << 40);
		else if (offset + 4 >= srclen)
		    retnum =
			((UV) s[offset    ] << 56) +
			((UV) s[offset + 1] << 48) +
			((UV) s[offset + 2] << 40) +
			((UV) s[offset + 3] << 32);
		else if (offset + 5 >= srclen)
		    retnum =
			((UV) s[offset    ] << 56) +
			((UV) s[offset + 1] << 48) +
			((UV) s[offset + 2] << 40) +
			((UV) s[offset + 3] << 32) +
			(     s[offset + 4] << 24);
		else if (offset + 6 >= srclen)
		    retnum =
			((UV) s[offset    ] << 56) +
			((UV) s[offset + 1] << 48) +
			((UV) s[offset + 2] << 40) +
			((UV) s[offset + 3] << 32) +
			((UV) s[offset + 4] << 24) +
			((UV) s[offset + 5] << 16);
		else
		    retnum =
			((UV) s[offset    ] << 56) +
			((UV) s[offset + 1] << 48) +
			((UV) s[offset + 2] << 40) +
			((UV) s[offset + 3] << 32) +
			((UV) s[offset + 4] << 24) +
			((UV) s[offset + 5] << 16) +
			(     s[offset + 6] <<  8);
	    }
#endif
	}
    }
    else if (size < 8)
	retnum = (s[offset >> 3] >> (offset & 7)) & ((1 << size) - 1);
    else {
	offset >>= 3;	/* turn into byte offset */
	if (size == 8)
	    retnum = s[offset];
	else if (size == 16)
	    retnum =
		((UV) s[offset] <<      8) +
		      s[offset + 1];
	else if (size == 32)
	    retnum =
		((UV) s[offset    ] << 24) +
		((UV) s[offset + 1] << 16) +
		(     s[offset + 2] <<  8) +
		      s[offset + 3];
#ifdef UV_IS_QUAD
	else if (size == 64) {
	    if (ckWARN(WARN_PORTABLE))
		Perl_warner(aTHX_ WARN_PORTABLE,
			    "Bit vector size > 32 non-portable");
	    retnum =
		((UV) s[offset    ] << 56) +
		((UV) s[offset + 1] << 48) +
		((UV) s[offset + 2] << 40) +
		((UV) s[offset + 3] << 32) +
		((UV) s[offset + 4] << 24) +
		((UV) s[offset + 5] << 16) +
		(     s[offset + 6] <<  8) +
		      s[offset + 7];
	}
#endif
    }

    return retnum;
}

/* currently converts input to bytes if possible but doesn't sweat failures,
 * although it does ensure that the string it clobbers is not marked as
 * utf8-valid any more
 */
void
Perl_do_vecset(pTHX_ SV *sv)
{
    SV *targ = LvTARG(sv);
    register I32 offset;
    register I32 size;
    register unsigned char *s;
    register UV lval;
    I32 mask;
    STRLEN targlen;
    STRLEN len;

    if (!targ)
	return;
    s = (unsigned char*)SvPV_force(targ, targlen);
    if (SvUTF8(targ)) {
	/* This is handled by the SvPOK_only below...
	if (!Perl_sv_utf8_downgrade(aTHX_ targ, TRUE))
	    SvUTF8_off(targ);
	 */
	(void) Perl_sv_utf8_downgrade(aTHX_ targ, TRUE);
    }

    (void)SvPOK_only(targ);
    lval = SvUV(sv);
    offset = LvTARGOFF(sv);
    if (offset < 0)
	Perl_croak(aTHX_ "Assigning to negative offset in vec");
    size = LvTARGLEN(sv);
    if (size < 1 || (size & (size-1))) /* size < 1 or not a power of two */
	Perl_croak(aTHX_ "Illegal number of bits in vec");

    offset *= size;			/* turn into bit offset */
    len = (offset + size + 7) / 8;	/* required number of bytes */
    if (len > targlen) {
	s = (unsigned char*)SvGROW(targ, len + 1);
	(void)memzero(s + targlen, len - targlen + 1);
	SvCUR_set(targ, len);
    }

    if (size < 8) {
	mask = (1 << size) - 1;
	size = offset & 7;
	lval &= mask;
	offset >>= 3;			/* turn into byte offset */
	s[offset] &= ~(mask << size);
	s[offset] |= lval << size;
    }
    else {
	offset >>= 3;			/* turn into byte offset */
	if (size == 8)
	    s[offset  ] = lval         & 0xff;
	else if (size == 16) {
	    s[offset  ] = (lval >>  8) & 0xff;
	    s[offset+1] = lval         & 0xff;
	}
	else if (size == 32) {
	    s[offset  ] = (lval >> 24) & 0xff;
	    s[offset+1] = (lval >> 16) & 0xff;
	    s[offset+2] = (lval >>  8) & 0xff;
	    s[offset+3] =  lval        & 0xff;
	}
#ifdef UV_IS_QUAD
	else if (size == 64) {
	    if (ckWARN(WARN_PORTABLE))
		Perl_warner(aTHX_ WARN_PORTABLE,
			    "Bit vector size > 32 non-portable");
	    s[offset  ] = (lval >> 56) & 0xff;
	    s[offset+1] = (lval >> 48) & 0xff;
	    s[offset+2] = (lval >> 40) & 0xff;
	    s[offset+3] = (lval >> 32) & 0xff;
	    s[offset+4] = (lval >> 24) & 0xff;
	    s[offset+5] = (lval >> 16) & 0xff;
	    s[offset+6] = (lval >>  8) & 0xff;
	    s[offset+7] =  lval        & 0xff;
	}
#endif
    }
    SvSETMAGIC(targ);
}

void
Perl_do_chop(pTHX_ register SV *astr, register SV *sv)
{
    STRLEN len;
    char *s;

    if (SvTYPE(sv) == SVt_PVAV) {
	register I32 i;
        I32 max;
	AV* av = (AV*)sv;
        max = AvFILL(av);
        for (i = 0; i <= max; i++) {
	    sv = (SV*)av_fetch(av, i, FALSE);
	    if (sv && ((sv = *(SV**)sv), sv != &PL_sv_undef))
		do_chop(astr, sv);
	}
        return;
    }
    else if (SvTYPE(sv) == SVt_PVHV) {
        HV* hv = (HV*)sv;
	HE* entry;
        (void)hv_iterinit(hv);
        /*SUPPRESS 560*/
        while ((entry = hv_iternext(hv)))
            do_chop(astr,hv_iterval(hv,entry));
        return;
    }
    else if (SvREADONLY(sv))
	Perl_croak(aTHX_ PL_no_modify);
    s = SvPV(sv, len);
    if (len && !SvPOK(sv))
	s = SvPV_force(sv, len);
    if (DO_UTF8(sv)) {
	if (s && len) {
	    char *send = s + len;
	    char *start = s;
	    s = send - 1;
	    while (s > start && UTF8_IS_CONTINUATION(*s))
		s--;
	    if (utf8_to_uv_simple((U8*)s, 0)) {
		sv_setpvn(astr, s, send - s);
		*s = '\0';
		SvCUR_set(sv, s - start);
		SvNIOK_off(sv);
		SvUTF8_on(astr);
	    }
	}
	else
	    sv_setpvn(astr, "", 0);
    }
    else if (s && len) {
	s += --len;
	sv_setpvn(astr, s, 1);
	*s = '\0';
	SvCUR_set(sv, len);
	SvUTF8_off(sv);
	SvNIOK_off(sv);
    }
    else
	sv_setpvn(astr, "", 0);
    SvSETMAGIC(sv);
}

I32
Perl_do_chomp(pTHX_ register SV *sv)
{
    register I32 count;
    STRLEN len;
    char *s;

    if (RsSNARF(PL_rs))
	return 0;
    if (RsRECORD(PL_rs))
      return 0;
    count = 0;
    if (SvTYPE(sv) == SVt_PVAV) {
	register I32 i;
        I32 max;
	AV* av = (AV*)sv;
        max = AvFILL(av);
        for (i = 0; i <= max; i++) {
	    sv = (SV*)av_fetch(av, i, FALSE);
	    if (sv && ((sv = *(SV**)sv), sv != &PL_sv_undef))
		count += do_chomp(sv);
	}
        return count;
    }
    else if (SvTYPE(sv) == SVt_PVHV) {
        HV* hv = (HV*)sv;
	HE* entry;
        (void)hv_iterinit(hv);
        /*SUPPRESS 560*/
        while ((entry = hv_iternext(hv)))
            count += do_chomp(hv_iterval(hv,entry));
        return count;
    }
    else if (SvREADONLY(sv))
	Perl_croak(aTHX_ PL_no_modify);
    s = SvPV(sv, len);
    if (len && !SvPOKp(sv))
	s = SvPV_force(sv, len);
    if (s && len) {
	s += --len;
	if (RsPARA(PL_rs)) {
	    if (*s != '\n')
		goto nope;
	    ++count;
	    while (len && s[-1] == '\n') {
		--len;
		--s;
		++count;
	    }
	}
	else {
	    STRLEN rslen;
	    char *rsptr = SvPV(PL_rs, rslen);
	    if (rslen == 1) {
		if (*s != *rsptr)
		    goto nope;
		++count;
	    }
	    else {
		if (len < rslen - 1)
		    goto nope;
		len -= rslen - 1;
		s -= rslen - 1;
		if (memNE(s, rsptr, rslen))
		    goto nope;
		count += rslen;
	    }
	}
	*s = '\0';
	SvCUR_set(sv, len);
	SvNIOK_off(sv);
    }
  nope:
    SvSETMAGIC(sv);
    return count;
}

void
Perl_do_vop(pTHX_ I32 optype, SV *sv, SV *left, SV *right)
{
#ifdef LIBERAL
    register long *dl;
    register long *ll;
    register long *rl;
#endif
    register char *dc;
    STRLEN leftlen;
    STRLEN rightlen;
    register char *lc;
    register char *rc;
    register I32 len;
    I32 lensave;
    char *lsave;
    char *rsave;
    bool left_utf = DO_UTF8(left);
    bool right_utf = DO_UTF8(right);
    I32 needlen;

    if (left_utf && !right_utf)
	sv_utf8_upgrade(right);
    else if (!left_utf && right_utf)
	sv_utf8_upgrade(left);

    if (sv != left || (optype != OP_BIT_AND && !SvOK(sv) && !SvGMAGICAL(sv)))
	sv_setpvn(sv, "", 0);	/* avoid undef warning on |= and ^= */
    lsave = lc = SvPV(left, leftlen);
    rsave = rc = SvPV(right, rightlen);
    len = leftlen < rightlen ? leftlen : rightlen;
    lensave = len;
    if ((left_utf || right_utf) && (sv == left || sv == right)) {
	needlen = optype == OP_BIT_AND ? len : leftlen + rightlen;
	Newz(801, dc, needlen + 1, char);
    }
    else if (SvOK(sv) || SvTYPE(sv) > SVt_PVMG) {
	STRLEN n_a;
	dc = SvPV_force(sv, n_a);
	if (SvCUR(sv) < len) {
	    dc = SvGROW(sv, len + 1);
	    (void)memzero(dc + SvCUR(sv), len - SvCUR(sv) + 1);
	}
	if (optype != OP_BIT_AND && (left_utf || right_utf))
	    dc = SvGROW(sv, leftlen + rightlen + 1);
    }
    else {
	needlen = ((optype == OP_BIT_AND)
		    ? len : (leftlen > rightlen ? leftlen : rightlen));
	Newz(801, dc, needlen + 1, char);
	(void)sv_usepvn(sv, dc, needlen);
	dc = SvPVX(sv);		/* sv_usepvn() calls Renew() */
    }
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);
    if (left_utf || right_utf) {
	UV duc, luc, ruc;
	char *dcsave = dc;
	STRLEN lulen = leftlen;
	STRLEN rulen = rightlen;
	STRLEN ulen;

	switch (optype) {
	case OP_BIT_AND:
	    while (lulen && rulen) {
		luc = utf8_to_uv((U8*)lc, lulen, &ulen, UTF8_ALLOW_ANYUV);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8_to_uv((U8*)rc, rulen, &ulen, UTF8_ALLOW_ANYUV);
		rc += ulen;
		rulen -= ulen;
		duc = luc & ruc;
		dc = (char*)uv_to_utf8((U8*)dc, duc);
	    }
	    if (sv == left || sv == right)
		(void)sv_usepvn(sv, dcsave, needlen);
	    SvCUR_set(sv, dc - dcsave);
	    break;
	case OP_BIT_XOR:
	    while (lulen && rulen) {
		luc = utf8_to_uv((U8*)lc, lulen, &ulen, UTF8_ALLOW_ANYUV);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8_to_uv((U8*)rc, rulen, &ulen, UTF8_ALLOW_ANYUV);
		rc += ulen;
		rulen -= ulen;
		duc = luc ^ ruc;
		dc = (char*)uv_to_utf8((U8*)dc, duc);
	    }
	    goto mop_up_utf;
	case OP_BIT_OR:
	    while (lulen && rulen) {
		luc = utf8_to_uv((U8*)lc, lulen, &ulen, UTF8_ALLOW_ANYUV);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8_to_uv((U8*)rc, rulen, &ulen, UTF8_ALLOW_ANYUV);
		rc += ulen;
		rulen -= ulen;
		duc = luc | ruc;
		dc = (char*)uv_to_utf8((U8*)dc, duc);
	    }
	  mop_up_utf:
	    if (sv == left || sv == right)
		(void)sv_usepvn(sv, dcsave, needlen);
	    SvCUR_set(sv, dc - dcsave);
	    if (rulen)
		sv_catpvn(sv, rc, rulen);
	    else if (lulen)
		sv_catpvn(sv, lc, lulen);
	    else
		*SvEND(sv) = '\0';
	    break;
	}
	SvUTF8_on(sv);
	goto finish;
    }
    else
#ifdef LIBERAL
    if (len >= sizeof(long)*4 &&
	!((long)dc % sizeof(long)) &&
	!((long)lc % sizeof(long)) &&
	!((long)rc % sizeof(long)))	/* It's almost always aligned... */
    {
	I32 remainder = len % (sizeof(long)*4);
	len /= (sizeof(long)*4);

	dl = (long*)dc;
	ll = (long*)lc;
	rl = (long*)rc;

	switch (optype) {
	case OP_BIT_AND:
	    while (len--) {
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
		*dl++ = *ll++ & *rl++;
	    }
	    break;
	case OP_BIT_XOR:
	    while (len--) {
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
		*dl++ = *ll++ ^ *rl++;
	    }
	    break;
	case OP_BIT_OR:
	    while (len--) {
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
		*dl++ = *ll++ | *rl++;
	    }
	}

	dc = (char*)dl;
	lc = (char*)ll;
	rc = (char*)rl;

	len = remainder;
    }
#endif
    {
	switch (optype) {
	case OP_BIT_AND:
	    while (len--)
		*dc++ = *lc++ & *rc++;
	    break;
	case OP_BIT_XOR:
	    while (len--)
		*dc++ = *lc++ ^ *rc++;
	    goto mop_up;
	case OP_BIT_OR:
	    while (len--)
		*dc++ = *lc++ | *rc++;
	  mop_up:
	    len = lensave;
	    if (rightlen > len)
		sv_catpvn(sv, rsave + len, rightlen - len);
	    else if (leftlen > len)
		sv_catpvn(sv, lsave + len, leftlen - len);
	    else
		*SvEND(sv) = '\0';
	    break;
	}
    }
finish:
    SvTAINT(sv);
}

OP *
Perl_do_kv(pTHX)
{
    dSP;
    HV *hv = (HV*)POPs;
    HV *keys;
    register HE *entry;
    SV *tmpstr;
    I32 gimme = GIMME_V;
    I32 dokeys =   (PL_op->op_type == OP_KEYS);
    I32 dovalues = (PL_op->op_type == OP_VALUES);
    I32 realhv = (SvTYPE(hv) == SVt_PVHV);

    if (PL_op->op_type == OP_RV2HV || PL_op->op_type == OP_PADHV)
	dokeys = dovalues = TRUE;

    if (!hv) {
	if (PL_op->op_flags & OPf_MOD || LVRET) {	/* lvalue */
	    dTARGET;		/* make sure to clear its target here */
	    if (SvTYPE(TARG) == SVt_PVLV)
		LvTARG(TARG) = Nullsv;
	    PUSHs(TARG);
	}
	RETURN;
    }

    keys = realhv ? hv : avhv_keys((AV*)hv);
    (void)hv_iterinit(keys);	/* always reset iterator regardless */

    if (gimme == G_VOID)
	RETURN;

    if (gimme == G_SCALAR) {
	IV i;
	dTARGET;

	if (PL_op->op_flags & OPf_MOD || LVRET) {	/* lvalue */
	    if (SvTYPE(TARG) < SVt_PVLV) {
		sv_upgrade(TARG, SVt_PVLV);
		sv_magic(TARG, Nullsv, 'k', Nullch, 0);
	    }
	    LvTYPE(TARG) = 'k';
	    if (LvTARG(TARG) != (SV*)keys) {
		if (LvTARG(TARG))
		    SvREFCNT_dec(LvTARG(TARG));
		LvTARG(TARG) = SvREFCNT_inc(keys);
	    }
	    PUSHs(TARG);
	    RETURN;
	}

	if (! SvTIED_mg((SV*)keys, 'P'))
	    i = HvKEYS(keys);
	else {
	    i = 0;
	    /*SUPPRESS 560*/
	    while (hv_iternext(keys)) i++;
	}
	PUSHi( i );
	RETURN;
    }

    EXTEND(SP, HvKEYS(keys) * (dokeys + dovalues));

    PUTBACK;	/* hv_iternext and hv_iterval might clobber stack_sp */
    while ((entry = hv_iternext(keys))) {
	SPAGAIN;
	if (dokeys)
	    XPUSHs(hv_iterkeysv(entry));	/* won't clobber stack_sp */
	if (dovalues) {
	    PUTBACK;
	    tmpstr = realhv ?
		     hv_iterval(hv,entry) : avhv_iterval((AV*)hv,entry);
	    DEBUG_H(Perl_sv_setpvf(aTHX_ tmpstr, "%lu%%%d=%lu",
			    (unsigned long)HeHASH(entry),
			    HvMAX(keys)+1,
			    (unsigned long)(HeHASH(entry) & HvMAX(keys))));
	    SPAGAIN;
	    XPUSHs(tmpstr);
	}
	PUTBACK;
    }
    return NORMAL;
}

