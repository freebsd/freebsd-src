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
    U8	str_len;
    U8  type;
    U16 next_off;
    char string[1];
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

#define ANYOF_BITMAP_SIZE	32	/* 256 b/(8 b/B) */
#define ANYOF_CLASSBITMAP_SIZE	 4

struct regnode_charclass {
    U8	flags;
    U8  type;
    U16 next_off;
    char bitmap[ANYOF_BITMAP_SIZE];
};

struct regnode_charclass_class {
    U8	flags;
    U8  type;
    U16 next_off;
    char bitmap[ANYOF_BITMAP_SIZE];
    char classflags[ANYOF_CLASSBITMAP_SIZE];
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
#define MASK(p)		((char*)OPERAND(p))
#define	STR_LEN(p)	(((struct regnode_string *)p)->str_len)
#define	STRING(p)	(((struct regnode_string *)p)->string)
#define STR_SZ(l)	((l + sizeof(regnode) - 1) / sizeof(regnode))
#define NODE_SZ_STR(p)	(STR_SZ(STR_LEN(p))+1)

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

#define REG_MAGIC 0234

#define SIZE_ONLY (PL_regcode == &PL_regdummy)

/* Flags for node->flags of ANYOF */

#define ANYOF_CLASS	0x08
#define ANYOF_INVERT	0x04
#define ANYOF_FOLD	0x02
#define ANYOF_LOCALE	0x01

/* Used for regstclass only */
#define ANYOF_EOS	0x10		/* Can match an empty string too */

/* Character classes for node->classflags of ANYOF */
/* Should be synchronized with a table in regprop() */
/* 2n should pair with 2n+1 */

#define ANYOF_ALNUM	 0	/* \w, PL_utf8_alnum, utf8::IsWord, ALNUM */
#define ANYOF_NALNUM	 1
#define ANYOF_SPACE	 2	/* \s */
#define ANYOF_NSPACE	 3
#define ANYOF_DIGIT	 4
#define ANYOF_NDIGIT	 5
#define ANYOF_ALNUMC	 6	/* isalnum(3), utf8::IsAlnum, ALNUMC */
#define ANYOF_NALNUMC	 7
#define ANYOF_ALPHA	 8
#define ANYOF_NALPHA	 9
#define ANYOF_ASCII	10
#define ANYOF_NASCII	11
#define ANYOF_CNTRL	12
#define ANYOF_NCNTRL	13
#define ANYOF_GRAPH	14
#define ANYOF_NGRAPH	15
#define ANYOF_LOWER	16
#define ANYOF_NLOWER	17
#define ANYOF_PRINT	18
#define ANYOF_NPRINT	19
#define ANYOF_PUNCT	20
#define ANYOF_NPUNCT	21
#define ANYOF_UPPER	22
#define ANYOF_NUPPER	23
#define ANYOF_XDIGIT	24
#define ANYOF_NXDIGIT	25
#define ANYOF_PSXSPC	26	/* POSIX space: \s plus the vertical tab */
#define ANYOF_NPSXSPC	27
#define ANYOF_BLANK	28	/* GNU extension: space and tab */
#define ANYOF_NBLANK	29

#define ANYOF_MAX	32

/* Backward source code compatibility. */

#define ANYOF_ALNUML	 ANYOF_ALNUM
#define ANYOF_NALNUML	 ANYOF_NALNUM
#define ANYOF_SPACEL	 ANYOF_SPACE
#define ANYOF_NSPACEL	 ANYOF_NSPACE

/* Utility macros for the bitmap and classes of ANYOF */

#define ANYOF_SIZE		(sizeof(struct regnode_charclass))
#define ANYOF_CLASS_SIZE	(sizeof(struct regnode_charclass_class))

#define ANYOF_FLAGS(p)		((p)->flags)
#define ANYOF_FLAGS_ALL		0xff

#define ANYOF_BIT(c)		(1 << ((c) & 7))

#define ANYOF_CLASS_BYTE(p, c)	(((struct regnode_charclass_class*)(p))->classflags[((c) >> 3) & 3])
#define ANYOF_CLASS_SET(p, c)	(ANYOF_CLASS_BYTE(p, c) |=  ANYOF_BIT(c))
#define ANYOF_CLASS_CLEAR(p, c)	(ANYOF_CLASS_BYTE(p, c) &= ~ANYOF_BIT(c))
#define ANYOF_CLASS_TEST(p, c)	(ANYOF_CLASS_BYTE(p, c) &   ANYOF_BIT(c))

#define ANYOF_CLASS_ZERO(ret)	Zero(((struct regnode_charclass_class*)(ret))->classflags, ANYOF_CLASSBITMAP_SIZE, char)
#define ANYOF_BITMAP_ZERO(ret)	Zero(((struct regnode_charclass*)(ret))->bitmap, ANYOF_BITMAP_SIZE, char)

#define ANYOF_BITMAP(p)		(((struct regnode_charclass*)(p))->bitmap)
#define ANYOF_BITMAP_BYTE(p, c)	(ANYOF_BITMAP(p)[((c) >> 3) & 31])
#define ANYOF_BITMAP_SET(p, c)	(ANYOF_BITMAP_BYTE(p, c) |=  ANYOF_BIT(c))
#define ANYOF_BITMAP_CLEAR(p,c)	(ANYOF_BITMAP_BYTE(p, c) &= ~ANYOF_BIT(c))
#define ANYOF_BITMAP_TEST(p, c)	(ANYOF_BITMAP_BYTE(p, c) &   ANYOF_BIT(c))

#define ANYOF_SKIP		((ANYOF_SIZE - 1)/sizeof(regnode))
#define ANYOF_CLASS_SKIP	((ANYOF_CLASS_SIZE - 1)/sizeof(regnode))
#define ANYOF_CLASS_ADD_SKIP	(ANYOF_CLASS_SKIP - ANYOF_SKIP)

/*
 * Utility definitions.
 */
#ifndef lint
#ifndef CHARMASK
#define	UCHARAT(p)	((int)*(U8*)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARMASK)
#endif
#else /* lint */
#define UCHARAT(p)	PL_regdummy
#endif /* lint */

#define EXTRA_SIZE(guy) ((sizeof(guy)-1)/sizeof(struct regnode))

#define REG_SEEN_ZERO_LEN	1
#define REG_SEEN_LOOKBEHIND	2
#define REG_SEEN_GPOS		4
#define REG_SEEN_EVAL		8

START_EXTERN_C

#include "regnodes.h"

/* The following have no fixed length. U8 so we can do strchr() on it. */
#ifndef DOINIT
EXTCONST U8 PL_varies[];
#else
EXTCONST U8 PL_varies[] = {
    BRANCH, BACK, STAR, PLUS, CURLY, CURLYX, REF, REFF, REFFL, 
    WHILEM, CURLYM, CURLYN, BRANCHJ, IFTHEN, SUSPEND, CLUMP, 0
};
#endif

/* The following always have a length of 1. U8 we can do strchr() on it. */
/* (Note that length 1 means "one character" under UTF8, not "one octet".) */
#ifndef DOINIT
EXTCONST U8 PL_simple[];
#else
EXTCONST U8 PL_simple[] = {
    REG_ANY, ANYUTF8, SANY, SANYUTF8, ANYOF, ANYOFUTF8,
    ALNUM, ALNUMUTF8, ALNUML, ALNUMLUTF8,
    NALNUM, NALNUMUTF8, NALNUML, NALNUMLUTF8,
    SPACE, SPACEUTF8, SPACEL, SPACELUTF8,
    NSPACE, NSPACEUTF8, NSPACEL, NSPACELUTF8,
    DIGIT, DIGITUTF8, NDIGIT, NDIGITUTF8, 0
};
#endif

END_EXTERN_C

typedef struct re_scream_pos_data_s
{
    char **scream_olds;		/* match pos */
    I32 *scream_pos;		/* Internal iterator of scream. */
} re_scream_pos_data;

struct reg_data {
    U32 count;
    U8 *what;
    void* data[1];
};

struct reg_substr_datum {
    I32 min_offset;
    I32 max_offset;
    SV *substr;
};

struct reg_substr_data {
    struct reg_substr_datum data[3];	/* Actual array */
};

#define anchored_substr substrs->data[0].substr
#define anchored_offset substrs->data[0].min_offset
#define float_substr substrs->data[1].substr
#define float_min_offset substrs->data[1].min_offset
#define float_max_offset substrs->data[1].max_offset
#define check_substr substrs->data[2].substr
#define check_offset_min substrs->data[2].min_offset
#define check_offset_max substrs->data[2].max_offset
