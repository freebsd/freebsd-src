/*-
 * Copyright (c) 1983, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	$FreeBSD$
 */

#ifndef lint
static char sccsid[] = "@(#)alloc.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"

char   *memtop = NULL;		/* PWP: top of current memory */
char   *membot = NULL;		/* PWP: bottom of allocatable memory */

ptr_t
Malloc(n)
    size_t  n;
{
    ptr_t   ptr;

    if (membot == NULL)
	memtop = membot = sbrk(0);
    if ((ptr = malloc(n)) == (ptr_t) 0) {
	child++;
	stderror(ERR_NOMEM);
    }
    return (ptr);
}

ptr_t
Realloc(p, n)
    ptr_t   p;
    size_t  n;
{
    ptr_t   ptr;

    if (membot == NULL)
	memtop = membot = sbrk(0);
    if ((ptr = realloc(p, n)) == (ptr_t) 0) {
	child++;
	stderror(ERR_NOMEM);
    }
    return (ptr);
}

ptr_t
Calloc(s, n)
    size_t  s, n;
{
    ptr_t   ptr;

    if (membot == NULL)
	memtop = membot = sbrk(0);
    if ((ptr = calloc(s, n)) == (ptr_t) 0) {
	child++;
	stderror(ERR_NOMEM);
    }

    return (ptr);
}

void
Free(p)
    ptr_t   p;
{
    if (p)
	free(p);
}

/*
 * mstats - print out statistics about malloc
 *
 * Prints two lines of numbers, one showing the length of the free list
 * for each size category, the second showing the number of mallocs -
 * frees for each size category.
 */
void
/*ARGSUSED*/
showall(v, t)
    Char **v;
    struct command *t;
{
    memtop = (char *) sbrk(0);
    (void) fprintf(cshout, "Allocated memory from 0x%lx to 0x%lx (%ld).\n",
	    (unsigned long) membot, (unsigned long) memtop,
                (unsigned long) (memtop - membot));
}
