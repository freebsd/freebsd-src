/* $RCSfile: regcomp.h,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:35 $
 *
 * $Log: regcomp.h,v $
 * Revision 1.1.1.1  1994/09/10  06:27:35  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:39  nate
 * PERL!
 *
 * Revision 4.0.1.1  91/06/07  11:49:40  lwall
 * patch4: no change
 *
 * Revision 4.0  91/03/20  01:39:09  lwall
 * 4.0 baseline.
 *
 */

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	str that must begin a match; Nullch if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 *  [regmust changed to STR* for bminstr()--law]
 * regmlen	length of regmust string
 *  [regmlen not used currently]
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
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

/* definition	number	opnd?	meaning */
#define	END	0	/* no	End of program. */
#define	BOL	1	/* no	Match "" at beginning of line. */
#define	EOL	2	/* no	Match "" at end of line. */
#define	ANY	3	/* no	Match any one character. */
#define	ANYOF	4	/* str	Match character in (or not in) this class. */
#define	CURLY	5	/* str	Match this simple thing {n,m} times. */
#define	BRANCH	6	/* node	Match this alternative, or the next... */
#define	BACK	7	/* no	Match "", "next" ptr points backward. */
#define	EXACTLY	8	/* str	Match this string (preceded by length). */
#define	NOTHING	9	/* no	Match empty string. */
#define	STAR	10	/* node	Match this (simple) thing 0 or more times. */
#define	PLUS	11	/* node	Match this (simple) thing 1 or more times. */
#define ALNUM	12	/* no	Match any alphanumeric character */
#define NALNUM	13	/* no	Match any non-alphanumeric character */
#define BOUND	14	/* no	Match "" at any word boundary */
#define NBOUND	15	/* no	Match "" at any word non-boundary */
#define SPACE	16	/* no	Match any whitespace character */
#define NSPACE	17	/* no	Match any non-whitespace character */
#define DIGIT	18	/* no	Match any numeric character */
#define NDIGIT	19	/* no	Match any non-numeric character */
#define REF	20	/* num	Match some already matched string */
#define	OPEN	21	/* num	Mark this point in input as start of #n. */
#define	CLOSE	22	/* num	Analogous to OPEN. */

/*
 * Opcode notes:
 *
 * BRANCH	The set of branches constituting a single choice are hooked
 *		together with their "next" pointers, since precedence prevents
 *		anything being concatenated to any individual branch.  The
 *		"next" pointer of the last BRANCH in a choice points to the
 *		thing following the whole choice.  This is also where the
 *		final "next" pointer of each individual branch points; each
 *		branch starts with the operand node of a BRANCH node.
 *
 * BACK		Normal "next" pointers all implicitly point forward; BACK
 *		exists to make loop structures possible.
 *
 * STAR,PLUS	'?', and complex '*' and '+', are implemented as circular
 *		BRANCH structures using BACK.  Simple cases (one character
 *		per match) are implemented with STAR and PLUS for speed
 *		and to minimize recursive plunges.
 *
 * OPEN,CLOSE	...are numbered at compile time.
 */

#ifndef DOINIT
extern char regarglen[];
#else
char regarglen[] = {0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2};
#endif

/* The following have no fixed length. */
#ifndef DOINIT
extern char varies[];
#else
char varies[] = {BRANCH,BACK,STAR,PLUS,CURLY,REF,0};
#endif

/* The following always have a length of 1. */
#ifndef DOINIT
extern char simple[];
#else
char simple[] = {ANY,ANYOF,ALNUM,NALNUM,SPACE,NSPACE,DIGIT,NDIGIT,0};
#endif

EXT char regdummy;

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
 * [If REGALIGN is defined, the "next" pointer is always aligned on an even
 * boundary, and reads the offset directly as a short.  Also, there is no
 * special test to reverse the sign of BACK pointers since the offset is
 * stored negative.]
 */

#ifndef gould
#ifndef cray
#ifndef eta10
#define REGALIGN
#endif
#endif
#endif

#define	OP(p)	(*(p))

#ifndef lint
#ifdef REGALIGN
#define NEXT(p) (*(short*)(p+1))
#define ARG1(p) (*(unsigned short*)(p+3))
#define ARG2(p) (*(unsigned short*)(p+5))
#else
#define	NEXT(p)	(((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define	ARG1(p)	(((*((p)+3)&0377)<<8) + (*((p)+4)&0377))
#define	ARG2(p)	(((*((p)+5)&0377)<<8) + (*((p)+6)&0377))
#endif
#else /* lint */
#define NEXT(p) 0
#endif /* lint */

#define	OPERAND(p)	((p) + 3)

#ifdef REGALIGN
#define	NEXTOPER(p)	((p) + 4)
#else
#define	NEXTOPER(p)	((p) + 3)
#endif

#define MAGIC 0234

/*
 * Utility definitions.
 */
#ifndef lint
#ifndef CHARBITS
#define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARBITS)
#endif
#else /* lint */
#define UCHARAT(p)	regdummy
#endif /* lint */

#define	FAIL(m)	fatal("/%s/: %s",regprecomp,m)

char *regnext();
#ifdef DEBUGGING
void regdump();
char *regprop();
#endif

