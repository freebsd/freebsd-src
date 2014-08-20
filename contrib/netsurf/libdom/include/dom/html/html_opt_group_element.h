/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef dom_html_opt_group_element_h_
#define dom_html_opt_group_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>
#include <dom/html/html_form_element.h>

typedef struct dom_html_opt_group_element dom_html_opt_group_element;

dom_exception dom_html_opt_group_element_get_form(
	dom_html_opt_group_element *opt_group, dom_html_form_element **form);

dom_exception dom_html_opt_group_element_get_disabled(
	dom_html_opt_group_element *opt_group, bool *disabled);

dom_exception dom_html_opt_group_element_set_disabled(
	dom_html_opt_group_element *opt_group, bool disabled);

dom_exception dom_html_opt_group_element_get_label(
	dom_html_opt_group_element *opt_group, dom_string **label);

dom_exception dom_html_opt_group_element_set_label(
	dom_html_opt_group_element *opt_group, dom_string *label);

#endif
