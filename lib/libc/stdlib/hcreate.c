/* $NetBSD: hcreate.c,v 1.7 2011/09/14 23:33:51 christos Exp $ */

/*
 * Copyright (c) 2001 Christopher G. Demetriou
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 * 
 * <<Id: LICENSE,v 1.2 2000/06/14 15:57:33 cgd Exp>>
 */

/*
 * hcreate() / hsearch() / hdestroy()
 *
 * SysV/XPG4 hash table functions.
 *
 * Implementation done based on NetBSD manual page and Solaris manual page,
 * plus my own personal experience about how they're supposed to work.
 *
 * I tried to look at Knuth (as cited by the Solaris manual page), but
 * nobody had a copy in the office, so...
 */

#include <sys/cdefs.h>
#if 0
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: hcreate.c,v 1.8 2011/09/17 16:54:39 christos Exp $");
#endif /* LIBC_SCCS and not lint */
#endif
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/queue.h>
#include <errno.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>

/*
 * DO NOT MAKE THIS STRUCTURE LARGER THAN 32 BYTES (4 ptrs on 64-bit
 * ptr machine) without adjusting MAX_BUCKETS_LG2 below.
 */
struct internal_entry {
	SLIST_ENTRY(internal_entry) link;
	ENTRY ent;
};
SLIST_HEAD(internal_head, internal_entry);

#define	MIN_BUCKETS_LG2	4
#define	MIN_BUCKETS	(1 << MIN_BUCKETS_LG2)

/*
 * max * sizeof internal_entry must fit into size_t.
 * assumes internal_entry is <= 32 (2^5) bytes.
 */
#define	MAX_BUCKETS_LG2	(sizeof (size_t) * 8 - 1 - 5)
#define	MAX_BUCKETS	((size_t)1 << MAX_BUCKETS_LG2)

/* Default hash function, from db/hash/hash_func.c */
extern u_int32_t (*__default_hash)(const void *, size_t);

static struct hsearch_data htable;

int
hcreate(size_t nel)
{

	/* Make sure this is not called when a table already exists. */
	if (htable.table != NULL) {
		errno = EINVAL;
		return 0;
	}
	return hcreate_r(nel, &htable);
}

int
hcreate_r(size_t nel, struct hsearch_data *head)
{
	struct internal_head *table;
	size_t idx;
	unsigned int p2;
	void *p;

	/* If nel is too small, make it min sized. */
	if (nel < MIN_BUCKETS)
		nel = MIN_BUCKETS;

	/* If it is too large, cap it. */
	if (nel > MAX_BUCKETS)
		nel = MAX_BUCKETS;

	/* If it is not a power of two in size, round up. */
	if ((nel & (nel - 1)) != 0) {
		for (p2 = 0; nel != 0; p2++)
			nel >>= 1;
		nel = 1 << p2;
	}
	
	/* Allocate the table. */
	head->size = nel;
	head->filled = 0;
	p = malloc(nel * sizeof table[0]);
	if (p == NULL) {
		errno = ENOMEM;
		return 0;
	}
	head->table = p;
	table = p;

	/* Initialize it. */
	for (idx = 0; idx < nel; idx++)
		SLIST_INIT(&table[idx]);

	return 1;
}

void
hdestroy(void)
{
	hdestroy_r(&htable);
}

void
hdestroy_r(struct hsearch_data *head)
{
	struct internal_entry *ie;
	size_t idx;
	void *p;
	struct internal_head *table;

	if (head == NULL)
		return;

	p = head->table;
	head->table = NULL;
	table = p;

	for (idx = 0; idx < head->size; idx++) {
		while (!SLIST_EMPTY(&table[idx])) {
			ie = SLIST_FIRST(&table[idx]);
			SLIST_REMOVE_HEAD(&table[idx], link);
			free(ie);
		}
	}
	free(table);
}

ENTRY *
hsearch(ENTRY item, ACTION action)
{
	ENTRY *ep;
	(void)hsearch_r(item, action, &ep, &htable);
	return ep;
}

int
hsearch_r(ENTRY item, ACTION action, ENTRY **itemp, struct hsearch_data *head)
{
	struct internal_head *table, *chain;
	struct internal_entry *ie;
	uint32_t hashval;
	size_t len;
	void *p;

	p = head->table;
	table = p;

	len = strlen(item.key);
	hashval = (*__default_hash)(item.key, len);

	chain = &table[hashval & (head->size - 1)];
	ie = SLIST_FIRST(chain);
	while (ie != NULL) {
		if (strcmp(ie->ent.key, item.key) == 0)
			break;
		ie = SLIST_NEXT(ie, link);
	}

	if (ie != NULL) {
		*itemp = &ie->ent;
		return 1;
	} else if (action == FIND) {
		*itemp = NULL;
		errno = ESRCH;
		return 1;
	}

	ie = malloc(sizeof *ie);
	if (ie == NULL)
		return 0;
	ie->ent.key = item.key;
	ie->ent.data = item.data;

	SLIST_INSERT_HEAD(chain, ie, link);
	*itemp = &ie->ent;
	head->filled++;
	return 1;
}
