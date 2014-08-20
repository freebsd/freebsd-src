/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_internal_core_document_h_
#define dom_internal_core_document_h_

#include <inttypes.h>
#include <stddef.h>

#include <dom/core/node.h>
#include <dom/core/attr.h>
#include <dom/core/cdatasection.h>
#include <dom/core/comment.h>
#include <dom/core/document.h>
#include <dom/core/document_type.h>
#include <dom/core/doc_fragment.h>
#include <dom/core/element.h>
#include <dom/core/entity_ref.h>
#include <dom/core/pi.h>
#include <dom/core/text.h>
#include <dom/core/implementation.h>

#include "core/string.h"
#include "core/node.h"
#include "core/nodelist.h"

#include "utils/hashtable.h"
#include "utils/list.h"

#include "events/document_event.h"

struct dom_doc_nl;

/**
 * DOM document
 * This should be protected, because later the HTMLDocument will inherit from
 * this. 
 */
struct dom_document {
	dom_node_internal base;		/**< Base node */

	struct dom_doc_nl *nodelists;	/**< List of active nodelists */

	dom_string *uri;		/**< The uri of this document */

	struct list_entry pending_nodes;
			/**< The deletion pending list */

	dom_string *id_name;		/**< The ID attribute's name */

	dom_string *class_string;	/**< The string "class". */

	dom_string *script_string;	/**< The string "script". */

	dom_document_event_internal dei;
			/**< The DocumentEvent interface */
	dom_document_quirks_mode quirks;
				/**< Document is in quirks mode */
	dom_string *_memo_empty;	/**< The string ''. */

	/* Memoised event strings */
	dom_string *_memo_domnodeinserted; /**< DOMNodeInserted */
	dom_string *_memo_domnoderemoved;  /**< DOMNodeRemoved */
	dom_string *_memo_domnodeinsertedintodocument; /**< DOMNodeInsertedIntoDocument */
	dom_string *_memo_domnoderemovedfromdocument;  /**< DOMNodeRemovedFromDocument */
	dom_string *_memo_domattrmodified; /**< DOMAttrModified */
	dom_string *_memo_domcharacterdatamodified; /**< DOMCharacterDataModified */
	dom_string *_memo_domsubtreemodified; /**< DOMSubtreeModified */
};

/* Create a DOM document */
dom_exception _dom_document_create(dom_events_default_action_fetcher daf,
				   void *daf_ctx,
				   dom_document **doc);

/* Initialise the document */
dom_exception _dom_document_initialise(dom_document *doc, 
		dom_events_default_action_fetcher daf,
				   void *daf_ctx);

/* Finalise the document */
bool _dom_document_finalise(dom_document *doc);

/* Begin the virtual functions */
dom_exception _dom_document_get_doctype(dom_document *doc,
		dom_document_type **result);
dom_exception _dom_document_get_implementation(dom_document *doc,
		dom_implementation **result);
dom_exception _dom_document_get_document_element(dom_document *doc,
		dom_element **result);
dom_exception _dom_document_create_element(dom_document *doc,
		dom_string *tag_name, dom_element **result);
dom_exception _dom_document_create_document_fragment(dom_document *doc,
		dom_document_fragment **result);
dom_exception _dom_document_create_text_node(dom_document *doc,
		dom_string *data, dom_text **result);
dom_exception _dom_document_create_comment(dom_document *doc,
		dom_string *data, dom_comment **result);
dom_exception _dom_document_create_cdata_section(dom_document *doc,
		dom_string *data, dom_cdata_section **result);
dom_exception _dom_document_create_processing_instruction(
		dom_document *doc, dom_string *target,
		dom_string *data,
		dom_processing_instruction **result);
dom_exception _dom_document_create_attribute(dom_document *doc,
		dom_string *name, dom_attr **result);
dom_exception _dom_document_create_entity_reference(dom_document *doc,
		dom_string *name,
		dom_entity_reference **result);
dom_exception _dom_document_get_elements_by_tag_name(dom_document *doc,
		dom_string *tagname, dom_nodelist **result);
dom_exception _dom_document_import_node(dom_document *doc,
		dom_node *node, bool deep, dom_node **result);
dom_exception _dom_document_create_element_ns(dom_document *doc,
		dom_string *namespace, dom_string *qname,
		dom_element **result);
dom_exception _dom_document_create_attribute_ns(dom_document *doc,
		dom_string *namespace, dom_string *qname,
		dom_attr **result);
dom_exception _dom_document_get_elements_by_tag_name_ns(
		dom_document *doc, dom_string *namespace,
		dom_string *localname, dom_nodelist **result);
dom_exception _dom_document_get_element_by_id(dom_document *doc,
		dom_string *id, dom_element **result);
dom_exception _dom_document_get_input_encoding(dom_document *doc,
		dom_string **result);
dom_exception _dom_document_get_xml_encoding(dom_document *doc,
		dom_string **result);
dom_exception _dom_document_get_xml_standalone(dom_document *doc,
		bool *result);
dom_exception _dom_document_set_xml_standalone(dom_document *doc,
		bool standalone);
dom_exception _dom_document_get_xml_version(dom_document *doc,
		dom_string **result);
dom_exception _dom_document_set_xml_version(dom_document *doc,
		dom_string *version);
dom_exception _dom_document_get_strict_error_checking(
		dom_document *doc, bool *result);
dom_exception _dom_document_set_strict_error_checking(
		dom_document *doc, bool strict);
dom_exception _dom_document_get_uri(dom_document *doc,
		dom_string **result);
dom_exception _dom_document_set_uri(dom_document *doc,
		dom_string *uri);
dom_exception _dom_document_adopt_node(dom_document *doc,
		dom_node *node, dom_node **result);
dom_exception _dom_document_get_dom_config(dom_document *doc,
		struct dom_configuration **result);
dom_exception _dom_document_normalize(dom_document *doc);
dom_exception _dom_document_rename_node(dom_document *doc,
		dom_node *node,
		dom_string *namespace, dom_string *qname,
		dom_node **result);
dom_exception _dom_document_get_quirks_mode(dom_document *doc,
		dom_document_quirks_mode *result);
dom_exception _dom_document_set_quirks_mode(dom_document *doc,
		dom_document_quirks_mode result);


dom_exception _dom_document_get_text_content(dom_node_internal *node,
					     dom_string **result);
dom_exception _dom_document_set_text_content(dom_node_internal *node,
					     dom_string *content);

#define DOM_DOCUMENT_VTABLE \
	_dom_document_get_doctype, \
	_dom_document_get_implementation, \
	_dom_document_get_document_element, \
	_dom_document_create_element, \
	_dom_document_create_document_fragment, \
	_dom_document_create_text_node, \
	_dom_document_create_comment, \
	_dom_document_create_cdata_section, \
	_dom_document_create_processing_instruction, \
	_dom_document_create_attribute, \
	_dom_document_create_entity_reference, \
	_dom_document_get_elements_by_tag_name, \
	_dom_document_import_node, \
	_dom_document_create_element_ns, \
	_dom_document_create_attribute_ns, \
	_dom_document_get_elements_by_tag_name_ns, \
	_dom_document_get_element_by_id, \
	_dom_document_get_input_encoding, \
	_dom_document_get_xml_encoding, \
	_dom_document_get_xml_standalone, \
	_dom_document_set_xml_standalone, \
	_dom_document_get_xml_version, \
	_dom_document_set_xml_version, \
	_dom_document_get_strict_error_checking, \
	_dom_document_set_strict_error_checking, \
	_dom_document_get_uri, \
	_dom_document_set_uri, \
	_dom_document_adopt_node, \
	_dom_document_get_dom_config, \
	_dom_document_normalize, \
	_dom_document_rename_node, \
	_dom_document_get_quirks_mode, \
	_dom_document_set_quirks_mode

/* End of vtable */

#define DOM_NODE_VTABLE_DOCUMENT \
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
	_dom_document_get_text_content, \
	_dom_document_set_text_content, \
	_dom_node_is_same, \
	_dom_node_lookup_prefix, \
	_dom_node_is_default_namespace, \
	_dom_node_lookup_namespace, \
	_dom_node_is_equal, \
	_dom_node_get_feature, \
	_dom_node_set_user_data, \
	_dom_node_get_user_data

/** \todo Unused! */
/**
 * The internal used vtable for document
 */
struct dom_document_protected_vtable {
	struct dom_node_protect_vtable base;
	dom_exception (*dom_document_get_base)(dom_document *doc,
			dom_string **base_uri);
			/* Get the document's base uri */
};

typedef struct dom_document_protected_vtable dom_document_protected_vtable;

/* Get the document's base URI */
static inline dom_exception dom_document_get_base(dom_document *doc,
		dom_string **base_uri)
{
	dom_node_internal *node = (dom_node_internal *) doc;
	return ((dom_document_protected_vtable *) node->vtable)->
			dom_document_get_base(doc, base_uri);
}
#define dom_document_get_base(d, b) dom_document_get_base( \
		(dom_document *) (d), (dom_string **) (b))

/* Following comes the protected vtable  */
void _dom_document_destroy(dom_node_internal *node);
dom_exception _dom_document_copy(dom_node_internal *old, 
		dom_node_internal **copy);

#define DOM_DOCUMENT_PROTECT_VTABLE \
	_dom_document_destroy, \
	_dom_document_copy


/*---------------------------- Helper functions ---------------------------*/

/* Try to destroy the document:
 * When the refcnt is zero and the pending list is empty, we can destroy this
 * document. */
void _dom_document_try_destroy(dom_document *doc);

/* Get a nodelist, creating one if necessary */
dom_exception _dom_document_get_nodelist(dom_document *doc,
		nodelist_type type, dom_node_internal *root,
		dom_string *tagname, dom_string *namespace,
		dom_string *localname, dom_nodelist **list);
/* Remove a nodelist */
void _dom_document_remove_nodelist(dom_document *doc, dom_nodelist *list);

/* Find element with certain ID in the subtree rooted at root */
dom_exception _dom_find_element_by_id(dom_node_internal *root, 
		dom_string *id, dom_element **result);

/* Set the ID attribute name of this document */
void _dom_document_set_id_name(dom_document *doc, dom_string *name);

#define _dom_document_get_id_name(d) (d->id_name)

#endif
