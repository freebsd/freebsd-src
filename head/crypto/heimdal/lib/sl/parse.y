%{
/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "make_cmds.h"
RCSID("$Id: parse.y 21745 2007-07-31 16:11:25Z lha $");

static void yyerror (char *s);

struct string_list* append_string(struct string_list*, char*);
void free_string_list(struct string_list *list);
unsigned string_to_flag(const char *);

/* This is for bison */

#if !defined(alloca) && !defined(HAVE_ALLOCA)
#define alloca(x) malloc(x)
#endif

%}

%union {
  char *string;
  unsigned number;
  struct string_list *list;
}

%token TABLE REQUEST UNKNOWN UNIMPLEMENTED END
%token <string> STRING
%type <number> flag flags
%type <list> aliases

%%

file		: /* */ 
		| statements
		;

statements	: statement
		| statements statement
		;

statement	: TABLE STRING ';'
		{
		    table_name = $2;
		}
		| REQUEST STRING ',' STRING ',' aliases ',' '(' flags ')' ';'
		{
		    add_command($2, $4, $6, $9);
		}
		| REQUEST STRING ',' STRING ',' aliases ';'
		{
		    add_command($2, $4, $6, 0);
		}
		| UNIMPLEMENTED STRING ',' STRING ',' aliases ';'
		{
		    free($2);
		    free($4);
		    free_string_list($6);
		}
		| UNKNOWN aliases ';'
		{
		    free_string_list($2);
		}
		| END ';'
		{
		    YYACCEPT;
		}
		;

aliases		: STRING
		{
		    $$ = append_string(NULL, $1);
		}
		| aliases ',' STRING
		{
		    $$ = append_string($1, $3);
		}
		;

flags		: flag
		{
		    $$ = $1;
		}
		| flags ',' flag
		{
		    $$ = $1 | $3;
		}
		;
flag		: STRING
		{
		    $$ = string_to_flag($1);
		    free($1);
		}
		;



%%

static void
yyerror (char *s)
{
    error_message ("%s\n", s);
}

struct string_list*
append_string(struct string_list *list, char *str)
{
    struct string_list *sl = malloc(sizeof(*sl));
    if (sl == NULL)
	return sl;
    sl->string = str;
    sl->next = NULL;
    if(list) {
	*list->tail = sl;
	list->tail = &sl->next;
	return list;
    }
    sl->tail = &sl->next;
    return sl;
}

void
free_string_list(struct string_list *list)
{
    while(list) {
	struct string_list *sl = list->next;
	free(list->string);
	free(list);
	list = sl;
    }
}

unsigned
string_to_flag(const char *string)
{
    return 0;
}
