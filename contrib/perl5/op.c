/*    op.c
 *
 *    Copyright (c) 1991-2001, Larry Wall
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
#define PERL_IN_OP_C
#include "perl.h"
#include "keywords.h"

/* #define PL_OP_SLAB_ALLOC */

#ifdef PL_OP_SLAB_ALLOC 
#define SLAB_SIZE 8192
static char    *PL_OpPtr  = NULL;
static int     PL_OpSpace = 0;
#define NewOp(m,var,c,type) do { if ((PL_OpSpace -= c*sizeof(type)) >= 0)     \
                              var =  (type *)(PL_OpPtr -= c*sizeof(type));    \
                             else                                             \
                              var = (type *) Slab_Alloc(m,c*sizeof(type));    \
                           } while (0)

STATIC void *           
S_Slab_Alloc(pTHX_ int m, size_t sz)
{ 
 Newz(m,PL_OpPtr,SLAB_SIZE,char);
 PL_OpSpace = SLAB_SIZE - sz;
 return PL_OpPtr += PL_OpSpace;
}

#else 
#define NewOp(m, var, c, type) Newz(m, var, c, type)
#endif
/*
 * In the following definition, the ", Nullop" is just to make the compiler
 * think the expression is of the right type: croak actually does a Siglongjmp.
 */
#define CHECKOP(type,o) \
    ((PL_op_mask && PL_op_mask[type])					\
     ? ( op_free((OP*)o),					\
	 Perl_croak(aTHX_ "%s trapped by operation mask", PL_op_desc[type]),	\
	 Nullop )						\
     : CALL_FPTR(PL_check[type])(aTHX_ (OP*)o))

#define PAD_MAX 999999999
#define RETURN_UNLIMITED_NUMBER (PERL_INT_MAX / 2)

STATIC char*
S_gv_ename(pTHX_ GV *gv)
{
    STRLEN n_a;
    SV* tmpsv = sv_newmortal();
    gv_efullname3(tmpsv, gv, Nullch);
    return SvPV(tmpsv,n_a);
}

STATIC OP *
S_no_fh_allowed(pTHX_ OP *o)
{
    yyerror(Perl_form(aTHX_ "Missing comma after first argument to %s function",
		 PL_op_desc[o->op_type]));
    return o;
}

STATIC OP *
S_too_few_arguments(pTHX_ OP *o, char *name)
{
    yyerror(Perl_form(aTHX_ "Not enough arguments for %s", name));
    return o;
}

STATIC OP *
S_too_many_arguments(pTHX_ OP *o, char *name)
{
    yyerror(Perl_form(aTHX_ "Too many arguments for %s", name));
    return o;
}

STATIC void
S_bad_type(pTHX_ I32 n, char *t, char *name, OP *kid)
{
    yyerror(Perl_form(aTHX_ "Type of arg %d to %s must be %s (not %s)",
		 (int)n, name, t, PL_op_desc[kid->op_type]));
}

STATIC void
S_no_bareword_allowed(pTHX_ OP *o)
{
    qerror(Perl_mess(aTHX_
		     "Bareword \"%s\" not allowed while \"strict subs\" in use",
		     SvPV_nolen(cSVOPo_sv)));
}

STATIC U8*
S_trlist_upgrade(pTHX_ U8** sp, U8** ep)
{
    U8 *s = *sp;
    U8 *e = *ep;
    U8 *d;

    Newz(801, d, (e - s) * 2, U8);
    *sp = d;

    while (s < e) {
        if (*s < 0x80 || *s == 0xff)
            *d++ = *s++;
	else {
            U8 c = *s++;
            *d++ = ((c >> 6)         | 0xc0);
            *d++ = ((c       & 0x3f) | 0x80);
        }
    }
    *ep = d;
    return *sp;
}
  

/* "register" allocation */

PADOFFSET
Perl_pad_allocmy(pTHX_ char *name)
{
    PADOFFSET off;
    SV *sv;

    if (!(PL_in_my == KEY_our ||
	  isALPHA(name[1]) ||
	  (PL_hints & HINT_UTF8 && UTF8_IS_START(name[1])) ||
	  (name[1] == '_' && (int)strlen(name) > 2)))
    {
	if (!isPRINT(name[1]) || strchr("\t\n\r\f", name[1])) {
	    /* 1999-02-27 mjd@plover.com */
	    char *p;
	    p = strchr(name, '\0');
	    /* The next block assumes the buffer is at least 205 chars
	       long.  At present, it's always at least 256 chars. */
	    if (p-name > 200) {
		strcpy(name+200, "...");
		p = name+199;
	    }
	    else {
		p[1] = '\0';
	    }
	    /* Move everything else down one character */
	    for (; p-name > 2; p--)
		*p = *(p-1);
	    name[2] = toCTRL(name[1]);
	    name[1] = '^';
	}
	yyerror(Perl_form(aTHX_ "Can't use global %s in \"my\"",name));
    }
    if (ckWARN(WARN_MISC) && AvFILLp(PL_comppad_name) >= 0) {
	SV **svp = AvARRAY(PL_comppad_name);
	HV *ourstash = (PL_curstash ? PL_curstash : PL_defstash);
	PADOFFSET top = AvFILLp(PL_comppad_name);
	for (off = top; off > PL_comppad_name_floor; off--) {
	    if ((sv = svp[off])
		&& sv != &PL_sv_undef
		&& (SvIVX(sv) == PAD_MAX || SvIVX(sv) == 0)
		&& (PL_in_my != KEY_our
		    || ((SvFLAGS(sv) & SVpad_OUR) && GvSTASH(sv) == ourstash))
		&& strEQ(name, SvPVX(sv)))
	    {
		Perl_warner(aTHX_ WARN_MISC,
		    "\"%s\" variable %s masks earlier declaration in same %s", 
		    (PL_in_my == KEY_our ? "our" : "my"),
		    name,
		    (SvIVX(sv) == PAD_MAX ? "scope" : "statement"));
		--off;
		break;
	    }
	}
	if (PL_in_my == KEY_our) {
	    do {
		if ((sv = svp[off])
		    && sv != &PL_sv_undef
		    && (SvIVX(sv) == PAD_MAX || SvIVX(sv) == 0)
		    && ((SvFLAGS(sv) & SVpad_OUR) && GvSTASH(sv) == ourstash)
		    && strEQ(name, SvPVX(sv)))
		{
		    Perl_warner(aTHX_ WARN_MISC,
			"\"our\" variable %s redeclared", name);
		    Perl_warner(aTHX_ WARN_MISC,
			"\t(Did you mean \"local\" instead of \"our\"?)\n");
		    break;
		}
	    } while ( off-- > 0 );
	}
    }
    off = pad_alloc(OP_PADSV, SVs_PADMY);
    sv = NEWSV(1102,0);
    sv_upgrade(sv, SVt_PVNV);
    sv_setpv(sv, name);
    if (PL_in_my_stash) {
	if (*name != '$')
	    yyerror(Perl_form(aTHX_ "Can't declare class for non-scalar %s in \"%s\"",
			 name, PL_in_my == KEY_our ? "our" : "my"));
	SvOBJECT_on(sv);
	(void)SvUPGRADE(sv, SVt_PVMG);
	SvSTASH(sv) = (HV*)SvREFCNT_inc(PL_in_my_stash);
	PL_sv_objcount++;
    }
    if (PL_in_my == KEY_our) {
	(void)SvUPGRADE(sv, SVt_PVGV);
	GvSTASH(sv) = (HV*)SvREFCNT_inc(PL_curstash ? (SV*)PL_curstash : (SV*)PL_defstash);
	SvFLAGS(sv) |= SVpad_OUR;
    }
    av_store(PL_comppad_name, off, sv);
    SvNVX(sv) = (NV)PAD_MAX;
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

STATIC PADOFFSET
S_pad_addlex(pTHX_ SV *proto_namesv)
{
    SV *namesv = NEWSV(1103,0);
    PADOFFSET newoff = pad_alloc(OP_PADSV, SVs_PADMY);
    sv_upgrade(namesv, SVt_PVNV);
    sv_setpv(namesv, SvPVX(proto_namesv));
    av_store(PL_comppad_name, newoff, namesv);
    SvNVX(namesv) = (NV)PL_curcop->cop_seq;
    SvIVX(namesv) = PAD_MAX;			/* A ref, intro immediately */
    SvFAKE_on(namesv);				/* A ref, not a real var */
    if (SvFLAGS(proto_namesv) & SVpad_OUR) {	/* An "our" variable */
	SvFLAGS(namesv) |= SVpad_OUR;
	(void)SvUPGRADE(namesv, SVt_PVGV);
	GvSTASH(namesv) = (HV*)SvREFCNT_inc((SV*)GvSTASH(proto_namesv));
    }
    if (SvOBJECT(proto_namesv)) {		/* A typed var */
	SvOBJECT_on(namesv);
	(void)SvUPGRADE(namesv, SVt_PVMG);
	SvSTASH(namesv) = (HV*)SvREFCNT_inc((SV*)SvSTASH(proto_namesv));
	PL_sv_objcount++;
    }
    return newoff;
}

#define FINDLEX_NOSEARCH	1		/* don't search outer contexts */

STATIC PADOFFSET
S_pad_findlex(pTHX_ char *name, PADOFFSET newoff, U32 seq, CV* startcv,
	    I32 cx_ix, I32 saweval, U32 flags)
{
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
		oldpad = (AV*)AvARRAY(curlist)[depth];
		oldsv = *av_fetch(oldpad, off, TRUE);
		if (!newoff) {		/* Not a mere clone operation. */
		    newoff = pad_addlex(sv);
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
				if (CvANON(bcv)) {
				    /* install the missing pad entry in intervening
				     * nested subs and mark them cloneable.
				     * XXX fix pad_foo() to not use globals */
				    AV *ocomppad_name = PL_comppad_name;
				    AV *ocomppad = PL_comppad;
				    SV **ocurpad = PL_curpad;
				    AV *padlist = CvPADLIST(bcv);
				    PL_comppad_name = (AV*)AvARRAY(padlist)[0];
				    PL_comppad = (AV*)AvARRAY(padlist)[1];
				    PL_curpad = AvARRAY(PL_comppad);
				    pad_addlex(sv);
				    PL_comppad_name = ocomppad_name;
				    PL_comppad = ocomppad;
				    PL_curpad = ocurpad;
				    CvCLONE_on(bcv);
				}
				else {
				    if (ckWARN(WARN_CLOSURE)
					&& !CvUNIQUE(bcv) && !CvUNIQUE(cv))
				    {
					Perl_warner(aTHX_ WARN_CLOSURE,
					  "Variable \"%s\" may be unavailable",
					     name);
				    }
				    break;
				}
			    }
			}
		    }
		    else if (!CvUNIQUE(PL_compcv)) {
			if (ckWARN(WARN_CLOSURE) && !SvFAKE(sv) && !CvUNIQUE(cv)
			    && !(SvFLAGS(sv) & SVpad_OUR))
			{
			    Perl_warner(aTHX_ WARN_CLOSURE,
				"Variable \"%s\" will not stay shared", name);
			}
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
	    case OP_DOFILE:
	    case OP_REQUIRE:
		/* require/do must have their own scope */
		return 0;
	    }
	    break;
	case CXt_FORMAT:
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
Perl_pad_findmy(pTHX_ char *name)
{
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
	    if (SvIVX(sv) || SvFLAGS(sv) & SVpad_OUR)
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
Perl_pad_leavemy(pTHX_ I32 fill)
{
    I32 off;
    SV **svp = AvARRAY(PL_comppad_name);
    SV *sv;
    if (PL_min_intro_pending && fill < PL_min_intro_pending) {
	for (off = PL_max_intro_pending; off >= PL_min_intro_pending; off--) {
	    if ((sv = svp[off]) && sv != &PL_sv_undef && ckWARN_d(WARN_INTERNAL))
		Perl_warner(aTHX_ WARN_INTERNAL, "%s never introduced", SvPVX(sv));
	}
    }
    /* "Deintroduce" my variables that are leaving with this scope. */
    for (off = AvFILLp(PL_comppad_name); off > fill; off--) {
	if ((sv = svp[off]) && sv != &PL_sv_undef && SvIVX(sv) == PAD_MAX)
	    SvIVX(sv) = PL_cop_seqmax;
    }
}

PADOFFSET
Perl_pad_alloc(pTHX_ I32 optype, U32 tmptype)
{
    SV *sv;
    I32 retval;

    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_alloc");
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
	    if (!(SvFLAGS(sv) & (SVs_PADTMP|SVs_PADMY)) && !IS_PADGV(sv))
		break;
	}
	retval = PL_padix;
    }
    SvFLAGS(sv) |= tmptype;
    PL_curpad = AvARRAY(PL_comppad);
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log,
			  "0x%"UVxf" Pad 0x%"UVxf" alloc %ld for %s\n",
			  PTR2UV(thr), PTR2UV(PL_curpad),
			  (long) retval, PL_op_name[optype]));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log,
			  "Pad 0x%"UVxf" alloc %ld for %s\n",
			  PTR2UV(PL_curpad),
			  (long) retval, PL_op_name[optype]));
#endif /* USE_THREADS */
    return (PADOFFSET)retval;
}

SV *
Perl_pad_sv(pTHX_ PADOFFSET po)
{
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log,
			  "0x%"UVxf" Pad 0x%"UVxf" sv %"IVdf"\n",
			  PTR2UV(thr), PTR2UV(PL_curpad), (IV)po));
#else
    if (!po)
	Perl_croak(aTHX_ "panic: pad_sv po");
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%"UVxf" sv %"IVdf"\n",
			  PTR2UV(PL_curpad), (IV)po));
#endif /* USE_THREADS */
    return PL_curpad[po];		/* eventually we'll turn this into a macro */
}

void
Perl_pad_free(pTHX_ PADOFFSET po)
{
    if (!PL_curpad)
	return;
    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_free curpad");
    if (!po)
	Perl_croak(aTHX_ "panic: pad_free po");
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log,
			  "0x%"UVxf" Pad 0x%"UVxf" free %"IVdf"\n",
			  PTR2UV(thr), PTR2UV(PL_curpad), (IV)po));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%"UVxf" free %"IVdf"\n",
			  PTR2UV(PL_curpad), (IV)po));
#endif /* USE_THREADS */
    if (PL_curpad[po] && PL_curpad[po] != &PL_sv_undef) {
	SvPADTMP_off(PL_curpad[po]);
#ifdef USE_ITHREADS
	SvREADONLY_off(PL_curpad[po]);	/* could be a freed constant */
#endif
    }
    if ((I32)po < PL_padix)
	PL_padix = po - 1;
}

void
Perl_pad_swipe(pTHX_ PADOFFSET po)
{
    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_swipe curpad");
    if (!po)
	Perl_croak(aTHX_ "panic: pad_swipe po");
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log,
			  "0x%"UVxf" Pad 0x%"UVxf" swipe %"IVdf"\n",
			  PTR2UV(thr), PTR2UV(PL_curpad), (IV)po));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%"UVxf" swipe %"IVdf"\n",
			  PTR2UV(PL_curpad), (IV)po));
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
Perl_pad_reset(pTHX)
{
#ifdef USE_BROKEN_PAD_RESET
    register I32 po;

    if (AvARRAY(PL_comppad) != PL_curpad)
	Perl_croak(aTHX_ "panic: pad_reset curpad");
#ifdef USE_THREADS
    DEBUG_X(PerlIO_printf(Perl_debug_log,
			  "0x%"UVxf" Pad 0x%"UVxf" reset\n",
			  PTR2UV(thr), PTR2UV(PL_curpad)));
#else
    DEBUG_X(PerlIO_printf(Perl_debug_log, "Pad 0x%"UVxf" reset\n",
			  PTR2UV(PL_curpad)));
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
Perl_find_threadsv(pTHX_ const char *name)
{
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
	DEBUG_S(PerlIO_printf(Perl_error_log,
			      "find_threadsv: new SV %p for $%s%c\n",
			      sv, (*name < 32) ? "^" : "",
			      (*name < 32) ? toCTRL(*name) : *name));
    }
    return key;
}
#endif /* USE_THREADS */

/* Destructor */

void
Perl_op_free(pTHX_ OP *o)
{
    register OP *kid, *nextkid;
    OPCODE type;

    if (!o || o->op_seq == (U16)-1)
	return;

    if (o->op_private & OPpREFCOUNTED) {
	switch (o->op_type) {
	case OP_LEAVESUB:
	case OP_LEAVESUBLV:
	case OP_LEAVEEVAL:
	case OP_LEAVE:
	case OP_SCOPE:
	case OP_LEAVEWRITE:
	    OP_REFCNT_LOCK;
	    if (OpREFCNT_dec(o)) {
		OP_REFCNT_UNLOCK;
		return;
	    }
	    OP_REFCNT_UNLOCK;
	    break;
	default:
	    break;
	}
    }

    if (o->op_flags & OPf_KIDS) {
	for (kid = cUNOPo->op_first; kid; kid = nextkid) {
	    nextkid = kid->op_sibling; /* Get before next freeing kid */
	    op_free(kid);
	}
    }
    type = o->op_type;
    if (type == OP_NULL)
	type = o->op_targ;

    /* COP* is not cleared by op_clear() so that we may track line
     * numbers etc even after null() */
    if (type == OP_NEXTSTATE || type == OP_SETSTATE || type == OP_DBSTATE)
	cop_free((COP*)o);

    op_clear(o);

#ifdef PL_OP_SLAB_ALLOC
    if ((char *) o == PL_OpPtr)
     {
     }
#else
    Safefree(o);
#endif
}

STATIC void
S_op_clear(pTHX_ OP *o)
{
    switch (o->op_type) {
    case OP_NULL:	/* Was holding old type, if any. */
    case OP_ENTEREVAL:	/* Was holding hints. */
#ifdef USE_THREADS
    case OP_THREADSV:	/* Was holding index into thr->threadsv AV. */
#endif
	o->op_targ = 0;
	break;
#ifdef USE_THREADS
    case OP_ENTERITER:
	if (!(o->op_flags & OPf_SPECIAL))
	    break;
	/* FALL THROUGH */
#endif /* USE_THREADS */
    default:
	if (!(o->op_flags & OPf_REF)
	    || (PL_check[o->op_type] != MEMBER_TO_FPTR(Perl_ck_ftst)))
	    break;
	/* FALL THROUGH */
    case OP_GVSV:
    case OP_GV:
    case OP_AELEMFAST:
#ifdef USE_ITHREADS
	if (cPADOPo->op_padix > 0) {
	    if (PL_curpad) {
		GV *gv = cGVOPo_gv;
		pad_swipe(cPADOPo->op_padix);
		/* No GvIN_PAD_off(gv) here, because other references may still
		 * exist on the pad */
		SvREFCNT_dec(gv);
	    }
	    cPADOPo->op_padix = 0;
	}
#else
	SvREFCNT_dec(cSVOPo->op_sv);
	cSVOPo->op_sv = Nullsv;
#endif
	break;
    case OP_METHOD_NAMED:
    case OP_CONST:
	SvREFCNT_dec(cSVOPo->op_sv);
	cSVOPo->op_sv = Nullsv;
	break;
    case OP_GOTO:
    case OP_NEXT:
    case OP_LAST:
    case OP_REDO:
	if (o->op_flags & (OPf_SPECIAL|OPf_STACKED|OPf_KIDS))
	    break;
	/* FALL THROUGH */
    case OP_TRANS:
	if (o->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF)) {
	    SvREFCNT_dec(cSVOPo->op_sv);
	    cSVOPo->op_sv = Nullsv;
	}
	else {
	    Safefree(cPVOPo->op_pv);
	    cPVOPo->op_pv = Nullch;
	}
	break;
    case OP_SUBST:
	op_free(cPMOPo->op_pmreplroot);
	goto clear_pmop;
    case OP_PUSHRE:
#ifdef USE_ITHREADS
	if ((PADOFFSET)cPMOPo->op_pmreplroot) {
	    if (PL_curpad) {
		GV *gv = (GV*)PL_curpad[(PADOFFSET)cPMOPo->op_pmreplroot];
		pad_swipe((PADOFFSET)cPMOPo->op_pmreplroot);
		/* No GvIN_PAD_off(gv) here, because other references may still
		 * exist on the pad */
		SvREFCNT_dec(gv);
	    }
	}
#else
	SvREFCNT_dec((SV*)cPMOPo->op_pmreplroot);
#endif
	/* FALL THROUGH */
    case OP_MATCH:
    case OP_QR:
clear_pmop:
	cPMOPo->op_pmreplroot = Nullop;
	ReREFCNT_dec(cPMOPo->op_pmregexp);
	cPMOPo->op_pmregexp = (REGEXP*)NULL;
	break;
    }

    if (o->op_targ > 0) {
	pad_free(o->op_targ);
	o->op_targ = 0;
    }
}

STATIC void
S_cop_free(pTHX_ COP* cop)
{
    Safefree(cop->cop_label);
#ifdef USE_ITHREADS
    Safefree(CopFILE(cop));		/* XXX share in a pvtable? */
    Safefree(CopSTASHPV(cop));		/* XXX share in a pvtable? */
#else
    /* NOTE: COP.cop_stash is not refcounted */
    SvREFCNT_dec(CopFILEGV(cop));
#endif
    if (! specialWARN(cop->cop_warnings))
	SvREFCNT_dec(cop->cop_warnings);
}

STATIC void
S_null(pTHX_ OP *o)
{
    if (o->op_type == OP_NULL)
	return;
    op_clear(o);
    o->op_targ = o->op_type;
    o->op_type = OP_NULL;
    o->op_ppaddr = PL_ppaddr[OP_NULL];
}

/* Contextualizers */

#define LINKLIST(o) ((o)->op_next ? (o)->op_next : linklist((OP*)o))

OP *
Perl_linklist(pTHX_ OP *o)
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
Perl_scalarkids(pTHX_ OP *o)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    scalar(kid);
    }
    return o;
}

STATIC OP *
S_scalarboolean(pTHX_ OP *o)
{
    if (o->op_type == OP_SASSIGN && cBINOPo->op_first->op_type == OP_CONST) {
	if (ckWARN(WARN_SYNTAX)) {
	    line_t oldline = CopLINE(PL_curcop);

	    if (PL_copline != NOLINE)
		CopLINE_set(PL_curcop, PL_copline);
	    Perl_warner(aTHX_ WARN_SYNTAX, "Found = in conditional, should be ==");
	    CopLINE_set(PL_curcop, oldline);
	}
    }
    return scalar(o);
}

OP *
Perl_scalar(pTHX_ OP *o)
{
    OP *kid;

    /* assumes no premature commitment */
    if (!o || (o->op_flags & OPf_WANT) || PL_error_count
	 || o->op_type == OP_RETURN)
    {
	return o;
    }

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
	while ((kid = kid->op_sibling)) {
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
Perl_scalarvoid(pTHX_ OP *o)
{
    OP *kid;
    char* useless = 0;
    SV* sv;
    U8 want;

    if (o->op_type == OP_NEXTSTATE
	|| o->op_type == OP_SETSTATE
	|| o->op_type == OP_DBSTATE
	|| (o->op_type == OP_NULL && (o->op_targ == OP_NEXTSTATE
				      || o->op_targ == OP_SETSTATE
				      || o->op_targ == OP_DBSTATE)))
	PL_curcop = (COP*)o;		/* for warning below */

    /* assumes no premature commitment */
    want = o->op_flags & OPf_WANT;
    if ((want && want != OPf_WANT_SCALAR) || PL_error_count
	 || o->op_type == OP_RETURN)
    {
	return o;
    }

    if ((o->op_private & OPpTARGET_MY)
	&& (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
    {
	return scalar(o);			/* As if inside SASSIGN */
    }
    
    o->op_flags = (o->op_flags & ~OPf_WANT) | OPf_WANT_VOID;

    switch (o->op_type) {
    default:
	if (!(PL_opargs[o->op_type] & OA_FOLDCONST))
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
	if (!(o->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO)))
	    useless = PL_op_desc[o->op_type];
	break;

    case OP_RV2GV:
    case OP_RV2SV:
    case OP_RV2AV:
    case OP_RV2HV:
	if (!(o->op_private & (OPpLVAL_INTRO|OPpOUR_INTRO)) &&
		(!o->op_sibling || o->op_sibling->op_type != OP_READLINE))
	    useless = "a variable";
	break;

    case OP_CONST:
	sv = cSVOPo_sv;
	if (cSVOPo->op_private & OPpCONST_STRICT)
	    no_bareword_allowed(o);
	else {
	    if (ckWARN(WARN_VOID)) {
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
	}
	null(o);		/* don't execute or even remember it */
	break;

    case OP_POSTINC:
	o->op_type = OP_PREINC;		/* pre-increment is faster */
	o->op_ppaddr = PL_ppaddr[OP_PREINC];
	break;

    case OP_POSTDEC:
	o->op_type = OP_PREDEC;		/* pre-decrement is faster */
	o->op_ppaddr = PL_ppaddr[OP_PREDEC];
	break;

    case OP_OR:
    case OP_AND:
    case OP_COND_EXPR:
	for (kid = cUNOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    scalarvoid(kid);
	break;

    case OP_NULL:
	if (o->op_flags & OPf_STACKED)
	    break;
	/* FALL THROUGH */
    case OP_NEXTSTATE:
    case OP_DBSTATE:
    case OP_ENTERTRY:
    case OP_ENTER:
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
	/* FALL THROUGH */
    case OP_SCALAR:
	return scalar(o);
    case OP_SPLIT:
	if ((kid = cLISTOPo->op_first) && kid->op_type == OP_PUSHRE) {
	    if (!kPMOP->op_pmreplroot)
		deprecate("implicit split to @_");
	}
	break;
    }
    if (useless && ckWARN(WARN_VOID))
	Perl_warner(aTHX_ WARN_VOID, "Useless use of %s in void context", useless);
    return o;
}

OP *
Perl_listkids(pTHX_ OP *o)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    list(kid);
    }
    return o;
}

OP *
Perl_list(pTHX_ OP *o)
{
    OP *kid;

    /* assumes no premature commitment */
    if (!o || (o->op_flags & OPf_WANT) || PL_error_count
	 || o->op_type == OP_RETURN)
    {
	return o;
    }

    if ((o->op_private & OPpTARGET_MY)
	&& (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
    {
	return o;				/* As if inside SASSIGN */
    }
    
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
	while ((kid = kid->op_sibling)) {
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
Perl_scalarseq(pTHX_ OP *o)
{
    OP *kid;

    if (o) {
	if (o->op_type == OP_LINESEQ ||
	     o->op_type == OP_SCOPE ||
	     o->op_type == OP_LEAVE ||
	     o->op_type == OP_LEAVETRY)
	{
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
S_modkids(pTHX_ OP *o, I32 type)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    mod(kid, type);
    }
    return o;
}

OP *
Perl_mod(pTHX_ OP *o, I32 type)
{
    OP *kid;
    STRLEN n_a;

    if (!o || PL_error_count)
	return o;

    if ((o->op_private & OPpTARGET_MY)
	&& (PL_opargs[o->op_type] & OA_TARGLEX))/* OPp share the meaning */
    {
	return o;
    }
    
    switch (o->op_type) {
    case OP_UNDEF:
	PL_modcount++;
	return o;
    case OP_CONST:
        if (o->op_private & (OPpCONST_BARE) && 
                !(type == OP_GREPSTART || type == OP_ENTERSUB || type == OP_REFGEN)) {
            SV *sv = ((SVOP*)o)->op_sv;
            GV *gv;

            /* Could be a filehandle */
            if (gv = gv_fetchpv(SvPV_nolen(sv), FALSE, SVt_PVIO)) {
                OP* gvio = newUNOP(OP_RV2GV, 0, newGVOP(OP_GV, 0, gv));
                op_free(o);
                o = gvio;
            } else {
                /* OK, it's a sub */
                OP* enter;
                gv = gv_fetchpv(SvPV_nolen(sv), TRUE, SVt_PVCV);

                enter = newUNOP(OP_ENTERSUB,0, 
                        newUNOP(OP_RV2CV, 0, 
                            newGVOP(OP_GV, 0, gv)
                        ));
                enter->op_private |= OPpLVAL_INTRO;
                op_free(o);
                o = enter;
            }
            break;
        }
	if (!(o->op_private & (OPpCONST_ARYBASE)))
	    goto nomod;
	if (PL_eval_start && PL_eval_start->op_type == OP_CONST) {
	    PL_compiling.cop_arybase = (I32)SvIV(cSVOPx(PL_eval_start)->op_sv);
	    PL_eval_start = 0;
	}
	else if (!type) {
	    SAVEI32(PL_compiling.cop_arybase);
	    PL_compiling.cop_arybase = 0;
	}
	else if (type == OP_REFGEN)
	    goto nomod;
	else
	    Perl_croak(aTHX_ "That use of $[ is unsupported");
	break;
    case OP_STUB:
	if (o->op_flags & OPf_PARENS)
	    break;
	goto nomod;
    case OP_ENTERSUB:
	if ((type == OP_UNDEF || type == OP_REFGEN) &&
	    !(o->op_flags & OPf_STACKED)) {
	    o->op_type = OP_RV2CV;		/* entersub => rv2cv */
	    o->op_ppaddr = PL_ppaddr[OP_RV2CV];
	    assert(cUNOPo->op_first->op_type == OP_NULL);
	    null(((LISTOP*)cUNOPo->op_first)->op_first);/* disable pushmark */
	    break;
	}
	else {				/* lvalue subroutine call */
	    o->op_private |= OPpLVAL_INTRO;
	    PL_modcount = RETURN_UNLIMITED_NUMBER;
	    if (type == OP_GREPSTART || type == OP_ENTERSUB || type == OP_REFGEN) {
		/* Backward compatibility mode: */
		o->op_private |= OPpENTERSUB_INARGS;
		break;
	    }
	    else {                      /* Compile-time error message: */
		OP *kid = cUNOPo->op_first;
		CV *cv;
		OP *okid;

		if (kid->op_type == OP_PUSHMARK)
		    goto skip_kids;
		if (kid->op_type != OP_NULL || kid->op_targ != OP_LIST)
		    Perl_croak(aTHX_
			       "panic: unexpected lvalue entersub "
			       "args: type/targ %ld:%ld",
			       (long)kid->op_type,kid->op_targ);
		kid = kLISTOP->op_first;
	      skip_kids:
		while (kid->op_sibling)
		    kid = kid->op_sibling;
		if (!(kid->op_type == OP_NULL && kid->op_targ == OP_RV2CV)) {
		    /* Indirect call */
		    if (kid->op_type == OP_METHOD_NAMED
			|| kid->op_type == OP_METHOD)
		    {
			UNOP *newop;

			if (kid->op_sibling || kid->op_next != kid) {
			    yyerror("panic: unexpected optree near method call");
			    break;
			}
			
			NewOp(1101, newop, 1, UNOP);
			newop->op_type = OP_RV2CV;
			newop->op_ppaddr = PL_ppaddr[OP_RV2CV];
			newop->op_first = Nullop;
                        newop->op_next = (OP*)newop;
			kid->op_sibling = (OP*)newop;
			newop->op_private |= OPpLVAL_INTRO;
			break;
		    }
		    
		    if (kid->op_type != OP_RV2CV)
			Perl_croak(aTHX_
				   "panic: unexpected lvalue entersub "
				   "entry via type/targ %ld:%ld",
				   (long)kid->op_type,kid->op_targ);
		    kid->op_private |= OPpLVAL_INTRO;
		    break;	/* Postpone until runtime */
		}
		
		okid = kid;		
		kid = kUNOP->op_first;
		if (kid->op_type == OP_NULL && kid->op_targ == OP_RV2SV)
		    kid = kUNOP->op_first;
		if (kid->op_type == OP_NULL)		
		    Perl_croak(aTHX_
			       "Unexpected constant lvalue entersub "
			       "entry via type/targ %ld:%ld",
			       (long)kid->op_type,kid->op_targ);
		if (kid->op_type != OP_GV) {
		    /* Restore RV2CV to check lvalueness */
		  restore_2cv:
		    if (kid->op_next && kid->op_next != kid) { /* Happens? */
			okid->op_next = kid->op_next;
			kid->op_next = okid;
		    }
		    else
			okid->op_next = Nullop;
		    okid->op_type = OP_RV2CV;
		    okid->op_targ = 0;
		    okid->op_ppaddr = PL_ppaddr[OP_RV2CV];
		    okid->op_private |= OPpLVAL_INTRO;
		    break;
		}
		
		cv = GvCV(kGVOP_gv);
		if (!cv) 
		    goto restore_2cv;
		if (CvLVALUE(cv))
		    break;
	    }
	}
	/* FALL THROUGH */
    default:
      nomod:
	/* grep, foreach, subcalls, refgen */
	if (type == OP_GREPSTART || type == OP_ENTERSUB || type == OP_REFGEN)
	    break;
	yyerror(Perl_form(aTHX_ "Can't modify %s in %s",
		     (o->op_type == OP_NULL && (o->op_flags & OPf_SPECIAL)
		      ? "do block"
		      : (o->op_type == OP_ENTERSUB
			? "non-lvalue subroutine call"
			: PL_op_desc[o->op_type])),
		     type ? PL_op_desc[type] : "local"));
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
	    Perl_croak(aTHX_ "Can't localize through a reference");
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS) {
           PL_modcount = RETURN_UNLIMITED_NUMBER;
	    return o;		/* Treat \(@foo) like ordinary list. */
	}
	/* FALL THROUGH */
    case OP_RV2GV:
	if (scalar_mod_type(o, type))
	    goto nomod;
	ref(cUNOPo->op_first, o->op_type);
	/* FALL THROUGH */
    case OP_ASLICE:
    case OP_HSLICE:
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	/* FALL THROUGH */
    case OP_AASSIGN:
    case OP_NEXTSTATE:
    case OP_DBSTATE:
    case OP_CHOMP:
       PL_modcount = RETURN_UNLIMITED_NUMBER;
	break;
    case OP_RV2SV:
	if (!type && cUNOPo->op_first->op_type != OP_GV)
	    Perl_croak(aTHX_ "Can't localize through a reference");
	ref(cUNOPo->op_first, o->op_type);
	/* FALL THROUGH */
    case OP_GV:
    case OP_AV2ARYLEN:
	PL_hints |= HINT_BLOCK_SCOPE;
    case OP_SASSIGN:
    case OP_ANDASSIGN:
    case OP_ORASSIGN:
    case OP_AELEMFAST:
	PL_modcount++;
	break;

    case OP_PADAV:
    case OP_PADHV:
       PL_modcount = RETURN_UNLIMITED_NUMBER;
	if (type == OP_REFGEN && o->op_flags & OPf_PARENS)
	    return o;		/* Treat \(@foo) like ordinary list. */
	if (scalar_mod_type(o, type))
	    goto nomod;
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	/* FALL THROUGH */
    case OP_PADSV:
	PL_modcount++;
	if (!type)
	    Perl_croak(aTHX_ "Can't localize lexical variable %s",
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
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
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
	if (type == OP_LEAVESUBLV)
	    o->op_private |= OPpMAYBE_LVSUB;
	PL_modcount++;
	break;

    case OP_SCOPE:
    case OP_LEAVE:
    case OP_ENTER:
    case OP_LINESEQ:
	if (o->op_flags & OPf_KIDS)
	    mod(cLISTOPo->op_last, type);
	break;

    case OP_NULL:
	if (o->op_flags & OPf_SPECIAL)		/* do BLOCK */
	    goto nomod;
	else if (!(o->op_flags & OPf_KIDS))
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

    case OP_RETURN:
	if (type != OP_LEAVESUBLV)
	    goto nomod;
	break; /* mod()ing was handled by ck_return() */
    }
    if (type != OP_LEAVESUBLV)
        o->op_flags |= OPf_MOD;

    if (type == OP_AASSIGN || type == OP_SASSIGN)
	o->op_flags |= OPf_SPECIAL|OPf_REF;
    else if (!type) {
	o->op_private |= OPpLVAL_INTRO;
	o->op_flags &= ~OPf_SPECIAL;
	PL_hints |= HINT_BLOCK_SCOPE;
    }
    else if (type != OP_GREPSTART && type != OP_ENTERSUB
             && type != OP_LEAVESUBLV)
	o->op_flags |= OPf_REF;
    return o;
}

STATIC bool
S_scalar_mod_type(pTHX_ OP *o, I32 type)
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
    case OP_ANDASSIGN:
    case OP_ORASSIGN:
	return TRUE;
    default:
	return FALSE;
    }
}

STATIC bool
S_is_handle_constructor(pTHX_ OP *o, I32 argnum)
{
    switch (o->op_type) {
    case OP_PIPE_OP:
    case OP_SOCKPAIR:
	if (argnum == 2)
	    return TRUE;
	/* FALL THROUGH */
    case OP_SYSOPEN:
    case OP_OPEN:
    case OP_SELECT:		/* XXX c.f. SelectSaver.pm */
    case OP_SOCKET:
    case OP_OPEN_DIR:
    case OP_ACCEPT:
	if (argnum == 1)
	    return TRUE;
	/* FALL THROUGH */
    default:
	return FALSE;
    }
}

OP *
Perl_refkids(pTHX_ OP *o, I32 type)
{
    OP *kid;
    if (o && o->op_flags & OPf_KIDS) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    ref(kid, type);
    }
    return o;
}

OP *
Perl_ref(pTHX_ OP *o, I32 type)
{
    OP *kid;

    if (!o || PL_error_count)
	return o;

    switch (o->op_type) {
    case OP_ENTERSUB:
	if ((type == OP_EXISTS || type == OP_DEFINED || type == OP_LOCK) &&
	    !(o->op_flags & OPf_STACKED)) {
	    o->op_type = OP_RV2CV;             /* entersub => rv2cv */
	    o->op_ppaddr = PL_ppaddr[OP_RV2CV];
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
	if (type == OP_DEFINED)
	    o->op_flags |= OPf_SPECIAL;		/* don't create GV */
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
	if (type == OP_DEFINED)
	    o->op_flags |= OPf_SPECIAL;		/* don't create GV */
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

STATIC OP *
S_dup_attrlist(pTHX_ OP *o)
{
    OP *rop = Nullop;

    /* An attrlist is either a simple OP_CONST or an OP_LIST with kids,
     * where the first kid is OP_PUSHMARK and the remaining ones
     * are OP_CONST.  We need to push the OP_CONST values.
     */
    if (o->op_type == OP_CONST)
	rop = newSVOP(OP_CONST, o->op_flags, SvREFCNT_inc(cSVOPo->op_sv));
    else {
	assert((o->op_type == OP_LIST) && (o->op_flags & OPf_KIDS));
	for (o = cLISTOPo->op_first; o; o=o->op_sibling) {
	    if (o->op_type == OP_CONST)
		rop = append_elem(OP_LIST, rop,
				  newSVOP(OP_CONST, o->op_flags,
					  SvREFCNT_inc(cSVOPo->op_sv)));
	}
    }
    return rop;
}

STATIC void
S_apply_attrs(pTHX_ HV *stash, SV *target, OP *attrs)
{
    SV *stashsv;

    /* fake up C<use attributes $pkg,$rv,@attrs> */
    ENTER;		/* need to protect against side-effects of 'use' */
    SAVEINT(PL_expect);
    if (stash && HvNAME(stash))
	stashsv = newSVpv(HvNAME(stash), 0);
    else
	stashsv = &PL_sv_no;

#define ATTRSMODULE "attributes"

    Perl_load_module(aTHX_ PERL_LOADMOD_IMPORT_OPS,
		     newSVpvn(ATTRSMODULE, sizeof(ATTRSMODULE)-1),
		     Nullsv,
		     prepend_elem(OP_LIST,
				  newSVOP(OP_CONST, 0, stashsv),
				  prepend_elem(OP_LIST,
					       newSVOP(OP_CONST, 0,
						       newRV(target)),
					       dup_attrlist(attrs))));
    LEAVE;
}

void
Perl_apply_attrs_string(pTHX_ char *stashpv, CV *cv,
                        char *attrstr, STRLEN len)
{
    OP *attrs = Nullop;

    if (!len) {
        len = strlen(attrstr);
    }

    while (len) {
        for (; isSPACE(*attrstr) && len; --len, ++attrstr) ;
        if (len) {
            char *sstr = attrstr;
            for (; !isSPACE(*attrstr) && len; --len, ++attrstr) ;
            attrs = append_elem(OP_LIST, attrs,
                                newSVOP(OP_CONST, 0,
                                        newSVpvn(sstr, attrstr-sstr)));
        }
    }

    Perl_load_module(aTHX_ PERL_LOADMOD_IMPORT_OPS,
                     newSVpvn(ATTRSMODULE, sizeof(ATTRSMODULE)-1),
                     Nullsv, prepend_elem(OP_LIST,
				  newSVOP(OP_CONST, 0, newSVpv(stashpv,0)),
				  prepend_elem(OP_LIST,
					       newSVOP(OP_CONST, 0,
						       newRV((SV*)cv)),
                                               attrs)));
}

STATIC OP *
S_my_kid(pTHX_ OP *o, OP *attrs)
{
    OP *kid;
    I32 type;

    if (!o || PL_error_count)
	return o;

    type = o->op_type;
    if (type == OP_LIST) {
	for (kid = cLISTOPo->op_first; kid; kid = kid->op_sibling)
	    my_kid(kid, attrs);
    } else if (type == OP_UNDEF) {
	return o;
    } else if (type == OP_RV2SV ||	/* "our" declaration */
	       type == OP_RV2AV ||
	       type == OP_RV2HV) { /* XXX does this let anything illegal in? */
	o->op_private |= OPpOUR_INTRO;
	return o;
    } else if (type != OP_PADSV &&
	     type != OP_PADAV &&
	     type != OP_PADHV &&
	     type != OP_PUSHMARK)
    {
	yyerror(Perl_form(aTHX_ "Can't declare %s in \"%s\"",
			  PL_op_desc[o->op_type],
			  PL_in_my == KEY_our ? "our" : "my"));
	return o;
    }
    else if (attrs && type != OP_PUSHMARK) {
	HV *stash;
	SV *padsv;
	SV **namesvp;

	PL_in_my = FALSE;
	PL_in_my_stash = Nullhv;

	/* check for C<my Dog $spot> when deciding package */
	namesvp = av_fetch(PL_comppad_name, o->op_targ, FALSE);
	if (namesvp && *namesvp && SvOBJECT(*namesvp) && HvNAME(SvSTASH(*namesvp)))
	    stash = SvSTASH(*namesvp);
	else
	    stash = PL_curstash;
	padsv = PAD_SV(o->op_targ);
	apply_attrs(stash, padsv, attrs);
    }
    o->op_flags |= OPf_MOD;
    o->op_private |= OPpLVAL_INTRO;
    return o;
}

OP *
Perl_my_attrs(pTHX_ OP *o, OP *attrs)
{
    if (o->op_flags & OPf_PARENS)
	list(o);
    if (attrs)
	SAVEFREEOP(attrs);
    o = my_kid(o, attrs);
    PL_in_my = FALSE;
    PL_in_my_stash = Nullhv;
    return o;
}

OP *
Perl_my(pTHX_ OP *o)
{
    return my_kid(o, Nullop);
}

OP *
Perl_sawparens(pTHX_ OP *o)
{
    if (o)
	o->op_flags |= OPf_PARENS;
    return o;
}

OP *
Perl_bind_match(pTHX_ I32 type, OP *left, OP *right)
{
    OP *o;

    if (ckWARN(WARN_MISC) &&
      (left->op_type == OP_RV2AV ||
       left->op_type == OP_RV2HV ||
       left->op_type == OP_PADAV ||
       left->op_type == OP_PADHV)) {
      char *desc = PL_op_desc[(right->op_type == OP_SUBST ||
                            right->op_type == OP_TRANS)
                           ? right->op_type : OP_MATCH];
      const char *sample = ((left->op_type == OP_RV2AV ||
			     left->op_type == OP_PADAV)
			    ? "@array" : "%hash");
      Perl_warner(aTHX_ WARN_MISC,
             "Applying %s to %s will act on scalar(%s)", 
             desc, sample, sample);
    }

    if (!(right->op_flags & OPf_STACKED) &&
       (right->op_type == OP_MATCH ||
	right->op_type == OP_SUBST ||
	right->op_type == OP_TRANS)) {
	right->op_flags |= OPf_STACKED;
	if (right->op_type != OP_MATCH &&
            ! (right->op_type == OP_TRANS &&
               right->op_private & OPpTRANS_IDENTICAL))
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
Perl_invert(pTHX_ OP *o)
{
    if (!o)
	return o;
    /* XXX need to optimize away NOT NOT here?  Or do we let optimizer do it? */
    return newUNOP(OP_NOT, OPf_SPECIAL, scalar(o));
}

OP *
Perl_scope(pTHX_ OP *o)
{
    if (o) {
	if (o->op_flags & OPf_PARENS || PERLDB_NOOPT || PL_tainting) {
	    o = prepend_elem(OP_LINESEQ, newOP(OP_ENTER, 0), o);
	    o->op_type = OP_LEAVE;
	    o->op_ppaddr = PL_ppaddr[OP_LEAVE];
	}
	else {
	    if (o->op_type == OP_LINESEQ) {
		OP *kid;
		o->op_type = OP_SCOPE;
		o->op_ppaddr = PL_ppaddr[OP_SCOPE];
		kid = ((LISTOP*)o)->op_first;
		if (kid->op_type == OP_NEXTSTATE || kid->op_type == OP_DBSTATE)
		    null(kid);
	    }
	    else
		o = newLISTOP(OP_SCOPE, 0, o, Nullop);
	}
    }
    return o;
}

void
Perl_save_hints(pTHX)
{
    SAVEI32(PL_hints);
    SAVESPTR(GvHV(PL_hintgv));
    GvHV(PL_hintgv) = newHVhv(GvHV(PL_hintgv));
    SAVEFREESV(GvHV(PL_hintgv));
}

int
Perl_block_start(pTHX_ int full)
{
    int retval = PL_savestack_ix;

    SAVEI32(PL_comppad_name_floor);
    PL_comppad_name_floor = AvFILLp(PL_comppad_name);
    if (full)
	PL_comppad_name_fill = PL_comppad_name_floor;
    if (PL_comppad_name_floor < 0)
	PL_comppad_name_floor = 0;
    SAVEI32(PL_min_intro_pending);
    SAVEI32(PL_max_intro_pending);
    PL_min_intro_pending = 0;
    SAVEI32(PL_comppad_name_fill);
    SAVEI32(PL_padix_floor);
    PL_padix_floor = PL_padix;
    PL_pad_reset_pending = FALSE;
    SAVEHINTS();
    PL_hints &= ~HINT_BLOCK_SCOPE;
    SAVESPTR(PL_compiling.cop_warnings); 
    if (! specialWARN(PL_compiling.cop_warnings)) {
        PL_compiling.cop_warnings = newSVsv(PL_compiling.cop_warnings) ;
        SAVEFREESV(PL_compiling.cop_warnings) ;
    }
    return retval;
}

OP*
Perl_block_end(pTHX_ I32 floor, OP *seq)
{
    int needblockscope = PL_hints & HINT_BLOCK_SCOPE;
    OP* retval = scalarseq(seq);
    LEAVE_SCOPE(floor);
    PL_pad_reset_pending = FALSE;
    PL_compiling.op_private = PL_hints;
    if (needblockscope)
	PL_hints |= HINT_BLOCK_SCOPE; /* propagate out */
    pad_leavemy(PL_comppad_name_fill);
    PL_cop_seqmax++;
    return retval;
}

STATIC OP *
S_newDEFSVOP(pTHX)
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
Perl_newPROG(pTHX_ OP *o)
{
    if (PL_in_eval) {
	if (PL_eval_root)
		return;
	PL_eval_root = newUNOP(OP_LEAVEEVAL,
			       ((PL_in_eval & EVAL_KEEPERR)
				? OPf_SPECIAL : 0), o);
	PL_eval_start = linklist(PL_eval_root);
	PL_eval_root->op_private |= OPpREFCOUNTED;
	OpREFCNT_set(PL_eval_root, 1);
	PL_eval_root->op_next = 0;
	peep(PL_eval_start);
    }
    else {
	if (!o)
	    return;
	PL_main_root = scope(sawparens(scalarvoid(o)));
	PL_curcop = &PL_compiling;
	PL_main_start = LINKLIST(PL_main_root);
	PL_main_root->op_private |= OPpREFCOUNTED;
	OpREFCNT_set(PL_main_root, 1);
	PL_main_root->op_next = 0;
	peep(PL_main_start);
	PL_compcv = 0;

	/* Register with debugger */
	if (PERLDB_INTER) {
	    CV *cv = get_cv("DB::postponed", FALSE);
	    if (cv) {
		dSP;
		PUSHMARK(SP);
		XPUSHs((SV*)CopFILEGV(&PL_compiling));
		PUTBACK;
		call_sv((SV*)cv, G_DISCARD);
	    }
	}
    }
}

OP *
Perl_localize(pTHX_ OP *o, I32 lex)
{
    if (o->op_flags & OPf_PARENS)
	list(o);
    else {
	if (ckWARN(WARN_PARENTHESIS) && PL_bufptr > PL_oldbufptr && PL_bufptr[-1] == ',') {
	    char *s;
	    for (s = PL_bufptr; *s && (isALNUM(*s) || UTF8_IS_CONTINUED(*s) || strchr("@$%, ",*s)); s++) ;
	    if (*s == ';' || *s == '=')
		Perl_warner(aTHX_ WARN_PARENTHESIS,
			    "Parentheses missing around \"%s\" list",
			    lex ? (PL_in_my == KEY_our ? "our" : "my") : "local");
	}
    }
    if (lex)
	o = my(o);
    else
	o = mod(o, OP_NULL);		/* a bit kludgey */
    PL_in_my = FALSE;
    PL_in_my_stash = Nullhv;
    return o;
}

OP *
Perl_jmaybe(pTHX_ OP *o)
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
Perl_fold_constants(pTHX_ register OP *o)
{
    register OP *curop;
    I32 type = o->op_type;
    SV *sv;

    if (PL_opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (PL_opargs[type] & OA_TARGET && !o->op_targ)
	o->op_targ = pad_alloc(type, SVs_PADTMP);

    /* integerize op, unless it happens to be C<-foo>.
     * XXX should pp_i_negate() do magic string negation instead? */
    if ((PL_opargs[type] & OA_OTHERINT) && (PL_hints & HINT_INTEGER)
	&& !(type == OP_NEGATE && cUNOPo->op_first->op_type == OP_CONST
	     && (cUNOPo->op_first->op_private & OPpCONST_BARE)))
    {
	o->op_ppaddr = PL_ppaddr[type = ++(o->op_type)];
    }

    if (!(PL_opargs[type] & OA_FOLDCONST))
	goto nope;

    switch (type) {
    case OP_NEGATE:
	/* XXX might want a ck_negate() for this */
	cUNOPo->op_first->op_private &= ~OPpCONST_STRICT;
	break;
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
	if ((curop->op_type != OP_CONST ||
	     (curop->op_private & OPpCONST_BARE)) &&
	    curop->op_type != OP_LIST &&
	    curop->op_type != OP_SCALAR &&
	    curop->op_type != OP_NULL &&
	    curop->op_type != OP_PUSHMARK)
	{
	    goto nope;
	}
    }

    curop = LINKLIST(o);
    o->op_next = 0;
    PL_op = curop;
    CALLRUNOPS(aTHX);
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
	    if ((NV)iv == SvNV(sv)) {
		SvREFCNT_dec(sv);
		sv = newSViv(iv);
	    }
	    else
		SvIOK_off(sv);			/* undo SvIV() damage */
	}
	return newSVOP(OP_CONST, 0, sv);
    }

  nope:
    if (!(PL_opargs[type] & OA_OTHERINT))
	return o;

    if (!(PL_hints & HINT_INTEGER)) {
	if (type == OP_MODULO
	    || type == OP_DIVIDE
	    || !(o->op_flags & OPf_KIDS))
	{
	    return o;
	}

	for (curop = ((UNOP*)o)->op_first; curop; curop = curop->op_sibling) {
	    if (curop->op_type == OP_CONST) {
		if (SvIOK(((SVOP*)curop)->op_sv))
		    continue;
		return o;
	    }
	    if (PL_opargs[curop->op_type] & OA_RETINTEGER)
		continue;
	    return o;
	}
	o->op_ppaddr = PL_ppaddr[++(o->op_type)];
    }

    return o;
}

OP *
Perl_gen_constant_list(pTHX_ register OP *o)
{
    register OP *curop;
    I32 oldtmps_floor = PL_tmps_floor;

    list(o);
    if (PL_error_count)
	return o;		/* Don't attempt to run with errors */

    PL_op = curop = LINKLIST(o);
    o->op_next = 0;
    peep(curop);
    pp_pushmark();
    CALLRUNOPS(aTHX);
    PL_op = curop;
    pp_anonlist();
    PL_tmps_floor = oldtmps_floor;

    o->op_type = OP_RV2AV;
    o->op_ppaddr = PL_ppaddr[OP_RV2AV];
    curop = ((UNOP*)o)->op_first;
    ((UNOP*)o)->op_first = newSVOP(OP_CONST, 0, SvREFCNT_inc(*PL_stack_sp--));
    op_free(curop);
    linklist(o);
    return list(o);
}

OP *
Perl_convert(pTHX_ I32 type, I32 flags, OP *o)
{
    OP *kid;
    OP *last = 0;

    if (!o || o->op_type != OP_LIST)
	o = newLISTOP(OP_LIST, 0, o, Nullop);
    else
	o->op_flags &= ~OPf_WANT;

    if (!(PL_opargs[type] & OA_MARK))
	null(cLISTOPo->op_first);

    o->op_type = type;
    o->op_ppaddr = PL_ppaddr[type];
    o->op_flags |= flags;

    o = CHECKOP(type, o);
    if (o->op_type != type)
	return o;

    return fold_constants(o);
}

/* List constructors */

OP *
Perl_append_elem(pTHX_ I32 type, OP *first, OP *last)
{
    if (!first)
	return last;

    if (!last)
	return first;

    if (first->op_type != type
	|| (type == OP_LIST && (first->op_flags & OPf_PARENS)))
    {
	return newLISTOP(type, 0, first, last);
    }

    if (first->op_flags & OPf_KIDS)
	((LISTOP*)first)->op_last->op_sibling = last;
    else {
	first->op_flags |= OPf_KIDS;
	((LISTOP*)first)->op_first = last;
    }
    ((LISTOP*)first)->op_last = last;
    return first;
}

OP *
Perl_append_list(pTHX_ I32 type, LISTOP *first, LISTOP *last)
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
    first->op_flags |= (last->op_flags & OPf_KIDS);

#ifdef PL_OP_SLAB_ALLOC
#else
    Safefree(last);     
#endif
    return (OP*)first;
}

OP *
Perl_prepend_elem(pTHX_ I32 type, OP *first, OP *last)
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
	last->op_flags |= OPf_KIDS;
	return last;
    }

    return newLISTOP(type, 0, first, last);
}

/* Constructors */

OP *
Perl_newNULLLIST(pTHX)
{
    return newOP(OP_STUB, 0);
}

OP *
Perl_force_list(pTHX_ OP *o)
{
    if (!o || o->op_type != OP_LIST)
	o = newLISTOP(OP_LIST, 0, o, Nullop);
    null(o);
    return o;
}

OP *
Perl_newLISTOP(pTHX_ I32 type, I32 flags, OP *first, OP *last)
{
    LISTOP *listop;

    NewOp(1101, listop, 1, LISTOP);

    listop->op_type = type;
    listop->op_ppaddr = PL_ppaddr[type];
    if (first || last)
	flags |= OPf_KIDS;
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

    return (OP*)listop;
}

OP *
Perl_newOP(pTHX_ I32 type, I32 flags)
{
    OP *o;
    NewOp(1101, o, 1, OP);
    o->op_type = type;
    o->op_ppaddr = PL_ppaddr[type];
    o->op_flags = flags;

    o->op_next = o;
    o->op_private = 0 + (flags >> 8);
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar(o);
    if (PL_opargs[type] & OA_TARGET)
	o->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, o);
}

OP *
Perl_newUNOP(pTHX_ I32 type, I32 flags, OP *first)
{
    UNOP *unop;

    if (!first)
	first = newOP(OP_STUB, 0);
    if (PL_opargs[type] & OA_MARK)
	first = force_list(first);

    NewOp(1101, unop, 1, UNOP);
    unop->op_type = type;
    unop->op_ppaddr = PL_ppaddr[type];
    unop->op_first = first;
    unop->op_flags = flags | OPf_KIDS;
    unop->op_private = 1 | (flags >> 8);
    unop = (UNOP*) CHECKOP(type, unop);
    if (unop->op_next)
	return (OP*)unop;

    return fold_constants((OP *) unop);
}

OP *
Perl_newBINOP(pTHX_ I32 type, I32 flags, OP *first, OP *last)
{
    BINOP *binop;
    NewOp(1101, binop, 1, BINOP);

    if (!first)
	first = newOP(OP_NULL, 0);

    binop->op_type = type;
    binop->op_ppaddr = PL_ppaddr[type];
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
    if (binop->op_next || binop->op_type != type)
	return (OP*)binop;

    binop->op_last = binop->op_first->op_sibling;

    return fold_constants((OP *)binop);
}

static int
utf8compare(const void *a, const void *b)
{
    int i;
    for (i = 0; i < 10; i++) {
	if ((*(U8**)a)[i] < (*(U8**)b)[i])
	    return -1;
	if ((*(U8**)a)[i] > (*(U8**)b)[i])
	    return 1;
    }
    return 0;
}

OP *
Perl_pmtrans(pTHX_ OP *o, OP *expr, OP *repl)
{
    SV *tstr = ((SVOP*)expr)->op_sv;
    SV *rstr = ((SVOP*)repl)->op_sv;
    STRLEN tlen;
    STRLEN rlen;
    U8 *t = (U8*)SvPV(tstr, tlen);
    U8 *r = (U8*)SvPV(rstr, rlen);
    register I32 i;
    register I32 j;
    I32 del;
    I32 complement;
    I32 squash;
    I32 grows = 0;
    register short *tbl;

    PL_hints |= HINT_BLOCK_SCOPE;
    complement	= o->op_private & OPpTRANS_COMPLEMENT;
    del		= o->op_private & OPpTRANS_DELETE;
    squash	= o->op_private & OPpTRANS_SQUASH;
    
    if (SvUTF8(tstr))
        o->op_private |= OPpTRANS_FROM_UTF;
    
    if (SvUTF8(rstr)) 
        o->op_private |= OPpTRANS_TO_UTF;

    if (o->op_private & (OPpTRANS_FROM_UTF|OPpTRANS_TO_UTF)) {
	SV* listsv = newSVpvn("# comment\n",10);
	SV* transv = 0;
	U8* tend = t + tlen;
	U8* rend = r + rlen;
	STRLEN ulen;
	U32 tfirst = 1;
	U32 tlast = 0;
	I32 tdiff;
	U32 rfirst = 1;
	U32 rlast = 0;
	I32 rdiff;
	I32 diff;
	I32 none = 0;
	U32 max = 0;
	I32 bits;
	I32 havefinal = 0;
	U32 final;
	I32 from_utf	= o->op_private & OPpTRANS_FROM_UTF;
	I32 to_utf	= o->op_private & OPpTRANS_TO_UTF;
	U8* tsave = from_utf ? NULL : trlist_upgrade(&t, &tend);
	U8* rsave = to_utf   ? NULL : trlist_upgrade(&r, &rend);

	if (complement) {
	    U8 tmpbuf[UTF8_MAXLEN+1];
	    U8** cp;
	    I32* cl;
	    UV nextmin = 0;
	    New(1109, cp, tlen, U8*);
	    i = 0;
	    transv = newSVpvn("",0);
	    while (t < tend) {
		cp[i++] = t;
		t += UTF8SKIP(t);
		if (t < tend && *t == 0xff) {
		    t++;
		    t += UTF8SKIP(t);
		}
	    }
	    qsort(cp, i, sizeof(U8*), utf8compare);
	    for (j = 0; j < i; j++) {
		U8 *s = cp[j];
		I32 cur = j < i - 1 ? cp[j+1] - s : tend - s;
		UV  val = utf8_to_uv(s, cur, &ulen, 0);
		s += ulen;
		diff = val - nextmin;
		if (diff > 0) {
		    t = uv_to_utf8(tmpbuf,nextmin);
		    sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
		    if (diff > 1) {
			t = uv_to_utf8(tmpbuf, val - 1);
			sv_catpvn(transv, "\377", 1);
			sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
		    }
	        }
		if (s < tend && *s == 0xff)
		    val = utf8_to_uv(s+1, cur - 1, &ulen, 0);
		if (val >= nextmin)
		    nextmin = val + 1;
	    }
	    t = uv_to_utf8(tmpbuf,nextmin);
	    sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
	    t = uv_to_utf8(tmpbuf, 0x7fffffff);
	    sv_catpvn(transv, "\377", 1);
	    sv_catpvn(transv, (char*)tmpbuf, t - tmpbuf);
	    t = (U8*)SvPVX(transv);
	    tlen = SvCUR(transv);
	    tend = t + tlen;
	    Safefree(cp);
	}
	else if (!rlen && !del) {
	    r = t; rlen = tlen; rend = tend;
	}
	if (!squash) {
		if (t == r ||
		    (tlen == rlen && memEQ((char *)t, (char *)r, tlen)))
		{
		    o->op_private |= OPpTRANS_IDENTICAL;
		}
	}

	while (t < tend || tfirst <= tlast) {
	    /* see if we need more "t" chars */
	    if (tfirst > tlast) {
		tfirst = (I32)utf8_to_uv(t, tend - t, &ulen, 0);
		t += ulen;
		if (t < tend && *t == 0xff) {	/* illegal utf8 val indicates range */
		    t++;
		    tlast = (I32)utf8_to_uv(t, tend - t, &ulen, 0);
		    t += ulen;
		}
		else
		    tlast = tfirst;
	    }

	    /* now see if we need more "r" chars */
	    if (rfirst > rlast) {
		if (r < rend) {
		    rfirst = (I32)utf8_to_uv(r, rend - r, &ulen, 0);
		    r += ulen;
		    if (r < rend && *r == 0xff) {	/* illegal utf8 val indicates range */
			r++;
			rlast = (I32)utf8_to_uv(r, rend - r, &ulen, 0);
			r += ulen;
		    }
		    else
			rlast = rfirst;
		}
		else {
		    if (!havefinal++)
			final = rlast;
		    rfirst = rlast = 0xffffffff;
		}
	    }

	    /* now see which range will peter our first, if either. */
	    tdiff = tlast - tfirst;
	    rdiff = rlast - rfirst;

	    if (tdiff <= rdiff)
		diff = tdiff;
	    else
		diff = rdiff;

	    if (rfirst == 0xffffffff) {
		diff = tdiff;	/* oops, pretend rdiff is infinite */
		if (diff > 0)
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t%04lx\tXXXX\n",
				   (long)tfirst, (long)tlast);
		else
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t\tXXXX\n", (long)tfirst);
	    }
	    else {
		if (diff > 0)
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t%04lx\t%04lx\n",
				   (long)tfirst, (long)(tfirst + diff),
				   (long)rfirst);
		else
		    Perl_sv_catpvf(aTHX_ listsv, "%04lx\t\t%04lx\n",
				   (long)tfirst, (long)rfirst);

		if (rfirst + diff > max)
		    max = rfirst + diff;
		rfirst += diff + 1;
		if (!grows)
		    grows = (UNISKIP(tfirst) < UNISKIP(rfirst));
	    }
	    tfirst += diff + 1;
	}

	none = ++max;
	if (del)
	    del = ++max;

	if (max > 0xffff)
	    bits = 32;
	else if (max > 0xff)
	    bits = 16;
	else
	    bits = 8;

	Safefree(cPVOPo->op_pv);
	cSVOPo->op_sv = (SV*)swash_init("utf8", "", listsv, bits, none);
	SvREFCNT_dec(listsv);
	if (transv)
	    SvREFCNT_dec(transv);

	if (!del && havefinal)
	    (void)hv_store((HV*)SvRV((cSVOPo->op_sv)), "FINAL", 5,
			   newSVuv((UV)final), 0);

	if (grows)
	    o->op_private |= OPpTRANS_GROWS;

	if (tsave)
	    Safefree(tsave);
	if (rsave)
	    Safefree(rsave);

	op_free(expr);
	op_free(repl);
	return o;
    }

    tbl = (short*)cPVOPo->op_pv;
    if (complement) {
	Zero(tbl, 256, short);
	for (i = 0; i < tlen; i++)
	    tbl[t[i]] = -1;
	for (i = 0, j = 0; i < 256; i++) {
	    if (!tbl[i]) {
		if (j >= rlen) {
		    if (del)
			tbl[i] = -2;
		    else if (rlen)
			tbl[i] = r[j-1];
		    else
			tbl[i] = i;
		}
		else {
		    if (i < 128 && r[j] >= 128)
			grows = 1;
		    tbl[i] = r[j++];
		}
	    }
	}
    }
    else {
	if (!rlen && !del) {
	    r = t; rlen = tlen;
	    if (!squash)
		o->op_private |= OPpTRANS_IDENTICAL;
	}
	for (i = 0; i < 256; i++)
	    tbl[i] = -1;
	for (i = 0, j = 0; i < tlen; i++,j++) {
	    if (j >= rlen) {
		if (del) {
		    if (tbl[t[i]] == -1)
			tbl[t[i]] = -2;
		    continue;
		}
		--j;
	    }
	    if (tbl[t[i]] == -1) {
		if (t[i] < 128 && r[j] >= 128)
		    grows = 1;
		tbl[t[i]] = r[j];
	    }
	}
    }
    if (grows)
	o->op_private |= OPpTRANS_GROWS;
    op_free(expr);
    op_free(repl);

    return o;
}

OP *
Perl_newPMOP(pTHX_ I32 type, I32 flags)
{
    PMOP *pmop;

    NewOp(1101, pmop, 1, PMOP);
    pmop->op_type = type;
    pmop->op_ppaddr = PL_ppaddr[type];
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
Perl_pmruntime(pTHX_ OP *o, OP *expr, OP *repl)
{
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
	if ((PL_hints & HINT_UTF8) || DO_UTF8(pat))
	    pm->op_pmdynflags |= PMdf_UTF8;
	pm->op_pmregexp = CALLREGCOMP(aTHX_ p, p + plen, pm);
	if (strEQ("\\s+", pm->op_pmregexp->precomp))
	    pm->op_pmflags |= PMf_WHITE;
	op_free(expr);
    }
    else {
	if (PL_hints & HINT_UTF8)
	    pm->op_pmdynflags |= PMdf_UTF8;
	if (pm->op_pmflags & PMf_KEEP || !(PL_hints & HINT_RE_EVAL))
	    expr = newUNOP((!(PL_hints & HINT_RE_EVAL) 
			    ? OP_REGCRESET
			    : OP_REGCMAYBE),0,expr);

	NewOp(1101, rcop, 1, LOGOP);
	rcop->op_type = OP_REGCOMP;
	rcop->op_ppaddr = PL_ppaddr[OP_REGCOMP];
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
	    if (CopLINE(PL_curcop) < PL_multi_end)
		CopLINE_set(PL_curcop, PL_multi_end);
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
		if (PL_opargs[curop->op_type] & OA_DANGEROUS) {
#ifdef USE_THREADS
		    if (curop->op_type == OP_THREADSV) {
			repl_has_vars = 1;
			if (strchr("&`'123456789+", curop->op_private))
			    break;
		    }
#else
		    if (curop->op_type == OP_GV) {
			GV *gv = cGVOPx_gv(curop);
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
	    NewOp(1101, rcop, 1, LOGOP);
	    rcop->op_type = OP_SUBSTCONT;
	    rcop->op_ppaddr = PL_ppaddr[OP_SUBSTCONT];
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
Perl_newSVOP(pTHX_ I32 type, I32 flags, SV *sv)
{
    SVOP *svop;
    NewOp(1101, svop, 1, SVOP);
    svop->op_type = type;
    svop->op_ppaddr = PL_ppaddr[type];
    svop->op_sv = sv;
    svop->op_next = (OP*)svop;
    svop->op_flags = flags;
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP*)svop);
    if (PL_opargs[type] & OA_TARGET)
	svop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, svop);
}

OP *
Perl_newPADOP(pTHX_ I32 type, I32 flags, SV *sv)
{
    PADOP *padop;
    NewOp(1101, padop, 1, PADOP);
    padop->op_type = type;
    padop->op_ppaddr = PL_ppaddr[type];
    padop->op_padix = pad_alloc(type, SVs_PADTMP);
    SvREFCNT_dec(PL_curpad[padop->op_padix]);
    PL_curpad[padop->op_padix] = sv;
    SvPADTMP_on(sv);
    padop->op_next = (OP*)padop;
    padop->op_flags = flags;
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP*)padop);
    if (PL_opargs[type] & OA_TARGET)
	padop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, padop);
}

OP *
Perl_newGVOP(pTHX_ I32 type, I32 flags, GV *gv)
{
#ifdef USE_ITHREADS
    GvIN_PAD_on(gv);
    return newPADOP(type, flags, SvREFCNT_inc(gv));
#else
    return newSVOP(type, flags, SvREFCNT_inc(gv));
#endif
}

OP *
Perl_newPVOP(pTHX_ I32 type, I32 flags, char *pv)
{
    PVOP *pvop;
    NewOp(1101, pvop, 1, PVOP);
    pvop->op_type = type;
    pvop->op_ppaddr = PL_ppaddr[type];
    pvop->op_pv = pv;
    pvop->op_next = (OP*)pvop;
    pvop->op_flags = flags;
    if (PL_opargs[type] & OA_RETSCALAR)
	scalar((OP*)pvop);
    if (PL_opargs[type] & OA_TARGET)
	pvop->op_targ = pad_alloc(type, SVs_PADTMP);
    return CHECKOP(type, pvop);
}

void
Perl_package(pTHX_ OP *o)
{
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
Perl_utilize(pTHX_ int aver, I32 floor, OP *version, OP *id, OP *arg)
{
    OP *pack;
    OP *rqop;
    OP *imop;
    OP *veop;
    GV *gv;

    if (id->op_type != OP_CONST)
	Perl_croak(aTHX_ "Module name must be constant");

    veop = Nullop;

    if (version != Nullop) {
	SV *vesv = ((SVOP*)version)->op_sv;

	if (arg == Nullop && !SvNIOKp(vesv)) {
	    arg = version;
	}
	else {
	    OP *pack;
	    SV *meth;

	    if (version->op_type != OP_CONST || !SvNIOKp(vesv))
		Perl_croak(aTHX_ "Version number must be constant number");

	    /* Make copy of id so we don't free it twice */
	    pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)id)->op_sv));

	    /* Fake up a method call to VERSION */
	    meth = newSVpvn("VERSION",7);
	    sv_upgrade(meth, SVt_PVIV);
	    (void)SvIOK_on(meth);
	    PERL_HASH(SvUVX(meth), SvPVX(meth), SvCUR(meth));
	    veop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
			    append_elem(OP_LIST,
					prepend_elem(OP_LIST, pack, list(version)),
					newSVOP(OP_METHOD_NAMED, 0, meth)));
	}
    }

    /* Fake up an import/unimport */
    if (arg && arg->op_type == OP_STUB)
	imop = arg;		/* no import on explicit () */
    else if (SvNIOKp(((SVOP*)id)->op_sv)) {
	imop = Nullop;		/* use 5.0; */
    }
    else {
	SV *meth;

	/* Make copy of id so we don't free it twice */
	pack = newSVOP(OP_CONST, 0, newSVsv(((SVOP*)id)->op_sv));

	/* Fake up a method call to import/unimport */
	meth = aver ? newSVpvn("import",6) : newSVpvn("unimport", 8);;
	sv_upgrade(meth, SVt_PVIV);
	(void)SvIOK_on(meth);
	PERL_HASH(SvUVX(meth), SvPVX(meth), SvCUR(meth));
	imop = convert(OP_ENTERSUB, OPf_STACKED|OPf_SPECIAL,
		       append_elem(OP_LIST,
				   prepend_elem(OP_LIST, pack, list(arg)),
				   newSVOP(OP_METHOD_NAMED, 0, meth)));
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
    newATTRSUB(floor,
	newSVOP(OP_CONST, 0, newSVpvn("BEGIN", 5)),
	Nullop,
	Nullop,
	append_elem(OP_LINESEQ,
	    append_elem(OP_LINESEQ,
	        newSTATEOP(0, Nullch, rqop),
	        newSTATEOP(0, Nullch, veop)),
	    newSTATEOP(0, Nullch, imop) ));

    PL_hints |= HINT_BLOCK_SCOPE;
    PL_copline = NOLINE;
    PL_expect = XSTATE;
}

void
Perl_load_module(pTHX_ U32 flags, SV *name, SV *ver, ...)
{
    va_list args;
    va_start(args, ver);
    vload_module(flags, name, ver, &args);
    va_end(args);
}

#ifdef PERL_IMPLICIT_CONTEXT
void
Perl_load_module_nocontext(U32 flags, SV *name, SV *ver, ...)
{
    dTHX;
    va_list args;
    va_start(args, ver);
    vload_module(flags, name, ver, &args);
    va_end(args);
}
#endif

void
Perl_vload_module(pTHX_ U32 flags, SV *name, SV *ver, va_list *args)
{
    OP *modname, *veop, *imop;

    modname = newSVOP(OP_CONST, 0, name);
    modname->op_private |= OPpCONST_BARE;
    if (ver) {
	veop = newSVOP(OP_CONST, 0, ver);
    }
    else
	veop = Nullop;
    if (flags & PERL_LOADMOD_NOIMPORT) {
	imop = sawparens(newNULLLIST());
    }
    else if (flags & PERL_LOADMOD_IMPORT_OPS) {
	imop = va_arg(*args, OP*);
    }
    else {
	SV *sv;
	imop = Nullop;
	sv = va_arg(*args, SV*);
	while (sv) {
	    imop = append_elem(OP_LIST, imop, newSVOP(OP_CONST, 0, sv));
	    sv = va_arg(*args, SV*);
	}
    }
    {
	line_t ocopline = PL_copline;
	int oexpect = PL_expect;

	utilize(!(flags & PERL_LOADMOD_DENY), start_subparse(FALSE, 0),
		veop, modname, imop);
	PL_expect = oexpect;
	PL_copline = ocopline;
    }
}

OP *
Perl_dofile(pTHX_ OP *term)
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
Perl_newSLICEOP(pTHX_ I32 flags, OP *subscript, OP *listval)
{
    return newBINOP(OP_LSLICE, flags,
	    list(force_list(subscript)),
	    list(force_list(listval)) );
}

STATIC I32
S_list_assignment(pTHX_ register OP *o)
{
    if (!o)
	return TRUE;

    if (o->op_type == OP_NULL && o->op_flags & OPf_KIDS)
	o = cUNOPo->op_first;

    if (o->op_type == OP_COND_EXPR) {
	I32 t = list_assignment(cLOGOPo->op_first->op_sibling);
	I32 f = list_assignment(cLOGOPo->op_first->op_sibling->op_sibling);

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
Perl_newASSIGNOP(pTHX_ I32 flags, OP *left, I32 optype, OP *right)
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
	OP *curop;

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
	curop = list(force_list(left));
	o = newBINOP(OP_AASSIGN, flags, list(force_list(right)), curop);
	o->op_private = 0 | (flags >> 8);
	for (curop = ((LISTOP*)curop)->op_first;
	     curop; curop = curop->op_sibling)
	{
	    if (curop->op_type == OP_RV2HV &&
		((UNOP*)curop)->op_first->op_type != OP_GV) {
		o->op_private |= OPpASSIGN_HASH;
		break;
	    }
	}
	if (!(left->op_private & OPpLVAL_INTRO)) {
	    OP *lastop = o;
	    PL_generation++;
	    for (curop = LINKLIST(o); curop != o; curop = LINKLIST(curop)) {
		if (PL_opargs[curop->op_type] & OA_DANGEROUS) {
		    if (curop->op_type == OP_GV) {
			GV *gv = cGVOPx_gv(curop);
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
#ifdef USE_ITHREADS
			    GV *gv = (GV*)PL_curpad[(PADOFFSET)((PMOP*)curop)->op_pmreplroot];
#else
			    GV *gv = (GV*)((PMOP*)curop)->op_pmreplroot;
#endif
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
		o->op_private |= OPpASSIGN_COMMON;
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
#ifdef USE_ITHREADS
			pm->op_pmreplroot = (OP*)cPADOPx(tmpop)->op_padix;
			cPADOPx(tmpop)->op_padix = 0;	/* steal it */
#else
			pm->op_pmreplroot = (OP*)cSVOPx(tmpop)->op_sv;
			cSVOPx(tmpop)->op_sv = Nullsv;	/* steal it */
#endif
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
                   if (PL_modcount < RETURN_UNLIMITED_NUMBER &&
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
Perl_newSTATEOP(pTHX_ I32 flags, char *label, OP *o)
{
    U32 seq = intro_my();
    register COP *cop;

    NewOp(1101, cop, 1, COP);
    if (PERLDB_LINE && CopLINE(PL_curcop) && PL_curstash != PL_debstash) {
	cop->op_type = OP_DBSTATE;
	cop->op_ppaddr = PL_ppaddr[ OP_DBSTATE ];
    }
    else {
	cop->op_type = OP_NEXTSTATE;
	cop->op_ppaddr = PL_ppaddr[ OP_NEXTSTATE ];
    }
    cop->op_flags = flags;
    cop->op_private = (PL_hints & HINT_BYTE);
#ifdef NATIVE_HINTS
    cop->op_private |= NATIVE_HINTS;
#endif
    PL_compiling.op_private = cop->op_private;
    cop->op_next = (OP*)cop;

    if (label) {
	cop->cop_label = label;
	PL_hints |= HINT_BLOCK_SCOPE;
    }
    cop->cop_seq = seq;
    cop->cop_arybase = PL_curcop->cop_arybase;
    if (specialWARN(PL_curcop->cop_warnings))
        cop->cop_warnings = PL_curcop->cop_warnings ;
    else 
        cop->cop_warnings = newSVsv(PL_curcop->cop_warnings) ;


    if (PL_copline == NOLINE)
        CopLINE_set(cop, CopLINE(PL_curcop));
    else {
	CopLINE_set(cop, PL_copline);
        PL_copline = NOLINE;
    }
#ifdef USE_ITHREADS
    CopFILE_set(cop, CopFILE(PL_curcop));	/* XXX share in a pvtable? */
#else
    CopFILEGV_set(cop, CopFILEGV(PL_curcop));
#endif
    CopSTASH_set(cop, PL_curstash);

    if (PERLDB_LINE && PL_curstash != PL_debstash) {
	SV **svp = av_fetch(CopFILEAV(PL_curcop), (I32)CopLINE(cop), FALSE);
	if (svp && *svp != &PL_sv_undef && !SvIOK(*svp)) {
	    (void)SvIOK_on(*svp);
	    SvIVX(*svp) = PTR2IV(cop);
	}
    }

    return prepend_elem(OP_LINESEQ, (OP*)cop, o);
}

/* "Introduce" my variables to visible status. */
U32
Perl_intro_my(pTHX)
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
	    SvNVX(sv) = (NV)PL_cop_seqmax;
	}
    }
    PL_min_intro_pending = 0;
    PL_comppad_name_fill = PL_max_intro_pending;	/* Needn't search higher */
    return PL_cop_seqmax++;
}

OP *
Perl_newLOGOP(pTHX_ I32 type, I32 flags, OP *first, OP *other)
{
    return new_logop(type, flags, &first, &other);
}

STATIC OP *
S_new_logop(pTHX_ I32 type, I32 flags, OP** firstp, OP** otherp)
{
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
	if (ckWARN(WARN_BAREWORD) && (first->op_private & OPpCONST_BARE))
	    Perl_warner(aTHX_ WARN_BAREWORD, "Bareword found in conditional"); 
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
    else if (ckWARN(WARN_MISC) && (first->op_flags & OPf_KIDS)) {
	OP *k1 = ((UNOP*)first)->op_first;
	OP *k2 = k1->op_sibling;
	OPCODE warnop = 0;
	switch (first->op_type)
	{
	case OP_NULL:
	    if (k2 && k2->op_type == OP_READLINE
		  && (k2->op_flags & OPf_STACKED)
		  && ((k1->op_flags & OPf_WANT) == OPf_WANT_SCALAR)) 
	    {
		warnop = k2->op_type;
	    }
	    break;

	case OP_SASSIGN:
	    if (k1->op_type == OP_READDIR
		  || k1->op_type == OP_GLOB
		  || (k1->op_type == OP_NULL && k1->op_targ == OP_GLOB)
		  || k1->op_type == OP_EACH)
	    {
		warnop = ((k1->op_type == OP_NULL)
			  ? k1->op_targ : k1->op_type);
	    }
	    break;
	}
	if (warnop) {
	    line_t oldline = CopLINE(PL_curcop);
	    CopLINE_set(PL_curcop, PL_copline);
	    Perl_warner(aTHX_ WARN_MISC,
		 "Value of %s%s can be \"0\"; test with defined()",
		 PL_op_desc[warnop],
		 ((warnop == OP_READLINE || warnop == OP_GLOB)
		  ? " construct" : "() operator"));
	    CopLINE_set(PL_curcop, oldline);
	}
    }

    if (!other)
	return first;

    if (type == OP_ANDASSIGN || type == OP_ORASSIGN)
	other->op_private |= OPpASSIGN_BACKWARDS;  /* other is an OP_SASSIGN */

    NewOp(1101, logop, 1, LOGOP);

    logop->op_type = type;
    logop->op_ppaddr = PL_ppaddr[type];
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
Perl_newCONDOP(pTHX_ I32 flags, OP *first, OP *trueop, OP *falseop)
{
    LOGOP *logop;
    OP *start;
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
    NewOp(1101, logop, 1, LOGOP);
    logop->op_type = OP_COND_EXPR;
    logop->op_ppaddr = PL_ppaddr[OP_COND_EXPR];
    logop->op_first = first;
    logop->op_flags = flags | OPf_KIDS;
    logop->op_private = 1 | (flags >> 8);
    logop->op_other = LINKLIST(trueop);
    logop->op_next = LINKLIST(falseop);


    /* establish postfix order */
    start = LINKLIST(first);
    first->op_next = (OP*)logop;

    first->op_sibling = trueop;
    trueop->op_sibling = falseop;
    o = newUNOP(OP_NULL, 0, (OP*)logop);

    trueop->op_next = falseop->op_next = o;

    o->op_next = start;
    return o;
}

OP *
Perl_newRANGE(pTHX_ I32 flags, OP *left, OP *right)
{
    LOGOP *range;
    OP *flip;
    OP *flop;
    OP *leftstart;
    OP *o;

    NewOp(1101, range, 1, LOGOP);

    range->op_type = OP_RANGE;
    range->op_ppaddr = PL_ppaddr[OP_RANGE];
    range->op_first = left;
    range->op_flags = OPf_KIDS;
    leftstart = LINKLIST(left);
    range->op_other = LINKLIST(right);
    range->op_private = 1 | (flags >> 8);

    left->op_sibling = right;

    range->op_next = (OP*)range;
    flip = newUNOP(OP_FLIP, flags, (OP*)range);
    flop = newUNOP(OP_FLOP, 0, flip);
    o = newUNOP(OP_NULL, 0, flop);
    linklist(flop);
    range->op_next = leftstart;

    left->op_next = flip;
    right->op_next = flop;

    range->op_targ = pad_alloc(OP_RANGE, SVs_PADMY);
    sv_upgrade(PAD_SV(range->op_targ), SVt_PVNV);
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
Perl_newLOOPOP(pTHX_ I32 flags, I32 debuggable, OP *expr, OP *block)
{
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
		      || (k1->op_type == OP_NULL && k1->op_targ == OP_NULL)
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
Perl_newWHILEOP(pTHX_ I32 flags, I32 debuggable, LOOP *loop, I32 whileline, OP *expr, OP *block, OP *cont)
{
    OP *redo;
    OP *next = 0;
    OP *listop;
    OP *o;
    OP *condop;
    U8 loopflags = 0;

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
		  || (k1->op_type == OP_NULL && k1->op_targ == OP_GLOB)
		  || k1->op_type == OP_EACH)
		expr = newUNOP(OP_DEFINED, 0, expr);
	    break;
	}
    }

    if (!block)
	block = newOP(OP_NULL, 0);
    else if (cont) {
	block = scope(block);
    }

    if (cont) {
	next = LINKLIST(cont);
    }
    if (expr) {
	OP *unstack = newOP(OP_UNSTACK, 0);
	if (!next)
	    next = unstack;
	cont = append_elem(OP_LINESEQ, cont, unstack);
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
    }
    else
	o = listop;

    if (!loop) {
	NewOp(1101,loop,1,LOOP);
	loop->op_type = OP_ENTERLOOP;
	loop->op_ppaddr = PL_ppaddr[OP_ENTERLOOP];
	loop->op_private = 0;
	loop->op_next = (OP*)loop;
    }

    o = newBINOP(OP_LEAVELOOP, 0, (OP*)loop, o);

    loop->op_redoop = redo;
    loop->op_lastop = o;
    o->op_private |= loopflags;

    if (next)
	loop->op_nextop = next;
    else
	loop->op_nextop = o;

    o->op_flags |= flags;
    o->op_private |= (flags >> 8);
    return o;
}

OP *
Perl_newFOROP(pTHX_ I32 flags,char *label,line_t forline,OP *sv,OP *expr,OP *block,OP *cont)
{
    LOOP *loop;
    OP *wop;
    int padoff = 0;
    I32 iterflags = 0;

    if (sv) {
	if (sv->op_type == OP_RV2SV) {	/* symbol table variable */
	    sv->op_type = OP_RV2GV;
	    sv->op_ppaddr = PL_ppaddr[OP_RV2GV];
	}
	else if (sv->op_type == OP_PADSV) { /* private variable */
	    padoff = sv->op_targ;
	    sv->op_targ = 0;
	    op_free(sv);
	    sv = Nullop;
	}
	else if (sv->op_type == OP_THREADSV) { /* per-thread variable */
	    padoff = sv->op_targ;
	    sv->op_targ = 0;
	    iterflags |= OPf_SPECIAL;
	    op_free(sv);
	    sv = Nullop;
	}
	else
	    Perl_croak(aTHX_ "Can't use %s for loop variable", PL_op_desc[sv->op_type]);
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
	LOGOP* range = (LOGOP*) flip->op_first;
	OP* left  = range->op_first;
	OP* right = left->op_sibling;
	LISTOP* listop;

	range->op_flags &= ~OPf_KIDS;
	range->op_first = Nullop;

	listop = (LISTOP*)newLISTOP(OP_LIST, 0, left, right);
	listop->op_first->op_next = range->op_next;
	left->op_next = range->op_other;
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
#ifdef PL_OP_SLAB_ALLOC
    {
	LOOP *tmp;
	NewOp(1234,tmp,1,LOOP);
	Copy(loop,tmp,1,LOOP);
	loop = tmp;
    }
#else
    Renew(loop, 1, LOOP);
#endif 
    loop->op_targ = padoff;
    wop = newWHILEOP(flags, 1, loop, forline, newOP(OP_ITER, 0), block, cont);
    PL_copline = forline;
    return newSTATEOP(0, label, wop);
}

OP*
Perl_newLOOPEX(pTHX_ I32 type, OP *label)
{
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
Perl_cv_undef(pTHX_ CV *cv)
{
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
	    Perl_croak(aTHX_ "Can't undef active subroutine");
#else
	if (CvDEPTH(cv))
	    Perl_croak(aTHX_ "Can't undef active subroutine");
#endif /* USE_THREADS */
	ENTER;

	SAVEVPTR(PL_curpad);
	PL_curpad = 0;

	op_free(CvROOT(cv));
	CvROOT(cv) = Nullop;
	LEAVE;
    }
    SvPOK_off((SV*)cv);		/* forget prototype */
    CvGV(cv) = Nullgv;
    /* Since closure prototypes have the same lifetime as the containing
     * CV, they don't hold a refcount on the outside CV.  This avoids
     * the refcount loop between the outer CV (which keeps a refcount to
     * the closure prototype in the pad entry for pp_anoncode()) and the
     * closure prototype, and the ensuing memory leak.  --GSAR */
    if (!CvANON(cv) || CvCLONED(cv))
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
    CvFLAGS(cv) = 0;
}

STATIC void
S_cv_dump(pTHX_ CV *cv)
{
#ifdef DEBUGGING
    CV *outside = CvOUTSIDE(cv);
    AV* padlist = CvPADLIST(cv);
    AV* pad_name;
    AV* pad;
    SV** pname;
    SV** ppad;
    I32 ix;

    PerlIO_printf(Perl_debug_log,
		  "\tCV=0x%"UVxf" (%s), OUTSIDE=0x%"UVxf" (%s)\n",
		  PTR2UV(cv),
		  (CvANON(cv) ? "ANON"
		   : (cv == PL_main_cv) ? "MAIN"
		   : CvUNIQUE(cv) ? "UNIQUE"
		   : CvGV(cv) ? GvNAME(CvGV(cv)) : "UNDEFINED"),
		  PTR2UV(outside),
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
	    PerlIO_printf(Perl_debug_log,
			  "\t%4d. 0x%"UVxf" (%s\"%s\" %"IVdf"-%"IVdf")\n",
			  (int)ix, PTR2UV(ppad[ix]),
			  SvFAKE(pname[ix]) ? "FAKE " : "",
			  SvPVX(pname[ix]),
			  (IV)I_32(SvNVX(pname[ix])),
			  SvIVX(pname[ix]));
    }
#endif /* DEBUGGING */
}

STATIC CV *
S_cv_clone2(pTHX_ CV *proto, CV *outside)
{
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
    SAVECOMPPAD();
    SAVESPTR(PL_comppad_name);
    SAVESPTR(PL_compcv);

    cv = PL_compcv = (CV*)NEWSV(1104,0);
    sv_upgrade((SV *)cv, SvTYPE(proto));
    CvFLAGS(cv) = CvFLAGS(proto) & ~CVf_CLONE;
    CvCLONED_on(cv);

#ifdef USE_THREADS
    New(666, CvMUTEXP(cv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(cv));
    CvOWNER(cv)		= 0;
#endif /* USE_THREADS */
    CvFILE(cv)		= CvFILE(proto);
    CvGV(cv)		= CvGV(proto);
    CvSTASH(cv)		= CvSTASH(proto);
    CvROOT(cv)		= OpREFCNT_inc(CvROOT(proto));
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
		    Perl_croak(aTHX_ "panic: cv_clone: %s", name);
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
	else if (IS_PADGV(ppad[ix]) || IS_PADCONST(ppad[ix])) {
	    PL_curpad[ix] = SvREFCNT_inc(ppad[ix]);
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
Perl_cv_clone(pTHX_ CV *proto)
{
    CV *cv;
    LOCK_CRED_MUTEX;			/* XXX create separate mutex */
    cv = cv_clone2(proto, CvOUTSIDE(proto));
    UNLOCK_CRED_MUTEX;			/* XXX create separate mutex */
    return cv;
}

void
Perl_cv_ckproto(pTHX_ CV *cv, GV *gv, char *p)
{
    if (((!p != !SvPOK(cv)) || (p && strNE(p, SvPVX(cv)))) && ckWARN_d(WARN_PROTOTYPE)) {
	SV* msg = sv_newmortal();
	SV* name = Nullsv;

	if (gv)
	    gv_efullname3(name = sv_newmortal(), gv, Nullch);
	sv_setpv(msg, "Prototype mismatch:");
	if (name)
	    Perl_sv_catpvf(aTHX_ msg, " sub %"SVf, name);
	if (SvPOK(cv))
	    Perl_sv_catpvf(aTHX_ msg, " (%s)", SvPVX(cv));
	sv_catpv(msg, " vs ");
	if (p)
	    Perl_sv_catpvf(aTHX_ msg, "(%s)", p);
	else
	    sv_catpv(msg, "none");
	Perl_warner(aTHX_ WARN_PROTOTYPE, "%"SVf, msg);
    }
}

SV *
Perl_cv_const_sv(pTHX_ CV *cv)
{
    if (!cv || !SvPOK(cv) || SvCUR(cv))
	return Nullsv;
    return op_const_sv(CvSTART(cv), cv);
}

SV *
Perl_op_const_sv(pTHX_ OP *o, CV *cv)
{
    SV *sv = Nullsv;

    if (!o)
	return Nullsv;
 
    if (o->op_type == OP_LINESEQ && cLISTOPo->op_first) 
	o = cLISTOPo->op_first->op_sibling;

    for (; o; o = o->op_next) {
	OPCODE type = o->op_type;

	if (sv && o->op_next == o) 
	    return sv;
	if (type == OP_NEXTSTATE || type == OP_NULL || type == OP_PUSHMARK)
	    continue;
	if (type == OP_LEAVESUB || type == OP_RETURN)
	    break;
	if (sv)
	    return Nullsv;
	if (type == OP_CONST && cSVOPo->op_sv)
	    sv = cSVOPo->op_sv;
	else if ((type == OP_PADSV || type == OP_CONST) && cv) {
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

void
Perl_newMYSUB(pTHX_ I32 floor, OP *o, OP *proto, OP *attrs, OP *block)
{
    if (o)
	SAVEFREEOP(o);
    if (proto)
	SAVEFREEOP(proto);
    if (attrs)
	SAVEFREEOP(attrs);
    if (block)
	SAVEFREEOP(block);
    Perl_croak(aTHX_ "\"my sub\" not yet implemented");
}

CV *
Perl_newSUB(pTHX_ I32 floor, OP *o, OP *proto, OP *block)
{
    return Perl_newATTRSUB(aTHX_ floor, o, proto, Nullop, block);
}

CV *
Perl_newATTRSUB(pTHX_ I32 floor, OP *o, OP *proto, OP *attrs, OP *block)
{
    STRLEN n_a;
    char *name;
    char *aname;
    GV *gv;
    char *ps = proto ? SvPVx(((SVOP*)proto)->op_sv, n_a) : Nullch;
    register CV *cv=0;
    I32 ix;

    name = o ? SvPVx(cSVOPo->op_sv, n_a) : Nullch;
    if (!name && PERLDB_NAMEANON && CopLINE(PL_curcop)) {
	SV *sv = sv_newmortal();
	Perl_sv_setpvf(aTHX_ sv, "__ANON__[%s:%"IVdf"]",
		       CopFILE(PL_curcop), (IV)CopLINE(PL_curcop));
	aname = SvPVX(sv);
    }
    else
	aname = Nullch;
    gv = gv_fetchpv(name ? name : (aname ? aname : "__ANON__"),
		    GV_ADDMULTI | ((block || attrs) ? 0 : GV_NOINIT),
		    SVt_PVCV);

    if (o)
	SAVEFREEOP(o);
    if (proto)
	SAVEFREEOP(proto);
    if (attrs)
	SAVEFREEOP(attrs);

    if (SvTYPE(gv) != SVt_PVGV) {	/* Maybe prototype now, and had at
					   maximum a prototype before. */
	if (SvTYPE(gv) > SVt_NULL) {
	    if (!SvPOK((SV*)gv) && !(SvIOK((SV*)gv) && SvIVX((SV*)gv) == -1)
		&& ckWARN_d(WARN_PROTOTYPE))
	    {
		Perl_warner(aTHX_ WARN_PROTOTYPE, "Runaway prototype");
	    }
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
    else if ((cv = GvCV(gv))) {
	cv_ckproto(cv, gv, ps);
	/* already defined (or promised)? */
	if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
	    SV* const_sv;
	    bool const_changed = TRUE;
	    if (!block && !attrs) {
		/* just a "sub foo;" when &foo is already defined */
		SAVEFREESV(PL_compcv);
		goto done;
	    }
	    /* ahem, death to those who redefine active sort subs */
	    if (PL_curstackinfo->si_type == PERLSI_SORT &&
		PL_sortcop == CvSTART(cv)) {
		op_free(block);
		Perl_croak(aTHX_ "Can't redefine active sort subroutine %s", name);
	    }
	    if (!block)
		goto withattrs;
	    if ((const_sv = cv_const_sv(cv)))
		const_changed = sv_cmp(const_sv, op_const_sv(block, Nullcv));
	    if ((const_sv || const_changed) && ckWARN(WARN_REDEFINE))
	    {
		line_t oldline = CopLINE(PL_curcop);
		CopLINE_set(PL_curcop, PL_copline);
		Perl_warner(aTHX_ WARN_REDEFINE,
			const_sv ? "Constant subroutine %s redefined"
				 : "Subroutine %s redefined", name);
		CopLINE_set(PL_curcop, oldline);
	    }
	    SvREFCNT_dec(cv);
	    cv = Nullcv;
	}
    }
  withattrs:
    if (attrs) {
	HV *stash;
	SV *rcv;

	/* Need to do a C<use attributes $stash_of_cv,\&cv,@attrs>
	 * before we clobber PL_compcv.
	 */
	if (cv && !block) {
	    rcv = (SV*)cv;
	    if (CvGV(cv) && GvSTASH(CvGV(cv)) && HvNAME(GvSTASH(CvGV(cv))))
		stash = GvSTASH(CvGV(cv));
	    else if (CvSTASH(cv) && HvNAME(CvSTASH(cv)))
		stash = CvSTASH(cv);
	    else
		stash = PL_curstash;
	}
	else {
	    /* possibly about to re-define existing subr -- ignore old cv */
	    rcv = (SV*)PL_compcv;
	    if (name && GvSTASH(gv) && HvNAME(GvSTASH(gv)))
		stash = GvSTASH(gv);
	    else
		stash = PL_curstash;
	}
	apply_attrs(stash, rcv, attrs);
    }
    if (cv) {				/* must reuse cv if autoloaded */
	if (!block) {
	    /* got here with just attrs -- work done, so bug out */
	    SAVEFREESV(PL_compcv);
	    goto done;
	}
	cv_undef(cv);
	CvFLAGS(cv) = CvFLAGS(PL_compcv);
	CvOUTSIDE(cv) = CvOUTSIDE(PL_compcv);
	CvOUTSIDE(PL_compcv) = 0;
	CvPADLIST(cv) = CvPADLIST(PL_compcv);
	CvPADLIST(PL_compcv) = 0;
	/* inner references to PL_compcv must be fixed up ... */
	{
	    AV *padlist = CvPADLIST(cv);
	    AV *comppad_name = (AV*)AvARRAY(padlist)[0];
	    AV *comppad = (AV*)AvARRAY(padlist)[1];
	    SV **namepad = AvARRAY(comppad_name);
	    SV **curpad = AvARRAY(comppad);
	    for (ix = AvFILLp(comppad_name); ix > 0; ix--) {
		SV *namesv = namepad[ix];
		if (namesv && namesv != &PL_sv_undef
		    && *SvPVX(namesv) == '&')
		{
		    CV *innercv = (CV*)curpad[ix];
		    if (CvOUTSIDE(innercv) == PL_compcv) {
			CvOUTSIDE(innercv) = cv;
			if (!CvANON(innercv) || CvCLONED(innercv)) {
			    (void)SvREFCNT_inc(cv);
			    SvREFCNT_dec(PL_compcv);
			}
		    }
		}
	    }
	}
	/* ... before we throw it away */
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
    CvGV(cv) = gv;
    CvFILE(cv) = CopFILE(PL_curcop);
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
		if (PL_in_eval & EVAL_KEEPERR)
		    Perl_croak(aTHX_ not_safe);
		else {
		    /* force display of errors found but not reported */
		    sv_catpv(ERRSV, not_safe);
		    Perl_croak(aTHX_ "%s", SvPVx(ERRSV, n_a));
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

    if (CvLVALUE(cv)) {
	CvROOT(cv) = newUNOP(OP_LEAVESUBLV, 0,
			     mod(scalarseq(block), OP_LEAVESUBLV));
    }
    else {
	CvROOT(cv) = newUNOP(OP_LEAVESUB, 0, scalarseq(block));
    }
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    peep(CvSTART(cv));

    /* now that optimizer has done its work, adjust pad values */
    if (CvCLONE(cv)) {
	SV **namep = AvARRAY(PL_comppad_name);
	for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	    SV *namesv;

	    if (SvIMMORTAL(PL_curpad[ix]) || IS_PADGV(PL_curpad[ix]) || IS_PADCONST(PL_curpad[ix]))
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
	    if (SvIMMORTAL(PL_curpad[ix]) || IS_PADGV(PL_curpad[ix]) || IS_PADCONST(PL_curpad[ix]))
		continue;
	    if (!SvPADMY(PL_curpad[ix]))
		SvPADTMP_on(PL_curpad[ix]);
	}
    }

    /* If a potential closure prototype, don't keep a refcount on outer CV.
     * This is okay as the lifetime of the prototype is tied to the
     * lifetime of the outer CV.  Avoids memory leak due to reference
     * loop. --GSAR */
    if (!name)
	SvREFCNT_dec(CvOUTSIDE(cv));

    if (name || aname) {
	char *s;
	char *tname = (name ? name : aname);

	if (PERLDB_SUBLINE && PL_curstash != PL_debstash) {
	    SV *sv = NEWSV(0,0);
	    SV *tmpstr = sv_newmortal();
	    GV *db_postponed = gv_fetchpv("DB::postponed", GV_ADDMULTI, SVt_PVHV);
	    CV *pcv;
	    HV *hv;

	    Perl_sv_setpvf(aTHX_ sv, "%s:%ld-%ld",
			   CopFILE(PL_curcop),
			   (long)PL_subline, (long)CopLINE(PL_curcop));
	    gv_efullname3(tmpstr, gv, Nullch);
	    hv_store(GvHV(PL_DBsub), SvPVX(tmpstr), SvCUR(tmpstr), sv, 0);
	    hv = GvHVn(db_postponed);
	    if (HvFILL(hv) > 0 && hv_exists(hv, SvPVX(tmpstr), SvCUR(tmpstr))
		&& (pcv = GvCV(db_postponed)))
	    {
		dSP;
		PUSHMARK(SP);
		XPUSHs(tmpstr);
		PUTBACK;
		call_sv((SV*)pcv, G_DISCARD);
	    }
	}

	if ((s = strrchr(tname,':')))
	    s++;
	else
	    s = tname;

	if (*s != 'B' && *s != 'E' && *s != 'C' && *s != 'I')
	    goto done;

	if (strEQ(s, "BEGIN")) {
	    I32 oldscope = PL_scopestack_ix;
	    ENTER;
	    SAVECOPFILE(&PL_compiling);
	    SAVECOPLINE(&PL_compiling);
	    save_svref(&PL_rs);
	    sv_setsv(PL_rs, PL_nrs);

	    if (!PL_beginav)
		PL_beginav = newAV();
	    DEBUG_x( dump_sub(gv) );
	    av_push(PL_beginav, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	    call_list(oldscope, PL_beginav);

	    PL_curcop = &PL_compiling;
	    PL_compiling.op_private = PL_hints;
	    LEAVE;
	}
	else if (strEQ(s, "END") && !PL_error_count) {
	    if (!PL_endav)
		PL_endav = newAV();
	    DEBUG_x( dump_sub(gv) );
	    av_unshift(PL_endav, 1);
	    av_store(PL_endav, 0, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
	else if (strEQ(s, "CHECK") && !PL_error_count) {
	    if (!PL_checkav)
		PL_checkav = newAV();
	    DEBUG_x( dump_sub(gv) );
	    if (PL_main_start && ckWARN(WARN_VOID))
		Perl_warner(aTHX_ WARN_VOID, "Too late to run CHECK block");
	    av_unshift(PL_checkav, 1);
	    av_store(PL_checkav, 0, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
	else if (strEQ(s, "INIT") && !PL_error_count) {
	    if (!PL_initav)
		PL_initav = newAV();
	    DEBUG_x( dump_sub(gv) );
	    if (PL_main_start && ckWARN(WARN_VOID))
		Perl_warner(aTHX_ WARN_VOID, "Too late to run INIT block");
	    av_push(PL_initav, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
    }

  done:
    PL_copline = NOLINE;
    LEAVE_SCOPE(floor);
    return cv;
}

/* XXX unsafe for threads if eval_owner isn't held */
/*
=for apidoc newCONSTSUB

Creates a constant sub equivalent to Perl C<sub FOO () { 123 }> which is
eligible for inlining at compile-time.

=cut
*/

void
Perl_newCONSTSUB(pTHX_ HV *stash, char *name, SV *sv)
{

    ENTER;

    SAVECOPLINE(PL_curcop);
    CopLINE_set(PL_curcop, PL_copline);

    SAVEHINTS();
    PL_hints &= ~HINT_BLOCK_SCOPE;

    if (stash) {
	SAVESPTR(PL_curstash);
	SAVECOPSTASH(PL_curcop);
	PL_curstash = stash;
#ifdef USE_ITHREADS
	CopSTASHPV(PL_curcop) = stash ? HvNAME(stash) : Nullch;
#else
	CopSTASH(PL_curcop) = stash;
#endif
    }

    newATTRSUB(
	start_subparse(FALSE, 0),
	newSVOP(OP_CONST, 0, newSVpv(name,0)),
	newSVOP(OP_CONST, 0, &PL_sv_no),	/* SvPV(&PL_sv_no) == "" -- GMB */
	Nullop,
	newSTATEOP(0, Nullch, newSVOP(OP_CONST, 0, sv))
    );

    LEAVE;
}

/*
=for apidoc U||newXS

Used by C<xsubpp> to hook up XSUBs as Perl subs.

=cut
*/

CV *
Perl_newXS(pTHX_ char *name, XSUBADDR_t subaddr, char *filename)
{
    GV *gv = gv_fetchpv(name ? name : "__ANON__", GV_ADDMULTI, SVt_PVCV);
    register CV *cv;

    if ((cv = (name ? GvCV(gv) : Nullcv))) {
	if (GvCVGEN(gv)) {
	    /* just a cached method */
	    SvREFCNT_dec(cv);
	    cv = 0;
	}
	else if (CvROOT(cv) || CvXSUB(cv) || GvASSUMECV(gv)) {
	    /* already defined (or promised) */
	    if (ckWARN(WARN_REDEFINE) && !(CvGV(cv) && GvSTASH(CvGV(cv))
			    && HvNAME(GvSTASH(CvGV(cv)))
			    && strEQ(HvNAME(GvSTASH(CvGV(cv))), "autouse"))) {
		line_t oldline = CopLINE(PL_curcop);
		if (PL_copline != NOLINE)
		    CopLINE_set(PL_curcop, PL_copline);
		Perl_warner(aTHX_ WARN_REDEFINE, "Subroutine %s redefined",name);
		CopLINE_set(PL_curcop, oldline);
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
    CvGV(cv) = gv;
#ifdef USE_THREADS
    New(666, CvMUTEXP(cv), 1, perl_mutex);
    MUTEX_INIT(CvMUTEXP(cv));
    CvOWNER(cv) = 0;
#endif /* USE_THREADS */
    (void)gv_fetchfile(filename);
    CvFILE(cv) = filename;	/* NOTE: not copied, as it is expected to be
				   an external constant string */
    CvXSUB(cv) = subaddr;

    if (name) {
	char *s = strrchr(name,':');
	if (s)
	    s++;
	else
	    s = name;

	if (*s != 'B' && *s != 'E' && *s != 'C' && *s != 'I')
	    goto done;

	if (strEQ(s, "BEGIN")) {
	    if (!PL_beginav)
		PL_beginav = newAV();
	    av_push(PL_beginav, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
	else if (strEQ(s, "END")) {
	    if (!PL_endav)
		PL_endav = newAV();
	    av_unshift(PL_endav, 1);
	    av_store(PL_endav, 0, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
	else if (strEQ(s, "CHECK")) {
	    if (!PL_checkav)
		PL_checkav = newAV();
	    if (PL_main_start && ckWARN(WARN_VOID))
		Perl_warner(aTHX_ WARN_VOID, "Too late to run CHECK block");
	    av_unshift(PL_checkav, 1);
	    av_store(PL_checkav, 0, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
	else if (strEQ(s, "INIT")) {
	    if (!PL_initav)
		PL_initav = newAV();
	    if (PL_main_start && ckWARN(WARN_VOID))
		Perl_warner(aTHX_ WARN_VOID, "Too late to run INIT block");
	    av_push(PL_initav, (SV*)cv);
	    GvCV(gv) = 0;		/* cv has been hijacked */
	}
    }
    else
	CvANON_on(cv);

done:
    return cv;
}

void
Perl_newFORM(pTHX_ I32 floor, OP *o, OP *block)
{
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
    if ((cv = GvFORM(gv))) {
	if (ckWARN(WARN_REDEFINE)) {
	    line_t oldline = CopLINE(PL_curcop);

	    CopLINE_set(PL_curcop, PL_copline);
	    Perl_warner(aTHX_ WARN_REDEFINE, "Format %s redefined",name);
	    CopLINE_set(PL_curcop, oldline);
	}
	SvREFCNT_dec(cv);
    }
    cv = PL_compcv;
    GvFORM(gv) = cv;
    CvGV(cv) = gv;
    CvFILE(cv) = CopFILE(PL_curcop);

    for (ix = AvFILLp(PL_comppad); ix > 0; ix--) {
	if (!SvPADMY(PL_curpad[ix]) && !SvIMMORTAL(PL_curpad[ix]))
	    SvPADTMP_on(PL_curpad[ix]);
    }

    CvROOT(cv) = newUNOP(OP_LEAVEWRITE, 0, scalarseq(block));
    CvROOT(cv)->op_private |= OPpREFCOUNTED;
    OpREFCNT_set(CvROOT(cv), 1);
    CvSTART(cv) = LINKLIST(CvROOT(cv));
    CvROOT(cv)->op_next = 0;
    peep(CvSTART(cv));
    op_free(o);
    PL_copline = NOLINE;
    LEAVE_SCOPE(floor);
}

OP *
Perl_newANONLIST(pTHX_ OP *o)
{
    return newUNOP(OP_REFGEN, 0,
	mod(list(convert(OP_ANONLIST, 0, o)), OP_REFGEN));
}

OP *
Perl_newANONHASH(pTHX_ OP *o)
{
    return newUNOP(OP_REFGEN, 0,
	mod(list(convert(OP_ANONHASH, 0, o)), OP_REFGEN));
}

OP *
Perl_newANONSUB(pTHX_ I32 floor, OP *proto, OP *block)
{
    return newANONATTRSUB(floor, proto, Nullop, block);
}

OP *
Perl_newANONATTRSUB(pTHX_ I32 floor, OP *proto, OP *attrs, OP *block)
{
    return newUNOP(OP_REFGEN, 0,
	newSVOP(OP_ANONCODE, 0,
		(SV*)newATTRSUB(floor, 0, proto, attrs, block)));
}

OP *
Perl_oopsAV(pTHX_ OP *o)
{
    switch (o->op_type) {
    case OP_PADSV:
	o->op_type = OP_PADAV;
	o->op_ppaddr = PL_ppaddr[OP_PADAV];
	return ref(o, OP_RV2AV);
	
    case OP_RV2SV:
	o->op_type = OP_RV2AV;
	o->op_ppaddr = PL_ppaddr[OP_RV2AV];
	ref(o, OP_RV2AV);
	break;

    default:
	if (ckWARN_d(WARN_INTERNAL))
	    Perl_warner(aTHX_ WARN_INTERNAL, "oops: oopsAV");
	break;
    }
    return o;
}

OP *
Perl_oopsHV(pTHX_ OP *o)
{
    switch (o->op_type) {
    case OP_PADSV:
    case OP_PADAV:
	o->op_type = OP_PADHV;
	o->op_ppaddr = PL_ppaddr[OP_PADHV];
	return ref(o, OP_RV2HV);

    case OP_RV2SV:
    case OP_RV2AV:
	o->op_type = OP_RV2HV;
	o->op_ppaddr = PL_ppaddr[OP_RV2HV];
	ref(o, OP_RV2HV);
	break;

    default:
	if (ckWARN_d(WARN_INTERNAL))
	    Perl_warner(aTHX_ WARN_INTERNAL, "oops: oopsHV");
	break;
    }
    return o;
}

OP *
Perl_newAVREF(pTHX_ OP *o)
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADAV;
	o->op_ppaddr = PL_ppaddr[OP_PADAV];
	return o;
    }
    return newUNOP(OP_RV2AV, 0, scalar(o));
}

OP *
Perl_newGVREF(pTHX_ I32 type, OP *o)
{
    if (type == OP_MAPSTART || type == OP_GREPSTART || type == OP_SORT)
	return newUNOP(OP_NULL, 0, o);
    return ref(newUNOP(OP_RV2GV, OPf_REF, o), type);
}

OP *
Perl_newHVREF(pTHX_ OP *o)
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADHV;
	o->op_ppaddr = PL_ppaddr[OP_PADHV];
	return o;
    }
    return newUNOP(OP_RV2HV, 0, scalar(o));
}

OP *
Perl_oopsCV(pTHX_ OP *o)
{
    Perl_croak(aTHX_ "NOT IMPL LINE %d",__LINE__);
    /* STUB */
    return o;
}

OP *
Perl_newCVREF(pTHX_ I32 flags, OP *o)
{
    return newUNOP(OP_RV2CV, flags, scalar(o));
}

OP *
Perl_newSVREF(pTHX_ OP *o)
{
    if (o->op_type == OP_PADANY) {
	o->op_type = OP_PADSV;
	o->op_ppaddr = PL_ppaddr[OP_PADSV];
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
Perl_ck_anoncode(pTHX_ OP *o)
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
Perl_ck_bitop(pTHX_ OP *o)
{
    o->op_private = PL_hints;
    return o;
}

OP *
Perl_ck_concat(pTHX_ OP *o)
{
    if (cUNOPo->op_first->op_type == OP_CONCAT)
	o->op_flags |= OPf_STACKED;
    return o;
}

OP *
Perl_ck_spair(pTHX_ OP *o)
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
	     !(PL_opargs[newop->op_type] & OA_RETSCALAR) ||
	     newop->op_type == OP_PADAV || newop->op_type == OP_PADHV ||
	     newop->op_type == OP_RV2AV || newop->op_type == OP_RV2HV)) {
	
	    return o;
	}
	op_free(kUNOP->op_first);
	kUNOP->op_first = newop;
    }
    o->op_ppaddr = PL_ppaddr[++o->op_type];
    return ck_fun(o);
}

OP *
Perl_ck_delete(pTHX_ OP *o)
{
    o = ck_fun(o);
    o->op_private = 0;
    if (o->op_flags & OPf_KIDS) {
	OP *kid = cUNOPo->op_first;
	switch (kid->op_type) {
	case OP_ASLICE:
	    o->op_flags |= OPf_SPECIAL;
	    /* FALL THROUGH */
	case OP_HSLICE:
	    o->op_private |= OPpSLICE;
	    break;
	case OP_AELEM:
	    o->op_flags |= OPf_SPECIAL;
	    /* FALL THROUGH */
	case OP_HELEM:
	    break;
	default:
	    Perl_croak(aTHX_ "%s argument is not a HASH or ARRAY element or slice",
		  PL_op_desc[o->op_type]);
	}
	null(kid);
    }
    return o;
}

OP *
Perl_ck_eof(pTHX_ OP *o)
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
Perl_ck_eval(pTHX_ OP *o)
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

	    NewOp(1101, enter, 1, LOGOP);
	    enter->op_type = OP_ENTERTRY;
	    enter->op_ppaddr = PL_ppaddr[OP_ENTERTRY];
	    enter->op_private = 0;

	    /* establish postfix order */
	    enter->op_next = (OP*)enter;

	    o = prepend_elem(OP_LINESEQ, (OP*)enter, (OP*)kid);
	    o->op_type = OP_LEAVETRY;
	    o->op_ppaddr = PL_ppaddr[OP_LEAVETRY];
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
Perl_ck_exit(pTHX_ OP *o)
{
#ifdef VMS
    HV *table = GvHV(PL_hintgv);
    if (table) {
       SV **svp = hv_fetch(table, "vmsish_exit", 11, FALSE);
       if (svp && *svp && SvTRUE(*svp))
           o->op_private |= OPpEXIT_VMSISH;
    }
#endif
    return ck_fun(o);
}

OP *
Perl_ck_exec(pTHX_ OP *o)
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
Perl_ck_exists(pTHX_ OP *o)
{
    o = ck_fun(o);
    if (o->op_flags & OPf_KIDS) {
	OP *kid = cUNOPo->op_first;
	if (kid->op_type == OP_ENTERSUB) {
	    (void) ref(kid, o->op_type);
	    if (kid->op_type != OP_RV2CV && !PL_error_count)
		Perl_croak(aTHX_ "%s argument is not a subroutine name",
			   PL_op_desc[o->op_type]);
	    o->op_private |= OPpEXISTS_SUB;
	}
	else if (kid->op_type == OP_AELEM)
	    o->op_flags |= OPf_SPECIAL;
	else if (kid->op_type != OP_HELEM)
	    Perl_croak(aTHX_ "%s argument is not a HASH or ARRAY element",
		       PL_op_desc[o->op_type]);
	null(kid);
    }
    return o;
}

#if 0
OP *
Perl_ck_gvconst(pTHX_ register OP *o)
{
    o = fold_constants(o);
    if (o->op_type == OP_CONST)
	o->op_type = OP_GV;
    return o;
}
#endif

OP *
Perl_ck_rvconst(pTHX_ register OP *o)
{
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
		Perl_croak(aTHX_ "Constant is not %s reference", badtype);
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
		Perl_croak(aTHX_ 
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
#ifdef USE_ITHREADS
	    /* XXX hack: dependence on sizeof(PADOP) <= sizeof(SVOP) */
	    kPADOP->op_padix = pad_alloc(OP_GV, SVs_PADTMP);
	    SvREFCNT_dec(PL_curpad[kPADOP->op_padix]);
	    GvIN_PAD_on(gv);
	    PL_curpad[kPADOP->op_padix] = SvREFCNT_inc(gv);
#else
	    kid->op_sv = SvREFCNT_inc(gv);
#endif
	    kid->op_private = 0;
	    kid->op_ppaddr = PL_ppaddr[OP_GV];
	}
    }
    return o;
}

OP *
Perl_ck_ftst(pTHX_ OP *o)
{
    I32 type = o->op_type;

    if (o->op_flags & OPf_REF) {
	/* nothing */
    }
    else if (o->op_flags & OPf_KIDS && cUNOPo->op_first->op_type != OP_STUB) {
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_CONST && (kid->op_private & OPpCONST_BARE)) {
	    STRLEN n_a;
	    OP *newop = newGVOP(type, OPf_REF,
		gv_fetchpv(SvPVx(kid->op_sv, n_a), TRUE, SVt_PVIO));
	    op_free(o);
	    o = newop;
	}
    }
    else {
	op_free(o);
	if (type == OP_FTTTY)
           o =  newGVOP(type, OPf_REF, gv_fetchpv("main::STDIN", TRUE,
				SVt_PVIO));
	else
	    o = newUNOP(type, 0, newDEFSVOP());
    }
#ifdef USE_LOCALE
    if (type == OP_FTTEXT || type == OP_FTBINARY) {
	o->op_private = 0;
	if (PL_hints & HINT_LOCALE)
	    o->op_private |= OPpLOCALE;
    }
#endif
    return o;
}

OP *
Perl_ck_fun(pTHX_ OP *o)
{
    register OP *kid;
    OP **tokid;
    OP *sibl;
    I32 numargs = 0;
    int type = o->op_type;
    register I32 oa = PL_opargs[type] >> OASHIFT;

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
	    (kid->op_type == OP_NULL && kid->op_targ == OP_PUSHMARK))
	{
	    tokid = &kid->op_sibling;
	    kid = kid->op_sibling;
	}
	if (!kid && PL_opargs[type] & OA_DEFGV)
	    *tokid = kid = newDEFSVOP();

	while (oa && kid) {
	    numargs++;
	    sibl = kid->op_sibling;
	    switch (oa & 7) {
	    case OA_SCALAR:
		/* list seen where single (scalar) arg expected? */
		if (numargs == 1 && !(oa >> 4)
		    && kid->op_type == OP_LIST && type != OP_SCALAR)
		{
		    return too_many_arguments(o,PL_op_desc[type]);
		}
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
		    (kid->op_private & OPpCONST_BARE))
		{
		    char *name = SvPVx(((SVOP*)kid)->op_sv, n_a);
		    OP *newop = newAVREF(newGVOP(OP_GV, 0,
			gv_fetchpv(name, TRUE, SVt_PVAV) ));
		    if (ckWARN(WARN_DEPRECATED))
			Perl_warner(aTHX_ WARN_DEPRECATED,
			    "Array @%s missing the @ in argument %"IVdf" of %s()",
			    name, (IV)numargs, PL_op_desc[type]);
		    op_free(kid);
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2AV && kid->op_type != OP_PADAV)
		    bad_type(numargs, "array", PL_op_desc[type], kid);
		mod(kid, type);
		break;
	    case OA_HVREF:
		if (kid->op_type == OP_CONST &&
		    (kid->op_private & OPpCONST_BARE))
		{
		    char *name = SvPVx(((SVOP*)kid)->op_sv, n_a);
		    OP *newop = newHVREF(newGVOP(OP_GV, 0,
			gv_fetchpv(name, TRUE, SVt_PVHV) ));
		    if (ckWARN(WARN_DEPRECATED))
			Perl_warner(aTHX_ WARN_DEPRECATED,
			    "Hash %%%s missing the %% in argument %"IVdf" of %s()",
			    name, (IV)numargs, PL_op_desc[type]);
		    op_free(kid);
		    kid = newop;
		    kid->op_sibling = sibl;
		    *tokid = kid;
		}
		else if (kid->op_type != OP_RV2HV && kid->op_type != OP_PADHV)
		    bad_type(numargs, "hash", PL_op_desc[type], kid);
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
			(kid->op_private & OPpCONST_BARE))
		    {
			OP *newop = newGVOP(OP_GV, 0,
			    gv_fetchpv(SvPVx(((SVOP*)kid)->op_sv, n_a), TRUE,
					SVt_PVIO) );
			op_free(kid);
			kid = newop;
		    }
		    else if (kid->op_type == OP_READLINE) {
			/* neophyte patrol: open(<FH>), close(<FH>) etc. */
			bad_type(numargs, "HANDLE", PL_op_desc[o->op_type], kid);
		    }
		    else {
			I32 flags = OPf_SPECIAL;
			I32 priv = 0;
			PADOFFSET targ = 0;

			/* is this op a FH constructor? */
			if (is_handle_constructor(o,numargs)) {
			    char *name = Nullch;
			    STRLEN len;

			    flags = 0;
			    /* Set a flag to tell rv2gv to vivify
			     * need to "prove" flag does not mean something
			     * else already - NI-S 1999/05/07
			     */
			    priv = OPpDEREF;
			    if (kid->op_type == OP_PADSV) {
				SV **namep = av_fetch(PL_comppad_name,
						      kid->op_targ, 4);
				if (namep && *namep)
				    name = SvPV(*namep, len);
			    }
			    else if (kid->op_type == OP_RV2SV
				     && kUNOP->op_first->op_type == OP_GV)
			    {
				GV *gv = cGVOPx_gv(kUNOP->op_first);
				name = GvNAME(gv);
				len = GvNAMELEN(gv);
			    }
			    else if (kid->op_type == OP_AELEM
				     || kid->op_type == OP_HELEM)
			    {
				name = "__ANONIO__";
				len = 10;
				mod(kid,type);
			    }
			    if (name) {
				SV *namesv;
				targ = pad_alloc(OP_RV2GV, SVs_PADTMP);
				namesv = PL_curpad[targ];
				(void)SvUPGRADE(namesv, SVt_PV);
				if (*name != '$')
				    sv_setpvn(namesv, "$", 1);
				sv_catpvn(namesv, name, len);
			    }
			}
			kid->op_sibling = 0;
			kid = newUNOP(OP_RV2GV, flags, scalar(kid));
			kid->op_targ = targ;
			kid->op_private |= priv;
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
	    return too_many_arguments(o,PL_op_desc[o->op_type]);
	listkids(o);
    }
    else if (PL_opargs[type] & OA_DEFGV) {
	op_free(o);
	return newUNOP(type, 0, newDEFSVOP());
    }

    if (oa) {
	while (oa & OA_OPTIONAL)
	    oa >>= 4;
	if (oa && oa != OA_LIST)
	    return too_few_arguments(o,PL_op_desc[o->op_type]);
    }
    return o;
}

OP *
Perl_ck_glob(pTHX_ OP *o)
{
    GV *gv;

    o = ck_fun(o);
    if ((o->op_flags & OPf_KIDS) && !cLISTOPo->op_first->op_sibling)
	append_elem(OP_GLOB, o, newDEFSVOP());

    if (!((gv = gv_fetchpv("glob", FALSE, SVt_PVCV)) && GvIMPORTED_CV(gv)))
	gv = gv_fetchpv("CORE::GLOBAL::glob", FALSE, SVt_PVCV);

#if !defined(PERL_EXTERNAL_GLOB)
    /* XXX this can be tightened up and made more failsafe. */
    if (!gv) {
	ENTER;
	Perl_load_module(aTHX_ 0, newSVpvn("File::Glob", 10), Nullsv,
			 /* null-terminated import list */
			 newSVpvn(":globally", 9), Nullsv);
	gv = gv_fetchpv("CORE::GLOBAL::glob", FALSE, SVt_PVCV);
	LEAVE;
    }
#endif /* PERL_EXTERNAL_GLOB */

    if (gv && GvIMPORTED_CV(gv)) {
	append_elem(OP_GLOB, o,
		    newSVOP(OP_CONST, 0, newSViv(PL_glob_index++)));
	o->op_type = OP_LIST;
	o->op_ppaddr = PL_ppaddr[OP_LIST];
	cLISTOPo->op_first->op_type = OP_PUSHMARK;
	cLISTOPo->op_first->op_ppaddr = PL_ppaddr[OP_PUSHMARK];
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
    SvREFCNT_dec((SV*)gv); /* had excess refcnt */
    scalarkids(o);
    return o;
}

OP *
Perl_ck_grep(pTHX_ OP *o)
{
    LOGOP *gwop;
    OP *kid;
    OPCODE type = o->op_type == OP_GREPSTART ? OP_GREPWHILE : OP_MAPWHILE;

    o->op_ppaddr = PL_ppaddr[OP_GREPSTART];
    NewOp(1101, gwop, 1, LOGOP);

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
	Perl_croak(aTHX_ "panic: ck_grep");
    kid = kUNOP->op_first;

    gwop->op_type = type;
    gwop->op_ppaddr = PL_ppaddr[type];
    gwop->op_first = listkids(o);
    gwop->op_flags |= OPf_KIDS;
    gwop->op_private = 1;
    gwop->op_other = LINKLIST(kid);
    gwop->op_targ = pad_alloc(type, SVs_PADTMP);
    kid->op_next = (OP*)gwop;

    kid = cLISTOPo->op_first->op_sibling;
    if (!kid || !kid->op_sibling)
	return too_few_arguments(o,PL_op_desc[o->op_type]);
    for (kid = kid->op_sibling; kid; kid = kid->op_sibling)
	mod(kid, OP_GREPSTART);

    return (OP*)gwop;
}

OP *
Perl_ck_index(pTHX_ OP *o)
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
Perl_ck_lengthconst(pTHX_ OP *o)
{
    /* XXX length optimization goes here */
    return ck_fun(o);
}

OP *
Perl_ck_lfun(pTHX_ OP *o)
{
    OPCODE type = o->op_type;
    return modkids(ck_fun(o), type);
}

OP *
Perl_ck_defined(pTHX_ OP *o)		/* 19990527 MJD */
{
    if ((o->op_flags & OPf_KIDS) && ckWARN(WARN_DEPRECATED)) {
	switch (cUNOPo->op_first->op_type) {
	case OP_RV2AV:
	    /* This is needed for
	       if (defined %stash::)
	       to work.   Do not break Tk.
	       */
	    break;                      /* Globals via GV can be undef */ 
	case OP_PADAV:
	case OP_AASSIGN:		/* Is this a good idea? */
	    Perl_warner(aTHX_ WARN_DEPRECATED,
			"defined(@array) is deprecated");
	    Perl_warner(aTHX_ WARN_DEPRECATED,
			"\t(Maybe you should just omit the defined()?)\n");
	break;
	case OP_RV2HV:
	    /* This is needed for
	       if (defined %stash::)
	       to work.   Do not break Tk.
	       */
	    break;                      /* Globals via GV can be undef */ 
	case OP_PADHV:
	    Perl_warner(aTHX_ WARN_DEPRECATED,
			"defined(%%hash) is deprecated");
	    Perl_warner(aTHX_ WARN_DEPRECATED,
			"\t(Maybe you should just omit the defined()?)\n");
	    break;
	default:
	    /* no warning */
	    break;
	}
    }
    return ck_rfun(o);
}

OP *
Perl_ck_rfun(pTHX_ OP *o)
{
    OPCODE type = o->op_type;
    return refkids(ck_fun(o), type);
}

OP *
Perl_ck_listiob(pTHX_ OP *o)
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
Perl_ck_fun_locale(pTHX_ OP *o)
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
Perl_ck_sassign(pTHX_ OP *o)
{
    OP *kid = cLISTOPo->op_first;
    /* has a disposable target? */
    if ((PL_opargs[kid->op_type] & OA_TARGLEX)
	&& !(kid->op_flags & OPf_STACKED)
	/* Cannot steal the second time! */
	&& !(kid->op_private & OPpTARGET_MY))
    {
	OP *kkid = kid->op_sibling;

	/* Can just relocate the target. */
	if (kkid && kkid->op_type == OP_PADSV
	    && !(kkid->op_private & OPpLVAL_INTRO))
	{
	    kid->op_targ = kkid->op_targ;
	    kkid->op_targ = 0;
	    /* Now we do not need PADSV and SASSIGN. */
	    kid->op_sibling = o->op_sibling;	/* NULL */
	    cLISTOPo->op_first = NULL;
	    op_free(o);
	    op_free(kkid);
	    kid->op_private |= OPpTARGET_MY;	/* Used for context settings */
	    return kid;
	}
    }
    return o;
}

OP *
Perl_ck_scmp(pTHX_ OP *o)
{
    o->op_private = 0;
#ifdef USE_LOCALE
    if (PL_hints & HINT_LOCALE)
	o->op_private |= OPpLOCALE;
#endif

    return o;
}

OP *
Perl_ck_match(pTHX_ OP *o)
{
    o->op_private |= OPpRUNTIME;
    return o;
}

OP *
Perl_ck_method(pTHX_ OP *o)
{
    OP *kid = cUNOPo->op_first;
    if (kid->op_type == OP_CONST) {
	SV* sv = kSVOP->op_sv;
	if (!(strchr(SvPVX(sv), ':') || strchr(SvPVX(sv), '\''))) {
	    OP *cmop;
	    (void)SvUPGRADE(sv, SVt_PVIV);
	    (void)SvIOK_on(sv);
	    PERL_HASH(SvUVX(sv), SvPVX(sv), SvCUR(sv));
	    cmop = newSVOP(OP_METHOD_NAMED, 0, sv);
	    kSVOP->op_sv = Nullsv;
	    op_free(o);
	    return cmop;
	}
    }
    return o;
}

OP *
Perl_ck_null(pTHX_ OP *o)
{
    return o;
}

OP *
Perl_ck_open(pTHX_ OP *o)
{
    HV *table = GvHV(PL_hintgv);
    if (table) {
	SV **svp;
	I32 mode;
	svp = hv_fetch(table, "open_IN", 7, FALSE);
	if (svp && *svp) {
	    mode = mode_from_discipline(*svp);
	    if (mode & O_BINARY)
		o->op_private |= OPpOPEN_IN_RAW;
	    else if (mode & O_TEXT)
		o->op_private |= OPpOPEN_IN_CRLF;
	}

	svp = hv_fetch(table, "open_OUT", 8, FALSE);
	if (svp && *svp) {
	    mode = mode_from_discipline(*svp);
	    if (mode & O_BINARY)
		o->op_private |= OPpOPEN_OUT_RAW;
	    else if (mode & O_TEXT)
		o->op_private |= OPpOPEN_OUT_CRLF;
	}
    }
    if (o->op_type == OP_BACKTICK)
	return o;
    return ck_fun(o);
}

OP *
Perl_ck_repeat(pTHX_ OP *o)
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
Perl_ck_require(pTHX_ OP *o)
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
	    if (SvREADONLY(kid->op_sv)) {
		SvREADONLY_off(kid->op_sv);
		sv_catpvn(kid->op_sv, ".pm", 3);
		SvREADONLY_on(kid->op_sv);
	    }
	    else
		sv_catpvn(kid->op_sv, ".pm", 3);
	}
    }
    return ck_fun(o);
}

OP *
Perl_ck_return(pTHX_ OP *o)
{
    OP *kid;
    if (CvLVALUE(PL_compcv)) {
	for (kid = cLISTOPo->op_first->op_sibling; kid; kid = kid->op_sibling)
	    mod(kid, OP_LEAVESUBLV);
    }
    return o;
}

#if 0
OP *
Perl_ck_retarget(pTHX_ OP *o)
{
    Perl_croak(aTHX_ "NOT IMPL LINE %d",__LINE__);
    /* STUB */
    return o;
}
#endif

OP *
Perl_ck_select(pTHX_ OP *o)
{
    OP* kid;
    if (o->op_flags & OPf_KIDS) {
	kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
	if (kid && kid->op_sibling) {
	    o->op_type = OP_SSELECT;
	    o->op_ppaddr = PL_ppaddr[OP_SSELECT];
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
Perl_ck_shift(pTHX_ OP *o)
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
Perl_ck_sort(pTHX_ OP *o)
{
    OP *firstkid;
    o->op_private = 0;
#ifdef USE_LOCALE
    if (PL_hints & HINT_LOCALE)
	o->op_private |= OPpLOCALE;
#endif

    if (o->op_type == OP_SORT && o->op_flags & OPf_STACKED)
	simplify_sort(o);
    firstkid = cLISTOPo->op_first->op_sibling;		/* get past pushmark */
    if (o->op_flags & OPf_STACKED) {			/* may have been cleared */
	OP *k;
	OP *kid = cUNOPx(firstkid)->op_first;		/* get past null */

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
			/* don't descend into loops */
			else if (k->op_type == OP_ENTERLOOP
				 || k->op_type == OP_ENTERITER)
			{
			    k = cLOOPx(k)->op_lastop;
			}
		    }
		}
		else
		    kid->op_next = 0;		/* just disconnect the leave */
		k = kLISTOP->op_first;
	    }
	    peep(k);

	    kid = firstkid;
	    if (o->op_type == OP_SORT) {
		/* provide scalar context for comparison function/block */
		kid = scalar(kid);
		kid->op_next = kid;
	    }
	    else
		kid->op_next = k;
	    o->op_flags |= OPf_SPECIAL;
	}
	else if (kid->op_type == OP_RV2SV || kid->op_type == OP_PADSV)
	    null(firstkid);

	firstkid = firstkid->op_sibling;
    }

    /* provide list context for arguments */
    if (o->op_type == OP_SORT)
	list(firstkid);

    return o;
}

STATIC void
S_simplify_sort(pTHX_ OP *o)
{
    register OP *kid = cLISTOPo->op_first->op_sibling;	/* get past pushmark */
    OP *k;
    int reversed;
    GV *gv;
    if (!(o->op_flags & OPf_STACKED))
	return;
    GvMULTI_on(gv_fetchpv("a", TRUE, SVt_PV)); 
    GvMULTI_on(gv_fetchpv("b", TRUE, SVt_PV)); 
    kid = kUNOP->op_first;				/* get past null */
    if (kid->op_type != OP_SCOPE)
	return;
    kid = kLISTOP->op_last;				/* get past scope */
    switch(kid->op_type) {
	case OP_NCMP:
	case OP_I_NCMP:
	case OP_SCMP:
	    break;
	default:
	    return;
    }
    k = kid;						/* remember this node*/
    if (kBINOP->op_first->op_type != OP_RV2SV)
	return;
    kid = kBINOP->op_first;				/* get past cmp */
    if (kUNOP->op_first->op_type != OP_GV)
	return;
    kid = kUNOP->op_first;				/* get past rv2sv */
    gv = kGVOP_gv;
    if (GvSTASH(gv) != PL_curstash)
	return;
    if (strEQ(GvNAME(gv), "a"))
	reversed = 0;
    else if (strEQ(GvNAME(gv), "b"))
	reversed = 1;
    else
	return;
    kid = k;						/* back to cmp */
    if (kBINOP->op_last->op_type != OP_RV2SV)
	return;
    kid = kBINOP->op_last;				/* down to 2nd arg */
    if (kUNOP->op_first->op_type != OP_GV)
	return;
    kid = kUNOP->op_first;				/* get past rv2sv */
    gv = kGVOP_gv;
    if (GvSTASH(gv) != PL_curstash
	|| ( reversed
	    ? strNE(GvNAME(gv), "a")
	    : strNE(GvNAME(gv), "b")))
	return;
    o->op_flags &= ~(OPf_STACKED | OPf_SPECIAL);
    if (reversed)
	o->op_private |= OPpSORT_REVERSE;
    if (k->op_type == OP_NCMP)
	o->op_private |= OPpSORT_NUMERIC;
    if (k->op_type == OP_I_NCMP)
	o->op_private |= OPpSORT_NUMERIC | OPpSORT_INTEGER;
    kid = cLISTOPo->op_first->op_sibling;
    cLISTOPo->op_first->op_sibling = kid->op_sibling; /* bypass old block */
    op_free(kid);				      /* then delete it */
}

OP *
Perl_ck_split(pTHX_ OP *o)
{
    register OP *kid;

    if (o->op_flags & OPf_STACKED)
	return no_fh_allowed(o);

    kid = cLISTOPo->op_first;
    if (kid->op_type != OP_NULL)
	Perl_croak(aTHX_ "panic: ck_split");
    kid = kid->op_sibling;
    op_free(cLISTOPo->op_first);
    cLISTOPo->op_first = kid;
    if (!kid) {
	cLISTOPo->op_first = kid = newSVOP(OP_CONST, 0, newSVpvn(" ", 1));
	cLISTOPo->op_last = kid; /* There was only one element previously */
    }

    if (kid->op_type != OP_MATCH || kid->op_flags & OPf_STACKED) {
	OP *sibl = kid->op_sibling;
	kid->op_sibling = 0;
	kid = pmruntime( newPMOP(OP_MATCH, OPf_SPECIAL), kid, Nullop);
	if (cLISTOPo->op_first == cLISTOPo->op_last)
	    cLISTOPo->op_last = kid;
	cLISTOPo->op_first = kid;
	kid->op_sibling = sibl;
    }

    kid->op_type = OP_PUSHRE;
    kid->op_ppaddr = PL_ppaddr[OP_PUSHRE];
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
	return too_many_arguments(o,PL_op_desc[o->op_type]);

    return o;
}

OP *
Perl_ck_join(pTHX_ OP *o) 
{
    if (ckWARN(WARN_SYNTAX)) {
	OP *kid = cLISTOPo->op_first->op_sibling;
	if (kid && kid->op_type == OP_MATCH) {
	    char *pmstr = "STRING";
	    if (kPMOP->op_pmregexp)
		pmstr = kPMOP->op_pmregexp->precomp;
	    Perl_warner(aTHX_ WARN_SYNTAX,
			"/%s/ should probably be written as \"%s\"",
			pmstr, pmstr);
	}
    }
    return ck_fun(o);
}

OP *
Perl_ck_subr(pTHX_ OP *o)
{
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

    o->op_private |= OPpENTERSUB_HASTARG;
    for (cvop = o2; cvop->op_sibling; cvop = cvop->op_sibling) ;
    if (cvop->op_type == OP_RV2CV) {
	SVOP* tmpop;
	o->op_private |= (cvop->op_private & OPpENTERSUB_AMPER);
	null(cvop);		/* disable rv2cv */
	tmpop = (SVOP*)((UNOP*)cvop)->op_first;
	if (tmpop->op_type == OP_GV && !(o->op_private & OPpENTERSUB_AMPER)) {
	    GV *gv = cGVOPx_gv(tmpop);
	    cv = GvCVu(gv);
	    if (!cv)
		tmpop->op_private |= OPpEARLY_CV;
	    else if (SvPOK(cv)) {
		namegv = CvANON(cv) ? gv : CvGV(cv);
		proto = SvPV((SV*)cv, n_a);
	    }
	}
    }
    else if (cvop->op_type == OP_METHOD || cvop->op_type == OP_METHOD_NAMED) {
	if (o2->op_type == OP_CONST)
	    o2->op_private &= ~OPpCONST_STRICT;
	else if (o2->op_type == OP_LIST) {
	    OP *o = ((UNOP*)o2)->op_first->op_sibling;
	    if (o && o->op_type == OP_CONST)
		o->op_private &= ~OPpCONST_STRICT;
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
		    bad_type(arg,
			arg == 1 ? "block or sub {}" : "sub {}",
			gv_ename(namegv), o2);
		break;
	    case '*':
		/* '*' allows any scalar type, including bareword */
		proto++;
		arg++;
		if (o2->op_type == OP_RV2GV)
		    goto wrapref;	/* autoconvert GLOB -> GLOBref */
		else if (o2->op_type == OP_CONST)
		    o2->op_private &= ~OPpCONST_STRICT;
		else if (o2->op_type == OP_ENTERSUB) {
		    /* accidental subroutine, revert to bareword */
		    OP *gvop = ((UNOP*)o2)->op_first;
		    if (gvop && gvop->op_type == OP_NULL) {
			gvop = ((UNOP*)gvop)->op_first;
			if (gvop) {
			    for (; gvop->op_sibling; gvop = gvop->op_sibling)
				;
			    if (gvop &&
				(gvop->op_private & OPpENTERSUB_NOPAREN) &&
				(gvop = ((UNOP*)gvop)->op_first) &&
				gvop->op_type == OP_GV)
			    {
				GV *gv = cGVOPx_gv(gvop);
				OP *sibling = o2->op_sibling;
				SV *n = newSVpvn("",0);
				op_free(o2);
				gv_fullname3(n, gv, "");
				if (SvCUR(n)>6 && strnEQ(SvPVX(n),"main::",6))
				    sv_chop(n, SvPVX(n)+6);
				o2 = newSVOP(OP_CONST, 0, n);
				prev->op_sibling = o2;
				o2->op_sibling = sibling;
			    }
			}
		    }
		}
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
		    if (o2->op_type != OP_ENTERSUB)
			bad_type(arg, "subroutine entry", gv_ename(namegv), o2);
		    goto wrapref;
		case '$':
		    if (o2->op_type != OP_RV2SV
			&& o2->op_type != OP_PADSV
			&& o2->op_type != OP_HELEM
			&& o2->op_type != OP_AELEM
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
		Perl_croak(aTHX_ "Malformed prototype for %s: %s",
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
Perl_ck_svconst(pTHX_ OP *o)
{
    SvREADONLY_on(cSVOPo->op_sv);
    return o;
}

OP *
Perl_ck_trunc(pTHX_ OP *o)
{
    if (o->op_flags & OPf_KIDS) {
	SVOP *kid = (SVOP*)cUNOPo->op_first;

	if (kid->op_type == OP_NULL)
	    kid = (SVOP*)kid->op_sibling;
	if (kid && kid->op_type == OP_CONST &&
	    (kid->op_private & OPpCONST_BARE))
	{
	    o->op_flags |= OPf_SPECIAL;
	    kid->op_private &= ~OPpCONST_STRICT;
	}
    }
    return ck_fun(o);
}

OP *
Perl_ck_substr(pTHX_ OP *o)
{
    o = ck_fun(o);
    if ((o->op_flags & OPf_KIDS) && o->op_private == 4) {
	OP *kid = cLISTOPo->op_first;

	if (kid->op_type == OP_NULL)
	    kid = kid->op_sibling;
	if (kid)
	    kid->op_flags |= OPf_MOD;

    }
    return o;
}

/* A peephole optimizer.  We visit the ops in the order they're to execute. */

void
Perl_peep(pTHX_ register OP *o)
{
    register OP* oldop = 0;
    STRLEN n_a;

    if (!o || o->op_seq)
	return;
    ENTER;
    SAVEOP();
    SAVEVPTR(PL_curcop);
    for (; o; o = o->op_next) {
	if (o->op_seq)
	    break;
	if (!PL_op_seqmax)
	    PL_op_seqmax++;
	PL_op = o;
	switch (o->op_type) {
	case OP_SETSTATE:
	case OP_NEXTSTATE:
	case OP_DBSTATE:
	    PL_curcop = ((COP*)o);		/* for warnings */
	    o->op_seq = PL_op_seqmax++;
	    break;

	case OP_CONST:
	    if (cSVOPo->op_private & OPpCONST_STRICT)
		no_bareword_allowed(o);
#ifdef USE_ITHREADS
	    /* Relocate sv to the pad for thread safety.
	     * Despite being a "constant", the SV is written to,
	     * for reference counts, sv_upgrade() etc. */
	    if (cSVOP->op_sv) {
		PADOFFSET ix = pad_alloc(OP_CONST, SVs_PADTMP);
		if (SvPADTMP(cSVOPo->op_sv)) {
		    /* If op_sv is already a PADTMP then it is being used by
		     * another pad, so make a copy. */
		    sv_setsv(PL_curpad[ix],cSVOPo->op_sv);
		    SvREADONLY_on(PL_curpad[ix]);
		    SvREFCNT_dec(cSVOPo->op_sv);
		}
		else {
		    SvREFCNT_dec(PL_curpad[ix]);
		    SvPADTMP_on(cSVOPo->op_sv);
		    PL_curpad[ix] = cSVOPo->op_sv;
		}
		cSVOPo->op_sv = Nullsv;
		o->op_targ = ix;
	    }
#endif
	    o->op_seq = PL_op_seqmax++;
	    break;

	case OP_CONCAT:
	    if (o->op_next && o->op_next->op_type == OP_STRINGIFY) {
		if (o->op_next->op_private & OPpTARGET_MY) {
		    if (o->op_flags & OPf_STACKED) /* chained concats */
			goto ignore_optimization;
		    else {
			/* assert(PL_opargs[o->op_type] & OA_TARGLEX); */
			o->op_targ = o->op_next->op_targ;
			o->op_next->op_targ = 0;
			o->op_private |= OPpTARGET_MY;
		    }
		}
		null(o->op_next);
	    }
	  ignore_optimization:
	    o->op_seq = PL_op_seqmax++;
	    break;
	case OP_STUB:
	    if ((o->op_flags & OPf_WANT) != OPf_WANT_LIST) {
		o->op_seq = PL_op_seqmax++;
		break; /* Scalar stub must produce undef.  List stub is noop */
	    }
	    goto nothin;
	case OP_NULL:
	    if (o->op_targ == OP_NEXTSTATE
		|| o->op_targ == OP_DBSTATE
		|| o->op_targ == OP_SETSTATE)
	    {
		PL_curcop = ((COP*)o);
	    }
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
		    o->op_private |= o->op_next->op_private & (OPpLVAL_INTRO
							       | OPpOUR_INTRO);
		    o->op_next = o->op_next->op_next;
		    o->op_type = OP_GVSV;
		    o->op_ppaddr = PL_ppaddr[OP_GVSV];
		}
	    }
	    else if (o->op_next->op_type == OP_RV2AV) {
		OP* pop = o->op_next->op_next;
		IV i;
		if (pop->op_type == OP_CONST &&
		    (PL_op = pop->op_next) &&
		    pop->op_next->op_type == OP_AELEM &&
		    !(pop->op_next->op_private &
		      (OPpLVAL_INTRO|OPpLVAL_DEFER|OPpDEREF|OPpMAYBE_LVSUB)) &&
		    (i = SvIV(((SVOP*)pop)->op_sv) - PL_compiling.cop_arybase)
				<= 255 &&
		    i >= 0)
		{
		    GV *gv;
		    null(o->op_next);
		    null(pop->op_next);
		    null(pop);
		    o->op_flags |= pop->op_next->op_flags & OPf_MOD;
		    o->op_next = pop->op_next->op_next;
		    o->op_type = OP_AELEMFAST;
		    o->op_ppaddr = PL_ppaddr[OP_AELEMFAST];
		    o->op_private = (U8)i;
		    gv = cGVOPo_gv;
		    GvAVn(gv);
		}
	    }
	    else if ((o->op_private & OPpEARLY_CV) && ckWARN(WARN_PROTOTYPE)) {
		GV *gv = cGVOPo_gv;
		if (SvTYPE(gv) == SVt_PVGV && GvCV(gv) && SvPVX(GvCV(gv))) {
		    /* XXX could check prototype here instead of just carping */
		    SV *sv = sv_newmortal();
		    gv_efullname3(sv, gv, Nullch);
		    Perl_warner(aTHX_ WARN_PROTOTYPE,
				"%s() called too early to check prototype",
				SvPV_nolen(sv));
		}
	    }

	    o->op_seq = PL_op_seqmax++;
	    break;

	case OP_MAPWHILE:
	case OP_GREPWHILE:
	case OP_AND:
	case OP_OR:
	case OP_ANDASSIGN:
	case OP_ORASSIGN:
	case OP_COND_EXPR:
	case OP_RANGE:
	    o->op_seq = PL_op_seqmax++;
	    while (cLOGOP->op_other->op_type == OP_NULL)
		cLOGOP->op_other = cLOGOP->op_other->op_next;
	    peep(cLOGOP->op_other);
	    break;

	case OP_ENTERLOOP:
	case OP_ENTERITER:
	    o->op_seq = PL_op_seqmax++;
	    while (cLOOP->op_redoop->op_type == OP_NULL)
		cLOOP->op_redoop = cLOOP->op_redoop->op_next;
	    peep(cLOOP->op_redoop);
	    while (cLOOP->op_nextop->op_type == OP_NULL)
		cLOOP->op_nextop = cLOOP->op_nextop->op_next;
	    peep(cLOOP->op_nextop);
	    while (cLOOP->op_lastop->op_type == OP_NULL)
		cLOOP->op_lastop = cLOOP->op_lastop->op_next;
	    peep(cLOOP->op_lastop);
	    break;

	case OP_QR:
	case OP_MATCH:
	case OP_SUBST:
	    o->op_seq = PL_op_seqmax++;
	    while (cPMOP->op_pmreplstart && 
		   cPMOP->op_pmreplstart->op_type == OP_NULL)
		cPMOP->op_pmreplstart = cPMOP->op_pmreplstart->op_next;
	    peep(cPMOP->op_pmreplstart);
	    break;

	case OP_EXEC:
	    o->op_seq = PL_op_seqmax++;
	    if (ckWARN(WARN_SYNTAX) && o->op_next 
		&& o->op_next->op_type == OP_NEXTSTATE) {
		if (o->op_next->op_sibling &&
			o->op_next->op_sibling->op_type != OP_EXIT &&
			o->op_next->op_sibling->op_type != OP_WARN &&
			o->op_next->op_sibling->op_type != OP_DIE) {
		    line_t oldline = CopLINE(PL_curcop);

		    CopLINE_set(PL_curcop, CopLINE((COP*)o->op_next));
		    Perl_warner(aTHX_ WARN_EXEC,
				"Statement unlikely to be reached");
		    Perl_warner(aTHX_ WARN_EXEC,
				"\t(Maybe you meant system() when you said exec()?)\n");
		    CopLINE_set(PL_curcop, oldline);
		}
	    }
	    break;
	
	case OP_HELEM: {
	    UNOP *rop;
	    SV *lexname;
	    GV **fields;
	    SV **svp, **indsvp, *sv;
	    I32 ind;
	    char *key;
	    STRLEN keylen;
	
	    o->op_seq = PL_op_seqmax++;
	    if ((o->op_private & (OPpLVAL_INTRO))
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
	    svp = cSVOPx_svp(((BINOP*)o)->op_last);
	    key = SvPV(*svp, keylen);
	    indsvp = hv_fetch(GvHV(*fields), key, keylen, FALSE);
	    if (!indsvp) {
		Perl_croak(aTHX_ "No such pseudo-hash field \"%s\" in variable %s of type %s",
		      key, SvPV(lexname, n_a), HvNAME(SvSTASH(lexname)));
	    }
	    ind = SvIV(*indsvp);
	    if (ind < 1)
		Perl_croak(aTHX_ "Bad index while coercing array into hash");
	    rop->op_type = OP_RV2AV;
	    rop->op_ppaddr = PL_ppaddr[OP_RV2AV];
	    o->op_type = OP_AELEM;
	    o->op_ppaddr = PL_ppaddr[OP_AELEM];
	    sv = newSViv(ind);
	    if (SvREADONLY(*svp))
		SvREADONLY_on(sv);
	    SvFLAGS(sv) |= (SvFLAGS(*svp)
			    & (SVs_PADBUSY|SVs_PADTMP|SVs_PADMY));
	    SvREFCNT_dec(*svp);
	    *svp = sv;
	    break;
	}
	
	case OP_HSLICE: {
	    UNOP *rop;
	    SV *lexname;
	    GV **fields;
	    SV **svp, **indsvp, *sv;
	    I32 ind;
	    char *key;
	    STRLEN keylen;
	    SVOP *first_key_op, *key_op;

	    o->op_seq = PL_op_seqmax++;
	    if ((o->op_private & (OPpLVAL_INTRO))
		/* I bet there's always a pushmark... */
		|| ((LISTOP*)o)->op_first->op_sibling->op_type != OP_LIST)
		/* hmmm, no optimization if list contains only one key. */
		break;
	    rop = (UNOP*)((LISTOP*)o)->op_last;
	    if (rop->op_type != OP_RV2HV || rop->op_first->op_type != OP_PADSV)
		break;
	    lexname = *av_fetch(PL_comppad_name, rop->op_first->op_targ, TRUE);
	    if (!SvOBJECT(lexname))
		break;
	    fields = (GV**)hv_fetch(SvSTASH(lexname), "FIELDS", 6, FALSE);
	    if (!fields || !GvHV(*fields))
		break;
	    /* Again guessing that the pushmark can be jumped over.... */
	    first_key_op = (SVOP*)((LISTOP*)((LISTOP*)o)->op_first->op_sibling)
		->op_first->op_sibling;
	    /* Check that the key list contains only constants. */
	    for (key_op = first_key_op; key_op;
		 key_op = (SVOP*)key_op->op_sibling)
		if (key_op->op_type != OP_CONST)
		    break;
	    if (key_op)
		break;
	    rop->op_type = OP_RV2AV;
	    rop->op_ppaddr = PL_ppaddr[OP_RV2AV];
	    o->op_type = OP_ASLICE;
	    o->op_ppaddr = PL_ppaddr[OP_ASLICE];
	    for (key_op = first_key_op; key_op;
		 key_op = (SVOP*)key_op->op_sibling) {
		svp = cSVOPx_svp(key_op);
		key = SvPV(*svp, keylen);
		indsvp = hv_fetch(GvHV(*fields), key, keylen, FALSE);
		if (!indsvp) {
		    Perl_croak(aTHX_ "No such pseudo-hash field \"%s\" "
			       "in variable %s of type %s",
			  key, SvPV(lexname, n_a), HvNAME(SvSTASH(lexname)));
		}
		ind = SvIV(*indsvp);
		if (ind < 1)
		    Perl_croak(aTHX_ "Bad index while coercing array into hash");
		sv = newSViv(ind);
		if (SvREADONLY(*svp))
		    SvREADONLY_on(sv);
		SvFLAGS(sv) |= (SvFLAGS(*svp)
				& (SVs_PADBUSY|SVs_PADTMP|SVs_PADMY));
		SvREFCNT_dec(*svp);
		*svp = sv;
	    }
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
