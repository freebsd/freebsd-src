/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit bitmaped image widget
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

#include <libnsfb.h>
#include <libnsfb_plot.h>

#include "desktop/browser.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/image_data.h"

#include "widget.h"

static int
fb_redraw_bitmap(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	nsfb_bbox_t bbox;
	nsfb_bbox_t rect;
	nsfb_t *nsfb;

	nsfb = fbtk_get_nsfb(widget);

	fbtk_get_bbox(widget, &bbox);

	rect = bbox;

	nsfb_claim(nsfb, &bbox);

	/* clear background */
	if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(nsfb, &bbox, widget->bg);
	}

	/* plot the image */
	nsfb_plot_bitmap(nsfb,
			 &rect,
			 (nsfb_colour_t *)widget->u.bitmap.bitmap->pixdata,
			 widget->u.bitmap.bitmap->width,
			 widget->u.bitmap.bitmap->height,
			 widget->u.bitmap.bitmap->width,
			 !widget->u.bitmap.bitmap->opaque);

	nsfb_update(nsfb, &bbox);

	return 0;
}

/* exported function documented in fbtk.h */
void
fbtk_set_bitmap(fbtk_widget_t *widget, struct fbtk_bitmap *image)
{
	if ((widget == NULL) || (widget->type != FB_WIDGET_TYPE_BITMAP))
		return;

	widget->u.bitmap.bitmap = image;

	fbtk_request_redraw(widget);
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_bitmap(fbtk_widget_t *parent,
		   int x,
		   int y,
		   int width,
		   int height,
		   colour c,
		   struct fbtk_bitmap *image)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_BITMAP, x, y, width, height);

	neww->bg = c;
	neww->mapped = true;
	neww->u.bitmap.bitmap = image;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_bitmap, NULL);

	return neww;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_button(fbtk_widget_t *parent,
		   int x,
		   int y,
		   int width,
		   int height,
		   colour c,
		   struct fbtk_bitmap *image,
		   fbtk_callback click,
		   void *pw)
{
	fbtk_widget_t *neww;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_BITMAP, x, y, width, height);

	neww->bg = c;
	neww->mapped = true;
	neww->u.bitmap.bitmap = image;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_bitmap, NULL);
	fbtk_set_handler(neww, FBTK_CBT_CLICK, click, pw);
	fbtk_set_handler(neww, FBTK_CBT_POINTERENTER, fbtk_set_ptr, &hand_image);

	return neww;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
