%{
/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#if 0
#ifndef lint
static char sccsid[] = "@(#)arith.y	8.3 (Berkeley) 5/4/95";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <limits.h>
#include <stdio.h>

#include "arith.h"
#include "shell.h"
#include "var.h"
%}
%union {
	arith_t l_value;
	char* s_value;
}
%token <l_value> ARITH_NUM ARITH_LPAREN ARITH_RPAREN
%token <s_value> ARITH_VAR

%type <l_value>	expr
%right ARITH_ASSIGN
%right ARITH_ADDASSIGN ARITH_SUBASSIGN
%right ARITH_MULASSIGN ARITH_DIVASSIGN ARITH_REMASSIGN
%right ARITH_RSHASSIGN ARITH_LSHASSIGN
%right ARITH_BANDASSIGN ARITH_BXORASSIGN ARITH_BORASSIGN
%left ARITH_OR
%left ARITH_AND
%left ARITH_BOR
%left ARITH_BXOR
%left ARITH_BAND
%left ARITH_EQ ARITH_NE
%left ARITH_LT ARITH_GT ARITH_GE ARITH_LE
%left ARITH_LSHIFT ARITH_RSHIFT
%left ARITH_ADD ARITH_SUB
%left ARITH_MUL ARITH_DIV ARITH_REM
%left ARITH_UNARYMINUS ARITH_UNARYPLUS ARITH_NOT ARITH_BNOT
%%

exp:
	expr
		{
		*YYPARSE_PARAM = $1;
		return (0);
		}
	;

expr:
	ARITH_LPAREN expr ARITH_RPAREN
		{ $$ = $2; } |
	expr ARITH_OR expr
		{ $$ = $1 ? $1 : $3 ? $3 : 0; } |
	expr ARITH_AND expr
		{ $$ = $1 ? ( $3 ? $3 : 0 ) : 0; } |
	expr ARITH_BOR expr
		{ $$ = $1 | $3; } |
	expr ARITH_BXOR expr
		{ $$ = $1 ^ $3; } |
	expr ARITH_BAND expr
		{ $$ = $1 & $3; } |
	expr ARITH_EQ expr
		{ $$ = $1 == $3; } |
	expr ARITH_GT expr
		{ $$ = $1 > $3; } |
	expr ARITH_GE expr
		{ $$ = $1 >= $3; } |
	expr ARITH_LT expr
		{ $$ = $1 < $3; } |
	expr ARITH_LE expr
		{ $$ = $1 <= $3; } |
	expr ARITH_NE expr
		{ $$ = $1 != $3; } |
	expr ARITH_LSHIFT expr
		{ $$ = $1 << $3; } |
	expr ARITH_RSHIFT expr
		{ $$ = $1 >> $3; } |
	expr ARITH_ADD expr
		{ $$ = $1 + $3; } |
	expr ARITH_SUB expr
		{ $$ = $1 - $3; } |
	expr ARITH_MUL expr
		{ $$ = $1 * $3; } |
	expr ARITH_DIV expr
		{
		if ($3 == 0)
			yyerror("division by zero");
		$$ = $1 / $3;
		} |
	expr ARITH_REM expr
		{
		if ($3 == 0)
			yyerror("division by zero");
		$$ = $1 % $3;
		} |
	ARITH_NOT expr
		{ $$ = !($2); } |
	ARITH_BNOT expr
		{ $$ = ~($2); } |
	ARITH_SUB expr %prec ARITH_UNARYMINUS
		{ $$ = -($2); } |
	ARITH_ADD expr %prec ARITH_UNARYPLUS
		{ $$ = $2; } |
	ARITH_NUM |
	ARITH_VAR
		{
		char *p;
		arith_t arith_val;
		char *str_val;

		if (lookupvar($1) == NULL)
			setvarsafe($1, "0", 0);
		str_val = lookupvar($1);
		arith_val = strtoarith_t(str_val, &p, 0);
		/*
		 * Conversion is successful only in case
		 * we've converted _all_ characters.
		 */
		if (*p != '\0')
			yyerror("variable conversion error");
		$$ = arith_val;
		} |
	ARITH_VAR ARITH_ASSIGN expr
		{
		if (arith_assign($1, $3) != 0)
			yyerror("variable assignment error");
		$$ = $3;
		} |
	ARITH_VAR ARITH_ADDASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) + $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_SUBASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) - $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_MULASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) * $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_DIVASSIGN expr
		{
		arith_t value;

		if ($3 == 0)
			yyerror("division by zero");

		value = atoarith_t(lookupvar($1)) / $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_REMASSIGN expr
		{
		arith_t value;

		if ($3 == 0)
			yyerror("division by zero");

		value = atoarith_t(lookupvar($1)) % $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_RSHASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) >> $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_LSHASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) << $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_BANDASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) & $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_BXORASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) ^ $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} |
	ARITH_VAR ARITH_BORASSIGN expr
		{
		arith_t value;

		value = atoarith_t(lookupvar($1)) | $3;
		if (arith_assign($1, value) != 0)
			yyerror("variable assignment error");
		$$ = value;
		} ;
%%
#include "error.h"
#include "output.h"
#include "memalloc.h"

#define YYPARSE_PARAM_TYPE arith_t *
#define YYPARSE_PARAM result

const char *arith_buf, *arith_startbuf;

int yylex(void);
int yyparse(YYPARSE_PARAM_TYPE);

static int
arith_assign(char *name, arith_t value)
{
	char *str;
	int ret;

	str = (char *)ckmalloc(DIGITS(value));
	sprintf(str, ARITH_FORMAT_STR, value);
	ret = setvarsafe(name, str, 0);
	free(str);
	return ret;
}

arith_t
arith(const char *s)
{
	arith_t result;

	arith_buf = arith_startbuf = s;

	INTOFF;
	yyparse(&result);
	arith_lex_reset();	/* Reprime lex. */
	INTON;

	return result;
}

static void
yyerror(const char *s)
{

	yyerrok;
	yyclearin;
	arith_lex_reset();	/* Reprime lex. */
	error("arithmetic expression: %s: \"%s\"", s, arith_startbuf);
}

/*
 *  The exp(1) builtin.
 */
int
expcmd(int argc, char **argv)
{
	const char *p;
	char *concat;
	char **ap;
	arith_t i;

	if (argc > 1) {
		p = argv[1];
		if (argc > 2) {
			/*
			 * Concatenate arguments.
			 */
			STARTSTACKSTR(concat);
			ap = argv + 2;
			for (;;) {
				while (*p)
					STPUTC(*p++, concat);
				if ((p = *ap++) == NULL)
					break;
				STPUTC(' ', concat);
			}
			STPUTC('\0', concat);
			p = grabstackstr(concat);
		}
	} else
		p = "";

	i = arith(p);

	out1fmt(ARITH_FORMAT_STR "\n", i);
	return !i;
}

/*************************/
#ifdef TEST_ARITH
#include <stdio.h>
main(int argc, char *argv[])
{
	printf("%d\n", exp(argv[1]));
}

error(const char *s)
{
	fprintf(stderr, "exp: %s\n", s);
	exit(1);
}
#endif
