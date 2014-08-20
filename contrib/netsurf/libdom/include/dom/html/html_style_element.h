/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_style_element_h_
#define dom_html_style_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>

typedef struct dom_html_style_element dom_html_style_element;

dom_exception dom_html_style_element_get_disabled(dom_html_style_element *ele,
		bool *disabled);

dom_exception dom_html_style_element_set_disabled(dom_html_style_element *ele,
		bool disabled);

#endif

