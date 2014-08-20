/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_select_propset_h_
#define css_select_propset_h_

#include <string.h>

#include <libcss/computed.h>
#include "computed.h"

/* Important: keep this file in sync with computed.h */
/** \todo Is there a better way to ensure this happens? */

static const css_computed_uncommon default_uncommon = {
	{ (CSS_LETTER_SPACING_INHERIT << 2) | CSS_OUTLINE_COLOR_INVERT,
	  (CSS_OUTLINE_WIDTH_MEDIUM << 1) | CSS_BORDER_SPACING_INHERIT,
	  0,
	  (CSS_WORD_SPACING_INHERIT << 2) | 
		(CSS_COUNTER_INCREMENT_NONE << 1) | CSS_COUNTER_RESET_NONE,
	  (CSS_CURSOR_INHERIT << 3) | (CSS_WRITING_MODE_INHERIT << 1) | 0,
	  0,
	  0,
	  (CSS_CLIP_AUTO << 2) | CSS_CONTENT_NORMAL
	},
	{ 0, 0 },
	{ 0, 0, 0, 0 },
	0,
	0,
	0,
	0,
	NULL,
	NULL,
	NULL,
	NULL
};

#define ENSURE_UNCOMMON do {						\
	if (style->uncommon == NULL) {					\
		style->uncommon = malloc(sizeof(css_computed_uncommon));\
		if (style->uncommon == NULL)				\
			return CSS_NOMEM;				\
									\
		memcpy(style->uncommon, &default_uncommon,		\
				sizeof(css_computed_uncommon));		\
	}								\
} while(0)

static const css_computed_page default_page = {
	{	
		(CSS_PAGE_BREAK_INSIDE_AUTO <<  6) | 
			(CSS_PAGE_BREAK_BEFORE_AUTO << 3) |
			CSS_PAGE_BREAK_AFTER_AUTO,
		(CSS_WIDOWS_SET << 1) | 
			CSS_ORPHANS_SET
	},
	2 << CSS_RADIX_POINT, 
	2 << CSS_RADIX_POINT
};

#define ENSURE_PAGE do {						\
	if (style->page == NULL) {					\
		style->page = malloc(sizeof(css_computed_page));	\
		if (style->page == NULL)				\
			return CSS_NOMEM;				\
									\
		memcpy(style->page, &default_page,			\
				sizeof(css_computed_page));		\
	}								\
} while(0)

#define LETTER_SPACING_INDEX 0
#define LETTER_SPACING_SHIFT 2
#define LETTER_SPACING_MASK  0xfc
static inline css_error set_letter_spacing(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[LETTER_SPACING_INDEX];

	/* 6bits: uuuutt : unit | type */
	*bits = (*bits & ~LETTER_SPACING_MASK) | 
			(((type & 0x3) | unit << 2) << LETTER_SPACING_SHIFT);

	style->uncommon->letter_spacing = length;

	return CSS_OK;
}
#undef LETTER_SPACING_MASK
#undef LETTER_SPACING_SHIFT
#undef LETTER_SPACING_INDEX

#define OUTLINE_COLOR_INDEX 0
#define OUTLINE_COLOR_SHIFT 0
#define OUTLINE_COLOR_MASK  0x3
static inline css_error set_outline_color(
		css_computed_style *style, uint8_t type, css_color color)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[OUTLINE_COLOR_INDEX];

	/* 2bits: tt : type */
	*bits = (*bits & ~OUTLINE_COLOR_MASK) |
			((type & 0x3) << OUTLINE_COLOR_SHIFT);

	style->uncommon->outline_color = color;

	return CSS_OK;
}
#undef OUTLINE_COLOR_MASK
#undef OUTLINE_COLOR_SHIFT
#undef OUTLINE_COLOR_INDEX

#define OUTLINE_WIDTH_INDEX 1
#define OUTLINE_WIDTH_SHIFT 1
#define OUTLINE_WIDTH_MASK  0xfe
static inline css_error set_outline_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[OUTLINE_WIDTH_INDEX];

	/* 7bits: uuuuttt : unit | type */
	*bits = (*bits & ~OUTLINE_WIDTH_MASK) |
			(((type & 0x7) | (unit << 3)) << OUTLINE_WIDTH_SHIFT);

	style->uncommon->outline_width = length;

	return CSS_OK;
}
#undef OUTLINE_WIDTH_MASK
#undef OUTLINE_WIDTH_SHIFT
#undef OUTLINE_WIDTH_INDEX

#define BORDER_SPACING_INDEX 1
#define BORDER_SPACING_SHIFT 0
#define BORDER_SPACING_MASK  0x1
#define BORDER_SPACING_INDEX1 2
#define BORDER_SPACING_SHIFT1 0
static inline css_error set_border_spacing(
		css_computed_style *style, uint8_t type, 
		css_fixed hlength, css_unit hunit,
		css_fixed vlength, css_unit vunit)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[BORDER_SPACING_INDEX];

	/* 1 bit: type */
	*bits = (*bits & ~BORDER_SPACING_MASK) | 
			((type & 0x1) << BORDER_SPACING_SHIFT);

	bits = &style->uncommon->bits[BORDER_SPACING_INDEX1];

	/* 8bits: hhhhvvvv : hunit | vunit */
	*bits = (((hunit << 4) | vunit) << BORDER_SPACING_SHIFT1);


	style->uncommon->border_spacing[0] = hlength;
	style->uncommon->border_spacing[1] = vlength;

	return CSS_OK;
}
#undef BORDER_SPACING_SHIFT1
#undef BORDER_SPACING_INDEX1
#undef BORDER_SPACING_MASK
#undef BORDER_SPACING_SHIFT
#undef BORDER_SPACING_INDEX

#define WORD_SPACING_INDEX 3
#define WORD_SPACING_SHIFT 2
#define WORD_SPACING_MASK  0xfc
static inline css_error set_word_spacing(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[WORD_SPACING_INDEX];

	/* 6bits: uuuutt : unit | type */
	*bits = (*bits & ~WORD_SPACING_MASK) |
			(((type & 0x3) | (unit << 2)) << WORD_SPACING_SHIFT);
	
	style->uncommon->word_spacing = length;

	return CSS_OK;
}
#undef WORD_SPACING_MASK
#undef WORD_SPACING_SHIFT
#undef WORD_SPACING_INDEX

#define WRITING_MODE_INDEX 4
#define WRITING_MODE_SHIFT 1
#define WRITING_MODE_MASK  0x6
static inline css_error set_writing_mode(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[WRITING_MODE_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~WRITING_MODE_MASK) |
			((type & 0x3) << WRITING_MODE_SHIFT);

	return CSS_OK;
}
#undef WRITING_MODE_MASK
#undef WRITING_MODE_SHIFT
#undef WRITING_MODE_INDEX

#define COUNTER_INCREMENT_INDEX 3
#define COUNTER_INCREMENT_SHIFT 1
#define COUNTER_INCREMENT_MASK  0x2
static inline css_error set_counter_increment(
		css_computed_style *style, uint8_t type, 
		css_computed_counter *counters)
{
	uint8_t *bits;
	css_computed_counter *oldcounters;
	css_computed_counter *c;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[COUNTER_INCREMENT_INDEX];
	oldcounters = style->uncommon->counter_increment;

	/* 1bit: type */
	*bits = (*bits & ~COUNTER_INCREMENT_MASK) |
			((type & 0x1) << COUNTER_INCREMENT_SHIFT);

	for (c = counters; c != NULL && c->name != NULL; c++)
		c->name = lwc_string_ref(c->name);

	style->uncommon->counter_increment = counters;

	/* Free existing array */
	if (oldcounters != NULL) {
		for (c = oldcounters; c->name != NULL; c++)
			lwc_string_unref(c->name);

		if (oldcounters != counters)
			free(oldcounters);
	}

	return CSS_OK;
}
#undef COUNTER_INCREMENT_MASK
#undef COUNTER_INCREMENT_SHIFT
#undef COUNTER_INCREMENT_INDEX

#define COUNTER_RESET_INDEX 3
#define COUNTER_RESET_SHIFT 0
#define COUNTER_RESET_MASK  0x1
static inline css_error set_counter_reset(
		css_computed_style *style, uint8_t type, 
		css_computed_counter *counters)
{
	uint8_t *bits;
	css_computed_counter *oldcounters;
	css_computed_counter *c;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[COUNTER_RESET_INDEX];
	oldcounters = style->uncommon->counter_reset;

	/* 1bit: type */
	*bits = (*bits & ~COUNTER_RESET_MASK) |
			((type & 0x1) << COUNTER_RESET_SHIFT);

	for (c = counters; c != NULL && c->name != NULL; c++)
		c->name = lwc_string_ref(c->name);

	style->uncommon->counter_reset = counters;

	/* Free existing array */
	if (oldcounters != NULL) {
		for (c = oldcounters; c->name != NULL; c++)
			lwc_string_unref(c->name);

		if (oldcounters != counters)
			free(oldcounters);
	}

	return CSS_OK;
}
#undef COUNTER_RESET_MASK
#undef COUNTER_RESET_SHIFT
#undef COUNTER_RESET_INDEX

#define CURSOR_INDEX 4
#define CURSOR_SHIFT 3
#define CURSOR_MASK  0xf8
static inline css_error set_cursor(
		css_computed_style *style, uint8_t type, 
		lwc_string **urls)
{
	uint8_t *bits;
	lwc_string **oldurls;
	lwc_string **s;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[CURSOR_INDEX];
	oldurls = style->uncommon->cursor;

	/* 5bits: type */
	*bits = (*bits & ~CURSOR_MASK) |
			((type & 0x1f) << CURSOR_SHIFT);

	for (s = urls; s != NULL && *s != NULL; s++)
		*s = lwc_string_ref(*s);

	style->uncommon->cursor = urls;

	/* Free existing array */
	if (oldurls != NULL) {
		for (s = oldurls; *s != NULL; s++)
			lwc_string_unref(*s);

		if (oldurls != urls)
			free(oldurls);
	}

	return CSS_OK;
}
#undef CURSOR_MASK
#undef CURSOR_SHIFT
#undef CURSOR_INDEX

#define CLIP_INDEX 7
#define CLIP_SHIFT 2
#define CLIP_MASK  0xfc
#define CLIP_INDEX1 5
#define CLIP_SHIFT1 0
#define CLIP_INDEX2 6
#define CLIP_SHIFT2 0
static inline css_error set_clip(
		css_computed_style *style, uint8_t type, 
		css_computed_clip_rect *rect)
{
	uint8_t *bits;

	ENSURE_UNCOMMON;

	bits = &style->uncommon->bits[CLIP_INDEX];

	/* 6bits: trblyy : top | right | bottom | left | type */
	*bits = (*bits & ~CLIP_MASK) | 
			((type & 0x3) << CLIP_SHIFT);

	if (type == CSS_CLIP_RECT) {
		*bits |= (((rect->top_auto ? 0x20 : 0) |
				(rect->right_auto ? 0x10 : 0) |
				(rect->bottom_auto ? 0x8 : 0) |
				(rect->left_auto ? 0x4 : 0)) << CLIP_SHIFT);

		bits = &style->uncommon->bits[CLIP_INDEX1];

		/* 8bits: ttttrrrr : top | right */
		*bits = (((rect->tunit << 4) | rect->runit) << CLIP_SHIFT1);

		bits = &style->uncommon->bits[CLIP_INDEX2];

		/* 8bits: bbbbllll : bottom | left */
		*bits = (((rect->bunit << 4) | rect->lunit) << CLIP_SHIFT2);

		style->uncommon->clip[0] = rect->top;
		style->uncommon->clip[1] = rect->right;
		style->uncommon->clip[2] = rect->bottom;
		style->uncommon->clip[3] = rect->left;
	}

	return CSS_OK;
}
#undef CLIP_SHIFT2
#undef CLIP_INDEX2
#undef CLIP_SHIFT1
#undef CLIP_INDEX1
#undef CLIP_MASK
#undef CLIP_SHIFT
#undef CLIP_INDEX

#define CONTENT_INDEX 7
#define CONTENT_SHIFT 0
#define CONTENT_MASK  0x3
static inline css_error set_content(
		css_computed_style *style, uint8_t type,
		css_computed_content_item *content)
{
	uint8_t *bits;
	css_computed_content_item *oldcontent;
	css_computed_content_item *c;

	ENSURE_UNCOMMON;

	/* 2bits: type */
	bits = &style->uncommon->bits[CONTENT_INDEX];
	oldcontent = style->uncommon->content;

	*bits = (*bits & ~CONTENT_MASK) |
			((type & 0x3) << CONTENT_SHIFT);

	for (c = content; c != NULL && 
			c->type != CSS_COMPUTED_CONTENT_NONE; c++) {
		switch (c->type) {
		case CSS_COMPUTED_CONTENT_STRING:
			c->data.string = lwc_string_ref(c->data.string);
			break;
		case CSS_COMPUTED_CONTENT_URI:
			c->data.uri = lwc_string_ref(c->data.uri);
			break;
		case CSS_COMPUTED_CONTENT_ATTR:
			c->data.attr = lwc_string_ref(c->data.attr);
			break;
		case CSS_COMPUTED_CONTENT_COUNTER:
			c->data.counter.name =
				lwc_string_ref(c->data.counter.name);
			break;
		case CSS_COMPUTED_CONTENT_COUNTERS:
			c->data.counters.name = 
				lwc_string_ref(c->data.counters.name);
			c->data.counters.sep = 
				lwc_string_ref(c->data.counters.sep);
			break;
		default:
			break;
		}
	}

	style->uncommon->content = content;

	/* Free existing array */
	if (oldcontent != NULL) {
		for (c = oldcontent; 
				c->type != CSS_COMPUTED_CONTENT_NONE; c++) {
			switch (c->type) {
			case CSS_COMPUTED_CONTENT_STRING:
				lwc_string_unref(c->data.string);
				break;
			case CSS_COMPUTED_CONTENT_URI:
				lwc_string_unref(c->data.uri);
				break;
			case CSS_COMPUTED_CONTENT_ATTR:
				lwc_string_unref(c->data.attr);
				break;
			case CSS_COMPUTED_CONTENT_COUNTER:
				lwc_string_unref(c->data.counter.name);
				break;
			case CSS_COMPUTED_CONTENT_COUNTERS:
				lwc_string_unref(c->data.counters.name);
				lwc_string_unref(c->data.counters.sep);
				break;
			default:
				break;
			}
		}

		if (oldcontent != content)
			free(oldcontent);
	}

	return CSS_OK;
}
#undef CONTENT_MASK
#undef CONTENT_SHIFT
#undef CONTENT_INDEX


#define VERTICAL_ALIGN_INDEX 0
#define VERTICAL_ALIGN_SHIFT 0
static inline css_error set_vertical_align(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[VERTICAL_ALIGN_INDEX];

	/* 8bits: uuuutttt : units | type */
	*bits = (((type & 0xf) | (unit << 4)) << VERTICAL_ALIGN_SHIFT);

	style->vertical_align = length;

	return CSS_OK;
}
#undef VERTICAL_ALIGN_SHIFT
#undef VERTICAL_ALIGN_INDEX

#define FONT_SIZE_INDEX 1
#define FONT_SIZE_SHIFT 0
static inline css_error set_font_size(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[FONT_SIZE_INDEX];

	/* 8bits: uuuutttt : units | type */
	*bits = (((type & 0xf) | (unit << 4)) << FONT_SIZE_SHIFT);

	style->font_size = length;

	return CSS_OK;
}
#undef FONT_SIZE_SHIFT
#undef FONT_SIZE_INDEX

#define BORDER_TOP_WIDTH_INDEX 2
#define BORDER_TOP_WIDTH_SHIFT 1
#define BORDER_TOP_WIDTH_MASK  0xfe
static inline css_error set_border_top_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[BORDER_TOP_WIDTH_INDEX];

	/* 7bits: uuuuttt : units | type */
	*bits = (*bits & ~BORDER_TOP_WIDTH_MASK) |
		(((type & 0x7) | (unit << 3)) << BORDER_TOP_WIDTH_SHIFT);

	style->border_width[0] = length;

	return CSS_OK;
}
#undef BORDER_TOP_WIDTH_MASK
#undef BORDER_TOP_WIDTH_SHIFT
#undef BORDER_TOP_WIDTH_INDEX

#define BORDER_RIGHT_WIDTH_INDEX 3
#define BORDER_RIGHT_WIDTH_SHIFT 1
#define BORDER_RIGHT_WIDTH_MASK  0xfe
static inline css_error set_border_right_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[BORDER_RIGHT_WIDTH_INDEX];

	/* 7bits: uuuuttt : units | type */
	*bits = (*bits & ~BORDER_RIGHT_WIDTH_MASK) |
		(((type & 0x7) | (unit << 3)) << BORDER_RIGHT_WIDTH_SHIFT);

	style->border_width[1] = length;

	return CSS_OK;
}
#undef BORDER_RIGHT_WIDTH_MASK
#undef BORDER_RIGHT_WIDTH_SHIFT
#undef BORDER_RIGHT_WIDTH_INDEX

#define BORDER_BOTTOM_WIDTH_INDEX 4
#define BORDER_BOTTOM_WIDTH_SHIFT 1
#define BORDER_BOTTOM_WIDTH_MASK  0xfe
static inline css_error set_border_bottom_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[BORDER_BOTTOM_WIDTH_INDEX];

	/* 7bits: uuuuttt : units | type */
	*bits = (*bits & ~BORDER_BOTTOM_WIDTH_MASK) |
		(((type & 0x7) | (unit << 3)) << BORDER_BOTTOM_WIDTH_SHIFT);

	style->border_width[2] = length;

	return CSS_OK;
}
#undef BORDER_BOTTOM_WIDTH_MASK
#undef BORDER_BOTTOM_WIDTH_SHIFT
#undef BORDER_BOTTOM_WIDTH_INDEX

#define BORDER_LEFT_WIDTH_INDEX 5
#define BORDER_LEFT_WIDTH_SHIFT 1
#define BORDER_LEFT_WIDTH_MASK  0xfe
static inline css_error set_border_left_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[BORDER_LEFT_WIDTH_INDEX];

	/* 7bits: uuuuttt : units | type */
	*bits = (*bits & ~BORDER_LEFT_WIDTH_MASK) |
		(((type & 0x7) | (unit << 3)) << BORDER_LEFT_WIDTH_SHIFT);

	style->border_width[3] = length;

	return CSS_OK;
}
#undef BORDER_LEFT_WIDTH_MASK
#undef BORDER_LEFT_WIDTH_SHIFT
#undef BORDER_LEFT_WIDTH_INDEX

#define BACKGROUND_IMAGE_INDEX 2
#define BACKGROUND_IMAGE_SHIFT 0
#define BACKGROUND_IMAGE_MASK  0x1
static inline css_error set_background_image(
		css_computed_style *style, uint8_t type, 
		lwc_string *url)
{
	uint8_t *bits = &style->bits[BACKGROUND_IMAGE_INDEX];
	lwc_string *oldurl = style->background_image;

	/* 1bit: type */
	*bits = (*bits & ~BACKGROUND_IMAGE_MASK) |
			((type & 0x1) << BACKGROUND_IMAGE_SHIFT);

	if (url != NULL) {
                style->background_image = lwc_string_ref(url);
	} else {
		style->background_image = NULL;
	}

	if (oldurl != NULL)
		lwc_string_unref(oldurl);

	return CSS_OK;
}
#undef BACKGROUND_IMAGE_MASK
#undef BACKGROUND_IMAGE_SHIFT
#undef BACKGROUND_IMAGE_INDEX

#define COLOR_INDEX 3
#define COLOR_SHIFT 0
#define COLOR_MASK  0x1
static inline css_error set_color(
		css_computed_style *style, uint8_t type, 
		css_color color)
{
	uint8_t *bits = &style->bits[COLOR_INDEX];

	/* 1bit: type */
	*bits = (*bits & ~COLOR_MASK) |
			((type & 0x1) << COLOR_SHIFT);

	style->color = color;

	return CSS_OK;
}
#undef COLOR_MASK
#undef COLOR_SHIFT
#undef COLOR_INDEX

#define LIST_STYLE_IMAGE_INDEX 4
#define LIST_STYLE_IMAGE_SHIFT 0
#define LIST_STYLE_IMAGE_MASK  0x1
static inline css_error set_list_style_image(
		css_computed_style *style, uint8_t type, 
		lwc_string *url)
{
	uint8_t *bits = &style->bits[LIST_STYLE_IMAGE_INDEX];
	lwc_string *oldurl = style->list_style_image;

	/* 1bit: type */
	*bits = (*bits & ~LIST_STYLE_IMAGE_MASK) |
			((type & 0x1) << LIST_STYLE_IMAGE_SHIFT);

	if (url != NULL) {
		style->list_style_image = lwc_string_ref(url);
	} else {
		style->list_style_image = NULL;
	}

	if (oldurl != NULL)
		lwc_string_unref(oldurl);

	return CSS_OK;
}
#undef LIST_STYLE_IMAGE_MASK
#undef LIST_STYLE_IMAGE_SHIFT
#undef LIST_STYLE_IMAGE_INDEX

#define QUOTES_INDEX 5
#define QUOTES_SHIFT 0
#define QUOTES_MASK  0x1
static inline css_error set_quotes(
		css_computed_style *style, uint8_t type, 
		lwc_string **quotes)
{
	uint8_t *bits = &style->bits[QUOTES_INDEX];
	lwc_string **oldquotes = style->quotes;
	lwc_string **s;

	/* 1bit: type */
	*bits = (*bits & ~QUOTES_MASK) |
			((type & 0x1) << QUOTES_SHIFT);

	for (s = quotes; s != NULL && *s != NULL; s++)
		*s = lwc_string_ref(*s);

	style->quotes = quotes;

	/* Free current quotes */
	if (oldquotes != NULL) {
		for (s = oldquotes; *s != NULL; s++)
			lwc_string_unref(*s);

		if (oldquotes != quotes)
			free(oldquotes);
	}

	return CSS_OK;
}
#undef QUOTES_MASK
#undef QUOTES_SHIFT
#undef QUOTES_INDEX

#define TOP_INDEX 6
#define TOP_SHIFT 2
#define TOP_MASK  0xfc
static inline css_error set_top(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[TOP_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~TOP_MASK) |
			(((type & 0x3) | (unit << 2)) << TOP_SHIFT);

	style->top = length;

	return CSS_OK;
}
#undef TOP_MASK
#undef TOP_SHIFT
#undef TOP_INDEX

#define RIGHT_INDEX 7
#define RIGHT_SHIFT 2
#define RIGHT_MASK  0xfc
static inline css_error set_right(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[RIGHT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~RIGHT_MASK) |
			(((type & 0x3) | (unit << 2)) << RIGHT_SHIFT);

	style->right = length;

	return CSS_OK;
}
#undef RIGHT_MASK
#undef RIGHT_SHIFT
#undef RIGHT_INDEX

#define BOTTOM_INDEX 8
#define BOTTOM_SHIFT 2
#define BOTTOM_MASK  0xfc
static inline css_error set_bottom(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[BOTTOM_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~BOTTOM_MASK) |
			(((type & 0x3) | (unit << 2)) << BOTTOM_SHIFT);

	style->bottom = length;

	return CSS_OK;
}
#undef BOTTOM_MASK
#undef BOTTOM_SHIFT
#undef BOTTOM_INDEX

#define LEFT_INDEX 9
#define LEFT_SHIFT 2
#define LEFT_MASK  0xfc
static inline css_error set_left(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[LEFT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~LEFT_MASK) |
			(((type & 0x3) | (unit << 2)) << LEFT_SHIFT);

	style->left = length;

	return CSS_OK;
}
#undef LEFT_MASK
#undef LEFT_SHIFT
#undef LEFT_INDEX

#define BORDER_TOP_COLOR_INDEX 6
#define BORDER_TOP_COLOR_SHIFT 0
#define BORDER_TOP_COLOR_MASK  0x3
static inline css_error set_border_top_color(
		css_computed_style *style, uint8_t type, 
		css_color color)
{
	uint8_t *bits = &style->bits[BORDER_TOP_COLOR_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BORDER_TOP_COLOR_MASK) |
			((type & 0x3) << BORDER_TOP_COLOR_SHIFT);

	style->border_color[0] = color;

	return CSS_OK;
}
#undef BORDER_TOP_COLOR_MASK
#undef BORDER_TOP_COLOR_SHIFT
#undef BORDER_TOP_COLOR_INDEX

#define BORDER_RIGHT_COLOR_INDEX 7
#define BORDER_RIGHT_COLOR_SHIFT 0
#define BORDER_RIGHT_COLOR_MASK  0x3
static inline css_error set_border_right_color(
		css_computed_style *style, uint8_t type, 
		css_color color)
{
	uint8_t *bits = &style->bits[BORDER_RIGHT_COLOR_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BORDER_RIGHT_COLOR_MASK) |
			((type & 0x3) << BORDER_RIGHT_COLOR_SHIFT);

	style->border_color[1] = color;

	return CSS_OK;
}
#undef BORDER_RIGHT_COLOR_MASK
#undef BORDER_RIGHT_COLOR_SHIFT
#undef BORDER_RIGHT_COLOR_INDEX

#define BORDER_BOTTOM_COLOR_INDEX 8
#define BORDER_BOTTOM_COLOR_SHIFT 0
#define BORDER_BOTTOM_COLOR_MASK  0x3
static inline css_error set_border_bottom_color(
		css_computed_style *style, uint8_t type, 
		css_color color)
{
	uint8_t *bits = &style->bits[BORDER_BOTTOM_COLOR_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BORDER_BOTTOM_COLOR_MASK) |
			((type & 0x3) << BORDER_BOTTOM_COLOR_SHIFT);

	style->border_color[2] = color;

	return CSS_OK;
}
#undef BORDER_BOTTOM_COLOR_MASK
#undef BORDER_BOTTOM_COLOR_SHIFT
#undef BORDER_BOTTOM_COLOR_INDEX

#define BORDER_LEFT_COLOR_INDEX 9
#define BORDER_LEFT_COLOR_SHIFT 0
#define BORDER_LEFT_COLOR_MASK  0x3
static inline css_error set_border_left_color(
		css_computed_style *style, uint8_t type, 
		css_color color)
{
	uint8_t *bits = &style->bits[BORDER_LEFT_COLOR_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BORDER_LEFT_COLOR_MASK) |
			((type & 0x3) << BORDER_LEFT_COLOR_SHIFT);

	style->border_color[3] = color;

	return CSS_OK;
}
#undef BORDER_LEFT_COLOR_MASK
#undef BORDER_LEFT_COLOR_SHIFT
#undef BORDER_LEFT_COLOR_INDEX

#define HEIGHT_INDEX 10
#define HEIGHT_SHIFT 2
#define HEIGHT_MASK  0xfc
static inline css_error set_height(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[HEIGHT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~HEIGHT_MASK) |
			(((type & 0x3) | (unit << 2)) << HEIGHT_SHIFT);

	style->height = length;

	return CSS_OK;
}
#undef HEIGHT_MASK
#undef HEIGHT_SHIFT
#undef HEIGHT_INDEX

#define LINE_HEIGHT_INDEX 11
#define LINE_HEIGHT_SHIFT 2
#define LINE_HEIGHT_MASK  0xfc
static inline css_error set_line_height(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[LINE_HEIGHT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~LINE_HEIGHT_MASK) |
			(((type & 0x3) | (unit << 2)) << LINE_HEIGHT_SHIFT);

	style->line_height = length;

	return CSS_OK;
}
#undef LINE_HEIGHT_MASK
#undef LINE_HEIGHT_SHIFT
#undef LINE_HEIGHT_INDEX

#define BACKGROUND_COLOR_INDEX 10
#define BACKGROUND_COLOR_SHIFT 0
#define BACKGROUND_COLOR_MASK  0x3
static inline css_error set_background_color(
		css_computed_style *style, uint8_t type, 
		css_color color)
{
	uint8_t *bits = &style->bits[BACKGROUND_COLOR_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BACKGROUND_COLOR_MASK) |
			((type & 0x3) << BACKGROUND_COLOR_SHIFT);

	style->background_color = color;

	return CSS_OK;
}
#undef BACKGROUND_COLOR_MASK
#undef BACKGROUND_COLOR_SHIFT
#undef BACKGROUND_COLOR_INDEX

#define Z_INDEX_INDEX 11
#define Z_INDEX_SHIFT 0
#define Z_INDEX_MASK  0x3
static inline css_error set_z_index(
		css_computed_style *style, uint8_t type, 
		int32_t z_index)
{
	uint8_t *bits = &style->bits[Z_INDEX_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~Z_INDEX_MASK) |
			((type & 0x3) << Z_INDEX_SHIFT);

	style->z_index = z_index;

	return CSS_OK;
}
#undef Z_INDEX_MASK
#undef Z_INDEX_SHIFT
#undef Z_INDEX_INDEX

#define MARGIN_TOP_INDEX 12
#define MARGIN_TOP_SHIFT 2
#define MARGIN_TOP_MASK  0xfc
static inline css_error set_margin_top(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MARGIN_TOP_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~MARGIN_TOP_MASK) |
			(((type & 0x3) | (unit << 2)) << MARGIN_TOP_SHIFT);

	style->margin[0] = length;

	return CSS_OK;
}
#undef MARGIN_TOP_MASK
#undef MARGIN_TOP_SHIFT
#undef MARGIN_TOP_INDEX

#define MARGIN_RIGHT_INDEX 13
#define MARGIN_RIGHT_SHIFT 2
#define MARGIN_RIGHT_MASK  0xfc
static inline css_error set_margin_right(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MARGIN_RIGHT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~MARGIN_RIGHT_MASK) |
			(((type & 0x3) | (unit << 2)) << MARGIN_RIGHT_SHIFT);

	style->margin[1] = length;

	return CSS_OK;
}
#undef MARGIN_RIGHT_MASK
#undef MARGIN_RIGHT_SHIFT
#undef MARGIN_RIGHT_INDEX

#define MARGIN_BOTTOM_INDEX 14
#define MARGIN_BOTTOM_SHIFT 2
#define MARGIN_BOTTOM_MASK  0xfc
static inline css_error set_margin_bottom(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MARGIN_BOTTOM_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~MARGIN_BOTTOM_MASK) |
			(((type & 0x3) | (unit << 2)) << MARGIN_BOTTOM_SHIFT);

	style->margin[2] = length;

	return CSS_OK;
}
#undef MARGIN_BOTTOM_MASK
#undef MARGIN_BOTTOM_SHIFT
#undef MARGIN_BOTTOM_INDEX

#define MARGIN_LEFT_INDEX 15
#define MARGIN_LEFT_SHIFT 2
#define MARGIN_LEFT_MASK  0xfc
static inline css_error set_margin_left(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MARGIN_LEFT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~MARGIN_LEFT_MASK) |
			(((type & 0x3) | (unit << 2)) << MARGIN_LEFT_SHIFT);

	style->margin[3] = length;

	return CSS_OK;
}
#undef MARGIN_LEFT_MASK
#undef MARGIN_LEFT_SHIFT
#undef MARGIN_LEFT_INDEX

#define BACKGROUND_ATTACHMENT_INDEX 12
#define BACKGROUND_ATTACHMENT_SHIFT 0
#define BACKGROUND_ATTACHMENT_MASK  0x3
static inline css_error set_background_attachment(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BACKGROUND_ATTACHMENT_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BACKGROUND_ATTACHMENT_MASK) |
			((type & 0x3) << BACKGROUND_ATTACHMENT_SHIFT);

	return CSS_OK;
}
#undef BACKGROUND_ATTACHMENT_MASK
#undef BACKGROUND_ATTACHMENT_SHIFT
#undef BACKGROUND_ATTACHMENT_INDEX

#define BORDER_COLLAPSE_INDEX 13
#define BORDER_COLLAPSE_SHIFT 0
#define BORDER_COLLAPSE_MASK  0x3
static inline css_error set_border_collapse(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BORDER_COLLAPSE_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~BORDER_COLLAPSE_MASK) |
			((type & 0x3) << BORDER_COLLAPSE_SHIFT);

	return CSS_OK;
}
#undef BORDER_COLLAPSE_MASK
#undef BORDER_COLLAPSE_SHIFT
#undef BORDER_COLLAPSE_INDEX

#define CAPTION_SIDE_INDEX 14
#define CAPTION_SIDE_SHIFT 0
#define CAPTION_SIDE_MASK  0x3
static inline css_error set_caption_side(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[CAPTION_SIDE_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~CAPTION_SIDE_MASK) |
			((type & 0x3) << CAPTION_SIDE_SHIFT);

	return CSS_OK;
}
#undef CAPTION_SIDE_MASK
#undef CAPTION_SIDE_SHIFT
#undef CAPTION_SIDE_INDEX

#define DIRECTION_INDEX 15
#define DIRECTION_SHIFT 0
#define DIRECTION_MASK  0x3
static inline css_error set_direction(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[DIRECTION_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~DIRECTION_MASK) |
			((type & 0x3) << DIRECTION_SHIFT);

	return CSS_OK;
}
#undef DIRECTION_MASK
#undef DIRECTION_SHIFT
#undef DIRECTION_INDEX

#define MAX_HEIGHT_INDEX 16
#define MAX_HEIGHT_SHIFT 2
#define MAX_HEIGHT_MASK  0xfc
static inline css_error set_max_height(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MAX_HEIGHT_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~MAX_HEIGHT_MASK) |
			(((type & 0x3) | (unit << 2)) << MAX_HEIGHT_SHIFT);

	style->max_height = length;

	return CSS_OK;
}
#undef MAX_HEIGHT_MASK
#undef MAX_HEIGHT_SHIFT
#undef MAX_HEIGHT_INDEX

#define MAX_WIDTH_INDEX 17
#define MAX_WIDTH_SHIFT 2
#define MAX_WIDTH_MASK  0xfc
static inline css_error set_max_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MAX_WIDTH_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~MAX_WIDTH_MASK) |
			(((type & 0x3) | (unit << 2)) << MAX_WIDTH_SHIFT);

	style->max_width = length;

	return CSS_OK;
}
#undef MAX_WIDTH_MASK
#undef MAX_WIDTH_SHIFT
#undef MAX_WIDTH_INDEX

#define WIDTH_INDEX 18
#define WIDTH_SHIFT 2
#define WIDTH_MASK  0xfc
static inline css_error set_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[WIDTH_INDEX];

	/* 6bits: uuuutt : units | type */
	*bits = (*bits & ~WIDTH_MASK) |
			(((type & 0x3) | (unit << 2)) << WIDTH_SHIFT);

	style->width = length;

	return CSS_OK;
}
#undef WIDTH_MASK
#undef WIDTH_SHIFT
#undef WIDTH_INDEX

#define EMPTY_CELLS_INDEX 16
#define EMPTY_CELLS_SHIFT 0
#define EMPTY_CELLS_MASK  0x3
static inline css_error set_empty_cells(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[EMPTY_CELLS_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~EMPTY_CELLS_MASK) |
			((type & 0x3) << EMPTY_CELLS_SHIFT);

	return CSS_OK;
}
#undef EMPTY_CELLS_MASK
#undef EMPTY_CELLS_SHIFT
#undef EMPTY_CELLS_INDEX

#define FLOAT_INDEX 17
#define FLOAT_SHIFT 0
#define FLOAT_MASK  0x3
static inline css_error set_float(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[FLOAT_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~FLOAT_MASK) |
			((type & 0x3) << FLOAT_SHIFT);

	return CSS_OK;
}
#undef FLOAT_MASK
#undef FLOAT_SHIFT
#undef FLOAT_INDEX

#define FONT_STYLE_INDEX 18
#define FONT_STYLE_SHIFT 0
#define FONT_STYLE_MASK  0x3
static inline css_error set_font_style(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[FONT_STYLE_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~FONT_STYLE_MASK) |
			((type & 0x3) << FONT_STYLE_SHIFT);

	return CSS_OK;
}
#undef FONT_STYLE_MASK
#undef FONT_STYLE_SHIFT
#undef FONT_STYLE_INDEX

#define MIN_HEIGHT_INDEX 19
#define MIN_HEIGHT_SHIFT 3
#define MIN_HEIGHT_MASK  0xf8
static inline css_error set_min_height(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MIN_HEIGHT_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~MIN_HEIGHT_MASK) |
			(((type & 0x1) | (unit << 1)) << MIN_HEIGHT_SHIFT);

	style->min_height = length;

	return CSS_OK;
}
#undef MIN_HEIGHT_MASK
#undef MIN_HEIGHT_SHIFT
#undef MIN_HEIGHT_INDEX

#define MIN_WIDTH_INDEX 20
#define MIN_WIDTH_SHIFT 3
#define MIN_WIDTH_MASK  0xf8
static inline css_error set_min_width(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[MIN_WIDTH_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~MIN_WIDTH_MASK) |
			(((type & 0x1) | (unit << 1)) << MIN_WIDTH_SHIFT);

	style->min_width = length;

	return CSS_OK;
}
#undef MIN_WIDTH_MASK
#undef MIN_WIDTH_SHIFT
#undef MIN_WIDTH_INDEX

#define BACKGROUND_REPEAT_INDEX 19
#define BACKGROUND_REPEAT_SHIFT 0
#define BACKGROUND_REPEAT_MASK  0x7
static inline css_error set_background_repeat(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BACKGROUND_REPEAT_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~BACKGROUND_REPEAT_MASK) |
			((type & 0x7) << BACKGROUND_REPEAT_SHIFT);

	return CSS_OK;
}
#undef BACKGROUND_REPEAT_MASK
#undef BACKGROUND_REPEAT_SHIFT
#undef BACKGROUND_REPEAT_INDEX

#define CLEAR_INDEX 20
#define CLEAR_SHIFT 0
#define CLEAR_MASK  0x7
static inline css_error set_clear(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[CLEAR_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~CLEAR_MASK) |
			((type & 0x7) << CLEAR_SHIFT);

	return CSS_OK;
}
#undef CLEAR_MASK
#undef CLEAR_SHIFT
#undef CLEAR_INDEX

#define PADDING_TOP_INDEX 21
#define PADDING_TOP_SHIFT 3
#define PADDING_TOP_MASK  0xf8
static inline css_error set_padding_top(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[PADDING_TOP_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~PADDING_TOP_MASK) |
			(((type & 0x1) | (unit << 1)) << PADDING_TOP_SHIFT);

	style->padding[0] = length;

	return CSS_OK;
}
#undef PADDING_TOP_MASK
#undef PADDING_TOP_SHIFT
#undef PADDING_TOP_INDEX

#define PADDING_RIGHT_INDEX 22
#define PADDING_RIGHT_SHIFT 3
#define PADDING_RIGHT_MASK  0xf8
static inline css_error set_padding_right(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[PADDING_RIGHT_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~PADDING_RIGHT_MASK) |
			(((type & 0x1) | (unit << 1)) << PADDING_RIGHT_SHIFT);

	style->padding[1] = length;

	return CSS_OK;
}
#undef PADDING_RIGHT_MASK
#undef PADDING_RIGHT_SHIFT
#undef PADDING_RIGHT_INDEX

#define PADDING_BOTTOM_INDEX 23
#define PADDING_BOTTOM_SHIFT 3
#define PADDING_BOTTOM_MASK  0xf8
static inline css_error set_padding_bottom(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[PADDING_BOTTOM_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~PADDING_BOTTOM_MASK) |
			(((type & 0x1) | (unit << 1)) << PADDING_BOTTOM_SHIFT);

	style->padding[2] = length;

	return CSS_OK;
}
#undef PADDING_BOTTOM_MASK
#undef PADDING_BOTTOM_SHIFT
#undef PADDING_BOTTOM_INDEX

#define PADDING_LEFT_INDEX 24
#define PADDING_LEFT_SHIFT 3
#define PADDING_LEFT_MASK  0xf8
static inline css_error set_padding_left(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[PADDING_LEFT_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~PADDING_LEFT_MASK) |
			(((type & 0x1) | (unit << 1)) << PADDING_LEFT_SHIFT);

	style->padding[3] = length;

	return CSS_OK;
}
#undef PADDING_LEFT_MASK
#undef PADDING_LEFT_SHIFT
#undef PADDING_LEFT_INDEX

#define OVERFLOW_INDEX 21
#define OVERFLOW_SHIFT 0
#define OVERFLOW_MASK  0x7
static inline css_error set_overflow(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[OVERFLOW_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~OVERFLOW_MASK) |
			((type & 0x7) << OVERFLOW_SHIFT);

	return CSS_OK;
}
#undef OVERFLOW_MASK
#undef OVERFLOW_SHIFT
#undef OVERFLOW_INDEX

#define POSITION_INDEX 22
#define POSITION_SHIFT 0
#define POSITION_MASK  0x7
static inline css_error set_position(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[POSITION_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~POSITION_MASK) |
			((type & 0x7) << POSITION_SHIFT);

	return CSS_OK;
}
#undef POSITION_MASK
#undef POSITION_SHIFT
#undef POSITION_INDEX

#define OPACITY_INDEX 23
#define OPACITY_SHIFT 2
#define OPACITY_MASK  0x04
static inline css_error set_opacity(
		css_computed_style *style, 
		uint8_t type, css_fixed opacity)
{
	uint8_t *bits = &style->bits[OPACITY_INDEX];

	/* 1bit: t : type */
	*bits = (*bits & ~OPACITY_MASK) |
			((type & 0x1) << OPACITY_SHIFT);

	style->opacity = opacity;

	return CSS_OK;
}
#undef OPACITY_MASK
#undef OPACITY_SHIFT
#undef OPACITY_INDEX

#define TEXT_TRANSFORM_INDEX 24
#define TEXT_TRANSFORM_SHIFT 0
#define TEXT_TRANSFORM_MASK  0x7
static inline css_error set_text_transform(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[TEXT_TRANSFORM_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~TEXT_TRANSFORM_MASK) |
			((type & 0x7) << TEXT_TRANSFORM_SHIFT);

	return CSS_OK;
}
#undef TEXT_TRANSFORM_MASK
#undef TEXT_TRANSFORM_SHIFT
#undef TEXT_TRANSFORM_INDEX

#define TEXT_INDENT_INDEX 25
#define TEXT_INDENT_SHIFT 3
#define TEXT_INDENT_MASK  0xf8
static inline css_error set_text_indent(
		css_computed_style *style, uint8_t type, 
		css_fixed length, css_unit unit)
{
	uint8_t *bits = &style->bits[TEXT_INDENT_INDEX];

	/* 5bits: uuuut : units | type */
	*bits = (*bits & ~TEXT_INDENT_MASK) |
			(((type & 0x1) | (unit << 1)) << TEXT_INDENT_SHIFT);

	style->text_indent = length;

	return CSS_OK;
}
#undef TEXT_INDENT_MASK
#undef TEXT_INDENT_SHIFT
#undef TEXT_INDENT_INDEX

#define WHITE_SPACE_INDEX 25
#define WHITE_SPACE_SHIFT 0
#define WHITE_SPACE_MASK  0x7
static inline css_error set_white_space(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[WHITE_SPACE_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~WHITE_SPACE_MASK) |
			((type & 0x7) << WHITE_SPACE_SHIFT);

	return CSS_OK;
}
#undef WHITE_SPACE_MASK
#undef WHITE_SPACE_SHIFT
#undef WHITE_SPACE_INDEX

#define BACKGROUND_POSITION_INDEX 27
#define BACKGROUND_POSITION_SHIFT 7
#define BACKGROUND_POSITION_MASK  0x80
#define BACKGROUND_POSITION_INDEX1 26
#define BACKGROUND_POSITION_SHIFT1 0
static inline css_error set_background_position(
		css_computed_style *style, uint8_t type, 
		css_fixed hlength, css_unit hunit,
		css_fixed vlength, css_unit vunit)
{
	uint8_t *bits;

	bits = &style->bits[BACKGROUND_POSITION_INDEX];

	/* 1 bit: type */
	*bits = (*bits & ~BACKGROUND_POSITION_MASK) | 
			((type & 0x1) << BACKGROUND_POSITION_SHIFT);

	bits = &style->bits[BACKGROUND_POSITION_INDEX1];

	/* 8bits: hhhhvvvv : hunit | vunit */
	*bits = (((hunit << 4) | vunit) << BACKGROUND_POSITION_SHIFT1);

	style->background_position[0] = hlength;
	style->background_position[1] = vlength;

	return CSS_OK;
}
#undef BACKGROUND_POSITION_SHIFT1
#undef BACKGROUND_POSITION_INDEX1
#undef BACKGROUND_POSITION_MASK
#undef BACKGROUND_POSITION_SHIFT
#undef BACKGROUND_POSITION_INDEX

#define DISPLAY_INDEX 27
#define DISPLAY_SHIFT 2
#define DISPLAY_MASK  0x7c
static inline css_error set_display(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[DISPLAY_INDEX];

	/* 5bits: type */
	*bits = (*bits & ~DISPLAY_MASK) |
			((type & 0x1f) << DISPLAY_SHIFT);

	return CSS_OK;
}
#undef DISPLAY_MASK
#undef DISPLAY_SHIFT
#undef DISPLAY_INDEX

#define FONT_VARIANT_INDEX 27
#define FONT_VARIANT_SHIFT 0
#define FONT_VARIANT_MASK  0x3
static inline css_error set_font_variant(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[FONT_VARIANT_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~FONT_VARIANT_MASK) |
			((type & 0x3) << FONT_VARIANT_SHIFT);

	return CSS_OK;
}
#undef FONT_VARIANT_MASK
#undef FONT_VARIANT_SHIFT
#undef FONT_VARIANT_INDEX

#define TEXT_DECORATION_INDEX 28
#define TEXT_DECORATION_SHIFT 3
#define TEXT_DECORATION_MASK  0xf8
static inline css_error set_text_decoration(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[TEXT_DECORATION_INDEX];

	/* 5bits: type */
	*bits = (*bits & ~TEXT_DECORATION_MASK) |
			((type & 0x1f) << TEXT_DECORATION_SHIFT);

	return CSS_OK;
}
#undef TEXT_DECORATION_MASK
#undef TEXT_DECORATION_SHIFT
#undef TEXT_DECORATION_INDEX

#define FONT_FAMILY_INDEX 28
#define FONT_FAMILY_SHIFT 0
#define FONT_FAMILY_MASK  0x7
static inline css_error set_font_family(
		css_computed_style *style, uint8_t type, 
		lwc_string **names)
{
	uint8_t *bits = &style->bits[FONT_FAMILY_INDEX];
	lwc_string **oldnames = style->font_family;
	lwc_string **s;

	/* 3bits: type */
	*bits = (*bits & ~FONT_FAMILY_MASK) |
			((type & 0x7) << FONT_FAMILY_SHIFT);

	for (s = names; s != NULL && *s != NULL; s++)
		*s = lwc_string_ref(*s);

	style->font_family = names;

	/* Free existing families */
	if (oldnames != NULL) {
		for (s = oldnames; *s != NULL; s++)
			lwc_string_unref(*s);

		if (oldnames != names)
			free(oldnames);
	}

	return CSS_OK;
}
#undef FONT_FAMILY_MASK
#undef FONT_FAMILY_SHIFT
#undef FONT_FAMILY_INDEX

#define BORDER_TOP_STYLE_INDEX 29
#define BORDER_TOP_STYLE_SHIFT 4
#define BORDER_TOP_STYLE_MASK  0xf0
static inline css_error set_border_top_style(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BORDER_TOP_STYLE_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~BORDER_TOP_STYLE_MASK) |
			((type & 0xf) << BORDER_TOP_STYLE_SHIFT);

	return CSS_OK;
}
#undef BORDER_TOP_STYLE_MASK
#undef BORDER_TOP_STYLE_SHIFT
#undef BORDER_TOP_STYLE_INDEX

#define BORDER_RIGHT_STYLE_INDEX 29
#define BORDER_RIGHT_STYLE_SHIFT 0
#define BORDER_RIGHT_STYLE_MASK  0xf
static inline css_error set_border_right_style(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BORDER_RIGHT_STYLE_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~BORDER_RIGHT_STYLE_MASK) |
			((type & 0xf) << BORDER_RIGHT_STYLE_SHIFT);

	return CSS_OK;
}
#undef BORDER_RIGHT_STYLE_MASK
#undef BORDER_RIGHT_STYLE_SHIFT
#undef BORDER_RIGHT_STYLE_INDEX

#define BORDER_BOTTOM_STYLE_INDEX 30
#define BORDER_BOTTOM_STYLE_SHIFT 4
#define BORDER_BOTTOM_STYLE_MASK  0xf0
static inline css_error set_border_bottom_style(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BORDER_BOTTOM_STYLE_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~BORDER_BOTTOM_STYLE_MASK) |
			((type & 0xf) << BORDER_BOTTOM_STYLE_SHIFT);

	return CSS_OK;
}
#undef BORDER_BOTTOM_STYLE_MASK
#undef BORDER_BOTTOM_STYLE_SHIFT
#undef BORDER_BOTTOM_STYLE_INDEX

#define BORDER_LEFT_STYLE_INDEX 30
#define BORDER_LEFT_STYLE_SHIFT 0
#define BORDER_LEFT_STYLE_MASK  0xf
static inline css_error set_border_left_style(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[BORDER_LEFT_STYLE_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~BORDER_LEFT_STYLE_MASK) |
			((type & 0xf) << BORDER_LEFT_STYLE_SHIFT);

	return CSS_OK;
}
#undef BORDER_LEFT_STYLE_MASK
#undef BORDER_LEFT_STYLE_SHIFT
#undef BORDER_LEFT_STYLE_INDEX

#define FONT_WEIGHT_INDEX 31
#define FONT_WEIGHT_SHIFT 4
#define FONT_WEIGHT_MASK  0xf0
static inline css_error set_font_weight(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[FONT_WEIGHT_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~FONT_WEIGHT_MASK) |
			((type & 0xf) << FONT_WEIGHT_SHIFT);

	return CSS_OK;
}
#undef FONT_WEIGHT_MASK
#undef FONT_WEIGHT_SHIFT
#undef FONT_WEIGHT_INDEX

#define LIST_STYLE_TYPE_INDEX 31
#define LIST_STYLE_TYPE_SHIFT 0
#define LIST_STYLE_TYPE_MASK  0xf
static inline css_error set_list_style_type(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[LIST_STYLE_TYPE_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~LIST_STYLE_TYPE_MASK) |
			((type & 0xf) << LIST_STYLE_TYPE_SHIFT);

	return CSS_OK;
}
#undef LIST_STYLE_TYPE_MASK
#undef LIST_STYLE_TYPE_SHIFT
#undef LIST_STYLE_TYPE_INDEX

#define OUTLINE_STYLE_INDEX 32
#define OUTLINE_STYLE_SHIFT 4
#define OUTLINE_STYLE_MASK  0xf0
static inline css_error set_outline_style(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[OUTLINE_STYLE_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~OUTLINE_STYLE_MASK) |
			((type & 0xf) << OUTLINE_STYLE_SHIFT);

	return CSS_OK;
}
#undef OUTLINE_STYLE_MASK
#undef OUTLINE_STYLE_SHIFT
#undef OUTLINE_STYLE_INDEX

#define TABLE_LAYOUT_INDEX 32
#define TABLE_LAYOUT_SHIFT 2
#define TABLE_LAYOUT_MASK  0xc
static inline css_error set_table_layout(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[TABLE_LAYOUT_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~TABLE_LAYOUT_MASK) |
			((type & 0x3) << TABLE_LAYOUT_SHIFT);

	return CSS_OK;
}
#undef TABLE_LAYOUT_MASK
#undef TABLE_LAYOUT_SHIFT
#undef TABLE_LAYOUT_INDEX

#define UNICODE_BIDI_INDEX 32
#define UNICODE_BIDI_SHIFT 0
#define UNICODE_BIDI_MASK  0x3
static inline css_error set_unicode_bidi(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[UNICODE_BIDI_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~UNICODE_BIDI_MASK) |
			((type & 0x3) << UNICODE_BIDI_SHIFT);

	return CSS_OK;
}
#undef UNICODE_BIDI_MASK
#undef UNICODE_BIDI_SHIFT
#undef UNICODE_BIDI_INDEX

#define VISIBILITY_INDEX 33
#define VISIBILITY_SHIFT 6
#define VISIBILITY_MASK  0xc0
static inline css_error set_visibility(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[VISIBILITY_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~VISIBILITY_MASK) |
			((type & 0x3) << VISIBILITY_SHIFT);

	return CSS_OK;
}
#undef VISIBILITY_MASK
#undef VISIBILITY_SHIFT
#undef VISIBILITY_INDEX

#define LIST_STYLE_POSITION_INDEX 33
#define LIST_STYLE_POSITION_SHIFT 4
#define LIST_STYLE_POSITION_MASK  0x30
static inline css_error set_list_style_position(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[LIST_STYLE_POSITION_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~LIST_STYLE_POSITION_MASK) |
			((type & 0x3) << LIST_STYLE_POSITION_SHIFT);

	return CSS_OK;
}
#undef LIST_STYLE_POSITION_MASK
#undef LIST_STYLE_POSITION_SHIFT
#undef LIST_STYLE_POSITION_INDEX

#define TEXT_ALIGN_INDEX 33
#define TEXT_ALIGN_SHIFT 0
#define TEXT_ALIGN_MASK  0xf
static inline uint8_t set_text_align(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits = &style->bits[TEXT_ALIGN_INDEX];

	/* 4bits: type */
	*bits = (*bits & ~TEXT_ALIGN_MASK) |
			((type & 0xf) << TEXT_ALIGN_SHIFT);

	return CSS_OK;
}
#undef TEXT_ALIGN_MASK
#undef TEXT_ALIGN_SHIFT
#undef TEXT_ALIGN_INDEX

#define PAGE_BREAK_AFTER_INDEX 0
#define PAGE_BREAK_AFTER_SHIFT 0
#define PAGE_BREAK_AFTER_MASK 0x7
static inline css_error set_page_break_after(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits;

	if (style->page == NULL) {
		if (type == CSS_PAGE_BREAK_AFTER_AUTO) {
			return CSS_OK;
		}
	}

	ENSURE_PAGE;

	bits = &style->page->bits[PAGE_BREAK_AFTER_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~PAGE_BREAK_AFTER_MASK) |
			((type & 0x7) << PAGE_BREAK_AFTER_SHIFT);

	return CSS_OK;
}
#undef PAGE_BREAK_AFTER_INDEX
#undef PAGE_BREAK_AFTER_SHIFT
#undef PAGE_BREAK_AFTER_MASK

#define PAGE_BREAK_BEFORE_INDEX 0
#define PAGE_BREAK_BEFORE_SHIFT 3
#define PAGE_BREAK_BEFORE_MASK 0x38
static inline css_error set_page_break_before(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits;

	if (style->page == NULL) {
		if (type == CSS_PAGE_BREAK_BEFORE_AUTO) {
			return CSS_OK;
		}
	}
	
	ENSURE_PAGE;
	
	bits = &style->page->bits[PAGE_BREAK_BEFORE_INDEX];

	/* 3bits: type */
	*bits = (*bits & ~PAGE_BREAK_BEFORE_MASK) |
			((type & 0x7) << PAGE_BREAK_BEFORE_SHIFT);

	return CSS_OK;
}
#undef PAGE_BREAK_BEFORE_INDEX
#undef PAGE_BREAK_BEFORE_SHIFT
#undef PAGE_BREAK_BEFORE_MASK

#define PAGE_BREAK_INSIDE_INDEX 0
#define PAGE_BREAK_INSIDE_SHIFT 6
#define PAGE_BREAK_INSIDE_MASK 0xc0
static inline css_error set_page_break_inside(
		css_computed_style *style, uint8_t type)
{
	uint8_t *bits;

	if (style->page == NULL) {
		if (type == CSS_PAGE_BREAK_INSIDE_AUTO) {
			return CSS_OK;
		}
	}
	
	ENSURE_PAGE;

	bits = &style->page->bits[PAGE_BREAK_INSIDE_INDEX];

	/* 2bits: type */
	*bits = (*bits & ~PAGE_BREAK_INSIDE_MASK) |
			((type & 0x3) << PAGE_BREAK_INSIDE_SHIFT);

	return CSS_OK;
}
#undef PAGE_BREAK_INSIDE_INDEX
#undef PAGE_BREAK_INSIDE_SHIFT
#undef PAGE_BREAK_INSIDE_MASK

#define ORPHANS_INDEX 1
#define ORPHANS_SHIFT 0
#define ORPHANS_MASK 0x1
static inline css_error set_orphans(
		css_computed_style *style, uint8_t type, int32_t count)
{
	uint8_t *bits;
	
	if (style->page == NULL) {
		if (type == CSS_ORPHANS_SET && count == 2) {
			return CSS_OK;
		}
	}

	ENSURE_PAGE;
	
	bits = &style->page->bits[ORPHANS_INDEX];
	
	/* 1bit: type */
	*bits = (*bits & ~ORPHANS_MASK) | ((type & 0x1) << ORPHANS_SHIFT);
	
	style->page->orphans = count;
	
	return CSS_OK;
}
#undef ORPHANS_INDEX
#undef ORPHANS_SHIFT
#undef ORPHANS_MASK

#define WIDOWS_INDEX 1
#define WIDOWS_SHIFT 1
#define WIDOWS_MASK 0x2
static inline css_error set_widows(
		css_computed_style *style, uint8_t type, int32_t count)
{
	uint8_t *bits;
	
	if (style->page == NULL) {
		if (type == CSS_WIDOWS_SET && count == 2) {
			return CSS_OK;
		}
	}
	
	ENSURE_PAGE;
	
	bits = &style->page->bits[WIDOWS_INDEX];
	
	/* 1bit: type */
	*bits = (*bits & ~WIDOWS_MASK) | ((type & 0x1) << WIDOWS_SHIFT);
	
	style->page->widows = count;
	
	return CSS_OK;
}
#undef WIDOWS_INDEX
#undef WIDOWS_SHIFT
#undef WIDOWS_MASK

#endif
