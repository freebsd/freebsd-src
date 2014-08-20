/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "css/dump.h"

static void dump_css_fixed(FILE *stream, css_fixed f);
static void dump_css_number(FILE *stream, css_fixed val);
static void dump_css_unit(FILE *stream, css_fixed val, css_unit unit);

/**
 * Dump a computed style \a style to the give file handle \a stream.
 *
 * \param stream  Stream to write to
 * \param style   Computed style to dump
 */
void nscss_dump_computed_style(FILE *stream, const css_computed_style *style)
{
	uint8_t val;
	css_color color = 0;
	lwc_string *url = NULL;
	css_fixed len1 = 0, len2 = 0;
	css_unit unit1 = CSS_UNIT_PX, unit2 = CSS_UNIT_PX;
	css_computed_clip_rect rect = { 0, 0, 0, 0, CSS_UNIT_PX, CSS_UNIT_PX,
					CSS_UNIT_PX, CSS_UNIT_PX, true, true,
					true, true };
	const css_computed_content_item *content = NULL;
	const css_computed_counter *counter = NULL;
	lwc_string **string_list = NULL;
	int32_t zindex = 0;

	fprintf(stream, "{ ");

	/* background-attachment */
	val = css_computed_background_attachment(style);
	switch (val) {
	case CSS_BACKGROUND_ATTACHMENT_FIXED:
		fprintf(stream, "background-attachment: fixed ");
		break;
	case CSS_BACKGROUND_ATTACHMENT_SCROLL:
		fprintf(stream, "background-attachment: scroll ");
		break;
	default:
		break;
	}

	/* background-color */
	val = css_computed_background_color(style, &color);
	switch (val) {
	case CSS_BACKGROUND_COLOR_COLOR:
		fprintf(stream, "background-color: #%08x ", color);
		break;
	default:
		break;
	}

	/* background-image */
	val = css_computed_background_image(style, &url);
	if (val == CSS_BACKGROUND_IMAGE_IMAGE && url != NULL) {
		fprintf(stream, "background-image: url('%.*s') ",
				(int) lwc_string_length(url), 
				lwc_string_data(url));
	} else if (val == CSS_BACKGROUND_IMAGE_NONE) {
		fprintf(stream, "background-image: none ");
	}

	/* background-position */
	val = css_computed_background_position(style, &len1, &unit1,
			&len2, &unit2);
	if (val == CSS_BACKGROUND_POSITION_SET) {
		fprintf(stream, "background-position: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		dump_css_unit(stream, len2, unit2);
		fprintf(stream, " ");
	}

	/* background-repeat */
	val = css_computed_background_repeat(style);
	switch (val) {
	case CSS_BACKGROUND_REPEAT_REPEAT_X:
		fprintf(stream, "background-repeat: repeat-x ");
		break;
	case CSS_BACKGROUND_REPEAT_REPEAT_Y:
		fprintf(stream, "background-repeat: repeat-y ");
		break;
	case CSS_BACKGROUND_REPEAT_REPEAT:
		fprintf(stream, "background-repeat: repeat ");
		break;
	case CSS_BACKGROUND_REPEAT_NO_REPEAT:
		fprintf(stream, "background-repeat: no-repeat ");
		break;
	default:
		break;
	}

	/* border-collapse */
	val = css_computed_border_collapse(style);
	switch (val) {
	case CSS_BORDER_COLLAPSE_SEPARATE:
		fprintf(stream, "border-collapse: separate ");
		break;
	case CSS_BORDER_COLLAPSE_COLLAPSE:
		fprintf(stream, "border-collapse: collapse ");
		break;
	default:

		break;
	}

	/* border-spacing */
	val = css_computed_border_spacing(style, &len1, &unit1, &len2, &unit2);
	if (val == CSS_BORDER_SPACING_SET) {
		fprintf(stream, "border-spacing: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		dump_css_unit(stream, len2, unit2);
		fprintf(stream, " ");
	}

	/* border-top-color */
	val = css_computed_border_top_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_COLOR:
		fprintf(stream, "border-top-color: #%08x ", color);
		break;
	default:
		break;
	}

	/* border-right-color */
	val = css_computed_border_right_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_COLOR:
		fprintf(stream, "border-right-color: #%08x ", color);
		break;
	default:
		break;
	}

	/* border-bottom-color */
	val = css_computed_border_bottom_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_COLOR:
		fprintf(stream, "border-bottom-color: #%08x ", color);
		break;
	default:
		break;
	}

	/* border-left-color */
	val = css_computed_border_left_color(style, &color);
	switch (val) {
	case CSS_BORDER_COLOR_COLOR:
		fprintf(stream, "border-left-color: #%08x ", color);
		break;
	default:
		break;
	}

	/* border-top-style */
	val = css_computed_border_top_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_NONE:
		fprintf(stream, "border-top-style: none ");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		fprintf(stream, "border-top-style: hidden ");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		fprintf(stream, "border-top-style: dotted ");
		break;
	case CSS_BORDER_STYLE_DASHED:
		fprintf(stream, "border-top-style: dashed ");
		break;
	case CSS_BORDER_STYLE_SOLID:
		fprintf(stream, "border-top-style: solid ");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		fprintf(stream, "border-top-style: double ");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		fprintf(stream, "border-top-style: groove ");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		fprintf(stream, "border-top-style: ridge ");
		break;
	case CSS_BORDER_STYLE_INSET:
		fprintf(stream, "border-top-style: inset ");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		fprintf(stream, "border-top-style: outset ");
		break;
	default:
		break;
	}

	/* border-right-style */
	val = css_computed_border_right_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_NONE:
		fprintf(stream, "border-right-style: none ");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		fprintf(stream, "border-right-style: hidden ");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		fprintf(stream, "border-right-style: dotted ");
		break;
	case CSS_BORDER_STYLE_DASHED:
		fprintf(stream, "border-right-style: dashed ");
		break;
	case CSS_BORDER_STYLE_SOLID:
		fprintf(stream, "border-right-style: solid ");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		fprintf(stream, "border-right-style: double ");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		fprintf(stream, "border-right-style: groove ");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		fprintf(stream, "border-right-style: ridge ");
		break;
	case CSS_BORDER_STYLE_INSET:
		fprintf(stream, "border-right-style: inset ");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		fprintf(stream, "border-right-style: outset ");
		break;
	default:
		break;
	}

	/* border-bottom-style */
	val = css_computed_border_bottom_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_NONE:
		fprintf(stream, "border-bottom-style: none ");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		fprintf(stream, "border-bottom-style: hidden ");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		fprintf(stream, "border-bottom-style: dotted ");
		break;
	case CSS_BORDER_STYLE_DASHED:
		fprintf(stream, "border-bottom-style: dashed ");
		break;
	case CSS_BORDER_STYLE_SOLID:
		fprintf(stream, "border-bottom-style: solid ");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		fprintf(stream, "border-bottom-style: double ");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		fprintf(stream, "border-bottom-style: groove ");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		fprintf(stream, "border-bottom-style: ridge ");
		break;
	case CSS_BORDER_STYLE_INSET:
		fprintf(stream, "border-bottom-style: inset ");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		fprintf(stream, "border-bottom-style: outset ");
		break;
	default:
		break;
	}

	/* border-left-style */
	val = css_computed_border_left_style(style);
	switch (val) {
	case CSS_BORDER_STYLE_NONE:
		fprintf(stream, "border-left-style: none ");
		break;
	case CSS_BORDER_STYLE_HIDDEN:
		fprintf(stream, "border-left-style: hidden ");
		break;
	case CSS_BORDER_STYLE_DOTTED:
		fprintf(stream, "border-left-style: dotted ");
		break;
	case CSS_BORDER_STYLE_DASHED:
		fprintf(stream, "border-left-style: dashed ");
		break;
	case CSS_BORDER_STYLE_SOLID:
		fprintf(stream, "border-left-style: solid ");
		break;
	case CSS_BORDER_STYLE_DOUBLE:
		fprintf(stream, "border-left-style: double ");
		break;
	case CSS_BORDER_STYLE_GROOVE:
		fprintf(stream, "border-left-style: groove ");
		break;
	case CSS_BORDER_STYLE_RIDGE:
		fprintf(stream, "border-left-style: ridge ");
		break;
	case CSS_BORDER_STYLE_INSET:
		fprintf(stream, "border-left-style: inset ");
		break;
	case CSS_BORDER_STYLE_OUTSET:
		fprintf(stream, "border-left-style: outset ");
		break;
	default:
		break;
	}

	/* border-top-width */
	val = css_computed_border_top_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_THIN:
		fprintf(stream, "border-top-width: thin ");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		fprintf(stream, "border-top-width: medium ");
		break;
	case CSS_BORDER_WIDTH_THICK:
		fprintf(stream, "border-top-width: thick ");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		fprintf(stream, "border-top-width: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* border-right-width */
	val = css_computed_border_right_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_THIN:
		fprintf(stream, "border-right-width: thin ");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		fprintf(stream, "border-right-width: medium ");
		break;
	case CSS_BORDER_WIDTH_THICK:
		fprintf(stream, "border-right-width: thick ");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		fprintf(stream, "border-right-width: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* border-bottom-width */
	val = css_computed_border_bottom_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_THIN:
		fprintf(stream, "border-bottom-width: thin ");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		fprintf(stream, "border-bottom-width: medium ");
		break;
	case CSS_BORDER_WIDTH_THICK:
		fprintf(stream, "border-bottom-width: thick ");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		fprintf(stream, "border-bottom-width: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* border-left-width */
	val = css_computed_border_left_width(style, &len1, &unit1);
	switch (val) {
	case CSS_BORDER_WIDTH_THIN:
		fprintf(stream, "border-left-width: thin ");
		break;
	case CSS_BORDER_WIDTH_MEDIUM:
		fprintf(stream, "border-left-width: medium ");
		break;
	case CSS_BORDER_WIDTH_THICK:
		fprintf(stream, "border-left-width: thick ");
		break;
	case CSS_BORDER_WIDTH_WIDTH:
		fprintf(stream, "border-left-width: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* bottom */
	val = css_computed_bottom(style, &len1, &unit1);
	switch (val) {
	case CSS_BOTTOM_AUTO:
		fprintf(stream, "bottom: auto ");
		break;
	case CSS_BOTTOM_SET:
		fprintf(stream, "bottom: ");
		dump_css_unit(stream, len1, unit1);
		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* caption-side */
	val = css_computed_caption_side(style);
	switch (val) {
	case CSS_CAPTION_SIDE_TOP:
		fprintf(stream, "caption_side: top ");
		break;
	case CSS_CAPTION_SIDE_BOTTOM:
		fprintf(stream, "caption_side: bottom ");
		break;
	default:
		break;
	}

	/* clear */
	val = css_computed_clear(style);
	switch (val) {
	case CSS_CLEAR_NONE:
		fprintf(stream, "clear: none ");
		break;
	case CSS_CLEAR_LEFT:
		fprintf(stream, "clear: left ");
		break;
	case CSS_CLEAR_RIGHT:
		fprintf(stream, "clear: right ");
		break;
	case CSS_CLEAR_BOTH:
		fprintf(stream, "clear: both ");
		break;
	default:
		break;
	}

	/* clip */
	val = css_computed_clip(style, &rect);
	switch (val) {
	case CSS_CLIP_AUTO:
		fprintf(stream, "clip: auto ");
		break;
	case CSS_CLIP_RECT:
		fprintf(stream, "clip: rect( ");

		if (rect.top_auto)
			fprintf(stream, "auto");
		else
			dump_css_unit(stream, rect.top, rect.tunit);
		fprintf(stream, ", ");

		if (rect.right_auto)
			fprintf(stream, "auto");
		else
			dump_css_unit(stream, rect.right, rect.runit);
		fprintf(stream, ", ");

		if (rect.bottom_auto)
			fprintf(stream, "auto");
		else
			dump_css_unit(stream, rect.bottom, rect.bunit);
		fprintf(stream, ", ");

		if (rect.left_auto)
			fprintf(stream, "auto");
		else
			dump_css_unit(stream, rect.left, rect.lunit);
		fprintf(stream, ") ");
		break;
	default:
		break;
	}

	/* color */
	val = css_computed_color(style, &color);
	if (val == CSS_COLOR_COLOR) {
		fprintf(stream, "color: #%08x ", color);
	}

	/* content */
	val = css_computed_content(style, &content);
	switch (val) {
	case CSS_CONTENT_NONE:
		fprintf(stream, "content: none ");
		break;
	case CSS_CONTENT_NORMAL:
		fprintf(stream, "content: normal ");
		break;
	case CSS_CONTENT_SET:
		fprintf(stream, "content:");

		while (content->type != CSS_COMPUTED_CONTENT_NONE) {
			fprintf(stream, " ");

			switch (content->type) {
			case CSS_COMPUTED_CONTENT_STRING:
				fprintf(stream,	"\"%.*s\"",
						(int) lwc_string_length(
						content->data.string),
						lwc_string_data(
						content->data.string));
				break;
			case CSS_COMPUTED_CONTENT_URI:
				fprintf(stream,	"uri(\"%.*s\")",
						(int) lwc_string_length(
						content->data.uri),
						lwc_string_data(
						content->data.uri));
				break;
			case CSS_COMPUTED_CONTENT_COUNTER:
				fprintf(stream, "counter(%.*s)",
						(int) lwc_string_length(
						content->data.counter.name),
						lwc_string_data(
						content->data.counter.name));
				break;
			case CSS_COMPUTED_CONTENT_COUNTERS:
				fprintf(stream, "counters(%.*s, \"%.*s\")",
						(int) lwc_string_length(
						content->data.counters.name),
						lwc_string_data(
						content->data.counters.name),
						(int) lwc_string_length(
						content->data.counters.sep),
						lwc_string_data(
						content->data.counters.sep));
				break;
			case CSS_COMPUTED_CONTENT_ATTR:
				fprintf(stream, "attr(%.*s)",
						(int) lwc_string_length(
						content->data.attr),
						lwc_string_data(
						content->data.attr));
				break;
			case CSS_COMPUTED_CONTENT_OPEN_QUOTE:
				fprintf(stream, "open-quote");
				break;
			case CSS_COMPUTED_CONTENT_CLOSE_QUOTE:
				fprintf(stream, "close-quote");
				break;
			case CSS_COMPUTED_CONTENT_NO_OPEN_QUOTE:
				fprintf(stream, "no-open-quote");
				break;
			case CSS_COMPUTED_CONTENT_NO_CLOSE_QUOTE:
				fprintf(stream, "no-close-quote");
				break;
			}

			content++;
		}

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* counter-increment */
	val = css_computed_counter_increment(style, &counter);
	if ((val == CSS_COUNTER_INCREMENT_NONE) || (counter == NULL)) {
		fprintf(stream, "counter-increment: none ");
	} else {
		fprintf(stream, "counter-increment:");

		while (counter->name != NULL) {
			fprintf(stream, " %.*s ",
					(int) lwc_string_length(counter->name),
					lwc_string_data(counter->name));

			dump_css_fixed(stream, counter->value);

			counter++;
		}

		fprintf(stream, " ");
	}

	/* counter-reset */
	val = css_computed_counter_reset(style, &counter);
	if ((val == CSS_COUNTER_RESET_NONE) || (counter == NULL)) {
		fprintf(stream, "counter-reset: none ");
	} else {
		fprintf(stream, "counter-reset:");

		while (counter->name != NULL) {
			fprintf(stream, " %.*s ",
					(int) lwc_string_length(counter->name),
					lwc_string_data(counter->name));

			dump_css_fixed(stream, counter->value);

			counter++;
		}

		fprintf(stream, " ");
	}

	/* cursor */
	val = css_computed_cursor(style, &string_list);
	fprintf(stream, "cursor:");

	if (string_list != NULL) {
		while (*string_list != NULL) {
			fprintf(stream, " url\"%.*s\")",
					(int) lwc_string_length(*string_list),
					lwc_string_data(*string_list));

			string_list++;
		}
	}
	switch (val) {
	case CSS_CURSOR_AUTO:
		fprintf(stream, " auto ");
		break;
	case CSS_CURSOR_CROSSHAIR:
		fprintf(stream, " crosshair ");
		break;
	case CSS_CURSOR_DEFAULT:
		fprintf(stream, " default ");
		break;
	case CSS_CURSOR_POINTER:
		fprintf(stream, " pointer ");
		break;
	case CSS_CURSOR_MOVE:
		fprintf(stream, " move ");
		break;
	case CSS_CURSOR_E_RESIZE:
		fprintf(stream, " e-resize ");
		break;
	case CSS_CURSOR_NE_RESIZE:
		fprintf(stream, " ne-resize ");
		break;
	case CSS_CURSOR_NW_RESIZE:
		fprintf(stream, " nw-resize ");
		break;
	case CSS_CURSOR_N_RESIZE:
		fprintf(stream, " n-resize ");
		break;
	case CSS_CURSOR_SE_RESIZE:
		fprintf(stream, " se-resize ");
		break;
	case CSS_CURSOR_SW_RESIZE:
		fprintf(stream, " sw-resize ");
		break;
	case CSS_CURSOR_S_RESIZE:
		fprintf(stream, " s-resize ");
		break;
	case CSS_CURSOR_W_RESIZE:
		fprintf(stream, " w-resize ");
		break;
	case CSS_CURSOR_TEXT:
		fprintf(stream, " text ");
		break;
	case CSS_CURSOR_WAIT:
		fprintf(stream, " wait ");
		break;
	case CSS_CURSOR_HELP:
		fprintf(stream, " help ");
		break;
	case CSS_CURSOR_PROGRESS:
		fprintf(stream, " progress ");
		break;
	default:
		break;
	}

	/* direction */
	val = css_computed_direction(style);
	switch (val) {
	case CSS_DIRECTION_LTR:
		fprintf(stream, "direction: ltr ");
		break;
	case CSS_DIRECTION_RTL:
		fprintf(stream, "direction: rtl ");
		break;
	default:
		break;
	}

	/* display */
	val = css_computed_display_static(style);
	switch (val) {
	case CSS_DISPLAY_INLINE:
		fprintf(stream, "display: inline ");
		break;
	case CSS_DISPLAY_BLOCK:
		fprintf(stream, "display: block ");
		break;
	case CSS_DISPLAY_LIST_ITEM:
		fprintf(stream, "display: list-item ");
		break;
	case CSS_DISPLAY_RUN_IN:
		fprintf(stream, "display: run-in ");
		break;
	case CSS_DISPLAY_INLINE_BLOCK:
		fprintf(stream, "display: inline-block ");
		break;
	case CSS_DISPLAY_TABLE:
		fprintf(stream, "display: table ");
		break;
	case CSS_DISPLAY_INLINE_TABLE:
		fprintf(stream, "display: inline-table ");
		break;
	case CSS_DISPLAY_TABLE_ROW_GROUP:
		fprintf(stream, "display: table-row-group ");
		break;
	case CSS_DISPLAY_TABLE_HEADER_GROUP:
		fprintf(stream, "display: table-header-group ");
		break;
	case CSS_DISPLAY_TABLE_FOOTER_GROUP:
		fprintf(stream, "display: table-footer-group ");
		break;
	case CSS_DISPLAY_TABLE_ROW:
		fprintf(stream, "display: table-row ");
		break;
	case CSS_DISPLAY_TABLE_COLUMN_GROUP:
		fprintf(stream, "display: table-column-group ");
		break;
	case CSS_DISPLAY_TABLE_COLUMN:
		fprintf(stream, "display: table-column ");
		break;
	case CSS_DISPLAY_TABLE_CELL:
		fprintf(stream, "display: table-cell ");
		break;
	case CSS_DISPLAY_TABLE_CAPTION:
		fprintf(stream, "display: table-caption ");
		break;
	case CSS_DISPLAY_NONE:
		fprintf(stream, "display: none ");
		break;
	default:
		break;
	}

	/* empty-cells */
	val = css_computed_empty_cells(style);
	switch (val) {
	case CSS_EMPTY_CELLS_SHOW:
		fprintf(stream, "empty-cells: show ");
		break;
	case CSS_EMPTY_CELLS_HIDE:
		fprintf(stream, "empty-cells: hide ");
		break;
	default:
		break;
	}

	/* float */
	val = css_computed_float(style);
	switch (val) {
	case CSS_FLOAT_LEFT:
		fprintf(stream, "float: left ");
		break;
	case CSS_FLOAT_RIGHT:
		fprintf(stream, "float: right ");
		break;
	case CSS_FLOAT_NONE:
		fprintf(stream, "float: none ");
		break;
	default:
		break;
	}

	/* font-family */
	val = css_computed_font_family(style, &string_list);
	if (val != CSS_FONT_FAMILY_INHERIT) {
		fprintf(stream, "font-family:");

		if (string_list != NULL) {
			while (*string_list != NULL) {
				fprintf(stream, " \"%.*s\"",
					(int) lwc_string_length(*string_list),
					lwc_string_data(*string_list));

				string_list++;
			}
		}
		switch (val) {
		case CSS_FONT_FAMILY_SERIF:
			fprintf(stream, " serif ");
			break;
		case CSS_FONT_FAMILY_SANS_SERIF:
			fprintf(stream, " sans-serif ");
			break;
		case CSS_FONT_FAMILY_CURSIVE:
			fprintf(stream, " cursive ");
			break;
		case CSS_FONT_FAMILY_FANTASY:
			fprintf(stream, " fantasy ");
			break;
		case CSS_FONT_FAMILY_MONOSPACE:
			fprintf(stream, " monospace ");
			break;
		}
	}

	/* font-size */
	val = css_computed_font_size(style, &len1, &unit1);
	switch (val) {
	case CSS_FONT_SIZE_XX_SMALL:
		fprintf(stream, "font-size: xx-small ");
		break;
	case CSS_FONT_SIZE_X_SMALL:
		fprintf(stream, "font-size: x-small ");
		break;
	case CSS_FONT_SIZE_SMALL:
		fprintf(stream, "font-size: small ");
		break;
	case CSS_FONT_SIZE_MEDIUM:
		fprintf(stream, "font-size: medium ");
		break;
	case CSS_FONT_SIZE_LARGE:
		fprintf(stream, "font-size: large ");
		break;
	case CSS_FONT_SIZE_X_LARGE:
		fprintf(stream, "font-size: x-large ");
		break;
	case CSS_FONT_SIZE_XX_LARGE:
		fprintf(stream, "font-size: xx-large ");
		break;
	case CSS_FONT_SIZE_LARGER:
		fprintf(stream, "font-size: larger ");
		break;
	case CSS_FONT_SIZE_SMALLER:
		fprintf(stream, "font-size: smaller ");
		break;
	case CSS_FONT_SIZE_DIMENSION:
		fprintf(stream, "font-size: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* font-style */
	val = css_computed_font_style(style);
	switch (val) {
	case CSS_FONT_STYLE_NORMAL:
		fprintf(stream, "font-style: normal ");
		break;
	case CSS_FONT_STYLE_ITALIC:
		fprintf(stream, "font-style: italic ");
		break;
	case CSS_FONT_STYLE_OBLIQUE:
		fprintf(stream, "font-style: oblique ");
		break;
	default:
		break;
	}

	/* font-variant */
	val = css_computed_font_variant(style);
	switch (val) {
	case CSS_FONT_VARIANT_NORMAL:
		fprintf(stream, "font-variant: normal ");
		break;
	case CSS_FONT_VARIANT_SMALL_CAPS:
		fprintf(stream, "font-variant: small-caps ");
		break;
	default:
		break;
	}

	/* font-weight */
	val = css_computed_font_weight(style);
	switch (val) {
	case CSS_FONT_WEIGHT_NORMAL:
		fprintf(stream, "font-weight: normal ");
		break;
	case CSS_FONT_WEIGHT_BOLD:
		fprintf(stream, "font-weight: bold ");
		break;
	case CSS_FONT_WEIGHT_BOLDER:
		fprintf(stream, "font-weight: bolder ");
		break;
	case CSS_FONT_WEIGHT_LIGHTER:
		fprintf(stream, "font-weight: lighter ");
		break;
	case CSS_FONT_WEIGHT_100:
		fprintf(stream, "font-weight: 100 ");
		break;
	case CSS_FONT_WEIGHT_200:
		fprintf(stream, "font-weight: 200 ");
		break;
	case CSS_FONT_WEIGHT_300:
		fprintf(stream, "font-weight: 300 ");
		break;
	case CSS_FONT_WEIGHT_400:
		fprintf(stream, "font-weight: 400 ");
		break;
	case CSS_FONT_WEIGHT_500:
		fprintf(stream, "font-weight: 500 ");
		break;
	case CSS_FONT_WEIGHT_600:
		fprintf(stream, "font-weight: 600 ");
		break;
	case CSS_FONT_WEIGHT_700:
		fprintf(stream, "font-weight: 700 ");
		break;
	case CSS_FONT_WEIGHT_800:
		fprintf(stream, "font-weight: 800 ");
		break;
	case CSS_FONT_WEIGHT_900:
		fprintf(stream, "font-weight: 900 ");
		break;
	default:
		break;
	}

	/* height */
	val = css_computed_height(style, &len1, &unit1);
	switch (val) {
	case CSS_HEIGHT_AUTO:
		fprintf(stream, "height: auto ");
		break;
	case CSS_HEIGHT_SET:
		fprintf(stream, "height: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* left */
	val = css_computed_left(style, &len1, &unit1);
	switch (val) {
	case CSS_LEFT_AUTO:
		fprintf(stream, "left: auto ");
		break;
	case CSS_LEFT_SET:
		fprintf(stream, "left: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* letter-spacing */
	val = css_computed_letter_spacing(style, &len1, &unit1);
	switch (val) {
	case CSS_LETTER_SPACING_NORMAL:
		fprintf(stream, "letter-spacing: normal ");
		break;
	case CSS_LETTER_SPACING_SET:
		fprintf(stream, "letter-spacing: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* line-height */
	val = css_computed_line_height(style, &len1, &unit1);
	switch (val) {
	case CSS_LINE_HEIGHT_NORMAL:
		fprintf(stream, "line-height: normal ");
		break;
	case CSS_LINE_HEIGHT_NUMBER:
		fprintf(stream, "line-height: ");

		dump_css_fixed(stream, len1);

		fprintf(stream, " ");
		break;
	case CSS_LINE_HEIGHT_DIMENSION:
		fprintf(stream, "line-height: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* list-style-image */
	val = css_computed_list_style_image(style, &url);
	if (url != NULL) {
		fprintf(stream, "list-style-image: url('%.*s') ",
				(int) lwc_string_length(url), 
				lwc_string_data(url));
	} else if (val == CSS_LIST_STYLE_IMAGE_NONE) {
		fprintf(stream, "list-style-image: none ");
	}

	/* list-style-position */
	val = css_computed_list_style_position(style);
	switch (val) {
	case CSS_LIST_STYLE_POSITION_INSIDE:
		fprintf(stream, "list-style-position: inside ");
		break;
	case CSS_LIST_STYLE_POSITION_OUTSIDE:
		fprintf(stream, "list-style-position: outside ");
		break;
	default:
		break;
	}

	/* list-style-type */
	val = css_computed_list_style_type(style);
	switch (val) {
	case CSS_LIST_STYLE_TYPE_DISC:
		fprintf(stream, "list-style-type: disc ");
		break;
	case CSS_LIST_STYLE_TYPE_CIRCLE:
		fprintf(stream, "list-style-type: circle ");
		break;
	case CSS_LIST_STYLE_TYPE_SQUARE:
		fprintf(stream, "list-style-type: square ");
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL:
		fprintf(stream, "list-style-type: decimal ");
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		fprintf(stream, "list-style-type: decimal-leading-zero ");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
		fprintf(stream, "list-style-type: lower-roman ");
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
		fprintf(stream, "list-style-type: upper-roman ");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_GREEK:
		fprintf(stream, "list-style-type: lower-greek ");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_LATIN:
		fprintf(stream, "list-style-type: lower-latin ");
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_LATIN:
		fprintf(stream, "list-style-type: upper-latin ");
		break;
	case CSS_LIST_STYLE_TYPE_ARMENIAN:
		fprintf(stream, "list-style-type: armenian ");
		break;
	case CSS_LIST_STYLE_TYPE_GEORGIAN:
		fprintf(stream, "list-style-type: georgian ");
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
		fprintf(stream, "list-style-type: lower-alpha ");
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
		fprintf(stream, "list-style-type: upper-alpha ");
		break;
	case CSS_LIST_STYLE_TYPE_NONE:
		fprintf(stream, "list-style-type: none ");
		break;
	default:
		break;
	}

	/* margin-top */
	val = css_computed_margin_top(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_AUTO:
		fprintf(stream, "margin-top: auto ");
		break;
	case CSS_MARGIN_SET:
		fprintf(stream, "margin-top: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* margin-right */
	val = css_computed_margin_right(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_AUTO:
		fprintf(stream, "margin-right: auto ");
		break;
	case CSS_MARGIN_SET:
		fprintf(stream, "margin-right: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* margin-bottom */
	val = css_computed_margin_bottom(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_AUTO:
		fprintf(stream, "margin-bottom: auto ");
		break;
	case CSS_MARGIN_SET:
		fprintf(stream, "margin-bottom: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* margin-left */
	val = css_computed_margin_left(style, &len1, &unit1);
	switch (val) {
	case CSS_MARGIN_AUTO:
		fprintf(stream, "margin-left: auto ");
		break;
	case CSS_MARGIN_SET:
		fprintf(stream, "margin-left: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* max-height */
	val = css_computed_max_height(style, &len1, &unit1);
	switch (val) {
	case CSS_MAX_HEIGHT_NONE:
		fprintf(stream, "max-height: none ");
		break;
	case CSS_MAX_HEIGHT_SET:
		fprintf(stream, "max-height: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* max-width */
	val = css_computed_max_width(style, &len1, &unit1);
	switch (val) {
	case CSS_MAX_WIDTH_NONE:
		fprintf(stream, "max-width: none ");
		break;
	case CSS_MAX_WIDTH_SET:
		fprintf(stream, "max-width: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* min-height */
	val = css_computed_min_height(style, &len1, &unit1);
	switch (val) {
	case CSS_MIN_HEIGHT_SET:
		fprintf(stream, "min-height: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* min-width */
	val = css_computed_min_width(style, &len1, &unit1);
	switch (val) {
	case CSS_MIN_WIDTH_SET:
		fprintf(stream, "min-width: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* opacity */
	val = css_computed_opacity(style, &len1);
	switch (val) {
	case CSS_OPACITY_SET:
		fprintf(stream, "opacity: ");

		dump_css_fixed(stream, len1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* outline-color */
	val = css_computed_outline_color(style, &color);
	switch (val) {
	case CSS_OUTLINE_COLOR_INVERT:
		fprintf(stream, "outline-color: invert ");
		break;
	case CSS_OUTLINE_COLOR_COLOR:
		fprintf(stream, "outline-color: #%08x ", color);
		break;
	default:
		break;
	}

	/* outline-style */
	val = css_computed_outline_style(style);
	switch (val) {
	case CSS_OUTLINE_STYLE_NONE:
		fprintf(stream, "outline-style: none ");
		break;
	case CSS_OUTLINE_STYLE_DOTTED:
		fprintf(stream, "outline-style: dotted ");
		break;
	case CSS_OUTLINE_STYLE_DASHED:
		fprintf(stream, "outline-style: dashed ");
		break;
	case CSS_OUTLINE_STYLE_SOLID:
		fprintf(stream, "outline-style: solid ");
		break;
	case CSS_OUTLINE_STYLE_DOUBLE:
		fprintf(stream, "outline-style: double ");
		break;
	case CSS_OUTLINE_STYLE_GROOVE:
		fprintf(stream, "outline-style: groove ");
		break;
	case CSS_OUTLINE_STYLE_RIDGE:
		fprintf(stream, "outline-style: ridge ");
		break;
	case CSS_OUTLINE_STYLE_INSET:
		fprintf(stream, "outline-style: inset ");
		break;
	case CSS_OUTLINE_STYLE_OUTSET:
		fprintf(stream, "outline-style: outset ");
		break;
	default:
		break;
	}

	/* outline-width */
	val = css_computed_outline_width(style, &len1, &unit1);
	switch (val) {
	case CSS_OUTLINE_WIDTH_THIN:
		fprintf(stream, "outline-width: thin ");
		break;
	case CSS_OUTLINE_WIDTH_MEDIUM:
		fprintf(stream, "outline-width: medium ");
		break;
	case CSS_OUTLINE_WIDTH_THICK:
		fprintf(stream, "outline-width: thick ");
		break;
	case CSS_OUTLINE_WIDTH_WIDTH:
		fprintf(stream, "outline-width: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* overflow */
	val = css_computed_overflow(style);
	switch (val) {
	case CSS_OVERFLOW_VISIBLE:
		fprintf(stream, "overflow: visible ");
		break;
	case CSS_OVERFLOW_HIDDEN:
		fprintf(stream, "overflow: hidden ");
		break;
	case CSS_OVERFLOW_SCROLL:
		fprintf(stream, "overflow: scroll ");
		break;
	case CSS_OVERFLOW_AUTO:
		fprintf(stream, "overflow: auto ");
		break;
	default:
		break;
	}

	/* padding-top */
	val = css_computed_padding_top(style, &len1, &unit1);
	switch (val) {
	case CSS_PADDING_SET:
		fprintf(stream, "padding-top: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* padding-right */
	val = css_computed_padding_right(style, &len1, &unit1);
	switch (val) {
	case CSS_PADDING_SET:
		fprintf(stream, "padding-right: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* padding-bottom */
	val = css_computed_padding_bottom(style, &len1, &unit1);
	switch (val) {
	case CSS_PADDING_SET:
		fprintf(stream, "padding-bottom: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* padding-left */
	val = css_computed_padding_left(style, &len1, &unit1);
	switch (val) {
	case CSS_PADDING_SET:
		fprintf(stream, "padding-left: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* position */
	val = css_computed_position(style);
	switch (val) {
	case CSS_POSITION_STATIC:
		fprintf(stream, "position: static ");
		break;
	case CSS_POSITION_RELATIVE:
		fprintf(stream, "position: relative ");
		break;
	case CSS_POSITION_ABSOLUTE:
		fprintf(stream, "position: absolute ");
		break;
	case CSS_POSITION_FIXED:
		fprintf(stream, "position: fixed ");
		break;
	default:
		break;
	}

	/* quotes */
	val = css_computed_quotes(style, &string_list);
	if (val == CSS_QUOTES_STRING && string_list != NULL) {
		fprintf(stream, "quotes:");

		while (*string_list != NULL) {
			fprintf(stream, " \"%.*s\"",
				(int) lwc_string_length(*string_list),
				lwc_string_data(*string_list));

			string_list++;
		}

		fprintf(stream, " ");
	} else {
		switch (val) {
		case CSS_QUOTES_NONE:
			fprintf(stream, "quotes: none ");
			break;
		default:
			break;
		}
	}

	/* right */
	val = css_computed_right(style, &len1, &unit1);
	switch (val) {
	case CSS_RIGHT_AUTO:
		fprintf(stream, "right: auto ");
		break;
	case CSS_RIGHT_SET:
		fprintf(stream, "right: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* table-layout */
	val = css_computed_table_layout(style);
	switch (val) {
	case CSS_TABLE_LAYOUT_AUTO:
		fprintf(stream, "table-layout: auto ");
		break;
	case CSS_TABLE_LAYOUT_FIXED:
		fprintf(stream, "table-layout: fixed ");
		break;
	default:
		break;
	}

	/* text-align */
	val = css_computed_text_align(style);
	switch (val) {
	case CSS_TEXT_ALIGN_LEFT:
		fprintf(stream, "text-align: left ");
		break;
	case CSS_TEXT_ALIGN_RIGHT:
		fprintf(stream, "text-align: right ");
		break;
	case CSS_TEXT_ALIGN_CENTER:
		fprintf(stream, "text-align: center ");
		break;
	case CSS_TEXT_ALIGN_JUSTIFY:
		fprintf(stream, "text-align: justify ");
		break;
	case CSS_TEXT_ALIGN_DEFAULT:
		fprintf(stream, "text-align: default ");
		break;
	case CSS_TEXT_ALIGN_LIBCSS_LEFT:
		fprintf(stream, "text-align: -libcss-left ");
		break;
	case CSS_TEXT_ALIGN_LIBCSS_CENTER:
		fprintf(stream, "text-align: -libcss-center ");
		break;
	case CSS_TEXT_ALIGN_LIBCSS_RIGHT:
		fprintf(stream, "text-align: -libcss-right ");
		break;
	default:
		break;
	}

	/* text-decoration */
	val = css_computed_text_decoration(style);
	if (val == CSS_TEXT_DECORATION_NONE) {
		fprintf(stream, "text-decoration: none ");
	} else {
		fprintf(stream, "text-decoration:");

		if (val & CSS_TEXT_DECORATION_BLINK) {
			fprintf(stream, " blink");
		}
		if (val & CSS_TEXT_DECORATION_LINE_THROUGH) {
			fprintf(stream, " line-through");
		}
		if (val & CSS_TEXT_DECORATION_OVERLINE) {
			fprintf(stream, " overline");
		}
		if (val & CSS_TEXT_DECORATION_UNDERLINE) {
			fprintf(stream, " underline");
		}

		fprintf(stream, " ");
	}

	/* text-indent */
	val = css_computed_text_indent(style, &len1, &unit1);
	switch (val) {
	case CSS_TEXT_INDENT_SET:
		fprintf(stream, "text-indent: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* text-transform */
	val = css_computed_text_transform(style);
	switch (val) {
	case CSS_TEXT_TRANSFORM_CAPITALIZE:
		fprintf(stream, "text-transform: capitalize ");
		break;
	case CSS_TEXT_TRANSFORM_UPPERCASE:
		fprintf(stream, "text-transform: uppercase ");
		break;
	case CSS_TEXT_TRANSFORM_LOWERCASE:
		fprintf(stream, "text-transform: lowercase ");
		break;
	case CSS_TEXT_TRANSFORM_NONE:
		fprintf(stream, "text-transform: none ");
		break;
	default:
		break;
	}

	/* top */
	val = css_computed_top(style, &len1, &unit1);
	switch (val) {
	case CSS_TOP_AUTO:
		fprintf(stream, "top: auto ");
		break;
	case CSS_TOP_SET:
		fprintf(stream, "top: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* unicode-bidi */
	val = css_computed_unicode_bidi(style);
	switch (val) {
	case CSS_UNICODE_BIDI_NORMAL:
		fprintf(stream, "unicode-bidi: normal ");
		break;
	case CSS_UNICODE_BIDI_EMBED:
		fprintf(stream, "unicode-bidi: embed ");
		break;
	case CSS_UNICODE_BIDI_BIDI_OVERRIDE:
		fprintf(stream, "unicode-bidi: bidi-override ");
		break;
	default:
		break;
	}

	/* vertical-align */
	val = css_computed_vertical_align(style, &len1, &unit1);
	switch (val) {
	case CSS_VERTICAL_ALIGN_BASELINE:
		fprintf(stream, "vertical-align: baseline ");
		break;
	case CSS_VERTICAL_ALIGN_SUB:
		fprintf(stream, "vertical-align: sub ");
		break;
	case CSS_VERTICAL_ALIGN_SUPER:
		fprintf(stream, "vertical-align: super ");
		break;
	case CSS_VERTICAL_ALIGN_TOP:
		fprintf(stream, "vertical-align: top ");
		break;
	case CSS_VERTICAL_ALIGN_TEXT_TOP:
		fprintf(stream, "vertical-align: text-top ");
		break;
	case CSS_VERTICAL_ALIGN_MIDDLE:
		fprintf(stream, "vertical-align: middle ");
		break;
	case CSS_VERTICAL_ALIGN_BOTTOM:
		fprintf(stream, "vertical-align: bottom ");
		break;
	case CSS_VERTICAL_ALIGN_TEXT_BOTTOM:
		fprintf(stream, "vertical-align: text-bottom ");
		break;
	case CSS_VERTICAL_ALIGN_SET:
		fprintf(stream, "vertical-align: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* visibility */
	val = css_computed_visibility(style);
	switch (val) {
	case CSS_VISIBILITY_VISIBLE:
		fprintf(stream, "visibility: visible ");
		break;
	case CSS_VISIBILITY_HIDDEN:
		fprintf(stream, "visibility: hidden ");
		break;
	case CSS_VISIBILITY_COLLAPSE:
		fprintf(stream, "visibility: collapse ");
		break;
	default:
		break;
	}

	/* white-space */
	val = css_computed_white_space(style);
	switch (val) {
	case CSS_WHITE_SPACE_NORMAL:
		fprintf(stream, "white-space: normal ");
		break;
	case CSS_WHITE_SPACE_PRE:
		fprintf(stream, "white-space: pre ");
		break;
	case CSS_WHITE_SPACE_NOWRAP:
		fprintf(stream, "white-space: nowrap ");
		break;
	case CSS_WHITE_SPACE_PRE_WRAP:
		fprintf(stream, "white-space: pre-wrap ");
		break;
	case CSS_WHITE_SPACE_PRE_LINE:
		fprintf(stream, "white-space: pre-line ");
		break;
	default:
		break;
	}

	/* width */
	val = css_computed_width(style, &len1, &unit1);
	switch (val) {
	case CSS_WIDTH_AUTO:
		fprintf(stream, "width: auto ");
		break;
	case CSS_WIDTH_SET:
		fprintf(stream, "width: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* word-spacing */
	val = css_computed_word_spacing(style, &len1, &unit1);
	switch (val) {
	case CSS_WORD_SPACING_NORMAL:
		fprintf(stream, "word-spacing: normal ");
		break;
	case CSS_WORD_SPACING_SET:
		fprintf(stream, "word-spacing: ");

		dump_css_unit(stream, len1, unit1);

		fprintf(stream, " ");
		break;
	default:
		break;
	}

	/* z-index */
	val = css_computed_z_index(style, &zindex);
	switch (val) {
	case CSS_Z_INDEX_AUTO:
		fprintf(stream, "z-index: auto ");
		break;
	case CSS_Z_INDEX_SET:
		fprintf(stream, "z-index: %d ", zindex);
		break;
	default:
		break;
	}

	fprintf(stream, "}");
}

/******************************************************************************
 * Helper functions for nscss_dump_computed_style                             *
 ******************************************************************************/

/**
 * Dump a fixed point value to the stream in a textual form.
 *
 * \param stream  Stream to write to
 * \param f       Value to write
 */
void dump_css_fixed(FILE *stream, css_fixed f)
{
#define NSCSS_ABS(x) (uint32_t)((x) < 0 ? -(x) : (x))
	uint32_t uintpart = FIXTOINT(NSCSS_ABS(f));
	/* + 500 to ensure round to nearest (division will truncate) */
	uint32_t fracpart = ((NSCSS_ABS(f) & 0x3ff) * 1000 + 500) / (1 << 10);
#undef NSCSS_ABS

	fprintf(stream, "%s%d.%03d", f < 0 ? "-" : "", uintpart, fracpart);
}

/**
 * Dump a numeric value to the stream in a textual form.
 *
 * \param stream  Stream to write to
 * \param val     Value to write
 */
void dump_css_number(FILE *stream, css_fixed val)
{
	if (INTTOFIX(FIXTOINT(val)) == val)
		fprintf(stream, "%d", FIXTOINT(val));
	else
		dump_css_fixed(stream, val);
}

/**
 * Dump a dimension to the stream in a textual form.
 *
 * \param stream  Stream to write to
 * \param val     Value to write
 * \param unit    Unit to write
 */
void dump_css_unit(FILE *stream, css_fixed val, css_unit unit)
{
	dump_css_number(stream, val);

	switch (unit) {
	case CSS_UNIT_PX:
		fprintf(stream, "px");
		break;
	case CSS_UNIT_EX:
		fprintf(stream, "ex");
		break;
	case CSS_UNIT_EM:
		fprintf(stream, "em");
		break;
	case CSS_UNIT_IN:
		fprintf(stream, "in");
		break;
	case CSS_UNIT_CM:
		fprintf(stream, "cm");
		break;
	case CSS_UNIT_MM:
		fprintf(stream, "mm");
		break;
	case CSS_UNIT_PT:
		fprintf(stream, "pt");
		break;
	case CSS_UNIT_PC:
		fprintf(stream, "pc");
		break;
	case CSS_UNIT_PCT:
		fprintf(stream, "%%");
		break;
	case CSS_UNIT_DEG:
		fprintf(stream, "deg");
		break;
	case CSS_UNIT_GRAD:
		fprintf(stream, "grad");
		break;
	case CSS_UNIT_RAD:
		fprintf(stream, "rad");
		break;
	case CSS_UNIT_MS:
		fprintf(stream, "ms");
		break;
	case CSS_UNIT_S:
		fprintf(stream, "s");
		break;
	case CSS_UNIT_HZ:
		fprintf(stream, "Hz");
		break;
	case CSS_UNIT_KHZ:
		fprintf(stream, "kHz");
		break;
	}
}

