/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2007 James Shaw <jshaw@netsurf-browser.org>
 */

#ifndef dom_core_document_type_h_
#define dom_core_document_type_h_

#include <dom/core/exceptions.h>
#include <dom/core/node.h>

struct dom_namednodemap;

typedef  struct dom_document_type dom_document_type;
/* The Dom DocumentType vtable */
typedef struct dom_document_type_vtable {
	struct dom_node_vtable base;

	dom_exception (*dom_document_type_get_name)(
			struct dom_document_type *doc_type, 
			dom_string **result);
	dom_exception (*dom_document_type_get_entities)(
			struct dom_document_type *doc_type,
			struct dom_namednodemap **result);
	dom_exception (*dom_document_type_get_notations)(
			struct dom_document_type *doc_type,
			struct dom_namednodemap **result);
	dom_exception (*dom_document_type_get_public_id)(
			struct dom_document_type *doc_type,
			dom_string **result);
	dom_exception (*dom_document_type_get_system_id)(
			struct dom_document_type *doc_type,
			dom_string **result);
	dom_exception (*dom_document_type_get_internal_subset)(
			struct dom_document_type *doc_type,
			dom_string **result);
} dom_document_type_vtable;

static inline dom_exception dom_document_type_get_name(
		struct dom_document_type *doc_type, dom_string **result)
{
	return ((dom_document_type_vtable *) ((dom_node *) (doc_type))->vtable)
			->dom_document_type_get_name(doc_type, result);
}
#define dom_document_type_get_name(dt, r) dom_document_type_get_name( \
		(dom_document_type *) (dt), (r))

static inline dom_exception dom_document_type_get_entities(
		struct dom_document_type *doc_type,
		struct dom_namednodemap **result)
{
	return ((dom_document_type_vtable *) ((dom_node *) (doc_type))->vtable)
			->dom_document_type_get_entities(doc_type, result);
}
#define dom_document_type_get_entities(dt, r) dom_document_type_get_entities( \
		(dom_document_type *) (dt), (struct dom_namednodemap **) (r))

static inline dom_exception dom_document_type_get_notations(
		struct dom_document_type *doc_type,
		struct dom_namednodemap **result)
{
	return ((dom_document_type_vtable *) ((dom_node *) (doc_type))->vtable)
			->dom_document_type_get_notations(doc_type, result);
}
#define dom_document_type_get_notations(dt, r) dom_document_type_get_notations(\
		(dom_document_type *) (dt), (struct dom_namednodemap **) (r))

static inline dom_exception dom_document_type_get_public_id(
		struct dom_document_type *doc_type,
		dom_string **result)
{
	return ((dom_document_type_vtable *) ((dom_node *) (doc_type))->vtable)
			->dom_document_type_get_public_id(doc_type, result);
}
#define dom_document_type_get_public_id(dt, r) \
		dom_document_type_get_public_id((dom_document_type *) (dt), \
		(r))

static inline dom_exception dom_document_type_get_system_id(
		struct dom_document_type *doc_type,
		dom_string **result)
{
	return ((dom_document_type_vtable *) ((dom_node *) (doc_type))->vtable)
			->dom_document_type_get_system_id(doc_type, result);
}
#define dom_document_type_get_system_id(dt, r) \
		dom_document_type_get_system_id((dom_document_type *) (dt), \
		(r))

static inline dom_exception dom_document_type_get_internal_subset(
		struct dom_document_type *doc_type,
		dom_string **result)
{
	return ((dom_document_type_vtable *) ((dom_node *) (doc_type))->vtable)
			->dom_document_type_get_internal_subset(doc_type,
			result);
}
#define dom_document_type_get_internal_subset(dt, r) \
		dom_document_type_get_internal_subset( \
		(dom_document_type *) (dt), (r))


#endif
