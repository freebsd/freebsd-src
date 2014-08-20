/*
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Write-Once hash table for string to string mappings */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#ifdef TEST_RIG
#include <assert.h>
#include <stdio.h>
#endif
#include "utils/hashtable.h"
#include "utils/log.h"


struct hash_entry {
	char *pairing;		 /**< block containing 'key\0value\0' */
	unsigned int key_length; /**< length of key */
	struct hash_entry *next; /**< next entry */
};

struct hash_table {
	unsigned int nchains;
	struct hash_entry **chain;
};

/**
 * Hash a string, returning a 32bit value.  The hash algorithm used is
 * Fowler Noll Vo - a very fast and simple hash, ideal for short strings.
 * See http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash for more details.
 *
 * \param  datum   The string to hash.
 * \param  len	   Pointer to unsigned integer to record datum's length in.
 * \return The calculated hash value for the datum.
 */

static inline unsigned int hash_string_fnv(const char *datum, unsigned int *len)
{
	unsigned int z = 0x811c9dc5;
	const char *start = datum;
	*len = 0;

	if (datum == NULL)
		return 0;

	while (*datum) {
		z *= 0x01000193;
		z ^= *datum++;
	}
	*len = datum - start;

	return z;
}


/**
 * Create a new hash table, and return a context for it.  The memory consumption
 * of a hash table is approximately 8 + (nchains * 12) bytes if it is empty.
 *
 * \param  chains Number of chains/buckets this hash table will have.  This
 *		  should be a prime number, and ideally a prime number just
 *		  over a power of two, for best performance and distribution.
 * \return struct hash_table containing the context of this hash table or NULL
 *	   if there is insufficent memory to create it and its chains.
 */

struct hash_table *hash_create(unsigned int chains)
{
	struct hash_table *r = malloc(sizeof(struct hash_table));

	if (r == NULL) {
		LOG(("Not enough memory for hash table."));
		return NULL;
	}

	r->nchains = chains;
	r->chain = calloc(chains, sizeof(struct hash_entry *));

	if (r->chain == NULL) {
		LOG(("Not enough memory for %d hash table chains.", chains));
		free(r);
		return NULL;
	}

	return r;
}

/**
 * Destroys a hash table, freeing all memory associated with it.
 *
 * \param  ht	 Hash table to destroy.  After the function returns, this
 *		 will nolonger be valid.
 */

void hash_destroy(struct hash_table *ht)
{
	unsigned int i;

	if (ht == NULL)
		return;

	for (i = 0; i < ht->nchains; i++) {
		if (ht->chain[i] != NULL) {
			struct hash_entry *e = ht->chain[i];
			while (e) {
				struct hash_entry *n = e->next;
				free(e->pairing);
				free(e);
				e = n;
			}
		}
	}

	free(ht->chain);
	free(ht);
}

/**
 * Adds a key/value pair to a hash table.  If the key you're adding is already
 * in the hash table, it does not replace it, but it does take precedent over
 * it.  The old key/value pair will be inaccessable but still in memory until
 * hash_destroy() is called on the hash table.
 *
 * \param  ht	  The hash table context to add the key/value pair to.
 * \param  key	  The key to associate the value with.  A copy is made.
 * \param  value  The value to associate the key with.  A copy is made.
 * \return true if the add succeeded, false otherwise.  (Failure most likely
 *	   indicates insufficent memory to make copies of the key and value.
 */

bool hash_add(struct hash_table *ht, const char *key, const char *value)
{
	unsigned int h, c, v;
	struct hash_entry *e;

	if (ht == NULL || key == NULL || value == NULL)
		return false;

	e = malloc(sizeof(struct hash_entry));
	if (e == NULL) {
		LOG(("Not enough memory for hash entry."));
		return false;
	}

	h = hash_string_fnv(key, &(e->key_length));
	c = h % ht->nchains;

	v = strlen(value) ;
	e->pairing = malloc(v + e->key_length + 2);
	if (e->pairing == NULL) {
		LOG(("Not enough memory for string duplication."));
		free(e);
		return false;
	}
	memcpy(e->pairing, key, e->key_length + 1);
	memcpy(e->pairing + e->key_length + 1, value, v + 1);

	e->next = ht->chain[c];
	ht->chain[c] = e;

	return true;
}

/**
 * Looks up a the value associated with with a key from a specific hash table.
 *
 * \param  ht	  The hash table context to look up the key in.
 * \param  key	  The key to search for.
 * \return The value associated with the key, or NULL if it was not found.
 */

const char *hash_get(struct hash_table *ht, const char *key)
{
	unsigned int h, c, key_length;
	struct hash_entry *e;

	if (ht == NULL || key == NULL)
		return NULL;

	h = hash_string_fnv(key, &key_length);
	c = h % ht->nchains;

	for (e = ht->chain[c]; e; e = e->next)
		if ((key_length == e->key_length) &&
				(memcmp(key, e->pairing, key_length) == 0))
			return e->pairing + key_length + 1;

	return NULL;
}

/**
 * Iterate through all available hash keys.
 *
 * \param  ht	The hash table context to iterate.
 * \param  c1	Pointer to first context
 * \param  c2	Pointer to second context (set to 0 on first call)
 * \return The next hash key, or NULL for no more keys
 */

const char *hash_iterate(struct hash_table *ht, unsigned int *c1, unsigned int **c2) {
	struct hash_entry **he = (struct hash_entry **)c2;

	if (ht == NULL)
		return NULL;

	if (!*he)
		*c1 = -1;
	else
		*he = (*he)->next;

	if (*he)
		return (*he)->pairing;

	while (!*he) {
		(*c1)++;
		if (*c1 >= ht->nchains)
			return NULL;
		*he = ht->chain[*c1];
	}
	return (*he)->pairing;
}

/* A simple test rig.  To compile, use:
 * gcc -o hashtest -I../ -DTEST_RIG utils/hashtable.c
 *
 * If you make changes to this hash table implementation, please rerun this
 * test, and if possible, through valgrind to make sure there are no memory
 * leaks or invalid memory accesses.  If you add new functionality, please
 * include a test for it that has good coverage along side the other tests.
 */

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	struct hash_table *a, *b;
	FILE *dict;
	char keybuf[BUFSIZ], valbuf[BUFSIZ];
	int i;

	a = hash_create(79);
	assert(a != NULL);

	b = hash_create(103);
	assert(b != NULL);

	hash_add(a, "cow", "moo");
	hash_add(b, "moo", "cow");

	hash_add(a, "pig", "oink");
	hash_add(b, "oink", "pig");

	hash_add(a, "chicken", "cluck");
	hash_add(b, "cluck", "chicken");

	hash_add(a, "dog", "woof");
	hash_add(b, "woof", "dog");

	hash_add(a, "cat", "meow");
	hash_add(b, "meow", "cat");

#define MATCH(x,y) assert(!strcmp(hash_get(a, x), y)); assert(!strcmp(hash_get(b, y), x))
	MATCH("cow", "moo");
	MATCH("pig", "oink");
	MATCH("chicken", "cluck");
	MATCH("dog", "woof");
	MATCH("cat", "meow");

	hash_destroy(a);
	hash_destroy(b);

	/* this test requires /usr/share/dict/words - a large list of English
	 * words.  We load the entire file - odd lines are used as keys, and
	 * even lines are used as the values for the previous line.  we then
	 * work through it again making sure everything matches.
	 *
	 * We do this twice - once in a hash table with many chains, and once
	 * with a hash table with fewer chains.
	 */

	a = hash_create(1031);
	b = hash_create(7919);

	dict = fopen("/usr/share/dict/words", "r");
	if (dict == NULL) {
		fprintf(stderr, "Unable to open /usr/share/dict/words - extensive testing skipped.\n");
		exit(0);
	}

	while (!feof(dict)) {
		fscanf(dict, "%s", keybuf);
		fscanf(dict, "%s", valbuf);
		hash_add(a, keybuf, valbuf);
		hash_add(b, keybuf, valbuf);
	}

	for (i = 0; i < 5; i++) {
		fseek(dict, 0, SEEK_SET);

		while (!feof(dict)) {
			fscanf(dict, "%s", keybuf);
			fscanf(dict, "%s", valbuf);
			assert(strcmp(hash_get(a, keybuf), valbuf) == 0);
			assert(strcmp(hash_get(b, keybuf), valbuf) == 0);
		}
	}

	hash_destroy(a);
	hash_destroy(b);

	fclose(dict);

	return 0;
}

#endif
