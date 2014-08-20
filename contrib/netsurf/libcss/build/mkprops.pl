#!/usr/bin/perl

use warnings;
use strict;

# Auto-generate stub handlers for CSS properties.

my @PROPS = split(/ /, "azimuth background_attachment background_color background_image background_position background_repeat border_bottom_color border_bottom_style border_bottom_width border_collapse border_left_color border_left_style border_left_width border_right_color border_right_style border_right_width border_spacing border_top_color border_top_style border_top_width bottom caption_side clear clip color content counter_increment counter_reset cue_after cue_before cursor direction display elevation empty_cells float font_family font_size font_style font_variant font_weight height left letter_spacing line_height list_style_image list_style_position list_style_type margin_bottom margin_left margin_right margin_top max_height max_width min_height min_width orphans outline_color outline_style outline_width overflow padding_bottom padding_left padding_right padding_top page_break_after page_break_before page_break_inside pause_after pause_before pitch_range pitch play_during position quotes richness right speak_header speak_numeral speak_punctuation speak speech_rate stress table_layout text_align text_decoration text_indent text_transform top unicode_bidi vertical_align visibility voice_family volume white_space widows width word_spacing z_index");

print <<EOF
/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb\@netsurf-browser.org>
 */

#ifndef css_parse_css21props_c_
#define css_parse_css21props_c_

EOF
;

foreach my $prop (@PROPS) {
print <<EOF
static css_error parse_$prop(css_css21 *c,
		const parserutils_vector *vector, int *ctx, 
		css_style **result);
EOF
}

print <<EOF

/**
 * Type of property handler function
 */
typedef css_error (*css_prop_handler)(css_css21 *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result);

/**
 * Dispatch table of property handlers, indexed by property enum
 */
static const css_prop_handler property_handlers[LAST_KNOWN - FIRST_PROP] =
{
EOF
;

foreach my $prop (@PROPS) {
	print "\tparse_$prop,\n";
}

print "};\n";

foreach my $prop (@PROPS) {
print <<EOF

css_error parse_$prop(css_css21 *c, 
		const parserutils_vector *vector, int *ctx, 
		css_style **result)
{
	UNUSED(c);
	UNUSED(vector);
	UNUSED(ctx);
	UNUSED(result);

	return CSS_OK;
}
EOF
}

print "\n#endif\n";

