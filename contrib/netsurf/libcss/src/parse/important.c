/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/important.h"

/**
 * Parse !important
 *
 * \param c       Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx     Pointer to vector iteration context
 * \param result  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_INVALID if "S* ! S* important" is not at the start of the vector
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_important(css_language *c,
		const parserutils_vector *vector, int *ctx,
		uint8_t *result)
{
	int orig_ctx = *ctx;
	bool match = false;
	const css_token *token;

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_iterate(vector, ctx);
	if (token != NULL && tokenIsChar(token, '!')) {
		consumeWhitespace(vector, ctx);

		token = parserutils_vector_iterate(vector, ctx);
		if (token == NULL || token->type != CSS_TOKEN_IDENT) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}

		if (lwc_string_caseless_isequal(token->idata, c->strings[IMPORTANT],
				&match) == lwc_error_ok && match) {
			*result |= FLAG_IMPORTANT;
		} else {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
	} else if (token != NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	return CSS_OK;
}

/**
 * Make a style important
 *
 * \param style  The style to modify
 */
void css__make_style_important(css_style *style)
{
	css_code_t *bytecode = style->bytecode;
	uint32_t length = style->used;
	uint32_t offset = 0;

	while (offset < length) {
		opcode_t op;
		uint8_t flags;
		uint32_t value;
		css_code_t opv = bytecode[offset];

		/* Extract opv components, setting important flag */
		op = getOpcode(opv);
		flags = getFlags(opv) | FLAG_IMPORTANT;
		value = getValue(opv);

		/* Write OPV back to bytecode */
		bytecode[offset] = buildOPV(op, flags, value);

		offset++;

		/* Advance past any property-specific data */
		if (isInherit(opv) == false) {
			switch (op) {
			case CSS_PROP_AZIMUTH:
				if ((value & ~AZIMUTH_BEHIND) == AZIMUTH_ANGLE)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_BORDER_TOP_COLOR:
			case CSS_PROP_BORDER_RIGHT_COLOR:
			case CSS_PROP_BORDER_BOTTOM_COLOR:
			case CSS_PROP_BORDER_LEFT_COLOR:
			case CSS_PROP_BACKGROUND_COLOR:
			case CSS_PROP_COLUMN_RULE_COLOR:
				assert(BACKGROUND_COLOR_SET == 
				       (enum op_background_color)BORDER_COLOR_SET);
				assert(BACKGROUND_COLOR_SET == 
				       (enum op_background_color)COLUMN_RULE_COLOR_SET);

				if (value == BACKGROUND_COLOR_SET)
					offset++; /* colour */
				break;

			case CSS_PROP_BACKGROUND_IMAGE:
			case CSS_PROP_CUE_AFTER:
			case CSS_PROP_CUE_BEFORE:
			case CSS_PROP_LIST_STYLE_IMAGE:
				assert(BACKGROUND_IMAGE_URI == 
				       (enum op_background_image)CUE_AFTER_URI);
				assert(BACKGROUND_IMAGE_URI == 
				       (enum op_background_image)CUE_BEFORE_URI);
				assert(BACKGROUND_IMAGE_URI ==
				       (enum op_background_image)LIST_STYLE_IMAGE_URI);

				if (value == BACKGROUND_IMAGE_URI) 
					offset++; /* string table entry */
				break;

			case CSS_PROP_BACKGROUND_POSITION:
				if ((value & 0xf0) == BACKGROUND_POSITION_HORZ_SET)
					offset += 2; /* length + units */

				if ((value & 0x0f) == BACKGROUND_POSITION_VERT_SET)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_BORDER_SPACING:
				if (value == BORDER_SPACING_SET)
					offset += 4; /* two length + units */
				break;

			case CSS_PROP_BORDER_TOP_WIDTH:
			case CSS_PROP_BORDER_RIGHT_WIDTH:
			case CSS_PROP_BORDER_BOTTOM_WIDTH:
			case CSS_PROP_BORDER_LEFT_WIDTH:
			case CSS_PROP_OUTLINE_WIDTH:
			case CSS_PROP_COLUMN_RULE_WIDTH:
				assert(BORDER_WIDTH_SET == 
				       (enum op_border_width)OUTLINE_WIDTH_SET);
				assert(BORDER_WIDTH_SET ==
				       (enum op_border_width)COLUMN_RULE_WIDTH_SET);

				if (value == BORDER_WIDTH_SET)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_MARGIN_TOP:
			case CSS_PROP_MARGIN_RIGHT:
			case CSS_PROP_MARGIN_BOTTOM:
			case CSS_PROP_MARGIN_LEFT:
			case CSS_PROP_BOTTOM:
			case CSS_PROP_LEFT:
			case CSS_PROP_RIGHT:
			case CSS_PROP_TOP:
			case CSS_PROP_HEIGHT:
			case CSS_PROP_WIDTH:
			case CSS_PROP_COLUMN_WIDTH:
			case CSS_PROP_COLUMN_GAP:
				assert(BOTTOM_SET == (enum op_bottom)LEFT_SET);
				assert(BOTTOM_SET == (enum op_bottom)RIGHT_SET);
				assert(BOTTOM_SET == (enum op_bottom)TOP_SET);
				assert(BOTTOM_SET == (enum op_bottom)HEIGHT_SET);
				assert(BOTTOM_SET == (enum op_bottom)MARGIN_SET);
				assert(BOTTOM_SET == (enum op_bottom)WIDTH_SET);
				assert(BOTTOM_SET == (enum op_bottom)COLUMN_WIDTH_SET);
				assert(BOTTOM_SET == (enum op_bottom)COLUMN_GAP_SET);

				if (value == BOTTOM_SET) 
					offset += 2; /* length + units */
				break;

			case CSS_PROP_CLIP:
				if ((value & CLIP_SHAPE_MASK) == CLIP_SHAPE_RECT) {
					if ((value & CLIP_RECT_TOP_AUTO) == 0)
						offset += 2; /* length + units */

					if ((value & CLIP_RECT_RIGHT_AUTO) == 0)
						offset += 2; /* length + units */

					if ((value & CLIP_RECT_BOTTOM_AUTO) == 0)
						offset += 2; /* length + units */

					if ((value & CLIP_RECT_LEFT_AUTO) == 0)
						offset += 2; /* length + units */

				}
				break;

			case CSS_PROP_COLOR:
				if (value == COLOR_SET)
					offset++; /* colour */
				break;

			case CSS_PROP_COLUMN_COUNT:
				if (value == COLUMN_COUNT_SET)
					offset++; /* colour */
				break;

			case CSS_PROP_CONTENT:
				while (value != CONTENT_NORMAL &&
						value != CONTENT_NONE) {
					switch (value & 0xff) {
					case CONTENT_COUNTER:
					case CONTENT_URI:
					case CONTENT_ATTR:
					case CONTENT_STRING:
						offset++; /* string table entry */
						break;

					case CONTENT_COUNTERS:
						offset+=2; /* two string entries */
						break;

					case CONTENT_OPEN_QUOTE:
					case CONTENT_CLOSE_QUOTE:
					case CONTENT_NO_OPEN_QUOTE:
					case CONTENT_NO_CLOSE_QUOTE:
						break;
					}

					value = bytecode[offset];
				        offset++;
				}
				break;

			case CSS_PROP_COUNTER_INCREMENT:
			case CSS_PROP_COUNTER_RESET:
				assert(COUNTER_INCREMENT_NONE == 
				       (enum op_counter_increment)COUNTER_RESET_NONE);

				while (value != COUNTER_INCREMENT_NONE) {
					offset+=2; /* string + integer */

					value = bytecode[offset];
				        offset++;
				}
				break;

			case CSS_PROP_CURSOR:
				while (value == CURSOR_URI) {
					offset++; /* string table entry */

					value = bytecode[offset];
				        offset++;
				}
				break;

			case CSS_PROP_ELEVATION:
				if (value == ELEVATION_ANGLE)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_FONT_FAMILY:
				while (value != FONT_FAMILY_END) {
					switch (value) {
					case FONT_FAMILY_STRING:
					case FONT_FAMILY_IDENT_LIST:
						offset++; /* string table entry */
						break;
					}

					value = bytecode[offset];
				        offset++;
				}
				break;

			case CSS_PROP_FONT_SIZE:
				if (value == FONT_SIZE_DIMENSION) 
					offset += 2; /* length + units */
				break;

			case CSS_PROP_LETTER_SPACING:
			case CSS_PROP_WORD_SPACING:
				assert(LETTER_SPACING_SET == 
				       (enum op_letter_spacing)WORD_SPACING_SET);

				if (value == LETTER_SPACING_SET)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_LINE_HEIGHT:
				switch (value) {
				case LINE_HEIGHT_NUMBER:
					offset++; /* value */
					break;

				case LINE_HEIGHT_DIMENSION:
					offset += 2; /* length + units */
					break;
				}
				break;

			case CSS_PROP_MAX_HEIGHT:
			case CSS_PROP_MAX_WIDTH:
				assert(MAX_HEIGHT_SET == 
				       (enum op_max_height)MAX_WIDTH_SET);

				if (value == MAX_HEIGHT_SET)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_PADDING_TOP:
			case CSS_PROP_PADDING_RIGHT:
			case CSS_PROP_PADDING_BOTTOM:
			case CSS_PROP_PADDING_LEFT:
			case CSS_PROP_MIN_HEIGHT:
			case CSS_PROP_MIN_WIDTH:
			case CSS_PROP_PAUSE_AFTER:
			case CSS_PROP_PAUSE_BEFORE:
			case CSS_PROP_TEXT_INDENT:
				assert(MIN_HEIGHT_SET == (enum op_min_height)MIN_WIDTH_SET);
				assert(MIN_HEIGHT_SET == (enum op_min_height)PADDING_SET);
				assert(MIN_HEIGHT_SET == (enum op_min_height)PAUSE_AFTER_SET);
				assert(MIN_HEIGHT_SET == (enum op_min_height)PAUSE_BEFORE_SET);
				assert(MIN_HEIGHT_SET == (enum op_min_height)TEXT_INDENT_SET);

				if (value == MIN_HEIGHT_SET)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_OPACITY:
				if (value == OPACITY_SET)
					offset++; /* value */
				break;

			case CSS_PROP_ORPHANS:
			case CSS_PROP_PITCH_RANGE:
			case CSS_PROP_RICHNESS:
			case CSS_PROP_STRESS:
			case CSS_PROP_WIDOWS:
				assert(ORPHANS_SET == (enum op_orphans)PITCH_RANGE_SET);
				assert(ORPHANS_SET == (enum op_orphans)RICHNESS_SET);
				assert(ORPHANS_SET == (enum op_orphans)STRESS_SET);
				assert(ORPHANS_SET == (enum op_orphans)WIDOWS_SET);

				if (value == ORPHANS_SET)
					offset++; /* value */
				break;

			case CSS_PROP_OUTLINE_COLOR:
				if (value == OUTLINE_COLOR_SET)
					offset++; /* color */
				break;

			case CSS_PROP_PITCH:
				if (value == PITCH_FREQUENCY)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_PLAY_DURING:
				if (value == PLAY_DURING_URI)
					offset++; /* string table entry */
				break;

			case CSS_PROP_QUOTES:
				while (value != QUOTES_NONE) {
					offset += 2; /* two string table entries */

					value = bytecode[offset];
				        offset++;
				}
				break;

			case CSS_PROP_SPEECH_RATE:
				if (value == SPEECH_RATE_SET) 
					offset++; /* rate */
				break;

			case CSS_PROP_VERTICAL_ALIGN:
				if (value == VERTICAL_ALIGN_SET)
					offset += 2; /* length + units */
				break;

			case CSS_PROP_VOICE_FAMILY:
				while (value != VOICE_FAMILY_END) {
					switch (value) {
					case VOICE_FAMILY_STRING:
					case VOICE_FAMILY_IDENT_LIST:
						offset++; /* string table entry */
						break;
					}

					value = bytecode[offset];
				        offset++;
				}
				break;

			case CSS_PROP_VOLUME:
				switch (value) {
				case VOLUME_NUMBER:
					offset++; /* value */
					break;

				case VOLUME_DIMENSION:
					offset += 2; /* value + units */
					break;
				}
				break;

			case CSS_PROP_Z_INDEX:
				if (value == Z_INDEX_SET)
					offset++; /* z index */
				break;

			default:
				break;
			}
		}
	}

}

