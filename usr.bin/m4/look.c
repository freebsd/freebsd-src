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
static char sccsid[] = "@(#)look.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * look.c
 * Facility: m4 macro processor
 * by: oz
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"

int
hash(name)
register char *name;
{
	register unsigned long h = 0;
	while (*name)
		h = (h << 5) + h + *name++;
	return (h % HASHSIZE);
}

/*
 * find name in the hash table
 */
ndptr 
lookup(name)
char *name;
{
	register ndptr p;

	for (p = hashtab[hash(name)]; p != nil; p = p->nxtptr)
		if (STREQ(name, p->name))
			break;
	return (p);
}

/*
 * hash and create an entry in the hash table.
 * The new entry is added in front of a hash bucket.
 */
ndptr 
addent(name)
char *name;
{
	register int h;
	ndptr p;

	h = hash(name);
	p = (ndptr) xalloc(sizeof(struct ndblock));
	p->nxtptr = hashtab[h];
	hashtab[h] = p;
	p->name = xstrdup(name);
	return p;
}

static void
freent(p)
ndptr p;
{
	if (!(p->type & STATIC)) {
		free((char *) p->name);
		if (p->defn != null)
			free((char *) p->defn);
	}
	free((char *) p);
}

/*
 * remove an entry from the hashtable
 */
void
remhash(name, all)
char *name;
int all;
{
	register int h;
	register ndptr xp, tp, mp;

	h = hash(name);
	mp = hashtab[h];
	tp = nil;
	while (mp != nil) {
		if (STREQ(mp->name, name)) {
			mp = mp->nxtptr;
			if (tp == nil) {
				freent(hashtab[h]);
				hashtab[h] = mp;
			}
			else {
				xp = tp->nxtptr;
				tp->nxtptr = mp;
				freent(xp);
			}
			if (!all)
				break;
		}
		else {
			tp = mp;
			mp = mp->nxtptr;
		}
	}
}
