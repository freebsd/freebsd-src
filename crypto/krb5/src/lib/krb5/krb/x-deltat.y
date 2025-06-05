/*
 * lib/krb5/krb/deltat.y
 *
 * Copyright 1999 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * krb5_string_to_deltat()
 */

/* For a clean, thread-safe interface, we must use the "pure parser"
   facility of GNU Bison.  Unfortunately, standard YACC has no such
   option.  */

/* N.B.: For simplicity in dealing with the distribution, the
   Makefile.in listing for deltat.c does *not* normally list this
   file.  If you change this file, tweak the Makefile so it'll rebuild
   deltat.c, or do it manually.  */
%{

/*
 * GCC optimizer will detect a variable used without being set in a YYERROR
 * path.  As this is generated code, suppress the complaint.
 */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif

#include "k5-int.h"
#include <ctype.h>

struct param {
    krb5_int32 delta;
    char *p;
};

#define MAX_TIME KRB5_INT32_MAX
#define MIN_TIME KRB5_INT32_MIN

#define DAY (24 * 3600)
#define HOUR 3600

#define MAX_DAY (MAX_TIME / DAY)
#define MIN_DAY (MIN_TIME / DAY)
#define MAX_HOUR (MAX_TIME / HOUR)
#define MIN_HOUR (MIN_TIME / HOUR)
#define MAX_MIN (MAX_TIME / 60)
#define MIN_MIN (MIN_TIME / 60)

/* An explanation of the tests being performed.
   We do not want to overflow a 32 bit integer with out manipulations,
   even for testing for overflow. Therefore we rely on the following:

   The lex parser will not return a number > MAX_TIME (which is out 32
   bit limit).

   Therefore, seconds (s) will require
       MIN_TIME < s < MAX_TIME

   For subsequent tests, the logic is as follows:

      If A < MAX_TIME and  B < MAX_TIME

      If we want to test if A+B < MAX_TIME, there are two cases
        if (A > 0)
         then A + B < MAX_TIME if B < MAX_TIME - A
	else A + B < MAX_TIME  always.

      if we want to test if MIN_TIME < A + B
          if A > 0 - then nothing to test
          otherwise, we test if MIN_TIME - A < B.

   We of course are testing for:
          MIN_TIME < A + B < MAX_TIME
*/


#define DAY_NOT_OK(d) (d) > MAX_DAY || (d) < MIN_DAY
#define HOUR_NOT_OK(h) (h) > MAX_HOUR || (h) < MIN_HOUR
#define MIN_NOT_OK(m) (m) > MAX_MIN || (m) < MIN_MIN
#define SUM_OK(a, b) (((a) > 0) ? ( (b) <= MAX_TIME - (a)) : (MIN_TIME - (a) <= (b)))
#define DO_SUM(res, a, b) if (!SUM_OK((a), (b))) YYERROR; \
                          res = (a) + (b)


#define OUT_D tmv->delta
#define DO(D,H,M,S) \
 { \
     /* Overflow testing - this does not handle negative values well.. */ \
     if (DAY_NOT_OK(D) || HOUR_NOT_OK(H) || MIN_NOT_OK(M)) YYERROR; \
     OUT_D = D * DAY; \
     DO_SUM(OUT_D, OUT_D, H * HOUR); \
     DO_SUM(OUT_D, OUT_D, M * 60); \
     DO_SUM(OUT_D, OUT_D, S); \
 }

static int mylex(int *intp, struct param *tmv);
#undef yylex
#define yylex(U, P)    mylex (&(U)->val, (P))

#undef yyerror
#define yyerror(tmv, msg)

static int yyparse(struct param *);

%}

%union {int val;}
%parse-param {struct param *tmv}
%lex-param {struct param *tmv}
%define api.pure

%token <val> tok_NUM tok_LONGNUM tok_OVERFLOW
%token '-' ':' 'd' 'h' 'm' 's' tok_WS

%type <val> num opt_hms opt_ms opt_s wsnum posnum

%start start

%%

start: deltat;
posnum: tok_NUM | tok_LONGNUM ;
num: posnum | '-' posnum { $$ = - $2; } ;
ws: /* nothing */ | tok_WS ;
wsnum: ws num { $$ = $2; }
        | ws tok_OVERFLOW { YYERROR; };
deltat:
	  wsnum 'd' opt_hms                          { DO ($1,  0,  0, $3); }
	| wsnum 'h' opt_ms                           { DO ( 0, $1,  0, $3); }
	| wsnum 'm' opt_s                            { DO ( 0,  0, $1, $3); }
	| wsnum 's'                                  { DO ( 0,  0,  0, $1); }
	| wsnum '-' tok_NUM ':' tok_NUM ':' tok_NUM  { DO ($1, $3, $5, $7); }
	| wsnum ':' tok_NUM ':' tok_NUM              { DO ( 0, $1, $3, $5); }
	| wsnum ':' tok_NUM                          { DO ( 0, $1, $3,  0); }
	| wsnum                                      { DO ( 0,  0,  0, $1); }
	                                             /* default to 's' */
	;

opt_hms:
	  opt_ms
	  | wsnum 'h' opt_ms		{ if (HOUR_NOT_OK($1)) YYERROR;
	                                  DO_SUM($$, $1 * 3600, $3); }; 
opt_ms:
	  opt_s
	| wsnum 'm' opt_s		{ if (MIN_NOT_OK($1)) YYERROR;
	                                  DO_SUM($$, $1 * 60, $3); }; 
opt_s:
	  ws				{ $$ = 0; }
	| wsnum 's' ;

%%

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

static int
mylex(int *intp, struct param *tmv)
{
    int num, c;
#define P (tmv->p)
    char *orig_p = P;

#ifdef isascii
    if (!isascii (*P))
	return 0;
#endif
    switch (c = *P++) {
    case '-':
    case ':':
    case 'd':
    case 'h':
    case 'm':
    case 's':
	return c;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
	/* XXX assumes ASCII */
	num = c - '0';
	while (isdigit ((int) *P)) {
	    if (num > MAX_TIME / 10)
	      return tok_OVERFLOW;
	    num *= 10;
	    if (num > MAX_TIME - (*P - '0'))
	      return tok_OVERFLOW;
	    num += *P++ - '0';
	}
	*intp = num;
	return (P - orig_p > 2) ? tok_LONGNUM : tok_NUM;
    case ' ':
    case '\t':
    case '\n':
	while (isspace ((int) *P))
	    P++;
	return tok_WS;
    default:
	return YYEOF;
    }
}

krb5_error_code KRB5_CALLCONV
krb5_string_to_deltat(char *string, krb5_deltat *deltatp)
{
    struct param p;
    p.delta = 0;
    p.p = string;
    if (yyparse (&p))
	return KRB5_DELTAT_BADFORMAT;
    *deltatp = p.delta;
    return 0;
}
