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
 * Determine if a given font-family ident is reserved
 *
 * \param c	 Parsing context
 * \param ident	 IDENT to consider
 * \return True if IDENT is reserved, false otherwise
 */
static bool font_family_reserved(css_language *c, const css_token *ident)
{
	bool match;

	return (lwc_string_caseless_isequal(
			ident->idata, c->strings[SERIF],
			&match) == lwc_error_ok && match) ||
		(lwc_string_caseless_isequal(
			ident->idata, c->strings[SANS_SERIF],
			&match) == lwc_error_ok && match) ||
		(lwc_string_caseless_isequal(
			ident->idata, c->strings[CURSIVE],
			&match) == lwc_error_ok && match) ||
		(lwc_string_caseless_isequal(
			ident->idata, c->strings[FANTASY],
			&match) == lwc_error_ok && match) ||
		(lwc_string_caseless_isequal(
			ident->idata, c->strings[MONOSPACE],
			&match) == lwc_error_ok && match);
}

/**
 * Convert a font-family token into a bytecode value
 *
 * \param c	 Parsing context
 * \param token	 Token to consider
 * \param first  Whether the token is the first
 * \return Bytecode value
 */
static css_code_t font_family_value(css_language *c, const css_token *token, bool first)
{
	uint16_t value;
	bool match;

	if (token->type == CSS_TOKEN_IDENT) {
		if ((lwc_string_caseless_isequal(
				token->idata, c->strings[SERIF],
				&match) == lwc_error_ok && match))
			value = FONT_FAMILY_SERIF;
		else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[SANS_SERIF],
				&match) == lwc_error_ok && match))
			value = FONT_FAMILY_SANS_SERIF;
		else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[CURSIVE],
				&match) == lwc_error_ok && match))
			value = FONT_FAMILY_CURSIVE;
		else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[FANTASY],
				&match) == lwc_error_ok && match))
			value = FONT_FAMILY_FANTASY;
		else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[MONOSPACE],
				&match) == lwc_error_ok && match))
			value = FONT_FAMILY_MONOSPACE;
		else
			value = FONT_FAMILY_IDENT_LIST;
	} else {
		value = FONT_FAMILY_STRING;
	}

	return first ? buildOPV(CSS_PROP_FONT_FAMILY, 0, value) : value;
}

/**
 * Parse font-family
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
css_error css__parse_font_family(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	bool match;

	/* [ IDENT+ | STRING ] [ ',' [ IDENT+ | STRING ] ]* | IDENT(inherit)
	 * 
	 * In the case of IDENT+, any whitespace between tokens is collapsed to
	 * a single space
	 *
	 * \todo Mozilla makes the comma optional. 
	 * Perhaps this is a quirk we should inherit?
	 */

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_STRING)) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT &&
			(lwc_string_caseless_isequal(
			token->idata, c->strings[INHERIT],
			&match) == lwc_error_ok && match)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_FONT_FAMILY);
	} else {
		*ctx = orig_ctx;

		error = css__comma_list_to_style(c, vector, ctx,
				font_family_reserved, font_family_value,
				result);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		error = css__stylesheet_style_append(result, FONT_FAMILY_END);
	}

	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	return CSS_OK;
}
