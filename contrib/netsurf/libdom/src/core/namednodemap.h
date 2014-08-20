/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_namednodemap_h_
#define dom_internal_core_namednodemap_h_

#include <stdbool.h>

#include <dom/core/namednodemap.h>
#include <dom/core/node.h>

struct dom_document;
struct dom_node;
struct dom_namednodemap;

struct nnm_operation {
	dom_exception (*namednodemap_get_length)(void *priv,
			uint32_t *length);

	dom_exception (*namednodemap_get_named_item)(void *priv,
			dom_string *name, struct dom_node **node);

	dom_exception (*namednodemap_set_named_item)(void *priv,
			struct dom_node *arg, struct dom_node **node);

	dom_exception (*namednodemap_remove_named_item)(
			void *priv, dom_string *name,
			struct dom_node **node);

	dom_exception (*namednodemap_item)(void *priv,
			uint32_t index, struct dom_node **node);

	dom_exception (*namednodemap_get_named_item_ns)(
			void *priv, dom_string *namespace,
			dom_string *localname, struct dom_node **node);

	dom_exception (*namednodemap_set_named_item_ns)(
			void *priv, struct dom_node *arg,
			struct dom_node **node);

	dom_exception (*namednodemap_remove_named_item_ns)(
			void *priv, dom_string *namespace,
			dom_string *localname, struct dom_node **node);

	void (*namednodemap_destroy)(void *priv);

	bool (*namednodemap_equal)(void *p1, void *p2);
};

/* Create a namednodemap */
dom_exception _dom_namednodemap_create(struct dom_document *doc,
		void *priv, struct nnm_operation *opt,
		struct dom_namednodemap **map);

/* Update the private data */
void _dom_namednodemap_update(struct dom_namednodemap *map, void *priv);

/* Test whether two maps are equal */
bool _dom_namednodemap_equal(struct dom_namednodemap *m1, 
		struct dom_namednodemap *m2);

#define dom_namednodemap_equal(m1, m2) _dom_namednodemap_equal( \
		(struct dom_namednodemap *) (m1), \
		(struct dom_namednodemap *) (m2))

#endif
