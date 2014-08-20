/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "stylesheet.h"
#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"
#include "utils/parserutilserror.h"


/**
 * Parse list-style-type value
 *
 * \param c	 Parsing context
 * \param ident	 Identifier to consider
 * \param value	 Pointer to location to receive value
 * \return CSS_OK on success,
 *	   CSS_INVALID if the input is not valid
 */
css_error css__parse_list_style_type_value(css_language *c, const css_token *ident,
		uint16_t *value)
{
	bool match;

	/* IDENT (disc, circle, square, decimal, decimal-leading-zero,
	 *	  lower-roman, upper-roman, lower-greek, lower-latin,
	 *	  upper-latin, armenian, georgian, lower-alpha, upper-alpha,
	 *	  none)
	 */
	if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[DISC],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_DISC;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[CIRCLE],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_CIRCLE;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[SQUARE],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_SQUARE;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[DECIMAL],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_DECIMAL;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[DECIMAL_LEADING_ZERO],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[LOWER_ROMAN],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_LOWER_ROMAN;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[UPPER_ROMAN],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_UPPER_ROMAN;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[LOWER_GREEK],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_LOWER_GREEK;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[LOWER_LATIN],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_LOWER_LATIN;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[UPPER_LATIN],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_UPPER_LATIN;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[ARMENIAN],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_ARMENIAN;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[GEORGIAN],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_GEORGIAN;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[LOWER_ALPHA],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_LOWER_ALPHA;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[UPPER_ALPHA],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_UPPER_ALPHA;
	} else if ((lwc_string_caseless_isequal(
			ident->idata, c->strings[NONE],
			&match) == lwc_error_ok && match)) {
		*value = LIST_STYLE_TYPE_NONE;
	} else
		return CSS_INVALID;

	return CSS_OK;
}



/**
 * Parse border-{top,right,bottom,left} shorthand
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param side	  The side we're parsing for
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_border_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum border_side_e side)
{
	int orig_ctx = *ctx;
	int prev_ctx;
	const css_token *token;
	css_error error = CSS_OK;
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
		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_TOP_COLOR + side);
		if (error != CSS_OK) 
			return error;

		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_TOP_STYLE + side);
		if (error != CSS_OK) 
			return error;		

		error = css_stylesheet_style_inherit(result, CSS_PROP_BORDER_TOP_WIDTH + side);
		if (error == CSS_OK) 
			parserutils_vector_iterate(vector, ctx);

		return error;
	} 

	/* allocate styles */
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
			goto css__parse_border_side_cleanup;
		}

		/* Try each property parser in turn, but only if we
		 * haven't already got a value for this property.
		 */
		if ((color) && 
		    (error = css__parse_border_side_color(c, vector, ctx, 
			     color_style, CSS_PROP_BORDER_TOP_COLOR + side)) == CSS_OK) {
			color = false;
		} else if ((style) && 
			   (error = css__parse_border_side_style(c, vector, ctx,
				    style_style, CSS_PROP_BORDER_TOP_STYLE + side)) == CSS_OK) {
			style = false;
		} else if ((width) && 
			   (error = css__parse_border_side_width(c, vector, ctx,
				    width_style, CSS_PROP_BORDER_TOP_WIDTH + side)) == CSS_OK) {
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

	if (style) {
		error = css__stylesheet_style_appendOPV(style_style,
				CSS_PROP_BORDER_TOP_STYLE + side, 0, 
				BORDER_STYLE_NONE);
		if (error != CSS_OK)
			goto css__parse_border_side_cleanup;
	}

	if (width) {
		error = css__stylesheet_style_appendOPV(width_style,
				CSS_PROP_BORDER_TOP_WIDTH + side, 0,
				BORDER_WIDTH_MEDIUM);
		if (error != CSS_OK)
			goto css__parse_border_side_cleanup;
	}

	if (color) {
		error = css__stylesheet_style_appendOPV(color_style,
				CSS_PROP_BORDER_TOP_COLOR + side, 0,
				BORDER_COLOR_CURRENT_COLOR);
		if (error != CSS_OK)
			goto css__parse_border_side_cleanup;
	}

	error = css__stylesheet_merge_style(result, color_style);
	if (error != CSS_OK)
		goto css__parse_border_side_cleanup;

	error = css__stylesheet_merge_style(result, style_style);
	if (error != CSS_OK)
		goto css__parse_border_side_cleanup;

	error = css__stylesheet_merge_style(result, width_style);

css__parse_border_side_cleanup:
	css__stylesheet_style_destroy(color_style);
	css__stylesheet_style_destroy(style_style);
	css__stylesheet_style_destroy(width_style);

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}


/**
 * Convert Hue Saturation Lightness value to RGB.
 *
 * \param hue Hue in degrees 0..360
 * \param sat Saturation value in percent 0..100
 * \param lit Lightness value in percent 0..100
 * \param r red component
 * \param g green component
 * \param b blue component
 */
static void HSL_to_RGB(css_fixed hue, css_fixed sat, css_fixed lit, uint8_t *r, uint8_t *g, uint8_t *b)
{
	css_fixed min_rgb, max_rgb, chroma;
	css_fixed relative_hue, scaled_hue, mid1, mid2;
	int sextant;

#define ORGB(R, G, B) \
	*r = FIXTOINT(FDIV(FMUL((R), F_255), F_100)); \
	*g = FIXTOINT(FDIV(FMUL((G), F_255), F_100)); \
	*b = FIXTOINT(FDIV(FMUL((B), F_255), F_100))

	/* If saturation is zero there is no hue and r = g = b = lit */
	if (sat == INTTOFIX(0)) {
		ORGB(lit, lit, lit);
		return;
	}

	/* Compute max(r,g,b) */
	if (lit <= INTTOFIX(50)) {
		max_rgb = FDIV(FMUL(lit, FADD(sat, F_100)), F_100);
	} else {
		max_rgb = FDIV(FSUB(FMUL(FADD(lit, sat), F_100), FMUL(lit, sat)), F_100);
	}

	/* Compute min(r,g,b) */
	min_rgb = FSUB(FMUL(lit, INTTOFIX(2)), max_rgb);

	/* We know that the value of at least one of the components is 
	 * max(r,g,b) and that the value of at least one of the other
	 * components is min(r,g,b).
	 *
	 * We can determine which components have these values by
	 * considering which the sextant of the hexcone the hue lies
	 * in:
	 *
	 * Sextant:	max(r,g,b):	min(r,g,b):
	 *
	 * 0		r		b
	 * 1		g		b
	 * 2		g		r
	 * 3		b		r
	 * 4		b		g
	 * 5		r		g
	 *
	 * Thus, we need only compute the value of the third component
	 */

	/* Chroma is the difference between min and max */
	chroma = FSUB(max_rgb, min_rgb);

	/* Compute which sextant the hue lies in (truncates result) */
	hue = FDIV(FMUL(hue, INTTOFIX(6)), F_360);
	sextant = FIXTOINT(hue);

	/* Compute offset of hue from start of sextant */
	relative_hue = FSUB(hue, INTTOFIX(sextant));

	/* Scale offset by chroma */
        scaled_hue = FMUL(relative_hue, chroma);

	/* Compute potential values of the third colour component */
        mid1 = FADD(min_rgb, scaled_hue);
        mid2 = FSUB(max_rgb, scaled_hue);

	/* Populate result */
        switch (sextant) {
	case 0: ORGB(max_rgb,   mid1,      min_rgb); break;
	case 1: ORGB(mid2,      max_rgb,   min_rgb); break;
	case 2: ORGB(min_rgb,   max_rgb,   mid1); break;
	case 3: ORGB(min_rgb,   mid2,      max_rgb); break;
	case 4: ORGB(mid1,      min_rgb,   max_rgb); break;
	case 5: ORGB(max_rgb,   min_rgb,   mid2); break;
        }

#undef ORGB
}

/**
 * Parse a colour specifier
 *
 * \param c       Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx     Pointer to vector iteration context
 * \param value   Pointer to location to receive value
 * \param result  Pointer to location to receive result (AARRGGBB)
 * \return CSS_OK      on success,
 *         CSS_INVALID if the input is invalid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_colour_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t *value, uint32_t *result)
{
	int orig_ctx = *ctx;
	const css_token *token;
	bool match;
	css_error error;

	consumeWhitespace(vector, ctx);

	/* IDENT(<colour name>) | 
	 * HASH(rgb | rrggbb) |
	 * FUNCTION(rgb) [ [ NUMBER | PERCENTAGE ] ',' ] {3} ')'
	 * FUNCTION(rgba) [ [ NUMBER | PERCENTAGE ] ',' ] {4} ')'
	 * FUNCTION(hsl) ANGLE ',' PERCENTAGE ',' PERCENTAGE  ')'
	 * FUNCTION(hsla) ANGLE ',' PERCENTAGE ',' PERCENTAGE ',' NUMBER ')'
	 *
	 * For quirks, NUMBER | DIMENSION | IDENT, too
	 * I.E. "123456" -> NUMBER, "1234f0" -> DIMENSION, "f00000" -> IDENT
	 */
	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
			token->type != CSS_TOKEN_HASH &&
			token->type != CSS_TOKEN_FUNCTION)) {
		if (c->sheet->quirks_allowed == false ||
				token == NULL ||
				(token->type != CSS_TOKEN_NUMBER &&
				token->type != CSS_TOKEN_DIMENSION))
			goto invalid;
	}

	if (token->type == CSS_TOKEN_IDENT) {
		if ((lwc_string_caseless_isequal(
				token->idata, c->strings[TRANSPARENT],
				&match) == lwc_error_ok && match)) {
			*value = COLOR_TRANSPARENT;
			*result = 0; /* black transparent */
			return CSS_OK;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[CURRENTCOLOR],
				&match) == lwc_error_ok && match)) {
			*value = COLOR_CURRENT_COLOR;
			*result = 0;
			return CSS_OK;
		}

		error = css__parse_named_colour(c, token->idata, result);
		if (error != CSS_OK && c->sheet->quirks_allowed) {
			error = css__parse_hash_colour(token->idata, result);
			if (error == CSS_OK)
				c->sheet->quirks_used = true;
		}

		if (error != CSS_OK)
			goto invalid;
	} else if (token->type == CSS_TOKEN_HASH) {
		error = css__parse_hash_colour(token->idata, result);
		if (error != CSS_OK)
			goto invalid;
	} else if (c->sheet->quirks_allowed &&
			token->type == CSS_TOKEN_NUMBER) {
		error = css__parse_hash_colour(token->idata, result);
		if (error == CSS_OK)
			c->sheet->quirks_used = true;
		else
			goto invalid;
	} else if (c->sheet->quirks_allowed &&
			token->type == CSS_TOKEN_DIMENSION) {
		error = css__parse_hash_colour(token->idata, result);
		if (error == CSS_OK)
			c->sheet->quirks_used = true;
		else
			goto invalid;
	} else if (token->type == CSS_TOKEN_FUNCTION) {
		uint8_t r = 0, g = 0, b = 0, a = 0xff;
		int colour_channels = 0;

		if ((lwc_string_caseless_isequal(
				token->idata, c->strings[RGB],
				&match) == lwc_error_ok && match)) {
			colour_channels = 3;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[RGBA],
				&match) == lwc_error_ok && match)) {
			colour_channels = 4;
		} if ((lwc_string_caseless_isequal(
				token->idata, c->strings[HSL],
				&match) == lwc_error_ok && match)) {
			colour_channels = 5;
		} else if ((lwc_string_caseless_isequal(
				token->idata, c->strings[HSLA],
				&match) == lwc_error_ok && match)) {
			colour_channels = 6;
		}

		if (colour_channels == 3 || colour_channels == 4) {
			int i;
			css_token_type valid = CSS_TOKEN_NUMBER;
			uint8_t *components[4] = { &r, &g, &b, &a };

			for (i = 0; i < colour_channels; i++) {
				uint8_t *component;
				css_fixed num;
				size_t consumed = 0;
				int32_t intval;
				bool int_only;

				component = components[i];

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL || (token->type !=
						CSS_TOKEN_NUMBER &&
						token->type !=
						CSS_TOKEN_PERCENTAGE))
					goto invalid;

				if (i == 0)
					valid = token->type;
				else if (i < 3 && token->type != valid)
					goto invalid;

				/* The alpha channel may be a float */
				if (i < 3)
					int_only = (valid == CSS_TOKEN_NUMBER);
				else
					int_only = false;

				num = css__number_from_lwc_string(token->idata,
						int_only, &consumed);
				if (consumed != lwc_string_length(token->idata))
					goto invalid;

				if (valid == CSS_TOKEN_NUMBER) {
					if (i == 3) {
						/* alpha channel */
						intval = FIXTOINT(
							FMUL(num, F_255));
					} else {
						/* colour channels */
						intval = FIXTOINT(num);
					}
				} else {
					intval = FIXTOINT(
						FDIV(FMUL(num, F_255), F_100));
				}

				if (intval > 255)
					*component = 255;
				else if (intval < 0)
					*component = 0;
				else
					*component = intval;

				parserutils_vector_iterate(vector, ctx);

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_peek(vector, *ctx);
				if (token == NULL)
					goto invalid;

				if (i != (colour_channels - 1) &&
						tokenIsChar(token, ',')) {
					parserutils_vector_iterate(vector, ctx);
				} else if (i == (colour_channels - 1) &&
						tokenIsChar(token, ')')) {
					parserutils_vector_iterate(vector, ctx);
				} else {
					goto invalid;
				}
			}
		} else if (colour_channels == 5 || colour_channels == 6) {
			/* hue - saturation - lightness */
			size_t consumed = 0;
			css_fixed hue, sat, lit;
			int32_t alpha = 255;

			/* hue is a number without a unit representing an 
			 * angle (0-360) degrees  
			 */
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);
			if ((token == NULL) || (token->type != CSS_TOKEN_NUMBER))
				goto invalid;

			hue = css__number_from_lwc_string(token->idata, false, &consumed);
			if (consumed != lwc_string_length(token->idata))
				goto invalid; /* failed to consume the whole string as a number */

			/* Normalise hue to the range [0, 360) */
			while (hue < 0)
				hue += F_360;
			while (hue >= F_360)
				hue -= F_360;

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);
			if (!tokenIsChar(token, ','))
				goto invalid;


			/* saturation */
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);
			if ((token == NULL) || (token->type != CSS_TOKEN_PERCENTAGE))
				goto invalid;

			sat = css__number_from_lwc_string(token->idata, false, &consumed);
			if (consumed != lwc_string_length(token->idata))
				goto invalid; /* failed to consume the whole string as a number */

			/* Normalise saturation to the range [0, 100] */
			if (sat < INTTOFIX(0))
				sat = INTTOFIX(0);
			else if (sat > INTTOFIX(100))
				sat = INTTOFIX(100);

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);
			if (!tokenIsChar(token, ','))
				goto invalid;


			/* lightness */
			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);
			if ((token == NULL) || (token->type != CSS_TOKEN_PERCENTAGE))
				goto invalid;

			lit = css__number_from_lwc_string(token->idata, false, &consumed);
			if (consumed != lwc_string_length(token->idata))
				goto invalid; /* failed to consume the whole string as a number */

			/* Normalise lightness to the range [0, 100] */
			if (lit < INTTOFIX(0))
				lit = INTTOFIX(0);
			else if (lit > INTTOFIX(100))
				lit = INTTOFIX(100);

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_iterate(vector, ctx);

			if (colour_channels == 6) {
				/* alpha */

				if (!tokenIsChar(token, ','))
					goto invalid;
			
				consumeWhitespace(vector, ctx);

				token = parserutils_vector_iterate(vector, ctx);
				if ((token == NULL) || (token->type != CSS_TOKEN_NUMBER))
					goto invalid;

				alpha = css__number_from_lwc_string(token->idata, false, &consumed);
				if (consumed != lwc_string_length(token->idata))
					goto invalid; /* failed to consume the whole string as a number */
				
				alpha = FIXTOINT(FMUL(alpha, F_255));

				consumeWhitespace(vector, ctx);

				token = parserutils_vector_iterate(vector, ctx);

			}

			if (!tokenIsChar(token, ')'))
				goto invalid;

			/* have a valid HSV entry, convert to RGB */
			HSL_to_RGB(hue, sat, lit, &r, &g, &b);

			/* apply alpha */
			if (alpha > 255)
				a = 255;
			else if (alpha < 0)
				a = 0;
			else
				a = alpha;

		} else {
			goto invalid;
		}

		*result = (a << 24) | (r << 16) | (g << 8) | b;
	}

	*value = COLOR_SET;

	return CSS_OK;

invalid:
	*ctx = orig_ctx;
	return CSS_INVALID;
}

/**
 * Parse a named colour
 *
 * \param c       Parsing context
 * \param data    Colour name string
 * \param result  Pointer to location to receive result
 * \return CSS_OK      on success,
 *         CSS_INVALID if the colour name is unknown
 */
css_error css__parse_named_colour(css_language *c, lwc_string *data,
		uint32_t *result)
{
	static const uint32_t colourmap[LAST_COLOUR + 1 - FIRST_COLOUR] = {
		0xfff0f8ff, /* ALICEBLUE */
		0xfffaebd7, /* ANTIQUEWHITE */
		0xff00ffff, /* AQUA */
		0xff7fffd4, /* AQUAMARINE */
		0xfff0ffff, /* AZURE */
		0xfff5f5dc, /* BEIGE */
		0xffffe4c4, /* BISQUE */
		0xff000000, /* BLACK */
		0xffffebcd, /* BLANCHEDALMOND */
		0xff0000ff, /* BLUE */
		0xff8a2be2, /* BLUEVIOLET */
		0xffa52a2a, /* BROWN */
		0xffdeb887, /* BURLYWOOD */
		0xff5f9ea0, /* CADETBLUE */
		0xff7fff00, /* CHARTREUSE */
		0xffd2691e, /* CHOCOLATE */
		0xffff7f50, /* CORAL */
		0xff6495ed, /* CORNFLOWERBLUE */
		0xfffff8dc, /* CORNSILK */
		0xffdc143c, /* CRIMSON */
		0xff00ffff, /* CYAN */
		0xff00008b, /* DARKBLUE */
		0xff008b8b, /* DARKCYAN */
		0xffb8860b, /* DARKGOLDENROD */
		0xffa9a9a9, /* DARKGRAY */
		0xff006400, /* DARKGREEN */
		0xffa9a9a9, /* DARKGREY */
		0xffbdb76b, /* DARKKHAKI */
		0xff8b008b, /* DARKMAGENTA */
		0xff556b2f, /* DARKOLIVEGREEN */
		0xffff8c00, /* DARKORANGE */
		0xff9932cc, /* DARKORCHID */
		0xff8b0000, /* DARKRED */
		0xffe9967a, /* DARKSALMON */
		0xff8fbc8f, /* DARKSEAGREEN */
		0xff483d8b, /* DARKSLATEBLUE */
		0xff2f4f4f, /* DARKSLATEGRAY */
		0xff2f4f4f, /* DARKSLATEGREY */
		0xff00ced1, /* DARKTURQUOISE */
		0xff9400d3, /* DARKVIOLET */
		0xffff1493, /* DEEPPINK */
		0xff00bfff, /* DEEPSKYBLUE */
		0xff696969, /* DIMGRAY */
		0xff696969, /* DIMGREY */
		0xff1e90ff, /* DODGERBLUE */
		0xffd19275, /* FELDSPAR */
		0xffb22222, /* FIREBRICK */
		0xfffffaf0, /* FLORALWHITE */
		0xff228b22, /* FORESTGREEN */
		0xffff00ff, /* FUCHSIA */
		0xffdcdcdc, /* GAINSBORO */
		0xfff8f8ff, /* GHOSTWHITE */
		0xffffd700, /* GOLD */
		0xffdaa520, /* GOLDENROD */
		0xff808080, /* GRAY */
		0xff008000, /* GREEN */
		0xffadff2f, /* GREENYELLOW */
		0xff808080, /* GREY */
		0xfff0fff0, /* HONEYDEW */
		0xffff69b4, /* HOTPINK */
		0xffcd5c5c, /* INDIANRED */
		0xff4b0082, /* INDIGO */
		0xfffffff0, /* IVORY */
		0xfff0e68c, /* KHAKI */
		0xffe6e6fa, /* LAVENDER */
		0xfffff0f5, /* LAVENDERBLUSH */
		0xff7cfc00, /* LAWNGREEN */
		0xfffffacd, /* LEMONCHIFFON */
		0xffadd8e6, /* LIGHTBLUE */
		0xfff08080, /* LIGHTCORAL */
		0xffe0ffff, /* LIGHTCYAN */
		0xfffafad2, /* LIGHTGOLDENRODYELLOW */
		0xffd3d3d3, /* LIGHTGRAY */
		0xff90ee90, /* LIGHTGREEN */
		0xffd3d3d3, /* LIGHTGREY */
		0xffffb6c1, /* LIGHTPINK */
		0xffffa07a, /* LIGHTSALMON */
		0xff20b2aa, /* LIGHTSEAGREEN */
		0xff87cefa, /* LIGHTSKYBLUE */
		0xff8470ff, /* LIGHTSLATEBLUE */
		0xff778899, /* LIGHTSLATEGRAY */
		0xff778899, /* LIGHTSLATEGREY */
		0xffb0c4de, /* LIGHTSTEELBLUE */
		0xffffffe0, /* LIGHTYELLOW */
		0xff00ff00, /* LIME */
		0xff32cd32, /* LIMEGREEN */
		0xfffaf0e6, /* LINEN */
		0xffff00ff, /* MAGENTA */
		0xff800000, /* MAROON */
		0xff66cdaa, /* MEDIUMAQUAMARINE */
		0xff0000cd, /* MEDIUMBLUE */
		0xffba55d3, /* MEDIUMORCHID */
		0xff9370db, /* MEDIUMPURPLE */
		0xff3cb371, /* MEDIUMSEAGREEN */
		0xff7b68ee, /* MEDIUMSLATEBLUE */
		0xff00fa9a, /* MEDIUMSPRINGGREEN */
		0xff48d1cc, /* MEDIUMTURQUOISE */
		0xffc71585, /* MEDIUMVIOLETRED */
		0xff191970, /* MIDNIGHTBLUE */
		0xfff5fffa, /* MINTCREAM */
		0xffffe4e1, /* MISTYROSE */
		0xffffe4b5, /* MOCCASIN */
		0xffffdead, /* NAVAJOWHITE */
		0xff000080, /* NAVY */
		0xfffdf5e6, /* OLDLACE */
		0xff808000, /* OLIVE */
		0xff6b8e23, /* OLIVEDRAB */
		0xffffa500, /* ORANGE */
		0xffff4500, /* ORANGERED */
		0xffda70d6, /* ORCHID */
		0xffeee8aa, /* PALEGOLDENROD */
		0xff98fb98, /* PALEGREEN */
		0xffafeeee, /* PALETURQUOISE */
		0xffdb7093, /* PALEVIOLETRED */
		0xffffefd5, /* PAPAYAWHIP */
		0xffffdab9, /* PEACHPUFF */
		0xffcd853f, /* PERU */
		0xffffc0cb, /* PINK */
		0xffdda0dd, /* PLUM */
		0xffb0e0e6, /* POWDERBLUE */
		0xff800080, /* PURPLE */
		0xffff0000, /* RED */
		0xffbc8f8f, /* ROSYBROWN */
		0xff4169e1, /* ROYALBLUE */
		0xff8b4513, /* SADDLEBROWN */
		0xfffa8072, /* SALMON */
		0xfff4a460, /* SANDYBROWN */
		0xff2e8b57, /* SEAGREEN */
		0xfffff5ee, /* SEASHELL */
		0xffa0522d, /* SIENNA */
		0xffc0c0c0, /* SILVER */
		0xff87ceeb, /* SKYBLUE */
		0xff6a5acd, /* SLATEBLUE */
		0xff708090, /* SLATEGRAY */
		0xff708090, /* SLATEGREY */
		0xfffffafa, /* SNOW */
		0xff00ff7f, /* SPRINGGREEN */
		0xff4682b4, /* STEELBLUE */
		0xffd2b48c, /* TAN */
		0xff008080, /* TEAL */
		0xffd8bfd8, /* THISTLE */
		0xffff6347, /* TOMATO */
		0xff40e0d0, /* TURQUOISE */
		0xffee82ee, /* VIOLET */
		0xffd02090, /* VIOLETRED */
		0xfff5deb3, /* WHEAT */
		0xffffffff, /* WHITE */
		0xfff5f5f5, /* WHITESMOKE */
		0xffffff00, /* YELLOW */
		0xff9acd32  /* YELLOWGREEN */
	};
	int i;
	bool match;

	for (i = FIRST_COLOUR; i <= LAST_COLOUR; i++) {
		if (lwc_string_caseless_isequal(data, c->strings[i],
				&match) == lwc_error_ok && match)
			break;
	}

	if (i <= LAST_COLOUR) {
		/* Known named colour */
		*result = colourmap[i - FIRST_COLOUR];
		return CSS_OK;
	}

	/* We don't know this colour name; ask the client */
	if (c->sheet->color != NULL)
		return c->sheet->color(c->sheet->color_pw, data, result);

	/* Invalid colour name */
	return CSS_INVALID;
}

/**
 * Parse a hash colour (#rgb or #rrggbb)
 *
 * \param data    Pointer to colour string
 * \param result  Pointer to location to receive result (AARRGGBB)
 * \return CSS_OK      on success,
 *         CSS_INVALID if the input is invalid
 */
css_error css__parse_hash_colour(lwc_string *data, uint32_t *result)
{
	uint8_t r = 0, g = 0, b = 0, a = 0xff;
	size_t len = lwc_string_length(data);
	const char *input = lwc_string_data(data);

	if (len == 3 &&	isHex(input[0]) && isHex(input[1]) &&
			isHex(input[2])) {
		r = charToHex(input[0]);
		g = charToHex(input[1]);
		b = charToHex(input[2]);

		r |= (r << 4);
		g |= (g << 4);
		b |= (b << 4);
	} else if (len == 6 && isHex(input[0]) && isHex(input[1]) &&
			isHex(input[2]) && isHex(input[3]) &&
			isHex(input[4]) && isHex(input[5])) {
		r = (charToHex(input[0]) << 4);
		r |= charToHex(input[1]);
		g = (charToHex(input[2]) << 4);
		g |= charToHex(input[3]);
		b = (charToHex(input[4]) << 4);
		b |= charToHex(input[5]);
	} else
		return CSS_INVALID;

	*result = (a << 24) | (r << 16) | (g << 8) | b;

	return CSS_OK;
}

/**
 * Parse a unit specifier
 *
 * \param c             Parsing context
 * \param vector        Vector of tokens to process
 * \param ctx           Pointer to current vector iteration context
 * \param default_unit  The default unit to use if none specified
 * \param length        Pointer to location to receive length
 * \param unit          Pointer to location to receive unit
 * \return CSS_OK      on success,
 *         CSS_INVALID if the tokens do not form a valid unit
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_unit_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t default_unit,
		css_fixed *length, uint32_t *unit)
{
	int orig_ctx = *ctx;
	const css_token *token;
	css_fixed num;
	size_t consumed = 0;
	css_error error;

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || (token->type != CSS_TOKEN_DIMENSION &&
			token->type != CSS_TOKEN_NUMBER &&
			token->type != CSS_TOKEN_PERCENTAGE)) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	num = css__number_from_lwc_string(token->idata, false, &consumed);

	if (token->type == CSS_TOKEN_DIMENSION) {
		size_t len = lwc_string_length(token->idata);
		const char *data = lwc_string_data(token->idata);
		css_unit temp_unit = CSS_UNIT_PX;

		error = css__parse_unit_keyword(data + consumed, len - consumed,
				&temp_unit);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}

		*unit = (uint32_t) temp_unit;
	} else if (token->type == CSS_TOKEN_NUMBER) {
		/* Non-zero values are permitted in quirks mode */
		if (num != 0) {
			if (c->sheet->quirks_allowed) {
				c->sheet->quirks_used = true;
			} else {
				*ctx = orig_ctx;
				return CSS_INVALID;
			}
		}

		*unit = default_unit;

		if (c->sheet->quirks_allowed) {
			/* Also, in quirks mode, we need to cater for
			 * dimensions separated from their units by whitespace
			 * (e.g. "0 px")
			 */
			int temp_ctx = *ctx;
			css_unit temp_unit;

			consumeWhitespace(vector, &temp_ctx);

			/* Try to parse the unit keyword, ignoring errors */
			token = parserutils_vector_iterate(vector, &temp_ctx);
			if (token != NULL && token->type == CSS_TOKEN_IDENT) {
				error = css__parse_unit_keyword(
						lwc_string_data(token->idata),
						lwc_string_length(token->idata),
						&temp_unit);
				if (error == CSS_OK) {
					c->sheet->quirks_used = true;
					*ctx = temp_ctx;
					*unit = (uint32_t) temp_unit;
				}
			}
		}
	} else {
		/* Percentage -- number must be entire token data */
		if (consumed != lwc_string_length(token->idata)) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
		*unit = UNIT_PCT;
	}

	*length = num;

	return CSS_OK;
}

/**
 * Parse a unit keyword
 *
 * \param ptr   Pointer to keyword string
 * \param len   Length, in bytes, of string
 * \param unit  Pointer to location to receive computed unit
 * \return CSS_OK      on success,
 *         CSS_INVALID on encountering an unknown keyword
 */
css_error css__parse_unit_keyword(const char *ptr, size_t len, uint32_t *unit)
{
	if (len == 4) {
		if (strncasecmp(ptr, "grad", 4) == 0)
			*unit = UNIT_GRAD;
		else
			return CSS_INVALID;
	} else if (len == 3) {
		if (strncasecmp(ptr, "kHz", 3) == 0)
			*unit = UNIT_KHZ;
		else if (strncasecmp(ptr, "deg", 3) == 0)
			*unit = UNIT_DEG;
		else if (strncasecmp(ptr, "rad", 3) == 0)
			*unit = UNIT_RAD;
		else
			return CSS_INVALID;
	} else if (len == 2) {
		if (strncasecmp(ptr, "Hz", 2) == 0)
			*unit = UNIT_HZ;
		else if (strncasecmp(ptr, "ms", 2) == 0)
			*unit = UNIT_MS;
		else if (strncasecmp(ptr, "px", 2) == 0)
			*unit = UNIT_PX;
		else if (strncasecmp(ptr, "ex", 2) == 0)
			*unit = UNIT_EX;
		else if (strncasecmp(ptr, "em", 2) == 0)
			*unit = UNIT_EM;
		else if (strncasecmp(ptr, "in", 2) == 0)
			*unit = UNIT_IN;
		else if (strncasecmp(ptr, "cm", 2) == 0)
			*unit = UNIT_CM;
		else if (strncasecmp(ptr, "mm", 2) == 0)
			*unit = UNIT_MM;
		else if (strncasecmp(ptr, "pt", 2) == 0)
			*unit = UNIT_PT;
		else if (strncasecmp(ptr, "pc", 2) == 0)
			*unit = UNIT_PC;
		else
			return CSS_INVALID;
	} else if (len == 1) {
		if (strncasecmp(ptr, "s", 1) == 0)
			*unit = UNIT_S;
		else
			return CSS_INVALID;
	} else
		return CSS_INVALID;

	return CSS_OK;
}

/**
 * Create a string from a list of IDENT/S tokens if the next token is IDENT
 * or references the next token's string if it is a STRING
 *
 * \param c          Parsing context
 * \param vector     Vector containing tokens
 * \param ctx        Vector iteration context
 * \param reserved   Callback to determine if an identifier is reserved
 * \param result     Pointer to location to receive resulting string
 * \return CSS_OK on success, appropriate error otherwise.
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 *
 *                 The resulting string's reference is passed to the caller
 */
css_error css__ident_list_or_string_to_string(css_language *c,
		const parserutils_vector *vector, int *ctx,
		bool (*reserved)(css_language *c, const css_token *ident),
		lwc_string **result)
{
	const css_token *token;
	
	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;
	
	if (token->type == CSS_TOKEN_STRING) {
		token = parserutils_vector_iterate(vector, ctx);
		*result = lwc_string_ref(token->idata);
		return CSS_OK;
	} else 	if(token->type == CSS_TOKEN_IDENT) {
		return css__ident_list_to_string(c, vector, ctx, reserved,
				result);
	}
	
	return CSS_INVALID;
}

/**
 * Create a string from a list of IDENT/S tokens
 *
 * \param c          Parsing context
 * \param vector     Vector containing tokens
 * \param ctx        Vector iteration context
 * \param reserved   Callback to determine if an identifier is reserved
 * \param result     Pointer to location to receive resulting string
 * \return CSS_OK on success, appropriate error otherwise.
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 *
 *                 The resulting string's reference is passed to the caller
 */
css_error css__ident_list_to_string(css_language *c,
		const parserutils_vector *vector, int *ctx,
		bool (*reserved)(css_language *c, const css_token *ident),
		lwc_string **result)
{
	int orig_ctx = *ctx;
	const css_token *token;
	css_error error = CSS_OK;
	parserutils_buffer *buffer;
	parserutils_error perror;
	lwc_string *interned;
	lwc_error lerror;

	perror = parserutils_buffer_create(&buffer);
	if (perror != PARSERUTILS_OK)
		return css_error_from_parserutils_error(perror);

	/* We know this token exists, and is an IDENT */
	token = parserutils_vector_iterate(vector, ctx);

	/* Consume all subsequent IDENT or S tokens */	
	while (token != NULL && (token->type == CSS_TOKEN_IDENT ||
			token->type == CSS_TOKEN_S)) {
		if (token->type == CSS_TOKEN_IDENT) {
			/* IDENT -- if reserved, reject style */
			if (reserved != NULL && reserved(c, token)) {
				error = CSS_INVALID;
				goto cleanup;
			}

			perror = parserutils_buffer_append(buffer, 
					(const uint8_t *) lwc_string_data(token->idata), 
					lwc_string_length(token->idata));
		} else {
			/* S */
			perror = parserutils_buffer_append(buffer, 
					(const uint8_t *) " ", 1);
		}

		if (perror != PARSERUTILS_OK) {
			error = css_error_from_parserutils_error(perror);
			goto cleanup;
		}

		token = parserutils_vector_iterate(vector, ctx);
	}

	/* Rewind context by one step if we consumed an unacceptable token */
	if (token != NULL)
		*ctx = *ctx - 1;

	/* Strip trailing whitespace */
	while (buffer->length > 0 && buffer->data[buffer->length - 1] == ' ')
		buffer->length--;

	/* Intern the buffer contents */
	lerror = lwc_intern_string((char *) buffer->data, buffer->length, &interned);
	if (lerror != lwc_error_ok) {
		error = css_error_from_lwc_error(lerror);
		goto cleanup;
	}

	*result = interned;

cleanup:
	parserutils_buffer_destroy(buffer);

	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}

/**
 * Parse a comma separated list, converting to bytecode
 *
 * \param c          Parsing context
 * \param vector     Vector of tokens to process
 * \param ctx        Pointer to vector iteration context
 * \param reserved   Callback to determine if an identifier is reserved
 * \param get_value  Callback to retrieve bytecode value for a token
 * \param style      Pointer to output style
 * \return CSS_OK      on success,
 *         CSS_INVALID if the input is invalid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__comma_list_to_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		bool (*reserved)(css_language *c, const css_token *ident),
		css_code_t (*get_value)(css_language *c, const css_token *token, bool first),
		css_style *result)
{
	int orig_ctx = *ctx;
	int prev_ctx = orig_ctx;
	const css_token *token;
	bool first = true;
	css_error error = CSS_OK;

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	while (token != NULL) {
		if (token->type == CSS_TOKEN_IDENT) {
			css_code_t value = get_value(c, token, first);

			if (reserved(c, token) == false) {
				lwc_string *str = NULL;
				uint32_t snumber;

				*ctx = prev_ctx;

				error = css__ident_list_to_string(c, vector, ctx,
						reserved, &str);
				if (error != CSS_OK)
					goto cleanup;

				error = css__stylesheet_string_add(c->sheet,
						str, &snumber);
				if (error != CSS_OK)
					goto cleanup;

				error = css__stylesheet_style_append(result, 
						value);
				if (error != CSS_OK)
					goto cleanup;

				error = css__stylesheet_style_append(result,
						snumber);
				if (error != CSS_OK)
					goto cleanup;
			} else {
				error = css__stylesheet_style_append(result,
						value);
				if (error != CSS_OK)
					goto cleanup;
			}
		} else if (token->type == CSS_TOKEN_STRING) {
			css_code_t value = get_value(c, token, first);
			uint32_t snumber;

			error = css__stylesheet_string_add(c->sheet, 
					lwc_string_ref(token->idata), &snumber);
			if (error != CSS_OK)
				goto cleanup;

			error = css__stylesheet_style_append(result, value);
			if (error != CSS_OK)
				goto cleanup;

			error = css__stylesheet_style_append(result, snumber);
			if (error != CSS_OK)
				goto cleanup;
		} else {
			error = CSS_INVALID;
			goto cleanup;
		}

		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
		if (token != NULL && tokenIsChar(token, ',')) {
			parserutils_vector_iterate(vector, ctx);

			consumeWhitespace(vector, ctx);

			token = parserutils_vector_peek(vector, *ctx);
			if (token == NULL || (token->type != CSS_TOKEN_IDENT &&
					token->type != CSS_TOKEN_STRING)) {
				error = CSS_INVALID;
				goto cleanup;
			}
		} else {
			break;
		}

		first = false;

		prev_ctx = *ctx;

		token = parserutils_vector_iterate(vector, ctx);
	}

cleanup:
	if (error != CSS_OK)
		*ctx = orig_ctx;

	return error;
}

