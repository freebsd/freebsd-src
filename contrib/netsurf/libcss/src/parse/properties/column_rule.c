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
 * Parse column-rule shorthand
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
css_error css__parse_column_rule(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	int prev_ctx;
	const css_token *token;
	css_error error;
	bool color = true;
	bool style = true;
	bool width = true;
	css_style *color_style;
	css_style *style_style;
	css_style *width_style;

	/* Firstly, handle inherit */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (is_css_inherit(c, token)) {
		error = css_stylesheet_style_inherit(result,
				CSS_PROP_COLUMN_RULE_COLOR);
		if (error != CSS_OK)
			return error;

		error = css_stylesheet_style_inherit(result,
				CSS_PROP_COLUMN_RULE_STYLE);
		if (error != CSS_OK)
			return error;

		error = css_stylesheet_style_inherit(result,
				CSS_PROP_COLUMN_RULE_WIDTH);

		if (error == CSS_OK)
			parserutils_vector_iterate(vector, ctx);

		return error;
	}

	/* Allocate for styles */
	error = css__stylesheet_style_create(c->sheet, &color_style);
	if (error != CSS_OK)
		return error;

	error = css__stylesheet_style_create(c->sheet, &style_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(color_style);
		return error;
	}

	error = css__stylesheet_style_create(c->sheet, &width_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(color_style);
		css__stylesheet_style_destroy(style_style);
		return error;
	}

	/* Attempt to parse the various longhand properties */
	do {
		prev_ctx = *ctx;
		error = CSS_OK;

		/* Ensure that we're not about to parse another inherit */
		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && is_css_inherit(c, token)) {
			error = CSS_INVALID;
			goto css__parse_column_rule_cleanup;
		}

		if ((color) &&
				(error = css__parse_column_rule_color(c, vector,
						ctx, color_style)) == CSS_OK) {
			color = false;
		} else if ((style) &&
				(error = css__parse_column_rule_style(c, vector,
						ctx, style_style)) == CSS_OK) {
			style = false;
		} else if ((width) &&
				(error = css__parse_column_rule_width(c, vector,
						ctx, width_style)) == CSS_OK) {
			width = false;
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
	if (color) {
		error = css__stylesheet_style_appendOPV(color_style,
				CSS_PROP_COLUMN_RULE_COLOR, 0,
				COLUMN_RULE_COLOR_SET);
		if (error != CSS_OK)
			goto css__parse_column_rule_cleanup;

		/** TODO: initial colour should be the UA-defined initial
		 *        value of the "color" property, not "0" */
		error = css__stylesheet_style_append(color_style, 0x00000000);
		if (error != CSS_OK)
			goto css__parse_column_rule_cleanup;
	}

	if (style) {
		error = css__stylesheet_style_appendOPV(style_style,
				CSS_PROP_COLUMN_RULE_STYLE, 0,
				COLUMN_RULE_STYLE_NONE);
		if (error != CSS_OK)
			goto css__parse_column_rule_cleanup;
	}

	if (width) {
		error = css__stylesheet_style_appendOPV(width_style,
				CSS_PROP_COLUMN_RULE_WIDTH, 0,
				COLUMN_RULE_WIDTH_MEDIUM);
		if (error != CSS_OK)
			goto css__parse_column_rule_cleanup;
	}

	/* Merge styles into the result */
	error = css__stylesheet_merge_style(result, color_style);
	if (error != CSS_OK)
		goto css__parse_column_rule_cleanup;

	error = css__stylesheet_merge_style(result, style_style);
	if (error != CSS_OK)
		goto css__parse_column_rule_cleanup;

	error = css__stylesheet_merge_style(result, width_style);


css__parse_column_rule_cleanup:

	css__stylesheet_style_destroy(width_style);
	css__stylesheet_style_destroy(style_style);
	css__stylesheet_style_destroy(color_style);

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}
