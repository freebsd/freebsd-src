/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_css__parse_properties_properties_h_
#define css_css__parse_properties_properties_h_

#include "stylesheet.h"
#include "lex/lex.h"
#include "parse/language.h"
#include "parse/propstrings.h"

/**
 * Type of property handler function
 */
typedef css_error (*css_prop_handler)(css_language *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style *result);

extern const css_prop_handler property_handlers[LAST_PROP + 1 - FIRST_PROP];

css_error css__parse_azimuth(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_background(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_background_attachment(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_background_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_background_image(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_background_position(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_background_repeat(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_bottom(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_bottom_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_bottom_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_bottom_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_color(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_collapse(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_left(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_left_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_left_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_left_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_right(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_right_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_right_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_right_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_spacing(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_top(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_border_top_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_top_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_top_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_border_width(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_bottom(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_break_after(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_break_before(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_break_inside(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_caption_side(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_clear(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_clip(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_columns(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_count(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_fill(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_gap(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_rule(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_rule_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_rule_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_rule_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_span(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_column_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_content(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_counter_increment(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_counter_reset(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_cue(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_cue_after(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_cue_before(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_cursor(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_direction(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_display(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_elevation(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_empty_cells(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_float(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_font(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_font_family(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_font_size(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_font_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_font_variant(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_font_weight(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_height(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_left(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_letter_spacing(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_line_height(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_list_style(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_list_style_image(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_list_style_position(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_list_style_type(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_margin(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_margin_bottom(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_margin_left(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_margin_right(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_margin_top(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_max_height(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_max_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_min_height(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_min_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_opacity(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_orphans(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_outline(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_outline_color(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_outline_style(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_outline_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_overflow(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_padding(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_padding_bottom(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_padding_left(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_padding_right(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_padding_top(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_page_break_after(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_page_break_before(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_page_break_inside(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_pause(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_pause_after(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_pause_before(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_pitch_range(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_pitch(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_play_during(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_position(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_quotes(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_richness(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_right(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_speak_header(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_speak_numeral(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_speak_punctuation(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_speak(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_speech_rate(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_stress(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_table_layout(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_text_align(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_text_decoration(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_text_indent(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_text_transform(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_top(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_unicode_bidi(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_vertical_align(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_visibility(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_voice_family(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_volume(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_white_space(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_widows(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_width(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_word_spacing(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);
css_error css__parse_writing_mode(css_language *c,
		const parserutils_vector *vector, int *ctx,
		css_style *result);
css_error css__parse_z_index(css_language *c,
		const parserutils_vector *vector, int *ctx, 
		css_style *result);

#endif

