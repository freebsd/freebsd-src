/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include <hubbub/errors.h>

/**
 * Convert a hubbub error code to a string
 *
 * \param error  The error code to convert
 * \return Pointer to string representation of error, or NULL if unknown.
 */
const char *hubbub_error_to_string(hubbub_error error)
{
	const char *result = NULL;

	switch (error) {
	case HUBBUB_OK:
		result = "No error";
		break;
	case HUBBUB_REPROCESS:
		result = "Internal (reprocess token)";
		break;
	case HUBBUB_ENCODINGCHANGE:
		result = "Encoding of document has changed";
		break;
	case HUBBUB_PAUSED:
		result = "Parser is paused";
		break;
	case HUBBUB_NOMEM:
		result = "Insufficient memory";
		break;
	case HUBBUB_BADPARM:
		result = "Bad parameter";
		break;
	case HUBBUB_INVALID:
		result = "Invalid input";
		break;
	case HUBBUB_FILENOTFOUND:
		result = "File not found";
		break;
	case HUBBUB_NEEDDATA:
		result = "Insufficient data";
		break;
	case HUBBUB_BADENCODING:
		result = "Unsupported charset";
		break;
	case HUBBUB_UNKNOWN:
		result = "Unknown error";
		break;
	}

	return result;
}

