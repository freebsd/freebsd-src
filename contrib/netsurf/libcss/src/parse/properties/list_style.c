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
 * Parse list-style
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
css_error css__parse_list_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	int prev_ctx;
	const css_token *token;
	css_error error;
	bool image = true;
	bool position = true;
	bool type = true;
	css_style *image_style;
	css_style *position_style;
	css_style *type_style;

	/* Firstly, handle inherit */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) 
		return CSS_INVALID;
		
	if (is_css_inherit(c, token)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_LIST_STYLE_IMAGE);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_LIST_STYLE_POSITION);
		if (error != CSS_OK) 
			return error;		

		error = css_stylesheet_style_inherit(result, CSS_PROP_LIST_STYLE_TYPE);

		if (error == CSS_OK) 
			parserutils_vector_iterate(vector, ctx);

		return error;
	} 

	/* allocate styles */
	error = css__stylesheet_style_create(c->sheet, &image_style);
	if (error != CSS_OK) 
		return error;

	error = css__stylesheet_style_create(c->sheet, &position_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(image_style);
		return error;
	}

	error = css__stylesheet_style_create(c->sheet, &type_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(image_style);
		css__stylesheet_style_destroy(position_style);
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
			goto css__parse_list_style_cleanup;
		}

		if ((type) && 
		    (error = css__parse_list_style_type(c, vector,
				ctx, type_style)) == CSS_OK) {
			type = false;
		} else if ((position) && 
			   (error = css__parse_list_style_position(c, vector, 
				ctx, position_style)) == CSS_OK) {
			position = false;
		} else if ((image) && 
			   (error = css__parse_list_style_image(c, vector, ctx,
				image_style)) == CSS_OK) {
			image = false;
		}

		if (error == CSS_OK) {
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
		} else {
			/* Forcibly cause loop to exit */
			token = NULL;
		}
	} while (*ctx != prev_ctx && token != NULL);


	/* defaults */
	if (image) {
		error = css__stylesheet_style_appendOPV(image_style, 
			       CSS_PROP_LIST_STYLE_IMAGE,
				0, LIST_STYLE_IMAGE_NONE);
	}

	if (position) {
		error = css__stylesheet_style_appendOPV(position_style, 
			       CSS_PROP_LIST_STYLE_POSITION,
				0, LIST_STYLE_POSITION_OUTSIDE);
	}

	if (type) {
		error = css__stylesheet_style_appendOPV(type_style, 
			       CSS_PROP_LIST_STYLE_TYPE,
				0, LIST_STYLE_TYPE_DISC);
	}


	error = css__stylesheet_merge_style(result, image_style);
	if (error != CSS_OK)
		goto css__parse_list_style_cleanup;

	error = css__stylesheet_merge_style(result, position_style);
	if (error != CSS_OK)
		goto css__parse_list_style_cleanup;

	error = css__stylesheet_merge_style(result, type_style);


css__parse_list_style_cleanup:

	css__stylesheet_style_destroy(type_style);
	css__stylesheet_style_destroy(position_style);
	css__stylesheet_style_destroy(image_style);

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;

}
