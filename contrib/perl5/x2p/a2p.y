%{
/* $RCSfile: a2p.y,v $$Revision: 4.1 $$Date: 92/08/07 18:29:12 $
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	a2p.y,v $
 */

#include "INTERN.h"
#include "a2p.h"

int root;
int begins = Nullop;
int ends = Nullop;

%}
%token BEGIN END
%token REGEX
%token SEMINEW NEWLINE COMMENT
%token FUN1 FUNN GRGR
%token PRINT PRINTF SPRINTF_OLD SPRINTF_NEW SPLIT
%token IF ELSE WHILE FOR IN
%token EXIT NEXT BREAK CONTINUE RET
%token GETLINE DO SUB GSUB MATCH
%token FUNCTION USERFUN DELETE

%right ASGNOP
%right '?' ':'
%left OROR
%left ANDAND
%left IN
%left NUMBER VAR SUBSTR INDEX
%left MATCHOP
%left RELOP '<' '>'
%left OR
%left STRING
%left '+' '-'
%left '*' '/' '%'
%right UMINUS
%left NOT
%right '^'
%left INCR DECR
%left FIELD VFIELD

%%

program	: junk hunks
		{ root = oper4(OPROG,$1,begins,$2,ends); }
	;

begin	: BEGIN '{' maybe states '}' junk
		{ begins = oper4(OJUNK,begins,$3,$4,$6); in_begin = FALSE;
		    $$ = Nullop; }
	;

end	: END '{' maybe states '}'
		{ ends = oper3(OJUNK,ends,$3,$4); $$ = Nullop; }
	| end NEWLINE
		{ $$ = $1; }
	;

hunks	: hunks hunk junk
		{ $$ = oper3(OHUNKS,$1,$2,$3); }
	| /* NULL */
		{ $$ = Nullop; }
	;

hunk	: patpat
		{ $$ = oper1(OHUNK,$1); need_entire = TRUE; }
	| patpat '{' maybe states '}'
		{ $$ = oper2(OHUNK,$1,oper2(OJUNK,$3,$4)); }
	| FUNCTION USERFUN '(' arg_list ')' maybe '{' maybe states '}'
		{ fixfargs($2,$4,0); $$ = oper5(OUSERDEF,$2,$4,$6,$8,$9); }
	| '{' maybe states '}'
		{ $$ = oper2(OHUNK,Nullop,oper2(OJUNK,$2,$3)); }
	| begin
	| end
	;

arg_list: expr_list
		{ $$ = rememberargs($$); }
	;

patpat	: cond
		{ $$ = oper1(OPAT,$1); }
	| cond ',' cond
		{ $$ = oper2(ORANGE,$1,$3); }
	;

cond	: expr
	| match
	| rel
	| compound_cond
	| cond '?' expr ':' expr
		{ $$ = oper3(OCOND,$1,$3,$5); }
	;

compound_cond
	: '(' compound_cond ')'
		{ $$ = oper1(OCPAREN,$2); }
	| cond ANDAND maybe cond
		{ $$ = oper3(OCANDAND,$1,$3,$4); }
	| cond OROR maybe cond
		{ $$ = oper3(OCOROR,$1,$3,$4); }
	| NOT cond
		{ $$ = oper1(OCNOT,$2); }
	;

rel	: expr RELOP expr
		{ $$ = oper3(ORELOP,$2,$1,$3); }
	| expr '>' expr
		{ $$ = oper3(ORELOP,string(">",1),$1,$3); }
	| expr '<' expr
		{ $$ = oper3(ORELOP,string("<",1),$1,$3); }
	| '(' rel ')'
		{ $$ = oper1(ORPAREN,$2); }
	;

match	: expr MATCHOP expr
		{ $$ = oper3(OMATCHOP,$2,$1,$3); }
	| expr MATCHOP REGEX
		{ $$ = oper3(OMATCHOP,$2,$1,oper1(OREGEX,$3)); }
	| REGEX		%prec MATCHOP
		{ $$ = oper1(OREGEX,$1); }
	| '(' match ')'
		{ $$ = oper1(OMPAREN,$2); }
	;

expr	: term
		{ $$ = $1; }
	| expr term
		{ $$ = oper2(OCONCAT,$1,$2); }
	| expr '?' expr ':' expr
		{ $$ = oper3(OCOND,$1,$3,$5); }
	| variable ASGNOP cond
		{ $$ = oper3(OASSIGN,$2,$1,$3);
			if ((ops[$1].ival & 255) == OFLD)
			    lval_field = TRUE;
			if ((ops[$1].ival & 255) == OVFLD)
			    lval_field = TRUE;
		}
	;

sprintf	: SPRINTF_NEW
	| SPRINTF_OLD ;

term	: variable
		{ $$ = $1; }
	| NUMBER
		{ $$ = oper1(ONUM,$1); }
	| STRING
		{ $$ = oper1(OSTR,$1); }
	| term '+' term
		{ $$ = oper2(OADD,$1,$3); }
	| term '-' term
		{ $$ = oper2(OSUBTRACT,$1,$3); }
	| term '*' term
		{ $$ = oper2(OMULT,$1,$3); }
	| term '/' term
		{ $$ = oper2(ODIV,$1,$3); }
	| term '%' term
		{ $$ = oper2(OMOD,$1,$3); }
	| term '^' term
		{ $$ = oper2(OPOW,$1,$3); }
	| term IN VAR
		{ $$ = oper2(ODEFINED,aryrefarg($3),$1); }
	| variable INCR
		{ $$ = oper1(OPOSTINCR,$1); }
	| variable DECR
		{ $$ = oper1(OPOSTDECR,$1); }
	| INCR variable
		{ $$ = oper1(OPREINCR,$2); }
	| DECR variable
		{ $$ = oper1(OPREDECR,$2); }
	| '-' term %prec UMINUS
		{ $$ = oper1(OUMINUS,$2); }
	| '+' term %prec UMINUS
		{ $$ = oper1(OUPLUS,$2); }
	| '(' cond ')'
		{ $$ = oper1(OPAREN,$2); }
	| GETLINE
		{ $$ = oper0(OGETLINE); }
	| GETLINE variable
		{ $$ = oper1(OGETLINE,$2); }
	| GETLINE '<' expr
		{ $$ = oper3(OGETLINE,Nullop,string("<",1),$3);
		    if (ops[$3].ival != OSTR + (1<<8)) do_fancy_opens = TRUE; }
	| GETLINE variable '<' expr
		{ $$ = oper3(OGETLINE,$2,string("<",1),$4);
		    if (ops[$4].ival != OSTR + (1<<8)) do_fancy_opens = TRUE; }
	| term 'p' GETLINE
		{ $$ = oper3(OGETLINE,Nullop,string("|",1),$1);
		    if (ops[$1].ival != OSTR + (1<<8)) do_fancy_opens = TRUE; }
	| term 'p' GETLINE variable
		{ $$ = oper3(OGETLINE,$4,string("|",1),$1);
		    if (ops[$1].ival != OSTR + (1<<8)) do_fancy_opens = TRUE; }
	| FUN1
		{ $$ = oper0($1); need_entire = do_chop = TRUE; }
	| FUN1 '(' ')'
		{ $$ = oper1($1,Nullop); need_entire = do_chop = TRUE; }
	| FUN1 '(' expr ')'
		{ $$ = oper1($1,$3); }
	| FUNN '(' expr_list ')'
		{ $$ = oper1($1,$3); }
	| USERFUN '(' expr_list ')'
		{ $$ = oper2(OUSERFUN,$1,$3); }
	| SPRINTF_NEW '(' expr_list ')'
		{ $$ = oper1(OSPRINTF,$3); }
	| sprintf expr_list
		{ $$ = oper1(OSPRINTF,$2); }
	| SUBSTR '(' expr ',' expr ',' expr ')'
		{ $$ = oper3(OSUBSTR,$3,$5,$7); }
	| SUBSTR '(' expr ',' expr ')'
		{ $$ = oper2(OSUBSTR,$3,$5); }
	| SPLIT '(' expr ',' VAR ',' expr ')'
		{ $$ = oper3(OSPLIT,$3,aryrefarg(numary($5)),$7); }
	| SPLIT '(' expr ',' VAR ',' REGEX ')'
		{ $$ = oper3(OSPLIT,$3,aryrefarg(numary($5)),oper1(OREGEX,$7));}
	| SPLIT '(' expr ',' VAR ')'
		{ $$ = oper2(OSPLIT,$3,aryrefarg(numary($5))); }
	| INDEX '(' expr ',' expr ')'
		{ $$ = oper2(OINDEX,$3,$5); }
	| MATCH '(' expr ',' REGEX ')'
		{ $$ = oper2(OMATCH,$3,oper1(OREGEX,$5)); }
	| MATCH '(' expr ',' expr ')'
		{ $$ = oper2(OMATCH,$3,$5); }
	| SUB '(' expr ',' expr ')'
		{ $$ = oper2(OSUB,$3,$5); }
	| SUB '(' REGEX ',' expr ')'
		{ $$ = oper2(OSUB,oper1(OREGEX,$3),$5); }
	| GSUB '(' expr ',' expr ')'
		{ $$ = oper2(OGSUB,$3,$5); }
	| GSUB '(' REGEX ',' expr ')'
		{ $$ = oper2(OGSUB,oper1(OREGEX,$3),$5); }
	| SUB '(' expr ',' expr ',' expr ')'
		{ $$ = oper3(OSUB,$3,$5,$7); }
	| SUB '(' REGEX ',' expr ',' expr ')'
		{ $$ = oper3(OSUB,oper1(OREGEX,$3),$5,$7); }
	| GSUB '(' expr ',' expr ',' expr ')'
		{ $$ = oper3(OGSUB,$3,$5,$7); }
	| GSUB '(' REGEX ',' expr ',' expr ')'
		{ $$ = oper3(OGSUB,oper1(OREGEX,$3),$5,$7); }
	;

variable: VAR
		{ $$ = oper1(OVAR,$1); }
	| VAR '[' expr_list ']'
		{ $$ = oper2(OVAR,aryrefarg($1),$3); }
	| FIELD
		{ $$ = oper1(OFLD,$1); }
	| VFIELD term
		{ $$ = oper1(OVFLD,$2); }
	;

expr_list
	: expr
	| clist
	| /* NULL */
		{ $$ = Nullop; }
	;

clist	: expr ',' maybe expr
		{ $$ = oper3(OCOMMA,$1,$3,$4); }
	| clist ',' maybe expr
		{ $$ = oper3(OCOMMA,$1,$3,$4); }
	| '(' clist ')'		/* these parens are invisible */
		{ $$ = $2; }
	;

junk	: junk hunksep
		{ $$ = oper2(OJUNK,$1,$2); }
	| /* NULL */
		{ $$ = Nullop; }
	;

hunksep : ';'
		{ $$ = oper2(OJUNK,oper0(OSEMICOLON),oper0(ONEWLINE)); }
	| SEMINEW
		{ $$ = oper2(OJUNK,oper0(OSEMICOLON),oper0(ONEWLINE)); }
	| NEWLINE
		{ $$ = oper0(ONEWLINE); }
	| COMMENT
		{ $$ = oper1(OCOMMENT,$1); }
	;

maybe	: maybe nlstuff
		{ $$ = oper2(OJUNK,$1,$2); }
	| /* NULL */
		{ $$ = Nullop; }
	;

nlstuff : NEWLINE
		{ $$ = oper0(ONEWLINE); }
	| COMMENT
		{ $$ = oper1(OCOMMENT,$1); }
	;

separator
	: ';' maybe
		{ $$ = oper2(OJUNK,oper0(OSEMICOLON),$2); }
	| SEMINEW maybe
		{ $$ = oper2(OJUNK,oper0(OSNEWLINE),$2); }
	| NEWLINE maybe
		{ $$ = oper2(OJUNK,oper0(OSNEWLINE),$2); }
	| COMMENT maybe
		{ $$ = oper2(OJUNK,oper1(OSCOMMENT,$1),$2); }
	;

states	: states statement
		{ $$ = oper2(OSTATES,$1,$2); }
	| /* NULL */
		{ $$ = Nullop; }
	;

statement
	: simple separator maybe
		{ $$ = oper2(OJUNK,oper2(OSTATE,$1,$2),$3); }
	| ';' maybe
		{ $$ = oper2(OSTATE,Nullop,oper2(OJUNK,oper0(OSEMICOLON),$2)); }
	| SEMINEW maybe
		{ $$ = oper2(OSTATE,Nullop,oper2(OJUNK,oper0(OSNEWLINE),$2)); }
	| compound
	;

simpnull: simple
	| /* NULL */
		{ $$ = Nullop; }
	;

simple
	: expr
	| PRINT expr_list redir expr
		{ $$ = oper3(OPRINT,$2,$3,$4);
		    do_opens = TRUE;
		    saw_ORS = saw_OFS = TRUE;
		    if (!$2) need_entire = TRUE;
		    if (ops[$4].ival != OSTR + (1<<8)) do_fancy_opens = TRUE; }
	| PRINT expr_list
		{ $$ = oper1(OPRINT,$2);
		    if (!$2) need_entire = TRUE;
		    saw_ORS = saw_OFS = TRUE;
		}
	| PRINTF expr_list redir expr
		{ $$ = oper3(OPRINTF,$2,$3,$4);
		    do_opens = TRUE;
		    if (!$2) need_entire = TRUE;
		    if (ops[$4].ival != OSTR + (1<<8)) do_fancy_opens = TRUE; }
	| PRINTF expr_list
		{ $$ = oper1(OPRINTF,$2);
		    if (!$2) need_entire = TRUE;
		}
	| BREAK
		{ $$ = oper0(OBREAK); }
	| NEXT
		{ $$ = oper0(ONEXT); }
	| EXIT
		{ $$ = oper0(OEXIT); }
	| EXIT expr
		{ $$ = oper1(OEXIT,$2); }
	| CONTINUE
		{ $$ = oper0(OCONTINUE); }
	| RET
		{ $$ = oper0(ORETURN); }
	| RET expr
		{ $$ = oper1(ORETURN,$2); }
	| DELETE VAR '[' expr_list ']'
		{ $$ = oper2(ODELETE,aryrefarg($2),$4); }
	;

redir	: '>'	%prec FIELD
		{ $$ = oper1(OREDIR,string(">",1)); }
	| GRGR
		{ $$ = oper1(OREDIR,string(">>",2)); }
	| '|'
		{ $$ = oper1(OREDIR,string("|",1)); }
	;

compound
	: IF '(' cond ')' maybe statement
		{ $$ = oper2(OIF,$3,bl($6,$5)); }
	| IF '(' cond ')' maybe statement ELSE maybe statement
		{ $$ = oper3(OIF,$3,bl($6,$5),bl($9,$8)); }
	| WHILE '(' cond ')' maybe statement
		{ $$ = oper2(OWHILE,$3,bl($6,$5)); }
	| DO maybe statement WHILE '(' cond ')'
		{ $$ = oper2(ODO,bl($3,$2),$6); }
	| FOR '(' simpnull ';' cond ';' simpnull ')' maybe statement
		{ $$ = oper4(OFOR,$3,$5,$7,bl($10,$9)); }
	| FOR '(' simpnull ';'  ';' simpnull ')' maybe statement
		{ $$ = oper4(OFOR,$3,string("",0),$6,bl($9,$8)); }
	| FOR '(' expr ')' maybe statement
		{ $$ = oper2(OFORIN,$3,bl($6,$5)); }
	| '{' maybe states '}' maybe
		{ $$ = oper3(OBLOCK,oper2(OJUNK,$2,$3),Nullop,$5); }
	;

%%

int yyparse (void);

#include "a2py.c"
