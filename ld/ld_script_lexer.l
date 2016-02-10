%{
/*-
 * Copyright (c) 2010-2013 Kai Wang
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ld.h"
#include "ld_script_parser.h"

ELFTC_VCSID("$Id: ld_script_lexer.l 3385 2016-01-31 14:26:26Z jkoshy $");

#define	YY_NO_UNPUT
int lineno = 1;

int	yylex(void);

static void	_calc_num(void);
static void	_get_string(void);
static void	_get_ident(void);
static void	_skip_comment(void);

%}

DEC		[0-9]+[kKmMdD]?
OCT		(0[0-7]*+[kKmM]?)|([0-7]+[oO])
HEX		(0[xX][0-9a-fA-F]+[kKmMhH]?)|([0-9a-fA-F]+[hH])
IDENT		[-_A-Za-z/:$.\\~][-_A-Za-z/:$.\\~0-9]*
WILDCARD	[-_A-Za-z/:$.\\~0-9\[\]?*]+
MATTR		\(!?[rRwWxXaAiIlL]+\)

%option noyywrap
%option never-interactive
%option nounput

%%

ABSOLUTE			{ return (T_ABSOLUTE); }
ADDR				{ return (T_ADDR); }
ALIGN				{ return (T_ALIGN); }
ALIGNOF				{ return (T_ALIGNOF); }
ASSERT				{ return (T_ASSERT); }
AS_NEEDED			{ return (T_AS_NEEDED); }
AT				{ return (T_AT); }
BIND				{ return (T_BIND); }
BLOCK				{ return (T_BLOCK); }
BYTE				{ return (T_BYTE); }
COMMONPAGESIZE			{ _get_ident(); return (T_COMMONPAGESIZE); }
CONSTANT			{ return (T_CONSTANT); }
CONSTRUCTORS			{ return (T_CONSTRUCTORS); }
COPY				{ return (T_COPY); }
CREATE_OBJECT_SYMBOLS		{ return (T_CREATE_OBJECT_SYMBOLS); }
DATA_SEGMENT_ALIGN		{ return (T_DATA_SEGMENT_ALIGN); }
DATA_SEGMENT_END		{ return (T_DATA_SEGMENT_END); }
DATA_SEGMENT_RELRO_END		{ return (T_DATA_SEGMENT_RELRO_END); }
DEFINED				{ return (T_DEFINED); }
DSECT				{ return (T_DSECT); }
ENTRY				{ return (T_ENTRY); }
EXCLUDE_FILE			{ return (T_EXCLUDE_FILE); }
EXTERN				{ return (T_EXTERN); }
FILEHDR				{ return (T_FILEHDR); }
FILL				{ return (T_FILL); }
FLAGS				{ return (T_FLAGS); }
FLOAT				{ return (T_FLOAT); }
FORCE_COMMON_ALLOCATION		{ return (T_FORCE_COMMON_ALLOCATION); }
GROUP				{ return (T_GROUP); }
HLL				{ return (T_HLL); }
INCLUDE				{ return (T_INCLUDE); }
INFO				{ return (T_INFO); }
INHIBIT_COMMON_ALLOCATION	{ return (T_INHIBIT_COMMON_ALLOCATION); }
INPUT				{ return (T_INPUT); }
KEEP				{ return (T_KEEP); }
LENGTH				{ return (T_LENGTH); }
LOADADDR			{ return (T_LOADADDR); }
LONG				{ return (T_LONG); }
MAP				{ return (T_MAP); }
MAX				{ return (T_MAX); }
MAXPAGESIZE			{ _get_ident(); return (T_MAXPAGESIZE); }
MEMORY				{ return (T_MEMORY); }
MIN				{ return (T_MIN); }
NEXT				{ return (T_NEXT); }
NOCROSSREFS			{ return (T_NOCROSSREFS); }
NOFLOAT				{ return (T_NOFLOAT); }
NOLOAD				{ return (T_NOLOAD); }
ONLY_IF_RO			{ return (T_ONLY_IF_RO); }
ONLY_IF_RW			{ return (T_ONLY_IF_RW); }
OPTION				{ return (T_OPTION); }
ORIGIN				{ return (T_ORIGIN); }
OUTPUT				{ return (T_OUTPUT); }
OUTPUT_ARCH			{ return (T_OUTPUT_ARCH); }
OUTPUT_FORMAT			{ return (T_OUTPUT_FORMAT); }
OVERLAY				{ return (T_OVERLAY); }
PHDRS				{ return (T_PHDRS); }
PROVIDE				{ return (T_PROVIDE); }
PROVIDE_HIDDEN			{ return (T_PROVIDE_HIDDEN); }
QUAD				{ return (T_QUAD); }
REGION_ALIAS			{ return (T_REGION_ALIAS); }
SEARCH_DIR			{ return (T_SEARCH_DIR); }
SECTIONS			{ return (T_SECTIONS); }
SEGMENT_START			{ return (T_SEGMENT_START); }
SHORT				{ return (T_SHORT); }
SIZEOF				{ return (T_SIZEOF); }
SIZEOF_HEADERS			{ return (T_SIZEOF_HEADERS); }
SORT				{ return (T_SORT_BY_NAME); }
SORT_BY_ALIGNMENT		{ return (T_SORT_BY_ALIGNMENT); }
SORT_BY_NAME			{ return (T_SORT_BY_NAME); }
SPECIAL				{ return (T_SPECIAL); }
SQUAD				{ return (T_SQUAD); }
STARTUP				{ return (T_STARTUP); }
SUBALIGN			{ return (T_SUBALIGN); }
SYSLIB				{ return (T_SYSLIB); }
TARGET				{ return (T_TARGET); }
TRUNCATE			{ return (T_TRUNCATE); }
extern				{ return (T_VER_EXTERN); }
global:				{ return (T_VER_GLOBAL); }
l				{ return (T_LENGTH); }
len				{ return (T_LENGTH); }
local:				{ return (T_VER_LOCAL); }
o				{ return (T_ORIGIN); }
org				{ return (T_ORIGIN); }
sizeof_headers			{ return (T_SIZEOF_HEADERS); }
"/*"				{ _skip_comment(); }
"\""[^\"]+"\""			{ _get_string(); return (T_STRING); }
{DEC}|{OCT}|{HEX}		{ _calc_num(); return (T_NUM); }
"<<="				{ return (T_LSHIFT_E); }
">>="				{ return (T_RSHIFT_E); }
"<<"				{ return (T_LSHIFT); }
">>"				{ return (T_RSHIFT); }
"=="				{ return (T_EQ); }
"!="				{ return (T_NE); }
">="				{ return (T_GE); }
"<="				{ return (T_LE); }
"+="				{ return (T_ADD_E); }
"-="				{ return (T_SUB_E); }
"*="				{ return (T_MUL_E); }
"/="				{ return (T_DIV_E); }
"&="				{ return (T_AND_E); }
"|="				{ return (T_OR_E); }
"&&"				{ return (T_LOGICAL_AND); }
"||"				{ return (T_LOGICAL_OR); }
"!"				{ return ('!'); }
"{"				{ return ('{'); }
"}"				{ return ('}'); }
"["				{ return ('['); }
"]"				{ return (']'); }
"("				{ return ('('); }
")"				{ return (')'); }
"?"				{ return ('?'); }
":"				{ return (':'); }
";"				{ return (';'); }
"&"				{ return ('&'); }
"|"				{ return ('|'); }
"~"				{ return ('~'); }
"+"				{ return ('+'); }
"-"				{ return ('-'); }
"*"				{ return ('*'); }
"/"				{ return ('/'); }
"%"				{ return ('%'); }
"="				{ return ('='); }
"<"				{ return ('<'); }
">"				{ return ('>'); }
","				{ return (','); }
"."				{ return ('.'); }
{MATTR}				{ _get_ident(); return (T_MEMORY_ATTR); }
{IDENT}				{ _get_ident(); return (T_IDENT); }
{WILDCARD}			{ _get_ident(); return (T_WILDCARD); }
"\n"				{ lineno++; }
[ \t]				/* Ignore whitespaces. */

%%

static void
_calc_num(void)
{
	int base, mul;

	base = 0;
	mul = 1;
	switch (yytext[yyleng - 1]) {
	case 'd': case 'D':
		base = 10;
		break;
	case 'o': case 'O':
		base = 8;
		break;
	case 'h': case 'H':
		base = 16;
		break;
	case 'k': case 'K':
		mul = 1024;
		break;
	case 'm': case 'M':
		mul = 1024 * 1024;
		break;
	default:
		break;
	}

	if (base || mul > 1)
		yytext[yyleng - 1] = '\0';

	yylval.num = strtoimax(yytext, NULL, base);
	yylval.num *= mul;
}

static void
_get_ident(void)
{

	yylval.str = strdup(yytext);
	if (yylval.str == NULL)
		err(1, "strdup");
}

static void
_get_string(void)
{

	yytext[yyleng - 1] = '\0';
	yylval.str = strdup(yytext + 1);
	if (yylval.str == NULL)
		err(1, "strdup");
}

static void
_skip_comment(void)
{
	int c;

	for (;;) {
		while ((c = input()) != '*' && c != EOF)
			if (c == '\n')
				lineno++;
		if (c == '*') {
			while ((c = input()) == '*')
				;
			if (c == '\n')
				lineno++;
			if (c == '/')
				break;
		}
		if (c == EOF)
			errx(1, "lexer: EOF in comment");
	}
}
