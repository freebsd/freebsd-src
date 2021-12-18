/*	$NetBSD: for.c,v 1.150 2021/12/12 15:44:41 rillig Exp $	*/

/*
 * Copyright (c) 1992, The Regents of the University of California.
 * All rights reserved.
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
 * Handling of .for/.endfor loops in a makefile.
 *
 * For loops have the form:
 *
 *	.for <varname...> in <value...>
 *	# the body
 *	.endfor
 *
 * When a .for line is parsed, the following lines are copied to the body of
 * the .for loop, until the corresponding .endfor line is reached.  In this
 * phase, the body is not yet evaluated.  This also applies to any nested
 * .for loops.
 *
 * After reaching the .endfor, the values from the .for line are grouped
 * according to the number of variables.  For each such group, the unexpanded
 * body is scanned for variable expressions, and those that match the
 * variable names are replaced with expressions of the form ${:U...}.  After
 * that, the body is treated like a file from an .include directive.
 *
 * Interface:
 *	For_Eval	Evaluate the loop in the passed line.
 *
 *	For_Run		Run accumulated loop
 */

#include "make.h"

/*	"@(#)for.c	8.1 (Berkeley) 6/6/93"	*/
MAKE_RCSID("$NetBSD: for.c,v 1.150 2021/12/12 15:44:41 rillig Exp $");


/* One of the variables to the left of the "in" in a .for loop. */
typedef struct ForVar {
	char *name;
	size_t nameLen;
} ForVar;

typedef struct ForLoop {
	Buffer body;		/* Unexpanded body of the loop */
	Vector /* of ForVar */ vars; /* Iteration variables */
	SubstringWords items;	/* Substitution items */
	Buffer curBody;		/* Expanded body of the current iteration */
	unsigned int nextItem;	/* Where to continue iterating */
} ForLoop;


static ForLoop *accumFor;	/* Loop being accumulated */
static int forLevel = 0;	/* Nesting level */


static ForLoop *
ForLoop_New(void)
{
	ForLoop *f = bmake_malloc(sizeof *f);

	Buf_Init(&f->body);
	Vector_Init(&f->vars, sizeof(ForVar));
	SubstringWords_Init(&f->items);
	Buf_Init(&f->curBody);
	f->nextItem = 0;

	return f;
}

static void
ForLoop_Free(ForLoop *f)
{
	Buf_Done(&f->body);

	while (f->vars.len > 0) {
		ForVar *var = Vector_Pop(&f->vars);
		free(var->name);
	}
	Vector_Done(&f->vars);

	SubstringWords_Free(f->items);
	Buf_Done(&f->curBody);

	free(f);
}

static void
ForLoop_AddVar(ForLoop *f, const char *name, size_t len)
{
	ForVar *var = Vector_Push(&f->vars);
	var->name = bmake_strldup(name, len);
	var->nameLen = len;
}

static bool
ForLoop_ParseVarnames(ForLoop *f, const char **pp)
{
	const char *p = *pp;

	for (;;) {
		size_t len;

		cpp_skip_whitespace(&p);
		if (*p == '\0') {
			Parse_Error(PARSE_FATAL, "missing `in' in for");
			return false;
		}

		/*
		 * XXX: This allows arbitrary variable names;
		 * see directive-for.mk.
		 */
		for (len = 1; p[len] != '\0' && !ch_isspace(p[len]); len++)
			continue;

		if (len == 2 && p[0] == 'i' && p[1] == 'n') {
			p += 2;
			break;
		}

		ForLoop_AddVar(f, p, len);
		p += len;
	}

	if (f->vars.len == 0) {
		Parse_Error(PARSE_FATAL, "no iteration variables in for");
		return false;
	}

	*pp = p;
	return true;
}

static bool
ForLoop_ParseItems(ForLoop *f, const char *p)
{
	char *items;

	cpp_skip_whitespace(&p);

	if (Var_Subst(p, SCOPE_GLOBAL, VARE_WANTRES, &items) != VPR_OK) {
		Parse_Error(PARSE_FATAL, "Error in .for loop items");
		return false;
	}

	f->items = Substring_Words(items, false);
	free(items);

	if (f->items.len == 1 && Substring_IsEmpty(f->items.words[0]))
		f->items.len = 0; /* .for var in ${:U} */

	if (f->items.len != 0 && f->items.len % f->vars.len != 0) {
		Parse_Error(PARSE_FATAL,
		    "Wrong number of words (%u) in .for "
		    "substitution list with %u variables",
		    (unsigned)f->items.len, (unsigned)f->vars.len);
		return false;
	}

	return true;
}

static bool
IsFor(const char *p)
{
	return p[0] == 'f' && p[1] == 'o' && p[2] == 'r' && ch_isspace(p[3]);
}

static bool
IsEndfor(const char *p)
{
	return p[0] == 'e' && strncmp(p, "endfor", 6) == 0 &&
	       (p[6] == '\0' || ch_isspace(p[6]));
}

/*
 * Evaluate the for loop in the passed line. The line looks like this:
 *	.for <varname...> in <value...>
 *
 * Input:
 *	line		Line to parse
 *
 * Results:
 *      0: Not a .for statement, parse the line
 *	1: We found a for loop
 *     -1: A .for statement with a bad syntax error, discard.
 */
int
For_Eval(const char *line)
{
	ForLoop *f;
	const char *p;

	p = line + 1;		/* skip the '.' */
	cpp_skip_whitespace(&p);

	if (!IsFor(p)) {
		if (IsEndfor(p)) {
			Parse_Error(PARSE_FATAL, "for-less endfor");
			return -1;
		}
		return 0;
	}
	p += 3;

	f = ForLoop_New();

	if (!ForLoop_ParseVarnames(f, &p)) {
		ForLoop_Free(f);
		return -1;
	}

	if (!ForLoop_ParseItems(f, p)) {
		/* Continue parsing the .for loop, but don't iterate. */
		f->items.len = 0;
	}

	accumFor = f;
	forLevel = 1;
	return 1;
}

/*
 * Add another line to the .for loop that is being built up.
 * Returns false when the matching .endfor is reached.
 */
bool
For_Accum(const char *line)
{
	const char *p = line;

	if (*p == '.') {
		p++;
		cpp_skip_whitespace(&p);

		if (IsEndfor(p)) {
			DEBUG1(FOR, "For: end for %d\n", forLevel);
			if (--forLevel <= 0)
				return false;
		} else if (IsFor(p)) {
			forLevel++;
			DEBUG1(FOR, "For: new loop %d\n", forLevel);
		}
	}

	Buf_AddStr(&accumFor->body, line);
	Buf_AddByte(&accumFor->body, '\n');
	return true;
}


static size_t
ExprLen(const char *s, const char *e)
{
	char expr_open, expr_close;
	int depth;
	const char *p;

	if (s == e)
		return 0;	/* just escape the '$' */

	expr_open = s[0];
	if (expr_open == '(')
		expr_close = ')';
	else if (expr_open == '{')
		expr_close = '}';
	else
		return 1;	/* Single char variable */

	depth = 1;
	for (p = s + 1; p != e; p++) {
		if (*p == expr_open)
			depth++;
		else if (*p == expr_close && --depth == 0)
			return (size_t)(p + 1 - s);
	}

	/* Expression end not found, escape the $ */
	return 0;
}

/*
 * The .for loop substitutes the items as ${:U<value>...}, which means
 * that characters that break this syntax must be backslash-escaped.
 */
static bool
NeedsEscapes(Substring value, char endc)
{
	const char *p;

	for (p = value.start; p != value.end; p++) {
		if (*p == ':' || *p == '$' || *p == '\\' || *p == endc ||
		    *p == '\n')
			return true;
	}
	return false;
}

/*
 * While expanding the body of a .for loop, write the item in the ${:U...}
 * expression, escaping characters as needed.
 *
 * The result is later unescaped by ApplyModifier_Defined.
 */
static void
Buf_AddEscaped(Buffer *cmds, Substring item, char endc)
{
	const char *p;
	char ch;

	if (!NeedsEscapes(item, endc)) {
		Buf_AddBytesBetween(cmds, item.start, item.end);
		return;
	}

	/* Escape ':', '$', '\\' and 'endc' - these will be removed later by
	 * :U processing, see ApplyModifier_Defined. */
	for (p = item.start; p != item.end; p++) {
		ch = *p;
		if (ch == '$') {
			size_t len = ExprLen(p + 1, item.end);
			if (len != 0) {
				/*
				 * XXX: Should a '\' be added here?
				 * See directive-for-escape.mk, ExprLen.
				 */
				Buf_AddBytes(cmds, p, 1 + len);
				p += len;
				continue;
			}
			Buf_AddByte(cmds, '\\');
		} else if (ch == ':' || ch == '\\' || ch == endc)
			Buf_AddByte(cmds, '\\');
		else if (ch == '\n') {
			Parse_Error(PARSE_FATAL, "newline in .for value");
			ch = ' ';	/* prevent newline injection */
		}
		Buf_AddByte(cmds, ch);
	}
}

/*
 * When expanding the body of a .for loop, replace the variable name of an
 * expression like ${i} or ${i:...} or $(i) or $(i:...) with ":Uvalue".
 */
static void
ForLoop_SubstVarLong(ForLoop *f, const char **pp, const char *bodyEnd,
		     char endc, const char **inout_mark)
{
	size_t i;
	const char *p = *pp;

	for (i = 0; i < f->vars.len; i++) {
		const ForVar *forVar = Vector_Get(&f->vars, i);
		const char *varname = forVar->name;
		size_t varnameLen = forVar->nameLen;

		if (varnameLen >= (size_t)(bodyEnd - p))
			continue;
		if (memcmp(p, varname, varnameLen) != 0)
			continue;
		/* XXX: why test for backslash here? */
		if (p[varnameLen] != ':' && p[varnameLen] != endc &&
		    p[varnameLen] != '\\')
			continue;

		/*
		 * Found a variable match.  Skip over the variable name and
		 * instead add ':U<value>' to the current body.
		 */
		Buf_AddBytesBetween(&f->curBody, *inout_mark, p);
		Buf_AddStr(&f->curBody, ":U");
		Buf_AddEscaped(&f->curBody,
		    f->items.words[f->nextItem + i], endc);

		p += varnameLen;
		*inout_mark = p;
		*pp = p;
		return;
	}
}

/*
 * When expanding the body of a .for loop, replace single-character
 * variable expressions like $i with their ${:U...} expansion.
 */
static void
ForLoop_SubstVarShort(ForLoop *f, const char *p, const char **inout_mark)
{
	const char ch = *p;
	const ForVar *vars;
	size_t i;

	/* Skip $$ and stupid ones. */
	if (ch == '}' || ch == ')' || ch == ':' || ch == '$')
		return;

	vars = Vector_Get(&f->vars, 0);
	for (i = 0; i < f->vars.len; i++) {
		const char *varname = vars[i].name;
		if (varname[0] == ch && varname[1] == '\0')
			goto found;
	}
	return;

found:
	Buf_AddBytesBetween(&f->curBody, *inout_mark, p);
	*inout_mark = p + 1;

	/* Replace $<ch> with ${:U<value>} */
	Buf_AddStr(&f->curBody, "{:U");
	Buf_AddEscaped(&f->curBody, f->items.words[f->nextItem + i], '}');
	Buf_AddByte(&f->curBody, '}');
}

/*
 * Compute the body for the current iteration by copying the unexpanded body,
 * replacing the expressions for the iteration variables on the way.
 *
 * Using variable expressions ensures that the .for loop can't generate
 * syntax, and that the later parsing will still see a variable.
 * This code assumes that the variable with the empty name will never be
 * defined, see unit-tests/varname-empty.mk for more details.
 *
 * The detection of substitutions of the loop control variables is naive.
 * Many of the modifiers use '\$' instead of '$$' to escape '$', so it is
 * possible to contrive a makefile where an unwanted substitution happens.
 */
static void
ForLoop_SubstBody(ForLoop *f)
{
	const char *p, *bodyEnd;
	const char *mark;	/* where the last substitution left off */

	Buf_Empty(&f->curBody);

	mark = f->body.data;
	bodyEnd = f->body.data + f->body.len;
	for (p = mark; (p = strchr(p, '$')) != NULL;) {
		if (p[1] == '{' || p[1] == '(') {
			char endc = p[1] == '{' ? '}' : ')';
			p += 2;
			ForLoop_SubstVarLong(f, &p, bodyEnd, endc, &mark);
		} else if (p[1] != '\0') {
			ForLoop_SubstVarShort(f, p + 1, &mark);
			p += 2;
		} else
			break;
	}

	Buf_AddBytesBetween(&f->curBody, mark, bodyEnd);
}

/*
 * Compute the body for the current iteration by copying the unexpanded body,
 * replacing the expressions for the iteration variables on the way.
 */
static char *
ForReadMore(void *v_arg, size_t *out_len)
{
	ForLoop *f = v_arg;

	if (f->nextItem == f->items.len) {
		/* No more iterations */
		ForLoop_Free(f);
		return NULL;
	}

	ForLoop_SubstBody(f);
	DEBUG1(FOR, "For: loop body:\n%s", f->curBody.data);
	f->nextItem += (unsigned int)f->vars.len;

	*out_len = f->curBody.len;
	return f->curBody.data;
}

/* Run the .for loop, imitating the actions of an include file. */
void
For_Run(int lineno)
{
	ForLoop *f = accumFor;
	accumFor = NULL;

	if (f->items.len == 0) {
		/*
		 * Nothing to expand - possibly due to an earlier syntax
		 * error.
		 */
		ForLoop_Free(f);
		return;
	}

	Parse_PushInput(NULL, lineno, -1, ForReadMore, f);
}
