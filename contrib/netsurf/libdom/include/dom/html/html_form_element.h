/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_form_element_h_
#define dom_html_form_element_h_

#include <dom/core/exceptions.h>
#include <dom/core/string.h>

struct dom_html_collection;

typedef struct dom_html_form_element dom_html_form_element;

dom_exception dom_html_form_element_get_elements(dom_html_form_element *ele,
		struct dom_html_collection **col);
dom_exception dom_html_form_element_get_length(dom_html_form_element *ele,
		uint32_t *len);

dom_exception dom_html_form_element_get_accept_charset(
	dom_html_form_element *ele, dom_string **accept_charset);

dom_exception dom_html_form_element_set_accept_charset(
	dom_html_form_element *ele, dom_string *accept_charset);

dom_exception dom_html_form_element_get_action(
	dom_html_form_element *ele, dom_string **action);

dom_exception dom_html_form_element_set_action(
	dom_html_form_element *ele, dom_string *action);

dom_exception dom_html_form_element_get_enctype(
	dom_html_form_element *ele, dom_string **enctype);

dom_exception dom_html_form_element_set_enctype(
	dom_html_form_element *ele, dom_string *enctype);

dom_exception dom_html_form_element_get_method(
	dom_html_form_element *ele, dom_string **method);

dom_exception dom_html_form_element_set_method(
	dom_html_form_element *ele, dom_string *method);

dom_exception dom_html_form_element_get_target(
	dom_html_form_element *ele, dom_string **target);

dom_exception dom_html_form_element_set_target(
	dom_html_form_element *ele, dom_string *target);

dom_exception dom_html_form_element_submit(dom_html_form_element *ele);
dom_exception dom_html_form_element_reset(dom_html_form_element *ele);

#endif

