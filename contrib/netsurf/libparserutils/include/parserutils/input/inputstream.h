/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_input_inputstream_h_
#define parserutils_input_inputstream_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#ifndef NDEBUG
#include <stdio.h>
#endif
#include <stdlib.h>
#include <inttypes.h>

#include <parserutils/errors.h>
#include <parserutils/functypes.h>
#include <parserutils/types.h>
#include <parserutils/charset/utf8.h>
#include <parserutils/utils/buffer.h>

/**
 * Type of charset detection function
 */
typedef parserutils_error (*parserutils_charset_detect_func)(
		const uint8_t *data, size_t len, 
		uint16_t *mibenum, uint32_t *source);

/**
 * Input stream object
 */
typedef struct parserutils_inputstream 
{
	parserutils_buffer *utf8;	/**< Buffer containing UTF-8 data */

	uint32_t cursor;		/**< Byte offset of current position */

	bool had_eof;			/**< Whether EOF has been reached */
} parserutils_inputstream;

/* Create an input stream */
parserutils_error parserutils_inputstream_create(const char *enc,
		uint32_t encsrc, parserutils_charset_detect_func csdetect,
		parserutils_inputstream **stream);
/* Destroy an input stream */
parserutils_error parserutils_inputstream_destroy(
		parserutils_inputstream *stream);

/* Append data to an input stream */
parserutils_error parserutils_inputstream_append(
		parserutils_inputstream *stream,
		const uint8_t *data, size_t len);
/* Insert data into stream at current location */
parserutils_error parserutils_inputstream_insert(
		parserutils_inputstream *stream,
		const uint8_t *data, size_t len);

/* Slow form of css_inputstream_peek. */
parserutils_error parserutils_inputstream_peek_slow(
		parserutils_inputstream *stream, 
		size_t offset, const uint8_t **ptr, size_t *length);

/**
 * Look at the character in the stream that starts at 
 * offset bytes from the cursor
 *
 * \param stream  Stream to look in
 * \param offset  Byte offset of start of character
 * \param ptr     Pointer to location to receive pointer to character data
 * \param length  Pointer to location to receive character length (in bytes)
 * \return PARSERUTILS_OK on success, 
 *                    _NEEDDATA on reaching the end of available input,
 *                    _EOF on reaching the end of all input,
 *                    _BADENCODING if the input cannot be decoded,
 *                    _NOMEM on memory exhaustion,
 *                    _BADPARM if bad parameters are passed.
 *
 * Once the character pointed to by the result of this call has been advanced
 * past (i.e. parserutils_inputstream_advance has caused the stream cursor to 
 * pass over the character), then no guarantee is made as to the validity of 
 * the data pointed to. Thus, any attempt to dereference the pointer after 
 * advancing past the data it points to is a bug.
 */
static inline parserutils_error parserutils_inputstream_peek(
		parserutils_inputstream *stream, size_t offset, 
		const uint8_t **ptr, size_t *length)
{
	parserutils_error error = PARSERUTILS_OK;
	const parserutils_buffer *utf8;
	const uint8_t *utf8_data;
	size_t len, off, utf8_len;

	if (stream == NULL || ptr == NULL || length == NULL)
		return PARSERUTILS_BADPARM;

#ifndef NDEBUG
#ifdef VERBOSE_INPUTSTREAM
	fprintf(stdout, "Peek: len: %zu cur: %u off: %zu\n",
			stream->utf8->length, stream->cursor, offset);
#endif
#ifdef RANDOMISE_INPUTSTREAM
	parserutils_buffer_randomise(stream->utf8);
#endif
#endif

	utf8 = stream->utf8;
	utf8_data = utf8->data;
	utf8_len = utf8->length;
	off = stream->cursor + offset;

#define IS_ASCII(x) (((x) & 0x80) == 0)

	if (off < utf8_len) {
		if (IS_ASCII(utf8_data[off])) {
			/* Early exit for ASCII case */
			(*length) = 1;
			(*ptr) = (utf8_data + off);
			return PARSERUTILS_OK;
		} else {
			error = parserutils_charset_utf8_char_byte_length(
				utf8_data + off, &len);

			if (error == PARSERUTILS_OK) {
				(*length) = len;
				(*ptr) = (utf8_data + off);
				return PARSERUTILS_OK;
			} else if (error != PARSERUTILS_NEEDDATA) {
				return error;
			}
		}
	}

#undef IS_ASCII

	return parserutils_inputstream_peek_slow(stream, offset, ptr, length);
}

/**
 * Advance the stream's current position
 *
 * \param stream  The stream whose position to advance
 * \param bytes   The number of bytes to advance
 */
static inline void parserutils_inputstream_advance(
		parserutils_inputstream *stream, size_t bytes)
{
	if (stream == NULL)
		return;

#if !defined(NDEBUG) && defined(VERBOSE_INPUTSTREAM)
	fprintf(stdout, "Advance: len: %zu cur: %u bytes: %zu\n",
			stream->utf8->length, stream->cursor, bytes);
#endif

	if (bytes > stream->utf8->length - stream->cursor)
		bytes = stream->utf8->length - stream->cursor;

	if (stream->cursor == stream->utf8->length)
		return;

	stream->cursor += bytes;
}

/* Read the document charset */
const char *parserutils_inputstream_read_charset(
		parserutils_inputstream *stream, uint32_t *source);
/* Change the document charset */
parserutils_error parserutils_inputstream_change_charset(
		parserutils_inputstream *stream, 
		const char *enc, uint32_t source);

#ifdef __cplusplus
}
#endif

#endif

