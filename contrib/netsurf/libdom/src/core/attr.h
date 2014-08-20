/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_attr_h_
#define dom_internal_core_attr_h_

#include <dom/core/attr.h>

struct dom_document;
struct dom_type_info;

dom_exception _dom_attr_create(struct dom_document *doc,
		dom_string *name, dom_string *namespace,
		dom_string *prefix, bool specified,
		struct dom_attr **result);
void _dom_attr_destroy(struct dom_attr *attr);
dom_exception _dom_attr_initialise(struct dom_attr *a,
		struct dom_document *doc, dom_string *name,
		dom_string *namespace, dom_string *prefix,
		bool specified, struct dom_attr **result);
void _dom_attr_finalise(struct dom_attr *attr);

/* Virtual functions for dom_attr */
dom_exception _dom_attr_get_name(struct dom_attr *attr,
				dom_string **result);
dom_exception _dom_attr_get_specified(struct dom_attr *attr, bool *result);
dom_exception _dom_attr_get_value(struct dom_attr *attr,
				dom_string **result);
dom_exception _dom_attr_set_value(struct dom_attr *attr,
				dom_string *value);
dom_exception _dom_attr_get_owner(struct dom_attr *attr,
				struct dom_element **result);
dom_exception _dom_attr_get_schema_type_info(struct dom_attr *attr,
				struct dom_type_info **result);
dom_exception _dom_attr_is_id(struct dom_attr *attr, bool *result);

#define DOM_ATTR_VTABLE 	\
	_dom_attr_get_name, \
	_dom_attr_get_specified, \
	_dom_attr_get_value, \
	_dom_attr_set_value, \
	_dom_attr_get_owner, \
	_dom_attr_get_schema_type_info, \
	_dom_attr_is_id

/* Overloading dom_node functions */
dom_exception _dom_attr_get_node_value(dom_node_internal *node,
		dom_string **result);
dom_exception _dom_attr_clone_node(dom_node_internal *node, bool deep,
		dom_node_internal **result);
dom_exception _dom_attr_set_prefix(dom_node_internal *node,
		dom_string *prefix);
dom_exception _dom_attr_normalize(dom_node_internal *node);
dom_exception _dom_attr_lookup_prefix(dom_node_internal *node,
		dom_string *namespace, dom_string **result);
dom_exception _dom_attr_is_default_namespace(dom_node_internal *node,
		dom_string *namespace, bool *result);
dom_exception _dom_attr_lookup_namespace(dom_node_internal *node,
		dom_string *prefix, dom_string **result);
#define DOM_NODE_VTABLE_ATTR \
	_dom_node_try_destroy, \
	_dom_node_get_node_name, \
	_dom_attr_get_node_value, /*overload*/\
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
	_dom_attr_clone_node, /*overload*/\
	_dom_node_normalize, \
	_dom_node_is_supported, \
	_dom_node_get_namespace, \
	_dom_node_get_prefix, \
	_dom_attr_set_prefix, /*overload*/\
	_dom_node_get_local_name, \
	_dom_node_has_attributes, \
	_dom_node_get_base, \
	_dom_node_compare_document_position, \
	_dom_node_get_text_content, \
	_dom_node_set_text_content, \
	_dom_node_is_same, \
	_dom_attr_lookup_prefix, /*overload*/\
	_dom_attr_is_default_namespace, /*overload*/\
	_dom_attr_lookup_namespace, /*overload*/\
	_dom_node_is_equal, \
	_dom_node_get_feature, \
	_dom_node_set_user_data, \
	_dom_node_get_user_data

/* The protected virtual functions */
void __dom_attr_destroy(dom_node_internal *node);
dom_exception _dom_attr_copy(dom_node_internal *old,
		dom_node_internal **copy);

#define DOM_ATTR_PROTECT_VTABLE \
	__dom_attr_destroy, \
	_dom_attr_copy


void _dom_attr_set_isid(struct dom_attr *attr, bool is_id);
void _dom_attr_set_specified(struct dom_attr *attr, bool specified);
bool _dom_attr_readonly(const dom_attr *a);

#endif
