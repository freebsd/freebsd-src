/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_entityrererence_h_
#define dom_internal_core_entityrererence_h_

#include <dom/core/exceptions.h>
#include <dom/core/entity_ref.h>

dom_exception _dom_entity_reference_create(dom_document *doc,
		dom_string *name, dom_string *value,
		dom_entity_reference **result);

void _dom_entity_reference_destroy(dom_entity_reference *entity);

#define _dom_entity_reference_initialise _dom_node_initialise
#define _dom_entity_reference_finalise	_dom_node_finalise

/* Following comes the protected vtable  */
void _dom_er_destroy(dom_node_internal *node);
dom_exception _dom_er_copy(dom_node_internal *old, dom_node_internal **copy);

#define DOM_ER_PROTECT_VTABLE \
	_dom_er_destroy, \
	_dom_er_copy

/* Helper functions */
dom_exception _dom_entity_reference_get_textual_representation(
		dom_entity_reference *entity,
		dom_string **result);

#endif
