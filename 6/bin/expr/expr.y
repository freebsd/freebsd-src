%{
/*-
 * Written by Pace Willisson (pace@blitz.com) 
 * and placed in the public domain.
 *
 * Largely rewritten by J.T. Conklin (jtc@wimsey.com)
 *
 * $FreeBSD$
 */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
  
/*
 * POSIX specifies a specific error code for syntax errors.  We exit
 * with this code for all errors.
 */
#define	ERR_EXIT	2

enum valtype {
	integer, numeric_string, string
} ;

struct val {
	enum valtype type;
	union {
		char *s;
		intmax_t i;
	} u;
} ;

struct val *result;

int		chk_div(intmax_t, intmax_t);
int		chk_minus(intmax_t, intmax_t, intmax_t);
int		chk_plus(intmax_t, intmax_t, intmax_t);
int		chk_times(intmax_t, intmax_t, intmax_t);
void		free_value(struct val *);
int		is_zero_or_null(struct val *);
int		isstring(struct val *);
struct val	*make_integer(intmax_t);
struct val	*make_str(const char *);
struct val	*op_and(struct val *, struct val *);
struct val	*op_colon(struct val *, struct val *);
struct val	*op_div(struct val *, struct val *);
struct val	*op_eq(struct val *, struct val *);
struct val	*op_ge(struct val *, struct val *);
struct val	*op_gt(struct val *, struct val *);
struct val	*op_le(struct val *, struct val *);
struct val	*op_lt(struct val *, struct val *);
struct val	*op_minus(struct val *, struct val *);
struct val	*op_ne(struct val *, struct val *);
struct val	*op_or(struct val *, struct val *);
struct val	*op_plus(struct val *, struct val *);
struct val	*op_rem(struct val *, struct val *);
struct val	*op_times(struct val *, struct val *);
intmax_t	to_integer(struct val *);
void		to_string(struct val *);
int		yyerror(const char *);
int		yylex(void);
int		yyparse(void);

static int	eflag;
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
make_integer(intmax_t i)
{
	struct val *vp;

	vp = (struct val *) malloc (sizeof (*vp));
	if (vp == NULL) {
		errx(ERR_EXIT, "malloc() failed");
	}

	vp->type = integer;
	vp->u.i  = i;
	return vp; 
}

struct val *
make_str(const char *s)
{
	struct val *vp;
	char *ep;

	vp = (struct val *) malloc (sizeof (*vp));
	if (vp == NULL || ((vp->u.s = strdup (s)) == NULL)) {
		errx(ERR_EXIT, "malloc() failed");
	}

	/*
	 * Previously we tried to scan the string to see if it ``looked like''
	 * an integer (erroneously, as it happened).  Let strtoimax() do the
	 * dirty work.  We could cache the value, except that we are using
	 * a union and need to preserve the original string form until we
	 * are certain that it is not needed.
	 *
	 * IEEE Std.1003.1-2001 says:
	 * /integer/ An argument consisting only of an (optional) unary minus  
	 *	     followed by digits.          
	 *
	 * This means that arguments which consist of digits followed by
	 * non-digits MUST NOT be considered integers.  strtoimax() will
	 * figure this out for us.
	 */
	if (eflag)
		(void)strtoimax(s, &ep, 10);
	else
		(void)strtol(s, &ep, 10);

	if (*ep != '\0')
		vp->type = string;
	else	
		vp->type = numeric_string;

	return vp;
}


void
free_value(struct val *vp)
{
	if (vp->type == string || vp->type == numeric_string)
		free (vp->u.s);	
}


intmax_t
to_integer(struct val *vp)
{
	intmax_t i;

	if (vp->type == integer)
		return 1;

	if (vp->type == string)
		return 0;

	/* vp->type == numeric_string, make it numeric */
	errno = 0;
	if (eflag) {
		i  = strtoimax(vp->u.s, (char **)NULL, 10);
		if (errno == ERANGE)
			err(ERR_EXIT, NULL);
	} else {
		i = strtol(vp->u.s, (char **)NULL, 10);
	}

	free (vp->u.s);
	vp->u.i = i;
	vp->type = integer;
	return 1;
}

void
to_string(struct val *vp)
{
	char *tmp;

	if (vp->type == string || vp->type == numeric_string)
		return;

	/*
	 * log_10(x) ~= 0.3 * log_2(x).  Rounding up gives the number
	 * of digits; add one each for the sign and terminating null
	 * character, respectively.
	 */
#define	NDIGITS(x) (3 * (sizeof(x) * CHAR_BIT) / 10 + 1 + 1 + 1)
	tmp = malloc(NDIGITS(vp->u.i));
	if (tmp == NULL)
		errx(ERR_EXIT, "malloc() failed");

	sprintf(tmp, "%jd", vp->u.i);
	vp->type = string;
	vp->u.s  = tmp;
}


int
isstring(struct val *vp)
{
	/* only TRUE if this string is not a valid integer */
	return (vp->type == string);
}


int
yylex(void)
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
is_zero_or_null(struct val *vp)
{
	if (vp->type == integer) {
		return (vp->u.i == 0);
	} else {
		return (*vp->u.s == 0 || (to_integer (vp) && vp->u.i == 0));
	}
	/* NOTREACHED */
}

int
main(int argc, char *argv[])
{
	int c;

	setlocale (LC_ALL, "");
	if (getenv("EXPR_COMPAT") != NULL
	    || check_utility_compat("expr")) {
		av = argv + 1;
		eflag = 1;
	} else {
		while ((c = getopt(argc, argv, "e")) != -1)
			switch (c) {
			case 'e':
				eflag = 1;
				break;

			default:
				fprintf(stderr,
				    "usage: expr [-e] expression\n");
				exit(ERR_EXIT);
			}
		av = argv + optind;
	}

	yyparse();

	if (result->type == integer)
		printf("%jd\n", result->u.i);
	else
		printf("%s\n", result->u.s);

	return (is_zero_or_null(result));
}

int
yyerror(const char *s __unused)
{
	errx(ERR_EXIT, "syntax error");
}


struct val *
op_or(struct val *a, struct val *b)
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
op_and(struct val *a, struct val *b)
{
	if (is_zero_or_null (a) || is_zero_or_null (b)) {
		free_value (a);
		free_value (b);
		return (make_integer ((intmax_t)0));
	} else {
		free_value (b);
		return (a);
	}
}

struct val *
op_eq(struct val *a, struct val *b)
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);	
		r = make_integer ((intmax_t)(strcoll (a->u.s, b->u.s) == 0));
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer ((intmax_t)(a->u.i == b->u.i));
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_gt(struct val *a, struct val *b)
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer ((intmax_t)(strcoll (a->u.s, b->u.s) > 0));
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer ((intmax_t)(a->u.i > b->u.i));
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_lt(struct val *a, struct val *b)
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer ((intmax_t)(strcoll (a->u.s, b->u.s) < 0));
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer ((intmax_t)(a->u.i < b->u.i));
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_ge(struct val *a, struct val *b)
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer ((intmax_t)(strcoll (a->u.s, b->u.s) >= 0));
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer ((intmax_t)(a->u.i >= b->u.i));
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_le(struct val *a, struct val *b)
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer ((intmax_t)(strcoll (a->u.s, b->u.s) <= 0));
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer ((intmax_t)(a->u.i <= b->u.i));
	}

	free_value (a);
	free_value (b);
	return r;
}

struct val *
op_ne(struct val *a, struct val *b)
{
	struct val *r;

	if (isstring (a) || isstring (b)) {
		to_string (a);
		to_string (b);
		r = make_integer ((intmax_t)(strcoll (a->u.s, b->u.s) != 0));
	} else {
		(void)to_integer(a);
		(void)to_integer(b);
		r = make_integer ((intmax_t)(a->u.i != b->u.i));
	}

	free_value (a);
	free_value (b);
	return r;
}

int
chk_plus(intmax_t a, intmax_t b, intmax_t r)
{

	/* sum of two positive numbers must be positive */
	if (a > 0 && b > 0 && r <= 0)
		return 1;
	/* sum of two negative numbers must be negative */
	if (a < 0 && b < 0 && r >= 0)
		return 1;
	/* all other cases are OK */
	return 0;
}

struct val *
op_plus(struct val *a, struct val *b)
{
	struct val *r;

	if (!to_integer(a) || !to_integer(b)) {
		errx(ERR_EXIT, "non-numeric argument");
	}

	if (eflag) {
		r = make_integer(a->u.i + b->u.i);
		if (chk_plus(a->u.i, b->u.i, r->u.i)) {
			errx(ERR_EXIT, "overflow");
		}
	} else
		r = make_integer((long)a->u.i + (long)b->u.i);

	free_value (a);
	free_value (b);
	return r;
}

int
chk_minus(intmax_t a, intmax_t b, intmax_t r)
{

	/* special case subtraction of INTMAX_MIN */
	if (b == INTMAX_MIN) {
		if (a >= 0)
			return 1;
		else
			return 0;
	}
	/* this is allowed for b != INTMAX_MIN */
	return chk_plus (a, -b, r);
}

struct val *
op_minus(struct val *a, struct val *b)
{
	struct val *r;

	if (!to_integer(a) || !to_integer(b)) {
		errx(ERR_EXIT, "non-numeric argument");
	}

	if (eflag) {
		r = make_integer(a->u.i - b->u.i);
		if (chk_minus(a->u.i, b->u.i, r->u.i)) {
			errx(ERR_EXIT, "overflow");
		}
	} else
		r = make_integer((long)a->u.i - (long)b->u.i);

	free_value (a);
	free_value (b);
	return r;
}

int
chk_times(intmax_t a, intmax_t b, intmax_t r)
{
	/* special case: first operand is 0, no overflow possible */
	if (a == 0)
		return 0;
	/* cerify that result of division matches second operand */
	if (r / a != b)
		return 1;
	return 0;
}

struct val *
op_times(struct val *a, struct val *b)
{
	struct val *r;

	if (!to_integer(a) || !to_integer(b)) {
		errx(ERR_EXIT, "non-numeric argument");
	}

	if (eflag) {
		r = make_integer(a->u.i * b->u.i);
		if (chk_times(a->u.i, b->u.i, r->u.i)) {
			errx(ERR_EXIT, "overflow");
		}
	} else
		r = make_integer((long)a->u.i * (long)b->u.i);

	free_value (a);
	free_value (b);
	return (r);
}

int
chk_div(intmax_t a, intmax_t b)
{
	/* div by zero has been taken care of before */
	/* only INTMAX_MIN / -1 causes overflow */
	if (a == INTMAX_MIN && b == -1)
		return 1;
	/* everything else is OK */
	return 0;
}

struct val *
op_div(struct val *a, struct val *b)
{
	struct val *r;

	if (!to_integer(a) || !to_integer(b)) {
		errx(ERR_EXIT, "non-numeric argument");
	}

	if (b->u.i == 0) {
		errx(ERR_EXIT, "division by zero");
	}

	if (eflag) {
		r = make_integer(a->u.i / b->u.i);
		if (chk_div(a->u.i, b->u.i)) {
			errx(ERR_EXIT, "overflow");
		}
	} else
		r = make_integer((long)a->u.i / (long)b->u.i);

	free_value (a);
	free_value (b);
	return r;
}
	
struct val *
op_rem(struct val *a, struct val *b)
{
	struct val *r;

	if (!to_integer(a) || !to_integer(b)) {
		errx(ERR_EXIT, "non-numeric argument");
	}

	if (b->u.i == 0) {
		errx(ERR_EXIT, "division by zero");
	}

	if (eflag)
		r = make_integer(a->u.i % b->u.i);
	        /* chk_rem necessary ??? */
	else
		r = make_integer((long)a->u.i % (long)b->u.i);

	free_value (a);
	free_value (b);
	return r;
}
	
struct val *
op_colon(struct val *a, struct val *b)
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
		errx(ERR_EXIT, "%s", errbuf);
	}

	/* compare string against pattern */
	/* remember that patterns are anchored to the beginning of the line */
	if (regexec(&rp, a->u.s, (size_t)2, rm, 0) == 0 && rm[0].rm_so == 0) {
		if (rm[1].rm_so >= 0) {
			*(a->u.s + rm[1].rm_eo) = '\0';
			v = make_str (a->u.s + rm[1].rm_so);

		} else {
			v = make_integer ((intmax_t)(rm[0].rm_eo - rm[0].rm_so));
		}
	} else {
		if (rp.re_nsub == 0) {
			v = make_integer ((intmax_t)0);
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
