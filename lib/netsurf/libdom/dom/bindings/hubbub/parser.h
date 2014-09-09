/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_hubbub_parser_h_
#define dom_hubbub_parser_h_

#include <stddef.h>
#include <inttypes.h>

#include <hubbub/errors.h>

#include <dom/dom.h>

#include "errors.h"

/**
 * Type of script completion function
 */
typedef dom_hubbub_error (*dom_script)(void *ctx, struct dom_node *node);

typedef struct dom_hubbub_parser dom_hubbub_parser;

/* The encoding source of the document */
typedef enum dom_hubbub_encoding_source {
	DOM_HUBBUB_ENCODING_SOURCE_HEADER,
	DOM_HUBBUB_ENCODING_SOURCE_DETECTED,
	DOM_HUBBUB_ENCODING_SOURCE_META
} dom_hubbub_encoding_source;

/* The recommended way to use the parser is:
 *
 * dom_hubbub_parser_create(...);
 * dom_hubbub_parser_parse_chunk(...);
 * call _parse_chunk for all chunks of data
 *
 * After you have parsed the data,
 *
 * dom_hubbub_parser_completed(...);
 * dom_hubbub_parser_destroy(...);
 *
 * Clients must ensure that these function calls above are called in
 * the order shown. dom_hubbub_parser_create() will pass the ownership
 * of the document to the client. After that, the parser should be destroyed.
 * The client must not call any method of this parser after destruction.
 */

/**
 * Parameter block for dom_hubbub_parser_create
 */
typedef struct dom_hubbub_parser_params {
	const char *enc; /**< Source charset, or NULL */
	bool fix_enc; /**< Whether fix the encoding */

	bool enable_script; /**< Whether scripting should be enabled. */
	dom_script script; /**< Script callback function */

	dom_msg msg; /**< Informational message function */
	void *ctx; /**< Pointer to client-specific private data */

	/** default action fetcher function */
	dom_events_default_action_fetcher daf;
} dom_hubbub_parser_params;

/* Create a Hubbub parser instance */
dom_hubbub_error dom_hubbub_parser_create(dom_hubbub_parser_params *params,
		dom_hubbub_parser **parser,
		dom_document **document);

/* Destroy a Hubbub parser instance */
void dom_hubbub_parser_destroy(dom_hubbub_parser *parser);

/* Parse a chunk of data */
dom_hubbub_error dom_hubbub_parser_parse_chunk(dom_hubbub_parser *parser,
		const uint8_t *data, size_t len);

/* insert data into the parse stream but do not parse it */
dom_hubbub_error dom_hubbub_parser_insert_chunk(dom_hubbub_parser *parser,
		const uint8_t *data, size_t length);

/* Notify parser that datastream is empty */
dom_hubbub_error dom_hubbub_parser_completed(dom_hubbub_parser *parser);

/* Retrieve the document's encoding */
const char *dom_hubbub_parser_get_encoding(dom_hubbub_parser *parser,
		dom_hubbub_encoding_source *source);

/**
 * Set the Parse pause state.
 *
 * \param parser  The parser object
 * \param pause   The pause state to set.
 * \return DOM_HUBBUB_OK on success,
 *         DOM_HUBBUB_HUBBUB_ERR | <hubbub_error> on failure
 */
dom_hubbub_error dom_hubbub_parser_pause(dom_hubbub_parser *parser, bool pause);

#endif
