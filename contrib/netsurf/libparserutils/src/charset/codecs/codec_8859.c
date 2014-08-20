/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include "charset/codecs/codec_impl.h"
#include "utils/endian.h"
#include "utils/utils.h"

#include "charset/codecs/8859_tables.h"

static struct {
	uint16_t mib;
	const char *name;
	size_t len;
	uint32_t *table;
} known_charsets[] = {
	{ 0, "ISO-8859-1", SLEN("ISO-8859-1"), t1 },
	{ 0, "ISO-8859-2", SLEN("ISO-8859-2"), t2 },
	{ 0, "ISO-8859-3", SLEN("ISO-8859-3"), t3 },
	{ 0, "ISO-8859-4", SLEN("ISO-8859-4"), t4 },
	{ 0, "ISO-8859-5", SLEN("ISO-8859-5"), t5 },
	{ 0, "ISO-8859-6", SLEN("ISO-8859-6"), t6 },
	{ 0, "ISO-8859-7", SLEN("ISO-8859-7"), t7 },
	{ 0, "ISO-8859-8", SLEN("ISO-8859-8"), t8 },
	{ 0, "ISO-8859-9", SLEN("ISO-8859-9"), t9 },
	{ 0, "ISO-8859-10", SLEN("ISO-8859-10"), t10 },
	{ 0, "ISO-8859-11", SLEN("ISO-8859-11"), t11 },
	{ 0, "ISO-8859-13", SLEN("ISO-8859-13"), t13 },
	{ 0, "ISO-8859-14", SLEN("ISO-8859-14"), t14 },
	{ 0, "ISO-8859-15", SLEN("ISO-8859-15"), t15 },
	{ 0, "ISO-8859-16", SLEN("ISO-8859-16"), t16 }
};

/**
 * ISO-8859-n charset codec
 */
typedef struct charset_8859_codec {
	parserutils_charset_codec base;	/**< Base class */

	uint32_t *table;		/**< Mapping table for 0xA0-0xFF */

#define READ_BUFSIZE (8)
	uint32_t read_buf[READ_BUFSIZE];	/**< Buffer for partial
						 * output sequences (decode)
						 * (host-endian) */
	size_t read_len;		/**< Character length of read_buf */

#define WRITE_BUFSIZE (8)
	uint32_t write_buf[WRITE_BUFSIZE];	/**< Buffer for partial
						 * output sequences (encode)
						 * (host-endian) */
	size_t write_len;		/**< Character length of write_buf */

} charset_8859_codec;

static bool charset_8859_codec_handles_charset(const char *charset);
static parserutils_error charset_8859_codec_create(const char *charset,
		parserutils_charset_codec **codec);
static parserutils_error charset_8859_codec_destroy(
		parserutils_charset_codec *codec);
static parserutils_error charset_8859_codec_encode(
		parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static parserutils_error charset_8859_codec_decode(
		parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static parserutils_error charset_8859_codec_reset(
		parserutils_charset_codec *codec);
static inline parserutils_error charset_8859_codec_read_char(
		charset_8859_codec *c,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static inline parserutils_error charset_8859_codec_output_decoded_char(
		charset_8859_codec *c,
		uint32_t ucs4, uint8_t **dest, size_t *destlen);
static inline parserutils_error charset_8859_from_ucs4(charset_8859_codec *c,
		uint32_t ucs4, uint8_t **s, size_t *len);
static inline parserutils_error charset_8859_to_ucs4(charset_8859_codec *c,
		const uint8_t *s, size_t len, uint32_t *ucs4);

/**
 * Determine whether this codec handles a specific charset
 *
 * \param charset  Charset to test
 * \return true if handleable, false otherwise
 */
bool charset_8859_codec_handles_charset(const char *charset)
{
	uint32_t i;
	uint16_t match = parserutils_charset_mibenum_from_name(charset,
			strlen(charset));

	if (known_charsets[0].mib == 0) {
		for (i = 0; i < N_ELEMENTS(known_charsets); i++) {
			known_charsets[i].mib =
				parserutils_charset_mibenum_from_name(
						known_charsets[i].name,
						known_charsets[i].len);
		}
	}

	for (i = 0; i < N_ELEMENTS(known_charsets); i++) {
		if (known_charsets[i].mib == match)
			return true;
	}

	return false;
}

/**
 * Create an ISO-8859-n codec
 *
 * \param charset  The charset to read from / write to
 * \param codec    Pointer to location to receive codec
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_BADPARM on bad parameters,
 *         PARSERUTILS_NOMEM on memory exhausion
 */
parserutils_error charset_8859_codec_create(const char *charset,
		parserutils_charset_codec **codec)
{
	uint32_t i;
	charset_8859_codec *c;
	uint16_t match = parserutils_charset_mibenum_from_name(
			charset, strlen(charset));
	uint32_t *table = NULL;

	for (i = 0; i < N_ELEMENTS(known_charsets); i++) {
		if (known_charsets[i].mib == match) {
			table = known_charsets[i].table;
			break;
		}
	}

	assert(table != NULL);

	c = malloc(sizeof(charset_8859_codec));
	if (c == NULL)
		return PARSERUTILS_NOMEM;

	c->table = table;

	c->read_buf[0] = 0;
	c->read_len = 0;

	c->write_buf[0] = 0;
	c->write_len = 0;

	/* Finally, populate vtable */
	c->base.handler.destroy = charset_8859_codec_destroy;
	c->base.handler.encode = charset_8859_codec_encode;
	c->base.handler.decode = charset_8859_codec_decode;
	c->base.handler.reset = charset_8859_codec_reset;

	*codec = (parserutils_charset_codec *) c;

	return PARSERUTILS_OK;
}

/**
 * Destroy an ISO-8859-n codec
 *
 * \param codec  The codec to destroy
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error charset_8859_codec_destroy (parserutils_charset_codec *codec)
{
	UNUSED(codec);

	return PARSERUTILS_OK;
}

/**
 * Encode a chunk of UCS-4 (big endian) data into ISO-8859-n
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return PARSERUTILS_OK          on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 *         PARSERUTILS_INVALID     if a character cannot be represented and the
 *                                 codec's error handling mode is set to STRICT,
 *
 * On exit, ::source will point immediately _after_ the last input character
 * read. Any remaining output for the character will be buffered by the
 * codec for writing on the next call.
 *
 * Note that, if failure occurs whilst attempting to write any output
 * buffered by the last call, then ::source and ::sourcelen will remain
 * unchanged (as nothing more has been read).
 *
 * ::sourcelen will be reduced appropriately on exit.
 *
 * ::dest will point immediately _after_ the last character written.
 *
 * ::destlen will be reduced appropriately on exit.
 */
parserutils_error charset_8859_codec_encode(parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	charset_8859_codec *c = (charset_8859_codec *) codec;
	uint32_t ucs4;
	uint32_t *towrite;
	size_t towritelen;
	parserutils_error error;

	/* Process any outstanding characters from the previous call */
	if (c->write_len > 0) {
		uint32_t *pwrite = c->write_buf;

		while (c->write_len > 0) {
			error = charset_8859_from_ucs4(c, pwrite[0],
					dest, destlen);
			if (error != PARSERUTILS_OK) {
				uint32_t len;
				assert(error == PARSERUTILS_NOMEM);

				for (len = 0; len < c->write_len; len++) {
					c->write_buf[len] = pwrite[len];
				}

				return error;
			}

			pwrite++;
			c->write_len--;
		}
	}

	/* Now process the characters for this call */
	while (*sourcelen > 0) {
		ucs4 = endian_big_to_host(*((uint32_t *) (void *) *source));
		towrite = &ucs4;
		towritelen = 1;

		/* Output current characters */
		while (towritelen > 0) {
			error = charset_8859_from_ucs4(c, towrite[0], dest,
					destlen);
			if (error != PARSERUTILS_OK) {
				uint32_t len;
				if (error != PARSERUTILS_NOMEM) {
					return error;
				}

				/* Insufficient output space */
				assert(towritelen < WRITE_BUFSIZE);

				c->write_len = towritelen;

				/* Copy pending chars to save area, for
				 * processing next call. */
				for (len = 0; len < towritelen; len++)
					c->write_buf[len] = towrite[len];

				/* Claim character we've just buffered,
				 * so it's not reprocessed */
				*source += 4;
				*sourcelen -= 4;

				return PARSERUTILS_NOMEM;
			}

			towrite++;
			towritelen--;
		}

		*source += 4;
		*sourcelen -= 4;
	}

	return PARSERUTILS_OK;
}

/**
 * Decode a chunk of ISO-8859-n data into UCS-4 (big endian)
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return PARSERUTILS_OK          on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 *         PARSERUTILS_INVALID     if a character cannot be represented and the
 *                                 codec's error handling mode is set to STRICT,
 *
 * On exit, ::source will point immediately _after_ the last input character
 * read, if the result is _OK or _NOMEM. Any remaining output for the
 * character will be buffered by the codec for writing on the next call.
 *
 * In the case of the result being _INVALID, ::source will point _at_ the
 * last input character read; nothing will be written or buffered for the
 * failed character. It is up to the client to fix the cause of the failure
 * and retry the decoding process.
 *
 * Note that, if failure occurs whilst attempting to write any output
 * buffered by the last call, then ::source and ::sourcelen will remain
 * unchanged (as nothing more has been read).
 *
 * If STRICT error handling is configured and an illegal sequence is split
 * over two calls, then _INVALID will be returned from the second call,
 * but ::source will point mid-way through the invalid sequence (i.e. it
 * will be unmodified over the second call). In addition, the internal
 * incomplete-sequence buffer will be emptied, such that subsequent calls
 * will progress, rather than re-evaluating the same invalid sequence.
 *
 * ::sourcelen will be reduced appropriately on exit.
 *
 * ::dest will point immediately _after_ the last character written.
 *
 * ::destlen will be reduced appropriately on exit.
 *
 * Call this with a source length of 0 to flush the output buffer.
 */
parserutils_error charset_8859_codec_decode(parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	charset_8859_codec *c = (charset_8859_codec *) codec;
	parserutils_error error;

	if (c->read_len > 0) {
		/* Output left over from last decode */
		uint32_t *pread = c->read_buf;

		while (c->read_len > 0 && *destlen >= c->read_len * 4) {
			*((uint32_t *) (void *) *dest) =
					endian_host_to_big(pread[0]);

			*dest += 4;
			*destlen -= 4;

			pread++;
			c->read_len--;
		}

		if (*destlen < c->read_len * 4) {
			/* Ran out of output buffer */
			size_t i;

			/* Shuffle remaining output down */
			for (i = 0; i < c->read_len; i++)
				c->read_buf[i] = pread[i];

			return PARSERUTILS_NOMEM;
		}
	}

	/* Finally, the "normal" case; process all outstanding characters */
	while (*sourcelen > 0) {
		error = charset_8859_codec_read_char(c,
				source, sourcelen, dest, destlen);
		if (error != PARSERUTILS_OK) {
			return error;
		}
	}

	return PARSERUTILS_OK;
}

/**
 * Clear an ISO-8859-n codec's encoding state
 *
 * \param codec  The codec to reset
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error charset_8859_codec_reset(parserutils_charset_codec *codec)
{
	charset_8859_codec *c = (charset_8859_codec *) codec;

	c->read_buf[0] = 0;
	c->read_len = 0;

	c->write_buf[0] = 0;
	c->write_len = 0;

	return PARSERUTILS_OK;
}


/**
 * Read a character from the ISO-8859-n to UCS-4 (big endian)
 *
 * \param c          The codec
 * \param source     Pointer to pointer to source buffer (updated on exit)
 * \param sourcelen  Pointer to length of source buffer (updated on exit)
 * \param dest       Pointer to pointer to output buffer (updated on exit)
 * \param destlen    Pointer to length of output buffer (updated on exit)
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 *         PARSERUTILS_INVALID     if a character cannot be represented and the
 *                                 codec's error handling mode is set to STRICT,
 *
 * On exit, ::source will point immediately _after_ the last input character
 * read, if the result is _OK or _NOMEM. Any remaining output for the
 * character will be buffered by the codec for writing on the next call.
 *
 * In the case of the result being _INVALID, ::source will point _at_ the
 * last input character read; nothing will be written or buffered for the
 * failed character. It is up to the client to fix the cause of the failure
 * and retry the decoding process.
 *
 * ::sourcelen will be reduced appropriately on exit.
 *
 * ::dest will point immediately _after_ the last character written.
 *
 * ::destlen will be reduced appropriately on exit.
 */
parserutils_error charset_8859_codec_read_char(charset_8859_codec *c,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	uint32_t ucs4;
	parserutils_error error;

	/* Convert a single character */
	error = charset_8859_to_ucs4(c, *source, *sourcelen, &ucs4);
	if (error == PARSERUTILS_OK) {
		/* Read a character */
		error = charset_8859_codec_output_decoded_char(c,
				ucs4, dest, destlen);
		if (error == PARSERUTILS_OK || error == PARSERUTILS_NOMEM) {
			/* output succeeded; update source pointers */
			*source += 1;
			*sourcelen -= 1;
		}

		return error;
	} else if (error == PARSERUTILS_NEEDDATA) {
		/* Can only happen if sourcelen == 0 */
		return error;
	} else if (error == PARSERUTILS_INVALID) {
		/* Illegal input sequence */

		/* Strict errormode; simply flag invalid character */
		if (c->base.errormode ==
				PARSERUTILS_CHARSET_CODEC_ERROR_STRICT) {
			return PARSERUTILS_INVALID;
		}

		/* output U+FFFD and continue processing. */
		error = charset_8859_codec_output_decoded_char(c,
				0xFFFD, dest, destlen);
		if (error == PARSERUTILS_OK || error == PARSERUTILS_NOMEM) {
			/* output succeeded; update source pointers */
			*source += 1;
			*sourcelen -= 1;
		}

		return error;
	}

	return PARSERUTILS_OK;
}

/**
 * Output a UCS-4 character (big endian)
 *
 * \param c        Codec to use
 * \param ucs4     UCS-4 character (host endian)
 * \param dest     Pointer to pointer to output buffer
 * \param destlen  Pointer to output buffer length
 * \return PARSERUTILS_OK          on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 */
parserutils_error charset_8859_codec_output_decoded_char(charset_8859_codec *c,
		uint32_t ucs4, uint8_t **dest, size_t *destlen)
{
	if (*destlen < 4) {
		/* Run out of output buffer */
		c->read_len = 1;
		c->read_buf[0] = ucs4;

		return PARSERUTILS_NOMEM;
	}

	*((uint32_t *) (void *) *dest) = endian_host_to_big(ucs4);
	*dest += 4;
	*destlen -= 4;

	return PARSERUTILS_OK;
}

/**
 * Convert a UCS4 (host endian) character to ISO-8859-n
 *
 * \param c     The codec instance
 * \param ucs4  The UCS4 character to convert
 * \param s     Pointer to pointer to destination buffer
 * \param len   Pointer to destination buffer length
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_NOMEM if there's insufficient space in the output buffer,
 *         PARSERUTILS_INVALID if the character cannot be represented
 *
 * _INVALID will only be returned if the codec's conversion mode is STRICT.
 * Otherwise, '?' will be output.
 *
 * On successful conversion, *s and *len will be updated.
 */
parserutils_error charset_8859_from_ucs4(charset_8859_codec *c,
		uint32_t ucs4, uint8_t **s, size_t *len)
{
	uint8_t out = 0;

	if (*len < 1)
		return PARSERUTILS_NOMEM;

	if (ucs4 < 0x80) {
		/* ASCII */
		out = ucs4;
	} else {
		uint32_t i;

		for (i = 0; i < 96; i++) {
			if (ucs4 == c->table[i])
				break;
		}

		if (i == 96) {
			if (c->base.errormode ==
					PARSERUTILS_CHARSET_CODEC_ERROR_STRICT)
				return PARSERUTILS_INVALID;
			else
				out = '?';
		} else {
			out = 0xA0 + i;
		}
	}

	*(*s) = out;
	(*s)++;
	(*len)--;

	return PARSERUTILS_OK;
}

/**
 * Convert an ISO-8859-n character to UCS4 (host endian)
 *
 * \param c     The codec instance
 * \param s     Pointer to source buffer
 * \param len   Source buffer length
 * \param ucs4  Pointer to destination buffer
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_NEEDDATA if there's insufficient input data
 *         PARSERUTILS_INVALID if the character cannot be represented
 */
parserutils_error charset_8859_to_ucs4(charset_8859_codec *c,
		const uint8_t *s, size_t len, uint32_t *ucs4)
{
	uint32_t out;

	if (len < 1)
		return PARSERUTILS_NEEDDATA;

	if (*s < 0x80) {
		out = *s;
	} else if (*s >= 0xA0) {
		if (c->table[*s - 0xA0] == 0xFFFF)
			return PARSERUTILS_INVALID;

		out = c->table[*s - 0xA0];
	} else {
		return PARSERUTILS_INVALID;
	}

	*ucs4 = out;

	return PARSERUTILS_OK;
}

const parserutils_charset_handler charset_8859_codec_handler = {
	charset_8859_codec_handles_charset,
	charset_8859_codec_create
};

