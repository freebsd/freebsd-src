/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include "charset/codecs/codec_impl.h"
#include "charset/encodings/utf8impl.h"
#include "utils/endian.h"
#include "utils/utils.h"

/**
 * UTF-8 charset codec
 */
typedef struct charset_utf8_codec {
	parserutils_charset_codec base;	/**< Base class */

#define INVAL_BUFSIZE (32)
	uint8_t inval_buf[INVAL_BUFSIZE];	/**< Buffer for fixing up
						 * incomplete input
						 * sequences */
	size_t inval_len;		/*< Byte length of inval_buf **/

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

} charset_utf8_codec;

static bool charset_utf8_codec_handles_charset(const char *charset);
static parserutils_error charset_utf8_codec_create(const char *charset,
		parserutils_charset_codec **codec);
static parserutils_error charset_utf8_codec_destroy(
		parserutils_charset_codec *codec);
static parserutils_error charset_utf8_codec_encode(
		parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static parserutils_error charset_utf8_codec_decode(
		parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static parserutils_error charset_utf8_codec_reset(
		parserutils_charset_codec *codec);
static inline parserutils_error charset_utf8_codec_read_char(
		charset_utf8_codec *c,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen);
static inline parserutils_error charset_utf8_codec_output_decoded_char(
		charset_utf8_codec *c,
		uint32_t ucs4, uint8_t **dest, size_t *destlen);

/**
 * Determine whether this codec handles a specific charset
 *
 * \param charset  Charset to test
 * \return true if handleable, false otherwise
 */
bool charset_utf8_codec_handles_charset(const char *charset)
{
	return parserutils_charset_mibenum_from_name(charset,
				strlen(charset)) ==
			parserutils_charset_mibenum_from_name("UTF-8",
				SLEN("UTF-8"));
}

/**
 * Create a UTF-8 codec
 *
 * \param charset  The charset to read from / write to
 * \param codec    Pointer to location to receive codec
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_BADPARM on bad parameters,
 *         PARSERUTILS_NOMEM on memory exhausion
 */
parserutils_error charset_utf8_codec_create(const char *charset,
		parserutils_charset_codec **codec)
{
	charset_utf8_codec *c;

	UNUSED(charset);

	c = malloc(sizeof(charset_utf8_codec));
	if (c == NULL)
		return PARSERUTILS_NOMEM;

	c->inval_buf[0] = '\0';
	c->inval_len = 0;

	c->read_buf[0] = 0;
	c->read_len = 0;

	c->write_buf[0] = 0;
	c->write_len = 0;

	/* Finally, populate vtable */
	c->base.handler.destroy = charset_utf8_codec_destroy;
	c->base.handler.encode = charset_utf8_codec_encode;
	c->base.handler.decode = charset_utf8_codec_decode;
	c->base.handler.reset = charset_utf8_codec_reset;

	*codec = (parserutils_charset_codec *) c;

	return PARSERUTILS_OK;
}

/**
 * Destroy a UTF-8 codec
 *
 * \param codec  The codec to destroy
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error charset_utf8_codec_destroy (parserutils_charset_codec *codec)
{
	UNUSED(codec);

	return PARSERUTILS_OK;
}

/**
 * Encode a chunk of UCS-4 (big endian) data into UTF-8
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return PARSERUTILS_OK          on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 *         PARSERUTILS_INVALID     if a character cannot be represented and the
 *                            codec's error handling mode is set to STRICT,
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
parserutils_error charset_utf8_codec_encode(parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	charset_utf8_codec *c = (charset_utf8_codec *) codec;
	uint32_t ucs4;
	uint32_t *towrite;
	size_t towritelen;
	parserutils_error error;

	/* Process any outstanding characters from the previous call */
	if (c->write_len > 0) {
		uint32_t *pwrite = c->write_buf;

		while (c->write_len > 0) {
			UTF8_FROM_UCS4(pwrite[0], dest, destlen, error);
			if (error != PARSERUTILS_OK) {
				uint32_t len;
				assert(error == PARSERUTILS_NOMEM);

				/* Insufficient output buffer space */
				for (len = 0; len < c->write_len; len++) {
					c->write_buf[len] = pwrite[len];
				}

				return PARSERUTILS_NOMEM;
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
			UTF8_FROM_UCS4(towrite[0], dest, destlen, error);
			if (error != PARSERUTILS_OK) {
				uint32_t len;
				assert(error == PARSERUTILS_NOMEM);

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
 * Decode a chunk of UTF-8 data into UCS-4 (big endian)
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return PARSERUTILS_OK          on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 *         PARSERUTILS_INVALID     if a character cannot be represented and the
 *                            codec's error handling mode is set to STRICT,
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
parserutils_error charset_utf8_codec_decode(parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	charset_utf8_codec *c = (charset_utf8_codec *) codec;
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

	if (c->inval_len > 0) {
		/* The last decode ended in an incomplete sequence.
		 * Fill up inval_buf with data from the start of the
		 * new chunk and process it. */
		uint8_t *in = c->inval_buf;
		size_t ol = c->inval_len;
		size_t l = min(INVAL_BUFSIZE - ol - 1, *sourcelen);
		size_t orig_l = l;

		memcpy(c->inval_buf + ol, *source, l);

		l += c->inval_len;

		error = charset_utf8_codec_read_char(c,
				(const uint8_t **) &in, &l, dest, destlen);
		if (error != PARSERUTILS_OK && error != PARSERUTILS_NOMEM) {
			return error;
		}

		/* And now, fix up source pointers */
		*source += max((signed) (orig_l - l), 0);
		*sourcelen -= max((signed) (orig_l - l), 0);

		/* Failed to resolve an incomplete character and
		 * ran out of buffer space. No recovery strategy
		 * possible, so explode everywhere. */
		assert((orig_l + ol) - l != 0);

		/* Report memory exhaustion case from above */
		if (error != PARSERUTILS_OK)
			return error;
	}

	/* Finally, the "normal" case; process all outstanding characters */
	while (*sourcelen > 0) {
		error = charset_utf8_codec_read_char(c,
				source, sourcelen, dest, destlen);
		if (error != PARSERUTILS_OK) {
			return error;
		}
	}

	return PARSERUTILS_OK;
}

/**
 * Clear a UTF-8 codec's encoding state
 *
 * \param codec  The codec to reset
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error charset_utf8_codec_reset(parserutils_charset_codec *codec)
{
	charset_utf8_codec *c = (charset_utf8_codec *) codec;

	c->inval_buf[0] = '\0';
	c->inval_len = 0;

	c->read_buf[0] = 0;
	c->read_len = 0;

	c->write_buf[0] = 0;
	c->write_len = 0;

	return PARSERUTILS_OK;
}


/**
 * Read a character from the UTF-8 to UCS-4 (big endian)
 *
 * \param c          The codec
 * \param source     Pointer to pointer to source buffer (updated on exit)
 * \param sourcelen  Pointer to length of source buffer (updated on exit)
 * \param dest       Pointer to pointer to output buffer (updated on exit)
 * \param destlen    Pointer to length of output buffer (updated on exit)
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_NOMEM       if output buffer is too small,
 *         PARSERUTILS_INVALID     if a character cannot be represented and the
 *                            codec's error handling mode is set to STRICT,
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
parserutils_error charset_utf8_codec_read_char(charset_utf8_codec *c,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	uint32_t ucs4;
	size_t sucs4;
	parserutils_error error;

	/* Convert a single character */
	{
		const uint8_t *src = *source;
		size_t srclen = *sourcelen;
		uint32_t *uptr = &ucs4;
		size_t *usptr = &sucs4;
		UTF8_TO_UCS4(src, srclen, uptr, usptr, error);
	}
	if (error == PARSERUTILS_OK) {
		/* Read a character */
		error = charset_utf8_codec_output_decoded_char(c,
				ucs4, dest, destlen);
		if (error == PARSERUTILS_OK || error == PARSERUTILS_NOMEM) {
			/* output succeeded; update source pointers */
			*source += sucs4;
			*sourcelen -= sucs4;
		}

		/* Clear inval buffer */
		c->inval_buf[0] = '\0';
		c->inval_len = 0;

		return error;
	} else if (error == PARSERUTILS_NEEDDATA) {
		/* Incomplete input sequence */
		assert(*sourcelen < INVAL_BUFSIZE);

		memmove(c->inval_buf, *source, *sourcelen);
		c->inval_buf[*sourcelen] = '\0';
		c->inval_len = *sourcelen;

		*source += *sourcelen;
		*sourcelen = 0;

		return PARSERUTILS_OK;
	} else if (error == PARSERUTILS_INVALID) {
		/* Illegal input sequence */
		uint32_t nextchar;

		/* Strict errormode; simply flag invalid character */
		if (c->base.errormode ==
				PARSERUTILS_CHARSET_CODEC_ERROR_STRICT) {
			/* Clear inval buffer */
			c->inval_buf[0] = '\0';
			c->inval_len = 0;

			return PARSERUTILS_INVALID;
		}

		/* Find next valid UTF-8 sequence.
		 * We're processing client-provided data, so let's
		 * be paranoid about its validity. */
		{
			const uint8_t *src = *source;
			size_t srclen = *sourcelen;
			uint32_t off = 0;
			uint32_t *ncptr = &nextchar;

			UTF8_NEXT_PARANOID(src, srclen, off, ncptr, error);
		}
		if (error != PARSERUTILS_OK) {
			if (error == PARSERUTILS_NEEDDATA) {
				/* Need more data to be sure */
				assert(*sourcelen < INVAL_BUFSIZE);

				memmove(c->inval_buf, *source, *sourcelen);
				c->inval_buf[*sourcelen] = '\0';
				c->inval_len = *sourcelen;

				*source += *sourcelen;
				*sourcelen = 0;

				nextchar = 0;
			} else {
				return error;
			}
		}

		/* Clear inval buffer */
		c->inval_buf[0] = '\0';
		c->inval_len = 0;

		/* output U+FFFD and continue processing. */
		error = charset_utf8_codec_output_decoded_char(c,
				0xFFFD, dest, destlen);
		if (error == PARSERUTILS_OK || error == PARSERUTILS_NOMEM) {
			/* output succeeded; update source pointers */
			*source += nextchar;
			*sourcelen -= nextchar;
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
parserutils_error charset_utf8_codec_output_decoded_char(charset_utf8_codec *c,
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


const parserutils_charset_handler charset_utf8_codec_handler = {
	charset_utf8_codec_handles_charset,
	charset_utf8_codec_create
};

