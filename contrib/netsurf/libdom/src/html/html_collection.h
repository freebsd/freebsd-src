/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_html_collection_h_
#define dom_internal_html_collection_h_

#include <dom/html/html_collection.h>

struct dom_node_internal;

typedef bool (*dom_callback_is_in_collection)(
	struct dom_node_internal *node, void *ctx);

/**
 * The html_collection structure
 */
struct dom_html_collection {
	dom_callback_is_in_collection ic;
			/**< The function pointer used to test
			 * whether some node is an element of
			 * this collection
			 */
	void *ctx; /**< Context for the callback */
	struct dom_html_document *doc;	/**< The document created this
					 * collection
					 */
	struct dom_node_internal *root;
			/**< The root node of this collection */
	uint32_t refcnt;
			/**< Reference counting */
};

dom_exception _dom_html_collection_create(struct dom_html_document *doc,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic,
		void *ctx,
		struct dom_html_collection **col);

dom_exception _dom_html_collection_initialise(struct dom_html_document *doc,
		struct dom_html_collection *col,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic, void *ctx);

void _dom_html_collection_finalise(struct dom_html_collection *col);

void _dom_html_collection_destroy(struct dom_html_collection *col);

#endif

