/*
 * Copyright 2008, 2014 Vincent Sanders <vince@netsurf-browser.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_event.h>

#include "desktop/browser_history.h"
#include "desktop/gui.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "utils/nsoption.h"
#include "utils/filepath.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/types.h"
#include "desktop/textinput.h"
#include "render/form.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/framebuffer.h"
#include "framebuffer/schedule.h"
#include "framebuffer/findfile.h"
#include "framebuffer/image_data.h"
#include "framebuffer/font.h"
#include "framebuffer/clipboard.h"
#include "framebuffer/fetch.h"

#include "content/urldb.h"
#include "content/fetch.h"

#define NSFB_TOOLBAR_DEFAULT_LAYOUT "blfsrutc"

fbtk_widget_t *fbtk;

struct gui_window *input_window = NULL;
struct gui_window *search_current_window;
struct gui_window *window_list = NULL;

/* private data for browser user widget */
struct browser_widget_s {
	struct browser_window *bw; /**< The browser window connected to this gui window */
	int scrollx, scrolly; /**< scroll offsets. */

	/* Pending window redraw state. */
	bool redraw_required; /**< flag indicating the foreground loop
			       * needs to redraw the browser widget.
			       */
	bbox_t redraw_box; /**< Area requiring redraw. */
	bool pan_required; /**< flag indicating the foreground loop
			    * needs to pan the window.
			    */
	int panx, pany; /**< Panning required. */
};

static struct gui_drag {
	enum state {
		GUI_DRAG_NONE,
		GUI_DRAG_PRESSED,
		GUI_DRAG_DRAG
	} state;
	int button;
	int x;
	int y;
	bool grabbed_pointer;
} gui_drag;


/* queue a redraw operation, co-ordinates are relative to the window */
static void
fb_queue_redraw(struct fbtk_widget_s *widget, int x0, int y0, int x1, int y1)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);

	bwidget->redraw_box.x0 = min(bwidget->redraw_box.x0, x0);
	bwidget->redraw_box.y0 = min(bwidget->redraw_box.y0, y0);
	bwidget->redraw_box.x1 = max(bwidget->redraw_box.x1, x1);
	bwidget->redraw_box.y1 = max(bwidget->redraw_box.y1, y1);

	if (fbtk_clip_to_widget(widget, &bwidget->redraw_box)) {
		bwidget->redraw_required = true;
		fbtk_request_redraw(widget);
	} else {
		bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
		bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = -(INT_MAX);
		bwidget->redraw_required = false;
	}
}

/* queue a window scroll */
static void
widget_scroll_y(struct gui_window *gw, int y, bool abs)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(gw->browser);
	int content_width, content_height;
	int height;

	LOG(("window scroll"));
	if (abs) {
		bwidget->pany = y - bwidget->scrolly;
	} else {
		bwidget->pany += y;
	}

	browser_window_get_extents(gw->bw, true,
			&content_width, &content_height);

	height = fbtk_get_height(gw->browser);

	/* dont pan off the top */
	if ((bwidget->scrolly + bwidget->pany) < 0)
		bwidget->pany = -bwidget->scrolly;

	/* do not pan off the bottom of the content */
	if ((bwidget->scrolly + bwidget->pany) > (content_height - height))
		bwidget->pany = (content_height - height) - bwidget->scrolly;

	if (bwidget->pany == 0) {
		fbtk_request_redraw(gw->browser); /* XXX redraw */
		return;
	}

	bwidget->pan_required = true;

	fbtk_request_redraw(gw->browser);

	fbtk_set_scroll_position(gw->vscroll, bwidget->scrolly + bwidget->pany);
}

/* queue a window scroll */
static void
widget_scroll_x(struct gui_window *gw, int x, bool abs)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(gw->browser);
	int content_width, content_height;
	int width;

	if (abs) {
		bwidget->panx = x - bwidget->scrollx;
	} else {
		bwidget->panx += x;
	}

	browser_window_get_extents(gw->bw, true,
			&content_width, &content_height);

	width = fbtk_get_width(gw->browser);

	/* dont pan off the left */
	if ((bwidget->scrollx + bwidget->panx) < 0)
		bwidget->panx = - bwidget->scrollx;

	/* do not pan off the right of the content */
	if ((bwidget->scrollx + bwidget->panx) > (content_width - width))
		bwidget->panx = (content_width - width) - bwidget->scrollx;

	if (bwidget->panx == 0) {
		fbtk_request_redraw(gw->browser); /* XXX redraw */
		return;
	}

	bwidget->pan_required = true;

	fbtk_request_redraw(gw->browser);

	fbtk_set_scroll_position(gw->hscroll, bwidget->scrollx + bwidget->panx);
}

static void
fb_pan(fbtk_widget_t *widget,
       struct browser_widget_s *bwidget,
       struct browser_window *bw)
{
	int x;
	int y;
	int width;
	int height;
	nsfb_bbox_t srcbox;
	nsfb_bbox_t dstbox;

	nsfb_t *nsfb = fbtk_get_nsfb(widget);

	height = fbtk_get_height(widget);
	width = fbtk_get_width(widget);

	LOG(("panning %d, %d", bwidget->panx, bwidget->pany));

	x = fbtk_get_absx(widget);
	y = fbtk_get_absy(widget);

	/* if the pan exceeds the viewport size just redraw the whole area */
	if (bwidget->pany >= height || bwidget->pany <= -height ||
	    bwidget->panx >= width || bwidget->panx <= -width) {

		bwidget->scrolly += bwidget->pany;
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, width, height);

		/* ensure we don't try to scroll again */
		bwidget->panx = 0;
		bwidget->pany = 0;
		bwidget->pan_required = false;
		return;
	}

	if (bwidget->pany < 0) {
		/* pan up by less then viewport height */
		srcbox.x0 = x;
		srcbox.y0 = y;
		srcbox.x1 = srcbox.x0 + width;
		srcbox.y1 = srcbox.y0 + height + bwidget->pany;

		dstbox.x0 = x;
		dstbox.y0 = y - bwidget->pany;
		dstbox.x1 = dstbox.x0 + width;
		dstbox.y1 = dstbox.y0 + height + bwidget->pany;

		/* move part that remains visible up */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, 0, width, - bwidget->pany);

	} else if (bwidget->pany > 0) {
		/* pan down by less then viewport height */
		srcbox.x0 = x;
		srcbox.y0 = y + bwidget->pany;
		srcbox.x1 = srcbox.x0 + width;
		srcbox.y1 = srcbox.y0 + height - bwidget->pany;

		dstbox.x0 = x;
		dstbox.y0 = y;
		dstbox.x1 = dstbox.x0 + width;
		dstbox.y1 = dstbox.y0 + height - bwidget->pany;

		/* move part that remains visible down */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrolly += bwidget->pany;
		fb_queue_redraw(widget, 0, height - bwidget->pany,
				width, height);
	}

	if (bwidget->panx < 0) {
		/* pan left by less then viewport width */
		srcbox.x0 = x;
		srcbox.y0 = y;
		srcbox.x1 = srcbox.x0 + width + bwidget->panx;
		srcbox.y1 = srcbox.y0 + height;

		dstbox.x0 = x - bwidget->panx;
		dstbox.y0 = y;
		dstbox.x1 = dstbox.x0 + width + bwidget->panx;
		dstbox.y1 = dstbox.y0 + height;

		/* move part that remains visible left */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, 0, 0, -bwidget->panx, height);

	} else if (bwidget->panx > 0) {
		/* pan right by less then viewport width */
		srcbox.x0 = x + bwidget->panx;
		srcbox.y0 = y;
		srcbox.x1 = srcbox.x0 + width - bwidget->panx;
		srcbox.y1 = srcbox.y0 + height;

		dstbox.x0 = x;
		dstbox.y0 = y;
		dstbox.x1 = dstbox.x0 + width - bwidget->panx;
		dstbox.y1 = dstbox.y0 + height;

		/* move part that remains visible right */
		nsfb_plot_copy(nsfb, &srcbox, nsfb, &dstbox);

		/* redraw newly exposed area */
		bwidget->scrollx += bwidget->panx;
		fb_queue_redraw(widget, width - bwidget->panx, 0,
				width, height);
	}

	bwidget->pan_required = false;
	bwidget->panx = 0;
	bwidget->pany = 0;
}

static void
fb_redraw(fbtk_widget_t *widget,
	  struct browser_widget_s *bwidget,
	  struct browser_window *bw)
{
	int x;
	int y;
	int caret_x, caret_y, caret_h;
	struct rect clip;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &fb_plotters
	};
	nsfb_t *nsfb = fbtk_get_nsfb(widget);
	float scale = browser_window_get_scale(bw);

	LOG(("%d,%d to %d,%d",
	     bwidget->redraw_box.x0,
	     bwidget->redraw_box.y0,
	     bwidget->redraw_box.x1,
	     bwidget->redraw_box.y1));

	x = fbtk_get_absx(widget);
	y = fbtk_get_absy(widget);

	/* adjust clipping co-ordinates according to window location */
	bwidget->redraw_box.y0 += y;
	bwidget->redraw_box.y1 += y;
	bwidget->redraw_box.x0 += x;
	bwidget->redraw_box.x1 += x;

	nsfb_claim(nsfb, &bwidget->redraw_box);

	/* redraw bounding box is relative to window */
	clip.x0 = bwidget->redraw_box.x0;
	clip.y0 = bwidget->redraw_box.y0;
	clip.x1 = bwidget->redraw_box.x1;
	clip.y1 = bwidget->redraw_box.y1;

	browser_window_redraw(bw,
			(x - bwidget->scrollx) / scale,
			(y - bwidget->scrolly) / scale,
			&clip, &ctx);

	if (fbtk_get_caret(widget, &caret_x, &caret_y, &caret_h)) {
		/* This widget has caret, so render it */
		nsfb_bbox_t line;
		nsfb_plot_pen_t pen;

		line.x0 = x - bwidget->scrollx + caret_x;
		line.y0 = y - bwidget->scrolly + caret_y;
		line.x1 = x - bwidget->scrollx + caret_x;
		line.y1 = y - bwidget->scrolly + caret_y + caret_h;

		pen.stroke_type = NFSB_PLOT_OPTYPE_SOLID;
		pen.stroke_width = 1;
		pen.stroke_colour = 0xFF0000FF;

		nsfb_plot_line(nsfb, &line, &pen);
	}

	nsfb_update(fbtk_get_nsfb(widget), &bwidget->redraw_box);

	bwidget->redraw_box.y0 = bwidget->redraw_box.x0 = INT_MAX;
	bwidget->redraw_box.y1 = bwidget->redraw_box.x1 = INT_MIN;
	bwidget->redraw_required = false;
}

static int
fb_browser_window_redraw(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget;

	bwidget = fbtk_get_userpw(widget);
	if (bwidget == NULL) {
		LOG(("browser widget from widget %p was null", widget));
		return -1;
	}

	if (bwidget->pan_required) {
		fb_pan(widget, bwidget, gw->bw);
	}

	if (bwidget->redraw_required) {
		fb_redraw(widget, bwidget, gw->bw);
	} else {
		bwidget->redraw_box.x0 = 0;
		bwidget->redraw_box.y0 = 0;
		bwidget->redraw_box.x1 = fbtk_get_width(widget);
		bwidget->redraw_box.y1 = fbtk_get_height(widget);
		fb_redraw(widget, bwidget, gw->bw);
	}
	return 0;
}

static int fb_browser_window_destroy(fbtk_widget_t *widget,
		fbtk_callback_info *cbi)
{
	struct browser_widget_s *browser_widget;

	if (widget == NULL) {
		return 0;
	}

	/* Free private data */
	browser_widget = fbtk_get_userpw(widget);
	free(browser_widget);

	return 0;
}


static const char *fename;
static int febpp;
static int fewidth;
static int feheight;
static const char *feurl;

static bool
process_cmdline(int argc, char** argv)
{
	int opt;

	LOG(("argc %d, argv %p", argc, argv));

	fename = "mtl";
	febpp = 32;

	fewidth = nsoption_int(window_width);
	if (fewidth <= 0) {
		fewidth = 800;
	}
	feheight = nsoption_int(window_height);
	if (feheight <= 0) {
		feheight = 480;
	}

	if ((nsoption_charp(homepage_url) != NULL) && 
	    (nsoption_charp(homepage_url)[0] != '\0')) {
		feurl = nsoption_charp(homepage_url);
	} else {
		feurl = NETSURF_HOMEPAGE;
	}

	while((opt = getopt(argc, argv, "f:b:w:h:")) != -1) {
		switch (opt) {
		case 'f':
			fename = optarg;
			break;

		case 'b':
			febpp = atoi(optarg);
			break;

		case 'w':
			fewidth = atoi(optarg);
			break;

		case 'h':
			feheight = atoi(optarg);
			break;

		default:
			fprintf(stderr,
				"Usage: %s [-f frontend] [-b bpp] url\n",
				argv[0]);
			return false;
		}
	}

	if (optind < argc) {
		feurl = argv[optind];
	}

	return true;
}

/**
 * Set option defaults for framebuffer frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	/* Set defaults for absent option strings */
	nsoption_setnull_charp(cookie_file, strdup("~/.netsurf/Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup("~/.netsurf/Cookies"));

	if (nsoption_charp(cookie_file) == NULL ||
	    nsoption_charp(cookie_jar) == NULL) {
		LOG(("Failed initialising cookie options"));
		return NSERROR_BAD_PARAMETER;
	}

	/* set system colours for framebuffer ui */
	nsoption_set_colour(sys_colour_ActiveBorder, 0x00000000);
	nsoption_set_colour(sys_colour_ActiveCaption, 0x00ddddcc);
	nsoption_set_colour(sys_colour_AppWorkspace, 0x00eeeeee);
	nsoption_set_colour(sys_colour_Background, 0x00aa0000);
	nsoption_set_colour(sys_colour_ButtonFace, 0x00dddddd);
	nsoption_set_colour(sys_colour_ButtonHighlight, 0x00cccccc);
	nsoption_set_colour(sys_colour_ButtonShadow, 0x00bbbbbb);
	nsoption_set_colour(sys_colour_ButtonText, 0x00000000);
	nsoption_set_colour(sys_colour_CaptionText, 0x00000000);
	nsoption_set_colour(sys_colour_GrayText, 0x00777777);
	nsoption_set_colour(sys_colour_Highlight, 0x00ee0000);
	nsoption_set_colour(sys_colour_HighlightText, 0x00000000);
	nsoption_set_colour(sys_colour_InactiveBorder, 0x00000000);
	nsoption_set_colour(sys_colour_InactiveCaption, 0x00ffffff);
	nsoption_set_colour(sys_colour_InactiveCaptionText, 0x00cccccc);
	nsoption_set_colour(sys_colour_InfoBackground, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_InfoText, 0x00000000);
	nsoption_set_colour(sys_colour_Menu, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_MenuText, 0x00000000);
	nsoption_set_colour(sys_colour_Scrollbar, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_ThreeDDarkShadow, 0x00555555);
	nsoption_set_colour(sys_colour_ThreeDFace, 0x00dddddd);
	nsoption_set_colour(sys_colour_ThreeDHighlight, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_ThreeDLightShadow, 0x00999999);
	nsoption_set_colour(sys_colour_ThreeDShadow, 0x00777777);
	nsoption_set_colour(sys_colour_Window, 0x00aaaaaa);
	nsoption_set_colour(sys_colour_WindowFrame, 0x00000000);
	nsoption_set_colour(sys_colour_WindowText, 0x00000000);

	return NSERROR_OK;
}


/**
 * Ensures output logging stream is correctly configured
 */
static bool nslog_stream_configure(FILE *fptr)
{
        /* set log stream to be non-buffering */
	setbuf(fptr, NULL);

	return true;
}



static void framebuffer_poll(bool active)
{
	nsfb_event_t event;
	int timeout; /* timeout in miliseconds */

	/* run the scheduler and discover how long to wait for the next event */
	timeout = schedule_run();

	/* if active do not wait for event, return immediately */
	if (active)
		timeout = 0;

	/* if redraws are pending do not wait for event, return immediately */
	if (fbtk_get_redraw_pending(fbtk))
		timeout = 0;

	if (fbtk_event(fbtk, &event, timeout)) {
		if ((event.type == NSFB_EVENT_CONTROL) &&
		    (event.value.controlcode ==  NSFB_CONTROL_QUIT))
			netsurf_quit = true;
	}

	fbtk_redraw(fbtk);

}

static void gui_quit(void)
{
	LOG(("gui_quit"));

	urldb_save_cookies(nsoption_charp(cookie_jar));

	framebuffer_finalise();
}

/* called back when click in browser window */
static int
fb_browser_window_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);
	browser_mouse_state mouse;
	float scale = browser_window_get_scale(gw->bw);
	int x = (cbi->x + bwidget->scrollx) / scale;
	int y = (cbi->y + bwidget->scrolly) / scale;
	unsigned int time_now;
	static struct {
		enum { CLICK_SINGLE, CLICK_DOUBLE, CLICK_TRIPLE } type;
		unsigned int time;
	} last_click;

	if (cbi->event->type != NSFB_EVENT_KEY_DOWN &&
	    cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	LOG(("browser window clicked at %d,%d", cbi->x, cbi->y));

	switch (cbi->event->type) {
	case NSFB_EVENT_KEY_DOWN:
		switch (cbi->event->value.keycode) {
		case NSFB_KEY_MOUSE_1:
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_PRESS_1, x, y);
			gui_drag.state = GUI_DRAG_PRESSED;
			gui_drag.button = 1;
			gui_drag.x = x;
			gui_drag.y = y;
			break;

#if 0
		case NSFB_KEY_MOUSE_3:
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_PRESS_2, x, y);
			gui_drag.state = GUI_DRAG_PRESSED;
			gui_drag.button = 2;
			gui_drag.x = x;
			gui_drag.y = y;
			break;
#else

		case NSFB_KEY_MOUSE_2:
			/* scroll right */
			if (browser_window_scroll_at_point(gw->bw, x, y,
					100, 0) == false)
				widget_scroll_x(gw, 100, false);
			break;

		case NSFB_KEY_MOUSE_3:
			/* scroll left */
			if (browser_window_scroll_at_point(gw->bw, x, y,
					-100, 0) == false)
				widget_scroll_x(gw, -100, false);
			break;
#endif

		case NSFB_KEY_MOUSE_4:
			/* scroll up */
			if (browser_window_scroll_at_point(gw->bw, x, y,
					0, -100) == false) {
				/* XXX scroll a full screen */
				/* XXX widget_scroll_y(gw, -100, false); */
				widget_scroll_y(gw, -fbtk_get_height(
						gw->browser), false);
			}
			break;

		case NSFB_KEY_MOUSE_5:
			/* scroll down */
			if (browser_window_scroll_at_point(gw->bw, x, y,
					0, 100) == false) {
				/* XXX widget_scroll_y(gw, 100, false); */
				widget_scroll_y(gw, fbtk_get_height(
						gw->browser), false);
			}
			break;

		default:
			break;

		}

		break;
	case NSFB_EVENT_KEY_UP:

		mouse = 0;
		time_now = wallclock();

		switch (cbi->event->value.keycode) {
		case NSFB_KEY_MOUSE_1:
			if (gui_drag.state == GUI_DRAG_DRAG) {
				/* End of a drag, rather than click */

				if (gui_drag.grabbed_pointer) {
					/* need to ungrab pointer */
					fbtk_tgrab_pointer(widget);
					gui_drag.grabbed_pointer = false;
				}

				gui_drag.state = GUI_DRAG_NONE;

				/* Tell core */
				browser_window_mouse_track(gw->bw, 0, x, y);
				break;
			}
			/* This is a click;
			 * clear PRESSED state and pass to core */
			gui_drag.state = GUI_DRAG_NONE;
			mouse = BROWSER_MOUSE_CLICK_1;
			break;

#if 0
		case NSFB_KEY_MOUSE_3:
			if (gui_drag.state == GUI_DRAG_DRAG) {
				/* End of a drag, rather than click */
				gui_drag.state = GUI_DRAG_NONE;

				if (gui_drag.grabbed_pointer) {
					/* need to ungrab pointer */
					fbtk_tgrab_pointer(widget);
					gui_drag.grabbed_pointer = false;
				}

				/* Tell core */
				browser_window_mouse_track(gw->bw, 0, x, y);
				break;
			}
			/* This is a click;
			 * clear PRESSED state and pass to core */
			gui_drag.state = GUI_DRAG_NONE;
			mouse = BROWSER_MOUSE_CLICK_2;
			break;
#endif

		default:
			break;

		}

		/* Determine if it's a double or triple click, allowing
		 * 0.5 seconds (50cs) between clicks */
		if (time_now < last_click.time + 50 &&
				cbi->event->value.keycode != NSFB_KEY_MOUSE_4 &&
				cbi->event->value.keycode != NSFB_KEY_MOUSE_5) {
			if (last_click.type == CLICK_SINGLE) {
				/* Set double click */
				mouse |= BROWSER_MOUSE_DOUBLE_CLICK;
				last_click.type = CLICK_DOUBLE;

			} else if (last_click.type == CLICK_DOUBLE) {
				/* Set triple click */
				mouse |= BROWSER_MOUSE_TRIPLE_CLICK;
				last_click.type = CLICK_TRIPLE;
			} else {
				/* Set normal click */
				last_click.type = CLICK_SINGLE;
			}
		} else {
			last_click.type = CLICK_SINGLE;
		}

		if (mouse)
			browser_window_mouse_click(gw->bw, mouse, x, y);

		last_click.time = time_now;

		break;
	default:
		break;

	}
	return 1;
}

/* called back when movement in browser window */
static int
fb_browser_window_move(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	browser_mouse_state mouse = 0;
	struct gui_window *gw = cbi->context;
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);
	float scale = browser_window_get_scale(gw->bw);
	int x = (cbi->x + bwidget->scrollx) / scale;
	int y = (cbi->y + bwidget->scrolly) / scale;

	if (gui_drag.state == GUI_DRAG_PRESSED &&
			(abs(x - gui_drag.x) > 5 ||
			 abs(y - gui_drag.y) > 5)) {
		/* Drag started */
		if (gui_drag.button == 1) {
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_DRAG_1,
					gui_drag.x, gui_drag.y);
		} else {
			browser_window_mouse_click(gw->bw,
					BROWSER_MOUSE_DRAG_2,
					gui_drag.x, gui_drag.y);
		}
		gui_drag.grabbed_pointer = fbtk_tgrab_pointer(widget);
		gui_drag.state = GUI_DRAG_DRAG;
	}

	if (gui_drag.state == GUI_DRAG_DRAG) {
		/* set up mouse state */
		mouse |= BROWSER_MOUSE_DRAG_ON;

		if (gui_drag.button == 1)
			mouse |= BROWSER_MOUSE_HOLDING_1;
		else
			mouse |= BROWSER_MOUSE_HOLDING_2;
	}

	browser_window_mouse_track(gw->bw, mouse, x, y);

	return 0;
}


static int
fb_browser_window_input(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	static fbtk_modifier_type modifier = FBTK_MOD_CLEAR;
	int ucs4 = -1;

	LOG(("got value %d", cbi->event->value.keycode));

	switch (cbi->event->type) {
	case NSFB_EVENT_KEY_DOWN:
		switch (cbi->event->value.keycode) {

		case NSFB_KEY_DELETE:
			browser_window_key_press(gw->bw, KEY_DELETE_RIGHT);
			break;

		case NSFB_KEY_PAGEUP:
			if (browser_window_key_press(gw->bw,
					KEY_PAGE_UP) == false)
				widget_scroll_y(gw, -fbtk_get_height(
						gw->browser), false);
			break;

		case NSFB_KEY_PAGEDOWN:
			if (browser_window_key_press(gw->bw,
					KEY_PAGE_DOWN) == false)
				widget_scroll_y(gw, fbtk_get_height(
						gw->browser), false);
			break;

		case NSFB_KEY_RIGHT:
			if (modifier & FBTK_MOD_RCTRL ||
					modifier & FBTK_MOD_LCTRL) {
				/* CTRL held */
				if (browser_window_key_press(gw->bw,
						KEY_LINE_END) == false)
					widget_scroll_x(gw, INT_MAX, true);

			} else if (modifier & FBTK_MOD_RSHIFT ||
					modifier & FBTK_MOD_LSHIFT) {
				/* SHIFT held */
				if (browser_window_key_press(gw->bw,
						KEY_WORD_RIGHT) == false)
					widget_scroll_x(gw, fbtk_get_width(
						gw->browser), false);

			} else {
				/* no modifier */
				if (browser_window_key_press(gw->bw,
						KEY_RIGHT) == false)
					widget_scroll_x(gw, 100, false);
			}
			break;

		case NSFB_KEY_LEFT:
			if (modifier & FBTK_MOD_RCTRL ||
					modifier & FBTK_MOD_LCTRL) {
				/* CTRL held */
				if (browser_window_key_press(gw->bw,
						KEY_LINE_START) == false)
					widget_scroll_x(gw, 0, true);

			} else if (modifier & FBTK_MOD_RSHIFT ||
					modifier & FBTK_MOD_LSHIFT) {
				/* SHIFT held */
				if (browser_window_key_press(gw->bw,
						KEY_WORD_LEFT) == false)
					widget_scroll_x(gw, -fbtk_get_width(
						gw->browser), false);

			} else {
				/* no modifier */
				if (browser_window_key_press(gw->bw,
						KEY_LEFT) == false)
					widget_scroll_x(gw, -100, false);
			}
			break;

		case NSFB_KEY_UP:
			if (browser_window_key_press(gw->bw,
					KEY_UP) == false)
				widget_scroll_y(gw, -100, false);
			break;

		case NSFB_KEY_DOWN:
			if (browser_window_key_press(gw->bw,
					KEY_DOWN) == false)
				widget_scroll_y(gw, 100, false);
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

		case NSFB_KEY_y:
		case NSFB_KEY_z:
			if (cbi->event->value.keycode == NSFB_KEY_z &&
					(modifier & FBTK_MOD_RCTRL ||
					 modifier & FBTK_MOD_LCTRL) &&
					(modifier & FBTK_MOD_RSHIFT ||
					 modifier & FBTK_MOD_LSHIFT)) {
				/* Z pressed with CTRL and SHIFT held */
				browser_window_key_press(gw->bw, KEY_REDO);
				break;

			} else if (cbi->event->value.keycode == NSFB_KEY_z &&
					(modifier & FBTK_MOD_RCTRL ||
					 modifier & FBTK_MOD_LCTRL)) {
				/* Z pressed with CTRL held */
				browser_window_key_press(gw->bw, KEY_UNDO);
				break;

			} else if (cbi->event->value.keycode == NSFB_KEY_y &&
					(modifier & FBTK_MOD_RCTRL ||
					 modifier & FBTK_MOD_LCTRL)) {
				/* Y pressed with CTRL held */
				browser_window_key_press(gw->bw, KEY_REDO);
				break;
			}
			/* Z or Y pressed but not undo or redo;
			 * Fall through to default handling */

		default:
			ucs4 = fbtk_keycode_to_ucs4(cbi->event->value.keycode,
						    modifier);
			if (ucs4 != -1)
				browser_window_key_press(gw->bw, ucs4);
			break;
		}
		break;

	case NSFB_EVENT_KEY_UP:
		switch (cbi->event->value.keycode) {
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
		break;

	default:
		break;
	}

	return 0;
}

static void
fb_update_back_forward(struct gui_window *gw)
{
	struct browser_window *bw = gw->bw;

	fbtk_set_bitmap(gw->back,
			(browser_window_back_available(bw)) ?
			&left_arrow : &left_arrow_g);
	fbtk_set_bitmap(gw->forward,
			(browser_window_forward_available(bw)) ?
			&right_arrow : &right_arrow_g);
}

/* left icon click routine */
static int
fb_leftarrow_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_window *bw = gw->bw;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	if (browser_window_back_available(bw))
		browser_window_history_back(bw, false);

	fb_update_back_forward(gw);

	return 1;
}

/* right arrow icon click routine */
static int
fb_rightarrow_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;
	struct browser_window *bw = gw->bw;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	if (browser_window_forward_available(bw))
		browser_window_history_forward(bw, false);

	fb_update_back_forward(gw);
	return 1;

}

/* reload icon click routine */
static int
fb_reload_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct browser_window *bw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	browser_window_reload(bw, true);
	return 1;
}

/* stop icon click routine */
static int
fb_stop_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct browser_window *bw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	browser_window_stop(bw);
	return 0;
}

static int
fb_osk_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	map_osk();

	return 0;
}

/* close browser window icon click routine */
static int
fb_close_click(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	netsurf_quit = true;
	return 0;
}

static int
fb_scroll_callback(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;

	switch (cbi->type) {
	case FBTK_CBT_SCROLLY:
		widget_scroll_y(gw, cbi->y, true);
		break;

	case FBTK_CBT_SCROLLX:
		widget_scroll_x(gw, cbi->x, true);
		break;

	default:
		break;
	}
	return 0;
}

static int
fb_url_enter(void *pw, char *text)
{
	struct browser_window *bw = pw;
	nsurl *url;
	nserror error;

	error = nsurl_create(text, &url);
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	} else {
		browser_window_navigate(bw, url, NULL, BW_NAVIGATE_HISTORY,
				NULL, NULL, NULL);
		nsurl_unref(url);
	}

	return 0;
}

static int
fb_url_move(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	framebuffer_set_cursor(&caret_image);
	return 0;
}

static int
set_ptr_default_move(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	framebuffer_set_cursor(&pointer_image);
	return 0;
}

static int
fb_localhistory_btn_clik(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	struct gui_window *gw = cbi->context;

	if (cbi->event->type != NSFB_EVENT_KEY_UP)
		return 0;

	fb_localhistory_map(gw->localhistory);

	return 0;
}


/** Create a toolbar window and populate it with buttons. 
 *
 * The toolbar layout uses a character to define buttons type and position:
 * b - back
 * l - local history
 * f - forward
 * s - stop 
 * r - refresh
 * u - url bar expands to fit remaining space
 * t - throbber/activity indicator
 * c - close the current window
 *
 * The default layout is "blfsrut" there should be no more than a
 * single url bar entry or behaviour will be undefined.
 *
 * @param gw Parent window 
 * @param toolbar_height The height in pixels of the toolbar
 * @param padding The padding in pixels round each element of the toolbar
 * @param frame_col Frame colour.
 * @param toolbar_layout A string defining which buttons and controls
 *                       should be added to the toolbar. May be empty
 *                       string to disable the bar..
 * 
 */
static fbtk_widget_t *
create_toolbar(struct gui_window *gw, 
	       int toolbar_height, 
	       int padding, 
	       colour frame_col,
	       const char *toolbar_layout)
{
	fbtk_widget_t *toolbar;
	fbtk_widget_t *widget;

	int xpos; /* The position of the next widget. */
	int xlhs = 0; /* extent of the left hand side widgets */
	int xdir = 1; /* the direction of movement + or - 1 */
	const char *itmtype; /* type of the next item */

	if (toolbar_layout == NULL) {
		toolbar_layout = NSFB_TOOLBAR_DEFAULT_LAYOUT;
	}

	LOG(("Using toolbar layout %s", toolbar_layout));

	itmtype = toolbar_layout;

	if (*itmtype == 0) {
		return NULL;
	}

	toolbar = fbtk_create_window(gw->window, 0, 0, 0, 
				     toolbar_height, 
				     frame_col);

	if (toolbar == NULL) {
		return NULL;
	}

	fbtk_set_handler(toolbar, 
			 FBTK_CBT_POINTERENTER, 
			 set_ptr_default_move, 
			 NULL);


	xpos = padding;

	/* loop proceeds creating widget on the left hand side until
	 * it runs out of layout or encounters a url bar declaration
	 * wherupon it works backwards from the end of the layout
	 * untill the space left is for the url bar
	 */
	while ((itmtype >= toolbar_layout) && 
	       (*itmtype != 0) && 
	       (xdir !=0)) {

		LOG(("toolbar adding %c", *itmtype));


		switch (*itmtype) {

		case 'b': /* back */
			widget = fbtk_create_button(toolbar, 
						    (xdir == 1) ? xpos : 
						     xpos - left_arrow.width, 
						    padding, 
						    left_arrow.width, 
						    -padding, 
						    frame_col, 
						    &left_arrow, 
						    fb_leftarrow_click, 
						    gw);
			gw->back = widget; /* keep reference */
			break;

		case 'l': /* local history */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1) ? xpos : 
						     xpos - history_image.width,
						    padding,
						    history_image.width,
						    -padding,
						    frame_col,
						    &history_image,
						    fb_localhistory_btn_clik,
						    gw);
			break;

		case 'f': /* forward */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - right_arrow.width,
						    padding,
						    right_arrow.width,
						    -padding,
						    frame_col,
						    &right_arrow,
						    fb_rightarrow_click,
						    gw);
			gw->forward = widget;
			break;

		case 'c': /* close the current window */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - stop_image_g.width,
						    padding,
						    stop_image_g.width,
						    -padding,
						    frame_col,
						    &stop_image_g,
						    fb_close_click,
						    gw->bw);
			break;

		case 's': /* stop  */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - stop_image.width,
						    padding,
						    stop_image.width,
						    -padding,
						    frame_col,
						    &stop_image,
						    fb_stop_click,
						    gw->bw);
			break;

		case 'r': /* reload */
			widget = fbtk_create_button(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - reload.width,
						    padding,
						    reload.width,
						    -padding,
						    frame_col,
						    &reload,
						    fb_reload_click,
						    gw->bw);
			break;

		case 't': /* throbber/activity indicator */
			widget = fbtk_create_bitmap(toolbar,
						    (xdir == 1)?xpos : 
						     xpos - throbber0.width,
						    padding,
						    throbber0.width,
						    -padding,
						    frame_col, 
						    &throbber0);
			gw->throbber = widget;
			break;


		case 'u': /* url bar*/
			if (xdir == -1) {
				/* met the u going backwards add url
				 * now we know available extent 
				 */ 

				widget = fbtk_create_writable_text(toolbar,
						   xlhs,
						   padding,
						   xpos - xlhs,
						   -padding,
						   FB_COLOUR_WHITE,
						   FB_COLOUR_BLACK,
						   true,
						   fb_url_enter,
						   gw->bw);

				fbtk_set_handler(widget, 
						 FBTK_CBT_POINTERENTER, 
						 fb_url_move, gw->bw);

				gw->url = widget; /* keep reference */

				/* toolbar is complete */
				xdir = 0;
				break;
			}
			/* met url going forwards, note position and
			 * reverse direction 
			 */
			itmtype = toolbar_layout + strlen(toolbar_layout);
			xdir = -1;
			xlhs = xpos;
			xpos = (2 * fbtk_get_width(toolbar));
			widget = toolbar;
			break;

		default:
			widget = NULL;
			xdir = 0;
			LOG(("Unknown element %c in toolbar layout", *itmtype));
		        break;

		}

		if (widget != NULL) {
			xpos += (xdir * (fbtk_get_width(widget) + padding));
		}

		LOG(("xpos is %d",xpos));

		itmtype += xdir;
	}

	fbtk_set_mapping(toolbar, true);

	return toolbar;
}

/** Routine called when "stripped of focus" event occours for browser widget.
 *
 * @param widget The widget reciving "stripped of focus" event.
 * @param cbi The callback parameters.
 * @return The callback result.
 */
static int
fb_browser_window_strip_focus(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	fbtk_set_caret(widget, false, 0, 0, 0, NULL);

	return 0;
}

static void
create_browser_widget(struct gui_window *gw, int toolbar_height, int furniture_width)
{
	struct browser_widget_s *browser_widget;
	browser_widget = calloc(1, sizeof(struct browser_widget_s));

	gw->browser = fbtk_create_user(gw->window,
				       0,
				       toolbar_height,
				       -furniture_width,
				       -furniture_width,
				       browser_widget);

	fbtk_set_handler(gw->browser, FBTK_CBT_REDRAW, fb_browser_window_redraw, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_DESTROY, fb_browser_window_destroy, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_INPUT, fb_browser_window_input, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_CLICK, fb_browser_window_click, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_STRIP_FOCUS, fb_browser_window_strip_focus, gw);
	fbtk_set_handler(gw->browser, FBTK_CBT_POINTERMOVE, fb_browser_window_move, gw);
}

static void
create_normal_browser_window(struct gui_window *gw, int furniture_width)
{
	fbtk_widget_t *widget;
	fbtk_widget_t *toolbar;
	int statusbar_width = 0;
	int toolbar_height = nsoption_int(fb_toolbar_size);

	LOG(("Normal window"));

	gw->window = fbtk_create_window(fbtk, 0, 0, 0, 0, 0);

	statusbar_width = nsoption_int(toolbar_status_size) *
		fbtk_get_width(gw->window) / 10000;

	/* toolbar */
	toolbar = create_toolbar(gw, 
				 toolbar_height, 
				 2, 
				 FB_FRAME_COLOUR, 
				 nsoption_charp(fb_toolbar_layout));

	/* set the actually created toolbar height */
	if (toolbar != NULL) {
		toolbar_height = fbtk_get_height(toolbar);
	} else {
		toolbar_height = 0;
	}

	/* status bar */
	gw->status = fbtk_create_text(gw->window,
				      0,
				      fbtk_get_height(gw->window) - furniture_width,
				      statusbar_width, furniture_width,
				      FB_FRAME_COLOUR, FB_COLOUR_BLACK,
				      false);
	fbtk_set_handler(gw->status, FBTK_CBT_POINTERENTER, set_ptr_default_move, NULL);

	LOG(("status bar %p at %d,%d", gw->status, fbtk_get_absx(gw->status), fbtk_get_absy(gw->status)));

	/* create horizontal scrollbar */
	gw->hscroll = fbtk_create_hscroll(gw->window,
					  statusbar_width,
					  fbtk_get_height(gw->window) - furniture_width,
					  fbtk_get_width(gw->window) - statusbar_width - furniture_width,
					  furniture_width,
					  FB_SCROLL_COLOUR,
					  FB_FRAME_COLOUR,
					  fb_scroll_callback,
					  gw);

	/* fill bottom right area */

	if (nsoption_bool(fb_osk) == true) {
		widget = fbtk_create_text_button(gw->window,
						 fbtk_get_width(gw->window) - furniture_width,
						 fbtk_get_height(gw->window) - furniture_width,
						 furniture_width,
						 furniture_width,
						 FB_FRAME_COLOUR, FB_COLOUR_BLACK,
						 fb_osk_click,
						 NULL);
		widget = fbtk_create_button(gw->window,
				fbtk_get_width(gw->window) - furniture_width,
				fbtk_get_height(gw->window) - furniture_width,
				furniture_width,
				furniture_width,
				FB_FRAME_COLOUR,
				&osk_image,
				fb_osk_click,
				NULL);
	} else {
		widget = fbtk_create_fill(gw->window,
					  fbtk_get_width(gw->window) - furniture_width,
					  fbtk_get_height(gw->window) - furniture_width,
					  furniture_width,
					  furniture_width,
					  FB_FRAME_COLOUR);

		fbtk_set_handler(widget, FBTK_CBT_POINTERENTER, set_ptr_default_move, NULL);
	}

	/* create vertical scrollbar */
	gw->vscroll = fbtk_create_vscroll(gw->window,
					  fbtk_get_width(gw->window) - furniture_width,
					  toolbar_height,
					  furniture_width,
					  fbtk_get_height(gw->window) - toolbar_height - furniture_width,
					  FB_SCROLL_COLOUR,
					  FB_FRAME_COLOUR,
					  fb_scroll_callback,
					  gw);

	/* browser widget */
	create_browser_widget(gw, toolbar_height, nsoption_int(fb_furniture_size));

	/* Give browser_window's user widget input focus */
	fbtk_set_focus(gw->browser);
}


static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *gw;

	gw = calloc(1, sizeof(struct gui_window));

	if (gw == NULL)
		return NULL;

	/* associate the gui window with the underlying browser window
	 */
	gw->bw = bw;

	create_normal_browser_window(gw, nsoption_int(fb_furniture_size));
	gw->localhistory = fb_create_localhistory(bw, fbtk, nsoption_int(fb_furniture_size));

	/* map and request redraw of gui window */
	fbtk_set_mapping(gw->window, true);

	return gw;
}

static void
gui_window_destroy(struct gui_window *gw)
{
	fbtk_destroy_widget(gw->window);

	free(gw);
}

static void
gui_window_redraw_window(struct gui_window *g)
{
	fb_queue_redraw(g->browser, 0, 0, fbtk_get_width(g->browser), fbtk_get_height(g->browser) );
}

static void
gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);
	fb_queue_redraw(g->browser,
			rect->x0 - bwidget->scrollx,
			rect->y0 - bwidget->scrolly,
			rect->x1 - bwidget->scrollx,
			rect->y1 - bwidget->scrolly);
}

static bool
gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);
	float scale = browser_window_get_scale(g->bw);

	*sx = bwidget->scrollx / scale;
	*sy = bwidget->scrolly / scale;

	return true;
}

static void
gui_window_set_scroll(struct gui_window *gw, int sx, int sy)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(gw->browser);
	float scale = browser_window_get_scale(gw->bw);

	assert(bwidget);

	widget_scroll_x(gw, sx * scale, true);
	widget_scroll_y(gw, sy * scale, true);
}


static void
gui_window_get_dimensions(struct gui_window *g,
			  int *width,
			  int *height,
			  bool scaled)
{
	float scale = browser_window_get_scale(g->bw);

	*width = fbtk_get_width(g->browser);
	*height = fbtk_get_height(g->browser);

	if (scaled) {
		*width /= scale;
		*height /= scale;
	}
}

static void
gui_window_update_extent(struct gui_window *gw)
{
	int w, h;
	browser_window_get_extents(gw->bw, true, &w, &h);

	fbtk_set_scroll_parameters(gw->hscroll, 0, w,
			fbtk_get_width(gw->browser), 100);

	fbtk_set_scroll_parameters(gw->vscroll, 0, h,
			fbtk_get_height(gw->browser), 100);
}

static void
gui_window_set_status(struct gui_window *g, const char *text)
{
	fbtk_set_text(g->status, text);
}

static void
gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	switch (shape) {
	case GUI_POINTER_POINT:
		framebuffer_set_cursor(&hand_image);
		break;

	case GUI_POINTER_CARET:
		framebuffer_set_cursor(&caret_image);
		break;

	case GUI_POINTER_MENU:
		framebuffer_set_cursor(&menu_image);
		break;

	case GUI_POINTER_PROGRESS:
		framebuffer_set_cursor(&progress_image);
		break;

	case GUI_POINTER_MOVE:
		framebuffer_set_cursor(&move_image);
		break;

	default:
		framebuffer_set_cursor(&pointer_image);
		break;
	}
}

static void
gui_window_set_url(struct gui_window *g, const char *url)
{
	fbtk_set_text(g->url, url);
}

static void
throbber_advance(void *pw)
{
	struct gui_window *g = pw;
	struct fbtk_bitmap *image;

	switch (g->throbber_index) {
	case 0:
		image = &throbber1;
		g->throbber_index = 1;
		break;

	case 1:
		image = &throbber2;
		g->throbber_index = 2;
		break;

	case 2:
		image = &throbber3;
		g->throbber_index = 3;
		break;

	case 3:
		image = &throbber4;
		g->throbber_index = 4;
		break;

	case 4:
		image = &throbber5;
		g->throbber_index = 5;
		break;

	case 5:
		image = &throbber6;
		g->throbber_index = 6;
		break;

	case 6:
		image = &throbber7;
		g->throbber_index = 7;
		break;

	case 7:
		image = &throbber8;
		g->throbber_index = 0;
		break;

	default:
		return;
	}

	if (g->throbber_index >= 0) {
		fbtk_set_bitmap(g->throbber, image);
		framebuffer_schedule(100, throbber_advance, g);
	}
}

static void
gui_window_start_throbber(struct gui_window *g)
{
	g->throbber_index = 0;
	framebuffer_schedule(100, throbber_advance, g);
}

static void
gui_window_stop_throbber(struct gui_window *gw)
{
	gw->throbber_index = -1;
	fbtk_set_bitmap(gw->throbber, &throbber0);

	fb_update_back_forward(gw);

}

static void
gui_window_remove_caret_cb(fbtk_widget_t *widget)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(widget);
	int c_x, c_y, c_h;

	if (fbtk_get_caret(widget, &c_x, &c_y, &c_h)) {
		/* browser window already had caret:
		 * redraw its area to remove it first */
		fb_queue_redraw(widget,
				c_x - bwidget->scrollx,
				c_y - bwidget->scrolly,
				c_x + 1 - bwidget->scrollx,
				c_y + c_h - bwidget->scrolly);
	}
}

static void
gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip)
{
	struct browser_widget_s *bwidget = fbtk_get_userpw(g->browser);

	/* set new pos */
	fbtk_set_caret(g->browser, true, x, y, height,
			gui_window_remove_caret_cb);

	/* redraw new caret pos */
	fb_queue_redraw(g->browser,
			x - bwidget->scrollx,
			y - bwidget->scrolly,
			x + 1 - bwidget->scrollx,
			y + height - bwidget->scrolly);
}

static void
gui_window_remove_caret(struct gui_window *g)
{
	int c_x, c_y, c_h;

	if (fbtk_get_caret(g->browser, &c_x, &c_y, &c_h)) {
		/* browser window owns the caret, so can remove it */
		fbtk_set_caret(g->browser, false, 0, 0, 0, NULL);
	}
}


static struct gui_window_table framebuffer_window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.redraw = gui_window_redraw_window,
	.update = gui_window_update_box,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.update_extent = gui_window_update_extent,

	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
	.remove_caret = gui_window_remove_caret,
	.start_throbber = gui_window_start_throbber,
	.stop_throbber = gui_window_stop_throbber,
};


static struct gui_browser_table framebuffer_browser_table = {
	.poll = framebuffer_poll,
	.schedule = framebuffer_schedule,

	.quit = gui_quit,
};

/** Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int
main(int argc, char** argv)
{
	struct browser_window *bw;
	char *options;
	char *messages;
	nsurl *url;
	nserror ret;
	nsfb_t *nsfb;
	struct gui_table framebuffer_gui_table = {
		.browser = &framebuffer_browser_table,
		.window = &framebuffer_window_table,
		.clipboard = framebuffer_clipboard_table,
		.fetch = framebuffer_fetch_table,
		.utf8 = framebuffer_utf8_table,
	};

	respaths = fb_init_resource(NETSURF_FB_RESPATH":"NETSURF_FB_FONTPATH);

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(nslog_stream_configure, &argc, argv);

	/* user options setup */
	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		die("Options failed to initialise");
	}
	options = filepath_find(respaths, "Choices");
	nsoption_read(options, nsoptions);
	free(options);
	nsoption_commandline(&argc, argv, nsoptions);

	/* common initialisation */
	messages = filepath_find(respaths, "Messages");
	ret = netsurf_init(messages, &framebuffer_gui_table);
	free(messages);
	if (ret != NSERROR_OK) {
		die("NetSurf failed to initialise");
	}

	/* Override, since we have no support for non-core SELECT menu */
	nsoption_set_bool(core_select_menu, true);

	if (process_cmdline(argc,argv) != true)
		die("unable to process command line.\n");

	nsfb = framebuffer_initialise(fename, fewidth, feheight, febpp);
	if (nsfb == NULL)
		die("Unable to initialise framebuffer");

	framebuffer_set_cursor(&pointer_image);

	if (fb_font_init() == false)
		die("Unable to initialise the font system");

	fbtk = fbtk_init(nsfb);

	fbtk_enable_oskb(fbtk);

	urldb_load_cookies(nsoption_charp(cookie_file));

	/* create an initial browser window */

	LOG(("calling browser_window_create"));

	ret = nsurl_create(feurl, &url);
	if (ret == NSERROR_OK) {
		ret = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      &bw);
		nsurl_unref(url);
	}
	if (ret != NSERROR_OK) {
		warn_user(messages_get_errorcode(ret), 0);
	} else {
		netsurf_main_loop();

		browser_window_destroy(bw);
	}

	netsurf_exit();

	if (fb_font_finalise() == false)
		LOG(("Font finalisation failed."));

	/* finalise options */
	nsoption_finalise(nsoptions, nsoptions_default);

	return 0;
}


/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
