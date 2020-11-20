/*	$NetBSD: cond.c,v 1.214 2020/11/13 09:01:59 rillig Exp $	*/

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
MAKE_RCSID("$NetBSD: cond.c,v 1.214 2020/11/13 09:01:59 rillig Exp $");

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
 * since lhs is already expanded, and at that point we cannot tell if
 * it was a variable reference or not.
 */
static Boolean lhsStrict;

static int
is_token(const char *str, const char *tok, size_t len)
{
    return strncmp(str, tok, len) == 0 && !ch_isalpha(str[len]);
}

static Token
ToToken(Boolean cond)
{
    return cond ? TOK_TRUE : TOK_FALSE;
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
 * Return the length of the argument, or 0 on error. */
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
	*out_arg = NULL;	/* Missing closing parenthesis: */
	return 0;		/* .if defined( */
    }

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
	     * Parse the variable spec and install it as part of the argument
	     * if it's valid. We tell Var_Parse to complain on an undefined
	     * variable, so we don't need to do it. Nor do we return an error,
	     * though perhaps we should...
	     */
	    void *nestedVal_freeIt;
	    VarEvalFlags eflags = doEval ? VARE_WANTRES | VARE_UNDEFERR
					 : VARE_NONE;
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

    cpp_skip_hspace(&p);

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
    DEBUG2(COND, "exists(%s) result is \"%s\"\n",
	   arg, path != NULL ? path : "");
    result = path != NULL;
    free(path);
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

/*
 * Convert the given number into a double.
 * We try a base 10 or 16 integer conversion first, if that fails
 * then we try a floating point conversion instead.
 *
 * Results:
 *	Returns TRUE if the conversion succeeded.
 *	Sets 'out_value' to the converted number.
 */
static Boolean
TryParseNumber(const char *str, double *out_value)
{
    char *end;
    unsigned long ul_val;
    double dbl_val;

    errno = 0;
    if (str[0] == '\0') {	/* XXX: why is an empty string a number? */
	*out_value = 0.0;
	return TRUE;
    }

    ul_val = strtoul(str, &end, str[1] == 'x' ? 16 : 10);
    if (*end == '\0' && errno != ERANGE) {
	*out_value = str[0] == '-' ? -(double)-ul_val : (double)ul_val;
	return TRUE;
    }

    if (*end != '\0' && *end != '.' && *end != 'e' && *end != 'E')
	return FALSE;		/* skip the expensive strtod call */
    dbl_val = strtod(str, &end);
    if (*end != '\0')
	return FALSE;

    *out_value = dbl_val;
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
 *	Sets out_quoted if the string was quoted.
 *	Sets out_freeIt.
 */
/* coverity:[+alloc : arg-*4] */
static const char *
CondParser_String(CondParser *par, Boolean doEval, Boolean strictLHS,
		  Boolean *out_quoted, void **out_freeIt)
{
    Buffer buf;
    const char *str;
    Boolean atStart;
    const char *nested_p;
    Boolean quoted;
    const char *start;
    VarEvalFlags eflags;
    VarParseResult parseResult;

    Buf_Init(&buf);
    str = NULL;
    *out_freeIt = NULL;
    *out_quoted = quoted = par->p[0] == '"';
    start = par->p;
    if (quoted)
	par->p++;
    while (par->p[0] != '\0' && str == NULL) {
	switch (par->p[0]) {
	case '\\':
	    par->p++;
	    if (par->p[0] != '\0') {
		Buf_AddByte(&buf, par->p[0]);
		par->p++;
	    }
	    continue;
	case '"':
	    if (quoted) {
		par->p++;	/* skip the closing quote */
		goto got_str;
	    }
	    Buf_AddByte(&buf, par->p[0]); /* likely? */
	    par->p++;
	    continue;
	case ')':		/* see is_separator */
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
	    /* if we are in quotes, an undefined variable is ok */
	    eflags = doEval && !quoted ? VARE_WANTRES | VARE_UNDEFERR :
		     doEval ? VARE_WANTRES :
		     VARE_NONE;

	    nested_p = par->p;
	    atStart = nested_p == start;
	    parseResult = Var_Parse(&nested_p, VAR_CMDLINE, eflags, &str,
				    out_freeIt);
	    /* TODO: handle errors */
	    if (str == var_Error) {
		if (parseResult & VPR_ANY_MSG)
		    par->printedError = TRUE;
		if (*out_freeIt != NULL) {
		    /* XXX: Can there be any situation in which a returned
		     * var_Error requires freeIt? */
		    free(*out_freeIt);
		    *out_freeIt = NULL;
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
	    if (*out_freeIt) {
		free(*out_freeIt);
		*out_freeIt = NULL;
	    }
	    str = NULL;		/* not finished yet */
	    continue;
	default:
	    if (strictLHS && !quoted && *start != '$' && !ch_isdigit(*start)) {
		/* lhs must be quoted, a variable reference or number */
		str = NULL;
		goto cleanup;
	    }
	    Buf_AddByte(&buf, par->p[0]);
	    par->p++;
	    continue;
	}
    }
got_str:
    *out_freeIt = Buf_GetAll(&buf, NULL);
    str = *out_freeIt;
cleanup:
    Buf_Destroy(&buf, FALSE);
    return str;
}

struct If {
    const char *form;		/* Form of if */
    size_t formlen;		/* Length of form */
    Boolean doNot;		/* TRUE if default function should be negated */
    Boolean (*defProc)(size_t, const char *); /* Default function to apply */
};

/* The different forms of .if directives. */
static const struct If ifs[] = {
    { "def",   3, FALSE, FuncDefined },
    { "ndef",  4, TRUE,  FuncDefined },
    { "make",  4, FALSE, FuncMake },
    { "nmake", 5, TRUE,  FuncMake },
    { "",      0, FALSE, FuncDefined },
    { NULL,    0, FALSE, NULL }
};
enum { PLAIN_IF_INDEX = 4 };

static Boolean
If_Eval(const struct If *if_info, const char *arg, size_t arglen)
{
    Boolean res = if_info->defProc(arglen, arg);
    return if_info->doNot ? !res : res;
}

/* Evaluate a "comparison without operator", such as in ".if ${VAR}" or
 * ".if 0". */
static Boolean
EvalNotEmpty(CondParser *par, const char *value, Boolean quoted)
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
    /* XXX: Whitespace should count as empty, just as in ParseEmptyArg. */
    if (par->if_info->form[0] == '\0')
	return value[0] != '\0';

    /* For the other variants of .ifxxx ${...}, use its default function. */
    return If_Eval(par->if_info, value, strlen(value));
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
	return ToToken(lhs != rhs);
    case '=':
	if (op[1] != '=') {
	    Parse_Error(PARSE_WARNING, "Unknown operator");
	    /* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	    return TOK_ERROR;
	}
	return ToToken(lhs == rhs);
    case '<':
	return ToToken(op[1] == '=' ? lhs <= rhs : lhs < rhs);
    case '>':
	return ToToken(op[1] == '=' ? lhs >= rhs : lhs > rhs);
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
    return ToToken((*op == '=') == (strcmp(lhs, rhs) == 0));
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
    void *lhs_freeIt, *rhs_freeIt;
    Boolean lhsQuoted, rhsQuoted;

    /*
     * Parse the variable spec and skip over it, saving its
     * value in lhs.
     */
    lhs = CondParser_String(par, doEval, lhsStrict, &lhsQuoted, &lhs_freeIt);
    if (lhs == NULL)
	goto done_lhs;

    CondParser_SkipWhitespace(par);

    op = par->p;
    switch (par->p[0]) {
    case '!':
    case '=':
    case '<':
    case '>':
	if (par->p[1] == '=')
	    par->p += 2;
	else
	    par->p++;
	break;
    default:
	/* Unknown operator, compare against an empty string or 0. */
	t = ToToken(doEval && EvalNotEmpty(par, lhs, lhsQuoted));
	goto done_lhs;
    }

    CondParser_SkipWhitespace(par);

    if (par->p[0] == '\0') {
	Parse_Error(PARSE_WARNING, "Missing right-hand-side of operator");
	/* The PARSE_FATAL is done as a follow-up by CondEvalExpression. */
	goto done_lhs;
    }

    rhs = CondParser_String(par, doEval, FALSE, &rhsQuoted, &rhs_freeIt);
    if (rhs == NULL)
	goto done_rhs;

    if (!doEval) {
	t = TOK_FALSE;
	goto done_rhs;
    }

    t = EvalCompare(lhs, lhsQuoted, op, rhs, rhsQuoted);

done_rhs:
    free(rhs_freeIt);
done_lhs:
    free(lhs_freeIt);
    return t;
}

/* The argument to empty() is a variable name, optionally followed by
 * variable modifiers. */
static size_t
ParseEmptyArg(const char **pp, Boolean doEval,
	      const char *func MAKE_ATTR_UNUSED, char **out_arg)
{
    void *val_freeIt;
    const char *val;
    size_t magic_res;

    /* We do all the work here and return the result as the length */
    *out_arg = NULL;

    (*pp)--;			/* Make (*pp)[1] point to the '('. */
    (void)Var_Parse(pp, VAR_CMDLINE, doEval ? VARE_WANTRES : VARE_NONE,
		    &val, &val_freeIt);
    /* TODO: handle errors */
    /* If successful, *pp points beyond the closing ')' now. */

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

static Boolean
CondParser_Func(CondParser *par, Boolean doEval, Token *out_token)
{
    static const struct fn_def {
	const char *fn_name;
	size_t fn_name_len;
	size_t (*fn_parse)(const char **, Boolean, const char *, char **);
	Boolean (*fn_eval)(size_t, const char *);
    } fns[] = {
	{ "defined",  7, ParseFuncArg,  FuncDefined },
	{ "make",     4, ParseFuncArg,  FuncMake },
	{ "exists",   6, ParseFuncArg,  FuncExists },
	{ "empty",    5, ParseEmptyArg, FuncEmpty },
	{ "target",   6, ParseFuncArg,  FuncTarget },
	{ "commands", 8, ParseFuncArg,  FuncCommands }
    };
    const struct fn_def *fn;
    char *arg = NULL;
    size_t arglen;
    const char *cp = par->p;
    const struct fn_def *fns_end = fns + sizeof fns / sizeof fns[0];

    for (fn = fns; fn != fns_end; fn++) {
	if (!is_token(cp, fn->fn_name, fn->fn_name_len))
	    continue;

	cp += fn->fn_name_len;
	cpp_skip_whitespace(&cp);
	if (*cp != '(')
	    break;

	arglen = fn->fn_parse(&cp, doEval, fn->fn_name, &arg);
	if (arglen == 0 || arglen == (size_t)-1) {
	    par->p = cp;
	    *out_token = arglen == 0 ? TOK_FALSE : TOK_ERROR;
	    return TRUE;
	}

	/* Evaluate the argument using the required function. */
	*out_token = ToToken(!doEval || fn->fn_eval(arglen, arg));
	free(arg);
	par->p = cp;
	return TRUE;
    }

    return FALSE;
}

/* Parse a function call, a number, a variable expression or a string
 * literal. */
static Token
CondParser_LeafToken(CondParser *par, Boolean doEval)
{
    Token t;
    char *arg = NULL;
    size_t arglen;
    const char *cp = par->p;
    const char *cp1;

    if (CondParser_Func(par, doEval, &t))
	return t;

    /* Push anything numeric through the compare expression */
    cp = par->p;
    if (ch_isdigit(cp[0]) || cp[0] == '-' || cp[0] == '+')
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
    t = ToToken(!doEval || If_Eval(par->if_info, arg, arglen));
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
	else if (opts.lint) {
	    Parse_Error(PARSE_FATAL, "Unknown operator '|'");
	    par->printedError = TRUE;
	    return TOK_ERROR;
	}
	return TOK_OR;

    case '&':
	par->p++;
	if (par->p[0] == '&')
	    par->p++;
	else if (opts.lint) {
	    Parse_Error(PARSE_FATAL, "Unknown operator '&'");
	    par->printedError = TRUE;
	    return TOK_ERROR;
	}
	return TOK_AND;

    case '!':
	par->p++;
	return TOK_NOT;

    case '#':			/* XXX: see unit-tests/cond-token-plain.mk */
    case '\n':			/* XXX: why should this end the condition? */
				/* Probably obsolete now, from 1993-03-21. */
    case '\0':
	return TOK_EOF;

    case '"':
    case '$':
	return CondParser_Comparison(par, doEval);

    default:
	return CondParser_LeafToken(par, doEval);
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
    CondParser par;
    CondEvalResult rval;

    lhsStrict = strictLHS;

    cpp_skip_hspace(&cond);

    par.if_info = info != NULL ? info : ifs + PLAIN_IF_INDEX;
    par.p = cond;
    par.curr = TOK_NONE;
    par.printedError = FALSE;

    rval = CondParser_Eval(&par, value);

    if (rval == COND_INVALID && eprint && !par.printedError)
	Parse_Error(PARSE_FATAL, "Malformed conditional (%s)", cond);

    return rval;
}

/* Evaluate a condition in a :? modifier, such as
 * ${"${VAR}" == value:?yes:no}. */
CondEvalResult
Cond_EvalCondition(const char *cond, Boolean *out_value)
{
	return CondEvalExpression(NULL, cond, out_value, FALSE, FALSE);
}

/* Evaluate the conditional directive in the line, which is one of:
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
 *			conditional (when <cond> evaluates to TRUE)
 *	COND_SKIP	to skip the lines after the conditional
 *			(when <cond> evaluates to FALSE, or when a previous
 *			branch has already been taken)
 *	COND_INVALID	if the conditional was not valid, either because of
 *			a syntax error or because some variable was undefined
 *			or because the condition could not be evaluated
 */
CondEvalResult
Cond_EvalLine(const char *const line)
{
    typedef enum IfState {

	/* None of the previous <cond> evaluated to TRUE. */
	IFS_INITIAL	= 0,

	/* The previous <cond> evaluated to TRUE.
	 * The lines following this condition are interpreted. */
	IFS_ACTIVE	= 1 << 0,

	/* The previous directive was an '.else'. */
	IFS_SEEN_ELSE	= 1 << 1,

	/* One of the previous <cond> evaluated to TRUE. */
	IFS_WAS_ACTIVE	= 1 << 2

    } IfState;

    static enum IfState *cond_states = NULL;
    static unsigned int cond_states_cap = 128;

    const struct If *ifp;
    Boolean isElif;
    Boolean value;
    IfState state;
    const char *p = line;

    if (cond_states == NULL) {
	cond_states = bmake_malloc(cond_states_cap * sizeof *cond_states);
	cond_states[0] = IFS_ACTIVE;
    }

    p++;		/* skip the leading '.' */
    cpp_skip_hspace(&p);

    /* Parse the name of the directive, such as 'if', 'elif', 'endif'. */
    if (p[0] == 'e') {
	if (p[1] != 'l') {
	    if (!is_token(p + 1, "ndif", 4)) {
		/* Unknown directive.  It might still be a transformation
		 * rule like '.elisp.scm', therefore no error message here. */
		return COND_INVALID;
	    }

	    /* It is an '.endif'. */
	    /* TODO: check for extraneous <cond> */

	    if (cond_depth == cond_min_depth) {
		Parse_Error(PARSE_FATAL, "if-less endif");
		return COND_PARSE;
	    }

	    /* Return state for previous conditional */
	    cond_depth--;
	    return cond_states[cond_depth] & IFS_ACTIVE
		   ? COND_PARSE : COND_SKIP;
	}

	/* Quite likely this is 'else' or 'elif' */
	p += 2;
	if (is_token(p, "se", 2)) {	/* It is an 'else'. */

	    if (opts.lint && p[2] != '\0')
		Parse_Error(PARSE_FATAL,
			    "The .else directive does not take arguments.");

	    if (cond_depth == cond_min_depth) {
		Parse_Error(PARSE_FATAL, "if-less else");
		return COND_PARSE;
	    }

	    state = cond_states[cond_depth];
	    if (state == IFS_INITIAL) {
		state = IFS_ACTIVE | IFS_SEEN_ELSE;
	    } else {
		if (state & IFS_SEEN_ELSE)
		    Parse_Error(PARSE_WARNING, "extra else");
		state = IFS_WAS_ACTIVE | IFS_SEEN_ELSE;
	    }
	    cond_states[cond_depth] = state;

	    return state & IFS_ACTIVE ? COND_PARSE : COND_SKIP;
	}
	/* Assume for now it is an elif */
	isElif = TRUE;
    } else
	isElif = FALSE;

    if (p[0] != 'i' || p[1] != 'f') {
	/* Unknown directive.  It might still be a transformation rule like
	 * '.elisp.scm', therefore no error message here. */
	return COND_INVALID;	/* Not an ifxxx or elifxxx line */
    }

    /*
     * Figure out what sort of conditional it is -- what its default
     * function is, etc. -- by looking in the table of valid "ifs"
     */
    p += 2;
    for (ifp = ifs;; ifp++) {
	if (ifp->form == NULL) {
	    /* TODO: Add error message about unknown directive,
	     * since there is no other known directive that starts with 'el'
	     * or 'if'.
	     * Example: .elifx 123 */
	    return COND_INVALID;
	}
	if (is_token(p, ifp->form, ifp->formlen)) {
	    p += ifp->formlen;
	    break;
	}
    }

    /* Now we know what sort of 'if' it is... */

    if (isElif) {
	if (cond_depth == cond_min_depth) {
	    Parse_Error(PARSE_FATAL, "if-less elif");
	    return COND_PARSE;
	}
	state = cond_states[cond_depth];
	if (state & IFS_SEEN_ELSE) {
	    Parse_Error(PARSE_WARNING, "extra elif");
	    cond_states[cond_depth] = IFS_WAS_ACTIVE | IFS_SEEN_ELSE;
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
					cond_states_cap * sizeof *cond_states);
	}
	state = cond_states[cond_depth];
	cond_depth++;
	if (!(state & IFS_ACTIVE)) {
	    /* If we aren't parsing the data, treat as always false */
	    cond_states[cond_depth] = IFS_WAS_ACTIVE;
	    return COND_SKIP;
	}
    }

    /* And evaluate the conditional expression */
    if (CondEvalExpression(ifp, p, &value, TRUE, TRUE) == COND_INVALID) {
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
