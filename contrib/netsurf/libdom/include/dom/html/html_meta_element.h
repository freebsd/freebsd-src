/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_meta_element_h_
#define dom_html_meta_element_h_

#include <dom/core/string.h>

typedef struct dom_html_meta_element dom_html_meta_element;

dom_exception dom_html_meta_element_get_content(dom_html_meta_element *ele,
		dom_string **content);

dom_exception dom_html_meta_element_set_content(dom_html_meta_element *ele,
		dom_string *content);

dom_exception dom_html_meta_element_get_http_equiv(dom_html_meta_element *ele,
		dom_string **http_equiv);

dom_exception dom_html_meta_element_set_http_equiv(dom_html_meta_element *ele,
		dom_string *http_equiv);

dom_exception dom_html_meta_element_get_name(dom_html_meta_element *ele,
		dom_string **name);

dom_exception dom_html_meta_element_set_name(dom_html_meta_element *ele,
		dom_string *name);

dom_exception dom_html_meta_element_get_scheme(dom_html_meta_element *ele,
		dom_string **scheme);

dom_exception dom_html_meta_element_set_scheme(dom_html_meta_element *ele,
		dom_string *scheme);

#endif

