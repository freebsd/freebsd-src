/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse quotes
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_quotes(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error = CSS_INVALID;
	const css_token *token;
	bool match;

	/* [ STRING STRING ]+ | IDENT(none,inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if ((token == NULL) ||
	    ((token->type != CSS_TOKEN_IDENT) &&
	     (token->type != CSS_TOKEN_STRING))) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if ((token->type == CSS_TOKEN_IDENT) &&
	    (lwc_string_caseless_isequal(token->idata,
			c->strings[INHERIT],
			&match) == lwc_error_ok && match)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_QUOTES);
	} else if ((token->type == CSS_TOKEN_IDENT) &&
		   (lwc_string_caseless_isequal(token->idata,
				c->strings[NONE],
				&match) == lwc_error_ok && match)) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_QUOTES, 0, QUOTES_NONE);
	} else if (token->type == CSS_TOKEN_STRING) {
		bool first = true;

/* Macro to output the value marker, awkward because we need to check
 * first to determine how the value is constructed.
 */
#define CSS_FIRST_APPEND(CSSVAL) css__stylesheet_style_append(result, first?buildOPV(CSS_PROP_QUOTES, 0, CSSVAL):CSSVAL)

		/* [ STRING STRING ]+ */
		while ((token != NULL) && (token->type == CSS_TOKEN_STRING)) {
			uint32_t open_snumber;
			uint32_t close_snumber;

			error = css__stylesheet_string_add(c->sheet, 
					lwc_string_ref(token->idata), 
					&open_snumber);
			if (error != CSS_OK) 
				break;

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);
			if ((token == NULL) || 
			    (token->type != CSS_TOKEN_STRING)) {
				error = CSS_INVALID;
				break;
			}

			error = css__stylesheet_string_add(c->sheet, 
					lwc_string_ref(token->idata), 
					&close_snumber);
			if (error != CSS_OK) 
				break;

			consumeWhitespace(vector, ctx);

			error = CSS_FIRST_APPEND(QUOTES_STRING);
			if (error != CSS_OK) 
				break;

			error = css__stylesheet_style_append(result, open_snumber);
			if (error != CSS_OK) 
				break;

			error = css__stylesheet_style_append(result, close_snumber);
			if (error != CSS_OK) 
				break;

			first = false;

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || token->type != CSS_TOKEN_STRING)
				break;
			token = parserutils_vector_iterate(vector, ctx);
		}

		if (error == CSS_OK) {
			/* AddTerminator */
			error = css__stylesheet_style_append(result, QUOTES_NONE);
		} 
	} else {
		error = CSS_INVALID;
	}

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}
