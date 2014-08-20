/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_internal_html_element_h_
#define dom_internal_html_element_h_

#include <dom/html/html_element.h>

#include "core/element.h"

struct dom_html_document;

/**
 * The dom_html_element class
 *
 */
struct dom_html_element {
	struct dom_element base;
			/**< The base class */
};

dom_exception _dom_html_element_create(struct dom_html_document *doc,
		dom_string *name, dom_string *namespace,
		dom_string *prefix, dom_html_element **result);

dom_exception _dom_html_element_initialise(struct dom_html_document *doc,
		struct dom_html_element *el, dom_string *name, 
		dom_string *namespace, dom_string *prefix);

void _dom_html_element_finalise(struct dom_html_element *ele);

/* Virtual functions */
dom_exception _dom_html_element_get_elements_by_tag_name(
		struct dom_element *element, dom_string *name,
		struct dom_nodelist **result);

dom_exception _dom_html_element_get_elements_by_tag_name_ns(
		struct dom_element *element, dom_string *namespace,
		dom_string *localname, struct dom_nodelist **result);


/* The protected virtual functions */
void _dom_html_element_destroy(dom_node_internal *node);
dom_exception _dom_html_element_copy(dom_node_internal *old,
		dom_node_internal **copy);

#define DOM_ELEMENT_VTABLE_HTML_ELEMENT \
	_dom_element_get_tag_name, \
	_dom_element_get_attribute, \
	_dom_element_set_attribute, \
	_dom_element_remove_attribute, \
	_dom_element_get_attribute_node, \
	_dom_element_set_attribute_node, \
	_dom_element_remove_attribute_node, \
	_dom_html_element_get_elements_by_tag_name, \
	_dom_element_get_attribute_ns, \
	_dom_element_set_attribute_ns, \
	_dom_element_remove_attribute_ns, \
	_dom_element_get_attribute_node_ns, \
	_dom_element_set_attribute_node_ns, \
	_dom_html_element_get_elements_by_tag_name_ns, \
	_dom_element_has_attribute, \
	_dom_element_has_attribute_ns, \
	_dom_element_get_schema_type_info, \
	_dom_element_set_id_attribute, \
	_dom_element_set_id_attribute_ns, \
	_dom_element_set_id_attribute_node, \
	_dom_element_get_classes, \
	_dom_element_has_class

#define DOM_HTML_ELEMENT_PROTECT_VTABLE \
	_dom_html_element_destroy, \
	_dom_html_element_copy


/* The API functions */
dom_exception _dom_html_element_get_id(dom_html_element *element,
                                       dom_string **id);
dom_exception _dom_html_element_set_id(dom_html_element *element,
                                       dom_string *id);
dom_exception _dom_html_element_get_title(dom_html_element *element,
                                       dom_string **title);
dom_exception _dom_html_element_set_title(dom_html_element *element,
                                       dom_string *title);
dom_exception _dom_html_element_get_lang(dom_html_element *element,
                                       dom_string **lang);
dom_exception _dom_html_element_set_lang(dom_html_element *element,
                                       dom_string *lang);
dom_exception _dom_html_element_get_dir(dom_html_element *element,
                                       dom_string **dir);
dom_exception _dom_html_element_set_dir(dom_html_element *element,
                                       dom_string *dir);
dom_exception _dom_html_element_get_class_name(dom_html_element *element,
                                       dom_string **class_name);
dom_exception _dom_html_element_set_class_name(dom_html_element *element,
                                       dom_string *class_name);

#define DOM_HTML_ELEMENT_VTABLE \
	_dom_html_element_get_id, \
	_dom_html_element_set_id, \
	_dom_html_element_get_title, \
	_dom_html_element_set_title, \
	_dom_html_element_get_lang, \
	_dom_html_element_set_lang, \
	_dom_html_element_get_dir, \
	_dom_html_element_set_dir, \
	_dom_html_element_get_class_name, \
	_dom_html_element_set_class_name

/* Some common functions used by all child classes */
dom_exception dom_html_element_get_bool_property(dom_html_element *ele,
		const char *name, uint32_t len, bool *has);
dom_exception dom_html_element_set_bool_property(dom_html_element *ele,
		const char *name, uint32_t len, bool has);

dom_exception dom_html_element_get_int32_t_property(dom_html_element *ele,
		const char *name, uint32_t len, int32_t *value);
dom_exception dom_html_element_set_int32_t_property(dom_html_element *ele,
		const char *name, uint32_t len, uint32_t value);

extern struct dom_html_element_vtable _dom_html_element_vtable;

#endif

