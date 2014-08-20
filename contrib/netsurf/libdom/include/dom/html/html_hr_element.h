/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 * Copyright 2014 Rupinder Singh Khokhar <rsk1coder99@gmail.com>
 */
#ifndef dom_html_hr_element_h_
#define dom_html_hr_element_h_

#include <stdbool.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

typedef struct dom_html_hr_element dom_html_hr_element;

dom_exception dom_html_hr_element_get_no_shade(
	dom_html_hr_element *ele, bool *no_shade);

dom_exception dom_html_hr_element_set_no_shade(
	dom_html_hr_element *ele, bool no_shade);

dom_exception dom_html_hr_element_get_align(
	dom_html_hr_element *element, dom_string **align);

dom_exception dom_html_hr_element_set_align(
	dom_html_hr_element *element, dom_string *align);

dom_exception dom_html_hr_element_get_size(
	dom_html_hr_element *element, dom_string **size);

dom_exception dom_html_hr_element_set_size(
	dom_html_hr_element *ele, dom_string *size);

dom_exception dom_html_hr_element_get_width(
	dom_html_hr_element *ele, dom_string **width);

dom_exception dom_html_hr_element_set_width(
	dom_html_hr_element *ele, dom_string *width);

#endif
