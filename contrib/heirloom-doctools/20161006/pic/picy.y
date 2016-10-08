%{
/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/pic/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)picy.y	1.4 (gritter) 11/28/05	*/

#include <stdio.h>
#include "pic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef	RAND_MAX
#define	RAND_MAX	32767
#endif

YYSTYPE	y;

extern	void	yyerror(char *);
extern	int	yylex(void);
%}

%token	<i>	BOX	1	/* DON'T CHANGE THESE! */
%token	<i>	LINE	2
%token	<i>	ARROW	3
%token	<i>	CIRCLE	4
%token	<i>	ELLIPSE	5
%token	<i>	ARC	6
%token	<i>	SPLINE	7
%token	<i>	BLOCK	8
%token	<p>	TEXT	9
%token	<p>	TROFF	10
%token	<i>	MOVE	11
%token	<i>	BLOCKEND 12
%token	<i>	PLACE	13
%token	<i>	PRINT RESET THRU UNTIL
%token	<o>	FOR IF COPY
%token	<p>	THENSTR ELSESTR DOSTR PLACENAME VARNAME SPRINTF
%token	<st>	DEFNAME
%token	<i>	ATTR TEXTATTR
%token	<i>	LEFT RIGHT UP DOWN FROM TO AT BY WITH HEAD CW CCW THEN
%token	<i>	HEIGHT WIDTH RADIUS DIAMETER LENGTH SIZE
%token	<i>	CORNER HERE LAST NTH SAME BETWEEN AND
%token	<i>	EAST WEST NORTH SOUTH NE NW SE SW START END
%token	<i>	DOTX DOTY DOTHT DOTWID DOTRAD
%token	<f>	NUMBER
%token	<f>	LOG EXP SIN COS ATAN2 SQRT RAND MIN MAX INT
%token	<i>	DIR
%token	<i>	DOT DASH CHOP FILL NOEDGE
%token	<o>	ST	/* statement terminator */

%right	<f>	'='
%left	<f>	OROR
%left	<f>	ANDAND
%nonassoc <f>	GT LT LE GE EQ NEQ
%left	<f>	'+' '-'
%left	<f>	'*' '/' '%'
%right	<f>	UMINUS NOT
%right	<f>	'^'

%type	<f>	expr if_expr asgn
%type	<p>	name text
%type	<i>	optop exprlist
%type	<o>	if for copy

/* this is a lie:  picture and position are really the whole union */
%type	<o>	leftbrace picture piclist position lbracket
%type	<o>	prim place blockname
%type	<i>	textlist textattr	/* not a sensible value */
%type	<i>	last type

%%

top:
	  piclist
	| /* empty */
	| error		{ WARNING("syntax error"); }
	;

piclist:
	  picture
	| piclist picture
	;

picture:
	  prim ST			{ codegen = 1; makeiattr(0, 0); }
	| leftbrace piclist '}'		{ rightthing($1, '}'); $$ = $2; }
	| PLACENAME ':' picture		{ y.o=$3; makevar($1,PLACENAME,y); $$ = $3; }
	| PLACENAME ':' ST picture	{ y.o=$4; makevar($1,PLACENAME,y); $$ = $4; }
	| PLACENAME ':' position ST	{ y.o=$3; makevar($1,PLACENAME,y); $$ = $3; }
	| asgn ST			{ y.f = $1; $$ = y.o; $$ = makenode(PLACE, 0); }
	| DIR				{ setdir($1); $$ = makenode(PLACE, 0); }
	| PRINT expr ST			{ printexpr($2); $$ = makenode(PLACE, 0); }
	| PRINT position ST		{ printpos($2); $$ = makenode(PLACE, 0); }
	| PRINT text ST			{ printf("%s\n", $2); free($2); $$ = makenode(PLACE, 0); }
	| RESET varlist ST		{ resetvar(); makeiattr(0, 0); $$ = makenode(PLACE, 0); }
	| copy
	| for
	| if
	| ST
	;

varlist:
	  /* empty */
	| VARNAME		{ makevattr($1); }
	| varlist VARNAME	{ makevattr($2); }
	| varlist ',' VARNAME	{ makevattr($3); }
	;

asgn:
	  VARNAME '=' expr	{ $$=y.f=$3; makevar($1,VARNAME,y); checkscale($1); }
	;

copy:
	  COPY copylist		{ copy(); }
	;
copylist:
	  copyattr
	| copylist copyattr
	;
copyattr:
	  text			{ copyfile($1); }
	| THRU DEFNAME		{ copydef($2); }
	| UNTIL text		{ copyuntil($2); }
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
	  IF if_expr THENSTR ELSESTR	{ ifstat($2, $3, $4); }
	| IF if_expr THENSTR		{ ifstat($2, $3, (char *) 0); }
	;
if_expr:
	  expr
	| text EQ text		{ $$ = strcmp($1,$3) == 0; free($1); free($3); }
	| text NEQ text		{ $$ = strcmp($1,$3) != 0; free($1); free($3); }
	;

name:
	  VARNAME	{ y.f = 0; makevar($1, VARNAME, y); }
	;
optop:
	  '+'		{ $$ = '+'; }
	| '-'		{ $$ = '-'; }
	| '*'		{ $$ = '*'; }
	| '/'		{ $$ = '/'; }
	| /* empty */	{ $$ = ' '; }
	;


leftbrace:
	  '{'			{ $$ = leftthing('{'); }
	;

prim:
	  BOX attrlist		{ $$ = boxgen(); }
	| CIRCLE attrlist	{ $$ = circgen($1); }
	| ELLIPSE attrlist	{ $$ = circgen($1); }
	| ARC attrlist		{ $$ = arcgen($1); }
	| LINE attrlist		{ $$ = linegen($1); }
	| ARROW attrlist	{ $$ = linegen($1); }
	| SPLINE attrlist	{ $$ = linegen($1); }
	| MOVE attrlist		{ $$ = movegen(); }
	| textlist attrlist	{ $$ = textgen(); }
	| TROFF			{ $$ = troffgen($1); }
	| lbracket piclist ']' { $<o>$=rightthing($1,']'); } attrlist
				{ $$ = blockgen($1, $<o>4); }
	;

lbracket:
	  '['			{ $$ = leftthing('['); }
	;

attrlist:
	  attrlist attr
	| /* empty */
	;

attr:
	  ATTR expr		{ makefattr($1, !DEFAULT, $2); }
	| ATTR			{ makefattr($1, DEFAULT, 0.0); }
	| expr			{ makefattr(curdir(), !DEFAULT, $1); }
	| DIR expr		{ makefattr($1, !DEFAULT, $2); }
	| DIR			{ makefattr($1, DEFAULT, 0.0); }
	| FROM position		{ makeoattr($1, $2); }
	| TO position		{ makeoattr($1, $2); }
	| AT position		{ makeoattr($1, $2); }
	| BY position		{ makeoattr($1, $2); }
	| WITH CORNER		{ makeiattr(WITH, $2); }
	| WITH '.' PLACENAME	{ makeoattr(PLACE, getblock(getlast(1,BLOCK), $3)); }
	| WITH '.' PLACENAME CORNER
		{ makeoattr(PLACE, getpos(getblock(getlast(1,BLOCK), $3), $4)); }
	| WITH position		{ makeoattr(PLACE, $2); }
	| SAME			{ makeiattr(SAME, $1); }
	| TEXTATTR		{ maketattr($1, (char *) 0); }
	| HEAD			{ makeiattr(HEAD, $1); }
	| DOT expr		{ makefattr(DOT, !DEFAULT, $2); }
	| DOT			{ makefattr(DOT, DEFAULT, 0.0); }
	| DASH expr		{ makefattr(DASH, !DEFAULT, $2); }
	| DASH			{ makefattr(DASH, DEFAULT, 0.0); }
	| CHOP expr		{ makefattr(CHOP, !DEFAULT, $2); }
	| CHOP			{ makefattr(CHOP, DEFAULT, 0.0); }
	| CHOP PLACENAME	{ makeattr(CHOP, PLACENAME, getvar($2)); }
	| FILL expr		{ makefattr(FILL, !DEFAULT, $2); }
	| FILL			{ makefattr(FILL, DEFAULT, 0.0); }
	| NOEDGE		{ makeiattr(NOEDGE, 0); }
	| textlist
	;

textlist:
	  textattr
	| textlist textattr
	;
textattr:
	  text			{ maketattr(CENTER, $1); }
	| text TEXTATTR		{ maketattr($2, $1); }
	| textattr TEXTATTR	{ addtattr($2); }
	;
text:
	  TEXT
	| SPRINTF '(' text ')'			{ $$ = sprintgen($3); }
	| SPRINTF '(' text ',' exprlist ')'	{ $$ = sprintgen($3); }
	;

exprlist:
	  expr			{ exprsave($1); $$ = 0; }
	| exprlist ',' expr	{ exprsave($3); }
	;

position:		/* absolute, not relative */
	  place
	| '(' position ')'			{ $$ = $2; }
	| expr ',' expr				{ $$ = makepos($1, $3); }
	| position '+' expr ',' expr		{ $$ = fixpos($1, $3, $5); }
	| position '-' expr ',' expr		{ $$ = fixpos($1, -$3, -$5); }
	| position '+' '(' expr ',' expr ')'	{ $$ = fixpos($1, $4, $6); }
	| position '-' '(' expr ',' expr ')'	{ $$ = fixpos($1, -$4, -$6); }
	| position '+' place			{ $$ = addpos($1, $3); }
	| position '-' place			{ $$ = subpos($1, $3); }
	| '(' place ',' place ')'	{ $$ = makepos(getcomp($2,DOTX), getcomp($4,DOTY)); }
	| expr LT position ',' position GT	{ $$ = makebetween($1, $3, $5); }
	| expr BETWEEN position AND position	{ $$ = makebetween($1, $3, $5); }
	;

place:
	  PLACENAME		{ y = getvar($1); $$ = y.o; }
	| PLACENAME CORNER	{ y = getvar($1); $$ = getpos(y.o, $2); }
	| CORNER PLACENAME	{ y = getvar($2); $$ = getpos(y.o, $1); }
	| HERE			{ $$ = gethere(); }
	| last type		{ $$ = getlast($1, $2); }
	| last type CORNER	{ $$ = getpos(getlast($1, $2), $3); }
	| CORNER last type	{ $$ = getpos(getlast($2, $3), $1); }
	| NTH type		{ $$ = getfirst($1, $2); }
	| NTH type CORNER	{ $$ = getpos(getfirst($1, $2), $3); }
	| CORNER NTH type	{ $$ = getpos(getfirst($2, $3), $1); }
	| blockname
	| blockname CORNER	{ $$ = getpos($1, $2); }
	| CORNER blockname	{ $$ = getpos($2, $1); }
	;

blockname:
	  last BLOCK '.' PLACENAME	{ $$ = getblock(getlast($1,$2), $4); }
	| NTH BLOCK '.' PLACENAME	{ $$ = getblock(getfirst($1,$2), $4); }
	| PLACENAME '.' PLACENAME	{ y = getvar($1); $$ = getblock(y.o, $3); }
	;

last:
	  last LAST		{ $$ = $1 + 1; }
	| NTH LAST		{ $$ = $1; }
	| LAST			{ $$ = 1; }
	;

type:
	  BOX
	| CIRCLE
	| ELLIPSE
	| ARC
	| LINE
	| ARROW
	| SPLINE
	| BLOCK
	;

expr:
	  NUMBER
	| VARNAME		{ $$ = getfval($1); }
	| asgn
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
	| place DOTX		{ $$ = getcomp($1, $2); }
	| place DOTY		{ $$ = getcomp($1, $2); }
	| place DOTHT		{ $$ = getcomp($1, $2); }
	| place DOTWID		{ $$ = getcomp($1, $2); }
	| place DOTRAD		{ $$ = getcomp($1, $2); }
	| PLACENAME '.' VARNAME	{ y = getvar($1); $$ = getblkvar(y.o, $3); }
	| last BLOCK '.' VARNAME { $$ = getblkvar(getlast($1,$2), $4); }
	| NTH BLOCK '.' VARNAME	{ $$ = getblkvar(getfirst($1,$2), $4); }
	| expr GT expr		{ $$ = $1 > $3; }
	| expr LT expr		{ $$ = $1 < $3; }
	| expr LE expr		{ $$ = $1 <= $3; }
	| expr GE expr		{ $$ = $1 >= $3; }
	| expr EQ expr		{ $$ = $1 == $3; }
	| expr NEQ expr		{ $$ = $1 != $3; }
	| expr ANDAND expr	{ $$ = $1 && $3; }
	| expr OROR expr	{ $$ = $1 || $3; }
	| NOT expr		{ $$ = !($2); }
	| LOG '(' expr ')'		{ $$ = Log10($3); }
	| EXP '(' expr ')'		{ $$ = Exp($3 * log(10.0)); }
	| expr '^' expr			{ $$ = pow($1, $3); }
	| SIN '(' expr ')'		{ $$ = sin($3); }
	| COS '(' expr ')'		{ $$ = cos($3); }
	| ATAN2 '(' expr ',' expr ')'	{ $$ = atan2($3, $5); }
	| SQRT '(' expr ')'		{ $$ = Sqrt($3); }
	| RAND '(' ')'			{ $$ = (float)random() / RAND_MAX; }
	| MAX '(' expr ',' expr ')'	{ $$ = $3 >= $5 ? $3 : $5; }
	| MIN '(' expr ',' expr ')'	{ $$ = $3 <= $5 ? $3 : $5; }
	| INT '(' expr ')'		{ $$ = (long) $3; }
	;
