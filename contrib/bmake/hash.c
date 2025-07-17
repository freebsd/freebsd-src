/*	$NetBSD: hash.c,v 1.80 2025/04/22 19:28:50 rillig Exp $	*/

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

/* Hash tables with string keys and pointer values. */

#include "make.h"

/*	"@(#)hash.c	8.1 (Berkeley) 6/6/93"	*/
MAKE_RCSID("$NetBSD: hash.c,v 1.80 2025/04/22 19:28:50 rillig Exp $");

/*
 * The ratio of # entries to # buckets at which we rebuild the table to
 * make it larger.
 */
#define rebuildLimit 3

/* This hash function matches Gosling's Emacs and java.lang.String. */
static unsigned
Hash_String(const char *key, const char **out_keyEnd)
{
	unsigned h;
	const char *p;

	h = 0;
	for (p = key; *p != '\0'; p++)
		h = 31 * h + (unsigned char)*p;

	*out_keyEnd = p;
	return h;
}

/* This hash function matches Gosling's Emacs and java.lang.String. */
unsigned
Hash_Substring(Substring key)
{
	unsigned h;
	const char *p;

	h = 0;
	for (p = key.start; p != key.end; p++)
		h = 31 * h + (unsigned char)*p;
	return h;
}

static HashEntry *
HashTable_Find(HashTable *t, Substring key, unsigned h)
{
	HashEntry *he;
	size_t keyLen = Substring_Length(key);

#ifdef DEBUG_HASH_LOOKUP
	DEBUG4(HASH, "HashTable_Find: %p h=%08x key=%.*s\n",
	    t, h, (int)keyLen, key.start);
#endif

	for (he = t->buckets[h & t->bucketsMask]; he != NULL; he = he->next) {
		if (he->hash == h &&
		    strncmp(he->key, key.start, keyLen) == 0 &&
		    he->key[keyLen] == '\0')
			break;
	}

	return he;
}

/* Set up the hash table. */
void
HashTable_Init(HashTable *t)
{
	unsigned n = 16, i;
	HashEntry **buckets = bmake_malloc(sizeof *buckets * n);
	for (i = 0; i < n; i++)
		buckets[i] = NULL;

	t->buckets = buckets;
	t->bucketsSize = n;
	t->numEntries = 0;
	t->bucketsMask = n - 1;
}

/*
 * Remove everything from the hash table and free up the memory for the keys
 * of the hash table, but not for the values associated to these keys.
 */
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
	const char *keyEnd;
	unsigned h = Hash_String(key, &keyEnd);
	return HashTable_Find(t, Substring_Init(key, keyEnd), h);
}

/* Find the value corresponding to the key, or return NULL. */
void *
HashTable_FindValue(HashTable *t, const char *key)
{
	HashEntry *he = HashTable_FindEntry(t, key);
	return he != NULL ? he->value : NULL;
}

/*
 * Find the value corresponding to the key and the precomputed hash,
 * or return NULL.
 */
void *
HashTable_FindValueBySubstringHash(HashTable *t, Substring key, unsigned h)
{
	HashEntry *he = HashTable_Find(t, key, h);
	return he != NULL ? he->value : NULL;
}

static unsigned
HashTable_MaxChain(const HashTable *t)
{
	unsigned b, cl, max_cl = 0;
	for (b = 0; b < t->bucketsSize; b++) {
		const HashEntry *he = t->buckets[b];
		for (cl = 0; he != NULL; he = he->next)
			cl++;
		if (cl > max_cl)
			max_cl = cl;
	}
	return max_cl;
}

/*
 * Make the hash table larger. Any bucket numbers from the old table become
 * invalid; the hash values stay valid though.
 */
static void
HashTable_Enlarge(HashTable *t)
{
	unsigned oldSize = t->bucketsSize;
	HashEntry **oldBuckets = t->buckets;
	unsigned newSize = 2 * oldSize;
	unsigned newMask = newSize - 1;
	HashEntry **newBuckets = bmake_malloc(sizeof *newBuckets * newSize);
	size_t i;

	for (i = 0; i < newSize; i++)
		newBuckets[i] = NULL;

	for (i = 0; i < oldSize; i++) {
		HashEntry *he = oldBuckets[i];
		while (he != NULL) {
			HashEntry *next = he->next;
			he->next = newBuckets[he->hash & newMask];
			newBuckets[he->hash & newMask] = he;
			he = next;
		}
	}

	free(oldBuckets);

	t->bucketsSize = newSize;
	t->bucketsMask = newMask;
	t->buckets = newBuckets;
	DEBUG4(HASH, "HashTable_Enlarge: %p size=%d entries=%d maxchain=%d\n",
	    (void *)t, t->bucketsSize, t->numEntries, HashTable_MaxChain(t));
}

/*
 * Find or create an entry corresponding to the key.
 * Return in out_isNew whether a new entry has been created.
 */
HashEntry *
HashTable_CreateEntry(HashTable *t, const char *key, bool *out_isNew)
{
	const char *keyEnd;
	unsigned h = Hash_String(key, &keyEnd);
	HashEntry *he = HashTable_Find(t, Substring_Init(key, keyEnd), h);

	if (he != NULL) {
		if (out_isNew != NULL)
			*out_isNew = false;
		return he;
	}

	if (t->numEntries >= rebuildLimit * t->bucketsSize)
		HashTable_Enlarge(t);

	he = bmake_malloc(sizeof *he + (size_t)(keyEnd - key));
	he->value = NULL;
	he->hash = h;
	memcpy(he->key, key, (size_t)(keyEnd - key) + 1);

	he->next = t->buckets[h & t->bucketsMask];
	t->buckets[h & t->bucketsMask] = he;
	t->numEntries++;

	if (out_isNew != NULL)
		*out_isNew = true;
	return he;
}

void
HashTable_Set(HashTable *t, const char *key, void *value)
{
	HashEntry *he = HashTable_CreateEntry(t, key, NULL);
	HashEntry_Set(he, value);
}

/* Delete the entry from the table, don't free the value of the entry. */
void
HashTable_DeleteEntry(HashTable *t, HashEntry *he)
{
	HashEntry **ref = &t->buckets[he->hash & t->bucketsMask];

	for (; *ref != he; ref = &(*ref)->next)
		continue;
	*ref = he->next;
	free(he);
	t->numEntries--;
}

/*
 * Place the next entry from the hash table in hi->entry, or return false if
 * the end of the table is reached.
 */
bool
HashIter_Next(HashIter *hi)
{
	HashTable *t = hi->table;
	HashEntry *he = hi->entry;
	HashEntry **buckets = t->buckets;
	unsigned bucketsSize = t->bucketsSize;

	if (he != NULL)
		he = he->next;	/* skip the most recently returned entry */

	while (he == NULL) {	/* find the next nonempty chain */
		if (hi->nextBucket >= bucketsSize)
			return false;
		he = buckets[hi->nextBucket++];
	}
	hi->entry = he;
	return true;
}

void
HashTable_DebugStats(const HashTable *t, const char *name)
{
	DEBUG4(HASH, "HashTable \"%s\": size=%u entries=%u maxchain=%u\n",
	    name, t->bucketsSize, t->numEntries, HashTable_MaxChain(t));
}
