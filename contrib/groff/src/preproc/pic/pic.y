/* Copyright (C) 1989, 1990, 1991, 1992, 2000 Free Software Foundation, Inc.
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */
%{
#include "pic.h"
#include "ptable.h"
#include "object.h"

extern int delim_flag;
extern void do_copy(const char *);
extern void copy_rest_thru(const char *, const char *);
extern void copy_file_thru(const char *, const char *, const char *);
extern void push_body(const char *);
extern void do_for(char *var, double from, double to,
		   int by_is_multiplicative, double by, char *body);
extern void do_lookahead();

#ifndef HAVE_FMOD
extern "C" {
  double fmod(double, double);
}
#endif

#undef rand
#undef srand
extern "C" {
  int rand();
#ifdef RET_TYPE_SRAND_IS_VOID
  void srand(unsigned int);
#else
  int srand(unsigned int);
#endif
}

/* Maximum number of characters produced by printf("%g") */
#define GDIGITS 14

int yylex();
void yyerror(const char *);

void reset(const char *nm);
void reset_all();

place *lookup_label(const char *);
void define_label(const char *label, const place *pl);

direction current_direction;
position current_position;

implement_ptable(place)

PTABLE(place) top_table;

PTABLE(place) *current_table = &top_table;
saved_state *current_saved_state = 0;

object_list olist;

const char *ordinal_postfix(int n);
const char *object_type_name(object_type type);
char *format_number(const char *form, double n);
char *do_sprintf(const char *form, const double *v, int nv);

%}


%union {
	char *str;
	int n;
	double x;
	struct { double x, y; } pair;
	struct { double x; char *body; } if_data;
	struct { char *str; const char *filename; int lineno; } lstr;
	struct { double *v; int nv; int maxv; } dv;
	struct { double val; int is_multiplicative; } by;
	place pl;
	object *obj;
	corner crn;
	path *pth;
	object_spec *spec;
	saved_state *pstate;
	graphics_state state;
	object_type obtype;
}

%token <str> LABEL
%token <str> VARIABLE
%token <x> NUMBER
%token <lstr> TEXT
%token <lstr> COMMAND_LINE
%token <str> DELIMITED
%token <n> ORDINAL
%token TH
%token LEFT_ARROW_HEAD
%token RIGHT_ARROW_HEAD
%token DOUBLE_ARROW_HEAD
%token LAST
%token UP
%token DOWN
%token LEFT
%token RIGHT
%token BOX
%token CIRCLE
%token ELLIPSE
%token ARC
%token LINE
%token ARROW
%token MOVE
%token SPLINE
%token HEIGHT
%token RADIUS
%token WIDTH
%token DIAMETER
%token UP
%token DOWN
%token RIGHT
%token LEFT
%token FROM
%token TO
%token AT
%token WITH
%token BY
%token THEN
%token SOLID
%token DOTTED
%token DASHED
%token CHOP
%token SAME
%token INVISIBLE
%token LJUST
%token RJUST
%token ABOVE
%token BELOW
%token OF
%token THE
%token WAY
%token BETWEEN
%token AND
%token HERE
%token DOT_N
%token DOT_E	
%token DOT_W
%token DOT_S
%token DOT_NE
%token DOT_SE
%token DOT_NW
%token DOT_SW
%token DOT_C
%token DOT_START
%token DOT_END
%token DOT_X
%token DOT_Y
%token DOT_HT
%token DOT_WID
%token DOT_RAD
%token SIN
%token COS
%token ATAN2
%token LOG
%token EXP
%token SQRT
%token K_MAX
%token K_MIN
%token INT
%token RAND
%token SRAND
%token COPY
%token THRU
%token TOP
%token BOTTOM
%token UPPER
%token LOWER
%token SH
%token PRINT
%token CW
%token CCW
%token FOR
%token DO
%token IF
%token ELSE
%token ANDAND
%token OROR
%token NOTEQUAL
%token EQUALEQUAL
%token LESSEQUAL
%token GREATEREQUAL
%token LEFT_CORNER
%token RIGHT_CORNER
%token CENTER
%token END
%token START
%token RESET
%token UNTIL
%token PLOT
%token THICKNESS
%token FILL
%token ALIGNED
%token SPRINTF
%token COMMAND

%token DEFINE
%token UNDEF

/* this ensures that plot 17 "%g" parses as (plot 17 "%g") */
%left PLOT
%left TEXT SPRINTF

/* give text adjustments higher precedence than TEXT, so that
box "foo" above ljust == box ("foo" above ljust)
*/

%left LJUST RJUST ABOVE BELOW

%left LEFT RIGHT
/* Give attributes that take an optional expression a higher
precedence than left and right, so that eg `line chop left'
parses properly. */
%left CHOP SOLID DASHED DOTTED UP DOWN FILL
%left LABEL

%left VARIABLE NUMBER '(' SIN COS ATAN2 LOG EXP SQRT K_MAX K_MIN INT RAND SRAND LAST 
%left ORDINAL HERE '`'

/* these need to be lower than '-' */
%left HEIGHT RADIUS WIDTH DIAMETER FROM TO AT THICKNESS

/* these must have higher precedence than CHOP so that `label %prec CHOP'
works */
%left DOT_N DOT_E DOT_W DOT_S DOT_NE DOT_SE DOT_NW DOT_SW DOT_C
%left DOT_START DOT_END TOP BOTTOM LEFT_CORNER RIGHT_CORNER
%left UPPER LOWER CENTER START END

%left ','
%left OROR
%left ANDAND
%left EQUALEQUAL NOTEQUAL
%left '<' '>' LESSEQUAL GREATEREQUAL

%left BETWEEN OF
%left AND

%left '+' '-'
%left '*' '/' '%'
%right '!'
%right '^'

%type <x> expr any_expr text_expr
%type <by> optional_by
%type <pair> expr_pair position_not_place
%type <if_data> simple_if
%type <obj> nth_primitive
%type <crn> corner
%type <pth> path label_path relative_path
%type <pl> place label element element_list middle_element_list
%type <spec> object_spec
%type <pair> position
%type <obtype> object_type
%type <n> optional_ordinal_last ordinal
%type <str> until
%type <dv> sprintf_args
%type <lstr> text print_args print_arg

%%

top:
	optional_separator
	| element_list
		{
		  if (olist.head)
		    print_picture(olist.head);
		}
	;


element_list:
	optional_separator middle_element_list optional_separator
		{ $$ = $2; }
	;

middle_element_list:
	element
		{ $$ = $1; }
	| middle_element_list separator element
		{ $$ = $1; }
	;

optional_separator:
	/* empty */
	| separator
	;

separator:
	';'
	| separator ';'
	;

placeless_element:
	VARIABLE '=' any_expr
		{
		  define_variable($1, $3);
		  a_delete $1;
		}
	| VARIABLE ':' '=' any_expr
		{
		  place *p = lookup_label($1);
		  if (!p) {
		    lex_error("variable `%1' not defined", $1);
		    YYABORT;
		  }
		  p->obj = 0;
		  p->x = $4;
		  p->y = 0.0;
		  a_delete $1;
		}
	| UP
		{ current_direction = UP_DIRECTION; }
	| DOWN
		{ current_direction = DOWN_DIRECTION; }
	| LEFT
		{ current_direction = LEFT_DIRECTION; }
	| RIGHT
		{ current_direction = RIGHT_DIRECTION; }
	| COMMAND_LINE
		{
		  olist.append(make_command_object($1.str, $1.filename,
						   $1.lineno));
		}
	| COMMAND print_args
		{
		  olist.append(make_command_object($2.str, $2.filename,
						   $2.lineno));
		}
	| PRINT print_args
		{
		  fprintf(stderr, "%s\n", $2.str);
		  a_delete $2.str;
	          fflush(stderr);
		}
	| SH
		{ delim_flag = 1; }
	  DELIMITED
		{
		  delim_flag = 0;
		  if (safer_flag)
		    lex_error("unsafe to run command `%1'", $3);
		  else
		    system($3);
		  a_delete $3;
		}
	| COPY TEXT
		{
		  if (yychar < 0)
		    do_lookahead();
		  do_copy($2.str);
		  // do not delete the filename
		}
	| COPY TEXT THRU
		{ delim_flag = 2; }
	  DELIMITED 
		{ delim_flag = 0; }
	  until
		{
		  if (yychar < 0)
		    do_lookahead();
		  copy_file_thru($2.str, $5, $7);
		  // do not delete the filename
		  a_delete $5;
		  a_delete $7;
		}
	| COPY THRU
		{ delim_flag = 2; }
	  DELIMITED
		{ delim_flag = 0; }
	  until
		{
		  if (yychar < 0)
		    do_lookahead();
		  copy_rest_thru($4, $6);
		  a_delete $4;
		  a_delete $6;
		}
	| FOR VARIABLE '=' expr TO expr optional_by DO
	  	{ delim_flag = 1; }
	  DELIMITED
	  	{
		  delim_flag = 0;
		  if (yychar < 0)
		    do_lookahead();
		  do_for($2, $4, $6, $7.is_multiplicative, $7.val, $10); 
		}
	| simple_if
		{
		  if (yychar < 0)
		    do_lookahead();
		  if ($1.x != 0.0)
		    push_body($1.body);
		  a_delete $1.body;
		}
	| simple_if ELSE
		{ delim_flag = 1; }
	  DELIMITED
		{
		  delim_flag = 0;
		  if (yychar < 0)
		    do_lookahead();
		  if ($1.x != 0.0)
		    push_body($1.body);
		  else
		    push_body($4);
		  a_delete $1.body;
		  a_delete $4;
		}
	| reset_variables
	| RESET
		{ define_variable("scale", 1.0); }
	;

reset_variables:
	RESET VARIABLE
		{ reset($2); a_delete $2; }
	| reset_variables VARIABLE
		{ reset($2); a_delete $2; }
	| reset_variables ',' VARIABLE
		{ reset($3); a_delete $3; }
	;

print_args:
	print_arg
		{ $$ = $1; }
	| print_args print_arg
		{
		  $$.str = new char[strlen($1.str) + strlen($2.str) + 1];
		  strcpy($$.str, $1.str);
		  strcat($$.str, $2.str);
		  a_delete $1.str;
		  a_delete $2.str;
		  if ($1.filename) {
		    $$.filename = $1.filename;
		    $$.lineno = $1.lineno;
		  }
		  else if ($2.filename) {
		    $$.filename = $2.filename;
		    $$.lineno = $2.lineno;
		  }
		}
	;

print_arg:
  	expr               %prec ','
		{
		  $$.str = new char[GDIGITS + 1];
		  sprintf($$.str, "%g", $1);
		  $$.filename = 0;
		  $$.lineno = 0;
		}
	| text
		{ $$ = $1; }
	| position          %prec ','
		{
		  $$.str = new char[GDIGITS + 2 + GDIGITS + 1];
		  sprintf($$.str, "%g, %g", $1.x, $1.y);
		  $$.filename = 0;
		  $$.lineno = 0;
		}

simple_if:
	IF any_expr THEN
		{ delim_flag = 1; }
	DELIMITED
		{ delim_flag = 0; $$.x = $2; $$.body = $5; }
	;

until:
	/* empty */
		{ $$ = 0; }
	| UNTIL TEXT
		{ $$ = $2.str; }
	;
	
any_expr:
	expr
		{ $$ = $1; }
	| text_expr
		{ $$ = $1; }
	;
	
text_expr:
	text EQUALEQUAL text
		{
		  $$ = strcmp($1.str, $3.str) == 0;
		  a_delete $1.str;
		  a_delete $3.str;
		}
	| text NOTEQUAL text
		{
		  $$ = strcmp($1.str, $3.str) != 0;
		  a_delete $1.str;
		  a_delete $3.str;
		}
	| text_expr ANDAND text_expr
		{ $$ = ($1 != 0.0 && $3 != 0.0); }
	| text_expr ANDAND expr
		{ $$ = ($1 != 0.0 && $3 != 0.0); }
	| expr ANDAND text_expr
		{ $$ = ($1 != 0.0 && $3 != 0.0); }
	| text_expr OROR text_expr
		{ $$ = ($1 != 0.0 || $3 != 0.0); }
	| text_expr OROR expr
		{ $$ = ($1 != 0.0 || $3 != 0.0); }
	| expr OROR text_expr
		{ $$ = ($1 != 0.0 || $3 != 0.0); }
	| '!' text_expr
		{ $$ = ($2 == 0.0); }
	;


optional_by:
	/* empty */
		{ $$.val = 1.0; $$.is_multiplicative = 0; }
	| BY expr
		{ $$.val = $2; $$.is_multiplicative = 0; }
	| BY '*' expr
		{ $$.val = $3; $$.is_multiplicative = 1; }
	;

element:
	object_spec
		{
		  $$.obj = $1->make_object(&current_position,
					   &current_direction);
		  if ($$.obj == 0)
		    YYABORT;
		  delete $1;
		  if ($$.obj)
		    olist.append($$.obj);
		  else {
		    $$.x = current_position.x;
		    $$.y = current_position.y;
		  }
		}
	| LABEL ':' optional_separator element
		{ $$ = $4; define_label($1, & $$); a_delete $1; }
	| LABEL ':' optional_separator position_not_place
		{
		  $$.obj = 0;
		  $$.x = $4.x;
		  $$.y = $4.y;
		  define_label($1, & $$);
		  a_delete $1;
		}
	| LABEL ':' optional_separator place
		{
		  $$ = $4;
		  define_label($1, & $$);
		  a_delete $1;
		}
	| '{'
		{
		  $<state>$.x = current_position.x;
		  $<state>$.y = current_position.y;
		  $<state>$.dir = current_direction;
		}
	  element_list '}'
		{
		  current_position.x = $<state>2.x;
		  current_position.y = $<state>2.y;
		  current_direction = $<state>2.dir;
		}
	  optional_element
		{
		  $$ = $3;
		}
	| placeless_element
		{
		  $$.obj = 0;
		  $$.x = current_position.x;
		  $$.y = current_position.y;
		}
	;

optional_element:
	/* empty */
		{}
	| element
		{}
	;

object_spec:
	BOX
		{
		  $$ = new object_spec(BOX_OBJECT);
		}
	| CIRCLE
		{
		  $$ = new object_spec(CIRCLE_OBJECT);
		}
	| ELLIPSE
		{
		  $$ = new object_spec(ELLIPSE_OBJECT);
		}
	| ARC
		{
		  $$ = new object_spec(ARC_OBJECT);
		  $$->dir = current_direction;
		}
	| LINE
		{
		  $$ = new object_spec(LINE_OBJECT);
		  lookup_variable("lineht", & $$->segment_height);
		  lookup_variable("linewid", & $$->segment_width);
		  $$->dir = current_direction;
		}
	| ARROW
		{
		  $$ = new object_spec(ARROW_OBJECT);
		  lookup_variable("lineht", & $$->segment_height);
		  lookup_variable("linewid", & $$->segment_width);
		  $$->dir = current_direction;
		}
	| MOVE
		{
		  $$ = new object_spec(MOVE_OBJECT);
		  lookup_variable("moveht", & $$->segment_height);
		  lookup_variable("movewid", & $$->segment_width);
		  $$->dir = current_direction;
		}
	| SPLINE
		{
		  $$ = new object_spec(SPLINE_OBJECT);
		  lookup_variable("lineht", & $$->segment_height);
		  lookup_variable("linewid", & $$->segment_width);
		  $$->dir = current_direction;
		}
	| text   %prec TEXT
		{
		  $$ = new object_spec(TEXT_OBJECT);
		  $$->text = new text_item($1.str, $1.filename, $1.lineno);
		}
	| PLOT expr
		{
		  $$ = new object_spec(TEXT_OBJECT);
		  $$->text = new text_item(format_number(0, $2), 0, -1);
		}
	| PLOT expr text
		{
		  $$ = new object_spec(TEXT_OBJECT);
		  $$->text = new text_item(format_number($3.str, $2),
					   $3.filename, $3.lineno);
		  a_delete $3.str;
		}
	| '[' 
		{
		  saved_state *p = new saved_state;
		  $<pstate>$ = p;
		  p->x = current_position.x;
		  p->y = current_position.y;
		  p->dir = current_direction;
		  p->tbl = current_table;
		  p->prev = current_saved_state;
		  current_position.x = 0.0;
		  current_position.y = 0.0;
		  current_table = new PTABLE(place);
		  current_saved_state = p;
		  olist.append(make_mark_object());
		}
	  element_list ']'
		{
		  current_position.x = $<pstate>2->x;
		  current_position.y = $<pstate>2->y;
		  current_direction = $<pstate>2->dir;
		  $$ = new object_spec(BLOCK_OBJECT);
		  olist.wrap_up_block(& $$->oblist);
		  $$->tbl = current_table;
		  current_table = $<pstate>2->tbl;
		  current_saved_state = $<pstate>2->prev;
		  delete $<pstate>2;
		}
	| object_spec HEIGHT expr
		{
		  $$ = $1;
		  $$->height = $3;
		  $$->flags |= HAS_HEIGHT;
		}
	| object_spec RADIUS expr
		{
		  $$ = $1;
		  $$->radius = $3;
		  $$->flags |= HAS_RADIUS;
		}
	| object_spec WIDTH expr
		{
		  $$ = $1;
		  $$->width = $3;
		  $$->flags |= HAS_WIDTH;
		}
	| object_spec DIAMETER expr
		{
		  $$ = $1;
		  $$->radius = $3/2.0;
		  $$->flags |= HAS_RADIUS;
		}
	| object_spec expr %prec HEIGHT
		{
		  $$ = $1;
		  $$->flags |= HAS_SEGMENT;
		  switch ($$->dir) {
		  case UP_DIRECTION:
		    $$->segment_pos.y += $2;
		    break;
		  case DOWN_DIRECTION:
		    $$->segment_pos.y -= $2;
		    break;
		  case RIGHT_DIRECTION:
		    $$->segment_pos.x += $2;
		    break;
		  case LEFT_DIRECTION:
		    $$->segment_pos.x -= $2;
		    break;
		  }
		}
	| object_spec UP
		{
		  $$ = $1;
		  $$->dir = UP_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.y += $$->segment_height;
		}
	| object_spec UP expr
		{
		  $$ = $1;
		  $$->dir = UP_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.y += $3;
		}
	| object_spec DOWN
		{
		  $$ = $1;
		  $$->dir = DOWN_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.y -= $$->segment_height;
		}
	| object_spec DOWN expr
		{
		  $$ = $1;
		  $$->dir = DOWN_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.y -= $3;
		}
	| object_spec RIGHT
		{
		  $$ = $1;
		  $$->dir = RIGHT_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.x += $$->segment_width;
		}
	| object_spec RIGHT expr
		{
		  $$ = $1;
		  $$->dir = RIGHT_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.x += $3;
		}
	| object_spec LEFT
		{
		  $$ = $1;
		  $$->dir = LEFT_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.x -= $$->segment_width;
		}
	| object_spec LEFT expr
		{
		  $$ = $1;
		  $$->dir = LEFT_DIRECTION;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.x -= $3;
		}
	| object_spec FROM position
		{
		  $$ = $1;
		  $$->flags |= HAS_FROM;
		  $$->from.x = $3.x;
		  $$->from.y = $3.y;
		}
	| object_spec TO position
		{
		  $$ = $1;
		  if ($$->flags & HAS_SEGMENT)
		    $$->segment_list = new segment($$->segment_pos,
						   $$->segment_is_absolute,
						   $$->segment_list);
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.x = $3.x;
		  $$->segment_pos.y = $3.y;
		  $$->segment_is_absolute = 1;
		  $$->flags |= HAS_TO;
		  $$->to.x = $3.x;
		  $$->to.y = $3.y;
		}
	| object_spec AT position
		{
		  $$ = $1;
		  $$->flags |= HAS_AT;
		  $$->at.x = $3.x;
		  $$->at.y = $3.y;
		  if ($$->type != ARC_OBJECT) {
		    $$->flags |= HAS_FROM;
		    $$->from.x = $3.x;
		    $$->from.y = $3.y;
		  }
		}
	| object_spec WITH path
		{
		  $$ = $1;
		  $$->flags |= HAS_WITH;
		  $$->with = $3;
		}
	| object_spec BY expr_pair
		{
		  $$ = $1;
		  $$->flags |= HAS_SEGMENT;
		  $$->segment_pos.x += $3.x;
		  $$->segment_pos.y += $3.y;
		}
	| object_spec THEN
  		{
		  $$ = $1;
		  if ($$->flags & HAS_SEGMENT) {
		    $$->segment_list = new segment($$->segment_pos,
						   $$->segment_is_absolute,
						   $$->segment_list);
		    $$->flags &= ~HAS_SEGMENT;
		    $$->segment_pos.x = $$->segment_pos.y = 0.0;
		    $$->segment_is_absolute = 0;
		  }
		}
	| object_spec SOLID
		{
		  $$ = $1;	// nothing
		}
	| object_spec DOTTED
		{
		  $$ = $1;
		  $$->flags |= IS_DOTTED;
		  lookup_variable("dashwid", & $$->dash_width);
		}
	| object_spec DOTTED expr
		{
		  $$ = $1;
		  $$->flags |= IS_DOTTED;
		  $$->dash_width = $3;
		}
	| object_spec DASHED
		{
		  $$ = $1;
		  $$->flags |= IS_DASHED;
		  lookup_variable("dashwid", & $$->dash_width);
		}
	| object_spec DASHED expr
		{
		  $$ = $1;
		  $$->flags |= IS_DASHED;
		  $$->dash_width = $3;
		}
	| object_spec FILL
		{
		  $$ = $1;
		  $$->flags |= IS_DEFAULT_FILLED;
		}
	| object_spec FILL expr
		{
		  $$ = $1;
		  $$->flags |= IS_FILLED;
		  $$->fill = $3;
		}
	| object_spec CHOP
  		{
		  $$ = $1;
		  // line chop chop means line chop 0 chop 0
		  if ($$->flags & IS_DEFAULT_CHOPPED) {
		    $$->flags |= IS_CHOPPED;
		    $$->flags &= ~IS_DEFAULT_CHOPPED;
		    $$->start_chop = $$->end_chop = 0.0;
		  }
		  else if ($$->flags & IS_CHOPPED) {
		    $$->end_chop = 0.0;
		  }
		  else {
		    $$->flags |= IS_DEFAULT_CHOPPED;
		  }
		}
	| object_spec CHOP expr
		{
		  $$ = $1;
		  if ($$->flags & IS_DEFAULT_CHOPPED) {
		    $$->flags |= IS_CHOPPED;
		    $$->flags &= ~IS_DEFAULT_CHOPPED;
		    $$->start_chop = 0.0;
		    $$->end_chop = $3;
		  }
		  else if ($$->flags & IS_CHOPPED) {
		    $$->end_chop = $3;
		  }
		  else {
		    $$->start_chop = $$->end_chop = $3;
		    $$->flags |= IS_CHOPPED;
		  }
		}
	| object_spec SAME
		{
		  $$ = $1;
		  $$->flags |= IS_SAME;
		}
	| object_spec INVISIBLE
		{
		  $$ = $1;
		  $$->flags |= IS_INVISIBLE;
		}
	| object_spec LEFT_ARROW_HEAD
		{
		  $$ = $1;
		  $$->flags |= HAS_LEFT_ARROW_HEAD;
		}
	| object_spec RIGHT_ARROW_HEAD
		{
		  $$ = $1;
		  $$->flags |= HAS_RIGHT_ARROW_HEAD;
		}
	| object_spec DOUBLE_ARROW_HEAD
		{
		  $$ = $1;
		  $$->flags |= (HAS_LEFT_ARROW_HEAD|HAS_RIGHT_ARROW_HEAD);
		}
	| object_spec CW
		{
		  $$ = $1;
		  $$->flags |= IS_CLOCKWISE;
		}
	| object_spec CCW
		{
		  $$ = $1;
		  $$->flags &= ~IS_CLOCKWISE;
		}
	| object_spec text   %prec TEXT
		{
		  $$ = $1;
		  text_item **p;
		  for (p = & $$->text; *p; p = &(*p)->next)
		    ;
		  *p = new text_item($2.str, $2.filename, $2.lineno);
		}
	| object_spec LJUST
		{
		  $$ = $1;
		  if ($$->text) {
		    text_item *p;
		    for (p = $$->text; p->next; p = p->next)
		      ;
		    p->adj.h = LEFT_ADJUST;
		  }
		}
	| object_spec RJUST
		{
		  $$ = $1;
		  if ($$->text) {
		    text_item *p;
		    for (p = $$->text; p->next; p = p->next)
		      ;
		    p->adj.h = RIGHT_ADJUST;
		  }
		}
	| object_spec ABOVE
		{
		  $$ = $1;
		  if ($$->text) {
		    text_item *p;
		    for (p = $$->text; p->next; p = p->next)
		      ;
		    p->adj.v = ABOVE_ADJUST;
		  }
		}
	| object_spec BELOW
		{
		  $$ = $1;
		  if ($$->text) {
		    text_item *p;
		    for (p = $$->text; p->next; p = p->next)
		      ;
		    p->adj.v = BELOW_ADJUST;
		  }
		}
	| object_spec THICKNESS expr
		{
		  $$ = $1;
		  $$->flags |= HAS_THICKNESS;
		  $$->thickness = $3;
		}
	| object_spec ALIGNED
		{
		  $$ = $1;
		  $$->flags |= IS_ALIGNED;
		}
	;

text:
	TEXT
		{
		  $$ = $1;
		}
	| SPRINTF '(' TEXT sprintf_args ')'
		{
		  $$.filename = $3.filename;
		  $$.lineno = $3.lineno;
		  $$.str = do_sprintf($3.str, $4.v, $4.nv);
		  a_delete $4.v;
		  a_delete $3.str;
		}
	;

sprintf_args:
	/* empty */
		{
		  $$.v = 0;
		  $$.nv = 0;
		  $$.maxv = 0;
		}
	| sprintf_args ',' expr
		{
		  $$ = $1;
		  if ($$.nv >= $$.maxv) {
		    if ($$.nv == 0) {
		      $$.v = new double[4];
		      $$.maxv = 4;
		    }
		    else {
		      double *oldv = $$.v;
		      $$.maxv *= 2;
		      $$.v = new double[$$.maxv];
		      memcpy($$.v, oldv, $$.nv*sizeof(double));
		      a_delete oldv;
		    }
		  }
		  $$.v[$$.nv] = $3;
		  $$.nv += 1;
		}
	;

position:
  	position_not_place
		{ $$ = $1; }
	| place
  		{
		  position pos = $1;
		  $$.x = pos.x;
		  $$.y = pos.y;
		}
	;

position_not_place:
	expr_pair
		{ $$ = $1; }
	| position '+' expr_pair
		{
		  $$.x = $1.x + $3.x;
		  $$.y = $1.y + $3.y;
		}
	| position '-' expr_pair
		{
		  $$.x = $1.x - $3.x;
		  $$.y = $1.y - $3.y;
		}
	| '(' position ',' position ')'
		{
		  $$.x = $2.x;
		  $$.y = $4.y;
		}
	| expr between position AND position
		{
		  $$.x = (1.0 - $1)*$3.x + $1*$5.x;
		  $$.y = (1.0 - $1)*$3.y + $1*$5.y;
		}
	| expr '<' position ',' position '>'
		{
		  $$.x = (1.0 - $1)*$3.x + $1*$5.x;
		  $$.y = (1.0 - $1)*$3.y + $1*$5.y;
		}
	;

between:
	BETWEEN
	| OF THE WAY BETWEEN
	;

expr_pair:
	expr ',' expr
		{ $$.x = $1; $$.y = $3; }
	| '(' expr_pair ')'
		{ $$ = $2; }
	;

place:
	label  %prec CHOP /* line at A left == line (at A) left */
		{ $$ = $1; }
	| label corner
		{
		  path pth($2);
		  if (!pth.follow($1, & $$))
		    YYABORT;
		}
	| corner label
		{
		  path pth($1);
		  if (!pth.follow($2, & $$))
		    YYABORT;
		}
	| corner OF label
		{
		  path pth($1);
		  if (!pth.follow($3, & $$))
		    YYABORT;
		}
	| HERE
		{
		  $$.x = current_position.x;
		  $$.y = current_position.y;
		  $$.obj = 0;
		}
	;

label:
	LABEL
		{
		  place *p = lookup_label($1);
		  if (!p) {
		    lex_error("there is no place `%1'", $1);
		    YYABORT;
		  }
		  $$ = *p;
		  a_delete $1;
		}
	| nth_primitive
		{
		  $$.obj = $1;
		}
	| label '.' LABEL
		{
		  path pth($3);
		  if (!pth.follow($1, & $$))
		    YYABORT;
		}
	;

ordinal:
	ORDINAL
		{ $$ = $1; }
	| '`' any_expr TH
		{
		  // XXX Check for overflow (and non-integers?).
		  $$ = (int)$2;
		}
	;

optional_ordinal_last:
        LAST
		{ $$ = 1; }
  	| ordinal LAST
		{ $$ = $1; }
	;

nth_primitive:
	ordinal object_type
		{
		  int count = 0;
		  object *p;
		  for (p = olist.head; p != 0; p = p->next)
		    if (p->type() == $2 && ++count == $1) {
		      $$ = p;
		      break;
		    }
		  if (p == 0) {
		    lex_error("there is no %1%2 %3", $1, ordinal_postfix($1),
			      object_type_name($2));
		    YYABORT;
		  }
		}
	| optional_ordinal_last object_type
		{
		  int count = 0;
		  object *p;
		  for (p = olist.tail; p != 0; p = p->prev)
		    if (p->type() == $2 && ++count == $1) {
		      $$ = p;
		      break;
		    }
		  if (p == 0) {
		    lex_error("there is no %1%2 last %3", $1,
			      ordinal_postfix($1), object_type_name($2));
		    YYABORT;
		  }
		}
	;

object_type:
	BOX
  		{ $$ = BOX_OBJECT; }
	| CIRCLE
		{ $$ = CIRCLE_OBJECT; }
	| ELLIPSE
		{ $$ = ELLIPSE_OBJECT; }
	| ARC
		{ $$ = ARC_OBJECT; }
	| LINE
		{ $$ = LINE_OBJECT; }
	| ARROW
		{ $$ = ARROW_OBJECT; }
	| SPLINE
		{ $$ = SPLINE_OBJECT; }
	| '[' ']'
		{ $$ = BLOCK_OBJECT; }
	| TEXT
		{ $$ = TEXT_OBJECT; }
	;

label_path:
 	'.' LABEL
		{
		  $$ = new path($2);
		}
	| label_path '.' LABEL
		{
		  $$ = $1;
		  $$->append($3);
		}
	;

relative_path:
	corner
		{
		  $$ = new path($1);
		}
	/* give this a lower precedence than LEFT and RIGHT so that
	   [A: box] with .A left == [A: box] with (.A left) */

  	| label_path %prec TEXT
		{
		  $$ = $1;
		}
	| label_path corner
		{
		  $$ = $1;
		  $$->append($2);
		}
	;

path:
	relative_path
		{
		  $$ = $1;
		}
	| '(' relative_path ',' relative_path ')'
		{
		  $$ = $2;
		  $$->set_ypath($4);
		}
	/* The rest of these rules are a compatibility sop. */
	| ORDINAL LAST object_type relative_path
		{
		  lex_warning("`%1%2 last %3' in `with' argument ignored",
			      $1, ordinal_postfix($1), object_type_name($3));
		  $$ = $4;
		}
	| LAST object_type relative_path
		{
		  lex_warning("`last %1' in `with' argument ignored",
			      object_type_name($2));
		  $$ = $3;
		}
	| ORDINAL object_type relative_path
		{
		  lex_warning("`%1%2 %3' in `with' argument ignored",
			      $1, ordinal_postfix($1), object_type_name($2));
		  $$ = $3;
		}
	| LABEL relative_path
		{
		  lex_warning("initial `%1' in `with' argument ignored", $1);
		  a_delete $1;
		  $$ = $2;
		}
	;

corner:
	DOT_N
		{ $$ = &object::north; }
	| DOT_E	
		{ $$ = &object::east; }
	| DOT_W
		{ $$ = &object::west; }
	| DOT_S
		{ $$ = &object::south; }
	| DOT_NE
		{ $$ = &object::north_east; }
	| DOT_SE
		{ $$ = &object:: south_east; }
	| DOT_NW
		{ $$ = &object::north_west; }
	| DOT_SW
		{ $$ = &object::south_west; }
	| DOT_C
		{ $$ = &object::center; }
	| DOT_START
		{ $$ = &object::start; }
	| DOT_END
		{ $$ = &object::end; }
  	| TOP
		{ $$ = &object::north; }
	| BOTTOM
		{ $$ = &object::south; }
	| LEFT
		{ $$ = &object::west; }
	| RIGHT
		{ $$ = &object::east; }
	| UPPER LEFT
		{ $$ = &object::north_west; }
	| LOWER LEFT
		{ $$ = &object::south_west; }
	| UPPER RIGHT
		{ $$ = &object::north_east; }
	| LOWER RIGHT
		{ $$ = &object::south_east; }
	| LEFT_CORNER
		{ $$ = &object::west; }
	| RIGHT_CORNER
		{ $$ = &object::east; }
	| UPPER LEFT_CORNER
		{ $$ = &object::north_west; }
	| LOWER LEFT_CORNER
		{ $$ = &object::south_west; }
	| UPPER RIGHT_CORNER
		{ $$ = &object::north_east; }
	| LOWER RIGHT_CORNER
		{ $$ = &object::south_east; }
	| CENTER
		{ $$ = &object::center; }
	| START
		{ $$ = &object::start; }
	| END
		{ $$ = &object::end; }
	;

expr:
	VARIABLE
		{
		  if (!lookup_variable($1, & $$)) {
		    lex_error("there is no variable `%1'", $1);
		    YYABORT;
		  }
		  a_delete $1;
		}
	| NUMBER
		{ $$ = $1; }
	| place DOT_X
  		{
		  if ($1.obj != 0)
		    $$ = $1.obj->origin().x;
		  else
		    $$ = $1.x;
		}			
	| place DOT_Y
		{
		  if ($1.obj != 0)
		    $$ = $1.obj->origin().y;
		  else
		    $$ = $1.y;
		}
	| place DOT_HT
		{
		  if ($1.obj != 0)
		    $$ = $1.obj->height();
		  else
		    $$ = 0.0;
		}
	| place DOT_WID
		{
		  if ($1.obj != 0)
		    $$ = $1.obj->width();
		  else
		    $$ = 0.0;
		}
	| place DOT_RAD
		{
		  if ($1.obj != 0)
		    $$ = $1.obj->radius();
		  else
		    $$ = 0.0;
		}
	| expr '+' expr
		{ $$ = $1 + $3; }
	| expr '-' expr
		{ $$ = $1 - $3; }
	| expr '*' expr
		{ $$ = $1 * $3; }
	| expr '/' expr
		{
		  if ($3 == 0.0) {
		    lex_error("division by zero");
		    YYABORT;
		  }
		  $$ = $1/$3;
		}
	| expr '%' expr
		{
		  if ($3 == 0.0) {
		    lex_error("modulus by zero");
		    YYABORT;
		  }
		  $$ = fmod($1, $3);
		}
	| expr '^' expr
		{
		  errno = 0;
		  $$ = pow($1, $3);
		  if (errno == EDOM) {
		    lex_error("arguments to `^' operator out of domain");
		    YYABORT;
		  }
		  if (errno == ERANGE) {
		    lex_error("result of `^' operator out of range");
		    YYABORT;
		  }
		}
	| '-' expr    %prec '!'
		{ $$ = -$2; }
	| '(' any_expr ')'
		{ $$ = $2; }
	| SIN '(' any_expr ')'
		{
		  errno = 0;
		  $$ = sin($3);
		  if (errno == ERANGE) {
		    lex_error("sin result out of range");
		    YYABORT;
		  }
		}
	| COS '(' any_expr ')'
		{
		  errno = 0;
		  $$ = cos($3);
		  if (errno == ERANGE) {
		    lex_error("cos result out of range");
		    YYABORT;
		  }
		}
	| ATAN2 '(' any_expr ',' any_expr ')'
		{
		  errno = 0;
		  $$ = atan2($3, $5);
		  if (errno == EDOM) {
		    lex_error("atan2 argument out of domain");
		    YYABORT;
		  }
		  if (errno == ERANGE) {
		    lex_error("atan2 result out of range");
		    YYABORT;
		  }
		}
	| LOG '(' any_expr ')'
		{
		  errno = 0;
		  $$ = log10($3);
		  if (errno == ERANGE) {
		    lex_error("log result out of range");
		    YYABORT;
		  }
		}
	| EXP '(' any_expr ')'
		{
		  errno = 0;
		  $$ = pow(10.0, $3);
		  if (errno == ERANGE) {
		    lex_error("exp result out of range");
		    YYABORT;
		  }
		}
	| SQRT '(' any_expr ')'
		{
		  errno = 0;
		  $$ = sqrt($3);
		  if (errno == EDOM) {
		    lex_error("sqrt argument out of domain");
		    YYABORT;
		  }
		}
	| K_MAX '(' any_expr ',' any_expr ')'
		{ $$ = $3 > $5 ? $3 : $5; }
	| K_MIN '(' any_expr ',' any_expr ')'
		{ $$ = $3 < $5 ? $3 : $5; }
	| INT '(' any_expr ')'
		{ $$ = floor($3); }
	| RAND '(' any_expr ')'
		{ $$ = 1.0 + floor(((rand()&0x7fff)/double(0x7fff))*$3); }
	| RAND '(' ')'
		{
		  /* return a random number in the range [0,1) */
		  /* portable, but not very random */
		  $$ = (rand() & 0x7fff) / double(0x8000);
		}
	| SRAND '(' any_expr ')'
		{ $$ = 0; srand((unsigned int)$3); }
	| expr '<' expr
		{ $$ = ($1 < $3); }
	| expr LESSEQUAL expr
		{ $$ = ($1 <= $3); }
	| expr '>' expr
		{ $$ = ($1 > $3); }
	| expr GREATEREQUAL expr
		{ $$ = ($1 >= $3); }
	| expr EQUALEQUAL expr
		{ $$ = ($1 == $3); }
	| expr NOTEQUAL expr
		{ $$ = ($1 != $3); }
	| expr ANDAND expr
		{ $$ = ($1 != 0.0 && $3 != 0.0); }
	| expr OROR expr
		{ $$ = ($1 != 0.0 || $3 != 0.0); }
	| '!' expr
		{ $$ = ($2 == 0.0); }

	;

%%

/* bison defines const to be empty unless __STDC__ is defined, which it
isn't under cfront */

#ifdef const
#undef const
#endif

static struct {
  const char *name;
  double val;
  int scaled;		     // non-zero if val should be multiplied by scale
} defaults_table[] = {
  { "arcrad", .25, 1 },
  { "arrowht", .1, 1 },
  { "arrowwid", .05, 1 },
  { "circlerad", .25, 1 },
  { "boxht", .5, 1 },
  { "boxwid", .75, 1 },
  { "boxrad", 0.0, 1 },
  { "dashwid", .05, 1 },
  { "ellipseht", .5, 1 },
  { "ellipsewid", .75, 1 },
  { "moveht", .5, 1 },
  { "movewid", .5, 1 },
  { "lineht", .5, 1 },
  { "linewid", .5, 1 },
  { "textht", 0.0, 1 },
  { "textwid", 0.0, 1 },
  { "scale", 1.0, 0 },
  { "linethick", -1.0, 0 },		// in points
  { "fillval", .5, 0 },
  { "arrowhead", 1.0, 0 },
  { "maxpswid", 8.5, 0 },
  { "maxpsht", 11.0, 0 },
};

place *lookup_label(const char *label)
{
  saved_state *state = current_saved_state;
  PTABLE(place) *tbl = current_table;
  for (;;) {
    place *pl = tbl->lookup(label);
    if (pl)
      return pl;
    if (!state)
      return 0;
    tbl = state->tbl;
    state = state->prev;
  }
}

void define_label(const char *label, const place *pl)
{
  place *p = new place;
  *p = *pl;
  current_table->define(label, p);
}

int lookup_variable(const char *name, double *val)
{
  place *pl = lookup_label(name);
  if (pl) {
    *val = pl->x;
    return 1;
  }
  return 0;
}

void define_variable(const char *name, double val)
{
  place *p = new place;
  p->obj = 0;
  p->x = val;
  p->y = 0.0;
  current_table->define(name, p);
  if (strcmp(name, "scale") == 0) {
    // When the scale changes, reset all scaled pre-defined variables to
    // their default values.
    for (int i = 0; i < sizeof(defaults_table)/sizeof(defaults_table[0]); i++) 
      if (defaults_table[i].scaled)
	define_variable(defaults_table[i].name, val*defaults_table[i].val);
  }
}

// called once only (not once per parse)

void parse_init()
{
  current_direction = RIGHT_DIRECTION;
  current_position.x = 0.0;
  current_position.y = 0.0;
  // This resets everything to its default value.
  reset_all();
}

void reset(const char *nm)
{
  for (int i = 0; i < sizeof(defaults_table)/sizeof(defaults_table[0]); i++)
    if (strcmp(nm, defaults_table[i].name) == 0) {
      double val = defaults_table[i].val;
      if (defaults_table[i].scaled) {
	double scale;
	lookup_variable("scale", &scale);
	val *= scale;
      }
      define_variable(defaults_table[i].name, val);
      return;
    }
  lex_error("`%1' is not a predefined variable", nm);
}

void reset_all()
{
  // We only have to explicitly reset the pre-defined variables that
  // aren't scaled because `scale' is not scaled, and changing the
  // value of `scale' will reset all the pre-defined variables that
  // are scaled.
  for (int i = 0; i < sizeof(defaults_table)/sizeof(defaults_table[0]); i++)
    if (!defaults_table[i].scaled)
      define_variable(defaults_table[i].name, defaults_table[i].val);
}

// called after each parse

void parse_cleanup()
{
  while (current_saved_state != 0) {
    delete current_table;
    current_table = current_saved_state->tbl;
    saved_state *tem = current_saved_state;
    current_saved_state = current_saved_state->prev;
    delete tem;
  }
  assert(current_table == &top_table);
  PTABLE_ITERATOR(place) iter(current_table);
  const char *key;
  place *pl;
  while (iter.next(&key, &pl))
    if (pl->obj != 0) {
      position pos = pl->obj->origin();
      pl->obj = 0;
      pl->x = pos.x;
      pl->y = pos.y;
    }
  while (olist.head != 0) {
    object *tem = olist.head;
    olist.head = olist.head->next;
    delete tem;
  }
  olist.tail = 0;
  current_direction = RIGHT_DIRECTION;
  current_position.x = 0.0;
  current_position.y = 0.0;
}

const char *ordinal_postfix(int n)
{
  if (n < 10 || n > 20)
    switch (n % 10) {
    case 1:
      return "st";
    case 2:
      return "nd";
    case 3:
      return "rd";
    }
  return "th";
}

const char *object_type_name(object_type type)
{
  switch (type) {
  case BOX_OBJECT:
    return "box";
  case CIRCLE_OBJECT:
    return "circle";
  case ELLIPSE_OBJECT:
    return "ellipse";
  case ARC_OBJECT:
    return "arc";
  case SPLINE_OBJECT:
    return "spline";
  case LINE_OBJECT:
    return "line";
  case ARROW_OBJECT:
    return "arrow";
  case MOVE_OBJECT:
    return "move";
  case TEXT_OBJECT:
    return "\"\"";
  case BLOCK_OBJECT:
    return "[]";
  case OTHER_OBJECT:
  case MARK_OBJECT:
  default:
    break;
  }
  return "object";
}

static char sprintf_buf[1024];

char *format_number(const char *form, double n)
{
  if (form == 0)
    form = "%g";
  else {
    // this is a fairly feeble attempt at validation of the format
    int nspecs = 0;
    for (const char *p = form; *p != '\0'; p++)
      if (*p == '%') {
	if (p[1] == '%')
	  p++;
	else
	  nspecs++;
      }
    if (nspecs > 1) {
      lex_error("bad format `%1'", form);
      return strsave(form);
    }
  }
  sprintf(sprintf_buf, form, n);
  return strsave(sprintf_buf);
}

char *do_sprintf(const char *form, const double *v, int nv)
{
  string result;
  int i = 0;
  string one_format;
  while (*form) {
    if (*form == '%') {
      one_format += *form++;
      for (; *form != '\0' && strchr("#-+ 0123456789.", *form) != 0; form++)
	one_format += *form;
      if (*form == '\0' || strchr("eEfgG%", *form) == 0) {
	lex_error("bad sprintf format");
	result += one_format;
	result += form;
	break;
      }
      if (*form == '%') {
	one_format += *form++;
	one_format += '\0';
	sprintf(sprintf_buf, one_format.contents());
      }
      else {
	if (i >= nv) {
	  lex_error("too few arguments to sprintf");
	  result += one_format;
	  result += form;
	  break;
	}
	one_format += *form++;
	one_format += '\0';
	sprintf(sprintf_buf, one_format.contents(), v[i++]);
      }
      one_format.clear();
      result += sprintf_buf;
    }
    else
      result += *form++;
  }
  result += '\0';
  return strsave(result.contents());
}
