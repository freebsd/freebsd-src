/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */
%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lib.h"
#include "box.h"
extern int non_empty_flag;
char *strsave(const char *);
int yylex();
void yyerror(const char *);
%}

%union {
	char *str;
	box *b;
	pile_box *pb;
	matrix_box *mb;
	int n;
	column *col;
}

%token OVER
%token SMALLOVER
%token SQRT
%token SUB
%token SUP
%token LPILE
%token RPILE
%token CPILE
%token PILE
%token LEFT
%token RIGHT
%token TO
%token FROM
%token SIZE
%token FONT
%token ROMAN
%token BOLD
%token ITALIC
%token FAT
%token ACCENT
%token BAR
%token UNDER
%token ABOVE
%token <str> TEXT
%token <str> QUOTED_TEXT
%token FWD
%token BACK
%token DOWN
%token UP
%token MATRIX
%token COL
%token LCOL
%token RCOL
%token CCOL
%token MARK
%token LINEUP
%token TYPE
%token VCENTER
%token PRIME
%token SPLIT
%token NOSPLIT
%token UACCENT
%token SPECIAL

/* these are handled in the lexer */
%token SPACE
%token GFONT
%token GSIZE
%token DEFINE
%token NDEFINE
%token TDEFINE
%token SDEFINE
%token UNDEF
%token IFDEF
%token INCLUDE
%token DELIM
%token CHARTYPE
%token SET
%token GRFONT
%token GBFONT

/* The original eqn manual says that `left' is right associative. It's lying.
Consider `left ( ~ left ( ~ right ) right )'. */

%right LEFT
%left RIGHT
%right LPILE RPILE CPILE PILE TEXT QUOTED_TEXT MATRIX MARK LINEUP '^' '~' '\t' '{' SPLIT NOSPLIT
%right FROM TO
%left SQRT OVER SMALLOVER
%right SUB SUP
%right ROMAN BOLD ITALIC FAT FONT SIZE FWD BACK DOWN UP TYPE VCENTER SPECIAL
%right BAR UNDER PRIME
%left ACCENT UACCENT

%type <b> mark from_to sqrt_over script simple equation nonsup
%type <n> number
%type <str> text delim
%type <pb> pile_element_list pile_arg
%type <mb> column_list
%type <col> column column_arg column_element_list

%%
top:
	/* empty */
	| equation
		{ $1->top_level(); non_empty_flag = 1; }
	;

equation:
	mark
		{ $$ = $1; }
	| equation mark
		{
		  list_box *lb = $1->to_list_box();
		  if (!lb)
		    lb = new list_box($1);
		  lb->append($2);
		  $$ = lb;
		}
	;

mark:
	from_to
		{ $$ = $1; }
	| MARK mark
		{ $$ = make_mark_box($2); }
	| LINEUP mark
		{ $$ = make_lineup_box($2); }
	;

from_to:
	sqrt_over  %prec FROM
		{ $$ = $1; }
	| sqrt_over TO from_to
		{ $$ = make_limit_box($1, 0, $3); }
	| sqrt_over FROM sqrt_over
		{ $$ = make_limit_box($1, $3, 0); }
	| sqrt_over FROM sqrt_over TO from_to
		{ $$ = make_limit_box($1, $3, $5); }
	| sqrt_over FROM sqrt_over FROM from_to
		{ $$ = make_limit_box($1, make_limit_box($3, $5, 0), 0); }
	;

sqrt_over:
	script
		{ $$ = $1; }
	| SQRT sqrt_over
		{ $$ = make_sqrt_box($2); }
	| sqrt_over OVER sqrt_over
		{ $$ = make_over_box($1, $3); }
	| sqrt_over SMALLOVER sqrt_over
		{ $$ = make_small_over_box($1, $3); }
	;

script:
	nonsup
		{ $$ = $1; }
	| simple SUP script
		{ $$ = make_script_box($1, 0, $3); }
	;

nonsup:
	simple  %prec SUP
		{ $$ = $1; }
	| simple SUB nonsup
		{ $$ = make_script_box($1, $3, 0); }
	| simple SUB simple SUP script
		{ $$ = make_script_box($1, $3, $5); }
	;

simple:
	TEXT
		{ $$ = split_text($1); }
	| QUOTED_TEXT
		{ $$ = new quoted_text_box($1); }
	| SPLIT QUOTED_TEXT
		{ $$ = split_text($2); }
	| NOSPLIT TEXT
		{ $$ = new quoted_text_box($2); }
	| '^'
		{ $$ = new half_space_box; }
	| '~'
		{ $$ = new space_box; }
	| '\t'
		{ $$ = new tab_box; }
	| '{' equation '}'
		{ $$ = $2; }
	| PILE pile_arg
		{ $2->set_alignment(CENTER_ALIGN); $$ = $2; }
	| LPILE pile_arg
		{ $2->set_alignment(LEFT_ALIGN); $$ = $2; }
	| RPILE pile_arg
		{ $2->set_alignment(RIGHT_ALIGN); $$ = $2; }
	| CPILE pile_arg
		{ $2->set_alignment(CENTER_ALIGN); $$ = $2; }
	| MATRIX '{' column_list '}'
		{ $$ = $3; }
	| LEFT delim equation RIGHT delim
		{ $$ = make_delim_box($2, $3, $5); }
	| LEFT delim equation
		{ $$ = make_delim_box($2, $3, 0); }
	| simple BAR
		{ $$ = make_overline_box($1); }
	| simple UNDER
		{ $$ = make_underline_box($1); }
	| simple PRIME
		{ $$ = make_prime_box($1); }
	| simple ACCENT simple
		{ $$ = make_accent_box($1, $3); }
	| simple UACCENT simple
		{ $$ = make_uaccent_box($1, $3); }
	| ROMAN simple
		{ $$ = new font_box(strsave(get_grfont()), $2); }
	| BOLD simple
		{ $$ = new font_box(strsave(get_gbfont()), $2); }
	| ITALIC simple
		{ $$ = new font_box(strsave(get_gfont()), $2); }
	| FAT simple
		{ $$ = new fat_box($2); }
	| FONT text simple
		{ $$ = new font_box($2, $3); }
	| SIZE text simple
		{ $$ = new size_box($2, $3); }
	| FWD number simple
		{ $$ = new hmotion_box($2, $3); }
	| BACK number simple
		{ $$ = new hmotion_box(-$2, $3); }
	| UP number simple
		{ $$ = new vmotion_box($2, $3); }
	| DOWN number simple
		{ $$ = new vmotion_box(-$2, $3); }
	| TYPE text simple
		{ $3->set_spacing_type($2); $$ = $3; }
	| VCENTER simple
		{ $$ = new vcenter_box($2); }
	| SPECIAL text simple
		{ $$ = make_special_box($2, $3); }
	;
	
number:
	text
		{
		  int n;
		  if (sscanf($1, "%d", &n) == 1)
		    $$ = n;
		  a_delete $1;
		}
	;

pile_element_list:
	equation
		{ $$ = new pile_box($1); }
	| pile_element_list ABOVE equation
		{ $1->append($3); $$ = $1; }
	;

pile_arg:
  	'{' pile_element_list '}'
		{ $$ = $2; }
	| number '{' pile_element_list '}'
		{ $3->set_space($1); $$ = $3; }
	;

column_list:
	column
		{ $$ = new matrix_box($1); }
	| column_list column
		{ $1->append($2); $$ = $1; }
	;

column_element_list:
	equation
		{ $$ = new column($1); }
	| column_element_list ABOVE equation
		{ $1->append($3); $$ = $1; }
	;

column_arg:
  	'{' column_element_list '}'
		{ $$ = $2; }
	| number '{' column_element_list '}'
		{ $3->set_space($1); $$ = $3; }
	;

column:
	COL column_arg
		{ $2->set_alignment(CENTER_ALIGN); $$ = $2; }
	| LCOL column_arg
		{ $2->set_alignment(LEFT_ALIGN); $$ = $2; }
	| RCOL column_arg
		{ $2->set_alignment(RIGHT_ALIGN); $$ = $2; }
	| CCOL column_arg
		{ $2->set_alignment(CENTER_ALIGN); $$ = $2; }
	;

text:	TEXT
		{ $$ = $1; }
	| QUOTED_TEXT
		{ $$ = $1; }
	;

delim:
	text
		{ $$ = $1; }
	| '{'
		{ $$ = strsave("{"); }
	| '}'
		{ $$ = strsave("}"); }
	;

%%
