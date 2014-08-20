/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_documentfragment_h_
#define dom_internal_core_documentfragment_h_

#include <dom/core/exceptions.h>
#include <dom/core/doc_fragment.h>

dom_exception _dom_document_fragment_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_document_fragment **result);

void _dom_document_fragment_destroy(dom_document_fragment *frag);

#define _dom_document_fragment_initialise	_dom_node_initialise
#define _dom_document_fragment_finalise		_dom_node_finalise


/* Following comes the protected vtable */
void _dom_df_destroy(dom_node_internal *node);
dom_exception _dom_df_copy(dom_node_internal *old, dom_node_internal **copy);

#define DOM_DF_PROTECT_VTABLE \
	_dom_df_destroy, \
	_dom_df_copy

#endif
