%{
/* sbc.y: A POSIX bc processor written for minix with no extensions.  */
 
/*  This file is part of GNU bc.
    Copyright (C) 1991, 1992, 1993, 1994, 1997 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

    You may contact the author by:
       e-mail:  phil@cs.wwu.edu
      us-mail:  Philip A. Nelson
                Computer Science Department, 9062
                Western Washington University
                Bellingham, WA 98226-9062
       
*************************************************************************/

#include "bcdefs.h"
#include "global.h"     /* To get the global variables. */
#include "proto.h"
%}

%start program

%union {
	char *s_value;
	char  c_value;
	int   i_value;
	arg_list *a_value;
       }

%token <i_value> NEWLINE AND OR NOT
%token <s_value> STRING NAME NUMBER
/*     '-', '+' are tokens themselves		*/
%token <c_value> ASSIGN_OP
/*     '=', '+=',  '-=', '*=', '/=', '%=', '^=' */
%token <s_value> REL_OP
/*     '==', '<=', '>=', '!=', '<', '>' 	*/
%token <c_value> INCR_DECR
/*     '++', '--' 				*/
%token <i_value> Define    Break    Quit    Length
/*     'define', 'break', 'quit', 'length' 	*/
%token <i_value> Return    For    If    While    Sqrt  Else
/*     'return', 'for', 'if', 'while', 'sqrt',  'else' 	*/
%token <i_value> Scale    Ibase    Obase    Auto  Read
/*     'scale', 'ibase', 'obase', 'auto', 'read' 	*/
%token <i_value> Warranty, Halt, Last, Continue, Print, Limits
/*     'warranty', 'halt', 'last', 'continue', 'print', 'limits'  */

/* The types of all other non-terminals. */
%type <i_value> expression named_expression return_expression
%type <a_value> opt_parameter_list parameter_list opt_auto_define_list
%type <a_value> define_list opt_argument_list argument_list
%type <i_value> program input_item semicolon_list statement_list
%type <i_value> statement_or_error statement function relational_expression 

/* precedence */
%nonassoc REL_OP
%right ASSIGN_OP
%left '+' '-'
%left '*' '/' '%'
%right '^'
%nonassoc UNARY_MINUS
%nonassoc INCR_DECR

%%
program			: /* empty */
			    {
			      $$ = 0;
			      std_only = TRUE;
			      if (interactive)
				{
				  printf ("s%s\n", BC_VERSION);
				  welcome();
				}
			    }
			| program input_item
			;
input_item		: semicolon_list NEWLINE
			    { run_code(); }
			| function
			    { run_code(); }
			| error NEWLINE
			    {
			      yyerrok; 
			      init_gen() ;
			    }
			;
semicolon_list		: /* empty */
			    { $$ = 0; }
			| statement_or_error
			| semicolon_list ';' statement_or_error
			| semicolon_list ';'
			;
statement_list		: /* empty */
			    { $$ = 0; }
			| statement
			| statement_list NEWLINE
			| statement_list NEWLINE statement
			| statement_list ';'
			| statement_list ';' statement
			;
statement_or_error	: statement
			| error statement
			    { $$ = $2; }
			;
statement 		: Warranty
			    { warranty("s"); }
			| expression
			    {
			      if ($1 & 1)
				generate ("W");
			      else
				generate ("p");
			    }
			| STRING
			    {
			      $$ = 0;
			      generate ("w");
			      generate ($1);
			      free ($1);
			    }
			| Break
			    {
			      if (break_label == 0)
				yyerror ("Break outside a for/while");
			      else
				{
				  sprintf (genstr, "J%1d:", break_label);
				  generate (genstr);
				}
			    }
			| Quit
			    { exit(0); }
			| Return
			    { generate ("0R"); }
			| Return '(' return_expression ')'
			    { generate ("R"); }
			| For 
			    {
			      $1 = break_label; 
			      break_label = next_label++;
			    }
			  '(' expression ';'
			    {
			      $4 = next_label++;
			      sprintf (genstr, "pN%1d:", $4);
			      generate (genstr);
			    }
			  relational_expression ';'
			    {
			      $7 = next_label++;
			      sprintf (genstr, "B%1d:J%1d:", $7, break_label);
			      generate (genstr);
			      $<i_value>$ = next_label++;
			      sprintf (genstr, "N%1d:", $<i_value>$);
			      generate (genstr);
			    }
			  expression ')'
			    {
			      sprintf (genstr, "pJ%1d:N%1d:", $4, $7);
			      generate (genstr);
			    }
			  statement
			    {
			      sprintf (genstr, "J%1d:N%1d:", $<i_value>9,
				       break_label);
			      generate (genstr);
			      break_label = $1;
			    }
			| If '(' relational_expression ')' 
			    {
			      $3 = next_label++;
			      sprintf (genstr, "Z%1d:", $3);
			      generate (genstr);
			    }
			  statement
			    {
			      sprintf (genstr, "N%1d:", $3); 
			      generate (genstr);
			    }
			| While 
			    {
			      $1 = next_label++;
			      sprintf (genstr, "N%1d:", $1);
			      generate (genstr);
			    }
			'(' relational_expression 
			    {
			      $4 = break_label; 
			      break_label = next_label++;
			      sprintf (genstr, "Z%1d:", break_label);
			      generate (genstr);
			    }
			')' statement
			    {
			      sprintf (genstr, "J%1d:N%1d:", $1, break_label);
			      generate (genstr);
			      break_label = $4;
			    }
			| '{' statement_list '}'
			    { $$ = 0; }
			;
function 		: Define NAME '(' opt_parameter_list ')' '{'
       			  NEWLINE opt_auto_define_list 
			    {
			      check_params ($4,$8);
			      sprintf (genstr, "F%d,%s.%s[", lookup($2,FUNCT),
				       arg_str ($4), arg_str ($8));
			      generate (genstr);
			      free_args ($4);
			      free_args ($8);
			      $1 = next_label;
			      next_label = 0;
			    }
			  statement_list NEWLINE '}'
			    {
			      generate ("0R]");
			      next_label = $1;
			    }
			;
opt_parameter_list	: /* empty */ 
			    { $$ = NULL; }
			| parameter_list
			;
parameter_list 		: NAME
			    { $$ = nextarg (NULL, lookup($1,SIMPLE), FALSE); }
			| define_list ',' NAME
			    { $$ = nextarg ($1, lookup($3,SIMPLE), FALSE); }
			;
opt_auto_define_list 	: /* empty */ 
			    { $$ = NULL; }
			| Auto define_list NEWLINE
			    { $$ = $2; } 
			| Auto define_list ';'
			    { $$ = $2; } 
			;
define_list 		: NAME
			    { $$ = nextarg (NULL, lookup($1,SIMPLE), FALSE); }
			| NAME '[' ']'
			    { $$ = nextarg (NULL, lookup($1,ARRAY), FALSE); }
			| define_list ',' NAME
			    { $$ = nextarg ($1, lookup($3,SIMPLE), FALSE); }
			| define_list ',' NAME '[' ']'
			    { $$ = nextarg ($1, lookup($3,ARRAY), FALSE); }
			;
opt_argument_list	: /* empty */
			    { $$ = NULL; }
			| argument_list
			;
argument_list 		: expression
			    { $$ = nextarg (NULL,0, FALSE); }
			| argument_list ',' expression
			    { $$ = nextarg ($1,0, FALSE); }
			;
relational_expression	: expression
			    { $$ = 0; }
			| expression REL_OP expression
			    {
			      $$ = 0;
			      switch (*($2))
				{
				case '=':
				  generate ("=");
				  break;
				case '!':
				  generate ("#");
				  break;
				case '<':
				  if ($2[1] == '=')
				    generate ("{");
				  else
				    generate ("<");
				  break;
				case '>':
				  if ($2[1] == '=')
				    generate ("}");
				  else
				    generate (">");
				  break;
				}
			    }
			;
return_expression	: /* empty */
			    {
			      $$ = 0;
			      generate ("0");
			    }
			| expression
			;
expression		: named_expression ASSIGN_OP 
			    {
			      if ($2 != '=')
				{
				  if ($1 < 0)
				    sprintf (genstr, "DL%d:", -$1);
				  else
				    sprintf (genstr, "l%d:", $1);
				  generate (genstr);
				}
			    }
			  expression
			    {
			      $$ = 0;
			      if ($2 != '=')
				{
				  sprintf (genstr, "%c", $2);
				  generate (genstr);
				}
			      if ($1 < 0)
				sprintf (genstr, "S%d:", -$1);
			      else
				sprintf (genstr, "s%d:", $1);
			      generate (genstr);
			    }
			| expression '+' expression
			    { generate ("+"); }
			| expression '-' expression
			    { generate ("-"); }
			| expression '*' expression
			    { generate ("*"); }
			| expression '/' expression
			    { generate ("/"); }
			| expression '%' expression
			    { generate ("%"); }
			| expression '^' expression
			    { generate ("^"); }
			| '-' expression           %prec UNARY_MINUS
			    { generate ("n"); $$ = 1;}
			| named_expression
			    {
			      $$ = 1;
			      if ($1 < 0)
				sprintf (genstr, "L%d:", -$1);
			      else
				sprintf (genstr, "l%d:", $1);
			      generate (genstr);
			    }
			| NUMBER
			    {
			      int len = strlen($1);
			      $$ = 1;
			      if (len == 1 && *$1 == '0')
				generate ("0");
			      else
				{
				  if (len == 1 && *$1 == '1')
				    generate ("1");
				  else
				    {
				      generate ("K");
				      generate ($1);
				      generate (":");
				    }
				  free ($1);
				}
			    }
			| '(' expression ')'
			    { $$ = 1; }
			| NAME '(' opt_argument_list ')'
			    {
			      $$ = 1;
			      if ($3 != NULL)
				{ 
				  sprintf (genstr, "C%d,%s:", lookup($1,FUNCT),
					   arg_str ($3));
				  free_args ($3);
				}
			      else
				  sprintf (genstr, "C%d:", lookup($1,FUNCT));
			      generate (genstr);
			    }
			| INCR_DECR named_expression
			    {
			      $$ = 1;
			      if ($2 < 0)
				{
				  if ($1 == '+')
				    sprintf (genstr, "DA%d:L%d:", -$2, -$2);
				  else
				    sprintf (genstr, "DM%d:L%d:", -$2, -$2);
				}
			      else
				{
				  if ($1 == '+')
				    sprintf (genstr, "i%d:l%d:", $2, $2);
				  else
				    sprintf (genstr, "d%d:l%d:", $2, $2);
				}
			      generate (genstr);
			    }
			| named_expression INCR_DECR
			    {
			      $$ = 1;
			      if ($1 < 0)
				{
				  sprintf (genstr, "DL%d:x", -$1);
				  generate (genstr); 
				  if ($2 == '+')
				    sprintf (genstr, "A%d:", -$1);
				  else
				    sprintf (genstr, "M%d:", -$1);
				}
			      else
				{
				  sprintf (genstr, "l%d:", $1);
				  generate (genstr);
				  if ($2 == '+')
				    sprintf (genstr, "i%d:", $1);
				  else
				    sprintf (genstr, "d%d:", $1);
				}
			      generate (genstr);
			    }
			| Length '(' expression ')'
			    { generate ("cL"); $$ = 1;}
			| Sqrt '(' expression ')'
			    { generate ("cR"); $$ = 1;}
			| Scale '(' expression ')'
			    { generate ("cS"); $$ = 1;}
			;
named_expression	: NAME
			    { $$ = lookup($1,SIMPLE); }
			| NAME '[' expression ']'
			    { $$ = lookup($1,ARRAY); }
			| Ibase
			    { $$ = 0; }
			| Obase
			    { $$ = 1; }
			| Scale
			    { $$ = 2; }
			;

%%
