/*
 * Copyright (c) 1996, 1997, 1998 Shigio Yamaguchi. All rights reserved.
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
 *	This product includes software developed by Shigio Yamaguchi.
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
 *
 *	C.c					12-Sep-98
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "C.h"
#include "gctags.h"
#include "defined.h"
#include "die.h"
#include "locatestring.h"
#include "strbuf.h"
#include "token.h"

static	int	function_definition __P((int));
static	void	condition_macro __P((int));
static	int	reserved __P((char *));

/*
 * #ifdef stack.
 */
static struct {
	short start;		/* level when #if block started */
	short end;		/* level when #if block end */
	short if0only;		/* #if 0 or notdef only */
} stack[MAXPIFSTACK], *cur;
static int piflevel;		/* condition macro level */
static int level;		/* brace level */

/*
 * C: read C (includes .h, .y) file and pickup tag entries.
 */
void
C(yacc)
	int	yacc;
{
	int	c, cc;
	int	savelevel;
	int	target;
	int	startmacro, startsharp;
	const	char *interested = "{}=;";
	STRBUF	*sb = stropen();
	/*
	 * yacc file format is like the following.
	 *
	 * declarations
	 * %%
	 * rules
	 * %%
	 * programs
	 *
	 */
	int	yaccstatus = (yacc) ? DECLARATIONS : PROGRAMS;
	int	inC = (yacc) ? 0 : 1;		/* 1 while C source */

	level = piflevel = 0;
	savelevel = -1;
	target = (sflag) ? SYM : (rflag) ? REF : DEF;
	startmacro = startsharp = 0;
	cmode = 1;			/* allow token like '#xxx' */
	crflag = 1;			/* require '\n' as a token */
	if (yacc)
		ymode = 1;		/* allow token like '%xxx' */

	while ((cc = nexttoken(interested, reserved)) != EOF) {
		switch (cc) {
		case SYMBOL:		/* symbol	*/
			if (inC && peekc(0) == '('/* ) */) {
				if (isnotfunction(token)) {
					if (target == REF && defined(token))
						PUT(token, lineno, sp);
				} else if (level > 0 || startmacro) {
					if (target == REF && defined(token))
						PUT(token, lineno, sp);
				} else if (level == 0 && !startmacro && !startsharp) {
					char	savetok[MAXTOKEN], *saveline;
					int	savelineno = lineno;

					strcpy(savetok, token);
					strstart(sb);
					strnputs(sb, sp, strlen(sp) + 1);
					saveline = strvalue(sb);
					if (function_definition(target))
						if (target == DEF)
							PUT(savetok, savelineno, saveline);
				}
			} else {
				if (target == SYM)
					PUT(token, lineno, sp);
			}
			break;
		case '{':  /* } */
			DBG_PRINT(level, "{"); /* } */
			if (yaccstatus == RULES && level == 0)
				inC = 1;
			++level;
			if (bflag && atfirst) {
				if (wflag && level != 1) /* { */
					fprintf(stderr, "Warning: forced level 1 block start by '{' at column 0 [+%d %s].\n", lineno, curfile);
				level = 1;
			}
			break;
			/* { */
		case '}':
			if (--level < 0) {
				if (wflag)
					fprintf(stderr, "Warning: missing left '{' [+%d %s].\n", lineno, curfile); /* } */
				level = 0;
			}
			if (eflag && atfirst) {
				if (wflag && level != 0) /* { */
					fprintf(stderr, "Warning: forced level 0 block end by '}' at column 0 [+%d %s].\n", lineno, curfile);
				level = 0;
			}
			if (yaccstatus == RULES && level == 0)
				inC = 0;
			/* { */
			DBG_PRINT(level, "}");
			break;
		case '\n':
			if (startmacro && level != savelevel) {
				if (wflag)
					fprintf(stderr, "Warning: different level before and after #define macro. reseted. [+%d %s].\n", lineno, curfile);
				level = savelevel;
			}
			startmacro = startsharp = 0;
			break;
		case YACC_SEP:		/* %% */
			if (level != 0) {
				if (wflag)
					fprintf(stderr, "Warning: forced level 0 block end by '%%' [+%d %s].\n", lineno, curfile);
				level = 0;
			}
			if (yaccstatus == DECLARATIONS) {
				if (target == DEF)
					PUT("yyparse", lineno, sp);
				yaccstatus = RULES;
			} else if (yaccstatus == RULES)
				yaccstatus = PROGRAMS;
			inC = (yaccstatus == PROGRAMS) ? 1 : 0;
			break;
		case YACC_BEGIN:	/* %{ */
			if (level != 0) {
				if (wflag)
					fprintf(stderr, "Warning: forced level 0 block end by '%%{' [+%d %s].\n", lineno, curfile);
				level = 0;
			}
			if (inC == 1 && wflag)
				fprintf(stderr, "Warning: '%%{' appeared in C mode. [+%d %s].\n", lineno, curfile);
			inC = 1;
			break;
		case YACC_END:		/* %} */
			if (level != 0) {
				if (wflag)
					fprintf(stderr, "Warning: forced level 0 block end by '%%}' [+%d %s].\n", lineno, curfile);
				level = 0;
			}
			if (inC == 0 && wflag)
				fprintf(stderr, "Warning: '%%}' appeared in Yacc mode. [+%d %s].\n", lineno, curfile);
			inC = 0;
			break;
		/*
		 * #xxx
		 */
		case CP_DEFINE:
			startmacro = 1;
			savelevel = level;
			if ((c = nexttoken(interested, reserved)) != SYMBOL) {
				pushbacktoken();
				break;
			}
			if (peekc(1) == '('/* ) */) {
				if (target == DEF)
					PUT(token, lineno, sp);
				while ((c = nexttoken("()", reserved)) != EOF && c != '\n' && c != /* ( */ ')')
					if (c == SYMBOL && target == SYM)
						PUT(token, lineno, sp);
				if (c == '\n')
					pushbacktoken();
			}
			break;
		case CP_INCLUDE:
		case CP_ERROR:
		case CP_LINE:
		case CP_PRAGMA:
			while ((c = nexttoken(interested, reserved)) != EOF && c != '\n')
				;
			break;
		case CP_IFDEF:
		case CP_IFNDEF:
		case CP_IF:
		case CP_ELIF:
		case CP_ELSE:
		case CP_ENDIF:
		case CP_UNDEF:
			condition_macro(cc);
			while ((c = nexttoken(interested, reserved)) != EOF && c != '\n') {
				if (!((cc == CP_IF || cc == CP_ELIF) && !strcmp(token, "defined")))
					continue;
				if (c == SYMBOL && target == SYM)
					PUT(token, lineno, sp);
			}
			break;
		case CP_SHARP:		/* ## */
			(void)nexttoken(interested, reserved);
			break;
		case C_STRUCT:
			c = nexttoken(interested, reserved);
			if (c == '{' /* } */) {
				pushbacktoken();
				break;
			}
			if (c == SYMBOL)
				if (target == SYM)
					PUT(token, lineno, sp);
			break;
		case C_EXTERN:
			if (startmacro)
				break;
			while ((c = nexttoken(interested, reserved)) != EOF && c != ';') {
				switch (c) {
				case CP_IFDEF:
				case CP_IFNDEF:
				case CP_IF:
				case CP_ELIF:
				case CP_ELSE:
				case CP_ENDIF:
				case CP_UNDEF:
					condition_macro(c);
					continue;
				}
				if (startmacro && c == '\n')
					break;
				if (c == '{')
					level++;
				else if (c == '}')
					level--;
				else if (c == SYMBOL)
					if (target == SYM)
						PUT(token, lineno, sp);
			}
			break;
		/* control statement check */
		case C_BREAK:
		case C_CASE:
		case C_CONTINUE:
		case C_DEFAULT:
		case C_DO:
		case C_ELSE:
		case C_FOR:
		case C_GOTO:
		case C_IF:
		case C_RETURN:
		case C_SWITCH:
		case C_WHILE:
			if (wflag && !startmacro && level == 0)
				fprintf(stderr, "Warning: Out of function. %8s [+%d %s]\n", token, lineno, curfile);
			break;
		default:
		}
	}
	strclose(sb);
	if (wflag) {
		if (level != 0)
			fprintf(stderr, "Warning: {} block unmatched. (last at level %d.)[+%d %s]\n", level, lineno, curfile);
		if (piflevel != 0)
			fprintf(stderr, "Warning: #if block unmatched. (last at level %d.)[+%d %s]\n", piflevel, lineno, curfile);
	}
}
/*
 * function_definition: return if function definition or not.
 *
 *	r)	target type
 */
static int
function_definition(target)
int	target;
{
	int	c;
	int     brace_level, isdefine;

	brace_level = isdefine = 0;
	while ((c = nexttoken("(,)", reserved)) != EOF) {
		switch (c) {
		case CP_IFDEF:
		case CP_IFNDEF:
		case CP_IF:
		case CP_ELIF:
		case CP_ELSE:
		case CP_ENDIF:
		case CP_UNDEF:
			condition_macro(c);
			continue;
		}
		if (c == '('/* ) */)
			brace_level++;
		else if (c == /* ( */')') {
			if (--brace_level == 0)
				break;
		} else if (c == SYMBOL) {
			if (target == SYM)
				PUT(token, lineno, sp);
		}
	}
	if (c == EOF)
		return 0;
	while ((c = nexttoken(",;{}=", reserved)) != EOF) {
		switch (c) {
		case CP_IFDEF:
		case CP_IFNDEF:
		case CP_IF:
		case CP_ELIF:
		case CP_ELSE:
		case CP_ENDIF:
		case CP_UNDEF:
			condition_macro(c);
			continue;
		}
		if (c == SYMBOL || IS_RESERVED(c))
			isdefine = 1;
		else if (c == ';' || c == ',') {
			if (!isdefine)
				break;
		} else if (c == '{' /* } */) {
			pushbacktoken();
			return 1;
		} else if (c == /* { */'}') {
			break;
		} else if (c == '=')
			break;
	}
	return 0;
}

/*
 * condition_macro: 
 *
 *	i)	cc	token
 */
static void
condition_macro(cc)
	int	cc;
{
	cur = &stack[piflevel];
	if (cc == CP_IFDEF || cc == CP_IFNDEF || cc == CP_IF) {
		DBG_PRINT(piflevel, "#if");
		if (++piflevel >= MAXPIFSTACK)
			die1("#if stack over flow. [%s]", curfile);
		++cur;
		cur->start = level;
		cur->end = -1;
		cur->if0only = 0;
		if (peekc(0) == '0')
			cur->if0only = 1;
		else if ((cc = nexttoken(NULL, reserved)) == SYMBOL && !strcmp(token, "notdef"))
			cur->if0only = 1;
		else
			pushbacktoken();
	} else if (cc == CP_ELIF || cc == CP_ELSE) {
		DBG_PRINT(piflevel - 1, "#else");
		if (cur->end == -1)
			cur->end = level;
		else if (cur->end != level && wflag)
			fprintf(stderr, "Warning: uneven level. [+%d %s]\n", lineno, curfile);
		level = cur->start;
		cur->if0only = 0;
	} else if (cc == CP_ENDIF) {
		if (cur->if0only)
			level = cur->start;
		else if (cur->end != -1) {
			if (cur->end != level && wflag)
				fprintf(stderr, "Warning: uneven level. [+%d %s]\n", lineno, curfile);
			level = cur->end;
		}
		--piflevel;
		DBG_PRINT(piflevel, "#endif");
	}
}
		/* sorted by alphabet */
static struct words words[] = {
	{"##",		CP_SHARP},
	{"#define",	CP_DEFINE},
	{"#elif",	CP_ELIF},
	{"#else",	CP_ELSE},
	{"#endif",	CP_ENDIF},
	{"#error",	CP_ERROR},
	{"#if",		CP_IF},
	{"#ifdef",	CP_IFDEF},
	{"#ifndef",	CP_IFNDEF},
	{"#include",	CP_INCLUDE},
	{"#line",	CP_LINE},
	{"#pragma",	CP_PRAGMA},
	{"#undef",	CP_UNDEF},
	{"%%",		YACC_SEP},
	{"%left",	YACC_LEFT},
	{"%nonassoc",	YACC_NONASSOC},
	{"%right",	YACC_RIGHT},
	{"%start",	YACC_START},
	{"%token",	YACC_TOKEN},
	{"%type",	YACC_TYPE},
	{"%{",		YACC_BEGIN},
	{"%}",		YACC_END},
	{"__P",		C___P},
	{"auto",	C_AUTO},
	{"break",	C_BREAK},
	{"case",	C_CASE},
	{"char",	C_CHAR},
	{"continue",	C_CONTINUE},
	{"default",	C_DEFAULT},
	{"do",		C_DO},
	{"double",	C_DOUBLE},
	{"else",	C_ELSE},
	{"extern",	C_EXTERN},
	{"float",	C_FLOAT},
	{"for",		C_FOR},
	{"goto",	C_GOTO},
	{"if",		C_IF},
	{"int",		C_INT},
	{"long",	C_LONG},
	{"register",	C_REGISTER},
	{"return",	C_RETURN},
	{"short",	C_SHORT},
	{"sizeof",	C_SIZEOF},
	{"static",	C_STATIC},
	{"struct",	C_STRUCT},
	{"switch",	C_SWITCH},
	{"typedef",	C_TYPEDEF},
	{"union",	C_UNION},
	{"unsigned",	C_UNSIGNED},
	{"void",	C_VOID},
	{"while",	C_WHILE},
};

static int
reserved(word)
        char *word;
{
	struct words tmp;
	struct words *result;

	tmp.name = word;
	result = (struct words *)bsearch(&tmp, words, sizeof(words)/sizeof(struct words), sizeof(struct words), cmp);
	return (result != NULL) ? result->val : SYMBOL;
}
