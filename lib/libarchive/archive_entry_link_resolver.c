/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "archive_platform.h"
__FBSDID("$FreeBSD$");

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "archive_entry.h"

/* Initial size of link cache. */
#define	links_cache_initial_size 1024

struct archive_entry_linkresolver {
	char			 *last_name;
	unsigned long		  number_entries;
	size_t			  number_buckets;
	struct links_entry	**buckets;
};

struct links_entry {
	struct links_entry	*next;
	struct links_entry	*previous;
	int			 links;
	dev_t			 dev;
	ino_t			 ino;
	char			*name;
};

struct archive_entry_linkresolver *
archive_entry_linkresolver_new(void)
{
	struct archive_entry_linkresolver *links_cache;
	size_t i;

	links_cache = malloc(sizeof(struct archive_entry_linkresolver));
	if (links_cache == NULL)
		return (NULL);
	memset(links_cache, 0, sizeof(struct archive_entry_linkresolver));
	links_cache->number_buckets = links_cache_initial_size;
	links_cache->buckets = malloc(links_cache->number_buckets *
	    sizeof(links_cache->buckets[0]));
	if (links_cache->buckets == NULL) {
		free(links_cache);
		return (NULL);
	}
	for (i = 0; i < links_cache->number_buckets; i++)
		links_cache->buckets[i] = NULL;
	return (links_cache);
}

void
archive_entry_linkresolver_free(struct archive_entry_linkresolver *links_cache)
{
	size_t i;

	if (links_cache->buckets == NULL)
		return;

	for (i = 0; i < links_cache->number_buckets; i++) {
		while (links_cache->buckets[i] != NULL) {
			struct links_entry *lp = links_cache->buckets[i]->next;
			if (links_cache->buckets[i]->name != NULL)
				free(links_cache->buckets[i]->name);
			free(links_cache->buckets[i]);
			links_cache->buckets[i] = lp;
		}
	}
	free(links_cache->buckets);
	links_cache->buckets = NULL;
}

const char *
archive_entry_linkresolve(struct archive_entry_linkresolver *links_cache,
    struct archive_entry *entry)
{
	struct links_entry	*le, **new_buckets;
	int			 hash;
	size_t			 i, new_size;
	dev_t			 dev;
	ino_t			 ino;
	int			 nlinks;


	/* Free a held name. */
	free(links_cache->last_name);
	links_cache->last_name = NULL;

	/* If the links cache overflowed and got flushed, don't bother. */
	if (links_cache->buckets == NULL)
		return (NULL);

	dev = archive_entry_dev(entry);
	ino = archive_entry_ino(entry);
	nlinks = archive_entry_nlink(entry);

	/* An entry with one link can't be a hard link. */
	if (nlinks == 1)
		return (NULL);

	/* If the links cache is getting too full, enlarge the hash table. */
	if (links_cache->number_entries > links_cache->number_buckets * 2)
	{
		/* Try to enlarge the bucket list. */
		new_size = links_cache->number_buckets * 2;
		new_buckets = malloc(new_size * sizeof(struct links_entry *));

		if (new_buckets != NULL) {
			memset(new_buckets, 0,
			    new_size * sizeof(struct links_entry *));
			for (i = 0; i < links_cache->number_buckets; i++) {
				while (links_cache->buckets[i] != NULL) {
					/* Remove entry from old bucket. */
					le = links_cache->buckets[i];
					links_cache->buckets[i] = le->next;

					/* Add entry to new bucket. */
					hash = (le->dev ^ le->ino) % new_size;

					if (new_buckets[hash] != NULL)
						new_buckets[hash]->previous =
						    le;
					le->next = new_buckets[hash];
					le->previous = NULL;
					new_buckets[hash] = le;
				}
			}
			free(links_cache->buckets);
			links_cache->buckets = new_buckets;
			links_cache->number_buckets = new_size;
		}
	}

	/* Try to locate this entry in the links cache. */
	hash = ( dev ^ ino ) % links_cache->number_buckets;
	for (le = links_cache->buckets[hash]; le != NULL; le = le->next) {
		if (le->dev == dev && le->ino == ino) {
			/*
			 * Decrement link count each time and release
			 * the entry if it hits zero.  This saves
			 * memory and is necessary for detecting
			 * missed links.
			 */
			--le->links;
			if (le->links > 0)
				return (le->name);
			/*
			 * When we release the entry, save the name
			 * until the next call.
			 */
			links_cache->last_name = le->name;
			/*
			 * Release the entry.
			 */
			if (le->previous != NULL)
				le->previous->next = le->next;
			if (le->next != NULL)
				le->next->previous = le->previous;
			if (links_cache->buckets[hash] == le)
				links_cache->buckets[hash] = le->next;
			links_cache->number_entries--;
			free(le);
			return (links_cache->last_name);
		}
	}

	/* Add this entry to the links cache. */
	le = malloc(sizeof(struct links_entry));
	if (le == NULL)
		return (NULL);
	le->name = strdup(archive_entry_pathname(entry));
	if (le->name == NULL) {
		free(le);
		return (NULL);
	}

	/* If we could allocate the entry, record it. */
	if (links_cache->buckets[hash] != NULL)
		links_cache->buckets[hash]->previous = le;
	links_cache->number_entries++;
	le->next = links_cache->buckets[hash];
	le->previous = NULL;
	links_cache->buckets[hash] = le;
	le->dev = dev;
	le->ino = ino;
	le->links = nlinks - 1;
	return (NULL);
}
