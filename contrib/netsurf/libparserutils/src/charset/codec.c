/*
 * This file is part of LibParserUtils.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include "charset/aliases.h"
#include "charset/codecs/codec_impl.h"

extern parserutils_charset_handler charset_ascii_codec_handler;
extern parserutils_charset_handler charset_8859_codec_handler;
extern parserutils_charset_handler charset_ext8_codec_handler;
extern parserutils_charset_handler charset_utf8_codec_handler;
extern parserutils_charset_handler charset_utf16_codec_handler;

static parserutils_charset_handler *handler_table[] = {
	&charset_utf8_codec_handler,
	&charset_utf16_codec_handler,
	&charset_8859_codec_handler,
	&charset_ext8_codec_handler,
	&charset_ascii_codec_handler,
	NULL,
};

/**
 * Create a charset codec
 *
 * \param charset  Target charset
 * \param codec    Pointer to location to receive codec instance
 * \return PARSERUTILS_OK on success,
 *         PARSERUTILS_BADPARM on bad parameters,
 *         PARSERUTILS_NOMEM on memory exhaustion,
 *         PARSERUTILS_BADENCODING on unsupported charset
 */
parserutils_error parserutils_charset_codec_create(const char *charset,
		parserutils_charset_codec **codec)
{
	parserutils_charset_codec *c;
	parserutils_charset_handler **handler;
	const parserutils_charset_aliases_canon * canon;
	parserutils_error error;

	if (charset == NULL || codec == NULL)
		return PARSERUTILS_BADPARM;

	/* Canonicalise parserutils_charset name. */
	canon = parserutils__charset_alias_canonicalise(charset, 
			strlen(charset));
	if (canon == NULL)
		return PARSERUTILS_BADENCODING;

	/* Search for handler class */
	for (handler = handler_table; *handler != NULL; handler++) {
		if ((*handler)->handles_charset(canon->name))
			break;
	}

	/* None found */
	if ((*handler) == NULL)
		return PARSERUTILS_BADENCODING;

	/* Instantiate class */
	error = (*handler)->create(canon->name, &c);
	if (error != PARSERUTILS_OK)
		return error;

	/* and initialise it */
	c->mibenum = canon->mib_enum;

	c->errormode = PARSERUTILS_CHARSET_CODEC_ERROR_LOOSE;

	*codec = c;

	return PARSERUTILS_OK;
}

/**
 * Destroy a charset codec
 *
 * \param codec  The codec to destroy
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_codec_destroy(
		parserutils_charset_codec *codec)
{
	if (codec == NULL)
		return PARSERUTILS_BADPARM;

	codec->handler.destroy(codec);

	free(codec);

	return PARSERUTILS_OK;
}

/**
 * Configure a charset codec
 *
 * \param codec   The codec to configure
 * \param type    The codec option type to configure
 * \param params  Option-specific parameters
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_codec_setopt(
		parserutils_charset_codec *codec,
		parserutils_charset_codec_opttype type,
		parserutils_charset_codec_optparams *params)
{
	if (codec == NULL || params == NULL)
		return PARSERUTILS_BADPARM;

	switch (type) {
	case PARSERUTILS_CHARSET_CODEC_ERROR_MODE:
		codec->errormode = params->error_mode.mode;
		break;
	}

	return PARSERUTILS_OK;
}

/**
 * Encode a chunk of UCS-4 data into a codec's charset
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return PARSERUTILS_OK on success, appropriate error otherwise.
 *
 * source, sourcelen, dest and destlen will be updated appropriately on exit
 */
parserutils_error parserutils_charset_codec_encode(
		parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	if (codec == NULL || source == NULL || *source == NULL ||
			sourcelen == NULL || dest == NULL || *dest == NULL ||
			destlen == NULL)
		return PARSERUTILS_BADPARM;

	return codec->handler.encode(codec, source, sourcelen, dest, destlen);
}

/**
 * Decode a chunk of data in a codec's charset into UCS-4
 *
 * \param codec      The codec to use
 * \param source     Pointer to pointer to source data
 * \param sourcelen  Pointer to length (in bytes) of source data
 * \param dest       Pointer to pointer to output buffer
 * \param destlen    Pointer to length (in bytes) of output buffer
 * \return PARSERUTILS_OK on success, appropriate error otherwise.
 *
 * source, sourcelen, dest and destlen will be updated appropriately on exit
 *
 * Call this with a source length of 0 to flush any buffers.
 */
parserutils_error parserutils_charset_codec_decode(
		parserutils_charset_codec *codec,
		const uint8_t **source, size_t *sourcelen,
		uint8_t **dest, size_t *destlen)
{
	if (codec == NULL || source == NULL || *source == NULL ||
			sourcelen == NULL || dest == NULL || *dest == NULL ||
			destlen == NULL)
		return PARSERUTILS_BADPARM;

	return codec->handler.decode(codec, source, sourcelen, dest, destlen);
}

/**
 * Clear a charset codec's encoding state
 *
 * \param codec  The codec to reset
 * \return PARSERUTILS_OK on success, appropriate error otherwise
 */
parserutils_error parserutils_charset_codec_reset(
		parserutils_charset_codec *codec)
{
	if (codec == NULL)
		return PARSERUTILS_BADPARM;

	return codec->handler.reset(codec);
}

