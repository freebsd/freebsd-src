/*
 * Copyright 2010 Ole Loots <ole@monochrom.net>
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

#ifndef NS_ATARI_BROWSER_WIN_H
#define NS_ATARI_BROWSER_WIN_H

#include <atari/gui.h>

#define GEMTK_WM_VISIBLE(gw) (gw->root->handle->status & WS_OPEN)
#define GEMWIN_VISIBLE(win) (win->status & WS_OPEN)

#define WIDGET_STATUSBAR  	0x1
#define WIDGET_TOOLBAR    	0x2
#define WIDGET_SCROLL	  	0x4
#define WIDGET_RESIZE	  	0x8
#define WIN_TOP				0x100

enum browser_area_e {
	BROWSER_AREA_CONTENT = 1,
	BROWSER_AREA_STATUSBAR,
	BROWSER_AREA_TOOLBAR,
	BROWSER_AREA_URL_INPUT,
	BROWSER_AREA_SEARCH
};


/* -------------------------------------------------------------------------- */
/* Public module functions:                                                   */
/* -------------------------------------------------------------------------- */

/*	Creates an normal Browser window with [toolbar], [statusbar] */
int window_create(struct gui_window * gw,
				struct browser_window * bw,
				struct gui_window * existing,
				unsigned long flags );
/* Destroys WinDom part of gui_window */
int window_destroy(ROOTWIN *rootwin);

/** show the window at specified position and make gw the active tab. */
void window_open(ROOTWIN *rootwin, struct gui_window *gw, GRECT pos);

void window_snd_redraw(ROOTWIN *rootwin, short x, short y, short w, short h );
/* Update Shade / Unshade state of the fwd/back buttons*/
void window_update_back_forward(struct s_gui_win_root * rootwin);
/* set root browser component: */
void window_attach_browser(ROOTWIN *rootwin, CMP_BROWSER b);

/* set focus element */
void window_set_focus(ROOTWIN *rootwin, enum focus_element_type type,
					void * element );
/* Shade / Unshade the forward/back bt. of toolbar, depending on history.*/
bool window_widget_has_focus(ROOTWIN *rootwin, enum focus_element_type t,
							void * element);
bool window_url_widget_has_focus(ROOTWIN *rootwin);
void window_set_url(ROOTWIN *rootwin, const char * text);
void window_set_stauts(ROOTWIN *rootwin, char * text);
void window_set_title(ROOTWIN *rootwin, char * text);
void window_set_content_size(ROOTWIN *rootwin, int w, int h);
void window_set_icon(ROOTWIN *rootwin, struct bitmap * bmp );
void window_set_active_gui_window(ROOTWIN *rootwin, struct gui_window *gw);
void window_restore_active_gui_window(ROOTWIN *rootwin);
void window_open_search(ROOTWIN *rootwin, bool reformat);
void window_close_search(ROOTWIN *rootwin);
void window_scroll_by(ROOTWIN *rootwin, int x, int y);
void window_schedule_redraw_grect(ROOTWIN *rootwin, GRECT *area);
void window_process_redraws(ROOTWIN * rootwin);
void window_place_caret(ROOTWIN *rootwin, short mode, int content_x,
						int content_y, int h, GRECT *work);
struct gui_window * window_get_active_gui_window(ROOTWIN * rootwin);
void window_get_scroll(ROOTWIN *rootwin, int *x, int *y);
void window_get_grect(ROOTWIN *rootwin, enum browser_area_e which, GRECT *d);
void window_redraw_favicon(struct s_gui_win_root * rootwin, GRECT *clip);
void window_unref_gui_window(ROOTWIN *rootwin, struct gui_window *gw);
bool window_key_input(unsigned short kcode, unsigned short kstate,
						unsigned short nkc);


/* -------------------------------------------------------------------------- */
/* Public event handlers:                                                     */
/* -------------------------------------------------------------------------- */

#endif
