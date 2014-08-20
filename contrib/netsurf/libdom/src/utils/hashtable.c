/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *			http://www.opensource.org/licenses/mit-license.php
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#ifdef TEST_RIG
#include <stdio.h>
#endif
#include "utils/hashtable.h"

#include <libwapcaplet/libwapcaplet.h>

/* The hash table entry */
struct _dom_hash_entry {
	void *key;			/**< The key pointer */
	void *value;			/**< The value pointer */
	struct _dom_hash_entry *next;	/**< Next entry */
};

/* The hash table */
struct dom_hash_table {
	const dom_hash_vtable *vtable;	/**< Vtable */
	void *pw;			/**< Client data */
	unsigned int nchains;		/**< Number of chains */
	struct _dom_hash_entry **chain;	/**< The chain head */
	uint32_t nentries;		/**< The entries in this table */
};


/**
 * Create a new hash table, and return a context for it.  The memory consumption
 * of a hash table is approximately 8 + (nchains * 12) bytes if it is empty.
 *
 * \param chains  Number of chains/buckets this hash table will have.  This
 *                should be a prime number, and ideally a prime number just
 *                over a power of two, for best performance and distribution
 * \param vtable  Client vtable
 * \param pw      Client private data
 * \return struct dom_hash_table containing the context of this hash table or
 *         NULL if there is insufficent memory to create it and its chains.
 */
dom_hash_table *_dom_hash_create(unsigned int chains, 
		const dom_hash_vtable *vtable, void *pw)
{
	dom_hash_table *r = malloc(sizeof(struct dom_hash_table));
	if (r == NULL) {
		return NULL;
	}

	r->vtable = vtable;
	r->pw = pw;
	r->nentries = 0;
	r->nchains = chains;
	r->chain = calloc(chains, sizeof(struct _dom_hash_entry *));
	if (r->chain == NULL) {
		free(r);
		return NULL;
	}

	return r;
}

/**
 * Clone a hash table.
 *
 * \param ht        Hash table to clone.
 *
 * \return The cloned hash table.
 */
dom_hash_table *_dom_hash_clone(dom_hash_table *ht)
{
	void *key = NULL, *nkey = NULL;
	void *value = NULL, *nvalue = NULL;
	uintptr_t c1, *c2 = NULL;
	struct dom_hash_table *ret;
	
	ret = _dom_hash_create(ht->nchains, ht->vtable, ht->pw);
	if (ret == NULL)
		return NULL;

	while ( (key = _dom_hash_iterate(ht, &c1, &c2)) != NULL) {
		nkey = ht->vtable->clone_key(key, ht->pw);
		if (nkey == NULL) {
			_dom_hash_destroy(ret);
			return NULL;
		}

		value = _dom_hash_get(ht, key);
		nvalue = ht->vtable->clone_value(value, ht->pw);
		if (nvalue == NULL) {
			ht->vtable->destroy_key(nkey, ht->pw);
			_dom_hash_destroy(ret);
			return NULL;
		}

		if (_dom_hash_add(ret, nkey, nvalue, false) == false) {
			_dom_hash_destroy(ret);
			return NULL;
		}
	}

	return ret;
}

/**
 * Destroys a hash table, freeing all memory associated with it.
 *
 * \param ht        Hash table to destroy. After the function returns, this
 *                  will nolonger be valid
 */
void _dom_hash_destroy(dom_hash_table *ht) 
{
	unsigned int i;

	if (ht == NULL)
		return;

	for (i = 0; i < ht->nchains; i++) {
		if (ht->chain[i] != NULL) {
			struct _dom_hash_entry *e = ht->chain[i];
			while (e) {
				struct _dom_hash_entry *n = e->next;
				ht->vtable->destroy_key(e->key, ht->pw);
				ht->vtable->destroy_value(e->value, ht->pw);
				free(e);
				e = n;
			}
		}
	}

	free(ht->chain);
	free(ht);
}

/**
 * Adds a key/value pair to a hash table
 *
 * \param  ht     The hash table context to add the key/value pair to.
 * \param  key    The key to associate the value with.
 * \param  value  The value to associate the key with.
 * \return true if the add succeeded, false otherwise.  (Failure most likely
 *         indicates insufficent memory to make copies of the key and value.
 */
bool _dom_hash_add(dom_hash_table *ht, void *key, void *value, 
		bool replace)
{
	unsigned int h, c;
	struct _dom_hash_entry *e;

	if (ht == NULL || key == NULL || value == NULL)
		return false;

	h = ht->vtable->hash(key, ht->pw);
	c = h % ht->nchains;

	for (e = ht->chain[c]; e; e = e->next) {
		if (ht->vtable->key_isequal(key, e->key, ht->pw)) {
			if (replace == true) {
				e->value = value;
				return true;
			} else {
				return false;
			}
		}
	}

	e = malloc(sizeof(struct _dom_hash_entry));
	if (e == NULL) {
		return false;
	}

	e->key = key;
	e->value = value;

	e->next = ht->chain[c];
	ht->chain[c] = e;
	ht->nentries++;

	return true;
}

/**
 * Looks up a the value associated with with a key from a specific hash table.
 *
 * \param  ht   The hash table context to look up
 * \param  key  The key to search for
 * \return The value associated with the key, or NULL if it was not found.
 */
void *_dom_hash_get(struct dom_hash_table *ht, void *key)
{
	unsigned int h, c;
	struct _dom_hash_entry *e;

	if (ht == NULL || key == NULL)
		return NULL;

	h = ht->vtable->hash(key, ht->pw);
	c = h % ht->nchains;

	for (e = ht->chain[c]; e; e = e->next) {
		if (ht->vtable->key_isequal(key, e->key, ht->pw))
			return e->value;
	}

	return NULL;
}

/**
 * Delete the key from the hashtable.
 *
 * \param ht   The hashtable object
 * \param key  The key to delete
 * \return The deleted value
 */
void *_dom_hash_del(struct dom_hash_table *ht, void *key)
{
	unsigned int h, c;
	struct _dom_hash_entry *e, *p;
	void *ret;

	if (ht == NULL || key == NULL)
		return NULL;

	h = ht->vtable->hash(key, ht->pw);
	c = h % ht->nchains;

	p = ht->chain[c];
	for (e = p; e; p = e, e = e->next) {
		if (ht->vtable->key_isequal(key, e->key, ht->pw)) {
			if (p != e) {
				p->next = e->next;
			} else {
				/* The first item in this chain is target*/
				ht->chain[c] = e->next;
			}

			ret = e->value;
			free(e);
			ht->nentries--;
			return ret;
		}
	}
	
	return NULL;
}

/**
 * Iterate through all available hash keys.
 *
 * \param  ht  The hash table context to iterate.
 * \param  c1  Pointer to first context
 * \param  c2  Pointer to second context (set to 0 on first call)
 * \return The next hash key, or NULL for no more keys
 */
void *_dom_hash_iterate(struct dom_hash_table *ht, uintptr_t *c1,
		uintptr_t **c2)
{
	struct _dom_hash_entry **he = (struct _dom_hash_entry **) c2;

	if (ht == NULL)
		return NULL;

	if (!*he)
		*c1 = -1;
	else
		*he = (*he)->next;

	if (*he)
		return (*he)->key;

	while (!*he) {
		(*c1)++;
		if (*c1 >= ht->nchains)
			return NULL;
		*he = ht->chain[*c1];
	}
	return (*he)->key;
}

/**
 * Get the number of elements in this hash table 
 *
 * \param ht  The hash table
 * 
 * \return the number of elements
 */
uint32_t _dom_hash_get_length(struct dom_hash_table *ht)
{
	return ht->nentries;
}

/*-----------------------------------------------------------------------*/

/* A simple test rig.  To compile, use:
 * gcc -g  -o hashtest -I../ -I../../include  -DTEST_RIG  hashtable.c
 *
 * If you make changes to this hash table implementation, please rerun this
 * test, and if possible, through valgrind to make sure there are no memory
 * leaks or invalid memory accesses.  If you add new functionality, please
 * include a test for it that has good coverage along side the other tests.
 */

#ifdef TEST_RIG


/**
 * Hash a pointer, returning a 32bit value.  
 *
 * \param  ptr  The pointer to hash.
 * \return the calculated hash value for the pointer.
 */

static inline unsigned int _dom_hash_pointer_fnv(void *ptr)
{
	return (unsigned int) ptr;
}

static void *test_alloc(void *p, size_t size, void *ptr)
{
	if (p != NULL) {
		free(p);
		return NULL;
	}

	if (p == NULL) {
		return malloc(size);
	}
}

int main(int argc, char *argv[])
{
	struct dom_hash_table *a, *b;
	FILE *dict;
	char keybuf[BUFSIZ], valbuf[BUFSIZ];
	int i;
	char *cow="cow", *moo="moo", *pig="pig", *oink="oink",
			*chicken="chikcken", *cluck="cluck",
			*dog="dog", *woof="woof", *cat="cat", 
			*meow="meow";
	void *ret;

	a = _dom_hash_create(79, _dom_hash_pointer_fnv, test_alloc, NULL);
	assert(a != NULL);

	b = _dom_hash_create(103, _dom_hash_pointer_fnv, test_alloc, NULL);
	assert(b != NULL);

	_dom_hash_add(a, cow, moo ,true);
	_dom_hash_add(b, moo, cow ,true);

	_dom_hash_add(a, pig, oink ,true);
	_dom_hash_add(b, oink, pig ,true);

	_dom_hash_add(a, chicken, cluck ,true);
	_dom_hash_add(b, cluck, chicken ,true);

	_dom_hash_add(a, dog, woof ,true);
	_dom_hash_add(b, woof, dog ,true);

	_dom_hash_add(a, cat, meow ,true);
	_dom_hash_add(b, meow, cat ,true);

#define MATCH(x,y) assert(!strcmp((char *)hash_get(a, x), (char *)y)); \
		assert(!strcmp((char *)hash_get(b, y), (char *)x))
	MATCH(cow, moo);
	MATCH(pig, oink);
	MATCH(chicken, cluck);
	MATCH(dog, woof);
	MATCH(cat, meow);

	assert(hash_get_length(a) == 5);
	assert(hash_get_length(b) == 5);

	_dom_hash_del(a, cat);
	_dom_hash_del(b, meow);
	assert(hash_get(a, cat) == NULL);
	assert(hash_get(b, meow) == NULL);

	assert(hash_get_length(a) == 4);
	assert(hash_get_length(b) == 4);

	_dom_hash_destroy(a, NULL, NULL);
	_dom_hash_destroy(b, NULL, NULL);

	/* This test requires /usr/share/dict/words - a large list of English
	 * words.  We load the entire file - odd lines are used as keys, and
	 * even lines are used as the values for the previous line.  we then
	 * work through it again making sure everything matches.
	 *
	 * We do this twice - once in a hash table with many chains, and once
	 * with a hash table with fewer chains.
	 */

	a = _dom_hash_create(1031, _dom_hash_pointer_fnv, test_alloc, NULL);
	b = _dom_hash_create(7919, _dom_hash_pointer_fnv, test_alloc, NULL);

	dict = fopen("/usr/share/dict/words", "r");
	if (dict == NULL) {
		fprintf(stderr, "Unable to open /usr/share/dict/words - \
				extensive testing skipped.\n");
		exit(0);
	}

	while (!feof(dict)) {
		fscanf(dict, "%s", keybuf);
		fscanf(dict, "%s", valbuf);
		_dom_hash_add(a, keybuf, valbuf, true);
		_dom_hash_add(b, keybuf, valbuf, true);
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

	_dom_hash_destroy(a, NULL, NULL);
	_dom_hash_destroy(b, NULL, NULL);

	fclose(dict);

	return 0;
}

#endif
