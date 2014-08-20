/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_utils_parserutilserror_h_
#define css_utils_parserutilserror_h_

#include <parserutils/errors.h>

#include <libcss/errors.h>

/**
 * Convert a ParserUtils error into a LibCSS error
 * 
 * \param error  The ParserUtils error to convert
 * \return The corresponding LibCSS error
 */
static inline css_error css_error_from_parserutils_error(
		parserutils_error error)
{
	/* Currently, there's a 1:1 mapping, so we've nothing to do */
	return (css_error) error;
}

#endif

