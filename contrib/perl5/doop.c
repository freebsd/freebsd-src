/*    doop.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "'So that was the job I felt I had to do when I started,' thought Sam."
 */

#include "EXTERN.h"
#include "perl.h"

#if !defined(NSIG) || defined(M_UNIX) || defined(M_XENIX)
#include <signal.h>
#endif

I32
do_trans(SV *sv, OP *arg)
{
    dTHR;
    register short *tbl;
    register U8 *s;
    register U8 *send;
    register U8 *d;
    register I32 ch;
    register I32 matches = 0;
    register I32 squash = PL_op->op_private & OPpTRANS_SQUASH;
    register U8 *p;
    STRLEN len;

    if (SvREADONLY(sv) && !(PL_op->op_private & OPpTRANS_COUNTONLY))
	croak(no_modify);
    tbl = (short*)cPVOP->op_pv;
    s = (U8*)SvPV(sv, len);
    if (!len)
	return 0;
    if (!SvPOKp(sv))
	s = (U8*)SvPV_force(sv, len);
    (void)SvPOK_only(sv);
    send = s + len;
    if (!tbl || !s)
	croak("panic: do_trans");
    DEBUG_t( deb("2.TBL\n"));
    if (!PL_op->op_private) {
	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		matches++;
		*s = ch;
	    }
	    s++;
	}
	SvSETMAGIC(sv);
    }
    else if (PL_op->op_private & OPpTRANS_COUNTONLY) {
	while (s < send) {
	    if (tbl[*s] >= 0)
		matches++;
	    s++;
	}
    }
    else {
	d = s;
	p = send;
	while (s < send) {
	    if ((ch = tbl[*s]) >= 0) {
		*d = ch;
		matches++;
		if (squash) {
		    if (p == d - 1 && *p == *d)
			matches--;
		    else
		        p = d++;
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
	SvCUR_set(sv, d - (U8*)SvPVX(sv));
	SvSETMAGIC(sv);
    }
    return matches;
}

void
do_join(register SV *sv, SV *del, register SV **mark, register SV **sp)
{
    SV **oldmark = mark;
    register I32 items = sp - mark;
    register STRLEN len;
    STRLEN delimlen;
    register char *delim = SvPV(del, delimlen);
    STRLEN tmplen;

    mark++;
    len = (items > 0 ? (delimlen * (items - 1) ) : 0);
    if (SvTYPE(sv) < SVt_PV)
	sv_upgrade(sv, SVt_PV);
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
	items = sp - mark;;
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
do_sprintf(SV *sv, I32 len, SV **sarg)
{
    STRLEN patlen;
    char *pat = SvPV(*sarg, patlen);
    bool do_taint = FALSE;

    sv_vsetpvfn(sv, pat, patlen, Null(va_list*), sarg + 1, len - 1, &do_taint);
    SvSETMAGIC(sv);
    if (do_taint)
	SvTAINTED_on(sv);
}

void
do_vecset(SV *sv)
{
    SV *targ = LvTARG(sv);
    register I32 offset;
    register I32 size;
    register unsigned char *s;
    register unsigned long lval;
    I32 mask;
    STRLEN targlen;
    STRLEN len;

    if (!targ)
	return;
    s = (unsigned char*)SvPV_force(targ, targlen);
    lval = U_L(SvNV(sv));
    offset = LvTARGOFF(sv);
    size = LvTARGLEN(sv);
    
    len = (offset + size + 7) / 8;
    if (len > targlen) {
	s = (unsigned char*)SvGROW(targ, len + 1);
	(void)memzero(s + targlen, len - targlen + 1);
	SvCUR_set(targ, len);
    }
    
    if (size < 8) {
	mask = (1 << size) - 1;
	size = offset & 7;
	lval &= mask;
	offset >>= 3;
	s[offset] &= ~(mask << size);
	s[offset] |= lval << size;
    }
    else {
	offset >>= 3;
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
do_chop(register SV *astr, register SV *sv)
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
    if (SvTYPE(sv) == SVt_PVHV) {
        HV* hv = (HV*)sv;
	HE* entry;
        (void)hv_iterinit(hv);
        /*SUPPRESS 560*/
        while (entry = hv_iternext(hv))
            do_chop(astr,hv_iterval(hv,entry));
        return;
    }
    s = SvPV(sv, len);
    if (len && !SvPOK(sv))
	s = SvPV_force(sv, len);
    if (s && len) {
	s += --len;
	sv_setpvn(astr, s, 1);
	*s = '\0';
	SvCUR_set(sv, len);
	SvNIOK_off(sv);
    }
    else
	sv_setpvn(astr, "", 0);
    SvSETMAGIC(sv);
} 

I32
do_chomp(register SV *sv)
{
    dTHR;
    register I32 count;
    STRLEN len;
    char *s;

    if (RsSNARF(PL_rs))
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
    if (SvTYPE(sv) == SVt_PVHV) {
        HV* hv = (HV*)sv;
	HE* entry;
        (void)hv_iterinit(hv);
        /*SUPPRESS 560*/
        while (entry = hv_iternext(hv))
            count += do_chomp(hv_iterval(hv,entry));
        return count;
    }
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
do_vop(I32 optype, SV *sv, SV *left, SV *right)
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
    SvTAINT(sv);
}

OP *
do_kv(ARGSproto)
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
    while (entry = hv_iternext(keys)) {
	SPAGAIN;
	if (dokeys)
	    XPUSHs(hv_iterkeysv(entry));	/* won't clobber stack_sp */
	if (dovalues) {
	    tmpstr = sv_newmortal();
	    PUTBACK;
	    sv_setsv(tmpstr,realhv ?
		     hv_iterval(hv,entry) : avhv_iterval((AV*)hv,entry));
	    DEBUG_H(sv_setpvf(tmpstr, "%lu%%%d=%lu",
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

