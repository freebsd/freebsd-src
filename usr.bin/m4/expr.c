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
 *      land    :       bor { "&&" bor }
 *      bor     :       xor { "|" xor }
 *      xor     :       band { "^" eqrel }
 *      band    :       eqrel { "&" eqrel }
 *      eqrel   :       nerel { ("==" | "!=") nerel }
 *      nerel   :       shift { ("<" | ">" | "<=" | ">=") shift }
 *      shift   :       primary { ("<<" | ">>") primary }
 *      primary :       term { ("+" | "-") term }
 *      term    :       exp { ("*" | "/" | "%") exp }
 *      exp     :       unary { "**" unary }
 *      unary   :       factor
 *              |       ("+" | "-" | "~" | "!") unary
 *      factor  :       constant
 *              |       "(" query ")"
 *      constant:       num
 *              |       "'" CHAR "'"
 *      num     :       DIGIT
 *              |       DIGIT num
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

static int query(int mayeval);
static int lor(int mayeval);
static int land(int mayeval);
static int bor(int mayeval);
static int xor(int mayeval);
static int band(int mayeval);
static int eqrel(int mayeval);
static int nerel(int mayeval);
static int shift(int mayeval);
static int primary(int mayeval);
static int term(int mayeval);
static int exp(int mayeval);
static int unary(int mayeval);
static int factor(int mayeval);
static int constant(int mayeval);
static int num(int mayeval);
static int geteqrel(int mayeval);
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

	rval = query(1);
	if (skipws() == EOS)
		return rval;

	printf("m4: ill-formed expression.\n");
	return FALSE;
}

/*
 * query : lor | lor '?' query ':' query
 */
static int
query(int mayeval)
{
	int result, true_val, false_val;

	result = lor(mayeval);
	if (skipws() != '?') {
		ungetch();
		return result;
	}

	true_val = query(result);
	if (skipws() != ':')
		experr("bad query: missing \":\"");

	false_val = query(!result);
	return result ? true_val : false_val;
}

/*
 * lor : land { '||' land }
 */
static int
lor(int mayeval)
{
	int c, vl, vr;

	vl = land(mayeval);
	while ((c = skipws()) == '|') {
		if (getch() != '|') {
			ungetch();
			break;
		}
		if (vl != 0)
			mayeval = 0;
		vr = land(mayeval);
		vl = vl || vr;
	}

	ungetch();
	return vl;
}

/*
 * land : not { '&&' not }
 */
static int
land(int mayeval)
{
	int c, vl, vr;

	vl = bor(mayeval);
	while ((c = skipws()) == '&') {
		if (getch() != '&') {
			ungetch();
			break;
		}
		if (vl == 0)
			mayeval = 0;
		vr = bor(mayeval);
		vl = vl && vr;
	}

	ungetch();
	return vl;
}

/*
 * bor : xor { "|" xor }
 */
static int
bor(int mayeval)
{
	int vl, vr, c, cr;

	vl = xor(mayeval);
	while ((c = skipws()) == '|') {
		cr = getch();
		ungetch();
		if (cr == '|')
			break;
		vr = xor(mayeval);
		vl |= vr;
	}
	ungetch();
	return (vl);
}

/*
 * xor : band { "^" band }
 */
static int
xor(int mayeval)
{
	int vl, vr, c;

	vl = band(mayeval);
	while ((c = skipws()) == '^') {
		vr = band(mayeval);
		vl ^= vr;
	}
	ungetch();
	return (vl);
}

/*
 * band : eqrel { "&" eqrel }
 */
static int
band(int mayeval)
{
	int c, cr, vl, vr;

	vl = eqrel(mayeval);
	while ((c = skipws()) == '&') {
		cr = getch();
		ungetch();
		if (cr == '&')
			break;
		vr = eqrel(mayeval);
		vl &= vr;
	}
	ungetch();
	return vl;
}

/*
 * eqrel : nerel { ("==" | "!=" ) nerel }
 */
static int
eqrel(int mayeval)
{
	int vl, vr, c, cr;

	vl = nerel(mayeval);
	while ((c = skipws()) == '!' || c == '=') {
		if ((cr = getch()) != '=') {
			ungetch();
			break;
		}
		vr = nerel(mayeval);
		switch (c) {
		case '=':
			vl = (vl == vr);
			break;
		case '!':
			vl = (vl != vr);
			break;
		}
	}
	ungetch();
	return vl;
}

/*
 * nerel : shift { ("<=" | ">=" | "<" | ">") shift }
 */
static int
nerel(int mayeval)
{
	int vl, vr, c, cr;

	vl = shift(mayeval);
	while ((c = skipws()) == '<' || c == '>') {
		if ((cr = getch()) != '=') {
			ungetch();
			cr = '\0';
		}
		vr = shift(mayeval);
		switch (c) {
		case '<':
			vl = (cr == '\0') ? (vl < vr) : (vl <= vr);
			break;
		case '>':
			vl = (cr == '\0') ? (vl > vr) : (vl >= vr);
			break;
		}
	}
	ungetch();
	return vl;
}

/*
 * shift : primary { ("<<" | ">>") primary }
 */
static int
shift(int mayeval)
{
	int vl, vr, c;

	vl = primary(mayeval);
	while (((c = skipws()) == '<' || c == '>') && getch() == c) {
		vr = primary(mayeval);

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
 * primary : term { ("+" | "-") term }
 */
static int
primary(int mayeval)
{
	int c, vl, vr;

	vl = term(mayeval);
	while ((c = skipws()) == '+' || c == '-') {
		vr = term(mayeval);

		if (c == '+')
			vl += vr;
		else
			vl -= vr;
	}

	ungetch();
	return vl;
}

/*
 * term : exp { ("*" | "/" | "%") exp }
 */
static int
term(int mayeval)
{
	int c, vl, vr;

	vl = exp(mayeval);
	while ((c = skipws()) == '*' || c == '/' || c == '%') {
		vr = exp(mayeval);

		switch (c) {
		case '*':
			vl *= vr;
			break;
		case '/':
			if (!mayeval)
				/* short-circuit */;
			else if (vr == 0)
				errx(1, "division by zero in eval.");
			else
				vl /= vr;
			break;
		case '%':
			if (!mayeval)
				/* short-circuit */;
			else if (vr == 0)
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
 * exp : unary { "**" exp }
 */
static int
exp(int mayeval)
{
	int c, vl, vr, n;

	vl = unary(mayeval);
	while ((c = skipws()) == '*') {
		if (getch() != '*') {
			ungetch();
			break;
		}
		vr = unary(mayeval);
		n = 1;
		while (vr-- > 0)
			n *= vl;
		return n;
	}

	ungetch();
	return vl;
}

/*
 * unary : factor | ("+" | "-" | "~" | "!") unary
 */
static int
unary(int mayeval)
{
	int val, c;

	if ((c = skipws()) == '+' || c == '-' || c == '~' || c == '!') {
		val = unary(mayeval);

		switch (c) {
		case '+':
			return val;
		case '-':
			return -val;
		case '~':
			return ~val;
		case '!':
			return !val;
		}
	}

	ungetch();
	return factor(mayeval);
}

/*
 * factor : constant | '(' query ')'
 */
static int
factor(int mayeval)
{
	int val;

	if (skipws() == '(') {
		val = query(mayeval);
		if (skipws() != ')')
			experr("bad factor: missing \")\"");
		return val;
	}

	ungetch();
	return constant(mayeval);
}

/*
 * constant: num | 'char'
 * Note: constant() handles multi-byte constants
 */
static int
constant(int mayeval)
{
	int i;
	int value;
	int c;
	int v[sizeof(int)];

	if (skipws() != '\'') {
		ungetch();
		return num(mayeval);
	}
	for (i = 0; i < (ssize_t)sizeof(int); i++) {
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
				c = num(mayeval);
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
num(int mayeval)
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
