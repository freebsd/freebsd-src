/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_title_element_h_
#define dom_html_title_element_h_

#include <dom/core/exceptions.h>
#include <dom/core/string.h>

typedef struct dom_html_title_element dom_html_title_element;

dom_exception dom_html_title_element_get_text(dom_html_title_element *ele,
		dom_string **text);

dom_exception dom_html_title_element_set_text(dom_html_title_element *ele,
		dom_string *text);

#endif

