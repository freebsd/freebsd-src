%{
/* Written by Pace Willisson (pace@blitz.com) 
 * and placed in the public domain.
 *
 * Largely rewritten by J.T. Conklin (jtc@wimsey.com)
 *
 * $Id: expr.y,v 1.10 1995/08/04 17:08:07 joerg Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <err.h>
  
enum valtype {
	integer, numeric_string, string
} ;

struct val {
	enum valtype type;
	union {
		char *s;
		int   i;
	} u;
} ;

struct val *result;
struct val *op_or ();
struct val *op_and ();
struct val *op_eq ();
struct val *op_gt ();
struct val *op_lt ();
struct val *op_ge ();
struct val *op_le ();
struct val *op_ne ();
struct val *op_plus ();
struct val *op_minus ();
struct val *op_times ();
struct val *op_div ();
struct val *op_rem ();
struct val *op_colon ();

char **av;
%}

%union
{
	struct val *val;
}

%left <val> '|'
%left <val> '&'
%left <val> '=' '>' '<' GE LE NE
%left <val> '+' '-'
%left <val> '*' '/' '%'
%left <val> ':'

%token <val> TOKEN
%type <val> start expr

%%

start: expr { result = $$; }

expr:	TOKEN
	| '(' expr ')' { $$ = $2; }
	| expr '|' expr { $$ = op_or ($1, $3); }
	| expr '&' expr { $$ = op_and ($1, $3); }
	| expr '=' expr { $$ = op_eq ($1, $3); }
	| expr '>' expr { $$ = op_gt ($1, $3); }
	| expr '<' expr { $$ = op_lt ($1, $3); }
	| expr GE expr  { $$ = op_ge ($1, $3); }
	| expr LE expr  { $$ = op_le ($1, $3); }
	| expr NE expr  { $$ = op_ne ($1, $3); }
	| expr '+' expr { $$ = op_plus ($1, $3); }
	| expr '-' expr { $$ = op_minus ($1, $3); }
	| expr '*' expr { $$ = op_times ($1, $3); }
	| expr '/' expr { $$ = op_div ($1, $3); }
	| expr '%' expr { $$ = op_rem ($1, $3); }
	| expr ':' expr { $$ = op_colon ($1, $3); }
	;


%%

struct val *
make_integer (i)
int i;
{
	struct val *vp;

	vp = (struct val *) malloc (sizeof (*vp));
	if (vp == NULL) {
		errx (2, "malloc() failed");
	}

	vp->type = integer;
	vp->u.i  = i;
	return vp; 
}

struct val *
make_str (s)
char *s;
{
	struct val *vp;
	int i, isint;

	vp = (struct val *) malloc (sizeof (*vp));
	if (vp == NULL || ((vp->u.s = strdup (s)) == NULL)) {
		errx (2, "malloc() failed");
	}

	for(i = 1, isint = isdigit(s[0]) || s[0] == '-';
	    isint && i < strlen(s);
	    i++)
	{
		if(!isdigit(s[i]))
			 isint = 0;
	}

	if (isint)
		vp->type = numeric_string;
	else	
		vp->type = string;

	return vp;
}


void
free_value (vp)
struct val *vp;
{
	if (vp->type == string || vp->type == numeric_string)
		free (vp->u.s);	
}


int
to_integer (vp)
struct val *vp;
{
	int i;

	if (vp->type == integer)
		return 1;

	if (vp->type == string)
		return 0;

	/* vp->type == numeric_string, make it numeric */
	i  = atoi(vp->u.s);
	free (vp->u.s);
	vp->u.i = i;
	vp->type = integer;
	return 1;
}

void
to_string (vp)
struct val *vp;
{
	char *tmp;

	if (vp->type == string || vp->type == numeric_string)
		return;

	tmp = malloc (25);
	if (tmp == NULL) {
		errx (2, "malloc() failed");
	}

	sprintf (tmp, "%d", vp->u.i);
	vp->type = string;
	vp->u.s  = tmp;
}


int
isstring (vp)
struct val *vp;
{
	/* only TRUE if this string is not a valid integer */
	return (vp->type == string);
}


int
yylex ()
{
	char *p;

	if (*av == NULL)
		return (0);

	p = *av++;

	if (strlen (p) == 1) {
		if (strchr ("|&=<>+-*/%:()", *p))
			return (*p);
	} else if (strlen (p) == 2 && p[1] == '=') {
		switch (*p) {
		case '>': return (GE);
		case '<': return (LE);
		case '!': return (NE);
		}
	}

	yylval.val = make_str (p);
	return (TOKEN);
}

int
is_zero_or_null (vp)
struct val *vp;
{
	if (vp->type == integer) {
		return (vp->u.i == 0);
	} else {
		return (*vp->u.s == 0 || (to_integer (vp) && vp->u.i == 0));
	}
	/* NOTREACHED */
}

int yyparse ();

int
main (argc, argv)
int argc;
char **argv;
{
	setlocale (LC_ALL, "");

	av = argv + 1;

	yyparse ();

	if (result->type == integer)
		printf ("%d\n", result->u.i);
	else
		printf ("%s\n", result->u.s);

	return (is_zero_or_null (result));
}

int
yyerror (s)
char *s;
{
	errx (2, "syntax error");
}


struct val *
op_or (a, b)
struct val *a, *b;
{
	if (is_zero_or_null (a)) {
		free_value (a);
		return (b);
	} else {
		free_value (b);
		return (a);
	}
}
		
struct val *
op_and (a, b)
struct val *a, *b;
{
	if (is_zero_or_null (a) || is_zero_or_null (b)) {
		free_value (a);
		free_value (b);
		return (make_integer (0));
	} else {
		free_value (b);
		return (a);
	}
}

struct val *
op_eq (a, b)
struct val *a, *b;
{
	struct val *r; 

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);	
		r = make_integer (strcoll (a->u.s, b->u.s) == 0);
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer (a->u.i == b->u.i);
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_gt (a, b)
struct val *a, *b;
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcoll (a->u.s, b->u.s) > 0);
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r= make_integer (a->u.i > b->u.i);
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_lt (a, b)
struct val *a, *b;
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcoll (a->u.s, b->u.s) < 0);
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer (a->u.i < b->u.i);
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_ge (a, b)
struct val *a, *b;
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcoll (a->u.s, b->u.s) >= 0);
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer (a->u.i >= b->u.i);
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_le (a, b)
struct val *a, *b;
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcoll (a->u.s, b->u.s) <= 0);
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer (a->u.i <= b->u.i);
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_ne (a, b)
struct val *a, *b;
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcoll (a->u.s, b->u.s) != 0);
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer (a->u.i != b->u.i);
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_plus (a, b)
struct val *a, *b;
{
	struct val *r;

	if (!to_integer (a) || !to_integer (b)) {
		errx (2, "non-numeric argument");
	}

	r = make_integer (a->u.i + b->u.i);
	free_value (a);
	free_value (b);
	return r;
}
	
struct val *
op_minus (a, b)
struct val *a, *b;
{
	struct val *r;

	if (!to_integer (a) || !to_integer (b)) {
		errx (2, "non-numeric argument");
	}

	r = make_integer (a->u.i - b->u.i);
	free_value (a);
	free_value (b);
	return r;
}
	
struct val *
op_times (a, b)
struct val *a, *b;
{
	struct val *r;

	if (!to_integer (a) || !to_integer (b)) {
		errx (2, "non-numeric argument");
	}

	r = make_integer (a->u.i * b->u.i);
	free_value (a);
	free_value (b);
	return (r);
}
	
struct val *
op_div (a, b)
struct val *a, *b;
{
	struct val *r;

	if (!to_integer (a) || !to_integer (b)) {
		errx (2, "non-numeric argument");
	}

	if (b->u.i == 0) {
		errx (2, "division by zero");
	}

	r = make_integer (a->u.i / b->u.i);
	free_value (a);
	free_value (b);
	return r;
}
	
struct val *
op_rem (a, b)
struct val *a, *b;
{
	struct val *r;

	if (!to_integer (a) || !to_integer (b)) {
		errx (2, "non-numeric argument");
	}

	if (b->u.i == 0) {
		errx (2, "division by zero");
	}

	r = make_integer (a->u.i % b->u.i);
	free_value (a);
	free_value (b);
	return r;
}
	
#include <sys/types.h>
#include <regex.h>

struct val *
op_colon (a, b)
struct val *a, *b;
{
	regex_t rp;
	regmatch_t rm[2];
	char errbuf[256];
	int eval;
	struct val *v;

	/* coerce to both arguments to strings */
	to_string(a);
	to_string(b);

	/* compile regular expression */
	if ((eval = regcomp (&rp, b->u.s, 0)) != 0) {
		regerror (eval, &rp, errbuf, sizeof(errbuf));
		errx (2, "%s", errbuf);
	}

	/* compare string against pattern */
	/* remember that patterns are anchored to the beginning of the line */
	if (regexec(&rp, a->u.s, 2, rm, 0) == 0 && rm[0].rm_so == 0) {
		if (rm[1].rm_so >= 0) {
			*(a->u.s + rm[1].rm_eo) = '\0';
			v = make_str (a->u.s + rm[1].rm_so);

		} else {
			v = make_integer (rm[0].rm_eo - rm[0].rm_so);
		}
	} else {
		if (rp.re_nsub == 0) {
			v = make_integer (0);
		} else {
			v = make_str ("");
		}
	}

	/* free arguments and pattern buffer */
	free_value (a);
	free_value (b);
	regfree (&rp);

	return v;
}
