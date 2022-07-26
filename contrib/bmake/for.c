/*	$NetBSD: for.c,v 1.168 2022/06/12 16:09:21 rillig Exp $	*/

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
MAKE_RCSID("$NetBSD: for.c,v 1.168 2022/06/12 16:09:21 rillig Exp $");


typedef struct ForLoop {
	Vector /* of 'char *' */ vars; /* Iteration variables */
	SubstringWords items;	/* Substitution items */
	Buffer body;		/* Unexpanded body of the loop */
	unsigned int nextItem;	/* Where to continue iterating */
} ForLoop;


static ForLoop *accumFor;	/* Loop being accumulated */


static ForLoop *
ForLoop_New(void)
{
	ForLoop *f = bmake_malloc(sizeof *f);

	Vector_Init(&f->vars, sizeof(char *));
	SubstringWords_Init(&f->items);
	Buf_Init(&f->body);
	f->nextItem = 0;

	return f;
}

void
ForLoop_Free(ForLoop *f)
{
	while (f->vars.len > 0)
		free(*(char **)Vector_Pop(&f->vars));
	Vector_Done(&f->vars);

	SubstringWords_Free(f->items);
	Buf_Done(&f->body);

	free(f);
}

char *
ForLoop_Details(ForLoop *f)
{
	size_t i, n;
	const char **vars;
	const Substring *items;
	Buffer buf;

	n = f->vars.len;
	vars = f->vars.items;
	assert(f->nextItem >= n);
	items = f->items.words + f->nextItem - n;

	Buf_Init(&buf);
	for (i = 0; i < n; i++) {
		if (i > 0)
			Buf_AddStr(&buf, ", ");
		Buf_AddStr(&buf, vars[i]);
		Buf_AddStr(&buf, " = ");
		Buf_AddBytesBetween(&buf, items[i].start, items[i].end);
	}
	return Buf_DoneData(&buf);
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

		*(char **)Vector_Push(&f->vars) = bmake_strldup(p, len);
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
		f->items.len = 0;	/* .for var in ${:U} */

	if (f->items.len % f->vars.len != 0) {
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
 * Results:
 *	0	not a .for directive
 *	1	found a .for directive
 *	-1	erroneous .for directive
 */
int
For_Eval(const char *line)
{
	const char *p;
	ForLoop *f;

	p = line + 1;		/* skip the '.' */
	cpp_skip_whitespace(&p);

	if (IsFor(p)) {
		p += 3;

		f = ForLoop_New();
		if (!ForLoop_ParseVarnames(f, &p)) {
			ForLoop_Free(f);
			return -1;
		}
		if (!ForLoop_ParseItems(f, p))
			f->items.len = 0;	/* don't iterate */

		accumFor = f;
		return 1;
	} else if (IsEndfor(p)) {
		Parse_Error(PARSE_FATAL, "for-less endfor");
		return -1;
	} else
		return 0;
}

/*
 * Add another line to the .for loop that is being built up.
 * Returns false when the matching .endfor is reached.
 */
bool
For_Accum(const char *line, int *forLevel)
{
	const char *p = line;

	if (*p == '.') {
		p++;
		cpp_skip_whitespace(&p);

		if (IsEndfor(p)) {
			DEBUG1(FOR, "For: end for %d\n", *forLevel);
			if (--*forLevel == 0)
				return false;
		} else if (IsFor(p)) {
			(*forLevel)++;
			DEBUG1(FOR, "For: new loop %d\n", *forLevel);
		}
	}

	Buf_AddStr(&accumFor->body, line);
	Buf_AddByte(&accumFor->body, '\n');
	return true;
}

/*
 * When the body of a '.for i' loop is prepared for an iteration, each
 * occurrence of $i in the body is replaced with ${:U...}, inserting the
 * value of the item.  If this item contains a '$', it may be the start of a
 * variable expression.  This expression is copied verbatim, its length is
 * determined here, in a rather naive way, ignoring escape characters and
 * funny delimiters in modifiers like ':S}from}to}'.
 */
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
 * expression, escaping characters as needed.  The result is later unescaped
 * by ApplyModifier_Defined.
 */
static void
AddEscaped(Buffer *cmds, Substring item, char endc)
{
	const char *p;
	char ch;

	if (!NeedsEscapes(item, endc)) {
		Buf_AddBytesBetween(cmds, item.start, item.end);
		return;
	}

	for (p = item.start; p != item.end;) {
		ch = *p;
		if (ch == '$') {
			size_t len = ExprLen(p + 1, item.end);
			if (len != 0) {
				/*
				 * XXX: Should a '\' be added here?
				 * See directive-for-escape.mk, ExprLen.
				 */
				Buf_AddBytes(cmds, p, 1 + len);
				p += 1 + len;
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
		p++;
	}
}

/*
 * When expanding the body of a .for loop, replace the variable name of an
 * expression like ${i} or ${i:...} or $(i) or $(i:...) with ":Uvalue".
 */
static void
ForLoop_SubstVarLong(ForLoop *f, unsigned int firstItem, Buffer *body,
		     const char **pp, char endc, const char **inout_mark)
{
	size_t i;
	const char *start = *pp;
	const char **vars = Vector_Get(&f->vars, 0);

	for (i = 0; i < f->vars.len; i++) {
		const char *p = start;

		if (!cpp_skip_string(&p, vars[i]))
			continue;
		/* XXX: why test for backslash here? */
		if (*p != ':' && *p != endc && *p != '\\')
			continue;

		/*
		 * Found a variable match.  Skip over the variable name and
		 * instead add ':U<value>' to the current body.
		 */
		Buf_AddBytesBetween(body, *inout_mark, start);
		Buf_AddStr(body, ":U");
		AddEscaped(body, f->items.words[firstItem + i], endc);

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
ForLoop_SubstVarShort(ForLoop *f, unsigned int firstItem, Buffer *body,
		      const char *p, const char **inout_mark)
{
	const char ch = *p;
	const char **vars;
	size_t i;

	/* Skip $$ and stupid ones. */
	if (ch == '}' || ch == ')' || ch == ':' || ch == '$')
		return;

	vars = Vector_Get(&f->vars, 0);
	for (i = 0; i < f->vars.len; i++) {
		const char *varname = vars[i];
		if (varname[0] == ch && varname[1] == '\0')
			goto found;
	}
	return;

found:
	Buf_AddBytesBetween(body, *inout_mark, p);
	*inout_mark = p + 1;

	/* Replace $<ch> with ${:U<value>} */
	Buf_AddStr(body, "{:U");
	AddEscaped(body, f->items.words[firstItem + i], '}');
	Buf_AddByte(body, '}');
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
ForLoop_SubstBody(ForLoop *f, unsigned int firstItem, Buffer *body)
{
	const char *p, *end;
	const char *mark;	/* where the last substitution left off */

	Buf_Clear(body);

	mark = f->body.data;
	end = f->body.data + f->body.len;
	for (p = mark; (p = strchr(p, '$')) != NULL;) {
		if (p[1] == '{' || p[1] == '(') {
			char endc = p[1] == '{' ? '}' : ')';
			p += 2;
			ForLoop_SubstVarLong(f, firstItem, body,
			    &p, endc, &mark);
		} else if (p[1] != '\0') {
			ForLoop_SubstVarShort(f, firstItem, body,
			    p + 1, &mark);
			p += 2;
		} else
			break;
	}

	Buf_AddBytesBetween(body, mark, end);
}

/*
 * Compute the body for the current iteration by copying the unexpanded body,
 * replacing the expressions for the iteration variables on the way.
 */
bool
For_NextIteration(ForLoop *f, Buffer *body)
{
	if (f->nextItem == f->items.len)
		return false;

	f->nextItem += (unsigned int)f->vars.len;
	ForLoop_SubstBody(f, f->nextItem - (unsigned int)f->vars.len, body);
	DEBUG1(FOR, "For: loop body:\n%s", body->data);
	return true;
}

/* Run the .for loop, imitating the actions of an include file. */
void
For_Run(unsigned headLineno, unsigned bodyReadLines)
{
	Buffer buf;
	ForLoop *f = accumFor;
	accumFor = NULL;

	if (f->items.len > 0) {
		Buf_Init(&buf);
		Parse_PushInput(NULL, headLineno, bodyReadLines, buf, f);
	} else
		ForLoop_Free(f);
}
