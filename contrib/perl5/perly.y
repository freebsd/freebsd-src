/*    perly.y
 *
 *    Copyright (c) 1991-1999, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

/*
 * 'I see,' laughed Strider.  'I look foul and feel fair.  Is that it?
 * All that is gold does not glitter, not all those who wander are lost.'
 */

%{
#include "EXTERN.h"
#include "perl.h"

static void
dep(void)
{
    deprecate("\"do\" to call subroutines");
}

%}

%start prog

%{
#ifndef OEMVS
%}

%union {
    I32	ival;
    char *pval;
    OP *opval;
    GV *gvval;
}

%{
#endif /* OEMVS */
%}

%token <ival> '{' ')'

%token <opval> WORD METHOD FUNCMETH THING PMFUNC PRIVATEREF
%token <opval> FUNC0SUB UNIOPSUB LSTOPSUB
%token <pval> LABEL
%token <ival> FORMAT SUB ANONSUB PACKAGE USE
%token <ival> WHILE UNTIL IF UNLESS ELSE ELSIF CONTINUE FOR
%token <ival> LOOPEX DOTDOT
%token <ival> FUNC0 FUNC1 FUNC UNIOP LSTOP
%token <ival> RELOP EQOP MULOP ADDOP
%token <ival> DOLSHARP DO HASHBRACK NOAMP
%token LOCAL MY

%type <ival> prog decl local format startsub startanonsub startformsub
%type <ival> remember mremember '&'
%type <opval> block mblock lineseq line loop cond else
%type <opval> expr term scalar ary hsh arylen star amper sideff
%type <opval> argexpr nexpr texpr iexpr mexpr mnexpr mtexpr miexpr
%type <opval> listexpr listexprcom indirob listop method
%type <opval> formname subname proto subbody cont my_scalar
%type <pval> label

%left <ival> OROP
%left ANDOP
%right NOTOP
%nonassoc LSTOP LSTOPSUB
%left ','
%right <ival> ASSIGNOP
%right '?' ':'
%nonassoc DOTDOT
%left OROR
%left ANDAND
%left <ival> BITOROP
%left <ival> BITANDOP
%nonassoc EQOP
%nonassoc RELOP
%nonassoc UNIOP UNIOPSUB
%left <ival> SHIFTOP
%left ADDOP
%left MULOP
%left <ival> MATCHOP
%right '!' '~' UMINUS REFGEN
%right <ival> POWOP
%nonassoc PREINC PREDEC POSTINC POSTDEC
%left ARROW
%left '('

%% /* RULES */

prog	:	/* NULL */
		{
#if defined(YYDEBUG) && defined(DEBUGGING)
		    yydebug = (PL_debug & 1);
#endif
		    PL_expect = XSTATE;
		}
	/*CONTINUED*/	lineseq
			{ newPROG($2); }
	;

block	:	'{' remember lineseq '}'
			{ if (PL_copline > (line_t)$1)
			      PL_copline = $1;
			  $$ = block_end($2, $3); }
	;

remember:	/* NULL */	/* start a full lexical scope */
			{ $$ = block_start(TRUE); }
	;

mblock	:	'{' mremember lineseq '}'
			{ if (PL_copline > (line_t)$1)
			      PL_copline = $1;
			  $$ = block_end($2, $3); }
	;

mremember:	/* NULL */	/* start a partial lexical scope */
			{ $$ = block_start(FALSE); }
	;

lineseq	:	/* NULL */
			{ $$ = Nullop; }
	|	lineseq decl
			{ $$ = $1; }
	|	lineseq line
			{   $$ = append_list(OP_LINESEQ,
				(LISTOP*)$1, (LISTOP*)$2);
			    PL_pad_reset_pending = TRUE;
			    if ($1 && $2) PL_hints |= HINT_BLOCK_SCOPE; }
	;

line	:	label cond
			{ $$ = newSTATEOP(0, $1, $2); }
	|	loop	/* loops add their own labels */
	|	label ';'
			{ if ($1 != Nullch) {
			      $$ = newSTATEOP(0, $1, newOP(OP_NULL, 0));
			    }
			    else {
			      $$ = Nullop;
			      PL_copline = NOLINE;
			    }
			    PL_expect = XSTATE; }
	|	label sideff ';'
			{ $$ = newSTATEOP(0, $1, $2);
			  PL_expect = XSTATE; }
	;

sideff	:	error
			{ $$ = Nullop; }
	|	expr
			{ $$ = $1; }
	|	expr IF expr
			{ $$ = newLOGOP(OP_AND, 0, $3, $1); }
	|	expr UNLESS expr
			{ $$ = newLOGOP(OP_OR, 0, $3, $1); }
	|	expr WHILE expr
			{ $$ = newLOOPOP(OPf_PARENS, 1, scalar($3), $1); }
	|	expr UNTIL iexpr
			{ $$ = newLOOPOP(OPf_PARENS, 1, $3, $1);}
	|	expr FOR expr
			{ $$ = newFOROP(0, Nullch, $2,
					Nullop, $3, $1, Nullop); }
	;

else	:	/* NULL */
			{ $$ = Nullop; }
	|	ELSE mblock
			{ $$ = scope($2); }
	|	ELSIF '(' mexpr ')' mblock else
			{ PL_copline = $1;
			    $$ = newSTATEOP(0, Nullch,
				   newCONDOP(0, $3, scope($5), $6));
			    PL_hints |= HINT_BLOCK_SCOPE; }
	;

cond	:	IF '(' remember mexpr ')' mblock else
			{ PL_copline = $1;
			    $$ = block_end($3,
				   newCONDOP(0, $4, scope($6), $7)); }
	|	UNLESS '(' remember miexpr ')' mblock else
			{ PL_copline = $1;
			    $$ = block_end($3,
				   newCONDOP(0, $4, scope($6), $7)); }
	;

cont	:	/* NULL */
			{ $$ = Nullop; }
	|	CONTINUE block
			{ $$ = scope($2); }
	;

loop	:	label WHILE '(' remember mtexpr ')' mblock cont
			{ PL_copline = $2;
			    $$ = block_end($4,
				   newSTATEOP(0, $1,
				     newWHILEOP(0, 1, (LOOP*)Nullop,
						$2, $5, $7, $8))); }
	|	label UNTIL '(' remember miexpr ')' mblock cont
			{ PL_copline = $2;
			    $$ = block_end($4,
				   newSTATEOP(0, $1,
				     newWHILEOP(0, 1, (LOOP*)Nullop,
						$2, $5, $7, $8))); }
	|	label FOR MY remember my_scalar '(' mexpr ')' mblock cont
			{ $$ = block_end($4,
				 newFOROP(0, $1, $2, $5, $7, $9, $10)); }
	|	label FOR scalar '(' remember mexpr ')' mblock cont
			{ $$ = block_end($5,
				 newFOROP(0, $1, $2, mod($3, OP_ENTERLOOP),
					  $6, $8, $9)); }
	|	label FOR '(' remember mexpr ')' mblock cont
			{ $$ = block_end($4,
				 newFOROP(0, $1, $2, Nullop, $5, $7, $8)); }
	|	label FOR '(' remember mnexpr ';' mtexpr ';' mnexpr ')' mblock
			/* basically fake up an initialize-while lineseq */
			{ OP *forop = append_elem(OP_LINESEQ,
					scalar($5),
					newWHILEOP(0, 1, (LOOP*)Nullop,
						   $2, scalar($7),
						   $11, scalar($9)));
			  PL_copline = $2;
			  $$ = block_end($4, newSTATEOP(0, $1, forop)); }
	|	label block cont  /* a block is a loop that happens once */
			{ $$ = newSTATEOP(0, $1,
				 newWHILEOP(0, 1, (LOOP*)Nullop,
					    NOLINE, Nullop, $2, $3)); }
	;

nexpr	:	/* NULL */
			{ $$ = Nullop; }
	|	sideff
	;

texpr	:	/* NULL means true */
			{ (void)scan_num("1"); $$ = yylval.opval; }
	|	expr
	;

iexpr	:	expr
			{ $$ = invert(scalar($1)); }
	;

mexpr	:	expr
			{ $$ = $1; intro_my(); }
	;

mnexpr	:	nexpr
			{ $$ = $1; intro_my(); }
	;

mtexpr	:	texpr
			{ $$ = $1; intro_my(); }
	;

miexpr	:	iexpr
			{ $$ = $1; intro_my(); }
	;

label	:	/* empty */
			{ $$ = Nullch; }
	|	LABEL
	;

decl	:	format
			{ $$ = 0; }
	|	subrout
			{ $$ = 0; }
	|	package
			{ $$ = 0; }
	|	use
			{ $$ = 0; }
	;

format	:	FORMAT startformsub formname block
			{ newFORM($2, $3, $4); }
	;

formname:	WORD		{ $$ = $1; }
	|	/* NULL */	{ $$ = Nullop; }
	;

subrout	:	SUB startsub subname proto subbody
			{ newSUB($2, $3, $4, $5); }
	;

startsub:	/* NULL */	/* start a regular subroutine scope */
			{ $$ = start_subparse(FALSE, 0); }
	;

startanonsub:	/* NULL */	/* start an anonymous subroutine scope */
			{ $$ = start_subparse(FALSE, CVf_ANON); }
	;

startformsub:	/* NULL */	/* start a format subroutine scope */
			{ $$ = start_subparse(TRUE, 0); }
	;

subname	:	WORD	{ STRLEN n_a; char *name = SvPV(((SVOP*)$1)->op_sv, n_a);
			  if (strEQ(name, "BEGIN") || strEQ(name, "END")
			      || strEQ(name, "INIT"))
			      CvSPECIAL_on(PL_compcv);
			  $$ = $1; }
	;

proto	:	/* NULL */
			{ $$ = Nullop; }
	|	THING
	;

subbody	:	block	{ $$ = $1; }
	|	';'	{ $$ = Nullop; PL_expect = XSTATE; }
	;

package :	PACKAGE WORD ';'
			{ package($2); }
	|	PACKAGE ';'
			{ package(Nullop); }
	;

use	:	USE startsub
			{ CvSPECIAL_on(PL_compcv); /* It's a BEGIN {} */ }
		    WORD WORD listexpr ';'
			{ utilize($1, $2, $4, $5, $6); }
	;

expr	:	expr ANDOP expr
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	expr OROP expr
			{ $$ = newLOGOP($2, 0, $1, $3); }
	|	argexpr
	;

argexpr	:	argexpr ','
			{ $$ = $1; }
	|	argexpr ',' term
			{ $$ = append_elem(OP_LIST, $1, $3); }
	|	term
	;

listop	:	LSTOP indirob argexpr
			{ $$ = convert($1, OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF($1,$2), $3) ); }
	|	FUNC '(' indirob expr ')'
			{ $$ = convert($1, OPf_STACKED,
				prepend_elem(OP_LIST, newGVREF($1,$3), $4) ); }
	|	term ARROW method '(' listexprcom ')'
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, scalar($1), $5),
				    newUNOP(OP_METHOD, 0, $3))); }
	|	METHOD indirob listexpr
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $3),
				    newUNOP(OP_METHOD, 0, $1))); }
	|	FUNCMETH indirob '(' listexprcom ')'
			{ $$ = convert(OP_ENTERSUB, OPf_STACKED,
				append_elem(OP_LIST,
				    prepend_elem(OP_LIST, $2, $4),
				    newUNOP(OP_METHOD, 0, $1))); }
	|	LSTOP listexpr
			{ $$ = convert($1, 0, $2); }
	|	FUNC '(' listexprcom ')'
			{ $$ = convert($1, 0, $3); }
	|	LSTOPSUB startanonsub block
			{ $3 = newANONSUB($2, 0, $3); }
		    listexpr		%prec LSTOP
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				 append_elem(OP_LIST,
				   prepend_elem(OP_LIST, $3, $5), $1)); }
	;

method	:	METHOD
	|	scalar
	;

term	:	term ASSIGNOP term
			{ $$ = newASSIGNOP(OPf_STACKED, $1, $2, $3); }
	|	term POWOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term MULOP term
			{   if ($2 != OP_REPEAT)
				scalar($1);
			    $$ = newBINOP($2, 0, $1, scalar($3)); }
	|	term ADDOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term SHIFTOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term RELOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term EQOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term BITANDOP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term BITOROP term
			{ $$ = newBINOP($2, 0, scalar($1), scalar($3)); }
	|	term DOTDOT term
			{ $$ = newRANGE($2, scalar($1), scalar($3));}
	|	term ANDAND term
			{ $$ = newLOGOP(OP_AND, 0, $1, $3); }
	|	term OROR term
			{ $$ = newLOGOP(OP_OR, 0, $1, $3); }
	|	term '?' term ':' term
			{ $$ = newCONDOP(0, $1, $3, $5); }
	|	term MATCHOP term
			{ $$ = bind_match($2, $1, $3); }

	|	'-' term %prec UMINUS
			{ $$ = newUNOP(OP_NEGATE, 0, scalar($2)); }
	|	'+' term %prec UMINUS
			{ $$ = $2; }
	|	'!' term
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	'~' term
			{ $$ = newUNOP(OP_COMPLEMENT, 0, scalar($2));}
	|	REFGEN term
			{ $$ = newUNOP(OP_REFGEN, 0, mod($2,OP_REFGEN)); }
	|	term POSTINC
			{ $$ = newUNOP(OP_POSTINC, 0,
					mod(scalar($1), OP_POSTINC)); }
	|	term POSTDEC
			{ $$ = newUNOP(OP_POSTDEC, 0,
					mod(scalar($1), OP_POSTDEC)); }
	|	PREINC term
			{ $$ = newUNOP(OP_PREINC, 0,
					mod(scalar($2), OP_PREINC)); }
	|	PREDEC term
			{ $$ = newUNOP(OP_PREDEC, 0,
					mod(scalar($2), OP_PREDEC)); }
	|	local term	%prec UNIOP
			{ $$ = localize($2,$1); }
	|	'(' expr ')'
			{ $$ = sawparens($2); }
	|	'(' ')'
			{ $$ = sawparens(newNULLLIST()); }
	|	'[' expr ']'				%prec '('
			{ $$ = newANONLIST($2); }
	|	'[' ']'					%prec '('
			{ $$ = newANONLIST(Nullop); }
	|	HASHBRACK expr ';' '}'			%prec '('
			{ $$ = newANONHASH($2); }
	|	HASHBRACK ';' '}'				%prec '('
			{ $$ = newANONHASH(Nullop); }
	|	ANONSUB startanonsub proto block		%prec '('
			{ $$ = newANONSUB($2, $3, $4); }
	|	scalar	%prec '('
			{ $$ = $1; }
	|	star '{' expr ';' '}'
			{ $$ = newBINOP(OP_GELEM, 0, $1, scalar($3)); }
	|	star	%prec '('
			{ $$ = $1; }
	|	scalar '[' expr ']'	%prec '('
			{ $$ = newBINOP(OP_AELEM, 0, oopsAV($1), scalar($3)); }
	|	term ARROW '[' expr ']'	%prec '('
			{ $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($4));}
	|	term '[' expr ']'	%prec '('
			{ assertref($1); $$ = newBINOP(OP_AELEM, 0,
					ref(newAVREF($1),OP_RV2AV),
					scalar($3));}
	|	hsh 	%prec '('
			{ $$ = $1; }
	|	ary 	%prec '('
			{ $$ = $1; }
	|	arylen 	%prec '('
			{ $$ = newUNOP(OP_AV2ARYLEN, 0, ref($1, OP_AV2ARYLEN));}
	|	scalar '{' expr ';' '}'	%prec '('
			{ $$ = newBINOP(OP_HELEM, 0, oopsHV($1), jmaybe($3));
			    PL_expect = XOPERATOR; }
	|	term ARROW '{' expr ';' '}'	%prec '('
			{ $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($4));
			    PL_expect = XOPERATOR; }
	|	term '{' expr ';' '}'	%prec '('
			{ assertref($1); $$ = newBINOP(OP_HELEM, 0,
					ref(newHVREF($1),OP_RV2HV),
					jmaybe($3));
			    PL_expect = XOPERATOR; }
	|	'(' expr ')' '[' expr ']'	%prec '('
			{ $$ = newSLICEOP(0, $5, $2); }
	|	'(' ')' '[' expr ']'	%prec '('
			{ $$ = newSLICEOP(0, $4, Nullop); }
	|	ary '[' expr ']'	%prec '('
			{ $$ = prepend_elem(OP_ASLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_ASLICE, 0,
					list($3),
					ref($1, OP_ASLICE))); }
	|	ary '{' expr ';' '}'	%prec '('
			{ $$ = prepend_elem(OP_HSLICE,
				newOP(OP_PUSHMARK, 0),
				    newLISTOP(OP_HSLICE, 0,
					list($3),
					ref(oopsHV($1), OP_HSLICE)));
			    PL_expect = XOPERATOR; }
	|	THING	%prec '('
			{ $$ = $1; }
	|	amper
			{ $$ = newUNOP(OP_ENTERSUB, 0, scalar($1)); }
	|	amper '(' ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED, scalar($1)); }
	|	amper '(' expr ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($1))); }
	|	NOAMP WORD listexpr
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $3, scalar($2))); }
	|	DO term	%prec UNIOP
			{ $$ = dofile($2); }
	|	DO block	%prec '('
			{ $$ = newUNOP(OP_NULL, OPf_SPECIAL, scope($2)); }
	|	DO WORD '(' ')'
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)),Nullop)); dep();}
	|	DO WORD '(' expr ')'
			{ $$ = newUNOP(OP_ENTERSUB,
			    OPf_SPECIAL|OPf_STACKED,
			    append_elem(OP_LIST,
				$4,
				scalar(newCVREF(
				    (OPpENTERSUB_AMPER<<8),
				    scalar($2)
				)))); dep();}
	|	DO scalar '(' ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				scalar(newCVREF(0,scalar($2))), Nullop)); dep();}
	|	DO scalar '(' expr ')'
			{ $$ = newUNOP(OP_ENTERSUB, OPf_SPECIAL|OPf_STACKED,
			    prepend_elem(OP_LIST,
				$4,
				scalar(newCVREF(0,scalar($2))))); dep();}
	|	term ARROW '(' ')'	%prec '('
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   newCVREF(0, scalar($1))); }
	|	term ARROW '(' expr ')'	%prec '('
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				   append_elem(OP_LIST, $4,
				       newCVREF(0, scalar($1)))); }
	|	LOOPEX
			{ $$ = newOP($1, OPf_SPECIAL);
			    PL_hints |= HINT_BLOCK_SCOPE; }
	|	LOOPEX term
			{ $$ = newLOOPEX($1,$2); }
	|	NOTOP argexpr
			{ $$ = newUNOP(OP_NOT, 0, scalar($2)); }
	|	UNIOP
			{ $$ = newOP($1, 0); }
	|	UNIOP block
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOP term
			{ $$ = newUNOP($1, 0, $2); }
	|	UNIOPSUB term
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
			    append_elem(OP_LIST, $2, scalar($1))); }
	|	FUNC0
			{ $$ = newOP($1, 0); }
	|	FUNC0 '(' ')'
			{ $$ = newOP($1, 0); }
	|	FUNC0SUB
			{ $$ = newUNOP(OP_ENTERSUB, OPf_STACKED,
				scalar($1)); }
	|	FUNC1 '(' ')'
			{ $$ = newOP($1, OPf_SPECIAL); }
	|	FUNC1 '(' expr ')'
			{ $$ = newUNOP($1, 0, $3); }
	|	PMFUNC '(' term ')'
			{ $$ = pmruntime($1, $3, Nullop); }
	|	PMFUNC '(' term ',' term ')'
			{ $$ = pmruntime($1, $3, $5); }
	|	WORD
	|	listop
	;

listexpr:	/* NULL */
			{ $$ = Nullop; }
	|	argexpr
			{ $$ = $1; }
	;

listexprcom:	/* NULL */
			{ $$ = Nullop; }
	|	expr
			{ $$ = $1; }
	|	expr ','
			{ $$ = $1; }
	;

local	:	LOCAL	{ $$ = 0; }
	|	MY	{ $$ = 1; }
	;

my_scalar:	scalar
			{ PL_in_my = 0; $$ = my($1); }
	;

amper	:	'&' indirob
			{ $$ = newCVREF($1,$2); }
	;

scalar	:	'$' indirob
			{ $$ = newSVREF($2); }
	;

ary	:	'@' indirob
			{ $$ = newAVREF($2); }
	;

hsh	:	'%' indirob
			{ $$ = newHVREF($2); }
	;

arylen	:	DOLSHARP indirob
			{ $$ = newAVREF($2); }
	;

star	:	'*' indirob
			{ $$ = newGVREF(0,$2); }
	;

indirob	:	WORD
			{ $$ = scalar($1); }
	|	scalar
			{ $$ = scalar($1);  }
	|	block
			{ $$ = scope($1); }

	|	PRIVATEREF
			{ $$ = $1; }
	;

%% /* PROGRAM */
