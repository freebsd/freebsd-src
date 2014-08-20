/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_charset_encodings_utf8impl_h_
#define parserutils_charset_encodings_utf8impl_h_

/** \file
 * UTF-8 manipulation macros (implementation).
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/** Number of continuation bytes for a given start byte */
extern const uint8_t numContinuations[256];

/**
 * Convert a UTF-8 multibyte sequence into a single UCS-4 character
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This macro conforms to RFC2279, however.
 *
 * \param s      The sequence to process
 * \param len    Length of sequence
 * \param ucs4   Pointer to location to receive UCS-4 character (host endian)
 * \param clen   Pointer to location to receive byte length of UTF-8 sequence
 * \param error  Location to receive error code
 */
#define UTF8_TO_UCS4(s, len, ucs4, clen, error)				\
do {									\
	uint32_t c, min;						\
	uint8_t n;							\
	uint8_t i;							\
									\
	error = PARSERUTILS_OK;						\
									\
	if (s == NULL || ucs4 == NULL || clen == NULL) {		\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	if (len == 0) {							\
		error = PARSERUTILS_NEEDDATA;				\
		break;							\
	}								\
									\
	c = s[0];							\
									\
	if (c < 0x80) {							\
		n = 1;							\
		min = 0;						\
	} else if ((c & 0xE0) == 0xC0) {				\
		c &= 0x1F;						\
		n = 2;							\
		min = 0x80;						\
	} else if ((c & 0xF0) == 0xE0) {				\
		c &= 0x0F;						\
		n = 3;							\
		min = 0x800;						\
	} else if ((c & 0xF8) == 0xF0) {				\
		c &= 0x07;						\
		n = 4;							\
		min = 0x10000;						\
	} else if ((c & 0xFC) == 0xF8) {				\
		c &= 0x03;						\
		n = 5;							\
		min = 0x200000;						\
	} else if ((c & 0xFE) == 0xFC) {				\
		c &= 0x01;						\
		n = 6;							\
		min = 0x4000000;					\
	} else {							\
		error = PARSERUTILS_INVALID;				\
		break;							\
	}								\
									\
	if (len < n) {							\
		error = PARSERUTILS_NEEDDATA;				\
		break;							\
	}								\
									\
	for (i = 1; i < n; i++) {					\
		uint32_t t = s[i];					\
									\
		if ((t & 0xC0) != 0x80) {				\
			error = PARSERUTILS_INVALID;			\
			break;						\
		}							\
									\
		c <<= 6;						\
		c |= t & 0x3F;						\
	}								\
									\
	if (error == PARSERUTILS_OK) {					\
		/* Detect overlong sequences, surrogates and fffe/ffff */ \
		if (c < min || (c >= 0xD800 && c <= 0xDFFF) ||		\
				c == 0xFFFE || c == 0xFFFF) {		\
			error = PARSERUTILS_INVALID;			\
			break;						\
		}							\
									\
		*ucs4 = c;						\
		*clen = n;						\
	}								\
} while(0)

/**
 * Convert a single UCS-4 character into a UTF-8 multibyte sequence
 *
 * Encoding of UCS values outside the UTF-16 plane has been removed from
 * RFC3629. This macro conforms to RFC2279, however.
 *
 * \param ucs4   The character to process (0 <= c <= 0x7FFFFFFF) (host endian)
 * \param s      Pointer to pointer to output buffer, updated on exit
 * \param len    Pointer to length, in bytes, of output buffer, updated on exit
 * \param error  Location to receive error code
 */
#define UTF8_FROM_UCS4(ucs4, s, len, error)				\
do {									\
	uint8_t *buf;							\
	uint8_t l = 0;							\
									\
	error = PARSERUTILS_OK;						\
									\
	if (s == NULL || *s == NULL || len == NULL) {			\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	if (ucs4 < 0x80) {						\
		l = 1;							\
	} else if (ucs4 < 0x800) {					\
		l = 2;							\
	} else if (ucs4 < 0x10000) {					\
		l = 3;							\
	} else if (ucs4 < 0x200000) {					\
		l = 4;							\
	} else if (ucs4 < 0x4000000) {					\
		l = 5;							\
	} else if (ucs4 <= 0x7FFFFFFF) {				\
		l = 6;							\
	} else {							\
		error = PARSERUTILS_INVALID;				\
		break;							\
	}								\
									\
	if (l > *len) {							\
		error = PARSERUTILS_NOMEM;				\
		break;							\
	}								\
									\
	buf = *s;							\
									\
	if (l == 1) {							\
		buf[0] = (uint8_t) ucs4;				\
	} else {							\
		uint8_t i;						\
		for (i = l; i > 1; i--) {				\
			buf[i - 1] = 0x80 | (ucs4 & 0x3F);		\
			ucs4 >>= 6;					\
		}							\
		buf[0] = ~((1 << (8 - l)) - 1) | ucs4;			\
	}								\
									\
	*s += l;							\
	*len -= l;							\
} while(0)

/**
 * Calculate the length (in characters) of a bounded UTF-8 string
 *
 * \param s      The string
 * \param max    Maximum length
 * \param len    Pointer to location to receive length of string
 * \param error  Location to receive error code
 */
#define UTF8_LENGTH(s, max, len, error)					\
do {									\
	const uint8_t *end = s + max;					\
	int l = 0;							\
									\
	error = PARSERUTILS_OK;						\
									\
	if (s == NULL || len == NULL) {					\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	while (s < end) {						\
		uint32_t c = s[0];					\
									\
		if ((c & 0x80) == 0x00)					\
			s += 1;						\
		else if ((c & 0xE0) == 0xC0)				\
			s += 2;						\
		else if ((c & 0xF0) == 0xE0)				\
			s += 3;						\
		else if ((c & 0xF8) == 0xF0)				\
			s += 4;						\
		else if ((c & 0xFC) == 0xF8)				\
			s += 5;						\
		else if ((c & 0xFE) == 0xFC)				\
			s += 6;						\
		else {							\
			error = PARSERUTILS_INVALID;			\
			break;						\
		}							\
									\
		l++;							\
	}								\
									\
	if (error == PARSERUTILS_OK)					\
		*len = l;						\
} while(0)

/**
 * Calculate the length (in bytes) of a UTF-8 character
 *
 * \param s      Pointer to start of character
 * \param len    Pointer to location to receive length
 * \param error  Location to receive error code
 */
#define UTF8_CHAR_BYTE_LENGTH(s, len, error)				\
do {									\
	if (s == NULL || len == NULL) {					\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	*len = numContinuations[s[0]] + 1 /* Start byte */;		\
									\
	error = PARSERUTILS_OK;						\
} while(0)

/**
 * Find previous legal UTF-8 char in string
 *
 * \param s        The string
 * \param off      Offset in the string to start at
 * \param prevoff  Pointer to location to receive offset of first byte of
 *                 previous legal character
 * \param error    Location to receive error code
 */
#define UTF8_PREV(s, off, prevoff, error)				\
do {									\
	if (s == NULL || prevoff == NULL) {				\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	while (off != 0 && (s[--off] & 0xC0) == 0x80)			\
		/* do nothing */;					\
									\
	*prevoff = off;							\
									\
	error = PARSERUTILS_OK;						\
} while(0)

/**
 * Find next legal UTF-8 char in string
 *
 * \param s        The string (assumed valid)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \param error    Location to receive error code
 */
#define UTF8_NEXT(s, len, off, nextoff, error)				\
do {									\
	if (s == NULL || off >= len || nextoff == NULL) {		\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	/* Skip current start byte (if present - may be mid-sequence) */\
	if (s[off] < 0x80 || (s[off] & 0xC0) == 0xC0)			\
		off++;							\
									\
	while (off < len && (s[off] & 0xC0) == 0x80)			\
		off++;							\
									\
	*nextoff = off;							\
									\
	error = PARSERUTILS_OK;						\
} while(0)

/**
 * Skip to start of next sequence in UTF-8 input
 *
 * \param s        The string (assumed to be of dubious validity)
 * \param len      Maximum offset in string
 * \param off      Offset in the string to start at
 * \param nextoff  Pointer to location to receive offset of first byte of
 *                 next legal character
 * \param error    Location to receive error code
 */
#define UTF8_NEXT_PARANOID(s, len, off, nextoff, error)			\
do {									\
	uint8_t c;							\
									\
	error = PARSERUTILS_OK;						\
									\
	if (s == NULL || off >= len || nextoff == NULL) {		\
		error = PARSERUTILS_BADPARM;				\
		break;							\
	}								\
									\
	c = s[off];							\
									\
	/* If we're mid-sequence, simply advance to next byte */	\
	if (!(c < 0x80 || (c & 0xC0) == 0xC0)) {			\
		off++;							\
	} else {							\
		uint32_t nCont = numContinuations[c];			\
		uint32_t nToSkip;					\
									\
		if (off + nCont + 1 >= len) {				\
			error = PARSERUTILS_NEEDDATA;			\
			break;						\
		}							\
									\
		/* Verify continuation bytes */				\
		for (nToSkip = 1; nToSkip <= nCont; nToSkip++) {	\
			if ((s[off + nToSkip] & 0xC0) != 0x80)		\
				break;					\
		}							\
									\
		/* Skip over the valid bytes */				\
		off += nToSkip;						\
	}								\
									\
	*nextoff = off;							\
} while(0)

#endif
