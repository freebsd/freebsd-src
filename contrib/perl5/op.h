/*    op.h
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * The fields of BASEOP are:
 *	op_next		Pointer to next ppcode to execute after this one.
 *			(Top level pre-grafted op points to first op,
 *			but this is replaced when op is grafted in, when
 *			this op will point to the real next op, and the new
 *			parent takes over role of remembering starting op.)
 *	op_ppaddr	Pointer to current ppcode's function.
 *	op_type		The type of the operation.
 *	op_flags	Flags common to all operations.  See OPf_* below.
 *	op_private	Flags peculiar to a particular operation (BUT,
 *			by default, set to the number of children until
 *			the operation is privatized by a check routine,
 *			which may or may not check number of children).
 */

typedef U32 PADOFFSET;
#define NOT_IN_PAD ((PADOFFSET) -1)

#ifdef DEBUGGING_OPS
#define OPCODE opcode
#else
#define OPCODE U16
#endif

#ifdef BASEOP_DEFINITION
#define BASEOP BASEOP_DEFINITION
#else
#define BASEOP				\
    OP*		op_next;		\
    OP*		op_sibling;		\
    OP*		(CPERLscope(*op_ppaddr))(pTHX);		\
    PADOFFSET	op_targ;		\
    OPCODE	op_type;		\
    U16		op_seq;			\
    U8		op_flags;		\
    U8		op_private;
#endif

#define OP_GIMME(op,dfl) \
	(((op)->op_flags & OPf_WANT) == OPf_WANT_VOID   ? G_VOID   : \
	 ((op)->op_flags & OPf_WANT) == OPf_WANT_SCALAR ? G_SCALAR : \
	 ((op)->op_flags & OPf_WANT) == OPf_WANT_LIST   ? G_ARRAY   : \
	 dfl)

/*
=for apidoc Amn|U32|GIMME_V
The XSUB-writer's equivalent to Perl's C<wantarray>.  Returns C<G_VOID>,
C<G_SCALAR> or C<G_ARRAY> for void, scalar or list context,
respectively.

=for apidoc Amn|U32|GIMME
A backward-compatible version of C<GIMME_V> which can only return
C<G_SCALAR> or C<G_ARRAY>; in a void context, it returns C<G_SCALAR>.
Deprecated.  Use C<GIMME_V> instead.

=cut
*/

#define GIMME_V		OP_GIMME(PL_op, block_gimme())

/* Public flags */

#define OPf_WANT	3	/* Mask for "want" bits: */
#define  OPf_WANT_VOID	 1	/*   Want nothing */
#define  OPf_WANT_SCALAR 2	/*   Want single value */
#define  OPf_WANT_LIST	 3	/*   Want list of any length */
#define OPf_KIDS	4	/* There is a firstborn child. */
#define OPf_PARENS	8	/* This operator was parenthesized. */
				/*  (Or block needs explicit scope entry.) */
#define OPf_REF		16	/* Certified reference. */
				/*  (Return container, not containee). */
#define OPf_MOD		32	/* Will modify (lvalue). */
#define OPf_STACKED	64	/* Some arg is arriving on the stack. */
#define OPf_SPECIAL	128	/* Do something weird for this op: */
				/*  On local LVAL, don't init local value. */
				/*  On OP_SORT, subroutine is inlined. */
				/*  On OP_NOT, inversion was implicit. */
				/*  On OP_LEAVE, don't restore curpm. */
				/*  On truncate, we truncate filehandle */
				/*  On control verbs, we saw no label */
				/*  On flipflop, we saw ... instead of .. */
				/*  On UNOPs, saw bare parens, e.g. eof(). */
				/*  On OP_ENTERSUB || OP_NULL, saw a "do". */
				/*  On OP_EXISTS, treat av as av, not avhv.  */
				/*  On OP_(ENTER|LEAVE)EVAL, don't clear $@ */
				/*  On OP_ENTERITER, loop var is per-thread */
				/*  On pushre, re is /\s+/ imp. by split " " */
				/*  On regcomp, "use re 'eval'" was in scope */

/* old names; don't use in new code, but don't break them, either */
#define OPf_LIST	OPf_WANT_LIST
#define OPf_KNOW	OPf_WANT

#define GIMME \
	  (PL_op->op_flags & OPf_WANT					\
	   ? ((PL_op->op_flags & OPf_WANT) == OPf_WANT_LIST		\
	      ? G_ARRAY							\
	      : G_SCALAR)						\
	   : dowantarray())

/* NOTE: OP_NEXTSTATE, OP_DBSTATE, and OP_SETSTATE (i.e. COPs) carry lower
 * bits of PL_hints in op_private */

/* Private for lvalues */
#define OPpLVAL_INTRO	128	/* Lvalue must be localized or lvalue sub */

/* Private for OP_LEAVE, OP_LEAVESUB, OP_LEAVESUBLV and OP_LEAVEWRITE */
#define OPpREFCOUNTED		64	/* op_targ carries a refcount */

/* Private for OP_AASSIGN */
#define OPpASSIGN_COMMON	64	/* Left & right have syms in common. */
#define OPpASSIGN_HASH		32	/* Assigning to possible pseudohash. */

/* Private for OP_SASSIGN */
#define OPpASSIGN_BACKWARDS	64	/* Left & right switched. */

/* Private for OP_MATCH and OP_SUBST{,CONST} */
#define OPpRUNTIME		64	/* Pattern coming in on the stack */

/* Private for OP_TRANS */
#define OPpTRANS_FROM_UTF	1
#define OPpTRANS_TO_UTF		2
#define OPpTRANS_IDENTICAL	4	/* right side is same as left */
#define OPpTRANS_SQUASH		8
#define OPpTRANS_DELETE		16
#define OPpTRANS_COMPLEMENT	32
#define OPpTRANS_GROWS		64

/* Private for OP_REPEAT */
#define OPpREPEAT_DOLIST	64	/* List replication. */

/* Private for OP_RV2?V, OP_?ELEM */
#define OPpDEREF		(32|64)	/* Want ref to something: */
#define OPpDEREF_AV		32	/*   Want ref to AV. */
#define OPpDEREF_HV		64	/*   Want ref to HV. */
#define OPpDEREF_SV		(32|64)	/*   Want ref to SV. */
  /* OP_ENTERSUB only */
#define OPpENTERSUB_DB		16	/* Debug subroutine. */
#define OPpENTERSUB_HASTARG	32	/* Called from OP tree. */
  /* OP_RV2CV only */
#define OPpENTERSUB_AMPER	8	/* Used & form to call. */
#define OPpENTERSUB_NOPAREN	128	/* bare sub call (without parens) */
#define OPpENTERSUB_INARGS	4	/* Lval used as arg to a sub. */
  /* OP_GV only */
#define OPpEARLY_CV		32	/* foo() called before sub foo was parsed */
  /* OP_?ELEM only */
#define OPpLVAL_DEFER		16	/* Defer creation of array/hash elem */
  /* OP_RV2?V, OP_GVSV only */
#define OPpOUR_INTRO		16	/* Variable was in an our() */
  /* OP_RV2[AH]V, OP_PAD[AH]V, OP_[AH]ELEM */
#define OPpMAYBE_LVSUB		8	/* We might be an lvalue to return */
  /* for OP_RV2?V, lower bits carry hints (currently only HINT_STRICT_REFS) */

/* Private for OPs with TARGLEX */
  /* (lower bits may carry MAXARG) */
#define OPpTARGET_MY		16	/* Target is PADMY. */

/* Private for OP_CONST */
#define	OPpCONST_STRICT		8	/* bearword subject to strict 'subs' */
#define OPpCONST_ENTERED	16	/* Has been entered as symbol. */
#define OPpCONST_ARYBASE	32	/* Was a $[ translated to constant. */
#define OPpCONST_BARE		64	/* Was a bare word (filehandle?). */
#define OPpCONST_WARNING	128	/* Was a $^W translated to constant. */

/* Private for OP_FLIP/FLOP */
#define OPpFLIP_LINENUM		64	/* Range arg potentially a line num. */

/* Private for OP_LIST */
#define OPpLIST_GUESSED		64	/* Guessed that pushmark was needed. */

/* Private for OP_DELETE */
#define OPpSLICE		64	/* Operating on a list of keys */

/* Private for OP_EXISTS */
#define OPpEXISTS_SUB		64	/* Checking for &sub, not {} or [].  */

/* Private for OP_SORT, OP_PRTF, OP_SPRINTF, OP_FTTEXT, OP_FTBINARY, */
/*             string comparisons, and case changers. */
#define OPpLOCALE		64	/* Use locale */

/* Private for OP_SORT */
#define OPpSORT_NUMERIC		1	/* Optimized away { $a <=> $b } */
#define OPpSORT_INTEGER		2	/* Ditto while under "use integer" */
#define OPpSORT_REVERSE		4	/* Descending sort */
/* Private for OP_THREADSV */
#define OPpDONE_SVREF		64	/* Been through newSVREF once */

/* Private for OP_OPEN and OP_BACKTICK */
#define OPpOPEN_IN_RAW		16	/* binmode(F,":raw") on input fh */
#define OPpOPEN_IN_CRLF		32	/* binmode(F,":crlf") on input fh */
#define OPpOPEN_OUT_RAW		64	/* binmode(F,":raw") on output fh */
#define OPpOPEN_OUT_CRLF	128	/* binmode(F,":crlf") on output fh */

/* Private for OP_EXIT */
#define OPpEXIT_VMSISH		128	/* exit(0) vs. exit(1) vmsish mode*/

struct op {
    BASEOP
};

struct unop {
    BASEOP
    OP *	op_first;
};

struct binop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
};

struct logop {
    BASEOP
    OP *	op_first;
    OP *	op_other;
};

struct listop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
};

struct pmop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    OP *	op_pmreplroot;
    OP *	op_pmreplstart;
    PMOP *	op_pmnext;		/* list of all scanpats */
    REGEXP *	op_pmregexp;		/* compiled expression */
    U16		op_pmflags;
    U16		op_pmpermflags;
    U8		op_pmdynflags;
};

#define PMdf_USED	0x01		/* pm has been used once already */
#define PMdf_TAINTED	0x02		/* pm compiled from tainted pattern */
#define PMdf_UTF8	0x04		/* pm compiled from utf8 data */

#define PMf_RETAINT	0x0001		/* taint $1 etc. if target tainted */
#define PMf_ONCE	0x0002		/* use pattern only once per reset */
#define PMf_REVERSED	0x0004		/* Should be matched right->left */
#define PMf_MAYBE_CONST	0x0008		/* replacement contains variables */
#define PMf_SKIPWHITE	0x0010		/* skip leading whitespace for split */
#define PMf_WHITE	0x0020		/* pattern is \s+ */
#define PMf_CONST	0x0040		/* subst replacement is constant */
#define PMf_KEEP	0x0080		/* keep 1st runtime pattern forever */
#define PMf_GLOBAL	0x0100		/* pattern had a g modifier */
#define PMf_CONTINUE	0x0200		/* don't reset pos() if //g fails */
#define PMf_EVAL	0x0400		/* evaluating replacement as expr */
#define PMf_LOCALE	0x0800		/* use locale for character types */
#define PMf_MULTILINE	0x1000		/* assume multiple lines */
#define PMf_SINGLELINE	0x2000		/* assume single line */
#define PMf_FOLD	0x4000		/* case insensitivity */
#define PMf_EXTENDED	0x8000		/* chuck embedded whitespace */

/* mask of bits stored in regexp->reganch */
#define PMf_COMPILETIME	(PMf_MULTILINE|PMf_SINGLELINE|PMf_LOCALE|PMf_FOLD|PMf_EXTENDED)

struct svop {
    BASEOP
    SV *	op_sv;
};

struct padop {
    BASEOP
    PADOFFSET	op_padix;
};

struct pvop {
    BASEOP
    char *	op_pv;
};

struct loop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    OP *	op_redoop;
    OP *	op_nextop;
    OP *	op_lastop;
};

#define cUNOPx(o)	((UNOP*)o)
#define cBINOPx(o)	((BINOP*)o)
#define cLISTOPx(o)	((LISTOP*)o)
#define cLOGOPx(o)	((LOGOP*)o)
#define cPMOPx(o)	((PMOP*)o)
#define cSVOPx(o)	((SVOP*)o)
#define cPADOPx(o)	((PADOP*)o)
#define cPVOPx(o)	((PVOP*)o)
#define cCOPx(o)	((COP*)o)
#define cLOOPx(o)	((LOOP*)o)

#define cUNOP		cUNOPx(PL_op)
#define cBINOP		cBINOPx(PL_op)
#define cLISTOP		cLISTOPx(PL_op)
#define cLOGOP		cLOGOPx(PL_op)
#define cPMOP		cPMOPx(PL_op)
#define cSVOP		cSVOPx(PL_op)
#define cPADOP		cPADOPx(PL_op)
#define cPVOP		cPVOPx(PL_op)
#define cCOP		cCOPx(PL_op)
#define cLOOP		cLOOPx(PL_op)

#define cUNOPo		cUNOPx(o)
#define cBINOPo		cBINOPx(o)
#define cLISTOPo	cLISTOPx(o)
#define cLOGOPo		cLOGOPx(o)
#define cPMOPo		cPMOPx(o)
#define cSVOPo		cSVOPx(o)
#define cPADOPo		cPADOPx(o)
#define cPVOPo		cPVOPx(o)
#define cCOPo		cCOPx(o)
#define cLOOPo		cLOOPx(o)

#define kUNOP		cUNOPx(kid)
#define kBINOP		cBINOPx(kid)
#define kLISTOP		cLISTOPx(kid)
#define kLOGOP		cLOGOPx(kid)
#define kPMOP		cPMOPx(kid)
#define kSVOP		cSVOPx(kid)
#define kPADOP		cPADOPx(kid)
#define kPVOP		cPVOPx(kid)
#define kCOP		cCOPx(kid)
#define kLOOP		cLOOPx(kid)


#ifdef USE_ITHREADS
#  define	cGVOPx_gv(o)	((GV*)PL_curpad[cPADOPx(o)->op_padix])
#  define	IS_PADGV(v)	(v && SvTYPE(v) == SVt_PVGV && GvIN_PAD(v))
#  define	IS_PADCONST(v)	(v && SvREADONLY(v))
#  define	cSVOPx_sv(v)	(cSVOPx(v)->op_sv \
				 ? cSVOPx(v)->op_sv : PL_curpad[(v)->op_targ])
#  define	cSVOPx_svp(v)	(cSVOPx(v)->op_sv \
				 ? &cSVOPx(v)->op_sv : &PL_curpad[(v)->op_targ])
#else
#  define	cGVOPx_gv(o)	((GV*)cSVOPx(o)->op_sv)
#  define	IS_PADGV(v)	FALSE
#  define	IS_PADCONST(v)	FALSE
#  define	cSVOPx_sv(v)	(cSVOPx(v)->op_sv)
#  define	cSVOPx_svp(v)	(&cSVOPx(v)->op_sv)
#endif

#define	cGVOP_gv		cGVOPx_gv(PL_op)
#define	cGVOPo_gv		cGVOPx_gv(o)
#define	kGVOP_gv		cGVOPx_gv(kid)
#define cSVOP_sv		cSVOPx_sv(PL_op)
#define cSVOPo_sv		cSVOPx_sv(o)
#define kSVOP_sv		cSVOPx_sv(kid)

#define Nullop Null(OP*)

/* Lowest byte of PL_opargs */
#define OA_MARK 1
#define OA_FOLDCONST 2
#define OA_RETSCALAR 4
#define OA_TARGET 8
#define OA_RETINTEGER 16
#define OA_OTHERINT 32
#define OA_DANGEROUS 64
#define OA_DEFGV 128
#define OA_TARGLEX 256

/* The next 4 bits encode op class information */
#define OCSHIFT 9

#define OA_CLASS_MASK (15 << OCSHIFT)

#define OA_BASEOP (0 << OCSHIFT)
#define OA_UNOP (1 << OCSHIFT)
#define OA_BINOP (2 << OCSHIFT)
#define OA_LOGOP (3 << OCSHIFT)
#define OA_LISTOP (4 << OCSHIFT)
#define OA_PMOP (5 << OCSHIFT)
#define OA_SVOP (6 << OCSHIFT)
#define OA_PADOP (7 << OCSHIFT)
#define OA_PVOP_OR_SVOP (8 << OCSHIFT)
#define OA_LOOP (9 << OCSHIFT)
#define OA_COP (10 << OCSHIFT)
#define OA_BASEOP_OR_UNOP (11 << OCSHIFT)
#define OA_FILESTATOP (12 << OCSHIFT)
#define OA_LOOPEXOP (13 << OCSHIFT)

#define OASHIFT 13

/* Remaining nybbles of PL_opargs */
#define OA_SCALAR 1
#define OA_LIST 2
#define OA_AVREF 3
#define OA_HVREF 4
#define OA_CVREF 5
#define OA_FILEREF 6
#define OA_SCALARREF 7
#define OA_OPTIONAL 8

#ifdef USE_ITHREADS
#  define OP_REFCNT_INIT		MUTEX_INIT(&PL_op_mutex)
#  define OP_REFCNT_LOCK		MUTEX_LOCK(&PL_op_mutex)
#  define OP_REFCNT_UNLOCK		MUTEX_UNLOCK(&PL_op_mutex)
#  define OP_REFCNT_TERM		MUTEX_DESTROY(&PL_op_mutex)
#else
#  define OP_REFCNT_INIT		NOOP
#  define OP_REFCNT_LOCK		NOOP
#  define OP_REFCNT_UNLOCK		NOOP
#  define OP_REFCNT_TERM		NOOP
#endif

#define OpREFCNT_set(o,n)		((o)->op_targ = (n))
#define OpREFCNT_inc(o)			((o) ? (++(o)->op_targ, (o)) : Nullop)
#define OpREFCNT_dec(o)			(--(o)->op_targ)

/* flags used by Perl_load_module() */
#define PERL_LOADMOD_DENY		0x1
#define PERL_LOADMOD_NOIMPORT		0x2
#define PERL_LOADMOD_IMPORT_OPS		0x4
