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

#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "desktop/browser.h"
#include "content/urldb.h"
#include "content/hlcache.h"
#include "desktop/sslcert_viewer.h"
#include "desktop/gui.h"
#include "desktop/core_window.h"
#include "utils/nsoption.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"

#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/treeview.h"
#include "atari/certview.h"
#include "atari/findfile.h"
#include "atari/gemtk/gemtk.h"
#include "atari/res/netsurf.rsh"

extern GRECT desk_area;


/* Setup Atari Treeview Callbacks: */
static nserror atari_sslcert_viewer_init_phase2(struct core_window *cw,
				struct core_window_callback_table * default_callbacks);
static void atari_sslcert_viewer_finish(struct core_window *cw);
static void atari_sslcert_viewer_keypress(struct core_window *cw,
												uint32_t ucs4);
static void atari_sslcert_viewer_mouse_action(struct core_window *cw,
												browser_mouse_state mouse,
												int x, int y);
static void atari_sslcert_viewer_draw(struct core_window *cw, int x,
											int y, struct rect *clip,
											const struct redraw_context *ctx);
static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8]);

static struct atari_treeview_callbacks atari_sslcert_viewer_treeview_callbacks = {
	.init_phase2 = 		atari_sslcert_viewer_init_phase2,
	.finish = 			atari_sslcert_viewer_finish,
	.draw = 			atari_sslcert_viewer_draw,
	.keypress = 		atari_sslcert_viewer_keypress,
	.mouse_action = 	atari_sslcert_viewer_mouse_action,
	.gemtk_user_func = 	handle_event
};

/* static functions */
static void atari_sslcert_viewer_destroy(struct atari_sslcert_viewer_s * cvwin);


static nserror atari_sslcert_viewer_init_phase2(struct core_window *cw,
								struct core_window_callback_table *cb_t)
{
	struct atari_sslcert_viewer_s *cvwin;
	struct sslcert_session_data *ssl_d;

	cvwin = (struct atari_sslcert_viewer_s *)atari_treeview_get_user_data(cw);

	assert(cvwin);

	ssl_d = cvwin->ssl_session_data;

	assert(ssl_d);

	LOG((""));

	return(sslcert_viewer_init(cb_t, cw, ssl_d));
}

static void atari_sslcert_viewer_finish(struct core_window *cw)
{
	struct atari_sslcert_viewer_s *cvwin;

	assert(cw);

	cvwin = (struct atari_sslcert_viewer_s *)atari_treeview_get_user_data(cw);

	/* This will also free the session data: */
	sslcert_viewer_fini(cvwin->ssl_session_data);

	LOG((""));
}

static void atari_sslcert_viewer_draw(struct core_window *cw, int x,
											int y, struct rect *clip,
											const struct redraw_context *ctx)
{
	struct atari_sslcert_viewer_s *cvwin;

	assert(cw);

	cvwin = (struct atari_sslcert_viewer_s *)atari_treeview_get_user_data(cw);

	assert(cvwin);

	sslcert_viewer_redraw(cvwin->ssl_session_data, x, y, clip, ctx);
}

static void atari_sslcert_viewer_keypress(struct core_window *cw, uint32_t ucs4)
{
	struct atari_sslcert_viewer_s *cvwin;

	assert(cw);

	cvwin = (struct atari_sslcert_viewer_s *)atari_treeview_get_user_data(cw);

	LOG(("ucs4: %lu\n", ucs4));
	sslcert_viewer_keypress(cvwin->ssl_session_data, ucs4);
}

static void atari_sslcert_viewer_mouse_action(struct core_window *cw,
												browser_mouse_state mouse,
												int x, int y)
{
	struct atari_sslcert_viewer_s *cvwin;

	assert(cw);

	cvwin = (struct atari_sslcert_viewer_s *)atari_treeview_get_user_data(cw);

	if ((mouse & BROWSER_MOUSE_HOVER)) {
		sslcert_viewer_mouse_action(cvwin->ssl_session_data, mouse, x, y);
	} else {
		sslcert_viewer_mouse_action(cvwin->ssl_session_data, mouse, x, y);
	}
}


static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	struct core_window *tv=NULL;
	GRECT tb_area;
	GUIWIN * gemtk_win;
	struct gui_window * gw;
	struct atari_sslcert_viewer_s *cvwin = NULL;
	char *cur_url = NULL;
	char *cur_title = NULL;
	short retval = 0;
	OBJECT *toolbar;

	LOG((""));

	if(ev_out->emo_events & MU_MESAG){
		switch (msg[0]) {

			case WM_TOOLBAR:
				toolbar = gemtk_obj_get_tree(TOOLBAR_SSL_CERT);
				LOG(("CERTVIEWER WM_TOOLBAR"));
				tv = (struct core_window*) gemtk_wm_get_user_data(win);
				assert(tv);
				cvwin = (struct atari_sslcert_viewer_s *)
							atari_treeview_get_user_data(tv);
				switch	(msg[4]) {

					case TOOLBAR_SSL_CERT_TRUSTED:

						if (toolbar[msg[4]].ob_state & OS_SELECTED) {

						} else {

						}
						break;
				}


				gemtk_win = atari_treeview_get_gemtk_window(tv);
				assert(gemtk_win);
				//gemtk_obj_get_tree(TOOLBAR_HOTLIST)[msg[4]].ob_state &= ~OS_SELECTED;
				atari_treeview_get_grect(tv, TREEVIEW_AREA_TOOLBAR, &tb_area);
				evnt_timer(150);
				gemtk_wm_exec_redraw(gemtk_win, &tb_area);
				retval = 1;
			break;

			case WM_CLOSED:
			// TODO set perrmissions
				toolbar = gemtk_obj_get_tree(TOOLBAR_SSL_CERT);
				tv = (struct core_window*) gemtk_wm_get_user_data(win);
				assert(tv);
				cvwin = (struct atari_sslcert_viewer_s *)
							atari_treeview_get_user_data(tv);
				if (toolbar[TOOLBAR_SSL_CERT_TRUSTED].ob_state & OS_SELECTED) {
					sslcert_viewer_accept(cvwin->ssl_session_data);
				} else {
					sslcert_viewer_reject(cvwin->ssl_session_data);
				}
				atari_sslcert_viewer_destroy(cvwin);
				retval = 1;
			break;

			default: break;
		}
	}

	return(retval);
}

static void atari_sslcert_viewer_init(struct atari_sslcert_viewer_s * cvwin,
									struct sslcert_session_data *ssl_d)
{
	assert(cvwin->init == false);
	assert(cvwin->window == NULL);
	assert(cvwin->tv == NULL);

	int flags = ATARI_TREEVIEW_WIDGETS;
	short handle = -1;
	GRECT desk;
	OBJECT * tree = gemtk_obj_get_tree(TOOLBAR_SSL_CERT);
	assert( tree );

	handle = wind_create(flags, 0, 0, desk_area.g_w, desk_area.g_h);
	cvwin->window = gemtk_wm_add(handle,
										GEMTK_WM_FLAG_DEFAULTS, NULL);
	if (cvwin->window == NULL ) {
		gemtk_msg_box_show(GEMTK_MSG_BOX_ALERT,
							"Failed to allocate Treeview:\nCertviewer");
		return;
	}
	wind_set_str(handle, WF_NAME, (char*)"SSL Certificate");
	gemtk_wm_set_toolbar(cvwin->window, tree, 0, 0);
	gemtk_wm_unlink(cvwin->window);

	cvwin->ssl_session_data = ssl_d;
	cvwin->tv = atari_treeview_create(cvwin->window,
									&atari_sslcert_viewer_treeview_callbacks,
									cvwin, flags);

	if (cvwin->tv == NULL) {
		/* handle it properly, clean up previous allocs */
		LOG(("Failed to allocate treeview"));
		return;
	}

	cvwin->init = true;
}

/*
* documented in certview.h
*/
void atari_sslcert_viewer_open(struct sslcert_session_data *ssl_d)
{
	struct atari_sslcert_viewer_s * cvwin;

	cvwin = calloc(1, sizeof(struct atari_sslcert_viewer_s));

	assert(cvwin);

	atari_sslcert_viewer_init(cvwin, ssl_d);

	if (atari_treeview_is_open(cvwin->tv) == false) {

	    GRECT pos;
	    pos.g_x = desk_area.g_w - desk_area.g_w / 4;
	    pos.g_y = desk_area.g_y;
	    pos.g_w = desk_area.g_w / 4;
	    pos.g_h = desk_area.g_h;

		atari_treeview_open(cvwin->tv, &pos);
	} else {
		wind_set(gemtk_wm_get_handle(cvwin->window), WF_TOP, 1, 0,
				0, 0);
	}
}


static void atari_sslcert_viewer_destroy(struct atari_sslcert_viewer_s * cvwin)
{
	assert(cvwin);
	assert(cvwin->init);
	assert(cvwin->window);

	LOG((""));

	if (atari_treeview_is_open(cvwin->tv))
		atari_treeview_close(cvwin->tv);
	wind_delete(gemtk_wm_get_handle(cvwin->window));
	gemtk_wm_remove(cvwin->window);
	cvwin->window = NULL;
	atari_treeview_delete(cvwin->tv);
	free(cvwin);
	LOG(("done"));
}

