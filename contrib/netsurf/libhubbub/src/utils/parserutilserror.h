/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_utils_parserutilserror_h_
#define hubbub_utils_parserutilserror_h_

#include <parserutils/errors.h>

#include <hubbub/errors.h>

/**
 * Convert a ParserUtils error into a Hubbub error
 *
 * \param error  The ParserUtils error to convert
 * \return The corresponding Hubbub error
 */
static inline hubbub_error hubbub_error_from_parserutils_error(
		parserutils_error error)
{
	if (error == PARSERUTILS_OK)
		return HUBBUB_OK;
	else if (error == PARSERUTILS_NOMEM)
		return HUBBUB_NOMEM;
	else if (error == PARSERUTILS_BADPARM)
		return HUBBUB_BADPARM;
	else if (error == PARSERUTILS_INVALID)
		return HUBBUB_INVALID;
	else if (error == PARSERUTILS_FILENOTFOUND)
		return HUBBUB_FILENOTFOUND;
	else if (error == PARSERUTILS_NEEDDATA)
		return HUBBUB_NEEDDATA;
	else if (error == PARSERUTILS_BADENCODING)
		return HUBBUB_BADENCODING;
	else if (error == PARSERUTILS_EOF)
		return HUBBUB_OK;

	return HUBBUB_UNKNOWN;
}

#endif

