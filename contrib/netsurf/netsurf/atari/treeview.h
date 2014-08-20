/*
 * Copyright 2013 Ole Loots <ole@monochrom.net>
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

#ifndef NSATARI_TREEVIEW_H
#define NSATARI_TREEVIEW_H

#include "desktop/core_window.h"
#include "atari/gui.h"
#include "atari/gemtk/gemtk.h"

/**
 * Default AES Window widgets for a treeview window, can be passed to
 * atari_treeview_create as the flags parameter to have an standardized treeview
 * window.
 */
#define ATARI_TREEVIEW_WIDGETS (CLOSER | MOVER | SIZER| NAME | FULLER | \
								SMALLER | VSLIDE | HSLIDE | UPARROW | DNARROW \
								| LFARROW | RTARROW)

enum treeview_area_e {
	TREEVIEW_AREA_WORK = 0,
	TREEVIEW_AREA_TOOLBAR,
	TREEVIEW_AREA_CONTENT
};

struct core_window;
struct atari_treeview_window;

/**
 * The atari treeview implementation wraps the core_window callbacks
 * So that it can process parameters and then it passes the event further
 * To the specific implementation window.
 * These callbacks must be implemented by any atari treeview window.
 */

// TODO: add drag_status callback
typedef nserror (*atari_treeview_init2_callback)(struct core_window *cw,
				struct core_window_callback_table * default_callbacks);
typedef void (*atari_treeview_finish_callback)(struct core_window *cw);
typedef void (*atari_treeview_keypress_callback)(struct core_window *cw,
												uint32_t ucs4);
typedef void (*atari_treeview_mouse_action_callback)(struct core_window *cw,
												browser_mouse_state mouse,
												int x, int y);
typedef void (*atari_treeview_draw_callback)(struct core_window *cw, int x,
											int y, struct rect *clip,
											const struct redraw_context *ctx);

struct atari_treeview_callbacks {
	atari_treeview_init2_callback init_phase2;
	atari_treeview_finish_callback finish;
	atari_treeview_draw_callback draw;
	atari_treeview_keypress_callback keypress;
	atari_treeview_mouse_action_callback mouse_action;
	gemtk_wm_event_handler_f gemtk_user_func;
};

/**
 * Initalize an window to be an treeview window.
 *
*/
struct core_window *atari_treeview_create(GUIWIN *win,
									struct atari_treeview_callbacks * callbacks,
									void * user_data, uint32_t flags);
/**
 * Free the Treeview, but not the gemtk window used for the treeview.
*/
void atari_treeview_delete(struct core_window *cw);

/**
 * Open the treeview window.
 */
void atari_treeview_open(struct core_window *cw, GRECT *pos);

/**
 * Returns the window "open" state.
*/
bool atari_treeview_is_open(struct core_window *cw);

/**
 * Closes (hides) the treeview window.
*/
void atari_treeview_close(struct core_window *cw);

/**
 * Get the window manager window handle
 */

GUIWIN * atari_treeview_get_gemtk_window(struct core_window *cw);

/**
 * Get an specific area inside the window.
*/
void atari_treeview_get_grect(struct core_window *cw, enum treeview_area_e mode,
									GRECT *dest);

/**
 * Process all pending redraw requests for a single treeview
 */
void atari_treeview_redraw(struct core_window *cw);

/**
 * Attach arbitary user data to the treeview.
*/
void atari_treeview_set_user_data(struct core_window *cw,
								void *user_data_ptr);

/**
 * Return the arbitary user data set by atari_treeview_set_user_data()
 */
void *atari_treeview_get_user_data(struct core_window *cw);

/**
 * Process all redraw request of all open Treeview windows
*/
void atari_treeview_flush_redraws(void);

#endif //NSATARI_TREEVIEW_H

