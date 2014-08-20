/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_cdatasection_h_
#define dom_internal_core_cdatasection_h_

#include <dom/core/exceptions.h>
#include <dom/core/cdatasection.h>
#include <dom/core/string.h>

struct dom_node_internal;
struct dom_document;

dom_exception _dom_cdata_section_create(struct dom_document *doc,
		dom_string *name, dom_string *value,
		dom_cdata_section **result);

void _dom_cdata_section_destroy(dom_cdata_section *cdata);

#define _dom_cdata_section_initialise 	_dom_text_initialise
#define _dom_cdata_section_finalise	_dom_text_finalise

/* Following comes the protected vtable  */
void __dom_cdata_section_destroy(struct dom_node_internal *node);
dom_exception _dom_cdata_section_copy(struct dom_node_internal *old, 
		struct dom_node_internal **copy);

#define DOM_CDATA_SECTION_PROTECT_VTABLE \
	__dom_cdata_section_destroy, \
	_dom_cdata_section_copy

#endif
