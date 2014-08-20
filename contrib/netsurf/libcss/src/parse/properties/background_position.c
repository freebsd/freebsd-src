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
 * Parse background-position
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
css_error css__parse_background_position(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value[2] = { 0 };
	css_fixed length[2] = { 0 };
	uint32_t unit[2] = { 0 };
	bool match;

	/* [length | percentage | IDENT(left, right, top, bottom, center)]{1,2}
	 * | IDENT(inherit) */
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
	} else {
		int i;

		for (i = 0; i < 2; i++) {
			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL)
				break;

			if (token->type == CSS_TOKEN_IDENT) {
				if ((lwc_string_caseless_isequal(
						token->idata, c->strings[LEFT],
						&match) == lwc_error_ok && 
						match)) {
					value[i] = 
						BACKGROUND_POSITION_HORZ_LEFT;
				} else if ((lwc_string_caseless_isequal(
						token->idata, c->strings[RIGHT],
						&match) == lwc_error_ok && 
						match)) {
					value[i] = 
						BACKGROUND_POSITION_HORZ_RIGHT;
				} else if ((lwc_string_caseless_isequal(
						token->idata, c->strings[TOP],
						&match) == lwc_error_ok && 
						match)) {
					value[i] = BACKGROUND_POSITION_VERT_TOP;
				} else if ((lwc_string_caseless_isequal(
						token->idata, 
						c->strings[BOTTOM],
						&match) == lwc_error_ok && 
						match)) {
					value[i] = 
						BACKGROUND_POSITION_VERT_BOTTOM;
				} else if ((lwc_string_caseless_isequal(
						token->idata, 
						c->strings[CENTER],
						&match) == lwc_error_ok && 
						match)) {
					/* We'll fix this up later */
					value[i] = 
						BACKGROUND_POSITION_VERT_CENTER;
				} else if (i == 1) {
					/* Second pass, so ignore this one */
					break;
				} else {
					/* First pass, so invalid */
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				parserutils_vector_iterate(vector, ctx);
			} else if (token->type == CSS_TOKEN_DIMENSION ||
					token->type == CSS_TOKEN_NUMBER ||
					token->type == CSS_TOKEN_PERCENTAGE) {
				error = css__parse_unit_specifier(c, vector, ctx, 
						UNIT_PX, &length[i], &unit[i]);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				if (unit[i] & UNIT_ANGLE || 
						unit[i] & UNIT_TIME || 
						unit[i] & UNIT_FREQ) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				/* We'll fix this up later, too */
				value[i] = BACKGROUND_POSITION_VERT_SET;
			} else {
				if (i == 1) {
					/* Second pass, so ignore */
					break;
				} else {
					/* First pass, so invalid */
					*ctx = orig_ctx;
					return CSS_INVALID;
				}
			}

			consumeWhitespace(vector, ctx);
		}

		assert(i != 0);

		/* Now, sort out the mess we've got */
		if (i == 1) {
			assert(BACKGROUND_POSITION_VERT_CENTER ==
					BACKGROUND_POSITION_HORZ_CENTER);

			/* Only one value, so the other is center */
			switch (value[0]) {
			case BACKGROUND_POSITION_HORZ_LEFT:
			case BACKGROUND_POSITION_HORZ_RIGHT:
			case BACKGROUND_POSITION_VERT_CENTER:
			case BACKGROUND_POSITION_VERT_TOP:
			case BACKGROUND_POSITION_VERT_BOTTOM:
				break;
			case BACKGROUND_POSITION_VERT_SET:
				value[0] = BACKGROUND_POSITION_HORZ_SET;
				break;
			}

			value[1] = BACKGROUND_POSITION_VERT_CENTER;
		} else if (value[0] != BACKGROUND_POSITION_VERT_SET &&
				value[1] != BACKGROUND_POSITION_VERT_SET) {
			/* Two keywords. Verify the axes differ */
			if (((value[0] & 0xf) != 0 && (value[1] & 0xf) != 0) ||
					((value[0] & 0xf0) != 0 && 
						(value[1] & 0xf0) != 0)) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		} else {
			/* One or two non-keywords. First is horizontal */
			if (value[0] == BACKGROUND_POSITION_VERT_SET)
				value[0] = BACKGROUND_POSITION_HORZ_SET;

			/* Verify the axes differ */
			if (((value[0] & 0xf) != 0 && (value[1] & 0xf) != 0) ||
					((value[0] & 0xf0) != 0 && 
						(value[1] & 0xf0) != 0)) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		}
	}

	error = css__stylesheet_style_appendOPV(result, 
					       CSS_PROP_BACKGROUND_POSITION, 
					       flags, 
					       value[0] | value[1]);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	if ((flags & FLAG_INHERIT) == false) {
		if (value[0] == BACKGROUND_POSITION_HORZ_SET) {
			error = css__stylesheet_style_append(result, length[0]);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			error = css__stylesheet_style_append(result, unit[0]);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}
		}
		if (value[1] == BACKGROUND_POSITION_VERT_SET) {
			error = css__stylesheet_style_append(result, length[1]);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			error = css__stylesheet_style_append(result, unit[1]);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}
		}
	}

	return CSS_OK;
}
