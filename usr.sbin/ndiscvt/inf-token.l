%{
/*
 * $Id: inf-token.l,v 1.2 2003/11/30 20:41:06 winter Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <regex.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "y.tab.h"

int lineno = 1;
#define YY_NO_UNPUT

int yylex(void);
void yyerror(const char *);

static void
update_lineno(const char *cp)
{
	while (*cp)
		if (*cp++ == '\n')
			lineno++;
}

%}

%%

[ \t]+			;
\n			{ lineno++; return EOL; }
\r			;
;.*$			;
=			{ return EQUALS; }
,			{ return COMMA; }
\"(\\\"|[^"])*\"	{
			int len = strlen(yytext) - 2;
			int blen = len + 1;
			char *walker;
			int i;
			update_lineno(yytext);
			yylval.str = (char *)malloc(blen);
			if (yylval.str == NULL)
				goto out;
			walker = yylval.str;
			for (i = 1; i <= len; i++) {
				if (yytext[i] == '\\') {
					switch (yytext[i + 1]) {
					case '\n':
						i += 2;
						while(isspace(yytext[i]))
							i++;
						break;
					case '\"':
						i++;
						break;
					case '(':
						i++;
						break;
					default:
						break;
					}
				}
				*walker++ = yytext[i];
			}
			*walker++ = '\0';
			out:;
			return STRING;
			}
\[[a-zA-Z0-9%&\{\}\-\.\/_\\\*\ ]+\]	{
				int len = strlen(yytext);
				yytext[len-1] = '\0';
				yylval.str = strdup(yytext+1);
				return SECTION;
			}
[a-zA-Z0-9%&\{\}\-\.\/_\\\*]+		{
				yylval.str = strdup(yytext);
				return WORD;
			}
%%

void
yyerror(const char *s)
{
	errx(1, "line %d: %s%s %s.", lineno, yytext, yytext?":":"", s);
}
