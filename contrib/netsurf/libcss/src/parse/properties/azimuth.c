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
 * Parse azimuth
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  style to place resulting bytcode in
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_azimuth(css_language *c, 
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

	/* angle | [ IDENT(left-side, far-left, left, center-left, center, 
	 *		   center-right, right, far-right, right-side) || 
	 *	   IDENT(behind) 
	 *	 ] 
	 *	 | IDENT(leftwards, rightwards, inherit)
	 */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(token->idata, c->strings[INHERIT],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		flags = FLAG_INHERIT;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(token->idata, c->strings[LEFTWARDS],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = AZIMUTH_LEFTWARDS;
	} else if (token->type == CSS_TOKEN_IDENT &&
		(lwc_string_caseless_isequal(token->idata, c->strings[RIGHTWARDS],
			&match) == lwc_error_ok && match)) {
		parserutils_vector_iterate(vector, ctx);
		value = AZIMUTH_RIGHTWARDS;
	} else if (token->type == CSS_TOKEN_IDENT) {
		parserutils_vector_iterate(vector, ctx);

		/* Now, we may have one of the other keywords or behind,
		 * potentially followed by behind or other keyword, 
		 * respectively */
		if ((lwc_string_caseless_isequal(
				token->idata, c->strings[LEFT_SIDE],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_LEFT_SIDE;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[FAR_LEFT],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_FAR_LEFT;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[LEFT],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_LEFT;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[CENTER_LEFT],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_CENTER_LEFT;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[CENTER],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_CENTER;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[CENTER_RIGHT],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_CENTER_RIGHT;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[RIGHT],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_RIGHT;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[FAR_RIGHT],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_FAR_RIGHT;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[RIGHT_SIDE],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_RIGHT_SIDE;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[BEHIND],
				&match) == lwc_error_ok && match)) {
			value = AZIMUTH_BEHIND;
		} else {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}

		consumeWhitespace(vector, ctx);

		/* Get potential following token */
		token = parserutils_vector_peek(vector, *ctx);

		if (token != NULL && token->type == CSS_TOKEN_IDENT &&
				value == AZIMUTH_BEHIND) {
			parserutils_vector_iterate(vector, ctx);

			if ((lwc_string_caseless_isequal(
					token->idata, c->strings[LEFT_SIDE],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_LEFT_SIDE;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[FAR_LEFT],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_FAR_LEFT;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[LEFT],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_LEFT;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[CENTER_LEFT],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_CENTER_LEFT;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[CENTER],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_CENTER;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[CENTER_RIGHT],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_CENTER_RIGHT;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[RIGHT],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_RIGHT;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[FAR_RIGHT],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_FAR_RIGHT;
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[RIGHT_SIDE],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_RIGHT_SIDE;
			} else {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else if (token != NULL && token->type == CSS_TOKEN_IDENT &&
				value != AZIMUTH_BEHIND) {
			parserutils_vector_iterate(vector, ctx);

			if ((lwc_string_caseless_isequal(
					token->idata, c->strings[BEHIND],
					&match) == lwc_error_ok && match)) {
				value |= AZIMUTH_BEHIND;
			} else {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else if ((token == NULL || token->type != CSS_TOKEN_IDENT) &&
				value == AZIMUTH_BEHIND) {
			value |= AZIMUTH_CENTER;
		}
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

		/* Valid angles lie between -360 and 360 degrees */
		if (unit == UNIT_DEG) {
			if ((length < -F_360) || (length > F_360)) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else if (unit == UNIT_GRAD) {
			if ((length < -F_400) || (length > F_400)) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else if (unit == UNIT_RAD) {
			if ((length < -F_2PI) || (length > F_2PI)) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		}

		value = AZIMUTH_ANGLE;
	}

	error = css__stylesheet_style_appendOPV(result, CSS_PROP_AZIMUTH, flags, value);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	if (((flags & FLAG_INHERIT) == false) && (value == AZIMUTH_ANGLE)) {
		error = css__stylesheet_style_vappend(result, 2, length, unit);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}
	}

	return CSS_OK;
}
