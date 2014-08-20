/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
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

#include "desktop/browser_history.h"
#include "desktop/plotters.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"

static int
localhistory_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_localhistory *glh = cbi->context;
	nsfb_bbox_t rbox;

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &fb_plotters
	};

	rbox.x0 = fbtk_get_absx(widget);
	rbox.y0 = fbtk_get_absy(widget);

	rbox.x1 = rbox.x0 + fbtk_get_width(widget);
	rbox.y1 = rbox.y0 + fbtk_get_height(widget);

	nsfb_claim(fbtk_get_nsfb(widget), &rbox);

	nsfb_plot_rectangle_fill(fbtk_get_nsfb(widget), &rbox, 0xffffffff);

	browser_window_history_redraw_rectangle(glh->bw,
				 glh->scrollx,
				 glh->scrolly,
				 fbtk_get_width(widget) + glh->scrollx,
				 fbtk_get_height(widget) + glh->scrolly,
				 0, 0, &ctx);

	nsfb_update(fbtk_get_nsfb(widget), &rbox);

	return 0;
}

static int
localhistory_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_localhistory *glh = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	browser_window_history_click(glh->bw, cbi->x, cbi->y, false);

	fbtk_set_mapping(glh->window, false);

	return 1;
}

struct gui_localhistory *
fb_create_localhistory(struct browser_window *bw,
		       fbtk_widget_t *parent,
		       int furniture_width)
{
	struct gui_localhistory *glh;
	glh = calloc(1, sizeof(struct gui_localhistory));

	if (glh == NULL)
		return NULL;

	glh->bw = bw;

	/* container window */
	glh->window = fbtk_create_window(parent, 0, 0, 0, 0, 0);

	glh->history = fbtk_create_user(glh->window, 0, 0, -furniture_width, -furniture_width, glh);

	fbtk_set_handler(glh->history, FBTK_CBT_REDRAW, localhistory_redraw, glh);
	fbtk_set_handler(glh->history, FBTK_CBT_CLICK, localhistory_click, glh);
	/*
	  fbtk_set_handler(gw->localhistory, FBTK_CBT_INPUT, fb_browser_window_input, gw);
	  fbtk_set_handler(gw->localhistory, FBTK_CBT_POINTERMOVE, fb_browser_window_move, bw);
	*/

	/* create horizontal scrollbar */
	glh->hscroll = fbtk_create_hscroll(glh->window,
					   0,
					   fbtk_get_height(glh->window) - furniture_width,
					   fbtk_get_width(glh->window) - furniture_width,
					   furniture_width,
					   FB_SCROLL_COLOUR,
					   FB_FRAME_COLOUR,
					   NULL,
					   NULL);

	glh->vscroll = fbtk_create_vscroll(glh->window,
					   fbtk_get_width(glh->window) - furniture_width,
					   0,
					   furniture_width,
					   fbtk_get_height(glh->window) - furniture_width,
					   FB_SCROLL_COLOUR,
					   FB_FRAME_COLOUR,
					   NULL,
					   NULL);

	fbtk_create_fill(glh->window,
			 fbtk_get_width(glh->window) - furniture_width,
			 fbtk_get_height(glh->window) - furniture_width,
			 furniture_width,
			 furniture_width,
			 FB_FRAME_COLOUR);

	return glh;
}

void
fb_localhistory_map(struct gui_localhistory * glh)
{
	fbtk_set_zorder(glh->window, INT_MIN);
	fbtk_set_mapping(glh->window, true);
}
