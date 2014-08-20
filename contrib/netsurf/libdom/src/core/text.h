/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_text_h_
#define dom_internal_core_text_h_

#include <stdbool.h>

#include <dom/core/exceptions.h>
#include <dom/core/text.h>

#include "core/characterdata.h"

struct dom_document;

/**
 * A DOM text node
 */
struct dom_text {
	dom_characterdata base;	/**< Base node */

	bool element_content_whitespace;	/**< This node is element
						 * content whitespace */
};

/* Constructor and Destructor */
dom_exception _dom_text_create(struct dom_document *doc,
		dom_string *name, dom_string *value,
		dom_text **result);

void _dom_text_destroy(dom_text *text);

dom_exception _dom_text_initialise(dom_text *text,
		struct dom_document *doc, dom_node_type type,
		dom_string *name, dom_string *value);

void _dom_text_finalise(dom_text *text);


/* Virtual functions for dom_text */
dom_exception _dom_text_split_text(dom_text *text,
		uint32_t offset, dom_text **result);
dom_exception _dom_text_get_is_element_content_whitespace(
		dom_text *text, bool *result);
dom_exception _dom_text_get_whole_text(dom_text *text,
		dom_string **result);
dom_exception _dom_text_replace_whole_text(dom_text *text,
		dom_string *content, dom_text **result);

#define DOM_TEXT_VTABLE \
	_dom_text_split_text, \
	_dom_text_get_is_element_content_whitespace, \
	_dom_text_get_whole_text, \
	_dom_text_replace_whole_text


/* Following comes the protected vtable  */
void __dom_text_destroy(struct dom_node_internal *node);
dom_exception _dom_text_copy(dom_node_internal *old, dom_node_internal **copy);

#define DOM_TEXT_PROTECT_VTABLE \
	__dom_text_destroy, \
	_dom_text_copy

extern struct dom_text_vtable text_vtable;

dom_exception _dom_text_copy_internal(dom_text *old, dom_text *new);
#define dom_text_copy_internal(o, n) \
		_dom_text_copy_internal((dom_text *) (o), (dom_text *) (n))

#endif
