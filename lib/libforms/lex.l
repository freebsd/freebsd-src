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

#include <string.h>
#include <unistd.h>

#include "parser.tab.h"
extern int lineno;
extern int charno;
extern int off;
%}

NUM		[0-9]+

%%
!Forms			{ return FORMS; }
Action			{ return ACTION; }
Active			{ return ACTIVE; }
as				{ return AS; }
at				{ return AT; }
Attributes		{ return ATTR; }
AttrTable		{ return ATTRTABLE; }
CallFunc		{ return CALLFUNC; }
ColorPairs		{ return COLORPAIRS; }
Default			{ return DEFAULT; }
Display			{ return A_DISPLAY; }
Down			{ return DOWN; }
Forms			{ return FORMS; }
Function		{ return FUNCTION; }
Height			{ return HEIGHT; }
Highlight		{ return HIGHLIGHT; }
Input			{ return INPUT; }
InputFile		{ return INPUTFILE; }
Label			{ return LABEL; }
Left			{ return LEFT; }
Limit			{ return LIMIT; }
Menu			{ return MENU; }
Ncurses			{ return NCURSES; }
Next			{ return NEXT; }
Object			{ return AN_OBJECT; }
on				{ return ON; }
OnEntry			{ return ONENTRY; }
OnExit			{ return ONEXIT; }
Options			{ return OPTIONS; }
OutputFile		{ return OUTPUTFILE; }
Right			{ return RIGHT; }
Selected		{ return SELECTED; }
Text			{ return TEXT; }
Ttyname			{ return TTYNAME; }
Type			{ return TYPE; }
Up				{ return UP; }
Use				{ return USE; }
UserDrawFunc	{ return USERDRAWFUNC; }
UserProcFunc	{ return USERPROCFUNC; }
Version			{ return VERSION; }
Width			{ return WIDTH; }
Window			{ return WINDOW; }

Black			{ return BLACK; }
Red				{ return RED; }
Green			{ return GREEN; }
Yellow			{ return YELLOW; }
Blue			{ return BLUE; }
Magenta			{ return MAGENTA; }
Cyan			{ return CYAN; }
White			{ return WHITE; }

,				{ return COMMA; }
\{				{ return LBRACE; }
\}				{ return RBRACE; }
;				{ return SEMICOLON; }

{NUM}+			{ yylval.ival = atoi(yytext); return NUMBER; }

[A-Za-z_][A-Za-z0-9_.]*		{ yylval.sval = strdup(yytext);
							  free(yytext);
							  return NAME;
							 }

\"[^"]+\"   { 
				yytext[strlen(yytext)-1] = '\0';
				yylval.sval = strdup(yytext + 1);
				free(yytext);
				return STRING;
			}

\n				{ lineno++; }
#.*				{ /* Ignored (comment) */; }
[ \t\f]*		{ /* Ignored (white space) */; }

