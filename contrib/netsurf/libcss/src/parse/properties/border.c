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
 * Parse border shorthand
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
css_error css__parse_border(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;

	error = css__parse_border_side(c, vector, ctx, result, BORDER_SIDE_TOP);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	*ctx = orig_ctx;
	error = css__parse_border_side(c, vector, ctx, result, BORDER_SIDE_RIGHT);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	*ctx = orig_ctx;
	error = css__parse_border_side(c, vector, ctx, result, BORDER_SIDE_BOTTOM);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	*ctx = orig_ctx;
	error = css__parse_border_side(c, vector, ctx, result, BORDER_SIDE_LEFT);
	if (error != CSS_OK) 
		*ctx = orig_ctx;
	
	return error;
		
}
