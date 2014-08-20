/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_select_propget_h_
#define css_select_propget_h_

#include <libcss/computed.h>
#include "computed.h"

/* Important: keep this file in sync with computed.h */
/** \todo Is there a better way to ensure this happens? */

#define LETTER_SPACING_INDEX 0
#define LETTER_SPACING_SHIFT 2
#define LETTER_SPACING_MASK  0xfc
static inline uint8_t get_letter_spacing(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[LETTER_SPACING_INDEX];
		bits &= LETTER_SPACING_MASK;
		bits >>= LETTER_SPACING_SHIFT;

		/* 6bits: uuuutt : unit | type */

		if ((bits & 3) == CSS_LETTER_SPACING_SET) {
			*length = style->uncommon->letter_spacing;
			*unit = bits >> 2;
		}

		return (bits & 3);
	}

	return CSS_LETTER_SPACING_NORMAL;
}
#undef LETTER_SPACING_MASK
#undef LETTER_SPACING_SHIFT
#undef LETTER_SPACING_INDEX

#define OUTLINE_COLOR_INDEX 0
#define OUTLINE_COLOR_SHIFT 0
#define OUTLINE_COLOR_MASK  0x3
static inline uint8_t get_outline_color(
		const css_computed_style *style, css_color *color)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[OUTLINE_COLOR_INDEX];
		bits &= OUTLINE_COLOR_MASK;
		bits >>= OUTLINE_COLOR_SHIFT;

		/* 2bits: tt : type */

		if ((bits & 3) == CSS_OUTLINE_COLOR_COLOR) {
			*color = style->uncommon->outline_color;
		}

		return (bits & 3);
	}

	return CSS_OUTLINE_COLOR_INVERT;
}
#undef OUTLINE_COLOR_MASK
#undef OUTLINE_COLOR_SHIFT
#undef OUTLINE_COLOR_INDEX

#define OUTLINE_WIDTH_INDEX 1
#define OUTLINE_WIDTH_SHIFT 1
#define OUTLINE_WIDTH_MASK  0xfe
static inline uint8_t get_outline_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[OUTLINE_WIDTH_INDEX];
		bits &= OUTLINE_WIDTH_MASK;
		bits >>= OUTLINE_WIDTH_SHIFT;

		/* 7bits: uuuuttt : unit | type */

		if ((bits & 7) == CSS_OUTLINE_WIDTH_WIDTH) {
			*length = style->uncommon->outline_width;
			*unit = bits >> 3;
		}

		return (bits & 7);
	}

	*length = INTTOFIX(2);
	*unit = CSS_UNIT_PX;

	return CSS_OUTLINE_WIDTH_WIDTH;
}
#undef OUTLINE_WIDTH_MASK
#undef OUTLINE_WIDTH_SHIFT
#undef OUTLINE_WIDTH_INDEX

#define BORDER_SPACING_INDEX 1
#define BORDER_SPACING_SHIFT 0
#define BORDER_SPACING_MASK  0x1
#define BORDER_SPACING_INDEX1 2
#define BORDER_SPACING_SHIFT1 0
#define BORDER_SPACING_MASK1 0xff
static inline uint8_t get_border_spacing(
		const css_computed_style *style, 
		css_fixed *hlength, css_unit *hunit,
		css_fixed *vlength, css_unit *vunit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[BORDER_SPACING_INDEX];
		bits &= BORDER_SPACING_MASK;
		bits >>= BORDER_SPACING_SHIFT;

		/* 1 bit: type */
		if (bits == CSS_BORDER_SPACING_SET) {
			uint8_t bits1 = 
				style->uncommon->bits[BORDER_SPACING_INDEX1];
			bits1 &= BORDER_SPACING_MASK1;
			bits1 >>= BORDER_SPACING_SHIFT1;

			/* 8bits: hhhhvvvv : hunit | vunit */

			*hlength = style->uncommon->border_spacing[0];
			*hunit = bits1 >> 4;

			*vlength = style->uncommon->border_spacing[1];
			*vunit = bits1 & 0xf;
		}

		return bits;
	} else {
		*hlength = *vlength = 0;
		*hunit = *vunit = CSS_UNIT_PX;
	}

	return CSS_BORDER_SPACING_SET;
}
#undef BORDER_SPACING_MASK1
#undef BORDER_SPACING_SHIFT1
#undef BORDER_SPACING_INDEX1
#undef BORDER_SPACING_MASK
#undef BORDER_SPACING_SHIFT
#undef BORDER_SPACING_INDEX

#define WORD_SPACING_INDEX 3
#define WORD_SPACING_SHIFT 2
#define WORD_SPACING_MASK  0xfc
static inline uint8_t get_word_spacing(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[WORD_SPACING_INDEX];
		bits &= WORD_SPACING_MASK;
		bits >>= WORD_SPACING_SHIFT;

		/* 6bits: uuuutt : unit | type */

		if ((bits & 3) == CSS_WORD_SPACING_SET) {
			*length = style->uncommon->word_spacing;
			*unit = bits >> 2;
		}

		return (bits & 3);
	}

	return CSS_WORD_SPACING_NORMAL;
}
#undef WORD_SPACING_MASK
#undef WORD_SPACING_SHIFT
#undef WORD_SPACING_INDEX

#define WRITING_MODE_INDEX 4
#define WRITING_MODE_MASK  0x6
#define WRITING_MODE_SHIFT 1
static inline uint8_t get_writing_mode(
		const css_computed_style *style)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[WRITING_MODE_INDEX];
		bits &= WRITING_MODE_MASK;
		bits >>= WRITING_MODE_SHIFT;

		/* 2bits: type */
		return bits;
	}

	return CSS_WRITING_MODE_HORIZONTAL_TB;
}
#undef WRITING_MODE_INDEX
#undef WRITING_MODE_MASK
#undef WRITING_MODE_SHIFT

#define COUNTER_INCREMENT_INDEX 3
#define COUNTER_INCREMENT_SHIFT 1
#define COUNTER_INCREMENT_MASK  0x2
static inline uint8_t get_counter_increment(
		const css_computed_style *style, 
		const css_computed_counter **counters)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[COUNTER_INCREMENT_INDEX];
		bits &= COUNTER_INCREMENT_MASK;
		bits >>= COUNTER_INCREMENT_SHIFT;

		/* 1bit: type */
		*counters = style->uncommon->counter_increment;

		return bits;
	}

	return CSS_COUNTER_INCREMENT_NONE;
}
#undef COUNTER_INCREMENT_MASK
#undef COUNTER_INCREMENT_SHIFT
#undef COUNTER_INCREMENT_INDEX

#define COUNTER_RESET_INDEX 3
#define COUNTER_RESET_SHIFT 0
#define COUNTER_RESET_MASK  0x1
static inline uint8_t get_counter_reset(
		const css_computed_style *style, 
		const css_computed_counter **counters)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[COUNTER_RESET_INDEX];
		bits &= COUNTER_RESET_MASK;
		bits >>= COUNTER_RESET_SHIFT;

		/* 1bit: type */
		*counters = style->uncommon->counter_reset;

		return bits;
	}

	return CSS_COUNTER_RESET_NONE;
}
#undef COUNTER_RESET_MASK
#undef COUNTER_RESET_SHIFT
#undef COUNTER_RESET_INDEX

#define CURSOR_INDEX 4
#define CURSOR_SHIFT 3
#define CURSOR_MASK  0xf8
static inline uint8_t get_cursor(
		const css_computed_style *style, 
		lwc_string ***urls)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[CURSOR_INDEX];
		bits &= CURSOR_MASK;
		bits >>= CURSOR_SHIFT;

		/* 5bits: type */
		*urls = style->uncommon->cursor;

		return bits;
	}

	return CSS_CURSOR_AUTO;
}
#undef CURSOR_MASK
#undef CURSOR_SHIFT
#undef CURSOR_INDEX

#define CLIP_INDEX 7
#define CLIP_SHIFT 2
#define CLIP_MASK  0xfc
#define CLIP_INDEX1 5
#define CLIP_SHIFT1 0
#define CLIP_MASK1 0xff
#define CLIP_INDEX2 6
#define CLIP_SHIFT2 0
#define CLIP_MASK2 0xff
static inline uint8_t get_clip(
		const css_computed_style *style, 
		css_computed_clip_rect *rect)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[CLIP_INDEX];
		bits &= CLIP_MASK;
		bits >>= CLIP_SHIFT;

		/* 6bits: trblyy : top | right | bottom | left | type */
		if ((bits & 0x3) == CSS_CLIP_RECT) {
			uint8_t bits1; 

			rect->left_auto = (bits & 0x4);
			rect->bottom_auto = (bits & 0x8);
			rect->right_auto = (bits & 0x10);
			rect->top_auto = (bits & 0x20);

			if (rect->top_auto == false ||
					rect->right_auto == false) {
				/* 8bits: ttttrrrr : top | right */
				bits1 = style->uncommon->bits[CLIP_INDEX1];
				bits1 &= CLIP_MASK1;
				bits1 >>= CLIP_SHIFT1;
			} else {
				bits1 = 0;
			}

			rect->top = style->uncommon->clip[0];
			rect->tunit = bits1 >> 4;

			rect->right = style->uncommon->clip[1];
			rect->runit = bits1 & 0xf;

			if (rect->bottom_auto == false ||
					rect->left_auto == false) {
				/* 8bits: bbbbllll : bottom | left */
				bits1 = style->uncommon->bits[CLIP_INDEX2];
				bits1 &= CLIP_MASK2;
				bits1 >>= CLIP_SHIFT2;
			} else {
				bits1 = 0;
			}

			rect->bottom = style->uncommon->clip[2];
			rect->bunit = bits1 >> 4;

			rect->left = style->uncommon->clip[3];
			rect->lunit = bits1 & 0xf;
		}

		return (bits & 0x3);
	}

	return CSS_CLIP_AUTO;
}
#undef CLIP_MASK2
#undef CLIP_SHIFT2
#undef CLIP_INDEX2
#undef CLIP_MASK1
#undef CLIP_SHIFT1
#undef CLIP_INDEX1
#undef CLIP_MASK
#undef CLIP_SHIFT
#undef CLIP_INDEX

#define CONTENT_INDEX 7
#define CONTENT_SHIFT 0
#define CONTENT_MASK  0x3
static inline uint8_t get_content(
		const css_computed_style *style, 
		const css_computed_content_item **content)
{
	if (style->uncommon != NULL) {
		uint8_t bits = style->uncommon->bits[CONTENT_INDEX];
		bits &= CONTENT_MASK;
		bits >>= CONTENT_SHIFT;

		/* 2bits: type */
		*content = style->uncommon->content;

		return bits;
	}

	return CSS_CONTENT_NORMAL;
}
#undef CONTENT_MASK
#undef CONTENT_SHIFT
#undef CONTENT_INDEX

#define VERTICAL_ALIGN_INDEX 0
#define VERTICAL_ALIGN_SHIFT 0
#define VERTICAL_ALIGN_MASK  0xff
static inline uint8_t get_vertical_align(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[VERTICAL_ALIGN_INDEX];
	bits &= VERTICAL_ALIGN_MASK;
	bits >>= VERTICAL_ALIGN_SHIFT;

	/* 8bits: uuuutttt : units | type */
	if ((bits & 0xf) == CSS_VERTICAL_ALIGN_SET) {
		*length = style->vertical_align;
		*unit = bits >> 4;
	}

	return (bits & 0xf);
}
#undef VERTICAL_ALIGN_MASK
#undef VERTICAL_ALIGN_SHIFT
#undef VERTICAL_ALIGN_INDEX

#define FONT_SIZE_INDEX 1
#define FONT_SIZE_SHIFT 0
#define FONT_SIZE_MASK  0xff
static inline uint8_t get_font_size(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[FONT_SIZE_INDEX];
	bits &= FONT_SIZE_MASK;
	bits >>= FONT_SIZE_SHIFT;

	/* 8bits: uuuutttt : units | type */
	if ((bits & 0xf) == CSS_FONT_SIZE_DIMENSION) {
		*length = style->font_size;
		*unit = bits >> 4;
	}

	return (bits & 0xf);
}
#undef FONT_SIZE_MASK
#undef FONT_SIZE_SHIFT
#undef FONT_SIZE_INDEX

#define BORDER_TOP_WIDTH_INDEX 2
#define BORDER_TOP_WIDTH_SHIFT 1
#define BORDER_TOP_WIDTH_MASK  0xfe
static inline uint8_t get_border_top_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_TOP_WIDTH_INDEX];
	bits &= BORDER_TOP_WIDTH_MASK;
	bits >>= BORDER_TOP_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[0];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_TOP_WIDTH_MASK
#undef BORDER_TOP_WIDTH_SHIFT
#undef BORDER_TOP_WIDTH_INDEX

#define BORDER_RIGHT_WIDTH_INDEX 3
#define BORDER_RIGHT_WIDTH_SHIFT 1
#define BORDER_RIGHT_WIDTH_MASK  0xfe
static inline uint8_t get_border_right_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_RIGHT_WIDTH_INDEX];
	bits &= BORDER_RIGHT_WIDTH_MASK;
	bits >>= BORDER_RIGHT_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[1];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_RIGHT_WIDTH_MASK
#undef BORDER_RIGHT_WIDTH_SHIFT
#undef BORDER_RIGHT_WIDTH_INDEX

#define BORDER_BOTTOM_WIDTH_INDEX 4
#define BORDER_BOTTOM_WIDTH_SHIFT 1
#define BORDER_BOTTOM_WIDTH_MASK  0xfe
static inline uint8_t get_border_bottom_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_BOTTOM_WIDTH_INDEX];
	bits &= BORDER_BOTTOM_WIDTH_MASK;
	bits >>= BORDER_BOTTOM_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[2];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_BOTTOM_WIDTH_MASK
#undef BORDER_BOTTOM_WIDTH_SHIFT
#undef BORDER_BOTTOM_WIDTH_INDEX

#define BORDER_LEFT_WIDTH_INDEX 5
#define BORDER_LEFT_WIDTH_SHIFT 1
#define BORDER_LEFT_WIDTH_MASK  0xfe
static inline uint8_t get_border_left_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BORDER_LEFT_WIDTH_INDEX];
	bits &= BORDER_LEFT_WIDTH_MASK;
	bits >>= BORDER_LEFT_WIDTH_SHIFT;

	/* 7bits: uuuuttt : units | type */
	if ((bits & 0x7) == CSS_BORDER_WIDTH_WIDTH) {
		*length = style->border_width[3];
		*unit = bits >> 3;
	}

	return (bits & 0x7);
}
#undef BORDER_LEFT_WIDTH_MASK
#undef BORDER_LEFT_WIDTH_SHIFT
#undef BORDER_LEFT_WIDTH_INDEX

#define BACKGROUND_IMAGE_INDEX 2
#define BACKGROUND_IMAGE_SHIFT 0
#define BACKGROUND_IMAGE_MASK  0x1
static inline uint8_t get_background_image(
		const css_computed_style *style, 
		lwc_string **url)
{
	uint8_t bits = style->bits[BACKGROUND_IMAGE_INDEX];
	bits &= BACKGROUND_IMAGE_MASK;
	bits >>= BACKGROUND_IMAGE_SHIFT;

	/* 1bit: type */
	*url = style->background_image;

	return bits;
}
#undef BACKGROUND_IMAGE_MASK
#undef BACKGROUND_IMAGE_SHIFT
#undef BACKGROUND_IMAGE_INDEX

#define COLOR_INDEX 3
#define COLOR_SHIFT 0
#define COLOR_MASK  0x1
static inline uint8_t get_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[COLOR_INDEX];
	bits &= COLOR_MASK;
	bits >>= COLOR_SHIFT;

	/* 1bit: type */
	*color = style->color;

	return bits;
}
#undef COLOR_MASK
#undef COLOR_SHIFT
#undef COLOR_INDEX

#define LIST_STYLE_IMAGE_INDEX 4
#define LIST_STYLE_IMAGE_SHIFT 0
#define LIST_STYLE_IMAGE_MASK  0x1
static inline uint8_t get_list_style_image(
		const css_computed_style *style, 
		lwc_string **url)
{
	uint8_t bits = style->bits[LIST_STYLE_IMAGE_INDEX];
	bits &= LIST_STYLE_IMAGE_MASK;
	bits >>= LIST_STYLE_IMAGE_SHIFT;

	/* 1bit: type */
	*url = style->list_style_image;

	return bits;
}
#undef LIST_STYLE_IMAGE_MASK
#undef LIST_STYLE_IMAGE_SHIFT
#undef LIST_STYLE_IMAGE_INDEX

#define QUOTES_INDEX 5
#define QUOTES_SHIFT 0
#define QUOTES_MASK  0x1
static inline uint8_t get_quotes(
		const css_computed_style *style, 
		lwc_string ***quotes)
{
	uint8_t bits = style->bits[QUOTES_INDEX];
	bits &= QUOTES_MASK;
	bits >>= QUOTES_SHIFT;

	/* 1bit: type */
	*quotes = style->quotes;

	return bits;
}
#undef QUOTES_MASK
#undef QUOTES_SHIFT
#undef QUOTES_INDEX

#define TOP_INDEX 6
#define TOP_SHIFT 2
#define TOP_MASK  0xfc
static inline uint8_t get_top(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[TOP_INDEX];
	bits &= TOP_MASK;
	bits >>= TOP_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_TOP_SET) {
		*length = style->top;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
static inline uint8_t get_top_bits(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TOP_INDEX];
	bits &= TOP_MASK;
	bits >>= TOP_SHIFT;

	/* 6bits: uuuutt : units | type */
	return bits;
}
#undef TOP_MASK
#undef TOP_SHIFT
#undef TOP_INDEX

#define RIGHT_INDEX 7
#define RIGHT_SHIFT 2
#define RIGHT_MASK  0xfc
static inline uint8_t get_right(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[RIGHT_INDEX];
	bits &= RIGHT_MASK;
	bits >>= RIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_RIGHT_SET) {
		*length = style->right;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
static inline uint8_t get_right_bits(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[RIGHT_INDEX];
	bits &= RIGHT_MASK;
	bits >>= RIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	return bits;
}
#undef RIGHT_MASK
#undef RIGHT_SHIFT
#undef RIGHT_INDEX

#define BOTTOM_INDEX 8
#define BOTTOM_SHIFT 2
#define BOTTOM_MASK  0xfc
static inline uint8_t get_bottom(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[BOTTOM_INDEX];
	bits &= BOTTOM_MASK;
	bits >>= BOTTOM_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_BOTTOM_SET) {
		*length = style->bottom;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
static inline uint8_t get_bottom_bits(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BOTTOM_INDEX];
	bits &= BOTTOM_MASK;
	bits >>= BOTTOM_SHIFT;

	/* 6bits: uuuutt : units | type */
	return bits;
}
#undef BOTTOM_MASK
#undef BOTTOM_SHIFT
#undef BOTTOM_INDEX

#define LEFT_INDEX 9
#define LEFT_SHIFT 2
#define LEFT_MASK  0xfc
static inline uint8_t get_left(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[LEFT_INDEX];
	bits &= LEFT_MASK;
	bits >>= LEFT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_LEFT_SET) {
		*length = style->left;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
static inline uint8_t get_left_bits(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[LEFT_INDEX];
	bits &= LEFT_MASK;
	bits >>= LEFT_SHIFT;

	/* 6bits: uuuutt : units | type */
	return bits;
}
#undef LEFT_MASK
#undef LEFT_SHIFT
#undef LEFT_INDEX

#define BORDER_TOP_COLOR_INDEX 6
#define BORDER_TOP_COLOR_SHIFT 0
#define BORDER_TOP_COLOR_MASK  0x3
static inline uint8_t get_border_top_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_TOP_COLOR_INDEX];
	bits &= BORDER_TOP_COLOR_MASK;
	bits >>= BORDER_TOP_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[0];

	return bits;
}
#undef BORDER_TOP_COLOR_MASK
#undef BORDER_TOP_COLOR_SHIFT
#undef BORDER_TOP_COLOR_INDEX

#define BORDER_RIGHT_COLOR_INDEX 7
#define BORDER_RIGHT_COLOR_SHIFT 0
#define BORDER_RIGHT_COLOR_MASK  0x3
static inline uint8_t get_border_right_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_RIGHT_COLOR_INDEX];
	bits &= BORDER_RIGHT_COLOR_MASK;
	bits >>= BORDER_RIGHT_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[1];

	return bits;
}
#undef BORDER_RIGHT_COLOR_MASK
#undef BORDER_RIGHT_COLOR_SHIFT
#undef BORDER_RIGHT_COLOR_INDEX

#define BORDER_BOTTOM_COLOR_INDEX 8
#define BORDER_BOTTOM_COLOR_SHIFT 0
#define BORDER_BOTTOM_COLOR_MASK  0x3
static inline uint8_t get_border_bottom_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_BOTTOM_COLOR_INDEX];
	bits &= BORDER_BOTTOM_COLOR_MASK;
	bits >>= BORDER_BOTTOM_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[2];

	return bits;
}
#undef BORDER_BOTTOM_COLOR_MASK
#undef BORDER_BOTTOM_COLOR_SHIFT
#undef BORDER_BOTTOM_COLOR_INDEX

#define BORDER_LEFT_COLOR_INDEX 9
#define BORDER_LEFT_COLOR_SHIFT 0
#define BORDER_LEFT_COLOR_MASK  0x3
static inline uint8_t get_border_left_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BORDER_LEFT_COLOR_INDEX];
	bits &= BORDER_LEFT_COLOR_MASK;
	bits >>= BORDER_LEFT_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->border_color[3];

	return bits;
}
#undef BORDER_LEFT_COLOR_MASK
#undef BORDER_LEFT_COLOR_SHIFT
#undef BORDER_LEFT_COLOR_INDEX

#define HEIGHT_INDEX 10
#define HEIGHT_SHIFT 2
#define HEIGHT_MASK  0xfc
static inline uint8_t get_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[HEIGHT_INDEX];
	bits &= HEIGHT_MASK;
	bits >>= HEIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_HEIGHT_SET) {
		*length = style->height;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef HEIGHT_MASK
#undef HEIGHT_SHIFT
#undef HEIGHT_INDEX

#define LINE_HEIGHT_INDEX 11
#define LINE_HEIGHT_SHIFT 2
#define LINE_HEIGHT_MASK  0xfc
static inline uint8_t get_line_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[LINE_HEIGHT_INDEX];
	bits &= LINE_HEIGHT_MASK;
	bits >>= LINE_HEIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_LINE_HEIGHT_NUMBER || 
			(bits & 0x3) == CSS_LINE_HEIGHT_DIMENSION) {
		*length = style->line_height;
	}

	if ((bits & 0x3) == CSS_LINE_HEIGHT_DIMENSION) {
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef LINE_HEIGHT_MASK
#undef LINE_HEIGHT_SHIFT
#undef LINE_HEIGHT_INDEX

#define BACKGROUND_COLOR_INDEX 10
#define BACKGROUND_COLOR_SHIFT 0
#define BACKGROUND_COLOR_MASK  0x3
static inline uint8_t get_background_color(
		const css_computed_style *style, 
		css_color *color)
{
	uint8_t bits = style->bits[BACKGROUND_COLOR_INDEX];
	bits &= BACKGROUND_COLOR_MASK;
	bits >>= BACKGROUND_COLOR_SHIFT;

	/* 2bits: type */
	*color = style->background_color;

	return bits;
}
#undef BACKGROUND_COLOR_MASK
#undef BACKGROUND_COLOR_SHIFT
#undef BACKGROUND_COLOR_INDEX

#define Z_INDEX_INDEX 11
#define Z_INDEX_SHIFT 0
#define Z_INDEX_MASK  0x3
static inline uint8_t get_z_index(
		const css_computed_style *style, 
		int32_t *z_index)
{
	uint8_t bits = style->bits[Z_INDEX_INDEX];
	bits &= Z_INDEX_MASK;
	bits >>= Z_INDEX_SHIFT;

	/* 2bits: type */
	*z_index = style->z_index;

	return bits;
}
#undef Z_INDEX_MASK
#undef Z_INDEX_SHIFT
#undef Z_INDEX_INDEX

#define MARGIN_TOP_INDEX 12
#define MARGIN_TOP_SHIFT 2
#define MARGIN_TOP_MASK  0xfc
static inline uint8_t get_margin_top(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_TOP_INDEX];
	bits &= MARGIN_TOP_MASK;
	bits >>= MARGIN_TOP_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[0];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_TOP_MASK
#undef MARGIN_TOP_SHIFT
#undef MARGIN_TOP_INDEX

#define MARGIN_RIGHT_INDEX 13
#define MARGIN_RIGHT_SHIFT 2
#define MARGIN_RIGHT_MASK  0xfc
static inline uint8_t get_margin_right(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_RIGHT_INDEX];
	bits &= MARGIN_RIGHT_MASK;
	bits >>= MARGIN_RIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[1];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_RIGHT_MASK
#undef MARGIN_RIGHT_SHIFT
#undef MARGIN_RIGHT_INDEX

#define MARGIN_BOTTOM_INDEX 14
#define MARGIN_BOTTOM_SHIFT 2
#define MARGIN_BOTTOM_MASK  0xfc
static inline uint8_t get_margin_bottom(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_BOTTOM_INDEX];
	bits &= MARGIN_BOTTOM_MASK;
	bits >>= MARGIN_BOTTOM_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[2];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_BOTTOM_MASK
#undef MARGIN_BOTTOM_SHIFT
#undef MARGIN_BOTTOM_INDEX

#define MARGIN_LEFT_INDEX 15
#define MARGIN_LEFT_SHIFT 2
#define MARGIN_LEFT_MASK  0xfc
static inline uint8_t get_margin_left(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MARGIN_LEFT_INDEX];
	bits &= MARGIN_LEFT_MASK;
	bits >>= MARGIN_LEFT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MARGIN_SET) {
		*length = style->margin[3];
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MARGIN_LEFT_MASK
#undef MARGIN_LEFT_SHIFT
#undef MARGIN_LEFT_INDEX

#define BACKGROUND_ATTACHMENT_INDEX 12
#define BACKGROUND_ATTACHMENT_SHIFT 0
#define BACKGROUND_ATTACHMENT_MASK  0x3
static inline uint8_t get_background_attachment(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BACKGROUND_ATTACHMENT_INDEX];
	bits &= BACKGROUND_ATTACHMENT_MASK;
	bits >>= BACKGROUND_ATTACHMENT_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef BACKGROUND_ATTACHMENT_MASK
#undef BACKGROUND_ATTACHMENT_SHIFT
#undef BACKGROUND_ATTACHMENT_INDEX

#define BORDER_COLLAPSE_INDEX 13
#define BORDER_COLLAPSE_SHIFT 0
#define BORDER_COLLAPSE_MASK  0x3
static inline uint8_t get_border_collapse(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BORDER_COLLAPSE_INDEX];
	bits &= BORDER_COLLAPSE_MASK;
	bits >>= BORDER_COLLAPSE_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef BORDER_COLLAPSE_MASK
#undef BORDER_COLLAPSE_SHIFT
#undef BORDER_COLLAPSE_INDEX

#define CAPTION_SIDE_INDEX 14
#define CAPTION_SIDE_SHIFT 0
#define CAPTION_SIDE_MASK  0x3
static inline uint8_t get_caption_side(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[CAPTION_SIDE_INDEX];
	bits &= CAPTION_SIDE_MASK;
	bits >>= CAPTION_SIDE_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef CAPTION_SIDE_MASK
#undef CAPTION_SIDE_SHIFT
#undef CAPTION_SIDE_INDEX

#define DIRECTION_INDEX 15
#define DIRECTION_SHIFT 0
#define DIRECTION_MASK  0x3
static inline uint8_t get_direction(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[DIRECTION_INDEX];
	bits &= DIRECTION_MASK;
	bits >>= DIRECTION_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef DIRECTION_MASK
#undef DIRECTION_SHIFT
#undef DIRECTION_INDEX

#define MAX_HEIGHT_INDEX 16
#define MAX_HEIGHT_SHIFT 2
#define MAX_HEIGHT_MASK  0xfc
static inline uint8_t get_max_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MAX_HEIGHT_INDEX];
	bits &= MAX_HEIGHT_MASK;
	bits >>= MAX_HEIGHT_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MAX_HEIGHT_SET) {
		*length = style->max_height;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MAX_HEIGHT_MASK
#undef MAX_HEIGHT_SHIFT
#undef MAX_HEIGHT_INDEX

#define MAX_WIDTH_INDEX 17
#define MAX_WIDTH_SHIFT 2
#define MAX_WIDTH_MASK  0xfc
static inline uint8_t get_max_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MAX_WIDTH_INDEX];
	bits &= MAX_WIDTH_MASK;
	bits >>= MAX_WIDTH_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_MAX_WIDTH_SET) {
		*length = style->max_width;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef MAX_WIDTH_MASK
#undef MAX_WIDTH_SHIFT
#undef MAX_WIDTH_INDEX

#define WIDTH_INDEX 18
#define WIDTH_SHIFT 2
#define WIDTH_MASK  0xfc
static inline uint8_t get_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[WIDTH_INDEX];
	bits &= WIDTH_MASK;
	bits >>= WIDTH_SHIFT;

	/* 6bits: uuuutt : units | type */
	if ((bits & 0x3) == CSS_WIDTH_SET) {
		*length = style->width;
		*unit = bits >> 2;
	}

	return (bits & 0x3);
}
#undef WIDTH_MASK
#undef WIDTH_SHIFT
#undef WIDTH_INDEX

#define EMPTY_CELLS_INDEX 16
#define EMPTY_CELLS_SHIFT 0
#define EMPTY_CELLS_MASK  0x3
static inline uint8_t get_empty_cells(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[EMPTY_CELLS_INDEX];
	bits &= EMPTY_CELLS_MASK;
	bits >>= EMPTY_CELLS_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef EMPTY_CELLS_MASK
#undef EMPTY_CELLS_SHIFT
#undef EMPTY_CELLS_INDEX

#define FLOAT_INDEX 17
#define FLOAT_SHIFT 0
#define FLOAT_MASK  0x3
static inline uint8_t get_float(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[FLOAT_INDEX];
	bits &= FLOAT_MASK;
	bits >>= FLOAT_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef FLOAT_MASK
#undef FLOAT_SHIFT
#undef FLOAT_INDEX

#define FONT_STYLE_INDEX 18
#define FONT_STYLE_SHIFT 0
#define FONT_STYLE_MASK  0x3
static inline uint8_t get_font_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[FONT_STYLE_INDEX];
	bits &= FONT_STYLE_MASK;
	bits >>= FONT_STYLE_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef FONT_STYLE_MASK
#undef FONT_STYLE_SHIFT
#undef FONT_STYLE_INDEX

#define MIN_HEIGHT_INDEX 19
#define MIN_HEIGHT_SHIFT 3
#define MIN_HEIGHT_MASK  0xf8
static inline uint8_t get_min_height(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MIN_HEIGHT_INDEX];
	bits &= MIN_HEIGHT_MASK;
	bits >>= MIN_HEIGHT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_MIN_HEIGHT_SET) {
		*length = style->min_height;
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef MIN_HEIGHT_MASK
#undef MIN_HEIGHT_SHIFT
#undef MIN_HEIGHT_INDEX

#define MIN_WIDTH_INDEX 20
#define MIN_WIDTH_SHIFT 3
#define MIN_WIDTH_MASK  0xf8
static inline uint8_t get_min_width(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[MIN_WIDTH_INDEX];
	bits &= MIN_WIDTH_MASK;
	bits >>= MIN_WIDTH_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_MIN_WIDTH_SET) {
		*length = style->min_width;
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef MIN_WIDTH_MASK
#undef MIN_WIDTH_SHIFT
#undef MIN_WIDTH_INDEX

#define BACKGROUND_REPEAT_INDEX 19
#define BACKGROUND_REPEAT_SHIFT 0
#define BACKGROUND_REPEAT_MASK  0x7
static inline uint8_t get_background_repeat(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BACKGROUND_REPEAT_INDEX];
	bits &= BACKGROUND_REPEAT_MASK;
	bits >>= BACKGROUND_REPEAT_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef BACKGROUND_REPEAT_MASK
#undef BACKGROUND_REPEAT_SHIFT
#undef BACKGROUND_REPEAT_INDEX

#define CLEAR_INDEX 20
#define CLEAR_SHIFT 0
#define CLEAR_MASK  0x7
static inline uint8_t get_clear(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[CLEAR_INDEX];
	bits &= CLEAR_MASK;
	bits >>= CLEAR_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef CLEAR_MASK
#undef CLEAR_SHIFT
#undef CLEAR_INDEX

#define PADDING_TOP_INDEX 21
#define PADDING_TOP_SHIFT 3
#define PADDING_TOP_MASK  0xf8
static inline uint8_t get_padding_top(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_TOP_INDEX];
	bits &= PADDING_TOP_MASK;
	bits >>= PADDING_TOP_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[0];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_TOP_MASK
#undef PADDING_TOP_SHIFT
#undef PADDING_TOP_INDEX

#define PADDING_RIGHT_INDEX 22
#define PADDING_RIGHT_SHIFT 3
#define PADDING_RIGHT_MASK  0xf8
static inline uint8_t get_padding_right(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_RIGHT_INDEX];
	bits &= PADDING_RIGHT_MASK;
	bits >>= PADDING_RIGHT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[1];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_RIGHT_MASK
#undef PADDING_RIGHT_SHIFT
#undef PADDING_RIGHT_INDEX

#define PADDING_BOTTOM_INDEX 23
#define PADDING_BOTTOM_SHIFT 3
#define PADDING_BOTTOM_MASK  0xf8
static inline uint8_t get_padding_bottom(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_BOTTOM_INDEX];
	bits &= PADDING_BOTTOM_MASK;
	bits >>= PADDING_BOTTOM_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[2];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_BOTTOM_MASK
#undef PADDING_BOTTOM_SHIFT
#undef PADDING_BOTTOM_INDEX

#define PADDING_LEFT_INDEX 24
#define PADDING_LEFT_SHIFT 3
#define PADDING_LEFT_MASK  0xf8
static inline uint8_t get_padding_left(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[PADDING_LEFT_INDEX];
	bits &= PADDING_LEFT_MASK;
	bits >>= PADDING_LEFT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_PADDING_SET) {
		*length = style->padding[3];
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef PADDING_LEFT_MASK
#undef PADDING_LEFT_SHIFT
#undef PADDING_LEFT_INDEX

#define OVERFLOW_INDEX 21
#define OVERFLOW_SHIFT 0
#define OVERFLOW_MASK  0x7
static inline uint8_t get_overflow(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[OVERFLOW_INDEX];
	bits &= OVERFLOW_MASK;
	bits >>= OVERFLOW_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef OVERFLOW_MASK
#undef OVERFLOW_SHIFT
#undef OVERFLOW_INDEX

#define POSITION_INDEX 22
#define POSITION_SHIFT 0
#define POSITION_MASK  0x7
static inline uint8_t get_position(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[POSITION_INDEX];
	bits &= POSITION_MASK;
	bits >>= POSITION_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef POSITION_MASK
#undef POSITION_SHIFT
#undef POSITION_INDEX

#define OPACITY_INDEX 23
#define OPACITY_SHIFT 2
#define OPACITY_MASK  0x04
static inline uint8_t get_opacity(
		const css_computed_style *style, 
		css_fixed *opacity)
{
	uint8_t bits = style->bits[OPACITY_INDEX];
	bits &= OPACITY_MASK;
	bits >>= OPACITY_SHIFT;

	/* 1bit: t : type */
	if ((bits & 0x1) == CSS_OPACITY_SET) {
		*opacity = style->opacity;
	}

	return (bits & 0x1);
}
#undef OPACITY_MASK
#undef OPACITY_SHIFT
#undef OPACITY_INDEX

#define TEXT_TRANSFORM_INDEX 24
#define TEXT_TRANSFORM_SHIFT 0
#define TEXT_TRANSFORM_MASK  0x7
static inline uint8_t get_text_transform(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TEXT_TRANSFORM_INDEX];
	bits &= TEXT_TRANSFORM_MASK;
	bits >>= TEXT_TRANSFORM_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef TEXT_TRANSFORM_MASK
#undef TEXT_TRANSFORM_SHIFT
#undef TEXT_TRANSFORM_INDEX

#define TEXT_INDENT_INDEX 25
#define TEXT_INDENT_SHIFT 3
#define TEXT_INDENT_MASK  0xf8
static inline uint8_t get_text_indent(
		const css_computed_style *style, 
		css_fixed *length, css_unit *unit)
{
	uint8_t bits = style->bits[TEXT_INDENT_INDEX];
	bits &= TEXT_INDENT_MASK;
	bits >>= TEXT_INDENT_SHIFT;

	/* 5bits: uuuut : units | type */
	if ((bits & 0x1) == CSS_TEXT_INDENT_SET) {
		*length = style->text_indent;
		*unit = bits >> 1;
	}

	return (bits & 0x1);
}
#undef TEXT_INDENT_MASK
#undef TEXT_INDENT_SHIFT
#undef TEXT_INDENT_INDEX

#define WHITE_SPACE_INDEX 25
#define WHITE_SPACE_SHIFT 0
#define WHITE_SPACE_MASK  0x7
static inline uint8_t get_white_space(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[WHITE_SPACE_INDEX];
	bits &= WHITE_SPACE_MASK;
	bits >>= WHITE_SPACE_SHIFT;

	/* 3bits: type */
	return bits;
}
#undef WHITE_SPACE_MASK
#undef WHITE_SPACE_SHIFT
#undef WHITE_SPACE_INDEX

#define BACKGROUND_POSITION_INDEX 27
#define BACKGROUND_POSITION_SHIFT 7
#define BACKGROUND_POSITION_MASK  0x80
#define BACKGROUND_POSITION_INDEX1 26
#define BACKGROUND_POSITION_SHIFT1 0
#define BACKGROUND_POSITION_MASK1 0xff
static inline uint8_t get_background_position(
		const css_computed_style *style, 
		css_fixed *hlength, css_unit *hunit,
		css_fixed *vlength, css_unit *vunit)
{
	uint8_t bits = style->bits[BACKGROUND_POSITION_INDEX];
	bits &= BACKGROUND_POSITION_MASK;
	bits >>= BACKGROUND_POSITION_SHIFT;

	/* 1bit: type */
	if (bits == CSS_BACKGROUND_POSITION_SET) {
		uint8_t bits1 = style->bits[BACKGROUND_POSITION_INDEX1];
		bits1 &= BACKGROUND_POSITION_MASK1;
		bits1 >>= BACKGROUND_POSITION_SHIFT1;

		/* 8bits: hhhhvvvv : hunit | vunit */
		*hlength = style->background_position[0];
		*hunit = bits1 >> 4;

		*vlength = style->background_position[1];
		*vunit = bits1 & 0xf;
	}

	return bits;
}
#undef BACKGROUND_POSITION_MASK1
#undef BACKGROUND_POSITION_SHIFT1
#undef BACKGROUND_POSITION_INDEX1
#undef BACKGROUND_POSITION_MASK
#undef BACKGROUND_POSITION_SHIFT
#undef BACKGROUND_POSITION_INDEX

#define DISPLAY_INDEX 27
#define DISPLAY_SHIFT 2
#define DISPLAY_MASK  0x7c
static inline uint8_t get_display(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[DISPLAY_INDEX];
	bits &= DISPLAY_MASK;
	bits >>= DISPLAY_SHIFT;

	/* 5bits: type */
	return bits;
}
#undef DISPLAY_MASK
#undef DISPLAY_SHIFT
#undef DISPLAY_INDEX

#define FONT_VARIANT_INDEX 27
#define FONT_VARIANT_SHIFT 0
#define FONT_VARIANT_MASK  0x3
static inline uint8_t get_font_variant(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[FONT_VARIANT_INDEX];
	bits &= FONT_VARIANT_MASK;
	bits >>= FONT_VARIANT_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef FONT_VARIANT_MASK
#undef FONT_VARIANT_SHIFT
#undef FONT_VARIANT_INDEX

#define TEXT_DECORATION_INDEX 28
#define TEXT_DECORATION_SHIFT 3
#define TEXT_DECORATION_MASK  0xf8
static inline uint8_t get_text_decoration(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TEXT_DECORATION_INDEX];
	bits &= TEXT_DECORATION_MASK;
	bits >>= TEXT_DECORATION_SHIFT;

	/* 5bits: type */
	return bits;
}
#undef TEXT_DECORATION_MASK
#undef TEXT_DECORATION_SHIFT
#undef TEXT_DECORATION_INDEX

#define FONT_FAMILY_INDEX 28
#define FONT_FAMILY_SHIFT 0
#define FONT_FAMILY_MASK  0x7
static inline uint8_t get_font_family(
		const css_computed_style *style, 
		lwc_string ***names)
{
	uint8_t bits = style->bits[FONT_FAMILY_INDEX];
	bits &= FONT_FAMILY_MASK;
	bits >>= FONT_FAMILY_SHIFT;

	/* 3bits: type */
	*names = style->font_family;

	return bits;
}
#undef FONT_FAMILY_MASK
#undef FONT_FAMILY_SHIFT
#undef FONT_FAMILY_INDEX

#define BORDER_TOP_STYLE_INDEX 29
#define BORDER_TOP_STYLE_SHIFT 4
#define BORDER_TOP_STYLE_MASK  0xf0
static inline uint8_t get_border_top_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BORDER_TOP_STYLE_INDEX];
	bits &= BORDER_TOP_STYLE_MASK;
	bits >>= BORDER_TOP_STYLE_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef BORDER_TOP_STYLE_MASK
#undef BORDER_TOP_STYLE_SHIFT
#undef BORDER_TOP_STYLE_INDEX

#define BORDER_RIGHT_STYLE_INDEX 29
#define BORDER_RIGHT_STYLE_SHIFT 0
#define BORDER_RIGHT_STYLE_MASK  0xf
static inline uint8_t get_border_right_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BORDER_RIGHT_STYLE_INDEX];
	bits &= BORDER_RIGHT_STYLE_MASK;
	bits >>= BORDER_RIGHT_STYLE_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef BORDER_RIGHT_STYLE_MASK
#undef BORDER_RIGHT_STYLE_SHIFT
#undef BORDER_RIGHT_STYLE_INDEX

#define BORDER_BOTTOM_STYLE_INDEX 30
#define BORDER_BOTTOM_STYLE_SHIFT 4
#define BORDER_BOTTOM_STYLE_MASK  0xf0
static inline uint8_t get_border_bottom_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BORDER_BOTTOM_STYLE_INDEX];
	bits &= BORDER_BOTTOM_STYLE_MASK;
	bits >>= BORDER_BOTTOM_STYLE_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef BORDER_BOTTOM_STYLE_MASK
#undef BORDER_BOTTOM_STYLE_SHIFT
#undef BORDER_BOTTOM_STYLE_INDEX

#define BORDER_LEFT_STYLE_INDEX 30
#define BORDER_LEFT_STYLE_SHIFT 0
#define BORDER_LEFT_STYLE_MASK  0xf
static inline uint8_t get_border_left_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[BORDER_LEFT_STYLE_INDEX];
	bits &= BORDER_LEFT_STYLE_MASK;
	bits >>= BORDER_LEFT_STYLE_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef BORDER_LEFT_STYLE_MASK
#undef BORDER_LEFT_STYLE_SHIFT
#undef BORDER_LEFT_STYLE_INDEX

#define FONT_WEIGHT_INDEX 31
#define FONT_WEIGHT_SHIFT 4
#define FONT_WEIGHT_MASK  0xf0
static inline uint8_t get_font_weight(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[FONT_WEIGHT_INDEX];
	bits &= FONT_WEIGHT_MASK;
	bits >>= FONT_WEIGHT_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef FONT_WEIGHT_MASK
#undef FONT_WEIGHT_SHIFT
#undef FONT_WEIGHT_INDEX

#define LIST_STYLE_TYPE_INDEX 31
#define LIST_STYLE_TYPE_SHIFT 0
#define LIST_STYLE_TYPE_MASK  0xf
static inline uint8_t get_list_style_type(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[LIST_STYLE_TYPE_INDEX];
	bits &= LIST_STYLE_TYPE_MASK;
	bits >>= LIST_STYLE_TYPE_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef LIST_STYLE_TYPE_MASK
#undef LIST_STYLE_TYPE_SHIFT
#undef LIST_STYLE_TYPE_INDEX

#define OUTLINE_STYLE_INDEX 32
#define OUTLINE_STYLE_SHIFT 4
#define OUTLINE_STYLE_MASK  0xf0
static inline uint8_t get_outline_style(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[OUTLINE_STYLE_INDEX];
	bits &= OUTLINE_STYLE_MASK;
	bits >>= OUTLINE_STYLE_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef OUTLINE_STYLE_MASK
#undef OUTLINE_STYLE_SHIFT
#undef OUTLINE_STYLE_INDEX

#define TABLE_LAYOUT_INDEX 32
#define TABLE_LAYOUT_SHIFT 2
#define TABLE_LAYOUT_MASK  0xc
static inline uint8_t get_table_layout(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TABLE_LAYOUT_INDEX];
	bits &= TABLE_LAYOUT_MASK;
	bits >>= TABLE_LAYOUT_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef TABLE_LAYOUT_MASK
#undef TABLE_LAYOUT_SHIFT
#undef TABLE_LAYOUT_INDEX

#define UNICODE_BIDI_INDEX 32
#define UNICODE_BIDI_SHIFT 0
#define UNICODE_BIDI_MASK  0x3
static inline uint8_t get_unicode_bidi(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[UNICODE_BIDI_INDEX];
	bits &= UNICODE_BIDI_MASK;
	bits >>= UNICODE_BIDI_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef UNICODE_BIDI_MASK
#undef UNICODE_BIDI_SHIFT
#undef UNICODE_BIDI_INDEX

#define VISIBILITY_INDEX 33
#define VISIBILITY_SHIFT 6
#define VISIBILITY_MASK  0xc0
static inline uint8_t get_visibility(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[VISIBILITY_INDEX];
	bits &= VISIBILITY_MASK;
	bits >>= VISIBILITY_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef VISIBILITY_MASK
#undef VISIBILITY_SHIFT
#undef VISIBILITY_INDEX

#define LIST_STYLE_POSITION_INDEX 33
#define LIST_STYLE_POSITION_SHIFT 4
#define LIST_STYLE_POSITION_MASK  0x30
static inline uint8_t get_list_style_position(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[LIST_STYLE_POSITION_INDEX];
	bits &= LIST_STYLE_POSITION_MASK;
	bits >>= LIST_STYLE_POSITION_SHIFT;

	/* 2bits: type */
	return bits;
}
#undef LIST_STYLE_POSITION_MASK
#undef LIST_STYLE_POSITION_SHIFT
#undef LIST_STYLE_POSITION_INDEX

#define TEXT_ALIGN_INDEX 33
#define TEXT_ALIGN_SHIFT 0
#define TEXT_ALIGN_MASK  0xf
static inline uint8_t get_text_align(
		const css_computed_style *style)
{
	uint8_t bits = style->bits[TEXT_ALIGN_INDEX];
	bits &= TEXT_ALIGN_MASK;
	bits >>= TEXT_ALIGN_SHIFT;

	/* 4bits: type */
	return bits;
}
#undef TEXT_ALIGN_MASK
#undef TEXT_ALIGN_SHIFT
#undef TEXT_ALIGN_INDEX

#define PAGE_BREAK_AFTER_INDEX 0
#define PAGE_BREAK_AFTER_SHIFT 0
#define PAGE_BREAK_AFTER_MASK 0x7
static inline uint8_t get_page_break_after(
		const css_computed_style *style)
{
	if (style->page != NULL) {
		uint8_t bits = style->page->bits[PAGE_BREAK_AFTER_INDEX];
		bits &= PAGE_BREAK_AFTER_MASK;
		bits >>= PAGE_BREAK_AFTER_SHIFT;

		/* 3bits: type */
		return bits;
	}

	return CSS_PAGE_BREAK_AFTER_AUTO;
}
#undef PAGE_BREAK_AFTER_MASK
#undef PAGE_BREAK_AFTER_SHIFT
#undef PAGE_BREAK_AFTER_INDEX
 
#define PAGE_BREAK_BEFORE_INDEX 0
#define PAGE_BREAK_BEFORE_SHIFT 3
#define PAGE_BREAK_BEFORE_MASK 0x38
static inline uint8_t get_page_break_before(
		const css_computed_style *style)
{
	if (style->page != NULL) {
		uint8_t bits = style->page->bits[PAGE_BREAK_BEFORE_INDEX];
		bits &= PAGE_BREAK_BEFORE_MASK;
		bits >>= PAGE_BREAK_BEFORE_SHIFT;

		/* 3bits: type */
		return bits;
	}
    
	return CSS_PAGE_BREAK_BEFORE_AUTO;
}
#undef PAGE_BREAK_BEFORE_MASK
#undef PAGE_BREAK_BEFORE_SHIFT
#undef PAGE_BREAK_BEFORE_INDEX
    
#define PAGE_BREAK_INSIDE_INDEX 0
#define PAGE_BREAK_INSIDE_SHIFT 6
#define PAGE_BREAK_INSIDE_MASK 0xc0
static inline uint8_t get_page_break_inside(
	    const css_computed_style *style)
{
	if (style->page != NULL) {
		uint8_t bits = style->page->bits[PAGE_BREAK_INSIDE_INDEX];
		bits &= PAGE_BREAK_INSIDE_MASK;
		bits >>= PAGE_BREAK_INSIDE_SHIFT;

		/* 2bits: type */
		return bits;
	}

	return CSS_PAGE_BREAK_INSIDE_AUTO;
}
#undef PAGE_BREAK_INSIDE_MASK
#undef PAGE_BREAK_INSIDE_SHIFT
#undef PAGE_BREAK_INSIDE_INDEX

#define ORPHANS_INDEX 1
#define ORPHANS_SHIFT 0
#define ORPHANS_MASK 0x1
static inline uint8_t get_orphans(
		const css_computed_style *style,
		int32_t *orphans)
{
	if (style->page != NULL) {
		uint8_t bits = style->page->bits[ORPHANS_INDEX];
		bits &= ORPHANS_MASK;
		bits >>= ORPHANS_SHIFT;
		
		*orphans = style->page->orphans;
		
		/* 1bit: type */
		return bits;
	}
	
	*orphans = 2;
	return CSS_ORPHANS_SET;
}
#undef ORPHANS_MASK
#undef ORPHANS_SHIFT
#undef ORPHANS_INDEX

#define WIDOWS_INDEX 1
#define WIDOWS_SHIFT 1
#define WIDOWS_MASK 0x2
static inline uint8_t get_widows(
		const css_computed_style *style,
		int32_t *widows)
{
	if (style->page != NULL) {
		uint8_t bits = style->page->bits[WIDOWS_INDEX];
		bits &= WIDOWS_MASK;
		bits >>= WIDOWS_SHIFT;
		
		*widows = style->page->widows;
		
		/* 1bit: type */
		return bits;
	}
	
	*widows = 2;
	return CSS_WIDOWS_SET;
}
#undef WIDOWS_MASK
#undef WIDOWS_SHIFT
#undef WIDOWS_INDEX

#endif
