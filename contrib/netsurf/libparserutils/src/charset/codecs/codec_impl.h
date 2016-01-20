/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef parserutils_charset_codecs_codecimpl_h_
#define parserutils_charset_codecs_codecimpl_h_

#include <stdbool.h>
#include <inttypes.h>

#include <parserutils/charset/codec.h>

/**
 * Core charset codec definition; implementations extend this
 */
struct parserutils_charset_codec {
	uint16_t mibenum;			/**< MIB enum for charset */

	parserutils_charset_codec_errormode errormode;	/**< error mode */

	struct {
		parserutils_error (*destroy)(parserutils_charset_codec *codec);
		parserutils_error (*encode)(parserutils_charset_codec *codec,
				const uint8_t **source, size_t *sourcelen,
				uint8_t **dest, size_t *destlen);
		parserutils_error (*decode)(parserutils_charset_codec *codec,
				const uint8_t **source, size_t *sourcelen,
				uint8_t **dest, size_t *destlen);
		parserutils_error (*reset)(parserutils_charset_codec *codec);
	} handler; /**< Vtable for handler code */
};

/**
 * Codec factory component definition
 */
typedef struct parserutils_charset_handler {
	bool (*handles_charset)(const char *charset);
	parserutils_error (*create)(const char *charset,
			parserutils_charset_codec **codec);
} parserutils_charset_handler;

#endif
