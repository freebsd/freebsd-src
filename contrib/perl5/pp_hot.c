/*    pp_hot.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * Then he heard Merry change the note, and up went the Horn-cry of Buckland,
 * shaking the air.
 *
 *            Awake!  Awake!  Fear, Fire, Foes!  Awake!
 *                     Fire, Foes!  Awake!
 */

#include "EXTERN.h"
#define PERL_IN_PP_HOT_C
#include "perl.h"

/* Hot code. */

#ifdef USE_THREADS
static void unset_cvowner(pTHXo_ void *cvarg);
#endif /* USE_THREADS */

PP(pp_const)
{
    dSP;
    XPUSHs(cSVOP_sv);
    RETURN;
}

PP(pp_nextstate)
{
    PL_curcop = (COP*)PL_op;
    TAINT_NOT;		/* Each statement is presumed innocent */
    PL_stack_sp = PL_stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;
    return NORMAL;
}

PP(pp_gvsv)
{
    dSP;
    EXTEND(SP,1);
    if (PL_op->op_private & OPpLVAL_INTRO)
	PUSHs(save_scalar(cGVOP_gv));
    else
	PUSHs(GvSV(cGVOP_gv));
    RETURN;
}

PP(pp_null)
{
    return NORMAL;
}

PP(pp_setstate)
{
    PL_curcop = (COP*)PL_op;
    return NORMAL;
}

PP(pp_pushmark)
{
    PUSHMARK(PL_stack_sp);
    return NORMAL;
}

PP(pp_stringify)
{
    dSP; dTARGET;
    STRLEN len;
    char *s;
    s = SvPV(TOPs,len);
    sv_setpvn(TARG,s,len);
    if (SvUTF8(TOPs))
	SvUTF8_on(TARG);
    else
	SvUTF8_off(TARG);
    SETTARG;
    RETURN;
}

PP(pp_gv)
{
    dSP;
    XPUSHs((SV*)cGVOP_gv);
    RETURN;
}

PP(pp_and)
{
    dSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_sassign)
{
    dSP; dPOPTOPssrl;

    if (PL_op->op_private & OPpASSIGN_BACKWARDS) {
	SV *temp;
	temp = left; left = right; right = temp;
    }
    if (PL_tainting && PL_tainted && !SvTAINTED(left))
	TAINT_NOT;
    SvSetMagicSV(right, left);
    SETs(right);
    RETURN;
}

PP(pp_cond_expr)
{
    dSP;
    if (SvTRUEx(POPs))
	RETURNOP(cLOGOP->op_other);
    else
	RETURNOP(cLOGOP->op_next);
}

PP(pp_unstack)
{
    I32 oldsave;
    TAINT_NOT;		/* Each statement is presumed innocent */
    PL_stack_sp = PL_stack_base + cxstack[cxstack_ix].blk_oldsp;
    FREETMPS;
    oldsave = PL_scopestack[PL_scopestack_ix - 1];
    LEAVE_SCOPE(oldsave);
    return NORMAL;
}

PP(pp_concat)
{
  dSP; dATARGET; tryAMAGICbin(concat,opASSIGN);
  {
    dPOPTOPssrl;
    SV* rcopy = Nullsv;

    if (SvGMAGICAL(left))
        mg_get(left);
    if (TARG == right && SvGMAGICAL(right))
        mg_get(right);

    if (TARG == right && left != right)
	/* Clone since otherwise we cannot prepend. */
	rcopy = sv_2mortal(newSVsv(right));

    if (TARG != left)
	sv_setsv(TARG, left);

    if (TARG == right) {
	if (left == right) {
	    /*  $right = $right . $right; */
	    STRLEN rlen;
	    char *rpv = SvPV(right, rlen);

	    sv_catpvn(TARG, rpv, rlen);
	}
	else /* $right = $left  . $right; */
	    sv_catsv(TARG, rcopy);
    }
    else {
	if (!SvOK(TARG)) /* Avoid warning when concatenating to undef. */
	    sv_setpv(TARG, "");
	/* $other = $left . $right; */
	/* $left  = $left . $right; */
	sv_catsv(TARG, right);
    }

#if defined(PERL_Y2KWARN)
    if ((SvIOK(right) || SvNOK(right)) && ckWARN(WARN_Y2K)) {
	STRLEN n;
	char *s = SvPV(TARG,n);
	if (n >= 2 && s[n-2] == '1' && s[n-1] == '9'
	    && (n == 2 || !isDIGIT(s[n-3])))
	{
	    Perl_warner(aTHX_ WARN_Y2K, "Possible Y2K bug: %s",
			"about to append an integer to '19'");
	}
    }
#endif

    SETTARG;
    RETURN;
  }
}

PP(pp_padsv)
{
    dSP; dTARGET;
    XPUSHs(TARG);
    if (PL_op->op_flags & OPf_MOD) {
	if (PL_op->op_private & OPpLVAL_INTRO)
	    SAVECLEARSV(PL_curpad[PL_op->op_targ]);
        else if (PL_op->op_private & OPpDEREF) {
	    PUTBACK;
	    vivify_ref(PL_curpad[PL_op->op_targ], PL_op->op_private & OPpDEREF);
	    SPAGAIN;
	}
    }
    RETURN;
}

PP(pp_readline)
{
    tryAMAGICunTARGET(iter, 0);
    PL_last_in_gv = (GV*)(*PL_stack_sp--);
    if (SvTYPE(PL_last_in_gv) != SVt_PVGV) {
	if (SvROK(PL_last_in_gv) && SvTYPE(SvRV(PL_last_in_gv)) == SVt_PVGV) 
	    PL_last_in_gv = (GV*)SvRV(PL_last_in_gv);
	else {
	    dSP;
	    XPUSHs((SV*)PL_last_in_gv);
	    PUTBACK;
	    pp_rv2gv();
	    PL_last_in_gv = (GV*)(*PL_stack_sp--);
	}
    }
    return do_readline();
}

PP(pp_eq)
{
    dSP; tryAMAGICbinSET(eq,0);
    {
      dPOPnv;
      SETs(boolSV(TOPn == value));
      RETURN;
    }
}

PP(pp_preinc)
{
    dSP;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	DIE(aTHX_ PL_no_modify);
    if (SvIOK_notUV(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
    	SvIVX(TOPs) != IV_MAX)
    {
	++SvIVX(TOPs);
	SvFLAGS(TOPs) &= ~(SVp_NOK|SVp_POK);
    }
    else
	sv_inc(TOPs);
    SvSETMAGIC(TOPs);
    return NORMAL;
}

PP(pp_or)
{
    dSP;
    if (SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_add)
{
    dSP; dATARGET; tryAMAGICbin(add,opASSIGN);
    {
      dPOPTOPnnrl_ul;
      SETn( left + right );
      RETURN;
    }
}

PP(pp_aelemfast)
{
    dSP;
    AV *av = GvAV(cGVOP_gv);
    U32 lval = PL_op->op_flags & OPf_MOD;
    SV** svp = av_fetch(av, PL_op->op_private, lval);
    SV *sv = (svp ? *svp : &PL_sv_undef);
    EXTEND(SP, 1);
    if (!lval && SvGMAGICAL(sv))	/* see note in pp_helem() */
	sv = sv_mortalcopy(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_join)
{
    dSP; dMARK; dTARGET;
    MARK++;
    do_join(TARG, *MARK, MARK, SP);
    SP = MARK;
    SETs(TARG);
    RETURN;
}

PP(pp_pushre)
{
    dSP;
#ifdef DEBUGGING
    /*
     * We ass_u_me that LvTARGOFF() comes first, and that two STRLENs
     * will be enough to hold an OP*.
     */
    SV* sv = sv_newmortal();
    sv_upgrade(sv, SVt_PVLV);
    LvTYPE(sv) = '/';
    Copy(&PL_op, &LvTARGOFF(sv), 1, OP*);
    XPUSHs(sv);
#else
    XPUSHs((SV*)PL_op);
#endif
    RETURN;
}

/* Oversized hot code. */

PP(pp_print)
{
    dSP; dMARK; dORIGMARK;
    GV *gv;
    IO *io;
    register PerlIO *fp;
    MAGIC *mg;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_STACKED)
	gv = (GV*)*++MARK;
    else
	gv = PL_defoutgv;
    if ((mg = SvTIED_mg((SV*)gv, 'q'))) {
      had_magic:
	if (MARK == ORIGMARK) {
	    /* If using default handle then we need to make space to 
	     * pass object as 1st arg, so move other args up ...
	     */
	    MEXTEND(SP, 1);
	    ++MARK;
	    Move(MARK, MARK + 1, (SP - MARK) + 1, SV*);
	    ++SP;
	}
	PUSHMARK(MARK - 1);
	*MARK = SvTIED_obj((SV*)gv, mg);
	PUTBACK;
	ENTER;
	call_method("PRINT", G_SCALAR);
	LEAVE;
	SPAGAIN;
	MARK = ORIGMARK + 1;
	*MARK = *SP;
	SP = MARK;
	RETURN;
    }
    if (!(io = GvIO(gv))) {
        if ((GvEGV(gv)) && (mg = SvTIED_mg((SV*)GvEGV(gv),'q')))
            goto had_magic;
	if (ckWARN2(WARN_UNOPENED,WARN_CLOSED))
	    report_evil_fh(gv, io, PL_op->op_type);
	SETERRNO(EBADF,RMS$_IFI);
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (ckWARN2(WARN_CLOSED, WARN_IO))  {
	    if (IoIFP(io)) {
		/* integrate with report_evil_fh()? */
	        char *name = NULL;
		if (isGV(gv)) {
		    SV* sv = sv_newmortal();
		    gv_efullname4(sv, gv, Nullch, FALSE);
		    name = SvPV_nolen(sv);
		}
		if (name && *name)
		  Perl_warner(aTHX_ WARN_IO,
			      "Filehandle %s opened only for input", name);
		else
		    Perl_warner(aTHX_ WARN_IO,
				"Filehandle opened only for input");
	    }
	    else if (ckWARN2(WARN_UNOPENED,WARN_CLOSED))
		report_evil_fh(gv, io, PL_op->op_type);
	}
	SETERRNO(EBADF,IoIFP(io)?RMS$_FAC:RMS$_IFI);
	goto just_say_no;
    }
    else {
	MARK++;
	if (PL_ofslen) {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
		if (MARK <= SP) {
		    if (PerlIO_write(fp, PL_ofs, PL_ofslen) == 0 || PerlIO_error(fp)) {
			MARK--;
			break;
		    }
		}
	    }
	}
	else {
	    while (MARK <= SP) {
		if (!do_print(*MARK, fp))
		    break;
		MARK++;
	    }
	}
	if (MARK <= SP)
	    goto just_say_no;
	else {
	    if (PL_orslen)
		if (PerlIO_write(fp, PL_ors, PL_orslen) == 0 || PerlIO_error(fp))
		    goto just_say_no;

	    if (IoFLAGS(io) & IOf_FLUSH)
		if (PerlIO_flush(fp) == EOF)
		    goto just_say_no;
	}
    }
    SP = ORIGMARK;
    PUSHs(&PL_sv_yes);
    RETURN;

  just_say_no:
    SP = ORIGMARK;
    PUSHs(&PL_sv_undef);
    RETURN;
}

PP(pp_rv2av)
{
    dSP; dTOPss;
    AV *av;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF(to_av);

	av = (AV*)SvRV(sv);
	if (SvTYPE(av) != SVt_PVAV)
	    DIE(aTHX_ "Not an ARRAY reference");
	if (PL_op->op_flags & OPf_REF) {
	    SETs((SV*)av);
	    RETURN;
	}
	else if (LVRET) {
	    if (GIMME == G_SCALAR)
		Perl_croak(aTHX_ "Can't return array to lvalue scalar context");
	    SETs((SV*)av);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVAV) {
	    av = (AV*)sv;
	    if (PL_op->op_flags & OPf_REF) {
		SETs((SV*)av);
		RETURN;
	    }
	    else if (LVRET) {
		if (GIMME == G_SCALAR)
		    Perl_croak(aTHX_ "Can't return array to lvalue"
			       " scalar context");
		SETs((SV*)av);
		RETURN;
	    }
	}
	else {
	    GV *gv;
	    
	    if (SvTYPE(sv) != SVt_PVGV) {
		char *sym;
		STRLEN len;

		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (PL_op->op_flags & OPf_REF ||
		      PL_op->op_private & HINT_STRICT_REFS)
			DIE(aTHX_ PL_no_usym, "an ARRAY");
		    if (ckWARN(WARN_UNINITIALIZED))
			report_uninit();
		    if (GIMME == G_ARRAY) {
			(void)POPs;
			RETURN;
		    }
		    RETSETUNDEF;
		}
		sym = SvPV(sv,len);
		if ((PL_op->op_flags & OPf_SPECIAL) &&
		    !(PL_op->op_flags & OPf_MOD))
		{
		    gv = (GV*)gv_fetchpv(sym, FALSE, SVt_PVAV);
		    if (!gv
			&& (!is_gv_magical(sym,len,0)
			    || !(gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PVAV))))
		    {
			RETSETUNDEF;
		    }
		}
		else {
		    if (PL_op->op_private & HINT_STRICT_REFS)
			DIE(aTHX_ PL_no_symref, sym, "an ARRAY");
		    gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PVAV);
		}
	    }
	    else {
		gv = (GV*)sv;
	    }
	    av = GvAVn(gv);
	    if (PL_op->op_private & OPpLVAL_INTRO)
		av = save_ary(gv);
	    if (PL_op->op_flags & OPf_REF) {
		SETs((SV*)av);
		RETURN;
	    }
	    else if (LVRET) {
		if (GIMME == G_SCALAR)
		    Perl_croak(aTHX_ "Can't return array to lvalue"
			       " scalar context");
		SETs((SV*)av);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL(av) + 1;
	(void)POPs;			/* XXXX May be optimized away? */
	EXTEND(SP, maxarg);          
	if (SvRMAGICAL(av)) {
	    U32 i; 
	    for (i=0; i < maxarg; i++) {
		SV **svp = av_fetch(av, i, FALSE);
		SP[i+1] = (svp) ? *svp : &PL_sv_undef;
	    }
	} 
	else {
	    Copy(AvARRAY(av), SP+1, maxarg, SV*);
	}
	SP += maxarg;
    }
    else {
	dTARGET;
	I32 maxarg = AvFILL(av) + 1;
	SETi(maxarg);
    }
    RETURN;
}

PP(pp_rv2hv)
{
    dSP; dTOPss;
    HV *hv;

    if (SvROK(sv)) {
      wasref:
	tryAMAGICunDEREF(to_hv);

	hv = (HV*)SvRV(sv);
	if (SvTYPE(hv) != SVt_PVHV && SvTYPE(hv) != SVt_PVAV)
	    DIE(aTHX_ "Not a HASH reference");
	if (PL_op->op_flags & OPf_REF) {
	    SETs((SV*)hv);
	    RETURN;
	}
	else if (LVRET) {
	    if (GIMME == G_SCALAR)
		Perl_croak(aTHX_ "Can't return hash to lvalue scalar context");
	    SETs((SV*)hv);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVHV || SvTYPE(sv) == SVt_PVAV) {
	    hv = (HV*)sv;
	    if (PL_op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	    else if (LVRET) {
		if (GIMME == G_SCALAR)
		    Perl_croak(aTHX_ "Can't return hash to lvalue"
			       " scalar context");
		SETs((SV*)hv);
		RETURN;
	    }
	}
	else {
	    GV *gv;
	    
	    if (SvTYPE(sv) != SVt_PVGV) {
		char *sym;
		STRLEN len;

		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (PL_op->op_flags & OPf_REF ||
		      PL_op->op_private & HINT_STRICT_REFS)
			DIE(aTHX_ PL_no_usym, "a HASH");
		    if (ckWARN(WARN_UNINITIALIZED))
			report_uninit();
		    if (GIMME == G_ARRAY) {
			SP--;
			RETURN;
		    }
		    RETSETUNDEF;
		}
		sym = SvPV(sv,len);
		if ((PL_op->op_flags & OPf_SPECIAL) &&
		    !(PL_op->op_flags & OPf_MOD))
		{
		    gv = (GV*)gv_fetchpv(sym, FALSE, SVt_PVHV);
		    if (!gv
			&& (!is_gv_magical(sym,len,0)
			    || !(gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PVHV))))
		    {
			RETSETUNDEF;
		    }
		}
		else {
		    if (PL_op->op_private & HINT_STRICT_REFS)
			DIE(aTHX_ PL_no_symref, sym, "a HASH");
		    gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PVHV);
		}
	    }
	    else {
		gv = (GV*)sv;
	    }
	    hv = GvHVn(gv);
	    if (PL_op->op_private & OPpLVAL_INTRO)
		hv = save_hash(gv);
	    if (PL_op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	    else if (LVRET) {
		if (GIMME == G_SCALAR)
		    Perl_croak(aTHX_ "Can't return hash to lvalue"
			       " scalar context");
		SETs((SV*)hv);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) { /* array wanted */
	*PL_stack_sp = (SV*)hv;
	return do_kv();
    }
    else {
	dTARGET;
	if (SvTYPE(hv) == SVt_PVAV)
	    hv = avhv_keys((AV*)hv);
	if (HvFILL(hv))
            Perl_sv_setpvf(aTHX_ TARG, "%"IVdf"/%"IVdf,
			   (IV)HvFILL(hv), (IV)HvMAX(hv) + 1);
	else
	    sv_setiv(TARG, 0);
	
	SETTARG;
	RETURN;
    }
}

STATIC int
S_do_maybe_phash(pTHX_ AV *ary, SV **lelem, SV **firstlelem, SV **relem,
		 SV **lastrelem)
{
    OP *leftop;
    I32 i;

    leftop = ((BINOP*)PL_op)->op_last;
    assert(leftop);
    assert(leftop->op_type == OP_NULL && leftop->op_targ == OP_LIST);
    leftop = ((LISTOP*)leftop)->op_first;
    assert(leftop);
    /* Skip PUSHMARK and each element already assigned to. */
    for (i = lelem - firstlelem; i > 0; i--) {
	leftop = leftop->op_sibling;
	assert(leftop);
    }
    if (leftop->op_type != OP_RV2HV)
	return 0;

    /* pseudohash */
    if (av_len(ary) > 0)
	av_fill(ary, 0);		/* clear all but the fields hash */
    if (lastrelem >= relem) {
	while (relem < lastrelem) {	/* gobble up all the rest */
	    SV *tmpstr;
	    assert(relem[0]);
	    assert(relem[1]);
	    /* Avoid a memory leak when avhv_store_ent dies. */
	    tmpstr = sv_newmortal();
	    sv_setsv(tmpstr,relem[1]);	/* value */
	    relem[1] = tmpstr;
	    if (avhv_store_ent(ary,relem[0],tmpstr,0))
		(void)SvREFCNT_inc(tmpstr);
	    if (SvMAGICAL(ary) != 0 && SvSMAGICAL(tmpstr))
		mg_set(tmpstr);
	    relem += 2;
	    TAINT_NOT;
	}
    }
    if (relem == lastrelem)
	return 1;
    return 2;
}

STATIC void
S_do_oddball(pTHX_ HV *hash, SV **relem, SV **firstrelem)
{
    if (*relem) {
	SV *tmpstr;
	if (ckWARN(WARN_MISC)) {
	    if (relem == firstrelem &&
		SvROK(*relem) &&
		(SvTYPE(SvRV(*relem)) == SVt_PVAV ||
		 SvTYPE(SvRV(*relem)) == SVt_PVHV))
	    {
		Perl_warner(aTHX_ WARN_MISC,
			    "Reference found where even-sized list expected");
	    }
	    else
		Perl_warner(aTHX_ WARN_MISC,
			    "Odd number of elements in hash assignment");
	}
	if (SvTYPE(hash) == SVt_PVAV) {
	    /* pseudohash */
	    tmpstr = sv_newmortal();
	    if (avhv_store_ent((AV*)hash,*relem,tmpstr,0))
		(void)SvREFCNT_inc(tmpstr);
	    if (SvMAGICAL(hash) && SvSMAGICAL(tmpstr))
		mg_set(tmpstr);
	}
	else {
	    HE *didstore;
	    tmpstr = NEWSV(29,0);
	    didstore = hv_store_ent(hash,*relem,tmpstr,0);
	    if (SvMAGICAL(hash)) {
		if (SvSMAGICAL(tmpstr))
		    mg_set(tmpstr);
		if (!didstore)
		    sv_2mortal(tmpstr);
	    }
	}
	TAINT_NOT;
    }
}

PP(pp_aassign)
{
    dSP;
    SV **lastlelem = PL_stack_sp;
    SV **lastrelem = PL_stack_base + POPMARK;
    SV **firstrelem = PL_stack_base + POPMARK + 1;
    SV **firstlelem = lastrelem + 1;

    register SV **relem;
    register SV **lelem;

    register SV *sv;
    register AV *ary;

    I32 gimme;
    HV *hash;
    I32 i;
    int magic;

    PL_delaymagic = DM_DELAY;		/* catch simultaneous items */

    /* If there's a common identifier on both sides we have to take
     * special care that assigning the identifier on the left doesn't
     * clobber a value on the right that's used later in the list.
     */
    if (PL_op->op_private & (OPpASSIGN_COMMON)) {
	EXTEND_MORTAL(lastrelem - firstrelem + 1);
	for (relem = firstrelem; relem <= lastrelem; relem++) {
	    /*SUPPRESS 560*/
	    if ((sv = *relem)) {
		TAINT_NOT;	/* Each item is independent */
		*relem = sv_mortalcopy(sv);
	    }
	}
    }

    relem = firstrelem;
    lelem = firstlelem;
    ary = Null(AV*);
    hash = Null(HV*);

    while (lelem <= lastlelem) {
	TAINT_NOT;		/* Each item stands on its own, taintwise. */
	sv = *lelem++;
	switch (SvTYPE(sv)) {
	case SVt_PVAV:
	    ary = (AV*)sv;
	    magic = SvMAGICAL(ary) != 0;
	    if (PL_op->op_private & OPpASSIGN_HASH) {
		switch (do_maybe_phash(ary, lelem, firstlelem, relem,
				       lastrelem))
		{
		case 0:
		    goto normal_array;
		case 1:
		    do_oddball((HV*)ary, relem, firstrelem);
		}
		relem = lastrelem + 1;
		break;
	    }
	normal_array:
	    av_clear(ary);
	    av_extend(ary, lastrelem - relem);
	    i = 0;
	    while (relem <= lastrelem) {	/* gobble up all the rest */
		SV **didstore;
		sv = NEWSV(28,0);
		assert(*relem);
		sv_setsv(sv,*relem);
		*(relem++) = sv;
		didstore = av_store(ary,i++,sv);
		if (magic) {
		    if (SvSMAGICAL(sv))
			mg_set(sv);
		    if (!didstore)
			sv_2mortal(sv);
		}
		TAINT_NOT;
	    }
	    break;
	case SVt_PVHV: {				/* normal hash */
		SV *tmpstr;

		hash = (HV*)sv;
		magic = SvMAGICAL(hash) != 0;
		hv_clear(hash);

		while (relem < lastrelem) {	/* gobble up all the rest */
		    HE *didstore;
		    if (*relem)
			sv = *(relem++);
		    else
			sv = &PL_sv_no, relem++;
		    tmpstr = NEWSV(29,0);
		    if (*relem)
			sv_setsv(tmpstr,*relem);	/* value */
		    *(relem++) = tmpstr;
		    didstore = hv_store_ent(hash,sv,tmpstr,0);
		    if (magic) {
			if (SvSMAGICAL(tmpstr))
			    mg_set(tmpstr);
			if (!didstore)
			    sv_2mortal(tmpstr);
		    }
		    TAINT_NOT;
		}
		if (relem == lastrelem) {
		    do_oddball(hash, relem, firstrelem);
		    relem++;
		}
	    }
	    break;
	default:
	    if (SvIMMORTAL(sv)) {
		if (relem <= lastrelem)
		    relem++;
		break;
	    }
	    if (relem <= lastrelem) {
		sv_setsv(sv, *relem);
		*(relem++) = sv;
	    }
	    else
		sv_setsv(sv, &PL_sv_undef);
	    SvSETMAGIC(sv);
	    break;
	}
    }
    if (PL_delaymagic & ~DM_DELAY) {
	if (PL_delaymagic & DM_UID) {
#ifdef HAS_SETRESUID
	    (void)setresuid(PL_uid,PL_euid,(Uid_t)-1);
#else
#  ifdef HAS_SETREUID
	    (void)setreuid(PL_uid,PL_euid);
#  else
#    ifdef HAS_SETRUID
	    if ((PL_delaymagic & DM_UID) == DM_RUID) {
		(void)setruid(PL_uid);
		PL_delaymagic &= ~DM_RUID;
	    }
#    endif /* HAS_SETRUID */
#    ifdef HAS_SETEUID
	    if ((PL_delaymagic & DM_UID) == DM_EUID) {
		(void)seteuid(PL_uid);
		PL_delaymagic &= ~DM_EUID;
	    }
#    endif /* HAS_SETEUID */
	    if (PL_delaymagic & DM_UID) {
		if (PL_uid != PL_euid)
		    DIE(aTHX_ "No setreuid available");
		(void)PerlProc_setuid(PL_uid);
	    }
#  endif /* HAS_SETREUID */
#endif /* HAS_SETRESUID */
	    PL_uid = PerlProc_getuid();
	    PL_euid = PerlProc_geteuid();
	}
	if (PL_delaymagic & DM_GID) {
#ifdef HAS_SETRESGID
	    (void)setresgid(PL_gid,PL_egid,(Gid_t)-1);
#else
#  ifdef HAS_SETREGID
	    (void)setregid(PL_gid,PL_egid);
#  else
#    ifdef HAS_SETRGID
	    if ((PL_delaymagic & DM_GID) == DM_RGID) {
		(void)setrgid(PL_gid);
		PL_delaymagic &= ~DM_RGID;
	    }
#    endif /* HAS_SETRGID */
#    ifdef HAS_SETEGID
	    if ((PL_delaymagic & DM_GID) == DM_EGID) {
		(void)setegid(PL_gid);
		PL_delaymagic &= ~DM_EGID;
	    }
#    endif /* HAS_SETEGID */
	    if (PL_delaymagic & DM_GID) {
		if (PL_gid != PL_egid)
		    DIE(aTHX_ "No setregid available");
		(void)PerlProc_setgid(PL_gid);
	    }
#  endif /* HAS_SETREGID */
#endif /* HAS_SETRESGID */
	    PL_gid = PerlProc_getgid();
	    PL_egid = PerlProc_getegid();
	}
	PL_tainting |= (PL_uid && (PL_euid != PL_uid || PL_egid != PL_gid));
    }
    PL_delaymagic = 0;

    gimme = GIMME_V;
    if (gimme == G_VOID)
	SP = firstrelem - 1;
    else if (gimme == G_SCALAR) {
	dTARGET;
	SP = firstrelem;
	SETi(lastrelem - firstrelem + 1);
    }
    else {
	if (ary || hash)
	    SP = lastrelem;
	else
	    SP = firstrelem + (lastlelem - firstlelem);
	lelem = firstlelem + (relem - firstrelem);
	while (relem <= SP)
	    *relem++ = (lelem <= lastlelem) ? *lelem++ : &PL_sv_undef;
    }
    RETURN;
}

PP(pp_qr)
{
    dSP;
    register PMOP *pm = cPMOP;
    SV *rv = sv_newmortal();
    SV *sv = newSVrv(rv, "Regexp");
    sv_magic(sv,(SV*)ReREFCNT_inc(pm->op_pmregexp),'r',0,0);
    RETURNX(PUSHs(rv));
}

PP(pp_match)
{
    dSP; dTARG;
    register PMOP *pm = cPMOP;
    register char *t;
    register char *s;
    char *strend;
    I32 global;
    I32 r_flags = REXEC_CHECKED;
    char *truebase;			/* Start of string  */
    register REGEXP *rx = pm->op_pmregexp;
    bool rxtainted;
    I32 gimme = GIMME;
    STRLEN len;
    I32 minmatch = 0;
    I32 oldsave = PL_savestack_ix;
    I32 update_minmatch = 1;
    I32 had_zerolen = 0;

    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }
    PUTBACK;				/* EVAL blocks need stack_sp. */
    s = SvPV(TARG, len);
    strend = s + len;
    if (!s)
	DIE(aTHX_ "panic: pp_match");
    rxtainted = ((pm->op_pmdynflags & PMdf_TAINTED) ||
		 (PL_tainted && (pm->op_pmflags & PMf_RETAINT)));
    TAINT_NOT;

    if (pm->op_pmdynflags & PMdf_USED) {
      failure:
	if (gimme == G_ARRAY)
	    RETURN;
	RETPUSHNO;
    }

    if (!rx->prelen && PL_curpm) {
	pm = PL_curpm;
	rx = pm->op_pmregexp;
    }
    if (rx->minlen > len) goto failure;

    truebase = t = s;

    /* XXXX What part of this is needed with true \G-support? */
    if ((global = pm->op_pmflags & PMf_GLOBAL)) {
	rx->startp[0] = -1;
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg && mg->mg_len >= 0) {
		if (!(rx->reganch & ROPT_GPOS_SEEN))
		    rx->endp[0] = rx->startp[0] = mg->mg_len; 
		else if (rx->reganch & ROPT_ANCH_GPOS) {
		    r_flags |= REXEC_IGNOREPOS;
		    rx->endp[0] = rx->startp[0] = mg->mg_len; 
		}
		minmatch = (mg->mg_flags & MGf_MINMATCH);
		update_minmatch = 0;
	    }
	}
    }
    if ((!global && rx->nparens)
	    || SvTEMP(TARG) || PL_sawampersand)
	r_flags |= REXEC_COPY_STR;
    if (SvSCREAM(TARG)) 
	r_flags |= REXEC_SCREAM;

    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(PL_multiline);
	PL_multiline = pm->op_pmflags & PMf_MULTILINE;
    }

play_it_again:
    if (global && rx->startp[0] != -1) {
	t = s = rx->endp[0] + truebase;
	if ((s + rx->minlen) > strend)
	    goto nope;
	if (update_minmatch++)
	    minmatch = had_zerolen;
    }
    if (rx->reganch & RE_USE_INTUIT &&
	DO_UTF8(TARG) == ((rx->reganch & ROPT_UTF8) != 0)) {
	s = CALLREG_INTUIT_START(aTHX_ rx, TARG, s, strend, r_flags, NULL);

	if (!s)
	    goto nope;
	if ( (rx->reganch & ROPT_CHECK_ALL)
	     && !PL_sawampersand 
	     && ((rx->reganch & ROPT_NOSCAN)
		 || !((rx->reganch & RE_INTUIT_TAIL)
		      && (r_flags & REXEC_SCREAM)))
	     && !SvROK(TARG))	/* Cannot trust since INTUIT cannot guess ^ */
	    goto yup;
    }
    if (CALLREGEXEC(aTHX_ rx, s, strend, truebase, minmatch, TARG, NULL, r_flags))
    {
	PL_curpm = pm;
	if (pm->op_pmflags & PMf_ONCE)
	    pm->op_pmdynflags |= PMdf_USED;
	goto gotcha;
    }
    else
	goto ret_no;
    /*NOTREACHED*/

  gotcha:
    if (rxtainted)
	RX_MATCH_TAINTED_on(rx);
    TAINT_IF(RX_MATCH_TAINTED(rx));
    if (gimme == G_ARRAY) {
	I32 iters, i, len;

	iters = rx->nparens;
	if (global && !iters)
	    i = 1;
	else
	    i = 0;
	SPAGAIN;			/* EVAL blocks could move the stack. */
	EXTEND(SP, iters + i);
	EXTEND_MORTAL(iters + i);
	for (i = !i; i <= iters; i++) {
	    PUSHs(sv_newmortal());
	    /*SUPPRESS 560*/
	    if ((rx->startp[i] != -1) && rx->endp[i] != -1 ) {
		len = rx->endp[i] - rx->startp[i];
		s = rx->startp[i] + truebase;
		sv_setpvn(*SP, s, len);
		if ((pm->op_pmdynflags & PMdf_UTF8) && !IN_BYTE) {
		    SvUTF8_on(*SP);
		    sv_utf8_downgrade(*SP, TRUE);
		}
	    }
	}
	if (global) {
	    had_zerolen = (rx->startp[0] != -1
			   && rx->startp[0] == rx->endp[0]);
	    PUTBACK;			/* EVAL blocks may use stack */
	    r_flags |= REXEC_IGNOREPOS | REXEC_NOT_FIRST;
	    goto play_it_again;
	}
	else if (!iters)
	    XPUSHs(&PL_sv_yes);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    else {
	if (global) {
	    MAGIC* mg = 0;
	    if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG))
		mg = mg_find(TARG, 'g');
	    if (!mg) {
		sv_magic(TARG, (SV*)0, 'g', Nullch, 0);
		mg = mg_find(TARG, 'g');
	    }
	    if (rx->startp[0] != -1) {
		mg->mg_len = rx->endp[0];
		if (rx->startp[0] == rx->endp[0])
		    mg->mg_flags |= MGf_MINMATCH;
		else
		    mg->mg_flags &= ~MGf_MINMATCH;
	    }
	}
	LEAVE_SCOPE(oldsave);
	RETPUSHYES;
    }

yup:					/* Confirmed by INTUIT */
    if (rxtainted)
	RX_MATCH_TAINTED_on(rx);
    TAINT_IF(RX_MATCH_TAINTED(rx));
    PL_curpm = pm;
    if (pm->op_pmflags & PMf_ONCE)
	pm->op_pmdynflags |= PMdf_USED;
    if (RX_MATCH_COPIED(rx))
	Safefree(rx->subbeg);
    RX_MATCH_COPIED_off(rx);
    rx->subbeg = Nullch;
    if (global) {
	rx->subbeg = truebase;
	rx->startp[0] = s - truebase;
	rx->endp[0] = s - truebase + rx->minlen;
	rx->sublen = strend - truebase;
	goto gotcha;
    } 
    if (PL_sawampersand) {
	I32 off;

	rx->subbeg = savepvn(t, strend - t);
	rx->sublen = strend - t;
	RX_MATCH_COPIED_on(rx);
	off = rx->startp[0] = s - t;
	rx->endp[0] = off + rx->minlen;
    }
    else {			/* startp/endp are used by @- @+. */
	rx->startp[0] = s - truebase;
	rx->endp[0] = s - truebase + rx->minlen;
    }
    rx->nparens = rx->lastparen = 0;	/* used by @- and @+ */
    LEAVE_SCOPE(oldsave);
    RETPUSHYES;

nope:
ret_no:
    if (global && !(pm->op_pmflags & PMf_CONTINUE)) {
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg)
		mg->mg_len = -1;
	}
    }
    LEAVE_SCOPE(oldsave);
    if (gimme == G_ARRAY)
	RETURN;
    RETPUSHNO;
}

OP *
Perl_do_readline(pTHX)
{
    dSP; dTARGETSTACKED;
    register SV *sv;
    STRLEN tmplen = 0;
    STRLEN offset;
    PerlIO *fp;
    register IO *io = GvIO(PL_last_in_gv);
    register I32 type = PL_op->op_type;
    I32 gimme = GIMME_V;
    MAGIC *mg;

    if ((mg = SvTIED_mg((SV*)PL_last_in_gv, 'q'))) {
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)PL_last_in_gv, mg));
	PUTBACK;
	ENTER;
	call_method("READLINE", gimme);
	LEAVE;
	SPAGAIN;
	if (gimme == G_SCALAR)
	    SvSetMagicSV_nosteal(TARG, TOPs);
	RETURN;
    }
    fp = Nullfp;
    if (io) {
	fp = IoIFP(io);
	if (!fp) {
	    if (IoFLAGS(io) & IOf_ARGV) {
		if (IoFLAGS(io) & IOf_START) {
		    IoLINES(io) = 0;
		    if (av_len(GvAVn(PL_last_in_gv)) < 0) {
			IoFLAGS(io) &= ~IOf_START;
			do_open(PL_last_in_gv,"-",1,FALSE,O_RDONLY,0,Nullfp);
			sv_setpvn(GvSV(PL_last_in_gv), "-", 1);
			SvSETMAGIC(GvSV(PL_last_in_gv));
			fp = IoIFP(io);
			goto have_fp;
		    }
		}
		fp = nextargv(PL_last_in_gv);
		if (!fp) { /* Note: fp != IoIFP(io) */
		    (void)do_close(PL_last_in_gv, FALSE); /* now it does*/
		}
	    }
	    else if (type == OP_GLOB) {
		SV *tmpcmd = NEWSV(55, 0);
		SV *tmpglob = POPs;
		ENTER;
		SAVEFREESV(tmpcmd);
#ifdef VMS /* expand the wildcards right here, rather than opening a pipe, */
           /* since spawning off a process is a real performance hit */
		{
#include <descrip.h>
#include <lib$routines.h>
#include <nam.h>
#include <rmsdef.h>
		    char rslt[NAM$C_MAXRSS+1+sizeof(unsigned short int)] = {'\0','\0'};
		    char vmsspec[NAM$C_MAXRSS+1];
		    char *rstr = rslt + sizeof(unsigned short int), *begin, *end, *cp;
		    char tmpfnam[L_tmpnam] = "SYS$SCRATCH:";
		    $DESCRIPTOR(dfltdsc,"SYS$DISK:[]*.*;");
		    PerlIO *tmpfp;
		    STRLEN i;
		    struct dsc$descriptor_s wilddsc
		       = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
		    struct dsc$descriptor_vs rsdsc
		       = {sizeof rslt, DSC$K_DTYPE_VT, DSC$K_CLASS_VS, rslt};
		    unsigned long int cxt = 0, sts = 0, ok = 1, hasdir = 0, hasver = 0, isunix = 0;

		    /* We could find out if there's an explicit dev/dir or version
		       by peeking into lib$find_file's internal context at
		       ((struct NAM *)((struct FAB *)cxt)->fab$l_nam)->nam$l_fnb
		       but that's unsupported, so I don't want to do it now and
		       have it bite someone in the future. */
		    strcat(tmpfnam,PerlLIO_tmpnam(NULL));
		    cp = SvPV(tmpglob,i);
		    for (; i; i--) {
		       if (cp[i] == ';') hasver = 1;
		       if (cp[i] == '.') {
		           if (sts) hasver = 1;
		           else sts = 1;
		       }
		       if (cp[i] == '/') {
		          hasdir = isunix = 1;
		          break;
		       }
		       if (cp[i] == ']' || cp[i] == '>' || cp[i] == ':') {
		           hasdir = 1;
		           break;
		       }
		    }
		    if ((tmpfp = PerlIO_open(tmpfnam,"w+","fop=dlt")) != NULL) {
		        Stat_t st;
		        if (!PerlLIO_stat(SvPVX(tmpglob),&st) && S_ISDIR(st.st_mode))
		          ok = ((wilddsc.dsc$a_pointer = tovmspath(SvPVX(tmpglob),vmsspec)) != NULL);
		        else ok = ((wilddsc.dsc$a_pointer = tovmsspec(SvPVX(tmpglob),vmsspec)) != NULL);
		        if (ok) wilddsc.dsc$w_length = (unsigned short int) strlen(wilddsc.dsc$a_pointer);
		        while (ok && ((sts = lib$find_file(&wilddsc,&rsdsc,&cxt,
		                                    &dfltdsc,NULL,NULL,NULL))&1)) {
		            end = rstr + (unsigned long int) *rslt;
		            if (!hasver) while (*end != ';') end--;
		            *(end++) = '\n';  *end = '\0';
		            for (cp = rstr; *cp; cp++) *cp = _tolower(*cp);
		            if (hasdir) {
		              if (isunix) trim_unixpath(rstr,SvPVX(tmpglob),1);
		              begin = rstr;
		            }
		            else {
		                begin = end;
		                while (*(--begin) != ']' && *begin != '>') ;
		                ++begin;
		            }
		            ok = (PerlIO_puts(tmpfp,begin) != EOF);
		        }
		        if (cxt) (void)lib$find_file_end(&cxt);
		        if (ok && sts != RMS$_NMF &&
		            sts != RMS$_DNF && sts != RMS$_FNF) ok = 0;
		        if (!ok) {
		            if (!(sts & 1)) {
		              SETERRNO((sts == RMS$_SYN ? EINVAL : EVMSERR),sts);
		            }
		            PerlIO_close(tmpfp);
		            fp = NULL;
		        }
		        else {
		           PerlIO_rewind(tmpfp);
		           IoTYPE(io) = IoTYPE_RDONLY;
		           IoIFP(io) = fp = tmpfp;
		           IoFLAGS(io) &= ~IOf_UNTAINT;  /* maybe redundant */
		        }
		    }
		}
#else /* !VMS */
#ifdef MACOS_TRADITIONAL
		sv_setpv(tmpcmd, "glob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, " |");
#else
#ifdef DOSISH
#ifdef OS2
		sv_setpv(tmpcmd, "for a in ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, "; do echo \"$a\\0\\c\"; done |");
#else
#ifdef DJGPP
		sv_setpv(tmpcmd, "/dev/dosglob/"); /* File System Extension */
		sv_catsv(tmpcmd, tmpglob);
#else
		sv_setpv(tmpcmd, "perlglob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, " |");
#endif /* !DJGPP */
#endif /* !OS2 */
#else /* !DOSISH */
#if defined(CSH)
		sv_setpvn(tmpcmd, PL_cshname, PL_cshlen);
		sv_catpv(tmpcmd, " -cf 'set nonomatch; glob ");
		sv_catsv(tmpcmd, tmpglob);
		sv_catpv(tmpcmd, "' 2>/dev/null |");
#else
		sv_setpv(tmpcmd, "echo ");
		sv_catsv(tmpcmd, tmpglob);
#if 'z' - 'a' == 25
		sv_catpv(tmpcmd, "|tr -s ' \t\f\r' '\\012\\012\\012\\012'|");
#else
		sv_catpv(tmpcmd, "|tr -s ' \t\f\r' '\\n\\n\\n\\n'|");
#endif
#endif /* !CSH */
#endif /* !DOSISH */
#endif /* MACOS_TRADITIONAL */
		(void)do_open(PL_last_in_gv, SvPVX(tmpcmd), SvCUR(tmpcmd),
			      FALSE, O_RDONLY, 0, Nullfp);
		fp = IoIFP(io);
#endif /* !VMS */
		LEAVE;
	    }
	}
	else if (type == OP_GLOB)
	    SP--;
	else if (ckWARN(WARN_IO)	/* stdout/stderr or other write fh */
		 && (IoTYPE(io) == IoTYPE_WRONLY || fp == PerlIO_stdout()
		     || fp == PerlIO_stderr()))
	{
	    /* integrate with report_evil_fh()? */
	    char *name = NULL;
	    if (isGV(PL_last_in_gv)) { /* can this ever fail? */
		SV* sv = sv_newmortal();
		gv_efullname4(sv, PL_last_in_gv, Nullch, FALSE);
		name = SvPV_nolen(sv);
	    }
	    if (name && *name)
		Perl_warner(aTHX_ WARN_IO,
			    "Filehandle %s opened only for output", name);
	    else
		Perl_warner(aTHX_ WARN_IO,
			    "Filehandle opened only for output");
	}
    }
    if (!fp) {
	if (ckWARN2(WARN_GLOB, WARN_CLOSED)
		&& (!io || !(IoFLAGS(io) & IOf_START))) {
	    if (type == OP_GLOB)
		Perl_warner(aTHX_ WARN_GLOB,
			    "glob failed (can't start child: %s)",
			    Strerror(errno));
	    else
		report_evil_fh(PL_last_in_gv, io, PL_op->op_type);
	}
	if (gimme == G_SCALAR) {
	    (void)SvOK_off(TARG);
	    PUSHTARG;
	}
	RETURN;
    }
  have_fp:
    if (gimme == G_SCALAR) {
	sv = TARG;
	if (SvROK(sv))
	    sv_unref(sv);
	(void)SvUPGRADE(sv, SVt_PV);
	tmplen = SvLEN(sv);	/* remember if already alloced */
	if (!tmplen)
	    Sv_Grow(sv, 80);	/* try short-buffering it */
	if (type == OP_RCATLINE)
	    offset = SvCUR(sv);
	else
	    offset = 0;
    }
    else {
	sv = sv_2mortal(NEWSV(57, 80));
	offset = 0;
    }

    /* This should not be marked tainted if the fp is marked clean */
#define MAYBE_TAINT_LINE(io, sv) \
    if (!(IoFLAGS(io) & IOf_UNTAINT)) { \
	TAINT;				\
	SvTAINTED_on(sv);		\
    }

/* delay EOF state for a snarfed empty file */
#define SNARF_EOF(gimme,rs,io,sv) \
    (gimme != G_SCALAR || SvCUR(sv)					\
     || (IoFLAGS(io) & IOf_NOLINE) || !RsSNARF(rs))

    for (;;) {
	if (!sv_gets(sv, fp, offset)
	    && (type == OP_GLOB || SNARF_EOF(gimme, PL_rs, io, sv)))
	{
	    PerlIO_clearerr(fp);
	    if (IoFLAGS(io) & IOf_ARGV) {
		fp = nextargv(PL_last_in_gv);
		if (fp)
		    continue;
		(void)do_close(PL_last_in_gv, FALSE);
	    }
	    else if (type == OP_GLOB) {
		if (!do_close(PL_last_in_gv, FALSE) && ckWARN(WARN_GLOB)) {
		    Perl_warner(aTHX_ WARN_GLOB,
			   "glob failed (child exited with status %d%s)",
			   (int)(STATUS_CURRENT >> 8),
			   (STATUS_CURRENT & 0x80) ? ", core dumped" : "");
		}
	    }
	    if (gimme == G_SCALAR) {
		(void)SvOK_off(TARG);
		PUSHTARG;
	    }
	    MAYBE_TAINT_LINE(io, sv);
	    RETURN;
	}
	MAYBE_TAINT_LINE(io, sv);
	IoLINES(io)++;
	IoFLAGS(io) |= IOf_NOLINE;
	SvSETMAGIC(sv);
	XPUSHs(sv);
	if (type == OP_GLOB) {
	    char *tmps;

	    if (SvCUR(sv) > 0 && SvCUR(PL_rs) > 0) {
		tmps = SvEND(sv) - 1;
		if (*tmps == *SvPVX(PL_rs)) {
		    *tmps = '\0';
		    SvCUR(sv)--;
		}
	    }
	    for (tmps = SvPVX(sv); *tmps; tmps++)
		if (!isALPHA(*tmps) && !isDIGIT(*tmps) &&
		    strchr("$&*(){}[]'\";\\|?<>~`", *tmps))
			break;
	    if (*tmps && PerlLIO_lstat(SvPVX(sv), &PL_statbuf) < 0) {
		(void)POPs;		/* Unmatched wildcard?  Chuck it... */
		continue;
	    }
	}
	if (gimme == G_ARRAY) {
	    if (SvLEN(sv) - SvCUR(sv) > 20) {
		SvLEN_set(sv, SvCUR(sv)+1);
		Renew(SvPVX(sv), SvLEN(sv), char);
	    }
	    sv = sv_2mortal(NEWSV(58, 80));
	    continue;
	}
	else if (gimme == G_SCALAR && !tmplen && SvLEN(sv) - SvCUR(sv) > 80) {
	    /* try to reclaim a bit of scalar space (only on 1st alloc) */
	    if (SvCUR(sv) < 60)
		SvLEN_set(sv, 80);
	    else
		SvLEN_set(sv, SvCUR(sv)+40);	/* allow some slop */
	    Renew(SvPVX(sv), SvLEN(sv), char);
	}
	RETURN;
    }
}

PP(pp_enter)
{
    dSP;
    register PERL_CONTEXT *cx;
    I32 gimme = OP_GIMME(PL_op, -1);

    if (gimme == -1) {
	if (cxstack_ix >= 0)
	    gimme = cxstack[cxstack_ix].blk_gimme;
	else
	    gimme = G_SCALAR;
    }

    ENTER;

    SAVETMPS;
    PUSHBLOCK(cx, CXt_BLOCK, SP);

    RETURN;
}

PP(pp_helem)
{
    dSP;
    HE* he;
    SV **svp;
    SV *keysv = POPs;
    HV *hv = (HV*)POPs;
    U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    U32 defer = PL_op->op_private & OPpLVAL_DEFER;
    SV *sv;

    if (SvTYPE(hv) == SVt_PVHV) {
	he = hv_fetch_ent(hv, keysv, lval && !defer, 0);
	svp = he ? &HeVAL(he) : 0;
    }
    else if (SvTYPE(hv) == SVt_PVAV) {
	if (PL_op->op_private & OPpLVAL_INTRO)
	    DIE(aTHX_ "Can't localize pseudo-hash element");
	svp = avhv_fetch_ent((AV*)hv, keysv, lval && !defer, 0);
    }
    else {
	RETPUSHUNDEF;
    }
    if (lval) {
	if (!svp || *svp == &PL_sv_undef) {
	    SV* lv;
	    SV* key2;
	    if (!defer) {
		STRLEN n_a;
		DIE(aTHX_ PL_no_helem, SvPV(keysv, n_a));
	    }
	    lv = sv_newmortal();
	    sv_upgrade(lv, SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, key2 = newSVsv(keysv), 'y', Nullch, 0);
	    SvREFCNT_dec(key2);	/* sv_magic() increments refcount */
	    LvTARG(lv) = SvREFCNT_inc(hv);
	    LvTARGLEN(lv) = 1;
	    PUSHs(lv);
	    RETURN;
	}
	if (PL_op->op_private & OPpLVAL_INTRO) {
	    if (HvNAME(hv) && isGV(*svp))
		save_gp((GV*)*svp, !(PL_op->op_flags & OPf_SPECIAL));
	    else
		save_helem(hv, keysv, svp);
	}
	else if (PL_op->op_private & OPpDEREF)
	    vivify_ref(*svp, PL_op->op_private & OPpDEREF);
    }
    sv = (svp ? *svp : &PL_sv_undef);
    /* This makes C<local $tied{foo} = $tied{foo}> possible.
     * Pushing the magical RHS on to the stack is useless, since
     * that magic is soon destined to be misled by the local(),
     * and thus the later pp_sassign() will fail to mg_get() the
     * old value.  This should also cure problems with delayed
     * mg_get()s.  GSAR 98-07-03 */
    if (!lval && SvGMAGICAL(sv))
	sv = sv_mortalcopy(sv);
    PUSHs(sv);
    RETURN;
}

PP(pp_leave)
{
    dSP;
    register PERL_CONTEXT *cx;
    register SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;

    if (PL_op->op_flags & OPf_SPECIAL) {
	cx = &cxstack[cxstack_ix];
	cx->blk_oldpm = PL_curpm;	/* fake block should preserve $1 et al */
    }

    POPBLOCK(cx,newpm);

    gimme = OP_GIMME(PL_op, -1);
    if (gimme == -1) {
	if (cxstack_ix >= 0)
	    gimme = cxstack[cxstack_ix].blk_gimme;
	else
	    gimme = G_SCALAR;
    }

    TAINT_NOT;
    if (gimme == G_VOID)
	SP = newsp;
    else if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP)
	    if (SvFLAGS(TOPs) & (SVs_PADTMP|SVs_TEMP))
		*MARK = TOPs;
	    else
		*MARK = sv_mortalcopy(TOPs);
	else {
	    MEXTEND(mark,0);
	    *MARK = &PL_sv_undef;
	}
	SP = MARK;
    }
    else if (gimme == G_ARRAY) {
	/* in case LEAVE wipes old return values */
	for (mark = newsp + 1; mark <= SP; mark++) {
	    if (!(SvFLAGS(*mark) & (SVs_PADTMP|SVs_TEMP))) {
		*mark = sv_mortalcopy(*mark);
		TAINT_NOT;	/* Each item is independent */
	    }
	}
    }
    PL_curpm = newpm;	/* Don't pop $1 et al till now */

    LEAVE;

    RETURN;
}

PP(pp_iter)
{
    dSP;
    register PERL_CONTEXT *cx;
    SV* sv;
    AV* av;
    SV **itersvp;

    EXTEND(SP, 1);
    cx = &cxstack[cxstack_ix];
    if (CxTYPE(cx) != CXt_LOOP)
	DIE(aTHX_ "panic: pp_iter");

    itersvp = CxITERVAR(cx);
    av = cx->blk_loop.iterary;
    if (SvTYPE(av) != SVt_PVAV) {
	/* iterate ($min .. $max) */
	if (cx->blk_loop.iterlval) {
	    /* string increment */
	    register SV* cur = cx->blk_loop.iterlval;
	    STRLEN maxlen;
	    char *max = SvPV((SV*)av, maxlen);
	    if (!SvNIOK(cur) && SvCUR(cur) <= maxlen) {
#ifndef USE_THREADS			  /* don't risk potential race */
		if (SvREFCNT(*itersvp) == 1 && !SvMAGICAL(*itersvp)) {
		    /* safe to reuse old SV */
		    sv_setsv(*itersvp, cur);
		}
		else 
#endif
		{
		    /* we need a fresh SV every time so that loop body sees a
		     * completely new SV for closures/references to work as
		     * they used to */
		    SvREFCNT_dec(*itersvp);
		    *itersvp = newSVsv(cur);
		}
		if (strEQ(SvPVX(cur), max))
		    sv_setiv(cur, 0); /* terminate next time */
		else
		    sv_inc(cur);
		RETPUSHYES;
	    }
	    RETPUSHNO;
	}
	/* integer increment */
	if (cx->blk_loop.iterix > cx->blk_loop.itermax)
	    RETPUSHNO;

#ifndef USE_THREADS			  /* don't risk potential race */
	if (SvREFCNT(*itersvp) == 1 && !SvMAGICAL(*itersvp)) {
	    /* safe to reuse old SV */
	    sv_setiv(*itersvp, cx->blk_loop.iterix++);
	}
	else 
#endif
	{
	    /* we need a fresh SV every time so that loop body sees a
	     * completely new SV for closures/references to work as they
	     * used to */
	    SvREFCNT_dec(*itersvp);
	    *itersvp = newSViv(cx->blk_loop.iterix++);
	}
	RETPUSHYES;
    }

    /* iterate array */
    if (cx->blk_loop.iterix >= (av == PL_curstack ? cx->blk_oldsp : AvFILL(av)))
	RETPUSHNO;

    SvREFCNT_dec(*itersvp);

    if ((sv = SvMAGICAL(av)
	      ? *av_fetch(av, ++cx->blk_loop.iterix, FALSE) 
	      : AvARRAY(av)[++cx->blk_loop.iterix]))
	SvTEMP_off(sv);
    else
	sv = &PL_sv_undef;
    if (av != PL_curstack && SvIMMORTAL(sv)) {
	SV *lv = cx->blk_loop.iterlval;
	if (lv && SvREFCNT(lv) > 1) {
	    SvREFCNT_dec(lv);
	    lv = Nullsv;
	}
	if (lv)
	    SvREFCNT_dec(LvTARG(lv));
	else {
	    lv = cx->blk_loop.iterlval = NEWSV(26, 0);
	    sv_upgrade(lv, SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, Nullsv, 'y', Nullch, 0);
	}
	LvTARG(lv) = SvREFCNT_inc(av);
	LvTARGOFF(lv) = cx->blk_loop.iterix;
	LvTARGLEN(lv) = (STRLEN)UV_MAX;
	sv = (SV*)lv;
    }

    *itersvp = SvREFCNT_inc(sv);
    RETPUSHYES;
}

PP(pp_subst)
{
    dSP; dTARG;
    register PMOP *pm = cPMOP;
    PMOP *rpm = pm;
    register SV *dstr, *rstr;
    register char *s;
    char *strend;
    register char *m;
    char *c;
    register char *d;
    STRLEN clen;
    I32 iters = 0;
    I32 maxiters;
    register I32 i;
    bool once;
    bool rxtainted;
    char *orig;
    I32 r_flags;
    register REGEXP *rx = pm->op_pmregexp;
    STRLEN len;
    int force_on_match = 0;
    I32 oldsave = PL_savestack_ix;
    bool do_utf8;
    STRLEN slen;

    /* known replacement string? */
    rstr = (pm->op_pmflags & PMf_CONST) ? POPs : Nullsv;
    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }
    do_utf8 = DO_UTF8(TARG);
    if (SvFAKE(TARG) && SvREADONLY(TARG))
	sv_force_normal(TARG);
    if (SvREADONLY(TARG)
	|| (SvTYPE(TARG) > SVt_PVLV
	    && !(SvTYPE(TARG) == SVt_PVGV && SvFAKE(TARG))))
	DIE(aTHX_ PL_no_modify);
    PUTBACK;

    s = SvPV(TARG, len);
    if (!SvPOKp(TARG) || SvTYPE(TARG) == SVt_PVGV)
	force_on_match = 1;
    rxtainted = ((pm->op_pmdynflags & PMdf_TAINTED) ||
		 (PL_tainted && (pm->op_pmflags & PMf_RETAINT)));
    if (PL_tainted)
	rxtainted |= 2;
    TAINT_NOT;

  force_it:
    if (!pm || !s)
	DIE(aTHX_ "panic: pp_subst");

    strend = s + len;
    slen = do_utf8 ? utf8_length((U8*)s, (U8*)strend) : len;
    maxiters = 2 * slen + 10;	/* We can match twice at each
				   position, once with zero-length,
				   second time with non-zero. */

    if (!rx->prelen && PL_curpm) {
	pm = PL_curpm;
	rx = pm->op_pmregexp;
    }
    r_flags = (rx->nparens || SvTEMP(TARG) || PL_sawampersand)
		? REXEC_COPY_STR : 0;
    if (SvSCREAM(TARG))
	r_flags |= REXEC_SCREAM;
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(PL_multiline);
	PL_multiline = pm->op_pmflags & PMf_MULTILINE;
    }
    orig = m = s;
    if (rx->reganch & RE_USE_INTUIT) {
	s = CALLREG_INTUIT_START(aTHX_ rx, TARG, s, strend, r_flags, NULL);

	if (!s)
	    goto nope;
	/* How to do it in subst? */
/*	if ( (rx->reganch & ROPT_CHECK_ALL)
	     && !PL_sawampersand 
	     && ((rx->reganch & ROPT_NOSCAN)
		 || !((rx->reganch & RE_INTUIT_TAIL)
		      && (r_flags & REXEC_SCREAM))))
	    goto yup;
*/
    }

    /* only replace once? */
    once = !(rpm->op_pmflags & PMf_GLOBAL);

    /* known replacement string? */
    c = rstr ? SvPV(rstr, clen) : Nullch;

    /* can do inplace substitution? */
    if (c && clen <= rx->minlen && (once || !(r_flags & REXEC_COPY_STR))
	&& do_utf8 == DO_UTF8(rstr)
	&& !(rx->reganch & ROPT_LOOKBEHIND_SEEN)) {
	if (!CALLREGEXEC(aTHX_ rx, s, strend, orig, 0, TARG, NULL,
			 r_flags | REXEC_CHECKED))
	{
	    SPAGAIN;
	    PUSHs(&PL_sv_no);
	    LEAVE_SCOPE(oldsave);
	    RETURN;
	}
	if (force_on_match) {
	    force_on_match = 0;
	    s = SvPV_force(TARG, len);
	    goto force_it;
	}
	d = s;
	PL_curpm = pm;
	SvSCREAM_off(TARG);	/* disable possible screamer */
	if (once) {
	    rxtainted |= RX_MATCH_TAINTED(rx);
	    m = orig + rx->startp[0];
	    d = orig + rx->endp[0];
	    s = orig;
	    if (m - s > strend - d) {  /* faster to shorten from end */
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
		SvCUR_set(TARG, m - s);
	    }
	    /*SUPPRESS 560*/
	    else if ((i = m - s)) {	/* faster from front */
		d -= clen;
		m = d;
		sv_chop(TARG, d-i);
		s += i;
		while (i--)
		    *--d = *--s;
		if (clen)
		    Copy(c, m, clen, char);
	    }
	    else if (clen) {
		d -= clen;
		sv_chop(TARG, d);
		Copy(c, d, clen, char);
	    }
	    else {
		sv_chop(TARG, d);
	    }
	    TAINT_IF(rxtainted & 1);
	    SPAGAIN;
	    PUSHs(&PL_sv_yes);
	}
	else {
	    do {
		if (iters++ > maxiters)
		    DIE(aTHX_ "Substitution loop");
		rxtainted |= RX_MATCH_TAINTED(rx);
		m = rx->startp[0] + orig;
		/*SUPPRESS 560*/
		if ((i = m - s)) {
		    if (s != d)
			Move(s, d, i, char);
		    d += i;
		}
		if (clen) {
		    Copy(c, d, clen, char);
		    d += clen;
		}
		s = rx->endp[0] + orig;
	    } while (CALLREGEXEC(aTHX_ rx, s, strend, orig, s == m,
				 TARG, NULL,
				 /* don't match same null twice */
				 REXEC_NOT_FIRST|REXEC_IGNOREPOS));
	    if (s != d) {
		i = strend - s;
		SvCUR_set(TARG, d - SvPVX(TARG) + i);
		Move(s, d, i+1, char);		/* include the NUL */
	    }
	    TAINT_IF(rxtainted & 1);
	    SPAGAIN;
	    PUSHs(sv_2mortal(newSViv((I32)iters)));
	}
	(void)SvPOK_only_UTF8(TARG);
	TAINT_IF(rxtainted);
	if (SvSMAGICAL(TARG)) {
	    PUTBACK;
	    mg_set(TARG);
	    SPAGAIN;
	}
	SvTAINT(TARG);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }

    if (CALLREGEXEC(aTHX_ rx, s, strend, orig, 0, TARG, NULL,
		    r_flags | REXEC_CHECKED))
    {
	bool isutf8;

	if (force_on_match) {
	    force_on_match = 0;
	    s = SvPV_force(TARG, len);
	    goto force_it;
	}
	rxtainted |= RX_MATCH_TAINTED(rx);
	dstr = NEWSV(25, len);
	sv_setpvn(dstr, m, s-m);
	if (do_utf8)
	    SvUTF8_on(dstr);
	PL_curpm = pm;
	if (!c) {
	    register PERL_CONTEXT *cx;
	    SPAGAIN;
	    PUSHSUBST(cx);
	    RETURNOP(cPMOP->op_pmreplroot);
	}
	r_flags |= REXEC_IGNOREPOS | REXEC_NOT_FIRST;
	do {
	    if (iters++ > maxiters)
		DIE(aTHX_ "Substitution loop");
	    rxtainted |= RX_MATCH_TAINTED(rx);
	    if (RX_MATCH_COPIED(rx) && rx->subbeg != orig) {
		m = s;
		s = orig;
		orig = rx->subbeg;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0] + orig;
	    sv_catpvn(dstr, s, m-s);
	    s = rx->endp[0] + orig;
	    if (clen)
		sv_catsv(dstr, rstr);
	    if (once)
		break;
	} while (CALLREGEXEC(aTHX_ rx, s, strend, orig, s == m, TARG, NULL, r_flags));
	sv_catpvn(dstr, s, strend - s);

	(void)SvOOK_off(TARG);
	Safefree(SvPVX(TARG));
	SvPVX(TARG) = SvPVX(dstr);
	SvCUR_set(TARG, SvCUR(dstr));
	SvLEN_set(TARG, SvLEN(dstr));
	isutf8 = DO_UTF8(dstr);
	SvPVX(dstr) = 0;
	sv_free(dstr);

	TAINT_IF(rxtainted & 1);
	SPAGAIN;
	PUSHs(sv_2mortal(newSViv((I32)iters)));

	(void)SvPOK_only(TARG);
	if (isutf8)
	    SvUTF8_on(TARG);
	TAINT_IF(rxtainted);
	SvSETMAGIC(TARG);
	SvTAINT(TARG);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    goto ret_no;

nope:
ret_no:         
    SPAGAIN;
    PUSHs(&PL_sv_no);
    LEAVE_SCOPE(oldsave);
    RETURN;
}

PP(pp_grepwhile)
{
    dSP;

    if (SvTRUEx(POPs))
	PL_stack_base[PL_markstack_ptr[-1]++] = PL_stack_base[*PL_markstack_ptr];
    ++*PL_markstack_ptr;
    LEAVE;					/* exit inner scope */

    /* All done yet? */
    if (PL_stack_base + *PL_markstack_ptr > SP) {
	I32 items;
	I32 gimme = GIMME_V;

	LEAVE;					/* exit outer scope */
	(void)POPMARK;				/* pop src */
	items = --*PL_markstack_ptr - PL_markstack_ptr[-1];
	(void)POPMARK;				/* pop dst */
	SP = PL_stack_base + POPMARK;		/* pop original mark */
	if (gimme == G_SCALAR) {
	    dTARGET;
	    XPUSHi(items);
	}
	else if (gimme == G_ARRAY)
	    SP += items;
	RETURN;
    }
    else {
	SV *src;

	ENTER;					/* enter inner scope */
	SAVEVPTR(PL_curpm);

	src = PL_stack_base[*PL_markstack_ptr];
	SvTEMP_off(src);
	DEFSV = src;

	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_leavesub)
{
    dSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register PERL_CONTEXT *cx;
    SV *sv;

    POPBLOCK(cx,newpm);
 
    TAINT_NOT;
    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (cx->blk_sub.cv && CvDEPTH(cx->blk_sub.cv) > 1) {
		if (SvTEMP(TOPs)) {
		    *MARK = SvREFCNT_inc(TOPs);
		    FREETMPS;
		    sv_2mortal(*MARK);
		}
		else {
		    sv = SvREFCNT_inc(TOPs);	/* FREETMPS could clobber it */
		    FREETMPS;
		    *MARK = sv_mortalcopy(sv);
		    SvREFCNT_dec(sv);
		}
	    }
	    else
		*MARK = SvTEMP(TOPs) ? TOPs : sv_mortalcopy(TOPs);
	}
	else {
	    MEXTEND(MARK, 0);
	    *MARK = &PL_sv_undef;
	}
	SP = MARK;
    }
    else if (gimme == G_ARRAY) {
	for (MARK = newsp + 1; MARK <= SP; MARK++) {
	    if (!SvTEMP(*MARK)) {
		*MARK = sv_mortalcopy(*MARK);
		TAINT_NOT;	/* Each item is independent */
	    }
	}
    }
    PUTBACK;
    
    POPSUB(cx,sv);	/* Stack values are safe: release CV and @_ ... */
    PL_curpm = newpm;	/* ... and pop $1 et al */

    LEAVE;
    LEAVESUB(sv);
    return pop_return();
}

/* This duplicates the above code because the above code must not
 * get any slower by more conditions */
PP(pp_leavesublv)
{
    dSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register PERL_CONTEXT *cx;
    SV *sv;

    POPBLOCK(cx,newpm);
 
    TAINT_NOT;

    if (cx->blk_sub.lval & OPpENTERSUB_INARGS) {
	/* We are an argument to a function or grep().
	 * This kind of lvalueness was legal before lvalue
	 * subroutines too, so be backward compatible:
	 * cannot report errors.  */

	/* Scalar context *is* possible, on the LHS of -> only,
	 * as in f()->meth().  But this is not an lvalue. */
	if (gimme == G_SCALAR)
	    goto temporise;
	if (gimme == G_ARRAY) {
	    if (!CvLVALUE(cx->blk_sub.cv))
		goto temporise_array;
	    EXTEND_MORTAL(SP - newsp);
	    for (mark = newsp + 1; mark <= SP; mark++) {
		if (SvTEMP(*mark))
		    /* empty */ ;
		else if (SvFLAGS(*mark) & (SVs_PADTMP | SVf_READONLY))
		    *mark = sv_mortalcopy(*mark);
		else {
		    /* Can be a localized value subject to deletion. */
		    PL_tmps_stack[++PL_tmps_ix] = *mark;
		    (void)SvREFCNT_inc(*mark);
		}
	    }
	}
    }
    else if (cx->blk_sub.lval) {     /* Leave it as it is if we can. */
	/* Here we go for robustness, not for speed, so we change all
	 * the refcounts so the caller gets a live guy. Cannot set
	 * TEMP, so sv_2mortal is out of question. */
	if (!CvLVALUE(cx->blk_sub.cv)) {
	    POPSUB(cx,sv);
	    PL_curpm = newpm;
	    LEAVE;
	    LEAVESUB(sv);
	    DIE(aTHX_ "Can't modify non-lvalue subroutine call");
	}
	if (gimme == G_SCALAR) {
	    MARK = newsp + 1;
	    EXTEND_MORTAL(1);
	    if (MARK == SP) {
		if (SvFLAGS(TOPs) & (SVs_TEMP | SVs_PADTMP | SVf_READONLY)) {
		    POPSUB(cx,sv);
		    PL_curpm = newpm;
		    LEAVE;
		    LEAVESUB(sv);
		    DIE(aTHX_ "Can't return a %s from lvalue subroutine",
			SvREADONLY(TOPs) ? "readonly value" : "temporary");
		}
		else {                  /* Can be a localized value
					 * subject to deletion. */
		    PL_tmps_stack[++PL_tmps_ix] = *mark;
		    (void)SvREFCNT_inc(*mark);
		}
	    }
	    else {			/* Should not happen? */
		POPSUB(cx,sv);
		PL_curpm = newpm;
		LEAVE;
		LEAVESUB(sv);
		DIE(aTHX_ "%s returned from lvalue subroutine in scalar context",
		    (MARK > SP ? "Empty array" : "Array"));
	    }
	    SP = MARK;
	}
	else if (gimme == G_ARRAY) {
	    EXTEND_MORTAL(SP - newsp);
	    for (mark = newsp + 1; mark <= SP; mark++) {
		if (SvFLAGS(*mark) & (SVs_TEMP | SVs_PADTMP | SVf_READONLY)) {
		    /* Might be flattened array after $#array =  */
		    PUTBACK;
		    POPSUB(cx,sv);
		    PL_curpm = newpm;
		    LEAVE;
		    LEAVESUB(sv);
		    DIE(aTHX_ "Can't return %s from lvalue subroutine",
			(*mark != &PL_sv_undef)
			? (SvREADONLY(TOPs)
			    ? "a readonly value" : "a temporary")
			: "an uninitialized value");
		}
		else {
		    /* Can be a localized value subject to deletion. */
		    PL_tmps_stack[++PL_tmps_ix] = *mark;
		    (void)SvREFCNT_inc(*mark);
		}
	    }
	}
    }
    else {
	if (gimme == G_SCALAR) {
	  temporise:
	    MARK = newsp + 1;
	    if (MARK <= SP) {
		if (cx->blk_sub.cv && CvDEPTH(cx->blk_sub.cv) > 1) {
		    if (SvTEMP(TOPs)) {
			*MARK = SvREFCNT_inc(TOPs);
			FREETMPS;
			sv_2mortal(*MARK);
		    }
		    else {
			sv = SvREFCNT_inc(TOPs); /* FREETMPS could clobber it */
			FREETMPS;
			*MARK = sv_mortalcopy(sv);
			SvREFCNT_dec(sv);
		    }
		}
		else
		    *MARK = SvTEMP(TOPs) ? TOPs : sv_mortalcopy(TOPs);
	    }
	    else {
		MEXTEND(MARK, 0);
		*MARK = &PL_sv_undef;
	    }
	    SP = MARK;
	}
	else if (gimme == G_ARRAY) {
	  temporise_array:
	    for (MARK = newsp + 1; MARK <= SP; MARK++) {
		if (!SvTEMP(*MARK)) {
		    *MARK = sv_mortalcopy(*MARK);
		    TAINT_NOT;  /* Each item is independent */
		}
	    }
	}
    }
    PUTBACK;
    
    POPSUB(cx,sv);	/* Stack values are safe: release CV and @_ ... */
    PL_curpm = newpm;	/* ... and pop $1 et al */

    LEAVE;
    LEAVESUB(sv);
    return pop_return();
}


STATIC CV *
S_get_db_sub(pTHX_ SV **svp, CV *cv)
{
    SV *dbsv = GvSV(PL_DBsub);

    if (!PERLDB_SUB_NN) {
	GV *gv = CvGV(cv);

	save_item(dbsv);
	if ( (CvFLAGS(cv) & (CVf_ANON | CVf_CLONED))
	     || strEQ(GvNAME(gv), "END") 
	     || ((GvCV(gv) != cv) && /* Could be imported, and old sub redefined. */
		 !( (SvTYPE(*svp) == SVt_PVGV) && (GvCV((GV*)*svp) == cv)
		    && (gv = (GV*)*svp) ))) {
	    /* Use GV from the stack as a fallback. */
	    /* GV is potentially non-unique, or contain different CV. */
	    SV *tmp = newRV((SV*)cv);
	    sv_setsv(dbsv, tmp);
	    SvREFCNT_dec(tmp);
	}
	else {
	    gv_efullname3(dbsv, gv, Nullch);
	}
    }
    else {
	(void)SvUPGRADE(dbsv, SVt_PVIV);
	(void)SvIOK_on(dbsv);
	SAVEIV(SvIVX(dbsv));
	SvIVX(dbsv) = PTR2IV(cv);	/* Do it the quickest way  */
    }

    if (CvXSUB(cv))
	PL_curcopdb = PL_curcop;
    cv = GvCV(PL_DBsub);
    return cv;
}

PP(pp_entersub)
{
    dSP; dPOPss;
    GV *gv;
    HV *stash;
    register CV *cv;
    register PERL_CONTEXT *cx;
    I32 gimme;
    bool hasargs = (PL_op->op_flags & OPf_STACKED) != 0;

    if (!sv)
	DIE(aTHX_ "Not a CODE reference");
    switch (SvTYPE(sv)) {
    default:
	if (!SvROK(sv)) {
	    char *sym;
	    STRLEN n_a;

	    if (sv == &PL_sv_yes) {		/* unfound import, ignore */
		if (hasargs)
		    SP = PL_stack_base + POPMARK;
		RETURN;
	    }
	    if (SvGMAGICAL(sv)) {
		mg_get(sv);
		sym = SvPOKp(sv) ? SvPVX(sv) : Nullch;
	    }
	    else
		sym = SvPV(sv, n_a);
	    if (!sym)
		DIE(aTHX_ PL_no_usym, "a subroutine");
	    if (PL_op->op_private & HINT_STRICT_REFS)
		DIE(aTHX_ PL_no_symref, sym, "a subroutine");
	    cv = get_cv(sym, TRUE);
	    break;
	}
	{
	    SV **sp = &sv;		/* Used in tryAMAGICunDEREF macro. */
	    tryAMAGICunDEREF(to_cv);
	}	
	cv = (CV*)SvRV(sv);
	if (SvTYPE(cv) == SVt_PVCV)
	    break;
	/* FALL THROUGH */
    case SVt_PVHV:
    case SVt_PVAV:
	DIE(aTHX_ "Not a CODE reference");
    case SVt_PVCV:
	cv = (CV*)sv;
	break;
    case SVt_PVGV:
	if (!(cv = GvCVu((GV*)sv)))
	    cv = sv_2cv(sv, &stash, &gv, FALSE);
	if (!cv) {
	    ENTER;
	    SAVETMPS;
	    goto try_autoload;
	}
	break;
    }

    ENTER;
    SAVETMPS;

  retry:
    if (!CvROOT(cv) && !CvXSUB(cv)) {
	GV* autogv;
	SV* sub_name;

	/* anonymous or undef'd function leaves us no recourse */
	if (CvANON(cv) || !(gv = CvGV(cv)))
	    DIE(aTHX_ "Undefined subroutine called");

	/* autoloaded stub? */
	if (cv != GvCV(gv)) {
	    cv = GvCV(gv);
	}
	/* should call AUTOLOAD now? */
	else {
try_autoload:
	    if ((autogv = gv_autoload4(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv),
				   FALSE)))
	    {
		cv = GvCV(autogv);
	    }
	    /* sorry */
	    else {
		sub_name = sv_newmortal();
		gv_efullname3(sub_name, gv, Nullch);
		DIE(aTHX_ "Undefined subroutine &%s called", SvPVX(sub_name));
	    }
	}
	if (!cv)
	    DIE(aTHX_ "Not a CODE reference");
	goto retry;
    }

    gimme = GIMME_V;
    if ((PL_op->op_private & OPpENTERSUB_DB) && GvCV(PL_DBsub) && !CvNODEBUG(cv)) {
	cv = get_db_sub(&sv, cv);
	if (!cv)
	    DIE(aTHX_ "No DBsub routine");
    }

#ifdef USE_THREADS
    /*
     * First we need to check if the sub or method requires locking.
     * If so, we gain a lock on the CV, the first argument or the
     * stash (for static methods), as appropriate. This has to be
     * inline because for FAKE_THREADS, COND_WAIT inlines code to
     * reschedule by returning a new op.
     */
    MUTEX_LOCK(CvMUTEXP(cv));
    if (CvFLAGS(cv) & CVf_LOCKED) {
	MAGIC *mg;	
	if (CvFLAGS(cv) & CVf_METHOD) {
	    if (SP > PL_stack_base + TOPMARK)
		sv = *(PL_stack_base + TOPMARK + 1);
	    else {
		AV *av = (AV*)PL_curpad[0];
		if (hasargs || !av || AvFILLp(av) < 0
		    || !(sv = AvARRAY(av)[0]))
		{
		    MUTEX_UNLOCK(CvMUTEXP(cv));
		    DIE(aTHX_ "no argument for locked method call");
		}
	    }
	    if (SvROK(sv))
		sv = SvRV(sv);
	    else {		
		STRLEN len;
		char *stashname = SvPV(sv, len);
		sv = (SV*)gv_stashpvn(stashname, len, TRUE);
	    }
	}
	else {
	    sv = (SV*)cv;
	}
	MUTEX_UNLOCK(CvMUTEXP(cv));
	mg = condpair_magic(sv);
	MUTEX_LOCK(MgMUTEXP(mg));
	if (MgOWNER(mg) == thr)
	    MUTEX_UNLOCK(MgMUTEXP(mg));
	else {
	    while (MgOWNER(mg))
		COND_WAIT(MgOWNERCONDP(mg), MgMUTEXP(mg));
	    MgOWNER(mg) = thr;
	    DEBUG_S(PerlIO_printf(Perl_debug_log, "%p: pp_entersub lock %p\n",
				  thr, sv);)
	    MUTEX_UNLOCK(MgMUTEXP(mg));
	    SAVEDESTRUCTOR_X(Perl_unlock_condpair, sv);
	}
	MUTEX_LOCK(CvMUTEXP(cv));
    }
    /*
     * Now we have permission to enter the sub, we must distinguish
     * four cases. (0) It's an XSUB (in which case we don't care
     * about ownership); (1) it's ours already (and we're recursing);
     * (2) it's free (but we may already be using a cached clone);
     * (3) another thread owns it. Case (1) is easy: we just use it.
     * Case (2) means we look for a clone--if we have one, use it
     * otherwise grab ownership of cv. Case (3) means we look for a
     * clone (for non-XSUBs) and have to create one if we don't
     * already have one.
     * Why look for a clone in case (2) when we could just grab
     * ownership of cv straight away? Well, we could be recursing,
     * i.e. we originally tried to enter cv while another thread
     * owned it (hence we used a clone) but it has been freed up
     * and we're now recursing into it. It may or may not be "better"
     * to use the clone but at least CvDEPTH can be trusted.
     */
    if (CvOWNER(cv) == thr || CvXSUB(cv))
	MUTEX_UNLOCK(CvMUTEXP(cv));
    else {
	/* Case (2) or (3) */
	SV **svp;
	
	/*
	 * XXX Might it be better to release CvMUTEXP(cv) while we
     	 * do the hv_fetch? We might find someone has pinched it
     	 * when we look again, in which case we would be in case
     	 * (3) instead of (2) so we'd have to clone. Would the fact
     	 * that we released the mutex more quickly make up for this?
     	 */
	if ((svp = hv_fetch(thr->cvcache, (char *)cv, sizeof(cv), FALSE)))
	{
	    /* We already have a clone to use */
	    MUTEX_UNLOCK(CvMUTEXP(cv));
	    cv = *(CV**)svp;
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
				  "entersub: %p already has clone %p:%s\n",
				  thr, cv, SvPEEK((SV*)cv)));
	    CvOWNER(cv) = thr;
	    SvREFCNT_inc(cv);
	    if (CvDEPTH(cv) == 0)
		SAVEDESTRUCTOR_X(unset_cvowner, (void*) cv);
	}
	else {
	    /* (2) => grab ownership of cv. (3) => make clone */
	    if (!CvOWNER(cv)) {
		CvOWNER(cv) = thr;
		SvREFCNT_inc(cv);
		MUTEX_UNLOCK(CvMUTEXP(cv));
		DEBUG_S(PerlIO_printf(Perl_debug_log,
			    "entersub: %p grabbing %p:%s in stash %s\n",
			    thr, cv, SvPEEK((SV*)cv), CvSTASH(cv) ?
	    			HvNAME(CvSTASH(cv)) : "(none)"));
	    }
	    else {
		/* Make a new clone. */
		CV *clonecv;
		SvREFCNT_inc(cv); /* don't let it vanish from under us */
		MUTEX_UNLOCK(CvMUTEXP(cv));
		DEBUG_S((PerlIO_printf(Perl_debug_log,
				       "entersub: %p cloning %p:%s\n",
				       thr, cv, SvPEEK((SV*)cv))));
		/*
	    	 * We're creating a new clone so there's no race
		 * between the original MUTEX_UNLOCK and the
		 * SvREFCNT_inc since no one will be trying to undef
		 * it out from underneath us. At least, I don't think
		 * there's a race...
		 */
	     	clonecv = cv_clone(cv);
    		SvREFCNT_dec(cv); /* finished with this */
		hv_store(thr->cvcache, (char*)cv, sizeof(cv), (SV*)clonecv,0);
		CvOWNER(clonecv) = thr;
		cv = clonecv;
		SvREFCNT_inc(cv);
	    }
	    DEBUG_S(if (CvDEPTH(cv) != 0)
			PerlIO_printf(Perl_debug_log, "depth %ld != 0\n",
				      CvDEPTH(cv)););
	    SAVEDESTRUCTOR_X(unset_cvowner, (void*) cv);
	}
    }
#endif /* USE_THREADS */

    if (CvXSUB(cv)) {
#ifdef PERL_XSUB_OLDSTYLE
	if (CvOLDSTYLE(cv)) {
	    I32 (*fp3)(int,int,int);
	    dMARK;
	    register I32 items = SP - MARK;
					/* We dont worry to copy from @_. */
	    while (SP > mark) {
		SP[1] = SP[0];
		SP--;
	    }
	    PL_stack_sp = mark + 1;
	    fp3 = (I32(*)(int,int,int))CvXSUB(cv);
	    items = (*fp3)(CvXSUBANY(cv).any_i32, 
			   MARK - PL_stack_base + 1,
			   items);
	    PL_stack_sp = PL_stack_base + items;
	}
	else
#endif /* PERL_XSUB_OLDSTYLE */
	{
	    I32 markix = TOPMARK;

	    PUTBACK;

	    if (!hasargs) {
		/* Need to copy @_ to stack. Alternative may be to
		 * switch stack to @_, and copy return values
		 * back. This would allow popping @_ in XSUB, e.g.. XXXX */
		AV* av;
		I32 items;
#ifdef USE_THREADS
		av = (AV*)PL_curpad[0];
#else
		av = GvAV(PL_defgv);
#endif /* USE_THREADS */		
		items = AvFILLp(av) + 1;   /* @_ is not tieable */

		if (items) {
		    /* Mark is at the end of the stack. */
		    EXTEND(SP, items);
		    Copy(AvARRAY(av), SP + 1, items, SV*);
		    SP += items;
		    PUTBACK ;		    
		}
	    }
	    /* We assume first XSUB in &DB::sub is the called one. */
	    if (PL_curcopdb) {
		SAVEVPTR(PL_curcop);
		PL_curcop = PL_curcopdb;
		PL_curcopdb = NULL;
	    }
	    /* Do we need to open block here? XXXX */
	    (void)(*CvXSUB(cv))(aTHXo_ cv);

	    /* Enforce some sanity in scalar context. */
	    if (gimme == G_SCALAR && ++markix != PL_stack_sp - PL_stack_base ) {
		if (markix > PL_stack_sp - PL_stack_base)
		    *(PL_stack_base + markix) = &PL_sv_undef;
		else
		    *(PL_stack_base + markix) = *PL_stack_sp;
		PL_stack_sp = PL_stack_base + markix;
	    }
	}
	LEAVE;
	return NORMAL;
    }
    else {
	dMARK;
	register I32 items = SP - MARK;
	AV* padlist = CvPADLIST(cv);
	SV** svp = AvARRAY(padlist);
	push_return(PL_op->op_next);
	PUSHBLOCK(cx, CXt_SUB, MARK);
	PUSHSUB(cx);
	CvDEPTH(cv)++;
	/* XXX This would be a natural place to set C<PL_compcv = cv> so
	 * that eval'' ops within this sub know the correct lexical space.
	 * Owing the speed considerations, we choose to search for the cv
	 * in doeval() instead.
	 */
	if (CvDEPTH(cv) < 2)
	    (void)SvREFCNT_inc(cv);
	else {	/* save temporaries on recursion? */
	    PERL_STACK_OVERFLOW_CHECK();
	    if (CvDEPTH(cv) > AvFILLp(padlist)) {
		AV *av;
		AV *newpad = newAV();
		SV **oldpad = AvARRAY(svp[CvDEPTH(cv)-1]);
		I32 ix = AvFILLp((AV*)svp[1]);
		I32 names_fill = AvFILLp((AV*)svp[0]);
		svp = AvARRAY(svp[0]);
		for ( ;ix > 0; ix--) {
		    if (names_fill >= ix && svp[ix] != &PL_sv_undef) {
			char *name = SvPVX(svp[ix]);
			if ((SvFLAGS(svp[ix]) & SVf_FAKE) /* outer lexical? */
			    || *name == '&')		  /* anonymous code? */
			{
			    av_store(newpad, ix, SvREFCNT_inc(oldpad[ix]));
			}
			else {				/* our own lexical */
			    if (*name == '@')
				av_store(newpad, ix, sv = (SV*)newAV());
			    else if (*name == '%')
				av_store(newpad, ix, sv = (SV*)newHV());
			    else
				av_store(newpad, ix, sv = NEWSV(0,0));
			    SvPADMY_on(sv);
			}
		    }
		    else if (IS_PADGV(oldpad[ix]) || IS_PADCONST(oldpad[ix])) {
			av_store(newpad, ix, sv = SvREFCNT_inc(oldpad[ix]));
		    }
		    else {
			av_store(newpad, ix, sv = NEWSV(0,0));
			SvPADTMP_on(sv);
		    }
		}
		av = newAV();		/* will be @_ */
		av_extend(av, 0);
		av_store(newpad, 0, (SV*)av);
		AvFLAGS(av) = AVf_REIFY;
		av_store(padlist, CvDEPTH(cv), (SV*)newpad);
		AvFILLp(padlist) = CvDEPTH(cv);
		svp = AvARRAY(padlist);
	    }
	}
#ifdef USE_THREADS
	if (!hasargs) {
	    AV* av = (AV*)PL_curpad[0];

	    items = AvFILLp(av) + 1;
	    if (items) {
		/* Mark is at the end of the stack. */
		EXTEND(SP, items);
		Copy(AvARRAY(av), SP + 1, items, SV*);
		SP += items;
		PUTBACK ;		    
	    }
	}
#endif /* USE_THREADS */		
	SAVEVPTR(PL_curpad);
    	PL_curpad = AvARRAY((AV*)svp[CvDEPTH(cv)]);
#ifndef USE_THREADS
	if (hasargs)
#endif /* USE_THREADS */
	{
	    AV* av;
	    SV** ary;

#if 0
	    DEBUG_S(PerlIO_printf(Perl_debug_log,
	    			  "%p entersub preparing @_\n", thr));
#endif
	    av = (AV*)PL_curpad[0];
	    if (AvREAL(av)) {
		/* @_ is normally not REAL--this should only ever
		 * happen when DB::sub() calls things that modify @_ */
		av_clear(av);
		AvREAL_off(av);
		AvREIFY_on(av);
	    }
#ifndef USE_THREADS
	    cx->blk_sub.savearray = GvAV(PL_defgv);
	    GvAV(PL_defgv) = (AV*)SvREFCNT_inc(av);
#endif /* USE_THREADS */
	    cx->blk_sub.oldcurpad = PL_curpad;
	    cx->blk_sub.argarray = av;
	    ++MARK;

	    if (items > AvMAX(av) + 1) {
		ary = AvALLOC(av);
		if (AvARRAY(av) != ary) {
		    AvMAX(av) += AvARRAY(av) - AvALLOC(av);
		    SvPVX(av) = (char*)ary;
		}
		if (items > AvMAX(av) + 1) {
		    AvMAX(av) = items - 1;
		    Renew(ary,items,SV*);
		    AvALLOC(av) = ary;
		    SvPVX(av) = (char*)ary;
		}
	    }
	    Copy(MARK,AvARRAY(av),items,SV*);
	    AvFILLp(av) = items - 1;
	    
	    while (items--) {
		if (*MARK)
		    SvTEMP_off(*MARK);
		MARK++;
	    }
	}
	/* warning must come *after* we fully set up the context
	 * stuff so that __WARN__ handlers can safely dounwind()
	 * if they want to
	 */
	if (CvDEPTH(cv) == 100 && ckWARN(WARN_RECURSION)
	    && !(PERLDB_SUB && cv == GvCV(PL_DBsub)))
	    sub_crush_depth(cv);
#if 0
	DEBUG_S(PerlIO_printf(Perl_debug_log,
			      "%p entersub returning %p\n", thr, CvSTART(cv)));
#endif
	RETURNOP(CvSTART(cv));
    }
}

void
Perl_sub_crush_depth(pTHX_ CV *cv)
{
    if (CvANON(cv))
	Perl_warner(aTHX_ WARN_RECURSION, "Deep recursion on anonymous subroutine");
    else {
	SV* tmpstr = sv_newmortal();
	gv_efullname3(tmpstr, CvGV(cv), Nullch);
	Perl_warner(aTHX_ WARN_RECURSION, "Deep recursion on subroutine \"%s\"", 
		SvPVX(tmpstr));
    }
}

PP(pp_aelem)
{
    dSP;
    SV** svp;
    IV elem = POPi;
    AV* av = (AV*)POPs;
    U32 lval = PL_op->op_flags & OPf_MOD || LVRET;
    U32 defer = (PL_op->op_private & OPpLVAL_DEFER) && (elem > AvFILL(av));
    SV *sv;

    if (elem > 0)
	elem -= PL_curcop->cop_arybase;
    if (SvTYPE(av) != SVt_PVAV)
	RETPUSHUNDEF;
    svp = av_fetch(av, elem, lval && !defer);
    if (lval) {
	if (!svp || *svp == &PL_sv_undef) {
	    SV* lv;
	    if (!defer)
		DIE(aTHX_ PL_no_aelem, elem);
	    lv = sv_newmortal();
	    sv_upgrade(lv, SVt_PVLV);
	    LvTYPE(lv) = 'y';
	    sv_magic(lv, Nullsv, 'y', Nullch, 0);
	    LvTARG(lv) = SvREFCNT_inc(av);
	    LvTARGOFF(lv) = elem;
	    LvTARGLEN(lv) = 1;
	    PUSHs(lv);
	    RETURN;
	}
	if (PL_op->op_private & OPpLVAL_INTRO)
	    save_aelem(av, elem, svp);
	else if (PL_op->op_private & OPpDEREF)
	    vivify_ref(*svp, PL_op->op_private & OPpDEREF);
    }
    sv = (svp ? *svp : &PL_sv_undef);
    if (!lval && SvGMAGICAL(sv))	/* see note in pp_helem() */
	sv = sv_mortalcopy(sv);
    PUSHs(sv);
    RETURN;
}

void
Perl_vivify_ref(pTHX_ SV *sv, U32 to_what)
{
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (!SvOK(sv)) {
	if (SvREADONLY(sv))
	    Perl_croak(aTHX_ PL_no_modify);
	if (SvTYPE(sv) < SVt_RV)
	    sv_upgrade(sv, SVt_RV);
	else if (SvTYPE(sv) >= SVt_PV) {
	    (void)SvOOK_off(sv);
	    Safefree(SvPVX(sv));
	    SvLEN(sv) = SvCUR(sv) = 0;
	}
	switch (to_what) {
	case OPpDEREF_SV:
	    SvRV(sv) = NEWSV(355,0);
	    break;
	case OPpDEREF_AV:
	    SvRV(sv) = (SV*)newAV();
	    break;
	case OPpDEREF_HV:
	    SvRV(sv) = (SV*)newHV();
	    break;
	}
	SvROK_on(sv);
	SvSETMAGIC(sv);
    }
}

PP(pp_method)
{
    dSP;
    SV* sv = TOPs;

    if (SvROK(sv)) {
	SV* rsv = SvRV(sv);
	if (SvTYPE(rsv) == SVt_PVCV) {
	    SETs(rsv);
	    RETURN;
	}
    }

    SETs(method_common(sv, Null(U32*)));
    RETURN;
}

PP(pp_method_named)
{
    dSP;
    SV* sv = cSVOP->op_sv;
    U32 hash = SvUVX(sv);

    XPUSHs(method_common(sv, &hash));
    RETURN;
}

STATIC SV *
S_method_common(pTHX_ SV* meth, U32* hashp)
{
    SV* sv;
    SV* ob;
    GV* gv;
    HV* stash;
    char* name;
    STRLEN namelen;
    char* packname;
    STRLEN packlen;

    name = SvPV(meth, namelen);
    sv = *(PL_stack_base + TOPMARK + 1);

    if (!sv)
	Perl_croak(aTHX_ "Can't call method \"%s\" on an undefined value", name);

    if (SvGMAGICAL(sv))
        mg_get(sv);
    if (SvROK(sv))
	ob = (SV*)SvRV(sv);
    else {
	GV* iogv;

	packname = Nullch;
	if (!SvOK(sv) ||
	    !(packname = SvPV(sv, packlen)) ||
	    !(iogv = gv_fetchpv(packname, FALSE, SVt_PVIO)) ||
	    !(ob=(SV*)GvIO(iogv)))
	{
	    if (!packname ||
		((UTF8_IS_START(*packname) && DO_UTF8(sv))
		    ? !isIDFIRST_utf8((U8*)packname)
		    : !isIDFIRST(*packname)
		))
	    {
		Perl_croak(aTHX_ "Can't call method \"%s\" %s", name,
			   SvOK(sv) ? "without a package or object reference"
				    : "on an undefined value");
	    }
	    stash = gv_stashpvn(packname, packlen, TRUE);
	    goto fetch;
	}
	*(PL_stack_base + TOPMARK + 1) = sv_2mortal(newRV((SV*)iogv));
    }

    if (!ob || !(SvOBJECT(ob)
		 || (SvTYPE(ob) == SVt_PVGV && (ob = (SV*)GvIO((GV*)ob))
		     && SvOBJECT(ob))))
    {
	Perl_croak(aTHX_ "Can't call method \"%s\" on unblessed reference",
		   name);
    }

    stash = SvSTASH(ob);

  fetch:
    /* shortcut for simple names */
    if (hashp) {
	HE* he = hv_fetch_ent(stash, meth, 0, *hashp);
	if (he) {
	    gv = (GV*)HeVAL(he);
	    if (isGV(gv) && GvCV(gv) &&
		(!GvCVGEN(gv) || GvCVGEN(gv) == PL_sub_generation))
		return (SV*)GvCV(gv);
	}
    }

    gv = gv_fetchmethod(stash, name);
    if (!gv) {
	char* leaf = name;
	char* sep = Nullch;
	char* p;
	GV* gv;

	for (p = name; *p; p++) {
	    if (*p == '\'')
		sep = p, leaf = p + 1;
	    else if (*p == ':' && *(p + 1) == ':')
		sep = p, leaf = p + 2;
	}
	if (!sep || ((sep - name) == 5 && strnEQ(name, "SUPER", 5))) {
	    packname = sep ? CopSTASHPV(PL_curcop) : HvNAME(stash);
	    packlen = strlen(packname);
	}
	else {
	    packname = name;
	    packlen = sep - name;
	}
	gv = gv_fetchpv(packname, 0, SVt_PVHV);
	if (gv && isGV(gv)) {
	    Perl_croak(aTHX_
		       "Can't locate object method \"%s\" via package \"%s\"",
		       leaf, packname);
	}
	else {
	    Perl_croak(aTHX_
		       "Can't locate object method \"%s\" via package \"%s\""
		       " (perhaps you forgot to load \"%s\"?)",
		       leaf, packname, packname);
	}
    }
    return isGV(gv) ? (SV*)GvCV(gv) : (SV*)gv;
}

#ifdef USE_THREADS
static void
unset_cvowner(pTHXo_ void *cvarg)
{
    register CV* cv = (CV *) cvarg;

    DEBUG_S((PerlIO_printf(Perl_debug_log, "%p unsetting CvOWNER of %p:%s\n",
			   thr, cv, SvPEEK((SV*)cv))));
    MUTEX_LOCK(CvMUTEXP(cv));
    DEBUG_S(if (CvDEPTH(cv) != 0)
		PerlIO_printf(Perl_debug_log, "depth %ld != 0\n",
			      CvDEPTH(cv)););
    assert(thr == CvOWNER(cv));
    CvOWNER(cv) = 0;
    MUTEX_UNLOCK(CvMUTEXP(cv));
    SvREFCNT_dec(cv);
}
#endif /* USE_THREADS */
