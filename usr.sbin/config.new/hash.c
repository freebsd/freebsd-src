/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratories.
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
 *	@(#)hash.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/*
 * Interned strings are kept in a hash table.  By making each string
 * unique, the program can compare strings by comparing pointers.
 */
struct hashent {
	struct	hashent *h_next;	/* hash buckets are chained */
	const char *h_name;		/* the string */
	u_int	h_hash;			/* its hash value */
	void	*h_value;		/* other values (for name=value) */
};
struct hashtab {
	size_t	ht_size;		/* size (power of 2) */
	u_int	ht_mask;		/* == ht_size - 1 */
	u_int	ht_used;		/* number of entries used */
	u_int	ht_lim;			/* when to expand */
	struct	hashent **ht_tab;	/* base of table */
};
static struct hashtab strings;

/*
 * HASHFRACTION controls ht_lim, which in turn controls the average chain
 * length.  We allow a few entries, on average, as comparing them is usually
 * cheap (the h_hash values prevent a strcmp).
 */
#define	HASHFRACTION(sz) ((sz) * 3 / 2)

/* round up to next multiple of y, where y is a power of 2 */
#define	ROUND(x, y) (((x) + (y) - 1) & ~((y) - 1))

/*
 * Allocate space that will never be freed.
 */
static void *
poolalloc(size)
	size_t size;
{
	register char *p;
	register size_t alloc;
	static char *pool;
	static size_t nleft;

	if (nleft < size) {
		/*
		 * Compute a `good' size to allocate via malloc.
		 * 16384 is a guess at a good page size for malloc;
		 * 32 is a guess at malloc's overhead.
		 */
		alloc = ROUND(size + 32, 16384) - 32;
		p = emalloc(alloc);
		nleft = alloc - size;
	} else {
		p = pool;
		nleft -= size;
	}
	pool = p + size;
	return (p);
}

/*
 * Initialize a new hash table.  The size must be a power of 2.
 */
static void
ht_init(ht, sz)
	register struct hashtab *ht;
	size_t sz;
{
	register struct hashent **h;
	register u_int n;

	h = emalloc(sz * sizeof *h);
	ht->ht_tab = h;
	ht->ht_size = sz;
	ht->ht_mask = sz - 1;
	for (n = 0; n < sz; n++)
		*h++ = NULL;
	ht->ht_used = 0;
	ht->ht_lim = HASHFRACTION(sz);
}

/*
 * Expand an existing hash table.
 */
static void
ht_expand(ht)
	register struct hashtab *ht;
{
	register struct hashent *p, **h, **oldh, *q;
	register u_int n, i;

	n = ht->ht_size * 2;
	h = emalloc(n * sizeof *h);
	for (i = 0; i < n; i++)
		h[i] = NULL;
	oldh = ht->ht_tab;
	n--;
	for (i = ht->ht_size; i != 0; i--) {
		for (p = *oldh++; p != NULL; p = q) {
			q = p->h_next;
			p->h_next = h[p->h_hash & n];
			h[p->h_hash & n] = p;
		}
	}
	free(ht->ht_tab);
	ht->ht_tab = h;
	ht->ht_mask = n;
	ht->ht_size = ++n;
	ht->ht_lim = HASHFRACTION(n);
}

/*
 * Make a new hash entry, setting its h_next to NULL.
 */
static inline struct hashent *
newhashent(name, h)
	const char *name;
	u_int h;
{
	register struct hashent *hp;
	register char *m;

	m = poolalloc(sizeof(*hp) + ALIGNBYTES);
	hp = (struct hashent *)ALIGN(m);
	hp->h_name = name;
	hp->h_hash = h;
	hp->h_next = NULL;
	return (hp);
}

/*
 * Hash a string.
 */
static inline u_int
hash(str)
	register const char *str;
{
	register u_int h;

	for (h = 0; *str;)
		h = (h << 5) + h + *str++;
	return (h);
}

void
initintern()
{

	ht_init(&strings, 128);
}

/*
 * Generate a single unique copy of the given string.  We expect this
 * function to be used frequently, so it should be fast.
 */
const char *
intern(s)
	register const char *s;
{
	register struct hashtab *ht;
	register struct hashent *hp, **hpp;
	register u_int h;
	register char *p;
	register size_t l;

	ht = &strings;
	h = hash(s);
	hpp = &ht->ht_tab[h & ht->ht_mask];
	for (; (hp = *hpp) != NULL; hpp = &hp->h_next)
		if (hp->h_hash == h && strcmp(hp->h_name, s) == 0)
			return (hp->h_name);
	l = strlen(s) + 1;
	p = poolalloc(l);
	bcopy(s, p, l);
	*hpp = newhashent(p, h);
	if (++ht->ht_used > ht->ht_lim)
		ht_expand(ht);
	return (p);
}

struct hashtab *
ht_new()
{
	register struct hashtab *ht;

	ht = emalloc(sizeof *ht);
	ht_init(ht, 8);
	return (ht);
}

/*
 * Insert and/or replace.
 */
int
ht_insrep(ht, nam, val, replace)
	register struct hashtab *ht;
	register const char *nam;
	void *val;
	int replace;
{
	register struct hashent *hp, **hpp;
	register u_int h;

	h = hash(nam);
	hpp = &ht->ht_tab[h & ht->ht_mask];
	for (; (hp = *hpp) != NULL; hpp = &hp->h_next) {
		if (hp->h_name == nam) {
			if (replace)
				hp->h_value = val;
			return (1);
		}
	}
	*hpp = hp = newhashent(nam, h);
	hp->h_value = val;
	return (0);
}

void *
ht_lookup(ht, nam)
	register struct hashtab *ht;
	register const char *nam;
{
	register struct hashent *hp, **hpp;
	register u_int h;

	h = hash(nam);
	hpp = &ht->ht_tab[h & ht->ht_mask];
	for (; (hp = *hpp) != NULL; hpp = &hp->h_next)
		if (hp->h_name == nam)
			return (hp->h_value);
	return (NULL);
}
