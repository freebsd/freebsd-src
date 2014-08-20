/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_isindex_element_h_
#define dom_html_isindex_element_h_

#include <dom/core/exceptions.h>

struct dom_html_form_element;

/**
 * Note: the HTML 4.01 spec said: this element is deprecated, use
 * <INPUT> element instead.
 */

typedef struct dom_html_isindex_element dom_html_isindex_element;

dom_exception dom_html_isindex_element_get_form(dom_html_isindex_element *ele,
		struct dom_html_form_element **form);

#endif

