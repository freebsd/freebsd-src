/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit scrollbar widgets.
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_plot_util.h>
#include <libnsfb_event.h>

#include "utils/log.h"
#include "desktop/browser.h"
#include "render/font.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/font.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/image_data.h"

#include "widget.h"

//#define TEXT_WIDGET_BORDER 3 /**< The pixel border round a text widget. */

/* Lighten a colour by taking seven eights of each channel's intensity
 * and adding a full eighth
 */
#define brighten_colour(c1)					\
	(((((7 * ((c1 >> 16) & 0xff)) >> 3) + 32) << 16) |	\
	 ((((7 * ((c1 >> 8) & 0xff)) >> 3) + 32) << 8) |	\
	 ((((7 * (c1 & 0xff)) >> 3) + 32) << 0))

/* Convert pixels to points, assuming a DPI of 90 */
#define px_to_pt(x) (((x) * 72) / FBTK_DPI)

/* Get a font style for a text input */
static inline void
fb_text_font_style(fbtk_widget_t *widget, int *font_height, int *padding,
		plot_font_style_t *font_style)
{
	if (widget->u.text.outline)
		*padding = 1;
	else
		*padding = 0;

#ifdef FB_USE_FREETYPE
	*padding += widget->height / 6;
	*font_height = widget->height - *padding - *padding;
#else
	*font_height = font_regular.height;
	*padding = (widget->height - *padding - *font_height) / 2;
#endif

	font_style->family = PLOT_FONT_FAMILY_SANS_SERIF;
	font_style->size = px_to_pt(*font_height * FONT_SIZE_SCALE);
	font_style->weight = 400;
	font_style->flags = FONTF_NONE;
	font_style->background = widget->bg;
	font_style->foreground = widget->fg;
}

/** Text redraw callback.
 *
 * Called when a text widget requires redrawing.
 *
 * @param widget The widget to be redrawn.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
fb_redraw_text(fbtk_widget_t *widget, fbtk_callback_info *cbi )
{
	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	fbtk_widget_t *root;
	plot_font_style_t font_style;
	int caret_x, caret_y, caret_h;
	int fh;
	int padding;
	int scroll = 0;
	bool caret = false;

	fb_text_font_style(widget, &fh, &padding, &font_style);

	if (fbtk_get_caret(widget, &caret_x, &caret_y, &caret_h)) {
		caret = true;
	}

	root = fbtk_get_root_widget(widget);

	fbtk_get_bbox(widget, &bbox);

	rect = bbox;

	nsfb_claim(root->u.root.fb, &bbox);

	/* clear background */
	if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
	}

	/* widget can have a single pixel outline border */
	if (widget->u.text.outline) {
		rect.x1--;
		rect.y1--;
		nsfb_plot_rectangle(root->u.root.fb, &rect, 1,
				0x00000000, false, false);
	}

	if (widget->u.text.text != NULL) {
		int x = bbox.x0 + padding;
		int y = bbox.y0 + ((fh * 3 + 2) / 4) + padding;

#ifdef FB_USE_FREETYPE
		/* Freetype renders text higher */
		y += 1;
#endif

		if (caret && widget->width - padding - padding < caret_x) {
			scroll = (widget->width - padding - padding) - caret_x;
			x +=  scroll;
		}

		/* Call the fb text plotting, baseline is 3/4 down the font */
		fb_plotters.text(x, y, widget->u.text.text,
				widget->u.text.len, &font_style);
	}

	if (caret) {
		/* This widget has caret, so render it */
		nsfb_t *nsfb = fbtk_get_nsfb(widget);
		nsfb_bbox_t line;
		nsfb_plot_pen_t pen;

		line.x0 = bbox.x0 + caret_x + scroll;
		line.y0 = bbox.y0 + caret_y;
		line.x1 = bbox.x0 + caret_x + scroll;
		line.y1 = bbox.y0 + caret_y + caret_h;

		pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
		pen.stroke_width = 1;
		pen.stroke_colour = 0xFF0000FF;

		nsfb_plot_line(nsfb, &line, &pen);
	}

	nsfb_update(root->u.root.fb, &bbox);

	return 0;
}

/** Text destroy callback.
 *
 * Called when a text widget is destroyed.
 *
 * @param widget The widget being destroyed.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int fb_destroy_text(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_TEXT)) {
		return 0;
	}

	if (widget->u.text.text != NULL) {
		free(widget->u.text.text);
	}

	return 0;
}

/** Text button redraw callback.
 *
 * Called when a text widget requires redrawing.
 *
 * @param widget The widget to be redrawn.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
fb_redraw_text_button(fbtk_widget_t *widget, fbtk_callback_info *cbi )
{
	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	nsfb_bbox_t line;
	nsfb_plot_pen_t pen;
	plot_font_style_t font_style;
	int fh;
	int border;
	fbtk_widget_t *root = fbtk_get_root_widget(widget);

	fb_text_font_style(widget, &fh, &border, &font_style);

	pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
	pen.stroke_width = 1;
	pen.stroke_colour = brighten_colour(widget->bg);

	fbtk_get_bbox(widget, &bbox);

	rect = bbox;
	rect.x1--;
	rect.y1--;

	nsfb_claim(root->u.root.fb, &bbox);

	/* clear background */
	if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);
	}

	if (widget->u.text.outline) {
		line.x0 = rect.x0;
		line.y0 = rect.y0;
		line.x1 = rect.x0;
		line.y1 = rect.y1;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		line.x0 = rect.x0;
		line.y0 = rect.y0;
		line.x1 = rect.x1;
		line.y1 = rect.y0;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		pen.stroke_colour = darken_colour(widget->bg);
		line.x0 = rect.x0;
		line.y0 = rect.y1;
		line.x1 = rect.x1;
		line.y1 = rect.y1;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
		line.x0 = rect.x1;
		line.y0 = rect.y0;
		line.x1 = rect.x1;
		line.y1 = rect.y1;
		nsfb_plot_line(root->u.root.fb, &line, &pen);
	}

	if (widget->u.text.text != NULL) {
		/* Call the fb text plotting, baseline is 3/4 down the font */
		fb_plotters.text(bbox.x0 + border,
				bbox.y0 + ((fh * 3) / 4) + border,
				widget->u.text.text,
				widget->u.text.len,
				&font_style);
	}

	nsfb_update(root->u.root.fb, &bbox);

	return 0;
}

static void
fb_text_input_remove_caret_cb(fbtk_widget_t *widget)
{
	int c_x, c_y, c_h;

	if (fbtk_get_caret(widget, &c_x, &c_y, &c_h)) {
		fbtk_request_redraw(widget);
	}
}

/** Routine called when text events occour in writeable widget.
 *
 * @param widget The widget reciving input events.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
text_input(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int value;
	static fbtk_modifier_type modifier = FBTK_MOD_CLEAR;
	char *temp;
	plot_font_style_t font_style;
	int fh;
	int border;
	bool caret_moved = false;

	fb_text_font_style(widget, &fh, &border, &font_style);

	if (cbi->event == NULL) {
		/* gain focus */
		if (widget->u.text.text == NULL)
			widget->u.text.text = calloc(1,1);

		return 0;
	}

	value = cbi->event->value.keycode;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN) {
		switch (value) {
		case NSFB_KEY_RSHIFT:
			modifier &= ~FBTK_MOD_RSHIFT;
			break;

		case NSFB_KEY_LSHIFT:
			modifier &= ~FBTK_MOD_LSHIFT;
			break;

		case NSFB_KEY_RCTRL:
			modifier &= ~FBTK_MOD_RCTRL;
			break;

		case NSFB_KEY_LCTRL:
			modifier &= ~FBTK_MOD_LCTRL;
			break;

		default:
			break;
		}
		return 0;
	}

	switch (value) {
	case NSFB_KEY_BACKSPACE:
		if (widget->u.text.idx <= 0)
			break;
		memmove(widget->u.text.text + widget->u.text.idx - 1,
				widget->u.text.text + widget->u.text.idx,
				widget->u.text.len - widget->u.text.idx);
		widget->u.text.idx--;
		widget->u.text.len--;
		widget->u.text.text[widget->u.text.len] = 0;

		nsfont.font_width(&font_style, widget->u.text.text,
				widget->u.text.len, &widget->u.text.width);

		caret_moved = true;
		break;

	case NSFB_KEY_RETURN:
		widget->u.text.enter(widget->u.text.pw, widget->u.text.text);
		break;

	case NSFB_KEY_RIGHT:
		if (widget->u.text.idx < widget->u.text.len) {
			if (modifier == FBTK_MOD_CLEAR)
				widget->u.text.idx++;
			else
				widget->u.text.idx = widget->u.text.len;

			caret_moved = true;
		}
		break;

	case NSFB_KEY_LEFT:
		if (widget->u.text.idx > 0) {
			if (modifier == FBTK_MOD_CLEAR)
				widget->u.text.idx--;
			else
				widget->u.text.idx = 0;

			caret_moved = true;
		}
		break;

	case NSFB_KEY_PAGEUP:
	case NSFB_KEY_PAGEDOWN:
	case NSFB_KEY_UP:
	case NSFB_KEY_DOWN:
		/* Not handling any of these correctly yet, but avoid putting
		 * charcters in the text widget when they're pressed. */
		break;

	case NSFB_KEY_RSHIFT:
		modifier |= FBTK_MOD_RSHIFT;
		break;

	case NSFB_KEY_LSHIFT:
		modifier |= FBTK_MOD_LSHIFT;
		break;

	case NSFB_KEY_RCTRL:
		modifier |= FBTK_MOD_RCTRL;
		break;

	case NSFB_KEY_LCTRL:
		modifier |= FBTK_MOD_LCTRL;
		break;

	default:
		if (modifier & FBTK_MOD_LCTRL || modifier & FBTK_MOD_RCTRL) {
			/* CTRL pressed, don't enter any text */
			if (value == NSFB_KEY_u) {
				/* CTRL+U: clear writable */
				widget->u.text.idx = 0;
				widget->u.text.len = 0;
				widget->u.text.text[widget->u.text.len] = '\0';
				widget->u.text.width = 0;
				caret_moved = true;
			}
			break;
		}

		/* allow for new character and null */
		temp = realloc(widget->u.text.text, widget->u.text.len + 2);
		if (temp == NULL) {
			break;
		}

		widget->u.text.text = temp;
		memmove(widget->u.text.text + widget->u.text.idx + 1,
				widget->u.text.text + widget->u.text.idx,
				widget->u.text.len - widget->u.text.idx);
		widget->u.text.text[widget->u.text.idx] =
				fbtk_keycode_to_ucs4(value, modifier);
		widget->u.text.idx++;
		widget->u.text.len++;
		widget->u.text.text[widget->u.text.len] = '\0';

		nsfont.font_width(&font_style, widget->u.text.text,
				widget->u.text.len, &widget->u.text.width);
		caret_moved = true;
		break;
	}

	if (caret_moved) {
		nsfont.font_width(&font_style, widget->u.text.text,
				widget->u.text.idx, &widget->u.text.idx_offset);
		fbtk_set_caret(widget, true,
				widget->u.text.idx_offset + border,
				border,
				widget->height - border - border,
				fb_text_input_remove_caret_cb);
	}

	fbtk_request_redraw(widget);

	return 0;
}

/** Routine called when click events occour in writeable widget.
 *
 * @param widget The widget reciving click events.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
text_input_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	plot_font_style_t font_style;
	int fh;
	int border;
	size_t idx;

	fb_text_font_style(widget, &fh, &border, &font_style);

	widget->u.text.idx = widget->u.text.len;

	nsfont.font_position_in_string(&font_style, widget->u.text.text,
			widget->u.text.len, cbi->x - border,
			&idx,
			&widget->u.text.idx_offset);
	widget->u.text.idx = idx;
	fbtk_set_caret(widget, true,
			widget->u.text.idx_offset + border,
			border,
			widget->height - border - border,
			fb_text_input_remove_caret_cb);

	fbtk_request_redraw(widget);

	return 0;
}

/** Routine called when "stripped of focus" event occours for writeable widget.
 *
 * @param widget The widget reciving "stripped of focus" event.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
text_input_strip_focus(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	fbtk_set_caret(widget, false, 0, 0, 0, NULL);

	return 0;
}

/* exported function documented in fbtk.h */
void
fbtk_writable_text(fbtk_widget_t *widget, fbtk_enter_t enter, void *pw)
{
	widget->u.text.enter = enter;
	widget->u.text.pw = pw;

	fbtk_set_handler(widget, FBTK_CBT_INPUT, text_input, widget);
}

/* exported function documented in fbtk.h */
void
fbtk_set_text(fbtk_widget_t *widget, const char *text)
{
	plot_font_style_t font_style;
	int c_x, c_y, c_h;
	int fh;
	int border;

	if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_TEXT))
		return;
	if (widget->u.text.text != NULL) {
		if (strcmp(widget->u.text.text, text) == 0)
			return; /* text is being set to the same thing */
		free(widget->u.text.text);
	}
	widget->u.text.text = strdup(text);
	widget->u.text.len = strlen(text);
	widget->u.text.idx = widget->u.text.len;


	fb_text_font_style(widget, &fh, &border, &font_style);
	nsfont.font_width(&font_style, widget->u.text.text,
			widget->u.text.len, &widget->u.text.width);
	nsfont.font_width(&font_style, widget->u.text.text,
			widget->u.text.idx, &widget->u.text.idx_offset);

	if (fbtk_get_caret(widget, &c_x, &c_y, &c_h)) {
		/* Widget has caret; move it to end of new string */
		fbtk_set_caret(widget, true,
				widget->u.text.idx_offset + border,
				border,
				widget->height - border - border,
				fb_text_input_remove_caret_cb);
	}

	fbtk_request_redraw(widget);
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_text(fbtk_widget_t *parent,
		 int x,
		 int y,
		 int width,
		 int height,
		 colour bg,
		 colour fg,
		 bool outline)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_TEXT, x, y, width, height);
	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;
	neww->u.text.outline = outline;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_text, NULL);
	fbtk_set_handler(neww, FBTK_CBT_DESTROY, fb_destroy_text, NULL);

	return neww;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_writable_text(fbtk_widget_t *parent,
			  int x,
			  int y,
			  int width,
			  int height,
			  colour bg,
			  colour fg,
			  bool outline,
			  fbtk_enter_t enter,
			  void *pw)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_TEXT, x, y, width, height);
	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;

	neww->u.text.outline = outline;
	neww->u.text.enter = enter;
	neww->u.text.pw = pw;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_text, NULL);
	fbtk_set_handler(neww, FBTK_CBT_DESTROY, fb_destroy_text, NULL);
	fbtk_set_handler(neww, FBTK_CBT_CLICK, text_input_click, pw);
	fbtk_set_handler(neww, FBTK_CBT_STRIP_FOCUS, text_input_strip_focus, NULL);
	fbtk_set_handler(neww, FBTK_CBT_INPUT, text_input, neww);

	return neww;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_text_button(fbtk_widget_t *parent,
			int x,
			int y,
			int width,
			int height,
			colour bg,
			colour fg,
			fbtk_callback click,
			void *pw)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_TEXT, x, y, width, height);
	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;

	neww->u.text.outline = true;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_text_button, NULL);
	fbtk_set_handler(neww, FBTK_CBT_DESTROY, fb_destroy_text, NULL);
	fbtk_set_handler(neww, FBTK_CBT_CLICK, click, pw);
	fbtk_set_handler(neww, FBTK_CBT_POINTERENTER, fbtk_set_ptr, &hand_image);

	return neww;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
