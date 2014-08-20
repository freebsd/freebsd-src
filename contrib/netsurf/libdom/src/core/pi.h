/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_processinginstruction_h_
#define dom_internal_core_processinginstruction_h_

#include <dom/core/exceptions.h>
#include <dom/core/pi.h>

dom_exception _dom_processing_instruction_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_processing_instruction **result);

void _dom_processing_instruction_destroy(dom_processing_instruction *pi);

#define _dom_processing_instruction_initialise	_dom_node_initialise
#define _dom_processing_instruction_finalise 	_dom_node_finalise

/* Following comes the protected vtable  */
void _dom_pi_destroy(dom_node_internal *node);
dom_exception _dom_pi_copy(dom_node_internal *old, dom_node_internal **copy);

#define DOM_PI_PROTECT_VTABLE \
	_dom_pi_destroy, \
	_dom_pi_copy

#endif
