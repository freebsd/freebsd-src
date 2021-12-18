/*	$NetBSD: cond.c,v 1.302 2021/12/12 09:36:00 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

/*
 * Handling of conditionals in a makefile.
 *
 * Interface:
 *	Cond_EvalLine   Evaluate the conditional directive, such as
 *			'.if <cond>', '.elifnmake <cond>', '.else', '.endif'.
 *
 *	Cond_EvalCondition
 *			Evaluate the conditional, which is either the argument
 *			of one of the .if directives or the condition in a
 *			':?then:else' variable modifier.
 *
 *	Cond_save_depth
 *	Cond_restore_depth
 *			Save and restore the nesting of the conditions, at
 *			the start and end of including another makefile, to
 *			ensure that in each makefile the conditional
 *			directives are well-balanced.
 */

#include <errno.h>

#include "make.h"
#include "dir.h"

/*	"@(#)cond.c	8.2 (Berkeley) 1/2/94"	*/
MAKE_RCSID("$NetBSD: cond.c,v 1.302 2021/12/12 09:36:00 rillig Exp $");

/*
 * The parsing of conditional expressions is based on this grammar:
 *	Or -> And ('||' And)*
 *	And -> Term ('&&' Term)*
 *	Term -> Function '(' Argument ')'
 *	Term -> Leaf Operator Leaf
 *	Term -> Leaf
 *	Term -> '(' Or ')'
 *	Term -> '!' Term
 *	Leaf -> "string"
 *	Leaf -> Number
 *	Leaf -> VariableExpression
 *	Leaf -> Symbol
 *	Operator -> '==' | '!=' | '>' | '<' | '>=' | '<='
 *
 * 'Symbol' is an unquoted string literal to which the default function is
 * applied.
 *
 * The tokens are scanned by CondToken, which returns:
 *	TOK_AND		for '&&'
 *	TOK_OR		for '||'
 *	TOK_NOT		for '!'
 *	TOK_LPAREN	for '('
 *	TOK_RPAREN	for ')'
 *
 * Other terminal symbols are evaluated using either the default function or
 * the function given in the terminal, they return either TOK_TRUE or
 * TOK_FALSE.
 */
typedef enum Token {
	TOK_FALSE, TOK_TRUE, TOK_AND, TOK_OR, TOK_NOT,
	TOK_LPAREN, TOK_RPAREN, TOK_EOF, TOK_NONE, TOK_ERROR
} Token;

typedef enum CondResult {
	CR_FALSE, CR_TRUE, CR_ERROR
} CondResult;

typedef enum ComparisonOp {
	LT, LE, GT, GE, EQ, NE
} ComparisonOp;

typedef struct CondParser {

	/*
	 * The plain '.if ${VAR}' evaluates to true if the value of the
	 * expression has length > 0.  The other '.if' variants delegate
	 * to evalBare instead.
	 */
	bool plain;

	/* The function to apply on unquoted bare words. */
	bool (*evalBare)(const char *);
	bool negateEvalBare;

	/*
	 * Whether the left-hand side of a comparison may be an unquoted
	 * string.  This is allowed for expressions of the form
	 * ${condition:?:}, see ApplyModifier_IfElse.  Such a condition is
	 * expanded before it is evaluated, due to ease of implementation.
	 * This means that at the point where the condition is evaluated,
	 * make cannot know anymore whether the left-hand side had originally
	 * been a variable expression or a plain word.
	 *
	 * In all other contexts, the left-hand side must either be a
	 * variable expression, a quoted string or a number.
	 */
	bool leftUnquotedOK;

	const char *p;		/* The remaining condition to parse */
	Token curr;		/* Single push-back token used in parsing */

	/*
	 * Whether an error message has already been printed for this
	 * condition. The first available error message is usually the most
	 * specific one, therefore it makes sense to suppress the standard
	 * "Malformed conditional" message.
	 */
	bool printedError;
} CondParser;

static CondResult CondParser_Or(CondParser *par, bool);

static unsigned int cond_depth = 0;	/* current .if nesting level */
static unsigned int cond_min_depth = 0;	/* depth at makefile open */

/* Names for ComparisonOp. */
static const char opname[][3] = { "<", "<=", ">", ">=", "==", "!=" };

static bool
is_token(const char *str, const char *tok, unsigned char len)
{
	return strncmp(str, tok, (size_t)len) == 0 && !ch_isalpha(str[len]);
}

static Token
ToToken(bool cond)
{
	return cond ? TOK_TRUE : TOK_FALSE;
}

static void
CondParser_SkipWhitespace(CondParser *par)
{
	cpp_skip_whitespace(&par->p);
}

/*
 * Parse a single word, taking into account balanced parentheses as well as
 * embedded expressions.  Used for the argument of a built-in function as
 * well as for bare words, which are then passed to the default function.
 *
 * Arguments:
 *	*pp initially points at the '(',
 *	upon successful return it points right after the ')'.
 *
 *	*out_arg receives the argument as string.
 *
 *	func says whether the argument belongs to an actual function, or
 *	NULL when parsing a bare word.
 *
 * Return the length of the argument, or an ambiguous 0 on error.
 */
static size_t
ParseWord(CondParser *par, const char **pp, bool doEval, const char *func,
	     char **out_arg)
{
	const char *p = *pp;
	Buffer argBuf;
	int paren_depth;
	size_t argLen;

	if (func != NULL)
		p++;		/* Skip opening '(' - verified by caller */

	cpp_skip_hspace(&p);

	Buf_InitSize(&argBuf, 16);

	paren_depth = 0;
	for (;;) {
		char ch = *p;
		if (ch == '\0' || ch == ' ' || ch == '\t')
			break;
		if ((ch == '&' || ch == '|') && paren_depth == 0)
			break;
		if (*p == '$') {
			/*
			 * Parse the variable expression and install it as
			 * part of the argument if it's valid. We tell
			 * Var_Parse to complain on an undefined variable,
			 * (XXX: but Var_Parse ignores that request)
			 * so we don't need to do it. Nor do we return an
			 * error, though perhaps we should.
			 */
			VarEvalMode emode = doEval
			    ? VARE_UNDEFERR
			    : VARE_PARSE_ONLY;
			FStr nestedVal;
			(void)Var_Parse(&p, SCOPE_CMDLINE, emode, &nestedVal);
			/* TODO: handle errors */
			Buf_AddStr(&argBuf, nestedVal.str);
			FStr_Done(&nestedVal);
			continue;
		}
		if (ch == '(')
			paren_depth++;
		else if (ch == ')' && --paren_depth < 0)
			break;
		Buf_AddByte(&argBuf, *p);
		p++;
	}

	argLen = argBuf.len;
	*out_arg = Buf_DoneData(&argBuf);

	cpp_skip_hspace(&p);

	if (func != NULL && *p++ != ')') {
		Parse_Error(PARSE_FATAL,
		    "Missing closing parenthesis for %s()", func);
		par->printedError = true;
		return 0;
	}

	*pp = p;
	return argLen;
}

/* Test whether the given variable is defined. */
static bool
FuncDefined(const char *arg)
{
	FStr value = Var_Value(SCOPE_CMDLINE, arg);
	bool result = value.str != NULL;
	FStr_Done(&value);
	return result;
}

/* See if the given target is requested to be made. */
static bool
FuncMake(const char *arg)
{
	StringListNode *ln;

	for (ln = opts.create.first; ln != NULL; ln = ln->next)
		if (Str_Match(ln->datum, arg))
			return true;
	return false;
}

/* See if the given file exists. */
static bool
FuncExists(const char *arg)
{
	bool result;
	char *path;

	path = Dir_FindFile(arg, &dirSearchPath);
	DEBUG2(COND, "exists(%s) result is \"%s\"\n",
	       arg, path != NULL ? path : "");
	result = path != NULL;
	free(path);
	return result;
}

/* See if the given node exists and is an actual target. */
static bool
FuncTarget(const char *arg)
{
	GNode *gn = Targ_FindNode(arg);
	return gn != NULL && GNode_IsTarget(gn);
}

/*
 * See if the given node exists and is an actual target with commands
 * associated with it.
 */
static bool
FuncCommands(const char *arg)
{
	GNode *gn = Targ_FindNode(arg);
	return gn != NULL && GNode_IsTarget(gn) && !Lst_IsEmpty(&gn->commands);
}

/*
 * Convert the given number into a double.
 * We try a base 10 or 16 integer conversion first, if that fails
 * then we try a floating point conversion instead.
 *
 * Results:
 *	Returns true if the conversion succeeded.
 *	Sets 'out_value' to the converted number.
 */
static bool
TryParseNumber(const char *str, double *out_value)
{
	char *end;
	unsigned long ul_val;
	double dbl_val;

	if (str[0] == '\0') {	/* XXX: why is an empty string a number? */
		*out_value = 0.0;
		return true;
	}

	errno = 0;
	ul_val = strtoul(str, &end, str[1] == 'x' ? 16 : 10);
	if (*end == '\0' && errno != ERANGE) {
		*out_value = str[0] == '-' ? -(double)-ul_val : (double)ul_val;
		return true;
	}

	if (*end != '\0' && *end != '.' && *end != 'e' && *end != 'E')
		return false;	/* skip the expensive strtod call */
	dbl_val = strtod(str, &end);
	if (*end != '\0')
		return false;

	*out_value = dbl_val;
	return true;
}

static bool
is_separator(char ch)
{
	return ch == '\0' || ch_isspace(ch) || ch == '!' || ch == '=' ||
	       ch == '>' || ch == '<' || ch == ')' /* but not '(' */;
}

/*
 * In a quoted or unquoted string literal or a number, parse a variable
 * expression.
 *
 * Example: .if x${CENTER}y == "${PREFIX}${SUFFIX}" || 0x${HEX}
 */
static bool
CondParser_StringExpr(CondParser *par, const char *start,
		      bool doEval, bool quoted,
		      Buffer *buf, FStr *inout_str)
{
	VarEvalMode emode;
	const char *nested_p;
	bool atStart;
	VarParseResult parseResult;

	emode = doEval && quoted ? VARE_WANTRES
	    : doEval ? VARE_UNDEFERR
	    : VARE_PARSE_ONLY;

	nested_p = par->p;
	atStart = nested_p == start;
	parseResult = Var_Parse(&nested_p, SCOPE_CMDLINE, emode, inout_str);
	/* TODO: handle errors */
	if (inout_str->str == var_Error) {
		if (parseResult == VPR_ERR) {
			/*
			 * FIXME: Even if an error occurs, there is no
			 *  guarantee that it is reported.
			 *
			 * See cond-token-plain.mk $$$$$$$$.
			 */
			par->printedError = true;
		}
		/*
		 * XXX: Can there be any situation in which a returned
		 * var_Error needs to be freed?
		 */
		FStr_Done(inout_str);
		/*
		 * Even if !doEval, we still report syntax errors, which is
		 * what getting var_Error back with !doEval means.
		 */
		*inout_str = FStr_InitRefer(NULL);
		return false;
	}
	par->p = nested_p;

	/*
	 * If the '$' started the string literal (which means no quotes), and
	 * the variable expression is followed by a space, looks like a
	 * comparison operator or is the end of the expression, we are done.
	 */
	if (atStart && is_separator(par->p[0]))
		return false;

	Buf_AddStr(buf, inout_str->str);
	FStr_Done(inout_str);
	*inout_str = FStr_InitRefer(NULL); /* not finished yet */
	return true;
}

/*
 * Parse a string from a variable expression or an optionally quoted
 * string.  This is called for the left-hand and right-hand sides of
 * comparisons.
 *
 * Results:
 *	Returns the string, absent any quotes, or NULL on error.
 *	Sets out_quoted if the leaf was a quoted string literal.
 */
static void
CondParser_Leaf(CondParser *par, bool doEval, bool unquotedOK,
		  FStr *out_str, bool *out_quoted)
{
	Buffer buf;
	FStr str;
	bool quoted;
	const char *start;

	Buf_Init(&buf);
	str = FStr_InitRefer(NULL);
	*out_quoted = quoted = par->p[0] == '"';
	start = par->p;
	if (quoted)
		par->p++;

	while (par->p[0] != '\0' && str.str == NULL) {
		switch (par->p[0]) {
		case '\\':
			par->p++;
			if (par->p[0] != '\0') {
				Buf_AddByte(&buf, par->p[0]);
				par->p++;
			}
			continue;
		case '"':
			par->p++;
			if (quoted)
				goto got_str;	/* skip the closing quote */
			Buf_AddByte(&buf, '"');
			continue;
		case ')':	/* see is_separator */
		case '!':
		case '=':
		case '>':
		case '<':
		case ' ':
		case '\t':
			if (!quoted)
				goto got_str;
			Buf_AddByte(&buf, par->p[0]);
			par->p++;
			continue;
		case '$':
			if (!CondParser_StringExpr(par,
			    start, doEval, quoted, &buf, &str))
				goto cleanup;
			continue;
		default:
			if (!unquotedOK && !quoted && *start != '$' &&
			    !ch_isdigit(*start)) {
				/*
				 * The left-hand side must be quoted,
				 * a variable expression or a number.
				 */
				str = FStr_InitRefer(NULL);
				goto cleanup;
			}
			Buf_AddByte(&buf, par->p[0]);
			par->p++;
			continue;
		}
	}
got_str:
	str = FStr_InitOwn(buf.data);
	buf.data = NULL;
cleanup:
	Buf_Done(&buf);
	*out_str = str;
}

static bool
EvalBare(const CondParser *par, const char *arg)
{
	bool res = par->evalBare(arg);
	return par->negateEvalBare ? !res : res;
}

/*
 * Evaluate a "comparison without operator", such as in ".if ${VAR}" or
 * ".if 0".
 */
static bool
EvalNotEmpty(CondParser *par, const char *value, bool quoted)
{
	double num;

	/* For .ifxxx "...", check for non-empty string. */
	if (quoted)
		return value[0] != '\0';

	/* For .ifxxx <number>, compare against zero */
	if (TryParseNumber(value, &num))
		return num != 0.0;

	/* For .if ${...}, check for non-empty string.  This is different from
	 * the evaluation function from that .if variant, which would test
	 * whether a variable of the given name were defined. */
	/*
	 * XXX: Whitespace should count as empty, just as in
	 * CondParser_FuncCallEmpty.
	 */
	if (par->plain)
		return value[0] != '\0';

	return EvalBare(par, value);
}

/* Evaluate a numerical comparison, such as in ".if ${VAR} >= 9". */
static bool
EvalCompareNum(double lhs, ComparisonOp op, double rhs)
{
	DEBUG3(COND, "lhs = %f, rhs = %f, op = %.2s\n", lhs, rhs, opname[op]);

	switch (op) {
	case LT:
		return lhs < rhs;
	case LE:
		return lhs <= rhs;
	case GT:
		return lhs > rhs;
	case GE:
		return lhs >= rhs;
	case NE:
		return lhs != rhs;
	default:
		return lhs == rhs;
	}
}

static Token
EvalCompareStr(CondParser *par, const char *lhs,
	       ComparisonOp op, const char *rhs)
{
	if (op != EQ && op != NE) {
		Parse_Error(PARSE_FATAL,
		    "String comparison operator must be either == or !=");
		par->printedError = true;
		return TOK_ERROR;
	}

	DEBUG3(COND, "lhs = \"%s\", rhs = \"%s\", op = %.2s\n",
	    lhs, rhs, opname[op]);
	return ToToken((op == EQ) == (strcmp(lhs, rhs) == 0));
}

/* Evaluate a comparison, such as "${VAR} == 12345". */
static Token
EvalCompare(CondParser *par, const char *lhs, bool lhsQuoted,
	    ComparisonOp op, const char *rhs, bool rhsQuoted)
{
	double left, right;

	if (!rhsQuoted && !lhsQuoted)
		if (TryParseNumber(lhs, &left) && TryParseNumber(rhs, &right))
			return ToToken(EvalCompareNum(left, op, right));

	return EvalCompareStr(par, lhs, op, rhs);
}

static bool
CondParser_ComparisonOp(CondParser *par, ComparisonOp *out_op)
{
	const char *p = par->p;

	if (p[0] == '<' && p[1] == '=') {
		*out_op = LE;
		goto length_2;
	} else if (p[0] == '<') {
		*out_op = LT;
		goto length_1;
	} else if (p[0] == '>' && p[1] == '=') {
		*out_op = GE;
		goto length_2;
	} else if (p[0] == '>') {
		*out_op = GT;
		goto length_1;
	} else if (p[0] == '=' && p[1] == '=') {
		*out_op = EQ;
		goto length_2;
	} else if (p[0] == '!' && p[1] == '=') {
		*out_op = NE;
		goto length_2;
	}
	return false;

length_2:
	par->p = p + 2;
	return true;
length_1:
	par->p = p + 1;
	return true;
}

/*
 * Parse a comparison condition such as:
 *
 *	0
 *	${VAR:Mpattern}
 *	${VAR} == value
 *	${VAR:U0} < 12345
 */
static Token
CondParser_Comparison(CondParser *par, bool doEval)
{
	Token t = TOK_ERROR;
	FStr lhs, rhs;
	ComparisonOp op;
	bool lhsQuoted, rhsQuoted;

	CondParser_Leaf(par, doEval, par->leftUnquotedOK, &lhs, &lhsQuoted);
	if (lhs.str == NULL)
		goto done_lhs;

	CondParser_SkipWhitespace(par);

	if (!CondParser_ComparisonOp(par, &op)) {
		/* Unknown operator, compare against an empty string or 0. */
		t = ToToken(doEval && EvalNotEmpty(par, lhs.str, lhsQuoted));
		goto done_lhs;
	}

	CondParser_SkipWhitespace(par);

	if (par->p[0] == '\0') {
		Parse_Error(PARSE_FATAL,
		    "Missing right-hand side of operator '%s'", opname[op]);
		par->printedError = true;
		goto done_lhs;
	}

	CondParser_Leaf(par, doEval, true, &rhs, &rhsQuoted);
	if (rhs.str == NULL)
		goto done_rhs;

	if (!doEval) {
		t = TOK_FALSE;
		goto done_rhs;
	}

	t = EvalCompare(par, lhs.str, lhsQuoted, op, rhs.str, rhsQuoted);

done_rhs:
	FStr_Done(&rhs);
done_lhs:
	FStr_Done(&lhs);
	return t;
}

/*
 * The argument to empty() is a variable name, optionally followed by
 * variable modifiers.
 */
static bool
CondParser_FuncCallEmpty(CondParser *par, bool doEval, Token *out_token)
{
	const char *cp = par->p;
	Token tok;
	FStr val;

	if (!is_token(cp, "empty", 5))
		return false;
	cp += 5;

	cpp_skip_whitespace(&cp);
	if (*cp != '(')
		return false;

	cp--;			/* Make cp[1] point to the '('. */
	(void)Var_Parse(&cp, SCOPE_CMDLINE,
	    doEval ? VARE_WANTRES : VARE_PARSE_ONLY, &val);
	/* TODO: handle errors */

	if (val.str == var_Error)
		tok = TOK_ERROR;
	else {
		cpp_skip_whitespace(&val.str);
		tok = val.str[0] != '\0' && doEval ? TOK_FALSE : TOK_TRUE;
	}

	FStr_Done(&val);
	*out_token = tok;
	par->p = cp;
	return true;
}

/* Parse a function call expression, such as 'defined(${file})'. */
static bool
CondParser_FuncCall(CondParser *par, bool doEval, Token *out_token)
{
	static const struct fn_def {
		const char fn_name[9];
		unsigned char fn_name_len;
		bool (*fn_eval)(const char *);
	} fns[] = {
		{ "defined",  7, FuncDefined },
		{ "make",     4, FuncMake },
		{ "exists",   6, FuncExists },
		{ "target",   6, FuncTarget },
		{ "commands", 8, FuncCommands }
	};
	const struct fn_def *fn;
	char *arg = NULL;
	size_t arglen;
	const char *cp = par->p;
	const struct fn_def *last_fn = fns + sizeof fns / sizeof fns[0] - 1;

	for (fn = fns; !is_token(cp, fn->fn_name, fn->fn_name_len); fn++)
		if (fn == last_fn)
			return false;

	cp += fn->fn_name_len;
	cpp_skip_whitespace(&cp);
	if (*cp != '(')
		return false;

	arglen = ParseWord(par, &cp, doEval, fn->fn_name, &arg);
	*out_token = ToToken(arglen != 0 && (!doEval || fn->fn_eval(arg)));

	free(arg);
	par->p = cp;
	return true;
}

/*
 * Parse a comparison that neither starts with '"' nor '$', such as the
 * unusual 'bare == right' or '3 == ${VAR}', or a simple leaf without
 * operator, which is a number, a variable expression or a string literal.
 *
 * TODO: Can this be merged into CondParser_Comparison?
 */
static Token
CondParser_ComparisonOrLeaf(CondParser *par, bool doEval)
{
	Token t;
	char *arg = NULL;
	const char *cp;
	const char *cp1;

	/* Push anything numeric through the compare expression */
	cp = par->p;
	if (ch_isdigit(cp[0]) || cp[0] == '-' || cp[0] == '+')
		return CondParser_Comparison(par, doEval);

	/*
	 * Most likely we have a naked token to apply the default function to.
	 * However ".if a == b" gets here when the "a" is unquoted and doesn't
	 * start with a '$'. This surprises people.
	 * If what follows the function argument is a '=' or '!' then the
	 * syntax would be invalid if we did "defined(a)" - so instead treat
	 * as an expression.
	 */
	/*
	 * XXX: Is it possible to have a variable expression evaluated twice
	 *  at this point?
	 */
	(void)ParseWord(par, &cp, doEval, NULL, &arg);
	cp1 = cp;
	cpp_skip_whitespace(&cp1);
	if (*cp1 == '=' || *cp1 == '!' || *cp1 == '<' || *cp1 == '>')
		return CondParser_Comparison(par, doEval);
	par->p = cp;

	/*
	 * Evaluate the argument using the default function.
	 * This path always treats .if as .ifdef. To get here, the character
	 * after .if must have been taken literally, so the argument cannot
	 * be empty - even if it contained a variable expansion.
	 */
	t = ToToken(!doEval || EvalBare(par, arg));
	free(arg);
	return t;
}

/* Return the next token or comparison result from the parser. */
static Token
CondParser_Token(CondParser *par, bool doEval)
{
	Token t;

	t = par->curr;
	if (t != TOK_NONE) {
		par->curr = TOK_NONE;
		return t;
	}

	cpp_skip_hspace(&par->p);

	switch (par->p[0]) {

	case '(':
		par->p++;
		return TOK_LPAREN;

	case ')':
		par->p++;
		return TOK_RPAREN;

	case '|':
		par->p++;
		if (par->p[0] == '|')
			par->p++;
		else if (opts.strict) {
			Parse_Error(PARSE_FATAL, "Unknown operator '|'");
			par->printedError = true;
			return TOK_ERROR;
		}
		return TOK_OR;

	case '&':
		par->p++;
		if (par->p[0] == '&')
			par->p++;
		else if (opts.strict) {
			Parse_Error(PARSE_FATAL, "Unknown operator '&'");
			par->printedError = true;
			return TOK_ERROR;
		}
		return TOK_AND;

	case '!':
		par->p++;
		return TOK_NOT;

	case '#':		/* XXX: see unit-tests/cond-token-plain.mk */
	case '\n':		/* XXX: why should this end the condition? */
		/* Probably obsolete now, from 1993-03-21. */
	case '\0':
		return TOK_EOF;

	case '"':
	case '$':
		return CondParser_Comparison(par, doEval);

	default:
		if (CondParser_FuncCallEmpty(par, doEval, &t))
			return t;
		if (CondParser_FuncCall(par, doEval, &t))
			return t;
		return CondParser_ComparisonOrLeaf(par, doEval);
	}
}

/* Skip the next token if it equals t. */
static bool
CondParser_Skip(CondParser *par, Token t)
{
	Token actual;

	actual = CondParser_Token(par, false);
	if (actual == t)
		return true;

	assert(par->curr == TOK_NONE);
	assert(actual != TOK_NONE);
	par->curr = actual;
	return false;
}

/*
 * Term -> '(' Or ')'
 * Term -> '!' Term
 * Term -> Leaf Operator Leaf
 * Term -> Leaf
 */
static CondResult
CondParser_Term(CondParser *par, bool doEval)
{
	CondResult res;
	Token t;

	t = CondParser_Token(par, doEval);
	if (t == TOK_TRUE)
		return CR_TRUE;
	if (t == TOK_FALSE)
		return CR_FALSE;

	if (t == TOK_LPAREN) {
		res = CondParser_Or(par, doEval);
		if (res == CR_ERROR)
			return CR_ERROR;
		if (CondParser_Token(par, doEval) != TOK_RPAREN)
			return CR_ERROR;
		return res;
	}

	if (t == TOK_NOT) {
		res = CondParser_Term(par, doEval);
		if (res == CR_TRUE)
			res = CR_FALSE;
		else if (res == CR_FALSE)
			res = CR_TRUE;
		return res;
	}

	return CR_ERROR;
}

/*
 * And -> Term ('&&' Term)*
 */
static CondResult
CondParser_And(CondParser *par, bool doEval)
{
	CondResult res, rhs;

	res = CR_TRUE;
	do {
		if ((rhs = CondParser_Term(par, doEval)) == CR_ERROR)
			return CR_ERROR;
		if (rhs == CR_FALSE) {
			res = CR_FALSE;
			doEval = false;
		}
	} while (CondParser_Skip(par, TOK_AND));

	return res;
}

/*
 * Or -> And ('||' And)*
 */
static CondResult
CondParser_Or(CondParser *par, bool doEval)
{
	CondResult res, rhs;

	res = CR_FALSE;
	do {
		if ((rhs = CondParser_And(par, doEval)) == CR_ERROR)
			return CR_ERROR;
		if (rhs == CR_TRUE) {
			res = CR_TRUE;
			doEval = false;
		}
	} while (CondParser_Skip(par, TOK_OR));

	return res;
}

static CondEvalResult
CondParser_Eval(CondParser *par, bool *out_value)
{
	CondResult res;

	DEBUG1(COND, "CondParser_Eval: %s\n", par->p);

	res = CondParser_Or(par, true);
	if (res == CR_ERROR)
		return COND_INVALID;

	if (CondParser_Token(par, false) != TOK_EOF)
		return COND_INVALID;

	*out_value = res == CR_TRUE;
	return COND_PARSE;
}

/*
 * Evaluate the condition, including any side effects from the variable
 * expressions in the condition. The condition consists of &&, ||, !,
 * function(arg), comparisons and parenthetical groupings thereof.
 *
 * Results:
 *	COND_PARSE	if the condition was valid grammatically
 *	COND_INVALID	if not a valid conditional.
 *
 *	*out_value	is set to the boolean value of the condition
 */
static CondEvalResult
CondEvalExpression(const char *cond, bool *out_value, bool plain,
		   bool (*evalBare)(const char *), bool negate,
		   bool eprint, bool leftUnquotedOK)
{
	CondParser par;
	CondEvalResult rval;

	cpp_skip_hspace(&cond);

	par.plain = plain;
	par.evalBare = evalBare;
	par.negateEvalBare = negate;
	par.leftUnquotedOK = leftUnquotedOK;
	par.p = cond;
	par.curr = TOK_NONE;
	par.printedError = false;

	rval = CondParser_Eval(&par, out_value);

	if (rval == COND_INVALID && eprint && !par.printedError)
		Parse_Error(PARSE_FATAL, "Malformed conditional (%s)", cond);

	return rval;
}

/*
 * Evaluate a condition in a :? modifier, such as
 * ${"${VAR}" == value:?yes:no}.
 */
CondEvalResult
Cond_EvalCondition(const char *cond, bool *out_value)
{
	return CondEvalExpression(cond, out_value, true,
	    FuncDefined, false, false, true);
}

static bool
IsEndif(const char *p)
{
	return p[0] == 'e' && p[1] == 'n' && p[2] == 'd' &&
	       p[3] == 'i' && p[4] == 'f' && !ch_isalpha(p[5]);
}

static bool
DetermineKindOfConditional(const char **pp, bool *out_plain,
			   bool (**out_evalBare)(const char *),
			   bool *out_negate)
{
	const char *p = *pp;

	p += 2;
	*out_plain = false;
	*out_evalBare = FuncDefined;
	*out_negate = false;
	if (*p == 'n') {
		p++;
		*out_negate = true;
	}
	if (is_token(p, "def", 3)) {		/* .ifdef and .ifndef */
		p += 3;
	} else if (is_token(p, "make", 4)) {	/* .ifmake and .ifnmake */
		p += 4;
		*out_evalBare = FuncMake;
	} else if (is_token(p, "", 0) && !*out_negate) { /* plain .if */
		*out_plain = true;
	} else {
		/*
		 * TODO: Add error message about unknown directive,
		 * since there is no other known directive that starts
		 * with 'el' or 'if'.
		 *
		 * Example: .elifx 123
		 */
		return false;
	}

	*pp = p;
	return true;
}

/*
 * Evaluate the conditional directive in the line, which is one of:
 *
 *	.if <cond>
 *	.ifmake <cond>
 *	.ifnmake <cond>
 *	.ifdef <cond>
 *	.ifndef <cond>
 *	.elif <cond>
 *	.elifmake <cond>
 *	.elifnmake <cond>
 *	.elifdef <cond>
 *	.elifndef <cond>
 *	.else
 *	.endif
 *
 * In these directives, <cond> consists of &&, ||, !, function(arg),
 * comparisons, expressions, bare words, numbers and strings, and
 * parenthetical groupings thereof.
 *
 * Results:
 *	COND_PARSE	to continue parsing the lines that follow the
 *			conditional (when <cond> evaluates to true)
 *	COND_SKIP	to skip the lines after the conditional
 *			(when <cond> evaluates to false, or when a previous
 *			branch has already been taken)
 *	COND_INVALID	if the conditional was not valid, either because of
 *			a syntax error or because some variable was undefined
 *			or because the condition could not be evaluated
 */
CondEvalResult
Cond_EvalLine(const char *line)
{
	typedef enum IfState {

		/* None of the previous <cond> evaluated to true. */
		IFS_INITIAL	= 0,

		/* The previous <cond> evaluated to true.
		 * The lines following this condition are interpreted. */
		IFS_ACTIVE	= 1 << 0,

		/* The previous directive was an '.else'. */
		IFS_SEEN_ELSE	= 1 << 1,

		/* One of the previous <cond> evaluated to true. */
		IFS_WAS_ACTIVE	= 1 << 2

	} IfState;

	static enum IfState *cond_states = NULL;
	static unsigned int cond_states_cap = 128;

	bool plain;
	bool (*evalBare)(const char *);
	bool negate;
	bool isElif;
	bool value;
	IfState state;
	const char *p = line;

	if (cond_states == NULL) {
		cond_states = bmake_malloc(
		    cond_states_cap * sizeof *cond_states);
		cond_states[0] = IFS_ACTIVE;
	}

	p++;			/* skip the leading '.' */
	cpp_skip_hspace(&p);

	if (IsEndif(p)) {	/* It is an '.endif'. */
		if (p[5] != '\0') {
			Parse_Error(PARSE_FATAL,
			    "The .endif directive does not take arguments");
		}

		if (cond_depth == cond_min_depth) {
			Parse_Error(PARSE_FATAL, "if-less endif");
			return COND_PARSE;
		}

		/* Return state for previous conditional */
		cond_depth--;
		return cond_states[cond_depth] & IFS_ACTIVE
		    ? COND_PARSE : COND_SKIP;
	}

	/* Parse the name of the directive, such as 'if', 'elif', 'endif'. */
	if (p[0] == 'e') {
		if (p[1] != 'l') {
			/*
			 * Unknown directive.  It might still be a
			 * transformation rule like '.err.txt',
			 * therefore no error message here.
			 */
			return COND_INVALID;
		}

		/* Quite likely this is 'else' or 'elif' */
		p += 2;
		if (is_token(p, "se", 2)) {	/* It is an 'else'. */

			if (p[2] != '\0')
				Parse_Error(PARSE_FATAL,
				    "The .else directive "
				    "does not take arguments");

			if (cond_depth == cond_min_depth) {
				Parse_Error(PARSE_FATAL, "if-less else");
				return COND_PARSE;
			}

			state = cond_states[cond_depth];
			if (state == IFS_INITIAL) {
				state = IFS_ACTIVE | IFS_SEEN_ELSE;
			} else {
				if (state & IFS_SEEN_ELSE)
					Parse_Error(PARSE_WARNING,
						    "extra else");
				state = IFS_WAS_ACTIVE | IFS_SEEN_ELSE;
			}
			cond_states[cond_depth] = state;

			return state & IFS_ACTIVE ? COND_PARSE : COND_SKIP;
		}
		/* Assume for now it is an elif */
		isElif = true;
	} else
		isElif = false;

	if (p[0] != 'i' || p[1] != 'f') {
		/*
		 * Unknown directive.  It might still be a transformation rule
		 * like '.elisp.scm', therefore no error message here.
		 */
		return COND_INVALID;	/* Not an ifxxx or elifxxx line */
	}

	if (!DetermineKindOfConditional(&p, &plain, &evalBare, &negate))
		return COND_INVALID;

	if (isElif) {
		if (cond_depth == cond_min_depth) {
			Parse_Error(PARSE_FATAL, "if-less elif");
			return COND_PARSE;
		}
		state = cond_states[cond_depth];
		if (state & IFS_SEEN_ELSE) {
			Parse_Error(PARSE_WARNING, "extra elif");
			cond_states[cond_depth] =
			    IFS_WAS_ACTIVE | IFS_SEEN_ELSE;
			return COND_SKIP;
		}
		if (state != IFS_INITIAL) {
			cond_states[cond_depth] = IFS_WAS_ACTIVE;
			return COND_SKIP;
		}
	} else {
		/* Normal .if */
		if (cond_depth + 1 >= cond_states_cap) {
			/*
			 * This is rare, but not impossible.
			 * In meta mode, dirdeps.mk (only runs at level 0)
			 * can need more than the default.
			 */
			cond_states_cap += 32;
			cond_states = bmake_realloc(cond_states,
						    cond_states_cap *
						    sizeof *cond_states);
		}
		state = cond_states[cond_depth];
		cond_depth++;
		if (!(state & IFS_ACTIVE)) {
			/*
			 * If we aren't parsing the data,
			 * treat as always false.
			 */
			cond_states[cond_depth] = IFS_WAS_ACTIVE;
			return COND_SKIP;
		}
	}

	/* And evaluate the conditional expression */
	if (CondEvalExpression(p, &value, plain, evalBare, negate,
	    true, false) == COND_INVALID) {
		/* Syntax error in conditional, error message already output. */
		/* Skip everything to matching .endif */
		/* XXX: An extra '.else' is not detected in this case. */
		cond_states[cond_depth] = IFS_WAS_ACTIVE;
		return COND_SKIP;
	}

	if (!value) {
		cond_states[cond_depth] = IFS_INITIAL;
		return COND_SKIP;
	}
	cond_states[cond_depth] = IFS_ACTIVE;
	return COND_PARSE;
}

void
Cond_restore_depth(unsigned int saved_depth)
{
	unsigned int open_conds = cond_depth - cond_min_depth;

	if (open_conds != 0 || saved_depth > cond_depth) {
		Parse_Error(PARSE_FATAL, "%u open conditional%s",
			    open_conds, open_conds == 1 ? "" : "s");
		cond_depth = cond_min_depth;
	}

	cond_min_depth = saved_depth;
}

unsigned int
Cond_save_depth(void)
{
	unsigned int depth = cond_min_depth;

	cond_min_depth = cond_depth;
	return depth;
}
