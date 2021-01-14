/*	$NetBSD: for.c,v 1.134 2021/01/10 21:20:46 rillig Exp $	*/

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

/*-
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
 * body is scanned for variable expressions, and those that match the variable
 * names are replaced with expressions of the form ${:U...} or $(:U...).
 * After that, the body is treated like a file from an .include directive.
 *
 * Interface:
 *	For_Eval	Evaluate the loop in the passed line.
 *
 *	For_Run		Run accumulated loop
 */

#include "make.h"

/*	"@(#)for.c	8.1 (Berkeley) 6/6/93"	*/
MAKE_RCSID("$NetBSD: for.c,v 1.134 2021/01/10 21:20:46 rillig Exp $");

static int forLevel = 0;	/* Nesting level */

/* One of the variables to the left of the "in" in a .for loop. */
typedef struct ForVar {
	char *name;
	size_t nameLen;
} ForVar;

/*
 * State of a for loop.
 */
typedef struct For {
	Buffer body;		/* Unexpanded body of the loop */
	Vector /* of ForVar */ vars; /* Iteration variables */
	Words items;		/* Substitution items */
	Buffer curBody;		/* Expanded body of the current iteration */
	/* Is any of the names 1 character long? If so, when the variable values
	 * are substituted, the parser must handle $V expressions as well, not
	 * only ${V} and $(V). */
	Boolean short_var;
	unsigned int sub_next;	/* Where to continue iterating */
} For;

static For *accumFor;		/* Loop being accumulated */

static void
ForAddVar(For *f, const char *name, size_t len)
{
	ForVar *var = Vector_Push(&f->vars);
	var->name = bmake_strldup(name, len);
	var->nameLen = len;
}

static void
For_Free(For *f)
{
	Buf_Destroy(&f->body, TRUE);

	while (f->vars.len > 0) {
		ForVar *var = Vector_Pop(&f->vars);
		free(var->name);
	}
	Vector_Done(&f->vars);

	Words_Free(f->items);
	Buf_Destroy(&f->curBody, TRUE);

	free(f);
}

static Boolean
IsFor(const char *p)
{
	return p[0] == 'f' && p[1] == 'o' && p[2] == 'r' && ch_isspace(p[3]);
}

static Boolean
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
	For *f;
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

	/*
	 * we found a for loop, and now we are going to parse it.
	 */

	f = bmake_malloc(sizeof *f);
	Buf_Init(&f->body);
	Vector_Init(&f->vars, sizeof(ForVar));
	f->items.words = NULL;
	f->items.freeIt = NULL;
	Buf_Init(&f->curBody);
	f->short_var = FALSE;
	f->sub_next = 0;

	/* Grab the variables. Terminate on "in". */
	for (;;) {
		size_t len;

		cpp_skip_whitespace(&p);
		if (*p == '\0') {
			Parse_Error(PARSE_FATAL, "missing `in' in for");
			For_Free(f);
			return -1;
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
		if (len == 1)
			f->short_var = TRUE;

		ForAddVar(f, p, len);
		p += len;
	}

	if (f->vars.len == 0) {
		Parse_Error(PARSE_FATAL, "no iteration variables in for");
		For_Free(f);
		return -1;
	}

	cpp_skip_whitespace(&p);

	{
		char *items;
		if (Var_Subst(p, VAR_GLOBAL, VARE_WANTRES, &items) != VPR_OK) {
			Parse_Error(PARSE_FATAL, "Error in .for loop items");
			f->items.len = 0;
			goto done;
		}

		f->items = Str_Words(items, FALSE);
		free(items);

		if (f->items.len == 1 && f->items.words[0][0] == '\0')
			f->items.len = 0; /* .for var in ${:U} */
	}

	{
		size_t nitems, nvars;

		if ((nitems = f->items.len) > 0 &&
		    nitems % (nvars = f->vars.len) != 0) {
			Parse_Error(PARSE_FATAL,
			    "Wrong number of words (%u) in .for "
			    "substitution list with %u variables",
			    (unsigned)nitems, (unsigned)nvars);
			/*
			 * Return 'success' so that the body of the .for loop
			 * is accumulated.
			 * Remove all items so that the loop doesn't iterate.
			 */
			f->items.len = 0;
		}
	}

done:
	accumFor = f;
	forLevel = 1;
	return 1;
}

/*
 * Add another line to a .for loop.
 * Returns FALSE when the matching .endfor is reached.
 */
Boolean
For_Accum(const char *line)
{
	const char *ptr = line;

	if (*ptr == '.') {
		ptr++;
		cpp_skip_whitespace(&ptr);

		if (IsEndfor(ptr)) {
			DEBUG1(FOR, "For: end for %d\n", forLevel);
			if (--forLevel <= 0)
				return FALSE;
		} else if (IsFor(ptr)) {
			forLevel++;
			DEBUG1(FOR, "For: new loop %d\n", forLevel);
		}
	}

	Buf_AddStr(&accumFor->body, line);
	Buf_AddByte(&accumFor->body, '\n');
	return TRUE;
}


static size_t
for_var_len(const char *var)
{
	char ch, var_start, var_end;
	int depth;
	size_t len;

	var_start = *var;
	if (var_start == '\0')
		/* just escape the $ */
		return 0;

	if (var_start == '(')
		var_end = ')';
	else if (var_start == '{')
		var_end = '}';
	else
		return 1;	/* Single char variable */

	depth = 1;
	for (len = 1; (ch = var[len++]) != '\0';) {
		if (ch == var_start)
			depth++;
		else if (ch == var_end && --depth == 0)
			return len;
	}

	/* Variable end not found, escape the $ */
	return 0;
}

/*
 * The .for loop substitutes the items as ${:U<value>...}, which means
 * that characters that break this syntax must be backslash-escaped.
 */
static Boolean
NeedsEscapes(const char *word, char endc)
{
	const char *p;

	for (p = word; *p != '\0'; p++) {
		if (*p == ':' || *p == '$' || *p == '\\' || *p == endc)
			return TRUE;
	}
	return FALSE;
}

/*
 * While expanding the body of a .for loop, write the item in the ${:U...}
 * expression, escaping characters as needed.
 *
 * The result is later unescaped by ApplyModifier_Defined.
 */
static void
Buf_AddEscaped(Buffer *cmds, const char *item, char endc)
{
	char ch;

	if (!NeedsEscapes(item, endc)) {
		Buf_AddStr(cmds, item);
		return;
	}

	/* Escape ':', '$', '\\' and 'endc' - these will be removed later by
	 * :U processing, see ApplyModifier_Defined. */
	while ((ch = *item++) != '\0') {
		if (ch == '$') {
			size_t len = for_var_len(item);
			if (len != 0) {
				Buf_AddBytes(cmds, item - 1, len + 1);
				item += len;
				continue;
			}
			Buf_AddByte(cmds, '\\');
		} else if (ch == ':' || ch == '\\' || ch == endc)
			Buf_AddByte(cmds, '\\');
		Buf_AddByte(cmds, ch);
	}
}

/*
 * While expanding the body of a .for loop, replace the variable name of an
 * expression like ${i} or ${i:...} or $(i) or $(i:...) with ":Uvalue".
 */
static void
SubstVarLong(For *f, const char **pp, const char *bodyEnd, char endc,
	     const char **inout_mark)
{
	size_t i;
	const char *p = *pp;

	for (i = 0; i < f->vars.len; i++) {
		ForVar *forVar = Vector_Get(&f->vars, i);
		char *varname = forVar->name;
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
		    f->items.words[f->sub_next + i], endc);

		p += varnameLen;
		*inout_mark = p;
		*pp = p;
		return;
	}
}

/*
 * While expanding the body of a .for loop, replace single-character
 * variable expressions like $i with their ${:U...} expansion.
 */
static void
SubstVarShort(For *f, const char *p, const char **inout_mark)
{
	const char ch = *p;
	ForVar *vars;
	size_t i;

	/* Skip $$ and stupid ones. */
	if (!f->short_var || strchr("}):$", ch) != NULL)
		return;

	vars = Vector_Get(&f->vars, 0);
	for (i = 0; i < f->vars.len; i++) {
		const char *varname = vars[i].name;
		if (varname[0] == ch && varname[1] == '\0')
			goto found;
	}
	return;

found:
	/* Replace $<ch> with ${:U<value>} */
	Buf_AddBytesBetween(&f->curBody, *inout_mark, p), *inout_mark = p + 1;
	Buf_AddStr(&f->curBody, "{:U");
	Buf_AddEscaped(&f->curBody, f->items.words[f->sub_next + i], '}');
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
 * The detection of substitutions of the loop control variable is naive.
 * Many of the modifiers use \ to escape $ (not $) so it is possible
 * to contrive a makefile where an unwanted substitution happens.
 */
static void
ForSubstBody(For *f)
{
	const char *p, *bodyEnd;
	const char *mark;	/* where the last replacement left off */

	Buf_Empty(&f->curBody);

	mark = f->body.data;
	bodyEnd = f->body.data + f->body.len;
	for (p = mark; (p = strchr(p, '$')) != NULL;) {
		if (p[1] == '{' || p[1] == '(') {
			p += 2;
			SubstVarLong(f, &p, bodyEnd, p[-1] == '{' ? '}' : ')',
			    &mark);
		} else if (p[1] != '\0') {
			SubstVarShort(f, p + 1, &mark);
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
	For *f = v_arg;

	if (f->sub_next == f->items.len) {
		/* No more iterations */
		For_Free(f);
		return NULL;
	}

	ForSubstBody(f);
	DEBUG1(FOR, "For: loop body:\n%s", f->curBody.data);
	f->sub_next += (unsigned int)f->vars.len;

	*out_len = f->curBody.len;
	return f->curBody.data;
}

/* Run the .for loop, imitating the actions of an include file. */
void
For_Run(int lineno)
{
	For *f = accumFor;
	accumFor = NULL;

	if (f->items.len == 0) {
		/*
		 * Nothing to expand - possibly due to an earlier syntax
		 * error.
		 */
		For_Free(f);
		return;
	}

	Parse_SetInput(NULL, lineno, -1, ForReadMore, f);
}
