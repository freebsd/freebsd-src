/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_comment_h_
#define dom_internal_core_comment_h_

#include <dom/core/exceptions.h>
#include <dom/core/comment.h>

struct dom_comment;
struct dom_document;

dom_exception _dom_comment_create(struct dom_document *doc,
		dom_string *name, dom_string *value,
		dom_comment **result);

#define  _dom_comment_initialise _dom_characterdata_initialise
#define  _dom_comment_finalise _dom_characterdata_finalise

void _dom_comment_destroy(dom_comment *comment);

/* Following comes the protected vtable  */
void __dom_comment_destroy(dom_node_internal *node);
dom_exception _dom_comment_copy(dom_node_internal *old, 
		dom_node_internal **copy);

#define DOM_COMMENT_PROTECT_VTABLE \
	__dom_comment_destroy, \
	_dom_comment_copy

#endif
