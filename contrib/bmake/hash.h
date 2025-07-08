/*	$NetBSD: hash.h,v 1.52 2025/04/22 19:28:50 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 *
 *	from: @(#)hash.h	8.1 (Berkeley) 6/6/93
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
 *
 *	from: @(#)hash.h	8.1 (Berkeley) 6/6/93
 */

/* Hash tables with string keys and pointer values. */

#ifndef MAKE_HASH_H
#define MAKE_HASH_H

/* A single key-value entry in the hash table. */
typedef struct HashEntry {
	struct HashEntry *next;	/* Used to link together all the entries
				 * associated with the same bucket. */
	void *value;
	unsigned hash;		/* hash value of the key */
	char key[1];		/* key string, variable length */
} HashEntry;

/* The hash table containing the entries. */
typedef struct HashTable {
	HashEntry **buckets;
	unsigned bucketsSize;
	unsigned numEntries;
	unsigned bucketsMask;	/* Used to select the bucket for a hash. */
} HashTable;

/* State of an iteration over all entries in a table. */
typedef struct HashIter {
	HashTable *table;	/* Table being searched. */
	unsigned nextBucket;	/* Next bucket to check (after current). */
	HashEntry *entry;	/* Next entry to check in current bucket. */
} HashIter;

/* A set of strings. */
typedef struct HashSet {
	HashTable tbl;
} HashSet;

MAKE_INLINE void * MAKE_ATTR_USE
HashEntry_Get(HashEntry *he)
{
	return he->value;
}

MAKE_INLINE void
HashEntry_Set(HashEntry *he, void *datum)
{
	he->value = datum;
}

/* Set things up for iterating over all entries in the hash table. */
MAKE_INLINE void
HashIter_Init(HashIter *hi, HashTable *t)
{
	hi->table = t;
	hi->nextBucket = 0;
	hi->entry = NULL;
}

void HashTable_Init(HashTable *);
void HashTable_Done(HashTable *);
HashEntry *HashTable_FindEntry(HashTable *, const char *) MAKE_ATTR_USE;
void *HashTable_FindValue(HashTable *, const char *) MAKE_ATTR_USE;
unsigned Hash_Substring(Substring) MAKE_ATTR_USE;
void *HashTable_FindValueBySubstringHash(HashTable *, Substring, unsigned)
    MAKE_ATTR_USE;
HashEntry *HashTable_CreateEntry(HashTable *, const char *, bool *);
void HashTable_Set(HashTable *, const char *, void *);
void HashTable_DeleteEntry(HashTable *, HashEntry *);
void HashTable_DebugStats(const HashTable *, const char *);

bool HashIter_Next(HashIter *) MAKE_ATTR_USE;

MAKE_INLINE void
HashSet_Init(HashSet *set)
{
	HashTable_Init(&set->tbl);
}

MAKE_INLINE void
HashSet_Done(HashSet *set)
{
	HashTable_Done(&set->tbl);
}

MAKE_INLINE bool
HashSet_Add(HashSet *set, const char *key)
{
	bool isNew;

	(void)HashTable_CreateEntry(&set->tbl, key, &isNew);
	return isNew;
}

MAKE_INLINE bool MAKE_ATTR_USE
HashSet_Contains(HashSet *set, const char *key)
{
	return HashTable_FindEntry(&set->tbl, key) != NULL;
}

MAKE_INLINE void
HashIter_InitSet(HashIter *hi, HashSet *set)
{
	HashIter_Init(hi, &set->tbl);
}

#endif
