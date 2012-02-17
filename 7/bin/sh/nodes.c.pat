/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)nodes.c.pat	8.2 (Berkeley) 5/4/95
 * $FreeBSD$
 */

#include <sys/param.h>
#include <stdlib.h>
#include <stddef.h>
/*
 * Routine for dealing with parsed shell commands.
 */

#include "shell.h"
#include "nodes.h"
#include "memalloc.h"
#include "mystring.h"


STATIC int     funcblocksize;	/* size of structures in function */
STATIC int     funcstringsize;	/* size of strings in node */
STATIC pointer funcblock;	/* block to allocate function from */
STATIC char   *funcstring;	/* block to allocate strings from */

%SIZES


STATIC void calcsize(union node *);
STATIC void sizenodelist(struct nodelist *);
STATIC union node *copynode(union node *);
STATIC struct nodelist *copynodelist(struct nodelist *);
STATIC char *nodesavestr(char *);


struct funcdef {
	unsigned int refcount;
	union node n;
};

/*
 * Make a copy of a parse tree.
 */

struct funcdef *
copyfunc(union node *n)
{
	struct funcdef *fn;

	if (n == NULL)
		return NULL;
	funcblocksize = offsetof(struct funcdef, n);
	funcstringsize = 0;
	calcsize(n);
	fn = ckmalloc(funcblocksize + funcstringsize);
	fn->refcount = 1;
	funcblock = (char *)fn + offsetof(struct funcdef, n);
	funcstring = (char *)fn + funcblocksize;
	copynode(n);
	return fn;
}


union node *
getfuncnode(struct funcdef *fn)
{
	return fn == NULL ? NULL : &fn->n;
}


STATIC void
calcsize(union node *n)
{
	%CALCSIZE
}



STATIC void
sizenodelist(struct nodelist *lp)
{
	while (lp) {
		funcblocksize += ALIGN(sizeof(struct nodelist));
		calcsize(lp->n);
		lp = lp->next;
	}
}



STATIC union node *
copynode(union node *n)
{
	union node *new;

	%COPY
	return new;
}


STATIC struct nodelist *
copynodelist(struct nodelist *lp)
{
	struct nodelist *start;
	struct nodelist **lpp;

	lpp = &start;
	while (lp) {
		*lpp = funcblock;
		funcblock = (char *)funcblock + ALIGN(sizeof(struct nodelist));
		(*lpp)->n = copynode(lp->n);
		lp = lp->next;
		lpp = &(*lpp)->next;
	}
	*lpp = NULL;
	return start;
}



STATIC char *
nodesavestr(char *s)
{
	char *p = s;
	char *q = funcstring;
	char   *rtn = funcstring;

	while ((*q++ = *p++) != '\0')
		continue;
	funcstring = q;
	return rtn;
}


void
reffunc(struct funcdef *fn)
{
	if (fn)
		fn->refcount++;
}


/*
 * Decrement the reference count of a function definition, freeing it
 * if it falls to 0.
 */

void
unreffunc(struct funcdef *fn)
{
	if (fn) {
		fn->refcount--;
		if (fn->refcount > 0)
			return;
		ckfree(fn);
	}
}
