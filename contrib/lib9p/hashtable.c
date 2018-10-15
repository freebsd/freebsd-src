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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/queue.h>
#include "lib9p_impl.h"
#include "hashtable.h"

static struct ht_item *ht_iter_advance(struct ht_iter *, struct ht_item *);

void
ht_init(struct ht *h, ssize_t size)
{
	ssize_t i;

	memset(h, 0, sizeof(struct ht));
	h->ht_nentries = size;
	h->ht_entries = l9p_calloc((size_t)size, sizeof(struct ht_entry));
	pthread_rwlock_init(&h->ht_rwlock, NULL);

	for (i = 0; i < size; i++)
		TAILQ_INIT(&h->ht_entries[i].hte_items);
}

void
ht_destroy(struct ht *h)
{
	struct ht_entry *he;
	struct ht_item *item, *tmp;
	ssize_t i;

	for (i = 0; i < h->ht_nentries; i++) {
		he = &h->ht_entries[i];
		TAILQ_FOREACH_SAFE(item, &he->hte_items, hti_link, tmp) {
			free(item);
		}
	}

	pthread_rwlock_destroy(&h->ht_rwlock);
	free(h->ht_entries);
	h->ht_entries = NULL;
}

void *
ht_find(struct ht *h, uint32_t hash)
{
	void *result;

	ht_rdlock(h);
	result = ht_find_locked(h, hash);
	ht_unlock(h);
	return (result);
}

void *
ht_find_locked(struct ht *h, uint32_t hash)
{
	struct ht_entry *entry;
	struct ht_item *item;

	entry = &h->ht_entries[hash % h->ht_nentries];

	TAILQ_FOREACH(item, &entry->hte_items, hti_link) {
		if (item->hti_hash == hash) {
			return (item->hti_data);
		}
	}

	return (NULL);
}

int
ht_add(struct ht *h, uint32_t hash, void *value)
{
	struct ht_entry *entry;
	struct ht_item *item;

	ht_wrlock(h);
	entry = &h->ht_entries[hash % h->ht_nentries];

	TAILQ_FOREACH(item, &entry->hte_items, hti_link) {
		if (item->hti_hash == hash) {
			errno = EEXIST;
			ht_unlock(h);
			return (-1);
		}
	}

	item = l9p_calloc(1, sizeof(struct ht_item));
	item->hti_hash = hash;
	item->hti_data = value;
	TAILQ_INSERT_TAIL(&entry->hte_items, item, hti_link);
	ht_unlock(h);

	return (0);
}

int
ht_remove(struct ht *h, uint32_t hash)
{
	int result;

	ht_wrlock(h);
	result = ht_remove_locked(h, hash);
	ht_unlock(h);
	return (result);
}

int
ht_remove_locked(struct ht *h, uint32_t hash)
{
	struct ht_entry *entry;
	struct ht_item *item, *tmp;
	ssize_t slot = hash % h->ht_nentries;

	entry = &h->ht_entries[slot];

	TAILQ_FOREACH_SAFE(item, &entry->hte_items, hti_link, tmp) {
		if (item->hti_hash == hash) {
			TAILQ_REMOVE(&entry->hte_items, item, hti_link);
			free(item);
			return (0);
		}
	}

	errno = ENOENT;
	return (-1);
}

/*
 * Inner workings for advancing the iterator.
 *
 * If we have a current item, that tells us how to find the
 * next item.  If not, we get the first item from the next
 * slot (well, the next slot with an item); in any case, we
 * record the new slot and return the next item.
 *
 * For bootstrapping, iter->htit_slot can be -1 to start
 * searching at slot 0.
 *
 * Caller must hold a lock on the table.
 */
static struct ht_item *
ht_iter_advance(struct ht_iter *iter, struct ht_item *cur)
{
	struct ht_item *next;
	struct ht *h;
	ssize_t slot;

	h = iter->htit_parent;

	if (cur == NULL)
		next = NULL;
	else
		next = TAILQ_NEXT(cur, hti_link);

	if (next == NULL) {
		slot = iter->htit_slot;
		while (++slot < h->ht_nentries) {
			next = TAILQ_FIRST(&h->ht_entries[slot].hte_items);
			if (next != NULL)
				break;
		}
		iter->htit_slot = slot;
	}
	return (next);
}

/*
 * Remove the current item - there must be one, or this is an
 * error.  This (necessarily) pre-locates the next item, so callers
 * must not use it on an actively-changing table.
 */
int
ht_remove_at_iter(struct ht_iter *iter)
{
	struct ht_item *item;
	struct ht *h;
	ssize_t slot;

	assert(iter != NULL);

	if ((item = iter->htit_curr) == NULL) {
		errno = EINVAL;
		return (-1);
	}

	/* remove the item from the table, saving the NEXT one */
	h = iter->htit_parent;
	ht_wrlock(h);
	slot = iter->htit_slot;
	iter->htit_next = ht_iter_advance(iter, item);
	TAILQ_REMOVE(&h->ht_entries[slot].hte_items, item, hti_link);
	ht_unlock(h);

	/* mark us as no longer on an item, then free it */
	iter->htit_curr = NULL;
	free(item);

	return (0);
}

/*
 * Initialize iterator.  Subsequent ht_next calls will find the
 * first item, then the next, and so on.  Callers should in general
 * not use this on actively-changing tables, though we do our best
 * to make it semi-sensible.
 */
void
ht_iter(struct ht *h, struct ht_iter *iter)
{

	iter->htit_parent = h;
	iter->htit_curr = NULL;
	iter->htit_next = NULL;
	iter->htit_slot = -1;	/* which will increment to 0 */
}

/*
 * Return the next item, which is the first item if we have not
 * yet been called on this iterator, or the next item if we have.
 */
void *
ht_next(struct ht_iter *iter)
{
	struct ht_item *item;
	struct ht *h;

	if ((item = iter->htit_next) == NULL) {
		/* no pre-loaded next; find next from current */
		h = iter->htit_parent;
		ht_rdlock(h);
		item = ht_iter_advance(iter, iter->htit_curr);
		ht_unlock(h);
	} else
		iter->htit_next = NULL;
	iter->htit_curr = item;
	return (item == NULL ? NULL : item->hti_data);
}
