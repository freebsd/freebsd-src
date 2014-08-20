/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_document_type_h_
#define dom_internal_core_document_type_h_

#include <dom/core/document_type.h>

struct dom_namednodemap;

/* Create a DOM document type */
dom_exception _dom_document_type_create(dom_string *qname,
		dom_string *public_id, 
		dom_string *system_id,
		dom_document_type **doctype);
/* Destroy a document type */
void _dom_document_type_destroy(dom_node_internal *doctypenode);
dom_exception _dom_document_type_initialise(dom_document_type *doctype,
		dom_string *qname, dom_string *public_id,
		dom_string *system_id);
void _dom_document_type_finalise(dom_document_type *doctype);

/* The virtual functions of DocumentType */
dom_exception _dom_document_type_get_name(dom_document_type *doc_type,
		dom_string **result);
dom_exception _dom_document_type_get_entities(
		dom_document_type *doc_type,
		struct dom_namednodemap **result);
dom_exception _dom_document_type_get_notations(
		dom_document_type *doc_type,
		struct dom_namednodemap **result);
dom_exception _dom_document_type_get_public_id(
		dom_document_type *doc_type,
		dom_string **result);
dom_exception _dom_document_type_get_system_id(
		dom_document_type *doc_type,
		dom_string **result);
dom_exception _dom_document_type_get_internal_subset(
		dom_document_type *doc_type,
		dom_string **result);

dom_exception _dom_document_type_get_text_content(dom_node_internal *node,
                                                  dom_string **result);
dom_exception _dom_document_type_set_text_content(dom_node_internal *node,
                                                  dom_string *content);

#define DOM_DOCUMENT_TYPE_VTABLE \
	_dom_document_type_get_name, \
	_dom_document_type_get_entities, \
	_dom_document_type_get_notations, \
	_dom_document_type_get_public_id, \
	_dom_document_type_get_system_id, \
	_dom_document_type_get_internal_subset

#define DOM_NODE_VTABLE_DOCUMENT_TYPE \
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
	_dom_document_type_get_text_content, \
	_dom_document_type_set_text_content, \
	_dom_node_is_same, \
	_dom_node_lookup_prefix, \
	_dom_node_is_default_namespace, \
	_dom_node_lookup_namespace, \
	_dom_node_is_equal, \
	_dom_node_get_feature, \
	_dom_node_set_user_data, \
	_dom_node_get_user_data

/* Following comes the protected vtable  */
void _dom_dt_destroy(dom_node_internal *node);
dom_exception _dom_dt_copy(dom_node_internal *old, dom_node_internal **copy);

#define DOM_DT_PROTECT_VTABLE \
	_dom_dt_destroy, \
	_dom_dt_copy

#endif
