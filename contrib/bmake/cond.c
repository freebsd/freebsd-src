/*	$NetBSD: cond.c,v 1.372 2025/04/10 21:41:35 rillig Exp $	*/

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
 *	Cond_EndFile	At the end of reading a makefile, ensure that the
 *			conditional directives are well-balanced.
 */

#include <errno.h>

#include "make.h"
#include "dir.h"

/*	"@(#)cond.c	8.2 (Berkeley) 1/2/94"	*/
MAKE_RCSID("$NetBSD: cond.c,v 1.372 2025/04/10 21:41:35 rillig Exp $");

/*
 * Conditional expressions conform to this grammar:
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
 *	Leaf -> BareWord
 *	Operator -> '==' | '!=' | '>' | '<' | '>=' | '<='
 *
 * BareWord is an unquoted string literal, its evaluation depends on the kind
 * of '.if' directive.
 *
 * The tokens are scanned by CondParser_Token, which returns:
 *	TOK_AND		for '&&'
 *	TOK_OR		for '||'
 *	TOK_NOT		for '!'
 *	TOK_LPAREN	for '('
 *	TOK_RPAREN	for ')'
 *
 * Other terminal symbols are evaluated using either the default function or
 * the function given in the terminal, they return either TOK_TRUE, TOK_FALSE
 * or TOK_ERROR.
 */
typedef enum Token {
	TOK_FALSE, TOK_TRUE, TOK_AND, TOK_OR, TOK_NOT,
	TOK_LPAREN, TOK_RPAREN, TOK_EOF, TOK_NONE, TOK_ERROR
} Token;

typedef enum ComparisonOp {
	LT, LE, GT, GE, EQ, NE
} ComparisonOp;

typedef struct CondParser {

	/*
	 * The plain '.if ${VAR}' evaluates to true if the value of the
	 * expression has length > 0 and is not numerically zero.  The other
	 * '.if' variants delegate to evalBare instead, for example '.ifdef
	 * ${VAR}' is equivalent to '.if defined(${VAR})', checking whether
	 * the variable named by the expression '${VAR}' is defined.
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
	 * been an expression or a plain word.
	 *
	 * In conditional directives like '.if', the left-hand side must
	 * either be an expression, a quoted string or a number.
	 */
	bool leftUnquotedOK;

	const char *p;		/* The remaining condition to parse */
	Token curr;		/* Single push-back token used in parsing */
} CondParser;

static CondResult CondParser_Or(CondParser *, bool);

unsigned int cond_depth = 0;	/* current .if nesting level */

/* Names for ComparisonOp. */
static const char opname[][3] = { "<", "<=", ">", ">=", "==", "!=" };

MAKE_INLINE bool
skip_string(const char **pp, const char *str)
{
	size_t len = strlen(str);
	bool ok = strncmp(*pp, str, len) == 0;
	if (ok)
		*pp += len;
	return ok;
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
 */
static char *
ParseWord(const char **pp, bool doEval)
{
	const char *p = *pp;
	Buffer word;
	int depth;

	Buf_Init(&word);

	depth = 0;
	for (;;) {
		char ch = *p;
		if (ch == '\0' || ch == ' ' || ch == '\t')
			break;
		if ((ch == '&' || ch == '|') && depth == 0)
			break;
		if (ch == '$') {
			VarEvalMode emode = doEval ? VARE_EVAL : VARE_PARSE;
			FStr nestedVal = Var_Parse(&p, SCOPE_CMDLINE, emode);
			/* TODO: handle errors */
			Buf_AddStr(&word, nestedVal.str);
			FStr_Done(&nestedVal);
			continue;
		}
		if (ch == '(')
			depth++;
		else if (ch == ')' && --depth < 0)
			break;
		Buf_AddByte(&word, ch);
		p++;
	}

	*pp = p;

	return Buf_DoneData(&word);
}

/* Parse the function argument, including the surrounding parentheses. */
static char *
ParseFuncArg(const char **pp, bool doEval, const char *func)
{
	const char *p = *pp, *argStart, *argEnd;
	char *res;

	p++;			/* skip the '(' */
	cpp_skip_hspace(&p);
	argStart = p;
	res = ParseWord(&p, doEval);
	argEnd = p;
	cpp_skip_hspace(&p);

	if (*p++ != ')') {
		int len = 0;
		while (ch_isalpha(func[len]))
			len++;

		Parse_Error(PARSE_FATAL,
		    "Missing ')' after argument '%.*s' for '%.*s'",
		    (int)(argEnd - argStart), argStart, len, func);
		free(res);
		return NULL;
	}

	*pp = p;
	return res;
}

/* See if the given variable is defined. */
static bool
FuncDefined(const char *var)
{
	return Var_Exists(SCOPE_CMDLINE, var);
}

/* See if a target matching targetPattern is requested to be made. */
static bool
FuncMake(const char *targetPattern)
{
	StringListNode *ln;
	bool warned = false;

	for (ln = opts.create.first; ln != NULL; ln = ln->next) {
		StrMatchResult res = Str_Match(ln->datum, targetPattern);
		if (res.error != NULL && !warned) {
			warned = true;
			Parse_Error(PARSE_WARNING,
			    "%s in pattern argument '%s' to function 'make'",
			    res.error, targetPattern);
		}
		if (res.matched)
			return true;
	}
	return false;
}

/* See if the given file exists. */
static bool
FuncExists(const char *file)
{
	bool result;
	char *path;

	path = Dir_FindFile(file, &dirSearchPath);
	DEBUG2(COND, "exists(%s) result is \"%s\"\n",
	    file, path != NULL ? path : "");
	result = path != NULL;
	free(path);
	return result;
}

/* See if the given node exists and is an actual target. */
static bool
FuncTarget(const char *node)
{
	GNode *gn = Targ_FindNode(node);
	return gn != NULL && GNode_IsTarget(gn);
}

/*
 * See if the given node exists and is an actual target with commands
 * associated with it.
 */
static bool
FuncCommands(const char *node)
{
	GNode *gn = Targ_FindNode(node);
	return gn != NULL && GNode_IsTarget(gn) &&
	       !Lst_IsEmpty(&gn->commands);
}

/*
 * Convert the string to a floating point number.  Accepted formats are
 * base-10 integer, base-16 integer and finite floating point numbers.
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
 * In a quoted or unquoted string literal or a number, parse an
 * expression and add its value to the buffer.
 *
 * Return whether to continue parsing the leaf.
 *
 * Example: .if x${CENTER}y == "${PREFIX}${SUFFIX}" || 0x${HEX}
 */
static bool
CondParser_StringExpr(CondParser *par, const char *start,
		      bool doEval, bool quoted,
		      Buffer *buf, FStr *inout_str)
{
	VarEvalMode emode;
	const char *p;
	bool atStart;		/* true means an expression outside quotes */

	emode = doEval && quoted ? VARE_EVAL
	    : doEval ? VARE_EVAL_DEFINED_LOUD
	    : VARE_PARSE;

	p = par->p;
	atStart = p == start;
	*inout_str = Var_Parse(&p, SCOPE_CMDLINE, emode);
	/* TODO: handle errors */
	if (inout_str->str == var_Error) {
		FStr_Done(inout_str);
		*inout_str = FStr_InitRefer(NULL);
		return false;
	}
	par->p = p;

	if (atStart && is_separator(par->p[0]))
		return false;

	Buf_AddStr(buf, inout_str->str);
	FStr_Done(inout_str);
	*inout_str = FStr_InitRefer(NULL);	/* not finished yet */
	return true;
}

/*
 * Parse a string from an expression or an optionally quoted string,
 * on the left-hand and right-hand sides of comparisons.
 *
 * Return the string without any enclosing quotes, or NULL on error.
 * Sets out_quoted if the leaf was a quoted string literal.
 */
static FStr
CondParser_Leaf(CondParser *par, bool doEval, bool unquotedOK,
		bool *out_quoted)
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
				goto return_buf;	/* skip the closing quote */
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
				goto return_buf;
			Buf_AddByte(&buf, par->p[0]);
			par->p++;
			continue;
		case '$':
			if (!CondParser_StringExpr(par,
			    start, doEval, quoted, &buf, &str))
				goto return_str;
			continue;
		default:
			if (!unquotedOK && !quoted && *start != '$' &&
			    !ch_isdigit(*start)) {
				str = FStr_InitRefer(NULL);
				goto return_str;
			}
			Buf_AddByte(&buf, par->p[0]);
			par->p++;
			continue;
		}
	}
return_buf:
	str = FStr_InitOwn(buf.data);
	buf.data = NULL;
return_str:
	Buf_Done(&buf);
	return str;
}

/*
 * Evaluate a "comparison without operator", such as in ".if ${VAR}" or
 * ".if 0".
 */
static bool
EvalTruthy(CondParser *par, const char *value, bool quoted)
{
	double num;

	if (quoted)
		return value[0] != '\0';
	if (TryParseNumber(value, &num))
		return num != 0.0;
	if (par->plain)
		return value[0] != '\0';
	return par->evalBare(value) != par->negateEvalBare;
}

/* Evaluate a numerical comparison, such as in ".if ${VAR} >= 9". */
static bool
EvalCompareNum(double lhs, ComparisonOp op, double rhs)
{
	DEBUG3(COND, "Comparing %f %s %f\n", lhs, opname[op], rhs);

	switch (op) {
	case LT:
		return lhs < rhs;
	case LE:
		return lhs <= rhs;
	case GT:
		return lhs > rhs;
	case GE:
		return lhs >= rhs;
	case EQ:
		return lhs == rhs;
	default:
		return lhs != rhs;
	}
}

static Token
EvalCompareStr(const char *lhs, ComparisonOp op, const char *rhs)
{
	if (op != EQ && op != NE) {
		Parse_Error(PARSE_FATAL,
		    "Comparison with '%s' requires both operands "
		    "'%s' and '%s' to be numeric",
		    opname[op], lhs, rhs);
		return TOK_ERROR;
	}

	DEBUG3(COND, "Comparing \"%s\" %s \"%s\"\n", lhs, opname[op], rhs);
	return ToToken((op == EQ) == (strcmp(lhs, rhs) == 0));
}

/* Evaluate a comparison, such as "${VAR} == 12345". */
static Token
EvalCompare(const char *lhs, bool lhsQuoted,
	    ComparisonOp op, const char *rhs, bool rhsQuoted)
{
	double left, right;

	if (!rhsQuoted && !lhsQuoted)
		if (TryParseNumber(lhs, &left) && TryParseNumber(rhs, &right))
			return ToToken(EvalCompareNum(left, op, right));

	return EvalCompareStr(lhs, op, rhs);
}

static bool
CondParser_ComparisonOp(CondParser *par, ComparisonOp *out_op)
{
	const char *p = par->p;

	if (p[0] == '<' && p[1] == '=')
		return par->p += 2, *out_op = LE, true;
	if (p[0] == '<')
		return par->p += 1, *out_op = LT, true;
	if (p[0] == '>' && p[1] == '=')
		return par->p += 2, *out_op = GE, true;
	if (p[0] == '>')
		return par->p += 1, *out_op = GT, true;
	if (p[0] == '=' && p[1] == '=')
		return par->p += 2, *out_op = EQ, true;
	if (p[0] == '!' && p[1] == '=')
		return par->p += 2, *out_op = NE, true;
	return false;
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

	lhs = CondParser_Leaf(par, doEval, par->leftUnquotedOK, &lhsQuoted);
	if (lhs.str == NULL)
		goto done_lhs;

	CondParser_SkipWhitespace(par);

	if (!CondParser_ComparisonOp(par, &op)) {
		t = ToToken(doEval && EvalTruthy(par, lhs.str, lhsQuoted));
		goto done_lhs;
	}

	CondParser_SkipWhitespace(par);

	if (par->p[0] == '\0') {
		Parse_Error(PARSE_FATAL,
		    "Missing right-hand side of operator '%s'", opname[op]);
		goto done_lhs;
	}

	rhs = CondParser_Leaf(par, doEval, true, &rhsQuoted);
	t = rhs.str == NULL ? TOK_ERROR
	    : !doEval ? TOK_FALSE
	    : EvalCompare(lhs.str, lhsQuoted, op, rhs.str, rhsQuoted);
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
	const char *p = par->p;
	Token tok;
	FStr val;

	if (!skip_string(&p, "empty"))
		return false;

	cpp_skip_whitespace(&p);
	if (*p != '(')
		return false;

	p--;			/* Make p[1] point to the '('. */
	val = Var_Parse(&p, SCOPE_CMDLINE, doEval ? VARE_EVAL : VARE_PARSE);
	/* TODO: handle errors */

	if (val.str == var_Error)
		tok = TOK_ERROR;
	else {
		cpp_skip_whitespace(&val.str);
		tok = ToToken(doEval && val.str[0] == '\0');
	}

	FStr_Done(&val);
	*out_token = tok;
	par->p = p;
	return true;
}

/* Parse a function call expression, such as 'exists(${file})'. */
static bool
CondParser_FuncCall(CondParser *par, bool doEval, Token *out_token)
{
	char *arg;
	const char *p = par->p;
	bool (*fn)(const char *);
	const char *fn_name = p;

	if (skip_string(&p, "defined"))
		fn = FuncDefined;
	else if (skip_string(&p, "make"))
		fn = FuncMake;
	else if (skip_string(&p, "exists"))
		fn = FuncExists;
	else if (skip_string(&p, "target"))
		fn = FuncTarget;
	else if (skip_string(&p, "commands"))
		fn = FuncCommands;
	else
		return false;

	cpp_skip_whitespace(&p);
	if (*p != '(')
		return false;

	arg = ParseFuncArg(&p, doEval, fn_name);
	*out_token = ToToken(doEval &&
	    arg != NULL && arg[0] != '\0' && fn(arg));
	free(arg);

	par->p = p;
	return true;
}

/*
 * Parse a comparison that neither starts with '"' nor '$', such as the
 * unusual 'bare == right' or '3 == ${VAR}', or a simple leaf without
 * operator, which is a number, an expression or a string literal.
 *
 * TODO: Can this be merged into CondParser_Comparison?
 */
static Token
CondParser_ComparisonOrLeaf(CondParser *par, bool doEval)
{
	Token t;
	char *arg;
	const char *p;

	p = par->p;
	if (ch_isdigit(p[0]) || p[0] == '-' || p[0] == '+')
		return CondParser_Comparison(par, doEval);

	/*
	 * Most likely we have a bare word to apply the default function to.
	 * However, ".if a == b" gets here when the "a" is unquoted and
	 * doesn't start with a '$'. This surprises people.
	 * If what follows the function argument is a '=' or '!' then the
	 * syntax would be invalid if we did "defined(a)" - so instead treat
	 * as an expression.
	 */
	/*
	 * XXX: In edge cases, an expression may be evaluated twice,
	 *  see cond-token-plain.mk, keyword 'twice'.
	 */
	arg = ParseWord(&p, doEval);
	assert(arg[0] != '\0');
	cpp_skip_hspace(&p);

	if (*p == '=' || *p == '!' || *p == '<' || *p == '>') {
		free(arg);
		return CondParser_Comparison(par, doEval);
	}
	par->p = p;

	/*
	 * Evaluate the argument using the default function.
	 * This path always treats .if as .ifdef. To get here, the character
	 * after .if must have been taken literally, so the argument cannot
	 * be empty - even if it contained an expression.
	 */
	t = ToToken(doEval && par->evalBare(arg) != par->negateEvalBare);
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
		else {
			Parse_Error(PARSE_FATAL, "Unknown operator '|'");
			return TOK_ERROR;
		}
		return TOK_OR;

	case '&':
		par->p++;
		if (par->p[0] == '&')
			par->p++;
		else {
			Parse_Error(PARSE_FATAL, "Unknown operator '&'");
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
	bool neg = false;

	while ((t = CondParser_Token(par, doEval)) == TOK_NOT)
		neg = !neg;

	if (t == TOK_TRUE || t == TOK_FALSE)
		return neg == (t == TOK_FALSE) ? CR_TRUE : CR_FALSE;

	if (t == TOK_LPAREN) {
		res = CondParser_Or(par, doEval);
		if (res == CR_ERROR)
			return CR_ERROR;
		if (CondParser_Token(par, doEval) != TOK_RPAREN)
			return CR_ERROR;
		return neg == (res == CR_FALSE) ? CR_TRUE : CR_FALSE;
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

/*
 * Evaluate the condition, including any side effects from the
 * expressions in the condition. The condition consists of &&, ||, !,
 * function(arg), comparisons and parenthetical groupings thereof.
 */
static CondResult
CondEvalExpression(const char *cond, bool plain,
		   bool (*evalBare)(const char *), bool negate,
		   bool eprint, bool leftUnquotedOK)
{
	CondParser par;
	CondResult rval;
	int parseErrorsBefore = parseErrors;

	cpp_skip_hspace(&cond);

	par.plain = plain;
	par.evalBare = evalBare;
	par.negateEvalBare = negate;
	par.leftUnquotedOK = leftUnquotedOK;
	par.p = cond;
	par.curr = TOK_NONE;

	DEBUG1(COND, "CondParser_Eval: %s\n", par.p);
	rval = CondParser_Or(&par, true);
	if (par.curr != TOK_EOF)
		rval = CR_ERROR;

	if (rval == CR_ERROR && eprint && parseErrors == parseErrorsBefore)
		Parse_Error(PARSE_FATAL, "Malformed conditional '%s'", cond);

	return rval;
}

/*
 * Evaluate a condition in a :? modifier, such as
 * ${"${VAR}" == value:?yes:no}.
 */
CondResult
Cond_EvalCondition(const char *cond)
{
	return CondEvalExpression(cond, true,
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
	const char *p = *pp + 2;

	*out_plain = false;
	*out_evalBare = FuncDefined;
	*out_negate = skip_string(&p, "n");

	if (skip_string(&p, "def")) {		/* .ifdef and .ifndef */
	} else if (skip_string(&p, "make"))	/* .ifmake and .ifnmake */
		*out_evalBare = FuncMake;
	else if (!*out_negate)			/* plain .if */
		*out_plain = true;
	else
		goto unknown_directive;
	if (ch_isalpha(*p))
		goto unknown_directive;

	*pp = p;
	return true;

unknown_directive:
	return false;
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
 *	CR_TRUE		to continue parsing the lines that follow the
 *			conditional (when <cond> evaluates to true)
 *	CR_FALSE	to skip the lines after the conditional
 *			(when <cond> evaluates to false, or when a previous
 *			branch was already taken)
 *	CR_ERROR	if the conditional was not valid, either because of
 *			a syntax error or because some variable was undefined
 *			or because the condition could not be evaluated
 */
CondResult
Cond_EvalLine(const char *line)
{
	typedef enum IfState {

		/* None of the previous <cond> evaluated to true. */
		IFS_INITIAL	= 0,

		/*
		 * The previous <cond> evaluated to true. The lines following
		 * this condition are interpreted.
		 */
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
	CondResult res;
	IfState state;
	const char *p = line;

	if (cond_states == NULL) {
		cond_states = bmake_malloc(
		    cond_states_cap * sizeof *cond_states);
		cond_states[0] = IFS_ACTIVE;
	}

	p++;			/* skip the leading '.' */
	cpp_skip_hspace(&p);

	if (IsEndif(p)) {
		if (p[5] != '\0') {
			Parse_Error(PARSE_FATAL,
			    "The .endif directive does not take arguments");
		}

		if (cond_depth == CurFile_CondMinDepth()) {
			Parse_Error(PARSE_FATAL, "if-less endif");
			return CR_TRUE;
		}

		/* Return state for previous conditional */
		cond_depth--;
		Parse_GuardEndif();
		return cond_states[cond_depth] & IFS_ACTIVE
		    ? CR_TRUE : CR_FALSE;
	}

	/* Parse the name of the directive, such as 'if', 'elif', 'endif'. */
	if (p[0] == 'e') {
		if (p[1] != 'l')
			return CR_ERROR;

		/* Quite likely this is 'else' or 'elif' */
		p += 2;
		if (strncmp(p, "se", 2) == 0 && !ch_isalpha(p[2])) {
			if (p[2] != '\0')
				Parse_Error(PARSE_FATAL,
				    "The .else directive "
				    "does not take arguments");

			if (cond_depth == CurFile_CondMinDepth()) {
				Parse_Error(PARSE_FATAL, "if-less else");
				return CR_TRUE;
			}
			Parse_GuardElse();

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

			return state & IFS_ACTIVE ? CR_TRUE : CR_FALSE;
		}
		/* Assume for now it is an elif */
		isElif = true;
	} else
		isElif = false;

	if (p[0] != 'i' || p[1] != 'f')
		return CR_ERROR;

	if (!DetermineKindOfConditional(&p, &plain, &evalBare, &negate))
		return CR_ERROR;

	if (isElif) {
		if (cond_depth == CurFile_CondMinDepth()) {
			Parse_Error(PARSE_FATAL, "if-less elif");
			return CR_TRUE;
		}
		Parse_GuardElse();
		state = cond_states[cond_depth];
		if (state & IFS_SEEN_ELSE) {
			Parse_Error(PARSE_WARNING, "extra elif");
			cond_states[cond_depth] =
			    IFS_WAS_ACTIVE | IFS_SEEN_ELSE;
			return CR_FALSE;
		}
		if (state != IFS_INITIAL) {
			cond_states[cond_depth] = IFS_WAS_ACTIVE;
			return CR_FALSE;
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
			    cond_states_cap * sizeof *cond_states);
		}
		state = cond_states[cond_depth];
		cond_depth++;
		if (!(state & IFS_ACTIVE)) {
			cond_states[cond_depth] = IFS_WAS_ACTIVE;
			return CR_FALSE;
		}
	}

	res = CondEvalExpression(p, plain, evalBare, negate, true, false);
	if (res == CR_ERROR) {
		/* Syntax error, error message already output. */
		/* Skip everything to the matching '.endif'. */
		/* An extra '.else' is not detected in this case. */
		cond_states[cond_depth] = IFS_WAS_ACTIVE;
		return CR_FALSE;
	}

	cond_states[cond_depth] = res == CR_TRUE ? IFS_ACTIVE : IFS_INITIAL;
	return res;
}

static bool
ParseVarnameGuard(const char **pp, const char **varname)
{
	const char *p = *pp;

	if (ch_isalpha(*p) || *p == '_') {
		while (ch_isalnum(*p) || *p == '_')
			p++;
		*varname = *pp;
		*pp = p;
		return true;
	}
	return false;
}

/* Extracts the multiple-inclusion guard from a conditional, if any. */
Guard *
Cond_ExtractGuard(const char *line)
{
	const char *p, *varname;
	Substring dir;
	Guard *guard;

	p = line + 1;		/* skip the '.' */
	cpp_skip_hspace(&p);

	dir.start = p;
	while (ch_isalpha(*p))
		p++;
	dir.end = p;
	cpp_skip_hspace(&p);

	if (Substring_Equals(dir, "if")) {
		if (skip_string(&p, "!defined(")) {
			if (ParseVarnameGuard(&p, &varname)
			    && strcmp(p, ")") == 0)
				goto found_variable;
		} else if (skip_string(&p, "!target(")) {
			const char *arg_p = p;
			free(ParseWord(&p, false));
			if (strcmp(p, ")") == 0) {
				guard = bmake_malloc(sizeof(*guard));
				guard->kind = GK_TARGET;
				guard->name = ParseWord(&arg_p, true);
				return guard;
			}
		}
	} else if (Substring_Equals(dir, "ifndef")) {
		if (ParseVarnameGuard(&p, &varname) && *p == '\0')
			goto found_variable;
	}
	return NULL;

found_variable:
	guard = bmake_malloc(sizeof(*guard));
	guard->kind = GK_VARIABLE;
	guard->name = bmake_strsedup(varname, p);
	return guard;
}

void
Cond_EndFile(void)
{
	unsigned int open_conds = cond_depth - CurFile_CondMinDepth();

	if (open_conds != 0) {
		Parse_Error(PARSE_FATAL, "%u open conditional%s",
		    open_conds, open_conds == 1 ? "" : "s");
		cond_depth = CurFile_CondMinDepth();
	}
}
