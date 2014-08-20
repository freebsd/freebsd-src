/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 *
 * This file contains the API used to validate whether certain element's 
 * name/namespace are legal according the XML 1.0 standard. See 
 *
 * http://www.w3.org/TR/2004/REC-xml-20040204/
 * 
 * for detail.
 */

#ifndef  dom_utils_valid_h_
#define  dom_utils_valid_h_

#include <stdbool.h>
#include <dom/core/string.h>

bool _dom_validate_name(dom_string *name);
bool _dom_validate_ncname(dom_string *name);

#endif

