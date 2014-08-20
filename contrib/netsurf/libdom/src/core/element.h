/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_element_h_
#define dom_internal_core_element_h_

#include <stdbool.h>

#include <dom/core/element.h>

#include "core/node.h"

struct dom_document;
struct dom_element;
struct dom_namednodemap;
struct dom_node;
struct dom_attr;
struct dom_attr_list;
struct dom_type_info;
struct dom_hash_table;

/**
 * DOM element node
 */
struct dom_element {
	struct dom_node_internal base;		/**< Base node */

	struct dom_attr_list *attributes;	/**< Element attributes */

	dom_string *id_ns;	/**< The id attribute's namespace */

	dom_string *id_name; 	/**< The id attribute's name */

	struct dom_type_info *schema_type_info;	/**< Type information */

	lwc_string **classes;
	uint32_t n_classes;
};

dom_exception _dom_element_create(struct dom_document *doc,
		dom_string *name, dom_string *namespace,
		dom_string *prefix, struct dom_element **result);

dom_exception _dom_element_initialise(struct dom_document *doc,
		struct dom_element *el, dom_string *name, 
		dom_string *namespace, dom_string *prefix);

void _dom_element_finalise(struct dom_element *ele);

void _dom_element_destroy(struct dom_element *element);


/* The virtual functions of dom_element */
dom_exception _dom_element_get_tag_name(struct dom_element *element,
		dom_string **name);
dom_exception _dom_element_get_attribute(struct dom_element *element,
		dom_string *name, dom_string **value);
dom_exception _dom_element_set_attribute(struct dom_element *element,
		dom_string *name, dom_string *value);
dom_exception _dom_element_remove_attribute(struct dom_element *element, 
		dom_string *name);
dom_exception _dom_element_get_attribute_node(struct dom_element *element, 
		dom_string *name, struct dom_attr **result);
dom_exception _dom_element_set_attribute_node(struct dom_element *element, 
		struct dom_attr *attr, struct dom_attr **result);
dom_exception _dom_element_remove_attribute_node(struct dom_element *element, 
		struct dom_attr *attr, struct dom_attr **result);
dom_exception _dom_element_get_elements_by_tag_name(
		struct dom_element *element, dom_string *name,
		struct dom_nodelist **result);

dom_exception _dom_element_get_attribute_ns(struct dom_element *element, 
		dom_string *namespace, dom_string *localname, 
		dom_string **value);
dom_exception _dom_element_set_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *qname,
		dom_string *value);
dom_exception _dom_element_remove_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname);
dom_exception _dom_element_get_attribute_node_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname, 
		struct dom_attr **result);
dom_exception _dom_element_set_attribute_node_ns(struct dom_element *element, 
		struct dom_attr *attr, struct dom_attr **result);
dom_exception _dom_element_get_elements_by_tag_name_ns(
		struct dom_element *element, dom_string *namespace, 
		dom_string *localname, struct dom_nodelist **result);
dom_exception _dom_element_has_attribute(struct dom_element *element,
		dom_string *name, bool *result);
dom_exception _dom_element_has_attribute_ns(struct dom_element *element,
		dom_string *namespace, dom_string *localname, 
		bool *result);
dom_exception _dom_element_get_schema_type_info(struct dom_element *element, 
		struct dom_type_info **result);
dom_exception _dom_element_set_id_attribute(struct dom_element *element, 
		dom_string *name, bool is_id);
dom_exception _dom_element_set_id_attribute_ns(struct dom_element *element, 
		dom_string *namespace, dom_string *localname, 
		bool is_id);
dom_exception _dom_element_set_id_attribute_node(struct dom_element *element,
		struct dom_attr *id_attr, bool is_id);
dom_exception _dom_element_get_classes(struct dom_element *element,
		lwc_string ***classes, uint32_t *n_classes);
dom_exception _dom_element_has_class(struct dom_element *element,
		lwc_string *name, bool *match);

#define DOM_ELEMENT_VTABLE \
	_dom_element_get_tag_name, \
	_dom_element_get_attribute, \
	_dom_element_set_attribute, \
	_dom_element_remove_attribute, \
	_dom_element_get_attribute_node, \
	_dom_element_set_attribute_node, \
	_dom_element_remove_attribute_node, \
	_dom_element_get_elements_by_tag_name, \
	_dom_element_get_attribute_ns, \
	_dom_element_set_attribute_ns, \
	_dom_element_remove_attribute_ns, \
	_dom_element_get_attribute_node_ns, \
	_dom_element_set_attribute_node_ns, \
	_dom_element_get_elements_by_tag_name_ns, \
	_dom_element_has_attribute, \
	_dom_element_has_attribute_ns, \
	_dom_element_get_schema_type_info, \
	_dom_element_set_id_attribute, \
	_dom_element_set_id_attribute_ns, \
	_dom_element_set_id_attribute_node, \
	_dom_element_get_classes, \
	_dom_element_has_class

/* Overloading dom_node functions */
dom_exception _dom_element_get_attributes(dom_node_internal *node,
		struct dom_namednodemap **result);
dom_exception _dom_element_has_attributes(dom_node_internal *node,
		bool *result);
dom_exception _dom_element_normalize(dom_node_internal *node);
dom_exception _dom_element_lookup_prefix(dom_node_internal *node,
		dom_string *namespace, dom_string **result);
dom_exception _dom_element_is_default_namespace(dom_node_internal *node,
		dom_string *namespace, bool *result);
dom_exception _dom_element_lookup_namespace(dom_node_internal *node,
		dom_string *prefix, dom_string **result);
#define DOM_NODE_VTABLE_ELEMENT \
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
	_dom_element_get_attributes, /*overload*/\
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
	_dom_element_has_attributes, /*overload*/\
	_dom_node_get_base, \
	_dom_node_compare_document_position, \
	_dom_node_get_text_content, \
	_dom_node_set_text_content, \
	_dom_node_is_same, \
	_dom_element_lookup_prefix, /*overload*/\
	_dom_element_is_default_namespace, /*overload*/\
	_dom_element_lookup_namespace, /*overload*/\
	_dom_node_is_equal, \
	_dom_node_get_feature, \
	_dom_node_set_user_data, \
	_dom_node_get_user_data

/**
 * The internal used vtable for element
 */
struct dom_element_protected_vtable {
	struct dom_node_protect_vtable base;

	dom_exception (*dom_element_parse_attribute)(dom_element *ele,
			dom_string *name, dom_string *value,
			dom_string **parsed);
			/**< Called by dom_attr_set_value, and used to check
			 *   whether the new attribute value is valid and 
			 *   return a valid on if it is not
			 */
};

typedef struct dom_element_protected_vtable dom_element_protected_vtable;

/* Parse the attribute's value */
static inline dom_exception dom_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed)
{
	struct dom_node_internal *node = (struct dom_node_internal *) ele;
	return ((dom_element_protected_vtable *) node->vtable)->
			dom_element_parse_attribute(ele, name, value, parsed);
}
#define dom_element_parse_attribute(e, n, v, p) dom_element_parse_attribute( \
		(dom_element *) (e), (dom_string *) (n), \
		(dom_string *) (v), (dom_string **) (p))


/* The protected virtual function */
dom_exception _dom_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed);
void __dom_element_destroy(dom_node_internal *node);
dom_exception _dom_element_copy(dom_node_internal *old, 
		dom_node_internal **copy);

#define DOM_ELEMENT_PROTECT_VTABLE \
	_dom_element_parse_attribute

#define DOM_NODE_PROTECT_VTABLE_ELEMENT \
	__dom_element_destroy, \
	_dom_element_copy

/* Helper functions*/
dom_exception _dom_element_get_id(struct dom_element *ele, dom_string **id);

extern struct dom_element_vtable _dom_element_vtable;

#endif
