/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_document_h_
#define dom_core_document_h_

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include <dom/core/exceptions.h>
#include <dom/core/implementation.h>
#include <dom/core/node.h>

struct dom_attr;
struct dom_cdata_section;
struct dom_characterdata;
struct dom_comment;
struct dom_configuration;
struct dom_document_fragment;
struct dom_document_type;
struct dom_element;
struct dom_entity_reference;
struct dom_node;
struct dom_nodelist;
struct dom_processing_instruction;
struct dom_text;
struct lwc_string_s;

typedef struct dom_document dom_document;

/**
 * Quirks mode flag
 */
typedef enum dom_document_quirks_mode {
	DOM_DOCUMENT_QUIRKS_MODE_NONE,
	DOM_DOCUMENT_QUIRKS_MODE_LIMITED,
	DOM_DOCUMENT_QUIRKS_MODE_FULL
} dom_document_quirks_mode;


/* DOM Document vtable */
typedef struct dom_document_vtable {
	struct dom_node_vtable base;

	dom_exception (*dom_document_get_doctype)(struct dom_document *doc,
			struct dom_document_type **result);
	dom_exception (*dom_document_get_implementation)(
			struct dom_document *doc, 
			dom_implementation **result);
	dom_exception (*dom_document_get_document_element)(
			struct dom_document *doc, struct dom_element **result);
	dom_exception (*dom_document_create_element)(struct dom_document *doc,
			dom_string *tag_name, 
			struct dom_element **result);
	dom_exception (*dom_document_create_document_fragment)(
			struct dom_document *doc, 
			struct dom_document_fragment **result);
	dom_exception (*dom_document_create_text_node)(struct dom_document *doc,
			dom_string *data, struct dom_text **result);
	dom_exception (*dom_document_create_comment)(struct dom_document *doc,
			dom_string *data, struct dom_comment **result);
	dom_exception (*dom_document_create_cdata_section)(
			struct dom_document *doc, dom_string *data, 
			struct dom_cdata_section **result);
	dom_exception (*dom_document_create_processing_instruction)(
			struct dom_document *doc, dom_string *target,
			dom_string *data,
			struct dom_processing_instruction **result);
	dom_exception (*dom_document_create_attribute)(struct dom_document *doc,
			dom_string *name, struct dom_attr **result);
	dom_exception (*dom_document_create_entity_reference)(
			struct dom_document *doc, dom_string *name,
			struct dom_entity_reference **result);
	dom_exception (*dom_document_get_elements_by_tag_name)(
			struct dom_document *doc, dom_string *tagname, 
			struct dom_nodelist **result);
	dom_exception (*dom_document_import_node)(struct dom_document *doc,
			struct dom_node *node, bool deep, 
			struct dom_node **result);
	dom_exception (*dom_document_create_element_ns)(
			struct dom_document *doc, dom_string *namespace, 
			dom_string *qname, struct dom_element **result);
	dom_exception (*dom_document_create_attribute_ns)(
			struct dom_document *doc, dom_string *namespace, 
			dom_string *qname, struct dom_attr **result);
	dom_exception (*dom_document_get_elements_by_tag_name_ns)(
			struct dom_document *doc, dom_string *namespace,
			dom_string *localname, 
			struct dom_nodelist **result);
	dom_exception (*dom_document_get_element_by_id)(
			struct dom_document *doc, dom_string *id, 
			struct dom_element **result);
	dom_exception (*dom_document_get_input_encoding)(
			struct dom_document *doc, dom_string **result);
	dom_exception (*dom_document_get_xml_encoding)(struct dom_document *doc,
			dom_string **result);
	dom_exception (*dom_document_get_xml_standalone)(
			struct dom_document *doc, bool *result);
	dom_exception (*dom_document_set_xml_standalone)(
			struct dom_document *doc, bool standalone);
	dom_exception (*dom_document_get_xml_version)(struct dom_document *doc,
			dom_string **result);
	dom_exception (*dom_document_set_xml_version)(struct dom_document *doc,
			dom_string *version);
	dom_exception (*dom_document_get_strict_error_checking)(
			struct dom_document *doc, bool *result);
	dom_exception (*dom_document_set_strict_error_checking)(
			struct dom_document *doc, bool strict);
	dom_exception (*dom_document_get_uri)(struct dom_document *doc,
			dom_string **result);
	dom_exception (*dom_document_set_uri)(struct dom_document *doc,
			dom_string *uri);
	dom_exception (*dom_document_adopt_node)(struct dom_document *doc,
			struct dom_node *node, struct dom_node **result);
	dom_exception (*dom_document_get_dom_config)(struct dom_document *doc,
			struct dom_configuration **result);
	dom_exception (*dom_document_normalize)(struct dom_document *doc);
	dom_exception (*dom_document_rename_node)(struct dom_document *doc,
			struct dom_node *node, dom_string *namespace, 
			dom_string *qname, struct dom_node **result);
	dom_exception (*get_quirks_mode)(dom_document *doc,
					 dom_document_quirks_mode *result);
	dom_exception (*set_quirks_mode)(dom_document *doc,
					 dom_document_quirks_mode quirks);
} dom_document_vtable;

static inline dom_exception dom_document_get_doctype(struct dom_document *doc,
		struct dom_document_type **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_doctype(doc, result);
}
#define dom_document_get_doctype(d, r) dom_document_get_doctype( \
		(dom_document *) (d), (struct dom_document_type **) (r))

static inline dom_exception dom_document_get_implementation(
		struct dom_document *doc, dom_implementation **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_implementation(doc, result);
}
#define dom_document_get_implementation(d, r) dom_document_get_implementation(\
		(dom_document *) (d), (dom_implementation **) (r))

static inline dom_exception dom_document_get_document_element(
		struct dom_document *doc, struct dom_element **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_document_element(doc, result);
}
#define dom_document_get_document_element(d, r) \
		dom_document_get_document_element((dom_document *) (d), \
		(struct dom_element **) (r))

static inline dom_exception dom_document_create_element(
		struct dom_document *doc, dom_string *tag_name, 
		struct dom_element **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_element(doc, tag_name, result);
}
#define dom_document_create_element(d, t, r) dom_document_create_element( \
		(dom_document *) (d), (t), \
		(struct dom_element **) (r))

static inline dom_exception dom_document_create_document_fragment(
		struct dom_document *doc, 
		struct dom_document_fragment **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_document_fragment(doc, result);
}
#define dom_document_create_document_fragment(d, r) \
		dom_document_create_document_fragment((dom_document *) (d), \
		(struct dom_document_fragment **) (r))

static inline dom_exception dom_document_create_text_node(
		struct dom_document *doc, dom_string *data, 
		struct dom_text **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_text_node(doc, data, result);
}
#define dom_document_create_text_node(d, data, r) \
		dom_document_create_text_node((dom_document *) (d), \
		 (data), (struct dom_text **) (r))

static inline dom_exception dom_document_create_comment(
		struct dom_document *doc, dom_string *data, 
		struct dom_comment **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_comment(doc, data, result);
}
#define dom_document_create_comment(d, data, r) dom_document_create_comment( \
		(dom_document *) (d), (data), \
		(struct dom_comment **) (r))

static inline dom_exception dom_document_create_cdata_section(
		struct dom_document *doc, dom_string *data, 
		struct dom_cdata_section **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_cdata_section(doc, data, result);
}
#define dom_document_create_cdata_section(d, data, r) \
		dom_document_create_cdata_section((dom_document *) (d), \
		(data), (struct dom_cdata_section **) (r))

static inline dom_exception dom_document_create_processing_instruction(
		struct dom_document *doc, dom_string *target,
		dom_string *data,
		struct dom_processing_instruction **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_processing_instruction(doc, target,
			data, result);
}
#define dom_document_create_processing_instruction(d, t, data, r) \
		dom_document_create_processing_instruction( \
		(dom_document *) (d), (t), (data), \
		(struct dom_processing_instruction **) (r))

static inline dom_exception dom_document_create_attribute(
		struct dom_document *doc, dom_string *name, 
		struct dom_attr **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_attribute(doc, name, result);
}
#define dom_document_create_attribute(d, n, r) dom_document_create_attribute( \
		(dom_document *) (d), (n), \
		(struct dom_attr **) (r))

static inline dom_exception dom_document_create_entity_reference(
		struct dom_document *doc, dom_string *name,
		struct dom_entity_reference **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_entity_reference(doc, name, 
			result);
}
#define dom_document_create_entity_reference(d, n, r) \
		dom_document_create_entity_reference((dom_document *) (d), \
		(n), (struct dom_entity_reference **) (r))

static inline dom_exception dom_document_get_elements_by_tag_name(
		struct dom_document *doc, dom_string *tagname, 
		struct dom_nodelist **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_elements_by_tag_name(doc, tagname,
			result);
}
#define dom_document_get_elements_by_tag_name(d, t, r) \
		dom_document_get_elements_by_tag_name((dom_document *) (d), \
		(t), (struct dom_nodelist **) (r))

static inline dom_exception dom_document_import_node(struct dom_document *doc,
		struct dom_node *node, bool deep, struct dom_node **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_import_node(doc, node, deep, result);
}
#define dom_document_import_node(d, n, deep, r) dom_document_import_node( \
		(dom_document *) (d), (dom_node *) (n), (bool) deep, \
		(dom_node **) (r))

static inline dom_exception dom_document_create_element_ns(
		struct dom_document *doc, dom_string *namespace, 
		dom_string *qname, struct dom_element **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_element_ns(doc, namespace,
			qname, result);
}
#define dom_document_create_element_ns(d, n, q, r) \
		dom_document_create_element_ns((dom_document *) (d), \
		(n), (q), \
		(struct dom_element **) (r))

static inline dom_exception dom_document_create_attribute_ns
		(struct dom_document *doc, dom_string *namespace, 
		dom_string *qname, struct dom_attr **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_create_attribute_ns(doc, namespace,
			qname, result);
}
#define dom_document_create_attribute_ns(d, n, q, r) \
		dom_document_create_attribute_ns((dom_document *) (d), \
		(n), (q), (struct dom_attr **) (r))

static inline dom_exception dom_document_get_elements_by_tag_name_ns(
		struct dom_document *doc, dom_string *namespace,
		dom_string *localname, struct dom_nodelist **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_elements_by_tag_name_ns(doc,
			namespace, localname, result);
}
#define dom_document_get_elements_by_tag_name_ns(d, n, l, r) \
		dom_document_get_elements_by_tag_name_ns((dom_document *) (d),\
		(n), (l), (struct dom_nodelist **) (r))

static inline dom_exception dom_document_get_element_by_id(
		struct dom_document *doc, dom_string *id, 
		struct dom_element **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_element_by_id(doc, id, result);
}
#define dom_document_get_element_by_id(d, i, r) \
		dom_document_get_element_by_id((dom_document *) (d), \
		(i), (struct dom_element **) (r))

static inline dom_exception dom_document_get_input_encoding(
		struct dom_document *doc, dom_string **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_input_encoding(doc, result);
}
#define dom_document_get_input_encoding(d, r) dom_document_get_input_encoding(\
		(dom_document *) (d), (r))

static inline dom_exception dom_document_get_xml_encoding(
		struct dom_document *doc, dom_string **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_xml_encoding(doc, result);
}
#define dom_document_get_xml_encoding(d, r) dom_document_get_xml_encoding( \
		(dom_document *) (d), (r))

static inline dom_exception dom_document_get_xml_standalone(
		struct dom_document *doc, bool *result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_xml_standalone(doc, result);
}
#define dom_document_get_xml_standalone(d, r) dom_document_get_xml_standalone(\
		(dom_document *) (d), (bool *) (r))

static inline dom_exception dom_document_set_xml_standalone(
		struct dom_document *doc, bool standalone)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_set_xml_standalone(doc, standalone);
}
#define dom_document_set_xml_standalone(d, s) dom_document_set_xml_standalone(\
		(dom_document *) (d), (bool) (s))

static inline dom_exception dom_document_get_xml_version(
		struct dom_document *doc, dom_string **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_xml_version(doc, result);
}
#define dom_document_get_xml_version(d, r) dom_document_get_xml_version( \
		(dom_document *) (d), (r))

static inline dom_exception dom_document_set_xml_version(
		struct dom_document *doc, dom_string *version)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_set_xml_version(doc, version);
}
#define dom_document_set_xml_version(d, v) dom_document_set_xml_version( \
		(dom_document *) (d), (v))

static inline dom_exception dom_document_get_strict_error_checking(
		struct dom_document *doc, bool *result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_strict_error_checking(doc, result);
}
#define dom_document_get_strict_error_checking(d, r) \
		dom_document_get_strict_error_checking((dom_document *) (d), \
		(bool *) (r))

static inline dom_exception dom_document_set_strict_error_checking(
		struct dom_document *doc, bool strict)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_set_strict_error_checking(doc, strict);
}
#define dom_document_set_strict_error_checking(d, s) \
		dom_document_set_strict_error_checking((dom_document *) (d), \
		(bool) (s))

static inline dom_exception dom_document_get_uri(struct dom_document *doc,
		dom_string **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_uri(doc, result);
}
#define dom_document_get_uri(d, r) dom_document_get_uri((dom_document *) (d), \
		(r))

static inline dom_exception dom_document_set_uri(struct dom_document *doc,
		dom_string *uri)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_set_uri(doc, uri);
}
#define dom_document_set_uri(d, u) dom_document_set_uri((dom_document *) (d), \
		(u))

static inline dom_exception dom_document_adopt_node(struct dom_document *doc,
		struct dom_node *node, struct dom_node **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_adopt_node(doc, node, result);
}
#define dom_document_adopt_node(d, n, r) dom_document_adopt_node( \
		(dom_document *) (d), (dom_node *) (n), (dom_node **) (r))

static inline dom_exception dom_document_get_dom_config(
		struct dom_document *doc, struct dom_configuration **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_get_dom_config(doc, result);
}
#define dom_document_get_dom_config(d, r) dom_document_get_dom_config( \
		(dom_document *) (d), (struct dom_configuration **) (r))

static inline dom_exception dom_document_normalize(struct dom_document *doc)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_normalize(doc);
}
#define dom_document_normalize(d) dom_document_normalize((dom_document *) (d))

static inline dom_exception dom_document_rename_node(struct dom_document *doc,
		struct dom_node *node,
		dom_string *namespace, dom_string *qname,
		struct dom_node **result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
			dom_document_rename_node(doc, node, namespace, qname,
			result);
}
#define dom_document_rename_node(d, n, ns, q, r) dom_document_rename_node( \
		(dom_document *) (d), (ns), \
		(q), (dom_node **) (r))

static inline dom_exception dom_document_get_quirks_mode(
	dom_document *doc, dom_document_quirks_mode *result)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
		get_quirks_mode(doc, result);
}
#define dom_document_get_quirks_mode(d, r) \
	dom_document_get_quirks_mode((dom_document *) (d), (r))

static inline dom_exception dom_document_set_quirks_mode(
	dom_document *doc, dom_document_quirks_mode quirks)
{
	return ((dom_document_vtable *) ((dom_node *) doc)->vtable)->
		set_quirks_mode(doc, quirks);
}
#define dom_document_set_quirks_mode(d, q) \
	dom_document_set_quirks_mode((dom_document *) (d), (q))

#endif
