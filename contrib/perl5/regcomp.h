/*    regcomp.h
 */

typedef OP OP_4tree;			/* Will be redefined later. */

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	sv that must begin a match; Nullch if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 *  [regmust changed to SV* for bminstr()--law]
 * regmlen	length of regmust string
 *  [regmlen not used currently]
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that pregcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in pregexec() needs it and pregcomp() is computing
 * it anyway.
 * [regmust is now supplied always.  The tests that use regmust have a
 * heuristic that disables the test if it usually matches.]
 *
 * [In fact, we now use regmust in many cases to locate where the search
 * starts in the string, so if regback is >= 0, the regmust search is never
 * wasted effort.  The regback variable says how many characters back from
 * where regmust matched is the earliest possible start of the match.
 * For instance, /[a-z].foo/ has a regmust of 'foo' and a regback of 2.]
 */

/*
 * Structure for regexp "program".  This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.  (Here we
 * have one of the subtle syntax dependencies:  an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:  the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 *
 * [The "next" pointer is always aligned on an even
 * boundary, and reads the offset directly as a short.  Also, there is no
 * special test to reverse the sign of BACK pointers since the offset is
 * stored negative.]
 */

struct regnode_string {
    U8	flags;
    U8  type;
    U16 next_off;
    U8 string[1];
};

struct regnode_1 {
    U8	flags;
    U8  type;
    U16 next_off;
    U32 arg1;
};

struct regnode_2 {
    U8	flags;
    U8  type;
    U16 next_off;
    U16 arg1;
    U16 arg2;
};

/* XXX fix this description.
   Impose a limit of REG_INFTY on various pattern matching operations
   to limit stack growth and to avoid "infinite" recursions.
*/
/* The default size for REG_INFTY is I16_MAX, which is the same as
   SHORT_MAX (see perl.h).  Unfortunately I16 isn't necessarily 16 bits
   (see handy.h).  On the Cray C90, sizeof(short)==4 and hence I16_MAX is
   ((1<<31)-1), while on the Cray T90, sizeof(short)==8 and I16_MAX is
   ((1<<63)-1).  To limit stack growth to reasonable sizes, supply a
   smaller default.
	--Andy Dougherty  11 June 1998
*/
#if SHORTSIZE > 2
#  ifndef REG_INFTY
#    define REG_INFTY ((1<<15)-1)
#  endif
#endif

#ifndef REG_INFTY
#  define REG_INFTY I16_MAX
#endif

#define ARG_VALUE(arg) (arg)
#define ARG__SET(arg,val) ((arg) = (val))

#define ARG(p) ARG_VALUE(ARG_LOC(p))
#define ARG1(p) ARG_VALUE(ARG1_LOC(p))
#define ARG2(p) ARG_VALUE(ARG2_LOC(p))
#define ARG_SET(p, val) ARG__SET(ARG_LOC(p), (val))
#define ARG1_SET(p, val) ARG__SET(ARG1_LOC(p), (val))
#define ARG2_SET(p, val) ARG__SET(ARG2_LOC(p), (val))

#ifndef lint
#  define NEXT_OFF(p) ((p)->next_off)
#  define NODE_ALIGN(node)
#  define NODE_ALIGN_FILL(node) ((node)->flags = 0xde) /* deadbeef */
#else /* lint */
#  define NEXT_OFF(p) 0
#  define NODE_ALIGN(node)
#  define NODE_ALIGN_FILL(node)
#endif /* lint */

#define SIZE_ALIGN NODE_ALIGN

#define	OP(p)		((p)->type)
#define	OPERAND(p)	(((struct regnode_string *)p)->string)
#define	NODE_ALIGN(node)
#define	ARG_LOC(p)	(((struct regnode_1 *)p)->arg1)
#define	ARG1_LOC(p)	(((struct regnode_2 *)p)->arg1)
#define	ARG2_LOC(p)	(((struct regnode_2 *)p)->arg2)
#define NODE_STEP_REGNODE	1	/* sizeof(regnode)/sizeof(regnode) */
#define EXTRA_STEP_2ARGS	EXTRA_SIZE(struct regnode_2)

#define NODE_STEP_B	4

#define	NEXTOPER(p)	((p) + NODE_STEP_REGNODE)
#define	PREVOPER(p)	((p) - NODE_STEP_REGNODE)

#define FILL_ADVANCE_NODE(ptr, op) STMT_START { \
    (ptr)->type = op;    (ptr)->next_off = 0;   (ptr)++; } STMT_END
#define FILL_ADVANCE_NODE_ARG(ptr, op, arg) STMT_START { \
    ARG_SET(ptr, arg);  FILL_ADVANCE_NODE(ptr, op); (ptr) += 1; } STMT_END

#define MAGIC 0234

#define SIZE_ONLY (PL_regcode == &PL_regdummy)

/* Flags for first parameter byte of ANYOF */
#define ANYOF_INVERT	0x40
#define ANYOF_FOLD	0x20
#define ANYOF_LOCALE	0x10
#define ANYOF_ISA	0x0F
#define ANYOF_ALNUML	 0x08
#define ANYOF_NALNUML	 0x04
#define ANYOF_SPACEL	 0x02
#define ANYOF_NSPACEL	 0x01

/* Utility macros for bitmap of ANYOF */
#define ANYOF_BYTE(p,c)     (p)[1 + (((c) >> 3) & 31)]
#define ANYOF_BIT(c)        (1 << ((c) & 7))
#define ANYOF_SET(p,c)      (ANYOF_BYTE(p,c) |=  ANYOF_BIT(c))
#define ANYOF_CLEAR(p,c)    (ANYOF_BYTE(p,c) &= ~ANYOF_BIT(c))
#define ANYOF_TEST(p,c)     (ANYOF_BYTE(p,c) &   ANYOF_BIT(c))

#define ANY_SKIP ((33 - 1)/sizeof(regnode) + 1)

/*
 * Utility definitions.
 */
#ifndef lint
#ifndef CHARMASK
#define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARMASK)
#endif
#else /* lint */
#define UCHARAT(p)	PL_regdummy
#endif /* lint */

#define	FAIL(m)		croak    ("/%.127s/: %s",  PL_regprecomp,m)
#define	FAIL2(pat,m)	re_croak2("/%.127s/: ",pat,PL_regprecomp,m)

#define EXTRA_SIZE(guy) ((sizeof(guy)-1)/sizeof(struct regnode))

#define REG_SEEN_ZERO_LEN	1
#define REG_SEEN_LOOKBEHIND	2
#define REG_SEEN_GPOS		4
#define REG_SEEN_EVAL		8

#include "regnodes.h"

/* The following have no fixed length. char* since we do strchr on it. */
#ifndef DOINIT
EXTCONST char varies[];
#else
EXTCONST char varies[] = {
    BRANCH, BACK, STAR, PLUS, CURLY, CURLYX, REF, REFF, REFFL, 
    WHILEM, CURLYM, CURLYN, BRANCHJ, IFTHEN, SUSPEND, 0
};
#endif

/* The following always have a length of 1. char* since we do strchr on it. */
#ifndef DOINIT
EXTCONST char simple[];
#else
EXTCONST char simple[] = {
    ANY, SANY, ANYOF,
    ALNUM, ALNUML, NALNUM, NALNUML,
    SPACE, SPACEL, NSPACE, NSPACEL,
    DIGIT, NDIGIT, 0
};
#endif

