/*	$NetBSD: err.c,v 1.8 1995/10/02 17:37:00 jpo Exp $	*/

/*
 * Copyright (c) 1994, 1995 Jochen Pohl
 * All Rights Reserved.
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
 *      This product includes software developed by Jochen Pohl for
 *	The NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef lint
static char rcsid[] = "$NetBSD: err.c,v 1.8 1995/10/02 17:37:00 jpo Exp $";
#endif

/* number of errors found */
int	nerr;

/* number of syntax errors */
int	sytxerr;

#include <stdlib.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "lint1.h"

static	const	char *basename __P((const char *));
static	void	verror __P((int, va_list));
static	void	vwarning __P((int, va_list));


const	char *msgs[] = {
	"syntax error: empty declaration",			      /* 0 */
	"old style declaration; add int",			      /* 1 */
	"empty declaration",					      /* 2 */
	"%s declared in argument declaration list",		      /* 3 */
	"illegal type combination",				      /* 4 */
	"modifying typedef with '%s'; only qualifiers allowed",	      /* 5 */
	"use 'double' instead of 'long float'",			      /* 6 */
	"only one storage class allowed",			      /* 7 */
	"illegal storage class",				      /* 8 */
	"only register valid as formal parameter storage class",      /* 9 */
	"duplicate '%s'",					      /* 10 */
	"bit-field initializer out of range",			      /* 11 */
	"compiler takes size of function",			      /* 12 */
	"incomplete enum type: %s",				      /* 13 */
	"compiler takes alignment of function",			      /* 14 */
	"function returns illegal type",			      /* 15 */
	"array of function is illegal",				      /* 16 */
	"null dimension",					      /* 17 */
	"illegal use of 'void'",				      /* 18 */
	"void type for %s",					      /* 19 */
	"zero or negative array dimension",			      /* 20 */
	"redeclaration of formal parameter %s",			      /* 21 */
	"incomplete or misplaced function definition",		      /* 22 */
	"undefined label %s",					      /* 23 */
	"cannot initialize function: %s",			      /* 24 */
	"cannot initialize typedef: %s",			      /* 25 */
	"cannot initialize extern declaration: %s",		      /* 26 */
	"redeclaration of %s",					      /* 27 */
	"redefinition of %s",					      /* 28 */
	"previously declared extern, becomes static: %s",	      /* 29 */
	"redeclaration of %s; ANSI C requires static",		      /* 30 */
	"incomplete structure or union %s: %s",			      /* 31 */
	"argument type defaults to 'int': %s",			      /* 32 */
	"duplicate member name: %s",				      /* 33 */
	"nonportable bit-field type",				      /* 34 */
	"illegal bit-field type",				      /* 35 */
	"illegal bit-field size",				      /* 36 */
	"zero size bit-field",					      /* 37 */
	"function illegal in structure or union",		      /* 38 */
	"illegal zero sized structure member: %s",		      /* 39 */
	"unknown size: %s",					      /* 40 */
	"illegal use of bit-field",				      /* 41 */
	"forward reference to enum type",			      /* 42 */
	"redefinition hides earlier one: %s",			      /* 43 */
	"declaration introduces new type in ANSI C: %s %s",	      /* 44 */
	"base type is really '%s %s'",				      /* 45 */
	"(%s) tag redeclared",					      /* 46 */
	"zero sized %s",					      /* 47 */
	"overflow in enumeration values: %s",			      /* 48 */
	"struct or union member must be named",			      /* 49 */
	"a function is declared as an argument: %s",		      /* 50 */
	"parameter mismatch: %d declared, %d defined",		      /* 51 */
	"cannot initialize parameter: %s",			      /* 52 */
	"declared argument %s is missing",			      /* 53 */
	"trailing ',' prohibited in enum declaration",		      /* 54 */
	"integral constant expression expected",		      /* 55 */
	"integral constant too large",				      /* 56 */
	"enumeration constant hides parameter: %s",		      /* 57 */
	"type does not match prototype: %s",			      /* 58 */
	"formal parameter lacks name: param #%d",		      /* 59 */
	"void must be sole parameter",				      /* 60 */
	"void parameter cannot have name: %s",			      /* 61 */
	"function prototype parameters must have types",	      /* 62 */
	"prototype does not match old-style definition",	      /* 63 */
	"()-less function definition",				      /* 64 */
	"%s has no named members",				      /* 65 */
	"syntax requires ';' after last struct/union member",	      /* 66 */
	"cannot return incomplete type",			      /* 67 */
	"typedef already qualified with '%s'",			      /* 68 */
	"inappropriate qualifiers with 'void'",			      /* 69 */
	"%soperand of '%s' is unsigned in ANSI C",		      /* 70 */
	"too many characters in character constant",		      /* 71 */
	"typedef declares no type name",			      /* 72 */
	"empty character constant",				      /* 73 */
	"no hex digits follow \\x",				      /* 74 */
	"overflow in hex escape",				      /* 75 */
	"character escape does not fit in character",		      /* 76 */
	"bad octal digit %c",					      /* 77 */
	"nonportable character escape",				      /* 78 */
	"dubious escape \\%c",					      /* 79 */
	"dubious escape \\%o",					      /* 80 */
	"\\a undefined in traditional C",			      /* 81 */
	"\\x undefined in traditional C",			      /* 82 */
	"storage class after type is obsolescent",		      /* 83 */
	"ANSI C requires formal parameter before '...'",	      /* 84 */
	"dubious tag declaration: %s %s",			      /* 85 */
	"automatic hides external declaration: %s",		      /* 86 */
	"static hides external declaration: %s",		      /* 87 */
	"typedef hides external declaration: %s",		      /* 88 */
	"typedef redeclared: %s",				      /* 89 */
	"inconsistent redeclaration of extern: %s",		      /* 90 */
	"declaration hides parameter: %s",			      /* 91 */
	"inconsistent redeclaration of static: %s",		      /* 92 */
	"dubious static function at block level: %s",		      /* 93 */
	"function has illegal storage class: %s",		      /* 94 */
	"declaration hides earlier one: %s",			      /* 95 */
	"cannot dereference non-pointer type",			      /* 96 */
	"suffix U is illegal in traditional C",			      /* 97 */
	"suffixes F and L are illegal in traditional C",	      /* 98 */
	"%s undefined",						      /* 99 */
	"unary + is illegal in traditional C",			      /* 100 */
	"undefined struct/union member: %s",			      /* 101 */
	"illegal member use: %s",				      /* 102 */
	"left operand of '.' must be struct/union object",	      /* 103 */
	"left operand of '->' must be pointer to struct/union",	      /* 104 */
	"non-unique member requires struct/union %s",		      /* 105 */
	"left operand of '->' must be pointer",			      /* 106 */
	"operands of '%s' have incompatible types",		      /* 107 */
	"operand of '%s' has incompatible type",		      /* 108 */
	"void type illegal in expression",			      /* 109 */
	"pointer to function is not allowed here",		      /* 110 */
	"unacceptable operand of '%s'",				      /* 111 */
	"cannot take address of bit-field",			      /* 112 */
	"cannot take address of register %s",			      /* 113 */
	"%soperand of '%s' must be lvalue",			      /* 114 */
	"%soperand of '%s' must be modifiable lvalue",		      /* 115 */
	"illegal pointer subtraction",				      /* 116 */
	"bitwise operation on signed value possibly nonportable",     /* 117 */
	"semantics of '%s' change in ANSI C; use explicit cast",      /* 118 */
	"conversion of '%s' to '%s' is out of range",		      /* 119 */
	"bitwise operation on signed value nonportable",	      /* 120 */
	"negative shift",					      /* 121 */
	"shift greater than size of object",			      /* 122 */
	"illegal combination of pointer and integer, op %s",	      /* 123 */
	"illegal pointer combination, op %s",			      /* 124 */
	"ANSI C forbids ordered comparisons of pointers to functions",/* 125 */
	"incompatible types in conditional",			      /* 126 */
	"'&' before array or function: ignored",		      /* 127 */
	"operands have incompatible pointer types, op %s",	      /* 128 */
	"expression has null effect",				      /* 129 */
	"enum type mismatch, op %s",				      /* 130 */
	"conversion to '%s' may sign-extend incorrectly",	      /* 131 */
	"conversion from '%s' may lose accuracy",		      /* 132 */
	"conversion of pointer to '%s' loses bits",		      /* 133 */
	"conversion of pointer to '%s' may lose bits",		      /* 134 */
	"possible pointer alignment problem",			      /* 135 */
	"cannot do pointer arithmetic on operand of unknown size",    /* 136 */
	"use of incomplete enum type, op %s",			      /* 137 */
	"unknown operand size, op %s",				      /* 138 */
	"division by 0",					      /* 139 */
	"modulus by 0",						      /* 140 */
	"integer overflow detected, op %s",			      /* 141 */
	"floating point overflow detected, op %s",		      /* 142 */
	"cannot take size of incomplete type",			      /* 143 */
	"cannot take size of function",				      /* 144 */
	"cannot take size of bit-field",			      /* 145 */
	"cannot take size of void",				      /* 146 */
	"invalid cast expression",				      /* 147 */
	"improper cast of void expression",			      /* 148 */
	"illegal function",					      /* 149 */
	"argument mismatch: %d arg%s passed, %d expected",	      /* 150 */
	"void expressions may not be arguments, arg #%d",	      /* 151 */
	"argument cannot have unknown size, arg #%d",		      /* 152 */
	"argument has incompatible pointer type, arg #%d",	      /* 153 */
	"illegal combination of pointer and integer, arg #%d",	      /* 154 */
	"argument is incompatible with prototype, arg #%d",	      /* 155 */
	"enum type mismatch, arg #%d",			       	      /* 156 */
	"ANSI C treats constant as unsigned",			      /* 157 */
	"%s may be used before set",			      	      /* 158 */
	"assignment in conditional context",			      /* 159 */
	"operator '==' found where '=' was expected",		      /* 160 */
	"constant in conditional context",			      /* 161 */
	"comparison of %s with %s, op %s",			      /* 162 */
	"a cast does not yield an lvalue",			      /* 163 */
	"assignment of negative constant to unsigned type",	      /* 164 */
	"constant truncated by assignment",			      /* 165 */
	"precision lost in bit-field assignment",		      /* 166 */
	"array subscript cannot be negative: %ld",		      /* 167 */
	"array subscript cannot be > %d: %ld",			      /* 168 */
	"precedence confusion possible: parenthesize!",		      /* 169 */
	"first operand must have scalar type, op ? :",		      /* 170 */
	"assignment type mismatch",				      /* 171 */
	"too many struct/union initializers",			      /* 172 */
	"too many array initializers",				      /* 173 */
	"too many initializers",				      /* 174 */
	"initialisation of an incomplete type",			      /* 175 */
	"invalid initializer",					      /* 176 */
	"non-constant initializer",				      /* 177 */
	"initializer does not fit",				      /* 178 */
	"cannot initialize struct/union with no named member",	      /* 179 */
	"bit-field initializer does not fit",			      /* 180 */
	"{}-enclosed initializer required",			      /* 181 */
	"incompatible pointer types",				      /* 182 */
	"illegal combination of pointer and integer",		      /* 183 */
	"illegal pointer combination",				      /* 184 */
	"initialisation type mismatch",				      /* 185 */
	"bit-field initialisation is illegal in traditional C",	      /* 186 */
	"non-null byte ignored in string initializer",		      /* 187 */
	"no automatic aggregate initialization in traditional C",     /* 188 */
	"assignment of struct/union illegal in traditional C",	      /* 189 */
	"empty array declaration: %s",				      /* 190 */
	"%s set but not used in function %s",		      	      /* 191 */
	"%s unused in function %s",				      /* 192 */
	"statement not reached",				      /* 193 */
	"label %s redefined",					      /* 194 */
	"case not in switch",					      /* 195 */
	"case label affected by conversion",			      /* 196 */
	"non-constant case expression",				      /* 197 */
	"non-integral case expression",				      /* 198 */
	"duplicate case in switch: %ld",			      /* 199 */
	"duplicate case in switch: %lu",			      /* 200 */
	"default outside switch",				      /* 201 */
	"duplicate default in switch",				      /* 202 */
	"case label must be of type `int' in traditional C",	      /* 203 */
	"controlling expressions must have scalar type",	      /* 204 */
	"switch expression must have integral type",		      /* 205 */
	"enumeration value(s) not handled in switch",		      /* 206 */
	"loop not entered at top",				      /* 207 */
	"break outside loop or switch",				      /* 208 */
	"continue outside loop",				      /* 209 */
	"enum type mismatch in initialisation",			      /* 210 */
	"return value type mismatch",				      /* 211 */
	"cannot return incomplete type",			      /* 212 */
	"void function %s cannot return value",			      /* 213 */
	"function %s expects to return value",			      /* 214 */
	"function implicitly declared to return int",		      /* 215 */
	"function %s has return (e); and return;",		      /* 216 */
	"function %s falls off bottom without returning value",	      /* 217 */
	"ANSI C treats constant as unsigned, op %s",		      /* 218 */
	"concatenated strings are illegal in traditional C",	      /* 219 */
	"fallthrough on case statement",			      /* 220 */
	"initialisation of unsigned with negative constant",	      /* 221 */
	"conversion of negative constant to unsigned type",	      /* 222 */
	"end-of-loop code not reached",				      /* 223 */
	"cannot recover from previous errors",			      /* 224 */
	"static function called but not defined: %s()",		      /* 225 */
	"static variable %s unused",				      /* 226 */
	"const object %s should have initializer",		      /* 227 */
	"function cannot return const or volatile object",	      /* 228 */
	"questionable conversion of function pointer",		      /* 229 */
	"nonportable character comparison, op %s",		      /* 230 */
	"argument %s unused in function %s",			      /* 231 */
	"label %s unused in function %s",			      /* 232 */
	"struct %s never defined",				      /* 233 */
	"union %s never defined",				      /* 234 */
	"enum %s never defined",				      /* 235 */
	"static function %s unused",				      /* 236 */
	"redeclaration of formal parameter %s",			      /* 237 */
	"initialisation of union is illegal in traditional C",	      /* 238 */
	"constant argument to NOT",				      /* 239 */
	"assignment of different structures",			      /* 240 */
	"dubious operation on enum, op %s",			      /* 241 */
	"combination of '%s' and '%s', op %s",			      /* 242 */
	"dubious comparison of enums, op %s",			      /* 243 */
	"illegal structure pointer combination",		      /* 244 */
	"illegal structure pointer combination, op %s",		      /* 245 */
	"dubious conversion of enum to '%s'",			      /* 246 */
	"pointer casts may be troublesome",			      /* 247 */
	"floating-point constant out of range",			      /* 248 */
	"syntax error",						      /* 249 */
	"unknown character \\%o",				      /* 250 */
	"malformed integer constant",				      /* 251 */
	"integer constant out of range",			      /* 252 */
	"unterminated character constant",			      /* 253 */
	"newline in string or char constant",			      /* 254 */
	"undefined or invalid # directive",			      /* 255 */
	"unterminated comment",					      /* 256 */
	"extra characters in lint comment",			      /* 257 */
	"unterminated string constant",				      /* 258 */
	"conversion to '%s' due to prototype, arg #%d",		      /* 259 */
	"previous declaration of %s",				      /* 260 */
	"previous definition of %s",				      /* 261 */
	"\\\" inside character constants undefined in traditional C", /* 262 */
	"\\? undefined in traditional C",			      /* 263 */
	"\\v undefined in traditional C",			      /* 264 */
	"%s C does not support 'long long'",			      /* 265 */
	"'long double' is illegal in traditional C",		      /* 266 */
	"shift equal to size of object",			      /* 267 */
	"variable declared inline: %s",				      /* 268 */
	"argument declared inline: %s",				      /* 269 */
	"function prototypes are illegal in traditional C",	      /* 270 */
	"switch expression must be of type `int' in traditional C",   /* 271 */
	"empty translation unit",				      /* 272 */
	"bit-field type '%s' invalid in ANSI C",		      /* 273 */
	"ANSI C forbids comparison of %s with %s",		      /* 274 */
	"cast discards 'const' from pointer target type",	      /* 275 */
	"",							      /* 276 */
	"initialisation of '%s' with '%s'",			      /* 277 */
	"combination of '%s' and '%s', arg #%d",		      /* 278 */
	"combination of '%s' and '%s' in return",		      /* 279 */
	"must be outside function: /* %s */",			      /* 280 */
	"duplicate use of /* %s */",				      /* 281 */
	"must precede function definition: /* %s */",		      /* 282 */
	"argument number mismatch with directive: /* %s */",	      /* 283 */
	"fallthrough on default statement",			      /* 284 */
	"prototype declaration",				      /* 285 */
	"function definition is not a prototype",		      /* 286 */
	"function declaration is not a prototype",		      /* 287 */
	"dubious use of /* VARARGS */ with /* %s */",		      /* 288 */
	"can't be used together: /* PRINTFLIKE */ /* SCANFLIKE */",   /* 289 */
	"static function %s declared but not defined",		      /* 290 */
	"invalid multibyte character",				      /* 291 */
	"cannot concatenate wide and regular string literals",	      /* 292 */
	"argument %d must be 'char *' for PRINTFLIKE/SCANFLIKE",      /* 293 */
	"multi-character character constant",			      /* 294 */
	"conversion of '%s' to '%s' is out of range, arg #%d",	      /* 295 */
	"conversion of negative constant to unsigned type, arg #%d",  /* 296 */
	"conversion to '%s' may sign-extend incorrectly, arg #%d",    /* 297 */
	"conversion from '%s' may lose accuracy, arg #%d",	      /* 298 */
	"prototype does not match old style definition, arg #%d",     /* 299 */
	"old style definition",					      /* 300 */
	"array of incomplete type",				      /* 301 */
	"%s returns pointer to automatic object",		      /* 302 */
	"ANSI C forbids conversion of %s to %s",		      /* 303 */
	"ANSI C forbids conversion of %s to %s, arg #%d",	      /* 304 */
	"ANSI C forbids conversion of %s to %s, op %s",		      /* 305 */
	"constant truncated by conversion, op %s",		      /* 306 */
	"static variable %s set but not used",			      /* 307 */
	"",							      /* 308 */
	"extra bits set to 0 in conversion of '%s' to '%s', op %s",   /* 309 */
};

/*
 * If Fflag is not set basename() returns a pointer to the last
 * component of the path, otherwise it returns the argument.
 */
static const char *
basename(path)
	const	char *path;
{
	const	char *cp, *cp1, *cp2;

	if (Fflag)
		return (path);

	cp = cp1 = cp2 = path;
	while (*cp != '\0') {
		if (*cp++ == '/') {
			cp2 = cp1;
			cp1 = cp;
		}
	}
	return (*cp1 == '\0' ? cp2 : cp1);
}

static void
verror(n, ap)
	int	n;
	va_list	ap;
{
	const	char *fn;

	fn = basename(curr_pos.p_file);
	(void)printf("%s:%d: ", fn, curr_pos.p_line);
	(void)vprintf(msgs[n], ap);
	(void)printf("\n");
	nerr++;
}

static void
vwarning(n, ap)
	int	n;
	va_list	ap;
{
	const	char *fn;

	if (nowarn)
		/* this warning is suppressed by a LINTED comment */
		return;

	fn = basename(curr_pos.p_file);
	(void)printf("%s:%d: warning: ", fn, curr_pos.p_line);
	(void)vprintf(msgs[n], ap);
	(void)printf("\n");
}

void
#ifdef __STDC__
error(int n, ...)
#else
error(n, va_alist)
	int	n;
	va_dcl
#endif
{
	va_list	ap;

#ifdef __STDC__
	va_start(ap, n);
#else
	va_start(ap);
#endif
	verror(n, ap);
	va_end(ap);
}

void
#ifdef __STDC__
lerror(const char *msg, ...)
#else
lerror(msg, va_alist)
	const	char *msg;
	va_dcl
#endif
{
	va_list	ap;
	const	char *fn;

#ifdef __STDC__
	va_start(ap, msg);
#else
	va_start(ap);
#endif
	fn = basename(curr_pos.p_file);
	(void)fprintf(stderr, "%s:%d: lint error: ", fn, curr_pos.p_line);
	(void)vfprintf(stderr, msg, ap);
	(void)fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

void
#ifdef __STDC__
warning(int n, ...)
#else
warning(n, va_alist)
	int	n;
	va_dcl
#endif
{
	va_list	ap;

#ifdef __STDC__
	va_start(ap, n);
#else
	va_start(ap);
#endif
	vwarning(n, ap);
	va_end(ap);
}

void
#ifdef __STDC__
message(int n, ...)
#else
message(n, va_alist)
	int	n;
	va_dcl
#endif
{
	va_list	ap;
	const	char *fn;

#ifdef __STDC__
	va_start(ap, n);
#else
	va_start(ap);
#endif
	fn = basename(curr_pos.p_file);
	(void)printf("%s:%d: ", fn, curr_pos.p_line);
	(void)vprintf(msgs[n], ap);
	(void)printf("\n");
	va_end(ap);
}

int
#ifdef __STDC__
gnuism(int n, ...)
#else
gnuism(n, va_alist)
	int	n;
	va_dcl
#endif
{
	va_list	ap;
	int	msg;

#ifdef __STDC__
	va_start(ap, n);
#else
	va_start(ap);
#endif
	if (sflag && !gflag) {
		verror(n, ap);
		msg = 1;
	} else if (!sflag && gflag) {
		msg = 0;
	} else {
		vwarning(n, ap);
		msg = 1;
	}
	va_end(ap);

	return (msg);
}
