%{
/*-
 * Copyright (c) 1995
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer, 
 *    verbatim and that no modifications are made prior to this 
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Paul Richards.
 * 4. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include "parser.tab.h"
extern int lineno;
extern int charno;
extern int off;
%}

%%
colortable		{ return COLORTABLE; }
colourtable		{ return COLORTABLE; }
Colors			{ return COLOR; }
Colours			{ return COLOR; }
black			{ return BLACK; }
red				{ return RED; }
green			{ return GREEN; }
yellow			{ return YELLOW; }
blue			{ return BLUE; }
magenta			{ return MAGENTA; }
cyan			{ return CYAN; }
white			{ return WHITE; }
pair			{ return PAIR; }
Form			{ return FORM; }
at				{ return AT; }
as				{ return AS; }
height			{ return HEIGHT; }
=				{ return EQUALS; }
width			{ return WIDTH; }
start			{ return STARTFIELD; }
text			{ return TEXT; }
attributes		{ return ATTR; }
highlight		{ return SELATTR; }
label			{ return LABEL; }
default			{ return DEFAULT; }
limit			{ return LIMIT; }
selected		{ return SELECTED; }
options			{ return OPTIONS; }
action			{ return ACTION; }
function		{ return FUNC; }
up				{ return UP; }
down			{ return DOWN; }
left			{ return LEFT; }
right			{ return RIGHT; }
next			{ return NEXT; }
,				{ return COMMA; }
\{				{ return LBRACE; }
\}				{ return RBRACE; }
[0-9]+			{ yylval.ival = atoi(yytext); return NUMBER; }
[A-Za-z_][A-Za-z0-9_()|&]*		{ yylval.sval = yytext; return NAME; }
\"[^"]* {
			if (yytext[yyleng-1] == '\\') {
				yymore();
			} else {
				input();
				yylval.sval = yytext+1;
				return STRING;
			}
		}
\n				{ lineno++; }
#.*				{ /* Ignored (comment) */; }
[ \t\f]*		{ /* Ignored (white space) */; }
