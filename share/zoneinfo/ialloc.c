/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Arthur David Olson of the National Cancer Institute.
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
static char sccsid[] = "@(#)ialloc.c	5.3 (Berkeley) 4/20/91";
#endif /* not lint */

#ifdef notdef
static char	elsieid[] = "@(#)ialloc.c	8.18";
#endif

/*LINTLIBRARY*/

#include <string.h>
#include <stdlib.h>

#ifdef MAL
#define NULLMAL(x)	((x) == NULL || (x) == MAL)
#else /* !defined MAL */
#define NULLMAL(x)	((x) == NULL)
#endif /* !defined MAL */

#define nonzero(n)	(((n) == 0) ? 1 : (n))

char *	icalloc __P((int nelem, int elsize));
char *	icatalloc __P((char * old, const char * new));
char *	icpyalloc __P((const char * string));
char *	imalloc __P((int n));
char *	irealloc __P((char * pointer, int size));
void	ifree __P((char * pointer));

char *
imalloc(n)
const int	n;
{
#ifdef MAL
	register char *	result;

	result = malloc((size_t) nonzero(n));
	return NULLMAL(result) ? NULL : result;
#else /* !defined MAL */
	return malloc((size_t) nonzero(n));
#endif /* !defined MAL */
}

char *
icalloc(nelem, elsize)
int	nelem;
int	elsize;
{
	if (nelem == 0 || elsize == 0)
		nelem = elsize = 1;
	return calloc((size_t) nelem, (size_t) elsize);
}

char *
irealloc(pointer, size)
char * const	pointer;
const int	size;
{
	if (NULLMAL(pointer))
		return imalloc(size);
	return realloc((void *) pointer, (size_t) nonzero(size));
}

char *
icatalloc(old, new)
char * const		old;
const char * const	new;
{
	register char *	result;
	register	oldsize, newsize;

	newsize = NULLMAL(new) ? 0 : strlen(new);
	if (NULLMAL(old))
		oldsize = 0;
	else if (newsize == 0)
		return old;
	else	oldsize = strlen(old);
	if ((result = irealloc(old, oldsize + newsize + 1)) != NULL)
		if (!NULLMAL(new))
			(void) strcpy(result + oldsize, new);
	return result;
}

char *
icpyalloc(string)
const char * const	string;
{
	return icatalloc((char *) NULL, string);
}

void
ifree(p)
char * const	p;
{
	if (!NULLMAL(p))
		(void) free(p);
}

void
icfree(p)
char * const	p;
{
	if (!NULLMAL(p))
		(void) free(p);
}
