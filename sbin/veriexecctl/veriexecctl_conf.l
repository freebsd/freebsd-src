%{
/*
 * $FreeBSD$
 *
 * Configuration file lexer for Verified exec
 *
 *
 */

#include <stdio.h>
#include <string.h>
#include "veriexecctl_parse.h"

int lineno = 1;

void yyerror(const char *message);
void warning(const char *message);
int yylex __P((void));

%}

%%

path     { return PATH; }
string   { return STRING; }
eol      { return EOL; }

\/[^ 	]+  {
	yylval.string = strdup(yytext);
	return PATH;
}

[0-9a-zA-Z]+  {
	yylval.string = strdup(yytext);
	return STRING;
}

\n      {
	lineno++;  /* for error reporting */
	return EOL;
}

[ \t\r] ;  /* eat white ones */

#.* ;      /* comment */

.    yyerror("invalid character");

%%

void yyerror(const char *string)
{
  fprintf(stderr, "%d: %s at %s\n", lineno, string, yytext);
}
