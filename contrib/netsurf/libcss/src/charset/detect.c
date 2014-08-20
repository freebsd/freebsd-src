/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdbool.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include "charset/detect.h"
#include "utils/utils.h"

static parserutils_error css_charset_read_bom_or_charset(const uint8_t *data, 
		size_t len, uint16_t *mibenum);
static parserutils_error try_utf32_charset(const uint8_t *data, 
		size_t len, uint16_t *result);
static parserutils_error try_utf16_charset(const uint8_t *data, 
		size_t len, uint16_t *result);
static parserutils_error try_ascii_compatible_charset(const uint8_t *data, 
		size_t len, uint16_t *result);

/**
 * Extract a charset from a chunk of data
 *
 * \param data     Pointer to buffer containing data
 * \param len      Buffer length
 * \param mibenum  Pointer to location containing current MIB enum
 * \param source   Pointer to location containing current charset source
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 *
 * ::mibenum and ::source will be updated on exit
 *
 * CSS 2.1 $4.4
 */
parserutils_error css__charset_extract(const uint8_t *data, size_t len,
		uint16_t *mibenum, uint32_t *source)
{
	parserutils_error error;
	uint16_t charset = 0;

	if (data == NULL || mibenum == NULL || source == NULL)
		return PARSERUTILS_BADPARM;

	/* If the charset was dictated by the client, we've nothing to detect */
	if (*source == CSS_CHARSET_DICTATED)
		return PARSERUTILS_OK;

	/* Look for a BOM and/or @charset */
	error = css_charset_read_bom_or_charset(data, len, &charset);
	if (error != PARSERUTILS_OK)
		return error;

	if (charset != 0) {
		*mibenum = charset;
		*source = CSS_CHARSET_DOCUMENT;

		return PARSERUTILS_OK;
	}

	/* If we've already got a charset from the linking mechanism or 
	 * referring document, then we've nothing further to do */
	if (*source != CSS_CHARSET_DEFAULT)
		return PARSERUTILS_OK;

	/* We've not yet found a charset, so use the default fallback */
	charset = parserutils_charset_mibenum_from_name("UTF-8", SLEN("UTF-8"));

	*mibenum = charset;
	*source = CSS_CHARSET_DEFAULT;

	return PARSERUTILS_OK;
}


/**
 * Inspect the beginning of a buffer of data for the presence of a
 * UTF Byte Order Mark and/or an @charset rule
 *
 * \param data     Pointer to buffer containing data
 * \param len      Buffer length
 * \param mibenum  Pointer to location to receive MIB enum
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error css_charset_read_bom_or_charset(const uint8_t *data, 
		size_t len, uint16_t *mibenum)
{
	parserutils_error error;
	uint16_t charset = 0;

	if (data == NULL)
		return PARSERUTILS_BADPARM;

	/* We require at least 4 bytes of data */
	if (len < 4)
		return PARSERUTILS_NEEDDATA;


	/* Look for BOM */
	if (data[0] == 0x00 && data[1] == 0x00 && 
			data[2] == 0xFE && data[3] == 0xFF) {
		charset = parserutils_charset_mibenum_from_name("UTF-32BE", 
				SLEN("UTF-32BE"));
	} else if (data[0] == 0xFF && data[1] == 0xFE &&
			data[2] == 0x00 && data[3] == 0x00) {
		charset = parserutils_charset_mibenum_from_name("UTF-32LE", 
				SLEN("UTF-32LE"));
	} else if (data[0] == 0xFE && data[1] == 0xFF) {
		charset = parserutils_charset_mibenum_from_name("UTF-16BE", 
				SLEN("UTF-16BE"));
	} else if (data[0] == 0xFF && data[1] == 0xFE) {
		charset = parserutils_charset_mibenum_from_name("UTF-16LE", 
				SLEN("UTF-16LE"));
	} else if (data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
		charset = parserutils_charset_mibenum_from_name("UTF-8", 
				SLEN("UTF-8"));
	}

	/* BOM beats @charset.
	 * UAs differ here, but none appear to match the spec. 
	 * The spec indicates that any @charset present in conjunction with a 
	 * BOM, should match the BOM. In reality, it appears UAs just take the 
	 * BOM as gospel and ignore any @charset rule. The w3c CSS validator 
	 * appears to do the same (at the least, it doesn't complain about a
	 * mismatch).
	 */
	if (charset != 0) {
		*mibenum = charset;
		return PARSERUTILS_OK;
	}

	error = try_utf32_charset(data, len, &charset);
	if (error == PARSERUTILS_OK && charset != 0) {
		*mibenum = charset;
		return PARSERUTILS_OK;
	}

	error = try_utf16_charset(data, len, &charset);
	if (error == PARSERUTILS_OK && charset != 0) {
		*mibenum = charset;
		return PARSERUTILS_OK;
	}

	error = try_ascii_compatible_charset(data, len, &charset);
	if (error == PARSERUTILS_OK)
		*mibenum = charset;

	return PARSERUTILS_OK;
}

static parserutils_error try_utf32_charset(const uint8_t *data, 
		size_t len, uint16_t *result)
{
	uint16_t charset = 0;

#define CHARSET_BE "\0\0\0@\0\0\0c\0\0\0h\0\0\0a\0\0\0r\0\0\0s\0\0\0e\0\0\0t\0\0\0 \0\0\0\""
#define CHARSET_LE "@\0\0\0c\0\0\0h\0\0\0a\0\0\0r\0\0\0s\0\0\0e\0\0\0t\0\0\0 \0\0\0\"\0\0\0"

	if (len <= SLEN(CHARSET_LE))
		return PARSERUTILS_NEEDDATA;

	/* Look for @charset, assuming UTF-32 source data */
	if (memcmp(data, CHARSET_LE, SLEN(CHARSET_LE)) == 0) {
		const uint8_t *start = data + SLEN(CHARSET_LE);
		const uint8_t *end;
		char buf[8];
		char *ptr = buf;

		/* Look for "; at end of charset declaration */
		for (end = start; end < data + len - 4; end += 4) {
			uint32_t c = end[0] | (end[1] << 8) | 
				     (end[2] << 16) | (end[3] << 24);

			/* Bail if non-ASCII */
			if (c > 0x007f)
				break;

			/* Reached the end? */
			if (c == '"' && end < data + len - 8) {
				uint32_t d = end[4] | (end[5] << 8) |
				    (end[6] << 16) | (end[7] << 24);

				if (d == ';')
					break;
			}

			/* Append to buf, if there's space */
			if ((size_t) (ptr - buf) < sizeof(buf)) {
				/* Uppercase */
				if ('a' <= c && c <= 'z')
					*ptr++ = c & ~0x20;
				else
					*ptr++ = c;
			}
		}

		if (end == data + len - 4) {
			/* Ran out of input */
			return PARSERUTILS_NEEDDATA;
		}

		/* Ensure we have something that looks like UTF-32(LE)? */
		if ((ptr - buf == SLEN("UTF-32LE") && 
				memcmp(buf, "UTF-32LE", ptr - buf) == 0) ||
				(ptr - buf == SLEN("UTF-32") &&
				memcmp(buf, "UTF-32", ptr - buf) == 0)) {
			/* Convert to MIB enum */
			charset = parserutils_charset_mibenum_from_name(
					"UTF-32LE", SLEN("UTF-32LE"));
		}
	} else if (memcmp(data, CHARSET_BE, SLEN(CHARSET_BE)) == 0) {
		const uint8_t *start = data + SLEN(CHARSET_BE);
		const uint8_t *end;
		char buf[8];
		char *ptr = buf;

		/* Look for "; at end of charset declaration */
		for (end = start; end < data + len - 4; end += 4) {
			uint32_t c = end[3] | (end[2] << 8) | 
				     (end[1] << 16) | (end[0] << 24);

			/* Bail if non-ASCII */
			if (c > 0x007f)
				break;

			/* Reached the end? */
			if (c == '"' && end < data + len - 8) {
				uint32_t d = end[7] | (end[6] << 8) |
				    (end[5] << 16) | (end[4] << 24);

				if (d == ';')
					break;
			}

			/* Append to buf, if there's space */
			if ((size_t) (ptr - buf) < sizeof(buf)) {
				/* Uppercase */
				if ('a' <= c && c <= 'z')
					*ptr++ = c & ~0x20;
				else
					*ptr++ = c;
			}
		}

		if (end == data + len - 4) {
			/* Ran out of input */
			return PARSERUTILS_NEEDDATA;
		}

		/* Ensure we have something that looks like UTF-32(BE)? */
		if ((ptr - buf == SLEN("UTF-32BE") && 
				memcmp(buf, "UTF-32BE", ptr - buf) == 0) ||
				(ptr - buf == SLEN("UTF-32") &&
				memcmp(buf, "UTF-32", ptr - buf) == 0)) {
			/* Convert to MIB enum */
			charset = parserutils_charset_mibenum_from_name(
					"UTF-32BE", SLEN("UTF-32BE"));
		}
	}

#undef CHARSET_LE
#undef CHARSET_BE

	*result = charset;

	return PARSERUTILS_OK;
}

static parserutils_error try_utf16_charset(const uint8_t *data, 
		size_t len, uint16_t *result)
{
	uint16_t charset = 0;

#define CHARSET_BE "\0@\0c\0h\0a\0r\0s\0e\0t\0 \0\""
#define CHARSET_LE "@\0c\0h\0a\0r\0s\0e\0t\0 \0\"\0"

	if (len <= SLEN(CHARSET_LE))
		return PARSERUTILS_NEEDDATA;

	/* Look for @charset, assuming UTF-16 source data */
	if (memcmp(data, CHARSET_LE, SLEN(CHARSET_LE)) == 0) {
		const uint8_t *start = data + SLEN(CHARSET_LE);
		const uint8_t *end;
		char buf[8];
		char *ptr = buf;

		/* Look for "; at end of charset declaration */
		for (end = start; end < data + len - 2; end += 2) {
			uint32_t c = end[0] | (end[1] << 8);

			/* Bail if non-ASCII */
			if (c > 0x007f)
				break;

			/* Reached the end? */
			if (c == '"' && end < data + len - 4) {
				uint32_t d = end[2] | (end[3] << 8);

				if (d == ';')
					break;
			}

			/* Append to buf, if there's space */
			if ((size_t) (ptr - buf) < sizeof(buf)) {
				/* Uppercase */
				if ('a' <= c && c <= 'z')
					*ptr++ = c & ~0x20;
				else
					*ptr++ = c;
			}
		}

		if (end == data + len - 2) {
			/* Ran out of input */
			return PARSERUTILS_NEEDDATA;
		}

		/* Ensure we have something that looks like UTF-16(LE)? */
		if ((ptr - buf == SLEN("UTF-16LE") && 
				memcmp(buf, "UTF-16LE", ptr - buf) == 0) ||
				(ptr - buf == SLEN("UTF-16") &&
				memcmp(buf, "UTF-16", ptr - buf) == 0)) {
			/* Convert to MIB enum */
			charset = parserutils_charset_mibenum_from_name(
					"UTF-16LE", SLEN("UTF-16LE"));
		}
	} else if (memcmp(data, CHARSET_BE, SLEN(CHARSET_BE)) == 0) {
		const uint8_t *start = data + SLEN(CHARSET_BE);
		const uint8_t *end;
		char buf[8];
		char *ptr = buf;

		/* Look for "; at end of charset declaration */
		for (end = start; end < data + len - 2; end += 2) {
			uint32_t c = end[1] | (end[0] << 8);

			/* Bail if non-ASCII */
			if (c > 0x007f)
				break;

			/* Reached the end? */
			if (c == '"' && end < data + len - 4) {
				uint32_t d = end[3] | (end[2] << 8);

				if (d == ';')
					break;
			}

			/* Append to buf, if there's space */
			if ((size_t) (ptr - buf) < sizeof(buf)) {
				/* Uppercase */
				if ('a' <= c && c <= 'z')
					*ptr++ = c & ~0x20;
				else
					*ptr++ = c;
			}
		}

		if (end == data + len - 2) {
			/* Ran out of input */
			return PARSERUTILS_NEEDDATA;
		}

		/* Ensure we have something that looks like UTF-16(BE)? */
		if ((ptr - buf == SLEN("UTF-16BE") && 
				memcmp(buf, "UTF-16BE", ptr - buf) == 0) ||
				(ptr - buf == SLEN("UTF-16") &&
				memcmp(buf, "UTF-16", ptr - buf) == 0)) {
			/* Convert to MIB enum */
			charset = parserutils_charset_mibenum_from_name(
					"UTF-16BE", SLEN("UTF-16BE"));
		}
	}

#undef CHARSET_LE
#undef CHARSET_BE

	*result = charset;

	return PARSERUTILS_OK;
}

parserutils_error try_ascii_compatible_charset(const uint8_t *data, size_t len,
		uint16_t *result)
{
	uint16_t charset = 0;

#define CHARSET "@charset \""

	if (len <= SLEN(CHARSET))
		return PARSERUTILS_NEEDDATA;

	/* Look for @charset, assuming ASCII-compatible source data */
	if (memcmp(data, CHARSET, SLEN(CHARSET)) == 0) {
		const uint8_t *start = data + SLEN(CHARSET);
		const uint8_t *end;

		/* Look for "; at end of charset declaration */
		for (end = start; end < data + len; end++) {
			if (*end == '"' && end < data + len - 1 && 
					*(end + 1) == ';')
				break;
		}

		if (end == data + len) {
			/* Ran out of input */
			return PARSERUTILS_NEEDDATA;
		}

		/* Convert to MIB enum */
		charset = parserutils_charset_mibenum_from_name(
				(const char *) start,  end - start);

		/* Any non-ASCII compatible charset must be ignored, as
		 * we've just used an ASCII parser to read it. */
		if (charset == parserutils_charset_mibenum_from_name(
					"UTF-32", SLEN("UTF-32")) ||
			charset == parserutils_charset_mibenum_from_name(
					"UTF-32LE", SLEN("UTF-32LE")) ||
			charset == parserutils_charset_mibenum_from_name(
					"UTF-32BE", SLEN("UTF-32BE")) ||
			charset == parserutils_charset_mibenum_from_name(
					"UTF-16", SLEN("UTF-16")) ||
			charset == parserutils_charset_mibenum_from_name(
					"UTF-16LE", SLEN("UTF-16LE")) ||
			charset == parserutils_charset_mibenum_from_name(
					"UTF-16BE", SLEN("UTF-16BE"))) {

			charset = 0;
		}
	}

#undef CHARSET

	*result = charset;

	return PARSERUTILS_OK;
}
