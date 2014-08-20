/*
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * UTF-8 manipulation functions (interface).
 */

#ifndef _NETSURF_UTILS_UTF8_H_
#define _NETSURF_UTILS_UTF8_H_

#include <stdbool.h>
#include <stdint.h>

#include "utils/errors.h"

/**
 * Convert a UTF-8 multibyte sequence into a single UCS4 character
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param s_in  The sequence to process
 * \param l  Length of sequence
 * \return   UCS4 character
 */
uint32_t utf8_to_ucs4(const char *s, size_t l);

/**
 * Convert a single UCS4 character into a UTF-8 multibyte sequence
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This function conforms to RFC2279, however.
 *
 * \param c  The character to process (0 <= c <= 0x7FFFFFFF)
 * \param s  Pointer to 6 byte long output buffer
 * \return   Length of multibyte sequence
 */
size_t utf8_from_ucs4(uint32_t c, char *s);


/**
 * Calculate the length (in characters) of a NULL-terminated UTF-8 string
 *
 * \param s  The string
 * \return   Length of string
 */
size_t utf8_length(const char *s);

/**
 * Calculated the length (in characters) of a bounded UTF-8 string
 *
 * \param s  The string
 * \param l  Maximum length of input (in bytes)
 * \return Length of string, in characters
 */
size_t utf8_bounded_length(const char *s, size_t l);

/**
 * Calculate the length (in bytes) of a bounded UTF-8 string
 *
 * \param s  The string
 * \param l  Maximum length of input (in bytes)
 * \param c  Maximum number of characters to measure
 * \return Length of string, in bytes
 */
size_t utf8_bounded_byte_length(const char *s, size_t l, size_t c);

/**
 * Calculate the length (in bytes) of a UTF-8 character
 *
 * \param s  Pointer to start of character
 * \return Length of character, in bytes
 */
size_t utf8_char_byte_length(const char *s);


/**
 * Find previous legal UTF-8 char in string
 *
 * \param s  The string
 * \param o  Offset in the string to start at
 * \return Offset of first byte of previous legal character
 */
size_t utf8_prev(const char *s, size_t o);

/**
 * Find next legal UTF-8 char in string
 *
 * \param s  The string
 * \param l  Maximum offset in string
 * \param o  Offset in the string to start at
 * \return Offset of first byte of next legal character
 */
size_t utf8_next(const char *s, size_t l, size_t o);


/**
 * Convert a UTF8 string into the named encoding
 *
 * \param string  The NULL-terminated string to convert
 * \param encname The encoding name (suitable for passing to iconv)
 * \param len     Length of input string to consider (in bytes), or 0
 * \param result  Pointer to location to store result (allocated on heap)
 * \return standard nserror value
 */
nserror utf8_to_enc(const char *string, const char *encname,
		size_t len, char **result);

/**
 * Convert a string in the named encoding into a UTF-8 string
 *
 * \param string  The NULL-terminated string to convert
 * \param encname The encoding name (suitable for passing to iconv)
 * \param len     Length of input string to consider (in bytes), or 0
 * \param result  Pointer to location to store result (allocated on heap)
 * \return standard nserror value
 */
nserror utf8_from_enc(const char *string, const char *encname,
		size_t len, char **result, size_t *result_len);

/**
 * Convert a UTF-8 encoded string into a string of the given encoding,
 * applying HTML escape sequences where necessary.
 *
 * \param string   String to convert (NUL-terminated)
 * \param encname  Name of encoding to convert to
 * \param len      Length, in bytes, of the input string, or 0
 * \param result   Pointer to location to receive result
 * \return standard nserror code
 */
nserror utf8_to_html(const char *string, const char *encname,
		size_t len, char **result);

/**
 * Save the given utf8 text to a file, converting to local encoding.
 *
 * \param  utf8_text	text to save to file
 * \param  path		pathname to save to
 * \return true iff the save succeeded
 */
bool utf8_save_text(const char *utf8_text, const char *path);


/**
 * Finalise the UTF-8 library
 */
nserror utf8_finalise(void);

#endif
