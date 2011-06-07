/*	$NetBSD: hash.c,v 1.7 2002/01/21 19:49:52 tv Exp $	*/

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
__RCSID("$NetBSD: hash.c,v 1.7 2002/01/21 19:49:52 tv Exp $");
#endif
__FBSDID("$FreeBSD$");

/*
 * XXX Really need a generalized hash table package
 */

#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "lint2.h"

/* pointer to hash table, initialized in inithash() */
static	hte_t	**htab;

static	int	hash(const char *);

/*
 * Initialize hash table.
 */
void
_inithash(hte_t ***tablep)
{

	if (tablep == NULL)
		tablep = &htab;

	*tablep = xcalloc(HSHSIZ2, sizeof (hte_t *));
}

/*
 * Compute hash value from a string.
 */
static int
hash(const char *s)
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
_hsearch(hte_t **table, const char *s, int mknew)
{
	int	h;
	hte_t	*hte;

	if (table == NULL)
		table = htab;

	h = hash(s);
	for (hte = table[h]; hte != NULL; hte = hte->h_link) {
		if (strcmp(hte->h_name, s) == 0)
			break;
	}

	if (hte != NULL || !mknew)
		return (hte);

	/* create a new hte */
	hte = xmalloc(sizeof (hte_t));
	hte->h_name = xstrdup(s);
	hte->h_used = 0;
	hte->h_def = 0;
	hte->h_static = 0;
	hte->h_syms = NULL;
	hte->h_lsym = &hte->h_syms;
	hte->h_calls = NULL;
	hte->h_lcall = &hte->h_calls;
	hte->h_usyms = NULL;
	hte->h_lusym = &hte->h_usyms;
	hte->h_link = table[h];
	hte->h_hte = NULL;
	table[h] = hte;

	return (hte);
}

/*
 * Call function f for each name in the hash table.
 */
void
_forall(hte_t **table, void (*f)(hte_t *))
{
	int	i;
	hte_t	*hte;

	if (table == NULL)
		table = htab;

	for (i = 0; i < HSHSIZ2; i++) {
		for (hte = table[i]; hte != NULL; hte = hte->h_link)
			(*f)(hte);
	}
}

/*
 * Free all contents of the hash table that this module allocated.
 */
void
_destroyhash(hte_t **table)
{
	int	i;
	hte_t	*hte, *nexthte;

	if (table == NULL)
		err(1, "_destroyhash called on main hash table");

	for (i = 0; i < HSHSIZ2; i++) {
		for (hte = table[i]; hte != NULL; hte = nexthte) {
			free((void *)hte->h_name);
			nexthte = hte->h_link;
			free(hte);
		}
	}
	free(table);
}
