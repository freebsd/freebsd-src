/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_utils_namespace_h_
#define dom_utils_namespace_h_

#include <dom/functypes.h>
#include <dom/core/exceptions.h>
#include <dom/core/string.h>

struct dom_document;

/* Ensure a QName is valid */
dom_exception _dom_namespace_validate_qname(dom_string *qname,
		dom_string *namespace);

/* Split a QName into a namespace prefix and localname string */
dom_exception _dom_namespace_split_qname(dom_string *qname,
		dom_string **prefix, dom_string **localname);

/* Get the XML prefix dom_string */
dom_string *_dom_namespace_get_xml_prefix(void);

/* Get the XMLNS prefix dom_string */
dom_string *_dom_namespace_get_xmlns_prefix(void);

#endif

