/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_css__parse_properties_utils_h_
#define css_css__parse_properties_utils_h_

#include "parse/language.h"

static inline bool is_css_inherit(css_language *c, const css_token *token) 
{
	bool match;
	return ((token->type == CSS_TOKEN_IDENT) &&
		(lwc_string_caseless_isequal(
			token->idata, c->strings[INHERIT],
			&match) == lwc_error_ok && match));
}

enum border_side_e { BORDER_SIDE_TOP = 0, BORDER_SIDE_RIGHT = 1, BORDER_SIDE_BOTTOM = 2, BORDER_SIDE_LEFT = 3 };

/**
 * Parse border-{top,right,bottom,left} shorthand
 *
 * \param c	  Parsing context.
 * \param vector  Vector of tokens to process.
 * \param ctx	  Pointer to vector iteration context.
 * \param result  Result style.
 * \param side	  The side we're parsing for.
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_border_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum border_side_e side);

/**
 * Parse border-{top,right,bottom,left}-color
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \param op	  Opcode to parse for (encodes side)
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_border_side_color(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum css_properties_e op);

/**
 * Parse border-{top,right,bottom,left}-style
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \param op	  Opcode to parse for (encodes side)
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_border_side_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum css_properties_e op);


/**
 * Parse border-{top,right,bottom,left}-width
 *
 * \param c	  Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx	  Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \param op	  Opcode to parse for (encodes side)
 * \return CSS_OK on success,
 *	   CSS_NOMEM on memory exhaustion,
 *	   CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *		   If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_border_side_width(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum css_properties_e op);


/**
 * Parse {top,right,bottom,left}
 *
 * \param c       Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx     Pointer to vector iteration context
 * \param op      Opcode to parse for
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion,
 *         CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum css_properties_e op);


/**
 * Parse margin-{top,right,bottom,left}
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
css_error css__parse_margin_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum css_properties_e op);

/**
 * Parse padding-{top,right,bottom,left}
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
css_error css__parse_padding_side(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result, enum css_properties_e op);







css_error css__parse_list_style_type_value(css_language *c,
		const css_token *token, uint16_t *value);

css_error css__parse_colour_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint16_t *value, uint32_t *result);

css_error css__parse_named_colour(css_language *c, lwc_string *data, 
		uint32_t *result);

css_error css__parse_hash_colour(lwc_string *data, uint32_t *result);

css_error css__parse_unit_specifier(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint32_t default_unit,
		css_fixed *length, uint32_t *unit);

css_error css__parse_unit_keyword(const char *ptr, size_t len, 
		uint32_t *unit);

css_error css__ident_list_or_string_to_string(css_language *c,
		const parserutils_vector *vector, int *ctx,
		bool (*reserved)(css_language *c, const css_token *ident),
		lwc_string **result);

css_error css__ident_list_to_string(css_language *c,
		const parserutils_vector *vector, int *ctx,
		bool (*reserved)(css_language *c, const css_token *ident),
		lwc_string **result);

css_error css__comma_list_to_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		bool (*reserved)(css_language *c, const css_token *ident),
		css_code_t (*get_value)(css_language *c,
				const css_token *token, 
				bool first),
		css_style *result);

#endif
