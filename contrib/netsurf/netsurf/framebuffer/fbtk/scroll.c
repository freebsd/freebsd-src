/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit scrollbar widgets
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

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/log.h"
#include "desktop/browser.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/image_data.h"

#include "widget.h"

/* Vertical scroll widget */

static int
vscroll_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int vscroll;
	int vpos;

	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	fbtk_widget_t *root = fbtk_get_root_widget(widget);

	fbtk_get_bbox(widget, &bbox);

	nsfb_claim(root->u.root.fb, &bbox);

	rect = bbox;

	/* background */
	nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

	/* scroll well */
	rect.x0 = bbox.x0 + 2;
	rect.y0 = bbox.y0 + 1;
	rect.x1 = bbox.x1 - 3;
	rect.y1 = bbox.y1 - 2;

	nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->fg);
	nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0xFF999999, false, false);

	/* scroll bar */
	if ((widget->u.scroll.maximum - widget->u.scroll.minimum) > 0) {
		vscroll = ((widget->height - 4) * widget->u.scroll.thumb) /
			(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
		vpos = ((widget->height - 4) * widget->u.scroll.position) /
			(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
	} else {
		vscroll = (widget->height - 4);
		vpos = 0;
	}

	rect.x0 = bbox.x0 + 5;
	rect.y0 = bbox.y0 + 3 + vpos;
	rect.x1 = bbox.x0 + widget->width - 5;
	rect.y1 = bbox.y0 + vscroll + vpos;

	nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

	nsfb_update(root->u.root.fb, &bbox);

	return 0;
}

static int
vscroll_drag(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int newpos;
	fbtk_widget_t *scrollw = cbi->context;

	newpos = ((widget->u.scroll.drag_position +
			(cbi->y - widget->u.scroll.drag)) *
			(widget->u.scroll.maximum - widget->u.scroll.minimum)) /
			(widget->height - 4);

	if (newpos < scrollw->u.scroll.minimum)
		newpos = scrollw->u.scroll.minimum;

	if (newpos > (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb ))
		newpos = (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb);

	if (newpos == scrollw->u.scroll.position)
		return 0;

	return fbtk_post_callback(widget, FBTK_CBT_SCROLLY, newpos);
}

static int
vscrollu_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int newpos;
	fbtk_widget_t *scrollw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN)
		return 0;

	newpos = scrollw->u.scroll.position - scrollw->u.scroll.page;
	if (newpos < scrollw->u.scroll.minimum)
		newpos = scrollw->u.scroll.minimum;

	if (newpos ==  scrollw->u.scroll.position)
		return 0;

	return fbtk_post_callback(scrollw, FBTK_CBT_SCROLLY, newpos);
}

static int
vscrolld_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int newpos;
	fbtk_widget_t *scrollw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN)
		return 0;

	newpos = scrollw->u.scroll.position + scrollw->u.scroll.page;
	if (newpos > (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb ))
		newpos = (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb);

	if (newpos == scrollw->u.scroll.position)
		return 0;

	return fbtk_post_callback(scrollw, FBTK_CBT_SCROLLY, newpos);
}

static int
vscrollarea_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int vscroll;
	int vpos;
	int newpos;
	int ret = 0;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN) {
		/* end all drags, just in case */
		if (fbtk_set_handler(widget, FBTK_CBT_POINTERMOVE, NULL, NULL) != NULL)
			fbtk_tgrab_pointer(widget);
		return 0;
	}

	switch (cbi->event->value.keycode) {

	case NSFB_KEY_MOUSE_4:
		/* scroll up */
		newpos = widget->u.scroll.position - widget->u.scroll.page;
		if (newpos < widget->u.scroll.minimum)
			newpos = widget->u.scroll.minimum;
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, newpos);
		break;

	case NSFB_KEY_MOUSE_5:
		/* scroll down */
		newpos = widget->u.scroll.position + widget->u.scroll.page;
		if (newpos > widget->u.scroll.maximum)
			newpos = widget->u.scroll.maximum;
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, newpos);
		break;

	default:

		if ((widget->u.scroll.maximum - widget->u.scroll.minimum) > 0) {
			vscroll = ((widget->height - 4) * widget->u.scroll.thumb) /
				(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
			vpos = ((widget->height - 4) * widget->u.scroll.position) /
				(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
		} else {
			vscroll = (widget->height - 4);
			vpos = 0;
		}

		if (cbi->y < vpos) {
			/* above bar */
			newpos = widget->u.scroll.position - widget->u.scroll.thumb;
			if (newpos < widget->u.scroll.minimum)
				newpos = widget->u.scroll.minimum;
			ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, newpos);
		} else if (cbi->y > (vpos + vscroll)) {
			/* below bar */
			newpos = widget->u.scroll.position + widget->u.scroll.thumb;
			if (newpos > widget->u.scroll.maximum)
				newpos = widget->u.scroll.maximum;
			ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLY, newpos);
		} else {
			/* on bar - start drag */
			widget->u.scroll.drag = cbi->y;
			widget->u.scroll.drag_position = vpos;
			fbtk_set_handler(widget, FBTK_CBT_POINTERMOVE, vscroll_drag, widget);
			fbtk_tgrab_pointer(widget);
		}
	}
	return ret;
}


/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_vscroll(fbtk_widget_t *parent,
		    int x,
		    int y,
		    int width,
		    int height,
		    colour fg,
		    colour bg,
		    fbtk_callback callback,
		    void *context)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent,
			       FB_WIDGET_TYPE_VSCROLL,
			       x,
			       y + scrollu.height,
			       width,
			       height  - scrollu.height - scrolld.height);

	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, vscroll_redraw, NULL);

	fbtk_set_handler(neww, FBTK_CBT_CLICK, vscrollarea_click, neww);

	fbtk_set_handler(neww, FBTK_CBT_SCROLLY, callback, context);

	neww->u.scroll.btnul = fbtk_create_button(parent,
						  x,
						  y,
						  width,
						  scrollu.height,
						  fg,
						  &scrollu,
						  vscrollu_click,
						  neww);

	neww->u.scroll.btndr = fbtk_create_button(parent,
						  x,
						  y + height - scrolld.height,
						  width,
						  scrolld.height,
						  fg,
						  &scrolld,
						  vscrolld_click,
						  neww);


	return neww;
}

/* Horizontal scroll widget */

static int
hscroll_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int hscroll;
	int hpos;
	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	fbtk_widget_t *root = fbtk_get_root_widget(widget);

	fbtk_get_bbox(widget, &bbox);

	nsfb_claim(root->u.root.fb, &bbox);

	rect = bbox;

	/* background */
	nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

	/* scroll well */
	rect.x0 = bbox.x0 + 1;
	rect.y0 = bbox.y0 + 2;
	rect.x1 = bbox.x1 - 2;
	rect.y1 = bbox.y1 - 3;
	nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->fg);

	/* scroll well outline */
	nsfb_plot_rectangle(root->u.root.fb, &rect, 1, 0xFF999999, false, false);

	if ((widget->u.scroll.maximum - widget->u.scroll.minimum) > 0) {
		hscroll = ((widget->width - 4) * widget->u.scroll.thumb) /
			(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
		hpos = ((widget->width - 4) * widget->u.scroll.position) /
			(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
	} else {
		hscroll = (widget->width - 4);
		hpos = 0;
	}

	LOG(("hscroll %d", hscroll));

	rect.x0 = bbox.x0 + 3 + hpos;
	rect.y0 = bbox.y0 + 5;
	rect.x1 = bbox.x0 + hscroll + hpos;
	rect.y1 = bbox.y0 + widget->height - 5;

	nsfb_plot_rectangle_fill(root->u.root.fb, &rect, widget->bg);

	nsfb_update(root->u.root.fb, &bbox);

	return 0;
}

static int
hscrolll_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int newpos;
	fbtk_widget_t *scrollw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN)
		return 0;

	newpos = scrollw->u.scroll.position - scrollw->u.scroll.page;
	if (newpos < scrollw->u.scroll.minimum)
		newpos = scrollw->u.scroll.minimum;

	if (newpos == scrollw->u.scroll.position) {
		LOG(("horiz scroll was the same %d", newpos));
		return 0;
	}

	return fbtk_post_callback(scrollw, FBTK_CBT_SCROLLX, newpos);
}

static int
hscrollr_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int newpos;
	fbtk_widget_t *scrollw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN)
		return 0;

	newpos = scrollw->u.scroll.position + scrollw->u.scroll.page;
	if (newpos > (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb ))
		newpos = (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb);

	if (newpos == scrollw->u.scroll.position)
		return 0;

	return fbtk_post_callback(scrollw, FBTK_CBT_SCROLLX, newpos);
}

static int
hscroll_drag(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int newpos;
	fbtk_widget_t *scrollw = cbi->context;

	newpos = ((widget->u.scroll.drag_position +
			(cbi->x - widget->u.scroll.drag)) *
			(widget->u.scroll.maximum - widget->u.scroll.minimum)) /
			(widget->width - 4);

	if (newpos < scrollw->u.scroll.minimum)
		newpos = scrollw->u.scroll.minimum;

	if (newpos > (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb ))
		newpos = (scrollw->u.scroll.maximum - scrollw->u.scroll.thumb);

	if (newpos == scrollw->u.scroll.position)
		return 0;

	return fbtk_post_callback(widget, FBTK_CBT_SCROLLX, newpos);
}

static int
hscrollarea_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	int hscroll;
	int hpos;
	int newpos;
	int ret = 0;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN) {
		/* end all drags, just in case */
		if (fbtk_set_handler(widget, FBTK_CBT_POINTERMOVE, NULL, NULL) != NULL)
			fbtk_tgrab_pointer(widget);
		return 0;
	}

	if ((widget->u.scroll.maximum - widget->u.scroll.minimum) > 0) {
		hscroll = ((widget->width - 4) * widget->u.scroll.thumb) /
			(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
		hpos = ((widget->width - 4) * widget->u.scroll.position) /
			(widget->u.scroll.maximum - widget->u.scroll.minimum) ;
	} else {
		hscroll = (widget->width - 4);
		hpos = 0;
	}

	if (cbi->x < hpos) {
		/* left of  bar */
		newpos = widget->u.scroll.position - widget->u.scroll.page;
		if (newpos < widget->u.scroll.minimum)
			newpos = widget->u.scroll.minimum;
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLX, newpos);
	} else if (cbi->x > (hpos + hscroll)) {
		/* right of bar */
		newpos = widget->u.scroll.position + widget->u.scroll.page;
		if (newpos > widget->u.scroll.maximum)
			newpos = widget->u.scroll.maximum;
		ret = fbtk_post_callback(cbi->context, FBTK_CBT_SCROLLX, newpos);
	} else {
		/* on bar - start drag */
		widget->u.scroll.drag = cbi->x;
		widget->u.scroll.drag_position = hpos;
		fbtk_set_handler(widget, FBTK_CBT_POINTERMOVE, hscroll_drag, widget);
		fbtk_tgrab_pointer(widget);
	}
	return ret;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_hscroll(fbtk_widget_t *parent,
		    int x,
		    int y,
		    int width,
		    int height,
		    colour fg,
		    colour bg,
		    fbtk_callback callback,
		    void *context)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent,
			       FB_WIDGET_TYPE_HSCROLL,
			       x + scrolll.width,
			       y,
			       width - scrolll.width - scrollr.width,
			       height);

	neww->fg = fg;
	neww->bg = bg;
	neww->mapped = true;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, hscroll_redraw, NULL);
	fbtk_set_handler(neww, FBTK_CBT_CLICK, hscrollarea_click, neww);
	fbtk_set_handler(neww, FBTK_CBT_SCROLLX, callback, context);

	neww->u.scroll.btnul = fbtk_create_button(parent,
						  x,
						  y,
						  scrolll.width,
						  height,
						  fg,
						  &scrolll,
						  hscrolll_click,
						  neww);

	neww->u.scroll.btndr = fbtk_create_button(parent,
						  x + width - scrollr.width,
						  y,
						  scrollr.width,
						  height,
						  fg,
						  &scrollr,
						  hscrollr_click,
						  neww);

	return neww;
}


/* exported function documented in fbtk.h */
bool
fbtk_set_scroll_parameters(fbtk_widget_t *widget,
			   int min,
			   int max,
			   int thumb,
			   int page)
{
	if (widget == NULL)
		return false;

	if ((widget->type != FB_WIDGET_TYPE_HSCROLL) &&
	    (widget->type != FB_WIDGET_TYPE_VSCROLL))
		return false;

	widget->u.scroll.minimum = min;
	widget->u.scroll.maximum = max;
	widget->u.scroll.thumb = thumb;
	widget->u.scroll.page = page;

	if (widget->u.scroll.position > max)
		widget->u.scroll.position = max;
	if (widget->u.scroll.position < min)
		widget->u.scroll.position = min;

	fbtk_request_redraw(widget);

	return true;
}

/* exported function documented in fbtk.h */
bool
fbtk_set_scroll_position(fbtk_widget_t *widget, int position)
{
	if (widget == NULL)
		return false;

	if ((widget->type != FB_WIDGET_TYPE_HSCROLL) &&
	    (widget->type != FB_WIDGET_TYPE_VSCROLL))
		return false;

	if ((position < widget->u.scroll.minimum) ||
	    (position > widget->u.scroll.maximum))
		return false;

	widget->u.scroll.position = position;

	fbtk_request_redraw(widget);

	return true;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
