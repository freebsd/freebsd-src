/*    cop.h
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

struct cop {
    BASEOP
    char *	cop_label;	/* label for this construct */
#ifdef USE_ITHREADS
    char *	cop_stashpv;	/* package line was compiled in */
    char *	cop_file;	/* file name the following line # is from */
#else
    HV *	cop_stash;	/* package line was compiled in */
    GV *	cop_filegv;	/* file the following line # is from */
#endif
    U32		cop_seq;	/* parse sequence number */
    I32		cop_arybase;	/* array base this line was compiled with */
    line_t      cop_line;       /* line # of this command */
    SV *	cop_warnings;	/* lexical warnings bitmask */
};

#define Nullcop Null(COP*)

#ifdef USE_ITHREADS
#  define CopFILE(c)		((c)->cop_file)
#  define CopFILEGV(c)		(CopFILE(c) \
				 ? gv_fetchfile(CopFILE(c)) : Nullgv)
#  define CopFILE_set(c,pv)	((c)->cop_file = savepv(pv))
#  define CopFILESV(c)		(CopFILE(c) \
				 ? GvSV(gv_fetchfile(CopFILE(c))) : Nullsv)
#  define CopFILEAV(c)		(CopFILE(c) \
				 ? GvAV(gv_fetchfile(CopFILE(c))) : Nullav)
#  define CopSTASHPV(c)		((c)->cop_stashpv)
#  define CopSTASHPV_set(c,pv)	((c)->cop_stashpv = ((pv) ? savepv(pv) : Nullch))
#  define CopSTASH(c)		(CopSTASHPV(c) \
				 ? gv_stashpv(CopSTASHPV(c),GV_ADD) : Nullhv)
#  define CopSTASH_set(c,hv)	CopSTASHPV_set(c, (hv) ? HvNAME(hv) : Nullch)
#  define CopSTASH_eq(c,hv)	((hv) 					\
				 && (CopSTASHPV(c) == HvNAME(hv)	\
				     || (CopSTASHPV(c) && HvNAME(hv)	\
					 && strEQ(CopSTASHPV(c), HvNAME(hv)))))
#else
#  define CopFILEGV(c)		((c)->cop_filegv)
#  define CopFILEGV_set(c,gv)	((c)->cop_filegv = (GV*)SvREFCNT_inc(gv))
#  define CopFILE_set(c,pv)	CopFILEGV_set((c), gv_fetchfile(pv))
#  define CopFILESV(c)		(CopFILEGV(c) ? GvSV(CopFILEGV(c)) : Nullsv)
#  define CopFILEAV(c)		(CopFILEGV(c) ? GvAV(CopFILEGV(c)) : Nullav)
#  define CopFILE(c)		(CopFILESV(c) ? SvPVX(CopFILESV(c)) : Nullch)
#  define CopSTASH(c)		((c)->cop_stash)
#  define CopSTASH_set(c,hv)	((c)->cop_stash = (hv))
#  define CopSTASHPV(c)		(CopSTASH(c) ? HvNAME(CopSTASH(c)) : Nullch)
   /* cop_stash is not refcounted */
#  define CopSTASHPV_set(c,pv)	CopSTASH_set((c), gv_stashpv(pv,GV_ADD))
#  define CopSTASH_eq(c,hv)	(CopSTASH(c) == (hv))
#endif /* USE_ITHREADS */

#define CopSTASH_ne(c,hv)	(!CopSTASH_eq(c,hv))
#define CopLINE(c)		((c)->cop_line)
#define CopLINE_inc(c)		(++CopLINE(c))
#define CopLINE_dec(c)		(--CopLINE(c))
#define CopLINE_set(c,l)	(CopLINE(c) = (l))

/*
 * Here we have some enormously heavy (or at least ponderous) wizardry.
 */

/* subroutine context */
struct block_sub {
    CV *	cv;
    GV *	gv;
    GV *	dfoutgv;
#ifndef USE_THREADS
    AV *	savearray;
#endif /* USE_THREADS */
    AV *	argarray;
    U16		olddepth;
    U8		hasargs;
    U8		lval;		/* XXX merge lval and hasargs? */
    SV **	oldcurpad;
};

#define PUSHSUB(cx)							\
	cx->blk_sub.cv = cv;						\
	cx->blk_sub.olddepth = CvDEPTH(cv);				\
	cx->blk_sub.hasargs = hasargs;					\
	cx->blk_sub.lval = PL_op->op_private &                          \
	                      (OPpLVAL_INTRO|OPpENTERSUB_INARGS);

#define PUSHFORMAT(cx)							\
	cx->blk_sub.cv = cv;						\
	cx->blk_sub.gv = gv;						\
	cx->blk_sub.hasargs = 0;					\
	cx->blk_sub.dfoutgv = PL_defoutgv;				\
	(void)SvREFCNT_inc(cx->blk_sub.dfoutgv)

#ifdef USE_THREADS
#  define POP_SAVEARRAY() NOOP
#else
#  define POP_SAVEARRAY()						\
    STMT_START {							\
	SvREFCNT_dec(GvAV(PL_defgv));					\
	GvAV(PL_defgv) = cx->blk_sub.savearray;				\
    } STMT_END
#endif /* USE_THREADS */

/* junk in @_ spells trouble when cloning CVs and in pp_caller(), so don't
 * leave any (a fast av_clear(ary), basically) */
#define CLEAR_ARGARRAY(ary) \
    STMT_START {							\
	AvMAX(ary) += AvARRAY(ary) - AvALLOC(ary);			\
	SvPVX(ary) = (char*)AvALLOC(ary);				\
	AvFILLp(ary) = -1;						\
    } STMT_END

#define POPSUB(cx,sv)							\
    STMT_START {							\
	if (cx->blk_sub.hasargs) {					\
	    POP_SAVEARRAY();						\
	    /* abandon @_ if it got reified */				\
	    if (AvREAL(cx->blk_sub.argarray)) {				\
		SSize_t fill = AvFILLp(cx->blk_sub.argarray);		\
		SvREFCNT_dec(cx->blk_sub.argarray);			\
		cx->blk_sub.argarray = newAV();				\
		av_extend(cx->blk_sub.argarray, fill);			\
		AvFLAGS(cx->blk_sub.argarray) = AVf_REIFY;		\
		cx->blk_sub.oldcurpad[0] = (SV*)cx->blk_sub.argarray;	\
	    }								\
	    else {							\
		CLEAR_ARGARRAY(cx->blk_sub.argarray);			\
	    }								\
	}								\
	sv = (SV*)cx->blk_sub.cv;					\
	if (sv && (CvDEPTH((CV*)sv) = cx->blk_sub.olddepth))		\
	    sv = Nullsv;						\
    } STMT_END

#define LEAVESUB(sv)							\
    STMT_START {							\
	if (sv)								\
	    SvREFCNT_dec(sv);						\
    } STMT_END

#define POPFORMAT(cx)							\
	setdefout(cx->blk_sub.dfoutgv);					\
	SvREFCNT_dec(cx->blk_sub.dfoutgv);

/* eval context */
struct block_eval {
    I32		old_in_eval;
    I32		old_op_type;
    SV *	old_namesv;
    OP *	old_eval_root;
    SV *	cur_text;
};

#define PUSHEVAL(cx,n,fgv)						\
    STMT_START {							\
	cx->blk_eval.old_in_eval = PL_in_eval;				\
	cx->blk_eval.old_op_type = PL_op->op_type;			\
	cx->blk_eval.old_namesv = (n ? newSVpv(n,0) : Nullsv);		\
	cx->blk_eval.old_eval_root = PL_eval_root;			\
	cx->blk_eval.cur_text = PL_linestr;				\
    } STMT_END

#define POPEVAL(cx)							\
    STMT_START {							\
	PL_in_eval = cx->blk_eval.old_in_eval;				\
	optype = cx->blk_eval.old_op_type;				\
	PL_eval_root = cx->blk_eval.old_eval_root;			\
	if (cx->blk_eval.old_namesv)					\
	    sv_2mortal(cx->blk_eval.old_namesv);			\
    } STMT_END

/* loop context */
struct block_loop {
    char *	label;
    I32		resetsp;
    OP *	redo_op;
    OP *	next_op;
    OP *	last_op;
#ifdef USE_ITHREADS
    void *	iterdata;
    SV **	oldcurpad;
#else
    SV **	itervar;
#endif
    SV *	itersave;
    SV *	iterlval;
    AV *	iterary;
    IV		iterix;
    IV		itermax;
};

#ifdef USE_ITHREADS
#  define CxITERVAR(c)							\
	((c)->blk_loop.iterdata						\
	 ? (CxPADLOOP(cx) 						\
	    ? &((c)->blk_loop.oldcurpad)[(PADOFFSET)(c)->blk_loop.iterdata]	\
	    : &GvSV((GV*)(c)->blk_loop.iterdata))			\
	 : (SV**)NULL)
#  define CX_ITERDATA_SET(cx,idata)					\
	cx->blk_loop.oldcurpad = PL_curpad;				\
	if ((cx->blk_loop.iterdata = (idata)))				\
	    cx->blk_loop.itersave = SvREFCNT_inc(*CxITERVAR(cx));
#else
#  define CxITERVAR(c)		((c)->blk_loop.itervar)
#  define CX_ITERDATA_SET(cx,ivar)					\
	if ((cx->blk_loop.itervar = (SV**)(ivar)))			\
	    cx->blk_loop.itersave = SvREFCNT_inc(*CxITERVAR(cx));
#endif

#define PUSHLOOP(cx, dat, s)						\
	cx->blk_loop.label = PL_curcop->cop_label;			\
	cx->blk_loop.resetsp = s - PL_stack_base;			\
	cx->blk_loop.redo_op = cLOOP->op_redoop;			\
	cx->blk_loop.next_op = cLOOP->op_nextop;			\
	cx->blk_loop.last_op = cLOOP->op_lastop;			\
	cx->blk_loop.iterlval = Nullsv;					\
	cx->blk_loop.iterary = Nullav;					\
	cx->blk_loop.iterix = -1;					\
	CX_ITERDATA_SET(cx,dat);

#define POPLOOP(cx)							\
	SvREFCNT_dec(cx->blk_loop.iterlval);				\
	if (CxITERVAR(cx)) {						\
	    SV **s_v_p = CxITERVAR(cx);					\
	    sv_2mortal(*s_v_p);						\
	    *s_v_p = cx->blk_loop.itersave;				\
	}								\
	if (cx->blk_loop.iterary && cx->blk_loop.iterary != PL_curstack)\
	    SvREFCNT_dec(cx->blk_loop.iterary);

/* context common to subroutines, evals and loops */
struct block {
    I32		blku_oldsp;	/* stack pointer to copy stuff down to */
    COP *	blku_oldcop;	/* old curcop pointer */
    I32		blku_oldretsp;	/* return stack index */
    I32		blku_oldmarksp;	/* mark stack index */
    I32		blku_oldscopesp;	/* scope stack index */
    PMOP *	blku_oldpm;	/* values of pattern match vars */
    U8		blku_gimme;	/* is this block running in list context? */

    union {
	struct block_sub	blku_sub;
	struct block_eval	blku_eval;
	struct block_loop	blku_loop;
    } blk_u;
};
#define blk_oldsp	cx_u.cx_blk.blku_oldsp
#define blk_oldcop	cx_u.cx_blk.blku_oldcop
#define blk_oldretsp	cx_u.cx_blk.blku_oldretsp
#define blk_oldmarksp	cx_u.cx_blk.blku_oldmarksp
#define blk_oldscopesp	cx_u.cx_blk.blku_oldscopesp
#define blk_oldpm	cx_u.cx_blk.blku_oldpm
#define blk_gimme	cx_u.cx_blk.blku_gimme
#define blk_sub		cx_u.cx_blk.blk_u.blku_sub
#define blk_eval	cx_u.cx_blk.blk_u.blku_eval
#define blk_loop	cx_u.cx_blk.blk_u.blku_loop

/* Enter a block. */
#define PUSHBLOCK(cx,t,sp) CXINC, cx = &cxstack[cxstack_ix],		\
	cx->cx_type		= t,					\
	cx->blk_oldsp		= sp - PL_stack_base,			\
	cx->blk_oldcop		= PL_curcop,				\
	cx->blk_oldmarksp	= PL_markstack_ptr - PL_markstack,	\
	cx->blk_oldscopesp	= PL_scopestack_ix,			\
	cx->blk_oldretsp	= PL_retstack_ix,			\
	cx->blk_oldpm		= PL_curpm,				\
	cx->blk_gimme		= gimme;				\
	DEBUG_l( PerlIO_printf(Perl_debug_log, "Entering block %ld, type %s\n",	\
		    (long)cxstack_ix, PL_block_type[CxTYPE(cx)]); )

/* Exit a block (RETURN and LAST). */
#define POPBLOCK(cx,pm) cx = &cxstack[cxstack_ix--],			\
	newsp		 = PL_stack_base + cx->blk_oldsp,		\
	PL_curcop	 = cx->blk_oldcop,				\
	PL_markstack_ptr = PL_markstack + cx->blk_oldmarksp,		\
	PL_scopestack_ix = cx->blk_oldscopesp,				\
	PL_retstack_ix	 = cx->blk_oldretsp,				\
	pm		 = cx->blk_oldpm,				\
	gimme		 = cx->blk_gimme;				\
	DEBUG_l( PerlIO_printf(Perl_debug_log, "Leaving block %ld, type %s\n",		\
		    (long)cxstack_ix+1,PL_block_type[CxTYPE(cx)]); )

/* Continue a block elsewhere (NEXT and REDO). */
#define TOPBLOCK(cx) cx  = &cxstack[cxstack_ix],			\
	PL_stack_sp	 = PL_stack_base + cx->blk_oldsp,		\
	PL_markstack_ptr = PL_markstack + cx->blk_oldmarksp,		\
	PL_scopestack_ix = cx->blk_oldscopesp,				\
	PL_retstack_ix	 = cx->blk_oldretsp,				\
	PL_curpm         = cx->blk_oldpm

/* substitution context */
struct subst {
    I32		sbu_iters;
    I32		sbu_maxiters;
    I32		sbu_rflags;
    I32		sbu_oldsave;
    bool	sbu_once;
    bool	sbu_rxtainted;
    char *	sbu_orig;
    SV *	sbu_dstr;
    SV *	sbu_targ;
    char *	sbu_s;
    char *	sbu_m;
    char *	sbu_strend;
    void *	sbu_rxres;
    REGEXP *	sbu_rx;
};
#define sb_iters	cx_u.cx_subst.sbu_iters
#define sb_maxiters	cx_u.cx_subst.sbu_maxiters
#define sb_rflags	cx_u.cx_subst.sbu_rflags
#define sb_oldsave	cx_u.cx_subst.sbu_oldsave
#define sb_once		cx_u.cx_subst.sbu_once
#define sb_rxtainted	cx_u.cx_subst.sbu_rxtainted
#define sb_orig		cx_u.cx_subst.sbu_orig
#define sb_dstr		cx_u.cx_subst.sbu_dstr
#define sb_targ		cx_u.cx_subst.sbu_targ
#define sb_s		cx_u.cx_subst.sbu_s
#define sb_m		cx_u.cx_subst.sbu_m
#define sb_strend	cx_u.cx_subst.sbu_strend
#define sb_rxres	cx_u.cx_subst.sbu_rxres
#define sb_rx		cx_u.cx_subst.sbu_rx

#define PUSHSUBST(cx) CXINC, cx = &cxstack[cxstack_ix],			\
	cx->sb_iters		= iters,				\
	cx->sb_maxiters		= maxiters,				\
	cx->sb_rflags		= r_flags,				\
	cx->sb_oldsave		= oldsave,				\
	cx->sb_once		= once,					\
	cx->sb_rxtainted	= rxtainted,				\
	cx->sb_orig		= orig,					\
	cx->sb_dstr		= dstr,					\
	cx->sb_targ		= targ,					\
	cx->sb_s		= s,					\
	cx->sb_m		= m,					\
	cx->sb_strend		= strend,				\
	cx->sb_rxres		= Null(void*),				\
	cx->sb_rx		= rx,					\
	cx->cx_type		= CXt_SUBST;				\
	rxres_save(&cx->sb_rxres, rx)

#define POPSUBST(cx) cx = &cxstack[cxstack_ix--];			\
	rxres_free(&cx->sb_rxres)

struct context {
    U32		cx_type;	/* what kind of context this is */
    union {
	struct block	cx_blk;
	struct subst	cx_subst;
    } cx_u;
};

#define CXTYPEMASK	0xff
#define CXt_NULL	0
#define CXt_SUB		1
#define CXt_EVAL	2
#define CXt_LOOP	3
#define CXt_SUBST	4
#define CXt_BLOCK	5
#define CXt_FORMAT	6

/* private flags for CXt_EVAL */
#define CXp_REAL	0x00000100	/* truly eval'', not a lookalike */
#define CXp_TRYBLOCK	0x00000200	/* eval{}, not eval'' or similar */

#ifdef USE_ITHREADS
/* private flags for CXt_LOOP */
#  define CXp_PADVAR	0x00000100	/* itervar lives on pad, iterdata
					   has pad offset; if not set,
					   iterdata holds GV* */
#  define CxPADLOOP(c)	(((c)->cx_type & (CXt_LOOP|CXp_PADVAR))		\
			 == (CXt_LOOP|CXp_PADVAR))
#endif

#define CxTYPE(c)	((c)->cx_type & CXTYPEMASK)
#define CxREALEVAL(c)	(((c)->cx_type & (CXt_EVAL|CXp_REAL))		\
			 == (CXt_EVAL|CXp_REAL))
#define CxTRYBLOCK(c)	(((c)->cx_type & (CXt_EVAL|CXp_TRYBLOCK))	\
			 == (CXt_EVAL|CXp_TRYBLOCK))

#define CXINC (cxstack_ix < cxstack_max ? ++cxstack_ix : (cxstack_ix = cxinc()))

/* "gimme" values */

/*
=for apidoc AmU||G_SCALAR
Used to indicate scalar context.  See C<GIMME_V>, C<GIMME>, and
L<perlcall>.

=for apidoc AmU||G_ARRAY
Used to indicate list context.  See C<GIMME_V>, C<GIMME> and
L<perlcall>.

=for apidoc AmU||G_VOID
Used to indicate void context.  See C<GIMME_V> and L<perlcall>.

=for apidoc AmU||G_DISCARD
Indicates that arguments returned from a callback should be discarded.  See
L<perlcall>.

=for apidoc AmU||G_EVAL

Used to force a Perl C<eval> wrapper around a callback.  See
L<perlcall>.

=for apidoc AmU||G_NOARGS

Indicates that no arguments are being sent to a callback.  See
L<perlcall>.

=cut
*/

#define G_SCALAR	0
#define G_ARRAY		1
#define G_VOID		128	/* skip this bit when adding flags below */

/* extra flags for Perl_call_* routines */
#define G_DISCARD	2	/* Call FREETMPS. */
#define G_EVAL		4	/* Assume eval {} around subroutine call. */
#define G_NOARGS	8	/* Don't construct a @_ array. */
#define G_KEEPERR      16	/* Append errors to $@, don't overwrite it */
#define G_NODEBUG      32	/* Disable debugging at toplevel.  */
#define G_METHOD       64       /* Calling method. */

/* flag bits for PL_in_eval */
#define EVAL_NULL	0	/* not in an eval */
#define EVAL_INEVAL	1	/* some enclosing scope is an eval */
#define EVAL_WARNONLY	2	/* used by yywarn() when calling yyerror() */
#define EVAL_KEEPERR	4	/* set by Perl_call_sv if G_KEEPERR */
#define EVAL_INREQUIRE	8	/* The code is being required. */

/* Support for switching (stack and block) contexts.
 * This ensures magic doesn't invalidate local stack and cx pointers.
 */

#define PERLSI_UNKNOWN		-1
#define PERLSI_UNDEF		0
#define PERLSI_MAIN		1
#define PERLSI_MAGIC		2
#define PERLSI_SORT		3
#define PERLSI_SIGNAL		4
#define PERLSI_OVERLOAD		5
#define PERLSI_DESTROY		6
#define PERLSI_WARNHOOK		7
#define PERLSI_DIEHOOK		8
#define PERLSI_REQUIRE		9

struct stackinfo {
    AV *		si_stack;	/* stack for current runlevel */
    PERL_CONTEXT *	si_cxstack;	/* context stack for runlevel */
    I32			si_cxix;	/* current context index */
    I32			si_cxmax;	/* maximum allocated index */
    I32			si_type;	/* type of runlevel */
    struct stackinfo *	si_prev;
    struct stackinfo *	si_next;
    I32			si_markoff;	/* offset where markstack begins for us.
					 * currently used only with DEBUGGING,
					 * but not #ifdef-ed for bincompat */
};

typedef struct stackinfo PERL_SI;

#define cxstack		(PL_curstackinfo->si_cxstack)
#define cxstack_ix	(PL_curstackinfo->si_cxix)
#define cxstack_max	(PL_curstackinfo->si_cxmax)

#ifdef DEBUGGING
#  define	SET_MARK_OFFSET \
    PL_curstackinfo->si_markoff = PL_markstack_ptr - PL_markstack
#else
#  define	SET_MARK_OFFSET NOOP
#endif

#define PUSHSTACKi(type) \
    STMT_START {							\
	PERL_SI *next = PL_curstackinfo->si_next;			\
	if (!next) {							\
	    next = new_stackinfo(32, 2048/sizeof(PERL_CONTEXT) - 1);	\
	    next->si_prev = PL_curstackinfo;				\
	    PL_curstackinfo->si_next = next;				\
	}								\
	next->si_type = type;						\
	next->si_cxix = -1;						\
	AvFILLp(next->si_stack) = 0;					\
	SWITCHSTACK(PL_curstack,next->si_stack);			\
	PL_curstackinfo = next;						\
	SET_MARK_OFFSET;						\
    } STMT_END

#define PUSHSTACK PUSHSTACKi(PERLSI_UNKNOWN)

/* POPSTACK works with PL_stack_sp, so it may need to be bracketed by
 * PUTBACK/SPAGAIN to flush/refresh any local SP that may be active */
#define POPSTACK \
    STMT_START {							\
	dSP;								\
	PERL_SI *prev = PL_curstackinfo->si_prev;			\
	if (!prev) {							\
	    PerlIO_printf(Perl_error_log, "panic: POPSTACK\n");		\
	    my_exit(1);							\
	}								\
	SWITCHSTACK(PL_curstack,prev->si_stack);			\
	/* don't free prev here, free them all at the END{} */		\
	PL_curstackinfo = prev;						\
    } STMT_END

#define POPSTACK_TO(s) \
    STMT_START {							\
	while (PL_curstack != s) {					\
	    dounwind(-1);						\
	    POPSTACK;							\
	}								\
    } STMT_END
