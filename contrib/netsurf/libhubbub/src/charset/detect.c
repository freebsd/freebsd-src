/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include <hubbub/types.h>

#include "utils/utils.h"

#include "detect.h"

static uint16_t hubbub_charset_read_bom(const uint8_t *data, size_t len);
static uint16_t hubbub_charset_scan_meta(const uint8_t *data, size_t len);
static uint16_t hubbub_charset_parse_attributes(const uint8_t **pos,
		const uint8_t *end);
static bool hubbub_charset_get_attribute(const uint8_t **data,
		const uint8_t *end,
		const uint8_t **name, uint32_t *namelen,
		const uint8_t **value, uint32_t *valuelen);

/**
 * Extract a charset from a chunk of data
 *
 * \param data     Pointer to buffer containing data
 * \param len      Buffer length
 * \param mibenum  Pointer to location containing current MIB enum
 * \param source   Pointer to location containint current charset source
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 *
 * ::mibenum and ::source will be updated on exit
 *
 * The larger a chunk of data fed to this routine, the better, as it allows
 * charset autodetection access to a larger dataset for analysis.
 */
parserutils_error hubbub_charset_extract(const uint8_t *data, size_t len,
		uint16_t *mibenum, uint32_t *source)
{
	uint16_t charset = 0;

	if (data == NULL || mibenum == NULL || source == NULL)
		return PARSERUTILS_BADPARM;

	/**
	 * Meaning of *source on entry:
	 *
	 * CONFIDENT - Do not pass Go, do not attempt auto-detection.
	 * TENTATIVE - We've tried to autodetect already, but subsequently
	 *             discovered that we don't actually support the detected 
	 *             charset. Thus, we've defaulted to Windows-1252. Don't 
	 *             perform auto-detection again, as it would be futile.
	 *             (This bit diverges from the spec)
	 * UNKNOWN   - No autodetection performed yet. Get on with it.
	 */

	/* 1. */

	/* If the source is dictated, there's nothing for us to do */
	if (*source == HUBBUB_CHARSET_CONFIDENT || 
			*source == HUBBUB_CHARSET_TENTATIVE) {
		return PARSERUTILS_OK;
	}

	/* 2. */

	/** \todo We probably want to wait for ~512 bytes of data / 500ms here */

	/* 3. */

	/* We need at least 3 bytes of data */
	if (len < 3)
		goto default_encoding;

	/* First, look for a BOM */
	charset = hubbub_charset_read_bom(data, len);
	if (charset != 0) {
		*mibenum = charset;
		*source = HUBBUB_CHARSET_CONFIDENT;

		return PARSERUTILS_OK;
	}

	/* 4. */

	/* No BOM was found, so we must look for a meta charset within
	 * the document itself. */
	charset = hubbub_charset_scan_meta(data, len);
	if (charset != 0) {
		/* Fix charsets according to HTML5,
		 * section 8.2.2.2. Character encoding requirements */
		hubbub_charset_fix_charset(&charset);

		/* If we've encountered a meta charset for a non-ASCII-
		 * compatible encoding, don't trust it.
		 *
		 * Firstly, it should have been sent with a BOM (and thus
		 * detected above).
		 *
		 * Secondly, we've just used an ASCII-only parser to
		 * extract the encoding from the document. Therefore,
		 * the document plainly isn't what the meta charset
		 * claims it is.
		 *
		 * What we do in this case is to ignore the meta charset's
		 * claims and leave the charset determination to the
		 * autodetection routines (or the fallback case if they
		 * fail).
		 */
		if (charset != parserutils_charset_mibenum_from_name(
					"UTF-32", SLEN("UTF-32")) &&
			charset != parserutils_charset_mibenum_from_name(
					"UTF-32LE", SLEN("UTF-32LE")) &&
			charset != parserutils_charset_mibenum_from_name(
					"UTF-32BE", SLEN("UTF-32BE"))) {

			*mibenum = charset;
			*source = HUBBUB_CHARSET_TENTATIVE;

			return PARSERUTILS_OK;
		}
	}

	/* No charset was specified within the document, attempt to
	 * autodetect the encoding from the data that we have available. */

	/** \todo Charset autodetection */

	/* We failed to autodetect a charset, so use the default fallback */
default_encoding:

	/* 7. */

	charset = parserutils_charset_mibenum_from_name("Windows-1252",
			SLEN("Windows-1252"));
	if (charset == 0)
		charset = parserutils_charset_mibenum_from_name("ISO-8859-1",
				SLEN("ISO-8859-1"));

	*mibenum = charset;
	*source = HUBBUB_CHARSET_TENTATIVE;

	return PARSERUTILS_OK;
}


/**
 * Inspect the beginning of a buffer of data for the presence of a
 * UTF Byte Order Mark.
 *
 * \param data  Pointer to buffer containing data
 * \param len   Buffer length
 * \return MIB enum representing encoding described by BOM, or 0 if not found
 */
uint16_t hubbub_charset_read_bom(const uint8_t *data, size_t len)
{
	if (data == NULL)
		return 0;

	/* We require at least 3 bytes of data */
	if (len < 3)
		return 0;

	if (data[0] == 0xFE && data[1] == 0xFF) {
		return parserutils_charset_mibenum_from_name("UTF-16BE",
				SLEN("UTF-16BE"));
	} else if (data[0] == 0xFF && data[1] == 0xFE) {
		return parserutils_charset_mibenum_from_name("UTF-16LE",
				SLEN("UTF-16LE"));
	} else if (data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
		return parserutils_charset_mibenum_from_name("UTF-8",
				SLEN("UTF-8"));
	}

	return 0;
}

#define PEEK(a)								\
	(pos < end - SLEN(a) && 					\
		strncasecmp((const char *) pos, a, SLEN(a)) == 0)

#define ADVANCE(a)							\
	while (pos < end - SLEN(a)) {					\
		if (PEEK(a))						\
			break;						\
		pos++;							\
	}								\
									\
	if (pos == end - SLEN(a))					\
		return 0;

#define ISSPACE(a)							\
	(a == 0x09 || a == 0x0a || a == 0x0c ||				\
			a == 0x0d || a == 0x20 || a == 0x2f)

/**
 * Search for a meta charset within a buffer of data
 *
 * \param data  Pointer to buffer containing data
 * \param len   Length of buffer
 * \return MIB enum representing encoding, or 0 if none found
 */
uint16_t hubbub_charset_scan_meta(const uint8_t *data, size_t len)
{
	const uint8_t *pos = data;
	const uint8_t *end;
	uint16_t mibenum;

	if (data == NULL)
		return 0;

	end = pos + min(512, len);

	/* 1. */
	while (pos < end) {
		/* a */
		if (PEEK("<!--")) {
			pos += SLEN("<!--");
			ADVANCE("-->");
		/* b */
		} else if (PEEK("<meta")) {
			if (pos + SLEN("<meta") >= end - 1)
				return 0;

			if (ISSPACE(*(pos + SLEN("<meta")))) {
				/* 1 */
				pos += SLEN("<meta");

				mibenum = hubbub_charset_parse_attributes(
						&pos, end);
				if (mibenum != 0)
					return mibenum;

				if (pos >= end)
					return 0;
			}
		/* c */
		} else if ((PEEK("</") && (pos < end - 3 &&
				(0x41 <= (*(pos + 2) & ~ 0x20) &&
				(*(pos + 2) & ~ 0x20) <= 0x5A))) ||
				(pos < end - 2 && *pos == '<' &&
				(0x41 <= (*(pos + 1) & ~ 0x20) &&
				(*(pos + 1) & ~ 0x20) <= 0x5A))) {

			/* skip '<' */
			pos++;

			/* 1. */
			while (pos < end) {
				if (ISSPACE(*pos) ||
						*pos == '>' || *pos == '<')
					break;
				pos++;
			}

			if (pos >= end)
				return 0;

			/* 3 */
			if (*pos != '<') {
				const uint8_t *n;
				const uint8_t *v;
				uint32_t nl, vl;

				while (hubbub_charset_get_attribute(&pos, end,
						&n, &nl, &v, &vl))
					; /* do nothing */
			/* 2 */
			} else
				continue;
		/* d */
		} else if (PEEK("<!") || PEEK("</") || PEEK("<?")) {
			pos++;
			ADVANCE(">");
		}

		/* e - do nothing */

		/* 2 */
		pos++;
	}

	return 0;
}

/**
 * Parse attributes on a meta tag
 *
 * \param pos  Pointer to pointer to current location (updated on exit)
 * \param end  Pointer to end of data stream
 * \return MIB enum of detected encoding, or 0 if none found
 */
uint16_t hubbub_charset_parse_attributes(const uint8_t **pos,
		const uint8_t *end)
{
	const uint8_t *name;
	const uint8_t *value;
	uint32_t namelen, valuelen;
	uint16_t mibenum = 0;

	if (pos == NULL || *pos == NULL || end == NULL)
		return 0;

	/* 2 */
	while (hubbub_charset_get_attribute(pos, end,
			&name, &namelen, &value, &valuelen)) {
		/* 3 done by default */

		/* 4 */
		if (namelen == SLEN("charset") && valuelen > 0 &&
				strncasecmp((const char *) name, "charset",
					SLEN("charset")) == 0) {
			/* strip value */
			while (ISSPACE(*value)) {
				value++;
				valuelen--;
			}

			while (valuelen > 0 && ISSPACE(value[valuelen - 1]))
				valuelen--;

			mibenum = parserutils_charset_mibenum_from_name(
					(const char *) value, valuelen);
		/* 5 */
		} else if (namelen == SLEN("content") && valuelen > 0 &&
				strncasecmp((const char *) name, "content",
					SLEN("content")) == 0) {
			mibenum = hubbub_charset_parse_content(value,
					valuelen);
		}

		/* 6 */
		if (mibenum == parserutils_charset_mibenum_from_name(
				"UTF-16LE", SLEN("UTF-16LE")) ||
			mibenum ==
				parserutils_charset_mibenum_from_name(
				"UTF-16BE", SLEN("UTF-16BE")) ||
			mibenum ==
				parserutils_charset_mibenum_from_name(
				"UTF-16", SLEN("UTF-16"))) {
			mibenum = parserutils_charset_mibenum_from_name(
					"UTF-8", SLEN("UTF-8"));
		}

		/* 7 */
		if (mibenum != 0) {
			/* confidence = tentative; */
			return mibenum;
		}
	}

	return 0;
}

/**
 * Parse a content= attribute's value
 *
 * \param value     Attribute's value
 * \param valuelen  Length of value
 * \return MIB enum of detected encoding, or 0 if none found
 */
uint16_t hubbub_charset_parse_content(const uint8_t *value,
		uint32_t valuelen)
{
	const uint8_t *end;
	const uint8_t *tentative = NULL;
	uint32_t tentative_len = 0;

	if (value == NULL)
		return 0;

	end = value + valuelen;

	/* 1 */
	while (value < end) {
		if (*value == ';') {
			value++;
			break;
		}

		value++;
	}

	if (value >= end)
		return 0;

	/* 2 */
	while (value < end && ISSPACE(*value)) {
		value++;
	}

	if (value >= end)
		return 0;

	/* 3 */
	if (value < end - SLEN("charset") &&
			strncasecmp((const char *) value,
					"charset", SLEN("charset")) != 0)
		return 0;

	value += SLEN("charset");

	/* 4 */
	while (value < end && ISSPACE(*value)) {
		value++;
	}

	if (value >= end)
		return 0;

	/* 5 */
	if (*value != '=')
		return 0;
	/* skip '=' */
	value++;

	/* 6 */
	while (value < end && ISSPACE(*value)) {
		value++;
	}

	if (value >= end)
		return 0;

	/* 7 */
	tentative = value;

	/* a */
	if (*value == '"') {
		while (++value < end && *value != '"') {
			tentative_len++;
		}

		if (value < end)
			tentative++;
		else
			tentative = NULL;
	/* b */
	} else if (*value == '\'') {
		while (++value < end && *value != '\'') {
			tentative_len++;
		}

		if (value < end)
			tentative++;
		else
			tentative = NULL;
	/* c */
	} else {
		while (value < end && !ISSPACE(*value)) {
			value++;
			tentative_len++;
		}
	}

	/* 8 */
	if (tentative != NULL) {
		return parserutils_charset_mibenum_from_name(
				(const char *) tentative, tentative_len);
	}

	/* 9 */
	return 0;
}

/**
 * Extract an attribute from the data stream
 *
 * \param data      Pointer to pointer to current location (updated on exit)
 * \param end       Pointer to end of data stream
 * \param name      Pointer to location to receive attribute name
 * \param namelen   Pointer to location to receive attribute name length
 * \param value     Pointer to location to receive attribute value
 * \param valuelen  Pointer to location to receive attribute value langth
 * \return true if attribute extracted, false otherwise.
 *
 * Note: The caller should heed the returned lengths; these are the only
 * indicator that useful content resides in name or value.
 */
bool hubbub_charset_get_attribute(const uint8_t **data, const uint8_t *end,
		const uint8_t **name, uint32_t *namelen,
		const uint8_t **value, uint32_t *valuelen)
{
	const uint8_t *pos;

	if (data == NULL || *data == NULL || end == NULL || name == NULL ||
			namelen == NULL || value == NULL || valuelen == NULL)
		return false;

	pos = *data;

	/* 1. Skip leading spaces or '/' characters */
	while (pos < end && (ISSPACE(*pos) || *pos == '/')) {
		pos++;
	}

	if (pos >= end) {
		*data = pos;
		return false;
	}

	/* 2. Invalid element open character */
	if (*pos == '<') {
		pos--;
		*data = pos;
		return false;
	}

	/* 3. End of element */
	if (*pos == '>') {
		*data = pos;
		return false;
	}

	/* 4. Initialise name & value to empty string */
	*name = pos;
	*namelen = 0;
	*value = (const uint8_t *) "";
	*valuelen = 0;

	/* 5. Extract name */
	while (pos < end) {
		/* a */
		if (*pos == '=') {
			break;
		}

		/* b */
		if (ISSPACE(*pos)) {
			break;
		}

		/* c */
		if (*pos == '/' || *pos == '<' || *pos == '>') {
			*data = pos;
			return true;
		}

		/* d is handled by strncasecmp in _parse_attributes */

		/* e */
		(*namelen)++;

		/* 6 */
		pos++;
	}

	if (pos >= end) {
		*data = pos;
		return false;
	}

	if (ISSPACE(*pos)) {
		/* 7. Skip trailing spaces */
		while (pos < end && ISSPACE(*pos)) {
			pos++;
		}

		if (pos >= end) {
			*data = pos;
			return false;
		}

		/* 8. Must be '=' */
		if (*pos != '=') {
			pos--;
			*data = pos;
			return true;
		}
	}

	/* 9. Skip '=' */
	pos++;

	/* 10. Skip any spaces after '=' */
	while (pos < end && ISSPACE(*pos)) {
		pos++;
	}

	if (pos >= end) {
		*data = pos;
		return false;
	}

	/* 11. Extract value, if quoted */
	/* a */
	if (*pos == '\'' || *pos == '"') {
		/* 1 */
		const uint8_t *quote = pos;

		/* 2 */
		while (++pos < end) {
			/* 3 */
			if (*pos == *quote) {
				*value = (quote + 1);
				*data = ++pos;
				return true;
			}

			/* 4 is handled by strncasecmp */

			/* 5 */
			(*valuelen)++;

			/* 6 */
		}

		if (pos >= end) {
			*data = pos;
			return false;
		}
	}

	/* b */
	if (*pos == '<' || *pos == '>') {
		*data = pos;
		return true;
	}

	/* c is handled by strncasecmp */

	/* d */
	*value = pos;

	while (pos < end) {
		/* 12. Extract unquoted value */
		/* a */
		if (ISSPACE(*pos) || *pos == '<' || *pos == '>') {
			*data = pos;
			return true;
		}

		/* b is handled by strncasecmp */

		/* c */
		(*valuelen)++;

		/* 13. Advance */
		pos++;
	}

	if (pos >= end) {
		*data = pos;
		return false;
	}

	/* should never be reached */
	abort();

	return false;
}

/**
 * Fix charsets, according to the override table in HTML5,
 * section 8.2.2.2. Character encoding requirements
 * <http://www.whatwg.org/specs/web-apps/current-work/#character0>
 *
 * \param charset	Pointer to charset value to fix
 */
void hubbub_charset_fix_charset(uint16_t *charset)
{
	uint16_t tmp = 0;
	assert(*charset != 0);

	/* ISO-8859-1 -> Windows-1252 */
	if (*charset == parserutils_charset_mibenum_from_name(
			"ISO-8859-1", SLEN("ISO-8859-1"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"Windows-1252", SLEN("Windows-1252"));
		assert(tmp != 0 && "Windows-1252 MUST be supported");
	/* ISO-8859-9 -> Windows-1254 */
	} else if (*charset == parserutils_charset_mibenum_from_name(
			"ISO-8859-9", SLEN("ISO-8859-9"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"Windows-1254", SLEN("Windows-1254"));
	/* ISO-8859-11 -> Windows-874 */
	} else if (*charset == parserutils_charset_mibenum_from_name(
			"ISO-8859-11", SLEN("ISO-8859-11"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"Windows-874", SLEN("Windows-874"));
	/* KS_C_5601-1987 and EUC-KR -> Windows-949 */
	} else if (*charset == parserutils_charset_mibenum_from_name(
			"KS_C_5601-1987", SLEN("KS_C_5601-1987")) ||
			*charset == parserutils_charset_mibenum_from_name(
					"EUC-KR", SLEN("EUC-KR"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"Windows-949", SLEN("Windows-949"));
	/* TIS-620 -> Windows-874 */
	} else if (*charset == parserutils_charset_mibenum_from_name(
			"TIS-620", SLEN("TIS-620"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"Windows-874", SLEN("Windows-874"));
	/* x-x-big5 -> Big5 */
	} else if (*charset == parserutils_charset_mibenum_from_name(
			"x-x-big5", SLEN("x-x-big5"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"Big5", SLEN("Big5"));
	/* GB2312 and GB_2312-80 -> GBK */
	} else if (*charset == parserutils_charset_mibenum_from_name(
			"GB2312", SLEN("GB2312")) ||
			*charset == parserutils_charset_mibenum_from_name(
					"GB_2312-80", SLEN("GB_2312-80"))) {
		tmp = parserutils_charset_mibenum_from_name(
				"GBK", SLEN("GBK"));
	}

	if (tmp != 0)
		*charset = tmp;
}
