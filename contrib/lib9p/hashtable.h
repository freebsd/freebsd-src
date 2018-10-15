/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LIB9P_HASHTABLE_H
#define LIB9P_HASHTABLE_H

#include <pthread.h>
#include <sys/queue.h>

struct ht {
	struct ht_entry * 	ht_entries;
	ssize_t 		ht_nentries;
	pthread_rwlock_t	ht_rwlock;
};

struct ht_entry {
	TAILQ_HEAD(, ht_item) hte_items;
};

struct ht_item {
	uint32_t		hti_hash;
	void *			hti_data;
	TAILQ_ENTRY(ht_item)	hti_link;
};

struct ht_iter {
	struct ht *		htit_parent;
	struct ht_item *	htit_curr;
	struct ht_item *	htit_next;
	ssize_t			htit_slot;
};

/*
 * Obtain read-lock on hash table.
 */
static inline int
ht_rdlock(struct ht *h)
{

	return pthread_rwlock_rdlock(&h->ht_rwlock);
}

/*
 * Obtain write-lock on hash table.
 */
static inline int
ht_wrlock(struct ht *h)
{

	return pthread_rwlock_wrlock(&h->ht_rwlock);
}

/*
 * Release lock on hash table.
 */
static inline int
ht_unlock(struct ht *h)
{

	return pthread_rwlock_unlock(&h->ht_rwlock);
}

void ht_init(struct ht *h, ssize_t size);
void ht_destroy(struct ht *h);
void *ht_find(struct ht *h, uint32_t hash);
void *ht_find_locked(struct ht *h, uint32_t hash);
int ht_add(struct ht *h, uint32_t hash, void *value);
int ht_remove(struct ht *h, uint32_t hash);
int ht_remove_locked(struct ht *h, uint32_t hash);
int ht_remove_at_iter(struct ht_iter *iter);
void ht_iter(struct ht *h, struct ht_iter *iter);
void *ht_next(struct ht_iter *iter);

#endif  /* LIB9P_HASHTABLE_H */
