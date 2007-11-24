%{
/*-
 * Copyright (c) 2003-2004, Maxime Henrion <mux@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"
#include "misc.h"
#include "token.h"

#define	YY_NO_UNPUT

int lineno = 1;

%}

%option noyywrap

%%

[ \t]+			;
#.*			;
\*default		{ return DEFAULT; }
base			{ yylval.i = PT_BASE; return NAME; }
date			{ yylval.i = PT_DATE; return NAME; }
host			{ yylval.i = PT_HOST; return NAME; }
prefix			{ yylval.i = PT_PREFIX; return NAME; }
release			{ yylval.i = PT_RELEASE; return NAME; }
tag			{ yylval.i = PT_TAG; return NAME; }
umask			{ yylval.i = PT_UMASK; return NAME; }
list			{ yylval.i = PT_LIST; return NAME; }
norsync			{ yylval.i = PT_NORSYNC; return NAME; }
=			{ return EQUAL; }
compress		{ yylval.i = PT_COMPRESS; return BOOLEAN; }
delete			{ yylval.i = PT_DELETE; return BOOLEAN; }
use-rel-suffix		{ yylval.i = PT_USE_REL_SUFFIX; return BOOLEAN; }
[a-zA-Z0-9./_-]+	{
			  yylval.str = strdup(yytext);
			  if (yylval.str == NULL)
			  	err(1, "strdup");
			  return STRING;
			}
\n			lineno++;

%%

void
yyerror(const char *s)
{

	lprintf(-1, "Parse error line %d: %s: %s\n", lineno, s, yytext);
	exit(1);
}
