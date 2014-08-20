/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_html_document_h_
#define dom_internal_html_document_h_

#include <dom/html/html_document.h>

#include "core/document.h"

/**
 * The dom_html_document class
 */
struct dom_html_document {
	struct dom_document base;	/**< The base class */
	
	dom_string *title;	/**< HTML document title */
	dom_string *referrer;	/**< HTML document referrer */
	dom_string *domain;	/**< HTML document domain */
	dom_string *url;	/**< HTML document URL */
	dom_string *cookie;	/**< HTML document cookie */
	
	/** Cached strings for html objects to use */
	dom_string **memoised;
};

#include "html_document_strings.h"

/* Create a HTMLDocument */
dom_exception _dom_html_document_create(
		dom_events_default_action_fetcher daf,
		void *daf_ctx,
		dom_html_document **doc);
/* Initialise a HTMLDocument */
dom_exception _dom_html_document_initialise(
		dom_html_document *doc,
		dom_events_default_action_fetcher daf,
		void *daf_ctx);
/* Finalise a HTMLDocument */
bool _dom_html_document_finalise(dom_html_document *doc);

void _dom_html_document_destroy(dom_node_internal *node);
dom_exception _dom_html_document_copy(dom_node_internal *old, 
		dom_node_internal **copy);

#define DOM_HTML_DOCUMENT_PROTECT_VTABLE \
	_dom_html_document_destroy, \
	_dom_html_document_copy

dom_exception _dom_html_document_get_title(dom_html_document *doc,
		dom_string **title);
dom_exception _dom_html_document_set_title(dom_html_document *doc,
		dom_string *title);
dom_exception _dom_html_document_get_referrer(dom_html_document *doc,
		dom_string **referrer);
dom_exception _dom_html_document_get_domain(dom_html_document *doc,
		dom_string **domain);
dom_exception _dom_html_document_get_url(dom_html_document *doc,
		dom_string **url);
dom_exception _dom_html_document_get_body(dom_html_document *doc,
		struct dom_html_element **body);
dom_exception _dom_html_document_set_body(dom_html_document *doc,
		struct dom_html_element *body);
dom_exception _dom_html_document_get_images(dom_html_document *doc,
		struct dom_html_collection **col);
dom_exception _dom_html_document_get_applets(dom_html_document *doc,
		struct dom_html_collection **col);
dom_exception _dom_html_document_get_links(dom_html_document *doc,
		struct dom_html_collection **col);
dom_exception _dom_html_document_get_forms(dom_html_document *doc,
		struct dom_html_collection **col);
dom_exception _dom_html_document_get_anchors(dom_html_document *doc,
		struct dom_html_collection **col);
dom_exception _dom_html_document_get_cookie(dom_html_document *doc,
		dom_string **cookie);
dom_exception _dom_html_document_set_cookie(dom_html_document *doc,
		dom_string *cookie);
 
dom_exception _dom_html_document_open(dom_html_document *doc);
dom_exception _dom_html_document_close(dom_html_document *doc);
dom_exception _dom_html_document_write(dom_html_document *doc,
		dom_string *text);
dom_exception _dom_html_document_writeln(dom_html_document *doc,
		dom_string *text);
dom_exception _dom_html_document_get_elements_by_name(dom_html_document *doc,
		dom_string *name, struct dom_nodelist **list);


#define DOM_HTML_DOCUMENT_VTABLE \
	_dom_html_document_get_title, \
	_dom_html_document_set_title, \
	_dom_html_document_get_referrer, \
	_dom_html_document_get_domain, \
	_dom_html_document_get_url, \
	_dom_html_document_get_body, \
	_dom_html_document_set_body, \
	_dom_html_document_get_images, \
	_dom_html_document_get_applets, \
	_dom_html_document_get_links, \
	_dom_html_document_get_forms, \
	_dom_html_document_get_anchors, \
	_dom_html_document_get_cookie, \
	_dom_html_document_set_cookie, \
	_dom_html_document_open, \
	_dom_html_document_close, \
	_dom_html_document_write, \
	_dom_html_document_writeln, \
	_dom_html_document_get_elements_by_name

dom_exception _dom_html_document_create_element(dom_document *doc,
		dom_string *tag_name, dom_element **result);
dom_exception _dom_html_document_create_element_ns(dom_document *doc,
		dom_string *namespace, dom_string *qname,
		dom_element **result);
dom_exception _dom_html_document_get_elements_by_tag_name(dom_document *doc,
		dom_string *tagname, dom_nodelist **result);
dom_exception _dom_html_document_get_elements_by_tag_name_ns(
		dom_document *doc, dom_string *namespace,
		dom_string *localname, dom_nodelist **result);
dom_exception _dom_html_document_create_attribute(dom_document *doc,
		dom_string *name, dom_attr **result);
dom_exception _dom_html_document_create_attribute_ns(dom_document *doc,
		dom_string *namespace, dom_string *qname,
		dom_attr **result);

#define DOM_DOCUMENT_VTABLE_HTML \
	_dom_document_get_doctype, \
	_dom_document_get_implementation, \
	_dom_document_get_document_element, \
	_dom_html_document_create_element, \
	_dom_document_create_document_fragment, \
	_dom_document_create_text_node, \
	_dom_document_create_comment, \
	_dom_document_create_cdata_section, \
	_dom_document_create_processing_instruction, \
	_dom_html_document_create_attribute, \
	_dom_document_create_entity_reference, \
	_dom_html_document_get_elements_by_tag_name, \
	_dom_document_import_node, \
	_dom_html_document_create_element_ns, \
	_dom_html_document_create_attribute_ns, \
	_dom_html_document_get_elements_by_tag_name_ns, \
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
	_dom_document_rename_node,	\
	_dom_document_get_quirks_mode,	\
	_dom_document_set_quirks_mode

#endif

