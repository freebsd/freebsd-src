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
 * Determine if a given voice-family ident is reserved
 *
 * \param c	 Parsing context
 * \param ident	 IDENT to consider
 * \return True if IDENT is reserved, false otherwise
 */
static bool voice_family_reserved(css_language *c, const css_token *ident)
{
	bool match;

	return (lwc_string_caseless_isequal(
			ident->idata, c->strings[MALE],
			&match) == lwc_error_ok && match) ||
		(lwc_string_caseless_isequal(
			ident->idata, c->strings[FEMALE],
			&match) == lwc_error_ok && match) ||
		(lwc_string_caseless_isequal(
			ident->idata, c->strings[CHILD],
			&match) == lwc_error_ok && match);
}

/**
 * Convert a voice-family token into a bytecode value
 *
 * \param c	 Parsing context
 * \param token	 Token to consider
 * \return Bytecode value
 */
static css_code_t voice_family_value(css_language *c, const css_token *token, bool first)
{
	uint16_t value;
	bool match;

	if (token->type == CSS_TOKEN_IDENT) {
		if ((lwc_string_caseless_isequal(
				token->idata, c->strings[MALE],
				&match) == lwc_error_ok && match))
			value = VOICE_FAMILY_MALE;
		else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[FEMALE],
				&match) == lwc_error_ok && match))
			value = VOICE_FAMILY_FEMALE;
		else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[CHILD],
				&match) == lwc_error_ok && match))
			value = VOICE_FAMILY_CHILD;
		else
			value = VOICE_FAMILY_IDENT_LIST;
	} else {
		value = VOICE_FAMILY_STRING;
	}

	return first ? buildOPV(CSS_PROP_VOICE_FAMILY, 0, value) : value;
}

/**
 * Parse voice-family
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *	 CSS_NOMEM on memory exhaustion,
 *	 CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_voice_family(css_language *c, 
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
		error = css_stylesheet_style_inherit(result, CSS_PROP_VOICE_FAMILY);
	} else {
		*ctx = orig_ctx;

		error = css__comma_list_to_style(c, vector, ctx,
				voice_family_reserved, voice_family_value,
				result);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		error = css__stylesheet_style_append(result, VOICE_FAMILY_END);
	}

	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	return CSS_OK;
}
