/*    pp.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $FreeBSD$
 */

/*
 * "It's a big house this, and very peculiar.  Always a bit more to discover,
 * and no knowing what you'll find around a corner.  And Elves, sir!" --Samwise
 */

#include "EXTERN.h"
#define PERL_IN_PP_C
#include "perl.h"

/*
 * The compiler on Concurrent CX/UX systems has a subtle bug which only
 * seems to show up when compiling pp.c - it generates the wrong double
 * precision constant value for (double)UV_MAX when used inline in the body
 * of the code below, so this makes a static variable up front (which the
 * compiler seems to get correct) and uses it in place of UV_MAX below.
 */
#ifdef CXUX_BROKEN_CONSTANT_CONVERT
static double UV_MAX_cxux = ((double)UV_MAX);
#endif

/*
 * Offset for integer pack/unpack.
 *
 * On architectures where I16 and I32 aren't really 16 and 32 bits,
 * which for now are all Crays, pack and unpack have to play games.
 */

/*
 * These values are required for portability of pack() output.
 * If they're not right on your machine, then pack() and unpack()
 * wouldn't work right anyway; you'll need to apply the Cray hack.
 * (I'd like to check them with #if, but you can't use sizeof() in
 * the preprocessor.)  --???
 */
/*
    The appropriate SHORTSIZE, INTSIZE, LONGSIZE, and LONGLONGSIZE
    defines are now in config.h.  --Andy Dougherty  April 1998
 */
#define SIZE16 2
#define SIZE32 4

/* CROSSCOMPILE and MULTIARCH are going to affect pp_pack() and pp_unpack().
   --jhi Feb 1999 */

#if SHORTSIZE != SIZE16 || LONGSIZE != SIZE32
#   define PERL_NATINT_PACK
#endif

#if LONGSIZE > 4 && defined(_CRAY)
#  if BYTEORDER == 0x12345678
#    define OFF16(p)	(char*)(p)
#    define OFF32(p)	(char*)(p)
#  else
#    if BYTEORDER == 0x87654321
#      define OFF16(p)	((char*)(p) + (sizeof(U16) - SIZE16))
#      define OFF32(p)	((char*)(p) + (sizeof(U32) - SIZE32))
#    else
       }}}} bad cray byte order
#    endif
#  endif
#  define COPY16(s,p)  (*(p) = 0, Copy(s, OFF16(p), SIZE16, char))
#  define COPY32(s,p)  (*(p) = 0, Copy(s, OFF32(p), SIZE32, char))
#  define COPYNN(s,p,n) (*(p) = 0, Copy(s, (char *)(p), n, char))
#  define CAT16(sv,p)  sv_catpvn(sv, OFF16(p), SIZE16)
#  define CAT32(sv,p)  sv_catpvn(sv, OFF32(p), SIZE32)
#else
#  define COPY16(s,p)  Copy(s, p, SIZE16, char)
#  define COPY32(s,p)  Copy(s, p, SIZE32, char)
#  define COPYNN(s,p,n) Copy(s, (char *)(p), n, char)
#  define CAT16(sv,p)  sv_catpvn(sv, (char*)(p), SIZE16)
#  define CAT32(sv,p)  sv_catpvn(sv, (char*)(p), SIZE32)
#endif

/* variations on pp_null */

/* XXX I can't imagine anyone who doesn't have this actually _needs_
   it, since pid_t is an integral type.
   --AD  2/20/1998
*/
#ifdef NEED_GETPID_PROTO
extern Pid_t getpid (void);
#endif

PP(pp_stub)
{
    dSP;
    if (GIMME_V == G_SCALAR)
	XPUSHs(&PL_sv_undef);
    RETURN;
}

PP(pp_scalar)
{
    return NORMAL;
}

/* Pushy stuff. */

PP(pp_padav)
{
    dSP; dTARGET;
    if (PL_op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(PL_curpad[PL_op->op_targ]);
    EXTEND(SP, 1);
    if (PL_op->op_flags & OPf_REF) {
	PUSHs(TARG);
	RETURN;
    } else if (LVRET) {
	if (GIMME == G_SCALAR)
	    Perl_croak(aTHX_ "Can't return array to lvalue scalar context");
	PUSHs(TARG);
	RETURN;
    }
    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	EXTEND(SP, maxarg);
	if (SvMAGICAL(TARG)) {
	    U32 i;
	    for (i=0; i < maxarg; i++) {
		SV **svp = av_fetch((AV*)TARG, i, FALSE);
		SP[i+1] = (svp) ? *svp : &PL_sv_undef;
	    }
	}
	else {
	    Copy(AvARRAY((AV*)TARG), SP+1, maxarg, SV*);
	}
	SP += maxarg;
    }
    else {
	SV* sv = sv_newmortal();
	I32 maxarg = AvFILL((AV*)TARG) + 1;
	sv_setiv(sv, maxarg);
	PUSHs(sv);
    }
    RETURN;
}

PP(pp_padhv)
{
    dSP; dTARGET;
    I32 gimme;

    XPUSHs(TARG);
    if (PL_op->op_private & OPpLVAL_INTRO)
	SAVECLEARSV(PL_curpad[PL_op->op_targ]);
    if (PL_op->op_flags & OPf_REF)
	RETURN;
    else if (LVRET) {
	if (GIMME == G_SCALAR)
	    Perl_croak(aTHX_ "Can't return hash to lvalue scalar context");
	RETURN;
    }
    gimme = GIMME_V;
    if (gimme == G_ARRAY) {
	RETURNOP(do_kv());
    }
    else if (gimme == G_SCALAR) {
	SV* sv = sv_newmortal();
	if (HvFILL((HV*)TARG))
	    Perl_sv_setpvf(aTHX_ sv, "%ld/%ld",
		      (long)HvFILL((HV*)TARG), (long)HvMAX((HV*)TARG) + 1);
	else
	    sv_setiv(sv, 0);
	SETs(sv);
    }
    RETURN;
}

PP(pp_padany)
{
    DIE(aTHX_ "NOT IMPL LINE %d",__LINE__);
}

/* Translations. */

PP(pp_rv2gv)
{
    dSP; dTOPss;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF(to_gv);

	sv = SvRV(sv);
	if (SvTYPE(sv) == SVt_PVIO) {
	    GV *gv = (GV*) sv_newmortal();
	    gv_init(gv, 0, "", 0, 0);
	    GvIOp(gv) = (IO *)sv;
	    (void)SvREFCNT_inc(sv);
	    sv = (SV*) gv;
	}
	else if (SvTYPE(sv) != SVt_PVGV)
	    DIE(aTHX_ "Not a GLOB reference");
    }
    else {
	if (SvTYPE(sv) != SVt_PVGV) {
	    char *sym;
	    STRLEN len;

	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv) && sv != &PL_sv_undef) {
		/* If this is a 'my' scalar and flag is set then vivify 
		 * NI-S 1999/05/07
		 */ 
		if (PL_op->op_private & OPpDEREF) {
		    char *name;
		    GV *gv;
		    if (cUNOP->op_targ) {
			STRLEN len;
			SV *namesv = PL_curpad[cUNOP->op_targ];
			name = SvPV(namesv, len);
			gv = (GV*)NEWSV(0,0);
			gv_init(gv, CopSTASH(PL_curcop), name, len, 0);
		    }
		    else {
			name = CopSTASHPV(PL_curcop);
			gv = newGVgen(name);
		    }
		    sv_upgrade(sv, SVt_RV);
		    SvRV(sv) = (SV*)gv;
		    SvROK_on(sv);
		    SvSETMAGIC(sv);
		    goto wasref;
		}
		if (PL_op->op_flags & OPf_REF ||
		    PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_usym, "a symbol");
		if (ckWARN(WARN_UNINITIALIZED))
		    report_uninit();
		RETSETUNDEF;
	    }
	    sym = SvPV(sv,len);
	    if ((PL_op->op_flags & OPf_SPECIAL) &&
		!(PL_op->op_flags & OPf_MOD))
	    {
		sv = (SV*)gv_fetchpv(sym, FALSE, SVt_PVGV);
		if (!sv
		    && (!is_gv_magical(sym,len,0)
			|| !(sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVGV))))
		{
		    RETSETUNDEF;
		}
	    }
	    else {
		if (PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_symref, sym, "a symbol");
		sv = (SV*)gv_fetchpv(sym, TRUE, SVt_PVGV);
	    }
	}
    }
    if (PL_op->op_private & OPpLVAL_INTRO)
	save_gp((GV*)sv, !(PL_op->op_flags & OPf_SPECIAL));
    SETs(sv);
    RETURN;
}

PP(pp_rv2sv)
{
    dSP; dTOPss;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF(to_sv);

	sv = SvRV(sv);
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	case SVt_PVHV:
	case SVt_PVCV:
	    DIE(aTHX_ "Not a SCALAR reference");
	}
    }
    else {
	GV *gv = (GV*)sv;
	char *sym;
	STRLEN len;

	if (SvTYPE(gv) != SVt_PVGV) {
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		if (SvROK(sv))
		    goto wasref;
	    }
	    if (!SvOK(sv)) {
		if (PL_op->op_flags & OPf_REF ||
		    PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_usym, "a SCALAR");
		if (ckWARN(WARN_UNINITIALIZED))
		    report_uninit();
		RETSETUNDEF;
	    }
	    sym = SvPV(sv, len);
	    if ((PL_op->op_flags & OPf_SPECIAL) &&
		!(PL_op->op_flags & OPf_MOD))
	    {
		gv = (GV*)gv_fetchpv(sym, FALSE, SVt_PV);
		if (!gv
		    && (!is_gv_magical(sym,len,0)
			|| !(gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PV))))
		{
		    RETSETUNDEF;
		}
	    }
	    else {
		if (PL_op->op_private & HINT_STRICT_REFS)
		    DIE(aTHX_ PL_no_symref, sym, "a SCALAR");
		gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PV);
	    }
	}
	sv = GvSV(gv);
    }
    if (PL_op->op_flags & OPf_MOD) {
	if (PL_op->op_private & OPpLVAL_INTRO)
	    sv = save_scalar((GV*)TOPs);
	else if (PL_op->op_private & OPpDEREF)
	    vivify_ref(sv, PL_op->op_private & OPpDEREF);
    }
    SETs(sv);
    RETURN;
}

PP(pp_av2arylen)
{
    dSP;
    AV *av = (AV*)TOPs;
    SV *sv = AvARYLEN(av);
    if (!sv) {
	AvARYLEN(av) = sv = NEWSV(0,0);
	sv_upgrade(sv, SVt_IV);
	sv_magic(sv, (SV*)av, '#', Nullch, 0);
    }
    SETs(sv);
    RETURN;
}

PP(pp_pos)
{
    dSP; dTARGET; dPOPss;

    if (PL_op->op_flags & OPf_MOD || LVRET) {
	if (SvTYPE(TARG) < SVt_PVLV) {
	    sv_upgrade(TARG, SVt_PVLV);
	    sv_magic(TARG, Nullsv, '.', Nullch, 0);
	}

	LvTYPE(TARG) = '.';
	if (LvTARG(TARG) != sv) {
	    if (LvTARG(TARG))
		SvREFCNT_dec(LvTARG(TARG));
	    LvTARG(TARG) = SvREFCNT_inc(sv);
	}
	PUSHs(TARG);	/* no SvSETMAGIC */
	RETURN;
    }
    else {
	MAGIC* mg;

	if (SvTYPE(sv) >= SVt_PVMG && SvMAGIC(sv)) {
	    mg = mg_find(sv, 'g');
	    if (mg && mg->mg_len >= 0) {
		I32 i = mg->mg_len;
		if (DO_UTF8(sv))
		    sv_pos_b2u(sv, &i);
		PUSHi(i + PL_curcop->cop_arybase);
		RETURN;
	    }
	}
	RETPUSHUNDEF;
    }
}

PP(pp_rv2cv)
{
    dSP;
    GV *gv;
    HV *stash;

    /* We usually try to add a non-existent subroutine in case of AUTOLOAD. */
    /* (But not in defined().) */
    CV *cv = sv_2cv(TOPs, &stash, &gv, !(PL_op->op_flags & OPf_SPECIAL));
    if (cv) {
	if (CvCLONE(cv))
	    cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
	if ((PL_op->op_private & OPpLVAL_INTRO)) {
	    if (gv && GvCV(gv) == cv && (gv = gv_autoload4(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv), FALSE)))
		cv = GvCV(gv);
	    if (!CvLVALUE(cv))
		DIE(aTHX_ "Can't modify non-lvalue subroutine call");
	}
    }
    else
	cv = (CV*)&PL_sv_undef;
    SETs((SV*)cv);
    RETURN;
}

PP(pp_prototype)
{
    dSP;
    CV *cv;
    HV *stash;
    GV *gv;
    SV *ret;

    ret = &PL_sv_undef;
    if (SvPOK(TOPs) && SvCUR(TOPs) >= 7) {
	char *s = SvPVX(TOPs);
	if (strnEQ(s, "CORE::", 6)) {
	    int code;
	    
	    code = keyword(s + 6, SvCUR(TOPs) - 6);
	    if (code < 0) {	/* Overridable. */
#define MAX_ARGS_OP ((sizeof(I32) - 1) * 2)
		int i = 0, n = 0, seen_question = 0;
		I32 oa;
		char str[ MAX_ARGS_OP * 2 + 2 ]; /* One ';', one '\0' */

		while (i < MAXO) {	/* The slow way. */
		    if (strEQ(s + 6, PL_op_name[i])
			|| strEQ(s + 6, PL_op_desc[i]))
		    {
			goto found;
		    }
		    i++;
		}
		goto nonesuch;		/* Should not happen... */
	      found:
		oa = PL_opargs[i] >> OASHIFT;
		while (oa) {
		    if (oa & OA_OPTIONAL) {
			seen_question = 1;
			str[n++] = ';';
		    }
		    else if (n && str[0] == ';' && seen_question) 
			goto set;	/* XXXX system, exec */
		    if ((oa & (OA_OPTIONAL - 1)) >= OA_AVREF 
			&& (oa & (OA_OPTIONAL - 1)) <= OA_HVREF) {
			str[n++] = '\\';
		    }
		    /* What to do with R ((un)tie, tied, (sys)read, recv)? */
		    str[n++] = ("?$@@%&*$")[oa & (OA_OPTIONAL - 1)];
		    oa = oa >> 4;
		}
		str[n++] = '\0';
		ret = sv_2mortal(newSVpvn(str, n - 1));
	    }
	    else if (code)		/* Non-Overridable */
		goto set;
	    else {			/* None such */
	      nonesuch:
		DIE(aTHX_ "Can't find an opnumber for \"%s\"", s+6);
	    }
	}
    }
    cv = sv_2cv(TOPs, &stash, &gv, FALSE);
    if (cv && SvPOK(cv))
	ret = sv_2mortal(newSVpvn(SvPVX(cv), SvCUR(cv)));
  set:
    SETs(ret);
    RETURN;
}

PP(pp_anoncode)
{
    dSP;
    CV* cv = (CV*)PL_curpad[PL_op->op_targ];
    if (CvCLONE(cv))
	cv = (CV*)sv_2mortal((SV*)cv_clone(cv));
    EXTEND(SP,1);
    PUSHs((SV*)cv);
    RETURN;
}

PP(pp_srefgen)
{
    dSP;
    *SP = refto(*SP);
    RETURN;
}

PP(pp_refgen)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;
	else
	    *MARK = &PL_sv_undef;
	*MARK = refto(*MARK);
	SP = MARK;
	RETURN;
    }
    EXTEND_MORTAL(SP - MARK);
    while (++MARK <= SP)
	*MARK = refto(*MARK);
    RETURN;
}

STATIC SV*
S_refto(pTHX_ SV *sv)
{
    SV* rv;

    if (SvTYPE(sv) == SVt_PVLV && LvTYPE(sv) == 'y') {
	if (LvTARGLEN(sv))
	    vivify_defelem(sv);
	if (!(sv = LvTARG(sv)))
	    sv = &PL_sv_undef;
	else
	    (void)SvREFCNT_inc(sv);
    }
    else if (SvTYPE(sv) == SVt_PVAV) {
	if (!AvREAL((AV*)sv) && AvREIFY((AV*)sv))
	    av_reify((AV*)sv);
	SvTEMP_off(sv);
	(void)SvREFCNT_inc(sv);
    }
    else if (SvPADTMP(sv))
	sv = newSVsv(sv);
    else {
	SvTEMP_off(sv);
	(void)SvREFCNT_inc(sv);
    }
    rv = sv_newmortal();
    sv_upgrade(rv, SVt_RV);
    SvRV(rv) = sv;
    SvROK_on(rv);
    return rv;
}

PP(pp_ref)
{
    dSP; dTARGET;
    SV *sv;
    char *pv;

    sv = POPs;

    if (sv && SvGMAGICAL(sv))
	mg_get(sv);

    if (!sv || !SvROK(sv))
	RETPUSHNO;

    sv = SvRV(sv);
    pv = sv_reftype(sv,TRUE);
    PUSHp(pv, strlen(pv));
    RETURN;
}

PP(pp_bless)
{
    dSP;
    HV *stash;

    if (MAXARG == 1)
	stash = CopSTASH(PL_curcop);
    else {
	SV *ssv = POPs;
	STRLEN len;
	char *ptr = SvPV(ssv,len);
	if (ckWARN(WARN_MISC) && len == 0)
	    Perl_warner(aTHX_ WARN_MISC, 
		   "Explicit blessing to '' (assuming package main)");
	stash = gv_stashpvn(ptr, len, TRUE);
    }

    (void)sv_bless(TOPs, stash);
    RETURN;
}

PP(pp_gelem)
{
    GV *gv;
    SV *sv;
    SV *tmpRef;
    char *elem;
    dSP;
    STRLEN n_a;
 
    sv = POPs;
    elem = SvPV(sv, n_a);
    gv = (GV*)POPs;
    tmpRef = Nullsv;
    sv = Nullsv;
    switch (elem ? *elem : '\0')
    {
    case 'A':
	if (strEQ(elem, "ARRAY"))
	    tmpRef = (SV*)GvAV(gv);
	break;
    case 'C':
	if (strEQ(elem, "CODE"))
	    tmpRef = (SV*)GvCVu(gv);
	break;
    case 'F':
	if (strEQ(elem, "FILEHANDLE")) /* XXX deprecate in 5.005 */
	    tmpRef = (SV*)GvIOp(gv);
	break;
    case 'G':
	if (strEQ(elem, "GLOB"))
	    tmpRef = (SV*)gv;
	break;
    case 'H':
	if (strEQ(elem, "HASH"))
	    tmpRef = (SV*)GvHV(gv);
	break;
    case 'I':
	if (strEQ(elem, "IO"))
	    tmpRef = (SV*)GvIOp(gv);
	break;
    case 'N':
	if (strEQ(elem, "NAME"))
	    sv = newSVpvn(GvNAME(gv), GvNAMELEN(gv));
	break;
    case 'P':
	if (strEQ(elem, "PACKAGE"))
	    sv = newSVpv(HvNAME(GvSTASH(gv)), 0);
	break;
    case 'S':
	if (strEQ(elem, "SCALAR"))
	    tmpRef = GvSV(gv);
	break;
    }
    if (tmpRef)
	sv = newRV(tmpRef);
    if (sv)
	sv_2mortal(sv);
    else
	sv = &PL_sv_undef;
    XPUSHs(sv);
    RETURN;
}

/* Pattern matching */

PP(pp_study)
{
    dSP; dPOPss;
    register unsigned char *s;
    register I32 pos;
    register I32 ch;
    register I32 *sfirst;
    register I32 *snext;
    STRLEN len;

    if (sv == PL_lastscream) {
	if (SvSCREAM(sv))
	    RETPUSHYES;
    }
    else {
	if (PL_lastscream) {
	    SvSCREAM_off(PL_lastscream);
	    SvREFCNT_dec(PL_lastscream);
	}
	PL_lastscream = SvREFCNT_inc(sv);
    }

    s = (unsigned char*)(SvPV(sv, len));
    pos = len;
    if (pos <= 0)
	RETPUSHNO;
    if (pos > PL_maxscream) {
	if (PL_maxscream < 0) {
	    PL_maxscream = pos + 80;
	    New(301, PL_screamfirst, 256, I32);
	    New(302, PL_screamnext, PL_maxscream, I32);
	}
	else {
	    PL_maxscream = pos + pos / 4;
	    Renew(PL_screamnext, PL_maxscream, I32);
	}
    }

    sfirst = PL_screamfirst;
    snext = PL_screamnext;

    if (!sfirst || !snext)
	DIE(aTHX_ "do_study: out of memory");

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
    }

    SvSCREAM_on(sv);
    sv_magic(sv, Nullsv, 'g', Nullch, 0);	/* piggyback on m//g magic */
    RETPUSHYES;
}

PP(pp_trans)
{
    dSP; dTARG;
    SV *sv;

    if (PL_op->op_flags & OPf_STACKED)
	sv = POPs;
    else {
	sv = DEFSV;
	EXTEND(SP,1);
    }
    TARG = sv_newmortal();
    PUSHi(do_trans(sv));
    RETURN;
}

/* Lvalue operators. */

PP(pp_schop)
{
    dSP; dTARGET;
    do_chop(TARG, TOPs);
    SETTARG;
    RETURN;
}

PP(pp_chop)
{
    dSP; dMARK; dTARGET; dORIGMARK;
    while (MARK < SP)
	do_chop(TARG, *++MARK);
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

PP(pp_schomp)
{
    dSP; dTARGET;
    SETi(do_chomp(TOPs));
    RETURN;
}

PP(pp_chomp)
{
    dSP; dMARK; dTARGET;
    register I32 count = 0;

    while (SP > MARK)
	count += do_chomp(POPs);
    PUSHi(count);
    RETURN;
}

PP(pp_defined)
{
    dSP;
    register SV* sv;

    sv = POPs;
    if (!sv || !SvANY(sv))
	RETPUSHNO;
    switch (SvTYPE(sv)) {
    case SVt_PVAV:
	if (AvMAX(sv) >= 0 || SvGMAGICAL(sv) || (SvRMAGICAL(sv) && mg_find(sv,'P')))
	    RETPUSHYES;
	break;
    case SVt_PVHV:
	if (HvARRAY(sv) || SvGMAGICAL(sv) || (SvRMAGICAL(sv) && mg_find(sv,'P')))
	    RETPUSHYES;
	break;
    case SVt_PVCV:
	if (CvROOT(sv) || CvXSUB(sv))
	    RETPUSHYES;
	break;
    default:
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvOK(sv))
	    RETPUSHYES;
    }
    RETPUSHNO;
}

PP(pp_undef)
{
    dSP;
    SV *sv;

    if (!PL_op->op_private) {
	EXTEND(SP, 1);
	RETPUSHUNDEF;
    }

    sv = POPs;
    if (!sv)
	RETPUSHUNDEF;

    if (SvTHINKFIRST(sv))
	sv_force_normal(sv);

    switch (SvTYPE(sv)) {
    case SVt_NULL:
	break;
    case SVt_PVAV:
	av_undef((AV*)sv);
	break;
    case SVt_PVHV:
	hv_undef((HV*)sv);
	break;
    case SVt_PVCV:
	if (ckWARN(WARN_MISC) && cv_const_sv((CV*)sv))
	    Perl_warner(aTHX_ WARN_MISC, "Constant subroutine %s undefined",
		 CvANON((CV*)sv) ? "(anonymous)" : GvENAME(CvGV((CV*)sv)));
	/* FALL THROUGH */
    case SVt_PVFM:
	{
	    /* let user-undef'd sub keep its identity */
	    GV* gv = CvGV((CV*)sv);
	    cv_undef((CV*)sv);
	    CvGV((CV*)sv) = gv;
	}
	break;
    case SVt_PVGV:
	if (SvFAKE(sv))
	    SvSetMagicSV(sv, &PL_sv_undef);
	else {
	    GP *gp;
	    gp_free((GV*)sv);
	    Newz(602, gp, 1, GP);
	    GvGP(sv) = gp_ref(gp);
	    GvSV(sv) = NEWSV(72,0);
	    GvLINE(sv) = CopLINE(PL_curcop);
	    GvEGV(sv) = (GV*)sv;
	    GvMULTI_on(sv);
	}
	break;
    default:
	if (SvTYPE(sv) >= SVt_PV && SvPVX(sv) && SvLEN(sv)) {
	    (void)SvOOK_off(sv);
	    Safefree(SvPVX(sv));
	    SvPV_set(sv, Nullch);
	    SvLEN_set(sv, 0);
	}
	(void)SvOK_off(sv);
	SvSETMAGIC(sv);
    }

    RETPUSHUNDEF;
}

PP(pp_predec)
{
    dSP;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    if (SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MIN)
    {
	--SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_postinc)
{
    dSP; dTARGET;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    sv_setsv(TARG, TOPs);
    if (SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MAX)
    {
	++SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    if (!SvOK(TARG))
	sv_setiv(TARG, 0);
    SETs(TARG);
    return NORMAL;
}

PP(pp_postdec)
{
    dSP; dTARGET;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    sv_setsv(TARG, TOPs);
    if (SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MIN)
    {
	--SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_dec(TOPs);
    SvSETMAGIC(TOPs);
    SETs(TARG);
    return NORMAL;
}

/* Ordinary operators. */

PP(pp_pow)
{
    dSP; dATARGET; tryAMAGICbin(pow,opASSIGN);
    {
      dPOPTOPnnrl;
      SETn( Perl_pow( left, right) );
      RETURN;
    }
}

PP(pp_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN);
    {
      dPOPTOPnnrl;
      SETn( left * right );
      RETURN;
    }
}

PP(pp_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN);
    {
      dPOPPOPnnrl;
      NV value;
      if (right == 0.0)
	DIE(aTHX_ "Illegal division by zero");
#ifdef SLOPPYDIVIDE
      /* insure that 20./5. == 4. */
      {
	IV k;
	if ((NV)I_V(left)  == left &&
	    (NV)I_V(right) == right &&
	    (k = I_V(left)/I_V(right))*I_V(right) == I_V(left)) {
	    value = k;
	}
	else {
	    value = left / right;
	}
      }
#else
      value = left / right;
#endif
      PUSHn( value );
      RETURN;
    }
}

PP(pp_modulo)
{
    dSP; dATARGET; tryAMAGICbin(modulo,opASSIGN);
    {
	UV left;
	UV right;
	bool left_neg;
	bool right_neg;
	bool use_double = 0;
	NV dright;
	NV dleft;

	if (SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)) {
	    IV i = SvIVX(POPs);
	    right = (right_neg = (i < 0)) ? -i : i;
	}
	else {
	    dright = POPn;
	    use_double = 1;
	    right_neg = dright < 0;
	    if (right_neg)
		dright = -dright;
	}

	if (!use_double && SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs)) {
	    IV i = SvIVX(POPs);
	    left = (left_neg = (i < 0)) ? -i : i;
	}
	else {
	    dleft = POPn;
	    if (!use_double) {
		use_double = 1;
		dright = right;
	    }
	    left_neg = dleft < 0;
	    if (left_neg)
		dleft = -dleft;
	}

	if (use_double) {
	    NV dans;

#if 1
/* Somehow U_V is pessimized even if CASTFLAGS is 0 */
#  if CASTFLAGS & 2
#    define CAST_D2UV(d) U_V(d)
#  else
#    define CAST_D2UV(d) ((UV)(d))
#  endif
	    /* Tried to do this only in the case DOUBLESIZE <= UV_SIZE,
	     * or, in other words, precision of UV more than of NV.
	     * But in fact the approach below turned out to be an
	     * optimization - floor() may be slow */
	    if (dright <= UV_MAX && dleft <= UV_MAX) {
		right = CAST_D2UV(dright);
		left  = CAST_D2UV(dleft);
		goto do_uv;
	    }
#endif

	    /* Backward-compatibility clause: */
	    dright = Perl_floor(dright + 0.5);
	    dleft  = Perl_floor(dleft + 0.5);

	    if (!dright)
		DIE(aTHX_ "Illegal modulus zero");

	    dans = Perl_fmod(dleft, dright);
	    if ((left_neg != right_neg) && dans)
		dans = dright - dans;
	    if (right_neg)
		dans = -dans;
	    sv_setnv(TARG, dans);
	}
	else {
	    UV ans;

	do_uv:
	    if (!right)
		DIE(aTHX_ "Illegal modulus zero");

	    ans = left % right;
	    if ((left_neg != right_neg) && ans)
		ans = right - ans;
	    if (right_neg) {
		/* XXX may warn: unary minus operator applied to unsigned type */
		/* could change -foo to be (~foo)+1 instead	*/
		if (ans <= ~((UV)IV_MAX)+1)
		    sv_setiv(TARG, ~ans+1);
		else
		    sv_setnv(TARG, -(NV)ans);
	    }
	    else
		sv_setuv(TARG, ans);
	}
	PUSHTARG;
	RETURN;
    }
}

PP(pp_repeat)
{
  dSP; dATARGET; tryAMAGICbin(repeat,opASSIGN);
  {
    register IV count = POPi;
    if (GIMME == G_ARRAY && PL_op->op_private & OPpREPEAT_DOLIST) {
	dMARK;
	I32 items = SP - MARK;
	I32 max;

	max = items * count;
	MEXTEND(MARK, max);
	if (count > 1) {
	    while (SP > MARK) {
		if (*SP)
		    SvTEMP_off((*SP));
		SP--;
	    }
	    MARK++;
	    repeatcpy((char*)(MARK + items), (char*)MARK,
		items * sizeof(SV*), count - 1);
	    SP += max;
	}
	else if (count <= 0)
	    SP -= items;
    }
    else {	/* Note: mark already snarfed by pp_list */
	SV *tmpstr = POPs;
	STRLEN len;
	bool isutf;

	SvSetSV(TARG, tmpstr);
	SvPV_force(TARG, len);
	isutf = DO_UTF8(TARG);
	if (count != 1) {
	    if (count < 1)
		SvCUR_set(TARG, 0);
	    else {
		SvGROW(TARG, (count * len) + 1);
		repeatcpy(SvPVX(TARG) + len, SvPVX(TARG), len, count - 1);
		SvCUR(TARG) *= count;
	    }
	    *SvEND(TARG) = '\0';
	}
	if (isutf)
	    (void)SvPOK_only_UTF8(TARG);
	else
	    (void)SvPOK_only(TARG);
	PUSHTARG;
    }
    RETURN;
  }
}

PP(pp_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN);
    {
      dPOPTOPnnrl_ul;
      SETn( left - right );
      RETURN;
    }
}

PP(pp_left_shift)
{
    dSP; dATARGET; tryAMAGICbin(lshift,opASSIGN);
    {
      IV shift = POPi;
      if (PL_op->op_private & HINT_INTEGER) {
	IV i = TOPi;
	SETi(i << shift);
      }
      else {
	UV u = TOPu;
	SETu(u << shift);
      }
      RETURN;
    }
}

PP(pp_right_shift)
{
    dSP; dATARGET; tryAMAGICbin(rshift,opASSIGN);
    {
      IV shift = POPi;
      if (PL_op->op_private & HINT_INTEGER) {
	IV i = TOPi;
	SETi(i >> shift);
      }
      else {
	UV u = TOPu;
	SETu(u >> shift);
      }
      RETURN;
    }
}

PP(pp_lt)
{
    dSP; tryAMAGICbinSET(lt,0);
    {
      dPOPnv;
      SETs(boolSV(TOPn < value));
      RETURN;
    }
}

PP(pp_gt)
{
    dSP; tryAMAGICbinSET(gt,0);
    {
      dPOPnv;
      SETs(boolSV(TOPn > value));
      RETURN;
    }
}

PP(pp_le)
{
    dSP; tryAMAGICbinSET(le,0);
    {
      dPOPnv;
      SETs(boolSV(TOPn <= value));
      RETURN;
    }
}

PP(pp_ge)
{
    dSP; tryAMAGICbinSET(ge,0);
    {
      dPOPnv;
      SETs(boolSV(TOPn >= value));
      RETURN;
    }
}

PP(pp_ne)
{
    dSP; tryAMAGICbinSET(ne,0);
    {
      dPOPnv;
      SETs(boolSV(TOPn != value));
      RETURN;
    }
}

PP(pp_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0);
    {
      dPOPTOPnnrl;
      I32 value;

#ifdef Perl_isnan
      if (Perl_isnan(left) || Perl_isnan(right)) {
	  SETs(&PL_sv_undef);
	  RETURN;
       }
      value = (left > right) - (left < right);
#else
      if (left == right)
	value = 0;
      else if (left < right)
	value = -1;
      else if (left > right)
	value = 1;
      else {
	SETs(&PL_sv_undef);
	RETURN;
      }
#endif
      SETi(value);
      RETURN;
    }
}

PP(pp_slt)
{
    dSP; tryAMAGICbinSET(slt,0);
    {
      dPOPTOPssrl;
      int cmp = ((PL_op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp < 0));
      RETURN;
    }
}

PP(pp_sgt)
{
    dSP; tryAMAGICbinSET(sgt,0);
    {
      dPOPTOPssrl;
      int cmp = ((PL_op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp > 0));
      RETURN;
    }
}

PP(pp_sle)
{
    dSP; tryAMAGICbinSET(sle,0);
    {
      dPOPTOPssrl;
      int cmp = ((PL_op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp <= 0));
      RETURN;
    }
}

PP(pp_sge)
{
    dSP; tryAMAGICbinSET(sge,0);
    {
      dPOPTOPssrl;
      int cmp = ((PL_op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETs(boolSV(cmp >= 0));
      RETURN;
    }
}

PP(pp_seq)
{
    dSP; tryAMAGICbinSET(seq,0);
    {
      dPOPTOPssrl;
      SETs(boolSV(sv_eq(left, right)));
      RETURN;
    }
}

PP(pp_sne)
{
    dSP; tryAMAGICbinSET(sne,0);
    {
      dPOPTOPssrl;
      SETs(boolSV(!sv_eq(left, right)));
      RETURN;
    }
}

PP(pp_scmp)
{
    dSP; dTARGET;  tryAMAGICbin(scmp,0);
    {
      dPOPTOPssrl;
      int cmp = ((PL_op->op_private & OPpLOCALE)
		 ? sv_cmp_locale(left, right)
		 : sv_cmp(left, right));
      SETi( cmp );
      RETURN;
    }
}

PP(pp_bit_and)
{
    dSP; dATARGET; tryAMAGICbin(band,opASSIGN);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = SvIV(left) & SvIV(right);
	  SETi(i);
	}
	else {
	  UV u = SvUV(left) & SvUV(right);
	  SETu(u);
	}
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_xor)
{
    dSP; dATARGET; tryAMAGICbin(bxor,opASSIGN);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = (USE_LEFT(left) ? SvIV(left) : 0) ^ SvIV(right);
	  SETi(i);
	}
	else {
	  UV u = (USE_LEFT(left) ? SvUV(left) : 0) ^ SvUV(right);
	  SETu(u);
	}
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_bit_or)
{
    dSP; dATARGET; tryAMAGICbin(bor,opASSIGN);
    {
      dPOPTOPssrl;
      if (SvNIOKp(left) || SvNIOKp(right)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = (USE_LEFT(left) ? SvIV(left) : 0) | SvIV(right);
	  SETi(i);
	}
	else {
	  UV u = (USE_LEFT(left) ? SvUV(left) : 0) | SvUV(right);
	  SETu(u);
	}
      }
      else {
	do_vop(PL_op->op_type, TARG, left, right);
	SETTARG;
      }
      RETURN;
    }
}

PP(pp_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    {
	dTOPss;
	if (SvGMAGICAL(sv))
	    mg_get(sv);
	if (SvIOKp(sv) && !SvNOKp(sv) && !SvPOKp(sv)) {
	    if (SvIsUV(sv)) {
		if (SvIVX(sv) == IV_MIN) {
		    SETi(SvIVX(sv));	/* special case: -((UV)IV_MAX+1) == IV_MIN */
		    RETURN;
		}
		else if (SvUVX(sv) <= IV_MAX) {
		    SETi(-SvIVX(sv));
		    RETURN;
		}
	    }
	    else if (SvIVX(sv) != IV_MIN) {
		SETi(-SvIVX(sv));
		RETURN;
	    }
	}
	if (SvNIOKp(sv))
	    SETn(-SvNV(sv));
	else if (SvPOKp(sv)) {
	    STRLEN len;
	    char *s = SvPV(sv, len);
	    if (isIDFIRST(*s)) {
		sv_setpvn(TARG, "-", 1);
		sv_catsv(TARG, sv);
	    }
	    else if (*s == '+' || *s == '-') {
		sv_setsv(TARG, sv);
		*SvPV_force(TARG, len) = *s == '-' ? '+' : '-';
	    }
	    else if (DO_UTF8(sv) && UTF8_IS_START(*s) && isIDFIRST_utf8((U8*)s)) {
		sv_setpvn(TARG, "-", 1);
		sv_catsv(TARG, sv);
	    }
	    else
		sv_setnv(TARG, -SvNV(sv));
	    SETTARG;
	}
	else
	    SETn(-SvNV(sv));
    }
    RETURN;
}

PP(pp_not)
{
    dSP; tryAMAGICunSET(not);
    *PL_stack_sp = boolSV(!SvTRUE(*PL_stack_sp));
    return NORMAL;
}

PP(pp_complement)
{
    dSP; dTARGET; tryAMAGICun(compl);
    {
      dTOPss;
      if (SvNIOKp(sv)) {
	if (PL_op->op_private & HINT_INTEGER) {
	  IV i = ~SvIV(sv);
	  SETi(i);
	}
	else {
	  UV u = ~SvUV(sv);
	  SETu(u);
	}
      }
      else {
	register U8 *tmps;
	register I32 anum;
	STRLEN len;

	SvSetSV(TARG, sv);
	tmps = (U8*)SvPV_force(TARG, len);
	anum = len;
	if (SvUTF8(TARG)) {
	  /* Calculate exact length, let's not estimate. */
	  STRLEN targlen = 0;
	  U8 *result;
	  U8 *send;
	  STRLEN l;
	  UV nchar = 0;
	  UV nwide = 0;

	  send = tmps + len;
	  while (tmps < send) {
	    UV c = utf8_to_uv(tmps, send-tmps, &l, UTF8_ALLOW_ANYUV);
	    tmps += UTF8SKIP(tmps);
	    targlen += UNISKIP(~c);
	    nchar++;
	    if (c > 0xff)
		nwide++;
	  }

	  /* Now rewind strings and write them. */
	  tmps -= len;

	  if (nwide) {
	      Newz(0, result, targlen + 1, U8);
	      while (tmps < send) {
		  UV c = utf8_to_uv(tmps, send-tmps, &l, UTF8_ALLOW_ANYUV);
		  tmps += UTF8SKIP(tmps);
		  result = uv_to_utf8(result, ~c);
	      }
	      *result = '\0';
	      result -= targlen;
	      sv_setpvn(TARG, (char*)result, targlen);
	      SvUTF8_on(TARG);
	  }
	  else {
	      Newz(0, result, nchar + 1, U8);
	      while (tmps < send) {
		  U8 c = (U8)utf8_to_uv(tmps, 0, &l, UTF8_ALLOW_ANY);
		  tmps += UTF8SKIP(tmps);
		  *result++ = ~c;
	      }
	      *result = '\0';
	      result -= nchar;
	      sv_setpvn(TARG, (char*)result, nchar);
	  }
	  Safefree(result);
	  SETs(TARG);
	  RETURN;
	}
#ifdef LIBERAL
	{
	    register long *tmpl;
	    for ( ; anum && (unsigned long)tmps % sizeof(long); anum--, tmps++)
		*tmps = ~*tmps;
	    tmpl = (long*)tmps;
	    for ( ; anum >= sizeof(long); anum -= sizeof(long), tmpl++)
		*tmpl = ~*tmpl;
	    tmps = (U8*)tmpl;
	}
#endif
	for ( ; anum > 0; anum--, tmps++)
	    *tmps = ~*tmps;

	SETs(TARG);
      }
      RETURN;
    }
}

/* integer versions of some of the above */

PP(pp_i_multiply)
{
    dSP; dATARGET; tryAMAGICbin(mult,opASSIGN);
    {
      dPOPTOPiirl;
      SETi( left * right );
      RETURN;
    }
}

PP(pp_i_divide)
{
    dSP; dATARGET; tryAMAGICbin(div,opASSIGN);
    {
      dPOPiv;
      if (value == 0)
	DIE(aTHX_ "Illegal division by zero");
      value = POPi / value;
      PUSHi( value );
      RETURN;
    }
}

PP(pp_i_modulo)
{
    dSP; dATARGET; tryAMAGICbin(modulo,opASSIGN);
    {
      dPOPTOPiirl;
      if (!right)
	DIE(aTHX_ "Illegal modulus zero");
      SETi( left % right );
      RETURN;
    }
}

PP(pp_i_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN);
    {
      dPOPTOPiirl_ul;
      SETi( left + right );
      RETURN;
    }
}

PP(pp_i_subtract)
{
    dSP; dATARGET; tryAMAGICbin(subtr,opASSIGN);
    {
      dPOPTOPiirl_ul;
      SETi( left - right );
      RETURN;
    }
}

PP(pp_i_lt)
{
    dSP; tryAMAGICbinSET(lt,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left < right));
      RETURN;
    }
}

PP(pp_i_gt)
{
    dSP; tryAMAGICbinSET(gt,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left > right));
      RETURN;
    }
}

PP(pp_i_le)
{
    dSP; tryAMAGICbinSET(le,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left <= right));
      RETURN;
    }
}

PP(pp_i_ge)
{
    dSP; tryAMAGICbinSET(ge,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left >= right));
      RETURN;
    }
}

PP(pp_i_eq)
{
    dSP; tryAMAGICbinSET(eq,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left == right));
      RETURN;
    }
}

PP(pp_i_ne)
{
    dSP; tryAMAGICbinSET(ne,0);
    {
      dPOPTOPiirl;
      SETs(boolSV(left != right));
      RETURN;
    }
}

PP(pp_i_ncmp)
{
    dSP; dTARGET; tryAMAGICbin(ncmp,0);
    {
      dPOPTOPiirl;
      I32 value;

      if (left > right)
	value = 1;
      else if (left < right)
	value = -1;
      else
	value = 0;
      SETi(value);
      RETURN;
    }
}

PP(pp_i_negate)
{
    dSP; dTARGET; tryAMAGICun(neg);
    SETi(-TOPi);
    RETURN;
}

/* High falutin' math. */

PP(pp_atan2)
{
    dSP; dTARGET; tryAMAGICbin(atan2,0);
    {
      dPOPTOPnnrl;
      SETn(Perl_atan2(left, right));
      RETURN;
    }
}

PP(pp_sin)
{
    dSP; dTARGET; tryAMAGICun(sin);
    {
      NV value;
      value = POPn;
      value = Perl_sin(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_cos)
{
    dSP; dTARGET; tryAMAGICun(cos);
    {
      NV value;
      value = POPn;
      value = Perl_cos(value);
      XPUSHn(value);
      RETURN;
    }
}

/* Support Configure command-line overrides for rand() functions.
   After 5.005, perhaps we should replace this by Configure support
   for drand48(), random(), or rand().  For 5.005, though, maintain
   compatibility by calling rand() but allow the user to override it.
   See INSTALL for details.  --Andy Dougherty  15 July 1998
*/
/* Now it's after 5.005, and Configure supports drand48() and random(),
   in addition to rand().  So the overrides should not be needed any more.
   --Jarkko Hietaniemi	27 September 1998
 */

#ifndef HAS_DRAND48_PROTO
extern double drand48 (void);
#endif

PP(pp_rand)
{
    dSP; dTARGET;
    NV value;
    if (MAXARG < 1)
	value = 1.0;
    else
	value = POPn;
    if (value == 0.0)
	value = 1.0;
    if (!PL_srand_called) {
	(void)seedDrand01((Rand_seed_t)seed());
	PL_srand_called = TRUE;
    }
    value *= Drand01();
    XPUSHn(value);
    RETURN;
}

PP(pp_srand)
{
    dSP;
    UV anum;
    if (MAXARG < 1)
	anum = seed();
    else
	anum = POPu;
    (void)seedDrand01((Rand_seed_t)anum);
    PL_srand_called = TRUE;
    EXTEND(SP, 1);
    RETPUSHYES;
}

STATIC U32
S_seed(pTHX)
{
    /*
     * This is really just a quick hack which grabs various garbage
     * values.  It really should be a real hash algorithm which
     * spreads the effect of every input bit onto every output bit,
     * if someone who knows about such things would bother to write it.
     * Might be a good idea to add that function to CORE as well.
     * No numbers below come from careful analysis or anything here,
     * except they are primes and SEED_C1 > 1E6 to get a full-width
     * value from (tv_sec * SEED_C1 + tv_usec).  The multipliers should
     * probably be bigger too.
     */
#if RANDBITS > 16
#  define SEED_C1	1000003
#define   SEED_C4	73819
#else
#  define SEED_C1	25747
#define   SEED_C4	20639
#endif
#define   SEED_C2	3
#define   SEED_C3	269
#define   SEED_C5	26107

#ifndef PERL_NO_DEV_RANDOM
    int fd;
#endif
    U32 u;
#ifdef VMS
#  include <starlet.h>
    /* when[] = (low 32 bits, high 32 bits) of time since epoch
     * in 100-ns units, typically incremented ever 10 ms.        */
    unsigned int when[2];
#else
#  ifdef HAS_GETTIMEOFDAY
    struct timeval when;
#  else
    Time_t when;
#  endif
#endif

/* This test is an escape hatch, this symbol isn't set by Configure. */
#ifndef PERL_NO_DEV_RANDOM
#ifndef PERL_RANDOM_DEVICE
   /* /dev/random isn't used by default because reads from it will block
    * if there isn't enough entropy available.  You can compile with
    * PERL_RANDOM_DEVICE to it if you'd prefer Perl to block until there
    * is enough real entropy to fill the seed. */
#  define PERL_RANDOM_DEVICE "/dev/urandom"
#endif
    fd = PerlLIO_open(PERL_RANDOM_DEVICE, 0);
    if (fd != -1) {
    	if (PerlLIO_read(fd, &u, sizeof u) != sizeof u)
	    u = 0;
	PerlLIO_close(fd);
	if (u)
	    return u;
    }
#endif

#ifdef VMS
    _ckvmssts(sys$gettim(when));
    u = (U32)SEED_C1 * when[0] + (U32)SEED_C2 * when[1];
#else
#  ifdef HAS_GETTIMEOFDAY
    gettimeofday(&when,(struct timezone *) 0);
    u = (U32)SEED_C1 * when.tv_sec + (U32)SEED_C2 * when.tv_usec;
#  else
    (void)time(&when);
    u = (U32)SEED_C1 * when;
#  endif
#endif
    u += SEED_C3 * (U32)PerlProc_getpid();
    u += SEED_C4 * (U32)PTR2UV(PL_stack_sp);
#ifndef PLAN9           /* XXX Plan9 assembler chokes on this; fix needed  */
    u += SEED_C5 * (U32)PTR2UV(&when);
#endif
    return u;
}

PP(pp_exp)
{
    dSP; dTARGET; tryAMAGICun(exp);
    {
      NV value;
      value = POPn;
      value = Perl_exp(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_log)
{
    dSP; dTARGET; tryAMAGICun(log);
    {
      NV value;
      value = POPn;
      if (value <= 0.0) {
	SET_NUMERIC_STANDARD();
	DIE(aTHX_ "Can't take log of %g", value);
      }
      value = Perl_log(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_sqrt)
{
    dSP; dTARGET; tryAMAGICun(sqrt);
    {
      NV value;
      value = POPn;
      if (value < 0.0) {
	SET_NUMERIC_STANDARD();
	DIE(aTHX_ "Can't take sqrt of %g", value);
      }
      value = Perl_sqrt(value);
      XPUSHn(value);
      RETURN;
    }
}

PP(pp_int)
{
    dSP; dTARGET;
    {
      NV value = TOPn;
      IV iv;

      if (SvIOKp(TOPs) && !SvNOKp(TOPs) && !SvPOKp(TOPs)) {
	iv = SvIVX(TOPs);
	SETi(iv);
      }
      else {
	  if (value >= 0.0) {
#if defined(HAS_MODFL) || defined(LONG_DOUBLE_EQUALS_DOUBLE)
	      (void)Perl_modf(value, &value);
#else
	      double tmp = (double)value;
	      (void)Perl_modf(tmp, &tmp);
	      value = (NV)tmp;
#endif
	  }
	else {
#if defined(HAS_MODFL) || defined(LONG_DOUBLE_EQUALS_DOUBLE)
	    (void)Perl_modf(-value, &value);
	    value = -value;
#else
	    double tmp = (double)value;
	    (void)Perl_modf(-tmp, &tmp);
	    value = -(NV)tmp;
#endif
	}
	iv = I_V(value);
	if (iv == value)
	  SETi(iv);
	else
	  SETn(value);
      }
    }
    RETURN;
}

PP(pp_abs)
{
    dSP; dTARGET; tryAMAGICun(abs);
    {
      NV value = TOPn;
      IV iv;

      if (SvIOKp(TOPs) && !SvNOKp(TOPs) && !SvPOKp(TOPs) &&
	  (iv = SvIVX(TOPs)) != IV_MIN) {
	if (iv < 0)
	  iv = -iv;
	SETi(iv);
      }
      else {
	if (value < 0.0)
	    value = -value;
	SETn(value);
      }
    }
    RETURN;
}

PP(pp_hex)
{
    dSP; dTARGET;
    char *tmps;
    STRLEN argtype;
    STRLEN len;

    tmps = (SvPVx(POPs, len));
    argtype = 1;		/* allow underscores */
    XPUSHn(scan_hex(tmps, len, &argtype));
    RETURN;
}

PP(pp_oct)
{
    dSP; dTARGET;
    NV value;
    STRLEN argtype;
    char *tmps;
    STRLEN len;

    tmps = (SvPVx(POPs, len));
    while (*tmps && len && isSPACE(*tmps))
       tmps++, len--;
    if (*tmps == '0')
       tmps++, len--;
    argtype = 1;		/* allow underscores */
    if (*tmps == 'x')
       value = scan_hex(++tmps, --len, &argtype);
    else if (*tmps == 'b')
       value = scan_bin(++tmps, --len, &argtype);
    else
       value = scan_oct(tmps, len, &argtype);
    XPUSHn(value);
    RETURN;
}

/* String stuff. */

PP(pp_length)
{
    dSP; dTARGET;
    SV *sv = TOPs;

    if (DO_UTF8(sv))
	SETi(sv_len_utf8(sv));
    else
	SETi(sv_len(sv));
    RETURN;
}

PP(pp_substr)
{
    dSP; dTARGET;
    SV *sv;
    I32 len;
    STRLEN curlen;
    STRLEN utf8_curlen;
    I32 pos;
    I32 rem;
    I32 fail;
    I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;
    char *tmps;
    I32 arybase = PL_curcop->cop_arybase;
    SV *repl_sv = NULL;
    char *repl = 0;
    STRLEN repl_len;
    int num_args = PL_op->op_private & 7;
    bool repl_need_utf8_upgrade = FALSE;
    bool repl_is_utf8 = FALSE;

    SvTAINTED_off(TARG);			/* decontaminate */
    SvUTF8_off(TARG);				/* decontaminate */
    if (num_args > 2) {
	if (num_args > 3) {
	    repl_sv = POPs;
	    repl = SvPV(repl_sv, repl_len);
	    repl_is_utf8 = DO_UTF8(repl_sv) && SvCUR(repl_sv);
	}
	len = POPi;
    }
    pos = POPi;
    sv = POPs;
    PUTBACK;
    if (repl_sv) {
	if (repl_is_utf8) {
	    if (!DO_UTF8(sv))
		sv_utf8_upgrade(sv);
	}
	else if (DO_UTF8(sv))
	    repl_need_utf8_upgrade = TRUE;
    }
    tmps = SvPV(sv, curlen);
    if (DO_UTF8(sv)) {
        utf8_curlen = sv_len_utf8(sv);
	if (utf8_curlen == curlen)
	    utf8_curlen = 0;
	else
	    curlen = utf8_curlen;
    }
    else
	utf8_curlen = 0;

    if (pos >= arybase) {
	pos -= arybase;
	rem = curlen-pos;
	fail = rem;
	if (num_args > 2) {
	    if (len < 0) {
		rem += len;
		if (rem < 0)
		    rem = 0;
	    }
	    else if (rem > len)
		     rem = len;
	}
    }
    else {
	pos += curlen;
	if (num_args < 3)
	    rem = curlen;
	else if (len >= 0) {
	    rem = pos+len;
	    if (rem > (I32)curlen)
		rem = curlen;
	}
	else {
	    rem = curlen+len;
	    if (rem < pos)
		rem = pos;
	}
	if (pos < 0)
	    pos = 0;
	fail = rem;
	rem -= pos;
    }
    if (fail < 0) {
	if (lvalue || repl)
	    Perl_croak(aTHX_ "substr outside of string");
	if (ckWARN(WARN_SUBSTR))
	    Perl_warner(aTHX_ WARN_SUBSTR, "substr outside of string");
	RETPUSHUNDEF;
    }
    else {
	I32 upos = pos;
	I32 urem = rem;
	if (utf8_curlen)
	    sv_pos_u2b(sv, &pos, &rem);
	tmps += pos;
	sv_setpvn(TARG, tmps, rem);
	if (utf8_curlen)
	    SvUTF8_on(TARG);
	if (repl) {
	    SV* repl_sv_copy = NULL;

	    if (repl_need_utf8_upgrade) {
		repl_sv_copy = newSVsv(repl_sv);
		sv_utf8_upgrade(repl_sv_copy);
		repl = SvPV(repl_sv_copy, repl_len);
		repl_is_utf8 = DO_UTF8(repl_sv_copy) && SvCUR(sv);
	    }
	    sv_insert(sv, pos, rem, repl, repl_len);
	    if (repl_is_utf8)
		SvUTF8_on(sv);
	    if (repl_sv_copy)
		SvREFCNT_dec(repl_sv_copy);
	}
	else if (lvalue) {		/* it's an lvalue! */
	    if (!SvGMAGICAL(sv)) {
		if (SvROK(sv)) {
		    STRLEN n_a;
		    SvPV_force(sv,n_a);
		    if (ckWARN(WARN_SUBSTR))
			Perl_warner(aTHX_ WARN_SUBSTR,
				"Attempt to use reference as lvalue in substr");
		}
		if (SvOK(sv))		/* is it defined ? */
		    (void)SvPOK_only_UTF8(sv);
		else
		    sv_setpvn(sv,"",0);	/* avoid lexical reincarnation */
	    }

	    if (SvTYPE(TARG) < SVt_PVLV) {
		sv_upgrade(TARG, SVt_PVLV);
		sv_magic(TARG, Nullsv, 'x', Nullch, 0);
	    }

	    LvTYPE(TARG) = 'x';
	    if (LvTARG(TARG) != sv) {
		if (LvTARG(TARG))
		    SvREFCNT_dec(LvTARG(TARG));
		LvTARG(TARG) = SvREFCNT_inc(sv);
	    }
	    LvTARGOFF(TARG) = upos;
	    LvTARGLEN(TARG) = urem;
	}
    }
    SPAGAIN;
    PUSHs(TARG);		/* avoid SvSETMAGIC here */
    RETURN;
}

PP(pp_vec)
{
    dSP; dTARGET;
    register IV size   = POPi;
    register IV offset = POPi;
    register SV *src = POPs;
    I32 lvalue = PL_op->op_flags & OPf_MOD || LVRET;

    SvTAINTED_off(TARG);		/* decontaminate */
    if (lvalue) {			/* it's an lvalue! */
	if (SvTYPE(TARG) < SVt_PVLV) {
	    sv_upgrade(TARG, SVt_PVLV);
	    sv_magic(TARG, Nullsv, 'v', Nullch, 0);
	}
	LvTYPE(TARG) = 'v';
	if (LvTARG(TARG) != src) {
	    if (LvTARG(TARG))
		SvREFCNT_dec(LvTARG(TARG));
	    LvTARG(TARG) = SvREFCNT_inc(src);
	}
	LvTARGOFF(TARG) = offset;
	LvTARGLEN(TARG) = size;
    }

    sv_setuv(TARG, do_vecget(src, offset, size));
    PUSHs(TARG);
    RETURN;
}

PP(pp_index)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    STRLEN biglen;
    I32 arybase = PL_curcop->cop_arybase;

    if (MAXARG < 3)
	offset = 0;
    else
	offset = POPi - arybase;
    little = POPs;
    big = POPs;
    tmps = SvPV(big, biglen);
    if (offset > 0 && DO_UTF8(big))
	sv_pos_u2b(big, &offset, 0);
    if (offset < 0)
	offset = 0;
    else if (offset > biglen)
	offset = biglen;
    if (!(tmps2 = fbm_instr((unsigned char*)tmps + offset,
      (unsigned char*)tmps + biglen, little, 0)))
	retval = -1;
    else
	retval = tmps2 - tmps;
    if (retval > 0 && DO_UTF8(big))
	sv_pos_b2u(big, &retval);
    PUSHi(retval + arybase);
    RETURN;
}

PP(pp_rindex)
{
    dSP; dTARGET;
    SV *big;
    SV *little;
    STRLEN blen;
    STRLEN llen;
    I32 offset;
    I32 retval;
    char *tmps;
    char *tmps2;
    I32 arybase = PL_curcop->cop_arybase;

    if (MAXARG >= 3)
	offset = POPi;
    little = POPs;
    big = POPs;
    tmps2 = SvPV(little, llen);
    tmps = SvPV(big, blen);
    if (MAXARG < 3)
	offset = blen;
    else {
	if (offset > 0 && DO_UTF8(big))
	    sv_pos_u2b(big, &offset, 0);
	offset = offset - arybase + llen;
    }
    if (offset < 0)
	offset = 0;
    else if (offset > blen)
	offset = blen;
    if (!(tmps2 = rninstr(tmps,  tmps  + offset,
			  tmps2, tmps2 + llen)))
	retval = -1;
    else
	retval = tmps2 - tmps;
    if (retval > 0 && DO_UTF8(big))
	sv_pos_b2u(big, &retval);
    PUSHi(retval + arybase);
    RETURN;
}

PP(pp_sprintf)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    do_sprintf(TARG, SP-MARK, MARK+1);
    TAINT_IF(SvTAINTED(TARG));
    SP = ORIGMARK;
    PUSHTARG;
    RETURN;
}

PP(pp_ord)
{
    dSP; dTARGET;
    SV *argsv = POPs;
    STRLEN len;
    U8 *s = (U8*)SvPVx(argsv, len);

    XPUSHu(DO_UTF8(argsv) ? utf8_to_uv_simple(s, 0) : (*s & 0xff));
    RETURN;
}

PP(pp_chr)
{
    dSP; dTARGET;
    char *tmps;
    UV value = POPu;

    (void)SvUPGRADE(TARG,SVt_PV);

    if (value > 255 && !IN_BYTE) {
	SvGROW(TARG, UTF8_MAXLEN+1);
	tmps = SvPVX(TARG);
	tmps = (char*)uv_to_utf8((U8*)tmps, (UV)value);
	SvCUR_set(TARG, tmps - SvPVX(TARG));
	*tmps = '\0';
	(void)SvPOK_only(TARG);
	SvUTF8_on(TARG);
	XPUSHs(TARG);
	RETURN;
    }

    SvGROW(TARG,2);
    SvCUR_set(TARG, 1);
    tmps = SvPVX(TARG);
    *tmps++ = value;
    *tmps = '\0';
    (void)SvPOK_only(TARG);
    XPUSHs(TARG);
    RETURN;
}

PP(pp_crypt)
{
    dSP; dTARGET; dPOPTOPssrl;
    STRLEN n_a;
#ifdef HAS_CRYPT
    char *tmps = SvPV(left, n_a);
#ifdef FCRYPT
    sv_setpv(TARG, fcrypt(tmps, SvPV(right, n_a)));
#else
    sv_setpv(TARG, PerlProc_crypt(tmps, SvPV(right, n_a)));
#endif
#else
    DIE(aTHX_ 
      "The crypt() function is unimplemented due to excessive paranoia.");
#endif
    SETs(TARG);
    RETURN;
}

PP(pp_ucfirst)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN slen;

    if (DO_UTF8(sv) && (s = (U8*)SvPV(sv, slen)) && slen && UTF8_IS_START(*s)) {
	STRLEN ulen;
	U8 tmpbuf[UTF8_MAXLEN+1];
	U8 *tend;
	UV uv = utf8_to_uv(s, slen, &ulen, 0);

	if (PL_op->op_private & OPpLOCALE) {
	    TAINT;
	    SvTAINTED_on(sv);
	    uv = toTITLE_LC_uni(uv);
	}
	else
	    uv = toTITLE_utf8(s);
	
	tend = uv_to_utf8(tmpbuf, uv);

	if (!SvPADTMP(sv) || tend - tmpbuf != ulen || SvREADONLY(sv)) {
	    dTARGET;
	    sv_setpvn(TARG, (char*)tmpbuf, tend - tmpbuf);
	    sv_catpvn(TARG, (char*)(s + ulen), slen - ulen);
	    SvUTF8_on(TARG);
	    SETs(TARG);
	}
	else {
	    s = (U8*)SvPV_force(sv, slen);
	    Copy(tmpbuf, s, ulen, U8);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}
	s = (U8*)SvPV_force(sv, slen);
	if (*s) {
	    if (PL_op->op_private & OPpLOCALE) {
		TAINT;
		SvTAINTED_on(sv);
		*s = toUPPER_LC(*s);
	    }
	    else
		*s = toUPPER(*s);
	}
    }
    if (SvSMAGICAL(sv))
	mg_set(sv);
    RETURN;
}

PP(pp_lcfirst)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN slen;

    if (DO_UTF8(sv) && (s = (U8*)SvPV(sv, slen)) && slen && UTF8_IS_START(*s)) {
	STRLEN ulen;
	U8 tmpbuf[UTF8_MAXLEN+1];
	U8 *tend;
	UV uv = utf8_to_uv(s, slen, &ulen, 0);

	if (PL_op->op_private & OPpLOCALE) {
	    TAINT;
	    SvTAINTED_on(sv);
	    uv = toLOWER_LC_uni(uv);
	}
	else
	    uv = toLOWER_utf8(s);
	
	tend = uv_to_utf8(tmpbuf, uv);

	if (!SvPADTMP(sv) || tend - tmpbuf != ulen || SvREADONLY(sv)) {
	    dTARGET;
	    sv_setpvn(TARG, (char*)tmpbuf, tend - tmpbuf);
	    sv_catpvn(TARG, (char*)(s + ulen), slen - ulen);
	    SvUTF8_on(TARG);
	    SETs(TARG);
	}
	else {
	    s = (U8*)SvPV_force(sv, slen);
	    Copy(tmpbuf, s, ulen, U8);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}
	s = (U8*)SvPV_force(sv, slen);
	if (*s) {
	    if (PL_op->op_private & OPpLOCALE) {
		TAINT;
		SvTAINTED_on(sv);
		*s = toLOWER_LC(*s);
	    }
	    else
		*s = toLOWER(*s);
	}
    }
    if (SvSMAGICAL(sv))
	mg_set(sv);
    RETURN;
}

PP(pp_uc)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN len;

    if (DO_UTF8(sv)) {
	dTARGET;
	STRLEN ulen;
	register U8 *d;
	U8 *send;

	s = (U8*)SvPV(sv,len);
	if (!len) {
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setpvn(TARG, "", 0);
	    SETs(TARG);
	}
	else {
	    (void)SvUPGRADE(TARG, SVt_PV);
	    SvGROW(TARG, (len * 2) + 1);
	    (void)SvPOK_only(TARG);
	    d = (U8*)SvPVX(TARG);
	    send = s + len;
	    if (PL_op->op_private & OPpLOCALE) {
		TAINT;
		SvTAINTED_on(TARG);
		while (s < send) {
		    d = uv_to_utf8(d, toUPPER_LC_uni( utf8_to_uv(s, len, &ulen, 0)));
		    s += ulen;
		}
	    }
	    else {
		while (s < send) {
		    d = uv_to_utf8(d, toUPPER_utf8( s ));
		    s += UTF8SKIP(s);
		}
	    }
	    *d = '\0';
	    SvUTF8_on(TARG);
	    SvCUR_set(TARG, d - (U8*)SvPVX(TARG));
	    SETs(TARG);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}
	s = (U8*)SvPV_force(sv, len);
	if (len) {
	    register U8 *send = s + len;

	    if (PL_op->op_private & OPpLOCALE) {
		TAINT;
		SvTAINTED_on(sv);
		for (; s < send; s++)
		    *s = toUPPER_LC(*s);
	    }
	    else {
		for (; s < send; s++)
		    *s = toUPPER(*s);
	    }
	}
    }
    if (SvSMAGICAL(sv))
	mg_set(sv);
    RETURN;
}

PP(pp_lc)
{
    dSP;
    SV *sv = TOPs;
    register U8 *s;
    STRLEN len;

    if (DO_UTF8(sv)) {
	dTARGET;
	STRLEN ulen;
	register U8 *d;
	U8 *send;

	s = (U8*)SvPV(sv,len);
	if (!len) {
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setpvn(TARG, "", 0);
	    SETs(TARG);
	}
	else {
	    (void)SvUPGRADE(TARG, SVt_PV);
	    SvGROW(TARG, (len * 2) + 1);
	    (void)SvPOK_only(TARG);
	    d = (U8*)SvPVX(TARG);
	    send = s + len;
	    if (PL_op->op_private & OPpLOCALE) {
		TAINT;
		SvTAINTED_on(TARG);
		while (s < send) {
		    d = uv_to_utf8(d, toLOWER_LC_uni( utf8_to_uv(s, len, &ulen, 0)));
		    s += ulen;
		}
	    }
	    else {
		while (s < send) {
		    d = uv_to_utf8(d, toLOWER_utf8(s));
		    s += UTF8SKIP(s);
		}
	    }
	    *d = '\0';
	    SvUTF8_on(TARG);
	    SvCUR_set(TARG, d - (U8*)SvPVX(TARG));
	    SETs(TARG);
	}
    }
    else {
	if (!SvPADTMP(sv) || SvREADONLY(sv)) {
	    dTARGET;
	    SvUTF8_off(TARG);				/* decontaminate */
	    sv_setsv(TARG, sv);
	    sv = TARG;
	    SETs(sv);
	}

	s = (U8*)SvPV_force(sv, len);
	if (len) {
	    register U8 *send = s + len;

	    if (PL_op->op_private & OPpLOCALE) {
		TAINT;
		SvTAINTED_on(sv);
		for (; s < send; s++)
		    *s = toLOWER_LC(*s);
	    }
	    else {
		for (; s < send; s++)
		    *s = toLOWER(*s);
	    }
	}
    }
    if (SvSMAGICAL(sv))
	mg_set(sv);
    RETURN;
}

PP(pp_quotemeta)
{
    dSP; dTARGET;
    SV *sv = TOPs;
    STRLEN len;
    register char *s = SvPV(sv,len);
    register char *d;

    SvUTF8_off(TARG);				/* decontaminate */
    if (len) {
	(void)SvUPGRADE(TARG, SVt_PV);
	SvGROW(TARG, (len * 2) + 1);
	d = SvPVX(TARG);
	if (DO_UTF8(sv)) {
	    while (len) {
		if (UTF8_IS_CONTINUED(*s)) {
		    STRLEN ulen = UTF8SKIP(s);
		    if (ulen > len)
			ulen = len;
		    len -= ulen;
		    while (ulen--)
			*d++ = *s++;
		}
		else {
		    if (!isALNUM(*s))
			*d++ = '\\';
		    *d++ = *s++;
		    len--;
		}
	    }
	    SvUTF8_on(TARG);
	}
	else {
	    while (len--) {
		if (!isALNUM(*s))
		    *d++ = '\\';
		*d++ = *s++;
	    }
	}
	*d = '\0';
	SvCUR_set(TARG, d - SvPVX(TARG));
	(void)SvPOK_only_UTF8(TARG);
    }
    else
	sv_setpvn(TARG, s, len);
    SETs(TARG);
    if (SvSMAGICAL(TARG))
	mg_set(TARG);
    RETURN;
}

/* Arrays. */

PP(pp_aslice)
{
    dSP; dMARK; dORIGMARK;
    register SV** svp;
    register AV* av = (AV*)POPs;
    register I32 lval = (PL_op->op_flags & OPf_MOD || LVRET);
    I32 arybase = PL_curcop->cop_arybase;
    I32 elem;

    if (SvTYPE(av) == SVt_PVAV) {
	if (lval && PL_op->op_private & OPpLVAL_INTRO) {
	    I32 max = -1;
	    for (svp = MARK + 1; svp <= SP; svp++) {
		elem = SvIVx(*svp);
		if (elem > max)
		    max = elem;
	    }
	    if (max > AvMAX(av))
		av_extend(av, max);
	}
	while (++MARK <= SP) {
	    elem = SvIVx(*MARK);

	    if (elem > 0)
		elem -= arybase;
	    svp = av_fetch(av, elem, lval);
	    if (lval) {
		if (!svp || *svp == &PL_sv_undef)
		    DIE(aTHX_ PL_no_aelem, elem);
		if (PL_op->op_private & OPpLVAL_INTRO)
		    save_aelem(av, elem, svp);
	    }
	    *MARK = svp ? *svp : &PL_sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* Associative arrays. */

PP(pp_each)
{
    dSP;
    HV *hash = (HV*)POPs;
    HE *entry;
    I32 gimme = GIMME_V;
    I32 realhv = (SvTYPE(hash) == SVt_PVHV);

    PUTBACK;
    /* might clobber stack_sp */
    entry = realhv ? hv_iternext(hash) : avhv_iternext((AV*)hash);
    SPAGAIN;

    EXTEND(SP, 2);
    if (entry) {
	PUSHs(hv_iterkeysv(entry));	/* won't clobber stack_sp */
	if (gimme == G_ARRAY) {
	    SV *val;
	    PUTBACK;
	    /* might clobber stack_sp */
	    val = realhv ?
		  hv_iterval(hash, entry) : avhv_iterval((AV*)hash, entry);
	    SPAGAIN;
	    PUSHs(val);
	}
    }
    else if (gimme == G_SCALAR)
	RETPUSHUNDEF;

    RETURN;
}

PP(pp_values)
{
    return do_kv();
}

PP(pp_keys)
{
    return do_kv();
}

PP(pp_delete)
{
    dSP;
    I32 gimme = GIMME_V;
    I32 discard = (gimme == G_VOID) ? G_DISCARD : 0;
    SV *sv;
    HV *hv;

    if (PL_op->op_private & OPpSLICE) {
	dMARK; dORIGMARK;
	U32 hvtype;
	hv = (HV*)POPs;
	hvtype = SvTYPE(hv);
	if (hvtype == SVt_PVHV) {			/* hash element */
	    while (++MARK <= SP) {
		sv = hv_delete_ent(hv, *MARK, discard, 0);
		*MARK = sv ? sv : &PL_sv_undef;
	    }
	}
	else if (hvtype == SVt_PVAV) {
	    if (PL_op->op_flags & OPf_SPECIAL) {	/* array element */
		while (++MARK <= SP) {
		    sv = av_delete((AV*)hv, SvIV(*MARK), discard);
		    *MARK = sv ? sv : &PL_sv_undef;
		}
	    }
	    else {					/* pseudo-hash element */
		while (++MARK <= SP) {
		    sv = avhv_delete_ent((AV*)hv, *MARK, discard, 0);
		    *MARK = sv ? sv : &PL_sv_undef;
		}
	    }
	}
	else
	    DIE(aTHX_ "Not a HASH reference");
	if (discard)
	    SP = ORIGMARK;
	else if (gimme == G_SCALAR) {
	    MARK = ORIGMARK;
	    *++MARK = *SP;
	    SP = MARK;
	}
    }
    else {
	SV *keysv = POPs;
	hv = (HV*)POPs;
	if (SvTYPE(hv) == SVt_PVHV)
	    sv = hv_delete_ent(hv, keysv, discard, 0);
	else if (SvTYPE(hv) == SVt_PVAV) {
	    if (PL_op->op_flags & OPf_SPECIAL)
		sv = av_delete((AV*)hv, SvIV(keysv), discard);
	    else
		sv = avhv_delete_ent((AV*)hv, keysv, discard, 0);
	}
	else
	    DIE(aTHX_ "Not a HASH reference");
	if (!sv)
	    sv = &PL_sv_undef;
	if (!discard)
	    PUSHs(sv);
    }
    RETURN;
}

PP(pp_exists)
{
    dSP;
    SV *tmpsv;
    HV *hv;

    if (PL_op->op_private & OPpEXISTS_SUB) {
	GV *gv;
	CV *cv;
	SV *sv = POPs;
	cv = sv_2cv(sv, &hv, &gv, FALSE);
	if (cv)
	    RETPUSHYES;
	if (gv && isGV(gv) && GvCV(gv) && !GvCVGEN(gv))
	    RETPUSHYES;
	RETPUSHNO;
    }
    tmpsv = POPs;
    hv = (HV*)POPs;
    if (SvTYPE(hv) == SVt_PVHV) {
	if (hv_exists_ent(hv, tmpsv, 0))
	    RETPUSHYES;
    }
    else if (SvTYPE(hv) == SVt_PVAV) {
	if (PL_op->op_flags & OPf_SPECIAL) {		/* array element */
	    if (av_exists((AV*)hv, SvIV(tmpsv)))
		RETPUSHYES;
	}
	else if (avhv_exists_ent((AV*)hv, tmpsv, 0))	/* pseudo-hash element */
	    RETPUSHYES;
    }
    else {
	DIE(aTHX_ "Not a HASH reference");
    }
    RETPUSHNO;
}

PP(pp_hslice)
{
    dSP; dMARK; dORIGMARK;
    register HV *hv = (HV*)POPs;
    register I32 lval = (PL_op->op_flags & OPf_MOD || LVRET);
    I32 realhv = (SvTYPE(hv) == SVt_PVHV);

    if (!realhv && PL_op->op_private & OPpLVAL_INTRO)
	DIE(aTHX_ "Can't localize pseudo-hash element");

    if (realhv || SvTYPE(hv) == SVt_PVAV) {
	while (++MARK <= SP) {
	    SV *keysv = *MARK;
	    SV **svp;
	    if (realhv) {
		HE *he = hv_fetch_ent(hv, keysv, lval, 0);
		svp = he ? &HeVAL(he) : 0;
	    }
	    else {
		svp = avhv_fetch_ent((AV*)hv, keysv, lval, 0);
	    }
	    if (lval) {
		if (!svp || *svp == &PL_sv_undef) {
		    STRLEN n_a;
		    DIE(aTHX_ PL_no_helem, SvPV(keysv, n_a));
		}
		if (PL_op->op_private & OPpLVAL_INTRO)
		    save_helem(hv, keysv, svp);
	    }
	    *MARK = svp ? *svp : &PL_sv_undef;
	}
    }
    if (GIMME != G_ARRAY) {
	MARK = ORIGMARK;
	*++MARK = *SP;
	SP = MARK;
    }
    RETURN;
}

/* List operators. */

PP(pp_list)
{
    dSP; dMARK;
    if (GIMME != G_ARRAY) {
	if (++MARK <= SP)
	    *MARK = *SP;		/* unwanted list, return last item */
	else
	    *MARK = &PL_sv_undef;
	SP = MARK;
    }
    RETURN;
}

PP(pp_lslice)
{
    dSP;
    SV **lastrelem = PL_stack_sp;
    SV **lastlelem = PL_stack_base + POPMARK;
    SV **firstlelem = PL_stack_base + POPMARK + 1;
    register SV **firstrelem = lastlelem + 1;
    I32 arybase = PL_curcop->cop_arybase;
    I32 lval = PL_op->op_flags & OPf_MOD;
    I32 is_something_there = lval;

    register I32 max = lastrelem - lastlelem;
    register SV **lelem;
    register I32 ix;

    if (GIMME != G_ARRAY) {
	ix = SvIVx(*lastlelem);
	if (ix < 0)
	    ix += max;
	else
	    ix -= arybase;
	if (ix < 0 || ix >= max)
	    *firstlelem = &PL_sv_undef;
	else
	    *firstlelem = firstrelem[ix];
	SP = firstlelem;
	RETURN;
    }

    if (max == 0) {
	SP = firstlelem - 1;
	RETURN;
    }

    for (lelem = firstlelem; lelem <= lastlelem; lelem++) {
	ix = SvIVx(*lelem);
	if (ix < 0)
	    ix += max;
	else 
	    ix -= arybase;
	if (ix < 0 || ix >= max)
	    *lelem = &PL_sv_undef;
	else {
	    is_something_there = TRUE;
	    if (!(*lelem = firstrelem[ix]))
		*lelem = &PL_sv_undef;
	}
    }
    if (is_something_there)
	SP = lastlelem;
    else
	SP = firstlelem - 1;
    RETURN;
}

PP(pp_anonlist)
{
    dSP; dMARK; dORIGMARK;
    I32 items = SP - MARK;
    SV *av = sv_2mortal((SV*)av_make(items, MARK+1));
    SP = ORIGMARK;		/* av_make() might realloc stack_sp */
    XPUSHs(av);
    RETURN;
}

PP(pp_anonhash)
{
    dSP; dMARK; dORIGMARK;
    HV* hv = (HV*)sv_2mortal((SV*)newHV());

    while (MARK < SP) {
	SV* key = *++MARK;
	SV *val = NEWSV(46, 0);
	if (MARK < SP)
	    sv_setsv(val, *++MARK);
	else if (ckWARN(WARN_MISC))
	    Perl_warner(aTHX_ WARN_MISC, "Odd number of elements in hash assignment");
	(void)hv_store_ent(hv,key,val,0);
    }
    SP = ORIGMARK;
    XPUSHs((SV*)hv);
    RETURN;
}

PP(pp_splice)
{
    dSP; dMARK; dORIGMARK;
    register AV *ary = (AV*)*++MARK;
    register SV **src;
    register SV **dst;
    register I32 i;
    register I32 offset;
    register I32 length;
    I32 newlen;
    I32 after;
    I32 diff;
    SV **tmparyval = 0;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)ary, 'P'))) {
	*MARK-- = SvTIED_obj((SV*)ary, mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER;
	call_method("SPLICE",GIMME_V);
	LEAVE;
	SPAGAIN;
	RETURN;
    }

    SP++;

    if (++MARK < SP) {
	offset = i = SvIVx(*MARK);
	if (offset < 0)
	    offset += AvFILLp(ary) + 1;
	else
	    offset -= PL_curcop->cop_arybase;
	if (offset < 0)
	    DIE(aTHX_ PL_no_aelem, i);
	if (++MARK < SP) {
	    length = SvIVx(*MARK++);
	    if (length < 0) {
		length += AvFILLp(ary) - offset + 1;
		if (length < 0)
		    length = 0;
	    }
	}
	else
	    length = AvMAX(ary) + 1;		/* close enough to infinity */
    }
    else {
	offset = 0;
	length = AvMAX(ary) + 1;
    }
    if (offset > AvFILLp(ary) + 1)
	offset = AvFILLp(ary) + 1;
    after = AvFILLp(ary) + 1 - (offset + length);
    if (after < 0) {				/* not that much array */
	length += after;			/* offset+length now in array */
	after = 0;
	if (!AvALLOC(ary))
	    av_extend(ary, 0);
    }

    /* At this point, MARK .. SP-1 is our new LIST */

    newlen = SP - MARK;
    diff = newlen - length;
    if (newlen && !AvREAL(ary) && AvREIFY(ary))
	av_reify(ary);

    if (diff < 0) {				/* shrinking the area */
	if (newlen) {
	    New(451, tmparyval, newlen, SV*);	/* so remember insertion */
	    Copy(MARK, tmparyval, newlen, SV*);
	}

	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    MEXTEND(MARK, length);
	    Copy(AvARRAY(ary)+offset, MARK, length, SV*);
	    if (AvREAL(ary)) {
		EXTEND_MORTAL(length);
		for (i = length, dst = MARK; i; i--) {
		    sv_2mortal(*dst);	/* free them eventualy */
		    dst++;
		}
	    }
	    MARK += length - 1;
	}
	else {
	    *MARK = AvARRAY(ary)[offset+length-1];
	    if (AvREAL(ary)) {
		sv_2mortal(*MARK);
		for (i = length - 1, dst = &AvARRAY(ary)[offset]; i > 0; i--)
		    SvREFCNT_dec(*dst++);	/* free them now */
	    }
	}
	AvFILLp(ary) += diff;

	/* pull up or down? */

	if (offset < after) {			/* easier to pull up */
	    if (offset) {			/* esp. if nothing to pull */
		src = &AvARRAY(ary)[offset-1];
		dst = src - diff;		/* diff is negative */
		for (i = offset; i > 0; i--)	/* can't trust Copy */
		    *dst-- = *src--;
	    }
	    dst = AvARRAY(ary);
	    SvPVX(ary) = (char*)(AvARRAY(ary) - diff); /* diff is negative */
	    AvMAX(ary) += diff;
	}
	else {
	    if (after) {			/* anything to pull down? */
		src = AvARRAY(ary) + offset + length;
		dst = src + diff;		/* diff is negative */
		Move(src, dst, after, SV*);
	    }
	    dst = &AvARRAY(ary)[AvFILLp(ary)+1];
						/* avoid later double free */
	}
	i = -diff;
	while (i)
	    dst[--i] = &PL_sv_undef;
	
	if (newlen) {
	    for (src = tmparyval, dst = AvARRAY(ary) + offset;
	      newlen; newlen--) {
		*dst = NEWSV(46, 0);
		sv_setsv(*dst++, *src++);
	    }
	    Safefree(tmparyval);
	}
    }
    else {					/* no, expanding (or same) */
	if (length) {
	    New(452, tmparyval, length, SV*);	/* so remember deletion */
	    Copy(AvARRAY(ary)+offset, tmparyval, length, SV*);
	}

	if (diff > 0) {				/* expanding */

	    /* push up or down? */

	    if (offset < after && diff <= AvARRAY(ary) - AvALLOC(ary)) {
		if (offset) {
		    src = AvARRAY(ary);
		    dst = src - diff;
		    Move(src, dst, offset, SV*);
		}
		SvPVX(ary) = (char*)(AvARRAY(ary) - diff);/* diff is positive */
		AvMAX(ary) += diff;
		AvFILLp(ary) += diff;
	    }
	    else {
		if (AvFILLp(ary) + diff >= AvMAX(ary))	/* oh, well */
		    av_extend(ary, AvFILLp(ary) + diff);
		AvFILLp(ary) += diff;

		if (after) {
		    dst = AvARRAY(ary) + AvFILLp(ary);
		    src = dst - diff;
		    for (i = after; i; i--) {
			*dst-- = *src--;
		    }
		}
	    }
	}

	for (src = MARK, dst = AvARRAY(ary) + offset; newlen; newlen--) {
	    *dst = NEWSV(46, 0);
	    sv_setsv(*dst++, *src++);
	}
	MARK = ORIGMARK + 1;
	if (GIMME == G_ARRAY) {			/* copy return vals to stack */
	    if (length) {
		Copy(tmparyval, MARK, length, SV*);
		if (AvREAL(ary)) {
		    EXTEND_MORTAL(length);
		    for (i = length, dst = MARK; i; i--) {
			sv_2mortal(*dst);	/* free them eventualy */
			dst++;
		    }
		}
		Safefree(tmparyval);
	    }
	    MARK += length - 1;
	}
	else if (length--) {
	    *MARK = tmparyval[length];
	    if (AvREAL(ary)) {
		sv_2mortal(*MARK);
		while (length-- > 0)
		    SvREFCNT_dec(tmparyval[length]);
	    }
	    Safefree(tmparyval);
	}
	else
	    *MARK = &PL_sv_undef;
    }
    SP = MARK;
    RETURN;
}

PP(pp_push)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv = &PL_sv_undef;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)ary, 'P'))) {
	*MARK-- = SvTIED_obj((SV*)ary, mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER;
	call_method("PUSH",G_SCALAR|G_DISCARD);
	LEAVE;
	SPAGAIN;
    }
    else {
	/* Why no pre-extend of ary here ? */
	for (++MARK; MARK <= SP; MARK++) {
	    sv = NEWSV(51, 0);
	    if (*MARK)
		sv_setsv(sv, *MARK);
	    av_push(ary, sv);
	}
    }
    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_pop)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_pop(av);
    if (AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_shift)
{
    dSP;
    AV *av = (AV*)POPs;
    SV *sv = av_shift(av);
    EXTEND(SP, 1);
    if (!sv)
	RETPUSHUNDEF;
    if (AvREAL(av))
	(void)sv_2mortal(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_unshift)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register AV *ary = (AV*)*++MARK;
    register SV *sv;
    register I32 i = 0;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)ary, 'P'))) {
	*MARK-- = SvTIED_obj((SV*)ary, mg);
	PUSHMARK(MARK);
	PUTBACK;
	ENTER;
	call_method("UNSHIFT",G_SCALAR|G_DISCARD);
	LEAVE;
	SPAGAIN;
    }
    else {
	av_unshift(ary, SP - MARK);
	while (MARK < SP) {
	    sv = NEWSV(27, 0);
	    sv_setsv(sv, *++MARK);
	    (void)av_store(ary, i++, sv);
	}
    }
    SP = ORIGMARK;
    PUSHi( AvFILL(ary) + 1 );
    RETURN;
}

PP(pp_reverse)
{
    dSP; dMARK;
    register SV *tmp;
    SV **oldsp = SP;

    if (GIMME == G_ARRAY) {
	MARK++;
	while (MARK < SP) {
	    tmp = *MARK;
	    *MARK++ = *SP;
	    *SP-- = tmp;
	}
	/* safe as long as stack cannot get extended in the above */
	SP = oldsp;
    }
    else {
	register char *up;
	register char *down;
	register I32 tmp;
	dTARGET;
	STRLEN len;

	SvUTF8_off(TARG);				/* decontaminate */
	if (SP - MARK > 1)
	    do_join(TARG, &PL_sv_no, MARK, SP);
	else
	    sv_setsv(TARG, (SP > MARK) ? *SP : DEFSV);
	up = SvPV_force(TARG, len);
	if (len > 1) {
	    if (DO_UTF8(TARG)) {	/* first reverse each character */
		U8* s = (U8*)SvPVX(TARG);
		U8* send = (U8*)(s + len);
		while (s < send) {
		    if (UTF8_IS_ASCII(*s)) {
			s++;
			continue;
		    }
		    else {
			if (!utf8_to_uv_simple(s, 0))
			    break;
			up = (char*)s;
			s += UTF8SKIP(s);
			down = (char*)(s - 1);
			/* reverse this character */
			while (down > up) {
			    tmp = *up;
			    *up++ = *down;
			    *down-- = tmp;
			}
		    }
		}
		up = SvPVX(TARG);
	    }
	    down = SvPVX(TARG) + len - 1;
	    while (down > up) {
		tmp = *up;
		*up++ = *down;
		*down-- = tmp;
	    }
	    (void)SvPOK_only_UTF8(TARG);
	}
	SP = MARK + 1;
	SETTARG;
    }
    RETURN;
}

STATIC SV *
S_mul128(pTHX_ SV *sv, U8 m)
{
  STRLEN          len;
  char           *s = SvPV(sv, len);
  char           *t;
  U32             i = 0;

  if (!strnEQ(s, "0000", 4)) {  /* need to grow sv */
    SV             *tmpNew = newSVpvn("0000000000", 10);

    sv_catsv(tmpNew, sv);
    SvREFCNT_dec(sv);		/* free old sv */
    sv = tmpNew;
    s = SvPV(sv, len);
  }
  t = s + len - 1;
  while (!*t)                   /* trailing '\0'? */
    t--;
  while (t > s) {
    i = ((*t - '0') << 7) + m;
    *(t--) = '0' + (i % 10);
    m = i / 10;
  }
  return (sv);
}

/* Explosives and implosives. */

#if 'I' == 73 && 'J' == 74
/* On an ASCII/ISO kind of system */
#define ISUUCHAR(ch)    ((ch) >= ' ' && (ch) < 'a')
#else
/*
  Some other sort of character set - use memchr() so we don't match
  the null byte.
 */
#define ISUUCHAR(ch)    (memchr(PL_uuemap, (ch), sizeof(PL_uuemap)-1) || (ch) == ' ')
#endif

PP(pp_unpack)
{
    dSP;
    dPOPPOPssrl;
    I32 start_sp_offset = SP - PL_stack_base;
    I32 gimme = GIMME_V;
    SV *sv;
    STRLEN llen;
    STRLEN rlen;
    register char *pat = SvPV(left, llen);
    register char *s = SvPV(right, rlen);
    char *strend = s + rlen;
    char *strbeg = s;
    register char *patend = pat + llen;
    I32 datumtype;
    register I32 len;
    register I32 bits;
    register char *str;

    /* These must not be in registers: */
    short ashort;
    int aint;
    long along;
#ifdef HAS_QUAD
    Quad_t aquad;
#endif
    U16 aushort;
    unsigned int auint;
    U32 aulong;
#ifdef HAS_QUAD
    Uquad_t auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    I32 checksum = 0;
    register U32 culong;
    NV cdouble;
    int commas = 0;
    int star;
#ifdef PERL_NATINT_PACK
    int natint;		/* native integer */
    int unatint;	/* unsigned native integer */
#endif

    if (gimme != G_ARRAY) {		/* arrange to do first one only */
	/*SUPPRESS 530*/
	for (patend = pat; !isALPHA(*patend) || *patend == 'x'; patend++) ;
	if (strchr("aAZbBhHP", *patend) || *pat == '%') {
	    patend++;
	    while (isDIGIT(*patend) || *patend == '*')
		patend++;
	}
	else
	    patend++;
    }
    while (pat < patend) {
      reparse:
	datumtype = *pat++ & 0xFF;
#ifdef PERL_NATINT_PACK
	natint = 0;
#endif
	if (isSPACE(datumtype))
	    continue;
	if (datumtype == '#') {
	    while (pat < patend && *pat != '\n')
		pat++;
	    continue;
	}
	if (*pat == '!') {
	    char *natstr = "sSiIlL";

	    if (strchr(natstr, datumtype)) {
#ifdef PERL_NATINT_PACK
		natint = 1;
#endif
		pat++;
	    }
	    else
		DIE(aTHX_ "'!' allowed only after types %s", natstr);
	}
	star = 0;
	if (pat >= patend)
	    len = 1;
	else if (*pat == '*') {
	    len = strend - strbeg;	/* long enough */
	    pat++;
	    star = 1;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat)) {
		len = (len * 10) + (*pat++ - '0');
		if (len < 0)
		    DIE(aTHX_ "Repeat count in unpack overflows");
	    }
	}
	else
	    len = (datumtype != '@');
      redo_switch:
	switch(datumtype) {
	default:
	    DIE(aTHX_ "Invalid type in unpack: '%c'", (int)datumtype);
	case ',': /* grandfather in commas but with a warning */
	    if (commas++ == 0 && ckWARN(WARN_UNPACK))
		Perl_warner(aTHX_ WARN_UNPACK,
			    "Invalid type in unpack: '%c'", (int)datumtype);
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
		DIE(aTHX_ "@ outside of string");
	    s = strbeg + len;
	    break;
	case 'X':
	    if (len > s - strbeg)
		DIE(aTHX_ "X outside of string");
	    s -= len;
	    break;
	case 'x':
	    if (len > strend - s)
		DIE(aTHX_ "x outside of string");
	    s += len;
	    break;
	case '/':
	    if (start_sp_offset >= SP - PL_stack_base)
		DIE(aTHX_ "/ must follow a numeric type");
	    datumtype = *pat++;
	    if (*pat == '*')
		pat++;		/* ignore '*' for compatibility with pack */
	    if (isDIGIT(*pat))
		DIE(aTHX_ "/ cannot take a count" );
	    len = POPi;
	    star = 0;
	    goto redo_switch;
	case 'A':
	case 'Z':
	case 'a':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum)
		goto uchar_checksum;
	    sv = NEWSV(35, len);
	    sv_setpvn(sv, s, len);
	    s += len;
	    if (datumtype == 'A' || datumtype == 'Z') {
		aptr = s;	/* borrow register */
		if (datumtype == 'Z') {	/* 'Z' strips stuff after first null */
		    s = SvPVX(sv);
		    while (*s)
			s++;
		}
		else {		/* 'A' strips both nulls and spaces */
		    s = SvPVX(sv) + len - 1;
		    while (s >= SvPVX(sv) && (!*s || isSPACE(*s)))
			s--;
		    *++s = '\0';
		}
		SvCUR_set(sv, s - SvPVX(sv));
		s = aptr;	/* unborrow register */
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'B':
	case 'b':
	    if (star || len > (strend - s) * 8)
		len = (strend - s) * 8;
	    if (checksum) {
		if (!PL_bitcount) {
		    Newz(601, PL_bitcount, 256, char);
		    for (bits = 1; bits < 256; bits++) {
			if (bits & 1)	PL_bitcount[bits]++;
			if (bits & 2)	PL_bitcount[bits]++;
			if (bits & 4)	PL_bitcount[bits]++;
			if (bits & 8)	PL_bitcount[bits]++;
			if (bits & 16)	PL_bitcount[bits]++;
			if (bits & 32)	PL_bitcount[bits]++;
			if (bits & 64)	PL_bitcount[bits]++;
			if (bits & 128)	PL_bitcount[bits]++;
		    }
		}
		while (len >= 8) {
		    culong += PL_bitcount[*(unsigned char*)s++];
		    len -= 8;
		}
		if (len) {
		    bits = *s;
		    if (datumtype == 'b') {
			while (len-- > 0) {
			    if (bits & 1) culong++;
			    bits >>= 1;
			}
		    }
		    else {
			while (len-- > 0) {
			    if (bits & 128) culong++;
			    bits <<= 1;
			}
		    }
		}
		break;
	    }
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    str = SvPVX(sv);
	    if (datumtype == 'b') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)		/*SUPPRESS 595*/
			bits >>= 1;
		    else
			bits = *s++;
		    *str++ = '0' + (bits & 1);
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 7)
			bits <<= 1;
		    else
			bits = *s++;
		    *str++ = '0' + ((bits & 128) != 0);
		}
	    }
	    *str = '\0';
	    XPUSHs(sv_2mortal(sv));
	    break;
	case 'H':
	case 'h':
	    if (star || len > (strend - s) * 2)
		len = (strend - s) * 2;
	    sv = NEWSV(35, len + 1);
	    SvCUR_set(sv, len);
	    SvPOK_on(sv);
	    str = SvPVX(sv);
	    if (datumtype == 'h') {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits >>= 4;
		    else
			bits = *s++;
		    *str++ = PL_hexdigit[bits & 15];
		}
	    }
	    else {
		aint = len;
		for (len = 0; len < aint; len++) {
		    if (len & 1)
			bits <<= 4;
		    else
			bits = *s++;
		    *str++ = PL_hexdigit[(bits >> 4) & 15];
		}
	    }
	    *str = '\0';
	    XPUSHs(sv_2mortal(sv));
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
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    aint = *s++;
		    if (aint >= 128)	/* fake up signed chars */
			aint -= 256;
		    sv = NEWSV(36, 0);
		    sv_setiv(sv, (IV)aint);
		    PUSHs(sv_2mortal(sv));
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
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    auint = *s++ & 255;
		    sv = NEWSV(37, 0);
		    sv_setiv(sv, (IV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'U':
	    if (len > strend - s)
		len = strend - s;
	    if (checksum) {
		while (len-- > 0 && s < strend) {
		    STRLEN alen;
		    auint = utf8_to_uv((U8*)s, strend - s, &alen, 0);
		    along = alen;
		    s += along;
		    if (checksum > 32)
			cdouble += (NV)auint;
		    else
			culong += auint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0 && s < strend) {
		    STRLEN alen;
		    auint = utf8_to_uv((U8*)s, strend - s, &alen, 0);
		    along = alen;
		    s += along;
		    sv = NEWSV(37, 0);
		    sv_setuv(sv, (UV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 's':
#if SHORTSIZE == SIZE16
	    along = (strend - s) / SIZE16;
#else
	    along = (strend - s) / (natint ? sizeof(short) : SIZE16);
#endif
	    if (len > along)
		len = along;
	    if (checksum) {
#if SHORTSIZE != SIZE16
		if (natint) {
		    short ashort;
		    while (len-- > 0) {
			COPYNN(s, &ashort, sizeof(short));
			s += sizeof(short);
			culong += ashort;

		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
			COPY16(s, &ashort);
#if SHORTSIZE > SIZE16
			if (ashort > 32767)
			  ashort -= 65536;
#endif
			s += SIZE16;
			culong += ashort;
		    }
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
#if SHORTSIZE != SIZE16
		if (natint) {
		    short ashort;
		    while (len-- > 0) {
			COPYNN(s, &ashort, sizeof(short));
			s += sizeof(short);
			sv = NEWSV(38, 0);
			sv_setiv(sv, (IV)ashort);
			PUSHs(sv_2mortal(sv));
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
			COPY16(s, &ashort);
#if SHORTSIZE > SIZE16
			if (ashort > 32767)
			  ashort -= 65536;
#endif
			s += SIZE16;
			sv = NEWSV(38, 0);
			sv_setiv(sv, (IV)ashort);
			PUSHs(sv_2mortal(sv));
		    }
		}
	    }
	    break;
	case 'v':
	case 'n':
	case 'S':
#if SHORTSIZE == SIZE16
	    along = (strend - s) / SIZE16;
#else
	    unatint = natint && datumtype == 'S';
	    along = (strend - s) / (unatint ? sizeof(unsigned short) : SIZE16);
#endif
	    if (len > along)
		len = along;
	    if (checksum) {
#if SHORTSIZE != SIZE16
		if (unatint) {
		    unsigned short aushort;
		    while (len-- > 0) {
			COPYNN(s, &aushort, sizeof(unsigned short));
			s += sizeof(unsigned short);
			culong += aushort;
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
			COPY16(s, &aushort);
			s += SIZE16;
#ifdef HAS_NTOHS
			if (datumtype == 'n')
			    aushort = PerlSock_ntohs(aushort);
#endif
#ifdef HAS_VTOHS
			if (datumtype == 'v')
			    aushort = vtohs(aushort);
#endif
			culong += aushort;
		    }
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
#if SHORTSIZE != SIZE16
		if (unatint) {
		    unsigned short aushort;
		    while (len-- > 0) {
			COPYNN(s, &aushort, sizeof(unsigned short));
			s += sizeof(unsigned short);
			sv = NEWSV(39, 0);
			sv_setiv(sv, (UV)aushort);
			PUSHs(sv_2mortal(sv));
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
			COPY16(s, &aushort);
			s += SIZE16;
			sv = NEWSV(39, 0);
#ifdef HAS_NTOHS
			if (datumtype == 'n')
			    aushort = PerlSock_ntohs(aushort);
#endif
#ifdef HAS_VTOHS
			if (datumtype == 'v')
			    aushort = vtohs(aushort);
#endif
			sv_setiv(sv, (UV)aushort);
			PUSHs(sv_2mortal(sv));
		    }
		}
	    }
	    break;
	case 'i':
	    along = (strend - s) / sizeof(int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    if (checksum > 32)
			cdouble += (NV)aint;
		    else
			culong += aint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &aint, 1, int);
		    s += sizeof(int);
		    sv = NEWSV(40, 0);
#ifdef __osf__
                    /* Without the dummy below unpack("i", pack("i",-1))
                     * return 0xFFffFFff instead of -1 for Digital Unix V4.0
                     * cc with optimization turned on.
		     *
		     * The bug was detected in
		     * DEC C V5.8-009 on Digital UNIX V4.0 (Rev. 1091) (V4.0E)
		     * with optimization (-O4) turned on.
		     * DEC C V5.2-040 on Digital UNIX V4.0 (Rev. 564) (V4.0B)
		     * does not have this problem even with -O4.
		     *
		     * This bug was reported as DECC_BUGS 1431
		     * and tracked internally as GEM_BUGS 7775.
		     *
		     * The bug is fixed in
		     * Tru64 UNIX V5.0:      Compaq C V6.1-006 or later
		     * UNIX V4.0F support:   DEC C V5.9-006 or later
		     * UNIX V4.0E support:   DEC C V5.8-011 or later
		     * and also in DTK.
		     *
		     * See also few lines later for the same bug.
		     */
                    (aint) ?
		    	sv_setiv(sv, (IV)aint) :
#endif
		    sv_setiv(sv, (IV)aint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'I':
	    along = (strend - s) / sizeof(unsigned int);
	    if (len > along)
		len = along;
	    if (checksum) {
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    if (checksum > 32)
			cdouble += (NV)auint;
		    else
			culong += auint;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &auint, 1, unsigned int);
		    s += sizeof(unsigned int);
		    sv = NEWSV(41, 0);
#ifdef __osf__
                    /* Without the dummy below unpack("I", pack("I",0xFFFFFFFF))
                     * returns 1.84467440737096e+19 instead of 0xFFFFFFFF.
		     * See details few lines earlier. */
                    (auint) ?
		        sv_setuv(sv, (UV)auint) :
#endif
		    sv_setuv(sv, (UV)auint);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'l':
#if LONGSIZE == SIZE32
	    along = (strend - s) / SIZE32;
#else
	    along = (strend - s) / (natint ? sizeof(long) : SIZE32);
#endif
	    if (len > along)
		len = along;
	    if (checksum) {
#if LONGSIZE != SIZE32
		if (natint) {
		    while (len-- > 0) {
			COPYNN(s, &along, sizeof(long));
			s += sizeof(long);
			if (checksum > 32)
			    cdouble += (NV)along;
			else
			    culong += along;
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
#if LONGSIZE > SIZE32 && INTSIZE == SIZE32
			I32 along;
#endif
			COPY32(s, &along);
#if LONGSIZE > SIZE32
			if (along > 2147483647)
			  along -= 4294967296;
#endif
			s += SIZE32;
			if (checksum > 32)
			    cdouble += (NV)along;
			else
			    culong += along;
		    }
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
#if LONGSIZE != SIZE32
		if (natint) {
		    while (len-- > 0) {
			COPYNN(s, &along, sizeof(long));
			s += sizeof(long);
			sv = NEWSV(42, 0);
			sv_setiv(sv, (IV)along);
			PUSHs(sv_2mortal(sv));
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
#if LONGSIZE > SIZE32 && INTSIZE == SIZE32
			I32 along;
#endif
			COPY32(s, &along);
#if LONGSIZE > SIZE32
			if (along > 2147483647)
			  along -= 4294967296;
#endif
			s += SIZE32;
			sv = NEWSV(42, 0);
			sv_setiv(sv, (IV)along);
			PUSHs(sv_2mortal(sv));
		    }
		}
	    }
	    break;
	case 'V':
	case 'N':
	case 'L':
#if LONGSIZE == SIZE32
	    along = (strend - s) / SIZE32;
#else
	    unatint = natint && datumtype == 'L';
	    along = (strend - s) / (unatint ? sizeof(unsigned long) : SIZE32);
#endif
	    if (len > along)
		len = along;
	    if (checksum) {
#if LONGSIZE != SIZE32
		if (unatint) {
		    unsigned long aulong;
		    while (len-- > 0) {
			COPYNN(s, &aulong, sizeof(unsigned long));
			s += sizeof(unsigned long);
			if (checksum > 32)
			    cdouble += (NV)aulong;
			else
			    culong += aulong;
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
			COPY32(s, &aulong);
			s += SIZE32;
#ifdef HAS_NTOHL
			if (datumtype == 'N')
			    aulong = PerlSock_ntohl(aulong);
#endif
#ifdef HAS_VTOHL
			if (datumtype == 'V')
			    aulong = vtohl(aulong);
#endif
			if (checksum > 32)
			    cdouble += (NV)aulong;
			else
			    culong += aulong;
		    }
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
#if LONGSIZE != SIZE32
		if (unatint) {
		    unsigned long aulong;
		    while (len-- > 0) {
			COPYNN(s, &aulong, sizeof(unsigned long));
			s += sizeof(unsigned long);
			sv = NEWSV(43, 0);
			sv_setuv(sv, (UV)aulong);
			PUSHs(sv_2mortal(sv));
		    }
		}
		else
#endif
                {
		    while (len-- > 0) {
			COPY32(s, &aulong);
			s += SIZE32;
#ifdef HAS_NTOHL
			if (datumtype == 'N')
			    aulong = PerlSock_ntohl(aulong);
#endif
#ifdef HAS_VTOHL
			if (datumtype == 'V')
			    aulong = vtohl(aulong);
#endif
			sv = NEWSV(43, 0);
			sv_setuv(sv, (UV)aulong);
			PUSHs(sv_2mortal(sv));
		    }
		}
	    }
	    break;
	case 'p':
	    along = (strend - s) / sizeof(char*);
	    if (len > along)
		len = along;
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (sizeof(char*) > strend - s)
		    break;
		else {
		    Copy(s, &aptr, 1, char*);
		    s += sizeof(char*);
		}
		sv = NEWSV(44, 0);
		if (aptr)
		    sv_setpv(sv, aptr);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'w':
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    {
		UV auv = 0;
		U32 bytes = 0;
		
		while ((len > 0) && (s < strend)) {
		    auv = (auv << 7) | (*s & 0x7f);
		    if (UTF8_IS_ASCII(*s++)) {
			bytes = 0;
			sv = NEWSV(40, 0);
			sv_setuv(sv, auv);
			PUSHs(sv_2mortal(sv));
			len--;
			auv = 0;
		    }
		    else if (++bytes >= sizeof(UV)) {	/* promote to string */
			char *t;
			STRLEN n_a;

			sv = Perl_newSVpvf(aTHX_ "%.*"UVf, (int)TYPE_DIGITS(UV), auv);
			while (s < strend) {
			    sv = mul128(sv, *s & 0x7f);
			    if (!(*s++ & 0x80)) {
				bytes = 0;
				break;
			    }
			}
			t = SvPV(sv, n_a);
			while (*t == '0')
			    t++;
			sv_chop(sv, t);
			PUSHs(sv_2mortal(sv));
			len--;
			auv = 0;
		    }
		}
		if ((s >= strend) && bytes)
		    DIE(aTHX_ "Unterminated compressed integer");
	    }
	    break;
	case 'P':
	    EXTEND(SP, 1);
	    if (sizeof(char*) > strend - s)
		break;
	    else {
		Copy(s, &aptr, 1, char*);
		s += sizeof(char*);
	    }
	    sv = NEWSV(44, 0);
	    if (aptr)
		sv_setpvn(sv, aptr, len);
	    PUSHs(sv_2mortal(sv));
	    break;
#ifdef HAS_QUAD
	case 'q':
	    along = (strend - s) / sizeof(Quad_t);
	    if (len > along)
		len = along;
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (s + sizeof(Quad_t) > strend)
		    aquad = 0;
		else {
		    Copy(s, &aquad, 1, Quad_t);
		    s += sizeof(Quad_t);
		}
		sv = NEWSV(42, 0);
		if (aquad >= IV_MIN && aquad <= IV_MAX)
		    sv_setiv(sv, (IV)aquad);
		else
		    sv_setnv(sv, (NV)aquad);
		PUSHs(sv_2mortal(sv));
	    }
	    break;
	case 'Q':
	    along = (strend - s) / sizeof(Quad_t);
	    if (len > along)
		len = along;
	    EXTEND(SP, len);
	    EXTEND_MORTAL(len);
	    while (len-- > 0) {
		if (s + sizeof(Uquad_t) > strend)
		    auquad = 0;
		else {
		    Copy(s, &auquad, 1, Uquad_t);
		    s += sizeof(Uquad_t);
		}
		sv = NEWSV(43, 0);
		if (auquad <= UV_MAX)
		    sv_setuv(sv, (UV)auquad);
		else
		    sv_setnv(sv, (NV)auquad);
		PUSHs(sv_2mortal(sv));
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
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    cdouble += afloat;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &afloat, 1, float);
		    s += sizeof(float);
		    sv = NEWSV(47, 0);
		    sv_setnv(sv, (NV)afloat);
		    PUSHs(sv_2mortal(sv));
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
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    cdouble += adouble;
		}
	    }
	    else {
		EXTEND(SP, len);
		EXTEND_MORTAL(len);
		while (len-- > 0) {
		    Copy(s, &adouble, 1, double);
		    s += sizeof(double);
		    sv = NEWSV(48, 0);
		    sv_setnv(sv, (NV)adouble);
		    PUSHs(sv_2mortal(sv));
		}
	    }
	    break;
	case 'u':
	    /* MKS:
	     * Initialise the decode mapping.  By using a table driven
             * algorithm, the code will be character-set independent
             * (and just as fast as doing character arithmetic)
             */
            if (PL_uudmap['M'] == 0) {
                int i;
 
                for (i = 0; i < sizeof(PL_uuemap); i += 1)
                    PL_uudmap[(U8)PL_uuemap[i]] = i;
                /*
                 * Because ' ' and '`' map to the same value,
                 * we need to decode them both the same.
                 */
                PL_uudmap[' '] = 0;
            }

	    along = (strend - s) * 3 / 4;
	    sv = NEWSV(42, along);
	    if (along)
		SvPOK_on(sv);
	    while (s < strend && *s > ' ' && ISUUCHAR(*s)) {
		I32 a, b, c, d;
		char hunk[4];

		hunk[3] = '\0';
		len = PL_uudmap[*(U8*)s++] & 077;
		while (len > 0) {
		    if (s < strend && ISUUCHAR(*s))
			a = PL_uudmap[*(U8*)s++] & 077;
 		    else
 			a = 0;
		    if (s < strend && ISUUCHAR(*s))
			b = PL_uudmap[*(U8*)s++] & 077;
 		    else
 			b = 0;
		    if (s < strend && ISUUCHAR(*s))
			c = PL_uudmap[*(U8*)s++] & 077;
 		    else
 			c = 0;
		    if (s < strend && ISUUCHAR(*s))
			d = PL_uudmap[*(U8*)s++] & 077;
		    else
			d = 0;
		    hunk[0] = (a << 2) | (b >> 4);
		    hunk[1] = (b << 4) | (c >> 2);
		    hunk[2] = (c << 6) | d;
		    sv_catpvn(sv, hunk, (len > 3) ? 3 : len);
		    len -= 3;
		}
		if (*s == '\n')
		    s++;
		else if (s[1] == '\n')		/* possible checksum byte */
		    s += 2;
	    }
	    XPUSHs(sv_2mortal(sv));
	    break;
	}
	if (checksum) {
	    sv = NEWSV(42, 0);
	    if (strchr("fFdD", datumtype) ||
	      (checksum > 32 && strchr("iIlLNU", datumtype)) ) {
		NV trouble;

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
		cdouble = Perl_modf(cdouble / adouble, &trouble) * adouble;
		sv_setnv(sv, cdouble);
	    }
	    else {
		if (checksum < 32) {
		    aulong = (1 << checksum) - 1;
		    culong &= aulong;
		}
		sv_setuv(sv, (UV)culong);
	    }
	    XPUSHs(sv_2mortal(sv));
	    checksum = 0;
	}
    }
    if (SP - PL_stack_base == start_sp_offset && gimme == G_SCALAR)
	PUSHs(&PL_sv_undef);
    RETURN;
}

STATIC void
S_doencodes(pTHX_ register SV *sv, register char *s, register I32 len)
{
    char hunk[5];

    *hunk = PL_uuemap[len];
    sv_catpvn(sv, hunk, 1);
    hunk[4] = '\0';
    while (len > 2) {
	hunk[0] = PL_uuemap[(077 & (*s >> 2))];
	hunk[1] = PL_uuemap[(077 & (((*s << 4) & 060) | ((s[1] >> 4) & 017)))];
	hunk[2] = PL_uuemap[(077 & (((s[1] << 2) & 074) | ((s[2] >> 6) & 03)))];
	hunk[3] = PL_uuemap[(077 & (s[2] & 077))];
	sv_catpvn(sv, hunk, 4);
	s += 3;
	len -= 3;
    }
    if (len > 0) {
	char r = (len > 1 ? s[1] : '\0');
	hunk[0] = PL_uuemap[(077 & (*s >> 2))];
	hunk[1] = PL_uuemap[(077 & (((*s << 4) & 060) | ((r >> 4) & 017)))];
	hunk[2] = PL_uuemap[(077 & ((r << 2) & 074))];
	hunk[3] = PL_uuemap[0];
	sv_catpvn(sv, hunk, 4);
    }
    sv_catpvn(sv, "\n", 1);
}

STATIC SV *
S_is_an_int(pTHX_ char *s, STRLEN l)
{
  STRLEN	 n_a;
  SV             *result = newSVpvn(s, l);
  char           *result_c = SvPV(result, n_a);	/* convenience */
  char           *out = result_c;
  bool            skip = 1;
  bool            ignore = 0;

  while (*s) {
    switch (*s) {
    case ' ':
      break;
    case '+':
      if (!skip) {
	SvREFCNT_dec(result);
	return (NULL);
      }
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      skip = 0;
      if (!ignore) {
	*(out++) = *s;
      }
      break;
    case '.':
      ignore = 1;
      break;
    default:
      SvREFCNT_dec(result);
      return (NULL);
    }
    s++;
  }
  *(out++) = '\0';
  SvCUR_set(result, out - result_c);
  return (result);
}

/* pnum must be '\0' terminated */
STATIC int
S_div128(pTHX_ SV *pnum, bool *done)
{
  STRLEN          len;
  char           *s = SvPV(pnum, len);
  int             m = 0;
  int             r = 0;
  char           *t = s;

  *done = 1;
  while (*t) {
    int             i;

    i = m * 10 + (*t - '0');
    m = i & 0x7F;
    r = (i >> 7);		/* r < 10 */
    if (r) {
      *done = 0;
    }
    *(t++) = '0' + r;
  }
  *(t++) = '\0';
  SvCUR_set(pnum, (STRLEN) (t - s));
  return (m);
}


PP(pp_pack)
{
    dSP; dMARK; dORIGMARK; dTARGET;
    register SV *cat = TARG;
    register I32 items;
    STRLEN fromlen;
    register char *pat = SvPVx(*++MARK, fromlen);
    char *patcopy;
    register char *patend = pat + fromlen;
    register I32 len;
    I32 datumtype;
    SV *fromstr;
    /*SUPPRESS 442*/
    static char null10[] = {0,0,0,0,0,0,0,0,0,0};
    static char *space10 = "          ";

    /* These must not be in registers: */
    char achar;
    I16 ashort;
    int aint;
    unsigned int auint;
    I32 along;
    U32 aulong;
#ifdef HAS_QUAD
    Quad_t aquad;
    Uquad_t auquad;
#endif
    char *aptr;
    float afloat;
    double adouble;
    int commas = 0;
#ifdef PERL_NATINT_PACK
    int natint;		/* native integer */
#endif

    items = SP - MARK;
    MARK++;
    sv_setpvn(cat, "", 0);
    patcopy = pat;
    while (pat < patend) {
	SV *lengthcode = Nullsv;
#define NEXTFROM ( lengthcode ? lengthcode : items-- > 0 ? *MARK++ : &PL_sv_no)
	datumtype = *pat++ & 0xFF;
#ifdef PERL_NATINT_PACK
	natint = 0;
#endif
	if (isSPACE(datumtype)) {
	    patcopy++;
	    continue;
        }
	if (datumtype == 'U' && pat == patcopy+1) 
	    SvUTF8_on(cat);
	if (datumtype == '#') {
	    while (pat < patend && *pat != '\n')
		pat++;
	    continue;
	}
        if (*pat == '!') {
	    char *natstr = "sSiIlL";

	    if (strchr(natstr, datumtype)) {
#ifdef PERL_NATINT_PACK
		natint = 1;
#endif
		pat++;
	    }
	    else
		DIE(aTHX_ "'!' allowed only after types %s", natstr);
	}
	if (*pat == '*') {
	    len = strchr("@Xxu", datumtype) ? 0 : items;
	    pat++;
	}
	else if (isDIGIT(*pat)) {
	    len = *pat++ - '0';
	    while (isDIGIT(*pat)) {
		len = (len * 10) + (*pat++ - '0');
		if (len < 0)
		    DIE(aTHX_ "Repeat count in pack overflows");
	    }
	}
	else
	    len = 1;
	if (*pat == '/') {
	    ++pat;
	    if ((*pat != 'a' && *pat != 'A' && *pat != 'Z') || pat[1] != '*')
		DIE(aTHX_ "/ must be followed by a*, A* or Z*");
	    lengthcode = sv_2mortal(newSViv(sv_len(items > 0
						   ? *MARK : &PL_sv_no)
                                            + (*pat == 'Z' ? 1 : 0)));
	}
	switch(datumtype) {
	default:
	    DIE(aTHX_ "Invalid type in pack: '%c'", (int)datumtype);
	case ',': /* grandfather in commas but with a warning */
	    if (commas++ == 0 && ckWARN(WARN_PACK))
		Perl_warner(aTHX_ WARN_PACK,
			    "Invalid type in pack: '%c'", (int)datumtype);
	    break;
	case '%':
	    DIE(aTHX_ "%% may only be used in unpack");
	case '@':
	    len -= SvCUR(cat);
	    if (len > 0)
		goto grow;
	    len = -len;
	    if (len > 0)
		goto shrink;
	    break;
	case 'X':
	  shrink:
	    if (SvCUR(cat) < len)
		DIE(aTHX_ "X outside of string");
	    SvCUR(cat) -= len;
	    *SvEND(cat) = '\0';
	    break;
	case 'x':
	  grow:
	    while (len >= 10) {
		sv_catpvn(cat, null10, 10);
		len -= 10;
	    }
	    sv_catpvn(cat, null10, len);
	    break;
	case 'A':
	case 'Z':
	case 'a':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    if (pat[-1] == '*') {
		len = fromlen;
		if (datumtype == 'Z')
		    ++len;
	    }
	    if (fromlen >= len) {
		sv_catpvn(cat, aptr, len);
		if (datumtype == 'Z')
		    *(SvEND(cat)-1) = '\0';
	    }
	    else {
		sv_catpvn(cat, aptr, fromlen);
		len -= fromlen;
		if (datumtype == 'A') {
		    while (len >= 10) {
			sv_catpvn(cat, space10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, space10, len);
		}
		else {
		    while (len >= 10) {
			sv_catpvn(cat, null10, 10);
			len -= 10;
		    }
		    sv_catpvn(cat, null10, len);
		}
	    }
	    break;
	case 'B':
	case 'b':
	    {
		register char *str;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		str = SvPV(fromstr, fromlen);
		if (pat[-1] == '*')
		    len = fromlen;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+7)/8;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'B') {
		    for (len = 0; len++ < aint;) {
			items |= *str++ & 1;
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
			if (*str++ & 1)
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
		str = SvPVX(cat) + SvCUR(cat);
		while (aptr <= str)
		    *aptr++ = '\0';

		items = saveitems;
	    }
	    break;
	case 'H':
	case 'h':
	    {
		register char *str;
		I32 saveitems;

		fromstr = NEXTFROM;
		saveitems = items;
		str = SvPV(fromstr, fromlen);
		if (pat[-1] == '*')
		    len = fromlen;
		aint = SvCUR(cat);
		SvCUR(cat) += (len+1)/2;
		SvGROW(cat, SvCUR(cat) + 1);
		aptr = SvPVX(cat) + aint;
		if (len > fromlen)
		    len = fromlen;
		aint = len;
		items = 0;
		if (datumtype == 'H') {
		    for (len = 0; len++ < aint;) {
			if (isALPHA(*str))
			    items |= ((*str++ & 15) + 9) & 15;
			else
			    items |= *str++ & 15;
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
			if (isALPHA(*str))
			    items |= (((*str++ & 15) + 9) & 15) << 4;
			else
			    items |= (*str++ & 15) << 4;
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
		str = SvPVX(cat) + SvCUR(cat);
		while (aptr <= str)
		    *aptr++ = '\0';

		items = saveitems;
	    }
	    break;
	case 'C':
	case 'c':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		achar = aint;
		sv_catpvn(cat, &achar, sizeof(char));
	    }
	    break;
	case 'U':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = SvUV(fromstr);
		SvGROW(cat, SvCUR(cat) + UTF8_MAXLEN + 1);
		SvCUR_set(cat, (char*)uv_to_utf8((U8*)SvEND(cat),auint)
			       - SvPVX(cat));
	    }
	    *SvEND(cat) = '\0';
	    break;
	/* Float and double added by gnb@melba.bby.oz.au  22/11/89 */
	case 'f':
	case 'F':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		afloat = (float)SvNV(fromstr);
		sv_catpvn(cat, (char *)&afloat, sizeof (float));
	    }
	    break;
	case 'd':
	case 'D':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		adouble = (double)SvNV(fromstr);
		sv_catpvn(cat, (char *)&adouble, sizeof (double));
	    }
	    break;
	case 'n':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTONS
		ashort = PerlSock_htons(ashort);
#endif
		CAT16(cat, &ashort);
	    }
	    break;
	case 'v':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		ashort = (I16)SvIV(fromstr);
#ifdef HAS_HTOVS
		ashort = htovs(ashort);
#endif
		CAT16(cat, &ashort);
	    }
	    break;
	case 'S':
#if SHORTSIZE != SIZE16
	    if (natint) {
		unsigned short aushort;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aushort = SvUV(fromstr);
		    sv_catpvn(cat, (char *)&aushort, sizeof(unsigned short));
		}
	    }
	    else
#endif
            {
		U16 aushort;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aushort = (U16)SvUV(fromstr);
		    CAT16(cat, &aushort);
		}

	    }
	    break;
	case 's':
#if SHORTSIZE != SIZE16
	    if (natint) {
		short ashort;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    ashort = SvIV(fromstr);
		    sv_catpvn(cat, (char *)&ashort, sizeof(short));
		}
	    }
	    else
#endif
            {
		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    ashort = (I16)SvIV(fromstr);
		    CAT16(cat, &ashort);
		}
	    }
	    break;
	case 'I':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auint = SvUV(fromstr);
		sv_catpvn(cat, (char*)&auint, sizeof(unsigned int));
	    }
	    break;
	case 'w':
            while (len-- > 0) {
		fromstr = NEXTFROM;
		adouble = Perl_floor(SvNV(fromstr));

		if (adouble < 0)
		    DIE(aTHX_ "Cannot compress negative numbers");

		if (
#if UVSIZE > 4 && UVSIZE >= NVSIZE
		    adouble <= 0xffffffff
#else
#   ifdef CXUX_BROKEN_CONSTANT_CONVERT
		    adouble <= UV_MAX_cxux
#   else
		    adouble <= UV_MAX
#   endif
#endif
		    )
		{
		    char   buf[1 + sizeof(UV)];
		    char  *in = buf + sizeof(buf);
		    UV     auv = U_V(adouble);

		    do {
			*--in = (auv & 0x7f) | 0x80;
			auv >>= 7;
		    } while (auv);
		    buf[sizeof(buf) - 1] &= 0x7f; /* clear continue bit */
		    sv_catpvn(cat, in, (buf + sizeof(buf)) - in);
		}
		else if (SvPOKp(fromstr)) {  /* decimal string arithmetics */
		    char           *from, *result, *in;
		    SV             *norm;
		    STRLEN          len;
		    bool            done;

		    /* Copy string and check for compliance */
		    from = SvPV(fromstr, len);
		    if ((norm = is_an_int(from, len)) == NULL)
			DIE(aTHX_ "can compress only unsigned integer");

		    New('w', result, len, char);
		    in = result + len;
		    done = FALSE;
		    while (!done)
			*--in = div128(norm, &done) | 0x80;
		    result[len - 1] &= 0x7F; /* clear continue bit */
		    sv_catpvn(cat, in, (result + len) - in);
		    Safefree(result);
		    SvREFCNT_dec(norm);	/* free norm */
                }
		else if (SvNOKp(fromstr)) {
		    char   buf[sizeof(double) * 2];	/* 8/7 <= 2 */
		    char  *in = buf + sizeof(buf);

		    do {
			double next = floor(adouble / 128);
			*--in = (unsigned char)(adouble - (next * 128)) | 0x80;
			if (in <= buf)  /* this cannot happen ;-) */
			    DIE(aTHX_ "Cannot compress integer");
			in--;
			adouble = next;
		    } while (adouble > 0);
		    buf[sizeof(buf) - 1] &= 0x7f; /* clear continue bit */
		    sv_catpvn(cat, in, (buf + sizeof(buf)) - in);
		}
		else
		    DIE(aTHX_ "Cannot compress non integer");
	    }
            break;
	case 'i':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aint = SvIV(fromstr);
		sv_catpvn(cat, (char*)&aint, sizeof(int));
	    }
	    break;
	case 'N':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
#ifdef HAS_HTONL
		aulong = PerlSock_htonl(aulong);
#endif
		CAT32(cat, &aulong);
	    }
	    break;
	case 'V':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aulong = SvUV(fromstr);
#ifdef HAS_HTOVL
		aulong = htovl(aulong);
#endif
		CAT32(cat, &aulong);
	    }
	    break;
	case 'L':
#if LONGSIZE != SIZE32
	    if (natint) {
		unsigned long aulong;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aulong = SvUV(fromstr);
		    sv_catpvn(cat, (char *)&aulong, sizeof(unsigned long));
		}
	    }
	    else
#endif
            {
		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    aulong = SvUV(fromstr);
		    CAT32(cat, &aulong);
		}
	    }
	    break;
	case 'l':
#if LONGSIZE != SIZE32
	    if (natint) {
		long along;

		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    along = SvIV(fromstr);
		    sv_catpvn(cat, (char *)&along, sizeof(long));
		}
	    }
	    else
#endif
            {
		while (len-- > 0) {
		    fromstr = NEXTFROM;
		    along = SvIV(fromstr);
		    CAT32(cat, &along);
		}
	    }
	    break;
#ifdef HAS_QUAD
	case 'Q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		auquad = (Uquad_t)SvUV(fromstr);
		sv_catpvn(cat, (char*)&auquad, sizeof(Uquad_t));
	    }
	    break;
	case 'q':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		aquad = (Quad_t)SvIV(fromstr);
		sv_catpvn(cat, (char*)&aquad, sizeof(Quad_t));
	    }
	    break;
#endif
	case 'P':
	    len = 1;		/* assume SV is correct length */
	    /* FALL THROUGH */
	case 'p':
	    while (len-- > 0) {
		fromstr = NEXTFROM;
		if (fromstr == &PL_sv_undef)
		    aptr = NULL;
		else {
		    STRLEN n_a;
		    /* XXX better yet, could spirit away the string to
		     * a safe spot and hang on to it until the result
		     * of pack() (and all copies of the result) are
		     * gone.
		     */
		    if (ckWARN(WARN_PACK) && (SvTEMP(fromstr)
						|| (SvPADTMP(fromstr)
						    && !SvREADONLY(fromstr))))
		    {
			Perl_warner(aTHX_ WARN_PACK,
				"Attempt to pack pointer to temporary value");
		    }
		    if (SvPOK(fromstr) || SvNIOK(fromstr))
			aptr = SvPV(fromstr,n_a);
		    else
			aptr = SvPV_force(fromstr,n_a);
		}
		sv_catpvn(cat, (char*)&aptr, sizeof(char*));
	    }
	    break;
	case 'u':
	    fromstr = NEXTFROM;
	    aptr = SvPV(fromstr, fromlen);
	    SvGROW(cat, fromlen * 4 / 3);
	    if (len <= 1)
		len = 45;
	    else
		len = len / 3 * 3;
	    while (fromlen > 0) {
		I32 todo;

		if (fromlen > len)
		    todo = len;
		else
		    todo = fromlen;
		doencodes(cat, aptr, todo);
		fromlen -= todo;
		aptr += todo;
	    }
	    break;
	}
    }
    SvSETMAGIC(cat);
    SP = ORIGMARK;
    PUSHs(cat);
    RETURN;
}
#undef NEXTFROM


PP(pp_split)
{
    dSP; dTARG;
    AV *ary;
    register IV limit = POPi;			/* note, negative is forever */
    SV *sv = POPs;
    STRLEN len;
    register char *s = SvPV(sv, len);
    bool do_utf8 = DO_UTF8(sv);
    char *strend = s + len;
    register PMOP *pm;
    register REGEXP *rx;
    register SV *dstr;
    register char *m;
    I32 iters = 0;
    STRLEN slen = do_utf8 ? utf8_length((U8*)s, (U8*)strend) : (strend - s);
    I32 maxiters = slen + 10;
    I32 i;
    char *orig;
    I32 origlimit = limit;
    I32 realarray = 0;
    I32 base;
    AV *oldstack = PL_curstack;
    I32 gimme = GIMME_V;
    I32 oldsave = PL_savestack_ix;
    I32 make_mortal = 1;
    MAGIC *mg = (MAGIC *) NULL;

#ifdef DEBUGGING
    Copy(&LvTARGOFF(POPs), &pm, 1, PMOP*);
#else
    pm = (PMOP*)POPs;
#endif
    if (!pm || !s)
	DIE(aTHX_ "panic: pp_split");
    rx = pm->op_pmregexp;

    TAINT_IF((pm->op_pmflags & PMf_LOCALE) &&
	     (pm->op_pmflags & (PMf_WHITE | PMf_SKIPWHITE)));

    if (pm->op_pmreplroot) {
#ifdef USE_ITHREADS
	ary = GvAVn((GV*)PL_curpad[(PADOFFSET)pm->op_pmreplroot]);
#else
	ary = GvAVn((GV*)pm->op_pmreplroot);
#endif
    }
    else if (gimme != G_ARRAY)
#ifdef USE_THREADS
	ary = (AV*)PL_curpad[0];
#else
	ary = GvAVn(PL_defgv);
#endif /* USE_THREADS */
    else
	ary = Nullav;
    if (ary && (gimme != G_ARRAY || (pm->op_pmflags & PMf_ONCE))) {
	realarray = 1;
	PUTBACK;
	av_extend(ary,0);
	av_clear(ary);
	SPAGAIN;
	if ((mg = SvTIED_mg((SV*)ary, 'P'))) {
	    PUSHMARK(SP);
	    XPUSHs(SvTIED_obj((SV*)ary, mg));
	}
	else {
	    if (!AvREAL(ary)) {
		AvREAL_on(ary);
		AvREIFY_off(ary);
		for (i = AvFILLp(ary); i >= 0; i--)
		    AvARRAY(ary)[i] = &PL_sv_undef;	/* don't free mere refs */
	    }
	    /* temporarily switch stacks */
	    SWITCHSTACK(PL_curstack, ary);
	    make_mortal = 0;
	}
    }
    base = SP - PL_stack_base;
    orig = s;
    if (pm->op_pmflags & PMf_SKIPWHITE) {
	if (pm->op_pmflags & PMf_LOCALE) {
	    while (isSPACE_LC(*s))
		s++;
	}
	else {
	    while (isSPACE(*s))
		s++;
	}
    }
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(PL_multiline);
	PL_multiline = pm->op_pmflags & PMf_MULTILINE;
    }

    if (!limit)
	limit = maxiters + 2;
    if (pm->op_pmflags & PMf_WHITE) {
	while (--limit) {
	    m = s;
	    while (m < strend &&
		   !((pm->op_pmflags & PMf_LOCALE)
		     ? isSPACE_LC(*m) : isSPACE(*m)))
		++m;
	    if (m >= strend)
		break;

	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (make_mortal)
		sv_2mortal(dstr);
	    if (do_utf8)
		(void)SvUTF8_on(dstr);
	    XPUSHs(dstr);

	    s = m + 1;
	    while (s < strend &&
		   ((pm->op_pmflags & PMf_LOCALE)
		    ? isSPACE_LC(*s) : isSPACE(*s)))
		++s;
	}
    }
    else if (strEQ("^", rx->precomp)) {
	while (--limit) {
	    /*SUPPRESS 530*/
	    for (m = s; m < strend && *m != '\n'; m++) ;
	    m++;
	    if (m >= strend)
		break;
	    dstr = NEWSV(30, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (make_mortal)
		sv_2mortal(dstr);
	    if (do_utf8)
		(void)SvUTF8_on(dstr);
	    XPUSHs(dstr);
	    s = m;
	}
    }
    else if ((rx->reganch & RE_USE_INTUIT) && !rx->nparens
	     && (rx->reganch & ROPT_CHECK_ALL)
	     && !(rx->reganch & ROPT_ANCH)) {
	int tail = (rx->reganch & RE_INTUIT_TAIL);
	SV *csv = CALLREG_INTUIT_STRING(aTHX_ rx);

	len = rx->minlen;
	if (len == 1 && !(rx->reganch & ROPT_UTF8) && !tail) {
	    STRLEN n_a;
	    char c = *SvPV(csv, n_a);
	    while (--limit) {
		/*SUPPRESS 530*/
		for (m = s; m < strend && *m != c; m++) ;
		if (m >= strend)
		    break;
		dstr = NEWSV(30, m-s);
		sv_setpvn(dstr, s, m-s);
		if (make_mortal)
		    sv_2mortal(dstr);
		if (do_utf8)
		    (void)SvUTF8_on(dstr);
		XPUSHs(dstr);
		/* The rx->minlen is in characters but we want to step
		 * s ahead by bytes. */
 		if (do_utf8)
		    s = (char*)utf8_hop((U8*)m, len);
 		else
		    s = m + len; /* Fake \n at the end */
	    }
	}
	else {
#ifndef lint
	    while (s < strend && --limit &&
	      (m = fbm_instr((unsigned char*)s, (unsigned char*)strend,
			     csv, PL_multiline ? FBMrf_MULTILINE : 0)) )
#endif
	    {
		dstr = NEWSV(31, m-s);
		sv_setpvn(dstr, s, m-s);
		if (make_mortal)
		    sv_2mortal(dstr);
		if (do_utf8)
		    (void)SvUTF8_on(dstr);
		XPUSHs(dstr);
		/* The rx->minlen is in characters but we want to step
		 * s ahead by bytes. */
 		if (do_utf8)
		    s = (char*)utf8_hop((U8*)m, len);
 		else
		    s = m + len; /* Fake \n at the end */
	    }
	}
    }
    else {
	maxiters += slen * rx->nparens;
	while (s < strend && --limit
/*	       && (!rx->check_substr 
		   || ((s = CALLREG_INTUIT_START(aTHX_ rx, sv, s, strend,
						 0, NULL))))
*/	       && CALLREGEXEC(aTHX_ rx, s, strend, orig,
			      1 /* minend */, sv, NULL, 0))
	{
	    TAINT_IF(RX_MATCH_TAINTED(rx));
	    if (RX_MATCH_COPIED(rx) && rx->subbeg != orig) {
		m = s;
		s = orig;
		orig = rx->subbeg;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0] + orig;
	    dstr = NEWSV(32, m-s);
	    sv_setpvn(dstr, s, m-s);
	    if (make_mortal)
		sv_2mortal(dstr);
	    if (do_utf8)
		(void)SvUTF8_on(dstr);
	    XPUSHs(dstr);
	    if (rx->nparens) {
		for (i = 1; i <= rx->nparens; i++) {
		    s = rx->startp[i] + orig;
		    m = rx->endp[i] + orig;
		    if (m && s) {
			dstr = NEWSV(33, m-s);
			sv_setpvn(dstr, s, m-s);
		    }
		    else
			dstr = NEWSV(33, 0);
		    if (make_mortal)
			sv_2mortal(dstr);
		    if (do_utf8)
			(void)SvUTF8_on(dstr);
		    XPUSHs(dstr);
		}
	    }
	    s = rx->endp[0] + orig;
	}
    }

    LEAVE_SCOPE(oldsave);
    iters = (SP - PL_stack_base) - base;
    if (iters > maxiters)
	DIE(aTHX_ "Split loop");

    /* keep field after final delim? */
    if (s < strend || (iters && origlimit)) {
        STRLEN l = strend - s;
	dstr = NEWSV(34, l);
	sv_setpvn(dstr, s, l);
	if (make_mortal)
	    sv_2mortal(dstr);
	if (do_utf8)
	    (void)SvUTF8_on(dstr);
	XPUSHs(dstr);
	iters++;
    }
    else if (!origlimit) {
	while (iters > 0 && (!TOPs || !SvANY(TOPs) || SvCUR(TOPs) == 0))
	    iters--, SP--;
    }

    if (realarray) {
	if (!mg) {
	    SWITCHSTACK(ary, oldstack);
	    if (SvSMAGICAL(ary)) {
		PUTBACK;
		mg_set((SV*)ary);
		SPAGAIN;
	    }
	    if (gimme == G_ARRAY) {
		EXTEND(SP, iters);
		Copy(AvARRAY(ary), SP + 1, iters, SV*);
		SP += iters;
		RETURN;
	    }
	}
	else {
	    PUTBACK;
	    ENTER;
	    call_method("PUSH",G_SCALAR|G_DISCARD);
	    LEAVE;
	    SPAGAIN;
	    if (gimme == G_ARRAY) {
		/* EXTEND should not be needed - we just popped them */
		EXTEND(SP, iters);
		for (i=0; i < iters; i++) {
		    SV **svp = av_fetch(ary, i, FALSE);
		    PUSHs((svp) ? *svp : &PL_sv_undef);
		}
		RETURN;
	    }
	}
    }
    else {
	if (gimme == G_ARRAY)
	    RETURN;
    }
    if (iters || !pm->op_pmreplroot) {
	GETTARGET;
	PUSHi(iters);
	RETURN;
    }
    RETPUSHUNDEF;
}

#ifdef USE_THREADS
void
Perl_unlock_condpair(pTHX_ void *svv)
{
    MAGIC *mg = mg_find((SV*)svv, 'm');

    if (!mg)
	Perl_croak(aTHX_ "panic: unlock_condpair unlocking non-mutex");
    MUTEX_LOCK(MgMUTEXP(mg));
    if (MgOWNER(mg) != thr)
	Perl_croak(aTHX_ "panic: unlock_condpair unlocking mutex that we don't own");
    MgOWNER(mg) = 0;
    COND_SIGNAL(MgOWNERCONDP(mg));
    DEBUG_S(PerlIO_printf(Perl_debug_log, "0x%"UVxf": unlock 0x%"UVxf"\n",
			  PTR2UV(thr), PTR2UV(svv));)
    MUTEX_UNLOCK(MgMUTEXP(mg));
}
#endif /* USE_THREADS */

PP(pp_lock)
{
    dSP;
    dTOPss;
    SV *retsv = sv;
#ifdef USE_THREADS
    sv_lock(sv);
#endif /* USE_THREADS */
    if (SvTYPE(retsv) == SVt_PVAV || SvTYPE(retsv) == SVt_PVHV
	|| SvTYPE(retsv) == SVt_PVCV) {
	retsv = refto(retsv);
    }
    SETs(retsv);
    RETURN;
}

PP(pp_threadsv)
{
#ifdef USE_THREADS
    dSP;
    EXTEND(SP, 1);
    if (PL_op->op_private & OPpLVAL_INTRO)
	PUSHs(*save_threadsv(PL_op->op_targ));
    else
	PUSHs(THREADSV(PL_op->op_targ));
    RETURN;
#else
    DIE(aTHX_ "tried to access per-thread data in non-threaded perl");
#endif /* USE_THREADS */
}
