/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku.com>
 */

#ifndef dom_html_html_element_h_
#define dom_html_html_element_h_

#include <dom/html/html_element.h>

typedef struct dom_html_html_element dom_html_html_element;

dom_exception dom_html_html_element_get_version(
		struct dom_html_html_element *element, dom_string **version);  
dom_exception dom_html_html_element_set_version(
		struct dom_html_html_element *element, dom_string *version);


#endif

