%{
/*
 * $Id: inf-parse.y,v 1.3 2003/11/30 21:58:16 winter Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>

#include "inf.h"

extern int yyparse (void);
extern int yylex (void);
extern void yyerror(const char *);
%}

%token	EQUALS COMMA EOL
%token	<str> SECTION
%token	<str> STRING
%token	<str> WORD

%union {
	char *str;
}

%%

inf_file
	: inf_list
	|
	;

inf_list
	: inf
	| inf_list inf
	;

inf
	: SECTION EOL
		{ section_add($1); }
	| WORD EQUALS assign EOL
		{ assign_add($1); }
	| WORD COMMA regkey EOL
		{ regkey_add($1); }
	| WORD EOL
		{ define_add($1); }
	| EOL
	;

assign
	: WORD
		{ push_word($1); }
	| STRING
		{ push_word($1); }
	| WORD COMMA assign
		{ push_word($1); }
	| STRING COMMA assign
		{ push_word($1); }
	| COMMA assign
		{ push_word(NULL); }
	| COMMA
		{ push_word(NULL); }
	|
	;

regkey
	: WORD
		{ push_word($1); }
	| STRING
		{ push_word($1); }
	| WORD COMMA regkey
		{ push_word($1); }
	| STRING COMMA regkey
		{ push_word($1); }
	| COMMA regkey
		{ push_word(NULL); }
	| COMMA
		{ push_word(NULL); }
	;
%%
