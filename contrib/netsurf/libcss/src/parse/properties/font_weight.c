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
 * Parse font-weight
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
css_error css__parse_font_weight(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	bool match;

	/* NUMBER (100, 200, 300, 400, 500, 600, 700, 800, 900) | 
	 * IDENT (normal, bold, bolder, lighter, inherit) */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_NUMBER)) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if ((lwc_string_caseless_isequal(
			token->idata, c->strings[INHERIT],
			&match) == lwc_error_ok && match)) {
		flags |= FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		css_fixed num = css__number_from_lwc_string(token->idata, 
				true, &consumed);
		/* Invalid if there are trailing characters */
		if (consumed != lwc_string_length(token->idata)) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}

		switch (FIXTOINT(num)) {
		case 100: value = FONT_WEIGHT_100; break;
		case 200: value = FONT_WEIGHT_200; break;
		case 300: value = FONT_WEIGHT_300; break;
		case 400: value = FONT_WEIGHT_400; break;
		case 500: value = FONT_WEIGHT_500; break;
		case 600: value = FONT_WEIGHT_600; break;
		case 700: value = FONT_WEIGHT_700; break;
		case 800: value = FONT_WEIGHT_800; break;
		case 900: value = FONT_WEIGHT_900; break;
		default: *ctx = orig_ctx; return CSS_INVALID;
		}
	} else if ((lwc_string_caseless_isequal(
			token->idata, c->strings[NORMAL],
			&match) == lwc_error_ok && match)) {
		value = FONT_WEIGHT_NORMAL;
	} else if ((lwc_string_caseless_isequal(
			token->idata, c->strings[BOLD],
			&match) == lwc_error_ok && match)) {
		value = FONT_WEIGHT_BOLD;
	} else if ((lwc_string_caseless_isequal(
			token->idata, c->strings[BOLDER],
			&match) == lwc_error_ok && match)) {
		value = FONT_WEIGHT_BOLDER;
	} else if ((lwc_string_caseless_isequal(
			token->idata, c->strings[LIGHTER],
			&match) == lwc_error_ok && match)) {
		value = FONT_WEIGHT_LIGHTER;
	} else {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	error = css__stylesheet_style_appendOPV(result,
					       CSS_PROP_FONT_WEIGHT,
					       flags,
					       value);
	if (error != CSS_OK) 
		*ctx = orig_ctx;
	

	return error;
}
