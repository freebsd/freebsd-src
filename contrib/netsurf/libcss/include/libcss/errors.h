/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef libcss_errors_h_
#define libcss_errors_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>

typedef enum css_error {
	CSS_OK               = 0,

	CSS_NOMEM            = 1,
	CSS_BADPARM          = 2,
	CSS_INVALID          = 3,
	CSS_FILENOTFOUND     = 4,
	CSS_NEEDDATA         = 5,
	CSS_BADCHARSET       = 6,
	CSS_EOF              = 7,
	CSS_IMPORTS_PENDING  = 8,
	CSS_PROPERTY_NOT_SET = 9
} css_error;

/* Convert a libcss error value to a string */
const char *css_error_to_string(css_error error);

#ifdef __cplusplus
}
#endif

#endif

