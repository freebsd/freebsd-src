/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit window widget.
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

#include "widget.h"

/** Window redraw callback.
 *
 * Called when a window requires redrawing.
 *
 * @param widget The widget to be redrawn.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
fb_redraw_window(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	nsfb_bbox_t bbox;
	nsfb_t *nsfb;

	if ((widget->bg & 0xFF000000) == 0)
		return 0;

	nsfb = fbtk_get_nsfb(widget);

	fbtk_get_bbox(widget, &bbox);

	nsfb_claim(nsfb, &bbox);

	nsfb_plot_rectangle_fill(nsfb, &bbox, widget->bg);

	nsfb_update(nsfb, &bbox);

	return 0;
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_create_window(fbtk_widget_t *parent,
		   int x,
		   int y,
		   int width,
		   int height,
		   colour bg)
{
	fbtk_widget_t *neww;

	if (parent == NULL)
		return NULL;

	neww = fbtk_widget_new(parent, FB_WIDGET_TYPE_WINDOW, x, y, width, height);

	neww->bg = bg;

	fbtk_set_handler(neww, FBTK_CBT_REDRAW, fb_redraw_window, NULL);

	return neww;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
