/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_html_options_collection_h_
#define dom_internal_html_options_collection_h_

#include <dom/html/html_options_collection.h>

#include "html/html_collection.h"

struct dom_node_internal;

/**
 * The html_options_collection structure
 */
struct dom_html_options_collection {
	struct dom_html_collection base;
			/**< The base class */
};

dom_exception _dom_html_options_collection_create(struct dom_html_document *doc,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic,
		void *ctx,
		struct dom_html_options_collection **col);

dom_exception _dom_html_options_collection_initialise(struct dom_html_document *doc,
		struct dom_html_options_collection *col,
		struct dom_node_internal *root,
		dom_callback_is_in_collection ic, void *ctx);

void _dom_html_options_collection_finalise(
		struct dom_html_options_collection *col);

void _dom_html_options_collection_destroy(
		struct dom_html_options_collection *col);

#endif

