/*    doop.c
 *
 *    Copyright (c) 1991-2000, Larry Wall
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
S_do_trans_CC_simple(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    I32 matches = 0;
    STRLEN len;
    short *tbl;
    I32 ch;

    tbl = (short*)cPVOP->op_pv;
    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans");

    s = (U8*)SvPV(sv, len);
    send = s + len;

    while (s < send) {
	if ((ch = tbl[*s]) >= 0) {
	    matches++;
	    *s = ch;
	}
	s++;
    }
    SvSETMAGIC(sv);

    return matches;
}

STATIC I32
S_do_trans_CC_count(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    I32 matches = 0;
    STRLEN len;
    short *tbl;

    tbl = (short*)cPVOP->op_pv;
    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans");

    s = (U8*)SvPV(sv, len);
    send = s + len;

    while (s < send) {
	if (tbl[*s] >= 0)
	    matches++;
	s++;
    }

    return matches;
}

STATIC I32
S_do_trans_CC_complex(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    I32 matches = 0;
    STRLEN len;
    short *tbl;
    I32 ch;

    tbl = (short*)cPVOP->op_pv;
    if (!tbl)
	Perl_croak(aTHX_ "panic: do_trans");

    s = (U8*)SvPV(sv, len);
    send = s + len;

    d = s;
    if (PL_op->op_private & OPpTRANS_SQUASH) {
	U8* p = send;

	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		*d = ch;
		matches++;
		if (p == d - 1 && *p == *d)
		    matches--;
		else
		    p = d++;
	    }
	    else if (ch == -1)		/* -1 is unmapped character */
		*d++ = *s;		/* -2 is delete character */
	    s++;
	}
    }
    else {
	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		*d = ch;
		matches++;
		d++;
	    }
	    else if (ch == -1)		/* -1 is unmapped character */
		*d++ = *s;		/* -2 is delete character */
	    s++;
	}
    }
    matches += send - d;	/* account for disappeared chars */
    *d = '\0';
    SvCUR_set(sv, d - (U8*)SvPVX(sv));
    SvSETMAGIC(sv);

    return matches;
}

STATIC I32
S_do_trans_UU_simple(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    I32 matches = 0;
    STRLEN len;

    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV extra = none + 1;
    UV final;
    UV uv;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    svp = hv_fetch(hv, "FINAL", 5, FALSE);
    if (svp)
	final = SvUV(*svp);

    d = s;
    while (s < send) {
	if ((uv = swash_fetch(rv, s)) < none) {
	    s += UTF8SKIP(s);
	    matches++;
	    d = uv_to_utf8(d, uv);
	}
	else if (uv == none) {
	    int i;
	    for (i = UTF8SKIP(s); i; i--)
		*d++ = *s++;
	}
	else if (uv == extra) {
	    s += UTF8SKIP(s);
	    matches++;
	    d = uv_to_utf8(d, final);
	}
	else
	    s += UTF8SKIP(s);
    }
    *d = '\0';
    SvCUR_set(sv, d - (U8*)SvPVX(sv));
    SvSETMAGIC(sv);

    return matches;
}

STATIC I32
S_do_trans_UU_count(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    I32 matches = 0;
    STRLEN len;

    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV uv;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    while (s < send) {
	if ((uv = swash_fetch(rv, s)) < none)
	    matches++;
	s += UTF8SKIP(s);
    }

    return matches;
}

STATIC I32
S_do_trans_UC_simple(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    I32 matches = 0;
    STRLEN len;

    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV extra = none + 1;
    UV final;
    UV uv;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    svp = hv_fetch(hv, "FINAL", 5, FALSE);
    if (svp)
	final = SvUV(*svp);

    d = s;
    while (s < send) {
	if ((uv = swash_fetch(rv, s)) < none) {
	    s += UTF8SKIP(s);
	    matches++;
	    *d++ = (U8)uv;
	}
	else if (uv == none) {
	    I32 ulen;
	    uv = utf8_to_uv(s, &ulen);
	    s += ulen;
	    *d++ = (U8)uv;
	}
	else if (uv == extra) {
	    s += UTF8SKIP(s);
	    matches++;
	    *d++ = (U8)final;
	}
	else
	    s += UTF8SKIP(s);
    }
    *d = '\0';
    SvCUR_set(sv, d - (U8*)SvPVX(sv));
    SvSETMAGIC(sv);

    return matches;
}

STATIC I32
S_do_trans_CU_simple(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    U8 *dst;
    I32 matches = 0;
    STRLEN len;

    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV extra = none + 1;
    UV final;
    UV uv;
    U8 tmpbuf[UTF8_MAXLEN];
    I32 bits = 16;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    svp = hv_fetch(hv, "BITS", 4, FALSE);
    if (svp)
	bits = (I32)SvIV(*svp);

    svp = hv_fetch(hv, "FINAL", 5, FALSE);
    if (svp)
	final = SvUV(*svp);

    Newz(801, d, len * (bits >> 3) + 1, U8);
    dst = d;

    while (s < send) {
	uv = *s++;
	if (uv < 0x80)
	    tmpbuf[0] = uv;
	else {
	    tmpbuf[0] = (( uv >>  6)         | 0xc0);
	    tmpbuf[1] = (( uv        & 0x3f) | 0x80);
	}

	if ((uv = swash_fetch(rv, tmpbuf)) < none) {
	    matches++;
	    d = uv_to_utf8(d, uv);
	}
	else if (uv == none)
	    d = uv_to_utf8(d, s[-1]);
	else if (uv == extra) {
	    matches++;
	    d = uv_to_utf8(d, final);
	}
    }
    *d = '\0';
    sv_usepvn_mg(sv, (char*)dst, d - dst);

    return matches;
}

/* utf-8 to latin-1 */

STATIC I32
S_do_trans_UC_trivial(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    STRLEN len;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    d = s;
    while (s < send) {
	if (*s < 0x80)
	    *d++ = *s++;
	else {
	    I32 ulen;
	    UV uv = utf8_to_uv(s, &ulen);
	    s += ulen;
	    *d++ = (U8)uv;
	}
    }
    *d = '\0';
    SvCUR_set(sv, d - (U8*)SvPVX(sv));
    SvSETMAGIC(sv);

    return SvCUR(sv);
}

/* latin-1 to utf-8 */

STATIC I32
S_do_trans_CU_trivial(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    U8 *dst;
    I32 matches;
    STRLEN len;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    Newz(801, d, len * 2 + 1, U8);
    dst = d;

    matches = send - s;

    while (s < send) {
	if (*s < 0x80)
	    *d++ = *s++;
	else {
	    UV uv = *s++;
	    *d++ = (( uv >>  6)         | 0xc0);
	    *d++ = (( uv        & 0x3f) | 0x80);
	}
    }
    *d = '\0';
    sv_usepvn_mg(sv, (char*)dst, d - dst);

    return matches;
}

STATIC I32
S_do_trans_UU_complex(pTHX_ SV *sv)
{
    dTHR;
    U8 *s;
    U8 *send;
    U8 *d;
    I32 matches = 0;
    I32 squash   = PL_op->op_private & OPpTRANS_SQUASH;
    I32 from_utf = PL_op->op_private & OPpTRANS_FROM_UTF;
    I32 to_utf   = PL_op->op_private & OPpTRANS_TO_UTF;
    I32 del      = PL_op->op_private & OPpTRANS_DELETE;
    SV* rv = (SV*)cSVOP->op_sv;
    HV* hv = (HV*)SvRV(rv);
    SV** svp = hv_fetch(hv, "NONE", 4, FALSE);
    UV none = svp ? SvUV(*svp) : 0x7fffffff;
    UV extra = none + 1;
    UV final;
    UV uv;
    STRLEN len;
    U8 *dst;

    s = (U8*)SvPV(sv, len);
    send = s + len;

    svp = hv_fetch(hv, "FINAL", 5, FALSE);
    if (svp)
	final = SvUV(*svp);

    if (PL_op->op_private & OPpTRANS_GROWS) {
	I32 bits = 16;

	svp = hv_fetch(hv, "BITS", 4, FALSE);
	if (svp)
	    bits = (I32)SvIV(*svp);

	Newz(801, d, len * (bits >> 3) + 1, U8);
	dst = d;
    }
    else {
	d = s;
	dst = 0;
    }

    if (squash) {
	UV puv = 0xfeedface;
	while (s < send) {
	    if (from_utf) {
		uv = swash_fetch(rv, s);
	    }
	    else {
		U8 tmpbuf[2];
		uv = *s++;
		if (uv < 0x80)
		    tmpbuf[0] = uv;
		else {
		    tmpbuf[0] = (( uv >>  6)         | 0xc0);
		    tmpbuf[1] = (( uv        & 0x3f) | 0x80);
		}
		uv = swash_fetch(rv, tmpbuf);
	    }
	    if (uv < none) {
		matches++;
		if (uv != puv) {
		    if (uv >= 0x80 && to_utf)
			d = uv_to_utf8(d, uv);
		    else
			*d++ = (U8)uv;
		    puv = uv;
		}
		if (from_utf)
		    s += UTF8SKIP(s);
		continue;
	    }
	    else if (uv == none) {	/* "none" is unmapped character */
		if (from_utf) {
		    if (*s < 0x80)
			*d++ = *s++;
		    else if (to_utf) {
			int i;
			for (i = UTF8SKIP(s); i; --i)
			    *d++ = *s++;
		    }
		    else {
			I32 ulen;
			*d++ = (U8)utf8_to_uv(s, &ulen);
			s += ulen;
		    }
		}
		else {	/* must be to_utf only */
		    d = uv_to_utf8(d, s[-1]);
		}
		puv = 0xfeedface;
		continue;
	    }
	    else if (uv == extra && !del) {
		matches++;
		if (uv != puv) {
		    if (final >= 0x80 && to_utf)
			d = uv_to_utf8(d, final);
		    else
			*d++ = (U8)final;
		    puv = final;
		}
		if (from_utf)
		    s += UTF8SKIP(s);
		continue;
	    }
	    matches++;		/* "none+1" is delete character */
	    if (from_utf)
		s += UTF8SKIP(s);
	}
    }
    else {
	while (s < send) {
	    if (from_utf) {
		uv = swash_fetch(rv, s);
	    }
	    else {
		U8 tmpbuf[2];
		uv = *s++;
		if (uv < 0x80)
		    tmpbuf[0] = uv;
		else {
		    tmpbuf[0] = (( uv >>  6)         | 0xc0);
		    tmpbuf[1] = (( uv        & 0x3f) | 0x80);
		}
		uv = swash_fetch(rv, tmpbuf);
	    }
	    if (uv < none) {
		matches++;
		if (uv >= 0x80 && to_utf)
		    d = uv_to_utf8(d, uv);
		else
		    *d++ = (U8)uv;
		if (from_utf)
		    s += UTF8SKIP(s);
		continue;
	    }
	    else if (uv == none) {	/* "none" is unmapped character */
		if (from_utf) {
		    if (*s < 0x80)
			*d++ = *s++;
		    else if (to_utf) {
			int i;
			for (i = UTF8SKIP(s); i; --i)
			    *d++ = *s++;
		    }
		    else {
			I32 ulen;
			*d++ = (U8)utf8_to_uv(s, &ulen);
			s += ulen;
		    }
		}
		else {	/* must be to_utf only */
		    d = uv_to_utf8(d, s[-1]);
		}
		continue;
	    }
	    else if (uv == extra && !del) {
		matches++;
		if (final >= 0x80 && to_utf)
		    d = uv_to_utf8(d, final);
		else
		    *d++ = (U8)final;
		if (from_utf)
		    s += UTF8SKIP(s);
		continue;
	    }
	    matches++;		/* "none+1" is delete character */
	    if (from_utf)
		s += UTF8SKIP(s);
	}
    }
    if (dst)
	sv_usepvn(sv, (char*)dst, d - dst);
    else {
	*d = '\0';
	SvCUR_set(sv, d - (U8*)SvPVX(sv));
    }
    SvSETMAGIC(sv);

    return matches;
}

I32
Perl_do_trans(pTHX_ SV *sv)
{
    dTHR;
    STRLEN len;

    if (SvREADONLY(sv) && !(PL_op->op_private & OPpTRANS_IDENTICAL))
	Perl_croak(aTHX_ PL_no_modify);

    (void)SvPV(sv, len);
    if (!len)
	return 0;
    if (!SvPOKp(sv))
	(void)SvPV_force(sv, len);
    (void)SvPOK_only(sv);

    DEBUG_t( Perl_deb(aTHX_ "2.TBL\n"));

    switch (PL_op->op_private & 63) {
    case 0:
	return do_trans_CC_simple(sv);

    case OPpTRANS_FROM_UTF:
	return do_trans_UC_simple(sv);

    case OPpTRANS_TO_UTF:
	return do_trans_CU_simple(sv);

    case OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF:
	return do_trans_UU_simple(sv);

    case OPpTRANS_IDENTICAL:
	return do_trans_CC_count(sv);

    case OPpTRANS_FROM_UTF|OPpTRANS_IDENTICAL:
	return do_trans_UC_trivial(sv);

    case OPpTRANS_TO_UTF|OPpTRANS_IDENTICAL:
	return do_trans_CU_trivial(sv);

    case OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF|OPpTRANS_IDENTICAL:
	return do_trans_UU_count(sv);

    default:
	if (PL_op->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF))
	    return do_trans_UU_complex(sv); /* could be UC or CU too */
	else
	    return do_trans_CC_complex(sv);
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
	    if (*mark && !SvGMAGICAL(*mark) && SvOK(*mark)) {
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
	char *s;

	if (*mark) {
	    s = SvPV(*mark, tmplen);
	    sv_setpvn(sv, s, tmplen);
	}
	else
	    sv_setpv(sv, "");
	mark++;
    }
    else
	sv_setpv(sv,"");
    len = delimlen;
    if (len) {
	for (; items > 0; items--,mark++) {
	    sv_catpvn(sv,delim,len);
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
		dTHR;
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
	    dTHR;
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
    lval = SvUV(sv);
    offset = LvTARGOFF(sv);
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
	    dTHR;
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
    dTHR;
    
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
	    while ((*s & 0xc0) == 0x80)
		--s;
	    if (UTF8SKIP(s) != send - s && ckWARN_d(WARN_UTF8))
		Perl_warner(aTHX_ WARN_UTF8, "Malformed UTF-8 character");
	    sv_setpvn(astr, s, send - s);
	    *s = '\0';
	    SvCUR_set(sv, s - start);
	    SvNIOK_off(sv);
	    SvUTF8_on(astr);
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
    dTHR;
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
    dTHR;	/* just for taint */
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

    if (left_utf && !right_utf)
	sv_utf8_upgrade(right);
    if (!left_utf && right_utf)
	sv_utf8_upgrade(left);

    if (sv != left || (optype != OP_BIT_AND && !SvOK(sv) && !SvGMAGICAL(sv)))
	sv_setpvn(sv, "", 0);	/* avoid undef warning on |= and ^= */
    lsave = lc = SvPV(left, leftlen);
    rsave = rc = SvPV(right, rightlen);
    len = leftlen < rightlen ? leftlen : rightlen;
    lensave = len;
    if (SvOK(sv) || SvTYPE(sv) > SVt_PVMG) {
	STRLEN n_a;
	dc = SvPV_force(sv, n_a);
	if (SvCUR(sv) < len) {
	    dc = SvGROW(sv, len + 1);
	    (void)memzero(dc + SvCUR(sv), len - SvCUR(sv) + 1);
	}
    }
    else {
	I32 needlen = ((optype == OP_BIT_AND)
			? len : (leftlen > rightlen ? leftlen : rightlen));
	Newz(801, dc, needlen + 1, char);
	(void)sv_usepvn(sv, dc, needlen);
	dc = SvPVX(sv);		/* sv_usepvn() calls Renew() */
    }
    SvCUR_set(sv, len);
    (void)SvPOK_only(sv);
    if (left_utf || right_utf) {
	UV duc, luc, ruc;
	STRLEN lulen = leftlen;
	STRLEN rulen = rightlen;
	STRLEN dulen = 0;
	I32 ulen;

	if (optype != OP_BIT_AND)
	    dc = SvGROW(sv, leftlen+rightlen+1);

	switch (optype) {
	case OP_BIT_AND:
	    while (lulen && rulen) {
		luc = utf8_to_uv((U8*)lc, &ulen);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8_to_uv((U8*)rc, &ulen);
		rc += ulen;
		rulen -= ulen;
		duc = luc & ruc;
		dc = (char*)uv_to_utf8((U8*)dc, duc);
	    }
	    dulen = dc - SvPVX(sv);
	    SvCUR_set(sv, dulen);
	    break;
	case OP_BIT_XOR:
	    while (lulen && rulen) {
		luc = utf8_to_uv((U8*)lc, &ulen);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8_to_uv((U8*)rc, &ulen);
		rc += ulen;
		rulen -= ulen;
		duc = luc ^ ruc;
		dc = (char*)uv_to_utf8((U8*)dc, duc);
	    }
	    goto mop_up_utf;
	case OP_BIT_OR:
	    while (lulen && rulen) {
		luc = utf8_to_uv((U8*)lc, &ulen);
		lc += ulen;
		lulen -= ulen;
		ruc = utf8_to_uv((U8*)rc, &ulen);
		rc += ulen;
		rulen -= ulen;
		duc = luc | ruc;
		dc = (char*)uv_to_utf8((U8*)dc, duc);
	    }
	  mop_up_utf:
	    dulen = dc - SvPVX(sv);
	    SvCUR_set(sv, dulen);
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
    djSP;
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
	if (PL_op->op_flags & OPf_MOD) {	/* lvalue */
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

	if (PL_op->op_flags & OPf_MOD) {	/* lvalue */
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

