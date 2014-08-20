/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#ifndef dom_html_body_element_h_
#define dom_html_body_element_h_

#include <dom/core/exceptions.h>
#include <dom/core/string.h>

typedef struct dom_html_body_element dom_html_body_element;

dom_exception dom_html_body_element_get_a_link(dom_html_body_element *ele,
		dom_string **a_link);

dom_exception dom_html_body_element_set_a_link(dom_html_body_element *ele,
		dom_string *a_link);

dom_exception dom_html_body_element_get_v_link(dom_html_body_element *ele,
		dom_string **v_link);

dom_exception dom_html_body_element_set_v_link(dom_html_body_element *ele,
		dom_string *v_link);

dom_exception dom_html_body_element_get_background(dom_html_body_element *ele,
		dom_string **background);

dom_exception dom_html_body_element_set_background(dom_html_body_element *ele,
		dom_string *background);

dom_exception dom_html_body_element_get_bg_color(dom_html_body_element *ele,
		dom_string **bg_color);

dom_exception dom_html_body_element_set_bg_color(dom_html_body_element *ele,
		dom_string *bg_color);

dom_exception dom_html_body_element_get_link(dom_html_body_element *ele,
		dom_string **link);

dom_exception dom_html_body_element_set_link(dom_html_body_element *ele,
		dom_string *link);

dom_exception dom_html_body_element_get_text(dom_html_body_element *ele,
		dom_string **text);

dom_exception dom_html_body_element_set_text(dom_html_body_element *ele,
		dom_string *text);

#endif

