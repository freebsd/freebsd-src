/*    op.c
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * "You see: Mr. Drogo, he married poor Miss Primula Brandybuck.  She was
 * our Mr. Bilbo's first cousin on the mother's side (her mother being the
 * youngest of the Old Took's daughters); and Mr. Drogo was his second
 * cousin.  So Mr. Frodo is his first *and* second cousin, once removed
 * either way, as the saying is, if you follow me."  --the Gaffer
 */

#include "EXTERN.h"
#include "perl.h"

#ifdef PERL_OBJECT
#define CHECKCALL this->*check
#else
#define CHECKCALL *check
#endif

/*
 * In the following definition, the ", Nullop" is just to make the compiler
 * think the expression is of the right type: croak actually does a Siglongjmp.
 */
#define CHECKOP(type,o) \
    ((PL_op_mask && PL_op_mask[type])					\
     ? ( op_free((OP*)o),					\
	 croak("%s trapped by operation mask", op_desc[type]),	\
	 Nullop )						\
     : (CHECKCALL[type])((OP*)o))

#define PAD_MAX 999999999

static bool scalar_mod_type _((OP *o, I32 type));
#ifndef PERL_OBJECT
static I32 list_assignment _((OP *o));
static void bad_type _((I32 n, char *t, char *name, OP *kid));
static OP *modkids _((OP *o, I32 type));
static OP *no_fh_allowed _((OP *o));
static OP *scalarboolean _((OP *o));
static OP *too_few_arguments _((OP *o, char* name));
static OP *too_many_arguments _((OP *o, char* name));
static void null _((OP* o));
static PADOFFSET pad_findlex _((char* name, PADOFFSET newoff, U32 seq,
	CV* startcv, I32 cx_ix, I32 saweval, U32 flags));
static OP *newDEFSVOP _((void));
static OP *new_logop _((I32 type, I32 flags, OP **firstp, OP **otherp));
#endif

STATIC char*
gv_ename(GV *gv)
{
    SV* tmpsv = sv_newmortal();
    STRLEN n_a;
    gv_efullname3(tmpsv, gv, Nullch);
    return SvPV(tmpsv,n_a);
}

STATIC OP *
no_fh_allowed(OP *o)
{
    yyerror(form("Missing comma after first argument to %s function",
		 op_desc[o->op_type]));
    return o;
}

STATIC OP *
too_few_arguments(OP *o, char *name)
{
    yyerror(form("Not enough arguments for %s", name));
    return o;
}

STATIC OP *
too_many_arguments(OP *o, char *name)
{
    yyerror(form("Too many arguments for %s", name));
    return o;
}

STATIC void
bad_type(I32 n, char *t, char *name, OP *kid)
{
    yyerror(form("Type of arg %d to %s must be %s (not %s)",
		 (int)n, name, t, op_desc[kid->op_type]));
}

void
assertref(OP *o)
{
    int type = o->op_type;
    if (type != OP_AELEM && type != OP_HELEM && type != OP_GELEM) {
	yyerror(form("Can't use subscript on %s", op_desc[type]));
	if (type == OP_ENTERSUB || type == OP_RV2HV || type == OP_PADHV) {
	    dTHR;
	    SV *msg = sv_2mortal(
			newSVpvf("(Did you mean $ or @ instead of %c?)\n",
				 type == OP_ENTERSUB ? '&' : '%'));
	    if (PL_in_eval & 2)
		warn("%_", msg);
	    else if (PL_in_eval)
		sv_catsv(GvSV(PL_errgv), msg);
	    else
		PerlIO_write(PerlIO_stderr(), SvPVX(msg), SvCUR(msg));
	}
    }
}

/* "register" allocation */

PADOFFSET
pad_allocmy(char *name)
{
    dTHR;
    PADOFFSET off;
    SV *sv;

    if (!(isALPHA(name[1]) || name[1] == '_' && (int)strlen(name) > 2)) {
	if (!isPRINT(name[1])) {
	    name[3] = '\0';
	    name[2] = toCTRL(name[1]);
	    name[1] = '^';
	}
	croak("Can't use global %s in \"my\"",name);
    }
    if (PL_dowarn && AvFILLp(PL_comppad_name) >= 0) {
	SV **svp = AvARRAY(PL_comppad_name);
	for (off = AvFILLp(PL_comppad_name); off > PL_comppad_name_floor; off--) {
	    if ((sv = svp[off])
		&& sv != &PL_sv_undef
		&& (SvIVX(sv) == PAD_MAX || SvIVX(sv) == 0)
		&& strEQ(name, SvPVX(sv)))
	    {
		warn("\"my\" variable %s masks earlier declaration in same %s",
			name, (SvIVX(sv) == PAD_MAX ? "scope" : "statement"));
		break;
	    }
	}
    }
    off = pad_alloc(OP_PADSV, SVs_PADMY);
    sv = NEWSV(1102,0);
    sv_upgrade(sv, SVt_PVNV);
    sv_setpv(sv, name);
    if (PL_in_my_stash) {
	if (*name != '$')
	    croak("Can't declare class for non-scalar %s in \"my\"",name);
	SvOBJECT_on(sv);
	(void)SvUPGRADE(sv, SVt_PVMG);
	SvSTASH(sv) = (HV*)SvREFCNT_inc(PL_in_my_stash);
	PL_sv_objcount++;
    }
    av_store(PL_comppad_name, off, sv);
    SvNVX(sv) = (double)PAD_MAX;
    SvIVX(sv) = 0;			/* Not yet introduced--see newSTATEOP */
    if (!PL_min_intro_pending)
	PL_min_intro_pending = off;
    PL_max_intro_pending = off;
    if (*name == '@')
	av_store(PL_comppad, off, (SV*)newAV());
    else if (*name == '%')
	av_store(PL_comppad, off, (SV*)newHV());
    SvPADMY_on(PL_curpad[off]);
    return off;
}

#define FINDLEX_NOSEARCH	1		/* don't search outer contexts */

STATIC PADOFFSET
pad_findlex(char *name, PADOFFSET newoff, U32 seq, CV* startcv, I32 cx_ix, I32 saweval,
	    U32 flags)
{
    dTHR;
    CV *cv;
    I32 off;
    SV *sv;
    register I32 i;
    register PERL_CONTEXT *cx;

    for (cv = startcv; cv; cv = CvOUTSIDE(cv)) {
	AV *curlist = CvPADLIST(cv);
	SV **svp = av_fetch(curlist, 0, FALSE);
	AV *curname;

	if (!svp || *svp == &PL_sv_undef)
	    continue;
	curname = (AV*)*svp;
	svp = AvARRAY(curname);
	for (off = AvFILLp(curname); off > 0; off--) {
	    if ((sv = svp[off]) &&
		sv != &PL_sv_undef &&
		seq <= SvIVX(sv) &&
		seq > I_32(SvNVX(sv)) &&
		strEQ(SvPVX(sv), name))
	    {
		I32 depth;
		AV *oldpad;
		SV *oldsv;

		depth = CvDEPTH(cv);
		if (!depth) {
		    if (newoff) {
			if (SvFAKE(sv))
			    continue;
			return 0; /* don't clone from inactive stack frame */
		    }
		    depth = 1;
		}
		oldpad = (AV*)*av_fetch(curlist, depth, FALSE);
		oldsv = *av_fetch(oldpad, off, TRUE);
		if (!newoff) {		/* Not a mere clone operation. */
		    SV *namesv = NEWSV(1103,0);
		    newoff = pad_alloc(OP_PADSV, SVs_PADMY);
		    sv_upgrade(namesv, SVt_PVNV);
		    sv_setpv(namesv, name);
		    av_store(PL_comppad_name, newoff, namesv);
		    SvNVX(namesv) = (double)PL_curcop->cop_seq;
		    SvIVX(namesv) = PAD_MAX;	/* A ref, intro immediately */
		    SvFAKE_on(namesv);		/* A ref, not a real var */
		    if (SvOBJECT(svp[off])) {	/* A typed var */
			SvOBJECT_on(namesv);
			(void)SvUPGRADE(namesv, SVt_PVMG);
			SvSTASH(namesv) = (HV*)SvREFCNT_inc((SV*)SvSTASH(svp[off]));
			PL_sv_objcount++;
		    }
		    if (CvANON(PL_compcv) || SvTYPE(PL_compcv) == SVt_PVFM) {
			/* "It's closures all the way down." */
			CvCLONE_on(PL_compcv);
			if (cv == startcv) {
			    if (CvANON(PL_compcv))
				oldsv = Nullsv; /* no need to keep ref */
			}
			else {
			    CV *bcv;
			    for (bcv = startcv;
				 bcv && bcv != cv && !CvCLONE(bcv);
				 bcv = CvOUTSIDE(bcv))
			    {
				if (CvANON(bcv))
				    CvCLONE_on(bcv);
				else {
				    if (PL_dowarn
					&& !CvUNIQUE(bcv) && !CvUNIQUE(cv))
				    {
					warn(
					  "Variable \"%s\" may be unavailable",
					     name);
				    }
				    break;
				}
			    }
			}
		    }
		    else if (!CvUNIQUE(PL_compcv)) {
			if (PL_dowarn && !SvFAKE(sv) && !CvUNIQUE(cv))
			    warn("Variable \"%s\" will not stay shared", name);
		    }
		}
		av_store(PL_comppad, newoff, SvREFCNT_inc(oldsv));
		return newoff;
	    }
	}
    }

    if (flags & FINDLEX_NOSEARCH)
	return 0;

    /* Nothing in current lexical context--try eval's context, if any.
     * This is necessary to let the perldb get at lexically scoped variables.
     * XXX This will also probably interact badly with eval tree caching.
     */

    for (i = cx_ix; i >= 0; i--) {
	cx = &cxstack[i];
	switch (CxTYPE(cx)) {
	default:
	    if (i == 0 && saweval) {
		seq = cxstack[saweval].blk_oldcop->cop_seq;
		return pad_findlex(name, newoff, seq, PL_main_cv, -1, saweval, 0);
	    }
	    break;
	case CXt_EVAL:
	    switch (cx->blk_eval.old_op_type) {
	    case OP_ENTEREVAL:
		if (CxREALEVAL(cx))
		    saweval = i;
		break;
	    case OP_REQUIRE:
		/* require must have its own scope */
		return 0;
	    }
	    break;
	case CXt_SUB:
	    if (!saweval)
		return 0;
	    cv = cx->blk_sub.cv;
	    if (PL_debstash && CvSTASH(cv) == PL_debstash) {	/* ignore DB'* scope */
		saweval = i;	/* so we know where we were called from */
		continue;
	    }
	    seq = cxstack[saweval].blk_oldcop->cop_seq;
	    return pad_findlex(name, newoff, seq, cv, i-1, saweval,FINDLEX_NOSEARCH);
	}
    }

    return 0;
}

PADOFFSET
pad_findmy(char *name)
{
    dTHR;
    I32 off;
    I32 pendoff = 0;
    SV *sv;
    SV **svp = AvARRAY(PL_comppad_name);
    U32 seq = PL_cop_seqmax;
    PERL_CONTEXT *cx;
    CV *outside;

#ifdef USE_THREADS
    /*
     * Special case to get lexical (and hence per-thread) @_.
     * XXX I need to find out how to tell at parse-time whether use
     * of @_ should refer to a lexical (from a sub) or defgv (global
     * scope and maybe weird sub-ish things like formats). See
     * startsub in perly.y.  It's possible that @_ could be lexical
     * (at least from subs) even in non-threaded perl.
     */
    if (strEQ(name, "@_"))
	return 0;		/* success. (NOT_IN_PAD indicates failure) */
#endif /* USE_THREADS */

    /* The one we're looking for is probably just before comppad_name_fill. */
    for (off = AvFILLp(PL_comppad_name); off > 0; off--) {
	if ((sv = svp[off]) &&
	    sv != &PL_sv_undef &&
	    (!SvIVX(sv) ||
	     (seq <= SvIVX(sv) &&
	      seq > I_32(SvNVX(sv)))) &&
	    strEQ(SvPVX(sv), name))
	{
	    if (SvIVX(sv))
		return (PADOFFSET)off;
	    pendoff = off;	/* this pending def. will override import */
	}
    }

    outside = CvOUTSIDE(PL_compcv);

    /* Check if if we're compiling an eval'', and adjust seq to be the
     * eval's seq number.  This depends on eval'' having a non-null
     * CvOUTSIDE() while it is being compiled.  The eval'' itself is
     * identified by CvEVAL being true and CvGV being null. */
    if (outside && CvEVAL(PL_compcv) && !CvGV(PL_compcv) && cxstack_ix >= 0) {
	cx = &cxstack[cxstack_ix];
	if (CxREALEVAL(cx))
	    seq = cx->blk_oldcop->cop_seq;
    }

    /* See if it's in a nested scope */
    off = pad_findlex(name, 0, seq, outside, cxstack_ix, 0, 0);
    if (off) {
	/* If there is a pending local definition, this new alias must die */
	if (pendoff)
	    SvIVX(AvARRAY(PL_comppad_name)[off]) = seq;
	return off;		/* pad_findlex returns 0 for failure...*/
    }
    return NOT_IN_PAD;		/* ...but we return NOT_IN_PAD for failure */
}

void
pad_leavemy(I32 fill)
{
    I32 off;
    SV **svp = AvARRAY(PL_comppad_name);
    SV *sv;
    if (PL_min_intro_pending && fill < PL_min_intro_pending) {
	for (off = PL_max_intro_pending; off >= PL_min_intro_pending; off--) {
	    if ((sv = svp[off]) && sv != &PL_sv_undef)
		warn("%s never introduced", SvPVX(sv));
	}
    }
    /* "Deintroduce" my variables that are leaving with this scope. */
    for (off = AvFILLp(PL_comppad_name); off > fill; off--) {
	if ((sv = svp[off]) && sv != &PL_sv_undef && SvIVX(sv) == PAD_MAX)
	    SvIVX(sv) = PL_cop_seqmax;
    }
}

PADOFFSET
pad_alloc(I32 optype, U32 tmptype)
{
    dTHR;
    SV *sv;
    I32 retval;

    if (AvARRAY(PL_comppad) != PL_curpad)
	croak("panic: pad_alloc");
    if (PL_pad_reset_pending)
	pad_reset();
    if (tmptype & SVs_PADMY) {
	do {
	    sv = *av_fetch(PL_comppad, AvFILLp(PL_comppad) + 1, TRUE);
	} while (SvPADBUSY(sv));		/* need a fresh one */
	retval = AvFILLp(PL_comppad);
    }
    else {
	SV **names = AvARRAY(PL_comppad_name);
	SSize_t names_fill = AvFILLp(PL_comppad_name);
	for (;;) {
	    /*
	     * "foreach" index vars temporarily become aliases to non-"my"
	     * values.  Thus we must skip, not just pad values that are
	     * marked as current pad values, but also those with names.
	     */
	    if (++PL_padix <= names_fill &&
		   (sv = names[PL_padix]) && sv != &PL_sv_undef)
		continue;
	    sv = *av_fetch(PL_comppad, PL_padix, TRUE);
	    if (!(SvFLAGS(sv) & (SVs_PADTMP|SVs_PADMY)))
		break;
	}
	retval = PL_padix;
    }
    SvFLAGS(sv) |= tmptype;
    PL_curpad = AvARRAY(PL_comppad);
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log, "0x%lx Pad 0x%lx alloc %ld for %s\n",
			  (unsigned long) thr, (unsigned long) PL_curpad,
			  (long) retval, op_name[optype]));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%lx alloc %ld for %s\n",
			  (unsigned long) PL_curpad,
			  (long) retval, op_name[optype]));
#endif /* USE_THREADS */
    return (PADOFFSET)retval;
}

SV *
pad_sv(PADOFFSET po)
{
    dTHR;
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log, "0x%lx Pad 0x%lx sv %d\n",
			  (unsigned long) thr, (unsigned long) PL_curpad, po));
#else
    if (!po)
	croak("panic: pad_sv po");
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%lx sv %d\n",
			  (unsigned long) PL_curpad, po));
#endif /* USE_THREADS */
    return PL_curpad[po];		/* eventually we'll turn this into a macro */
}

void
pad_free(PADOFFSET po)
{
    dTHR;
    if (!PL_curpad)
	return;
    if (AvARRAY(PL_comppad) != PL_curpad)
	croak("panic: pad_free curpad");
    if (!po)
	croak("panic: pad_free po");
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log, "0x%lx Pad 0x%lx free %d\n",
			  (unsigned long) thr, (unsigned long) PL_curpad, po));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%lx free %d\n",
			  (unsigned long) PL_curpad, po));
#endif /* USE_THREADS */
    if (PL_curpad[po] && PL_curpad[po] != &PL_sv_undef)
	SvPADTMP_off(PL_curpad[po]);
    if ((I32)po < PL_padix)
	PL_padix = po - 1;
}

void
pad_swipe(PADOFFSET po)
{
    dTHR;
    if (AvARRAY(PL_comppad) != PL_curpad)
	croak("panic: pad_swipe curpad");
    if (!po)
	croak("panic: pad_swipe po");
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log, "0x%lx Pad 0x%lx swipe %d\n",
			  (unsigned long) thr, (unsigned long) PL_curpad, po));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%lx swipe %d\n",
			  (unsigned long) PL_curpad, po));
#endif /* USE_THREADS */
    SvPADTMP_off(PL_curpad[po]);
    PL_curpad[po] = NEWSV(1107,0);
    SvPADTMP_on(PL_curpad[po]);
    if ((I32)po < PL_padix)
	PL_padix = po - 1;
}

/* XXX pad_reset() is currently disabled because it results in serious bugs.
 * It causes pad temp TARGs to be shared between OPs. Since TARGs are pushed
 * on the stack by OPs that use them, there are several ways to get an alias
 * to  a shared TARG.  Such an alias will change randomly and unpredictably.
 * We avoid doing this until we can think of a Better Way.
 * GSAR 97-10-29 */
void
pad_reset(void)
{
#ifdef USE_BROKEN_PAD_RESET
    dTHR;
    register I32 po;

    if (AvARRAY(PL_comppad) != PL_curpad)
	croak("panic: pad_reset curpad");
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log, "0x%lx Pad 0x%lx reset\n",
			  (unsigned long) thr, (unsigned long) PL_curpad));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%lx reset\n",
			  (unsigned long) PL_curpad));
#endif /* USE_THREADS */
    if (!PL_tainting) {	/* Can't mix tainted and non-tainted temporaries. */
	for (po = AvMAX(PL_comppad); po > PL_padix_floor; po--) {
	    if (PL_curpad[po] && !SvIMMORTAL(PL_curpad[po]))
		SvPADTMP_off(PL_curpad[po]);
	}
	PL_padix = PL_padix_floor;
    }
#endif
    PL_pad_reset_pending = FALSE;
}

#ifdef USE_THREADS
/* find_threadsv is not reentrant */
PADOFFSET
find_threadsv(char *name)
{
    dTHR;
    char *p;
    PADOFFSET key;
    SV **svp;
    /* We currently only handle names of a single character */
    p = strchr(PL_threadsv_names, *name);
    if (!p)
	return NOT_IN_PAD;
    key = p - PL_threadsv_names;
    MUTEX_LOCK(&thr->mutex);
    svp = av_fetch(thr->threadsv, key, FALSE);
    if (svp)
	MUTEX_UNLOCK(&thr->mutex);
    else {
	SV *sv = NEWSV(0, 0);
	av_store(thr->threadsv, key, sv);
	thr->threadsvp = AvARRAY(thr->threadsv);
	MUTEX_UNLOCK(&thr->mutex);
	/*
	 * Some magic variables used to be automagically initialised
	 * in gv_fetchpv. Those which are now per-thread magicals get
	 * initialised here instead.
	 */
	switch (*name) {
	case '_':
	    break;
	case ';':
	    sv_setpv(sv, "\034");
	    sv_magic(sv, 0, 0, name, 1); 
	    break;
	case '&':
	case '`':
	case '\'':
	    PL_sawampersand = TRUE;
	    /* FALL THROUGH */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    SvREADONLY_on(sv);
	    /* FALL THROUGH */

	/* XXX %! tied to Errno.pm needs to be added here.
	 * See gv_fetchpv(). */
	/* case '!': */

	default:
	    sv_magic(sv, 0, 0, name, 1); 
	}
	DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			      "find_threadsv: new SV %p for $%s%c\n",
			      sv, (*name < 32) ? "^" : "",
			      (*name < 32) ? toCTRL(*name) : *name));
    }
    return key;
}
#endif /* USE_THREADS */

/* Destructor */

void
op_free(OP *o)
{
    register OP *kid, *nextkid;

    if (!o || o->op_seq == (U16)-1)
	return;

    if (o->op_flags & OPf_KIDS) {
	for (kid = cUNOPo->op_first; kid; kid = nextkid) {
	    nextkid = kid->op_sibling; /* Get before next freeing kid */
	    op_free(kid);
	}
    }

    switch (o->op_type) {
    case OP_NULL:
	o->op_targ = 0;	/* Was holding old type, if any. */
	break;
    case OP_ENTEREVAL:
	o->op_targ = 0;	/* Was holding hints. */
	break;
#ifdef USE_THREADS
    case OP_ENTERITER:
	if (!(o->op_flags & OPf_SPECIAL))
	    break;
	/* FALL THROUGH */
    case OP_THREADSV:
	o->op_targ = 0;	/* Was holding index into thr->threadsv AV. */
	break;
#endif /* USE_THREADS */
    default:
	if (!(o->op_flags & OPf_REF)
	    || (check[o->op_type] != FUNC_NAME_TO_PTR(ck_ftst)))
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_GV:
    case OP_AELEMFAST:
	SvREFCNT_dec(cGVOPo->op_gv);
	break;
    case OP_NEXTSTATE:
    case OP_DBSTATE:
	Safefree(cCOPo->cop_label);
	SvREFCNT_dec(cCOPo->cop_filegv);
	break;
    case OP_CONST:
	SvREFCNT_dec(cSVOPo->op_sv);
	break;
    case OP_GOTO:
    case OP_NEXT:
    case OP_LAST:
    case OP_REDO:
	if (o->op_flags & (OPf_SPECIAL|OPf_STACKED|OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_TRANS:
	Safefree(cPVOPo->op_pv);
	break;
    case OP_SUBST:
	op_free(cPMOPo->op_pmreplroot);
	/* FALL THROUGH */
    case OP_PUSHRE:
    case OP_MATCH:
    case OP_QR:
	ReREFCNT_dec(cPMOPo->op_pmregexp);
	break;
    }

    if (o->op_targ > 0)
	pad_free(o->op_targ);

    Safefree(o);
}

STATIC void
null(OP *o)
{
    if (o->op_type != OP_NULL && o->op_type != OP_THREADSV && o->op_targ > 0)
	pad_free(o->op_targ);
    o->op_targ = o->op_type;
    o->op_type = OP_NULL;
    o->op_ppaddr = ppaddr[OP_NULL];
}

/* Contextualizers */

#define LINKLIST(o) ((o)->op_next ? (o)->op_next : linklist((OP*)o))

OP *
linklist(OP *o)
{
    register OP *kid;

    if (o->op_next)
	return o->op_next;

    /* establish postfix order */
    if (cUNOPo->op_first) {
	o->op_next = LINKLIST(cUNOPo->op_first);
	for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling) {
	    if (kid->op_sibling)
		kid->op_next = LINKLIST(kid->op_sibling);
	    else
		kid->op_next = o;
	}
    }
    else
	o->op_next = o;

    return o->op_next;
}

OP *
scalarkids(OP *o)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    scalar(kid);
    }
    return o;
}

STATIC OP *
scalarboolean(OP *o)
{
    if (PL_dowarn &&
	o->op_type == OP_SASSIGN && cBINOPo->op_first->op_type == OP_CONST) {
	dTHR;
	line_t oldline = PL_curcop->cop_line;

	if (PL_copline != NOLINE)
	    PL_curcop->cop_line = PL_copline;
	warn("Found = in conditional, should be ==");
	PL_curcop->cop_line = oldline;
    }
    return scalar(o);
}

OP *
scalar(OP *o)
{
    OP *kid;

    /* assumes no premature commitment */
    if (!o || (o->op_flags & OPf_WANT) || PL_error_count
	 || o->op_type == OP_RETURN)
	return o;

    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_SCALAR;

    switch (o->op_type) {
    case OP_REPEAT:
	if (o->op_private & OPpREPEAT_DOLIST)
	    null(((LISTOP*)cBINOPo->op_first)->op_first);
	scalar(cBINOPo->op_first);
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalar(kid);
	break;
    case OP_SPLIT:
	if ((kid = cLISTOPo->op_first) && kid->op_type == OP_PUSHRE) {
	    if (!kPMOP->op_pmreplroot)
		deprecate("implicit split to @_");
	}
	/* FALL THROUGH */
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
    case OP_NULL:
    default:
	if (o->op_flags & OPf_KIDS) {
	    for (kid = cUNOPo->op_first; kid; kid = kid->op_sibling)
		scalar(kid);
	}
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOPo->op_first;
	scalar(kid);
	while (kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		scalar(kid);
	}
	WITH_THR(PL_curcop = &PL_compiling);
	break;
    case OP_SCOPE:
    case OP_LINESEQ:
    case OP_LIST:
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		scalar(kid);
	}
	WITH_THR(PL_curcop = &PL_compiling);
	break;
    }
    return o;
}

OP *
scalarvoid(OP *o)
{
    OP *kid;
    char* useless = 0;
    SV* sv;

    /* assumes no premature commitment */
    U8 want = o->op_flags & OPf_WANT;
    if (!o || (want && want != OPf_WANT_SCALAR) || PL_error_count
	 || o->op_type == OP_RETURN)
	return o;

    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

    switch (o->op_type) {
    default:
	if (!(opargs[o->op_type] & OA_FOLDCONST))
	    break;
	/* FALL THROUGH */
    case OP_REPEAT:
	if (o->op_flags & OPf_STACKED)
	    break;
	goto func_ops;
    case OP_SUBSTR:
	if (o->op_private == 4)
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_WANTARRAY:
    case OP_GV:
    case OP_PADSV:
    case OP_PADAV:
    case OP_PADHV:
    case OP_PADANY:
    case OP_AV2ARYLEN:
    case OP_REF:
    case OP_REFGEN:
    case OP_SREFGEN:
    case OP_DEFINED:
    case OP_HEX:
    case OP_OCT:
    case OP_LENGTH:
    case OP_VEC:
    case OP_INDEX:
    case OP_RINDEX:
    case OP_SPRINTF:
    case OP_AELEM:
    case OP_AELEMFAST:
    case OP_ASLICE:
    case OP_HELEM:
    case OP_HSLICE:
    case OP_UNPACK:
    case OP_PACK:
    case OP_JOIN:
    case OP_LSLICE:
    case OP_ANONLIST:
    case OP_ANONHASH:
    case OP_SORT:
    case OP_REVERSE:
    case OP_RANGE:
    case OP_FLIP:
    case OP_FLOP:
    case OP_CALLER:
    case OP_FILENO:
    case OP_EOF:
    case OP_TELL:
    case OP_GETSOCKNAME:
    case OP_GETPEERNAME:
    case OP_READLINK:
    case OP_TELLDIR:
    case OP_GETPPID:
    case OP_GETPGRP:
    case OP_GETPRIORITY:
    case OP_TIME:
    case OP_TMS:
    case OP_LOCALTIME:
    case OP_GMTIME:
    case OP_GHBYNAME:
    case OP_GHBYADDR:
    case OP_GHOSTENT:
    case OP_GNBYNAME:
    case OP_GNBYADDR:
    case OP_GNETENT:
    case OP_GPBYNAME:
    case OP_GPBYNUMBER:
    case OP_GPROTOENT:
    case OP_GSBYNAME:
    case OP_GSBYPORT:
    case OP_GSERVENT:
    case OP_GPWNAM:
    case OP_GPWUID:
    case OP_GGRNAM:
    case OP_GGRGID:
    case OP_GETLOGIN:
      func_ops:
	if (!(o->op_private & OPpLVAL_INTRO))
	    useless = op_desc[o->op_type];
	break;

    case OP_RV2GV:
    case OP_RV2SV:
    case OP_RV2AV:
    case OP_RV2HV:
	if (!(o->op_private & OPpLVAL_INTRO) &&
		(!o->op_sibling || o->op_sibling->op_type != OP_READLINE))
	    useless = "a variable";
	break;

    case OP_NEXTSTATE:
    case OP_DBSTATE:
	WITH_THR(PL_curcop = ((COP*)o));		/* for warning below */
	break;

    case OP_CONST:
	sv = cSVOPo->op_sv;
	if (PL_dowarn) {
	    useless = "a constant";
	    if (SvNIOK(sv) && (SvNV(sv) == 0.0 || SvNV(sv) == 1.0))
		useless = 0;
	    else if (SvPOK(sv)) {
		if (strnEQ(SvPVX(sv), "di", 2) ||
		    strnEQ(SvPVX(sv), "ds", 2) ||
		    strnEQ(SvPVX(sv), "ig", 2))
			useless = 0;
	    }
	}
	null(o);		/* don't execute a constant */
	SvREFCNT_dec(sv);	/* don't even remember it */
	break;

    case OP_POSTINC:
	o->op_type = OP_PREINC;		/* pre-increment is faster */
	o->op_ppaddr = ppaddr[OP_PREINC];
	break;

    case OP_POSTDEC:
	o->op_type = OP_PREDEC;		/* pre-decrement is faster */
	o->op_ppaddr = ppaddr[OP_PREDEC];
	break;

    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;

    case OP_NULL:
	if (o->op_targ == OP_NEXTSTATE || o->op_targ == OP_DBSTATE)
	    WITH_THR(PL_curcop = ((COP*)o));	/* for warning below */
	if (o->op_flags & OPf_STACKED)
	    break;
	/* FALL THROUGH */
    case OP_ENTERTRY:
    case OP_ENTER:
    case OP_SCALAR:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_SCOPE:
    case OP_LEAVE:
    case OP_LEAVETRY:
    case OP_LEAVELOOP:
    case OP_LINESEQ:
    case OP_LIST:
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;
    case OP_ENTEREVAL:
	scalarkids(o);
	break;
    case OP_REQUIRE:
	/* all requires must return a boolean value */
	o->op_flags &= ~OPf_WANT;
	return scalar(o);
    case OP_SPLIT:
	if ((kid = cLISTOPo->op_first) && kid->op_type == OP_PUSHRE) {
	    if (!kPMOP->op_pmreplroot)
		deprecate("implicit split to @_");
	}
	break;
    }
    if (useless && PL_dowarn)
	warn("Useless use of %s in void context", useless);
    return o;
}

OP *
listkids(OP *o)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    list(kid);
    }
    return o;
}

OP *
list(OP *o)
{
    OP *kid;

    /* assumes no premature commitment */
    if (!o || (o->op_flags & OPf_WANT) || PL_error_count
	 || o->op_type == OP_RETURN)
	return o;

    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_LIST;

    switch (o->op_type) {
    case OP_FLOP:
    case OP_REPEAT:
	list(cBINOPo->op_first);
	break;
    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    list(kid);
	break;
    default:
    case OP_MATCH:
    case OP_QR:
    case OP_SUBST:
    case OP_NULL:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	if (!o->op_next && cUNOPo->op_first->op_type == OP_FLOP) {
	    list(cBINOPo->op_first);
	    return gen_constant_list(o);
	}
    case OP_LIST:
	listkids(o);
	break;
    case OP_LEAVE:
    case OP_LEAVETRY:
	kid = cLISTOPo->op_first;
	list(kid);
	while (kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		list(kid);
	}
	WITH_THR(PL_curcop = &PL_compiling);
	break;
    case OP_SCOPE:
    case OP_LINESEQ:
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling) {
	    if (kid->op_sibling)
		scalarvoid(kid);
	    else
		list(kid);
	}
	WITH_THR(PL_curcop = &PL_compiling);
	break;
    case OP_REQUIRE:
	/* all requires must return a boolean value */
	o->op_flags &= ~OPf_WANT;
	return scalar(o);
    }
    return o;
}

OP *
scalarseq(OP *o)
{
    OP *kid;

    if (o) {
	if (o->op_type == OP_LINESEQ ||
	     o->op_type == OP_SCOPE ||
	     o->op_type == OP_LEAVE ||
	     o->op_type == OP_LEAVETRY)
	{
	    dTHR;
	    for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling) {
		if (kid->op_sibling) {
		    scalarvoid(kid);
		}
	    }
	    PL_curcop = &PL_compiling;
	}
	o->op_flags &= ~OPf_PARENS;
	if (PL_hints & HINT_BLOCK_SCOPE)
	    o->op_flags |= OPf_PARENS;
    }
    else
	o = newOP(OP_STUB, 0);
    return o;
}

STATIC OP *
modkids(OP *o, I32 type)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    mod(kid, type);
    }
    return o;
}

OP *
mod(OP *o, I32 type)
{
    dTHR;
    OP *kid;
    SV *sv;
    STRLEN n_a;

    if (!o || PL_error_count)
	return o;

    switch (o->op_type) {
    case OP_UNDEF:
	PL_modcount++;
	return o;
    case OP_CONST:
	if (!(o->op_private & (OPpCONST_ARYBASE)))
	    goto nomod;
	if (PL_eval_start && PL_eval_start->op_type == OP_CONST) {
	    PL_compiling.cop_arybase = (I32)SvIV(((SVOP*)PL_eval_start)->op_sv);
	    PL_eval_start = 0;
	}
	else if (!type) {
	    SAVEI32(PL_compiling.cop_arybase);
	    PL_compiling.cop_arybase = 0;
	}
	else if (type == OP_REFGEN)
	    goto nomod;
	else
	    croak("That use of $[ is unsupported");
	break;
    case OP_STUB:
	if (o->op_flags & OPf_PARENS)
	    break;
	goto nomod;
    case OP_ENTERSUB:
	if ((type == OP_UNDEF || type == OP_REFGEN) &&
	    !(o->op_flags & OPf_STACKED)) {
	    o->op_type = OP_RV2CV;		/* entersub => rv2cv */
	    o->op_ppaddr = ppaddr[OP_RV2CV];
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    null(((LISTOP*)cUNOPo->op_first)->op_first);/* disable pushmark */
	    break;
	}
	/* FALL THROUGH */
    default:
      nomod:
	/* grep, foreach, subcalls, refgen */
	if (type == OP_GREPSTART || type == OP_ENTERSUB || type == OP_REFGEN)
	    break;
	yyerror(form("Can't modify %s in %s",
		     op_desc[o->op_type],
		     type ? op_desc[type] : "local"));
	return o;

    case OP_PREINC:
    case OP_PREDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
    case OP_REPEAT:
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_CONCAT:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT:
    case OP_BIT_AND:
    case OP_BIT_XOR:
    case OP_BIT_OR:
    case OP_I_MULTIPLY:
    case OP_I_DIVIDE:
    case OP_I_MODULO:
    case OP_I_ADD:
    case OP_I_SUBTRACT:
	if (!(o->op_flags & OPf_STACKED))
	    goto nomod;
	PL_modcount++;
	break;
	
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    mod(kid, type);
	break;

    case OP_RV2AV:
    case OP_RV2HV:
	if (!type && cUNOPo->op_first->op_type != OP_GV)
	    croak("Can't localize through a reference");
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS) {
	    PL_modcount = 10000;
	    return o;		/* Treat \(@foo) like ordinary list. */
	}
	/* FALL THROUGH */
    case OP_RV2GV:
	if (scalar_mod_type(o, type))
	    goto nomod;
	ref(cUNOPo->op_first, o->op_type);
	/* FALL THROUGH */
    case OP_AASSIGN:
    case OP_ASLICE:
    case OP_HSLICE:
    case OP_NEXTSTATE:
    case OP_DBSTATE:
    case OP_REFGEN:
    case OP_CHOMP:
	PL_modcount = 10000;
	break;
    case OP_RV2SV:
	if (!type && cUNOPo->op_first->op_type != OP_GV)
	    croak("Can't localize through a reference");
	ref(cUNOPo->op_first, o->op_type);
	/* FALL THROUGH */
    case OP_GV:
    case OP_AV2ARYLEN:
	PL_hints |= HINT_BLOCK_SCOPE;
    case OP_SASSIGN:
    case OP_AELEMFAST:
	PL_modcount++;
	break;

    case OP_PADAV:
    case OP_PADHV:
	PL_modcount = 10000;
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS)
	    return o;		/* Treat \(@foo) like ordinary list. */
	if (scalar_mod_type(o, type))
	    goto nomod;
	/* FALL THROUGH */
    case OP_PADSV:
	PL_modcount++;
	if (!type)
	    croak("Can't localize lexical variable %s",
		SvPV(*av_fetch(PL_comppad_name, o->op_targ, 4), n_a));
	break;

#ifdef USE_THREADS
    case OP_THREADSV:
	PL_modcount++;	/* XXX ??? */
	break;
#endif /* USE_THREADS */

    case OP_PUSHMARK:
	break;
	
    case OP_KEYS:
	if (type != OP_SASSIGN)
	    goto nomod;
	goto lvalue_func;
    case OP_SUBSTR:
	if (o->op_private == 4) /* don't allow 4 arg substr as lvalue */
	    goto nomod;
	/* FALL THROUGH */
    case OP_POS:
    case OP_VEC:
      lvalue_func:
	pad_free(o->op_targ);
	o->op_targ = pad_alloc(o->op_type, SVs_PADMY);
	assert(SvTYPE(PAD_SV(o->op_targ)) == SVt_NULL);
	if (o->op_flags & OPf_KIDS)
	    mod(cBINOPo->op_first->op_sibling, type);
	break;

    case OP_AELEM:
    case OP_HELEM:
	ref(cBINOPo->op_first, o->op_type);
	if (type == OP_ENTERSUB &&
	     !(o->op_private & (OPpLVAL_INTRO | OPpDEREF)))
	    o->op_private |= OPpLVAL_DEFER;
	PL_modcount++;
	break;

    case OP_SCOPE:
    case OP_LEAVE:
    case OP_ENTER:
	if (o->op_flags & OPf_KIDS)
	    mod(cLISTOPo->op_last, type);
	break;

    case OP_NULL:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	if (o->op_targ != OP_LIST) {
	    mod(cBINOPo->op_first, type);
	    break;
	}
	/* FALL THROUGH */
    case OP_LIST:
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    mod(kid, type);
	break;
    }
    o->op_flags |= OPf_MOD;

    if (type == OP_AASSIGN || type == OP_SASSIGN)
	o->op_flags |= OPf_SPECIAL|OPf_REF;
    else if (!type) {
	o->op_private |= OPpLVAL_INTRO;
	o->op_flags &= ~OPf_SPECIAL;
	PL_hints |= HINT_BLOCK_SCOPE;
    }
    else if (type != OP_GREPSTART && type != OP_ENTERSUB)
	o->op_flags |= OPf_REF;
    return o;
}

static bool
scalar_mod_type(OP *o, I32 type)
{
    switch (type) {
    case OP_SASSIGN:
	if (o->op_type == OP_RV2GV)
	    return FALSE;
	/* FALL THROUGH */
    case OP_PREINC:
    case OP_PREDEC:
    case OP_POSTINC:
    case OP_POSTDEC:
    case OP_I_PREINC:
    case OP_I_PREDEC:
    case OP_I_POSTINC:
    case OP_I_POSTDEC:
    case OP_POW:
    case OP_MULTIPLY:
    case OP_DIVIDE:
    case OP_MODULO:
    case OP_REPEAT:
    case OP_ADD:
    case OP_SUBTRACT:
    case OP_I_MULTIPLY:
    case OP_I_DIVIDE:
    case OP_I_MODULO:
    case OP_I_ADD:
    case OP_I_SUBTRACT:
    case OP_LEFT_SHIFT:
    case OP_RIGHT_SHIFT:
    case OP_BIT_AND:
    case OP_BIT_XOR:
    case OP_BIT_OR:
    case OP_CONCAT:
    case OP_SUBST:
    case OP_TRANS:
    case OP_READ:
    case OP_SYSREAD:
    case OP_RECV:
    case OP_ANDASSIGN:	/* may work later */
    case OP_ORASSIGN:	/* may work later */
	return TRUE;
    default:
	return FALSE;
    }
}

OP *
refkids(OP *o, I32 type)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    ref(kid, type);
    }
    return o;
}

OP *
ref(OP *o, I32 type)
{
    OP *kid;

    if (!o || PL_error_count)
	return o;

    switch (o->op_type) {
    case OP_ENTERSUB:
	if ((type == OP_DEFINED || type == OP_LOCK) &&
	    !(o->op_flags & OPf_STACKED)) {
	    o->op_type = OP_RV2CV;             /* entersub => rv2cv */
	    o->op_ppaddr = ppaddr[OP_RV2CV];
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    null(((LISTOP*)cUNOPo->op_first)->op_first);	/* disable pushmark */
	    o->op_flags |= OPf_SPECIAL;
	}
	break;

    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    ref(kid, type);
	break;
    case OP_RV2SV:
	ref(cUNOPo->op_first, o->op_type);
	/* FALL THROUGH */
    case OP_PADSV:
	if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV) {
	    o->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			      : type == OP_RV2HV ? OPpDEREF_HV
			      : OPpDEREF_SV);
	    o->op_flags |= OPf_MOD;
	}
	break;
      
    case OP_THREADSV:
	o->op_flags |= OPf_MOD;		/* XXX ??? */
	break;

    case OP_RV2AV:
    case OP_RV2HV:
	o->op_flags |= OPf_REF;
	/* FALL THROUGH */
    case OP_RV2GV:
	ref(cUNOPo->op_first, o->op_type);
	break;

    case OP_PADAV:
    case OP_PADHV:
	o->op_flags |= OPf_REF;
	break;

    case OP_SCALAR:
    case OP_NULL:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	ref(cBINOPo->op_first, type);
	break;
    case OP_AELEM:
    case OP_HELEM:
	ref(cBINOPo->op_first, o->op_type);
	if (type == OP_RV2SV || type == OP_RV2AV || type == OP_RV2HV) {
	    o->op_private |= (type == OP_RV2AV ? OPpDEREF_AV
			      : type == OP_RV2HV ? OPpDEREF_HV
			      : OPpDEREF_SV);
	    o->op_flags |= OPf_MOD;
	}
	break;

    case OP_SCOPE:
    case OP_LEAVE:
    case OP_ENTER:
    case OP_LIST:
	if (!(o->op_flags & OPf_KIDS))
	    break;
	ref(cLISTOPo->op_last, type);
	break;
    default:
	break;
    }
    return scalar(o);

}

OP *
my(OP *o)
{
    OP *kid;
    I32 type;

    if (!o || PL_error_count)
	return o;

    type = o->op_type;
    if (type == OP_LIST) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    my(kid);
    } else if (type == OP_UNDEF) {
	return o;
    } else if (type != OP_PADSV &&
	     type != OP_PADAV &&
	     type != OP_PADHV &&
	     type != OP_PUSHMARK)
    {
	yyerror(form("Can't declare %s in my", op_desc[o->op_type]));
	return o;
    }
    o->op_flags |= OPf_MOD;
    o->op_private |= OPpLVAL_INTRO;
    return o;
}

OP *
sawparens(OP *o)
{
    if (o)
	o->op_flags |= OPf_PARENS;
    return o;
}

OP *
bind_match(I32 type, OP *left, OP *right)
{
    OP *o;

    if (PL_dowarn &&
	(left->op_type == OP_RV2AV ||
	 left->op_type == OP_RV2HV ||
	 left->op_type == OP_PADAV ||
	 left->op_type == OP_PADHV)) {
	char *desc = op_desc[(right->op_type == OP_SUBST ||
			      right->op_type == OP_TRANS)
			     ? right->op_type : OP_MATCH];
	char *sample = ((left->op_type == OP_RV2AV ||
			 left->op_type == OP_PADAV)
			? "@array" : "%hash");
	warn("Applying %s to %s will act on scalar(%s)", desc, sample, sample);
    }

    if (right->op_type == OP_MATCH ||
	right->op_type == OP_SUBST ||
	right->op_type == OP_TRANS) {
	right->op_flags |= OPf_STACKED;
	if (right->op_type != OP_MATCH)
	    left = mod(left, right->op_type);
	if (right->op_type == OP_TRANS)
	    o = newBINOP(OP_NULL, OPf_STACKED, scalar(left), right);
	else
	    o = prepend_elem(right->op_type, scalar(left), right);
	if (type == OP_NOT)
	    return newUNOP(OP_NOT, 0, scalar(o));
	return o;
    }
    else
	return bind_match(type, left,
		pmruntime(newPMOP(OP_MATCH, 0), right, Nullop));
}

OP *
invert(OP *o)
{
    if (!o)
	return o;
    /* XXX need to optimize away NOT NOT here?  Or do we let optimizer do it? */
    return newUNOP(OP_NOT, OPf_SPECIAL, scalar(o));
}

OP *
scope(OP *o)
{
    if (o) {
	if (o->op_flags & OPf_PARENS || PERLDB_NOOPT || PL_tainting) {
	    o = prepend_elem(OP_LINESEQ, newOP(OP_ENTER, 0), o);
	    o->op_type = OP_LEAVE;
	    o->op_ppaddr = ppaddr[OP_LEAVE];
	}
	else {
	    if (o->op_type == OP_LINESEQ) {
		OP *kid;
		o->op_type = OP_SCOPE;
		o->op_ppaddr = ppaddr[OP_SCOPE];
		kid = ((LISTOP*)o)->op_first;
		if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE){
		    SvREFCNT_dec(((COP*)kid)->cop_filegv);
		    null(kid);
		}
	    }
	    else
		o = newLISTOP(OP_SCOPE, 0, o, Nullop);
	}
    }
    return o;
}

void
save_hints(void)
{
    SAVEI32(PL_hints);
    SAVESPTR(GvHV(PL_hintgv));
    GvHV(PL_hintgv) = newHVhv(GvHV(PL_hintgv));
    SAVEFREESV(GvHV(PL_hintgv));
}

int
block_start(int full)
{
    dTHR;
    int retval = PL_savestack_ix;

    SAVEI32(PL_comppad_name_floor);
    if (full) {
	if ((PL_comppad_name_fill = AvFILLp(PL_comppad_name)) > 0)
	    PL_comppad_name_floor = PL_comppad_name_fill;
	else
	    PL_comppad_name_floor = 0;
    }
    SAVEI32(PL_min_intro_pending);
    SAVEI32(PL_max_intro_pending);
    PL_min_intro_pending = 0;
    SAVEI32(PL_comppad_name_fill);
    SAVEI32(PL_padix_floor);
    PL_padix_floor = PL_padix;
    PL_pad_reset_pending = FALSE;
    SAVEHINTS();
    PL_hints &= ~HINT_BLOCK_SCOPE;
    return retval;
}

OP*
block_end(I32 floor, OP *seq)
{
    dTHR;
    int needblockscope = PL_hints & HINT_BLOCK_SCOPE;
    OP* retval = scalarseq(seq);
    LEAVE_SCOPE(floor);
    PL_pad_reset_pending = FALSE;
    if (needblockscope)
	PL_hints |= HINT_BLOCK_SCOPE; /* propagate out */
    pad_leavemy(PL_comppad_name_fill);
    PL_cop_seqmax++;
    return retval;
}

STATIC OP *
newDEFSVOP(void)
{
#ifdef USE_THREADS
    OP *o = newOP(OP_THREADSV, 0);
    o->op_targ = find_threadsv("_");
    return o;
#else
    return newSVREF(newGVOP(OP_GV, 0, PL_defgv));
#endif /* USE_THREADS */
}

void
newPROG(OP *o)
{
    dTHR;
    if (PL_in_eval) {
	PL_eval_root = newUNOP(OP_LEAVEEVAL, ((PL_in_eval & 4) ? OPf_SPECIAL : 0), o);
	PL_eval_start = linklist(PL_eval_root);
	PL_eval_root->op_next = 0;
	peep(PL_eval_start);
    }
    else {
	if (!o)
	    return;
	PL_main_root = scope(sawparens(scalarvoid(o)));
	PL_curcop = &PL_compiling;
	PL_main_start = LINKLIST(PL_main_root);
	PL_main_root->op_next = 0;
	peep(PL_main_start);
	PL_compcv = 0;

	/* Register with debugger */
	if (PERLDB_INTER) {
	    CV *cv = perl_get_cv("DB::postponed", FALSE);
	    if (cv) {
		dSP;
		PUSHMARK(SP);
		XPUSHs((SV*)PL_compiling.cop_filegv);
		PUTBACK;
		perl_call_sv((SV*)cv, G_DISCARD);
	    }
	}
    }
}

OP *
localize(OP *o, I32 lex)
{
    if (o->op_flags & OPf_PARENS)
	list(o);
    else {
	if (PL_dowarn && PL_bufptr > PL_oldbufptr && PL_bufptr[-1] == ',') {
	    char *s;
	    for (s = PL_bufptr; *s && (isALNUM(*s) || strchr("@$%, ",*s)); s++) ;
	    if (*s == ';' || *s == '=')
		warn("Parens missing around \"%s\" list", lex ? "my" : "local");
	}
    }
    PL_in_my = FALSE;
    PL_in_my_stash = Nullhv;
    if (lex)
	return my(o);
    else
	return mod(o, OP_NULL);		/* a bit kludgey */
}

OP *
jmaybe(OP *o)
{
    if (o->op_type == OP_LIST) {
	OP *o2;
#ifdef USE_THREADS
	o2 = newOP(OP_THREADSV, 0);
	o2->op_targ = find_threadsv(";");
#else
	o2 = newSVREF(newGVOP(OP_GV, 0, gv_fetchpv(";", TRUE, SVt_PV))),
#endif /* USE_THREADS */
	o = convert(OP_JOIN, 0, prepend_elem(OP_LIST, o2, o));
    }
    return o;
}

OP *
fold_constants(register OP *o)
{
    dTHR;
    register OP *curop;
    I32 type = o->op_type;
    SV *sv;

    if (opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (opargs[type] & OA_TARGET)
	o->op_targ = pad_alloc(type, SVs_PADTMP);

    if ((opargs[type] & OA_OTHERINT) && (PL_hints & HINT_INTEGER))
	o->op_ppaddr = ppaddr[type = ++(o->op_type)];

    if (!(opargs[type] & OA_FOLDCONST))
	goto nope;

    switch (type) {
    case OP_SPRINTF:
    case OP_UCFIRST:
    case OP_LCFIRST:
    case OP_UC:
    case OP_LC:
    case OP_SLT:
    case OP_SGT:
    case OP_SLE:
    case OP_SGE:
    case OP_SCMP:

	if (o->op_private & OPpLOCALE)
	    goto nope;
    }

    if (PL_error_count)
	goto nope;		/* Don't try to run w/ errors */

    for (curop = LINKLIST(o); curop != o; curop = LINKLIST(curop)) {
	if (curop->op_type != OP_CONST &&
		curop->op_type != OP_LIST &&
		curop->op_type != OP_SCALAR &&
		curop->op_type != OP_NULL &&
		curop->op_type != OP_PUSHMARK) {
	    goto nope;
	}
    }

    curop = LINKLIST(o);
    o->op_next = 0;
    PL_op = curop;
    CALLRUNOPS();
    sv = *(PL_stack_sp--);
    if (o->op_targ && sv == PAD_SV(o->op_targ))	/* grab pad temp? */
	pad_swipe(o->op_targ);
    else if (SvTEMP(sv)) {			/* grab mortal temp? */
	(void)SvREFCNT_inc(sv);
	SvTEMP_off(sv);
    }
    op_free(o);
    if (type == OP_RV2GV)
	return newGVOP(OP_GV, 0, (GV*)sv);
    else {
	/* try to smush double to int, but don't smush -2.0 to -2 */
	if ((SvFLAGS(sv) & (SVf_IOK|SVf_NOK|SVf_POK)) == SVf_NOK &&
	    type != OP_NEGATE)
	{
	    IV iv = SvIV(sv);
	    if ((double)iv == SvNV(sv)) {
		SvREFCNT_dec(sv);
		sv = newSViv(iv);
	    }
	    else
		SvIOK_off(sv);			/* undo SvIV() damage */
	}
	return newSVOP(OP_CONST, 0, sv);
    }

  nope:
    if (!(opargs[type] & OA_OTHERINT))
	return o;

    if (!(PL_hints & HINT_INTEGER)) {
	if (type == OP_DIVIDE || !(o->op_flags & OPf_KIDS))
	    return o;

	for (curop = ((UNOP*)o)->op_first; curop; curop = curop->op_sibling) {
	    if (curop->op_type == OP_CONST) {
		if (SvIOK(((SVOP*)curop)->op_sv))
		    continue;
		return o;
	    }
	    if (opargs[curop->op_type] & OA_RETINTEGER)
		continue;
	    return o;
	}
	o->op_ppaddr = ppaddr[++(o->op_type)];
    }

    return o;
}

OP *
gen_constant_list(register OP *o)
{
    dTHR;
    register OP *curop;
    I32 oldtmps_floor = PL_tmps_floor;

    list(o);
    if (PL_error_count)
	return o;		/* Don't attempt to run with errors */

    PL_op = curop = LINKLIST(o);
    o->op_next = 0;
    pp_pushmark(ARGS);
    CALLRUNOPS();
    PL_op = curop;
    pp_anonlist(ARGS);
    PL_tmps_floor = oldtmps_floor;

    o->op_type = OP_RV2AV;
    o->op_ppaddr = ppaddr[OP_RV2AV];
    curop = ((UNOP*)o)->op_first;
    ((UNOP*)o)->op_first = newSVOP(OP_CONST, 0, SvREFCNT_inc(*PL_stack_sp--));
    op_free(curop);
    linklist(o);
    return list(o);
}

OP *
convert(I32 type, I32 flags, OP *o)
{
    OP *kid;
    OP *last = 0;

    if (!o || o->op_type != OP_LIST)
	o = newLISTOP(OP_LIST, 0, o, Nullop);
    else
	o->op_flags &= ~OPf_WANT;

    if (!(opargs[type] & OA_MARK))
	null(cLISTOPo->op_first);

    o->op_type = type;
    o->op_ppaddr = ppaddr[type];
    o->op_flags |= flags;

    o = CHECKOP(type, o);
    if (o->op_type != type)
	return o;

    if (cLISTOPo->op_children < 7) {
	/* XXX do we really need to do this if we're done appending?? */
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    last = kid;
	cLISTOPo->op_last = last;	/* in case check substituted last arg */
    }

    return fold_constants(o);
}

/* List constructors */

OP *
append_elem(I32 type, OP *first, OP *last)
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (first->op_type != type || type==OP_LIST && first->op_flags & OPf_PARENS)
	    return newLISTOP(type, 0, first, last);

    if (first->op_flags & OPf_KIDS)
	((LISTOP*)first)->op_last->op_sibling = last;
    else {
	first->op_flags |= OPf_KIDS;
	((LISTOP*)first)->op_first = last;
    }
    ((LISTOP*)first)->op_last = last;
    ((LISTOP*)first)->op_children++;
    return first;
}

OP *
append_list(I32 type, LISTOP *first, LISTOP *last)
{
    if (!first)
	return (OP*)last;

    if (!last)
	return (OP*)first;

    if (first->op_type != type)
	return prepend_elem(type, (OP*)first, (OP*)last);

    if (last->op_type != type)
	return append_elem(type, (OP*)first, (OP*)last);

    first->op_last->op_sibling = last->op_first;
    first->op_last = last->op_last;
    first->op_children += last->op_children;
    if (first->op_children)
	first->op_flags |= OPf_KIDS;

    Safefree(last);
    return (OP*)first;
}

OP *
prepend_elem(I32 type, OP *first, OP *last)
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (last->op_type == type) {
	if (type == OP_LIST) {	/* already a PUSHMARK there */
	    first->op_sibling = ((LISTOP*)last)->op_first->op_sibling;
	    ((LISTOP*)last)->op_first->op_sibling = first;
	}
	else {
	    if (!(last->op_flags & OPf_KIDS)) {
		((LISTOP*)last)->op_last = first;
		last->op_flags |= OPf_KIDS;
	    }
	    first->op_sibling = ((LISTOP*)last)->op_first;
	    ((LISTOP*)last)->op_first = first;
	}
	((LISTOP*)last)->op_children++;
	return last;
    }

    return newLISTOP(type, 0, first, last);
}

/* Constructors */

OP *
newNULLLIST(void)
{
    return newOP(OP_STUB, 0);
}

OP *
force_list(OP *o)
{
    if (!o || o->op_type != OP_LIST)
	o = newLISTOP(OP_LIST, 0, o, Nullop);
    null(o);
    return o;
}

OP *
newLISTOP(I32 type, I32 flags, OP *first, OP *last)
{
    LISTOP *listop;

    Newz(1101, listop, 1, LISTOP);

    listop->op_type = type;
    listop->op_ppaddr = ppaddr[type];
    listop->op_children = (first != 0) + (last != 0);
    listop->op_flags = flags;

    if (!last && first)
	last = first;
    else if (!first && last)
	first = last;
    else if (first)
	first->op_sibling = last;
    listop->op_first = first;
    listop->op_last = last;
    if (type == OP_LIST) {
	OP* pushop;
	pushop = newOP(OP_PUSHMARK, 0);
	pushop->op_sibling = first;
	listop->op_first = pushop;
	listop->op_flags |= OPf_KIDS;
	if (!last)
	    listop->op_last = pushop;
    }
    else if (listop->op_children)
	listop->op_flags |= OPf_KIDS;

    return (OP*)listop;
}

OP *
newOP(I32 type, I32 flags)
{
    OP *o;
    Newz(1101, o, 1, OP);
    o->op_type = type;
    o->op_ppaddr = ppaddr[type];
    o->op_flags = flags;

    o->op_next = o;
    o->op_private = 0 + (flags >> 8);
    if (opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (opargs[type] & OA_TARGET)
	o->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, o);
}

OP *
newUNOP(I32 type, I32 flags, OP *first)
{
    UNOP *unop;

    if (!first)
	first = newOP(OP_STUB, 0);
    if (opargs[type] & OA_MARK)
	first = force_list(first);

    Newz(1101, unop, 1, UNOP);
    unop->op_type = type;
    unop->op_ppaddr = ppaddr[type];
    unop->op_first = first;
    unop->op_flags = flags | OPf_KIDS;
    unop->op_private = 1 | (flags >> 8);
    unop = (UNOP*) CHECKOP(type, unop);
    if (unop->op_next)
	return (OP*)unop;

    return fold_constants((OP *) unop);
}

OP *
newBINOP(I32 type, I32 flags, OP *first, OP *last)
{
    BINOP *binop;
    Newz(1101, binop, 1, BINOP);

    if (!first)
	first = newOP(OP_NULL, 0);

    binop->op_type = type;
    binop->op_ppaddr = ppaddr[type];
    binop->op_first = first;
    binop->op_flags = flags | OPf_KIDS;
    if (!last) {
	last = first;
	binop->op_private = 1 | (flags >> 8);
    }
    else {
	binop->op_private = 2 | (flags >> 8);
	first->op_sibling = last;
    }

    binop = (BINOP*)CHECKOP(type, binop);
    if (binop->op_next)
	return (OP*)binop;

    binop->op_last = last = binop->op_first->op_sibling;

    return fold_constants((OP *)binop);
}

OP *
pmtrans(OP *o, OP *expr, OP *repl)
{
    SV *tstr = ((SVOP*)expr)->op_sv;
    SV *rstr = ((SVOP*)repl)->op_sv;
    STRLEN tlen;
    STRLEN rlen;
    register U8 *t = (U8*)SvPV(tstr, tlen);
    register U8 *r = (U8*)SvPV(rstr, rlen);
    register I32 i;
    register I32 j;
    I32 Delete;
    I32 complement;
    I32 squash;
    register short *tbl;

    tbl = (short*)cPVOPo->op_pv;
    complement	= o->op_private & OPpTRANS_COMPLEMENT;
    Delete	= o->op_private & OPpTRANS_DELETE;
    squash	= o->op_private & OPpTRANS_SQUASH;

    if (complement) {
	Zero(tbl, 256, short);
	for (i = 0; i < tlen; i++)
	    tbl[t[i]] = -1;
	for (i = 0, j = 0; i < 256; i++) {
	    if (!tbl[i]) {
		if (j >= rlen) {
		    if (Delete)
			tbl[i] = -2;
		    else if (rlen)
			tbl[i] = r[j-1];
		    else
			tbl[i] = i;
		}
		else
		    tbl[i] = r[j++];
	    }
	}
    }
    else {
	if (!rlen && !Delete) {
	    r = t; rlen = tlen;
	    if (!squash)
		o->op_private |= OPpTRANS_COUNTONLY;
	}
	for (i = 0; i < 256; i++)
	    tbl[i] = -1;
	for (i = 0, j = 0; i < tlen; i++,j++) {
	    if (j >= rlen) {
		if (Delete) {
		    if (tbl[t[i]] == -1)
			tbl[t[i]] = -2;
		    continue;
		}
		--j;
	    }
	    if (tbl[t[i]] == -1)
		tbl[t[i]] = r[j];
	}
    }
    op_free(expr);
    op_free(repl);

    return o;
}

OP *
newPMOP(I32 type, I32 flags)
{
    dTHR;
    PMOP *pmop;

    Newz(1101, pmop, 1, PMOP);
    pmop->op_type = type;
    pmop->op_ppaddr = ppaddr[type];
    pmop->op_flags = flags;
    pmop->op_private = 0 | (flags >> 8);

    if (PL_hints & HINT_RE_TAINT)
	pmop->op_pmpermflags |= PMf_RETAINT;
    if (PL_hints & HINT_LOCALE)
	pmop->op_pmpermflags |= PMf_LOCALE;
    pmop->op_pmflags = pmop->op_pmpermflags;

    /* link into pm list */
    if (type != OP_TRANS && PL_curstash) {
	pmop->op_pmnext = HvPMROOT(PL_curstash);
	HvPMROOT(PL_curstash) = pmop;
    }

    return (OP*)pmop;
}

OP *
pmruntime(OP *o, OP *expr, OP *repl)
{
    dTHR;
    PMOP *pm;
    LOGOP *rcop;
    I32 repl_has_vars = 0;

    if (o->op_type == OP_TRANS)
	return pmtrans(o, expr, repl);

    PL_hints |= HINT_BLOCK_SCOPE;
    pm = (PMOP*)o;

    if (expr->op_type == OP_CONST) {
	STRLEN plen;
	SV *pat = ((SVOP*)expr)->op_sv;
	char *p = SvPV(pat, plen);
	if ((o->op_flags & OPf_SPECIAL) && strEQ(p, " ")) {
	    sv_setpvn(pat, "\\s+", 3);
	    p = SvPV(pat, plen);
	    pm->op_pmflags |= PMf_SKIPWHITE;
	}
	pm->op_pmregexp = CALLREGCOMP(p, p + plen, pm);
	if (strEQ("\\s+", pm->op_pmregexp->precomp))
	    pm->op_pmflags |= PMf_WHITE;
	op_free(expr);
    }
    else {
	if (pm->op_pmflags & PMf_KEEP || !(PL_hints & HINT_RE_EVAL))
	    expr = newUNOP((!(PL_hints & HINT_RE_EVAL) 
			    ? OP_REGCRESET
			    : OP_REGCMAYBE),0,expr);

	Newz(1101, rcop, 1, LOGOP);
	rcop->op_type = OP_REGCOMP;
	rcop->op_ppaddr = ppaddr[OP_REGCOMP];
	rcop->op_first = scalar(expr);
	rcop->op_flags |= ((PL_hints & HINT_RE_EVAL) 
			   ? (OPf_SPECIAL | OPf_KIDS)
			   : OPf_KIDS);
	rcop->op_private = 1;
	rcop->op_other = o;

	/* establish postfix order */
	if (pm->op_pmflags & PMf_KEEP || !(PL_hints & HINT_RE_EVAL)) {
	    LINKLIST(expr);
	    rcop->op_next = expr;
	    ((UNOP*)expr)->op_first->op_next = (OP*)rcop;
	}
	else {
	    rcop->op_next = LINKLIST(expr);
	    expr->op_next = (OP*)rcop;
	}

	prepend_elem(o->op_type, scalar((OP*)rcop), o);
    }

    if (repl) {
	OP *curop;
	if (pm->op_pmflags & PMf_EVAL) {
	    curop = 0;
	    if (PL_curcop->cop_line < PL_multi_end)
		PL_curcop->cop_line = PL_multi_end;
	}
#ifdef USE_THREADS
	else if (repl->op_type == OP_THREADSV
		 && strchr("&`'123456789+",
			   PL_threadsv_names[repl->op_targ]))
	{
	    curop = 0;
	}
#endif /* USE_THREADS */
	else if (repl->op_type == OP_CONST)
	    curop = repl;
	else {
	    OP *lastop = 0;
	    for (curop = LINKLIST(repl); curop!=repl; curop = LINKLIST(curop)) {
		if (opargs[curop->op_type] & OA_DANGEROUS) {
#ifdef USE_THREADS
		    if (curop->op_type == OP_THREADSV) {
			repl_has_vars = 1;
			if (strchr("&`'123456789+", curop->op_private))
			    break;
		    }
#else
		    if (curop->op_type == OP_GV) {
			GV *gv = ((GVOP*)curop)->op_gv;
			repl_has_vars = 1;
			if (strchr("&`'123456789+", *GvENAME(gv)))
			    break;
		    }
#endif /* USE_THREADS */
		    else if (curop->op_type == OP_RV2CV)
			break;
		    else if (curop->op_type == OP_RV2SV ||
			     curop->op_type == OP_RV2AV ||
			     curop->op_type == OP_RV2HV ||
			     curop->op_type == OP_RV2GV) {
			if (lastop && lastop->op_type != OP_GV)	/*funny deref?*/
			    break;
		    }
		    else if (curop->op_type == OP_PADSV ||
			     curop->op_type == OP_PADAV ||
			     curop->op_type == OP_PADHV ||
			     curop->op_type == OP_PADANY) {
			repl_has_vars = 1;
		    }
		    else if (curop->op_type == OP_PUSHRE)
			; /* Okay here, dangerous in newASSIGNOP */
		    else
			break;
		}
		lastop = curop;
	    }
	}
	if (curop == repl
	    && !(repl_has_vars 
		 && (!pm->op_pmregexp 
		     || pm->op_pmregexp->reganch & ROPT_EVAL_SEEN))) {
	    pm->op_pmflags |= PMf_CONST;	/* const for long enough */
	    pm->op_pmpermflags |= PMf_CONST;	/* const for long enough */
	    prepend_elem(o->op_type, scalar(repl), o);
	}
	else {
	    if (curop == repl && !pm->op_pmregexp) { /* Has variables. */
		pm->op_pmflags |= PMf_MAYBE_CONST;
		pm->op_pmpermflags |= PMf_MAYBE_CONST;
	    }
	    Newz(1101, rcop, 1, LOGOP);
	    rcop->op_type = OP_SUBSTCONT;
	    rcop->op_ppaddr = ppaddr[OP_SUBSTCONT];
	    rcop->op_first = scalar(repl);
	    rcop->op_flags |= OPf_KIDS;
	    rcop->op_private = 1;
	    rcop->op_other = o;

	    /* establish postfix order */
	    rcop->op_next = LINKLIST(repl);
	    repl->op_next = (OP*)rcop;

	    pm->op_pmreplroot = scalar((OP*)rcop);
	    pm->op_pmreplstart = LINKLIST(rcop);
	    rcop->op_next = 0;
	}
    }

    return (OP*)pm;
}

OP *
newSVOP(I32 type, I32 flags, SV *sv)
{
    SVOP *svop;
    Newz(1101, svop, 1, SVOP);
    svop->op_type = type;
    svop->op_ppaddr = ppaddr[type];
    svop->op_sv = sv;
    svop->op_next = (OP*)svop;
    svop->op_flags = flags;
    if (opargs[type] & OA_RETSCALAR)
	scalar((OP*)svop);
    if (opargs[type] & OA_TARGET)
	svop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, svop);
}

OP *
newGVOP(I32 type, I32 flags, GV *gv)
{
    dTHR;
    GVOP *gvop;
    Newz(1101, gvop, 1, GVOP);
    gvop->op_type = type;
    gvop->op_ppaddr = ppaddr[type];
    gvop->op_gv = (GV*)SvREFCNT_inc(gv);
    gvop->op_next = (OP*)gvop;
    gvop->op_flags = flags;
    if (opargs[type] & OA_RETSCALAR)
	scalar((OP*)gvop);
    if (opargs[type] & OA_TARGET)
	gvop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, gvop);
}

OP *
newPVOP(I32 type, I32 flags, char *pv)
{
    PVOP *pvop;
    Newz(1101, pvop, 1, PVOP);
    pvop->op_type = type;
    pvop->op_ppaddr = ppaddr[type];
    pvop->op_pv = pv;
    pvop->op_next = (OP*)pvop;
    pvop->op_flags = flags;
    if (opargs[type] & OA_RETSCALAR)
	scalar((OP*)pvop);
    if (opargs[type] & OA_TARGET)
	pvop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, pvop);
}

void
package(OP *o)
{
    dTHR;
    SV *sv;

    save_hptr(&PL_curstash);
    save_item(PL_curstname);
    if (o) {
	STRLEN len;
	char *name;
	sv = cSVOPo->op_sv;
	name = SvPV(sv, len);
	PL_curstash = gv_stashpvn(name,len,TRUE);
	sv_setpvn(PL_curstname, name, len);
	op_free(o);
    }
    else {
	sv_setpv(PL_curstname,"<none>");
	PL_curstash = Nullhv;
    }
    PL_hints |= HINT_BLOCK_SCOPE;
    PL_copline = NOLINE;
    PL_expect = XSTATE;
}

void
utilize(int aver, I32 floor, OP *version, OP *id, OP *arg)
{
    OP *pack;
    OP *meth;
    OP *rqop;
    OP *imop;
    OP *veop;
    GV *gv;

    if (id->op_type != OP_CONST)
	croak("Module name must be constant");

    veop = Nullop;

    if(version != Nullop) {
	SV *vesv = ((SVOP*)version)->op_sv;

	if (arg == Nullop && !SvNIOK(vesv)) {
	    arg = version;
	}
	else {
	    OP *pack;
	    OP *meth;

	    if (version->op_type != OP_CONST || !SvNIOK(vesv))
		croak("Version number must be constant number");

	    /* Make copy of id so we don't free it twice */
	    pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)id)->op_sv));

	    /* Fake up a method call to VERSION */
	    meth = newSVOP(OP_CONST, 0, newSVpv("VERSION", 7));
	    veop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
			    append_elem(OP_LIST,
			    prepend_elem(OP_LIST, pack, list(version)),
			    newUNOP(OP_METHOD, 0, meth)));
	}
    }

    /* Fake up an import/unimport */
    if (arg && arg->op_type == OP_STUB)
	imop = arg;		/* no import on explicit () */
    else if(SvNIOK(((SVOP*)id)->op_sv)) {
	imop = Nullop;		/* use 5.0; */
    }
    else {
	/* Make copy of id so we don't free it twice */
	pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)id)->op_sv));
	meth = newSVOP(OP_CONST, 0,
	    aver
		? newSVpv("import", 6)
		: newSVpv("unimport", 8)
	    );
	imop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
		    append_elem(OP_LIST,
			prepend_elem(OP_LIST, pack, list(arg)),
			newUNOP(OP_METHOD, 0, meth)));
    }

    /* Fake up a require, handle override, if any */
    gv = gv_fetchpv("require", FALSE, SVt_PVCV);
    if (!(gv && GvIMPORTED_CV(gv)))
	gv = gv_fetchpv("CORE::GLOBAL::require", FALSE, SVt_PVCV);

    if (gv && GvIMPORTED_CV(gv)) {
	rqop = ck_subr(newUNOP(OP_ENTERSUB, OPf_STACKED,
			       append_elem(OP_LIST, id,
					   scalar(newUNOP(OP_RV2CV, 0,
							  newGVOP(OP_GV, 0,
								  gv))))));
    }
    else {
	rqop = newUNOP(OP_REQUIRE, 0, id);
    }

    /* Fake up the BEGIN {}, which does its thing immediately. */
    newSUB(floor,
	newSVOP(OP_CONST, 0, newSVpv("BEGIN", 5)),
	Nullop,
	append_elem(OP_LINESEQ,
	    append_elem(OP_LINESEQ,
	        newSTATEOP(0, Nullch, rqop),
	        newSTATEOP(0, Nullch, veop)),
	    newSTATEOP(0, Nullch, imop) ));

    PL_copline = NOLINE;
    PL_expect = XSTATE;
}

OP *
dofile(OP *term)
{
    OP *doop;
    GV *gv;

    gv = gv_fetchpv("do", FALSE, SVt_PVCV);
    if (!(gv && GvIMPORTED_CV(gv)))
	gv = gv_fetchpv("CORE::GLOBAL::do", FALSE, SVt_PVCV);

    if (gv && GvIMPORTED_CV(gv)) {
	doop = ck_subr(newUNOP(OP_ENTERSUB, OPf_STACKED,
			       append_elem(OP_LIST, term,
					   scalar(newUNOP(OP_RV2CV, 0,
							  newGVOP(OP_GV, 0,
								  gv))))));
    }
    else {
	doop = newUNOP(OP_DOFILE, 0, scalar(term));
    }
    return doop;
}

OP *
newSLICEOP(I32 flags, OP *subscript, OP *listval)
{
    return newBINOP(OP_LSLICE, flags,
	    list(force_list(subscript)),
	    list(force_list(listval)) );
}

STATIC I32
list_assignment(register OP *o)
{
    if (!o)
	return TRUE;

    if (o->op_type == OP_NULL && o->op_flags & OPf_KIDS)
	o = cUNOPo->op_first;

    if (o->op_type == OP_COND_EXPR) {
	I32 t = list_assignment(cCONDOPo->op_first->op_sibling);
	I32 f = list_assignment(cCONDOPo->op_first->op_sibling->op_sibling);

	if (t && f)
	    return TRUE;
	if (t || f)
	    yyerror("Assignment to both a list and a scalar");
	return FALSE;
    }

    if (o->op_type == OP_LIST || o->op_flags & OPf_PARENS ||
	o->op_type == OP_RV2AV || o->op_type == OP_RV2HV ||
	o->op_type == OP_ASLICE || o->op_type == OP_HSLICE)
	return TRUE;

    if (o->op_type == OP_PADAV || o->op_type == OP_PADHV)
	return TRUE;

    if (o->op_type == OP_RV2SV)
	return FALSE;

    return FALSE;
}

OP *
newASSIGNOP(I32 flags, OP *left, I32 optype, OP *right)
{
    OP *o;

    if (optype) {
	if (optype == OP_ANDASSIGN || optype == OP_ORASSIGN) {
	    return newLOGOP(optype, 0,
		mod(scalar(left), optype),
		newUNOP(OP_SASSIGN, 0, scalar(right)));
	}
	else {
	    return newBINOP(optype, OPf_STACKED,
		mod(scalar(left), optype), scalar(right));
	}
    }

    if (list_assignment(left)) {
	dTHR;
	PL_modcount = 0;
	PL_eval_start = right;	/* Grandfathering $[ assignment here.  Bletch.*/
	left = mod(left, OP_AASSIGN);
	if (PL_eval_start)
	    PL_eval_start = 0;
	else {
	    op_free(left);
	    op_free(right);
	    return Nullop;
	}
	o = newBINOP(OP_AASSIGN, flags,
		list(force_list(right)),
		list(force_list(left)) );
	o->op_private = 0 | (flags >> 8);
	if (!(left->op_private & OPpLVAL_INTRO)) {
	    OP *curop;
	    OP *lastop = o;
	    PL_generation++;
	    for (curop = LINKLIST(o); curop != o; curop = LINKLIST(curop)) {
		if (opargs[curop->op_type] & OA_DANGEROUS) {
		    if (curop->op_type == OP_GV) {
			GV *gv = ((GVOP*)curop)->op_gv;
			if (gv == PL_defgv || SvCUR(gv) == PL_generation)
			    break;
			SvCUR(gv) = PL_generation;
		    }
		    else if (curop->op_type == OP_PADSV ||
			     curop->op_type == OP_PADAV ||
			     curop->op_type == OP_PADHV ||
			     curop->op_type == OP_PADANY) {
			SV **svp = AvARRAY(PL_comppad_name);
			SV *sv = svp[curop->op_targ];
			if (SvCUR(sv) == PL_generation)
			    break;
			SvCUR(sv) = PL_generation;	/* (SvCUR not used any more) */
		    }
		    else if (curop->op_type == OP_RV2CV)
			break;
		    else if (curop->op_type == OP_RV2SV ||
			     curop->op_type == OP_RV2AV ||
			     curop->op_type == OP_RV2HV ||
			     curop->op_type == OP_RV2GV) {
			if (lastop->op_type != OP_GV)	/* funny deref? */
			    break;
		    }
		    else if (curop->op_type == OP_PUSHRE) {
			if (((PMOP*)curop)->op_pmreplroot) {
			    GV *gv = (GV*)((PMOP*)curop)->op_pmreplroot;
			    if (gv == PL_defgv || SvCUR(gv) == PL_generation)
				break;
			    SvCUR(gv) = PL_generation;
			}	
		    }
		    else
			break;
		}
		lastop = curop;
	    }
	    if (curop != o)
		o->op_private = OPpASSIGN_COMMON;
	}
	if (right && right->op_type == OP_SPLIT) {
	    OP* tmpop;
	    if ((tmpop = ((LISTOP*)right)->op_first) &&
		tmpop->op_type == OP_PUSHRE)
	    {
		PMOP *pm = (PMOP*)tmpop;
		if (left->op_type == OP_RV2AV &&
		    !(left->op_private & OPpLVAL_INTRO) &&
		    !(o->op_private & OPpASSIGN_COMMON) )
		{
		    tmpop = ((UNOP*)left)->op_first;
		    if (tmpop->op_type == OP_GV && !pm->op_pmreplroot) {
			pm->op_pmreplroot = (OP*)((GVOP*)tmpop)->op_gv;
			pm->op_pmflags |= PMf_ONCE;
			tmpop = cUNOPo->op_first;	/* to list (nulled) */
			tmpop = ((UNOP*)tmpop)->op_first; /* to pushmark */
			tmpop->op_sibling = Nullop;	/* don't free split */
			right->op_next = tmpop->op_next;  /* fix starting loc */
			op_free(o);			/* blow off assign */
			right->op_flags &= ~OPf_WANT;
				/* "I don't know and I don't care." */
			return right;
		    }
		}
		else {
		    if (PL_modcount < 10000 &&
		      ((LISTOP*)right)->op_last->op_type == OP_CONST)
		    {
			SV *sv = ((SVOP*)((LISTOP*)right)->op_last)->op_sv;
			if (SvIVX(sv) == 0)
			    sv_setiv(sv, PL_modcount+1);
		    }
		}
	    }
	}
	return o;
    }
    if (!right)
	right = newOP(OP_UNDEF, 0);
    if (right->op_type == OP_READLINE) {
	right->op_flags |= OPf_STACKED;
	return newBINOP(OP_NULL, flags, mod(scalar(left), OP_SASSIGN), scalar(right));
    }
    else {
	PL_eval_start = right;	/* Grandfathering $[ assignment here.  Bletch.*/
	o = newBINOP(OP_SASSIGN, flags,
	    scalar(right), mod(scalar(left), OP_SASSIGN) );
	if (PL_eval_start)
	    PL_eval_start = 0;
	else {
	    op_free(o);
	    return Nullop;
	}
    }
    return o;
}

OP *
newSTATEOP(I32 flags, char *label, OP *o)
{
    dTHR;
    U32 seq = intro_my();
    register COP *cop;

    Newz(1101, cop, 1, COP);
    if (PERLDB_LINE && PL_curcop->cop_line && PL_curstash != PL_debstash) {
	cop->op_type = OP_DBSTATE;
	cop->op_ppaddr = ppaddr[ OP_DBSTATE ];
    }
    else {
	cop->op_type = OP_NEXTSTATE;
	cop->op_ppaddr = ppaddr[ OP_NEXTSTATE ];
    }
    cop->op_flags = flags;
    cop->op_private = 0 | (flags >> 8);
#ifdef NATIVE_HINTS
    cop->op_private |= NATIVE_HINTS;
#endif
    cop->op_next = (OP*)cop;

    if (label) {
	cop->cop_label = label;
	PL_hints |= HINT_BLOCK_SCOPE;
    }
    cop->cop_seq = seq;
    cop->cop_arybase = PL_curcop->cop_arybase;

    if (PL_copline == NOLINE)
        cop->cop_line = PL_curcop->cop_line;
    else {
        cop->cop_line = PL_copline;
        PL_copline = NOLINE;
    }
    cop->cop_filegv = (GV*)SvREFCNT_inc(PL_curcop->cop_filegv);
    cop->cop_stash = PL_curstash;

    if (PERLDB_LINE && PL_curstash != PL_debstash) {
	SV **svp = av_fetch(GvAV(PL_curcop->cop_filegv),(I32)cop->cop_line, FALSE);
	if (svp && *svp != &PL_sv_undef && !SvIOK(*svp)) {
	    (void)SvIOK_on(*svp);
	    SvIVX(*svp) = 1;
	    SvSTASH(*svp) = (HV*)cop;
	}
    }

    return prepend_elem(OP_LINESEQ, (OP*)cop, o);
}

/* "Introduce" my variables to visible status. */
U32
intro_my(void)
{
    SV **svp;
    SV *sv;
    I32 i;

    if (! PL_min_intro_pending)
	return PL_cop_seqmax;

    svp = AvARRAY(PL_comppad_name);
    for (i = PL_min_intro_pending; i <= PL_max_intro_pending; i++) {
	if ((sv = svp[i]) && sv != &PL_sv_undef && !SvIVX(sv)) {
	    SvIVX(sv) = PAD_MAX;	/* Don't know scope end yet. */
	    SvNVX(sv) = (double)PL_cop_seqmax;
	}
    }
    PL_min_intro_pending = 0;
    PL_comppad_name_fill = PL_max_intro_pending;	/* Needn't search higher */
    return PL_cop_seqmax++;
}

OP *
newLOGOP(I32 type, I32 flags, OP *first, OP *other)
{
    return new_logop(type, flags, &first, &other);
}

STATIC OP *
new_logop(I32 type, I32 flags, OP** firstp, OP** otherp)
{
    dTHR;
    LOGOP *logop;
    OP *o;
    OP *first = *firstp;
    OP *other = *otherp;

    if (type == OP_XOR)		/* Not short circuit, but here by precedence. */
	return newBINOP(type, flags, scalar(first), scalar(other));

    scalarboolean(first);
    /* optimize "!a && b" to "a || b", and "!a || b" to "a && b" */
    if (first->op_type == OP_NOT && (first->op_flags & OPf_SPECIAL)) {
	if (type == OP_AND || type == OP_OR) {
	    if (type == OP_AND)
		type = OP_OR;
	    else
		type = OP_AND;
	    o = first;
	    first = *firstp = cUNOPo->op_first;
	    if (o->op_next)
		first->op_next = o->op_next;
	    cUNOPo->op_first = Nullop;
	    op_free(o);
	}
    }
    if (first->op_type == OP_CONST) {
	if (PL_dowarn && (first->op_private & OPpCONST_BARE))
	    warn("Probable precedence problem on %s", op_desc[type]);
	if ((type == OP_AND) == (SvTRUE(((SVOP*)first)->op_sv))) {
	    op_free(first);
	    *firstp = Nullop;
	    return other;
	}
	else {
	    op_free(other);
	    *otherp = Nullop;
	    return first;
	}
    }
    else if (first->op_type == OP_WANTARRAY) {
	if (type == OP_AND)
	    list(other);
	else
	    scalar(other);
    }
    else if (PL_dowarn && (first->op_flags & OPf_KIDS)) {
	OP *k1 = ((UNOP*)first)->op_first;
	OP *k2 = k1->op_sibling;
	OPCODE warnop = 0;
	switch (first->op_type)
	{
	case OP_NULL:
	    if (k2 && k2->op_type == OP_READLINE
		  && (k2->op_flags & OPf_STACKED)
		  && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR)) 
		warnop = k2->op_type;
	    break;

	case OP_SASSIGN:
	    if (k1->op_type == OP_READDIR
		  || k1->op_type == OP_GLOB
		  || k1->op_type == OP_EACH)
		warnop = k1->op_type;
	    break;
	}
	if (warnop) {
	    line_t oldline = PL_curcop->cop_line;
	    PL_curcop->cop_line = PL_copline;
	    warn("Value of %s%s can be \"0\"; test with defined()",
		 op_desc[warnop],
		 ((warnop == OP_READLINE || warnop == OP_GLOB)
		  ? " construct" : "() operator"));
	    PL_curcop->cop_line = oldline;
	}
    }

    if (!other)
	return first;

    if (type == OP_ANDASSIGN || type == OP_ORASSIGN)
	other->op_private |= OPpASSIGN_BACKWARDS;  /* other is an OP_SASSIGN */

    Newz(1101, logop, 1, LOGOP);

    logop->op_type = type;
    logop->op_ppaddr = ppaddr[type];
    logop->op_first = first;
    logop->op_flags = flags | OPf_KIDS;
    logop->op_other = LINKLIST(other);
    logop->op_private = 1 | (flags >> 8);

    /* establish postfix order */
    logop->op_next = LINKLIST(first);
    first->op_next = (OP*)logop;
    first->op_sibling = other;

    o = newUNOP(OP_NULL, 0, (OP*)logop);
    other->op_next = o;

    return o;
}

OP *
newCONDOP(I32 flags, OP *first, OP *trueop, OP *falseop)
{
    dTHR;
    CONDOP *condop;
    OP *o;

    if (!falseop)
	return newLOGOP(OP_AND, 0, first, trueop);
    if (!trueop)
	return newLOGOP(OP_OR, 0, first, falseop);

    scalarboolean(first);
    if (first->op_type == OP_CONST) {
	if (SvTRUE(((SVOP*)first)->op_sv)) {
	    op_free(first);
	    op_free(falseop);
	    return trueop;
	}
	else {
	    op_free(first);
	    op_free(trueop);
	    return falseop;
	}
    }
    else if (first->op_type == OP_WANTARRAY) {
	list(trueop);
	scalar(falseop);
    }
    Newz(1101, condop, 1, CONDOP);

    condop->op_type = OP_COND_EXPR;
    condop->op_ppaddr = ppaddr[OP_COND_EXPR];
    condop->op_first = first;
    condop->op_flags = flags | OPf_KIDS;
    condop->op_true = LINKLIST(trueop);
    condop->op_false = LINKLIST(falseop);
    condop->op_private = 1 | (flags >> 8);

    /* establish postfix order */
    condop->op_next = LINKLIST(first);
    first->op_next = (OP*)condop;

    first->op_sibling = trueop;
    trueop->op_sibling = falseop;
    o = newUNOP(OP_NULL, 0, (OP*)condop);

    trueop->op_next = o;
    falseop->op_next = o;

    return o;
}

OP *
newRANGE(I32 flags, OP *left, OP *right)
{
    dTHR;
    CONDOP *condop;
    OP *flip;
    OP *flop;
    OP *o;

    Newz(1101, condop, 1, CONDOP);

    condop->op_type = OP_RANGE;
    condop->op_ppaddr = ppaddr[OP_RANGE];
    condop->op_first = left;
    condop->op_flags = OPf_KIDS;
    condop->op_true = LINKLIST(left);
    condop->op_false = LINKLIST(right);
    condop->op_private = 1 | (flags >> 8);

    left->op_sibling = right;

    condop->op_next = (OP*)condop;
    flip = newUNOP(OP_FLIP, flags, (OP*)condop);
    flop = newUNOP(OP_FLOP, 0, flip);
    o = newUNOP(OP_NULL, 0, flop);
    linklist(flop);

    left->op_next = flip;
    right->op_next = flop;

    condop->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(condop->op_targ), SVt_PVNV);
    flip->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(flip->op_targ), SVt_PVNV);

    flip->op_private =  left->op_type == OP_CONST ? OPpFLIP_LINENUM : 0;
    flop->op_private = right->op_type == OP_CONST ? OPpFLIP_LINENUM : 0;

    flip->op_next = o;
    if (!flip->op_private || !flop->op_private)
	linklist(o);		/* blow off optimizer unless constant */

    return o;
}

OP *
newLOOPOP(I32 flags, I32 debuggable, OP *expr, OP *block)
{
    dTHR;
    OP* listop;
    OP* o;
    int once = block && block->op_flags & OPf_SPECIAL &&
      (block->op_type == OP_ENTERSUB || block->op_type == OP_NULL);

    if (expr) {
	if (once && expr->op_type == OP_CONST && !SvTRUE(((SVOP*)expr)->op_sv))
	    return block;	/* do {} while 0 does once */
	if (expr->op_type == OP_READLINE || expr->op_type == OP_GLOB
	    || (expr->op_type == OP_NULL && expr->op_targ == OP_GLOB)) {
	    expr = newUNOP(OP_DEFINED, 0,
		newASSIGNOP(0, newDEFSVOP(), 0, expr) );
	} else if (expr->op_flags & OPf_KIDS) {
	    OP *k1 = ((UNOP*)expr)->op_first;
	    OP *k2 = (k1) ? k1->op_sibling : NULL;
	    switch (expr->op_type) {
	      case OP_NULL: 
		if (k2 && k2->op_type == OP_READLINE
		      && (k2->op_flags & OPf_STACKED)
		      && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR)) 
		    expr = newUNOP(OP_DEFINED, 0, expr);
		break;                                

	      case OP_SASSIGN:
		if (k1->op_type == OP_READDIR
		      || k1->op_type == OP_GLOB
		      || k1->op_type == OP_EACH)
		    expr = newUNOP(OP_DEFINED, 0, expr);
		break;
	    }
	}
    }

    listop = append_elem(OP_LINESEQ, block, newOP(OP_UNSTACK, 0));
    o = new_logop(OP_AND, 0, &expr, &listop);

    if (listop)
	((LISTOP*)listop)->op_last->op_next = LINKLIST(o);

    if (once && o != listop)
	o->op_next = ((LOGOP*)cUNOPo->op_first)->op_other;

    if (o == listop)
	o = newUNOP(OP_NULL, 0, o);	/* or do {} while 1 loses outer block */

    o->op_flags |= flags;
    o = scope(o);
    o->op_flags |= OPf_SPECIAL;	/* suppress POPBLOCK curpm restoration*/
    return o;
}

OP *
newWHILEOP(I32 flags, I32 debuggable, LOOP *loop, I32 whileline, OP *expr, OP *block, OP *cont)
{
    dTHR;
    OP *redo;
    OP *next = 0;
    OP *listop;
    OP *o;
    OP *condop;

    if (expr && (expr->op_type == OP_READLINE || expr->op_type == OP_GLOB
		 || (expr->op_type == OP_NULL && expr->op_targ == OP_GLOB))) {
	expr = newUNOP(OP_DEFINED, 0,
	    newASSIGNOP(0, newDEFSVOP(), 0, expr) );
    } else if (expr && (expr->op_flags & OPf_KIDS)) {
	OP *k1 = ((UNOP*)expr)->op_first;
	OP *k2 = (k1) ? k1->op_sibling : NULL;
	switch (expr->op_type) {
	  case OP_NULL: 
	    if (k2 && k2->op_type == OP_READLINE
		  && (k2->op_flags & OPf_STACKED)
		  && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR)) 
		expr = newUNOP(OP_DEFINED, 0, expr);
	    break;                                

	  case OP_SASSIGN:
	    if (k1->op_type == OP_READDIR
		  || k1->op_type == OP_GLOB
		  || k1->op_type == OP_EACH)
		expr = newUNOP(OP_DEFINED, 0, expr);
	    break;
	}
    }

    if (!block)
	block = newOP(OP_NULL, 0);

    if (cont)
	next = LINKLIST(cont);
    if (expr) {
	cont = append_elem(OP_LINESEQ, cont, newOP(OP_UNSTACK, 0));
	if ((line_t)whileline != NOLINE) {
	    PL_copline = whileline;
	    cont = append_elem(OP_LINESEQ, cont,
			       newSTATEOP(0, Nullch, Nullop));
	}
    }

    listop = append_list(OP_LINESEQ, (LISTOP*)block, (LISTOP*)cont);
    redo = LINKLIST(listop);

    if (expr) {
	PL_copline = whileline;
	scalar(listop);
	o = new_logop(OP_AND, 0, &expr, &listop);
	if (o == expr && o->op_type == OP_CONST && !SvTRUE(cSVOPo->op_sv)) {
	    op_free(expr);		/* oops, it's a while (0) */
	    op_free((OP*)loop);
	    return Nullop;		/* listop already freed by new_logop */
	}
	if (listop)
	    ((LISTOP*)listop)->op_last->op_next = condop =
		(o == listop ? redo : LINKLIST(o));
	if (!next)
	    next = condop;
    }
    else
	o = listop;

    if (!loop) {
	Newz(1101,loop,1,LOOP);
	loop->op_type = OP_ENTERLOOP;
	loop->op_ppaddr = ppaddr[OP_ENTERLOOP];
	loop->op_private = 0;
	loop->op_next = (OP*)loop;
    }

    o = newBINOP(OP_LEAVELOOP, 0, (OP*)loop, o);

    loop->op_redoop = redo;
    loop->op_lastop = o;

    if (next)
	loop->op_nextop = next;
    else
	loop->op_nextop = o;

    o->op_flags |= flags;
    o->op_private |= (flags >> 8);
    return o;
}

OP *
newFOROP(I32 flags,char *label,line_t forline,OP *sv,OP *expr,OP *block,OP *cont)
{
    LOOP *loop;
    OP *wop;
    int padoff = 0;
    I32 iterflags = 0;

    if (sv) {
	if (sv->op_type == OP_RV2SV) {	/* symbol table variable */
	    sv->op_type = OP_RV2GV;
	    sv->op_ppaddr = ppaddr[OP_RV2GV];
	}
	else if (sv->op_type == OP_PADSV) { /* private variable */
	    padoff = sv->op_targ;
	    op_free(sv);
	    sv = Nullop;
	}
	else if (sv->op_type == OP_THREADSV) { /* per-thread variable */
	    padoff = sv->op_targ;
	    iterflags |= OPf_SPECIAL;
	    op_free(sv);
	    sv = Nullop;
	}
	else
	    croak("Can't use %s for loop variable", op_desc[sv->op_type]);
    }
    else {
#ifdef USE_THREADS
	padoff = find_threadsv("_");
	iterflags |= OPf_SPECIAL;
#else
	sv = newGVOP(OP_GV, 0, PL_defgv);
#endif
    }
    if (expr->op_type == OP_RV2AV || expr->op_type == OP_PADAV) {
	expr = mod(force_list(scalar(ref(expr, OP_ITER))), OP_GREPSTART);
	iterflags |= OPf_STACKED;
    }
    else if (expr->op_type == OP_NULL &&
             (expr->op_flags & OPf_KIDS) &&
             ((BINOP*)expr)->op_first->op_type == OP_FLOP)
    {
	/* Basically turn for($x..$y) into the same as for($x,$y), but we
	 * set the STACKED flag to indicate that these values are to be
	 * treated as min/max values by 'pp_iterinit'.
	 */
	UNOP* flip = (UNOP*)((UNOP*)((BINOP*)expr)->op_first)->op_first;
	CONDOP* range = (CONDOP*) flip->op_first;
	OP* left  = range->op_first;
	OP* right = left->op_sibling;
	LISTOP* listop;

	range->op_flags &= ~OPf_KIDS;
	range->op_first = Nullop;

	listop = (LISTOP*)newLISTOP(OP_LIST, 0, left, right);
	listop->op_first->op_next = range->op_true;
	left->op_next = range->op_false;
	right->op_next = (OP*)listop;
	listop->op_next = listop->op_first;

	op_free(expr);
	expr = (OP*)(listop);
        null(expr);
	iterflags |= OPf_STACKED;
    }
    else {
        expr = mod(force_list(expr), OP_GREPSTART);
    }


    loop = (LOOP*)list(convert(OP_ENTERITER, iterflags,
			       append_elem(OP_LIST, expr, scalar(sv))));
    assert(!loop->op_next);
    Renew(loop, 1, LOOP);
    loop->op_targ = padoff;
    wop = newWHILEOP(flags, 1, loop, forline, newOP(OP_ITER, 0), block, cont);
    PL_copline = forline;
    return newSTATEOP(0, label, wop);
}

OP*
newLOOPEX(I32 type, OP *label)
{
    dTHR;
    OP *o;
    STRLEN n_a;
    if (type != OP_GOTO || label->op_type == OP_CONST) {
	/* "last()" means "last" */
	if (label->op_type == OP_STUB && (label->op_flags & OPf_PARENS))
	    o = newOP(type, OPf_SPECIAL);
	else {
	    o = newPVOP(type, 0, savepv(label->op_type == OP_CONST
					? SvPVx(((SVOP*)label)->op_sv, n_a)
					: ""));
	}
	op_free(label);
    }
    else {
	if (label->op_type == OP_ENTERSUB)
	    label = newUNOP(OP_REFGEN, 0, mod(label, OP_REFGEN));
	o = newUNOP(type, OPf_STACKED, label);
    }
    PL_hints |= HINT_BLOCK_SCOPE;
    return o;
}

void
cv_undef(CV *cv)
{
    dTHR;
#ifdef USE_THREADS
    if (CvMUTEXP(cv)) {
	MUTEX_DESTROY(CvMUTEXP(cv));
	Safefree(CvMUTEXP(cv));
	CvMUTEXP(cv) = 0;
    }
#endif /* USE_THREADS */

    if (!CvXSUB(cv) && CvROOT(cv)) {
#ifdef USE_THREADS
	if (CvDEPTH(cv) || (CvOWNER(cv) && CvOWNER(cv) != thr))
	    croak("Can't undef active subroutine");
#else
	if (CvDEPTH(cv))
	    croak("Can't undef active subroutine");
#endif /* USE_THREADS */
	ENTER;

	SAVESPTR(PL_curpad);
	PL_curpad = 0;

	if (!CvCLONED(cv))
	    op_free(CvROOT(cv));
	CvROOT(cv) = Nullop;
	LEAVE;
    }
    SvPOK_off((SV*)cv);		/* forget prototype */
    CvFLAGS(cv) = 0;
    SvREFCNT_dec(CvGV(cv));
    CvGV(cv) = Nullgv;
    SvREFCNT_dec(CvOUTSIDE(cv));
    CvOUTSIDE(cv) = Nullcv;
    if (CvPADLIST(cv)) {
	/* may be during global destruction */
	if (SvREFCNT(CvPADLIST(cv))) {
	    I32 i = AvFILLp(CvPADLIST(cv));
	    while (i >= 0) {
		SV** svp = av_fetch(CvPADLIST(cv), i--, FALSE);
		SV* sv = svp ? *svp : Nullsv;
		if (!sv)
		    continue;
		if (sv == (SV*)PL_comppad_name)
		    PL_comppad_name = Nullav;
		else if (sv == (SV*)PL_comppad) {
		    PL_comppad = Nullav;
		    PL_curpad = Null(SV**);
		}
		SvREFCNT_dec(sv);
	    }
	    SvREFCNT_dec((SV*)CvPADLIST(cv));
	}
	CvPADLIST(cv) = Nullav;
    }
}

#ifdef DEBUG_CLOSURES
STATIC void
cv_dump(cv)
CV* cv;
{
    CV *outside = CvOUTSIDE(cv);
    AV* padlist = CvPADLIST(cv);
    AV* pad_name;
    AV* pad;
    SV** pname;
    SV** ppad;
    I32 ix;

    PerlIO_printf(Perl_debug_log, "\tCV=0x%lx (%s), OUTSIDE=0x%lx (%s)\n",
		  cv,
		  (CvANON(cv) ? "ANON"
		   : (cv == PL_main_cv) ? "MAIN"
		   : CvUNIQUE(cv) ? "UNIQUE"
		   : CvGV(cv) ? GvNAME(CvGV(cv)) : "UNDEFINED"),
		  outside,
		  (!outside ? "null"
		   : CvANON(outside) ? "ANON"
		   : (outside == PL_main_cv) ? "MAIN"
		   : CvUNIQUE(outside) ? "UNIQUE"
		   : CvGV(outside) ? GvNAME(CvGV(outside)) : "UNDEFINED"));

    if (!padlist)
	return;

    pad_name = (AV*)*av_fetch(padlist, 0, FALSE);
    pad = (AV*)*av_fetch(padlist, 1, FALSE);
    pname = AvARRAY(pad_name);
    ppad = AvARRAY(pad);

    for (ix = 1; ix <= AvFILLp(pad_name); ix++) {
	if (SvPOK(pname[ix]))
	    PerlIO_printf(Perl_debug_log, "\t%4d. 0x%lx (%s\"%s\" %ld-%ld)\n",
			  ix, ppad[ix],
			  SvFAKE(pname[ix]) ? "FAKE " : "",
			  SvPVX(pname[ix]),
			  (long)I_32(SvNVX(pname[ix])),
			  (long)SvIVX(pname[ix]));
    }
}
#endif /* DEBUG_CLOSURES */

STATIC CV *
cv_clone2(CV *proto, CV *outside)
{
    dTHR;
    AV* av;
    I32 ix;
    AV* protopadlist = CvPADLIST(proto);
    AV* protopad_name = (AV*)*av_fetch(protopadlist, 0, FALSE);
    AV* protopad = (AV*)*av_fetch(protopadlist, 1, FALSE);
    SV** pname = AvARRAY(protopad_name);
    SV** ppad = AvARRAY(protopad);
    I32 fname = AvFILLp(protopad_name);
    I32 fpad = AvFILLp(protopad);
    AV* comppadlist;
    CV* cv;

    assert(!CvUNIQUE(proto));

    ENTER;
    SAVESPTR(PL_curpad);
    SAVESPTR(PL_comppad);
    SAVESPTR(PL_comppad_name);
    SAVESPTR(PL_compcv);

    cv = PL_compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)cv, SvTYPE(proto));
    CvCLONED_on(cv);
    if (CvANON(proto))
	CvANON_on(cv);

#ifdef USE_THREADS
    New(666, CvMUTEXP(cv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(cv));
    CvOWNER(cv)		= 0;
#endif /* USE_THREADS */
    CvFILEGV(cv)	= CvFILEGV(proto);
    CvGV(cv)		= (GV*)SvREFCNT_inc(CvGV(proto));
    CvSTASH(cv)		= CvSTASH(proto);
    CvROOT(cv)		= CvROOT(proto);
    CvSTART(cv)		= CvSTART(proto);
    if (outside)
	CvOUTSIDE(cv)	= (CV*)SvREFCNT_inc(outside);

    if (SvPOK(proto))
	sv_setpvn((SV*)cv, SvPVX(proto), SvCUR(proto));

    PL_comppad_name = newAV();
    for (ix = fname; ix >= 0; ix--)
	av_store(PL_comppad_name, ix, SvREFCNT_inc(pname[ix]));

    PL_comppad = newAV();

    comppadlist = newAV();
    AvREAL_off(comppadlist);
    av_store(comppadlist, 0, (SV*)PL_comppad_name);
    av_store(comppadlist, 1, (SV*)PL_comppad);
    CvPADLIST(cv) = comppadlist;
    av_fill(PL_comppad, AvFILLp(protopad));
    PL_curpad = AvARRAY(PL_comppad);

    av = newAV();           /* will be @_ */
    av_extend(av, 0);
    av_store(PL_comppad, 0, (SV*)av);
    AvFLAGS(av) = AVf_REIFY;

    for (ix = fpad; ix > 0; ix--) {
	SV* namesv = (ix <= fname) ? pname[ix] : Nullsv;
	if (namesv && namesv != &PL_sv_undef) {
	    char *name = SvPVX(namesv);    /* XXX */
	    if (SvFLAGS(namesv) & SVf_FAKE) {   /* lexical from outside? */
		I32 off = pad_findlex(name, ix, SvIVX(namesv),
				      CvOUTSIDE(cv), cxstack_ix, 0, 0);
		if (!off)
		    PL_curpad[ix] = SvREFCNT_inc(ppad[ix]);
		else if (off != ix)
		    croak("panic: cv_clone: %s", name);
	    }
	    else {				/* our own lexical */
		SV* sv;
		if (*name == '&') {
		    /* anon code -- we'll come back for it */
		    sv = SvREFCNT_inc(ppad[ix]);
		}
		else if (*name == '@')
		    sv = (SV*)newAV();
		else if (*name == '%')
		    sv = (SV*)newHV();
		else
		    sv = NEWSV(0,0);
		if (!SvPADBUSY(sv))
		    SvPADMY_on(sv);
		PL_curpad[ix] = sv;
	    }
	}
	else {
	    SV* sv = NEWSV(0,0);
	    SvPADTMP_on(sv);
	    PL_curpad[ix] = sv;
	}
    }

    /* Now that vars are all in place, clone nested closures. */

    for (ix = fpad; ix > 0; ix--) {
	SV* namesv = (ix <= fname) ? pname[ix] : Nullsv;
	if (namesv
	    && namesv != &PL_sv_undef
	    && !(SvFLAGS(namesv) & SVf_FAKE)
	    && *SvPVX(namesv) == '&'
	    && CvCLONE(ppad[ix]))
	{
	    CV *kid = cv_clone2((CV*)ppad[ix], cv);
	    SvREFCNT_dec(ppad[ix]);
	    CvCLONE_on(kid);
	    SvPADMY_on(kid);
	    PL_curpad[ix] = (SV*)kid;
	}
    }

#ifdef DEBUG_CLOSURES
    PerlIO_printf(Perl_debug_log, "Cloned inside:\n");
    cv_dump(outside);
    PerlIO_printf(Perl_debug_log, "  from:\n");
    cv_dump(proto);
    PerlIO_printf(Perl_debug_log, "   to:\n");
    cv_dump(cv);
#endif

    LEAVE;
    return cv;
}

CV *
cv_clone(CV *proto)
{
    CV *cv;
    MUTEX_LOCK(&PL_cred_mutex);		/* XXX create separate mutex */
    cv = cv_clone2(proto, CvOUTSIDE(proto));
    MUTEX_UNLOCK(&PL_cred_mutex);	/* XXX create separate mutex */
    return cv;
}

void
cv_ckproto(CV *cv, GV *gv, char *p)
{
    if ((!p != !SvPOK(cv)) || (p && strNE(p, SvPVX(cv)))) {
	SV* msg = sv_newmortal();
	SV* name = Nullsv;

	if (gv)
	    gv_efullname3(name = sv_newmortal(), gv, Nullch);
	sv_setpv(msg, "Prototype mismatch:");
	if (name)
	    sv_catpvf(msg, " sub %_", name);
	if (SvPOK(cv))
	    sv_catpvf(msg, " (%s)", SvPVX(cv));
	sv_catpv(msg, " vs ");
	if (p)
	    sv_catpvf(msg, "(%s)", p);
	else
	    sv_catpv(msg, "none");
	warn("%_", msg);
    }
}

SV *
cv_const_sv(CV *cv)
{
    if (!cv || !SvPOK(cv) || SvCUR(cv))
	return Nullsv;
    return op_const_sv(CvSTART(cv), cv);
}

SV *
op_const_sv(OP *o, CV *cv)
{
    SV *sv = Nullsv;

    if(!o)
	return Nullsv;
 
    if(o->op_type == OP_LINESEQ && cLISTOPo->op_first) 
	o = cLISTOPo->op_first->op_sibling;

    for (; o; o = o->op_next) {
	OPCODE type = o->op_type;

	if(sv && o->op_next == o) 
	    return sv;
	if (type == OP_NEXTSTATE || type == OP_NULL || type == OP_PUSHMARK)
	    continue;
	if (type == OP_LEAVESUB || type == OP_RETURN)
	    break;
	if (sv)
	    return Nullsv;
	if (type == OP_CONST)
	    sv = cSVOPo->op_sv;
	else if (type == OP_PADSV && cv) {
	    AV* padav = (AV*)(AvARRAY(CvPADLIST(cv))[1]);
	    sv = padav ? AvARRAY(padav)[o->op_targ] : Nullsv;
	    if (!sv || (!SvREADONLY(sv) && SvREFCNT(sv) > 1))
		return Nullsv;
	}
	else
	    return Nullsv;
    }
    if (sv)
	SvREADONLY_on(sv);
    return sv;
}

CV *
newSUB(I32 floor, OP *o, OP *proto, OP *block)
{
    dTHR;
    STRLEN n_a;
    char *name = o ? SvPVx(cSVOPo->op_sv, n_a) : Nullch;
    GV *gv = gv_fetchpv(name ? name : "__ANON__",
			GV_ADDMULTI | (block ? 0 : GV_NOINIT), SVt_PVCV);
    char *ps = proto ? SvPVx(((SVOP*)proto)->op_sv, n_a) : Nullch;
    register CV *cv=0;
    I32 ix;

    if (o)
	SAVEFREEOP(o);
    if (proto)
	SAVEFREEOP(proto);

    if (SvTYPE(gv) != SVt_PVGV) {	/* Prototype now, and had
					   maximum a prototype before. */
	if (SvTYPE(gv) > SVt_NULL) {
	    if (!SvPOK((SV*)gv) && !(SvIOK((SV*)gv) && SvIVX((SV*)gv) == -1))
		warn("Runaway prototype");
	    cv_ckproto((CV*)gv, NULL, ps);
	}
	if (ps)
	    sv_setpv((SV*)gv, ps);
	else
	    sv_setiv((SV*)gv, -1);
	SvREFCNT_dec(PL_compcv);
	cv = PL_compcv = NULL;
	PL_sub_generation++;
	goto noblock;
    }

    if (!name || GvCVGEN(gv))
	cv = Nullcv;
    else if (cv = GvCV(gv)) {
	cv_ckproto(cv, gv, ps);
	/* already defined (or promised)? */
	if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
	    SV* const_sv;
	    bool const_changed = TRUE;
	    if (!block) {
		/* just a "sub foo;" when &foo is already defined */
		SAVEFREESV(PL_compcv);
		goto done;
	    }
	    /* ahem, death to those who redefine active sort subs */
	    if (PL_curstackinfo->si_type == PERLSI_SORT && PL_sortcop == CvSTART(cv))
		croak("Can't redefine active sort subroutine %s", name);
	    if(const_sv = cv_const_sv(cv))
		const_changed = sv_cmp(const_sv, op_const_sv(block, Nullcv));
	    if ((const_sv && const_changed) || PL_dowarn && !(CvGV(cv) && GvSTASH(CvGV(cv))
					&& HvNAME(GvSTASH(CvGV(cv)))
					&& strEQ(HvNAME(GvSTASH(CvGV(cv))),
						 "autouse"))) {
		line_t oldline = PL_curcop->cop_line;
		PL_curcop->cop_line = PL_copline;
		warn(const_sv ? "Constant subroutine %s redefined"
		     : "Subroutine %s redefined", name);
		PL_curcop->cop_line = oldline;
	    }
	    SvREFCNT_dec(cv);
	    cv = Nullcv;
	}
    }
    if (cv) {				/* must reuse cv if autoloaded */
	cv_undef(cv);
	CvFLAGS(cv) = CvFLAGS(PL_compcv);
	CvOUTSIDE(cv) = CvOUTSIDE(PL_compcv);
	CvOUTSIDE(PL_compcv) = 0;
	CvPADLIST(cv) = CvPADLIST(PL_compcv);
	CvPADLIST(PL_compcv) = 0;
	if (SvREFCNT(PL_compcv) > 1) /* XXX Make closures transit through stub. */
	    CvOUTSIDE(PL_compcv) = (CV*)SvREFCNT_inc((SV*)cv);
	SvREFCNT_dec(PL_compcv);
    }
    else {
	cv = PL_compcv;
	if (name) {
	    GvCV(gv) = cv;
	    GvCVGEN(gv) = 0;
	    PL_sub_generation++;
	}
    }
    CvGV(cv) = (GV*)SvREFCNT_inc(gv);
    CvFILEGV(cv) = PL_curcop->cop_filegv;
    CvSTASH(cv) = PL_curstash;
#ifdef USE_THREADS
    CvOWNER(cv) = 0;
    if (!CvMUTEXP(cv)) {
	New(666, CvMUTEXP(cv), 1, perl_mutex);
	MUTEX_INIT(CvMUTEXP(cv));
    }
#endif /* USE_THREADS */

    if (ps)
	sv_setpv((SV*)cv, ps);

    if (PL_error_count) {
	op_free(block);
	block = Nullop;
	if (name) {
	    char *s = strrchr(name, ':');
	    s = s ? s+1 : name;
	    if (strEQ(s, "BEGIN")) {
		char *not_safe =
		    "BEGIN not safe after errors--compilation aborted";
		if (PL_in_eval & 4)
		    croak(not_safe);
		else {
		    /* force display of errors found but not reported */
		    sv_catpv(ERRSV, not_safe);
		    croak("%s", SvPVx(ERRSV, n_a));
		}
	    }
	}
    }
    if (!block) {
      noblock:
	PL_copline = NOLINE;
	LEAVE_SCOPE(floor);
	return cv;
    }

    if (AvFILLp(PL_comppad_name) < AvFILLp(PL_comppad))
	av_store(PL_comppad_name, AvFILLp(PL_comppad), Nullsv);

    if (CvCLONE(cv)) {
	SV **namep = AvARRAY(PL_comppad_name);
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    SV *namesv;

	    if (SvIMMORTAL(PL_curpad[ix]))
		continue;
	    /*
	     * The only things that a clonable function needs in its
	     * pad are references to outer lexicals and anonymous subs.
	     * The rest are created anew during cloning.
	     */
	    if (!((namesv = namep[ix]) != Nullsv &&
		  namesv != &PL_sv_undef &&
		  (SvFAKE(namesv) ||
		   *SvPVX(namesv) == '&')))
	    {
		SvREFCNT_dec(PL_curpad[ix]);
		PL_curpad[ix] = Nullsv;
	    }
	}
    }
    else {
	AV *av = newAV();			/* Will be @_ */
	av_extend(av, 0);
	av_store(PL_comppad, 0, (SV*)av);
	AvFLAGS(av) = AVf_REIFY;

	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    if (SvIMMORTAL(PL_curpad[ix]))
		continue;
	    if (!SvPADMY(PL_curpad[ix]))
		SvPADTMP_on(PL_curpad[ix]);
	}
    }

    CvROOT(cv) = newUNOP(OP_LEAVESUB, 0, scalarseq(block));
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    peep(CvSTART(cv));

    if (name) {
	char *s;

	if (PERLDB_SUBLINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(0,0);
	    SV *tmpstr = sv_newmortal();
	    GV *db_postponed = gv_fetchpv("DB::postponed", GV_ADDMULTI, SVt_PVHV);
	    CV *cv;
	    HV *hv;

	    sv_setpvf(sv, "%_:%ld-%ld",
		    GvSV(PL_curcop->cop_filegv),
		    (long)PL_subline, (long)PL_curcop->cop_line);
	    gv_efullname3(tmpstr, gv, Nullch);
	    hv_store(GvHV(PL_DBsub), SvPVX(tmpstr), SvCUR(tmpstr), sv, 0);
	    hv = GvHVn(db_postponed);
	    if (HvFILL(hv) > 0 && hv_exists(hv, SvPVX(tmpstr), SvCUR(tmpstr))
		  && (cv = GvCV(db_postponed))) {
		dSP;
		PUSHMARK(SP);
		XPUSHs(tmpstr);
		PUTBACK;
		perl_call_sv((SV*)cv, G_DISCARD);
	    }
	}

	if ((s = strrchr(name,':')))
	    s++;
	else
	    s = name;
	if (strEQ(s, "BEGIN")) {
	    I32 oldscope = PL_scopestack_ix;
	    ENTER;
	    SAVESPTR(PL_compiling.cop_filegv);
	    SAVEI16(PL_compiling.cop_line);
	    save_svref(&PL_rs);
	    sv_setsv(PL_rs, PL_nrs);

	    if (!PL_beginav)
		PL_beginav = newAV();
	    DEBUG_x( dump_sub(gv) );
	    av_push(PL_beginav, (SV *)cv);
	    GvCV(gv) = 0;
	    call_list(oldscope, PL_beginav);

	    PL_curcop = &PL_compiling;
	    LEAVE;
	}
	else if (strEQ(s, "END") && !PL_error_count) {
	    if (!PL_endav)
		PL_endav = newAV();
	    av_unshift(PL_endav, 1);
	    av_store(PL_endav, 0, (SV *)cv);
	    GvCV(gv) = 0;
	}
	else if (strEQ(s, "INIT") && !PL_error_count) {
	    if (!PL_initav)
		PL_initav = newAV();
	    av_push(PL_initav, SvREFCNT_inc(cv));
	    GvCV(gv) = 0;
	}
    }

  done:
    PL_copline = NOLINE;
    LEAVE_SCOPE(floor);
    return cv;
}

/* XXX unsafe for threads if eval_owner isn't held */
void
newCONSTSUB(HV *stash, char *name, SV *sv)
{
    dTHR;
    U32 oldhints = PL_hints;
    HV *old_cop_stash = PL_curcop->cop_stash;
    HV *old_curstash = PL_curstash;
    line_t oldline = PL_curcop->cop_line;
    PL_curcop->cop_line = PL_copline;

    PL_hints &= ~HINT_BLOCK_SCOPE;
    if(stash)
	PL_curstash = PL_curcop->cop_stash = stash;

    newSUB(
	start_subparse(FALSE, 0),
	newSVOP(OP_CONST, 0, newSVpv(name,0)),
	newSVOP(OP_CONST, 0, &PL_sv_no),	/* SvPV(&PL_sv_no) == "" -- GMB */
	newSTATEOP(0, Nullch, newSVOP(OP_CONST, 0, sv))
    );

    PL_hints = oldhints;
    PL_curcop->cop_stash = old_cop_stash;
    PL_curstash = old_curstash;
    PL_curcop->cop_line = oldline;
}

CV *
newXS(char *name, void (*subaddr) (CV * _CPERLproto), char *filename)
{
    dTHR;
    GV *gv = gv_fetchpv(name ? name : "__ANON__", GV_ADDMULTI, SVt_PVCV);
    register CV *cv;

    if (cv = (name ? GvCV(gv) : Nullcv)) {
	if (GvCVGEN(gv)) {
	    /* just a cached method */
	    SvREFCNT_dec(cv);
	    cv = 0;
	}
	else if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
	    /* already defined (or promised) */
	    if (PL_dowarn && !(CvGV(cv) && GvSTASH(CvGV(cv))
			    && HvNAME(GvSTASH(CvGV(cv)))
			    && strEQ(HvNAME(GvSTASH(CvGV(cv))), "autouse"))) {
		line_t oldline = PL_curcop->cop_line;
		if (PL_copline != NOLINE)
		    PL_curcop->cop_line = PL_copline;
		warn("Subroutine %s redefined",name);
		PL_curcop->cop_line = oldline;
	    }
	    SvREFCNT_dec(cv);
	    cv = 0;
	}
    }

    if (cv)				/* must reuse cv if autoloaded */
	cv_undef(cv);
    else {
	cv = (CV*)NEWSV(1105,0);
	sv_upgrade((SV *)cv, SVt_PVCV);
	if (name) {
	    GvCV(gv) = cv;
	    GvCVGEN(gv) = 0;
	    PL_sub_generation++;
	}
    }
    CvGV(cv) = (GV*)SvREFCNT_inc(gv);
#ifdef USE_THREADS
    New(666, CvMUTEXP(cv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(cv));
    CvOWNER(cv) = 0;
#endif /* USE_THREADS */
    CvFILEGV(cv) = gv_fetchfile(filename);
    CvXSUB(cv) = subaddr;

    if (name) {
	char *s = strrchr(name,':');
	if (s)
	    s++;
	else
	    s = name;
	if (strEQ(s, "BEGIN")) {
	    if (!PL_beginav)
		PL_beginav = newAV();
	    av_push(PL_beginav, (SV *)cv);
	    GvCV(gv) = 0;
	}
	else if (strEQ(s, "END")) {
	    if (!PL_endav)
		PL_endav = newAV();
	    av_unshift(PL_endav, 1);
	    av_store(PL_endav, 0, (SV *)cv);
	    GvCV(gv) = 0;
	}
	else if (strEQ(s, "INIT")) {
	    if (!PL_initav)
		PL_initav = newAV();
	    av_push(PL_initav, (SV *)cv);
	    GvCV(gv) = 0;
	}
    }
    else
	CvANON_on(cv);

    return cv;
}

void
newFORM(I32 floor, OP *o, OP *block)
{
    dTHR;
    register CV *cv;
    char *name;
    GV *gv;
    I32 ix;
    STRLEN n_a;

    if (o)
	name = SvPVx(cSVOPo->op_sv, n_a);
    else
	name = "STDOUT";
    gv = gv_fetchpv(name,TRUE, SVt_PVFM);
    GvMULTI_on(gv);
    if (cv = GvFORM(gv)) {
	if (PL_dowarn) {
	    line_t oldline = PL_curcop->cop_line;

	    PL_curcop->cop_line = PL_copline;
	    warn("Format %s redefined",name);
	    PL_curcop->cop_line = oldline;
	}
	SvREFCNT_dec(cv);
    }
    cv = PL_compcv;
    GvFORM(gv) = cv;
    CvGV(cv) = (GV*)SvREFCNT_inc(gv);
    CvFILEGV(cv) = PL_curcop->cop_filegv;

    for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	if (!SvPADMY(PL_curpad[ix]) && !SvIMMORTAL(PL_curpad[ix]))
	    SvPADTMP_on(PL_curpad[ix]);
    }

    CvROOT(cv) = newUNOP(OP_LEAVEWRITE, 0, scalarseq(block));
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    peep(CvSTART(cv));
    op_free(o);
    PL_copline = NOLINE;
    LEAVE_SCOPE(floor);
}

OP *
newANONLIST(OP *o)
{
    return newUNOP(OP_REFGEN, 0,
	mod(list(convert(OP_ANONLIST, 0, o)), OP_REFGEN));
}

OP *
newANONHASH(OP *o)
{
    return newUNOP(OP_REFGEN, 0,
	mod(list(convert(OP_ANONHASH, 0, o)), OP_REFGEN));
}

OP *
newANONSUB(I32 floor, OP *proto, OP *block)
{
    return newUNOP(OP_REFGEN, 0,
	newSVOP(OP_ANONCODE, 0, (SV*)newSUB(floor, 0, proto, block)));
}

OP *
oopsAV(OP *o)
{
    switch (o->op_type) {
    case OP_PADSV:
	o->op_type = OP_PADAV;
	o->op_ppaddr = ppaddr[OP_PADAV];
	return ref(o, OP_RV2AV);
	
    case OP_RV2SV:
	o->op_type = OP_RV2AV;
	o->op_ppaddr = ppaddr[OP_RV2AV];
	ref(o, OP_RV2AV);
	break;

    default:
	warn("oops: oopsAV");
	break;
    }
    return o;
}

OP *
oopsHV(OP *o)
{
    switch (o->op_type) {
    case OP_PADSV:
    case OP_PADAV:
	o->op_type = OP_PADHV;
	o->op_ppaddr = ppaddr[OP_PADHV];
	return ref(o, OP_RV2HV);

    case OP_RV2SV:
    case OP_RV2AV:
	o->op_type = OP_RV2HV;
	o->op_ppaddr = ppaddr[OP_RV2HV];
	ref(o, OP_RV2HV);
	break;

    default:
	warn("oops: oopsHV");
	break;
    }
    return o;
}

OP *
newAVREF(OP *o)
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADAV;
	o->op_ppaddr = ppaddr[OP_PADAV];
	return o;
    }
    return newUNOP(OP_RV2AV, 0, scalar(o));
}

OP *
newGVREF(I32 type, OP *o)
{
    if (type == OP_MAPSTART || type == OP_GREPSTART)
	return newUNOP(OP_NULL, 0, o);
    return ref(newUNOP(OP_RV2GV, OPf_REF, o), type);
}

OP *
newHVREF(OP *o)
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADHV;
	o->op_ppaddr = ppaddr[OP_PADHV];
	return o;
    }
    return newUNOP(OP_RV2HV, 0, scalar(o));
}

OP *
oopsCV(OP *o)
{
    croak("NOT IMPL LINE %d",__LINE__);
    /* STUB */
    return o;
}

OP *
newCVREF(I32 flags, OP *o)
{
    return newUNOP(OP_RV2CV, flags, scalar(o));
}

OP *
newSVREF(OP *o)
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADSV;
	o->op_ppaddr = ppaddr[OP_PADSV];
	return o;
    }
    else if (o->op_type == OP_THREADSV && !(o->op_flags & OPpDONE_SVREF)) {
	o->op_flags |= OPpDONE_SVREF;
	return o;
    }
    return newUNOP(OP_RV2SV, 0, scalar(o));
}

/* Check routines. */

OP *
ck_anoncode(OP *o)
{
    PADOFFSET ix;
    SV* name;

    name = NEWSV(1106,0);
    sv_upgrade(name, SVt_PVNV);
    sv_setpvn(name, "&", 1);
    SvIVX(name) = -1;
    SvNVX(name) = 1;
    ix = pad_alloc(o->op_type, SVs_PADMY);
    av_store(PL_comppad_name, ix, name);
    av_store(PL_comppad, ix, cSVOPo->op_sv);
    SvPADMY_on(cSVOPo->op_sv);
    cSVOPo->op_sv = Nullsv;
    cSVOPo->op_targ = ix;
    return o;
}

OP *
ck_bitop(OP *o)
{
    o->op_private = PL_hints;
    return o;
}

OP *
ck_concat(OP *o)
{
    if (cUNOPo->op_first->op_type == OP_CONCAT)
	o->op_flags |= OPf_STACKED;
    return o;
}

OP *
ck_spair(OP *o)
{
    if (o->op_flags & OPf_KIDS) {
	OP* newop;
	OP* kid;
	OPCODE type = o->op_type;
	o = modkids(ck_fun(o), type);
	kid = cUNOPo->op_first;
	newop = kUNOP->op_first->op_sibling;
	if (newop &&
	    (newop->op_sibling ||
	     !(opargs[newop->op_type] & OA_RETSCALAR) ||
	     newop->op_type == OP_PADAV || newop->op_type == OP_PADHV ||
	     newop->op_type == OP_RV2AV || newop->op_type == OP_RV2HV)) {
	
	    return o;
	}
	op_free(kUNOP->op_first);
	kUNOP->op_first = newop;
    }
    o->op_ppaddr = ppaddr[++o->op_type];
    return ck_fun(o);
}

OP *
ck_delete(OP *o)
{
    o = ck_fun(o);
    o->op_private = 0;
    if (o->op_flags & OPf_KIDS) {
	OP *kid = cUNOPo->op_first;
	if (kid->op_type == OP_HSLICE)
	    o->op_private |= OPpSLICE;
	else if (kid->op_type != OP_HELEM)
	    croak("%s argument is not a HASH element or slice",
		  op_desc[o->op_type]);
	null(kid);
    }
    return o;
}

OP *
ck_eof(OP *o)
{
    I32 type = o->op_type;

    if (o->op_flags & OPf_KIDS) {
	if (cLISTOPo->op_first->op_type == OP_STUB) {
	    op_free(o);
	    o = newUNOP(type, OPf_SPECIAL,
		newGVOP(OP_GV, 0, gv_fetchpv("main::ARGV", TRUE, SVt_PVAV)));
	}
	return ck_fun(o);
    }
    return o;
}

OP *
ck_eval(OP *o)
{
    PL_hints |= HINT_BLOCK_SCOPE;
    if (o->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (!kid) {
	    o->op_flags &= ~OPf_KIDS;
	    null(o);
	}
	else if (kid->op_type == OP_LINESEQ) {
	    LOGOP *enter;

	    kid->op_next = o->op_next;
	    cUNOPo->op_first = 0;
	    op_free(o);

	    Newz(1101, enter, 1, LOGOP);
	    enter->op_type = OP_ENTERTRY;
	    enter->op_ppaddr = ppaddr[OP_ENTERTRY];
	    enter->op_private = 0;

	    /* establish postfix order */
	    enter->op_next = (OP*)enter;

	    o = prepend_elem(OP_LINESEQ, (OP*)enter, (OP*)kid);
	    o->op_type = OP_LEAVETRY;
	    o->op_ppaddr = ppaddr[OP_LEAVETRY];
	    enter->op_other = o;
	    return o;
	}
	else
	    scalar((OP*)kid);
    }
    else {
	op_free(o);
	o = newUNOP(OP_ENTEREVAL, 0, newDEFSVOP());
    }
    o->op_targ = (PADOFFSET)PL_hints;
    return o;
}

OP *
ck_exec(OP *o)
{
    OP *kid;
    if (o->op_flags & OPf_STACKED) {
	o = ck_fun(o);
	kid = cUNOPo->op_first->op_sibling;
	if (kid->op_type == OP_RV2GV)
	    null(kid);
    }
    else
	o = listkids(o);
    return o;
}

OP *
ck_exists(OP *o)
{
    o = ck_fun(o);
    if (o->op_flags & OPf_KIDS) {
	OP *kid = cUNOPo->op_first;
	if (kid->op_type != OP_HELEM)
	    croak("%s argument is not a HASH element", op_desc[o->op_type]);
	null(kid);
    }
    return o;
}

OP *
ck_gvconst(register OP *o)
{
    o = fold_constants(o);
    if (o->op_type == OP_CONST)
	o->op_type = OP_GV;
    return o;
}

OP *
ck_rvconst(register OP *o)
{
    dTHR;
    SVOP *kid = (SVOP*)cUNOPo->op_first;

    o->op_private |= (PL_hints & HINT_STRICT_REFS);
    if (kid->op_type == OP_CONST) {
	char *name;
	int iscv;
	GV *gv;
	SV *kidsv = kid->op_sv;
	STRLEN n_a;

	/* Is it a constant from cv_const_sv()? */
	if (SvROK(kidsv) && SvREADONLY(kidsv)) {
	    SV *rsv = SvRV(kidsv);
	    int svtype = SvTYPE(rsv);
	    char *badtype = Nullch;

	    switch (o->op_type) {
	    case OP_RV2SV:
		if (svtype > SVt_PVMG)
		    badtype = "a SCALAR";
		break;
	    case OP_RV2AV:
		if (svtype != SVt_PVAV)
		    badtype = "an ARRAY";
		break;
	    case OP_RV2HV:
		if (svtype != SVt_PVHV) {
		    if (svtype == SVt_PVAV) {	/* pseudohash? */
			SV **ksv = av_fetch((AV*)rsv, 0, FALSE);
			if (ksv && SvROK(*ksv)
			    && SvTYPE(SvRV(*ksv)) == SVt_PVHV)
			{
				break;
			}
		    }
		    badtype = "a HASH";
		}
		break;
	    case OP_RV2CV:
		if (svtype != SVt_PVCV)
		    badtype = "a CODE";
		break;
	    }
	    if (badtype)
		croak("Constant is not %s reference", badtype);
	    return o;
	}
	name = SvPV(kidsv, n_a);

	if ((PL_hints & HINT_STRICT_REFS) && (kid->op_private & OPpCONST_BARE)) {
	    char *badthing = Nullch;
	    switch (o->op_type) {
	    case OP_RV2SV:
		badthing = "a SCALAR";
		break;
	    case OP_RV2AV:
		badthing = "an ARRAY";
		break;
	    case OP_RV2HV:
		badthing = "a HASH";
		break;
	    }
	    if (badthing)
		croak(
	  "Can't use bareword (\"%s\") as %s ref while \"strict refs\" in use",
		      name, badthing);
	}
	/*
	 * This is a little tricky.  We only want to add the symbol if we
	 * didn't add it in the lexer.  Otherwise we get duplicate strict
	 * warnings.  But if we didn't add it in the lexer, we must at
	 * least pretend like we wanted to add it even if it existed before,
	 * or we get possible typo warnings.  OPpCONST_ENTERED says
	 * whether the lexer already added THIS instance of this symbol.
	 */
	iscv = (o->op_type == OP_RV2CV) * 2;
	do {
	    gv = gv_fetchpv(name,
		iscv | !(kid->op_private & OPpCONST_ENTERED),
		iscv
		    ? SVt_PVCV
		    : o->op_type == OP_RV2SV
			? SVt_PV
			: o->op_type == OP_RV2AV
			    ? SVt_PVAV
			    : o->op_type == OP_RV2HV
				? SVt_PVHV
				: SVt_PVGV);
	} while (!gv && !(kid->op_private & OPpCONST_ENTERED) && !iscv++);
	if (gv) {
	    kid->op_type = OP_GV;
	    SvREFCNT_dec(kid->op_sv);
	    kid->op_sv = SvREFCNT_inc(gv);
	}
    }
    return o;
}

OP *
ck_ftst(OP *o)
{
    dTHR;
    I32 type = o->op_type;

    if (o->op_flags & OPf_REF)
	return o;

    if (o->op_flags & OPf_KIDS && cUNOPo->op_first->op_type != OP_STUB) {
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE)) {
	    STRLEN n_a;
	    OP *newop = newGVOP(type, OPf_REF,
		gv_fetchpv(SvPVx(kid->op_sv, n_a), TRUE, SVt_PVIO));
	    op_free(o);
	    return newop;
	}
    }
    else {
	op_free(o);
	if (type == OP_FTTTY)
           return newGVOP(type, OPf_REF, gv_fetchpv("main::STDIN", TRUE,
				SVt_PVIO));
	else
	    return newUNOP(type, 0, newDEFSVOP());
    }
    return o;
}

OP *
ck_fun(OP *o)
{
    dTHR;
    register OP *kid;
    OP **tokid;
    OP *sibl;
    I32 numargs = 0;
    int type = o->op_type;
    register I32 oa = opargs[type] >> OASHIFT;

    if (o->op_flags & OPf_STACKED) {
	if ((oa & OA_OPTIONAL) && (oa >> 4) && !((oa >> 4) & OA_OPTIONAL))
	    oa &= ~OA_OPTIONAL;
	else
	    return no_fh_allowed(o);
    }

    if (o->op_flags & OPf_KIDS) {
	STRLEN n_a;
	tokid = &cLISTOPo->op_first;
	kid = cLISTOPo->op_first;
	if (kid->op_type == OP_PUSHMARK ||
	    kid->op_type == OP_NULL && kid->op_targ == OP_PUSHMARK)
	{
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
	if (!kid && opargs[type] & OA_DEFGV)
	    *tokid = kid = newDEFSVOP();

	while (oa && kid) {
	    numargs++;
	    sibl = kid->op_sibling;
	    switch (oa & 7) {
	    case OA_SCALAR:
		scalar(kid);
		break;
	    case OA_LIST:
		if (oa < 16) {
		    kid = 0;
		    continue;
		}
		else
		    list(kid);
		break;
	    case OA_AVREF:
		if (kid->op_type == OP_CONST &&
		  (kid->op_private & OPpCONST_BARE)) {
		    char *name = SvPVx(((SVOP*)kid)->op_sv, n_a);
		    OP *newop = newAVREF(newGVOP(OP_GV, 0,
			gv_fetchpv(name, TRUE, SVt_PVAV) ));
		    if (PL_dowarn)
			warn("Array @%s missing the @ in argument %ld of %s()",
			    name, (long)numargs, op_desc[type]);
		    op_free(kid);
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2AV && kid->op_type != OP_PADAV)
		    bad_type(numargs, "array", op_desc[o->op_type], kid);
		mod(kid, type);
		break;
	    case OA_HVREF:
		if (kid->op_type == OP_CONST &&
		  (kid->op_private & OPpCONST_BARE)) {
		    char *name = SvPVx(((SVOP*)kid)->op_sv, n_a);
		    OP *newop = newHVREF(newGVOP(OP_GV, 0,
			gv_fetchpv(name, TRUE, SVt_PVHV) ));
		    if (PL_dowarn)
			warn("Hash %%%s missing the %% in argument %ld of %s()",
			    name, (long)numargs, op_desc[type]);
		    op_free(kid);
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2HV && kid->op_type != OP_PADHV)
		    bad_type(numargs, "hash", op_desc[o->op_type], kid);
		mod(kid, type);
		break;
	    case OA_CVREF:
		{
		    OP *newop = newUNOP(OP_NULL, 0, kid);
		    kid->op_sibling = 0;
		    linklist(kid);
		    newop->op_next = newop;
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		break;
	    case OA_FILEREF:
		if (kid->op_type != OP_GV && kid->op_type != OP_RV2GV) {
		    if (kid->op_type == OP_CONST &&
		      (kid->op_private & OPpCONST_BARE)) {
			OP *newop = newGVOP(OP_GV, 0,
			    gv_fetchpv(SvPVx(((SVOP*)kid)->op_sv, n_a), TRUE,
					SVt_PVIO) );
			op_free(kid);
			kid = newop;
		    }
		    else if (kid->op_type == OP_READLINE) {
			/* neophyte patrol: open(<FH>), close(<FH>) etc. */
			bad_type(numargs, "HANDLE", op_desc[o->op_type], kid);
		    }
		    else {
			kid->op_sibling = 0;
			kid = newUNOP(OP_RV2GV, 0, scalar(kid));
		    }
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		scalar(kid);
		break;
	    case OA_SCALARREF:
		mod(scalar(kid), type);
		break;
	    }
	    oa >>= 4;
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
	o->op_private |= numargs;
	if (kid)
	    return too_many_arguments(o,op_desc[o->op_type]);
	listkids(o);
    }
    else if (opargs[type] & OA_DEFGV) {
	op_free(o);
	return newUNOP(type, 0, newDEFSVOP());
    }

    if (oa) {
	while (oa & OA_OPTIONAL)
	    oa >>= 4;
	if (oa && oa != OA_LIST)
	    return too_few_arguments(o,op_desc[o->op_type]);
    }
    return o;
}

OP *
ck_glob(OP *o)
{
    GV *gv;

    if ((o->op_flags & OPf_KIDS) && !cLISTOPo->op_first->op_sibling)
	append_elem(OP_GLOB, o, newDEFSVOP());

    if (!((gv = gv_fetchpv("glob", FALSE, SVt_PVCV)) && GvIMPORTED_CV(gv)))
	gv = gv_fetchpv("CORE::GLOBAL::glob", FALSE, SVt_PVCV);

    if (gv && GvIMPORTED_CV(gv)) {
#ifndef PERL_OBJECT
	static int glob_index;
#endif

	append_elem(OP_GLOB, o,
		    newSVOP(OP_CONST, 0, newSViv(glob_index++)));
	o->op_type = OP_LIST;
	o->op_ppaddr = ppaddr[OP_LIST];
	cLISTOPo->op_first->op_type = OP_PUSHMARK;
	cLISTOPo->op_first->op_ppaddr = ppaddr[OP_PUSHMARK];
	o = newUNOP(OP_ENTERSUB, OPf_STACKED,
		    append_elem(OP_LIST, o,
				scalar(newUNOP(OP_RV2CV, 0,
					       newGVOP(OP_GV, 0, gv)))));
	o = newUNOP(OP_NULL, 0, ck_subr(o));
	o->op_targ = OP_GLOB;		/* hint at what it used to be */
	return o;
    }
    gv = newGVgen("main");
    gv_IOadd(gv);
    append_elem(OP_GLOB, o, newGVOP(OP_GV, 0, gv));
    scalarkids(o);
    return ck_fun(o);
}

OP *
ck_grep(OP *o)
{
    LOGOP *gwop;
    OP *kid;
    OPCODE type = o->op_type == OP_GREPSTART ? OP_GREPWHILE : OP_MAPWHILE;

    o->op_ppaddr = ppaddr[OP_GREPSTART];
    Newz(1101, gwop, 1, LOGOP);

    if (o->op_flags & OPf_STACKED) {
	OP* k;
	o = ck_sort(o);
        kid = cLISTOPo->op_first->op_sibling;
	for (k = cLISTOPo->op_first->op_sibling->op_next; k; k = k->op_next) {
	    kid = k;
	}
	kid->op_next = (OP*)gwop;
	o->op_flags &= ~OPf_STACKED;
    }
    kid = cLISTOPo->op_first->op_sibling;
    if (type == OP_MAPWHILE)
	list(kid);
    else
	scalar(kid);
    o = ck_fun(o);
    if (PL_error_count)
	return o;
    kid = cLISTOPo->op_first->op_sibling;
    if (kid->op_type != OP_NULL)
	croak("panic: ck_grep");
    kid = kUNOP->op_first;

    gwop->op_type = type;
    gwop->op_ppaddr = ppaddr[type];
    gwop->op_first = listkids(o);
    gwop->op_flags |= OPf_KIDS;
    gwop->op_private = 1;
    gwop->op_other = LINKLIST(kid);
    gwop->op_targ = pad_alloc(type, SVs_PADTMP);
    kid->op_next = (OP*)gwop;

    kid = cLISTOPo->op_first->op_sibling;
    if (!kid || !kid->op_sibling)
	return too_few_arguments(o,op_desc[o->op_type]);
    for (kid = kid->op_sibling; kid; kid = kid->op_sibling)
	mod(kid, OP_GREPSTART);

    return (OP*)gwop;
}

OP *
ck_index(OP *o)
{
    if (o->op_flags & OPf_KIDS) {
	OP *kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	if (kid)
	    kid = kid->op_sibling;			/* get past "big" */
	if (kid && kid->op_type == OP_CONST)
	    fbm_compile(((SVOP*)kid)->op_sv, 0);
    }
    return ck_fun(o);
}

OP *
ck_lengthconst(OP *o)
{
    /* XXX length optimization goes here */
    return ck_fun(o);
}

OP *
ck_lfun(OP *o)
{
    OPCODE type = o->op_type;
    return modkids(ck_fun(o), type);
}

OP *
ck_rfun(OP *o)
{
    OPCODE type = o->op_type;
    return refkids(ck_fun(o), type);
}

OP *
ck_listiob(OP *o)
{
    register OP *kid;

    kid = cLISTOPo->op_first;
    if (!kid) {
	o = force_list(o);
	kid = cLISTOPo->op_first;
    }
    if (kid->op_type == OP_PUSHMARK)
	kid = kid->op_sibling;
    if (kid && o->op_flags & OPf_STACKED)
	kid = kid->op_sibling;
    else if (kid && !kid->op_sibling) {		/* print HANDLE; */
	if (kid->op_type == OP_CONST && kid->op_private & OPpCONST_BARE) {
	    o->op_flags |= OPf_STACKED;	/* make it a filehandle */
	    kid = newUNOP(OP_RV2GV, OPf_REF, scalar(kid));
	    cLISTOPo->op_first->op_sibling = kid;
	    cLISTOPo->op_last = kid;
	    kid = kid->op_sibling;
	}
    }
	
    if (!kid)
	append_elem(o->op_type, o, newDEFSVOP());

    o = listkids(o);

    o->op_private = 0;
#ifdef USE_LOCALE
    if (PL_hints & HINT_LOCALE)
	o->op_private |= OPpLOCALE;
#endif

    return o;
}

OP *
ck_fun_locale(OP *o)
{
    o = ck_fun(o);

    o->op_private = 0;
#ifdef USE_LOCALE
    if (PL_hints & HINT_LOCALE)
	o->op_private |= OPpLOCALE;
#endif

    return o;
}

OP *
ck_scmp(OP *o)
{
    o->op_private = 0;
#ifdef USE_LOCALE
    if (PL_hints & HINT_LOCALE)
	o->op_private |= OPpLOCALE;
#endif

    return o;
}

OP *
ck_match(OP *o)
{
    o->op_private |= OPpRUNTIME;
    return o;
}

OP *
ck_null(OP *o)
{
    return o;
}

OP *
ck_repeat(OP *o)
{
    if (cBINOPo->op_first->op_flags & OPf_PARENS) {
	o->op_private |= OPpREPEAT_DOLIST;
	cBINOPo->op_first = force_list(cBINOPo->op_first);
    }
    else
	scalar(o);
    return o;
}

OP *
ck_require(OP *o)
{
    if (o->op_flags & OPf_KIDS) {	/* Shall we supply missing .pm? */
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE)) {
	    char *s;
	    for (s = SvPVX(kid->op_sv); *s; s++) {
		if (*s == ':' && s[1] == ':') {
		    *s = '/';
		    Move(s+2, s+1, strlen(s+2)+1, char);
		    --SvCUR(kid->op_sv);
		}
	    }
	    sv_catpvn(kid->op_sv, ".pm", 3);
	}
    }
    return ck_fun(o);
}

OP *
ck_retarget(OP *o)
{
    croak("NOT IMPL LINE %d",__LINE__);
    /* STUB */
    return o;
}

OP *
ck_select(OP *o)
{
    OP* kid;
    if (o->op_flags & OPf_KIDS) {
	kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	if (kid && kid->op_sibling) {
	    o->op_type = OP_SSELECT;
	    o->op_ppaddr = ppaddr[OP_SSELECT];
	    o = ck_fun(o);
	    return fold_constants(o);
	}
    }
    o = ck_fun(o);
    kid = cLISTOPo->op_first->op_sibling;    /* get past pushmark */
    if (kid && kid->op_type == OP_RV2GV)
	kid->op_private &= ~HINT_STRICT_REFS;
    return o;
}

OP *
ck_shift(OP *o)
{
    I32 type = o->op_type;

    if (!(o->op_flags & OPf_KIDS)) {
	OP *argop;
	
	op_free(o);
#ifdef USE_THREADS
	if (!CvUNIQUE(PL_compcv)) {
	    argop = newOP(OP_PADAV, OPf_REF);
	    argop->op_targ = 0;		/* PL_curpad[0] is @_ */
	}
	else {
	    argop = newUNOP(OP_RV2AV, 0,
		scalar(newGVOP(OP_GV, 0,
		    gv_fetchpv("ARGV", TRUE, SVt_PVAV))));
	}
#else
	argop = newUNOP(OP_RV2AV, 0,
	    scalar(newGVOP(OP_GV, 0, !CvUNIQUE(PL_compcv) ?
			   PL_defgv : gv_fetchpv("ARGV", TRUE, SVt_PVAV))));
#endif /* USE_THREADS */
	return newUNOP(type, 0, scalar(argop));
    }
    return scalar(modkids(ck_fun(o), type));
}

OP *
ck_sort(OP *o)
{
    o->op_private = 0;
#ifdef USE_LOCALE
    if (PL_hints & HINT_LOCALE)
	o->op_private |= OPpLOCALE;
#endif

    if (o->op_flags & OPf_STACKED) {
	OP *kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	OP *k;

	if (o->op_type == OP_SORT) {
	    GvMULTI_on(gv_fetchpv("a", TRUE, SVt_PV));
	    GvMULTI_on(gv_fetchpv("b", TRUE, SVt_PV));
	}
	kid = kUNOP->op_first;				/* get past rv2gv */

	if (kid->op_type == OP_SCOPE || kid->op_type == OP_LEAVE) {
	    linklist(kid);
	    if (kid->op_type == OP_SCOPE) {
		k = kid->op_next;
		kid->op_next = 0;
	    }
	    else if (kid->op_type == OP_LEAVE) {
		if (o->op_type == OP_SORT) {
		    null(kid);			/* wipe out leave */
		    kid->op_next = kid;

		    for (k = kLISTOP->op_first->op_next; k; k = k->op_next) {
			if (k->op_next == kid)
			    k->op_next = 0;
		    }
		}
		else
		    kid->op_next = 0;		/* just disconnect the leave */
		k = kLISTOP->op_first;
	    }
	    peep(k);

	    kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	    null(kid);					/* wipe out rv2gv */
	    if (o->op_type == OP_SORT)
		kid->op_next = kid;
	    else
		kid->op_next = k;
	    o->op_flags |= OPf_SPECIAL;
	}
	else if (kid->op_type == OP_RV2SV || kid->op_type == OP_PADSV)
	    null(cLISTOPo->op_first->op_sibling);
     }

    return o;
}

OP *
ck_split(OP *o)
{
    register OP *kid;

    if (o->op_flags & OPf_STACKED)
	return no_fh_allowed(o);

    kid = cLISTOPo->op_first;
    if (kid->op_type != OP_NULL)
	croak("panic: ck_split");
    kid = kid->op_sibling;
    op_free(cLISTOPo->op_first);
    cLISTOPo->op_first = kid;
    if (!kid) {
	cLISTOPo->op_first = kid = newSVOP(OP_CONST, 0, newSVpv(" ", 1));
	cLISTOPo->op_last = kid; /* There was only one element previously */
    }

    if (kid->op_type != OP_MATCH) {
	OP *sibl = kid->op_sibling;
	kid->op_sibling = 0;
	kid = pmruntime( newPMOP(OP_MATCH, OPf_SPECIAL), kid, Nullop);
	if (cLISTOPo->op_first == cLISTOPo->op_last)
	    cLISTOPo->op_last = kid;
	cLISTOPo->op_first = kid;
	kid->op_sibling = sibl;
    }

    kid->op_type = OP_PUSHRE;
    kid->op_ppaddr = ppaddr[OP_PUSHRE];
    scalar(kid);

    if (!kid->op_sibling)
	append_elem(OP_SPLIT, o, newDEFSVOP());

    kid = kid->op_sibling;
    scalar(kid);

    if (!kid->op_sibling)
	append_elem(OP_SPLIT, o, newSVOP(OP_CONST, 0, newSViv(0)));

    kid = kid->op_sibling;
    scalar(kid);

    if (kid->op_sibling)
	return too_many_arguments(o,op_desc[o->op_type]);

    return o;
}

OP *
ck_subr(OP *o)
{
    dTHR;
    OP *prev = ((cUNOPo->op_first->op_sibling)
	     ? cUNOPo : ((UNOP*)cUNOPo->op_first))->op_first;
    OP *o2 = prev->op_sibling;
    OP *cvop;
    char *proto = 0;
    CV *cv = 0;
    GV *namegv = 0;
    int optional = 0;
    I32 arg = 0;
    STRLEN n_a;

    for (cvop = o2; cvop->op_sibling; cvop = cvop->op_sibling) ;
    if (cvop->op_type == OP_RV2CV) {
	SVOP* tmpop;
	o->op_private |= (cvop->op_private & OPpENTERSUB_AMPER);
	null(cvop);		/* disable rv2cv */
	tmpop = (SVOP*)((UNOP*)cvop)->op_first;
	if (tmpop->op_type == OP_GV) {
	    cv = GvCVu(tmpop->op_sv);
	    if (cv && SvPOK(cv) && !(o->op_private & OPpENTERSUB_AMPER)) {
		namegv = CvANON(cv) ? (GV*)tmpop->op_sv : CvGV(cv);
		proto = SvPV((SV*)cv, n_a);
	    }
	}
    }
    o->op_private |= (PL_hints & HINT_STRICT_REFS);
    if (PERLDB_SUB && PL_curstash != PL_debstash)
	o->op_private |= OPpENTERSUB_DB;
    while (o2 != cvop) {
	if (proto) {
	    switch (*proto) {
	    case '\0':
		return too_many_arguments(o, gv_ename(namegv));
	    case ';':
		optional = 1;
		proto++;
		continue;
	    case '$':
		proto++;
		arg++;
		scalar(o2);
		break;
	    case '%':
	    case '@':
		list(o2);
		arg++;
		break;
	    case '&':
		proto++;
		arg++;
		if (o2->op_type != OP_REFGEN && o2->op_type != OP_UNDEF)
		    bad_type(arg, "block", gv_ename(namegv), o2);
		break;
	    case '*':
		/* '*' allows any scalar type, including bareword */
		proto++;
		arg++;
		if (o2->op_type == OP_RV2GV)
		    goto wrapref;	/* autoconvert GLOB -> GLOBref */
		scalar(o2);
		break;
	    case '\\':
		proto++;
		arg++;
		switch (*proto++) {
		case '*':
		    if (o2->op_type != OP_RV2GV)
			bad_type(arg, "symbol", gv_ename(namegv), o2);
		    goto wrapref;
		case '&':
		    if (o2->op_type != OP_RV2CV)
			bad_type(arg, "sub", gv_ename(namegv), o2);
		    goto wrapref;
		case '$':
		    if (o2->op_type != OP_RV2SV
			&& o2->op_type != OP_PADSV
			&& o2->op_type != OP_THREADSV)
		    {
			bad_type(arg, "scalar", gv_ename(namegv), o2);
		    }
		    goto wrapref;
		case '@':
		    if (o2->op_type != OP_RV2AV && o2->op_type != OP_PADAV)
			bad_type(arg, "array", gv_ename(namegv), o2);
		    goto wrapref;
		case '%':
		    if (o2->op_type != OP_RV2HV && o2->op_type != OP_PADHV)
			bad_type(arg, "hash", gv_ename(namegv), o2);
		  wrapref:
		    {
			OP* kid = o2;
			OP* sib = kid->op_sibling;
			kid->op_sibling = 0;
			o2 = newUNOP(OP_REFGEN, 0, kid);
			o2->op_sibling = sib;
			prev->op_sibling = o2;
		    }
		    break;
		default: goto oops;
		}
		break;
	    case ' ':
		proto++;
		continue;
	    default:
	      oops:
		croak("Malformed prototype for %s: %s",
			gv_ename(namegv), SvPV((SV*)cv, n_a));
	    }
	}
	else
	    list(o2);
	mod(o2, OP_ENTERSUB);
	prev = o2;
	o2 = o2->op_sibling;
    }
    if (proto && !optional &&
	  (*proto && *proto != '@' && *proto != '%' && *proto != ';'))
	return too_few_arguments(o, gv_ename(namegv));
    return o;
}

OP *
ck_svconst(OP *o)
{
    SvREADONLY_on(cSVOPo->op_sv);
    return o;
}

OP *
ck_trunc(OP *o)
{
    if (o->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_NULL)
	    kid = (SVOP*)kid->op_sibling;
	if (kid &&
	  kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE))
	    o->op_flags |= OPf_SPECIAL;
    }
    return ck_fun(o);
}

/* A peephole optimizer.  We visit the ops in the order they're to execute. */

void
peep(register OP *o)
{
    dTHR;
    register OP* oldop = 0;
    STRLEN n_a;
    if (!o || o->op_seq)
	return;
    ENTER;
    SAVEOP();
    SAVESPTR(PL_curcop);
    for (; o; o = o->op_next) {
	if (o->op_seq)
	    break;
	if (!PL_op_seqmax)
	    PL_op_seqmax++;
	PL_op = o;
	switch (o->op_type) {
	case OP_NEXTSTATE:
	case OP_DBSTATE:
	    PL_curcop = ((COP*)o);		/* for warnings */
	    o->op_seq = PL_op_seqmax++;
	    break;

	case OP_CONCAT:
	case OP_CONST:
	case OP_JOIN:
	case OP_UC:
	case OP_UCFIRST:
	case OP_LC:
	case OP_LCFIRST:
	case OP_QUOTEMETA:
	    if (o->op_next && o->op_next->op_type == OP_STRINGIFY)
		null(o->op_next);
	    o->op_seq = PL_op_seqmax++;
	    break;
	case OP_STUB:
	    if ((o->op_flags & OPf_WANT) != OPf_WANT_LIST) {
		o->op_seq = PL_op_seqmax++;
		break; /* Scalar stub must produce undef.  List stub is noop */
	    }
	    goto nothin;
	case OP_NULL:
	    if (o->op_targ == OP_NEXTSTATE || o->op_targ == OP_DBSTATE)
		PL_curcop = ((COP*)o);
	    goto nothin;
	case OP_SCALAR:
	case OP_LINESEQ:
	case OP_SCOPE:
	  nothin:
	    if (oldop && o->op_next) {
		oldop->op_next = o->op_next;
		continue;
	    }
	    o->op_seq = PL_op_seqmax++;
	    break;

	case OP_GV:
	    if (o->op_next->op_type == OP_RV2SV) {
		if (!(o->op_next->op_private & OPpDEREF)) {
		    null(o->op_next);
		    o->op_private |= o->op_next->op_private & OPpLVAL_INTRO;
		    o->op_next = o->op_next->op_next;
		    o->op_type = OP_GVSV;
		    o->op_ppaddr = ppaddr[OP_GVSV];
		}
	    }
	    else if (o->op_next->op_type == OP_RV2AV) {
		OP* pop = o->op_next->op_next;
		IV i;
		if (pop->op_type == OP_CONST &&
		    (PL_op = pop->op_next) &&
		    pop->op_next->op_type == OP_AELEM &&
		    !(pop->op_next->op_private &
		      (OPpLVAL_INTRO|OPpLVAL_DEFER|OPpDEREF)) &&
		    (i = SvIV(((SVOP*)pop)->op_sv) - PL_compiling.cop_arybase)
				<= 255 &&
		    i >= 0)
		{
		    SvREFCNT_dec(((SVOP*)pop)->op_sv);
		    null(o->op_next);
		    null(pop->op_next);
		    null(pop);
		    o->op_flags |= pop->op_next->op_flags & OPf_MOD;
		    o->op_next = pop->op_next->op_next;
		    o->op_type = OP_AELEMFAST;
		    o->op_ppaddr = ppaddr[OP_AELEMFAST];
		    o->op_private = (U8)i;
		    GvAVn(((GVOP*)o)->op_gv);
		}
	    }
	    o->op_seq = PL_op_seqmax++;
	    break;

	case OP_MAPWHILE:
	case OP_GREPWHILE:
	case OP_AND:
	case OP_OR:
	    o->op_seq = PL_op_seqmax++;
	    while (cLOGOP->op_other->op_type == OP_NULL)
		cLOGOP->op_other = cLOGOP->op_other->op_next;
	    peep(cLOGOP->op_other);
	    break;

	case OP_COND_EXPR:
	    o->op_seq = PL_op_seqmax++;
	    peep(cCONDOP->op_true);
	    peep(cCONDOP->op_false);
	    break;

	case OP_ENTERLOOP:
	    o->op_seq = PL_op_seqmax++;
	    peep(cLOOP->op_redoop);
	    peep(cLOOP->op_nextop);
	    peep(cLOOP->op_lastop);
	    break;

	case OP_QR:
	case OP_MATCH:
	case OP_SUBST:
	    o->op_seq = PL_op_seqmax++;
	    peep(cPMOP->op_pmreplstart);
	    break;

	case OP_EXEC:
	    o->op_seq = PL_op_seqmax++;
	    if (PL_dowarn && o->op_next && o->op_next->op_type == OP_NEXTSTATE) {
		if (o->op_next->op_sibling &&
			o->op_next->op_sibling->op_type != OP_EXIT &&
			o->op_next->op_sibling->op_type != OP_WARN &&
			o->op_next->op_sibling->op_type != OP_DIE) {
		    line_t oldline = PL_curcop->cop_line;

		    PL_curcop->cop_line = ((COP*)o->op_next)->cop_line;
		    warn("Statement unlikely to be reached");
		    warn("(Maybe you meant system() when you said exec()?)\n");
		    PL_curcop->cop_line = oldline;
		}
	    }
	    break;
	
	case OP_HELEM: {
	    UNOP *rop;
	    SV *lexname;
	    GV **fields;
	    SV **svp, **indsvp;
	    I32 ind;
	    char *key;
	    STRLEN keylen;
	
	    if (o->op_private & (OPpDEREF_HV|OPpDEREF_AV|OPpLVAL_INTRO)
		|| ((BINOP*)o)->op_last->op_type != OP_CONST)
		break;
	    rop = (UNOP*)((BINOP*)o)->op_first;
	    if (rop->op_type != OP_RV2HV || rop->op_first->op_type != OP_PADSV)
		break;
	    lexname = *av_fetch(PL_comppad_name, rop->op_first->op_targ, TRUE);
	    if (!SvOBJECT(lexname))
		break;
	    fields = (GV**)hv_fetch(SvSTASH(lexname), "FIELDS", 6, FALSE);
	    if (!fields || !GvHV(*fields))
		break;
	    svp = &((SVOP*)((BINOP*)o)->op_last)->op_sv;
	    key = SvPV(*svp, keylen);
	    indsvp = hv_fetch(GvHV(*fields), key, keylen, FALSE);
	    if (!indsvp) {
		croak("No such field \"%s\" in variable %s of type %s",
		      key, SvPV(lexname, n_a), HvNAME(SvSTASH(lexname)));
	    }
	    ind = SvIV(*indsvp);
	    if (ind < 1)
		croak("Bad index while coercing array into hash");
	    rop->op_type = OP_RV2AV;
	    rop->op_ppaddr = ppaddr[OP_RV2AV];
	    o->op_type = OP_AELEM;
	    o->op_ppaddr = ppaddr[OP_AELEM];
	    SvREFCNT_dec(*svp);
	    *svp = newSViv(ind);
	    break;
	}

	default:
	    o->op_seq = PL_op_seqmax++;
	    break;
	}
	oldop = o;
    }
    LEAVE;
}
