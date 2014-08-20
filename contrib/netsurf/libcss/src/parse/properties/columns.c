/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Michael Drake <tlsa@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse columns shorthand
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
css_error css__parse_columns(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	int prev_ctx;
	const css_token *token;
	css_error error = CSS_OK;
	bool width = true;
	bool count = true;
	css_style *width_style;
	css_style *count_style;

	/* Firstly, handle inherit */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (is_css_inherit(c, token)) {
		error = css_stylesheet_style_inherit(result,
				CSS_PROP_COLUMN_WIDTH);
		if (error != CSS_OK)
			return error;

		error = css_stylesheet_style_inherit(result,
				CSS_PROP_COLUMN_COUNT);
		if (error == CSS_OK)
			parserutils_vector_iterate(vector, ctx);

		return error;
	}

	/* Allocate for styles */
	error = css__stylesheet_style_create(c->sheet, &width_style);
	if (error != CSS_OK)
		return error;

	error = css__stylesheet_style_create(c->sheet, &count_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(width_style);
		return error;
	}

	/* Attempt to parse the various longhand properties */
	do {
		prev_ctx = *ctx;
		error = CSS_OK;

		if (is_css_inherit(c, token)) {
			/* Can't have another inherit */
			error = CSS_INVALID;
			goto css__parse_columns_cleanup;
		}

		/* Try each property parser in turn, but only if we
		 * haven't already got a value for this property.
		 */
		if ((width) &&
				(error = css__parse_column_width(c, vector, ctx,
						width_style)) == CSS_OK) {
			width = false;
		} else if ((count) &&
				(error = css__parse_column_count(c, vector, ctx,
						count_style)) == CSS_OK) {
			count = false;
		}

		if (error == CSS_OK) {
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
		} else {
			/* Forcibly cause loop to exit */
			token = NULL;
		}
	} while (*ctx != prev_ctx && token != NULL);

	/* Set unset properties to initial values */
	if (width) {
		error = css__stylesheet_style_appendOPV(width_style,
				CSS_PROP_COLUMN_WIDTH, 0,
				COLUMN_WIDTH_AUTO);
		if (error != CSS_OK)
			goto css__parse_columns_cleanup;
	}

	if (count) {
		error = css__stylesheet_style_appendOPV(count_style,
				CSS_PROP_COLUMN_COUNT, 0,
				COLUMN_COUNT_AUTO);
		if (error != CSS_OK)
			goto css__parse_columns_cleanup;
	}

	/* Merge styles into the result */
	error = css__stylesheet_merge_style(result, width_style);
	if (error != CSS_OK)
		goto css__parse_columns_cleanup;

	error = css__stylesheet_merge_style(result, count_style);


css__parse_columns_cleanup:
	css__stylesheet_style_destroy(width_style);
	css__stylesheet_style_destroy(count_style);

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}

