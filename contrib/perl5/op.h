/*    op.h
 *
 *    Copyright (c) 1991-1999, Larry Wall
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
    OP*		(CPERLscope(*op_ppaddr))_((ARGSproto));		\
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
				/*  On OP_(ENTER|LEAVE)EVAL, don't clear $@ */
				/*  On OP_ENTERITER, loop var is per-thread */

/* old names; don't use in new code, but don't break them, either */
#define OPf_LIST	OPf_WANT_LIST
#define OPf_KNOW	OPf_WANT
#define GIMME \
	  (PL_op->op_flags & OPf_WANT					\
	   ? ((PL_op->op_flags & OPf_WANT) == OPf_WANT_LIST		\
	      ? G_ARRAY							\
	      : G_SCALAR)						\
	   : dowantarray())

/* Private for lvalues */
#define OPpLVAL_INTRO	128	/* Lvalue must be localized */

/* Private for OP_AASSIGN */
#define OPpASSIGN_COMMON	64	/* Left & right have syms in common. */

/* Private for OP_SASSIGN */
#define OPpASSIGN_BACKWARDS	64	/* Left & right switched. */

/* Private for OP_MATCH and OP_SUBST{,CONST} */
#define OPpRUNTIME		64	/* Pattern coming in on the stack */

/* Private for OP_TRANS */
#define OPpTRANS_COUNTONLY	8
#define OPpTRANS_SQUASH		16
#define OPpTRANS_DELETE		32
#define OPpTRANS_COMPLEMENT	64

/* Private for OP_REPEAT */
#define OPpREPEAT_DOLIST	64	/* List replication. */

/* Private for OP_ENTERSUB, OP_RV2?V, OP_?ELEM */
#define OPpDEREF		(32|64)	/* Want ref to something: */
#define OPpDEREF_AV		32	/*   Want ref to AV. */
#define OPpDEREF_HV		64	/*   Want ref to HV. */
#define OPpDEREF_SV		(32|64)	/*   Want ref to SV. */
  /* OP_ENTERSUB only */
#define OPpENTERSUB_DB		16	/* Debug subroutine. */
#define OPpENTERSUB_AMPER	8	/* Used & form to call. */
  /* OP_?ELEM only */
#define OPpLVAL_DEFER		16	/* Defer creation of array/hash elem */
  /* for OP_RV2?V, lower bits carry hints */

/* Private for OP_CONST */
#define OPpCONST_ENTERED	16	/* Has been entered as symbol. */
#define OPpCONST_ARYBASE	32	/* Was a $[ translated to constant. */
#define OPpCONST_BARE		64	/* Was a bare word (filehandle?). */

/* Private for OP_FLIP/FLOP */
#define OPpFLIP_LINENUM		64	/* Range arg potentially a line num. */

/* Private for OP_LIST */
#define OPpLIST_GUESSED		64	/* Guessed that pushmark was needed. */

/* Private for OP_DELETE */
#define OPpSLICE		64	/* Operating on a list of keys */

/* Private for OP_SORT, OP_PRTF, OP_SPRINTF, string cmp'n, and case changers */
#define OPpLOCALE		64	/* Use locale */

/* Private for OP_THREADSV */
#define OPpDONE_SVREF		64	/* Been through newSVREF once */

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

struct condop {
    BASEOP
    OP *	op_first;
    OP *	op_true;
    OP *	op_false;
};

struct listop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    U32		op_children;
};

struct pmop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    U32		op_children;
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

struct gvop {
    BASEOP
    GV *	op_gv;
};

struct pvop {
    BASEOP
    char *	op_pv;
};

struct loop {
    BASEOP
    OP *	op_first;
    OP *	op_last;
    U32		op_children;
    OP *	op_redoop;
    OP *	op_nextop;
    OP *	op_lastop;
};

#define cUNOP ((UNOP*)PL_op)
#define cBINOP ((BINOP*)PL_op)
#define cLISTOP ((LISTOP*)PL_op)
#define cLOGOP ((LOGOP*)PL_op)
#define cCONDOP ((CONDOP*)PL_op)
#define cPMOP ((PMOP*)PL_op)
#define cSVOP ((SVOP*)PL_op)
#define cGVOP ((GVOP*)PL_op)
#define cPVOP ((PVOP*)PL_op)
#define cCOP ((COP*)PL_op)
#define cLOOP ((LOOP*)PL_op)

#define cUNOPo ((UNOP*)o)
#define cBINOPo ((BINOP*)o)
#define cLISTOPo ((LISTOP*)o)
#define cLOGOPo ((LOGOP*)o)
#define cCONDOPo ((CONDOP*)o)
#define cPMOPo ((PMOP*)o)
#define cSVOPo ((SVOP*)o)
#define cGVOPo ((GVOP*)o)
#define cPVOPo ((PVOP*)o)
#define cCVOPo ((CVOP*)o)
#define cCOPo ((COP*)o)
#define cLOOPo ((LOOP*)o)

#define kUNOP ((UNOP*)kid)
#define kBINOP ((BINOP*)kid)
#define kLISTOP ((LISTOP*)kid)
#define kLOGOP ((LOGOP*)kid)
#define kCONDOP ((CONDOP*)kid)
#define kPMOP ((PMOP*)kid)
#define kSVOP ((SVOP*)kid)
#define kGVOP ((GVOP*)kid)
#define kPVOP ((PVOP*)kid)
#define kCOP ((COP*)kid)
#define kLOOP ((LOOP*)kid)

#define Nullop Null(OP*)

/* Lowest byte of opargs */
#define OA_MARK 1
#define OA_FOLDCONST 2
#define OA_RETSCALAR 4
#define OA_TARGET 8
#define OA_RETINTEGER 16
#define OA_OTHERINT 32
#define OA_DANGEROUS 64
#define OA_DEFGV 128

/* The next 4 bits encode op class information */
#define OA_CLASS_MASK (15 << 8)

#define OA_BASEOP (0 << 8)
#define OA_UNOP (1 << 8)
#define OA_BINOP (2 << 8)
#define OA_LOGOP (3 << 8)
#define OA_CONDOP (4 << 8)
#define OA_LISTOP (5 << 8)
#define OA_PMOP (6 << 8)
#define OA_SVOP (7 << 8)
#define OA_GVOP (8 << 8)
#define OA_PVOP (9 << 8)
#define OA_LOOP (10 << 8)
#define OA_COP (11 << 8)
#define OA_BASEOP_OR_UNOP (12 << 8)
#define OA_FILESTATOP (13 << 8)
#define OA_LOOPEXOP (14 << 8)

#define OASHIFT 12

/* Remaining nybbles of opargs */
#define OA_SCALAR 1
#define OA_LIST 2
#define OA_AVREF 3
#define OA_HVREF 4
#define OA_CVREF 5
#define OA_FILEREF 6
#define OA_SCALARREF 7
#define OA_OPTIONAL 8

