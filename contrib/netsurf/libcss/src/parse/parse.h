/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_css__parse_parse_h_
#define css_css__parse_parse_h_

#include <libwapcaplet/libwapcaplet.h>

#include <parserutils/utils/vector.h>

#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/types.h>

typedef struct css_parser css_parser;

/**
 * Parser event types
 */
typedef enum css_parser_event {
	CSS_PARSER_START_STYLESHEET,
	CSS_PARSER_END_STYLESHEET,
	CSS_PARSER_START_RULESET,
	CSS_PARSER_END_RULESET,
	CSS_PARSER_START_ATRULE,
	CSS_PARSER_END_ATRULE,
	CSS_PARSER_START_BLOCK,
	CSS_PARSER_END_BLOCK,
	CSS_PARSER_BLOCK_CONTENT,
	CSS_PARSER_DECLARATION
} css_parser_event;

typedef css_error (*css_parser_event_handler)(css_parser_event type, 
		const parserutils_vector *tokens, void *pw);

/**
 * Parser option types
 */
typedef enum css_parser_opttype {
	CSS_PARSER_QUIRKS,
	CSS_PARSER_EVENT_HANDLER
} css_parser_opttype;

/**
 * Parser option parameters
 */
typedef union css_parser_optparams {
	bool quirks;

	struct {
		css_parser_event_handler handler;
		void *pw;
	} event_handler;
} css_parser_optparams;

css_error css__parser_create(const char *charset, css_charset_source cs_source,
		css_parser **parser);
css_error css__parser_create_for_inline_style(const char *charset, 
		css_charset_source cs_source, css_parser **parser);
css_error css__parser_destroy(css_parser *parser);

css_error css__parser_setopt(css_parser *parser, css_parser_opttype type,
		css_parser_optparams *params);

css_error css__parser_parse_chunk(css_parser *parser, const uint8_t *data, 
		size_t len);
css_error css__parser_completed(css_parser *parser);

const char *css__parser_read_charset(css_parser *parser, 
		css_charset_source *source);
bool css__parser_quirks_permitted(css_parser *parser);

#endif

