%{
/* Written by Pace Willisson (pace@blitz.com) 
 * and placed in the public domain
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00148
 * --------------------         -----   ----------------------
 *
 * 20 Apr 93	J. T. Conklin		Many fixes for () and other such things
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum valtype {
	integer, string
} ;
    
struct val {
	enum valtype type;
	union {
		char *s;
		int   i;
	} u;
};

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
%left UNARY

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
	| '-' expr %prec UNARY { $$ = op_minus (NULL, $2); }
	;


%%

struct val *
make_integer (i)
int i;
{
	struct val *vp;

	vp = (struct val *) malloc (sizeof (*vp));
	if (vp == NULL) {
		fprintf (stderr, "expr: out of memory\n");
		exit (2);
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

	vp = (struct val *) malloc (sizeof (*vp));
	if (vp == NULL || ((vp->u.s = strdup (s)) == NULL)) {
		fprintf (stderr, "expr: out of memory\n");
		exit (2);
	}

	vp->type = string;
	return vp;
}


void
free_value (vp)
struct val *vp;
{
	if (vp->type == string)
		free (vp->u.s);	
}


int
to_integer (vp)
struct val *vp;
{
	char *s;
	int neg;
	int i;

	if (vp->type == integer)
		return 1;

	s = vp->u.s;
	i = 0;
      
	neg = (*s == '-');
	if (neg)
		s++;
      
	for (;*s; s++) {
		if (!isdigit (*s)) 
			return 0;
      
		i *= 10;
		i += *s - '0';
	}

	free (vp->u.s);
	if (neg) 
		i *= -1;
  
	vp->type = integer;
	vp->u.i  = i;
	return 1;
}

void
to_string (vp)
struct val *vp;
{
	char *tmp;

	if (vp->type == string)
		return;

	tmp = malloc (25);
	if (tmp == NULL) {
		fprintf (stderr, "expr: out of memory\n");
		exit (2);
	}

	sprintf (tmp, "%d", vp->u.i);
	vp->type = string;
	vp->u.s  = tmp;
}


int
isstring (vp)
struct val *vp;
{
	return (vp->type == string);
}


int
yylex ()
{
	struct val *vp;
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
	/* Like most other versions of expr, this version will return
	   false for a string value of multiple zeros.*/

	if (vp->type == integer) {
		return (vp->u.i == 0);
	} else {
		return (*vp->u.s == 0 || strcmp (vp->u.s, "0") == 0);
	}
	/* NOTREACHED */
}

void
main (argc, argv)
int argc;
char **argv;
{
	av = argv + 1;

	yyparse ();

	if (result->type == integer)
		printf ("%d\n", result->u.i);
	else
		printf ("%s\n", result->u.s);

	if (is_zero_or_null (result))
		exit (1);
	else
		exit (0);
}

int
yyerror (s)
char *s;
{
	fprintf (stderr, "expr: syntax error\n");
	exit (2);
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

	/* attempt to coerce both arguments to integers */
	(void) to_integer (a);
	(void) to_integer (b);

	/* But if either one of them really is a string, do 
	   a string comparison */
	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);	
		r = make_integer (strcmp (a->u.s, b->u.s) == 0);
	} else {
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

	/* attempt to coerce both arguments to integers */
	(void) to_integer (a);
	(void) to_integer (b);

	/* But if either one of them really is a string, do 
	   a string comparison */
	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcmp (a->u.s, b->u.s) > 0);
	} else {
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

	/* attempt to coerce both arguments to integers */
	(void) to_integer (a);
	(void) to_integer (b);

	/* But if either one of them really is a string, do 
	   a string comparison */
	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcmp (a->u.s, b->u.s) < 0);
	} else {
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

	/* attempt to coerce both arguments to integers */
	(void) to_integer (a);
	(void) to_integer (b);

	/* But if either one of them really is a string, do 
	   a string comparison */
	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcmp (a->u.s, b->u.s) >= 0);
	} else {
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

	/* attempt to coerce both arguments to integers */
	(void) to_integer (a);
	(void) to_integer (b);

	/* But if either one of them really is a string, do 
	   a string comparison */
	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcmp (a->u.s, b->u.s) <= 0);
	} else {
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

	/* attempt to coerce both arguments to integers */
	(void) to_integer (a);
	(void) to_integer (b);

	/* But if either one of them really is a string, do 
	   a string comparison */
	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer (strcmp (a->u.s, b->u.s) != 0);
	} else {
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
		fprintf (stderr, "expr: non-numeric argument\n");
		exit (2);
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
		fprintf (stderr, "expr: non-numeric argument\n");
		exit (2);
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
		fprintf (stderr, "expr: non-numeric argument\n");
		exit (2);
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
		fprintf (stderr, "expr: non-numeric argument\n");
		exit (2);
	}

	if (b->u.i == 0) {
		fprintf (stderr, "expr: division by zero\n");
		exit (2);
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
		fprintf (stderr, "expr: non-numeric argument\n");
		exit (2);
	}

	if (b->u.i == 0) {
		fprintf (stderr, "expr: division by zero\n");
		exit (2);
	}

	r = make_integer (a->u.i % b->u.i);
	free_value (a);
	free_value (b);
	return r;
}
	
#include <regexp.h>

struct val *
op_colon (a, b)
struct val *a, *b;
{
	regexp *rp;
	char *newexp;
	char *p;
	char *q;

	newexp = malloc (3 * strlen (b->u.s));
	p = b->u.s;
	q = newexp;

	*q++ = '^';
	while (*p) {
		if (*p == '\\') {
			p++;
			if (*p == '(' || *p == ')') {
				*q++ = *p++;
			} else {
				*q++ = '\\';
				*q++ = *p++;
			}
		} else if (*p == '(' || *p == ')') {
			*q++ = '\\';
			*q++ = *p++;
		} else {
			*q++ = *p++;
		}
	}
	*q = 0;
				
	if ((rp = regcomp (newexp)) == NULL)
		yyerror ("invalid regular expression");

	if (regexec (rp, a->u.s)) {
		if (rp->startp[1]) {
			rp->endp[1][0] = 0;
			return (make_str (rp->startp[1]));
		} else {
			return (make_integer (rp->endp[0] - rp->startp[0]));
		}
	} else {
		return (make_integer (0));
	}
}

void
regerror (s)
const char *s;
{
	fprintf (stderr, "expr: %s\n", s);
	exit (2);
}
