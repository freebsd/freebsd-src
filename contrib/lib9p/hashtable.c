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
#include <sys/types.h>
#include <sys/queue.h>
#include "lib9p_impl.h"
#include "hashtable.h"

void
ht_init(struct ht *h, size_t size)
{
	size_t i;

	memset(h, 0, sizeof(struct ht));
	h->ht_nentries = size;
	h->ht_entries = l9p_calloc(size, sizeof(struct ht_entry));

	for (i = 0; i < size; i++)
		TAILQ_INIT(&h->ht_entries[i].hte_items);
}

void
ht_destroy(struct ht *h)
{
	struct ht_entry *he;
	struct ht_item *hi;
	size_t i;

	for (i = 0; i < h->ht_nentries; i++) {
		he = &h->ht_entries[i];
		hi = TAILQ_FIRST(&he->hte_items);

		while ((hi = TAILQ_NEXT(hi, hti_link)) != NULL)
			TAILQ_REMOVE(&he->hte_items, hi, hti_link);
	}

	free(h->ht_entries);
	free(h);
}

void *
ht_find(struct ht *h, uint32_t hash)
{
	struct ht_entry *entry;
	struct ht_item *item;

	entry = &h->ht_entries[hash % h->ht_nentries];

	TAILQ_FOREACH(item, &entry->hte_items, hti_link) {
		if (item->hti_hash == hash)
			return (item->hti_data);
	}

	return (NULL);
}

int
ht_add(struct ht *h, uint32_t hash, void *value)
{
	struct ht_entry *entry;
	struct ht_item *item;

	entry = &h->ht_entries[hash % h->ht_nentries];

	TAILQ_FOREACH(item, &entry->hte_items, hti_link) {
		if (item->hti_hash == hash) {
			errno = EEXIST;
			return (-1);
		}
	}

	item = l9p_calloc(1, sizeof(struct ht_item));
	item->hti_hash = hash;
	item->hti_data = value;
	TAILQ_INSERT_TAIL(&entry->hte_items, item, hti_link);

	return (0);
}

int
ht_remove(struct ht *h, uint32_t hash)
{
	struct ht_entry *entry;
	struct ht_item *item, *tmp;
	size_t slot = hash % h->ht_nentries;

	entry = &h->ht_entries[slot];

	TAILQ_FOREACH_SAFE(item, &entry->hte_items, hti_link, tmp) {
		if (item->hti_hash == hash) {
			TAILQ_REMOVE(&entry->hte_items, item, hti_link);
			free(item->hti_data);
			free(item);
			return (0);
		}
	}

	errno = ENOENT;
	return (-1);
}

int
ht_remove_at_iter(struct ht_iter *iter)
{
	assert(iter != NULL);

	if (iter->htit_cursor == NULL) {
		errno = EINVAL;
		return (-1);
	}

	TAILQ_REMOVE(&iter->htit_parent->ht_entries[iter->htit_slot].hte_items,
	    iter->htit_cursor, hti_link);
	return (0);
}

void
ht_iter(struct ht *h, struct ht_iter *iter)
{
	iter->htit_parent = h;
	iter->htit_slot = 0;
	iter->htit_cursor = TAILQ_FIRST(&h->ht_entries[0].hte_items);
}

void *
ht_next(struct ht_iter *iter)
{
	struct ht_item *item;

	item = iter->htit_cursor;

retry:
	if ((iter->htit_cursor = TAILQ_NEXT(iter->htit_cursor, hti_link)) == NULL)
	{
		if (iter->htit_slot == iter->htit_parent->ht_nentries)
			return (NULL);

		iter->htit_slot++;
		goto retry;

	}

	return (item);
}
