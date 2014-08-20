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
 *
 * Module Description:
 *
 * This File implements the NetSurf Browser window, or passed functionality to
 * the appropriate widget's.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <osbind.h>

#include <mt_gem.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/plotters.h"
#include "desktop/textinput.h"
#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"

#include "atari/res/netsurf.rsh"
#include "atari/gemtk/gemtk.h"
#include "atari/ctxmenu.h"
#include "atari/gui.h"
#include "atari/rootwin.h"
#include "atari/misc.h"
#include "atari/plot/plot.h"
#include "atari/toolbar.h"
#include "atari/statusbar.h"
#include "atari/search.h"
#include "atari/osspec.h"
#include "atari/encoding.h"
#include "atari/redrawslots.h"
#include "atari/toolbar.h"
#include "atari/findfile.h"

extern struct gui_window *input_window;
extern EVMULT_OUT aes_event_out;
extern GRECT desk_area;

struct rootwin_data_s {
    struct s_gui_win_root *rootwin;
};

/* -------------------------------------------------------------------------- */
/* Static module event handlers                                               */
/* -------------------------------------------------------------------------- */
static void on_redraw(ROOTWIN *rootwin, short msg[8]);
static void on_resized(ROOTWIN *rootwin);
static void on_file_dropped(ROOTWIN *rootwin, short msg[8]);
static short on_window_key_input(ROOTWIN * rootwin, unsigned short nkc);
static bool on_content_mouse_click(ROOTWIN *rootwin);
static bool on_content_mouse_move(ROOTWIN *rootwin, GRECT *content_area);
static void	toolbar_redraw_cb(GUIWIN *win, uint16_t msg, GRECT *clip);

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy);

static bool redraw_active = false;

static const struct redraw_context rootwin_rdrw_ctx = {
    .interactive = true,
    .background_images = true,
    .plot = &atari_plotters
};

static short handle_event(GUIWIN *win, EVMULT_OUT *ev_out, short msg[8])
{
    short retval = 0;
    GRECT area;
    static bool prev_url = false;
    static short prev_x=0;
    static short prev_y=0;
    struct rootwin_data_s * data = gemtk_wm_get_user_data(win);
    struct gui_window *tmp;
    OBJECT *obj;


    if ((ev_out->emo_events & MU_MESAG) != 0) {
        // handle message
        //printf("root win msg: %d\n", msg[0]);
        switch (msg[0]) {

        case WM_REDRAW:
			LOG(("WM_REDRAW"));
            on_redraw(data->rootwin, msg);
            break;

        case WM_REPOSED:
        case WM_SIZED:
        case WM_MOVED:
        case WM_FULLED:
			LOG(("WM_SIZED"));
            on_resized(data->rootwin);
            break;

        case WM_ICONIFY:
            // TODO: find next active gui window and schedule redraw for that.
			tmp = window_list;
			while(tmp != NULL){
				if(tmp->root != data->rootwin){
					gemtk_wm_send_msg(tmp->root->win, WM_TOPPED, 0, 0, 0, 0);
					break;
				}
				tmp = tmp->next;
			}
            break;

        case WM_TOPPED:
        case WM_NEWTOP:
        case WM_UNICONIFY:
			LOG(("WM_TOPPED"));
            gui_set_input_gui_window(data->rootwin->active_gui_window);
            //window_restore_active_gui_window(data->rootwin);
            // TODO: use something like "restore_active_gui_window_state()"

            break;

        case WM_CLOSED:
            // TODO: this needs to iterate through all gui windows and
            // check if the rootwin is this window...
            if (data->rootwin->active_gui_window != NULL) {
                LOG(("WM_CLOSED initiated destroy for bw %p",
                     data->rootwin->active_gui_window->browser->bw));
                browser_window_destroy(
                    data->rootwin->active_gui_window->browser->bw);
            }
            break;

        case AP_DRAGDROP:
            on_file_dropped(data->rootwin, msg);
            break;

        case WM_TOOLBAR:
            toolbar_mouse_input(data->rootwin->toolbar, msg[4], msg[7]);
            break;

        default:
            break;
        }
    }
    if ((ev_out->emo_events & MU_KEYBD) != 0) {

        // handle key
        uint16_t nkc = gem_to_norm( (short)ev_out->emo_kmeta,
                                    (short)ev_out->emo_kreturn);
		LOG(("rootwin MU_KEYBD input, nkc: %x\n", nkc));
        retval = on_window_key_input(data->rootwin, nkc);
        // printf("on_window_key_input: %d\n", retval);

    }
    if ((ev_out->emo_events & MU_BUTTON) != 0) {
		LOG(("rootwin MU_BUTTON input, x: %d, y: %d\n", ev_out->emo_mouse.p_x,
			ev_out->emo_mouse.p_x));
        window_get_grect(data->rootwin, BROWSER_AREA_CONTENT,
                         &area);
        if (POINT_WITHIN(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y,
                         area)) {
            on_content_mouse_click(data->rootwin);
        }
    }
    if ((ev_out->emo_events & (MU_M1)) != 0) {

        short ghandle = wind_find(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y);

        if (data->rootwin->aes_handle==ghandle) {
            // The window found at x,y is an gui_window
            // and it's the input window.
            window_get_grect(data->rootwin, BROWSER_AREA_CONTENT,
                             &area);
            if (POINT_WITHIN(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y,
                             area)) {
                on_content_mouse_move(data->rootwin, &area);
            } else {
                GRECT tb_area;
                window_get_grect(data->rootwin, BROWSER_AREA_URL_INPUT, &tb_area);
                if (POINT_WITHIN(ev_out->emo_mouse.p_x, ev_out->emo_mouse.p_y,
                                 tb_area)) {
                    gem_set_cursor(&gem_cursors.ibeam);
                    prev_url = true;
                } else {
                    if(prev_url) {
                        struct gui_window *gw;
                        gw = window_get_active_gui_window(data->rootwin);
                        gem_set_cursor(gw->cursor);
                        prev_url = false;
                    }
                }
            }
        }
    }

    return(retval);
}

/* -------------------------------------------------------------------------- */
/* Module public functions:                                                   */
/* -------------------------------------------------------------------------- */

int window_create(struct gui_window * gw,
                  struct browser_window * bw,
                  struct gui_window * existing,
                  unsigned long inflags)
{
    int err = 0;
    bool tb, sb;
    int flags;
    struct rootwin_data_s *data;
    struct gemtk_wm_scroll_info_s *slid;

    tb = (inflags & WIDGET_TOOLBAR);
    sb = (inflags & WIDGET_STATUSBAR);

    flags = CLOSER | MOVER | NAME | FULLER | SMALLER;
    if( inflags & WIDGET_SCROLL ) {
        flags |= (UPARROW | DNARROW | LFARROW | RTARROW | VSLIDE | HSLIDE);
    }
    if( inflags & WIDGET_RESIZE ) {
        flags |= ( SIZER );
    }
    if( inflags & WIDGET_STATUSBAR ) {
        flags |= ( INFO );
    }

    gw->root = malloc(sizeof(struct s_gui_win_root));
    if (gw->root == NULL)
        return(-1);
    memset(gw->root, 0, sizeof(struct s_gui_win_root) );
    gw->root->title = malloc(atari_sysinfo.aes_max_win_title_len+1);

    redraw_slots_init(&gw->root->redraw_slots, 8);

    gw->root->aes_handle = wind_create(flags, 40, 40, desk_area.g_w,
                                       desk_area.g_h);
    if(gw->root->aes_handle<0) {
        free(gw->root->title);
        free(gw->root);
        return( -1 );
    }
    gw->root->win = gemtk_wm_add(gw->root->aes_handle,
                               GEMTK_WM_FLAG_PREPROC_WM | GEMTK_WM_FLAG_RECV_PREPROC_WM, handle_event);

    data = malloc(sizeof(struct rootwin_data_s));
    data->rootwin = gw->root;
    gemtk_wm_set_user_data(gw->root->win, (void*)data);
    slid = gemtk_wm_get_scroll_info(gw->root->win);
    slid->y_unit_px = 32;
    slid->x_unit_px = 32;

    /* create */
    if(tb) {
        gw->root->toolbar = toolbar_create(gw->root);
        assert(gw->root->toolbar);
        gemtk_wm_set_toolbar(gw->root->win, gw->root->toolbar->form, 0, 0);
		gemtk_wm_set_toolbar_redraw_func(gw->root->win, toolbar_redraw_cb);
    } else {
        gw->root->toolbar = NULL;
    }

    /* create browser component: */
    gw->browser = (struct s_browser *)malloc( sizeof(struct s_browser));

    assert(gw->browser);

    gw->browser->bw = bw;
    if(existing)
        gw->browser->bw->scale = existing->browser->bw->scale;
    else
        gw->browser->bw->scale = 1;


    /* create statusbar component: */
    if(sb) {
        gw->root->statusbar = sb_create( gw );
    } else {
        gw->root->statusbar = NULL;
    }

    // Setup some window defaults:
    wind_set_str(gw->root->aes_handle, WF_NAME, (char*)"NetSurf");
    wind_set(gw->root->aes_handle, WF_OPTS, 1, WO0_FULLREDRAW, 0, 0);
    wind_set(gw->root->aes_handle, WF_OPTS, 1, WO0_NOBLITW, 0, 0);
    wind_set(gw->root->aes_handle, WF_OPTS, 1, WO0_NOBLITH, 0, 0);

    if (inflags & WIN_TOP) {
        window_set_focus(gw->root, BROWSER, gw->browser);
    }

    return (err);
}

void window_unref_gui_window(ROOTWIN *rootwin, struct gui_window *gw)
{
    struct gui_window *w;
    input_window = NULL;

    LOG(("window: %p, gui_window: %p", rootwin, gw));

    w = window_list;
    // find the next active tab:
    while( w != NULL ) {
        if(w->root == rootwin && w != gw) {
        	LOG(("activating next tab %p", w));
            gui_set_input_gui_window(w);
            break;
        }
        w = w->next;
    }
    if(input_window == NULL) {
        // the last gui window for this rootwin was removed:
        redraw_slots_free(&rootwin->redraw_slots);
        window_destroy(rootwin);
    }
}

int window_destroy(ROOTWIN *rootwin)
{
    int err = 0;
    struct gui_window *w;

    assert(rootwin != NULL);

    LOG(("%p", rootwin));

    if (gemtk_wm_get_user_data(rootwin->win) != NULL) {
        free(gemtk_wm_get_user_data(rootwin->win));
    }

    // make sure we do not destroy windows which have gui_windows attached:
    w = window_list;
    while( w != NULL ) {
        if(w->root == rootwin) {
            assert(rootwin == NULL);
        }
        w = w->next;
    }

    if (rootwin->toolbar)
        toolbar_destroy(rootwin->toolbar);

    if(rootwin->statusbar)
        sb_destroy(rootwin->statusbar);

    if(rootwin->title)
        free(rootwin->title);

    gemtk_wm_remove(rootwin->win);
    wind_close(rootwin->aes_handle);
    wind_delete(rootwin->aes_handle);
    free(rootwin);
    return(err);
}


void window_open(ROOTWIN *rootwin, struct gui_window *gw, GRECT pos)
{
    GRECT br, g;

    rootwin->active_gui_window = gw;

    assert(rootwin->active_gui_window != NULL);

    wind_open(rootwin->aes_handle, pos.g_x, pos.g_y, pos.g_w, pos.g_h );
    wind_set_str(rootwin->aes_handle, WF_NAME, (char *)"");

    rootwin->active_gui_window->browser->attached = true;
    if(rootwin->statusbar != NULL) {
        sb_attach(rootwin->statusbar, rootwin->active_gui_window);
    }

    /* Set initial size of the toolbar region: */
    gemtk_wm_get_grect(rootwin->win, GEMTK_WM_AREA_TOOLBAR, &g);
    toolbar_set_attached(rootwin->toolbar, true);
    toolbar_set_dimensions(rootwin->toolbar, &g);

    /* initially hide the search area of the toolbar: */
	window_close_search(rootwin);

    window_update_back_forward(rootwin);

    window_set_focus(rootwin, BROWSER, rootwin->active_gui_window->browser);
}

void window_restore_active_gui_window(ROOTWIN *rootwin)
{
	GRECT tb_area;
	struct gui_window *gw;

	LOG((""));

	assert(rootwin->active_gui_window);

	gw = rootwin->active_gui_window;

    window_set_icon(rootwin, gw->icon);
    window_set_stauts(rootwin, gw->status);
    window_set_title(rootwin, gw->title);

	if (gw->search != NULL) {
		// TODO: update search session (especially browser window)
    }

	toolbar_get_grect(rootwin->toolbar, 0, &tb_area);
	gemtk_wm_set_toolbar_size(rootwin->win, tb_area.g_h);

	window_update_back_forward(rootwin);

    toolbar_set_url(rootwin->toolbar, gw->url);
}


/* update back forward buttons (see tb_update_buttons (bug) ) */
void window_update_back_forward(struct s_gui_win_root *rootwin)
{
    struct gui_window * active_gw = rootwin->active_gui_window;
    toolbar_update_buttons(rootwin->toolbar, active_gw->browser->bw, -1);
}

void window_set_stauts(struct s_gui_win_root *rootwin, char * text)
{
    assert(rootwin != NULL);

    CMP_STATUSBAR sb = rootwin->statusbar;

    if( sb == NULL)
        return;

    if(text != NULL)
        sb_set_text(sb, text);
    else
        sb_set_text(sb, "");
}

void window_set_title(struct s_gui_win_root * rootwin, char *title)
{
    wind_set_str(rootwin->aes_handle, WF_NAME, title);
}

void window_scroll_by(ROOTWIN *root, int sx, int sy)
{
    int units;
    GRECT content_area;
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(root->win);

    if (sx < 0) {
        sx = 0;
    }
    if (sy < 0) {
        sy = 0;
    }
    int xunits = sx / slid->x_unit_px;
    int yunits = sy / slid->y_unit_px;

    gemtk_wm_scroll(root->win, GEMTK_WM_VSLIDER, yunits - slid->y_pos, false);
    gemtk_wm_scroll(root->win, GEMTK_WM_HSLIDER, xunits - slid->x_pos, false);
    gemtk_wm_update_slider(root->win, GEMTK_WM_VH_SLIDER);
}

/**
* Set the dimensions of the scrollable content.
*
*/
void window_set_content_size(ROOTWIN *rootwin, int width, int height)
{
    GRECT area;
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(rootwin->win);

    window_get_grect(rootwin, BROWSER_AREA_CONTENT, &area);

    slid->x_units = (width/slid->x_unit_px);
    slid->y_units = (height/slid->y_unit_px);
    if(slid->x_units < slid->x_pos)
        slid->x_pos = 0;
    if(slid->y_units < slid->y_pos)
        slid->y_pos = 0;
    gemtk_wm_update_slider(rootwin->win, GEMTK_WM_VH_SLIDER);
}

/* set focus to an arbitary element */
void window_set_focus(struct s_gui_win_root *rootwin,
                      enum focus_element_type type, void * element)
{
    struct textarea * ta;

    assert(rootwin != NULL);

    if (rootwin->focus.type != type || rootwin->focus.element != element) {
        LOG(("Set focus: %p (%d)\n", element, type));
        rootwin->focus.type = type;
        rootwin->focus.element = element;
		switch( type ) {

		case URL_WIDGET:
                // TODO: make something like: toolbar_text_select_all();
			toolbar_key_input(rootwin->toolbar, (short)(NKF_CTRL | 'A') );
/*
			ta = toolbar_get_textarea(rootwin->toolbar,
										URL_INPUT_TEXT_AREA);
			textarea_keypress(ta, KEY_SELECT_ALL);
			*/
			break;

		case SEARCH_INPUT:
			gemtk_wm_set_toolbar_edit_obj(rootwin->win, TOOLBAR_TB_SRCH, 0);
			break;

		default:
			break;

		}
    }
}

/* check if the url widget has focus */
bool window_url_widget_has_focus(struct s_gui_win_root *rootwin)
{
    assert(rootwin != NULL);

    if (rootwin->focus.type == URL_WIDGET) {
        return true;
    }
    return false;
}

/* check if an arbitary window widget / or frame has the focus */
bool window_widget_has_focus(struct s_gui_win_root *rootwin,
                             enum focus_element_type t, void * element)
{
    assert(rootwin != NULL);
    if( element == NULL  ) {
        return((rootwin->focus.type == t));
    }

    return((element == rootwin->focus.element && t == rootwin->focus.type));
}

void window_set_icon(ROOTWIN *rootwin, struct bitmap * bmp )
{
    rootwin->icon = bmp;
    /* redraw window when it is iconyfied: */
    if (rootwin->icon != NULL) {
        short info, dummy;
        if (gemtk_wm_get_state(rootwin->win) & GEMTK_WM_STATUS_ICONIFIED) {
            window_redraw_favicon(rootwin, NULL);
        }
    }
}

void window_set_active_gui_window(ROOTWIN *rootwin, struct gui_window *gw)
{
	struct gui_window *old_gw = rootwin->active_gui_window;

	LOG((""));

    if (rootwin->active_gui_window != NULL) {
        if(rootwin->active_gui_window == gw) {
        	LOG(("nothing to do..."));
            return;
        }
    }

	// TODO: when the window isn't on top, initiate WM_TOPPED.

	rootwin->active_gui_window = gw;
	if (old_gw != NULL) {
		LOG(("restoring window..."));
		window_restore_active_gui_window(rootwin);
	}
}

struct gui_window * window_get_active_gui_window(ROOTWIN * rootwin)
{
    return(rootwin->active_gui_window);
}

void window_get_scroll(ROOTWIN *rootwin, int *x, int *y)
{
    struct gemtk_wm_scroll_info_s *slid;

    slid = gemtk_wm_get_scroll_info(rootwin->win);

    *x = slid->x_pos * slid->x_unit_px;
    *y = slid->y_pos * slid->y_unit_px;
}

void window_get_grect(ROOTWIN *rootwin, enum browser_area_e which, GRECT *d)
{

    d->g_x = 0;
    d->g_y = 0;
    d->g_w = 0;
    d->g_h = 0;

    if (which == BROWSER_AREA_TOOLBAR) {
        // gemtk_wm_get_grect(rootwin->win, GEMTK_WM_AREA_TOOLBAR, d);
        toolbar_get_grect(rootwin->toolbar, 0, d);

    } else if (which == BROWSER_AREA_CONTENT) {

        GRECT tb_area;

        gemtk_wm_get_grect(rootwin->win, GEMTK_WM_AREA_WORK, d);
        toolbar_get_grect(rootwin->toolbar, 0, &tb_area);

        d->g_y += tb_area.g_h;
        d->g_h -= tb_area.g_h;

    } else if (which == BROWSER_AREA_URL_INPUT) {

        toolbar_get_grect(rootwin->toolbar, TOOLBAR_AREA_URL, d);

    } else if (which == BROWSER_AREA_SEARCH) {
        // todo: check if search is visible
        toolbar_get_grect(rootwin->toolbar, TOOLBAR_AREA_SEARCH, d);
    } else {

    }


    // sanitize the results
    if (d->g_h < 0) {
        d->g_h = 0;
    }
    if (d->g_w < 0) {
        d->g_w = 0;
    }

    //printf("window_get_grect %d:", which);
    //dbg_grect("", d);

}


void window_open_search(ROOTWIN *rootwin, bool reformat)
{
	struct browser_window *bw;
	struct gui_window *gw;
	GRECT area;
	OBJECT *obj;

	LOG((""));

	gw = rootwin->active_gui_window;
	bw = gw->browser->bw;
	obj = toolbar_get_form(rootwin->toolbar);

	if (gw->search == NULL) {
		gw->search = nsatari_search_session_create(obj, bw);
	}

	toolbar_set_visible(rootwin->toolbar, TOOLBAR_AREA_SEARCH, true);
	window_get_grect(rootwin, BROWSER_AREA_TOOLBAR, &area);
	gemtk_wm_set_toolbar_size(rootwin->win, area.g_h);
	window_get_grect(rootwin, BROWSER_AREA_SEARCH, &area);
	window_schedule_redraw_grect(rootwin, &area);
	window_process_redraws(rootwin);
	window_set_focus(rootwin, SEARCH_INPUT, NULL);

	window_get_grect(rootwin, BROWSER_AREA_CONTENT, &area);
	if (reformat) {
		browser_window_reformat(bw, false, area.g_w, area.g_h);
	}
}

void window_close_search(ROOTWIN *rootwin)
{
	struct browser_window *bw;
	struct gui_window *gw;
	GRECT area;
	OBJECT *obj;


	gw = rootwin->active_gui_window;
	bw = gw->browser->bw;
	obj = gemtk_obj_get_tree(TOOLBAR);

	if (gw->search != NULL) {
		nsatari_search_session_destroy(gw->search);
		gw->search = NULL;
	}

	toolbar_set_visible(rootwin->toolbar, TOOLBAR_AREA_SEARCH, false);
	window_get_grect(rootwin, BROWSER_AREA_TOOLBAR, &area);
	gemtk_wm_set_toolbar_size(rootwin->win, area.g_h);
	window_get_grect(rootwin, BROWSER_AREA_CONTENT, &area);
	browser_window_reformat(bw, false, area.g_w, area.g_h);
}

/**
 * Redraw the favicon
*/
void window_redraw_favicon(ROOTWIN *rootwin, GRECT *clip_ro)
{
    GRECT work, visible, clip;

    assert(rootwin);

    //printf("window_redraw_favicon: root: %p, win: %p\n", rootwin, rootwin->win);

    gemtk_wm_clear(rootwin->win);
    gemtk_wm_get_grect(rootwin->win, GEMTK_WM_AREA_WORK, &work);

    if (clip_ro == NULL) {
        clip = work;
    } else {
    	clip = *clip_ro;
        if(!rc_intersect(&work, &clip)) {
            return;
        }
    }

    //dbg_grect("favicon redrw area", clip);
	//dbg_grect("favicon work area", &work);

    if (rootwin->icon == NULL) {
        //printf("window_redraw_favicon OBJCTREE\n");

        OBJECT * tree = gemtk_obj_get_tree(ICONIFY);
        tree->ob_x = work.g_x;
        tree->ob_y = work.g_y;
        tree->ob_width = work.g_w;
        tree->ob_height = work.g_h;

        wind_get_grect(rootwin->aes_handle, WF_FIRSTXYWH, &visible);
		while (visible.g_h > 0 && visible.g_w > 0) {

			if (rc_intersect(&clip, &visible)) {
				//dbg_grect("redraw vis area", &visible);
				objc_draw(tree, 0, 8, visible.g_x, visible.g_y, visible.g_w,
							visible.g_h);
			} else {
				//dbg_grect("redraw vis area outside", &visible);
			}

			wind_get_grect(rootwin->aes_handle, WF_NEXTXYWH, &visible);
		}

    } else {
        //printf("window_redraw_favicon image %p\n", rootwin->icon);
        VdiHdl plot_vdi_handle = plot_get_vdi_handle();
        struct rect work_clip = { 0,0,work.g_w,work.g_h };
        short pxy[4];
        int xoff=0;

        if (work.g_w > work.g_h) {
            xoff = ((work.g_w-work.g_h)/2);
            work.g_w = work.g_h;
        }
        plot_set_dimensions( work.g_x+xoff, work.g_y, work.g_w,
							work.g_h);
		//plot_clip(&work_clip);

		wind_get_grect(rootwin->aes_handle, WF_FIRSTXYWH, &visible);
		while (visible.g_h > 0 && visible.g_w > 0) {

			if (rc_intersect(&clip, &visible)) {

				//dbg_grect("redraw vis area", &visible);

				// Manually clip drawing region:
				pxy[0] = visible.g_x;
				pxy[1] = visible.g_y;
				pxy[2] = pxy[0] + visible.g_w-1;
				pxy[3] = pxy[1] + visible.g_h-1;
				vs_clip(plot_vdi_handle, 1, (short*)&pxy);
				//dbg_pxy("vdi clip", (short*)&pxy);

				atari_plotters.bitmap(0, 0, work.g_w, work.g_h,
										rootwin->icon, 0xffffff, 0);
			} else {
				//dbg_grect("redraw vis area outside", &visible);
			}

			wind_get_grect(rootwin->aes_handle, WF_NEXTXYWH, &visible);
		}
    }
}

/***
*   Schedule an redraw area, redraw requests during redraw are
*   not optimized (merged) into other areas, so that the redraw
*   functions can spot the change.
*
*/
void window_schedule_redraw_grect(ROOTWIN *rootwin, GRECT *area)
{
    GRECT work;


    //dbg_grect("window_schedule_redraw_grect input ", area);

    gemtk_wm_get_grect(rootwin->win, GEMTK_WM_AREA_WORK, &work);
    if(!rc_intersect(area, &work))
        return;

    //dbg_grect("window_schedule_redraw_grect intersection ", &work);

    redraw_slot_schedule_grect(&rootwin->redraw_slots, &work, redraw_active);
}

static void window_redraw_content(ROOTWIN *rootwin, GRECT *content_area,
                                  GRECT *clip,
                                  struct gemtk_wm_scroll_info_s * slid,
                                  struct browser_window *bw)
{

    struct rect redraw_area;
    GRECT content_area_rel;
    float oldscale = 1.0;

    //dbg_grect("browser redraw, content area", content_area);
    //dbg_grect("browser redraw, content clip", clip);

    plot_set_dimensions(content_area->g_x, content_area->g_y,
                        content_area->g_w, content_area->g_h);
    oldscale = plot_set_scale(gui_window_get_scale(rootwin->active_gui_window));

    /* first, we make the coords relative to the content area: */
    content_area_rel.g_x = clip->g_x - content_area->g_x;
    content_area_rel.g_y = clip->g_y - content_area->g_y;
    content_area_rel.g_w = clip->g_w;
    content_area_rel.g_h = clip->g_h;

    if (content_area_rel.g_x < 0) {
        content_area_rel.g_w += content_area_rel.g_x;
        content_area_rel.g_x = 0;
    }

    if (content_area_rel.g_y < 0) {
        content_area_rel.g_h += content_area_rel.g_y;
        content_area_rel.g_y = 0;
    }

    //dbg_grect("browser redraw, relative plot coords:", &content_area_rel);

    redraw_area.x0 = content_area_rel.g_x;
    redraw_area.y0 = content_area_rel.g_y;
    redraw_area.x1 = content_area_rel.g_x + content_area_rel.g_w;
    redraw_area.y1 = content_area_rel.g_y + content_area_rel.g_h;

    plot_clip(&redraw_area);

    //dbg_rect("rdrw area", &redraw_area);

    browser_window_redraw( bw, -(slid->x_pos*slid->x_unit_px),
                           -(slid->y_pos*slid->y_unit_px), &redraw_area, &rootwin_rdrw_ctx);

    plot_set_scale(oldscale);
}


void window_place_caret(ROOTWIN *rootwin, short mode, int content_x,
                        int content_y, int h, GRECT *work)
{
    struct s_caret *caret = &rootwin->caret;
    VdiHdl vh = gemtk_wm_get_vdi_handle(rootwin->win);
    short pxy[8];
    GRECT mywork, caret_pos;
    MFDB screen;
    int i, scroll_x, scroll_y;
    uint16_t *fd_addr;
    struct gemtk_wm_scroll_info_s *slid;
    short colors[2] = {G_BLACK, G_WHITE};
    bool render_required = false;

    // avoid duplicate draw of the caret:
    if (mode == 1 &&(caret->state&CARET_STATE_VISIBLE)!=0) {
        if (caret->dimensions.g_x == content_x
                && caret->dimensions.g_y == content_y
                && caret->dimensions.g_h == h) {
            return;
        }
    }

    if(work == NULL) {
        window_get_grect(rootwin, BROWSER_AREA_CONTENT, &mywork);
        work = &mywork;
    }
    slid = gemtk_wm_get_scroll_info(rootwin->win);

    scroll_x = slid->x_pos * slid->x_unit_px;
    scroll_y = slid->y_pos * slid->y_unit_px;

    init_mfdb(0, 1, h, 0, &screen);

    // enable clipping:
    pxy[0] = work->g_x;
    pxy[1] = work->g_y;
    pxy[2] = pxy[0] + work->g_w - 1;
    pxy[3] = pxy[1] + work->g_h - 1;
    vs_clip(vh, 1, pxy);

    // when the caret is visible, undraw it:
    if (caret->symbol.fd_addr != NULL
            && (caret->state&CARET_STATE_VISIBLE)!=0) {

        caret_pos.g_x = work->g_x + (caret->dimensions.g_x - scroll_x);
        caret_pos.g_y = work->g_y + (caret->dimensions.g_y - scroll_y);
        caret_pos.g_w = caret->dimensions.g_w;
        caret_pos.g_h = caret->dimensions.g_h;

        if (rc_intersect(work, &caret_pos)) {

            pxy[0] = 0;
            pxy[1] = 0;
            pxy[2] = caret->dimensions.g_w-1;
            pxy[3] = caret->dimensions.g_h-1;

            pxy[4] = caret_pos.g_x;
            pxy[5] = caret_pos.g_y;
            pxy[6] = pxy[4] + caret_pos.g_w-1;
            pxy[7] = pxy[5] + caret_pos.g_h-1;

            vrt_cpyfm(vh, MD_XOR, pxy, &caret->symbol, &screen, colors);
        }
    }
    if (mode == 0) {
        // update state:
        caret->state &= ~CARET_STATE_VISIBLE;
        goto exit;
    }

    // when the caret isn't allocated, create it:
    if (caret->symbol.fd_addr == NULL) {
        caret->fd_size = init_mfdb(1, 16, h, MFDB_FLAG_ZEROMEM,
                                   &caret->symbol);
        render_required = true;
    } else {
        // the caret may need more memory:
        if (caret->dimensions.g_h < h) {
            caret->fd_size = init_mfdb(1, 16, h, MFDB_FLAG_NOALLOC,
                                       &caret->symbol);
            realloc(caret->symbol.fd_addr, caret->fd_size);
            render_required = true;
        }
    }

    // set new caret position:
    caret->dimensions.g_x = content_x;
    caret->dimensions.g_y = content_y;
    caret->dimensions.g_w = 1;
    caret->dimensions.g_h = h;

    // draw the caret into the mfdb buffer:
    if (render_required) {

        assert(caret->symbol.fd_nplanes == 1);
        assert(caret->symbol.fd_w == 16);

        // draw an vertical line into the mfdb buffer
        fd_addr = (uint16_t*)caret->symbol.fd_addr;
        for(i = 0; i<caret->symbol.fd_h; i++) {
            fd_addr[i] = 0xFFFF;
        }
    }

    // convert content coords to screen coords:

    caret_pos.g_x = work->g_x + (content_x - scroll_x);
    caret_pos.g_y = work->g_y + (content_y - scroll_y);
    caret_pos.g_w = caret->dimensions.g_w;
    caret_pos.g_h = caret->dimensions.g_h;

    if (rc_intersect(work, &caret_pos) && redraw_active == false) {

        pxy[0] = 0;
        pxy[1] = 0;
        pxy[2] = caret->dimensions.g_w-1;
        pxy[3] = caret->dimensions.g_h-1;

        pxy[4] = caret_pos.g_x;
        pxy[5] = caret_pos.g_y;
        pxy[6] = pxy[4] + caret_pos.g_w-1;
        pxy[7] = pxy[5] + caret_pos.g_h-1;

        //dbg_pxy("caret screen coords (md_repl)", &pxy[4]);

        // TODO: walk rectangle list (use MD_REPLACE then)
        // draw caret to screen coords:
        vrt_cpyfm(vh, /*MD_REPLACE*/ MD_XOR, pxy, &caret->symbol, &screen, colors);

        // update state:
        caret->state |= CARET_STATE_VISIBLE;
    }

exit:
    // disable clipping:
    vs_clip(gemtk_wm_get_vdi_handle(rootwin->win), 0, pxy);
}

void window_process_redraws(ROOTWIN * rootwin)
{
    GRECT work, visible_ro, tb_area, content_area;
    short i;
    short scroll_x=0, scroll_y=0;
    bool toolbar_rdrw_required;
    bool caret_rdrw_required = false;
    struct gemtk_wm_scroll_info_s *slid =NULL;
    int caret_h = 0;
    struct s_caret *caret = &rootwin->caret;

    redraw_active = true;

    window_get_grect(rootwin, BROWSER_AREA_TOOLBAR, &tb_area);
    //gemtk_wm_set_toolbar_size(rootwin->win, tb_area.g_h);
    window_get_grect(rootwin, BROWSER_AREA_CONTENT, &content_area);

    //dbg_grect("content area", &content_area);
    //dbg_grect("window_process_redraws toolbar area", &tb_area);

    while(plot_lock() == false);

    if (((rootwin->caret.state & CARET_STATE_ENABLED)!=0)
            && rootwin->caret.dimensions.g_h > 0) {
        // hide caret:
        window_place_caret(rootwin, 0, -1, -1, -1, &content_area);
    }
/*
    short pxy_clip[4];
    pxy_clip[0] = tb_area.g_x;
    pxy_clip[0] = tb_area.g_y;
    pxy_clip[0] = pxy_clip[0] + tb_area.g_w + content_area.g_w - 1;
    pxy_clip[0] = pxy_clip[1] + tb_area.g_h + content_area.g_h - 1;
    vs_clip(gemtk_wm_get_vdi_handle(rootwin->win), 1, pxy_clip);
	//gemtk_wm_clear(rootwin->win);
*/
    wind_get_grect(rootwin->aes_handle, WF_FIRSTXYWH, &visible_ro);
    while (visible_ro.g_w > 0 && visible_ro.g_h > 0) {
        plot_set_abs_clipping(&visible_ro);

    	//dbg_grect("visible ", &visible_ro);

        // TODO: optimze the rectangle list -
        // remove rectangles which were completly inside the visible area.
        // that way we don't have to loop over again...
        for(i=0; i<rootwin->redraw_slots.areas_used; i++) {

            GRECT rdrw_area_ro = {
                rootwin->redraw_slots.areas[i].x0,
                rootwin->redraw_slots.areas[i].y0,
                rootwin->redraw_slots.areas[i].x1 -
                rootwin->redraw_slots.areas[i].x0,
                rootwin->redraw_slots.areas[i].y1 -
                rootwin->redraw_slots.areas[i].y0
            };

            if (!rc_intersect(&visible_ro, &rdrw_area_ro)) {
				continue;
            }
            GRECT rdrw_area = rdrw_area_ro;

            if (rc_intersect(&tb_area, &rdrw_area)) {
				toolbar_redraw(rootwin->toolbar, &rdrw_area);
            }

            rdrw_area = rdrw_area_ro;
            if (rc_intersect(&content_area, &rdrw_area)) {

                if(slid == NULL) {
                    slid = gemtk_wm_get_scroll_info(rootwin->win);

                    scroll_x = slid->x_pos * slid->x_unit_px;
                    scroll_y = slid->y_pos * slid->y_unit_px;
                }

                window_redraw_content(rootwin, &content_area, &rdrw_area,
                                      slid,
                                      rootwin->active_gui_window->browser->bw);
                if (((rootwin->caret.state & CARET_STATE_ENABLED)!=0)) {

                    GRECT caret_pos;

                    caret_pos.g_x = content_area.g_x +
                                    (caret->dimensions.g_x - scroll_x);
                    caret_pos.g_y = content_area.g_y +
                                    (caret->dimensions.g_y - scroll_y);
                    caret_pos.g_w = caret->dimensions.g_w;
                    caret_pos.g_h = caret->dimensions.g_h;

                    if (gemtk_rc_intersect_ro(&caret_pos, &content_area)) {
                        caret_rdrw_required = true;
                    }
                }

            }
        }
        wind_get_grect(rootwin->aes_handle, WF_NEXTXYWH, &visible_ro);
    }


    // disable clipping:
    //vs_clip(gemtk_wm_get_vdi_handle(rootwin->win), 0, pxy_clip);

    if (caret_rdrw_required && ((rootwin->caret.state & CARET_STATE_ENABLED)!=0)) {

        // force redraw of caret:
        caret_h = rootwin->caret.dimensions.g_h;
        rootwin->caret.dimensions.g_h = 0;
        redraw_active = false;
        window_place_caret(rootwin, 1, rootwin->caret.dimensions.g_x,
                           rootwin->caret.dimensions.g_y,
                           caret_h, &content_area);
    }

    rootwin->redraw_slots.areas_used = 0;
    redraw_active = false;

    plot_unlock();
}


/* -------------------------------------------------------------------------- */
/* Event Handlers:                                                            */
/* -------------------------------------------------------------------------- */
static bool on_content_mouse_move(ROOTWIN *rootwin, GRECT *content_area)
{
    int mx, my, sx, sy;
    struct gemtk_wm_scroll_info_s *slid;
    struct gui_window *gw;
    struct browser_window *bw;

    // make relative mouse coords:
    mx = aes_event_out.emo_mouse.p_x - content_area->g_x;
    my = aes_event_out.emo_mouse.p_y - content_area->g_y;

    slid = gemtk_wm_get_scroll_info(rootwin->win);
    gw = window_get_active_gui_window(rootwin);
    bw = gw->browser->bw;

    // calculate scroll pos. in pixel:
    sx = slid->x_pos * slid->x_unit_px;
    sy = slid->y_pos * slid->y_unit_px;

    browser_window_mouse_track(bw, 0, mx + sx, my + sy);
}

static bool on_content_mouse_click(ROOTWIN *rootwin)
{
    short dummy, mbut, mx, my;
    GRECT cwork;
    browser_mouse_state bmstate = 0;
    struct gui_window *gw;
    struct gemtk_wm_scroll_info_s *slid;

    gw = window_get_active_gui_window(rootwin);
    if(input_window != gw) {
        gui_set_input_gui_window(gw);
    }

    window_set_focus(gw->root, BROWSER, (void*)gw->browser );
    window_get_grect(gw->root, BROWSER_AREA_CONTENT, &cwork);

    /* convert screen coords to component coords: */
    mx = aes_event_out.emo_mouse.p_x - cwork.g_x;
    my = aes_event_out.emo_mouse.p_y - cwork.g_y;
    //printf("content click at %d,%d\n", mx, my);

    /* Translate GEM key state to netsurf mouse modifier */
    if ( aes_event_out.emo_kmeta & (K_RSHIFT | K_LSHIFT)) {
        bmstate |= BROWSER_MOUSE_MOD_1;
    } else {
        bmstate &= ~(BROWSER_MOUSE_MOD_1);
    }
    if ( (aes_event_out.emo_kmeta & K_CTRL) ) {
        bmstate |= BROWSER_MOUSE_MOD_2;
    } else {
        bmstate &= ~(BROWSER_MOUSE_MOD_2);
    }
    if ( (aes_event_out.emo_kmeta & K_ALT) ) {
        bmstate |= BROWSER_MOUSE_MOD_3;
    } else {
        bmstate &= ~(BROWSER_MOUSE_MOD_3);
    }

    /* convert component coords to scrolled content coords: */
    slid = gemtk_wm_get_scroll_info(rootwin->win);
    int sx_origin = mx;
    int sy_origin = my;

    short rel_cur_x, rel_cur_y;
    short prev_x=sx_origin, prev_y=sy_origin;
    bool dragmode = false;

    /* Detect left mouse button state and compare with event state: */
    graf_mkstate(&rel_cur_x, &rel_cur_y, &mbut, &dummy);
    if( (mbut & 1) && (aes_event_out.emo_mbutton & 1) ) {
        /* Mouse still pressed, report drag */
        rel_cur_x = (rel_cur_x - cwork.g_x);
        rel_cur_y = (rel_cur_y - cwork.g_y);
        browser_window_mouse_click( gw->browser->bw,
                                    BROWSER_MOUSE_DRAG_ON|BROWSER_MOUSE_DRAG_1,
                                    rel_cur_x + slid->x_pos * slid->x_unit_px,
                                    rel_cur_y + slid->y_pos * slid->y_unit_px);
        do {
            // only consider movements of 5px or more as drag...:
            if( abs(prev_x-rel_cur_x) > 5 || abs(prev_y-rel_cur_y) > 5 ) {
                browser_window_mouse_track( gw->browser->bw,
                                            BROWSER_MOUSE_DRAG_ON|BROWSER_MOUSE_DRAG_1,
                                            rel_cur_x + slid->x_pos * slid->x_unit_px,
                                            rel_cur_y + slid->y_pos * slid->y_unit_px);
                prev_x = rel_cur_x;
                prev_y = rel_cur_y;
                dragmode = true;
            } else {
                if( dragmode == false ) {
                    browser_window_mouse_track( gw->browser->bw,BROWSER_MOUSE_PRESS_1,
                                                rel_cur_x + slid->x_pos * slid->x_unit_px,
                                                rel_cur_y + slid->y_pos * slid->y_unit_px);
                }
            }

            // we may need to process scrolling:
            // TODO: this doesn't work, because gemtk schedules redraw via
            // AES window messages but we do not process them right here...
            if (rootwin->redraw_slots.areas_used > 0) {
                window_process_redraws(rootwin);
            }
            evnt_timer(150);

            graf_mkstate(&rel_cur_x, &rel_cur_y, &mbut, &dummy);
            rel_cur_x = (rel_cur_x - cwork.g_x);
            rel_cur_y = (rel_cur_y - cwork.g_y);
        } while( mbut & 1 );
        browser_window_mouse_track(gw->browser->bw, 0,
                                   rel_cur_x + slid->x_pos * slid->x_unit_px,
                                   rel_cur_y + slid->y_pos * slid->y_unit_px);
    } else {
        /* Right button pressed? */
        if ((aes_event_out.emo_mbutton & 2 ) ) {
            context_popup(gw, aes_event_out.emo_mouse.p_x,
                          aes_event_out.emo_mouse.p_y);
        } else {
            browser_window_mouse_click(gw->browser->bw,
                                       bmstate|BROWSER_MOUSE_PRESS_1,
                                       sx_origin + slid->x_pos * slid->x_unit_px,
                                       sy_origin + slid->y_pos * slid->y_unit_px);
            browser_window_mouse_click(gw->browser->bw,
                                       bmstate|BROWSER_MOUSE_CLICK_1,
                                       sx_origin + slid->x_pos * slid->x_unit_px,
                                       sy_origin + slid->y_pos * slid->y_unit_px);
        }
    }
    if (rootwin->redraw_slots.areas_used > 0) {
        window_process_redraws(rootwin);
    }
}

/*
	Report keypress to browser component.
	parameter:
		- unsigned short nkc ( CFLIB normalised key code )
*/
static bool on_content_keypress(struct browser_window *bw, unsigned short nkc)
{
    bool r = false;
    unsigned char ascii = (nkc & 0xFF);
    long ucs4;
    long ik = nkc_to_input_key( nkc, &ucs4 );

    // pass event to specific control?

    if (ik == 0) {
        if (ascii >= 9) {
            r = browser_window_key_press(bw, ucs4);
        }
    } else {
        r = browser_window_key_press(bw, ik);
        if (r == false) {

            GRECT g;
            GUIWIN * w = bw->window->root->win;
            window_get_grect(bw->window->root, BROWSER_AREA_CONTENT, &g);

            struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(w);

            switch( ik ) {
            case KEY_LINE_START:
                gemtk_wm_scroll(w, GEMTK_WM_HSLIDER, -(g.g_w/slid->x_unit_px),
                              false);
				r = true;
                break;

            case KEY_LINE_END:
                gemtk_wm_scroll(w, GEMTK_WM_HSLIDER, (g.g_w/slid->x_unit_px),
                              false);
				r = true;
                break;

            case KEY_PAGE_UP:
                gemtk_wm_scroll(w, GEMTK_WM_VSLIDER, -(g.g_h/slid->y_unit_px),
                              false);
				r = true;
                break;

            case KEY_PAGE_DOWN:
                gemtk_wm_scroll(w, GEMTK_WM_VSLIDER, (g.g_h/slid->y_unit_px),
                              false);
				r = true;
                break;

            case KEY_RIGHT:
                gemtk_wm_scroll(w, GEMTK_WM_HSLIDER, -1, false);
				r = true;
                break;

            case KEY_LEFT:
                gemtk_wm_scroll(w, GEMTK_WM_HSLIDER, 1, false);
				r = true;
                break;

            case KEY_UP:
                gemtk_wm_scroll(w, GEMTK_WM_VSLIDER, -1, false);
				r = true;
                break;

            case KEY_DOWN:
                gemtk_wm_scroll(w, GEMTK_WM_VSLIDER, 1, false);
                r = true;
                break;

			case KEY_TEXT_START:
				window_scroll_by(bw->window->root, 0, 0);
				r = true;
				break;

            default:
                break;
            }
            gemtk_wm_update_slider(w, GEMTK_WM_VSLIDER|GEMTK_WM_HSLIDER);
        }
    }

    return(r);
}

static short on_window_key_input(ROOTWIN *rootwin, unsigned short nkc)
{
    bool done = false;
    struct gui_window * gw = window_get_active_gui_window(rootwin);
    struct gui_window * gw_tmp;

    if( gw == NULL )
        return(false);

    if(window_url_widget_has_focus((void*)gw->root)) {
        /* make sure we report for the root window and report...: */
        done = toolbar_key_input(gw->root->toolbar, nkc);
    }  else  {
        if( window_widget_has_focus(input_window->root, BROWSER,
			(void*)input_window->browser)) {
			done = on_content_keypress(input_window->browser->bw, nkc);
		}
		else if(window_widget_has_focus(input_window->root, SEARCH_INPUT, NULL)) {
			OBJECT * obj;
				obj = toolbar_get_form(input_window->root->toolbar);
				obj[TOOLBAR_BT_SEARCH_FWD].ob_state &= ~OS_DISABLED;
				obj[TOOLBAR_BT_SEARCH_BACK].ob_state &= ~OS_DISABLED;
				window_schedule_redraw_grect(input_window->root,
							gemtk_obj_screen_rect(obj, TOOLBAR_BT_SEARCH_FWD));
				window_schedule_redraw_grect(input_window->root,
							gemtk_obj_screen_rect(obj, TOOLBAR_BT_SEARCH_BACK));
		}
    }
    return((done==true) ? 1 : 0);
}


static void on_redraw(ROOTWIN *rootwin, short msg[8])
{
    short handle;

    GRECT clip = {msg[4], msg[5], msg[6], msg[7]};

    //dbg_grect("on_redraw", &clip);

    if(gemtk_wm_get_state(rootwin->win) & GEMTK_WM_STATUS_ICONIFIED) {
        // TODO: remove asignment:
        GRECT clip = {msg[4], msg[5], msg[6], msg[7]};
        window_redraw_favicon(rootwin, NULL);
    } else {
        window_schedule_redraw_grect(rootwin, &clip);
    }
}

static void on_resized(ROOTWIN *rootwin)
{
    GRECT g, work;
    OBJECT *toolbar;
    struct gui_window *gw;

    gw = window_get_active_gui_window(rootwin);

    //printf("resized...\n");

    assert(gw != NULL);

    if(gw == NULL)
        return;

    wind_get_grect(rootwin->aes_handle, WF_CURRXYWH, &g);
	gemtk_wm_get_grect(rootwin->win, GEMTK_WM_AREA_WORK, &work);

    if (rootwin->loc.g_w != g.g_w || rootwin->loc.g_h != g.g_h) {

		/* resized */
    	toolbar_set_width(rootwin->toolbar, work.g_w);

        if ( gw->browser->bw->current_content != NULL ) {
            browser_window_reformat(gw->browser->bw, true, work.g_w, work.g_h);
        }
    }
    if (rootwin->loc.g_x != g.g_x || rootwin->loc.g_y != g.g_y) {
        /* moved */
        toolbar_set_origin(rootwin->toolbar, work.g_x, work.g_y);
    }

    rootwin->loc = g;
}

static void on_file_dropped(ROOTWIN *rootwin, short msg[8])
{
    char file[DD_NAMEMAX];
    char name[DD_NAMEMAX];
    char *buff=NULL;
    int dd_hdl;
    int dd_msg; /* pipe-handle */
    long size;
    char ext[32];
    short mx,my,bmstat,mkstat;
    struct gui_window *gw;

    graf_mkstate(&mx, &my, &bmstat, &mkstat);

    gw = window_get_active_gui_window(rootwin);

    if( gw == NULL )
        return;

    if(gemtk_wm_get_state(rootwin->win) & GEMTK_WM_STATUS_ICONIFIED)
        return;

    dd_hdl = gemtk_dd_open( msg[7], DD_OK);
    if( dd_hdl<0)
        return;	/* pipe not open */
    memset( ext, 0, 32);
    strcpy( ext, "ARGS");
    dd_msg = gemtk_dd_sexts( dd_hdl, ext);
    if( dd_msg<0)
        goto error;
    dd_msg = gemtk_dd_rtry( dd_hdl, (char*)&name[0], (char*)&file[0], (char*)&ext[0], &size);
    if( size+1 >= PATH_MAX )
        goto error;
    if( !strncmp( ext, "ARGS", 4) && dd_msg > 0) {
        gemtk_dd_reply(dd_hdl, DD_OK);
        buff = (char*)malloc(sizeof(char)*(size+1));
        if (buff != NULL) {
            if (Fread(dd_hdl, size, buff ) == size) {

                int sx, sy;
                bool processed = false;
                GRECT content_area;
                struct browser_window * bw = gw->browser->bw;

                buff[size] = 0;

                LOG(("file: %s, ext: %s, size: %d dropped at: %d,%d\n",
                 (char*)buff, (char*)&ext,
                 size, mx, my
                ));

                window_get_grect(rootwin, BROWSER_AREA_CONTENT, &content_area);
                mx = mx - content_area.g_x;
                my = my - content_area.g_y;
                if((mx < 0 || mx > content_area.g_w)
                        || (my < 0 || my > content_area.g_h))
                    return;

                processed = browser_window_drop_file_at_point(gw->browser->bw,
                                                              mx+sx, my+sy,
                                                              NULL);
                if(processed == true) {
                    nserror ret;
                    char *utf8_fn;

                    ret = utf8_from_local_encoding(buff, 0, &utf8_fn);
                    if (ret != NSERROR_OK) {
                        free(buff);
                        /* A bad encoding should never happen */
                        LOG(("utf8_from_local_encoding failed"));
                        assert(ret != NSERROR_BAD_ENCODING);
                        /* no memory */
                        goto error;
                    }
                    gui_window_get_scroll(gw, &sx, &sy);
                    processed = browser_window_drop_file_at_point(gw->browser->bw,
                                                                  mx+sx, my+sy,
                                                                  utf8_fn);
                    free(utf8_fn);
                }

                if(processed == false) {
                    // TODO: use localized string:
                    if(gemtk_msg_box_show(GEMTK_MSG_BOX_CONFIRM, "Open File?") > 0) {
                        nsurl * ns_url = NULL;
                        char * tmp_url = local_file_to_url(buff);
                        if ((tmp_url  != NULL)
                            && nsurl_create(tmp_url, &ns_url) == NSERROR_OK) {
                            browser_window_navigate(gw->browser->bw, ns_url, NULL,
                                BW_NAVIGATE_HISTORY,
                                NULL, NULL, NULL);
                            nsurl_unref(ns_url);
                        }
                    }
                }
            }
        }
    }
error:
    if (buff) {
        free(buff);
    }
    gemtk_dd_close( dd_hdl);
}

static void	toolbar_redraw_cb(GUIWIN *win, uint16_t msg, GRECT *clip)
{
	struct rootwin_data_s * ud;

	if (msg != WM_REDRAW) {
		ud = gemtk_wm_get_user_data(win);

		assert(ud);

		toolbar_redraw(ud->rootwin->toolbar, clip);
	}
}
