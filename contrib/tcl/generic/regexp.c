/*
 * TclRegComp and TclRegExec -- TclRegSub is elsewhere
 *
 *	Copyright (c) 1986 by University of Toronto.
 *	Written by Henry Spencer.  Not derived from licensed software.
 *
 *	Permission is granted to anyone to use this software for any
 *	purpose on any computer system, and to redistribute it freely,
 *	subject to the following restrictions:
 *
 *	1. The author is not responsible for the consequences of use of
 *		this software, no matter how awful, even if they arise
 *		from defects in it.
 *
 *	2. The origin of this software must not be misrepresented, either
 *		by explicit claim or by omission.
 *
 *	3. Altered versions must be plainly marked as such, and must not
 *		be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 *
 * *** NOTE: this code has been altered slightly for use in Tcl: ***
 * *** 1. Use ckalloc and ckfree instead of  malloc and free.	 ***
 * *** 2. Add extra argument to regexp to specify the real	 ***
 * ***    start of the string separately from the start of the	 ***
 * ***    current search. This is needed to search for multiple	 ***
 * ***    matches within a string.				 ***
 * *** 3. Names have been changed, e.g. from regcomp to		 ***
 * ***    TclRegComp, to avoid clashes with other 		 ***
 * ***    regexp implementations used by applications. 		 ***
 * *** 4. Added errMsg declaration and TclRegError procedure	 ***
 * *** 5. Various lint-like things, such as casting arguments	 ***
 * ***	  in procedure calls.					 ***
 *
 * *** NOTE: This code has been altered for use in MT-Sturdy Tcl ***
 * *** 1. All use of static variables has been changed to access ***
 * ***    fields of a structure.                                 ***
 * *** 2. This in addition to changes to TclRegError makes the   ***
 * ***    code multi-thread safe.                                ***
 *
 * SCCS: @(#) regexp.c 1.12 96/04/02 13:54:57
 */

#include "tclInt.h"
#include "tclPort.h"

/*
 * The variable below is set to NULL before invoking regexp functions
 * and checked after those functions.  If an error occurred then TclRegError
 * will set the variable to point to a (static) error message.  This
 * mechanism unfortunately does not support multi-threading, but the
 * procedures TclRegError and TclGetRegError can be modified to use
 * thread-specific storage for the variable and thereby make the code
 * thread-safe.
 */

static char *errMsg = NULL;

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart	char that must begin a match; '\0' if none obvious
 * reganch	is the match anchored (at beginning-of-line only)?
 * regmust	string (pointer into program) that match must include, or NULL
 * regmlen	length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that TclRegComp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in TclRegExec() needs it and TclRegComp() is
 * computing it anyway.
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
#define	ANYOF	4	/* str	Match any character in this string. */
#define	ANYBUT	5	/* str	Match any character not in this string. */
#define	BRANCH	6	/* node	Match this alternative, or the next... */
#define	BACK	7	/* no	Match "", "next" ptr points backward. */
#define	EXACTLY	8	/* str	Match this string. */
#define	NOTHING	9	/* no	Match empty string. */
#define	STAR	10	/* node	Match this (simple) thing 0 or more times. */
#define	PLUS	11	/* node	Match this (simple) thing 1 or more times. */
#define	OPEN	20	/* no	Mark this point in input as start of #n. */
			/*	OPEN+1 is number 1, etc. */
#define	CLOSE	(OPEN+NSUBEXP)	/* no	Analogous to OPEN. */

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

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define	OP(p)	(*(p))
#define	NEXT(p)	(((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define	OPERAND(p)	((p) + 3)

/*
 * See regmagic.h for one further detail of program structure.
 */


/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARBITS)
#endif

#define	FAIL(m)	{ TclRegError(m); return(NULL); }
#define	ISMULT(c)	((c) == '*' || (c) == '+' || (c) == '?')
#define	META	"^$.[()|?+*\\"

/*
 * Flags to be passed up and down.
 */
#define	HASWIDTH	01	/* Known never to match null string. */
#define	SIMPLE		02	/* Simple enough to be STAR/PLUS operand. */
#define	SPSTART		04	/* Starts with * or +. */
#define	WORST		0	/* Worst case. */

/*
 * Global work variables for TclRegComp().
 */
struct regcomp_state  {
    char *regparse;		/* Input-scan pointer. */
    int regnpar;		/* () count. */
    char *regcode;		/* Code-emit pointer; &regdummy = don't. */
    long regsize;		/* Code size. */
};

static char regdummy;

/*
 * The first byte of the regexp internal "program" is actually this magic
 * number; the start node begins in the second byte.
 */
#define	MAGIC	0234


/*
 * Forward declarations for TclRegComp()'s friends.
 */

static char *		reg _ANSI_ARGS_((int paren, int *flagp,
			    struct regcomp_state *rcstate));
static char *		regatom _ANSI_ARGS_((int *flagp,
			    struct regcomp_state *rcstate));
static char *		regbranch _ANSI_ARGS_((int *flagp,
			    struct regcomp_state *rcstate));
static void		regc _ANSI_ARGS_((int b,
			    struct regcomp_state *rcstate));
static void		reginsert _ANSI_ARGS_((int op, char *opnd,
			    struct regcomp_state *rcstate));
static char *		regnext _ANSI_ARGS_((char *p));
static char *		regnode _ANSI_ARGS_((int op,
			    struct regcomp_state *rcstate));
static void 		regoptail _ANSI_ARGS_((char *p, char *val));
static char *		regpiece _ANSI_ARGS_((int *flagp,
			    struct regcomp_state *rcstate));
static void 		regtail _ANSI_ARGS_((char *p, char *val));

#ifdef STRCSPN
static int strcspn _ANSI_ARGS_((char *s1, char *s2));
#endif

/*
 - TclRegComp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
regexp *
TclRegComp(exp)
char *exp;
{
	register regexp *r;
	register char *scan;
	register char *longest;
	register int len;
	int flags;
	struct regcomp_state state;
	struct regcomp_state *rcstate= &state;

	if (exp == NULL)
		FAIL("NULL argument");

	/* First pass: determine size, legality. */
	rcstate->regparse = exp;
	rcstate->regnpar = 1;
	rcstate->regsize = 0L;
	rcstate->regcode = &regdummy;
	regc(MAGIC, rcstate);
	if (reg(0, &flags, rcstate) == NULL)
		return(NULL);

	/* Small enough for pointer-storage convention? */
	if (rcstate->regsize >= 32767L)		/* Probably could be 65535L. */
		FAIL("regexp too big");

	/* Allocate space. */
	r = (regexp *)ckalloc(sizeof(regexp) + (unsigned)rcstate->regsize);
	if (r == NULL)
		FAIL("out of space");

	/* Second pass: emit code. */
	rcstate->regparse = exp;
	rcstate->regnpar = 1;
	rcstate->regcode = r->program;
	regc(MAGIC, rcstate);
	if (reg(0, &flags, rcstate) == NULL)
		return(NULL);

	/* Dig out information for optimizations. */
	r->regstart = '\0';	/* Worst-case defaults. */
	r->reganch = 0;
	r->regmust = NULL;
	r->regmlen = 0;
	scan = r->program+1;			/* First BRANCH. */
	if (OP(regnext(scan)) == END) {		/* Only one top-level choice. */
		scan = OPERAND(scan);

		/* Starting-point info. */
		if (OP(scan) == EXACTLY)
			r->regstart = *OPERAND(scan);
		else if (OP(scan) == BOL)
			r->reganch++;

		/*
		 * If there's something expensive in the r.e., find the
		 * longest literal string that must appear and make it the
		 * regmust.  Resolve ties in favor of later strings, since
		 * the regstart check works with the beginning of the r.e.
		 * and avoiding duplication strengthens checking.  Not a
		 * strong reason, but sufficient in the absence of others.
		 */
		if (flags&SPSTART) {
			longest = NULL;
			len = 0;
			for (; scan != NULL; scan = regnext(scan))
				if (OP(scan) == EXACTLY && ((int) strlen(OPERAND(scan))) >= len) {
					longest = OPERAND(scan);
					len = strlen(OPERAND(scan));
				}
			r->regmust = longest;
			r->regmlen = len;
		}
	}

	return(r);
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
static char *
reg(paren, flagp, rcstate)
int paren;			/* Parenthesized? */
int *flagp;
struct regcomp_state *rcstate;
{
	register char *ret;
	register char *br;
	register char *ender;
	register int parno = 0;
	int flags;

	*flagp = HASWIDTH;	/* Tentatively. */

	/* Make an OPEN node, if parenthesized. */
	if (paren) {
		if (rcstate->regnpar >= NSUBEXP)
			FAIL("too many ()");
		parno = rcstate->regnpar;
		rcstate->regnpar++;
		ret = regnode(OPEN+parno,rcstate);
	} else
		ret = NULL;

	/* Pick up the branches, linking them together. */
	br = regbranch(&flags,rcstate);
	if (br == NULL)
		return(NULL);
	if (ret != NULL)
		regtail(ret, br);	/* OPEN -> first. */
	else
		ret = br;
	if (!(flags&HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags&SPSTART;
	while (*rcstate->regparse == '|') {
		rcstate->regparse++;
		br = regbranch(&flags,rcstate);
		if (br == NULL)
			return(NULL);
		regtail(ret, br);	/* BRANCH -> BRANCH. */
		if (!(flags&HASWIDTH))
			*flagp &= ~HASWIDTH;
		*flagp |= flags&SPSTART;
	}

	/* Make a closing node, and hook it on the end. */
	ender = regnode((paren) ? CLOSE+parno : END,rcstate);	
	regtail(ret, ender);

	/* Hook the tails of the branches to the closing node. */
	for (br = ret; br != NULL; br = regnext(br))
		regoptail(br, ender);

	/* Check for proper termination. */
	if (paren && *rcstate->regparse++ != ')') {
		FAIL("unmatched ()");
	} else if (!paren && *rcstate->regparse != '\0') {
		if (*rcstate->regparse == ')') {
			FAIL("unmatched ()");
		} else
			FAIL("junk on end");	/* "Can't happen". */
		/* NOTREACHED */
	}

	return(ret);
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
static char *
regbranch(flagp, rcstate)
int *flagp;
struct regcomp_state *rcstate;
{
	register char *ret;
	register char *chain;
	register char *latest;
	int flags;

	*flagp = WORST;		/* Tentatively. */

	ret = regnode(BRANCH,rcstate);
	chain = NULL;
	while (*rcstate->regparse != '\0' && *rcstate->regparse != '|' &&
				*rcstate->regparse != ')') {
		latest = regpiece(&flags, rcstate);
		if (latest == NULL)
			return(NULL);
		*flagp |= flags&HASWIDTH;
		if (chain == NULL)	/* First piece. */
			*flagp |= flags&SPSTART;
		else
			regtail(chain, latest);
		chain = latest;
	}
	if (chain == NULL)	/* Loop ran zero times. */
		(void) regnode(NOTHING,rcstate);

	return(ret);
}

/*
 - regpiece - something followed by possible [*+?]
 *
 * Note that the branching code sequences used for ? and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static char *
regpiece(flagp, rcstate)
int *flagp;
struct regcomp_state *rcstate;
{
	register char *ret;
	register char op;
	register char *next;
	int flags;

	ret = regatom(&flags,rcstate);
	if (ret == NULL)
		return(NULL);

	op = *rcstate->regparse;
	if (!ISMULT(op)) {
		*flagp = flags;
		return(ret);
	}

	if (!(flags&HASWIDTH) && op != '?')
		FAIL("*+ operand could be empty");
	*flagp = (op != '+') ? (WORST|SPSTART) : (WORST|HASWIDTH);

	if (op == '*' && (flags&SIMPLE))
		reginsert(STAR, ret, rcstate);
	else if (op == '*') {
		/* Emit x* as (x&|), where & means "self". */
		reginsert(BRANCH, ret, rcstate);			/* Either x */
		regoptail(ret, regnode(BACK,rcstate));		/* and loop */
		regoptail(ret, ret);			/* back */
		regtail(ret, regnode(BRANCH,rcstate));		/* or */
		regtail(ret, regnode(NOTHING,rcstate));		/* null. */
	} else if (op == '+' && (flags&SIMPLE))
		reginsert(PLUS, ret, rcstate);
	else if (op == '+') {
		/* Emit x+ as x(&|), where & means "self". */
		next = regnode(BRANCH,rcstate);			/* Either */
		regtail(ret, next);
		regtail(regnode(BACK,rcstate), ret);		/* loop back */
		regtail(next, regnode(BRANCH,rcstate));		/* or */
		regtail(ret, regnode(NOTHING,rcstate));		/* null. */
	} else if (op == '?') {
		/* Emit x? as (x|) */
		reginsert(BRANCH, ret, rcstate);			/* Either x */
		regtail(ret, regnode(BRANCH,rcstate));		/* or */
		next = regnode(NOTHING,rcstate);		/* null. */
		regtail(ret, next);
		regoptail(ret, next);
	}
	rcstate->regparse++;
	if (ISMULT(*rcstate->regparse))
		FAIL("nested *?+");

	return(ret);
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.  Backslashed characters are exceptions, each becoming a
 * separate node; the code is simpler that way and it's not worth fixing.
 */
static char *
regatom(flagp, rcstate)
int *flagp;
struct regcomp_state *rcstate;
{
	register char *ret;
	int flags;

	*flagp = WORST;		/* Tentatively. */

	switch (*rcstate->regparse++) {
	case '^':
		ret = regnode(BOL,rcstate);
		break;
	case '$':
		ret = regnode(EOL,rcstate);
		break;
	case '.':
		ret = regnode(ANY,rcstate);
		*flagp |= HASWIDTH|SIMPLE;
		break;
	case '[': {
			register int clss;
			register int classend;

			if (*rcstate->regparse == '^') {	/* Complement of range. */
				ret = regnode(ANYBUT,rcstate);
				rcstate->regparse++;
			} else
				ret = regnode(ANYOF,rcstate);
			if (*rcstate->regparse == ']' || *rcstate->regparse == '-')
				regc(*rcstate->regparse++,rcstate);
			while (*rcstate->regparse != '\0' && *rcstate->regparse != ']') {
				if (*rcstate->regparse == '-') {
					rcstate->regparse++;
					if (*rcstate->regparse == ']' || *rcstate->regparse == '\0')
						regc('-',rcstate);
					else {
						clss = UCHARAT(rcstate->regparse-2)+1;
						classend = UCHARAT(rcstate->regparse);
						if (clss > classend+1)
							FAIL("invalid [] range");
						for (; clss <= classend; clss++)
							regc((char)clss,rcstate);
						rcstate->regparse++;
					}
				} else
					regc(*rcstate->regparse++,rcstate);
			}
			regc('\0',rcstate);
			if (*rcstate->regparse != ']')
				FAIL("unmatched []");
			rcstate->regparse++;
			*flagp |= HASWIDTH|SIMPLE;
		}
		break;
	case '(':
		ret = reg(1, &flags, rcstate);
		if (ret == NULL)
			return(NULL);
		*flagp |= flags&(HASWIDTH|SPSTART);
		break;
	case '\0':
	case '|':
	case ')':
		FAIL("internal urp");	/* Supposed to be caught earlier. */
		/* NOTREACHED */
		break;
	case '?':
	case '+':
	case '*':
		FAIL("?+* follows nothing");
		/* NOTREACHED */
		break;
	case '\\':
		if (*rcstate->regparse == '\0')
			FAIL("trailing \\");
		ret = regnode(EXACTLY,rcstate);
		regc(*rcstate->regparse++,rcstate);
		regc('\0',rcstate);
		*flagp |= HASWIDTH|SIMPLE;
		break;
	default: {
			register int len;
			register char ender;

			rcstate->regparse--;
			len = strcspn(rcstate->regparse, META);
			if (len <= 0)
				FAIL("internal disaster");
			ender = *(rcstate->regparse+len);
			if (len > 1 && ISMULT(ender))
				len--;		/* Back off clear of ?+* operand. */
			*flagp |= HASWIDTH;
			if (len == 1)
				*flagp |= SIMPLE;
			ret = regnode(EXACTLY,rcstate);
			while (len > 0) {
				regc(*rcstate->regparse++,rcstate);
				len--;
			}
			regc('\0',rcstate);
		}
		break;
	}

	return(ret);
}

/*
 - regnode - emit a node
 */
static char *			/* Location. */
regnode(op, rcstate)
int op;
struct regcomp_state *rcstate;
{
	register char *ret;
	register char *ptr;

	ret = rcstate->regcode;
	if (ret == &regdummy) {
		rcstate->regsize += 3;
		return(ret);
	}

	ptr = ret;
	*ptr++ = (char)op;
	*ptr++ = '\0';		/* Null "next" pointer. */
	*ptr++ = '\0';
	rcstate->regcode = ptr;

	return(ret);
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void
regc(b, rcstate)
int b;
struct regcomp_state *rcstate;
{
	if (rcstate->regcode != &regdummy)
		*rcstate->regcode++ = (char)b;
	else
		rcstate->regsize++;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void
reginsert(op, opnd, rcstate)
int op;
char *opnd;
struct regcomp_state *rcstate;
{
	register char *src;
	register char *dst;
	register char *place;

	if (rcstate->regcode == &regdummy) {
		rcstate->regsize += 3;
		return;
	}

	src = rcstate->regcode;
	rcstate->regcode += 3;
	dst = rcstate->regcode;
	while (src > opnd)
		*--dst = *--src;

	place = opnd;		/* Op node, where operand used to be. */
	*place++ = (char)op;
	*place++ = '\0';
	*place = '\0';
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void
regtail(p, val)
char *p;
char *val;
{
	register char *scan;
	register char *temp;
	register int offset;

	if (p == &regdummy)
		return;

	/* Find last node. */
	scan = p;
	for (;;) {
		temp = regnext(scan);
		if (temp == NULL)
			break;
		scan = temp;
	}

	if (OP(scan) == BACK)
		offset = scan - val;
	else
		offset = val - scan;
	*(scan+1) = (char)((offset>>8)&0377);
	*(scan+2) = (char)(offset&0377);
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */
static void
regoptail(p, val)
char *p;
char *val;
{
	/* "Operandless" and "op != BRANCH" are synonymous in practice. */
	if (p == NULL || p == &regdummy || OP(p) != BRANCH)
		return;
	regtail(OPERAND(p), val);
}

/*
 * TclRegExec and friends
 */

/*
 * Global work variables for TclRegExec().
 */
struct regexec_state  {
    char *reginput;		/* String-input pointer. */
    char *regbol;		/* Beginning of input, for ^ check. */
    char **regstartp;	/* Pointer to startp array. */
    char **regendp;		/* Ditto for endp. */
};

/*
 * Forwards.
 */
static int 		regtry _ANSI_ARGS_((regexp *prog, char *string,
			    struct regexec_state *restate));
static int 		regmatch _ANSI_ARGS_((char *prog,
			    struct regexec_state *restate));
static int 		regrepeat _ANSI_ARGS_((char *p,
			    struct regexec_state *restate));

#ifdef DEBUG
int regnarrate = 0;
void regdump _ANSI_ARGS_((regexp *r));
static char *regprop _ANSI_ARGS_((char *op));
#endif

/*
 - TclRegExec - match a regexp against a string
 */
int
TclRegExec(prog, string, start)
register regexp *prog;
register char *string;
char *start;
{
	register char *s;
	struct regexec_state state;
	struct regexec_state *restate= &state;

	/* Be paranoid... */
	if (prog == NULL || string == NULL) {
		TclRegError("NULL parameter");
		return(0);
	}

	/* Check validity of program. */
	if (UCHARAT(prog->program) != MAGIC) {
		TclRegError("corrupted program");
		return(0);
	}

	/* If there is a "must appear" string, look for it. */
	if (prog->regmust != NULL) {
		s = string;
		while ((s = strchr(s, prog->regmust[0])) != NULL) {
			if (strncmp(s, prog->regmust, (size_t) prog->regmlen)
			    == 0)
				break;	/* Found it. */
			s++;
		}
		if (s == NULL)	/* Not present. */
			return(0);
	}

	/* Mark beginning of line for ^ . */
	restate->regbol = start;

	/* Simplest case:  anchored match need be tried only once. */
	if (prog->reganch)
		return(regtry(prog, string, restate));

	/* Messy cases:  unanchored match. */
	s = string;
	if (prog->regstart != '\0')
		/* We know what char it must start with. */
		while ((s = strchr(s, prog->regstart)) != NULL) {
			if (regtry(prog, s, restate))
				return(1);
			s++;
		}
	else
		/* We don't -- general case. */
		do {
			if (regtry(prog, s, restate))
				return(1);
		} while (*s++ != '\0');

	/* Failure. */
	return(0);
}

/*
 - regtry - try match at specific point
 */
static int			/* 0 failure, 1 success */
regtry(prog, string, restate)
regexp *prog;
char *string;
struct regexec_state *restate;
{
	register int i;
	register char **sp;
	register char **ep;

	restate->reginput = string;
	restate->regstartp = prog->startp;
	restate->regendp = prog->endp;

	sp = prog->startp;
	ep = prog->endp;
	for (i = NSUBEXP; i > 0; i--) {
		*sp++ = NULL;
		*ep++ = NULL;
	}
	if (regmatch(prog->program + 1,restate)) {
		prog->startp[0] = string;
		prog->endp[0] = restate->reginput;
		return(1);
	} else
		return(0);
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
static int			/* 0 failure, 1 success */
regmatch(prog, restate)
char *prog;
struct regexec_state *restate;
{
    register char *scan;	/* Current node. */
    char *next;		/* Next node. */

    scan = prog;
#ifdef DEBUG
    if (scan != NULL && regnarrate)
	fprintf(stderr, "%s(\n", regprop(scan));
#endif
    while (scan != NULL) {
#ifdef DEBUG
	if (regnarrate)
	    fprintf(stderr, "%s...\n", regprop(scan));
#endif
	next = regnext(scan);

	switch (OP(scan)) {
	    case BOL:
		if (restate->reginput != restate->regbol) {
		    return 0;
		}
		break;
	    case EOL:
		if (*restate->reginput != '\0') {
		    return 0;
		}
		break;
	    case ANY:
		if (*restate->reginput == '\0') {
		    return 0;
		}
		restate->reginput++;
		break;
	    case EXACTLY: {
		register int len;
		register char *opnd;

		opnd = OPERAND(scan);
		/* Inline the first character, for speed. */
		if (*opnd != *restate->reginput) {
		    return 0 ;
		}
		len = strlen(opnd);
		if (len > 1 && strncmp(opnd, restate->reginput, (size_t) len)
			!= 0) {
		    return 0;
		}
		restate->reginput += len;
		break;
	    }
	    case ANYOF:
		if (*restate->reginput == '\0'
			|| strchr(OPERAND(scan), *restate->reginput) == NULL) {
		    return 0;
		}
		restate->reginput++;
		break;
	    case ANYBUT:
		if (*restate->reginput == '\0'
			|| strchr(OPERAND(scan), *restate->reginput) != NULL) {
		    return 0;
		}
		restate->reginput++;
		break;
	    case NOTHING:
		break;
	    case BACK:
		break;
	    case OPEN+1:
	    case OPEN+2:
	    case OPEN+3:
	    case OPEN+4:
	    case OPEN+5:
	    case OPEN+6:
	    case OPEN+7:
	    case OPEN+8:
	    case OPEN+9: {
		register int no;
		register char *save;

	doOpen:
		no = OP(scan) - OPEN;
		save = restate->reginput;

		if (regmatch(next,restate)) {
		    /*
		     * Don't set startp if some later invocation of the
		     * same parentheses already has.
		     */
		    if (restate->regstartp[no] == NULL) {
			restate->regstartp[no] = save;
		    }
		    return 1;
		} else {
		    return 0;
		}
	    }
	    case CLOSE+1:
	    case CLOSE+2:
	    case CLOSE+3:
	    case CLOSE+4:
	    case CLOSE+5:
	    case CLOSE+6:
	    case CLOSE+7:
	    case CLOSE+8:
	    case CLOSE+9: {
		register int no;
		register char *save;

	doClose:
		no = OP(scan) - CLOSE;
		save = restate->reginput;

		if (regmatch(next,restate)) {
				/*
				 * Don't set endp if some later
				 * invocation of the same parentheses
				 * already has.
				 */
		    if (restate->regendp[no] == NULL)
			restate->regendp[no] = save;
		    return 1;
		} else {
		    return 0;
		}
	    }
	    case BRANCH: {
		register char *save;

		if (OP(next) != BRANCH) { /* No choice. */
		    next = OPERAND(scan); /* Avoid recursion. */
		} else {
		    do {
			save = restate->reginput;
			if (regmatch(OPERAND(scan),restate))
			    return(1);
			restate->reginput = save;
			scan = regnext(scan);
		    } while (scan != NULL && OP(scan) == BRANCH);
		    return 0;
		}
		break;
	    }
	    case STAR:
	    case PLUS: {
		register char nextch;
		register int no;
		register char *save;
		register int min;

		/*
		 * Lookahead to avoid useless match attempts
		 * when we know what character comes next.
		 */
		nextch = '\0';
		if (OP(next) == EXACTLY)
		    nextch = *OPERAND(next);
		min = (OP(scan) == STAR) ? 0 : 1;
		save = restate->reginput;
		no = regrepeat(OPERAND(scan),restate);
		while (no >= min) {
		    /* If it could work, try it. */
		    if (nextch == '\0' || *restate->reginput == nextch)
			if (regmatch(next,restate))
			    return(1);
		    /* Couldn't or didn't -- back up. */
		    no--;
		    restate->reginput = save + no;
		}
		return(0);
	    }
	    case END:
		return(1);	/* Success! */
	    default:
		if (OP(scan) > OPEN && OP(scan) < OPEN+NSUBEXP) {
		    goto doOpen;
		} else if (OP(scan) > CLOSE && OP(scan) < CLOSE+NSUBEXP) {
		    goto doClose;
		}
		TclRegError("memory corruption");
		return 0;
	}

	scan = next;
    }

    /*
     * We get here only if there's trouble -- normally "case END" is
     * the terminating point.
     */
    TclRegError("corrupted pointers");
    return(0);
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int
regrepeat(p, restate)
char *p;
struct regexec_state *restate;
{
	register int count = 0;
	register char *scan;
	register char *opnd;

	scan = restate->reginput;
	opnd = OPERAND(p);
	switch (OP(p)) {
	case ANY:
		count = strlen(scan);
		scan += count;
		break;
	case EXACTLY:
		while (*opnd == *scan) {
			count++;
			scan++;
		}
		break;
	case ANYOF:
		while (*scan != '\0' && strchr(opnd, *scan) != NULL) {
			count++;
			scan++;
		}
		break;
	case ANYBUT:
		while (*scan != '\0' && strchr(opnd, *scan) == NULL) {
			count++;
			scan++;
		}
		break;
	default:		/* Oh dear.  Called inappropriately. */
		TclRegError("internal foulup");
		count = 0;	/* Best compromise. */
		break;
	}
	restate->reginput = scan;

	return(count);
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static char *
regnext(p)
register char *p;
{
	register int offset;

	if (p == &regdummy)
		return(NULL);

	offset = NEXT(p);
	if (offset == 0)
		return(NULL);

	if (OP(p) == BACK)
		return(p-offset);
	else
		return(p+offset);
}

#ifdef DEBUG

static char *regprop();

/*
 - regdump - dump a regexp onto stdout in vaguely comprehensible form
 */
void
regdump(r)
regexp *r;
{
	register char *s;
	register char op = EXACTLY;	/* Arbitrary non-END op. */
	register char *next;


	s = r->program + 1;
	while (op != END) {	/* While that wasn't END last time... */
		op = OP(s);
		printf("%2d%s", s-r->program, regprop(s));	/* Where, what. */
		next = regnext(s);
		if (next == NULL)		/* Next ptr. */
			printf("(0)");
		else 
			printf("(%d)", (s-r->program)+(next-s));
		s += 3;
		if (op == ANYOF || op == ANYBUT || op == EXACTLY) {
			/* Literal string, where present. */
			while (*s != '\0') {
				putchar(*s);
				s++;
			}
			s++;
		}
		putchar('\n');
	}

	/* Header fields of interest. */
	if (r->regstart != '\0')
		printf("start `%c' ", r->regstart);
	if (r->reganch)
		printf("anchored ");
	if (r->regmust != NULL)
		printf("must have \"%s\"", r->regmust);
	printf("\n");
}

/*
 - regprop - printable representation of opcode
 */
static char *
regprop(op)
char *op;
{
	register char *p;
	static char buf[50];

	(void) strcpy(buf, ":");

	switch (OP(op)) {
	case BOL:
		p = "BOL";
		break;
	case EOL:
		p = "EOL";
		break;
	case ANY:
		p = "ANY";
		break;
	case ANYOF:
		p = "ANYOF";
		break;
	case ANYBUT:
		p = "ANYBUT";
		break;
	case BRANCH:
		p = "BRANCH";
		break;
	case EXACTLY:
		p = "EXACTLY";
		break;
	case NOTHING:
		p = "NOTHING";
		break;
	case BACK:
		p = "BACK";
		break;
	case END:
		p = "END";
		break;
	case OPEN+1:
	case OPEN+2:
	case OPEN+3:
	case OPEN+4:
	case OPEN+5:
	case OPEN+6:
	case OPEN+7:
	case OPEN+8:
	case OPEN+9:
		sprintf(buf+strlen(buf), "OPEN%d", OP(op)-OPEN);
		p = NULL;
		break;
	case CLOSE+1:
	case CLOSE+2:
	case CLOSE+3:
	case CLOSE+4:
	case CLOSE+5:
	case CLOSE+6:
	case CLOSE+7:
	case CLOSE+8:
	case CLOSE+9:
		sprintf(buf+strlen(buf), "CLOSE%d", OP(op)-CLOSE);
		p = NULL;
		break;
	case STAR:
		p = "STAR";
		break;
	case PLUS:
		p = "PLUS";
		break;
	default:
		if (OP(op) > OPEN && OP(op) < OPEN+NSUBEXP) {
		    sprintf(buf+strlen(buf), "OPEN%d", OP(op)-OPEN);
		    p = NULL;
		    break;
		} else if (OP(op) > CLOSE && OP(op) < CLOSE+NSUBEXP) {
		    sprintf(buf+strlen(buf), "CLOSE%d", OP(op)-CLOSE);
		    p = NULL;
		} else {
		    TclRegError("corrupted opcode");
		}
		break;
	}
	if (p != NULL)
		(void) strcat(buf, p);
	return(buf);
}
#endif

/*
 * The following is provided for those people who do not have strcspn() in
 * their C libraries.  They should get off their butts and do something
 * about it; at least one public-domain implementation of those (highly
 * useful) string routines has been published on Usenet.
 */
#ifdef STRCSPN
/*
 * strcspn - find length of initial segment of s1 consisting entirely
 * of characters not from s2
 */

static int
strcspn(s1, s2)
char *s1;
char *s2;
{
	register char *scan1;
	register char *scan2;
	register int count;

	count = 0;
	for (scan1 = s1; *scan1 != '\0'; scan1++) {
		for (scan2 = s2; *scan2 != '\0';)	/* ++ moved down. */
			if (*scan1 == *scan2++)
				return(count);
		count++;
	}
	return(count);
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TclRegError --
 *
 *	This procedure is invoked by the regexp code when an error
 *	occurs.  It saves the error message so it can be seen by the
 *	code that called Spencer's code.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The value of "string" is saved in "errMsg".
 *
 *----------------------------------------------------------------------
 */

void
TclRegError(string)
    char *string;			/* Error message. */
{
    errMsg = string;
}

char *
TclGetRegError()
{
    return errMsg;
}
