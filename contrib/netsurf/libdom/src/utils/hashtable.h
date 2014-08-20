/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_utils_hashtable_h_
#define dom_utils_hashtable_h_

#include <stdbool.h>
#include <dom/functypes.h>

typedef struct dom_hash_table dom_hash_table;

typedef struct dom_hash_vtable {
	uint32_t (*hash)(void *key, void *pw);
	void *(*clone_key)(void *key, void *pw);
	void (*destroy_key)(void *key, void *pw);
	void *(*clone_value)(void *value, void *pw);
	void (*destroy_value)(void *value, void *pw);
	bool (*key_isequal)(void *key1, void *key2, void *pw);
} dom_hash_vtable;

dom_hash_table *_dom_hash_create(unsigned int chains, 
		const dom_hash_vtable *vtable, void *pw);
dom_hash_table *_dom_hash_clone(dom_hash_table *ht);
void _dom_hash_destroy(dom_hash_table *ht);
bool _dom_hash_add(dom_hash_table *ht, void *key, void *value, 
		bool replace);
void *_dom_hash_get(dom_hash_table *ht, void *key);
void *_dom_hash_del(dom_hash_table *ht, void *key);
void *_dom_hash_iterate(dom_hash_table *ht, uintptr_t *c1, uintptr_t **c2);
uint32_t _dom_hash_get_length(dom_hash_table *ht);

#endif
