N	[0-9]
PC	[^\"]
AN      [A-Z_a-z0-9]
%%

command_table	return l_command_table();
request		return l_request();
unimplemented	return l_unimplemented();
end		return l_end();

[\t\n ]		;

\"{PC}*\"	return l_quoted_string();

{AN}*		return l_string();

#.*\n		;

.		return (*yytext);

%%

/*
 * User-subroutines section.
 *
 * Have to put all this stuff here so that the include file
 * from YACC output can be included, since LEX doesn't allow
 * an include file before the code it generates for the above
 * rules.
 *
 * Copyright 1987, 1988 by MIT Student Information Processing Board.
 *
 * For copyright info, see copyright.h.
 */

#include <string.h>
#include "ct.tab.h"
#include "copyright.h"

extern char *last_token, *ds();

static l_command_table()
{
     last_token = "command_table";
     return COMMAND_TABLE;
}

static l_request()
{
     last_token = "request";
     return REQUEST;
}

static l_unimplemented()
{
     last_token = "unimplemented";
     return UNIMPLEMENTED;
}

static l_end()
{
     last_token = "end";
     return END;
}

static l_quoted_string()
{
     register char *p;
     yylval.dynstr = ds(yytext+1);
     if (p=rindex(yylval.dynstr, '"'))
	  *p='\0';
     last_token = ds(yylval.dynstr);
     return STRING;
}

static l_string()
{
     yylval.dynstr = ds(yytext);
     last_token = ds(yylval.dynstr);
     return STRING;
}
