/*  File   : expr.c
    Authors: Mike Lutz & Bob Harper
    Editors: Ozan Yigit & Richard A. O'Keefe
    Updated: %G%
    Purpose: arithmetic expression evaluator.

    expr() performs a standard recursive descent parse to evaluate any
    expression permitted byf the following grammar:

      expr    :       query EOS
      query   :       lor
              |       lor "?" query ":" query
      lor     :       land { "||" land }	or OR,  for Pascal
      land    :       bor { "&&" bor }		or AND, for Pascal
      bor     :       bxor { "|" bxor }
      bxor    :       band { "^" band }
      band    :       eql { "&" eql }
      eql     :       relat { eqrel relat }
      relat   :       shift { rel shift }
      shift   :       primary { shop primary }
      primary :       term { addop term }
      term    :       unary { mulop unary }
      unary   :       factor
              |       unop unary
      factor  :       constant
              |       "(" query ")"
      constant:       num
              |       "'" CHAR "'"		or '"' CHAR '"'
      num     :       DIGIT			full ANSI C syntax
              |       DIGIT num
      shop    :       "<<"
              |       ">>"
      eqlrel  :       "="
              |       "=="
              |       "!="
      rel     :       "<"			or <>, Pascal not-equal
              |       ">"
              |       "<="			or =<, for Prolog users.
              |       ">="

    This expression evaluator was lifted from a public-domain
    C Pre-Processor included with the DECUS C Compiler distribution.
    It has been hacked somewhat to be suitable for m4.

    26-Mar-1993		Changed to work in any of EBCDIC, ASCII, DEC MNCS,
			or ISO 8859/n.

    26-Mar-1993		Changed to use "long int" rather than int, so that
			we get the same 32-bit arithmetic on a PC as on a Sun.
			It isn't fully portable, of course, but then on a 64-
			bit machine we _want_ 64-bit arithmetic...
			Shifting rewritten (using LONG_BIT) to give signed
			shifts even when (long) >> (long) is unsigned.

    26-Mar-1993		I finally got sick of the fact that &&, ||, and ?:
			don't do conditional evaluation.  What is the good
			of having eval(0&&(1/0)) crash and dump core?  Now
			every function has a doit? argument.

    26-Mar-1993		charcon() didn't actually accept 'abcd', which it
			should have.  Fixed it.

    20-Apr-1993		eval(1/0) and eval(1%0) dumped core and crashed.
			This is also true of the System V r 3.2 m4, but
			it isn't good enough for ours!  Changed it so that
			x % 0 => x	as per Concrete Mathematics
			x / 0 => error and return 0 from expr().
*/

#define FALSE   0
#define	TRUE	1

#include <stdio.h>
#include <setjmp.h>
static jmp_buf expjump;		/* Error exit point for expr() */

static unsigned char *nxtchr;	/* Parser scan pointer */

#define	deblank0	while ((unsigned)(*nxtchr-1) < ' ') nxtchr++
#define deblank1	while ((unsigned)(*++nxtchr - 1) < ' ')
#define deblank2	nxtchr++; deblank1

#include "ourlims.h"
static char digval[1+UCHAR_MAX];

/*  This file should work in any C implementation that doesn't have too
    many characters to fit in one table.  We use a table to convert
    (unsigned) characters to numeric codes:
	 0 to  9	for '0' to '9'
	10 to 35	for 'a' to 'z'
	10 to 35	for 'A' to 'Z'
	36		for '_'
    Instead of asking whether tolower(c) == 'a' we ask whether
    digval[c] == DIGIT_A, and so on.  This essentially duplicates the
    chtype[] table in main.c; we should use just one table.
*/
#define	DIGIT_A 10
#define	DIGIT_B 11
#define	DIGIT_C 12
#define	DIGIT_D 13
#define	DIGIT_E 14
#define	DIGIT_F 15
#define	DIGIT_G 16
#define DIGIT_H 17
#define	DIGIT_I	18
#define	DIGIT_J 19
#define DIGIT_K 20
#define	DIGIT_L	21
#define DIGIT_M 22
#define DIGIT_N 23
#define	DIGIT_O 24
#define	DIGIT_P 25
#define	DIGIT_Q 26
#define	DIGIT_R	27
#define	DIGIT_S 28
#define	DIGIT_T 29
#define	DIGIT_U 30
#define	DIGIT_V 31
#define	DIGIT_W 32
#define	DIGIT_X 33
#define	DIGIT_Y 34
#define	DIGIT_Z 35


#ifdef	__STDC__
static long int query(int);
#else
static long int query();
#endif


/*  experr(msg)
    prints an error message, resets environment to expr(), and
    forces expr() to return FALSE.
*/
void experr(msg)
    char *msg;
    {
	(void) fprintf(stderr, "m4: %s\n", msg);
	longjmp(expjump, -1);	/* Force expr() to return FALSE */
    }


/*  <numcon> ::= '0x' <hex> | '0X' <hex> | '0' <oct> | <dec>
    For ANSI C, an integer may be followed by u, l, ul, or lu,
    in any mix of cases.  We accept and ignore those letters;
    all the numbers are treated as long.
*/
static long int numcon(doit)
    int doit;
    {
	register long int v;	/* current value */
	register int b;		/* base (radix) */
	register int c;		/* character or digit value */

	if (!doit) {
	    do nxtchr++; while (digval[*nxtchr] <= 36);
	    deblank0;
	    return 0;
	}

	v = digval[*nxtchr++];	/* We already know it's a digit */
	if (v != 0) {
	    b = 10;		/* decimal number */
	} else
	if (digval[*nxtchr] == DIGIT_X) {
	    nxtchr++;
	    b = 16;		/* hexadecimal number */
	} else {
	    b = 8;		/* octal number */
	}
	do {
	    while (digval[c = *nxtchr++] < b) v = v*b + digval[c];
	} while (c == '_');
	while (digval[c] == DIGIT_L || digval[c] == DIGIT_U) c = *nxtchr++;
	nxtchr--;		/* unread c */
	if ((unsigned)(c-1) < ' ') { deblank1; }
	return v;
    }


/*  <charcon> ::= <qt> { <char> } <qt>
    Note: multibyte constants are accepted.
    Note: BEL (\a) and ESC (\e) have the same values in EBCDIC and ASCII.
*/
static long int charcon(doit)
    int doit;
    {
	register int i;
	long int value;
	register int c;
	int q;
	int v[sizeof value];

	q = *nxtchr++;		/* the quote character */
	for (i = 0; ; i++) {
	    c = *nxtchr++;
	    if (c == q) {	/* end of literal, or doubled quote */
		if (*nxtchr != c) break;
		nxtchr++;	/* doubled quote stands for one quote */
	    }
	    if (i == sizeof value) experr("Unterminated character constant");
	    if (c == '\\') {
		switch (c = *nxtchr++) {
		    case '0': case '1': case '2': case '3':
		    case '4': case '5': case '6': case '7':
			c -= '0';
			if ((unsigned)(*nxtchr - '0') < 8)
			    c = (c << 3) | (*nxtchr++ - '0');
			if ((unsigned)(*nxtchr - '0') < 8)
			    c = (c << 3) | (*nxtchr++ - '0');
			break;
		    case 'n': case 'N': c = '\n'; break;
		    case 'r': case 'R': c = '\r'; break;
		    case 't': case 'T': c = '\t'; break;
		    case 'b': case 'B': c = '\b'; break;
		    case 'f': case 'F': c = '\f'; break;
		    case 'a': case 'A': c = 007;  break;
		    case 'e': case 'E': c = 033;  break;
#if	' ' == 64
		    case 'd': case 'D': c = 045;  break; /*EBCDIC DEL */
#else
		    case 'd': case 'D': c = 127;  break; /* ASCII DEL */
#endif
		    default :			  break;
		}
	    }
	    v[i] = c;
	}
	deblank0;
	if (!doit) return 0;
	for (value = 0; --i >= 0; ) value = (value << CHAR_BIT) | v[i];
	return value;
    }


/*  <unary> ::= <unop> <unary> | <factor>
    <unop> ::= '!' || '~' | '-'
    <factor> ::= '(' <query> ')' | <'> <char> <'> | <"> <char> <"> | <num>
*/
static long int unary(doit)
    int doit;
    {
	long int v;

	switch (nxtchr[0]) {
	    case 'n': case 'N':
			if (digval[nxtchr[1]] != DIGIT_O
			||  digval[nxtchr[2]] != DIGIT_T)
			    experr("Bad 'not'");
			nxtchr += 2;
	    case '!':	deblank1; return !unary(doit);
	    case '~':	deblank1; return ~unary(doit);
	    case '-':	deblank1; return -unary(doit);
	    case '+':	deblank1; return  unary(doit);
	    case '(':	deblank1; v = query(doit);
			if (nxtchr[0] != ')') experr("Bad factor");
			deblank1; return v;
	    case '\'':
	    case '\"':	return charcon(doit);
	    case '0': case '1': case '2':
	    case '3': case '4': case '5':
	    case '6': case '7': case '8':
	    case '9':	return numcon(doit);
	    default :   experr("Bad constant");
	}
	return 0;	/*NOTREACHED*/
    }


/*  <term> ::= <unary> { <mulop> <unary> }
    <mulop> ::= '*' | '/' || '%'
*/
static long int term(doit)
    int doit;
    {
	register long int vl, vr;

	vl = unary(doit);
	for (;;)
	    switch (nxtchr[0]) {
		case '*':
		    deblank1;
		    vr = unary(doit);
		    if (doit) vl *= vr;
		    break;
		case 'd': case 'D':
		    if (digval[nxtchr[1]] != DIGIT_I
		    ||  digval[nxtchr[2]] != DIGIT_V)
			experr("Bad 'div'");
		    nxtchr += 2;
		    /*FALLTHROUGH*/
		case '/':
		    deblank1;
		    vr = unary(doit);
		    if (doit) {
			if (vr == 0) experr("Division by 0");
			vl /= vr;
		    }
		    break;
		case 'm': case 'M':
		    if (digval[nxtchr[1]] != DIGIT_O
		    ||  digval[nxtchr[2]] != DIGIT_D)
			experr("Bad 'mod'");
		    nxtchr += 2;
		    /*FALLTHROUGH*/
		case '%':
		    deblank1;
		    vr = unary(doit);
		    if (doit) {
			if (vr != 0) vl %= vr;
		    }
		    break;
		default:
		    return vl;
	    }
	/*NOTREACHED*/
    }

/*  <primary> ::= <term> { <addop> <term> }
    <addop> ::= '+' | '-'
*/
static long int primary(doit)
    int doit;
    {
	register long int vl;

	vl = term(doit);
	for (;;)
	    if (nxtchr[0] == '+') {
		deblank1;
		if (doit) vl += term(doit); else (void)term(doit);
	    } else
	    if (nxtchr[0] == '-') {
		deblank1;
		if (doit) vl -= term(doit); else (void)term(doit);
	    } else
		return vl;
	/*NOTREACHED*/
    }


/*  <shift> ::= <primary> { <shop> <primary> }
    <shop> ::= '<<' | '>>'
*/
static long int shift(doit)
    int doit;
    {
	register long int vl, vr;

	vl = primary(doit);
	for (;;) {
	    if (nxtchr[0] == '<' && nxtchr[1] == '<') {
		deblank2;
		vr = primary(doit);
	    } else
	    if (nxtchr[0] == '>' && nxtchr[1] == '>') {
		deblank2;
		vr = -primary(doit);
	    } else {
		return vl;
	    }
	    /* The following code implements shifts portably */
	    /* Shifts are signed shifts, and the shift count */
	    /* acts like repeated one-bit shifts, not modulo anything */
	    if (doit) {
		if (vr >= LONG_BIT) {
		    vl = 0;
		} else
		if (vr <= -LONG_BIT) {
		    vl = -(vl < 0);
		} else
		if (vr > 0) {
		    vl <<= vr;
		} else
		if (vr < 0) {
		    vl = (vl >> -vr) | (-(vl < 0) << (LONG_BIT + vr));
		}
	    }
	}
	/*NOTREACHED*/
    }


/*  <relat> ::= <shift> { <rel> <shift> }
    <rel> ::= '<=' | '>=' | '=<' | '=>' | '<' | '>'
    Here I rely on the fact that '<<' and '>>' are swallowed by <shift>
*/
static long int relat(doit)
    int doit;
    {
	register long int vl;

	vl = shift(doit);
	for (;;)
	    switch (nxtchr[0]) {
		case '=':
		    switch (nxtchr[1]) {
			case '<':			/* =<, take as <= */
			    deblank2;
			    vl = vl <= shift(doit);
			    break;
			case '>':			/* =>, take as >= */
			    deblank2;
			    vl = vl >= shift(doit);
			    break;
			default:			/* == or =; OOPS */
			    return vl;
		    }
		    break;
		case '<':
		    if (nxtchr[1] == '=') {		/* <= */
			deblank2;
			vl = vl <= shift(doit);
		    } else
		    if (nxtchr[1] == '>') {		/* <> (Pascal) */
			deblank2;
			vl = vl != shift(doit);
		    } else {				/* < */
			deblank1;
			vl = vl < shift(doit);
		    }
		    break;
		case '>':
		    if (nxtchr[1] == '=') {		/* >= */
			deblank2;
			vl = vl >= shift(doit);
		    } else {				/* > */
			deblank1;
			vl = vl > shift(doit);
		    }
		    break;
		default:
		    return vl;
	}
	/*NOTREACHED*/
    }


/*  <eql> ::= <relat> { <eqrel> <relat> }
    <eqlrel> ::= '!=' | '==' | '='
*/
static long int eql(doit)
    int doit;
    {
	register long int vl;

	vl = relat(doit);
	for (;;)
	    if (nxtchr[0] == '!' && nxtchr[1] == '=') {
		deblank2;
		vl = vl != relat(doit);
	    } else
	    if (nxtchr[0] == '=' && nxtchr[1] == '=') {
		deblank2;
		vl = vl == relat(doit);
	    } else
	    if (nxtchr[0] == '=') {
		deblank1;
		vl = vl == relat(doit);
	    } else
		return vl;
	/*NOTREACHED*/
    }


/*  <band> ::= <eql> { '&' <eql> }
*/
static long int band(doit)
    int doit;
    {
	register long int vl;

	vl = eql(doit);
	while (nxtchr[0] == '&' && nxtchr[1] != '&') {
	    deblank1;
	    if (doit) vl &= eql(doit); else (void)eql(doit);
	}
	return vl;
    }


/*  <bxor> ::= <band> { '^' <band> }
*/
static long int bxor(doit)
    int doit;
    {
	register long int vl;

	vl = band(doit);
	while (nxtchr[0] == '^') {
	    deblank1;
	    if (doit) vl ^= band(doit); else (void)band(doit);
	}
	return vl;
    }


/*  <bor> ::= <bxor> { '|' <bxor> }
*/
static long int bor(doit)
    int doit;
    {
	register long int vl;

	vl = bxor(doit);
	while (nxtchr[0] == '|' && nxtchr[1] != '|') {
	    deblank1;
	    if (doit) vl |= bxor(doit); else (void)bxor(doit);
	}
	return vl;
    }


/*  <land> ::= <bor> { '&&' <bor> }
*/
static long int land(doit)
    int doit;
    {
	register long int vl;

	vl = bor(doit);
	for (;;) {
	    if (nxtchr[0] == '&') {
		if (nxtchr[1] != '&') break;
		deblank2;
	    } else
	    if (digval[nxtchr[0]] == DIGIT_A) {
		if (digval[nxtchr[1]] != DIGIT_N) break;
		if (digval[nxtchr[2]] != DIGIT_D) break;
		nxtchr += 2; deblank1;
	    } else {
		/* neither && nor and */
		break;
	    }
	    vl = bor(doit && vl) != 0;
	}
	return vl;
    }


/*  <lor> ::= <land> { '||' <land> }
*/
static long int lor(doit)
    int doit;
    {
	register long int vl;

	vl = land(doit);
	for (;;) {
	    if (nxtchr[0] == '|') {
		if (nxtchr[1] != '|') break;
	    } else
	    if (digval[nxtchr[0]] == DIGIT_O) {
		if (digval[nxtchr[1]] != DIGIT_R) break;
	    } else {
		/* neither || nor or */
		break;
	    }
	    deblank2;
	    vl = land(doit && !vl) != 0;
	}
	return vl;
    }


/*  <query> ::= <lor> [ '?' <query> ':' <query> ]
*/
static long int query(doit)
    int doit;
    {
	register long int bool, true_val, false_val;

	bool = lor(doit);
	if (*nxtchr != '?') return bool;
	deblank1;
	true_val = query(doit && bool);
	if (*nxtchr != ':') experr("Bad query");
	deblank1;
	false_val = query(doit && !bool);
	return bool ? true_val : false_val;
    }


static void initialise_digval()
    {
	register unsigned char *s;
	register int c;

	for (c = 0; c <= UCHAR_MAX; c++) digval[c] = 99;
	for (c =  0, s = (unsigned char *)"0123456789";
	/*while*/ *s;
	/*doing*/ digval[*s++] = c++) /* skip */;
	for (c = 10, s = (unsigned char *)"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	/*while*/ *s;
	/*doing*/ digval[*s++] = c++) /* skip */;
	for (c = 10, s = (unsigned char *)"abcdefghijklmnopqrstuvwxyz";
	/*while*/ *s;
	/*doing*/ digval[*s++] = c++) /* skip */;
	digval['_'] = 36;
    }


long int expr(expbuf)
    char *expbuf;
    {
	register int rval;

	if (digval['1'] == 0) initialise_digval();
	nxtchr = (unsigned char *)expbuf;
	deblank0;
	if (setjmp(expjump) != 0) return FALSE;
	rval = query(TRUE);
	if (*nxtchr) experr("Ill-formed expression");
	return rval;
    }

