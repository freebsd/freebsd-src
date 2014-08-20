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
 * Parse border-style shorthand
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
css_error css__parse_border_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	int prev_ctx;
	const css_token *token;
	uint16_t side_val[4];
	uint32_t side_count = 0;
	bool match;
	css_error error;

	/* Firstly, handle inherit */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) 
		return CSS_INVALID;
		
	if (is_css_inherit(c, token)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_TOP_STYLE);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_RIGHT_STYLE);
		if (error != CSS_OK) 
			return error;		

		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_BOTTOM_STYLE);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_LEFT_STYLE);
		if (error == CSS_OK) 
			parserutils_vector_iterate(vector, ctx);

		return error;
	} 

	/* Attempt to parse up to 4 styles */
	do {
		prev_ctx = *ctx;

		if ((token != NULL) && is_css_inherit(c, token)) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}

		if (token->type != CSS_TOKEN_IDENT) 
			break;

		if ((lwc_string_caseless_isequal(token->idata, c->strings[NONE], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_NONE;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[HIDDEN], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_HIDDEN;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[DOTTED], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_DOTTED;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[DASHED], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_DASHED;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[SOLID], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_SOLID;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[LIBCSS_DOUBLE], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_DOUBLE;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[GROOVE], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_GROOVE;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[RIDGE], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_RIDGE;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[INSET], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_INSET;
		} else if ((lwc_string_caseless_isequal(token->idata, c->strings[OUTSET], &match) == lwc_error_ok && match)) {
			side_val[side_count] = BORDER_STYLE_OUTSET;
		} else {
			break;
		}

		side_count++;
				
		parserutils_vector_iterate(vector, ctx);
		
		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
	} while ((*ctx != prev_ctx) && (token != NULL) && (side_count < 4));


#define SIDE_APPEND(OP,NUM)								\
	error = css__stylesheet_style_appendOPV(result, (OP), 0, side_val[(NUM)]);	\
	if (error != CSS_OK)								\
		break 

	switch (side_count) {
	case 1:
		SIDE_APPEND(CSS_PROP_BORDER_TOP_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_RIGHT_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_BOTTOM_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_LEFT_STYLE, 0);
		break;
	case 2:
		SIDE_APPEND(CSS_PROP_BORDER_TOP_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_RIGHT_STYLE, 1);
		SIDE_APPEND(CSS_PROP_BORDER_BOTTOM_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_LEFT_STYLE, 1);
		break;
	case 3:
		SIDE_APPEND(CSS_PROP_BORDER_TOP_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_RIGHT_STYLE, 1);
		SIDE_APPEND(CSS_PROP_BORDER_BOTTOM_STYLE, 2);
		SIDE_APPEND(CSS_PROP_BORDER_LEFT_STYLE, 1);
		break;
	case 4:
		SIDE_APPEND(CSS_PROP_BORDER_TOP_STYLE, 0);
		SIDE_APPEND(CSS_PROP_BORDER_RIGHT_STYLE, 1);
		SIDE_APPEND(CSS_PROP_BORDER_BOTTOM_STYLE, 2);
		SIDE_APPEND(CSS_PROP_BORDER_LEFT_STYLE, 3);
		break;

	default:
		error = CSS_INVALID;
		break;
	}

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}
