%{
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright info, see copyright.h.
 */
#include <stdio.h>
#include "copyright.h"

char *str_concat3(), *ds(), *generate_rqte(), *malloc(), *realloc(), *quote();
long flag_value();
char *last_token = (char *)NULL;
FILE *output_file;
long gensym_n = 0;

%}
%union {
	char *dynstr;
	long flags;
}

%token COMMAND_TABLE REQUEST UNKNOWN UNIMPLEMENTED END
%token <dynstr> STRING
%token <dynstr> FLAGNAME
%type <dynstr> namelist header request_list
%type <dynstr> request_entry
%type <flags> flag_list options
%left OPTIONS
%{
#include "ss.h"
%}
%start command_table
%%
command_table :	header request_list END ';'
		{ write_ct($1, $2); }
	;

header	:	COMMAND_TABLE STRING ';'
		{ $$ = $2; }
	;

request_list :	request_list request_entry
		{ $$ = str_concat3($1, $2, ""); }
	|
		{ $$ = ""; }
	;

request_entry :	REQUEST STRING ',' STRING ',' namelist ',' options ';'
		{ $$ = generate_rqte($2, quote($4), $6, $8); }
	|	REQUEST STRING ',' STRING ',' namelist ';'
		{ $$ = generate_rqte($2, quote($4), $6, 0); }
	|	UNKNOWN namelist ';'
		{ $$ = generate_rqte("ss_unknown_request",
					(char *)NULL, $2, 0); }
	|	UNIMPLEMENTED STRING ',' STRING ',' namelist ';'
		{ $$ = generate_rqte("ss_unimplemented", quote($4), $6, 3); }
	;

options	:	'(' flag_list ')'
		{ $$ = $2; }
	|	'(' ')'
		{ $$ = 0; }
	;

flag_list :	flag_list ',' STRING
		{ $$ = $1 | flag_val($3); }
	|	STRING
		{ $$ = flag_val($1); }
	;

namelist: 	STRING
		{ $$ = quote(ds($1)); }
	|	namelist ',' STRING
		{ $$ = str_concat3($1, quote($3), ",\n    "); }
	;

%%
