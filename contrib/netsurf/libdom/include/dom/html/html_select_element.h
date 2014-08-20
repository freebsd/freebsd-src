/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef dom_html_select_element_h_
#define dom_html_select_element_h_

#include <stdbool.h>

#include <dom/core/exceptions.h>

#include <dom/html/html_form_element.h>

typedef struct dom_html_select_element dom_html_select_element;

struct dom_html_options_collection;
struct dom_html_element;

dom_exception dom_html_select_element_get_type(
		dom_html_select_element *ele, dom_string **type);

dom_exception dom_html_select_element_get_selected_index(
		dom_html_select_element *ele, int32_t *index);
dom_exception dom_html_select_element_set_selected_index(
		dom_html_select_element *ele, int32_t index);

dom_exception dom_html_select_element_get_value(
		dom_html_select_element *ele, dom_string **value);
dom_exception dom_html_select_element_set_value(
		dom_html_select_element *ele, dom_string *value);

dom_exception dom_html_select_element_get_length(
		dom_html_select_element *ele, uint32_t *len);
dom_exception dom_html_select_element_set_length(
		dom_html_select_element *ele, uint32_t len);

dom_exception dom_html_select_element_get_form(
		dom_html_select_element *ele, dom_html_form_element **form);

dom_exception dom__html_select_element_get_options(
		dom_html_select_element *ele,
		struct dom_html_options_collection **col);
#define dom_html_select_element_get_options(e, c) \
	dom__html_select_element_get_options((dom_html_select_element *) (e), \
			(struct dom_html_options_collection **) (c))

dom_exception dom_html_select_element_get_disabled(
		dom_html_select_element *ele, bool *disabled);
dom_exception dom_html_select_element_set_disabled(
		dom_html_select_element *ele, bool disabled);

dom_exception dom_html_select_element_get_multiple(
		dom_html_select_element *ele, bool *multiple);
dom_exception dom_html_select_element_set_multiple(
		dom_html_select_element *ele, bool multiple);

dom_exception dom_html_select_element_get_name(
		dom_html_select_element *ele, dom_string **name);
dom_exception dom_html_select_element_set_name(
		dom_html_select_element *ele, dom_string *name);

dom_exception dom_html_select_element_get_size(
		dom_html_select_element *ele, int32_t *size);
dom_exception dom_html_select_element_set_size(
		dom_html_select_element *ele, int32_t size);

dom_exception dom_html_select_element_get_tab_index(
		dom_html_select_element *ele, int32_t *tab_index);
dom_exception dom_html_select_element_set_tab_index(
		dom_html_select_element *ele, int32_t tab_index);

/* Functions */
dom_exception dom__html_select_element_add(dom_html_select_element *select,
		struct dom_html_element *ele, struct dom_html_element *before);
#define dom_html_select_element_add(s, e, b) \
	dom__html_select_element_add((dom_html_select_element *) (s), \
		(struct dom_html_element *) (e), \
		(struct dom_html_element *) (b))
dom_exception dom_html_select_element_remove(dom_html_select_element *ele,
		int32_t index);
dom_exception dom_html_select_element_blur(struct dom_html_select_element *ele);
dom_exception dom_html_select_element_focus(struct dom_html_select_element *ele);

#endif

