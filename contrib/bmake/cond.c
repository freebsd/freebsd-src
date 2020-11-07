/*	$NetBSD: cond.c,v 1.173 2020/10/30 20:30:44 rillig Exp $	*/

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

/* Handling of conditionals in a makefile.
 *
 * Interface:
 *	Cond_EvalLine	Evaluate the conditional.
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
MAKE_RCSID("$NetBSD: cond.c,v 1.173 2020/10/30 20:30:44 rillig Exp $");

/*
 * The parsing of conditional expressions is based on this grammar:
 *	E -> F || E
 *	E -> F
 *	F -> T && F
 *	F -> T
 *	T -> defined(variable)
 *	T -> make(target)
 *	T -> exists(file)
 *	T -> empty(varspec)
 *	T -> target(name)
 *	T -> commands(name)
 *	T -> symbol
 *	T -> $(varspec) op value
 *	T -> $(varspec) == "string"
 *	T -> $(varspec) != "string"
 *	T -> "string"
 *	T -> ( E )
 *	T -> ! T
 *	op -> == | != | > | < | >= | <=
 *
 * 'symbol' is some other symbol to which the default function is applied.
 *
 * The tokens are scanned by CondToken, which returns:
 *	TOK_AND		for '&' or '&&'
 *	TOK_OR		for '|' or '||'
 *	TOK_NOT		for '!'
 *	TOK_LPAREN	for '('
 *	TOK_RPAREN	for ')'
 * Other terminal symbols are evaluated using either the default function or
 * the function given in the terminal, they return either TOK_TRUE or
 * TOK_FALSE.
 *
 * TOK_FALSE is 0 and TOK_TRUE 1 so we can directly assign C comparisons.
 *
 * All non-terminal functions (CondParser_Expr, CondParser_Factor and
 * CondParser_Term) return either TOK_FALSE, TOK_TRUE, or TOK_ERROR on error.
 */
typedef enum Token {
    TOK_FALSE = 0, TOK_TRUE = 1, TOK_AND, TOK_OR, TOK_NOT,
    TOK_LPAREN, TOK_RPAREN, TOK_EOF, TOK_NONE, TOK_ERROR
} Token;

typedef struct CondParser {
    const struct If *if_info;	/* Info for current statement */
    const char *p;		/* The remaining condition to parse */
    Token curr;			/* Single push-back token used in parsing */

    /* Whether an error message has already been printed for this condition.
     * The first available error message is usually the most specific one,
     * therefore it makes sense to suppress the standard "Malformed
     * conditional" message. */
    Boolean printedError;
} CondParser;

static Token CondParser_Expr(CondParser *par, Boolean);

static unsigned int cond_depth = 0;	/* current .if nesting level */
static unsigned int cond_min_depth = 0;	/* depth at makefile open */

/*
 * Indicate when we should be strict about lhs of comparisons.
 * In strict mode, the lhs must be a variable expression or a string literal
 * in quotes. In non-strict mode it may also be an unquoted string literal.
 *
 * TRUE when CondEvalExpression is called from Cond_EvalLine (.if etc)
 * FALSE when CondEvalExpression is called from ApplyModifier_IfElse
 * since lhs is already expanded and we cannot tell if
 * it was a variable reference or not.
 */
static Boolean lhsStrict;

static int
is_token(const char *str, const char *tok, size_t len)
{
    return strncmp(str, tok, len) == 0 && !ch_isalpha(str[len]);
}

/* Push back the most recent token read. We only need one level of this. */
static void
CondParser_PushBack(CondParser *par, Token t)
{
    assert(par->curr == TOK_NONE);
    assert(t != TOK_NONE);

    par->curr = t;
}

static void
CondParser_SkipWhitespace(CondParser *par)
{
    cpp_skip_whitespace(&par->p);
}

/* Parse the argument of a built-in function.
 *
 * Arguments:
 *	*pp initially points at the '(',
 *	upon successful return it points right after the ')'.
 *
 *	*out_arg receives the argument as string.
 *
 *	func says whether the argument belongs to an actual function, or
 *	whether the parsed argument is passed to the default function.
 *
 * Return the length of the argument. */
static size_t
ParseFuncArg(const char **pp, Boolean doEval, const char *func,
	     char **out_arg) {
    const char *p = *pp;
    Buffer argBuf;
    int paren_depth;
    size_t argLen;

    if (func != NULL)
	p++;			/* Skip opening '(' - verified by caller */

    if (*p == '\0') {
	/*
	 * No arguments whatsoever. Because 'make' and 'defined' aren't really
	 * "reserved words", we don't print a message. I think this is better
	 * than hitting the user with a warning message every time s/he uses
	 * the word 'make' or 'defined' at the beginning of a symbol...
	 */
	*out_arg = NULL;
	return 0;
    }

    while (*p == ' ' || *p == '\t') {
	p++;
    }

    Buf_Init(&argBuf, 16);

    paren_depth = 0;
    for (;;) {
	char ch = *p;
	if (ch == 0 || ch == ' ' || ch == '\t')
	    break;
	if ((ch == '&' || ch == '|') && paren_depth == 0)
	    break;
	if (*p == '$') {
	    /*
	     * Parse the variable spec and install it as part of the argument
	     * if it's valid. We tell Var_Parse to complain on an undefined
	     * variable, so we don't need to do it. Nor do we return an error,
	     * though perhaps we should...
	     */
	    void *nestedVal_freeIt;
	    VarEvalFlags eflags = VARE_UNDEFERR | (doEval ? VARE_WANTRES : 0);
	    const char *nestedVal;
	    (void)Var_Parse(&p, VAR_CMDLINE, eflags, &nestedVal,
			    &nestedVal_freeIt);
	    /* TODO: handle errors */
	    Buf_AddStr(&argBuf, nestedVal);
	    free(nestedVal_freeIt);
	    continue;
	}
	if (ch == '(')
	    paren_depth++;
	else if (ch == ')' && --paren_depth < 0)
	    break;
	Buf_AddByte(&argBuf, *p);
	p++;
    }

    *out_arg = Buf_GetAll(&argBuf, &argLen);
    Buf_Destroy(&argBuf, FALSE);

    while (*p == ' ' || *p == '\t') {
	p++;
    }

    if (func != NULL && *p++ != ')') {
	Parse_Error(PARSE_WARNING, "Missing closing parenthesis for %s()",
		    func);
	/* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	return 0;
    }

    *pp = p;
    return argLen;
}

/* Test whether the given variable is defined. */
static Boolean
FuncDefined(size_t argLen MAKE_ATTR_UNUSED, const char *arg)
{
    void *freeIt;
    Boolean result = Var_Value(arg, VAR_CMDLINE, &freeIt) != NULL;
    bmake_free(freeIt);
    return result;
}

/* See if the given target is being made. */
static Boolean
FuncMake(size_t argLen MAKE_ATTR_UNUSED, const char *arg)
{
    StringListNode *ln;

    for (ln = opts.create->first; ln != NULL; ln = ln->next)
	if (Str_Match(ln->datum, arg))
	    return TRUE;
    return FALSE;
}

/* See if the given file exists. */
static Boolean
FuncExists(size_t argLen MAKE_ATTR_UNUSED, const char *arg)
{
    Boolean result;
    char *path;

    path = Dir_FindFile(arg, dirSearchPath);
    DEBUG2(COND, "exists(%s) result is \"%s\"\n", arg, path ? path : "");
    if (path != NULL) {
	result = TRUE;
	free(path);
    } else {
	result = FALSE;
    }
    return result;
}

/* See if the given node exists and is an actual target. */
static Boolean
FuncTarget(size_t argLen MAKE_ATTR_UNUSED, const char *arg)
{
    GNode *gn = Targ_FindNode(arg);
    return gn != NULL && GNode_IsTarget(gn);
}

/* See if the given node exists and is an actual target with commands
 * associated with it. */
static Boolean
FuncCommands(size_t argLen MAKE_ATTR_UNUSED, const char *arg)
{
    GNode *gn = Targ_FindNode(arg);
    return gn != NULL && GNode_IsTarget(gn) && !Lst_IsEmpty(gn->commands);
}

/*-
 * Convert the given number into a double.
 * We try a base 10 or 16 integer conversion first, if that fails
 * then we try a floating point conversion instead.
 *
 * Results:
 *	Sets 'value' to double value of string.
 *	Returns TRUE if the conversion succeeded.
 */
static Boolean
TryParseNumber(const char *str, double *value)
{
    char *eptr, ech;
    unsigned long l_val;
    double d_val;

    errno = 0;
    if (!*str) {
	*value = 0.0;
	return TRUE;
    }
    l_val = strtoul(str, &eptr, str[1] == 'x' ? 16 : 10);
    ech = *eptr;
    if (ech == '\0' && errno != ERANGE) {
	d_val = str[0] == '-' ? -(double)-l_val : (double)l_val;
    } else {
	if (ech != '\0' && ech != '.' && ech != 'e' && ech != 'E')
	    return FALSE;
	d_val = strtod(str, &eptr);
	if (*eptr)
	    return FALSE;
    }

    *value = d_val;
    return TRUE;
}

static Boolean
is_separator(char ch)
{
    return ch == '\0' || ch_isspace(ch) || strchr("!=><)", ch) != NULL;
}

/*-
 * Parse a string from a variable reference or an optionally quoted
 * string.  This is called for the lhs and rhs of string comparisons.
 *
 * Results:
 *	Returns the string, absent any quotes, or NULL on error.
 *	Sets quoted if the string was quoted.
 *	Sets freeIt if needed.
 */
/* coverity:[+alloc : arg-*4] */
static const char *
CondParser_String(CondParser *par, Boolean doEval, Boolean strictLHS,
		  Boolean *quoted, void **freeIt)
{
    Buffer buf;
    const char *str;
    Boolean atStart;
    const char *nested_p;
    Boolean qt;
    const char *start;
    VarEvalFlags eflags;
    VarParseResult parseResult;

    Buf_Init(&buf, 0);
    str = NULL;
    *freeIt = NULL;
    *quoted = qt = par->p[0] == '"' ? 1 : 0;
    start = par->p;
    if (qt)
	par->p++;
    while (par->p[0] && str == NULL) {
	switch (par->p[0]) {
	case '\\':
	    par->p++;
	    if (par->p[0] != '\0') {
		Buf_AddByte(&buf, par->p[0]);
		par->p++;
	    }
	    continue;
	case '"':
	    if (qt) {
		par->p++;	/* we don't want the quotes */
		goto got_str;
	    }
	    Buf_AddByte(&buf, par->p[0]); /* likely? */
	    par->p++;
	    continue;
	case ')':
	case '!':
	case '=':
	case '>':
	case '<':
	case ' ':
	case '\t':
	    if (!qt)
		goto got_str;
	    Buf_AddByte(&buf, par->p[0]);
	    par->p++;
	    continue;
	case '$':
	    /* if we are in quotes, an undefined variable is ok */
	    eflags = ((!qt && doEval) ? VARE_UNDEFERR : 0) |
		     (doEval ? VARE_WANTRES : 0);
	    nested_p = par->p;
	    atStart = nested_p == start;
	    parseResult = Var_Parse(&nested_p, VAR_CMDLINE, eflags, &str,
				    freeIt);
	    /* TODO: handle errors */
	    if (str == var_Error) {
		if (parseResult & VPR_ANY_MSG)
		    par->printedError = TRUE;
		if (*freeIt) {
		    free(*freeIt);
		    *freeIt = NULL;
		}
		/*
		 * Even if !doEval, we still report syntax errors, which
		 * is what getting var_Error back with !doEval means.
		 */
		str = NULL;
		goto cleanup;
	    }
	    par->p = nested_p;

	    /*
	     * If the '$' started the string literal (which means no quotes),
	     * and the variable expression is followed by a space, looks like
	     * a comparison operator or is the end of the expression, we are
	     * done.
	     */
	    if (atStart && is_separator(par->p[0]))
		goto cleanup;

	    Buf_AddStr(&buf, str);
	    if (*freeIt) {
		free(*freeIt);
		*freeIt = NULL;
	    }
	    str = NULL;		/* not finished yet */
	    continue;
	default:
	    if (strictLHS && !qt && *start != '$' && !ch_isdigit(*start)) {
		/* lhs must be quoted, a variable reference or number */
		if (*freeIt) {
		    free(*freeIt);
		    *freeIt = NULL;
		}
		str = NULL;
		goto cleanup;
	    }
	    Buf_AddByte(&buf, par->p[0]);
	    par->p++;
	    continue;
	}
    }
got_str:
    *freeIt = Buf_GetAll(&buf, NULL);
    str = *freeIt;
cleanup:
    Buf_Destroy(&buf, FALSE);
    return str;
}

/* The different forms of .if directives. */
static const struct If {
    const char *form;		/* Form of if */
    size_t formlen;		/* Length of form */
    Boolean doNot;		/* TRUE if default function should be negated */
    Boolean (*defProc)(size_t, const char *); /* Default function to apply */
} ifs[] = {
    { "def",   3, FALSE, FuncDefined },
    { "ndef",  4, TRUE,  FuncDefined },
    { "make",  4, FALSE, FuncMake },
    { "nmake", 5, TRUE,  FuncMake },
    { "",      0, FALSE, FuncDefined },
    { NULL,    0, FALSE, NULL }
};

/* Evaluate a "comparison without operator", such as in ".if ${VAR}" or
 * ".if 0". */
static Token
EvalNotEmpty(CondParser *par, const char *lhs, Boolean lhsQuoted)
{
    double left;

    /* For .ifxxx "..." check for non-empty string. */
    if (lhsQuoted)
	return lhs[0] != '\0';

    /* For .ifxxx <number> compare against zero */
    if (TryParseNumber(lhs, &left))
	return left != 0.0;

    /* For .if ${...} check for non-empty string (defProc is ifdef). */
    if (par->if_info->form[0] == '\0')
	return lhs[0] != 0;

    /* Otherwise action default test ... */
    return par->if_info->defProc(strlen(lhs), lhs) == !par->if_info->doNot;
}

/* Evaluate a numerical comparison, such as in ".if ${VAR} >= 9". */
static Token
EvalCompareNum(double lhs, const char *op, double rhs)
{
    DEBUG3(COND, "lhs = %f, rhs = %f, op = %.2s\n", lhs, rhs, op);

    switch (op[0]) {
    case '!':
	if (op[1] != '=') {
	    Parse_Error(PARSE_WARNING, "Unknown operator");
	    /* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	    return TOK_ERROR;
	}
	return lhs != rhs;
    case '=':
	if (op[1] != '=') {
	    Parse_Error(PARSE_WARNING, "Unknown operator");
	    /* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	    return TOK_ERROR;
	}
	return lhs == rhs;
    case '<':
	return op[1] == '=' ? lhs <= rhs : lhs < rhs;
    case '>':
	return op[1] == '=' ? lhs >= rhs : lhs > rhs;
    }
    return TOK_ERROR;
}

static Token
EvalCompareStr(const char *lhs, const char *op, const char *rhs)
{
    if (!((op[0] == '!' || op[0] == '=') && op[1] == '=')) {
	Parse_Error(PARSE_WARNING,
		    "String comparison operator must be either == or !=");
	/* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	return TOK_ERROR;
    }

    DEBUG3(COND, "lhs = \"%s\", rhs = \"%s\", op = %.2s\n", lhs, rhs, op);
    return (*op == '=') == (strcmp(lhs, rhs) == 0);
}

/* Evaluate a comparison, such as "${VAR} == 12345". */
static Token
EvalCompare(const char *lhs, Boolean lhsQuoted, const char *op,
	    const char *rhs, Boolean rhsQuoted)
{
    double left, right;

    if (!rhsQuoted && !lhsQuoted)
	if (TryParseNumber(lhs, &left) && TryParseNumber(rhs, &right))
	    return EvalCompareNum(left, op, right);

    return EvalCompareStr(lhs, op, rhs);
}

/* Parse a comparison condition such as:
 *
 *	0
 *	${VAR:Mpattern}
 *	${VAR} == value
 *	${VAR:U0} < 12345
 */
static Token
CondParser_Comparison(CondParser *par, Boolean doEval)
{
    Token t = TOK_ERROR;
    const char *lhs, *op, *rhs;
    void *lhsFree, *rhsFree;
    Boolean lhsQuoted, rhsQuoted;

    rhs = NULL;
    lhsFree = rhsFree = NULL;
    lhsQuoted = rhsQuoted = FALSE;

    /*
     * Parse the variable spec and skip over it, saving its
     * value in lhs.
     */
    lhs = CondParser_String(par, doEval, lhsStrict, &lhsQuoted, &lhsFree);
    if (!lhs)
	goto done;

    CondParser_SkipWhitespace(par);

    /*
     * Make sure the operator is a valid one. If it isn't a
     * known relational operator, pretend we got a
     * != 0 comparison.
     */
    op = par->p;
    switch (par->p[0]) {
    case '!':
    case '=':
    case '<':
    case '>':
	if (par->p[1] == '=') {
	    par->p += 2;
	} else {
	    par->p++;
	}
	break;
    default:
	t = doEval ? EvalNotEmpty(par, lhs, lhsQuoted) : TOK_FALSE;
	goto done;
    }

    CondParser_SkipWhitespace(par);

    if (par->p[0] == '\0') {
	Parse_Error(PARSE_WARNING, "Missing right-hand-side of operator");
	/* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	goto done;
    }

    rhs = CondParser_String(par, doEval, FALSE, &rhsQuoted, &rhsFree);
    if (rhs == NULL)
	goto done;

    if (!doEval) {
	t = TOK_FALSE;
	goto done;
    }

    t = EvalCompare(lhs, lhsQuoted, op, rhs, rhsQuoted);

done:
    free(lhsFree);
    free(rhsFree);
    return t;
}

static size_t
ParseEmptyArg(const char **linePtr, Boolean doEval,
	      const char *func MAKE_ATTR_UNUSED, char **argPtr)
{
    void *val_freeIt;
    const char *val;
    size_t magic_res;

    /* We do all the work here and return the result as the length */
    *argPtr = NULL;

    (*linePtr)--;		/* Make (*linePtr)[1] point to the '('. */
    (void)Var_Parse(linePtr, VAR_CMDLINE, doEval ? VARE_WANTRES : 0,
		    &val, &val_freeIt);
    /* TODO: handle errors */
    /* If successful, *linePtr points beyond the closing ')' now. */

    if (val == var_Error) {
	free(val_freeIt);
	return (size_t)-1;
    }

    /* A variable is empty when it just contains spaces... 4/15/92, christos */
    cpp_skip_whitespace(&val);

    /*
     * For consistency with the other functions we can't generate the
     * true/false here.
     */
    magic_res = *val != '\0' ? 2 : 1;
    free(val_freeIt);
    return magic_res;
}

static Boolean
FuncEmpty(size_t arglen, const char *arg MAKE_ATTR_UNUSED)
{
    /* Magic values ahead, see ParseEmptyArg. */
    return arglen == 1;
}

static Token
CondParser_Func(CondParser *par, Boolean doEval)
{
    static const struct fn_def {
	const char *fn_name;
	size_t fn_name_len;
	size_t (*fn_parse)(const char **, Boolean, const char *, char **);
	Boolean (*fn_eval)(size_t, const char *);
    } fn_defs[] = {
	{ "defined",  7, ParseFuncArg,  FuncDefined },
	{ "make",     4, ParseFuncArg,  FuncMake },
	{ "exists",   6, ParseFuncArg,  FuncExists },
	{ "empty",    5, ParseEmptyArg, FuncEmpty },
	{ "target",   6, ParseFuncArg,  FuncTarget },
	{ "commands", 8, ParseFuncArg,  FuncCommands },
	{ NULL,       0, NULL, NULL },
    };
    const struct fn_def *fn_def;
    Token t;
    char *arg = NULL;
    size_t arglen;
    const char *cp = par->p;
    const char *cp1;

    for (fn_def = fn_defs; fn_def->fn_name != NULL; fn_def++) {
	if (!is_token(cp, fn_def->fn_name, fn_def->fn_name_len))
	    continue;
	cp += fn_def->fn_name_len;
	/* There can only be whitespace before the '(' */
	cpp_skip_whitespace(&cp);
	if (*cp != '(')
	    break;

	arglen = fn_def->fn_parse(&cp, doEval, fn_def->fn_name, &arg);
	if (arglen == 0 || arglen == (size_t)-1) {
	    par->p = cp;
	    return arglen == 0 ? TOK_FALSE : TOK_ERROR;
	}
	/* Evaluate the argument using the required function. */
	t = !doEval || fn_def->fn_eval(arglen, arg);
	free(arg);
	par->p = cp;
	return t;
    }

    /* Push anything numeric through the compare expression */
    cp = par->p;
    if (ch_isdigit(cp[0]) || strchr("+-", cp[0]))
	return CondParser_Comparison(par, doEval);

    /*
     * Most likely we have a naked token to apply the default function to.
     * However ".if a == b" gets here when the "a" is unquoted and doesn't
     * start with a '$'. This surprises people.
     * If what follows the function argument is a '=' or '!' then the syntax
     * would be invalid if we did "defined(a)" - so instead treat as an
     * expression.
     */
    arglen = ParseFuncArg(&cp, doEval, NULL, &arg);
    cp1 = cp;
    cpp_skip_whitespace(&cp1);
    if (*cp1 == '=' || *cp1 == '!')
	return CondParser_Comparison(par, doEval);
    par->p = cp;

    /*
     * Evaluate the argument using the default function.
     * This path always treats .if as .ifdef. To get here, the character
     * after .if must have been taken literally, so the argument cannot
     * be empty - even if it contained a variable expansion.
     */
    t = !doEval || par->if_info->defProc(arglen, arg) == !par->if_info->doNot;
    free(arg);
    return t;
}

/* Return the next token or comparison result from the parser. */
static Token
CondParser_Token(CondParser *par, Boolean doEval)
{
    Token t;

    t = par->curr;
    if (t != TOK_NONE) {
	par->curr = TOK_NONE;
	return t;
    }

    while (par->p[0] == ' ' || par->p[0] == '\t') {
	par->p++;
    }

    switch (par->p[0]) {

    case '(':
	par->p++;
	return TOK_LPAREN;

    case ')':
	par->p++;
	return TOK_RPAREN;

    case '|':
	par->p++;
	if (par->p[0] == '|') {
	    par->p++;
	}
	return TOK_OR;

    case '&':
	par->p++;
	if (par->p[0] == '&') {
	    par->p++;
	}
	return TOK_AND;

    case '!':
	par->p++;
	return TOK_NOT;

    case '#':
    case '\n':
    case '\0':
	return TOK_EOF;

    case '"':
    case '$':
	return CondParser_Comparison(par, doEval);

    default:
	return CondParser_Func(par, doEval);
    }
}

/* Parse a single term in the expression. This consists of a terminal symbol
 * or TOK_NOT and a term (not including the binary operators):
 *
 *	T -> defined(variable) | make(target) | exists(file) | symbol
 *	T -> ! T | ( E )
 *
 * Results:
 *	TOK_TRUE, TOK_FALSE or TOK_ERROR.
 */
static Token
CondParser_Term(CondParser *par, Boolean doEval)
{
    Token t;

    t = CondParser_Token(par, doEval);

    if (t == TOK_EOF) {
	/*
	 * If we reached the end of the expression, the expression
	 * is malformed...
	 */
	t = TOK_ERROR;
    } else if (t == TOK_LPAREN) {
	/*
	 * T -> ( E )
	 */
	t = CondParser_Expr(par, doEval);
	if (t != TOK_ERROR) {
	    if (CondParser_Token(par, doEval) != TOK_RPAREN) {
		t = TOK_ERROR;
	    }
	}
    } else if (t == TOK_NOT) {
	t = CondParser_Term(par, doEval);
	if (t == TOK_TRUE) {
	    t = TOK_FALSE;
	} else if (t == TOK_FALSE) {
	    t = TOK_TRUE;
	}
    }
    return t;
}

/* Parse a conjunctive factor (nice name, wot?)
 *
 *	F -> T && F | T
 *
 * Results:
 *	TOK_TRUE, TOK_FALSE or TOK_ERROR
 */
static Token
CondParser_Factor(CondParser *par, Boolean doEval)
{
    Token l, o;

    l = CondParser_Term(par, doEval);
    if (l != TOK_ERROR) {
	o = CondParser_Token(par, doEval);

	if (o == TOK_AND) {
	    /*
	     * F -> T && F
	     *
	     * If T is TOK_FALSE, the whole thing will be TOK_FALSE, but we
	     * have to parse the r.h.s. anyway (to throw it away).
	     * If T is TOK_TRUE, the result is the r.h.s., be it a TOK_ERROR
	     * or not.
	     */
	    if (l == TOK_TRUE) {
		l = CondParser_Factor(par, doEval);
	    } else {
		(void)CondParser_Factor(par, FALSE);
	    }
	} else {
	    /*
	     * F -> T
	     */
	    CondParser_PushBack(par, o);
	}
    }
    return l;
}

/* Main expression production.
 *
 *	E -> F || E | F
 *
 * Results:
 *	TOK_TRUE, TOK_FALSE or TOK_ERROR.
 */
static Token
CondParser_Expr(CondParser *par, Boolean doEval)
{
    Token l, o;

    l = CondParser_Factor(par, doEval);
    if (l != TOK_ERROR) {
	o = CondParser_Token(par, doEval);

	if (o == TOK_OR) {
	    /*
	     * E -> F || E
	     *
	     * A similar thing occurs for ||, except that here we make sure
	     * the l.h.s. is TOK_FALSE before we bother to evaluate the r.h.s.
	     * Once again, if l is TOK_FALSE, the result is the r.h.s. and once
	     * again if l is TOK_TRUE, we parse the r.h.s. to throw it away.
	     */
	    if (l == TOK_FALSE) {
		l = CondParser_Expr(par, doEval);
	    } else {
		(void)CondParser_Expr(par, FALSE);
	    }
	} else {
	    /*
	     * E -> F
	     */
	    CondParser_PushBack(par, o);
	}
    }
    return l;
}

static CondEvalResult
CondParser_Eval(CondParser *par, Boolean *value)
{
    Token res;

    DEBUG1(COND, "CondParser_Eval: %s\n", par->p);

    res = CondParser_Expr(par, TRUE);
    if (res != TOK_FALSE && res != TOK_TRUE)
	return COND_INVALID;

    if (CondParser_Token(par, TRUE /* XXX: Why TRUE? */) != TOK_EOF)
	return COND_INVALID;

    *value = res == TOK_TRUE;
    return COND_PARSE;
}

/* Evaluate the condition, including any side effects from the variable
 * expressions in the condition. The condition consists of &&, ||, !,
 * function(arg), comparisons and parenthetical groupings thereof.
 *
 * Results:
 *	COND_PARSE	if the condition was valid grammatically
 *	COND_INVALID	if not a valid conditional.
 *
 *	(*value) is set to the boolean value of the condition
 */
static CondEvalResult
CondEvalExpression(const struct If *info, const char *cond, Boolean *value,
		    Boolean eprint, Boolean strictLHS)
{
    static const struct If *dflt_info;
    CondParser par;
    int rval;

    lhsStrict = strictLHS;

    while (*cond == ' ' || *cond == '\t')
	cond++;

    if (info == NULL && (info = dflt_info) == NULL) {
	/* Scan for the entry for .if - it can't be first */
	for (info = ifs;; info++)
	    if (info->form[0] == 0)
		break;
	dflt_info = info;
    }
    assert(info != NULL);

    par.if_info = info;
    par.p = cond;
    par.curr = TOK_NONE;
    par.printedError = FALSE;

    rval = CondParser_Eval(&par, value);

    if (rval == COND_INVALID && eprint && !par.printedError)
	Parse_Error(PARSE_FATAL, "Malformed conditional (%s)", cond);

    return rval;
}

CondEvalResult
Cond_EvalCondition(const char *cond, Boolean *out_value)
{
	return CondEvalExpression(NULL, cond, out_value, FALSE, FALSE);
}

/* Evaluate the conditional in the passed line. The line looks like this:
 *	.<cond-type> <expr>
 * In this line, <cond-type> is any of if, ifmake, ifnmake, ifdef, ifndef,
 * elif, elifmake, elifnmake, elifdef, elifndef.
 * In this line, <expr> consists of &&, ||, !, function(arg), comparisons
 * and parenthetical groupings thereof.
 *
 * Note that the states IF_ACTIVE and ELSE_ACTIVE are only different in order
 * to detect spurious .else lines (as are SKIP_TO_ELSE and SKIP_TO_ENDIF),
 * otherwise .else could be treated as '.elif 1'.
 *
 * Results:
 *	COND_PARSE	to continue parsing the lines after the conditional
 *			(when .if or .else returns TRUE)
 *	COND_SKIP	to skip the lines after the conditional
 *			(when .if or .elif returns FALSE, or when a previous
 *			branch has already been taken)
 *	COND_INVALID	if the conditional was not valid, either because of
 *			a syntax error or because some variable was undefined
 *			or because the condition could not be evaluated
 */
CondEvalResult
Cond_EvalLine(const char *line)
{
    enum { MAXIF = 128 };	/* maximum depth of .if'ing */
    enum { MAXIF_BUMP = 32 };	/* how much to grow by */
    enum if_states {
	IF_ACTIVE,		/* .if or .elif part active */
	ELSE_ACTIVE,		/* .else part active */
	SEARCH_FOR_ELIF,	/* searching for .elif/else to execute */
	SKIP_TO_ELSE,		/* has been true, but not seen '.else' */
	SKIP_TO_ENDIF		/* nothing else to execute */
    };
    static enum if_states *cond_state = NULL;
    static unsigned int max_if_depth = MAXIF;

    const struct If *ifp;
    Boolean isElif;
    Boolean value;
    enum if_states state;

    if (!cond_state) {
	cond_state = bmake_malloc(max_if_depth * sizeof(*cond_state));
	cond_state[0] = IF_ACTIVE;
    }
    /* skip leading character (the '.') and any whitespace */
    for (line++; *line == ' ' || *line == '\t'; line++)
	continue;

    /* Find what type of if we're dealing with.  */
    if (line[0] == 'e') {
	if (line[1] != 'l') {
	    if (!is_token(line + 1, "ndif", 4))
		return COND_INVALID;
	    /* End of conditional section */
	    if (cond_depth == cond_min_depth) {
		Parse_Error(PARSE_FATAL, "if-less endif");
		return COND_PARSE;
	    }
	    /* Return state for previous conditional */
	    cond_depth--;
	    return cond_state[cond_depth] <= ELSE_ACTIVE
		   ? COND_PARSE : COND_SKIP;
	}

	/* Quite likely this is 'else' or 'elif' */
	line += 2;
	if (is_token(line, "se", 2)) {
	    /* It is else... */
	    if (cond_depth == cond_min_depth) {
		Parse_Error(PARSE_FATAL, "if-less else");
		return COND_PARSE;
	    }

	    state = cond_state[cond_depth];
	    switch (state) {
	    case SEARCH_FOR_ELIF:
		state = ELSE_ACTIVE;
		break;
	    case ELSE_ACTIVE:
	    case SKIP_TO_ENDIF:
		Parse_Error(PARSE_WARNING, "extra else");
		/* FALLTHROUGH */
	    default:
	    case IF_ACTIVE:
	    case SKIP_TO_ELSE:
		state = SKIP_TO_ENDIF;
		break;
	    }
	    cond_state[cond_depth] = state;
	    return state <= ELSE_ACTIVE ? COND_PARSE : COND_SKIP;
	}
	/* Assume for now it is an elif */
	isElif = TRUE;
    } else
	isElif = FALSE;

    if (line[0] != 'i' || line[1] != 'f')
	/* Not an ifxxx or elifxxx line */
	return COND_INVALID;

    /*
     * Figure out what sort of conditional it is -- what its default
     * function is, etc. -- by looking in the table of valid "ifs"
     */
    line += 2;
    for (ifp = ifs;; ifp++) {
	if (ifp->form == NULL)
	    return COND_INVALID;
	if (is_token(ifp->form, line, ifp->formlen)) {
	    line += ifp->formlen;
	    break;
	}
    }

    /* Now we know what sort of 'if' it is... */

    if (isElif) {
	if (cond_depth == cond_min_depth) {
	    Parse_Error(PARSE_FATAL, "if-less elif");
	    return COND_PARSE;
	}
	state = cond_state[cond_depth];
	if (state == SKIP_TO_ENDIF || state == ELSE_ACTIVE) {
	    Parse_Error(PARSE_WARNING, "extra elif");
	    cond_state[cond_depth] = SKIP_TO_ENDIF;
	    return COND_SKIP;
	}
	if (state != SEARCH_FOR_ELIF) {
	    /* Either just finished the 'true' block, or already SKIP_TO_ELSE */
	    cond_state[cond_depth] = SKIP_TO_ELSE;
	    return COND_SKIP;
	}
    } else {
	/* Normal .if */
	if (cond_depth + 1 >= max_if_depth) {
	    /*
	     * This is rare, but not impossible.
	     * In meta mode, dirdeps.mk (only runs at level 0)
	     * can need more than the default.
	     */
	    max_if_depth += MAXIF_BUMP;
	    cond_state = bmake_realloc(cond_state,
				       max_if_depth * sizeof(*cond_state));
	}
	state = cond_state[cond_depth];
	cond_depth++;
	if (state > ELSE_ACTIVE) {
	    /* If we aren't parsing the data, treat as always false */
	    cond_state[cond_depth] = SKIP_TO_ELSE;
	    return COND_SKIP;
	}
    }

    /* And evaluate the conditional expression */
    if (CondEvalExpression(ifp, line, &value, TRUE, TRUE) == COND_INVALID) {
	/* Syntax error in conditional, error message already output. */
	/* Skip everything to matching .endif */
	cond_state[cond_depth] = SKIP_TO_ELSE;
	return COND_SKIP;
    }

    if (!value) {
	cond_state[cond_depth] = SEARCH_FOR_ELIF;
	return COND_SKIP;
    }
    cond_state[cond_depth] = IF_ACTIVE;
    return COND_PARSE;
}

void
Cond_restore_depth(unsigned int saved_depth)
{
    unsigned int open_conds = cond_depth - cond_min_depth;

    if (open_conds != 0 || saved_depth > cond_depth) {
	Parse_Error(PARSE_FATAL, "%u open conditional%s", open_conds,
		    open_conds == 1 ? "" : "s");
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
