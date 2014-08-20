/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_lex_lex_h_
#define css_lex_lex_h_

#include <libwapcaplet/libwapcaplet.h>

#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/types.h>

#include <parserutils/input/inputstream.h>

typedef struct css_lexer css_lexer;

/**
 * Lexer option types
 */
typedef enum css_lexer_opttype {
	CSS_LEXER_EMIT_COMMENTS
} css_lexer_opttype;

/**
 * Lexer option parameters
 */
typedef union css_lexer_optparams {
	bool emit_comments;
} css_lexer_optparams;

/**
 * Token type
 */
typedef enum css_token_type { 
	CSS_TOKEN_IDENT, CSS_TOKEN_ATKEYWORD, CSS_TOKEN_HASH,
	CSS_TOKEN_FUNCTION, CSS_TOKEN_STRING, CSS_TOKEN_INVALID_STRING, 
	CSS_TOKEN_URI, CSS_TOKEN_UNICODE_RANGE, CSS_TOKEN_CHAR, 
	CSS_TOKEN_NUMBER, CSS_TOKEN_PERCENTAGE, CSS_TOKEN_DIMENSION,

	/* Those tokens that want strings interned appear above */
	CSS_TOKEN_LAST_INTERN,

 	CSS_TOKEN_CDO, CSS_TOKEN_CDC, CSS_TOKEN_S, CSS_TOKEN_COMMENT, 
	CSS_TOKEN_INCLUDES, CSS_TOKEN_DASHMATCH, CSS_TOKEN_PREFIXMATCH, 
	CSS_TOKEN_SUFFIXMATCH, CSS_TOKEN_SUBSTRINGMATCH, CSS_TOKEN_EOF 
} css_token_type;

/**
 * Token object
 */
typedef struct css_token {
	css_token_type type;

        struct {
                uint8_t *data;
                size_t len;
        } data;

	lwc_string *idata;
	
	uint32_t col;
	uint32_t line;
} css_token;

css_error css__lexer_create(parserutils_inputstream *input, css_lexer **lexer);
css_error css__lexer_destroy(css_lexer *lexer);

css_error css__lexer_setopt(css_lexer *lexer, css_lexer_opttype type, 
		css_lexer_optparams *params);

css_error css__lexer_get_token(css_lexer *lexer, css_token **token);

#endif

