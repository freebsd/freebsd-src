/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file
 * UTF-8 manipulation functions (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/charset/utf8.h>
#include "charset/encodings/utf8impl.h"

/** Number of continuation bytes for a given start byte */
const uint8_t numContinuations[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
};

/**
 * Convert a UTF-8 multibyte sequence into a single UCS-4 character
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param s     The sequence to process
 * \param len   Length of sequence
 * \param ucs4  Pointer to location to receive UCS-4 character (host endian)
 * \param clen  Pointer to location to receive byte length of UTF-8 sequence
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_to_ucs4(const uint8_t *s, size_t len,
		uint32_t *ucs4, size_t *clen)
{
	parserutils_error error;

	UTF8_TO_UCS4(s, len, ucs4, clen, error);

	return error;
}

/**
 * Convert a single UCS-4 character into a UTF-8 multibyte sequence
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param ucs4  The character to process (0 <= c <= 0x7FFFFFFF) (host endian)
 * \param s     Pointer to pointer to output buffer, updated on exit
 * \param len   Pointer to length, in bytes, of output buffer, updated on exit
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_from_ucs4(uint32_t ucs4, 
		uint8_t **s, size_t *len)
{
	parserutils_error error;

	UTF8_FROM_UCS4(ucs4, s, len, error);

	return error;
}

/**
 * Calculate the length (in characters) of a bounded UTF-8 string
 *
 * \param s    The string
 * \param max  Maximum length
 * \param len  Pointer to location to receive length of string
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_length(const uint8_t *s, size_t max,
		size_t *len)
{
	parserutils_error error;

	UTF8_LENGTH(s, max, len, error);

	return error;
}

/**
 * Calculate the length (in bytes) of a UTF-8 character
 *
 * \param s    Pointer to start of character
 * \param len  Pointer to location to receive length
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_char_byte_length(const uint8_t *s,
		size_t *len)
{
	parserutils_error error;

	UTF8_CHAR_BYTE_LENGTH(s, len, error);

	return error;
}

/**
 * Find previous legal UTF-8 char in string
 *
 * \param s        The string
 * \param off      Offset in the string to start at
 * \param prevoff  Pointer to location to receive offset of first byte of
 *                 previous legal character
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_prev(const uint8_t *s, uint32_t off,
		uint32_t *prevoff)
{
	parserutils_error error;

	UTF8_PREV(s, off, prevoff, error);

	return error;
}

/**
 * Find next legal UTF-8 char in string
 *
 * \param s        The string (assumed valid)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_next(const uint8_t *s, uint32_t len,
		uint32_t off, uint32_t *nextoff)
{
	parserutils_error error;

	UTF8_NEXT(s, len, off, nextoff, error);

	return error;
}

/**
 * Find next legal UTF-8 char in string
 *
 * \param s        The string (assumed to be of dubious validity)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf8_next_paranoid(const uint8_t *s, 
		uint32_t len, uint32_t off, uint32_t *nextoff)
{
	parserutils_error error;

	UTF8_NEXT_PARANOID(s, len, off, nextoff, error);

	return error;
}

