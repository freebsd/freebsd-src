/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_html_select_element_h_
#define dom_internal_html_select_element_h_

#include <dom/html/html_select_element.h>

#include "html/html_element.h"
#include "html/html_options_collection.h"

struct dom_html_select_element {
	struct dom_html_element base;
			/**< The base class */
	int32_t selected;
			/**< The selected element's index */
	dom_html_form_element *form;
			/**< The form associated with select */
};

/* Create a dom_html_select_element object */
dom_exception _dom_html_select_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_select_element **ele);

/* Initialise a dom_html_select_element object */
dom_exception _dom_html_select_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_select_element *ele);

/* Finalise a dom_html_select_element object */
void _dom_html_select_element_finalise(struct dom_html_select_element *ele);

/* Destroy a dom_html_select_element object */
void _dom_html_select_element_destroy(struct dom_html_select_element *ele);

/* The protected virtual functions */
dom_exception _dom_html_select_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed);
void _dom_virtual_html_select_element_destroy(dom_node_internal *node);
dom_exception _dom_html_select_element_copy(dom_node_internal *old,
		dom_node_internal **copy);

#define DOM_HTML_SELECT_ELEMENT_PROTECT_VTABLE \
	_dom_html_select_element_parse_attribute

#define DOM_NODE_PROTECT_VTABLE_HTML_SELECT_ELEMENT \
	_dom_virtual_html_select_element_destroy, \
	_dom_html_select_element_copy

/* Internal function for bindings */

dom_exception _dom_html_select_element_set_form(
	dom_html_select_element *select, dom_html_form_element *form);

#endif

