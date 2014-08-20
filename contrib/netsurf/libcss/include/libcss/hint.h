/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef libcss_hint_h_
#define libcss_hint_h_

#ifdef __cplusplus
extern "C"
{
#endif

#include <libwapcaplet/libwapcaplet.h>

#include <libcss/computed.h>
#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/types.h>

/**
 * Length object for use in presentational hints
 */
typedef struct css_hint_length {
	css_fixed value;
	css_unit unit;
} css_hint_length;

/**
 * Presentational hints
 */
typedef struct css_hint {
	/* Ownership of all data is passed to libcss */
	union {
		css_computed_clip_rect *clip;
		css_color color;
		css_computed_content_item *content;
		css_computed_counter *counter;
		css_fixed fixed;
		int32_t integer;
		css_hint_length length;
		struct {
			css_hint_length h;
			css_hint_length v;
		} position;
		lwc_string *string;
		lwc_string **strings;
	} data;

	uint8_t status;
} css_hint;

#ifdef __cplusplus
}
#endif

#endif
