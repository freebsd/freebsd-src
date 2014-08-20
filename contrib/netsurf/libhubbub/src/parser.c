/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007-8 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>
#include <parserutils/input/inputstream.h>

#include <hubbub/parser.h>

#include "charset/detect.h"
#include "tokeniser/tokeniser.h"
#include "treebuilder/treebuilder.h"
#include "utils/parserutilserror.h"

/**
 * Hubbub parser object
 */
struct hubbub_parser {
	parserutils_inputstream *stream;	/**< Input stream instance */
	hubbub_tokeniser *tok;		/**< Tokeniser instance */
	hubbub_treebuilder *tb;		/**< Treebuilder instance */
};

/**
 * Create a hubbub parser
 *
 * \param enc      Source document encoding, or NULL to autodetect
 * \param fix_enc  Permit fixing up of encoding if it's frequently misused
 * \param parser   Pointer to location to receive parser instance
 * \return HUBBUB_OK on success,
 *         HUBBUB_BADPARM on bad parameters,
 *         HUBBUB_NOMEM on memory exhaustion,
 *         HUBBUB_BADENCODING if ::enc is unsupported
 */
hubbub_error hubbub_parser_create(const char *enc, bool fix_enc,
		hubbub_parser **parser)
{
	parserutils_error perror;
	hubbub_error error;
	hubbub_parser *p;

	if (parser == NULL)
		return HUBBUB_BADPARM;

	p = malloc(sizeof(hubbub_parser));
	if (p == NULL)
		return HUBBUB_NOMEM;

	/* If we have an encoding and we're permitted to fix up likely broken
	 * ones, then attempt to do so. */
	if (enc != NULL && fix_enc == true) {
		uint16_t mibenum = parserutils_charset_mibenum_from_name(enc,
				strlen(enc));

		if (mibenum != 0) {
			hubbub_charset_fix_charset(&mibenum);

			enc = parserutils_charset_mibenum_to_name(mibenum);
		}
	}

	perror = parserutils_inputstream_create(enc,
		enc != NULL ? HUBBUB_CHARSET_CONFIDENT : HUBBUB_CHARSET_UNKNOWN,
		hubbub_charset_extract, &p->stream);
	if (perror != PARSERUTILS_OK) {
		free(p);
		return hubbub_error_from_parserutils_error(perror);
	}

	error = hubbub_tokeniser_create(p->stream, &p->tok);
	if (error != HUBBUB_OK) {
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return error;
	}

	error = hubbub_treebuilder_create(p->tok, &p->tb);
	if (error != HUBBUB_OK) {
		hubbub_tokeniser_destroy(p->tok);
		parserutils_inputstream_destroy(p->stream);
		free(p);
		return error;
	}

	*parser = p;

	return HUBBUB_OK;
}

/**
 * Destroy a hubbub parser
 *
 * \param parser  Parser instance to destroy
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_destroy(hubbub_parser *parser)
{
	if (parser == NULL)
		return HUBBUB_BADPARM;

	hubbub_treebuilder_destroy(parser->tb);

	hubbub_tokeniser_destroy(parser->tok);

	parserutils_inputstream_destroy(parser->stream);

	free(parser);

	return HUBBUB_OK;
}

/**
 * Configure a hubbub parser
 *
 * \param parser  Parser instance to configure
 * \param type    Option to set
 * \param params  Option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_setopt(hubbub_parser *parser,
		hubbub_parser_opttype type,
		hubbub_parser_optparams *params)
{
	hubbub_error result = HUBBUB_OK;

	if (parser == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_PARSER_TOKEN_HANDLER:
		if (parser->tb != NULL) {
			/* Client is defining their own token handler,
			 * so we must destroy the default treebuilder */
			hubbub_treebuilder_destroy(parser->tb);
			parser->tb = NULL;
		}
		result = hubbub_tokeniser_setopt(parser->tok,
				HUBBUB_TOKENISER_TOKEN_HANDLER,
				(hubbub_tokeniser_optparams *) params);
		break;

	case HUBBUB_PARSER_ERROR_HANDLER:
		/* The error handler does not cascade, so tell both the
		 * treebuilder (if extant) and the tokeniser. */
		if (parser->tb != NULL) {
			result = hubbub_treebuilder_setopt(parser->tb,
					HUBBUB_TREEBUILDER_ERROR_HANDLER,
					(hubbub_treebuilder_optparams *) params);
		}
		if (result == HUBBUB_OK) {
			result = hubbub_tokeniser_setopt(parser->tok,
					HUBBUB_TOKENISER_ERROR_HANDLER,
					(hubbub_tokeniser_optparams *) params);
		}
		break;

	case HUBBUB_PARSER_CONTENT_MODEL:
		result = hubbub_tokeniser_setopt(parser->tok,
				HUBBUB_TOKENISER_CONTENT_MODEL,
				(hubbub_tokeniser_optparams *) params);
		break;

	case HUBBUB_PARSER_PAUSE:
		result = hubbub_tokeniser_setopt(parser->tok,
				HUBBUB_TOKENISER_PAUSE,
				(hubbub_tokeniser_optparams *) params);
		break;

	case HUBBUB_PARSER_TREE_HANDLER:
		if (parser->tb != NULL) {
			result = hubbub_treebuilder_setopt(parser->tb,
					HUBBUB_TREEBUILDER_TREE_HANDLER,
					(hubbub_treebuilder_optparams *) params);
		}
		break;

	case HUBBUB_PARSER_DOCUMENT_NODE:
		if (parser->tb != NULL) {
			result = hubbub_treebuilder_setopt(parser->tb,
					HUBBUB_TREEBUILDER_DOCUMENT_NODE,
					(hubbub_treebuilder_optparams *) params);
		}
		break;

	case HUBBUB_PARSER_ENABLE_SCRIPTING:
		if (parser->tb != NULL) {
			result = hubbub_treebuilder_setopt(parser->tb,
					HUBBUB_TREEBUILDER_ENABLE_SCRIPTING,
					(hubbub_treebuilder_optparams *) params);
		}
		break;

	default:
		result = HUBBUB_INVALID;
	}

	return result;
}

/**
 * Insert a chunk of data into a hubbub parser input stream
 *
 * Inserts the given data into the input stream ready for parsing but
 * does not cause any additional processing of the input. This is
 * useful to allow hubbub callbacks to add computed data to the input.
 * 
 * \param parser  Parser instance to use
 * \param data    Data to parse (encoded in UTF-8)
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_insert_chunk(hubbub_parser *parser,
		const uint8_t *data, size_t len)
{
	if (parser == NULL || data == NULL)
		return HUBBUB_BADPARM;

	return hubbub_tokeniser_insert_chunk(parser->tok, data, len);
}

/**
 * Pass a chunk of data to a hubbub parser for parsing
 *
 * \param parser  Parser instance to use
 * \param data    Data to parse (encoded in the input charset)
 * \param len     Length, in bytes, of data
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_parse_chunk(hubbub_parser *parser,
		const uint8_t *data, size_t len)
{
	parserutils_error perror;
	hubbub_error error;

	if (parser == NULL || data == NULL)
		return HUBBUB_BADPARM;

	perror = parserutils_inputstream_append(parser->stream, data, len);
	if (perror != PARSERUTILS_OK)
		return hubbub_error_from_parserutils_error(perror);

	error = hubbub_tokeniser_run(parser->tok);
	if (error == HUBBUB_BADENCODING) {
		/* Ok, we autodetected an encoding that we don't actually
		 * support. We've not actually processed any data at this
		 * point so fall back to Windows-1252 and hope for the best
		 */
		perror = parserutils_inputstream_change_charset(parser->stream,
				"Windows-1252", HUBBUB_CHARSET_TENTATIVE);
		/* Under no circumstances should we get here if we've managed
		 * to process data. If there is a way, I want to know about it
		 */
		assert(perror != PARSERUTILS_INVALID);
		if (perror != PARSERUTILS_OK)
			return hubbub_error_from_parserutils_error(perror);

		/* Retry the tokenisation */
		error = hubbub_tokeniser_run(parser->tok);
	}

	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Inform the parser that the last chunk of data has been parsed
 *
 * \param parser  Parser to inform
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_parser_completed(hubbub_parser *parser)
{
	parserutils_error perror;
	hubbub_error error;

	if (parser == NULL)
		return HUBBUB_BADPARM;

	perror = parserutils_inputstream_append(parser->stream, NULL, 0);
	if (perror != PARSERUTILS_OK)
		return hubbub_error_from_parserutils_error(perror);

	error = hubbub_tokeniser_run(parser->tok);
	if (error != HUBBUB_OK)
		return error;

	return HUBBUB_OK;
}

/**
 * Read the document charset
 *
 * \param parser  Parser instance to query
 * \param source  Pointer to location to receive charset source
 * \return Pointer to charset name (constant; do not free), or NULL if unknown
 */
const char *hubbub_parser_read_charset(hubbub_parser *parser,
		hubbub_charset_source *source)
{
	const char *name;
	uint32_t src;

	if (parser == NULL || source == NULL)
		return NULL;

	name = parserutils_inputstream_read_charset(parser->stream, &src);

	*source = (hubbub_charset_source) src;

	return name;
}

