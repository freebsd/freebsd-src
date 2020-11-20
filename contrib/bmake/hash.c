/*	$NetBSD: hash.c,v 1.57 2020/11/14 21:29:44 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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

/* Hash tables with string keys. */

#include "make.h"

/*	"@(#)hash.c	8.1 (Berkeley) 6/6/93"	*/
MAKE_RCSID("$NetBSD: hash.c,v 1.57 2020/11/14 21:29:44 rillig Exp $");

/*
 * The ratio of # entries to # buckets at which we rebuild the table to
 * make it larger.
 */
#define rebuildLimit 3

/* This hash function matches Gosling's emacs and java.lang.String. */
static unsigned int
hash(const char *key, size_t *out_keylen)
{
	unsigned int h = 0;
	const char *p = key;
	while (*p != '\0')
		h = (h << 5) - h + (unsigned char)*p++;
	if (out_keylen != NULL)
		*out_keylen = (size_t)(p - key);
	return h;
}

unsigned int
Hash_Hash(const char *key)
{
    return hash(key, NULL);
}

static HashEntry *
HashTable_Find(HashTable *t, unsigned int h, const char *key)
{
	HashEntry *e;
	unsigned int chainlen = 0;

#ifdef DEBUG_HASH_LOOKUP
	DEBUG4(HASH, "%s: %p h=%08x key=%s\n", __func__, t, h, key);
#endif

	for (e = t->buckets[h & t->bucketsMask]; e != NULL; e = e->next) {
		chainlen++;
		if (e->key_hash == h && strcmp(e->key, key) == 0)
			break;
	}

	if (chainlen > t->maxchain)
		t->maxchain = chainlen;

	return e;
}

/* Set up the hash table. */
void
HashTable_Init(HashTable *t)
{
	unsigned int n = 16, i;
	HashEntry **buckets = bmake_malloc(sizeof *buckets * n);
	for (i = 0; i < n; i++)
		buckets[i] = NULL;

	t->buckets = buckets;
	t->bucketsSize = n;
	t->numEntries = 0;
	t->bucketsMask = n - 1;
	t->maxchain = 0;
}

/* Remove everything from the hash table and frees up the memory. */
void
HashTable_Done(HashTable *t)
{
	HashEntry **buckets = t->buckets;
	size_t i, n = t->bucketsSize;

	for (i = 0; i < n; i++) {
		HashEntry *he = buckets[i];
		while (he != NULL) {
			HashEntry *next = he->next;
			free(he);
			he = next;
		}
	}
	free(t->buckets);

#ifdef CLEANUP
	t->buckets = NULL;
#endif
}

/* Find the entry corresponding to the key, or return NULL. */
HashEntry *
HashTable_FindEntry(HashTable *t, const char *key)
{
	unsigned int h = hash(key, NULL);
	return HashTable_Find(t, h, key);
}

/* Find the value corresponding to the key, or return NULL. */
void *
HashTable_FindValue(HashTable *t, const char *key)
{
	HashEntry *he = HashTable_FindEntry(t, key);
	return he != NULL ? he->value : NULL;
}

/* Find the value corresponding to the key and the precomputed hash,
 * or return NULL. */
void *
HashTable_FindValueHash(HashTable *t, const char *key, unsigned int h)
{
	HashEntry *he = HashTable_Find(t, h, key);
	return he != NULL ? he->value : NULL;
}

/* Make the hash table larger. Any bucket numbers from the old table become
 * invalid; the hash codes stay valid though. */
static void
HashTable_Enlarge(HashTable *t)
{
	unsigned int oldSize = t->bucketsSize;
	HashEntry **oldBuckets = t->buckets;
	unsigned int newSize = 2 * oldSize;
	unsigned int newMask = newSize - 1;
	HashEntry **newBuckets = bmake_malloc(sizeof *newBuckets * newSize);
	size_t i;

	for (i = 0; i < newSize; i++)
		newBuckets[i] = NULL;

	for (i = 0; i < oldSize; i++) {
		HashEntry *he = oldBuckets[i];
		while (he != NULL) {
			HashEntry *next = he->next;
			he->next = newBuckets[he->key_hash & newMask];
			newBuckets[he->key_hash & newMask] = he;
			he = next;
		}
	}

	free(oldBuckets);

	t->bucketsSize = newSize;
	t->bucketsMask = newMask;
	t->buckets = newBuckets;
	DEBUG5(HASH, "%s: %p size=%d entries=%d maxchain=%d\n",
	       __func__, t, t->bucketsSize, t->numEntries, t->maxchain);
	t->maxchain = 0;
}

/* Find or create an entry corresponding to the key.
 * Return in out_isNew whether a new entry has been created. */
HashEntry *
HashTable_CreateEntry(HashTable *t, const char *key, Boolean *out_isNew)
{
	size_t keylen;
	unsigned int h = hash(key, &keylen);
	HashEntry *he = HashTable_Find(t, h, key);

	if (he != NULL) {
		if (out_isNew != NULL)
			*out_isNew = FALSE;
		return he;
	}

	if (t->numEntries >= rebuildLimit * t->bucketsSize)
		HashTable_Enlarge(t);

	he = bmake_malloc(sizeof *he + keylen);
	he->value = NULL;
	he->key_hash = h;
	memcpy(he->key, key, keylen + 1);

	he->next = t->buckets[h & t->bucketsMask];
	t->buckets[h & t->bucketsMask] = he;
	t->numEntries++;

	if (out_isNew != NULL)
		*out_isNew = TRUE;
	return he;
}

HashEntry *
HashTable_Set(HashTable *t, const char *key, void *value)
{
	HashEntry *he = HashTable_CreateEntry(t, key, NULL);
	HashEntry_Set(he, value);
	return he;
}

/* Delete the entry from the table and free the associated memory. */
void
HashTable_DeleteEntry(HashTable *t, HashEntry *he)
{
	HashEntry **ref = &t->buckets[he->key_hash & t->bucketsMask];
	HashEntry *p;

	for (; (p = *ref) != NULL; ref = &p->next) {
		if (p == he) {
			*ref = p->next;
			free(p);
			t->numEntries--;
			return;
		}
	}
	abort();
}

/* Set things up for iterating over all entries in the hash table. */
void
HashIter_Init(HashIter *hi, HashTable *t)
{
	hi->table = t;
	hi->nextBucket = 0;
	hi->entry = NULL;
}

/* Return the next entry in the hash table, or NULL if the end of the table
 * is reached. */
HashEntry *
HashIter_Next(HashIter *hi)
{
	HashTable *t = hi->table;
	HashEntry *he = hi->entry;
	HashEntry **buckets = t->buckets;
	unsigned int bucketsSize = t->bucketsSize;

	if (he != NULL)
		he = he->next;	/* skip the most recently returned entry */

	while (he == NULL) {	/* find the next nonempty chain */
		if (hi->nextBucket >= bucketsSize)
			return NULL;
		he = buckets[hi->nextBucket++];
	}
	hi->entry = he;
	return he;
}

void
HashTable_DebugStats(HashTable *t, const char *name)
{
	DEBUG4(HASH, "HashTable %s: size=%u numEntries=%u maxchain=%u\n",
	       name, t->bucketsSize, t->numEntries, t->maxchain);
}
