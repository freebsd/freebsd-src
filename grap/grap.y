%{
/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/grap/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)grap.y	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "grap.h"

#ifndef	RAND_MAX
#define	RAND_MAX 32767	/* if your rand() returns bigger, change this too */
#endif

extern int yylex(void);
extern int yyparse(void);

%}

%token	<i>	FRAME TICKS GRID LABEL COORD
%token	<i>	LINE ARROW CIRCLE DRAW NEW PLOT NEXT
%token	<p>	PIC
%token	<i>	COPY THRU UNTIL
%token	<i>	FOR FROM TO BY AT WITH
%token	<i>	IF
%token	<p>	GRAPH THEN ELSE DOSTR
%token	<i>	DOT DASH INVIS SOLID
%token	<i>	TEXT JUST SIZE
%token	<i>	LOG EXP SIN COS ATAN2 SQRT RAND MAX MIN INT PRINT SPRINTF
%token	<i>	X Y SIDE IN OUT OFF UP DOWN ACROSS
%token	<i>	HEIGHT WIDTH RADIUS
%token	<f>	NUMBER
%token	<op>	NAME VARNAME DEFNAME
%token	<p>	STRING
%token	<i>	ST '(' ')' ','

%right	<f>	'='
%left	<f>	OR
%left	<f>	AND
%nonassoc <f>	GT LT LE GE EQ NE
%left	<f>	'+' '-'
%left	<f>	'*' '/' '%'
%right	<f>	UMINUS NOT
%right	<f>	'^'

%type	<f>	expr optexpr if_expr number assign
%type	<i>	optop
%type	<p>	optstring if
%type	<op>	optname iterator name
%type	<pt>	point
%type	<i>	side optside numlist comma linetype drawtype
%type	<ap>	linedesc optdesc stringlist string stringattr sattrlist exprlist
%type	<i>	frameitem framelist coordlog
%type	<f>	string_expr

%%

top:
	  graphseq		{ if (codegen && !synerr) graph((char *) 0); }
	| /* empty */		{ codegen = 0; }
	| error			{ codegen = 0; WARNING("syntax error"); }
	;

graphseq:
	  statlist
	| graph statlist
	| graphseq graph statlist
	;
graph:
	  GRAPH			{ graph($1); endstat(); }
	;

statlist:
	  ST
	| stat ST		{ endstat(); }
	| statlist stat ST	{ endstat(); }
	;

stat:
	  FRAME framelist	{ codegen = 1; }
	| ticks			{ codegen = 1; }
	| grid			{ codegen = 1; }
	| label			{ codegen = 1; }
	| coord
	| plot			{ codegen = 1; }
	| line			{ codegen = 1; }
	| circle		{ codegen = 1; }
	| draw
	| next			{ codegen = 1; }
	| PIC			{ codegen = 1; pic($1); }
	| for
	| if
	| copy
	| numlist		{ codegen = 1; numlist(); }
	| assign
	| PRINT expr		{ fprintf(stderr, "\t%g\n", $2); }
	| PRINT string		{ fprintf(stderr, "\t%s\n", $2->sval); freeattr($2); }
	| /* empty */
	;

numlist:
	  number		{ savenum(0, $1); $$ = 1; }
	| numlist number	{ savenum($1, $2); $$ = $1+1; }
	| numlist comma number	{ savenum($1, $3); $$ = $1+1; }
	;
number:
	  NUMBER
	| '-' NUMBER %prec UMINUS	{ $$ = -$2; }
	| '+' NUMBER %prec UMINUS	{ $$ = $2; }
	;

label:
	  LABEL optside stringlist lablist	{ label($2, $3); }
	;
lablist:
	  labattr
	| lablist labattr
	| /* empty */
	;
labattr:
	  UP expr		{ labelmove($1, $2); }
	| DOWN expr		{ labelmove($1, $2); }
	| SIDE expr		{ labelmove($1, $2); /* LEFT or RIGHT only */ }
	| WIDTH expr		{ labelwid($2); }
	;

framelist:
	  framelist frameitem
	| /* empty */		{ $$ = 0; }
	;
frameitem:
	  HEIGHT expr		{ frameht($2); }
	| WIDTH expr		{ framewid($2); }
	| side linedesc		{ frameside($1, $2); }
	| linedesc		{ frameside(0, $1); }
	;
side:
	  SIDE
	;
optside:
	  side
	| /* empty */		{ $$ = 0; }
	;

linedesc:
	  linetype optexpr	{ $$ = makeattr($1, $2, (char *) 0, 0, 0); }
	;
linetype:
	  DOT | DASH | SOLID | INVIS
	;
optdesc:
	  linedesc
	| /* empty */		{ $$ = makeattr(0, 0.0, (char *) 0, 0, 0); }
	;

ticks:
	  TICKS tickdesc	{ ticks(); }
	;
tickdesc:
	  tickattr
	| tickdesc tickattr
	;
tickattr:
	  side			{ tickside($1); }
	| IN expr		{ tickdir(IN, $2, 1); }
	| OUT expr		{ tickdir(OUT, $2, 1); }
	| IN			{ tickdir(IN, 0.0, 0); }
	| OUT			{ tickdir(OUT, 0.0, 0); }
	| AT optname ticklist	{ setlist(); ticklist($2, AT); }
	| iterator		{ setlist(); ticklist($1, AT); }
	| side OFF		{ tickoff($1); }
	| OFF			{ tickoff(LEFT|RIGHT|TOP|BOT); }
	| labattr
	;
ticklist:
	  tickpoint
	| ticklist comma tickpoint
	;
tickpoint:
	  expr			{ savetick($1, (char *) 0); }
	| expr string		{ savetick($1, $2->sval); }
	;
iterator:
	  FROM optname expr TO optname expr BY optop expr optstring
			{ iterator($3, $6, $8, $9, $10); $$ = $2; }
	| FROM optname expr TO optname expr optstring
			{ iterator($3, $6, '+', 1.0, $7); $$ = $2; }
	;
optop:
	  '+'		{ $$ = '+'; }
	| '-'		{ $$ = '-'; }
	| '*'		{ $$ = '*'; }
	| '/'		{ $$ = '/'; }
	| /* empty */	{ $$ = ' '; }
	;
optstring:
	  string	{ $$ = $1->sval; }
	| /* empty */	{ $$ = (char *) 0; }
	;

grid:
	  GRID griddesc		{ ticks(); }
	;
griddesc:
	  gridattr
	| griddesc gridattr
	;
gridattr:
	  side			{ tickside($1); }
	| X			{ tickside(BOT); }
	| Y			{ tickside(LEFT); }
	| linedesc		{ griddesc($1); }
	| AT optname ticklist	{ setlist(); gridlist($2); }
	| iterator		{ setlist(); gridlist($1); }
	| TICKS OFF		{ gridtickoff(); }
	| OFF			{ gridtickoff(); }
	| labattr
	;

line:
	  LINE FROM point TO point optdesc	{ line($1, $3, $5, $6); }
	| LINE optdesc FROM point TO point	{ line($1, $4, $6, $2); }
	;
circle:
	  CIRCLE RADIUS expr AT point		{ circle($3, $5); }
	| CIRCLE AT point RADIUS expr		{ circle($5, $3); }
	| CIRCLE AT point			{ circle(0.0, $3); }
	;

stringlist:
	  string
	| stringlist string	{ $$ = addattr($1, $2); }
	;
string:
	  STRING sattrlist	{ $$ = makesattr($1); }
	| SPRINTF '(' STRING ')' sattrlist
				{ $$ = makesattr(sprntf($3, (Attr*) 0)); }
	| SPRINTF '(' STRING ',' exprlist ')' sattrlist
				{ $$ = makesattr(sprntf($3, $5)); }
	;
exprlist:
	  expr			{ $$ = makefattr(NUMBER, $1); }
	| exprlist ',' expr	{ $$ = addattr($1, makefattr(NUMBER, $3)); }
	;
sattrlist:
	  stringattr
	| sattrlist stringattr
	| /* empty */		{ $$ = (Attr *) 0; }
	;
stringattr:
	  JUST			{ setjust($1); }
	| SIZE optop expr	{ setsize($2, $3); }
	;

coord:
	  COORD optname coordlist	{ coord($2); }
	| COORD optname			{ resetcoord($2); }
	;
coordlist:
	  coorditem
	| coordlist coorditem
	;
coorditem:
	  coordlog	{ coordlog($1); }
	| X point	{ coord_x($2); }
	| Y point	{ coord_y($2); }
	| X optname expr TO expr		{ coord_x(makepoint($2, $3, $5)); }
	| Y optname expr TO expr		{ coord_y(makepoint($2, $3, $5)); }
	| X FROM optname expr TO expr		{ coord_x(makepoint($3, $4, $6)); }
	| Y FROM optname expr TO expr		{ coord_y(makepoint($3, $4, $6)); }
	;
coordlog:
	  LOG X		{ $$ = XFLAG; }
	| LOG Y		{ $$ = YFLAG; }
	| LOG X LOG Y	{ $$ = XFLAG|YFLAG; }
	| LOG Y LOG X	{ $$ = XFLAG|YFLAG; }
	| LOG LOG	{ $$ = XFLAG|YFLAG; }
	;

plot:
	  stringlist AT point		{ plot($1, $3); }
	| PLOT stringlist AT point	{ plot($2, $4); }
	| PLOT expr optstring AT point	{ plotnum($2, $3, $5); }
	;

draw:
	  drawtype optname linedesc		{ drawdesc($1, $2, $3, (char *) 0); }
	| drawtype optname optdesc string	{ drawdesc($1, $2, $3, $4->sval); }
	| drawtype optname string optdesc	{ drawdesc($1, $2, $4, $3->sval); }
	;
drawtype:
	  DRAW
	| NEW
	;

next:
	  NEXT optname AT point optdesc		{ next($2, $4, $5); }

copy:
	  COPY copylist		{ copy(); }
	;
copylist:
	  copyattr
	| copylist copyattr
	;
copyattr:
	  string		{ copyfile($1->sval); }
	| THRU DEFNAME		{ copydef($2); }
	| UNTIL string		{ copyuntil($2->sval); }
	;

for:
	  FOR name FROM expr TO expr BY optop expr DOSTR
		{ forloop($2, $4, $6, $8, $9, $10); }
	| FOR name FROM expr TO expr DOSTR
		{ forloop($2, $4, $6, '+', 1.0, $7); }
	| FOR name '=' expr TO expr BY optop expr DOSTR
		{ forloop($2, $4, $6, $8, $9, $10); }
	| FOR name '=' expr TO expr DOSTR
		{ forloop($2, $4, $6, '+', 1.0, $7); }
	;

if:
	  IF if_expr THEN ELSE		{ $$ = ifstat($2, $3, $4); }
	| IF if_expr THEN		{ $$ = ifstat($2, $3, (char *) 0); }
	;
if_expr:
	  expr
	| string_expr
	| if_expr AND string_expr	{ $$ = $1 && $3; }
	| if_expr OR string_expr	{ $$ = $1 || $3; }
	;
string_expr:
	  STRING EQ STRING	{ $$ = strcmp($1,$3) == 0; free($1); free($3); }
	| STRING NE STRING	{ $$ = strcmp($1,$3) != 0; free($1); free($3); }
	;

point:
	  optname expr comma expr		{ $$ = makepoint($1, $2, $4); }
	| optname '(' expr comma expr ')'	{ $$ = makepoint($1, $3, $5); }
	;
comma:
	  ','		{ $$ = ','; }
	;

optname:
	  NAME		{ $$ = $1; }
	| /* empty */	{ $$ = lookup(curr_coord, 1); }
	;

expr:
	  NUMBER
	| assign
	| '(' string_expr ')'	{ $$ = $2; }
	| VARNAME		{ $$ = getvar($1); }
	| expr '+' expr		{ $$ = $1 + $3; }
	| expr '-' expr		{ $$ = $1 - $3; }
	| expr '*' expr		{ $$ = $1 * $3; }
	| expr '/' expr		{ if ($3 == 0.0) {
					WARNING("division by 0"); $3 = 1; }
				  $$ = $1 / $3; }
	| expr '%' expr		{ if ((long)$3 == 0) {
					WARNING("mod division by 0"); $3 = 1; }
				  $$ = (long)$1 % (long)$3; }
	| '-' expr %prec UMINUS	{ $$ = -$2; }
	| '+' expr %prec UMINUS	{ $$ = $2; }
	| '(' expr ')'		{ $$ = $2; }
	| LOG '(' expr ')'		{ $$ = Log10($3); }
	| EXP '(' expr ')'		{ $$ = Exp($3 * log(10.0)); }
	| expr '^' expr			{ $$ = pow($1, $3); }
	| SIN '(' expr ')'		{ $$ = sin($3); }
	| COS '(' expr ')'		{ $$ = cos($3); }
	| ATAN2 '(' expr ',' expr ')'	{ $$ = atan2($3, $5); }
	| SQRT '(' expr ')'		{ $$ = Sqrt($3); }
	| RAND '(' ')'			{ $$ = (double)random() / (double)RAND_MAX; }
	| MAX '(' expr ',' expr ')'	{ $$ = $3 >= $5 ? $3 : $5; }
	| MIN '(' expr ',' expr ')'	{ $$ = $3 <= $5 ? $3 : $5; }
	| INT '(' expr ')'	{ $$ = (long) $3; }
	| expr GT expr		{ $$ = $1 > $3; }
	| expr LT expr		{ $$ = $1 < $3; }
	| expr LE expr		{ $$ = $1 <= $3; }
	| expr GE expr		{ $$ = $1 >= $3; }
	| expr EQ expr		{ $$ = $1 == $3; }
	| expr NE expr		{ $$ = $1 != $3; }
	| expr AND expr		{ $$ = $1 && $3; }
	| expr OR expr		{ $$ = $1 || $3; }
	| NOT expr		{ $$ = !($2); }
	;
assign:
	  name '=' expr		{ $$ = setvar($1, $3); }
	;

name:
	  NAME
	| VARNAME
	;

optexpr:
	  expr
	| /* empty */		{ $$ = 0.0; }
	;
