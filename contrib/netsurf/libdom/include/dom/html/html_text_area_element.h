/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef dom_html_text_area_element_h_
#define dom_html_text_area_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/html/html_form_element.h>

typedef struct dom_html_text_area_element dom_html_text_area_element;

dom_exception dom_html_text_area_element_get_default_value(
	dom_html_text_area_element *text_area, dom_string **default_value);

dom_exception dom_html_text_area_element_set_default_value(
	dom_html_text_area_element *text_area, dom_string *default_value);

dom_exception dom_html_text_area_element_get_form(
	dom_html_text_area_element *text_area, dom_html_form_element **form);

dom_exception dom_html_text_area_element_get_access_key(
	dom_html_text_area_element *text_area, dom_string **access_key);

dom_exception dom_html_text_area_element_set_access_key(
	dom_html_text_area_element *text_area, dom_string *access_key);

dom_exception dom_html_text_area_element_get_disabled(
	dom_html_text_area_element *text_area, bool *disabled);

dom_exception dom_html_text_area_element_set_disabled(
	dom_html_text_area_element *text_area, bool disabled);

dom_exception dom_html_text_area_element_get_cols(
	dom_html_text_area_element *text_area, int32_t *cols);

dom_exception dom_html_text_area_element_set_cols(
	dom_html_text_area_element *text_area, uint32_t cols);

dom_exception dom_html_text_area_element_get_rows(
	dom_html_text_area_element *text_area, int32_t *rows);

dom_exception dom_html_text_area_element_set_rows(
	dom_html_text_area_element *text_area, uint32_t rows);

dom_exception dom_html_text_area_element_get_name(
	dom_html_text_area_element *text_area, dom_string **name);

dom_exception dom_html_text_area_element_set_name(
	dom_html_text_area_element *text_area, dom_string *name);

dom_exception dom_html_text_area_element_get_read_only(
	dom_html_text_area_element *text_area, bool *read_only);

dom_exception dom_html_text_area_element_set_read_only(
	dom_html_text_area_element *text_area, bool read_only);

dom_exception dom_html_text_area_element_get_tab_index(
	dom_html_text_area_element *text_area, int32_t *tab_index);

dom_exception dom_html_text_area_element_set_tab_index(
	dom_html_text_area_element *text_area, uint32_t tab_index);

dom_exception dom_html_text_area_element_get_type(
	dom_html_text_area_element *text_area, dom_string **type);

dom_exception dom_html_text_area_element_get_value(
	dom_html_text_area_element *text_area, dom_string **value);

dom_exception dom_html_text_area_element_set_value(
	dom_html_text_area_element *text_area, dom_string *value);

dom_exception dom_html_text_area_element_blur(dom_html_text_area_element *ele);
dom_exception dom_html_text_area_element_focus(dom_html_text_area_element *ele);
dom_exception dom_html_text_area_element_select(dom_html_text_area_element *ele);

#endif
