%{
/*-
 * Copyright (c) 1983 The Regents of the University of California.
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

#ifndef lint
static char sccsid[] = "@(#)scan.l	5.8 (Berkeley) 4/19/91";
#endif /* not lint */

extern int yylineno;
int yylineno = 1;

#include "y.tab.h"
#include "htable.h"
%}

BLANK	[ \t]
DIGIT	[0-9]
ALPHA	[A-Za-z]
ANUM	[0-9A-Za-z]
NAMECHR	[0-9A-Za-z./-]

%%
"NET"		{
			yylval.number = KW_NET;
			return (KEYWORD);
		}

"GATEWAY"	{
			yylval.number = KW_GATEWAY;
			return (KEYWORD);
		}

"HOST"		{
			yylval.number = KW_HOST;
			return (KEYWORD);
		}

{ALPHA}{NAMECHR}*{ANUM}	{
			yylval.namelist = newname(yytext);
			return (NAME);
		}

{ALPHA}		{
			yylval.namelist = newname(yytext);
			return (NAME);
		}

{DIGIT}+{ALPHA}{NAMECHR}* {
			fprintf(stderr, "Warning: nonstandard name \"%s\"\n",
				yytext);
			yylval.namelist = newname(yytext);
			return (NAME);
		}

{DIGIT}+	{
			yylval.number = atoi(yytext);
			return (NUMBER);
		}

"."		return ('.');
":"		return (':');
","		return (',');
"/"		return ('/');
";".*		;
"\n"{BLANK}+	++yylineno;
{BLANK}+	;
"\n"		++yylineno; return (END);
.		fprintf(stderr, "Illegal char: '%s'\n", yytext);

%%
