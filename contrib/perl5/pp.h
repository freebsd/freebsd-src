/*    pp.h
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#ifdef USE_THREADS
#define ARGS thr
#define dARGS struct perl_thread *thr;
#else
#define ARGS
#define dARGS
#endif /* USE_THREADS */
#ifdef PERL_OBJECT
#define PP(s) OP * CPerlObj::s(ARGSproto)
#else
#define PP(s) OP * s(ARGSproto)
#endif

#define SP sp
#define MARK mark
#define TARG targ

#define PUSHMARK(p) if (++PL_markstack_ptr == PL_markstack_max)	\
			markstack_grow();			\
		    *PL_markstack_ptr = (p) - PL_stack_base

#define TOPMARK		(*PL_markstack_ptr)
#define POPMARK		(*PL_markstack_ptr--)

#define djSP		register SV **sp = PL_stack_sp
#define dSP		dTHR; djSP
#define dMARK		register SV **mark = PL_stack_base + POPMARK
#define dORIGMARK	I32 origmark = mark - PL_stack_base
#define SETORIGMARK	origmark = mark - PL_stack_base
#define ORIGMARK	(PL_stack_base + origmark)

#define SPAGAIN		sp = PL_stack_sp
#define MSPAGAIN	sp = PL_stack_sp; mark = ORIGMARK

#define GETTARGETSTACKED targ = (PL_op->op_flags & OPf_STACKED ? POPs : PAD_SV(PL_op->op_targ))
#define dTARGETSTACKED SV * GETTARGETSTACKED

#define GETTARGET targ = PAD_SV(PL_op->op_targ)
#define dTARGET SV * GETTARGET

#define GETATARGET targ = (PL_op->op_flags & OPf_STACKED ? sp[-1] : PAD_SV(PL_op->op_targ))
#define dATARGET SV * GETATARGET

#define dTARG SV *targ

#define NORMAL PL_op->op_next
#define DIE return die

#define PUTBACK		PL_stack_sp = sp
#define RETURN		return PUTBACK, NORMAL
#define RETURNOP(o)	return PUTBACK, o
#define RETURNX(x)	return x, PUTBACK, NORMAL

#define POPs		(*sp--)
#define POPp		(SvPVx(POPs, PL_na))		/* deprecated */
#define POPpx		(SvPVx(POPs, n_a))
#define POPn		(SvNVx(POPs))
#define POPi		((IV)SvIVx(POPs))
#define POPu		((UV)SvUVx(POPs))
#define POPl		((long)SvIVx(POPs))

#define TOPs		(*sp)
#define TOPp		(SvPV(TOPs, PL_na))		/* deprecated */
#define TOPpx		(SvPV(TOPs, n_a))
#define TOPn		(SvNV(TOPs))
#define TOPi		((IV)SvIV(TOPs))
#define TOPu		((UV)SvUV(TOPs))
#define TOPl		((long)SvIV(TOPs))

/* Go to some pains in the rare event that we must extend the stack. */
#define EXTEND(p,n)	STMT_START { if (PL_stack_max - p < (n)) {		\
			    sp = stack_grow(sp,p, (int) (n));		\
			} } STMT_END

/* Same thing, but update mark register too. */
#define MEXTEND(p,n)	STMT_START {if (PL_stack_max - p < (n)) {		\
			    int markoff = mark - PL_stack_base;		\
			    sp = stack_grow(sp,p,(int) (n));		\
			    mark = PL_stack_base + markoff;		\
			} } STMT_END

#define PUSHs(s)	(*++sp = (s))
#define PUSHTARG	STMT_START { SvSETMAGIC(TARG); PUSHs(TARG); } STMT_END
#define PUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); PUSHTARG; } STMT_END
#define PUSHn(n)	STMT_START { sv_setnv(TARG, (double)(n)); PUSHTARG; } STMT_END
#define PUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); PUSHTARG; } STMT_END
#define PUSHu(u)	STMT_START { sv_setuv(TARG, (UV)(u)); PUSHTARG; } STMT_END

#define XPUSHs(s)	STMT_START { EXTEND(sp,1); (*++sp = (s)); } STMT_END
#define XPUSHTARG	STMT_START { SvSETMAGIC(TARG); XPUSHs(TARG); } STMT_END
#define XPUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); XPUSHTARG; } STMT_END
#define XPUSHn(n)	STMT_START { sv_setnv(TARG, (double)(n)); XPUSHTARG; } STMT_END
#define XPUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); XPUSHTARG; } STMT_END
#define XPUSHu(u)	STMT_START { sv_setuv(TARG, (UV)(u)); XPUSHTARG; } STMT_END

#define SETs(s)		(*sp = s)
#define SETTARG		STMT_START { SvSETMAGIC(TARG); SETs(TARG); } STMT_END
#define SETp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); SETTARG; } STMT_END
#define SETn(n)		STMT_START { sv_setnv(TARG, (double)(n)); SETTARG; } STMT_END
#define SETi(i)		STMT_START { sv_setiv(TARG, (IV)(i)); SETTARG; } STMT_END
#define SETu(u)		STMT_START { sv_setuv(TARG, (UV)(u)); SETTARG; } STMT_END

#define dTOPss		SV *sv = TOPs
#define dPOPss		SV *sv = POPs
#define dTOPnv		double value = TOPn
#define dPOPnv		double value = POPn
#define dTOPiv		IV value = TOPi
#define dPOPiv		IV value = POPi
#define dTOPuv		UV value = TOPu
#define dPOPuv		UV value = POPu

#define dPOPXssrl(X)	SV *right = POPs; SV *left = CAT2(X,s)
#define dPOPXnnrl(X)	double right = POPn; double left = CAT2(X,n)
#define dPOPXiirl(X)	IV right = POPi; IV left = CAT2(X,i)

#define USE_LEFT(sv) \
	(SvOK(sv) || SvGMAGICAL(sv) || !(PL_op->op_flags & OPf_STACKED))
#define dPOPXnnrl_ul(X)	\
    double right = POPn;				\
    SV *leftsv = CAT2(X,s);				\
    double left = USE_LEFT(leftsv) ? SvNV(leftsv) : 0.0
#define dPOPXiirl_ul(X) \
    IV right = POPi;					\
    SV *leftsv = CAT2(X,s);				\
    IV left = USE_LEFT(leftsv) ? SvIV(leftsv) : 0

#define dPOPPOPssrl	dPOPXssrl(POP)
#define dPOPPOPnnrl	dPOPXnnrl(POP)
#define dPOPPOPnnrl_ul	dPOPXnnrl_ul(POP)
#define dPOPPOPiirl	dPOPXiirl(POP)
#define dPOPPOPiirl_ul	dPOPXiirl_ul(POP)

#define dPOPTOPssrl	dPOPXssrl(TOP)
#define dPOPTOPnnrl	dPOPXnnrl(TOP)
#define dPOPTOPnnrl_ul	dPOPXnnrl_ul(TOP)
#define dPOPTOPiirl	dPOPXiirl(TOP)
#define dPOPTOPiirl_ul	dPOPXiirl_ul(TOP)

#define RETPUSHYES	RETURNX(PUSHs(&PL_sv_yes))
#define RETPUSHNO	RETURNX(PUSHs(&PL_sv_no))
#define RETPUSHUNDEF	RETURNX(PUSHs(&PL_sv_undef))

#define RETSETYES	RETURNX(SETs(&PL_sv_yes))
#define RETSETNO	RETURNX(SETs(&PL_sv_no))
#define RETSETUNDEF	RETURNX(SETs(&PL_sv_undef))

#define ARGTARG		PL_op->op_targ
#define MAXARG		PL_op->op_private

#define SWITCHSTACK(f,t) \
    STMT_START {							\
	AvFILLp(f) = sp - PL_stack_base;					\
	PL_stack_base = AvARRAY(t);					\
	PL_stack_max = PL_stack_base + AvMAX(t);				\
	sp = PL_stack_sp = PL_stack_base + AvFILLp(t);			\
	PL_curstack = t;							\
    } STMT_END

#define EXTEND_MORTAL(n) \
	STMT_START { \
	    if (PL_tmps_ix + (n) >= PL_tmps_max) \
		Renew(PL_tmps_stack, PL_tmps_max = PL_tmps_ix + (n) + 1, SV*); \
	} STMT_END

#ifdef OVERLOAD

#define AMGf_noright	1
#define AMGf_noleft	2
#define AMGf_assign	4
#define AMGf_unary	8

#define tryAMAGICbinW(meth,assign,set) STMT_START { \
          if (PL_amagic_generation) { \
	    SV* tmpsv; \
	    SV* right= *(sp); SV* left= *(sp-1);\
	    if ((SvAMAGIC(left)||SvAMAGIC(right))&&\
		(tmpsv=amagic_call(left, \
				   right, \
				   CAT2(meth,_amg), \
				   (assign)? AMGf_assign: 0))) {\
	       SPAGAIN;	\
	       (void)POPs; set(tmpsv); RETURN; } \
	  } \
	} STMT_END

#define tryAMAGICbin(meth,assign) tryAMAGICbinW(meth,assign,SETsv)
#define tryAMAGICbinSET(meth,assign) tryAMAGICbinW(meth,assign,SETs)

#define AMG_CALLun(sv,meth) amagic_call(sv,&PL_sv_undef,  \
					CAT2(meth,_amg),AMGf_noright | AMGf_unary)
#define AMG_CALLbinL(left,right,meth) \
            amagic_call(left,right,CAT2(meth,_amg),AMGf_noright)

#define tryAMAGICunW(meth,set) STMT_START { \
          if (PL_amagic_generation) { \
	    SV* tmpsv; \
	    SV* arg= *(sp); \
	    if ((SvAMAGIC(arg))&&\
		(tmpsv=AMG_CALLun(arg,meth))) {\
	       SPAGAIN;	\
	       set(tmpsv); RETURN; } \
	  } \
	} STMT_END

#define tryAMAGICun	tryAMAGICunSET
#define tryAMAGICunSET(meth) tryAMAGICunW(meth,SETs)

#define opASSIGN (PL_op->op_flags & OPf_STACKED)
#define SETsv(sv)	STMT_START {					\
		if (opASSIGN) { sv_setsv(TARG, (sv)); SETTARG; }	\
		else SETs(sv); } STMT_END

/* newSVsv does not behave as advertised, so we copy missing
 * information by hand */

/* SV* ref causes confusion with the member variable
   changed SV* ref to SV* tmpRef */
#define RvDEEPCP(rv) STMT_START { SV* tmpRef=SvRV(rv);      \
  if (SvREFCNT(tmpRef)>1) {                 \
    SvREFCNT_dec(tmpRef);                   \
    SvRV(rv)=AMG_CALLun(rv,copy);        \
  } } STMT_END
#else

#define tryAMAGICbin(a,b)
#define tryAMAGICbinSET(a,b)
#define tryAMAGICun(a)
#define tryAMAGICunSET(a)

#endif /* OVERLOAD */
