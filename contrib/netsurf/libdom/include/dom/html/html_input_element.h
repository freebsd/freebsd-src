/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef dom_html_input_element_h_
#define dom_html_input_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/html/html_form_element.h>

typedef struct dom_html_input_element dom_html_input_element;

dom_exception dom_html_input_element_get_default_value(
	dom_html_input_element *input, dom_string **default_value);

dom_exception dom_html_input_element_set_default_value(
	dom_html_input_element *input, dom_string *default_value);

dom_exception dom_html_input_element_get_default_checked(
	dom_html_input_element *input, bool *default_checked);

dom_exception dom_html_input_element_set_default_checked(
	dom_html_input_element *input, bool default_checked);

dom_exception dom_html_input_element_get_form(
	dom_html_input_element *input, dom_html_form_element **form);

dom_exception dom_html_input_element_get_accept(
	dom_html_input_element *input, dom_string **accept);

dom_exception dom_html_input_element_set_accept(
	dom_html_input_element *input, dom_string *accept);

dom_exception dom_html_input_element_get_access_key(
	dom_html_input_element *input, dom_string **access_key);

dom_exception dom_html_input_element_set_access_key(
	dom_html_input_element *input, dom_string *access_key);

dom_exception dom_html_input_element_get_align(
	dom_html_input_element *input, dom_string **align);

dom_exception dom_html_input_element_set_align(
	dom_html_input_element *input, dom_string *align);

dom_exception dom_html_input_element_get_alt(
	dom_html_input_element *input, dom_string **alt);

dom_exception dom_html_input_element_set_alt(
	dom_html_input_element *input, dom_string *alt);

dom_exception dom_html_input_element_get_checked(
	dom_html_input_element *input, bool *checked);

dom_exception dom_html_input_element_set_checked(
	dom_html_input_element *input, bool checked);

dom_exception dom_html_input_element_get_disabled(
	dom_html_input_element *input, bool *disabled);

dom_exception dom_html_input_element_set_disabled(
	dom_html_input_element *input, bool disabled);

dom_exception dom_html_input_element_get_max_length(
	dom_html_input_element *input, int32_t *max_length);

dom_exception dom_html_input_element_set_max_length(
	dom_html_input_element *input, uint32_t max_length);

dom_exception dom_html_input_element_get_name(
	dom_html_input_element *input, dom_string **name);

dom_exception dom_html_input_element_set_name(
	dom_html_input_element *input, dom_string *name);

dom_exception dom_html_input_element_get_read_only(
	dom_html_input_element *input, bool *read_only);

dom_exception dom_html_input_element_set_read_only(
	dom_html_input_element *input, bool read_only);

dom_exception dom_html_input_element_get_size(
	dom_html_input_element *input, dom_string **size);

dom_exception dom_html_input_element_set_size(
	dom_html_input_element *input, dom_string *size);

dom_exception dom_html_input_element_get_src(
	dom_html_input_element *input, dom_string **src);

dom_exception dom_html_input_element_set_src(
	dom_html_input_element *input, dom_string *src);

dom_exception dom_html_input_element_get_tab_index(
	dom_html_input_element *input, int32_t *tab_index);

dom_exception dom_html_input_element_set_tab_index(
	dom_html_input_element *input, uint32_t tab_index);

dom_exception dom_html_input_element_get_type(
	dom_html_input_element *input, dom_string **type);

dom_exception dom_html_input_element_get_use_map(
	dom_html_input_element *input, dom_string **use_map);

dom_exception dom_html_input_element_set_use_map(
	dom_html_input_element *input, dom_string *use_map);

dom_exception dom_html_input_element_get_value(
	dom_html_input_element *input, dom_string **value);

dom_exception dom_html_input_element_set_value(
	dom_html_input_element *input, dom_string *value);

dom_exception dom_html_input_element_blur(dom_html_input_element *ele);
dom_exception dom_html_input_element_focus(dom_html_input_element *ele);
dom_exception dom_html_input_element_select(dom_html_input_element *ele);
dom_exception dom_html_input_element_click(dom_html_input_element *ele);

#endif
