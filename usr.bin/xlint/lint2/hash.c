/*	$NetBSD: hash.c,v 1.2 1995/07/03 21:24:47 cgd Exp $	*/

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

#ifndef lint
static char rcsid[] = "$NetBSD: hash.c,v 1.2 1995/07/03 21:24:47 cgd Exp $";
#endif

#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "lint2.h"

/* pointer to hash table, initialized in inithash() */
static	hte_t	**htab;

static	int	hash __P((const char *));

/*
 * Initialize hash table.
 */
void
inithash()
{
	htab = xcalloc(HSHSIZ2, sizeof (hte_t *));
}

/*
 * Compute hash value from a string.
 */
static int
hash(s)
	const	char *s;
{
	u_int	v;
	const	u_char *us;

	v = 0;
	for (us = (const u_char *)s; *us != '\0'; us++) {
		v = (v << sizeof (v)) + *us;
		v ^= v >> (sizeof (v) * CHAR_BIT - sizeof (v));
	}
	return (v % HSHSIZ2);
}

/*
 * Look for a hash table entry. If no hash table entry for the
 * given name exists and mknew is set, create a new one.
 */
hte_t *
hsearch(s, mknew)
	const	char *s;
	int	mknew;
{
	int	h;
	hte_t	*hte;

	h = hash(s);
	for (hte = htab[h]; hte != NULL; hte = hte->h_link) {
		if (strcmp(hte->h_name, s) == 0)
			break;
	}

	if (hte != NULL || !mknew)
		return (hte);

	/* create a new hte */
	hte = xalloc(sizeof (hte_t));
	hte->h_name = xstrdup(s);
	hte->h_lsym = &hte->h_syms;
	hte->h_lcall = &hte->h_calls;
	hte->h_lusym = &hte->h_usyms;
	hte->h_link = htab[h];
	htab[h] = hte;

	return (hte);
}

/*
 * Call function f for each name in the hash table.
 */
void
forall(f)
	void	(*f) __P((hte_t *));
{
	int	i;
	hte_t	*hte;

	for (i = 0; i < HSHSIZ2; i++) {
		for (hte = htab[i]; hte != NULL; hte = hte->h_link)
			(*f)(hte);
	}
}
