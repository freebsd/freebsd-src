%{
/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
%}
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


%{#
/*	from "e.y	1.6	05/06/10 SMI"	"ucbeqn:e.y 1.1" */

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)e.y	1.7 (gritter) 10/2/07
 */
/*
 * Changes Copyright (c) 2014 Carsten Kunze (carsten.kunze at arcor.de)
 */
#include "e.h"
#include <stdlib.h>
#include <inttypes.h>

int	fromflg;

#define	__YYSCLASS	/* to get external access to yyval with HP-UX yacc */
%}
%union {
	int token;
	char *str;
}
%token <str> CONTIG QTEXT SPACE THIN TAB
%token <token> MATRIX LCOL CCOL RCOL COL
%token <token> MARK LINEUP
%token <token> SUM INT PROD UNION INTER
%token <token> LPILE PILE CPILE RPILE ABOVE
%token <token> DEFINE TDEFINE NDEFINE DELIM GSIZE GFONT INCLUDE
%type <str> text
%type <token> eqn box lineupbox matrix lcol ccol rcol col sbox tbox size font
%type <token> lpile cpile rpile pile sub sup int left right diacrit fwd up back
%type <token> down from to pbox
%right	FROM TO
%left	OVER SQRT
%right	SUP SUB
%right	SIZE FONT ROMAN ITALIC BOLD FAT
%right	UP DOWN BACK FWD
%left	LEFT RIGHT
%right	DOT DOTDOT HAT TILDE BAR UNDER VEC DYAD

%%

stuff	: eqn 	{ putout($1); }
	| error	{ error(!FATAL, "syntax error"); }
	|	{ eqnreg = 0; }
	;

eqn	: box
	| eqn box	{ eqnbox($1, $2, 0); }
	| eqn lineupbox	{ eqnbox($1, $2, 1); }
	| LINEUP	{ lineup(0); }
	;

lineupbox: LINEUP box	{ $$ = $2; lineup(1); }
	;

matrix	: MATRIX	{ $$ = ct; } ;

collist	: column
	| collist column
	;

column	: lcol '{' list '}'	{ column('L', $1); }
	| ccol '{' list '}'	{ column('C', $1); }
	| rcol '{' list '}'	{ column('R', $1); }
	| col '{' list '}'	{ column('-', $1); }
	;

lcol	: LCOL		{ $$ = ct++; } ;
ccol	: CCOL		{ $$ = ct++; } ;
rcol	: RCOL		{ $$ = ct++; } ;
col	: COL		{ $$ = ct++; } ;

sbox	: sup box	%prec SUP	{ $$ = $2; }
	;

tbox	: to box	%prec TO	{ $$ = $2; }
	|		%prec FROM	{ $$ = 0; }
	;

box	: box OVER box	{ boverb($1, $3); }
	| MARK box	{ mark($2); }
	| size box	%prec SIZE	{ size($1, $2); }
	| font box	%prec FONT	{ font($1, $2); }
	| FAT box	{ fatbox($2); }
	| SQRT box	{ sqrt($2); }
	| lpile '{' list '}'	{ lpile('L', $1, ct); ct = $1; }
	| cpile '{' list '}'	{ lpile('C', $1, ct); ct = $1; }
	| rpile '{' list '}'	{ lpile('R', $1, ct); ct = $1; }
	| pile '{' list '}'	{ lpile('-', $1, ct); ct = $1; }
	| box sub box sbox	%prec SUB	{ shift2($1, $3, $4); }
	| box sub box		%prec SUB	{ bshiftb($1, $2, $3); }
	| box sup box		%prec SUP	{ bshiftb($1, $2, $3); }
	| int sub box sbox	%prec SUB	{ integral($1, $3, $4); }
	| int sub box		%prec SUB	{ integral($1, $3, 0); }
	| int sup box		%prec SUP	{ integral($1, 0, $3); }
	| int					{ integral($1, 0, 0); }
	| left eqn right	{ paren($1, $2, $3); }
	| pbox
	| box from box tbox	%prec FROM	{ fromto($1, $3, $4); fromflg=0; }
	| box to box	%prec TO	{ fromto($1, 0, $3); }
	| box diacrit	{ diacrit($1, $2); }
	| fwd box	%prec UP	{ move(FWD, $1, $2); }
	| up box	%prec UP	{ move(UP, $1, $2); }
	| back box	%prec UP	{ move(BACK, $1, $2); }
	| down box	%prec UP	{ move(DOWN, $1, $2); }
	| matrix '{' collist '}'	{ matrix($1); }
	;

int	: INT	{ setintegral(); }
	;

fwd	: FWD text	{ $$ = atoi((char *) $2); } ;
up	: UP text	{ $$ = atoi((char *) $2); } ;
back	: BACK text	{ $$ = atoi((char *) $2); } ;
down	: DOWN text	{ $$ = atoi((char *) $2); } ;

diacrit	: HAT	{ $$ = HAT; }
	| VEC	{ $$ = VEC; }
	| DYAD	{ $$ = DYAD; }
	| BAR	{ $$ = BAR; }
	| UNDER	{ $$ = UNDER; }	/* under bar */
	| DOT	{ $$ = DOT; }
	| TILDE	{ $$ = TILDE; }
	| DOTDOT	{ $$ = DOTDOT; } /* umlaut = double dot */
	;

from	: FROM	{ $$=ps; ps -= 3; fromflg = 1;
		if(dbg)printf(".\tfrom: old ps %d, new ps %g, fflg %d\n", $$, ps, fromflg);
		}
	;

to	: TO	{ $$=ps; if(fromflg==0)ps -= 3; 
			if(dbg)printf(".\tto: old ps %d, new ps %g\n", $$, ps);
		}
	;

left	: LEFT text	{ $$ = ((char *)$2)[0]; }
	| LEFT '{'	{ $$ = '{'; }
	;

right	: RIGHT text	{ $$ = ((char *)$2)[0]; }
	| RIGHT '}'	{ $$ = '}'; }
	|		{ $$ = 0; }
	;

list	: eqn	{ lp[ct++] = $1; }
	| list ABOVE eqn	{ lp[ct++] = $3; }
	;

lpile	: LPILE	{ $$ = ct; } ;
cpile	: CPILE	{ $$ = ct; } ;
pile	: PILE	{ $$ = ct; } ;
rpile	: RPILE	{ $$ = ct; } ;

size	: SIZE text	{ $$ = ps; setsize((char *) $2); }
	;

font	: ROMAN		{ setfont(ROM); }
	| ITALIC	{ setfont(ITAL); }
	| BOLD		{ setfont(BLD); }
	| FONT text	{ setfont(((char *)$2)[0]); }
	;

sub	: SUB	{ shift(SUB); }
	;

sup	: SUP	{ shift(SUP); }
	;

pbox	: '{' eqn '}'	{ $$ = $2; }
	| QTEXT		{ text(QTEXT, (char *) $1); }
	| CONTIG	{ text(CONTIG, (char *) $1); }
	| SPACE		{ text(SPACE, 0); }
	| THIN		{ text(THIN, 0); }
	| TAB		{ text(TAB, 0); }
	| SUM		{ funny(SUM); }
	| PROD		{ funny(PROD); }
	| UNION		{ funny(UNION); }
	| INTER		{ funny(INTER); }	/* intersection */
	;

text	: CONTIG
	| QTEXT
	;

%%
