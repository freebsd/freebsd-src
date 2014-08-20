/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_html_option_element_h_
#define dom_html_option_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/html/html_form_element.h>

typedef struct dom_html_option_element dom_html_option_element;

dom_exception dom_html_option_element_get_form(
	dom_html_option_element *option, dom_html_form_element **form);

dom_exception dom_html_option_element_get_default_selected(
	dom_html_option_element *option, bool *default_selected);

dom_exception dom_html_option_element_set_default_selected(
	dom_html_option_element *option, bool default_selected);

dom_exception dom_html_option_element_get_text(
	dom_html_option_element *option, dom_string **text);

dom_exception dom_html_option_element_get_index(
	dom_html_option_element *option, unsigned long *index);

dom_exception dom_html_option_element_get_disabled(
	dom_html_option_element *option, bool *disabled);

dom_exception dom_html_option_element_set_disabled(
	dom_html_option_element *option, bool disabled);

dom_exception dom_html_option_element_get_label(
	dom_html_option_element *option, dom_string **label);

dom_exception dom_html_option_element_set_label(
	dom_html_option_element *option, dom_string *label);

dom_exception dom_html_option_element_get_selected(
	dom_html_option_element *option, bool *selected);

dom_exception dom_html_option_element_set_selected(
	dom_html_option_element *option, bool selected);

dom_exception dom_html_option_element_get_value(
	dom_html_option_element *option, dom_string **value);

dom_exception dom_html_option_element_set_value(
	dom_html_option_element *option, dom_string *value);

#endif
