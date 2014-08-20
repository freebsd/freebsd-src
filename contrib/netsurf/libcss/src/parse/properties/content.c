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
 * Parse content
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
css_error css__parse_content(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	bool match;

	/* IDENT(normal, none, inherit) | [ ... ]+ */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}


	if ((token->type == CSS_TOKEN_IDENT) &&
	    (lwc_string_caseless_isequal(token->idata,
					 c->strings[INHERIT],
					 &match) == lwc_error_ok && match)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_CONTENT);
	} else if ((token->type == CSS_TOKEN_IDENT) &&
		   (lwc_string_caseless_isequal(token->idata,
						c->strings[NORMAL],
						&match) == lwc_error_ok && match)) {
		error = css__stylesheet_style_appendOPV(result, CSS_PROP_CONTENT, 0, CONTENT_NORMAL);
	} else if ((token->type == CSS_TOKEN_IDENT) &&
		   (lwc_string_caseless_isequal(token->idata,
						c->strings[NONE],
						&match) == lwc_error_ok && match)) {
		error = css__stylesheet_style_appendOPV(result, CSS_PROP_CONTENT, 0, CONTENT_NONE);
	} else {

/* Macro to output the value marker, awkward because we need to check
 * first to determine how the value is constructed.
 */
#define CSS_APPEND(CSSVAL) css__stylesheet_style_append(result, first?buildOPV(CSS_PROP_CONTENT, 0, CSSVAL):CSSVAL)

		bool first = true;
		int prev_ctx = orig_ctx;

		/* [
		 *   IDENT(open-quote, close-quote, no-open-quote,
		 *         no-close-quote) |
		 *   STRING |
		 *   URI |
		 *   FUNCTION(attr) IDENT ')' |
		 *   FUNCTION(counter) IDENT IDENT? ')' |
		 *   FUNCTION(counters) IDENT STRING IDENT? ')'
		 * ]+
		 */

		while (token != NULL) {
			if ((token->type == CSS_TOKEN_IDENT) &&
			    (lwc_string_caseless_isequal(
				    token->idata, c->strings[OPEN_QUOTE],
				    &match) == lwc_error_ok && match)) {

				error = CSS_APPEND(CONTENT_OPEN_QUOTE);

			} else if (token->type == CSS_TOKEN_IDENT &&
				   (lwc_string_caseless_isequal(
					   token->idata, c->strings[CLOSE_QUOTE],
					   &match) == lwc_error_ok && match)) {

				error = CSS_APPEND(CONTENT_CLOSE_QUOTE);
			} else if (token->type == CSS_TOKEN_IDENT &&
				   (lwc_string_caseless_isequal(
					   token->idata, c->strings[NO_OPEN_QUOTE],
					   &match) == lwc_error_ok && match)) {
				error = CSS_APPEND(CONTENT_NO_OPEN_QUOTE);
			} else if (token->type == CSS_TOKEN_IDENT &&
				   (lwc_string_caseless_isequal(
					   token->idata, c->strings[NO_CLOSE_QUOTE],
					   &match) == lwc_error_ok && match)) {
				error = CSS_APPEND(CONTENT_NO_CLOSE_QUOTE);
			} else if (token->type == CSS_TOKEN_STRING) {
				uint32_t snumber;

				error = css__stylesheet_string_add(c->sheet, lwc_string_ref(token->idata), &snumber);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = CSS_APPEND(CONTENT_STRING);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_style_append(result, snumber);
			} else if (token->type == CSS_TOKEN_URI) {
				lwc_string *uri;
				uint32_t uri_snumber;

				error = c->sheet->resolve(c->sheet->resolve_pw,
							  c->sheet->url,
							  token->idata, 
							  &uri);
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

				error = CSS_APPEND(CONTENT_URI);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_style_append(result, uri_snumber);
			} else if (token->type == CSS_TOKEN_FUNCTION &&
				   (lwc_string_caseless_isequal(
					   token->idata, c->strings[ATTR],
					   &match) == lwc_error_ok && match)) {
				uint32_t snumber;

				consumeWhitespace(vector, ctx);

				/* Expect IDENT */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || token->type != CSS_TOKEN_IDENT) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				error = css__stylesheet_string_add(c->sheet, lwc_string_ref(token->idata), &snumber);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = CSS_APPEND(CONTENT_ATTR);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_style_append(result, snumber);

				consumeWhitespace(vector, ctx);

				/* Expect ')' */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || tokenIsChar(token, ')') == false) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}
			} else if (token->type == CSS_TOKEN_FUNCTION &&
				   (lwc_string_caseless_isequal(
					   token->idata, c->strings[COUNTER],
					   &match) == lwc_error_ok && match)) {
				lwc_string *name;
				uint32_t snumber;
				uint32_t opv;

				opv = CONTENT_COUNTER;

				consumeWhitespace(vector, ctx);

				/* Expect IDENT */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || token->type != CSS_TOKEN_IDENT) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				name = token->idata;

				consumeWhitespace(vector, ctx);

				/* Possible ',' */
				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL ||
				    (tokenIsChar(token, ',') == false &&
				     tokenIsChar(token, ')') == false)) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				if (tokenIsChar(token, ',')) {
					uint16_t v;

					parserutils_vector_iterate(vector, ctx);

					consumeWhitespace(vector, ctx);

					/* Expect IDENT */
					token = parserutils_vector_peek(vector, *ctx);
					if (token == NULL || token->type !=
					    CSS_TOKEN_IDENT) {
						*ctx = orig_ctx;
						return CSS_INVALID;
					}

					error = css__parse_list_style_type_value(c, token, &v);
					if (error != CSS_OK) {
						*ctx = orig_ctx;
						return error;
					}

					opv |= v << CONTENT_COUNTER_STYLE_SHIFT;

					parserutils_vector_iterate(vector, ctx);

					consumeWhitespace(vector, ctx);
				} else {
					opv |= LIST_STYLE_TYPE_DECIMAL <<
						CONTENT_COUNTER_STYLE_SHIFT;
				}

				/* Expect ')' */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || tokenIsChar(token,	')') == false) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}


				error = css__stylesheet_string_add(c->sheet, lwc_string_ref(name), &snumber);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = CSS_APPEND(opv);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_style_append(result, snumber);
			} else if (token->type == CSS_TOKEN_FUNCTION &&
				   (lwc_string_caseless_isequal(
					   token->idata, c->strings[COUNTERS],
					   &match) == lwc_error_ok && match)) {
				lwc_string *name;
				lwc_string *sep;
				uint32_t name_snumber;
				uint32_t sep_snumber;
				uint32_t opv;

				opv = CONTENT_COUNTERS;

				consumeWhitespace(vector, ctx);

				/* Expect IDENT */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || token->type != CSS_TOKEN_IDENT) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				name = token->idata;

				consumeWhitespace(vector, ctx);

				/* Expect ',' */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || tokenIsChar(token, ',') == false) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				consumeWhitespace(vector, ctx);

				/* Expect STRING */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || token->type != CSS_TOKEN_STRING) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				sep = token->idata;

				consumeWhitespace(vector, ctx);

				/* Possible ',' */
				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL ||
				    (tokenIsChar(token, ',') == false &&
				     tokenIsChar(token, ')') == false)) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}

				if (tokenIsChar(token, ',')) {
					uint16_t v;

					parserutils_vector_iterate(vector, ctx);

					consumeWhitespace(vector, ctx);

					/* Expect IDENT */
					token = parserutils_vector_peek(vector, *ctx);
					if (token == NULL || token->type !=
					    CSS_TOKEN_IDENT) {
						*ctx = orig_ctx;
						return CSS_INVALID;
					}

					error = css__parse_list_style_type_value(c,
									    token, &v);
					if (error != CSS_OK) {
						*ctx = orig_ctx;
						return error;
					}

					opv |= v << CONTENT_COUNTERS_STYLE_SHIFT;

					parserutils_vector_iterate(vector, ctx);

					consumeWhitespace(vector, ctx);
				} else {
					opv |= LIST_STYLE_TYPE_DECIMAL <<
						CONTENT_COUNTERS_STYLE_SHIFT;
				}

				/* Expect ')' */
				token = parserutils_vector_iterate(vector, ctx);
				if (token == NULL || tokenIsChar(token, ')') == false) {
					*ctx = orig_ctx;
					return CSS_INVALID;
				}


				error = css__stylesheet_string_add(c->sheet, lwc_string_ref(name), &name_snumber);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_string_add(c->sheet, lwc_string_ref(sep), &sep_snumber);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = CSS_APPEND(opv);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_style_append(result, name_snumber);
				if (error != CSS_OK) {
					*ctx = orig_ctx;
					return error;
				}

				error = css__stylesheet_style_append(result, sep_snumber);
			} else if (first) {
				/* Invalid if this is the first iteration */
				error = CSS_INVALID;
			} else {
				/* Give up, ensuring current token is reprocessed */
				*ctx = prev_ctx;
				error = CSS_OK;
				break;
			}

			/* if there was an error bail */
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}

			first = false;

			consumeWhitespace(vector, ctx);

			prev_ctx = *ctx;
			token = parserutils_vector_iterate(vector, ctx);
		} /* while */

		/* Write list terminator */
		css__stylesheet_style_append(result, CONTENT_NORMAL);
	}

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}

