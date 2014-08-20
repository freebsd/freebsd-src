/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_nodelist_h_
#define dom_internal_core_nodelist_h_

#include <stdbool.h>

#include <dom/core/nodelist.h>

struct dom_document;
struct dom_node;
struct dom_nodelist;

/**
 * The NodeList type
 */
typedef enum { 
	DOM_NODELIST_CHILDREN,
	DOM_NODELIST_BY_NAME,
	DOM_NODELIST_BY_NAMESPACE,
	DOM_NODELIST_BY_NAME_CASELESS,
	DOM_NODELIST_BY_NAMESPACE_CASELESS
} nodelist_type;

/* Create a nodelist */
dom_exception _dom_nodelist_create(struct dom_document *doc, nodelist_type type,
		struct dom_node_internal *root, dom_string *tagname,
		dom_string *namespace, dom_string *localname,
		struct dom_nodelist **list);

/* Match a nodelist instance against a set of nodelist creation parameters */
bool _dom_nodelist_match(struct dom_nodelist *list, nodelist_type type,
		struct dom_node_internal *root, dom_string *tagname, 
		dom_string *namespace, dom_string *localname);

bool _dom_nodelist_equal(struct dom_nodelist *l1, struct dom_nodelist *l2);
#define dom_nodelist_equal(l1, l2) _dom_nodelist_equal( \
		(struct dom_nodelist *) (l1), (struct dom_nodelist *) (l2))

#endif
