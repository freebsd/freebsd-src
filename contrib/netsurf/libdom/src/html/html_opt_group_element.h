/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef dom_internal_html_opt_group_element_h_
#define dom_internal_html_opt_group_element_h_

#include <dom/html/html_opt_group_element.h>

#include "html/html_element.h"

struct dom_html_opt_group_element {
	struct dom_html_element base;
			/**< The base class */
};

/* Create a dom_html_opt_group_element object */
dom_exception _dom_html_opt_group_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_opt_group_element **ele);

/* Initialise a dom_html_opt_group_element object */
dom_exception _dom_html_opt_group_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_opt_group_element *ele);

/* Finalise a dom_html_opt_group_element object */
void _dom_html_opt_group_element_finalise(struct dom_html_opt_group_element *ele);

/* Destroy a dom_html_opt_group_element object */
void _dom_html_opt_group_element_destroy(struct dom_html_opt_group_element *ele);

/* The protected virtual functions */
dom_exception _dom_html_opt_group_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed);
void _dom_virtual_html_opt_group_element_destroy(dom_node_internal *node);
dom_exception _dom_html_opt_group_element_copy(dom_node_internal *old,
		dom_node_internal **copy);

#define DOM_HTML_OPT_GROUP_ELEMENT_PROTECT_VTABLE \
	_dom_html_opt_group_element_parse_attribute

#define DOM_NODE_PROTECT_VTABLE_HTML_OPT_GROUP_ELEMENT \
	_dom_virtual_html_opt_group_element_destroy, \
	_dom_html_opt_group_element_copy

#endif
