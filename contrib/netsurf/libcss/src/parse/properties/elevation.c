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
 * Parse elevation
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
css_error css__parse_elevation(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	css_fixed length = 0;
	uint32_t unit = 0;
	bool match;

	/* angle | IDENT(below, level, above, higher, lower, inherit) */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[INHERIT],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[BELOW],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_BELOW;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[LEVEL],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_LEVEL;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[ABOVE],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_ABOVE;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[HIGHER],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_HIGHER;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[LOWER],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = ELEVATION_LOWER;
	} else {
		error = css__parse_unit_specifier(c, vector, ctx, UNIT_DEG,
				&length, &unit);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		if ((unit & UNIT_ANGLE) == false) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}

		/* Valid angles lie between -90 and 90 degrees */
		if (unit == UNIT_DEG) {
			if (length < -F_90 || length > F_90) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else if (unit == UNIT_GRAD) {
			if (length < -F_100 || length > F_100) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else if (unit == UNIT_RAD) {
			if (length < -F_PI_2 || length > F_PI_2) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		}

		value = ELEVATION_ANGLE;
	}

	error = css__stylesheet_style_appendOPV(result, CSS_PROP_ELEVATION, flags, value);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	if (((flags & FLAG_INHERIT) == false) && (value == ELEVATION_ANGLE)) {
		error = css__stylesheet_style_vappend(result, 2, length, unit);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}
	}

	return CSS_OK;
}
