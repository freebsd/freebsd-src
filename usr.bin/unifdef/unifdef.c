/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Dave Yost. Support for #if and #elif was added by Tony Finch.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1985, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";

#ifdef __IDSTRING
__IDSTRING(Berkeley, "@(#)unifdef.c	8.1 (Berkeley) 6/6/93");
__IDSTRING(NetBSD, "$NetBSD: unifdef.c,v 1.8 2000/07/03 02:51:36 matt Exp $");
__IDSTRING(dotat, "$dotat: things/unifdef.c,v 1.75 2002/09/24 19:16:29 fanf2 Exp $");
#endif
#ifdef __FBSDID
__FBSDID("$FreeBSD$");
#endif
#endif

/*
 * unifdef - remove ifdef'ed lines
 *
 *  Warning: will not work correctly if input contains nul characters.
 *
 *  Wishlist:
 *      provide an option which will append the name of the
 *        appropriate symbol after #else's and #endif's
 *      provide an option which will check symbols after
 *        #else's and #endif's to see that they match their
 *        corresponding #ifdef or #ifndef
 *      generate #line directives in place of deleted code
 */

#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* types of input lines: */
typedef enum {
	LT_PLAIN,		/* ordinary line */
	LT_TRUE,		/* a true #if */
	LT_FALSE,		/* a false #if */
	LT_ELTRUE,		/* a true #elif */
	LT_ELFALSE,		/* a false #elif */
	LT_IF,			/* an unknown #if */
	LT_ELIF,		/* an unknown #elif */
	LT_ELSE,		/* #else */
	LT_ENDIF,		/* #endif */
	LT_EOF			/* end of file */
} Linetype;

typedef enum {		/* 0 or 1: pass thru; 1 or 2: ignore comments */
	REJ_NO,
	REJ_IGNORE,
	REJ_YES
} Reject_level;

typedef enum {
	NO_COMMENT = false,
	C_COMMENT,
	CXX_COMMENT
} Comment_state;

typedef enum {
	QUOTE_NONE = false,
	QUOTE_SINGLE,
	QUOTE_DOUBLE
} Quote_state;

const char *const errs[] = {
#define NO_ERR      0
	"",
#define END_ERR     1
	"",
#define ELIF_ERR    2
	"Inappropriate elif",
#define ELSE_ERR    3
	"Inappropriate else",
#define ENDIF_ERR   4
	"Inappropriate endif",
#define IEOF_ERR    5
	"Premature EOF in ifdef",
#define CEOF_ERR    6
	"Premature EOF in comment",
#define Q1EOF_ERR   7
	"Premature EOF in quoted character",
#define Q2EOF_ERR   8
	"Premature EOF in quoted string"
};

/*
 * These are the operators that are supported by the expression evaluator.
 */
static int op_lt(int a, int b) { return a < b; }
static int op_gt(int a, int b) { return a > b; }
static int op_le(int a, int b) { return a <= b; }
static int op_ge(int a, int b) { return a >= b; }
static int op_eq(int a, int b) { return a == b; }
static int op_ne(int a, int b) { return a != b; }
static int op_or(int a, int b) { return a || b; }
static int op_and(int a, int b) { return a && b; }

struct ops;

/*
 * An evaluation function takes three arguments, as follows: (1) a pointer to
 * an element of the precedence table which lists the operators at the current
 * level of precedence; (2) a pointer to an integer which will receive the
 * value of the expression; and (3) a pointer to a char* that points to the
 * expression to be evaluated and that is updated to the end of the expression
 * when evaluation is complete. The function returns LT_FALSE if the value of
 * the expression is zero, LT_TRUE if it is non-zero, or LT_IF if the
 * expression could not be evaluated.
 */
typedef Linetype eval_fn(struct ops *, int *, const char **);

eval_fn eval_table, eval_unary;

/*
 * The precedence table. Expressions involving binary operators are evaluated
 * in a table-driven way by eval_table. When it evaluates a subexpression it
 * calls the inner function with its first argument pointing to the next
 * element of the table. Innermost expressions have special non-table-driven
 * handling.
 */
struct ops {
	eval_fn *inner;
	struct op {
		const char *str;
		int (*fn)(int, int);
	} op[5];
} eval_ops[] = {
	{ eval_table, { { "||", op_or } } },
	{ eval_table, { { "&&", op_and } } },
	{ eval_table, { { "==", op_eq },
			{ "!=", op_ne } } },
	{ eval_unary, { { "<=", op_le },
			{ ">=", op_ge },
			{ "<", op_lt },
			{ ">", op_gt } } }
};

FILE           *input;
const char     *filename;
int             linenum;	/* current line number */
int             stifline;	/* start of current #if */
int             stqcline;	/* start of current coment or quote */
bool            keepthis;	/* ignore this #if's value 'cause it's const */

#define MAXLINE 1024
#define KWSIZE 8
/* tline has extra space so that it isn't overflowed when editing #elifs */
char    tline[MAXLINE+KWSIZE];	/* input buffer */
char   *keyword;        	/* used for editing #elif's */

bool            complement;	/* -c option in effect: do the complement */
bool            debugging;	/* -d option in effect: debugging reports */
bool            killconsts;	/* -k option in effect: eval constant #ifs */
bool            lnblank;	/* -l option in effect: blank deleted lines */
bool            symlist;	/* -s option in effect: output symbol list */
bool            text;		/* -t option in effect: this is a text file */

int             exitstat;	/* program exit status */

#define MAXSYMS 1000
const char     *symname[MAXSYMS];	/* symbol name */
const char     *value[MAXSYMS];		/* -Dsym=value */
bool            ignore[MAXSYMS];	/* -iDsym or -iUsym */

int             nsyms = 1;	/* symbol 0 is used for tracking #ifs */

Reject_level    reject;		/* what kind of filtering we are doing */
Comment_state   incomment;	/* inside C comment */
Quote_state     inquote;	/* inside single or double quotes */

Linetype        checkline(int *);
void            debug(const char *, ...);
Linetype        process(int);
void            doif(int, Linetype, bool);
void            elif2if(void);
void            elif2endif(void);
void            error(int, int);
void            addsym(bool, bool, char *);
int             findsym(const char *);
void            flushline(bool);
int             getline(char *, int, FILE *, bool);
Linetype        ifeval(const char **);
int             main(int, char **);
const char     *skipcomment(const char *);
const char     *skipquote(const char *, Quote_state);
const char     *skipsym(const char *);
void            usage(void);

#define endsym(c) (!isalpha((unsigned char)c) && !isdigit((unsigned char)c) && c != '_')

int
main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "i:D:U:I:cdklst")) != -1)
		switch (opt) {
		case 'i': /* treat stuff controlled by these symbols as text */
			/*
			 * For strict backwards-compatibility the U or D
			 * should be immediately after the -i but it doesn't
			 * matter much if we relax that requirement.
			 */
			opt = *optarg++;
			if (opt == 'D')
				addsym(true, true, optarg);
			else if (opt == 'U')
				addsym(true, false, optarg);
			else
				usage();
			break;
		case 'D': /* define a symbol */
			addsym(false, true, optarg);
			break;
		case 'U': /* undef a symbol */
			addsym(false, false, optarg);
			break;
		case 'I':
			/* ignore for compatibility with cpp */
			break;
		case 'c': /* treat -D as -U and vice versa */
			complement = true;
			break;
		case 'k': /* process constant #ifs */
			killconsts = true;
			break;
		case 'd':
			debugging = true;
			break;
		case 'l': /* blank deleted lines instead of omitting them */
			lnblank = true;
			break;
		case 's': /* only output list of symbols that control #ifs */
			symlist = true;
			break;
		case 't': /* don't parse C comments or strings */
			text = true;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
	if (nsyms == 1 && !symlist) {
		warnx("must -D or -U at least one symbol");
		usage();
	}
	if (argc > 1) {
		errx(2, "can only do one file");
	} else if (argc == 1 && strcmp(*argv, "-") != 0) {
		filename = *argv;
		if ((input = fopen(filename, "r")) != NULL) {
			(void) process(0);
			(void) fclose(input);
		} else
			err(2, "can't open %s", *argv);
	} else {
		filename = "[stdin]";
		input = stdin;
		(void) process(0);
	}

	exit(exitstat);
}

void
usage(void)
{
	fprintf (stderr, "usage: %s",
"unifdef [-cdklst] [[-Dsym[=val]] [-Usym] [-iDsym[=val]] [-iUsym]] ... [file]\n");
	exit (2);
}

/*
 * This function processes #if lines and alters the pass-through
 * state accordingly. All the complicated state transition suff is
 * dealt with in this function, as well as checking that the
 * #if/#elif/#else/#endif lines happen in the correct order. Lines
 * between #if lines are handled by a recursive call to process().
 */
void
doif(int depth, Linetype lineval, bool ignoring)
{
	Reject_level savereject;
	bool active;
	bool donetrue;
	bool inelse;
	int saveline;

	debug("#if line %d code %d depth %d",
	    linenum, lineval, depth);
	saveline = stifline;
	stifline = linenum;
	savereject = reject;
	inelse = false;
	donetrue = false;
	if (lineval == LT_IF || reject != REJ_NO) {
		active = false;
		ignoring = false;
		flushline(true);
	} else if (ignoring) {
		active = false;
		flushline(true);
		if (lineval == LT_FALSE)
			reject = REJ_IGNORE;
		else
			donetrue = true;
	} else {
		active = true;
		flushline(false);
		if (lineval == LT_FALSE)
			reject = REJ_YES;
		else
			donetrue = true;
	}
	debug("active %d ignore %d", active, ignoring);
	for (;;) {
		switch (lineval = process(depth)) {
		case LT_ELIF:
			debug("#elif start %d line %d code %d depth %d",
			    stifline, linenum, lineval, depth);
			if (inelse)
				error(ELIF_ERR, depth);
			donetrue = false;
			reject = savereject;
			if (active) {
				active = false;
				elif2if();
				flushline(true);
			} else {
				ignoring = false;
				flushline(true);
			}
			debug("active %d ignore %d", active, ignoring);
			break;
		case LT_ELTRUE:
		case LT_ELFALSE:
			debug("#elif start %d line %d code %d depth %d",
			    stifline, linenum, lineval, depth);
			if (inelse)
				error(ELIF_ERR, depth);
			if (active) {
				flushline(false);
			} else {
				ignoring = false;
				active = true;
				elif2endif();
				flushline(true);
			}
			if (lineval == LT_ELFALSE)
				reject = REJ_YES;
			else {
				reject = REJ_NO;
				donetrue = true;
			}
			debug("active %d ignore %d", active, ignoring);
			break;
		case LT_ELSE:
			debug("#else start %d line %d code %d depth %d",
			    stifline, linenum, lineval, depth);
			if (inelse)
				error(ELSE_ERR, depth);
			if (active) {
				flushline(false);
				if (reject == REJ_YES && !donetrue)
					reject = REJ_NO;
				else
					reject = REJ_YES;
			} else {
				flushline(true);
				if (ignoring) {
					if (reject == REJ_IGNORE)
						reject = REJ_NO;
					else
						reject = REJ_IGNORE;
				}
			}
			inelse = true;
			debug("active %d ignore %d", active, ignoring);
			break;
		case LT_ENDIF:
			debug("#endif start %d line %d code %d depth %d",
			    stifline, linenum, lineval, depth);
			if (active)
				flushline(false);
			else
				flushline(true);
			reject = savereject;
			stifline = saveline;
			return;
		default:
			/* bug */
			abort();
		}
	}
}

/*
 * The main file processing routine. This function deals with passing
 * through normal non-#if lines, correct nesting of #if sections, and
 * checking that things terminate correctly at the end of file. The
 * complicated stuff is delegated to doif().
 */
Linetype
process(int depth)
{
	Linetype lineval;
	int cursym;

	for (;;) {
		linenum++;
		if (getline(tline, MAXLINE, input, false) == EOF) {
			if (incomment)
				error(CEOF_ERR, depth);
			if (inquote == QUOTE_SINGLE)
				error(Q1EOF_ERR, depth);
			if (inquote == QUOTE_DOUBLE)
				error(Q2EOF_ERR, depth);
			if (depth != 0)
				error(IEOF_ERR, depth);
			return LT_EOF;
		}
		switch (lineval = checkline(&cursym)) {
		case LT_PLAIN:
			flushline(true);
			break;
		case LT_IF:
		case LT_TRUE:
		case LT_FALSE:
			doif(depth + 1, lineval, ignore[cursym]);
			break;
		case LT_ELIF:
		case LT_ELTRUE:
		case LT_ELFALSE:
		case LT_ELSE:
		case LT_ENDIF:
			if (depth != 0)
				return lineval;
			if (lineval == LT_ENDIF)
				error(ENDIF_ERR, depth);
			if (lineval == LT_ELSE)
				error(ELSE_ERR, depth);
			error(ELIF_ERR, depth);
		default:
			/* bug */
			abort();
		}
	}
}

/*
 * Parse a line and determine its type.
 */
Linetype
checkline(int *cursym)
{
	const char *cp;
	char *symp;
	Linetype retval;
	char kw[KWSIZE];

	retval = LT_PLAIN;
	cp = skipcomment(tline);
	if (*cp != '#'
	    || incomment
	    || inquote == QUOTE_SINGLE
	    || inquote == QUOTE_DOUBLE
	    )
		goto eol;

	cp = skipcomment(++cp);
	keyword = (char *)cp;
	symp = kw;
	while (!endsym(*cp)) {
		*symp = *cp++;
		if (++symp >= &kw[KWSIZE])
			goto eol;
	}
	*symp = '\0';

	if (strcmp(kw, "ifdef") == 0) {
		retval = LT_TRUE;
		goto ifdef;
	} else if (strcmp(kw, "ifndef") == 0) {
		retval = LT_FALSE;
	ifdef:
		cp = skipcomment(++cp);
		if (incomment) {
			retval = LT_PLAIN;
			goto eol;
		}
		if ((*cursym = findsym(cp)) == 0)
			retval = LT_IF;
		else if (value[*cursym] == NULL)
			retval = (retval == LT_TRUE)
			    ? LT_FALSE : LT_TRUE;
	} else if (strcmp(kw, "if") == 0) {
		retval = ifeval(&cp);
		cp = skipcomment(cp);
		if (*cp != '\n' || keepthis)
			retval = LT_IF;
		*cursym = 0;
	} else if (strcmp(kw, "elif") == 0) {
		retval = ifeval(&cp);
		cp = skipcomment(cp);
		if (*cp != '\n' || keepthis)
			retval = LT_ELIF;
		if (retval == LT_IF)
			retval = LT_ELIF;
		if (retval == LT_TRUE)
			retval = LT_ELTRUE;
		if (retval == LT_FALSE)
			retval = LT_ELFALSE;
		*cursym = 0;
	} else if (strcmp(kw, "else") == 0)
		retval = LT_ELSE;
	else if (strcmp(kw, "endif") == 0)
		retval = LT_ENDIF;

eol:
	if (!text && reject != REJ_IGNORE)
		for (; *cp;) {
			if (incomment)
				cp = skipcomment(cp);
			else if (inquote == QUOTE_SINGLE)
				cp = skipquote(cp, QUOTE_SINGLE);
			else if (inquote == QUOTE_DOUBLE)
				cp = skipquote(cp, QUOTE_DOUBLE);
			else if (*cp == '/' && (cp[1] == '*' || cp[1] == '/'))
				cp = skipcomment(cp);
			else if (*cp == '\'')
				cp = skipquote(cp, QUOTE_SINGLE);
			else if (*cp == '"')
				cp = skipquote(cp, QUOTE_DOUBLE);
			else
				cp++;
		}
	return retval;
}

/*
 * Turn a #elif line into a #if. This function is used when we are
 * processing a #if/#elif/#else/#endif sequence that starts off with a
 * #if that we understand (and therefore it has been deleted) which is
 * followed by a #elif that we don't understand and therefore must be
 * kept. We turn it into a #if to keep the nesting correct.
 */
void
elif2if(void)
{
	strncpy(keyword, "if  ", 4);
}

/*
 * Turn a #elif line into a #endif. This is used in the opposite
 * situation to elif2if, i.e. a #if that we don't understand is
 * followed by a #elif that we do; rather than deleting the #elif (as
 * we would for a #if) we turn it into a #endif to keep the nesting
 * correct.
 */
void
elif2endif(void)
{
	strcpy(keyword, "endif\n");
}

/*
 * Function for evaluating the innermost parts of expressions,
 * viz. !expr (expr) defined(symbol) symbol number
 * We reset the keepthis flag when we find a non-constant subexpression.
 */
Linetype
eval_unary(struct ops *ops, int *valp, const char **cpp)
{
	const char *cp;
	char *ep;
	int sym;

	cp = skipcomment(*cpp);
	if(*cp == '!') {
		debug("eval%d !", ops - eval_ops);
		cp++;
		if (eval_unary(ops, valp, &cp) == LT_IF)
			return LT_IF;
		*valp = !*valp;
	} else if (*cp == '(') {
		cp++;
		debug("eval%d (", ops - eval_ops);
		if (eval_table(eval_ops, valp, &cp) == LT_IF)
			return LT_IF;
		cp = skipcomment(cp);
		if (*cp++ != ')')
			return LT_IF;
	} else if (isdigit((unsigned char)*cp)) {
		debug("eval%d number", ops - eval_ops);
		*valp = strtol(cp, &ep, 0);
		cp = skipsym(cp);
	} else if (strncmp(cp, "defined", 7) == 0 && endsym(cp[7])) {
		cp = skipcomment(cp+7);
		debug("eval%d defined", ops - eval_ops);
		if (*cp++ != '(')
			return LT_IF;
		cp = skipcomment(cp);
		sym = findsym(cp);
		if (sym == 0 && !symlist)
			return LT_IF;
		*valp = (value[sym] != NULL);
		cp = skipsym(cp);
		cp = skipcomment(cp);
		if (*cp++ != ')')
			return LT_IF;
		keepthis = false;
	} else if (!endsym(*cp)) {
		debug("eval%d symbol", ops - eval_ops);
		sym = findsym(cp);
		if (sym == 0 && !symlist)
			return LT_IF;
		if (value[sym] == NULL)
			*valp = 0;
		else {
			*valp = strtol(value[sym], &ep, 0);
			if (*ep != '\0' || ep == value[sym])
				return LT_IF;
		}
		cp = skipsym(cp);
		keepthis = false;
	} else
		return LT_IF;

	*cpp = cp;
	debug("eval%d = %d", ops - eval_ops, *valp);
	return *valp ? LT_TRUE : LT_FALSE;
}

/*
 * Table-driven evaluation of binary operators.
 */
Linetype
eval_table(struct ops *ops, int *valp, const char **cpp)
{
	const char *cp;
	struct op *op;
	int val;

	debug("eval%d", ops - eval_ops);
	cp = *cpp;
	if (ops->inner(ops+1, valp, &cp) == LT_IF)
		return LT_IF;
	for (;;) {
		cp = skipcomment(cp);
		for (op = ops->op; op->str != NULL; op++)
			if (strncmp(cp, op->str, strlen(op->str)) == 0)
				break;
		if (op->str == NULL)
			break;
		cp += strlen(op->str);
		debug("eval%d %s", ops - eval_ops, op->str);
		if (ops->inner(ops+1, &val, &cp) == LT_IF)
			return LT_IF;
		*valp = op->fn(*valp, val);
	}

	*cpp = cp;
	debug("eval%d = %d", ops - eval_ops, *valp);
	return *valp ? LT_TRUE : LT_FALSE;
}

/*
 * Evaluate the expression on a #if or #elif line. If we can work out
 * the result we return LT_TRUE or LT_FALSE accordingly, otherwise we
 * return just a generic LT_IF. If the expression is constant and
 * we are not processing constant #ifs then the keepthis flag is true.
 */
Linetype
ifeval(const char **cpp)
{
	int val;
	debug("eval %s", *cpp);
	keepthis = killconsts ? false : true;
	return eval_table(eval_ops, &val, cpp);
}

/*
 * Skip over comments and stop at the next character position that is
 * not whitespace.
 */
const char *
skipcomment(const char *cp)
{
	if (incomment)
		goto inside;
	for (;; cp++) {
		while (*cp == ' ' || *cp == '\t')
			cp++;
		if (text)
			return cp;
		if (cp[0] != '/')
			return cp;

		if (cp[1] == '*') {
			if (!incomment) {
				incomment = C_COMMENT;
				stqcline = linenum;
			}
		} else if (cp[1] == '/') {
			if (!incomment) {
				incomment = CXX_COMMENT;
				stqcline = linenum;
			}
		} else
			return cp;

		cp += 2;
inside:
		if (incomment == C_COMMENT) {
			for (;;) {
				for (; *cp != '*'; cp++)
					if (*cp == '\0')
						return cp;
				if (*++cp == '/') {
					incomment = NO_COMMENT;
					break;
				}
			}
		}
		else if (incomment == CXX_COMMENT) {
			for (; *cp != '\n'; cp++)
				if (*cp == '\0')
					return cp;
			incomment = NO_COMMENT;
		}
	}
}

/*
 * Skip over a quoted string or character and stop at the next charaacter
 * position that is not whitespace.
 */
const char *
skipquote(const char *cp, Quote_state type)
{
	char qchar;

	qchar = type == QUOTE_SINGLE ? '\'' : '"';

	if (inquote == type)
		goto inside;
	for (;; cp++) {
		if (*cp != qchar)
			return cp;
		cp++;
		inquote = type;
		stqcline = linenum;
inside:
		for (;; cp++) {
			if (*cp == qchar)
				break;
			if (*cp == '\0' || (*cp == '\\' && *++cp == '\0'))
				return cp;
		}
		inquote = QUOTE_NONE;
	}
}

/*
 * Skip over an identifier.
 */
const char *
skipsym(const char *cp)
{
	while (!endsym(*cp))
		++cp;
	return cp;
}

/*
 * Look for the symbol in the symbol table. If is is found, we return
 * the symbol table index, else we return 0.
 */
int
findsym(const char *str)
{
	const char *cp;
	const char *symp;
	int symind;

	if (symlist) {
		for (cp = str; !endsym(*cp); cp++)
			continue;
		printf("%.*s\n", cp-str, str);
	}
	for (symind = 1; symind < nsyms; ++symind) {
		for (cp = str, symp = symname[symind]
		    ; *cp && *symp && *cp == *symp
		    ; cp++, symp++
		    )
			continue;
		if (*symp == '\0' && endsym(*cp)) {
			debug("findsym %s %s", symname[symind],
			    value[symind] ? value[symind] : "");
			return symind;
		}
	}
	return 0;
}

/*
 * Add a symbol to the symbol table.
 */
void
addsym(bool ignorethis, bool definethis, char *sym)
{
	int symind;
	char *val;

	symind = findsym(sym);
	if (symind == 0) {
		if (nsyms >= MAXSYMS)
			errx(2, "too many symbols");
		symind = nsyms++;
	}
	symname[symind] = sym;
	ignore[symind] = ignorethis;
	val = (char *)skipsym(sym);
	if (definethis) {
		if (*val == '=') {
			value[symind] = val+1;
			*val = '\0';
		} else if (*val == '\0')
			value[symind] = "";
		else
			usage();
	} else {
		if (*val != '\0')
			usage();
		value[symind] = NULL;
	}
}

/*
 * Read a line from the input and expand tabs if requested and (if
 * compiled in) treats form-feed as an end-of-line.
 */
int
getline(char *line, int maxline, FILE *inp, bool expandtabs)
{
	int tmp;
	int num;
	int chr;
#ifdef  FFSPECIAL
	static bool havechar = false;	/* have leftover char from last time */
	static char svchar;
#endif	/* FFSPECIAL */

	num = 0;
#ifdef  FFSPECIAL
	if (havechar) {
		havechar = false;
		chr = svchar;
		goto ent;
	}
#endif	/* FFSPECIAL */
	while (num + 8 < maxline) {	/* leave room for tab */
		chr = getc(inp);
		if (isprint(chr)) {
#ifdef  FFSPECIAL
	ent:
#endif	/* FFSPECIAL */
			*line++ = chr;
			num++;
		} else
			switch (chr) {
			case EOF:
				return EOF;

			case '\t':
				if (expandtabs) {
					num += tmp = 8 - (num & 7);
					do
						*line++ = ' ';
					while (--tmp);
					break;
				}
			default:
				*line++ = chr;
				num++;
				break;

			case '\n':
				*line = '\n';
				num++;
				goto end;

#ifdef  FFSPECIAL
			case '\f':
				if (++num == 1)
					*line = '\f';
				else {
					*line = '\n';
					havechar = true;
					svchar = chr;
				}
				goto end;
#endif	/* FFSPECIAL */
			}
	}
end:
	*++line = '\0';
	return num;
}

/*
 * Write a line to the output or not, according to the current
 * filtering state.
 */
void
flushline(bool keep)
{
	if (symlist)
		return;
	if ((keep && reject != REJ_YES) ^ complement)
		fputs(tline, stdout);
	else if (lnblank)
		putc('\n', stdout);
	return;
}

void
debug(const char *msg, ...)
{
	va_list ap;

	if (debugging) {
		va_start(ap, msg);
		vwarnx(msg, ap);
		va_end(ap);
	}
}

void
error(int code, int depth)
{
	if (incomment || inquote)
		errx(2, "error in %s line %d: %s (#if depth %d)",
		    filename, stqcline, errs[code], depth);
	else
		errx(2, "error in %s line %d: %s"
		    " (#if depth %d start line %d)",
		    filename, linenum, errs[code], depth, stifline);
}
