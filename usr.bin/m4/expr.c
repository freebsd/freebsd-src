/*	$OpenBSD: expr.c,v 1.14 2002/04/26 16:15:16 espie Exp $	*/
/*	$NetBSD: expr.c,v 1.7 1995/09/28 05:37:31 tls Exp $	*/

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
#if 0
static char sccsid[] = "@(#)expr.c	8.2 (Berkeley) 4/29/95";
#else
#if 0
static char rcsid[] = "$OpenBSD: expr.c,v 1.14 2002/04/26 16:15:16 espie Exp $";
#endif
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <ctype.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include "mdef.h"
#include "extern.h"

/*
 *      expression evaluator: performs a standard recursive
 *      descent parse to evaluate any expression permissible
 *      within the following grammar:
 *
 *      expr    :       query EOS
 *      query   :       lor
 *              |       lor "?" query ":" query
 *      lor     :       land { "||" land }
 *      land    :       not { "&&" not }
 *	not	:	eqrel
 *		|	'!' not
 *      eqrel   :       shift { eqrelop shift }
 *      shift   :       primary { shop primary }
 *      primary :       term { addop term }
 *      term    :       exp { mulop exp }
 *	exp	:	unary { expop unary }
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
 *      eqrel   :       "="
 *              |       "=="
 *              |       "!="
 *      	|       "<"
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

#define EQL     0
#define NEQ     1
#define LSS     2
#define LEQ     3
#define GTR     4
#define GEQ     5
#define OCTAL   8
#define DECIMAL 10
#define HEX	16

static const char *nxtch;		       /* Parser scan pointer */
static const char *where;

static int query(void);
static int lor(void);
static int land(void);
static int not(void);
static int eqrel(void);
static int shift(void);
static int primary(void);
static int term(void);
static int exp(void);
static int unary(void);
static int factor(void);
static int constant(void);
static int num(void);
static int geteqrel(void);
static int skipws(void);
static void experr(const char *);

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
expr(const char *expbuf)
{
	int rval;

	nxtch = expbuf;
	where = expbuf;
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
query(void)
{
	int result, true_val, false_val;

	result = lor();
	if (skipws() != '?') {
		ungetch();
		return result;
	}

	true_val = query();
	if (skipws() != ':')
		experr("bad query");

	false_val = query();
	return result ? true_val : false_val;
}

/*
 * lor : land { '||' land }
 */
static int
lor(void)
{
	int c, vl, vr;

	vl = land();
	while ((c = skipws()) == '|') {
		if (getch() != '|')
			ungetch();
		vr = land();
		vl = vl || vr;
	}

	ungetch();
	return vl;
}

/*
 * land : not { '&&' not }
 */
static int
land(void)
{
	int c, vl, vr;

	vl = not();
	while ((c = skipws()) == '&') {
		if (getch() != '&')
			ungetch();
		vr = not();
		vl = vl && vr;
	}

	ungetch();
	return vl;
}

/*
 * not : eqrel | '!' not
 */
static int
not(void)
{
	int val, c;

	if ((c = skipws()) == '!' && getch() != '=') {
		ungetch();
		val = not();
		return !val;
	}

	if (c == '!')
		ungetch();
	ungetch();
	return eqrel();
}

/*
 * eqrel : shift { eqrelop shift }
 */
static int
eqrel(void)
{
	int vl, vr, op;

	vl = shift();
	while ((op = geteqrel()) != -1) {
		vr = shift();

		switch (op) {

		case EQL:
			vl = (vl == vr);
			break;
		case NEQ:
			vl = (vl != vr);
			break;

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
shift(void)
{
	int vl, vr, c;

	vl = primary();
	while (((c = skipws()) == '<' || c == '>') && getch() == c) {
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
primary(void)
{
	int c, vl, vr;

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
 * <term> := <exp> { <mulop> <exp> }
 */
static int
term(void)
{
	int c, vl, vr;

	vl = exp();
	while ((c = skipws()) == '*' || c == '/' || c == '%') {
		vr = exp();

		switch (c) {
		case '*':
			vl *= vr;
			break;
		case '/':
			if (vr == 0)
				errx(1, "division by zero in eval.");
			else
				vl /= vr;
			break;
		case '%':
			if (vr == 0)
				errx(1, "modulo zero in eval.");
			else
				vl %= vr;
			break;
		}
	}
	ungetch();
	return vl;
}

/*
 * <term> := <unary> { <expop> <unary> }
 */
static int
exp(void)
{
	int c, vl, vr, n;

	vl = unary();
	switch (c = skipws()) {

	case '*':
		if (getch() != '*') {
			ungetch();
			break;
		}

	case '^':
		vr = exp();
		n = 1;
		while (vr-- > 0)
			n *= vl;
		return n;
	}

	ungetch();
	return vl;
}

/*
 * unary : factor | unop unary
 */
static int
unary(void)
{
	int val, c;

	if ((c = skipws()) == '+' || c == '-' || c == '~') {
		val = unary();

		switch (c) {
		case '+':
			return val;
		case '-':
			return -val;
		case '~':
			return ~val;
		}
	}

	ungetch();
	return factor();
}

/*
 * factor : constant | '(' query ')'
 */
static int
factor(void)
{
	int val;

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
constant(void)
{
	int i;
	int value;
	int c;
	int v[sizeof(int)];

	if (skipws() != '\'') {
		ungetch();
		return num();
	}
	for (i = 0; i < (int)sizeof(int); i++) {
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
num(void)
{
	int rval, c, base;
	int ndig;

	rval = 0;
	ndig = 0;
	c = skipws();
	if (c == '0') {
		c = skipws();
		if (c == 'x' || c == 'X') {
			base = HEX;
			c = skipws();
		} else {
			base = OCTAL;
			ndig++;
		}
	} else
		base = DECIMAL;
	for(;;) {
		switch(c) {
			case '8': case '9':
				if (base == OCTAL)
					goto bad_digit;
				/*FALLTHRU*/
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				rval *= base;
				rval += c - '0';
				break;
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
				c = tolower(c);
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
				if (base == HEX) {
					rval *= base;
					rval += c - 'a' + 10;
					break;
				}
				/*FALLTHRU*/
			default:
				goto bad_digit;
		}
		c = getch();
		ndig++;
	}
bad_digit:
	ungetch();

	if (ndig == 0)
		experr("bad constant");

	return rval;
}

/*
 * eqrel : '=' | '==' | '!=' | '<' | '>' | '<=' | '>='
 */
static int
geteqrel(void)
{
	int c1, c2;

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
skipws(void)
{
	int c;

	while ((c = getch()) <= ' ' && c > EOS)
		;
	return c;
}

/*
 * resets environment to eval(), prints an error
 * and forces eval to return FALSE.
 */
static void
experr(const char *msg)
{
	printf("m4: %s in expr %s.\n", msg, where);
	longjmp(expjump, -1);
}
