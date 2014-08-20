/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include <libcss/errors.h>

/**
 * Convert a LibCSS error code to a string
 *
 * \param error  The error code to convert
 * \return Pointer to string representation of error, or NULL if unknown.
 */
const char *css_error_to_string(css_error error)
{
	const char *result = NULL;

	switch (error) {
	case CSS_OK:
		result = "No error";
		break;
	case CSS_NOMEM:
		result = "Insufficient memory";
		break;
	case CSS_BADPARM:
		result = "Bad parameter";
		break;
	case CSS_INVALID:
		result = "Invalid input";
		break;
	case CSS_FILENOTFOUND:
		result = "File not found";
		break;
	case CSS_NEEDDATA:
		result = "Insufficient data";
		break;
	case CSS_BADCHARSET:
		result = "BOM and @charset mismatch";
		break;
	case CSS_EOF:
		result = "EOF encountered";
		break;
	case CSS_IMPORTS_PENDING:
		result = "Imports pending";
		break;
	case CSS_PROPERTY_NOT_SET:
		result = "Property not set";
		break;
	}

	return result;
}
