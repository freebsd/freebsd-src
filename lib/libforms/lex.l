%{
#include "y.tab.h"
%}

%%
"Form template:"	{ yylval.ival = FORM; return FORM; }
Input	{ yylval.ival = INPUT; return INPUT; }
Text	{ yylval.ival = TEXT; return TEXT; }

[0-9]+	{ yylval.ival = atoi(yytext); return NUMBER; }
\"[^"]*	{
				if (yytext[yyleng-1] == '\\') {
					yymore();
				} else {
					input();
					yylval.sval = yytext+1;
					return STRING;
				}
			}
