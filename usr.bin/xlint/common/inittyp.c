/*	$NetBSD: inittyp.c,v 1.3 2002/01/30 06:55:02 thorpej Exp $	*/

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
 */

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(lint)
__RCSID("$NetBSD: inittyp.c,v 1.3 2002/01/30 06:55:02 thorpej Exp $");
#endif

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

#include "lint.h"

/* various type information */
ttab_t	ttab[NTSPEC];

void
inittyp(void)
{
	int	i;
	static const struct {
		tspec_t	it_tspec;
		ttab_t	it_ttab;
	} ittab[NTSPEC] = {
		{ SIGNED,   { 0, 0,
				      SIGNED, UNSIGN,
				      0, 0, 0, 0, 0, "signed" } },
		{ UNSIGN,   { 0, 0,
				      SIGNED, UNSIGN,
				      0, 0, 0, 0, 0, "unsigned" } },
		{ CHAR,	    { CHAR_SIZE, CHAR_BIT,
				      SCHAR, UCHAR,
				      1, 0, 0, 1, 1, "char" } },
		{ SCHAR,    { CHAR_SIZE, CHAR_BIT,
				      SCHAR, UCHAR,
				      1, 0, 0, 1, 1, "signed char" } },
		{ UCHAR,    { CHAR_SIZE, CHAR_BIT,
				      SCHAR, UCHAR,
				      1, 1, 0, 1, 1, "unsigned char" } },
		{ SHORT,    { SHORT_SIZE, 2 * CHAR_BIT,
				      SHORT, USHORT,
				      1, 0, 0, 1, 1, "short" } },
		{ USHORT,   { SHORT_SIZE, 2 * CHAR_BIT,
				      SHORT, USHORT,
				      1, 1, 0, 1, 1, "unsigned short" } },
		{ INT,      { INT_SIZE, 3 * CHAR_BIT,
				      INT, UINT,
				      1, 0, 0, 1, 1, "int" } },
		{ UINT,     { INT_SIZE, 3 * CHAR_BIT,
				      INT, UINT,
				      1, 1, 0, 1, 1, "unsigned int" } },
		{ LONG,     { LONG_SIZE, 4 * CHAR_BIT,
				      LONG, ULONG,
				      1, 0, 0, 1, 1, "long" } },
		{ ULONG,    { LONG_SIZE, 4 * CHAR_BIT,
				      LONG, ULONG,
				      1, 1, 0, 1, 1, "unsigned long" } },
		{ QUAD,     { QUAD_SIZE, 8 * CHAR_BIT,
				      QUAD, UQUAD,
				      1, 0, 0, 1, 1, "long long" } },
		{ UQUAD,    { QUAD_SIZE, 8 * CHAR_BIT,
				      QUAD, UQUAD,
				      1, 1, 0, 1, 1, "unsigned long long" } },
		{ FLOAT,    { FLOAT_SIZE, 4 * CHAR_BIT,
				      FLOAT, FLOAT,
				      0, 0, 1, 1, 1, "float" } },
		{ DOUBLE,   { DOUBLE_SIZE, 8 * CHAR_BIT,
				      DOUBLE, DOUBLE,
				      0, 0, 1, 1, 1, "double" } },
		{ LDOUBLE,  { LDOUBLE_SIZE, 10 * CHAR_BIT,
				      LDOUBLE, LDOUBLE,
				      0, 0, 1, 1, 1, "long double" } },
		{ VOID,     { -1, -1,
				      VOID, VOID,
				      0, 0, 0, 0, 0, "void" } },
		{ STRUCT,   { -1, -1,
				      STRUCT, STRUCT,
				      0, 0, 0, 0, 0, "struct" } },
		{ UNION,    { -1, -1,
				      UNION, UNION,
				      0, 0, 0, 0, 0, "union" } },
		{ ENUM,     { ENUM_SIZE, 3 * CHAR_BIT,
				      ENUM, ENUM,
				      1, 0, 0, 1, 1, "enum" } },
		{ PTR,      { PTR_SIZE, 4 * CHAR_BIT,
				      PTR, PTR,
				      0, 1, 0, 0, 1, "pointer" } },
		{ ARRAY,    { -1, -1,
				      ARRAY, ARRAY,
				      0, 0, 0, 0, 0, "array" } },
		{ FUNC,     { -1, -1,
				      FUNC, FUNC,
				      0, 0, 0, 0, 0, "function" } },
	};

	for (i = 0; i < sizeof (ittab) / sizeof (ittab[0]); i++)
		STRUCT_ASSIGN(ttab[ittab[i].it_tspec], ittab[i].it_ttab);
	if (!pflag) {
		for (i = 0; i < NTSPEC; i++)
			ttab[i].tt_psz = ttab[i].tt_sz;
	}
}
