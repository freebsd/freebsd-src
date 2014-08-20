/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_link_element_h_
#define dom_html_link_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

typedef struct dom_html_link_element dom_html_link_element;

dom_exception dom_html_link_element_get_disabled(dom_html_link_element *ele,
		bool *disabled);

dom_exception dom_html_link_element_set_disabled(dom_html_link_element *ele,
		bool disabled);

dom_exception dom_html_link_element_get_charset(dom_html_link_element *ele,
		dom_string **charset);

dom_exception dom_html_link_element_set_charset(dom_html_link_element *ele,
		dom_string *charset);

dom_exception dom_html_link_element_get_href(dom_html_link_element *ele,
		dom_string **href);

dom_exception dom_html_link_element_set_href(dom_html_link_element *ele,
		dom_string *href);

dom_exception dom_html_link_element_get_hreflang(dom_html_link_element *ele,
		dom_string **hreflang);

dom_exception dom_html_link_element_set_hreflang(dom_html_link_element *ele,
		dom_string *hreflang);

dom_exception dom_html_link_element_get_media(dom_html_link_element *ele,
		dom_string **media);

dom_exception dom_html_link_element_set_media(dom_html_link_element *ele,
		dom_string *media);

dom_exception dom_html_link_element_get_rel(dom_html_link_element *ele,
		dom_string **rel);

dom_exception dom_html_link_element_set_rel(dom_html_link_element *ele,
		dom_string *rel);

dom_exception dom_html_link_element_get_rev(dom_html_link_element *ele,
		dom_string **rev);

dom_exception dom_html_link_element_set_rev(dom_html_link_element *ele,
		dom_string *rev);

dom_exception dom_html_link_element_get_target(dom_html_link_element *ele,
		dom_string **target);

dom_exception dom_html_link_element_set_target(dom_html_link_element *ele,
		dom_string *target);

dom_exception dom_html_link_element_get_type(dom_html_link_element *ele,
		dom_string **type);

dom_exception dom_html_link_element_set_type(dom_html_link_element *ele,
		dom_string *type);

#endif

