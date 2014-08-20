/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_characterdata_h_
#define dom_internal_core_characterdata_h_

#include <dom/core/characterdata.h>

#include "core/node.h"

/**
 * DOM character data node
 */
struct dom_characterdata {
	struct dom_node_internal base;		/**< Base node */
};

/* The CharacterData is a intermediate node type, so the following function
 * may never be used */
dom_characterdata *_dom_characterdata_create(void);
dom_exception _dom_characterdata_initialise(struct dom_characterdata *cdata,
		struct dom_document *doc, dom_node_type type,
		dom_string *name, dom_string *value);

void _dom_characterdata_finalise(struct dom_characterdata *cdata);

/* The virtual functions for dom_characterdata */
dom_exception _dom_characterdata_get_data(struct dom_characterdata *cdata,
		dom_string **data);
dom_exception _dom_characterdata_set_data(struct dom_characterdata *cdata,
		dom_string *data);
dom_exception _dom_characterdata_get_length(struct dom_characterdata *cdata,
		uint32_t *length);
dom_exception _dom_characterdata_substring_data(
		struct dom_characterdata *cdata, uint32_t offset,
		uint32_t count, dom_string **data);
dom_exception _dom_characterdata_append_data(struct dom_characterdata *cdata,
		dom_string *data);
dom_exception _dom_characterdata_insert_data(struct dom_characterdata *cdata,
		uint32_t offset, dom_string *data);
dom_exception _dom_characterdata_delete_data(struct dom_characterdata *cdata,
		uint32_t offset, uint32_t count);
dom_exception _dom_characterdata_replace_data(struct dom_characterdata *cdata,
		uint32_t offset, uint32_t count,
		dom_string *data);
dom_exception _dom_characterdata_get_text_content(
		dom_node_internal *node,
		dom_string **result);
dom_exception _dom_characterdata_set_text_content(
		dom_node_internal *node,
		dom_string *content);

#define DOM_CHARACTERDATA_VTABLE \
	_dom_characterdata_get_data, \
	_dom_characterdata_set_data, \
	_dom_characterdata_get_length, \
	_dom_characterdata_substring_data, \
	_dom_characterdata_append_data, \
	_dom_characterdata_insert_data, \
	_dom_characterdata_delete_data, \
	_dom_characterdata_replace_data 

#define DOM_NODE_VTABLE_CHARACTERDATA \
	_dom_node_try_destroy, \
	_dom_node_get_node_name, \
	_dom_node_get_node_value, \
	_dom_node_set_node_value, \
	_dom_node_get_node_type, \
	_dom_node_get_parent_node, \
	_dom_node_get_child_nodes, \
	_dom_node_get_first_child, \
	_dom_node_get_last_child, \
	_dom_node_get_previous_sibling, \
	_dom_node_get_next_sibling, \
	_dom_node_get_attributes, \
	_dom_node_get_owner_document, \
	_dom_node_insert_before, \
	_dom_node_replace_child, \
	_dom_node_remove_child, \
	_dom_node_append_child, \
	_dom_node_has_child_nodes, \
	_dom_node_clone_node, \
	_dom_node_normalize, \
	_dom_node_is_supported, \
	_dom_node_get_namespace, \
	_dom_node_get_prefix, \
	_dom_node_set_prefix, \
	_dom_node_get_local_name, \
	_dom_node_has_attributes, \
	_dom_node_get_base, \
	_dom_node_compare_document_position, \
	_dom_characterdata_get_text_content, /* override */ \
	_dom_characterdata_set_text_content, /* override */ \
	_dom_node_is_same, \
	_dom_node_lookup_prefix, \
	_dom_node_is_default_namespace, \
	_dom_node_lookup_namespace, \
	_dom_node_is_equal, \
	_dom_node_get_feature, \
	_dom_node_set_user_data, \
	_dom_node_get_user_data

/* Following comes the protected vtable 
 *
 * Only the _copy function can be used by sub-class of this.
 */
void _dom_characterdata_destroy(dom_node_internal *node);
dom_exception _dom_characterdata_copy(dom_node_internal *old, 
		dom_node_internal **copy);

#define DOM_CHARACTERDATA_PROTECT_VTABLE \
	_dom_characterdata_destroy, \
	_dom_characterdata_copy

extern struct dom_characterdata_vtable characterdata_vtable;

dom_exception _dom_characterdata_copy_internal(dom_characterdata *old, 
		dom_characterdata *new);
#define dom_characterdata_copy_internal(o, n) \
		_dom_characterdata_copy_internal( \
		(dom_characterdata *) (o), (dom_characterdata *) (n))

#endif
