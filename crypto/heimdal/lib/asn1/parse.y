/*
 * Copyright (c) 1997 - 2000 Kungliga Tekniska Högskolan
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

/* $Id: parse.y,v 1.16 2000/07/08 11:35:47 assar Exp $ */

%{
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symbol.h"
#include "lex.h"
#include "gen_locl.h"

RCSID("$Id: parse.y,v 1.16 2000/07/08 11:35:47 assar Exp $");

static Type *new_type (Typetype t);
void yyerror (char *);

static void append (Member *l, Member *r);

%}

%union {
  int constant;
  char *name;
  Type *type;
  Member *member;
}

%token INTEGER SEQUENCE OF OCTET STRING GeneralizedTime GeneralString
%token BIT APPLICATION OPTIONAL EEQUAL TBEGIN END DEFINITIONS EXTERNAL
%token DOTDOT
%token IMPORTS FROM
%token <name> IDENTIFIER 
%token <constant> CONSTANT

%type <constant> constant optional2
%type <type> type
%type <member> memberdecls memberdecl bitdecls bitdecl

%start envelope

%%

envelope	: IDENTIFIER DEFINITIONS EEQUAL TBEGIN specification END {}
		;

specification	:
		| specification declaration
		;

declaration	: imports_decl
		| type_decl
		| constant_decl
		;

referencenames	: IDENTIFIER ',' referencenames
		{
			Symbol *s = addsym($1);
			s->stype = Stype;
		}
		| IDENTIFIER
		{
			Symbol *s = addsym($1);
			s->stype = Stype;
		}
		;

imports_decl	: IMPORTS referencenames FROM IDENTIFIER ';'
		;

type_decl	: IDENTIFIER EEQUAL type
		{
		  Symbol *s = addsym ($1);
		  s->stype = Stype;
		  s->type = $3;
		  generate_type (s);
		}
		;

constant_decl	: IDENTIFIER type EEQUAL constant
		{
		  Symbol *s = addsym ($1);
		  s->stype = SConstant;
		  s->constant = $4;
		  generate_constant (s);
		}
		;

type		: INTEGER     { $$ = new_type(TInteger); }
		| INTEGER '(' constant DOTDOT constant ')' {
		    if($3 != 0)
			error_message("Only 0 supported as low range");
		    if($5 != INT_MIN && $5 != UINT_MAX && $5 != INT_MAX)
			error_message("Only %u supported as high range",
				      UINT_MAX);
		    $$ = new_type(TUInteger);
		}
                | INTEGER '{' bitdecls '}'
                {
			$$ = new_type(TInteger);
			$$->members = $3;
                }
		| OCTET STRING { $$ = new_type(TOctetString); }
		| GeneralString { $$ = new_type(TGeneralString); }
		| GeneralizedTime { $$ = new_type(TGeneralizedTime); }
		| SEQUENCE OF type
		{
		  $$ = new_type(TSequenceOf);
		  $$->subtype = $3;
		}
		| SEQUENCE '{' memberdecls '}'
		{
		  $$ = new_type(TSequence);
		  $$->members = $3;
		}
		| BIT STRING '{' bitdecls '}'
		{
		  $$ = new_type(TBitString);
		  $$->members = $4;
		}
		| IDENTIFIER
		{
		  Symbol *s = addsym($1);
		  $$ = new_type(TType);
		  if(s->stype != Stype)
		    error_message ("%s is not a type\n", $1);
		  else
		    $$->symbol = s;
		}
		| '[' APPLICATION constant ']' type
		{
		  $$ = new_type(TApplication);
		  $$->subtype = $5;
		  $$->application = $3;
		}
		;

memberdecls	: { $$ = NULL; }
		| memberdecl	{ $$ = $1; }
		| memberdecls ',' memberdecl { $$ = $1; append($$, $3); }
		;

memberdecl	: IDENTIFIER '[' constant ']' type optional2
		{
		  $$ = malloc(sizeof(*$$));
		  $$->name = $1;
		  $$->gen_name = strdup($1);
		  output_name ($$->gen_name);
		  $$->val = $3;
		  $$->optional = $6;
		  $$->type = $5;
		  $$->next = $$->prev = $$;
		}
		;

optional2	: { $$ = 0; }
		| OPTIONAL { $$ = 1; }
		;

bitdecls	: { $$ = NULL; }
		| bitdecl { $$ = $1; }
		| bitdecls ',' bitdecl { $$ = $1; append($$, $3); }
		;

bitdecl		: IDENTIFIER '(' constant ')'
		{
		  $$ = malloc(sizeof(*$$));
		  $$->name = $1;
		  $$->gen_name = strdup($1);
		  output_name ($$->gen_name);
		  $$->val = $3;
		  $$->optional = 0;
		  $$->type = NULL;
		  $$->prev = $$->next = $$;
		}
		;

constant	: CONSTANT	{ $$ = $1; }
		| IDENTIFIER	{
				  Symbol *s = addsym($1);
				  if(s->stype != SConstant)
				    error_message ("%s is not a constant\n",
						   s->name);
				  else
				    $$ = s->constant;
				}
		;
%%

void
yyerror (char *s)
{
     error_message ("%s\n", s);
}

static Type *
new_type (Typetype tt)
{
  Type *t = malloc(sizeof(*t));
  if (t == NULL) {
      error_message ("out of memory in malloc(%u)", sizeof(*t));
      exit (1);
  }
  t->type = tt;
  t->application = 0;
  t->members = NULL;
  t->subtype = NULL;
  t->symbol  = NULL;
  return t;
}

static void
append (Member *l, Member *r)
{
  l->prev->next = r;
  r->prev = l->prev;
  l->prev = r;
  r->next = l;
}
