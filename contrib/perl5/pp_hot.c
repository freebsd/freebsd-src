/*    pp_hot.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
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
#include "perl.h"

#ifdef I_UNISTD
#include <unistd.h>
#endif
#ifdef I_FCNTL
#include <fcntl.h>
#endif
#ifdef I_SYS_FILE
#include <sys/file.h>
#endif

/* Hot code. */

#ifdef USE_THREADS
static void
unset_cvowner(void *cvarg)
{
    register CV* cv = (CV *) cvarg;
#ifdef DEBUGGING
    dTHR;
#endif /* DEBUGGING */

    DEBUG_S((PerlIO_printf(PerlIO_stderr(), "%p unsetting CvOWNER of %p:%s\n",
			   thr, cv, SvPEEK((SV*)cv))));
    MUTEX_LOCK(CvMUTEXP(cv));
    DEBUG_S(if (CvDEPTH(cv) != 0)
		PerlIO_printf(PerlIO_stderr(), "depth %ld != 0\n",
			      CvDEPTH(cv)););
    assert(thr == CvOWNER(cv));
    CvOWNER(cv) = 0;
    MUTEX_UNLOCK(CvMUTEXP(cv));
    SvREFCNT_dec(cv);
}
#endif /* USE_THREADS */

PP(pp_const)
{
    djSP;
    XPUSHs(cSVOP->op_sv);
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
    djSP;
    EXTEND(SP,1);
    if (PL_op->op_private & OPpLVAL_INTRO)
	PUSHs(save_scalar(cGVOP->op_gv));
    else
	PUSHs(GvSV(cGVOP->op_gv));
    RETURN;
}

PP(pp_null)
{
    return NORMAL;
}

PP(pp_pushmark)
{
    PUSHMARK(PL_stack_sp);
    return NORMAL;
}

PP(pp_stringify)
{
    djSP; dTARGET;
    STRLEN len;
    char *s;
    s = SvPV(TOPs,len);
    sv_setpvn(TARG,s,len);
    SETTARG;
    RETURN;
}

PP(pp_gv)
{
    djSP;
    XPUSHs((SV*)cGVOP->op_gv);
    RETURN;
}

PP(pp_and)
{
    djSP;
    if (!SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_sassign)
{
    djSP; dPOPTOPssrl;
    MAGIC *mg;

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
    djSP;
    if (SvTRUEx(POPs))
	RETURNOP(cCONDOP->op_true);
    else
	RETURNOP(cCONDOP->op_false);
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
  djSP; dATARGET; tryAMAGICbin(concat,opASSIGN);
  {
    dPOPTOPssrl;
    STRLEN len;
    char *s;
    if (TARG != left) {
	s = SvPV(left,len);
	sv_setpvn(TARG,s,len);
    }
    else if (SvGMAGICAL(TARG))
	mg_get(TARG);
    else if (!SvOK(TARG) && SvTYPE(TARG) <= SVt_PVMG) {
	sv_setpv(TARG, "");	/* Suppress warning. */
	s = SvPV_force(TARG, len);
    }
    s = SvPV(right,len);
    if (SvOK(TARG))
	sv_catpvn(TARG,s,len);
    else
	sv_setpvn(TARG,s,len);	/* suppress warning */
    SETTARG;
    RETURN;
  }
}

PP(pp_padsv)
{
    djSP; dTARGET;
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
    PL_last_in_gv = (GV*)(*PL_stack_sp--);
    return do_readline();
}

PP(pp_eq)
{
    djSP; tryAMAGICbinSET(eq,0); 
    {
      dPOPnv;
      SETs(boolSV(TOPn == value));
      RETURN;
    }
}

PP(pp_preinc)
{
    djSP;
    if (SvREADONLY(TOPs) || SvTYPE(TOPs) > SVt_PVLV)
	croak(no_modify);
    if (SvIOK(TOPs) && !SvNOK(TOPs) && !SvPOK(TOPs) &&
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
    djSP;
    if (SvTRUE(TOPs))
	RETURN;
    else {
	--SP;
	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_add)
{
    djSP; dATARGET; tryAMAGICbin(add,opASSIGN); 
    {
      dPOPTOPnnrl_ul;
      SETn( left + right );
      RETURN;
    }
}

PP(pp_aelemfast)
{
    djSP;
    AV *av = GvAV((GV*)cSVOP->op_sv);
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
    djSP; dMARK; dTARGET;
    MARK++;
    do_join(TARG, *MARK, MARK, SP);
    SP = MARK;
    SETs(TARG);
    RETURN;
}

PP(pp_pushre)
{
    djSP;
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
    djSP; dMARK; dORIGMARK;
    GV *gv;
    IO *io;
    register PerlIO *fp;
    MAGIC *mg;
    STRLEN n_a;

    if (PL_op->op_flags & OPf_STACKED)
	gv = (GV*)*++MARK;
    else
	gv = PL_defoutgv;
    if (mg = SvTIED_mg((SV*)gv, 'q')) {
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
	perl_call_method("PRINT", G_SCALAR);
	LEAVE;
	SPAGAIN;
	MARK = ORIGMARK + 1;
	*MARK = *SP;
	SP = MARK;
	RETURN;
    }
    if (!(io = GvIO(gv))) {
	if (PL_dowarn) {
	    SV* sv = sv_newmortal();
            gv_fullname3(sv, gv, Nullch);
            warn("Filehandle %s never opened", SvPV(sv,n_a));
        }

	SETERRNO(EBADF,RMS$_IFI);
	goto just_say_no;
    }
    else if (!(fp = IoOFP(io))) {
	if (PL_dowarn)  {
	    SV* sv = sv_newmortal();
            gv_fullname3(sv, gv, Nullch);
	    if (IoIFP(io))
		warn("Filehandle %s opened only for input", SvPV(sv,n_a));
	    else
		warn("print on closed filehandle %s", SvPV(sv,n_a));
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
    djSP; dPOPss;
    AV *av;

    if (SvROK(sv)) {
      wasref:
	av = (AV*)SvRV(sv);
	if (SvTYPE(av) != SVt_PVAV)
	    DIE("Not an ARRAY reference");
	if (PL_op->op_flags & OPf_REF) {
	    PUSHs((SV*)av);
	    RETURN;
	}
    }
    else {
	if (SvTYPE(sv) == SVt_PVAV) {
	    av = (AV*)sv;
	    if (PL_op->op_flags & OPf_REF) {
		PUSHs((SV*)av);
		RETURN;
	    }
	}
	else {
	    GV *gv;
	    
	    if (SvTYPE(sv) != SVt_PVGV) {
		char *sym;
		STRLEN n_a;

		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (PL_op->op_flags & OPf_REF ||
		      PL_op->op_private & HINT_STRICT_REFS)
			DIE(no_usym, "an ARRAY");
		    if (PL_dowarn)
			warn(warn_uninit);
		    if (GIMME == G_ARRAY)
			RETURN;
		    RETPUSHUNDEF;
		}
		sym = SvPV(sv,n_a);
		if (PL_op->op_private & HINT_STRICT_REFS)
		    DIE(no_symref, sym, "an ARRAY");
		gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PVAV);
	    } else {
		gv = (GV*)sv;
	    }
	    av = GvAVn(gv);
	    if (PL_op->op_private & OPpLVAL_INTRO)
		av = save_ary(gv);
	    if (PL_op->op_flags & OPf_REF) {
		PUSHs((SV*)av);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) {
	I32 maxarg = AvFILL(av) + 1;
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
	PUSHi(maxarg);
    }
    RETURN;
}

PP(pp_rv2hv)
{
    djSP; dTOPss;
    HV *hv;

    if (SvROK(sv)) {
      wasref:
	hv = (HV*)SvRV(sv);
	if (SvTYPE(hv) != SVt_PVHV && SvTYPE(hv) != SVt_PVAV)
	    DIE("Not a HASH reference");
	if (PL_op->op_flags & OPf_REF) {
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
	}
	else {
	    GV *gv;
	    
	    if (SvTYPE(sv) != SVt_PVGV) {
		char *sym;
		STRLEN n_a;

		if (SvGMAGICAL(sv)) {
		    mg_get(sv);
		    if (SvROK(sv))
			goto wasref;
		}
		if (!SvOK(sv)) {
		    if (PL_op->op_flags & OPf_REF ||
		      PL_op->op_private & HINT_STRICT_REFS)
			DIE(no_usym, "a HASH");
		    if (PL_dowarn)
			warn(warn_uninit);
		    if (GIMME == G_ARRAY) {
			SP--;
			RETURN;
		    }
		    RETSETUNDEF;
		}
		sym = SvPV(sv,n_a);
		if (PL_op->op_private & HINT_STRICT_REFS)
		    DIE(no_symref, sym, "a HASH");
		gv = (GV*)gv_fetchpv(sym, TRUE, SVt_PVHV);
	    } else {
		gv = (GV*)sv;
	    }
	    hv = GvHVn(gv);
	    if (PL_op->op_private & OPpLVAL_INTRO)
		hv = save_hash(gv);
	    if (PL_op->op_flags & OPf_REF) {
		SETs((SV*)hv);
		RETURN;
	    }
	}
    }

    if (GIMME == G_ARRAY) { /* array wanted */
	*PL_stack_sp = (SV*)hv;
	return do_kv(ARGS);
    }
    else {
	dTARGET;
	if (SvTYPE(hv) == SVt_PVAV)
	    hv = avhv_keys((AV*)hv);
	if (HvFILL(hv))
	    sv_setpvf(TARG, "%ld/%ld",
		      (long)HvFILL(hv), (long)HvMAX(hv) + 1);
	else
	    sv_setiv(TARG, 0);
	
	SETTARG;
	RETURN;
    }
}

PP(pp_aassign)
{
    djSP;
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
    if (PL_op->op_private & OPpASSIGN_COMMON) {
        for (relem = firstrelem; relem <= lastrelem; relem++) {
            /*SUPPRESS 560*/
            if (sv = *relem) {
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
			SvREFCNT_dec(sv);
		}
		TAINT_NOT;
	    }
	    break;
	case SVt_PVHV: {
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
			    SvREFCNT_dec(tmpstr);
		    }
		    TAINT_NOT;
		}
		if (relem == lastrelem) {
		    if (*relem) {
			HE *didstore;
			if (PL_dowarn) {
			    if (relem == firstrelem &&
				SvROK(*relem) &&
				( SvTYPE(SvRV(*relem)) == SVt_PVAV ||
				  SvTYPE(SvRV(*relem)) == SVt_PVHV ) )
				warn("Reference found where even-sized list expected");
			    else
				warn("Odd number of elements in hash assignment");
			}
			tmpstr = NEWSV(29,0);
			didstore = hv_store_ent(hash,*relem,tmpstr,0);
			if (magic) {
			    if (SvSMAGICAL(tmpstr))
				mg_set(tmpstr);
			    if (!didstore)
				SvREFCNT_dec(tmpstr);
			}
			TAINT_NOT;
		    }
		    relem++;
		}
	    }
	    break;
	default:
	    if (SvTHINKFIRST(sv)) {
		if (SvREADONLY(sv) && PL_curcop != &PL_compiling) {
		    if (!SvIMMORTAL(sv))
			DIE(no_modify);
		    if (relem <= lastrelem)
			relem++;
		    break;
		}
		if (SvROK(sv))
		    sv_unref(sv);
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
		    DIE("No setreuid available");
		(void)PerlProc_setuid(PL_uid);
	    }
#  endif /* HAS_SETREUID */
#endif /* HAS_SETRESUID */
	    PL_uid = (int)PerlProc_getuid();
	    PL_euid = (int)PerlProc_geteuid();
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
		    DIE("No setregid available");
		(void)PerlProc_setgid(PL_gid);
	    }
#  endif /* HAS_SETREGID */
#endif /* HAS_SETRESGID */
	    PL_gid = (int)PerlProc_getgid();
	    PL_egid = (int)PerlProc_getegid();
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
    djSP;
    register PMOP *pm = cPMOP;
    SV *rv = sv_newmortal();
    SV *sv = newSVrv(rv, "Regexp");
    sv_magic(sv,(SV*)ReREFCNT_inc(pm->op_pmregexp),'r',0,0);
    RETURNX(PUSHs(rv));
}

PP(pp_match)
{
    djSP; dTARG;
    register PMOP *pm = cPMOP;
    register char *t;
    register char *s;
    char *strend;
    I32 global;
    I32 safebase;
    char *truebase;
    register REGEXP *rx = pm->op_pmregexp;
    bool rxtainted;
    I32 gimme = GIMME;
    STRLEN len;
    I32 minmatch = 0;
    I32 oldsave = PL_savestack_ix;
    I32 update_minmatch = 1;
    SV *screamer;

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
	DIE("panic: do_match");
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

    screamer = ( (SvSCREAM(TARG) && rx->check_substr
		  && SvTYPE(rx->check_substr) == SVt_PVBM
		  && SvVALID(rx->check_substr)) 
		? TARG : Nullsv);
    truebase = t = s;
    if (global = pm->op_pmflags & PMf_GLOBAL) {
	rx->startp[0] = 0;
	if (SvTYPE(TARG) >= SVt_PVMG && SvMAGIC(TARG)) {
	    MAGIC* mg = mg_find(TARG, 'g');
	    if (mg && mg->mg_len >= 0) {
		rx->endp[0] = rx->startp[0] = s + mg->mg_len; 
		minmatch = (mg->mg_flags & MGf_MINMATCH);
		update_minmatch = 0;
	    }
	}
    }
    safebase = ((gimme != G_ARRAY && !global && rx->nparens)
		|| SvTEMP(TARG) || PL_sawampersand)
		? REXEC_COPY_STR : 0;
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(PL_multiline);
	PL_multiline = pm->op_pmflags & PMf_MULTILINE;
    }

play_it_again:
    if (global && rx->startp[0]) {
	t = s = rx->endp[0];
	if ((s + rx->minlen) > strend)
	    goto nope;
	if (update_minmatch++)
	    minmatch = (s == rx->startp[0]);
    }
    if (rx->check_substr) {
	if (!(rx->reganch & ROPT_NOSCAN)) { /* Floating checkstring. */
	    if ( screamer ) {
		I32 p = -1;
		
		if (PL_screamfirst[BmRARE(rx->check_substr)] < 0)
		    goto nope;
		else if (!(s = screaminstr(TARG, rx->check_substr, 
					   rx->check_offset_min, 0, &p, 0)))
		    goto nope;
		else if ((rx->reganch & ROPT_CHECK_ALL)
			 && !PL_sawampersand && !SvTAIL(rx->check_substr))
		    goto yup;
	    }
	    else if (!(s = fbm_instr((unsigned char*)s + rx->check_offset_min,
				     (unsigned char*)strend, 
				     rx->check_substr, 0)))
		goto nope;
	    else if ((rx->reganch & ROPT_CHECK_ALL) && !PL_sawampersand)
		goto yup;
	    if (s && rx->check_offset_max < s - t) {
		++BmUSEFUL(rx->check_substr);
		s -= rx->check_offset_max;
	    }
	    else
		s = t;
	}
	/* Now checkstring is fixed, i.e. at fixed offset from the
	   beginning of match, and the match is anchored at s. */
	else if (!PL_multiline) {	/* Anchored near beginning of string. */
	    I32 slen;
	    if (*SvPVX(rx->check_substr) != s[rx->check_offset_min]
		|| ((slen = SvCUR(rx->check_substr)) > 1
		    && memNE(SvPVX(rx->check_substr), 
			     s + rx->check_offset_min, slen)))
		goto nope;
	}
	if (!rx->naughty && --BmUSEFUL(rx->check_substr) < 0
	    && rx->check_substr == rx->float_substr) {
	    SvREFCNT_dec(rx->check_substr);
	    rx->check_substr = Nullsv;	/* opt is being useless */
	    rx->float_substr = Nullsv;
	}
    }
    if (CALLREGEXEC(rx, s, strend, truebase, minmatch,
		      screamer, NULL, safebase))
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
	    if ((s = rx->startp[i]) && rx->endp[i] ) {
		len = rx->endp[i] - s;
		sv_setpvn(*SP, s, len);
	    }
	}
	if (global) {
	    truebase = rx->subbeg;
	    strend = rx->subend;
	    if (rx->startp[0] && rx->startp[0] == rx->endp[0])
		++rx->endp[0];
	    PUTBACK;			/* EVAL blocks may use stack */
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
	    if (rx->startp[0]) {
		mg->mg_len = rx->endp[0] - rx->subbeg;
		if (rx->startp[0] == rx->endp[0])
		    mg->mg_flags |= MGf_MINMATCH;
		else
		    mg->mg_flags &= ~MGf_MINMATCH;
	    }
	}
	LEAVE_SCOPE(oldsave);
	RETPUSHYES;
    }

yup:					/* Confirmed by check_substr */
    if (rxtainted)
	RX_MATCH_TAINTED_on(rx);
    TAINT_IF(RX_MATCH_TAINTED(rx));
    ++BmUSEFUL(rx->check_substr);
    PL_curpm = pm;
    if (pm->op_pmflags & PMf_ONCE)
	pm->op_pmdynflags |= PMdf_USED;
    Safefree(rx->subbase);
    rx->subbase = Nullch;
    if (global) {
	rx->subbeg = truebase;
	rx->subend = strend;
	rx->startp[0] = s;
	rx->endp[0] = s + SvCUR(rx->check_substr);
	goto gotcha;
    }
    if (PL_sawampersand) {
	char *tmps;

	tmps = rx->subbase = savepvn(t, strend-t);
	rx->subbeg = tmps;
	rx->subend = tmps + (strend-t);
	tmps = rx->startp[0] = tmps + (s - t);
	rx->endp[0] = tmps + SvCUR(rx->check_substr);
    }
    LEAVE_SCOPE(oldsave);
    RETPUSHYES;

nope:
    if (rx->check_substr)
	++BmUSEFUL(rx->check_substr);

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
do_readline(void)
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

    if (mg = SvTIED_mg((SV*)PL_last_in_gv, 'q')) {
	PUSHMARK(SP);
	XPUSHs(SvTIED_obj((SV*)PL_last_in_gv, mg));
	PUTBACK;
	ENTER;
	perl_call_method("READLINE", gimme);
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
		    IoFLAGS(io) &= ~IOf_START;
		    IoLINES(io) = 0;
		    if (av_len(GvAVn(PL_last_in_gv)) < 0) {
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
		    IoFLAGS(io) |= IOf_START;
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
		           IoTYPE(io) = '<';
		           IoIFP(io) = fp = tmpfp;
		           IoFLAGS(io) &= ~IOf_UNTAINT;  /* maybe redundant */
		        }
		    }
		}
#else /* !VMS */
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
		(void)do_open(PL_last_in_gv, SvPVX(tmpcmd), SvCUR(tmpcmd),
			      FALSE, O_RDONLY, 0, Nullfp);
		fp = IoIFP(io);
#endif /* !VMS */
		LEAVE;
	    }
	}
	else if (type == OP_GLOB)
	    SP--;
    }
    if (!fp) {
	if (PL_dowarn && io && !(IoFLAGS(io) & IOf_START))
	    warn("Read on closed filehandle <%s>", GvENAME(PL_last_in_gv));
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

/* flip-flop EOF state for a snarfed empty file */
#define SNARF_EOF(gimme,rs,io,sv) \
    ((gimme != G_SCALAR || SvCUR(sv)					\
      || (IoFLAGS(io) & IOf_NOLINE) || IoLINES(io) || !RsSNARF(rs))	\
	? ((IoFLAGS(io) &= ~IOf_NOLINE), TRUE)				\
	: ((IoFLAGS(io) |= IOf_NOLINE), FALSE))

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
		IoFLAGS(io) |= IOf_START;
	    }
	    else if (type == OP_GLOB) {
		if (!do_close(PL_last_in_gv, FALSE)) {
		    warn("glob failed (child exited with status %d%s)",
			 STATUS_CURRENT >> 8,
			 (STATUS_CURRENT & 0x80) ? ", core dumped" : "");
		}
	    }
	    if (gimme == G_SCALAR) {
		(void)SvOK_off(TARG);
		PUSHTARG;
	    }
	    RETURN;
	}
	/* This should not be marked tainted if the fp is marked clean */
	if (!(IoFLAGS(io) & IOf_UNTAINT)) {
	    TAINT;
	    SvTAINTED_on(sv);
	}
	IoLINES(io)++;
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
	    if (*tmps && PerlLIO_stat(SvPVX(sv), &PL_statbuf) < 0) {
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
    djSP;
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
    djSP;
    HE* he;
    SV **svp;
    SV *keysv = POPs;
    HV *hv = (HV*)POPs;
    U32 lval = PL_op->op_flags & OPf_MOD;
    U32 defer = PL_op->op_private & OPpLVAL_DEFER;
    SV *sv;

    if (SvTYPE(hv) == SVt_PVHV) {
	he = hv_fetch_ent(hv, keysv, lval && !defer, 0);
	svp = he ? &HeVAL(he) : 0;
    }
    else if (SvTYPE(hv) == SVt_PVAV) {
	if (PL_op->op_private & OPpLVAL_INTRO)
	    DIE("Can't localize pseudo-hash element");
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
		DIE(no_helem, SvPV(keysv, n_a));
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
    djSP;
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
    djSP;
    register PERL_CONTEXT *cx;
    SV* sv;
    AV* av;

    EXTEND(SP, 1);
    cx = &cxstack[cxstack_ix];
    if (CxTYPE(cx) != CXt_LOOP)
	DIE("panic: pp_iter");

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
		if (SvREFCNT(*cx->blk_loop.itervar) == 1
		    && !SvMAGICAL(*cx->blk_loop.itervar))
		{
		    /* safe to reuse old SV */
		    sv_setsv(*cx->blk_loop.itervar, cur);
		}
		else 
#endif
		{
		    /* we need a fresh SV every time so that loop body sees a
		     * completely new SV for closures/references to work as
		     * they used to */
		    SvREFCNT_dec(*cx->blk_loop.itervar);
		    *cx->blk_loop.itervar = newSVsv(cur);
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
	if (SvREFCNT(*cx->blk_loop.itervar) == 1
	    && !SvMAGICAL(*cx->blk_loop.itervar))
	{
	    /* safe to reuse old SV */
	    sv_setiv(*cx->blk_loop.itervar, cx->blk_loop.iterix++);
	}
	else 
#endif
	{
	    /* we need a fresh SV every time so that loop body sees a
	     * completely new SV for closures/references to work as they
	     * used to */
	    SvREFCNT_dec(*cx->blk_loop.itervar);
	    *cx->blk_loop.itervar = newSViv(cx->blk_loop.iterix++);
	}
	RETPUSHYES;
    }

    /* iterate array */
    if (cx->blk_loop.iterix >= (av == PL_curstack ? cx->blk_oldsp : AvFILL(av)))
	RETPUSHNO;

    SvREFCNT_dec(*cx->blk_loop.itervar);

    if (sv = (SvMAGICAL(av)) 
	    ? *av_fetch(av, ++cx->blk_loop.iterix, FALSE) 
	    : AvARRAY(av)[++cx->blk_loop.iterix])
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
	LvTARGLEN(lv) = (UV) -1;
	sv = (SV*)lv;
    }

    *cx->blk_loop.itervar = SvREFCNT_inc(sv);
    RETPUSHYES;
}

PP(pp_subst)
{
    djSP; dTARG;
    register PMOP *pm = cPMOP;
    PMOP *rpm = pm;
    register SV *dstr;
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
    I32 safebase;
    register REGEXP *rx = pm->op_pmregexp;
    STRLEN len;
    int force_on_match = 0;
    I32 oldsave = PL_savestack_ix;
    I32 update_minmatch = 1;
    SV *screamer;

    /* known replacement string? */
    dstr = (pm->op_pmflags & PMf_CONST) ? POPs : Nullsv;
    if (PL_op->op_flags & OPf_STACKED)
	TARG = POPs;
    else {
	TARG = DEFSV;
	EXTEND(SP,1);
    }                  
    if (SvREADONLY(TARG)
	|| (SvTYPE(TARG) > SVt_PVLV
	    && !(SvTYPE(TARG) == SVt_PVGV && SvFAKE(TARG))))
	croak(no_modify);
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
	DIE("panic: do_subst");

    strend = s + len;
    maxiters = 2*(strend - s) + 10;	/* We can match twice at each 
					   position, once with zero-length,
					   second time with non-zero. */

    if (!rx->prelen && PL_curpm) {
	pm = PL_curpm;
	rx = pm->op_pmregexp;
    }
    screamer = ( (SvSCREAM(TARG) && rx->check_substr
		  && SvTYPE(rx->check_substr) == SVt_PVBM
		  && SvVALID(rx->check_substr)) 
		? TARG : Nullsv);
    safebase = (rx->nparens || SvTEMP(TARG) || PL_sawampersand)
		? REXEC_COPY_STR : 0;
    if (pm->op_pmflags & (PMf_MULTILINE|PMf_SINGLELINE)) {
	SAVEINT(PL_multiline);
	PL_multiline = pm->op_pmflags & PMf_MULTILINE;
    }
    orig = m = s;
    if (rx->check_substr) {
	if (!(rx->reganch & ROPT_NOSCAN)) { /* It floats. */
	    if (screamer) {
		I32 p = -1;
		
		if (PL_screamfirst[BmRARE(rx->check_substr)] < 0)
		    goto nope;
		else if (!(s = screaminstr(TARG, rx->check_substr, rx->check_offset_min, 0, &p, 0)))
		    goto nope;
	    }
	    else if (!(s = fbm_instr((unsigned char*)s + rx->check_offset_min, 
				     (unsigned char*)strend,
				     rx->check_substr, 0)))
		goto nope;
	    if (s && rx->check_offset_max < s - m) {
		++BmUSEFUL(rx->check_substr);
		s -= rx->check_offset_max;
	    }
	    else
		s = m;
	}
	/* Now checkstring is fixed, i.e. at fixed offset from the
	   beginning of match, and the match is anchored at s. */
	else if (!PL_multiline) { /* Anchored at beginning of string. */
	    I32 slen;
	    if (*SvPVX(rx->check_substr) != s[rx->check_offset_min]
		|| ((slen = SvCUR(rx->check_substr)) > 1
		    && memNE(SvPVX(rx->check_substr), 
			     s + rx->check_offset_min, slen)))
		goto nope;
	}
	if (!rx->naughty && --BmUSEFUL(rx->check_substr) < 0
	    && rx->check_substr == rx->float_substr) {
	    SvREFCNT_dec(rx->check_substr);
	    rx->check_substr = Nullsv;	/* opt is being useless */
	    rx->float_substr = Nullsv;
	}
    }

    /* only replace once? */
    once = !(rpm->op_pmflags & PMf_GLOBAL);

    /* known replacement string? */
    c = dstr ? SvPV(dstr, clen) : Nullch;

    /* can do inplace substitution? */
    if (c && clen <= rx->minlen && (once || !(safebase & REXEC_COPY_STR))
	&& !(rx->reganch & ROPT_LOOKBEHIND_SEEN)) {
	if (!CALLREGEXEC(rx, s, strend, orig, 0, screamer, NULL, safebase)) {
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
	    if (rx->subbase) {
		m = orig + (rx->startp[0] - rx->subbase);
		d = orig + (rx->endp[0] - rx->subbase);
	    } else {
		m = rx->startp[0];
		d = rx->endp[0];
	    }
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
	    else if (i = m - s) {	/* faster from front */
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
		    DIE("Substitution loop");
		rxtainted |= RX_MATCH_TAINTED(rx);
		m = rx->startp[0];
		/*SUPPRESS 560*/
		if (i = m - s) {
		    if (s != d)
			Move(s, d, i, char);
		    d += i;
		}
		if (clen) {
		    Copy(c, d, clen, char);
		    d += clen;
		}
		s = rx->endp[0];
	    } while (CALLREGEXEC(rx, s, strend, orig, s == m,
			      Nullsv, NULL, 0)); /* don't match same null twice */
	    if (s != d) {
		i = strend - s;
		SvCUR_set(TARG, d - SvPVX(TARG) + i);
		Move(s, d, i+1, char);		/* include the NUL */
	    }
	    TAINT_IF(rxtainted & 1);
	    SPAGAIN;
	    PUSHs(sv_2mortal(newSViv((I32)iters)));
	}
	(void)SvPOK_only(TARG);
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

    if (CALLREGEXEC(rx, s, strend, orig, 0, screamer, NULL, safebase)) {
	if (force_on_match) {
	    force_on_match = 0;
	    s = SvPV_force(TARG, len);
	    goto force_it;
	}
	rxtainted |= RX_MATCH_TAINTED(rx);
	dstr = NEWSV(25, len);
	sv_setpvn(dstr, m, s-m);
	PL_curpm = pm;
	if (!c) {
	    register PERL_CONTEXT *cx;
	    SPAGAIN;
	    PUSHSUBST(cx);
	    RETURNOP(cPMOP->op_pmreplroot);
	}
	do {
	    if (iters++ > maxiters)
		DIE("Substitution loop");
	    rxtainted |= RX_MATCH_TAINTED(rx);
	    if (rx->subbase && rx->subbase != orig) {
		m = s;
		s = orig;
		orig = rx->subbase;
		s = orig + (m - s);
		strend = s + (strend - m);
	    }
	    m = rx->startp[0];
	    sv_catpvn(dstr, s, m-s);
	    s = rx->endp[0];
	    if (clen)
		sv_catpvn(dstr, c, clen);
	    if (once)
		break;
	} while (CALLREGEXEC(rx, s, strend, orig, s == m, Nullsv, NULL, safebase));
	sv_catpvn(dstr, s, strend - s);

	(void)SvOOK_off(TARG);
	Safefree(SvPVX(TARG));
	SvPVX(TARG) = SvPVX(dstr);
	SvCUR_set(TARG, SvCUR(dstr));
	SvLEN_set(TARG, SvLEN(dstr));
	SvPVX(dstr) = 0;
	sv_free(dstr);

	TAINT_IF(rxtainted & 1);
	SPAGAIN;
	PUSHs(sv_2mortal(newSViv((I32)iters)));

	(void)SvPOK_only(TARG);
	TAINT_IF(rxtainted);
	SvSETMAGIC(TARG);
	SvTAINT(TARG);
	LEAVE_SCOPE(oldsave);
	RETURN;
    }
    goto ret_no;

nope:
    ++BmUSEFUL(rx->check_substr);

ret_no:         
    SPAGAIN;
    PUSHs(&PL_sv_no);
    LEAVE_SCOPE(oldsave);
    RETURN;
}

PP(pp_grepwhile)
{
    djSP;

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
	SAVESPTR(PL_curpm);

	src = PL_stack_base[*PL_markstack_ptr];
	SvTEMP_off(src);
	DEFSV = src;

	RETURNOP(cLOGOP->op_other);
    }
}

PP(pp_leavesub)
{
    djSP;
    SV **mark;
    SV **newsp;
    PMOP *newpm;
    I32 gimme;
    register PERL_CONTEXT *cx;
    struct block_sub cxsub;

    POPBLOCK(cx,newpm);
    POPSUB1(cx);	/* Delay POPSUB2 until stack values are safe */
 
    TAINT_NOT;
    if (gimme == G_SCALAR) {
	MARK = newsp + 1;
	if (MARK <= SP) {
	    if (cxsub.cv && CvDEPTH(cxsub.cv) > 1) {
		if (SvTEMP(TOPs)) {
		    *MARK = SvREFCNT_inc(TOPs);
		    FREETMPS;
		    sv_2mortal(*MARK);
		} else {
		    FREETMPS;
		    *MARK = sv_mortalcopy(TOPs);
		}
	    } else
		*MARK = SvTEMP(TOPs) ? TOPs : sv_mortalcopy(TOPs);
	} else {
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
    
    POPSUB2();		/* Stack values are safe: release CV and @_ ... */
    PL_curpm = newpm;	/* ... and pop $1 et al */

    LEAVE;
    return pop_return();
}

STATIC CV *
get_db_sub(SV **svp, CV *cv)
{
    dTHR;
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
	    sv_setsv(dbsv, newRV((SV*)cv));
	}
	else {
	    gv_efullname3(dbsv, gv, Nullch);
	}
    }
    else {
	SvUPGRADE(dbsv, SVt_PVIV);
	SvIOK_on(dbsv);
	SAVEIV(SvIVX(dbsv));
	SvIVX(dbsv) = (IV)cv;		/* Do it the quickest way  */
    }

    if (CvXSUB(cv))
	PL_curcopdb = PL_curcop;
    cv = GvCV(PL_DBsub);
    return cv;
}

PP(pp_entersub)
{
    djSP; dPOPss;
    GV *gv;
    HV *stash;
    register CV *cv;
    register PERL_CONTEXT *cx;
    I32 gimme;
    bool hasargs = (PL_op->op_flags & OPf_STACKED) != 0;

    if (!sv)
	DIE("Not a CODE reference");
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
		DIE(no_usym, "a subroutine");
	    if (PL_op->op_private & HINT_STRICT_REFS)
		DIE(no_symref, sym, "a subroutine");
	    cv = perl_get_cv(sym, TRUE);
	    break;
	}
	cv = (CV*)SvRV(sv);
	if (SvTYPE(cv) == SVt_PVCV)
	    break;
	/* FALL THROUGH */
    case SVt_PVHV:
    case SVt_PVAV:
	DIE("Not a CODE reference");
    case SVt_PVCV:
	cv = (CV*)sv;
	break;
    case SVt_PVGV:
	if (!(cv = GvCVu((GV*)sv)))
	    cv = sv_2cv(sv, &stash, &gv, TRUE);
	break;
    }

    ENTER;
    SAVETMPS;

  retry:
    if (!cv)
	DIE("Not a CODE reference");

    if (!CvROOT(cv) && !CvXSUB(cv)) {
	GV* autogv;
	SV* sub_name;

	/* anonymous or undef'd function leaves us no recourse */
	if (CvANON(cv) || !(gv = CvGV(cv)))
	    DIE("Undefined subroutine called");
	/* autoloaded stub? */
	if (cv != GvCV(gv)) {
	    cv = GvCV(gv);
	    goto retry;
	}
	/* should call AUTOLOAD now? */
	if ((autogv = gv_autoload4(GvSTASH(gv), GvNAME(gv), GvNAMELEN(gv),
				   FALSE)))
	{
	    cv = GvCV(autogv);
	    goto retry;
	}
	/* sorry */
	sub_name = sv_newmortal();
	gv_efullname3(sub_name, gv, Nullch);
	DIE("Undefined subroutine &%s called", SvPVX(sub_name));
    }

    gimme = GIMME_V;
    if ((PL_op->op_private & OPpENTERSUB_DB) && GvCV(PL_DBsub) && !CvNODEBUG(cv))
	cv = get_db_sub(&sv, cv);
    if (!cv)
	DIE("No DBsub routine");

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
		MUTEX_UNLOCK(CvMUTEXP(cv));
		croak("no argument for locked method call");
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
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(), "%p: pp_entersub lock %p\n",
				  thr, sv);)
	    MUTEX_UNLOCK(MgMUTEXP(mg));
	    save_destructor(unlock_condpair, sv);
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
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
				  "entersub: %p already has clone %p:%s\n",
				  thr, cv, SvPEEK((SV*)cv)));
	    CvOWNER(cv) = thr;
	    SvREFCNT_inc(cv);
	    if (CvDEPTH(cv) == 0)
		SAVEDESTRUCTOR(unset_cvowner, (void*) cv);
	}
	else {
	    /* (2) => grab ownership of cv. (3) => make clone */
	    if (!CvOWNER(cv)) {
		CvOWNER(cv) = thr;
		SvREFCNT_inc(cv);
		MUTEX_UNLOCK(CvMUTEXP(cv));
		DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			    "entersub: %p grabbing %p:%s in stash %s\n",
			    thr, cv, SvPEEK((SV*)cv), CvSTASH(cv) ?
	    			HvNAME(CvSTASH(cv)) : "(none)"));
	    } else {
		/* Make a new clone. */
		CV *clonecv;
		SvREFCNT_inc(cv); /* don't let it vanish from under us */
		MUTEX_UNLOCK(CvMUTEXP(cv));
		DEBUG_S((PerlIO_printf(PerlIO_stderr(),
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
			PerlIO_printf(PerlIO_stderr(), "depth %ld != 0\n",
				      CvDEPTH(cv)););
	    SAVEDESTRUCTOR(unset_cvowner, (void*) cv);
	}
    }
#endif /* USE_THREADS */

    if (CvXSUB(cv)) {
	if (CvOLDSTYLE(cv)) {
	    I32 (*fp3)_((int,int,int));
	    dMARK;
	    register I32 items = SP - MARK;
					/* We dont worry to copy from @_. */
	    while (SP > mark) {
		SP[1] = SP[0];
		SP--;
	    }
	    PL_stack_sp = mark + 1;
	    fp3 = (I32(*)_((int,int,int)))CvXSUB(cv);
	    items = (*fp3)(CvXSUBANY(cv).any_i32, 
			   MARK - PL_stack_base + 1,
			   items);
	    PL_stack_sp = PL_stack_base + items;
	}
	else {
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
	    if (PL_curcopdb) {		/* We assume that the first
					   XSUB in &DB::sub is the
					   called one. */
		SAVESPTR(PL_curcop);
		PL_curcop = PL_curcopdb;
		PL_curcopdb = NULL;
	    }
	    /* Do we need to open block here? XXXX */
	    (void)(*CvXSUB(cv))(cv _PERL_OBJECT_THIS);

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
	    if (CvDEPTH(cv) > AvFILLp(padlist)) {
		AV *av;
		AV *newpad = newAV();
		SV **oldpad = AvARRAY(svp[CvDEPTH(cv)-1]);
		I32 ix = AvFILLp((AV*)svp[1]);
		svp = AvARRAY(svp[0]);
		for ( ;ix > 0; ix--) {
		    if (svp[ix] != &PL_sv_undef) {
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
	SAVESPTR(PL_curpad);
    	PL_curpad = AvARRAY((AV*)svp[CvDEPTH(cv)]);
#ifndef USE_THREADS
	if (hasargs)
#endif /* USE_THREADS */
	{
	    AV* av;
	    SV** ary;

#if 0
	    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
	    			  "%p entersub preparing @_\n", thr));
#endif
	    av = (AV*)PL_curpad[0];
	    if (AvREAL(av)) {
		av_clear(av);
		AvREAL_off(av);
	    }
#ifndef USE_THREADS
	    cx->blk_sub.savearray = GvAV(PL_defgv);
	    GvAV(PL_defgv) = (AV*)SvREFCNT_inc(av);
#endif /* USE_THREADS */
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
	if (CvDEPTH(cv) == 100 && PL_dowarn
	    && !(PERLDB_SUB && cv == GvCV(PL_DBsub)))
	    sub_crush_depth(cv);
#if 0
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "%p entersub returning %p\n", thr, CvSTART(cv)));
#endif
	RETURNOP(CvSTART(cv));
    }
}

void
sub_crush_depth(CV *cv)
{
    if (CvANON(cv))
	warn("Deep recursion on anonymous subroutine");
    else {
	SV* tmpstr = sv_newmortal();
	gv_efullname3(tmpstr, CvGV(cv), Nullch);
	warn("Deep recursion on subroutine \"%s\"", SvPVX(tmpstr));
    }
}

PP(pp_aelem)
{
    djSP;
    SV** svp;
    I32 elem = POPi;
    AV* av = (AV*)POPs;
    U32 lval = PL_op->op_flags & OPf_MOD;
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
		DIE(no_aelem, elem);
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
vivify_ref(SV *sv, U32 to_what)
{
    if (SvGMAGICAL(sv))
	mg_get(sv);
    if (!SvOK(sv)) {
	if (SvREADONLY(sv))
	    croak(no_modify);
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
    djSP;
    SV* sv;
    SV* ob;
    GV* gv;
    HV* stash;
    char* name;
    char* packname;
    STRLEN packlen;

    if (SvROK(TOPs)) {
	sv = SvRV(TOPs);
	if (SvTYPE(sv) == SVt_PVCV) {
	    SETs(sv);
	    RETURN;
	}
    }

    name = SvPV(TOPs, packlen);
    sv = *(PL_stack_base + TOPMARK + 1);
    
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
	    if (!packname || !isIDFIRST(*packname))
		DIE("Can't call method \"%s\" %s", name,
		    SvOK(sv)? "without a package or object reference"
			    : "on an undefined value");
	    stash = gv_stashpvn(packname, packlen, TRUE);
	    goto fetch;
	}
	*(PL_stack_base + TOPMARK + 1) = sv_2mortal(newRV((SV*)iogv));
    }

    if (!ob || !SvOBJECT(ob))
	DIE("Can't call method \"%s\" on unblessed reference", name);

    stash = SvSTASH(ob);

  fetch:
    gv = gv_fetchmethod(stash, name);
    if (!gv) {
	char* leaf = name;
	char* sep = Nullch;
	char* p;

	for (p = name; *p; p++) {
	    if (*p == '\'')
		sep = p, leaf = p + 1;
	    else if (*p == ':' && *(p + 1) == ':')
		sep = p, leaf = p + 2;
	}
	if (!sep || ((sep - name) == 5 && strnEQ(name, "SUPER", 5))) {
	    packname = HvNAME(sep ? PL_curcop->cop_stash : stash);
	    packlen = strlen(packname);
	}
	else {
	    packname = name;
	    packlen = sep - name;
	}
	DIE("Can't locate object method \"%s\" via package \"%.*s\"",
	    leaf, (int)packlen, packname);
    }
    SETs(isGV(gv) ? (SV*)GvCV(gv) : (SV*)gv);
    RETURN;
}

