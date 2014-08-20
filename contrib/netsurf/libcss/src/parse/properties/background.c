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
 * Parse background
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
css_error css__parse_background(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result)
{
	int orig_ctx = *ctx;
	int prev_ctx;
	const css_token *token;
	css_error error = CSS_OK;
	bool attachment = true;
	bool color = true;
	bool image = true;
	bool position = true;
	bool repeat = true;
	css_style * attachment_style;
	css_style * color_style;
	css_style * image_style;
	css_style * position_style;
	css_style * repeat_style;


	/* Firstly, handle inherit */
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) 
		return CSS_INVALID;
		
	if (is_css_inherit(c, token)) {
		error = css_stylesheet_style_inherit(result, CSS_PROP_BACKGROUND_ATTACHMENT);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_BACKGROUND_COLOR);
		if (error != CSS_OK) 
			return error;		

		error = css_stylesheet_style_inherit(result, CSS_PROP_BACKGROUND_IMAGE);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_BACKGROUND_POSITION);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_BACKGROUND_REPEAT);
		if (error == CSS_OK) 
			parserutils_vector_iterate(vector, ctx);

		return error;
	} 

	/* allocate styles */
	error = css__stylesheet_style_create(c->sheet, &attachment_style);
	if (error != CSS_OK) 
		return error;

	error = css__stylesheet_style_create(c->sheet, &color_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(attachment_style);
		return error;
	}

	error = css__stylesheet_style_create(c->sheet, &image_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(attachment_style);
		css__stylesheet_style_destroy(color_style);
		return error;
	}

	error = css__stylesheet_style_create(c->sheet, &position_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(attachment_style);
		css__stylesheet_style_destroy(color_style);
		css__stylesheet_style_destroy(image_style);
		return error;
	}

	error = css__stylesheet_style_create(c->sheet, &repeat_style);
	if (error != CSS_OK) {
		css__stylesheet_style_destroy(attachment_style);
		css__stylesheet_style_destroy(color_style);
		css__stylesheet_style_destroy(image_style);
		css__stylesheet_style_destroy(position_style);
		return error;
	}

	/* Attempt to parse the various longhand properties */
	do {
		prev_ctx = *ctx;
		error = CSS_OK;

		if (is_css_inherit(c, token)) {
			error = CSS_INVALID;
			goto css__parse_background_cleanup;
		}

		/* Try each property parser in turn, but only if we
		 * haven't already got a value for this property.
		 */
		if ((attachment) && 
		    (error = css__parse_background_attachment(c, vector, ctx, 
					    attachment_style)) == CSS_OK) {
			attachment = false;
		} else if ((color) && 
			   (error = css__parse_background_color(c, vector, ctx,
					    color_style)) == CSS_OK) {
			color = false;
		} else if ((image) && 
			   (error = css__parse_background_image(c, vector, ctx,
					    image_style)) == CSS_OK) {
			image = false;
		} else if ((position) &&
			   (error = css__parse_background_position(c, vector, ctx, 
					position_style)) == CSS_OK) {
			position = false;
		} else if ((repeat) &&
			   (error = css__parse_background_repeat(c, vector, ctx, 
					repeat_style)) == CSS_OK) {
			repeat = false;
		}

		if (error == CSS_OK) {
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
		} else {
			/* Forcibly cause loop to exit */
			token = NULL;
		}
	} while (*ctx != prev_ctx && token != NULL);

	if (attachment) {
		error = css__stylesheet_style_appendOPV(attachment_style, 
				CSS_PROP_BACKGROUND_ATTACHMENT, 0, 
				BACKGROUND_ATTACHMENT_SCROLL);
		if (error != CSS_OK)
			goto css__parse_background_cleanup;
	}

	if (color) {
		error = css__stylesheet_style_appendOPV(color_style, 
				CSS_PROP_BACKGROUND_COLOR, 0, 
				BACKGROUND_COLOR_TRANSPARENT);
		if (error != CSS_OK)
			goto css__parse_background_cleanup;
	}

	if (image) {
		error = css__stylesheet_style_appendOPV(image_style, 
				CSS_PROP_BACKGROUND_IMAGE,
				0, BACKGROUND_IMAGE_NONE);
		if (error != CSS_OK)
			goto css__parse_background_cleanup;
	}

	if (position) {
		error = css__stylesheet_style_appendOPV(position_style,
				CSS_PROP_BACKGROUND_POSITION,
				0, BACKGROUND_POSITION_HORZ_LEFT |
				BACKGROUND_POSITION_VERT_TOP);
		if (error != CSS_OK)
			goto css__parse_background_cleanup;
	}

	if (repeat) {
		error = css__stylesheet_style_appendOPV(repeat_style, 
				CSS_PROP_BACKGROUND_REPEAT,
				0, BACKGROUND_REPEAT_REPEAT);
		if (error != CSS_OK)
			goto css__parse_background_cleanup;
	}

	error = css__stylesheet_merge_style(result, attachment_style);
	if (error != CSS_OK)
		goto css__parse_background_cleanup;

	error = css__stylesheet_merge_style(result, color_style);
	if (error != CSS_OK)
		goto css__parse_background_cleanup;

	error = css__stylesheet_merge_style(result, image_style);
	if (error != CSS_OK)
		goto css__parse_background_cleanup;

	error = css__stylesheet_merge_style(result, position_style);
	if (error != CSS_OK)
		goto css__parse_background_cleanup;

	error = css__stylesheet_merge_style(result, repeat_style);

css__parse_background_cleanup:
	css__stylesheet_style_destroy(attachment_style);
	css__stylesheet_style_destroy(color_style);
	css__stylesheet_style_destroy(image_style);
	css__stylesheet_style_destroy(position_style);
	css__stylesheet_style_destroy(repeat_style);

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;


}






