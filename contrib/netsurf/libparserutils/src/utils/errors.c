/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include <parserutils/errors.h>

/**
 * Convert a parserutils error code to a string
 *
 * \param error  The error code to convert
 * \return Pointer to string representation of error, or NULL if unknown.
 */
const char *parserutils_error_to_string(parserutils_error error)
{
	const char *result = NULL;

	switch (error) {
	case PARSERUTILS_OK:
		result = "No error";
		break;
	case PARSERUTILS_NOMEM:
		result = "Insufficient memory";
		break;
	case PARSERUTILS_BADPARM:
		result = "Bad parameter";
		break;
	case PARSERUTILS_INVALID:
		result = "Invalid input";
		break;
	case PARSERUTILS_FILENOTFOUND:
		result = "File not found";
		break;
	case PARSERUTILS_NEEDDATA:
		result = "Insufficient data";
		break;
	case PARSERUTILS_BADENCODING:
		result = "Unsupported encoding";
		break;
	case PARSERUTILS_EOF:
		result = "EOF";
		break;
	}

	return result;
}

/**
 * Convert a string representation of an error name to a parserutils error code
 *
 * \param str  String containing error name
 * \param len  Length of string (bytes)
 * \return Error code, or PARSERUTILS_OK if unknown
 */
parserutils_error parserutils_error_from_string(const char *str, size_t len)
{
	if (strncmp(str, "PARSERUTILS_OK", len) == 0) {
		return PARSERUTILS_OK;
	} else if (strncmp(str, "PARSERUTILS_NOMEM", len) == 0) {
		return PARSERUTILS_NOMEM;
	} else if (strncmp(str, "PARSERUTILS_BADPARM", len) == 0) {
		return PARSERUTILS_BADPARM;
	} else if (strncmp(str, "PARSERUTILS_INVALID", len) == 0) {
		return PARSERUTILS_INVALID;
	} else if (strncmp(str, "PARSERUTILS_FILENOTFOUND", len) == 0) {
		return PARSERUTILS_FILENOTFOUND;
	} else if (strncmp(str, "PARSERUTILS_NEEDDATA", len) == 0) {
		return PARSERUTILS_NEEDDATA;
	} else if (strncmp(str, "PARSERUTILS_BADENCODING", len) == 0) {
		return PARSERUTILS_BADENCODING;
	} else if (strncmp(str, "PARSERUTILS_EOF", len) == 0) {
		return PARSERUTILS_EOF;
	}

	return PARSERUTILS_OK;
}
