/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef dom_html_button_element_h_
#define dom_html_button_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/html/html_form_element.h>

typedef struct dom_html_button_element dom_html_button_element;

dom_exception dom_html_button_element_get_form(
	dom_html_button_element *button, dom_html_form_element **form);

dom_exception dom_html_button_element_get_access_key(
	dom_html_button_element *button, dom_string **access_key);

dom_exception dom_html_button_element_set_access_key(
	dom_html_button_element *button, dom_string *access_key);

dom_exception dom_html_button_element_get_disabled(
	dom_html_button_element *button, bool *disabled);

dom_exception dom_html_button_element_set_disabled(
	dom_html_button_element *button, bool disabled);

dom_exception dom_html_button_element_get_name(
	dom_html_button_element *button, dom_string **name);

dom_exception dom_html_button_element_set_name(
	dom_html_button_element *button, dom_string *name);

dom_exception dom_html_button_element_get_tab_index(
	dom_html_button_element *button, int32_t *tab_index);

dom_exception dom_html_button_element_set_tab_index(
	dom_html_button_element *button, uint32_t tab_index);

dom_exception dom_html_button_element_get_type(
	dom_html_button_element *button, dom_string **type);

dom_exception dom_html_button_element_get_value(
	dom_html_button_element *button, dom_string **value);

dom_exception dom_html_button_element_set_value(
	dom_html_button_element *button, dom_string *value);

#endif
