%{
/*-
 * DEVD (Device action daemon)
 *
 * Copyright (c) 2002 M. Warner Losh <imp@freebsd.org>.
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

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "devd.h"
#include "y.tab.h"

int lineno = 1;
#define YY_NO_UNPUT

%}

%%

[ \t]+			;
\n			lineno++;
;			{ return SEMICOLON; }
\/\/.*$			;
\/\*(.|\n)*\*\/		;
\{			{ return BEGINBLOCK; }
\}			{ return ENDBLOCK; }
[0-9]+			{ yylval.i = atoi(yytext); return NUMBER; }
\"[^"]+\"		{
				int len = strlen(yytext) - 2;
				if ((yylval.str = (char *) malloc(len + 1)) == NULL)
					goto out;
				memcpy(yylval.str, yytext + 1, len);
				yylval.str[len] = '\0';
			out:;
				return STRING;
			}


options			{ return OPTIONS; }
set			{ return SET; }
directory		{ return DIRECTORY; }
pid-file		{ return PID_FILE; }
attach			{ return ATTACH; }
detach			{ return DETACH; }
device-name		{ return DEVICE_NAME; }
action			{ return ACTION; }
match			{ return MATCH; }
nomatch			{ return NOMATCH; }
[A-Za-z][A-Za-z0-9-]*	{
				int len = strlen(yytext);
				if ((yylval.str = (char *) malloc(len + 1)) == NULL)
					goto out2;
				memcpy(yylval.str, yytext + 1, len);
				yylval.str[len] = '\0';
			out2:;
				return ID;
			}
%%

void
yyerror(const char *s)
{
	syslog(LOG_ERR, "line %d: %s%s %s.\n", lineno, yytext, yytext?":":"", s);
}
