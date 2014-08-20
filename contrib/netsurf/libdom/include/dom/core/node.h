/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_node_h_
#define dom_core_node_h_

#include <inttypes.h>
#include <stdbool.h>

#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/events/event_target.h>

struct dom_document;
struct dom_nodelist;
struct dom_namednodemap;
struct dom_node;

/**
 * Bits defining position of a node in a document relative to some other node
 */
typedef enum {
	DOM_DOCUMENT_POSITION_DISCONNECTED		= 0x01,
	DOM_DOCUMENT_POSITION_PRECEDING			= 0x02,
	DOM_DOCUMENT_POSITION_FOLLOWING			= 0x04,
	DOM_DOCUMENT_POSITION_CONTAINS			= 0x08,
	DOM_DOCUMENT_POSITION_CONTAINED_BY		= 0x10,
	DOM_DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC	= 0x20
} dom_document_position;

/**
 * Type of node operation being notified to user_data_handler
 */
typedef enum {
	DOM_NODE_CLONED		= 1,
	DOM_NODE_IMPORTED	= 2,
	DOM_NODE_DELETED	= 3,
	DOM_NODE_RENAMED	= 4,
	DOM_NODE_ADOPTED	= 5
} dom_node_operation;

/**
 * Type of handler function for user data registered on a DOM node
 */
typedef void (*dom_user_data_handler)(dom_node_operation operation,
		dom_string *key, void *data, struct dom_node *src,
		struct dom_node *dst);

/**
 * Type of a DOM node
 */
typedef enum {
	DOM_ELEMENT_NODE		= 1,
	DOM_ATTRIBUTE_NODE		= 2,
	DOM_TEXT_NODE			= 3,
	DOM_CDATA_SECTION_NODE		= 4,
	DOM_ENTITY_REFERENCE_NODE	= 5,
	DOM_ENTITY_NODE			= 6,
	DOM_PROCESSING_INSTRUCTION_NODE	= 7,
	DOM_COMMENT_NODE		= 8,
	DOM_DOCUMENT_NODE		= 9,
	DOM_DOCUMENT_TYPE_NODE		= 10,
	DOM_DOCUMENT_FRAGMENT_NODE	= 11,
	DOM_NOTATION_NODE		= 12,

	/* And a count of the number of node types */
	DOM_NODE_TYPE_COUNT		= DOM_NOTATION_NODE
} dom_node_type;

typedef struct dom_node_internal dom_node_internal;

/**
 * DOM node type
 */
typedef struct dom_node {
	void *vtable;
	uint32_t refcnt;
} dom_node;

/* DOM node vtable */
typedef struct dom_node_vtable {
	dom_event_target_vtable base;
	/* pre-destruction hook */
	dom_exception (*dom_node_try_destroy)(dom_node_internal *node);
	/* The DOM level 3 node's oprations */
	dom_exception (*dom_node_get_node_name)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_get_node_value)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_set_node_value)(dom_node_internal *node,
			dom_string *value);
	dom_exception (*dom_node_get_node_type)(dom_node_internal *node,
			dom_node_type *result);
	dom_exception (*dom_node_get_parent_node)(dom_node_internal *node,
			dom_node_internal **result);
	dom_exception (*dom_node_get_child_nodes)(dom_node_internal *node,
			struct dom_nodelist **result);
	dom_exception (*dom_node_get_first_child)(dom_node_internal *node,
			dom_node_internal **result);
	dom_exception (*dom_node_get_last_child)(dom_node_internal *node,
			dom_node_internal **result);
	dom_exception (*dom_node_get_previous_sibling)(dom_node_internal *node,
			dom_node_internal **result);
	dom_exception (*dom_node_get_next_sibling)(dom_node_internal *node,
			dom_node_internal **result);
	dom_exception (*dom_node_get_attributes)(dom_node_internal *node,
			struct dom_namednodemap **result);
	dom_exception (*dom_node_get_owner_document)(dom_node_internal *node,
			struct dom_document **result);
	dom_exception (*dom_node_insert_before)(dom_node_internal *node,
			dom_node_internal *new_child, 
			dom_node_internal *ref_child,
			dom_node_internal **result);
	dom_exception (*dom_node_replace_child)(dom_node_internal *node,
			dom_node_internal *new_child, 
			dom_node_internal *old_child,
			dom_node_internal **result);
	dom_exception (*dom_node_remove_child)(dom_node_internal *node,
			dom_node_internal *old_child,
			dom_node_internal **result);
	dom_exception (*dom_node_append_child)(dom_node_internal *node,
			dom_node_internal *new_child,
			dom_node_internal **result);
	dom_exception (*dom_node_has_child_nodes)(dom_node_internal *node, 
			bool *result);
	dom_exception (*dom_node_clone_node)(dom_node_internal *node, bool deep,
			dom_node_internal **result);
	dom_exception (*dom_node_normalize)(dom_node_internal *node);
	dom_exception (*dom_node_is_supported)(dom_node_internal *node,
			dom_string *feature, dom_string *version,
			bool *result);
	dom_exception (*dom_node_get_namespace)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_get_prefix)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_set_prefix)(dom_node_internal *node,
			dom_string *prefix);
	dom_exception (*dom_node_get_local_name)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_has_attributes)(dom_node_internal *node, 
			bool *result);
	dom_exception (*dom_node_get_base)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_compare_document_position)(
			dom_node_internal *node, dom_node_internal *other,
			uint16_t *result);
	dom_exception (*dom_node_get_text_content)(dom_node_internal *node,
			dom_string **result);
	dom_exception (*dom_node_set_text_content)(dom_node_internal *node,
			dom_string *content);
	dom_exception (*dom_node_is_same)(dom_node_internal *node, 
			dom_node_internal *other, bool *result);
	dom_exception (*dom_node_lookup_prefix)(dom_node_internal *node,
			dom_string *namespace, 
			dom_string **result);
	dom_exception (*dom_node_is_default_namespace)(dom_node_internal *node,
			dom_string *namespace, bool *result);
	dom_exception (*dom_node_lookup_namespace)(dom_node_internal *node,
			dom_string *prefix, dom_string **result);
	dom_exception (*dom_node_is_equal)(dom_node_internal *node,
			dom_node_internal *other, bool *result);
	dom_exception (*dom_node_get_feature)(dom_node_internal *node,
			dom_string *feature, dom_string *version,
			void **result);
	dom_exception (*dom_node_set_user_data)(dom_node_internal *node,
			dom_string *key, void *data,
			dom_user_data_handler handler, void **result);
	dom_exception (*dom_node_get_user_data)(dom_node_internal *node,
			dom_string *key, void **result);
} dom_node_vtable;

/* The ref/unref methods define */

static inline dom_node *dom_node_ref(dom_node *node)
{
	if (node != NULL)
		node->refcnt++;
	
	return node;
}

#define dom_node_ref(n) dom_node_ref((dom_node *) (n))

static inline dom_exception dom_node_try_destroy(dom_node *node)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_try_destroy(
			(dom_node_internal *) node);
}
#define dom_node_try_destroy(n) dom_node_try_destroy((dom_node *) (n))

static inline void dom_node_unref(dom_node *node)
{
	if (node != NULL) {
		if (--node->refcnt == 0)
			dom_node_try_destroy(node);
	}
		
}
#define dom_node_unref(n) dom_node_unref((dom_node *) (n))

static inline dom_exception dom_node_get_node_name(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_node_name(
			(dom_node_internal *) node, result);
}
#define dom_node_get_node_name(n, r) dom_node_get_node_name((dom_node *) (n), (r))

static inline dom_exception dom_node_get_node_value(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_node_value(
			(dom_node_internal *) node, result);
}
#define dom_node_get_node_value(n, r) dom_node_get_node_value( \
		(dom_node *) (n), (r))

static inline dom_exception dom_node_set_node_value(struct dom_node *node,
		dom_string *value)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_set_node_value(
			(dom_node_internal *) node, value);
}
#define dom_node_set_node_value(n, v) dom_node_set_node_value( \
		(dom_node *) (n), (v))

static inline dom_exception dom_node_get_node_type(struct dom_node *node,
		dom_node_type *result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_node_type(
			(dom_node_internal *) node, result);
}
#define dom_node_get_node_type(n, r) dom_node_get_node_type( \
		(dom_node *) (n), (dom_node_type *) (r))

static inline dom_exception dom_node_get_parent_node(struct dom_node *node,
		dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_parent_node(
			(dom_node_internal *) node, 
			(dom_node_internal **) result);
}
#define dom_node_get_parent_node(n, r) dom_node_get_parent_node( \
		(dom_node *) (n), (dom_node **) (r))

static inline dom_exception dom_node_get_child_nodes(struct dom_node *node,
		struct dom_nodelist **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_child_nodes(
			(dom_node_internal *) node, result);
}
#define dom_node_get_child_nodes(n, r) dom_node_get_child_nodes( \
		(dom_node *) (n), (struct dom_nodelist **) (r))

static inline dom_exception dom_node_get_first_child(struct dom_node *node,
		dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_first_child(
			(dom_node_internal *) node, 
			(dom_node_internal **) result);
}
#define dom_node_get_first_child(n, r) dom_node_get_first_child( \
		(dom_node *) (n), (dom_node **) (r))

static inline dom_exception dom_node_get_last_child(struct dom_node *node,
		dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_last_child(
			(dom_node_internal *) node, 
			(dom_node_internal **) result);
}
#define dom_node_get_last_child(n, r) dom_node_get_last_child( \
		(dom_node *) (n), (dom_node **) (r))

static inline dom_exception dom_node_get_previous_sibling(
		struct dom_node *node, dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->
			dom_node_get_previous_sibling(
			(dom_node_internal *) node, 
			(dom_node_internal **) result);
}
#define dom_node_get_previous_sibling(n, r) dom_node_get_previous_sibling( \
		(dom_node *) (n), (dom_node **) (r))

static inline dom_exception dom_node_get_next_sibling(struct dom_node *node,
		dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_next_sibling(
			(dom_node_internal *) node, 
			(dom_node_internal **) result);
}
#define dom_node_get_next_sibling(n, r) dom_node_get_next_sibling( \
		(dom_node *) (n), (dom_node **) (r))

static inline dom_exception dom_node_get_attributes(struct dom_node *node,
		struct dom_namednodemap **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_attributes(
			(dom_node_internal *) node, result);
}
#define dom_node_get_attributes(n, r) dom_node_get_attributes( \
		(dom_node *) (n), (struct dom_namednodemap **) (r))

static inline dom_exception dom_node_get_owner_document(struct dom_node *node,
		struct dom_document **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_owner_document(
			(dom_node_internal *) node, result);
}
#define dom_node_get_owner_document(n, r) dom_node_get_owner_document( \
		(dom_node *) (n), (struct dom_document **) (r))

static inline dom_exception dom_node_insert_before(struct dom_node *node,
		struct dom_node *new_child, struct dom_node *ref_child,
		struct dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_insert_before(
			(dom_node_internal *) node,
			(dom_node_internal *) new_child,
			(dom_node_internal *) ref_child,
			(dom_node_internal **) result);
}
#define dom_node_insert_before(n, nn, ref, ret) dom_node_insert_before( \
		(dom_node *) (n), (dom_node *) (nn), (dom_node *) (ref),\
		(dom_node **) (ret))

static inline dom_exception dom_node_replace_child(struct dom_node *node,
		struct dom_node *new_child, struct dom_node *old_child,
		struct dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_replace_child( 
			(dom_node_internal *) node,
			(dom_node_internal *) new_child,
			(dom_node_internal *) old_child,
			(dom_node_internal **) result);
}
#define dom_node_replace_child(n, nn, old, ret) dom_node_replace_child( \
		(dom_node *) (n), (dom_node *) (nn), (dom_node *) (old),\
		(dom_node **) (ret))

static inline dom_exception dom_node_remove_child(struct dom_node *node,
		struct dom_node *old_child,
		struct dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_remove_child( 
			(dom_node_internal *) node,
			(dom_node_internal *) old_child,
			(dom_node_internal **) result);
}
#define dom_node_remove_child(n, old, ret) dom_node_remove_child( \
		(dom_node *) (n), (dom_node *) (old), (dom_node **) (ret))

static inline dom_exception dom_node_append_child(struct dom_node *node,
		struct dom_node *new_child,
		struct dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_append_child(
			(dom_node_internal *) node,
			(dom_node_internal *) new_child,
			(dom_node_internal **) result);
}
#define dom_node_append_child(n, nn, ret) dom_node_append_child( \
		(dom_node *) (n), (dom_node *) (nn), (dom_node **) (ret))

static inline dom_exception dom_node_has_child_nodes(struct dom_node *node, 
		bool *result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_has_child_nodes(
			(dom_node_internal *) node, result);
}
#define dom_node_has_child_nodes(n, r) dom_node_has_child_nodes( \
		(dom_node *) (n), (bool *) (r))

static inline dom_exception dom_node_clone_node(struct dom_node *node, 
		bool deep, struct dom_node **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_clone_node(
			(dom_node_internal *) node, deep,
			(dom_node_internal **) result);
}
#define dom_node_clone_node(n, d, r) dom_node_clone_node((dom_node *) (n), \
		(bool) (d), (dom_node **) (r))

static inline dom_exception dom_node_normalize(struct dom_node *node)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_normalize(
			(dom_node_internal *) node);
}
#define dom_node_normalize(n) dom_node_normalize((dom_node *) (n))

static inline dom_exception dom_node_is_supported(struct dom_node *node,
		dom_string *feature, dom_string *version,
		bool *result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_is_supported(
			(dom_node_internal *) node, feature, 
			version, result);
}
#define dom_node_is_supported(n, f, v, r) dom_node_is_supported( \
		(dom_node *) (n), (f), (v), (bool *) (r))

static inline dom_exception dom_node_get_namespace(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_namespace(
			(dom_node_internal *) node, result);
}
#define dom_node_get_namespace(n, r) dom_node_get_namespace((dom_node *) (n), (r))

static inline dom_exception dom_node_get_prefix(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_prefix(
			(dom_node_internal *) node, result);
}
#define dom_node_get_prefix(n, r) dom_node_get_prefix((dom_node *) (n), (r))

static inline dom_exception dom_node_set_prefix(struct dom_node *node,
		dom_string *prefix)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_set_prefix(
			(dom_node_internal *) node, prefix);
}
#define dom_node_set_prefix(n, p) dom_node_set_prefix((dom_node *) (n), (p))

static inline dom_exception dom_node_get_local_name(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_local_name(
			(dom_node_internal *) node, result);
}
#define dom_node_get_local_name(n, r) dom_node_get_local_name((dom_node *) (n), (r))

static inline dom_exception dom_node_has_attributes(struct dom_node *node, 
		bool *result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_has_attributes(
			(dom_node_internal *) node, result);
}
#define dom_node_has_attributes(n, r) dom_node_has_attributes( \
		(dom_node *) (n), (bool *) (r))

static inline dom_exception dom_node_get_base(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_base(
			(dom_node_internal *) node, result);
}
#define dom_node_get_base(n, r) dom_node_get_base((dom_node *) (n), (r))

static inline dom_exception dom_node_compare_document_position(
		struct dom_node *node, struct dom_node *other,
		uint16_t *result)
{
	return ((dom_node_vtable *) node->vtable)->
			dom_node_compare_document_position(
			(dom_node_internal *) node,
			(dom_node_internal *) other, result);
}
#define dom_node_compare_document_position(n, o, r) \
		dom_node_compare_document_position((dom_node *) (n), \
		(dom_node *) (o), (uint16_t *) (r))

static inline dom_exception dom_node_get_text_content(struct dom_node *node,
		dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_text_content(
			(dom_node_internal *) node, result);
}
#define dom_node_get_text_content(n, r) dom_node_get_text_content( \
		(dom_node *) (n), (r))

static inline dom_exception dom_node_set_text_content(struct dom_node *node,
		dom_string *content)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_set_text_content(
			(dom_node_internal *) node, content);
}
#define dom_node_set_text_content(n, c) dom_node_set_text_content( \
		(dom_node *) (n), (c))

static inline dom_exception dom_node_is_same(struct dom_node *node, 
		struct dom_node *other, bool *result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_is_same(
			(dom_node_internal *) node,
			(dom_node_internal *) other,
			result);
}
#define dom_node_is_same(n, o, r) dom_node_is_same((dom_node *) (n), \
		(dom_node *) (o), (bool *) (r))

static inline dom_exception dom_node_lookup_prefix(struct dom_node *node,
		dom_string *namespace, dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_lookup_prefix(
			(dom_node_internal *) node, namespace, result);
}
#define dom_node_lookup_prefix(n, ns, r) dom_node_lookup_prefix( \
		(dom_node *) (n), (ns), (r))

static inline dom_exception dom_node_is_default_namespace(
		struct dom_node *node, dom_string *namespace,
		bool *result)
{
	return ((dom_node_vtable *) node->vtable)->
			dom_node_is_default_namespace(
			(dom_node_internal *) node, namespace, result);
}
#define dom_node_is_default_namespace(n, ns, r) dom_node_is_default_namespace(\
		(dom_node *) (n), (ns), (bool *) (r))

static inline dom_exception dom_node_lookup_namespace(struct dom_node *node,
		dom_string *prefix, dom_string **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_lookup_namespace(
			(dom_node_internal *) node, prefix, result);
}
#define dom_node_lookup_namespace(n, p, r) dom_node_lookup_namespace( \
		(dom_node *) (n), (p), (r))

static inline dom_exception dom_node_is_equal(struct dom_node *node,
		struct dom_node *other, bool *result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_is_equal(
			(dom_node_internal *) node,
			(dom_node_internal *) other,
			result);
}
#define dom_node_is_equal(n, o, r) dom_node_is_equal((dom_node *) (n), \
		(dom_node *) (o), (bool *) (r))

static inline dom_exception dom_node_get_feature(struct dom_node *node,
		dom_string *feature, dom_string *version,
		void **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_feature(
			(dom_node_internal *) node, feature, version, result);
}
#define dom_node_get_feature(n, f, v, r) dom_node_get_feature( \
		(dom_node *) (n), (f), (v), (void **) (r))

static inline dom_exception dom_node_set_user_data(struct dom_node *node,
		dom_string *key, void *data,
		dom_user_data_handler handler, void **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_set_user_data(
			(dom_node_internal *) node, key, data, handler,
			result);
}
#define dom_node_set_user_data(n, k, d, h, r) dom_node_set_user_data( \
		(dom_node *) (n), (k), (void *) (d), \
		(dom_user_data_handler) h, (void **) (r))

static inline dom_exception dom_node_get_user_data(struct dom_node *node,
		dom_string *key, void **result)
{
	return ((dom_node_vtable *) node->vtable)->dom_node_get_user_data(
			(dom_node_internal *) node, key, result);
}
#define dom_node_get_user_data(n, k, r) dom_node_get_user_data( \
		(dom_node *) (n), (k), (void **) (r))

#endif
