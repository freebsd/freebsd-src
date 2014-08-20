/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

/** \file
 * UTF-16 manipulation functions (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/charset/utf16.h>

/**
 * Convert a UTF-16 sequence into a single UCS-4 character
 *
 * \param s     The sequence to process
 * \param len   Length of sequence in bytes
 * \param ucs4  Pointer to location to receive UCS-4 character (host endian)
 * \param clen  Pointer to location to receive byte length of UTF-16 sequence
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_to_ucs4(const uint8_t *s, 
		size_t len, uint32_t *ucs4, size_t *clen)
{
	const uint16_t *ss = (const uint16_t *) (const void *) s;

	if (s == NULL || ucs4 == NULL || clen == NULL)
		return PARSERUTILS_BADPARM;

	if (len < 2)
		return PARSERUTILS_NEEDDATA;

	if (*ss < 0xD800 || *ss > 0xDFFF) {
		*ucs4 = *ss;
		*clen = 2;
	} else if (0xD800 <= *ss && *ss <= 0xDBFF) {
		/* High-surrogate code unit.  */
		if (len < 4)
			return PARSERUTILS_NEEDDATA;

		if (0xDC00 <= ss[1] && ss[1] <= 0xDFFF) {
			/* We have a valid surrogate pair.  */
			*ucs4 = (((ss[0] & 0x3FF) << 10) | (ss[1] & 0x3FF))
				+ (1<<16);
			*clen = 4;
		} else {
			return PARSERUTILS_INVALID;
		}
	} else {
		/* Low-surrogate code unit.  */
		return PARSERUTILS_INVALID;
	}

	return PARSERUTILS_OK;
}

/**
 * Convert a single UCS-4 character into a UTF-16 sequence
 *
 * \param ucs4  The character to process (0 <= c <= 0x7FFFFFFF) (host endian)
 * \param s     Pointer to 4 byte long output buffer
 * \param len   Pointer to location to receive length of multibyte sequence
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_from_ucs4(uint32_t ucs4, uint8_t *s,
		size_t *len)
{
	uint16_t *ss = (uint16_t *) (void *) s;
	uint32_t l = 0;

	if (s == NULL || len == NULL)
		return PARSERUTILS_BADPARM;
	else if (ucs4 < 0x10000) {
		*ss = (uint16_t) ucs4;
		l = 2;
	} else if (ucs4 < 0x110000) {
		ss[0] = 0xD800 | (((ucs4 >> 16) & 0x1f) - 1) | (ucs4 >> 10);
		ss[1] = 0xDC00 | (ucs4 & 0x3ff);
		l = 4;
	} else {
		return PARSERUTILS_INVALID;
	}

	*len = l;

	return PARSERUTILS_OK;
}

/**
 * Calculate the length (in characters) of a bounded UTF-16 string
 *
 * \param s    The string
 * \param max  Maximum length
 * \param len  Pointer to location to receive length of string
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_length(const uint8_t *s, size_t max,
		size_t *len)
{
	const uint16_t *ss = (const uint16_t *) (const void *) s;
	const uint16_t *end = (const uint16_t *) (const void *) (s + max);
	int l = 0;

	if (s == NULL || len == NULL)
		return PARSERUTILS_BADPARM;

	while (ss < end) {
		if (*ss < 0xD800 || 0xDFFF < *ss)
			ss++;
		else
			ss += 2;

		l++;
	}

	*len = l;

	return PARSERUTILS_OK;
}

/**
 * Calculate the length (in bytes) of a UTF-16 character
 *
 * \param s    Pointer to start of character
 * \param len  Pointer to location to receive length
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_char_byte_length(const uint8_t *s,
		size_t *len)
{
	const uint16_t *ss = (const uint16_t *) (const void *) s;

	if (s == NULL || len == NULL)
		return PARSERUTILS_BADPARM;

	if (*ss < 0xD800 || 0xDFFF < *ss)
		*len = 2;
	else
		*len = 4;

	return PARSERUTILS_OK;
}

/**
 * Find previous legal UTF-16 char in string
 *
 * \param s        The string
 * \param off      Offset in the string to start at
 * \param prevoff  Pointer to location to receive offset of first byte of
 *                 previous legal character
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_prev(const uint8_t *s, uint32_t off,
		uint32_t *prevoff)
{
	const uint16_t *ss = (const uint16_t *) (const void *) s;

	if (s == NULL || prevoff == NULL)
		return PARSERUTILS_BADPARM;

	if (off < 2)
		*prevoff = 0;
	else if (ss[-1] < 0xDC00 || ss[-1] > 0xDFFF)
		*prevoff = off - 2;
	else
		*prevoff = (off < 4) ? 0 : off - 4;

	return PARSERUTILS_OK;
}

/**
 * Find next legal UTF-16 char in string
 *
 * \param s        The string (assumed valid)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_next(const uint8_t *s, uint32_t len,
		uint32_t off, uint32_t *nextoff)
{
	const uint16_t *ss = (const uint16_t *) (const void *) s;

	if (s == NULL || off >= len || nextoff == NULL)
		return PARSERUTILS_BADPARM;

	if (len - off < 4)
		*nextoff = len;
	else if (ss[1] < 0xD800 || ss[1] > 0xDBFF)
		*nextoff = off + 2;
	else
		*nextoff = (len - off < 6) ? len : off + 4;

	return PARSERUTILS_OK;
}

/**
 * Find next legal UTF-16 char in string
 *
 * \param s        The string (assumed to be of dubious validity)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_utf16_next_paranoid(const uint8_t *s,
		uint32_t len, uint32_t off, uint32_t *nextoff)
{
	const uint16_t *ss = (const uint16_t *) (const void *) s;

	if (s == NULL || off >= len || nextoff == NULL)
		return PARSERUTILS_BADPARM;

	while (1) {
		if (len - off < 4) {
			return PARSERUTILS_NEEDDATA;
		} else if (ss[1] < 0xD800 || ss[1] > 0xDFFF) {
			*nextoff = off + 2;
			break;
		} else if (ss[1] >= 0xD800 && ss[1] <= 0xDBFF) {
			if (len - off < 6)
				return PARSERUTILS_NEEDDATA;

			if (ss[2] >= 0xDC00 && ss[2] <= 0xDFFF) {
				*nextoff = off + 4;
				break;
			} else {
				ss++;
				off += 2;
			}
		}
	}

	return PARSERUTILS_OK;
}

