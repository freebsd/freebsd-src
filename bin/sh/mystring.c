/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
/*static char sccsid[] = "from: @(#)mystring.c	5.1 (Berkeley) 3/7/91";*/
static char rcsid[] = "mystring.c,v 1.4 1993/08/01 18:58:05 mycroft Exp";
#endif /* not lint */

/*
 * String functions.
 *
 *	equal(s1, s2)		Return true if strings are equal.
 *	scopy(from, to)		Copy a string.
 *	scopyn(from, to, n)	Like scopy, but checks for overflow.
 *	strchr(s, c)		Find first occurance of c in s.
 *	bcopy(from, to, n)	Copy a block of memory.
 *	number(s)		Convert a string of digits to an integer.
 *	is_number(s)		Return true if s is a string of digits.
 */

#include "shell.h"
#include "syntax.h"
#include "error.h"
#include "mystring.h"


char nullstr[1];		/* zero length string */


/*
 * scopyn - copy a string from "from" to "to", truncating the string
 *		if necessary.  "To" is always nul terminated, even if
 *		truncation is performed.  "Size" is the size of "to".
 */

void
scopyn(from, to, size)
	register char const *from;
	register char *to;
	register int size;
	{

	while (--size > 0) {
		if ((*to++ = *from++) == '\0')
			return;
	}
	*to = '\0';
}


/*
 * strchr - find first occurrence of a character in a string.
 */

#ifndef SYS5
char *
mystrchr(s, charwanted)
	char const *s;
	register char charwanted;
	{
	register char const *scan;

	/*
	 * The odd placement of the two tests is so NUL is findable.
	 */
	for (scan = s ; *scan != charwanted ; )	/* ++ moved down for opt. */
		if (*scan++ == '\0')
			return NULL;
	return (char *)scan;
}
#endif



/*
 * bcopy - copy bytes
 *
 * This routine was derived from code by Henry Spencer.
 */

void
mybcopy(src, dst, length)
	pointer dst;
	const pointer src;
	register int length;
	{
	register char *d = dst;
	register char *s = src;

	while (--length >= 0)
		*d++ = *s++;
}


/*
 * prefix -- see if pfx is a prefix of string.
 */

int
prefix(pfx, string)
	register char const *pfx;
	register char const *string;
	{
	while (*pfx) {
		if (*pfx++ != *string++)
			return 0;
	}
	return 1;
}


/*
 * Convert a string of digits to an integer, printing an error message on
 * failure.
 */

int
number(s)
	const char *s;
	{

	if (! is_number(s))
		error2("Illegal number", (char *)s);
	return atoi(s);
}



/*
 * Check for a valid number.  This should be elsewhere.
 */

int
is_number(p)
	register const char *p;
	{
	do {
		if (! is_digit(*p))
			return 0;
	} while (*++p != '\0');
	return 1;
}
