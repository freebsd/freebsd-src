/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)expr.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/cdefs.h>
#include <stdio.h>

/*
 *      expression evaluator: performs a standard recursive
 *      descent parse to evaluate any expression permissible
 *      within the following grammar:
 *
 *      expr    :       query EOS
 *      query   :       lor
 *              |       lor "?" query ":" query
 *      lor     :       land { "||" land }
 *      land    :       bor { "&&" bor }
 *      bor     :       bxor { "|" bxor }
 *      bxor    :       band { "^" band }
 *      band    :       eql { "&" eql }
 *      eql     :       relat { eqrel relat }
 *      relat   :       shift { rel shift }
 *      shift   :       primary { shop primary }
 *      primary :       term { addop term }
 *      term    :       unary { mulop unary }
 *      unary   :       factor
 *              |       unop unary
 *      factor  :       constant
 *              |       "(" query ")"
 *      constant:       num
 *              |       "'" CHAR "'"
 *      num     :       DIGIT
 *              |       DIGIT num
 *      shop    :       "<<"
 *              |       ">>"
 *      eqlrel  :       "="
 *              |       "=="
 *              |       "!="
 *      rel     :       "<"
 *              |       ">"
 *              |       "<="
 *              |       ">="
 *
 *
 *      This expression evaluator is lifted from a public-domain
 *      C Pre-Processor included with the DECUS C Compiler distribution.
 *      It is hacked somewhat to be suitable for m4.
 *
 *      Originally by:  Mike Lutz
 *                      Bob Harper
 */

#define TRUE    1
#define FALSE   0
#define EOS     (char) 0
#define EQL     0
#define NEQ     1
#define LSS     2
#define LEQ     3
#define GTR     4
#define GEQ     5
#define OCTAL   8
#define DECIMAL 10

static char *nxtch;		       /* Parser scan pointer */

static int query __P((void));
static int lor __P((void));
static int land __P((void));
static int bor __P((void));
static int bxor __P((void));
static int band __P((void));
static int eql __P((void));
static int relat __P((void));
static int shift __P((void));
static int primary __P((void));
static int term __P((void));
static int unary __P((void));
static int factor __P((void));
static int constant __P((void));
static int num __P((void));
static int geteql __P((void));
static int getrel __P((void));
static int skipws __P((void));
static void experr __P((char *));

/*
 * For longjmp
 */
#include <setjmp.h>
static jmp_buf expjump;

/*
 * macros:
 *      ungetch - Put back the last character examined.
 *      getch   - return the next character from expr string.
 */
#define ungetch()       nxtch--
#define getch()         *nxtch++

int
expr(expbuf)
char *expbuf;
{
	register int rval;

	nxtch = expbuf;
	if (setjmp(expjump) != 0)
		return FALSE;

	rval = query();
	if (skipws() == EOS)
		return rval;

	printf("m4: ill-formed expression.\n");
	return FALSE;
}

/*
 * query : lor | lor '?' query ':' query
 */
static int
query()
{
	register int bool, true_val, false_val;

	bool = lor();
	if (skipws() != '?') {
		ungetch();
		return bool;
	}

	true_val = query();
	if (skipws() != ':')
		experr("bad query");

	false_val = query();
	return bool ? true_val : false_val;
}

/*
 * lor : land { '||' land }
 */
static int
lor()
{
	register int c, vl, vr;

	vl = land();
	while ((c = skipws()) == '|' && getch() == '|') {
		vr = land();
		vl = vl || vr;
	}

	if (c == '|')
		ungetch();
	ungetch();
	return vl;
}

/*
 * land : bor { '&&' bor }
 */
static int
land()
{
	register int c, vl, vr;

	vl = bor();
	while ((c = skipws()) == '&' && getch() == '&') {
		vr = bor();
		vl = vl && vr;
	}

	if (c == '&')
		ungetch();
	ungetch();
	return vl;
}

/*
 * bor : bxor { '|' bxor }
 */
static int
bor()
{
	register int vl, vr, c;

	vl = bxor();
	while ((c = skipws()) == '|' && getch() != '|') {
		ungetch();
		vr = bxor();
		vl |= vr;
	}

	if (c == '|')
		ungetch();
	ungetch();
	return vl;
}

/*
 * bxor : band { '^' band }
 */
static int
bxor()
{
	register int vl, vr;

	vl = band();
	while (skipws() == '^') {
		vr = band();
		vl ^= vr;
	}

	ungetch();
	return vl;
}

/*
 * band : eql { '&' eql }
 */
static int
band()
{
	register int vl, vr, c;

	vl = eql();
	while ((c = skipws()) == '&' && getch() != '&') {
		ungetch();
		vr = eql();
		vl &= vr;
	}

	if (c == '&')
		ungetch();
	ungetch();
	return vl;
}

/*
 * eql : relat { eqrel relat }
 */
static int
eql()
{
	register int vl, vr, rel;

	vl = relat();
	while ((rel = geteql()) != -1) {
		vr = relat();

		switch (rel) {

		case EQL:
			vl = (vl == vr);
			break;
		case NEQ:
			vl = (vl != vr);
			break;
		}
	}
	return vl;
}

/*
 * relat : shift { rel shift }
 */
static int
relat()
{
	register int vl, vr, rel;

	vl = shift();
	while ((rel = getrel()) != -1) {

		vr = shift();
		switch (rel) {

		case LEQ:
			vl = (vl <= vr);
			break;
		case LSS:
			vl = (vl < vr);
			break;
		case GTR:
			vl = (vl > vr);
			break;
		case GEQ:
			vl = (vl >= vr);
			break;
		}
	}
	return vl;
}

/*
 * shift : primary { shop primary }
 */
static int
shift()
{
	register int vl, vr, c;

	vl = primary();
	while (((c = skipws()) == '<' || c == '>') && c == getch()) {
		vr = primary();

		if (c == '<')
			vl <<= vr;
		else
			vl >>= vr;
	}

	if (c == '<' || c == '>')
		ungetch();
	ungetch();
	return vl;
}

/*
 * primary : term { addop term }
 */
static int
primary()
{
	register int c, vl, vr;

	vl = term();
	while ((c = skipws()) == '+' || c == '-') {
		vr = term();
		if (c == '+')
			vl += vr;
		else
			vl -= vr;
	}

	ungetch();
	return vl;
}

/*
 * <term> := <unary> { <mulop> <unary> }
 */
static int
term()
{
	register int c, vl, vr;

	vl = unary();
	while ((c = skipws()) == '*' || c == '/' || c == '%') {
		vr = unary();

		switch (c) {
		case '*':
			vl *= vr;
			break;
		case '/':
			vl /= vr;
			break;
		case '%':
			vl %= vr;
			break;
		}
	}
	ungetch();
	return vl;
}

/*
 * unary : factor | unop unary
 */
static int
unary()
{
	register int val, c;

	if ((c = skipws()) == '!' || c == '~' || c == '-') {
		val = unary();

		switch (c) {
		case '!':
			return !val;
		case '~':
			return ~val;
		case '-':
			return -val;
		}
	}

	ungetch();
	return factor();
}

/*
 * factor : constant | '(' query ')'
 */
static int
factor()
{
	register int val;

	if (skipws() == '(') {
		val = query();
		if (skipws() != ')')
			experr("bad factor");
		return val;
	}

	ungetch();
	return constant();
}

/*
 * constant: num | 'char'
 * Note: constant() handles multi-byte constants
 */
static int
constant()
{
	register int i;
	register int value;
	register char c;
	int v[sizeof(int)];

	if (skipws() != '\'') {
		ungetch();
		return num();
	}
	for (i = 0; i < sizeof(int); i++) {
		if ((c = getch()) == '\'') {
			ungetch();
			break;
		}
		if (c == '\\') {
			switch (c = getch()) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				ungetch();
				c = num();
				break;
			case 'n':
				c = 012;
				break;
			case 'r':
				c = 015;
				break;
			case 't':
				c = 011;
				break;
			case 'b':
				c = 010;
				break;
			case 'f':
				c = 014;
				break;
			}
		}
		v[i] = c;
	}
	if (i == 0 || getch() != '\'')
		experr("illegal character constant");
	for (value = 0; --i >= 0;) {
		value <<= 8;
		value += v[i];
	}
	return value;
}

/*
 * num : digit | num digit
 */
static int
num()
{
	register int rval, c, base;
	int ndig;

	base = ((c = skipws()) == '0') ? OCTAL : DECIMAL;
	rval = 0;
	ndig = 0;
	while (c >= '0' && c <= (base == OCTAL ? '7' : '9')) {
		rval *= base;
		rval += (c - '0');
		c = getch();
		ndig++;
	}
	ungetch();
	
	if (ndig == 0)
		experr("bad constant");
	
	return rval;

}

/*
 * eqlrel : '=' | '==' | '!='
 */
static int
geteql()
{
	register int c1, c2;

	c1 = skipws();
	c2 = getch();

	switch (c1) {

	case '=':
		if (c2 != '=')
			ungetch();
		return EQL;

	case '!':
		if (c2 == '=')
			return NEQ;
		ungetch();
		ungetch();
		return -1;

	default:
		ungetch();
		ungetch();
		return -1;
	}
}

/*
 * rel : '<' | '>' | '<=' | '>='
 */
static int
getrel()
{
	register int c1, c2;

	c1 = skipws();
	c2 = getch();

	switch (c1) {

	case '<':
		if (c2 == '=')
			return LEQ;
		ungetch();
		return LSS;

	case '>':
		if (c2 == '=')
			return GEQ;
		ungetch();
		return GTR;

	default:
		ungetch();
		ungetch();
		return -1;
	}
}

/*
 * Skip over any white space and return terminating char.
 */
static int
skipws()
{
	register char c;

	while ((c = getch()) <= ' ' && c > EOS)
		;
	return c;
}

/*
 * resets environment to eval(), prints an error 
 * and forces eval to return FALSE.
 */
static void
experr(msg)
char *msg;
{
	printf("m4: %s in expr.\n", msg);
	longjmp(expjump, -1);
}
