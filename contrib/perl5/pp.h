/*    pp.h
 *
 *    Copyright (c) 1991-2001, Larry Wall
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

#define PP(s) OP * Perl_##s(pTHX)

/*
=for apidoc AmU||SP
Stack pointer.  This is usually handled by C<xsubpp>.  See C<dSP> and
C<SPAGAIN>.

=for apidoc AmU||MARK
Stack marker variable for the XSUB.  See C<dMARK>.

=for apidoc Ams||PUSHMARK
Opening bracket for arguments on a callback.  See C<PUTBACK> and
L<perlcall>.

=for apidoc Ams||dSP
Declares a local copy of perl's stack pointer for the XSUB, available via
the C<SP> macro.  See C<SP>.

=for apidoc Ams||dMARK
Declare a stack marker variable, C<mark>, for the XSUB.  See C<MARK> and
C<dORIGMARK>.

=for apidoc Ams||dORIGMARK
Saves the original stack mark for the XSUB.  See C<ORIGMARK>.

=for apidoc AmU||ORIGMARK
The original stack mark for the XSUB.  See C<dORIGMARK>.

=for apidoc Ams||SPAGAIN
Refetch the stack pointer.  Used after a callback.  See L<perlcall>.

=cut
*/

#undef SP /* Solaris 2.7 i386 has this in /usr/include/sys/reg.h */
#define SP sp
#define MARK mark
#define TARG targ

#define PUSHMARK(p) if (++PL_markstack_ptr == PL_markstack_max)	\
			markstack_grow();			\
		    *PL_markstack_ptr = (p) - PL_stack_base

#define TOPMARK		(*PL_markstack_ptr)
#define POPMARK		(*PL_markstack_ptr--)

#define dSP		register SV **sp = PL_stack_sp
#define djSP		dSP
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
#define DIE return Perl_die

/*
=for apidoc Ams||PUTBACK
Closing bracket for XSUB arguments.  This is usually handled by C<xsubpp>.
See C<PUSHMARK> and L<perlcall> for other uses.

=for apidoc Amn|SV*|POPs
Pops an SV off the stack.

=for apidoc Amn|char*|POPp
Pops a string off the stack.

=for apidoc Amn|NV|POPn
Pops a double off the stack.

=for apidoc Amn|IV|POPi
Pops an integer off the stack.

=for apidoc Amn|long|POPl
Pops a long off the stack.

=cut
*/

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
#define POPul		((unsigned long)SvIVx(POPs))
#ifdef HAS_QUAD
#define POPq		((Quad_t)SvIVx(POPs))
#define POPuq		((Uquad_t)SvUVx(POPs))
#endif

#define TOPs		(*sp)
#define TOPp		(SvPV(TOPs, PL_na))		/* deprecated */
#define TOPpx		(SvPV(TOPs, n_a))
#define TOPn		(SvNV(TOPs))
#define TOPi		((IV)SvIV(TOPs))
#define TOPu		((UV)SvUV(TOPs))
#define TOPl		((long)SvIV(TOPs))
#define TOPul		((unsigned long)SvUV(TOPs))
#ifdef HAS_QUAD
#define TOPq		((Quad_t)SvIV(TOPs))
#define TOPuq		((Uquad_t)SvUV(TOPs))
#endif

/* Go to some pains in the rare event that we must extend the stack. */

/*
=for apidoc Am|void|EXTEND|SP|int nitems
Used to extend the argument stack for an XSUB's return values. Once
used, guarantees that there is room for at least C<nitems> to be pushed
onto the stack.

=for apidoc Am|void|PUSHs|SV* sv
Push an SV onto the stack.  The stack must have room for this element.
Does not handle 'set' magic.  See C<XPUSHs>.

=for apidoc Am|void|PUSHp|char* str|STRLEN len
Push a string onto the stack.  The stack must have room for this element.
The C<len> indicates the length of the string.  Handles 'set' magic.  See
C<XPUSHp>.

=for apidoc Am|void|PUSHn|NV nv
Push a double onto the stack.  The stack must have room for this element.
Handles 'set' magic.  See C<XPUSHn>.

=for apidoc Am|void|PUSHi|IV iv
Push an integer onto the stack.  The stack must have room for this element.
Handles 'set' magic.  See C<XPUSHi>.

=for apidoc Am|void|PUSHu|UV uv
Push an unsigned integer onto the stack.  The stack must have room for this
element.  See C<XPUSHu>.

=for apidoc Am|void|XPUSHs|SV* sv
Push an SV onto the stack, extending the stack if necessary.  Does not
handle 'set' magic.  See C<PUSHs>.

=for apidoc Am|void|XPUSHp|char* str|STRLEN len
Push a string onto the stack, extending the stack if necessary.  The C<len>
indicates the length of the string.  Handles 'set' magic.  See
C<PUSHp>.

=for apidoc Am|void|XPUSHn|NV nv
Push a double onto the stack, extending the stack if necessary.  Handles
'set' magic.  See C<PUSHn>.

=for apidoc Am|void|XPUSHi|IV iv
Push an integer onto the stack, extending the stack if necessary.  Handles
'set' magic. See C<PUSHi>.

=for apidoc Am|void|XPUSHu|UV uv
Push an unsigned integer onto the stack, extending the stack if necessary.
See C<PUSHu>.

=cut
*/

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
#define PUSHn(n)	STMT_START { sv_setnv(TARG, (NV)(n)); PUSHTARG; } STMT_END
#define PUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); PUSHTARG; } STMT_END
#define PUSHu(u)	STMT_START { sv_setuv(TARG, (UV)(u)); PUSHTARG; } STMT_END

#define XPUSHs(s)	STMT_START { EXTEND(sp,1); (*++sp = (s)); } STMT_END
#define XPUSHTARG	STMT_START { SvSETMAGIC(TARG); XPUSHs(TARG); } STMT_END
#define XPUSHp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); XPUSHTARG; } STMT_END
#define XPUSHn(n)	STMT_START { sv_setnv(TARG, (NV)(n)); XPUSHTARG; } STMT_END
#define XPUSHi(i)	STMT_START { sv_setiv(TARG, (IV)(i)); XPUSHTARG; } STMT_END
#define XPUSHu(u)	STMT_START { sv_setuv(TARG, (UV)(u)); XPUSHTARG; } STMT_END
#define XPUSHundef	STMT_START { SvOK_off(TARG); XPUSHs(TARG); } STMT_END

#define SETs(s)		(*sp = s)
#define SETTARG		STMT_START { SvSETMAGIC(TARG); SETs(TARG); } STMT_END
#define SETp(p,l)	STMT_START { sv_setpvn(TARG, (p), (l)); SETTARG; } STMT_END
#define SETn(n)		STMT_START { sv_setnv(TARG, (NV)(n)); SETTARG; } STMT_END
#define SETi(i)		STMT_START { sv_setiv(TARG, (IV)(i)); SETTARG; } STMT_END
#define SETu(u)		STMT_START { sv_setuv(TARG, (UV)(u)); SETTARG; } STMT_END

#define dTOPss		SV *sv = TOPs
#define dPOPss		SV *sv = POPs
#define dTOPnv		NV value = TOPn
#define dPOPnv		NV value = POPn
#define dTOPiv		IV value = TOPi
#define dPOPiv		IV value = POPi
#define dTOPuv		UV value = TOPu
#define dPOPuv		UV value = POPu
#ifdef HAS_QUAD
#define dTOPqv		Quad_t value = TOPu
#define dPOPqv		Quad_t value = POPu
#define dTOPuqv		Uquad_t value = TOPuq
#define dPOPuqv		Uquad_t value = POPuq
#endif

#define dPOPXssrl(X)	SV *right = POPs; SV *left = CAT2(X,s)
#define dPOPXnnrl(X)	NV right = POPn; NV left = CAT2(X,n)
#define dPOPXiirl(X)	IV right = POPi; IV left = CAT2(X,i)

#define USE_LEFT(sv) \
	(SvOK(sv) || SvGMAGICAL(sv) || !(PL_op->op_flags & OPf_STACKED))
#define dPOPXnnrl_ul(X)	\
    NV right = POPn;				\
    SV *leftsv = CAT2(X,s);				\
    NV left = USE_LEFT(leftsv) ? SvNV(leftsv) : 0.0
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

    /* See OPpTARGET_MY: */
#define MAXARG		(PL_op->op_private & 15)

#define SWITCHSTACK(f,t) \
    STMT_START {							\
	AvFILLp(f) = sp - PL_stack_base;				\
	PL_stack_base = AvARRAY(t);					\
	PL_stack_max = PL_stack_base + AvMAX(t);			\
	sp = PL_stack_sp = PL_stack_base + AvFILLp(t);			\
	PL_curstack = t;						\
    } STMT_END

#define EXTEND_MORTAL(n) \
    STMT_START {							\
	if (PL_tmps_ix + (n) >= PL_tmps_max)				\
	    tmps_grow(n);						\
    } STMT_END

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

#define tryAMAGICunW(meth,set,shift,ret) STMT_START { \
          if (PL_amagic_generation) { \
	    SV* tmpsv; \
	    SV* arg= sp[shift]; \
	  am_again: \
	    if ((SvAMAGIC(arg))&&\
		(tmpsv=AMG_CALLun(arg,meth))) {\
	       SPAGAIN; if (shift) sp += shift; \
	       set(tmpsv); ret; } \
	  } \
	} STMT_END

#define FORCE_SETs(sv) STMT_START { sv_setsv(TARG, (sv)); SETTARG; } STMT_END

#define tryAMAGICun(meth)	tryAMAGICunW(meth,SETsvUN,0,RETURN)
#define tryAMAGICunSET(meth)	tryAMAGICunW(meth,SETs,0,RETURN)
#define tryAMAGICunTARGET(meth, shift)					\
	{ dSP; sp--; 	/* get TARGET from below PL_stack_sp */		\
	    { dTARGETSTACKED; 						\
		{ dSP; tryAMAGICunW(meth,FORCE_SETs,shift,RETURN);}}}

#define setAGAIN(ref) sv = ref;							\
  if (!SvROK(ref))								\
      Perl_croak(aTHX_ "Overloaded dereference did not return a reference");	\
  if (ref != arg && SvRV(ref) != SvRV(arg)) {					\
      arg = ref;								\
      goto am_again;								\
  }

#define tryAMAGICunDEREF(meth) tryAMAGICunW(meth,setAGAIN,0,(void)0)

#define opASSIGN (PL_op->op_flags & OPf_STACKED)
#define SETsv(sv)	STMT_START {					\
		if (opASSIGN || (SvFLAGS(TARG) & SVs_PADMY))		\
		   { sv_setsv(TARG, (sv)); SETTARG; }			\
		else SETs(sv); } STMT_END

#define SETsvUN(sv)	STMT_START {					\
		if (SvFLAGS(TARG) & SVs_PADMY)		\
		   { sv_setsv(TARG, (sv)); SETTARG; }			\
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

/*
=for apidoc mU||LVRET
True if this op will be the return value of an lvalue subroutine

=cut */
#define LVRET ((PL_op->op_private & OPpMAYBE_LVSUB) && is_lvalue_sub())
