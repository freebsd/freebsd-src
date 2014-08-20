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
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "utils/nsoption.h"
#include "desktop/hotlist.h"
#include "desktop/tree.h"
#include "desktop/gui.h"
#include "desktop/core_window.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/url.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/treeview.h"
#include "atari/hotlist.h"
#include "atari/findfile.h"
#include "atari/gemtk/gemtk.h"
#include "atari/res/netsurf.rsh"

extern GRECT desk_area;

struct atari_hotlist hl;

/* Setup Atari Treeview Callbacks: */
static nserror atari_hotlist_init_phase2(struct core_window *cw,
				struct core_window_callback_table * default_callbacks);
static void atari_hotlist_finish(struct core_window *cw);
static void atari_hotlist_keypress(struct core_window *cw,
												uint32_t ucs4);
static void atari_hotlist_mouse_action(struct core_window *cw,
												browser_mouse_state mouse,
												int x, int y);
static void atari_hotlist_draw(struct core_window *cw, int x,
											int y, struct rect *clip,
											const struct redraw_context *ctx);
static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8]);

static struct atari_treeview_callbacks atari_hotlist_treeview_callbacks = {
	.init_phase2 = atari_hotlist_init_phase2,
	.finish = atari_hotlist_finish,
	.draw = atari_hotlist_draw,
	.keypress = atari_hotlist_keypress,
	.mouse_action = atari_hotlist_mouse_action,
	.gemtk_user_func = handle_event
};

static nserror atari_hotlist_init_phase2(struct core_window *cw,
								struct core_window_callback_table *cb_t)
{
	LOG((""));
	return(hotlist_init(cb_t, cw, hl.path));
}

static void atari_hotlist_finish(struct core_window *cw)
{
	LOG((""));
	hotlist_fini(hl.path);
}

static void atari_hotlist_draw(struct core_window *cw, int x,
											int y, struct rect *clip,
											const struct redraw_context *ctx)
{
	hotlist_redraw(x, y, clip, ctx);
}

static void atari_hotlist_keypress(struct core_window *cw, uint32_t ucs4)
{
	GUIWIN *gemtk_win;
	GRECT area;
	LOG(("ucs4: %lu\n", ucs4));
	hotlist_keypress(ucs4);
	gemtk_win = atari_treeview_get_gemtk_window(cw);
	atari_treeview_get_grect(cw, TREEVIEW_AREA_CONTENT, &area);
	//gemtk_wm_exec_redraw(gemtk_win, &area);
}

static void atari_hotlist_mouse_action(struct core_window *cw,
												browser_mouse_state mouse,
												int x, int y)
{
	LOG(("x:  %d, y: %d\n", x, y));
	if((mouse & BROWSER_MOUSE_HOVER) && hotlist_has_selection()){
		hotlist_mouse_action(mouse, x, y);
	} else {
		hotlist_mouse_action(mouse, x, y);
	}

}



static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
	char *cur_url = NULL;
	char *cur_title = NULL;
	short retval = 0;
	struct atari_treeview_window *tv = NULL;
	struct core_window *cw;
	struct gui_window * gw;
	OBJECT *toolbar;
	GRECT tb_area;
	GUIWIN * gemtk_win;

	LOG((""));

	tv = (struct atari_treeview_window*) gemtk_wm_get_user_data(win);
	cw = (struct core_window *)tv;

	if(ev_out->emo_events & MU_MESAG){
		switch (msg[0]) {

			case WM_TOOLBAR:
				LOG(("WM_TOOLBAR"));

				toolbar = gemtk_obj_get_tree(TOOLBAR_HOTLIST);

				assert(toolbar);
				assert(tv);

				switch	(msg[4]) {
					case TOOLBAR_HOTLIST_CREATE_FOLDER:
						hotlist_add_folder(NULL, 0, 0);
						break;

					case TOOLBAR_HOTLIST_ADD:
						gw = gui_get_input_window();
						if(gw && gw->browser){
							cur_url = gui_window_get_url(gw);
							cur_title = gui_window_get_title(gw);
							// TODO: read language string.
							cur_title = (cur_title ? cur_title : (char*)"New bookmark");
						} else {
							cur_url = (char*)"http://www";
						}
						atari_hotlist_add_page(cur_url, cur_title);
						break;

					case TOOLBAR_HOTLIST_DELETE:
						hotlist_keypress(KEY_DELETE_LEFT);
						break;

					case TOOLBAR_HOTLIST_EDIT:
						hotlist_edit_selection();
						break;
				}

				gemtk_win = atari_treeview_get_gemtk_window(cw);
				assert(gemtk_win);
				toolbar[msg[4]].ob_state &= ~OS_SELECTED;
				atari_treeview_get_grect(cw, TREEVIEW_AREA_TOOLBAR, &tb_area);
				evnt_timer(150);
				gemtk_wm_exec_redraw(gemtk_win, &tb_area);
				retval = 1;
			break;

			case WM_CLOSED:
				atari_hotlist_close();
				retval = 1;
			break;

			default: break;
		}
	}

	return(retval);
}



void atari_hotlist_init(void)
{
	if (hl.init == false) {
		if( strcmp(nsoption_charp(hotlist_file), "") == 0 ){
			atari_find_resource( (char*)&hl.path, "hotlist", "hotlist" );
		} else {
			strncpy( (char*)&hl.path, nsoption_charp(hotlist_file), PATH_MAX-1 );
		}

		LOG(("Hotlist: %s",  (char*)&hl.path ));

		if( hl.window == NULL ){
			int flags = ATARI_TREEVIEW_WIDGETS;
			short handle = -1;
			GRECT desk;
			OBJECT * tree = gemtk_obj_get_tree(TOOLBAR_HOTLIST);
			assert( tree );

			handle = wind_create(flags, 0, 0, desk_area.g_w, desk_area.g_h);
			hl.window = gemtk_wm_add(handle, GEMTK_WM_FLAG_DEFAULTS, NULL);
			if( hl.window == NULL ) {
				gemtk_msg_box_show(GEMTK_MSG_BOX_ALERT,
									"Failed to allocate Hotlist");
				return;
			}
			wind_set_str(handle, WF_NAME, (char*)messages_get("Hotlist"));
			gemtk_wm_set_toolbar(hl.window, tree, 0, 0);
			gemtk_wm_unlink(hl.window);
			tree_hotlist_path = (const char*)&hl.path;

			hl.tv = atari_treeview_create(hl.window, &atari_hotlist_treeview_callbacks,
									NULL, flags);

			if (hl.tv == NULL) {
				/* handle it properly, clean up previous allocs */
				LOG(("Failed to allocate treeview"));
				return;
			}

		} else {

		}
	}
	hl.init = true;
}

void atari_hotlist_open(void)
{
	assert(hl.init);
	if (hl.init == false) {
		return;
	}

	if (atari_treeview_is_open(hl.tv) == false) {

	    GRECT pos;
	    pos.g_x = desk_area.g_w - desk_area.g_w / 4;
	    pos.g_y = desk_area.g_y;
	    pos.g_w = desk_area.g_w / 4;
	    pos.g_h = desk_area.g_h;

		atari_treeview_open(hl.tv, &pos);
	} else {
		wind_set(gemtk_wm_get_handle(hl.window), WF_TOP, 1, 0, 0, 0);
	}
}

void atari_hotlist_close(void)
{
	atari_treeview_close(hl.tv);
}

void atari_hotlist_destroy(void)
{

	if( hl.init == false) {
		return;
	}
	if( hl.window != NULL ) {
		if (atari_treeview_is_open(hl.tv))
			atari_hotlist_close();
		wind_delete(gemtk_wm_get_handle(hl.window));
		gemtk_wm_remove(hl.window);
		hl.window = NULL;
		atari_treeview_delete(hl.tv);
		hl.init = false;
	}
	LOG(("done"));
}

void atari_hotlist_redraw(void)
{
	atari_treeview_redraw(hl.tv);
}

struct node;

void atari_hotlist_add_page( const char * url, const char * title )
{
	struct node * root;
	struct node * selected = NULL;
	struct node * folder = NULL;
	nsurl *nsurl;

	if(hl.tv == NULL)
		return;

	atari_hotlist_open();

	if (nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

    if (hotlist_has_url(nsurl)) {
        LOG(("URL already added as Bookmark"));
        nsurl_unref(nsurl);
        return;
    }

	/* doesn't look nice:
	if( hl.tv->click.x >= 0 && hl.tv->click.y >= 0 ){
		hotlist_add_entry( nsurl, title, true, hl.tv->click.y );
	} else {

	}*/
	//hotlist_add_url(nsurl);
	hotlist_add_entry(nsurl, title, 0, 0);
	nsurl_unref(nsurl);
}

