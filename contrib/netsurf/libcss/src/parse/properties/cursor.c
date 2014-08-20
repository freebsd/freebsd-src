/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse cursor
 *
 * \param c       Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx     Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion,
 *         CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_cursor(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error = CSS_OK;
	const css_token *token;
	bool match;

	/* [ (URI ',')* IDENT(auto, crosshair, default, pointer, move, e-resize,
	 *              ne-resize, nw-resize, n-resize, se-resize, sw-resize,
	 *              s-resize, w-resize, text, wait, help, progress) ] 
	 * | IDENT(inherit) 
	 */
	token = parserutils_vector_iterate(vector, ctx);
	if ((token == NULL) || 
	    (token->type != CSS_TOKEN_IDENT &&
	     token->type != CSS_TOKEN_URI)) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if ((token->type == CSS_TOKEN_IDENT) &&
	    (lwc_string_caseless_isequal(token->idata,
					 c->strings[INHERIT],
					 &match) == lwc_error_ok && match)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_CURSOR);
	} else {
		bool first = true;

/* Macro to output the value marker, awkward because we need to check
 * first to determine how the value is constructed.
 */
#define CSS_APPEND(CSSVAL) css__stylesheet_style_append(result, first?buildOPV(CSS_PROP_CURSOR, 0, CSSVAL):CSSVAL)


		/* URI* */
		while (token != NULL && token->type == CSS_TOKEN_URI) {
			lwc_string *uri;
			uint32_t uri_snumber;

			error = c->sheet->resolve(c->sheet->resolve_pw,
					c->sheet->url,
					token->idata, &uri);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			error = css__stylesheet_string_add(c->sheet, 
							  uri, 
							  &uri_snumber);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			error = CSS_APPEND(CURSOR_URI);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			error = css__stylesheet_style_append(result, uri_snumber);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			consumeWhitespace(vector, ctx);

			/* Expect ',' */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || tokenIsChar(token, ',') == false) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}

			consumeWhitespace(vector, ctx);

			/* Expect either URI or IDENT */
			token = parserutils_vector_iterate(vector, ctx);
			if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_URI)) {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}

			first = false;
		}

		/* IDENT */
		if (token != NULL && token->type == CSS_TOKEN_IDENT) {
			if ((lwc_string_caseless_isequal(
					token->idata, c->strings[AUTO],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_AUTO);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[CROSSHAIR],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_CROSSHAIR);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[DEFAULT],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_DEFAULT);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[POINTER],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_POINTER);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[MOVE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_MOVE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[E_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_E_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[NE_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_NE_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[NW_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_NW_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[N_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_N_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[SE_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_SE_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[SW_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_SW_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[S_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_S_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[W_RESIZE],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_W_RESIZE);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[LIBCSS_TEXT],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_TEXT);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[WAIT],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_WAIT);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[HELP],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_HELP);
			} else if ((lwc_string_caseless_isequal(
					token->idata, c->strings[PROGRESS],
					&match) == lwc_error_ok && match)) {
				error=CSS_APPEND(CURSOR_PROGRESS);
			} else {
				error =  CSS_INVALID;
			}
		}

	}

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}

