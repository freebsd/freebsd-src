/*
 * Copyright 2012 Ole Loots <ole@monochrom.net>
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
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "utils/log.h"
#include "desktop/gui.h"
#include "desktop/netsurf.h"
#include "desktop/browser.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/plot_style.h"
#include "desktop/plotters.h"
#include "desktop/tree.h"
#include "desktop/hotlist.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/utf8.h"
#include "atari/clipboard.h"
#include "atari/gui.h"
#include "atari/toolbar.h"
#include "atari/rootwin.h"

#include "atari/clipboard.h"
#include "atari/misc.h"
#include "atari/plot/plot.h"
#include "cflib.h"
#include "atari/res/netsurf.rsh"

#include "desktop/textarea.h"
#include "desktop/textinput.h"
#include "content/hlcache.h"
#include "atari/encoding.h"


#define TB_BUTTON_WIDTH 32
#define THROBBER_WIDTH 32
#define THROBBER_MIN_INDEX 1
#define THROBBER_MAX_INDEX 12
#define THROBBER_INACTIVE_INDEX 13

enum e_toolbar_button_states {
        button_on = 0,
        button_off = 1
};
#define TOOLBAR_BUTTON_NUM_STATES   2

struct s_toolbar;

struct s_tb_button
{
	short rsc_id;
	void (*cb_click)(struct s_toolbar *tb);
	hlcache_handle *icon[TOOLBAR_BUTTON_NUM_STATES];
	struct s_toolbar *owner;
    enum e_toolbar_button_states state;
    short index;
    GRECT area;
};


extern char * option_homepage_url;
extern void * h_gem_rsrc;
extern struct gui_window * input_window;
extern long atari_plot_flags;
extern int atari_plot_vdi_handle;
extern EVMULT_OUT aes_event_out;

static OBJECT * aes_toolbar = NULL;
static OBJECT * throbber_form = NULL;
static bool init = false;
static int area_navigation_height = 0;
static int area_search_height = 0;
static int area_full_height = 0;
static float toolbar_url_scale = 1.0;

static plot_font_style_t font_style_url = {
    .family = PLOT_FONT_FAMILY_SANS_SERIF,
    .size = 14*FONT_SIZE_SCALE,
    .weight = 400,
    .flags = FONTF_NONE,
    .background = 0xffffff,
    .foreground = 0x0
 };


/* prototypes & order for button widgets: */


static struct s_tb_button tb_buttons[] =
{
	{
        TOOLBAR_BT_BACK,
        toolbar_back_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_HOME,
        toolbar_home_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_FORWARD,
        toolbar_forward_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_STOP,
        toolbar_stop_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{
        TOOLBAR_BT_RELOAD,
        toolbar_reload_click,
        {0,0},
        0, 0, 0, {0,0,0,0}
    },
	{ 0, 0, {0,0}, 0, -1, 0, {0,0,0,0}}
};

struct s_toolbar_style {
	int font_height_pt;
};

static struct s_toolbar_style toolbar_styles[] =
{
	/* small (18 px height) */
	{9},
	/* medium (default - 26 px height) */
	{14},
	/* large ( 49 px height ) */
	{18},
	/* custom style: */
	{18}
};

static const struct redraw_context toolbar_rdrw_ctx = {
				.interactive = true,
				.background_images = true,
				.plot = &atari_plotters
			};

static void tb_txt_request_redraw(void *data, int x, int y, int w, int h );
static nserror toolbar_icon_callback( hlcache_handle *handle,
		const hlcache_event *event, void *pw );

/**
*   Find a button for a specific resource ID
*/
static struct s_tb_button *find_button(struct s_toolbar *tb, int rsc_id)
{
	int i = 0;
	while (i < tb->btcnt) {
		if (tb->buttons[i].rsc_id == rsc_id) {
			return(&tb->buttons[i]);
		}
		i++;
	}
	return(NULL);
}

/**
*   Callback for textarea redraw
*/
static void tb_txt_request_redraw(void *data, int x, int y, int w, int h)
{

    GRECT area;
	struct s_toolbar * tb = (struct s_toolbar *)data;

	if (tb->attached == false) {
        return;
    }

	toolbar_get_grect(tb, TOOLBAR_AREA_URL, &area);
	area.g_x += x;
	area.g_y += y;
	area.g_w = w;
	area.g_h = h;
	//dbg_grect("tb_txt_request_redraw", &area);
	window_schedule_redraw_grect(tb->owner, &area);
    return;
}

static void tb_txt_callback(void *data, struct textarea_msg *msg)
{
	switch (msg->type) {

		case TEXTAREA_MSG_DRAG_REPORT:
		break;

		case TEXTAREA_MSG_REDRAW_REQUEST:
			tb_txt_request_redraw(data,
				msg->data.redraw.x0, msg->data.redraw.y0,
				msg->data.redraw.x1 - msg->data.redraw.x0,
				msg->data.redraw.y1 - msg->data.redraw.y0);
		break;

		default:
		break;
	}
}

static struct s_tb_button *button_init(struct s_toolbar *tb, OBJECT * tree, int index,
							struct s_tb_button * instance)
{
	*instance = tb_buttons[index];
	instance->owner = tb;

    return(instance);
}


static short __CDECL toolbar_url_userdraw(PARMBLK *parmblock)
{
	return(0);
}

void toolbar_init( void )
{
	static USERBLK userblk;

    aes_toolbar = gemtk_obj_get_tree(TOOLBAR);
    throbber_form = gemtk_obj_get_tree(THROBBER);

	userblk.ub_code = toolbar_url_userdraw;
    userblk.ub_parm = (long) aes_toolbar[TOOLBAR_AREA_URL].ob_spec.userblk;
    aes_toolbar[TOOLBAR_AREA_URL].ob_spec.userblk = &userblk;

	aes_toolbar[TOOLBAR_CB_SHOWALL].ob_state &= ~OS_SELECTED;
	aes_toolbar[TOOLBAR_CB_CASESENSE].ob_state &= ~OS_SELECTED;

	/* init default values: */
	gemtk_obj_set_str_safe(aes_toolbar, TOOLBAR_TB_SRCH, (char*)"");

    area_full_height = aes_toolbar->ob_height;
	area_search_height = aes_toolbar[TOOLBAR_AREA_SEARCH].ob_height;
	area_navigation_height = aes_toolbar[TOOLBAR_AREA_NAVIGATION].ob_height;

    init = true;
}


void toolbar_exit(void)
{

}


struct s_toolbar *toolbar_create(struct s_gui_win_root *owner)
{
	int i;
	struct s_toolbar *t;

	LOG((""));

	assert(init == true);

	t = calloc(1, sizeof(struct s_toolbar));

	assert(t);

	/* initialize the toolbar values: */
	t->owner = owner;
	t->style = 1;
	t->search_visible = false;
	t->visible = true;
	t->reflow = true;

	/* dublicate the form template: */
	t->form = gemtk_obj_tree_copy(aes_toolbar);


	/* count buttons and add them as components: */
	i = 0;
	while(tb_buttons[i].rsc_id > 0) {
		i++;
	}
	t->btcnt = i;
	t->buttons = malloc(t->btcnt * sizeof(struct s_tb_button));
	memset(t->buttons, 0, t->btcnt * sizeof(struct s_tb_button));
	for (i=0; i < t->btcnt; i++) {
		button_init(t, aes_toolbar, i, &t->buttons[i]);
	}

	/* create the url widget: */
	font_style_url.size =
		toolbar_styles[t->style].font_height_pt * FONT_SIZE_SCALE;

	textarea_flags ta_flags = TEXTAREA_INTERNAL_CARET;
	textarea_setup ta_setup;
	ta_setup.width = 300;
	ta_setup.height = t->form[TOOLBAR_AREA_URL].ob_height;
	ta_setup.pad_top = 0;
	ta_setup.pad_right = 4;
	ta_setup.pad_bottom = 0;
	ta_setup.pad_left = 4;
	ta_setup.border_width = 1;
	ta_setup.border_col = 0x000000;
	ta_setup.selected_text = 0xffffff;
	ta_setup.selected_bg = 0x000000;
	ta_setup.text = font_style_url;
	ta_setup.text.foreground = 0x000000;
	ta_setup.text.background = 0xffffff;
	t->url.textarea = textarea_create(ta_flags, &ta_setup,
			tb_txt_callback, t);

	/* create the throbber widget: */
	t->throbber.index = THROBBER_INACTIVE_INDEX;
	t->throbber.max_index = THROBBER_MAX_INDEX;
	t->throbber.running = false;

	LOG(("created toolbar: %p, root: %p, textarea: %p, throbber: %p", t,
        owner, t->url.textarea, t->throbber));
	return( t );
}


void toolbar_destroy(struct s_toolbar *tb)
{
    free(tb->buttons);
	free(tb->form);

	textarea_destroy(tb->url.textarea);

	free(tb);
}

static int toolbar_calculate_height(struct s_toolbar *tb)
{
	int r = 0;

	if (tb->visible == false) {
		return(0);
	}

	r += area_navigation_height;

	if (tb->search_visible) {
		r += area_search_height;
	}

	return(r);
}

static void toolbar_reflow(struct s_toolbar *tb)
{
    int i;
    short offx, offy;

    // position toolbar areas:
    tb->form->ob_x = tb->area.g_x;
    tb->form->ob_y = tb->area.g_y;
    tb->form->ob_width = tb->area.g_w;
    tb->form->ob_height = toolbar_calculate_height(tb);

	// expand the "main" areas to the current width:
    tb->form[TOOLBAR_AREA_NAVIGATION].ob_width = tb->area.g_w;
    tb->form[TOOLBAR_AREA_SEARCH].ob_width = tb->area.g_w;

    if (tb->search_visible) {
		tb->form[TOOLBAR_AREA_SEARCH].ob_state &= ~OF_HIDETREE;
    } else {
    	tb->form[TOOLBAR_AREA_SEARCH].ob_state |= OF_HIDETREE;

    }

	// align TOOLBAR_AREA_RIGHT IBOX at right edge:
    tb->form[TOOLBAR_AREA_RIGHT].ob_x = tb->area.g_w
        - tb->form[TOOLBAR_AREA_RIGHT].ob_width;

	// center the URL area:
    tb->form[TOOLBAR_AREA_URL].ob_width = tb->area.g_w
       - (tb->form[TOOLBAR_AREA_LEFT].ob_width
       + tb->form[TOOLBAR_AREA_RIGHT].ob_width);

    // position throbber image above IBOX:
    objc_offset(tb->form, TOOLBAR_THROBBER_AREA, &offx, &offy);
    throbber_form[tb->throbber.index].ob_x = offx;
    throbber_form[tb->throbber.index].ob_y = offy;

	// align the search button:
	tb->form[TOOLBAR_SEARCH_ALIGN_RIGHT].ob_x = tb->area.g_w
        - tb->form[TOOLBAR_SEARCH_ALIGN_RIGHT].ob_width;

    // set button states:
    for (i=0; i < tb->btcnt; i++ ) {
        if (tb->buttons[i].state == button_off) {
            tb->form[tb->buttons[i].rsc_id].ob_state |= OS_DISABLED;
        }
        else if (tb->buttons[i].state == button_on) {
            tb->form[tb->buttons[i].rsc_id].ob_state &= ~OS_DISABLED;
        }
	}
    tb->reflow = false;
    // TODO: iterate through all other toolbars and set reflow = true
}

void toolbar_redraw(struct s_toolbar *tb, GRECT *clip)
{
    GRECT area, area_ro;
    float old_scale;

    if (tb->attached == false) {
        return;
    }

    if(tb->reflow == true)
        toolbar_reflow(tb);

	//dbg_grect("toolbar redraw clip", clip);

	/* Redraw the AES objects: */
    objc_draw_grect(tb->form,0,8,clip);
    objc_draw_grect(&throbber_form[tb->throbber.index], 0, 1, clip);

    toolbar_get_grect(tb, TOOLBAR_AREA_URL, &area_ro);
    area = area_ro;

    if (rc_intersect(clip, &area)) {

        plot_set_dimensions(area_ro.g_x, area_ro.g_y, area_ro.g_w, area_ro.g_h);
        struct rect r = {
					.x0 = MAX(0,area.g_x - area_ro.g_x),
					.y0 = MAX(0,area.g_y - area_ro.g_y),
					.x1 = MAX(0,area.g_x - area_ro.g_x) + area.g_w,
					.y1 = MAX(0,area.g_y - area_ro.g_y) + area.g_h
		};
		//dbg_rect("tb textarea clip: ", &r);
		// TODO: let this be handled by an userdef object redraw function:
		/* Redraw the url input: */
		old_scale = plot_set_scale(toolbar_url_scale);
        textarea_redraw(tb->url.textarea, 0, 0, 0xffffff, 1.0, &r,
                        &toolbar_rdrw_ctx);
        plot_set_scale(old_scale);
    }
}


void toolbar_update_buttons(struct s_toolbar *tb, struct browser_window *bw,
                       short button)
{
    LOG((""));

	struct s_tb_button * bt;
	bool enable = false;
	GRECT area;
	nsurl * ns_url;
	char * c_url;
	size_t c_url_len;

	assert(bw != NULL);

	if (button == TOOLBAR_BT_BACK || button <= 0 ) {
	    bt = find_button(tb, TOOLBAR_BT_BACK);
		enable = browser_window_back_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

	if (button == TOOLBAR_BT_HOME || button <= 0 ) {

	}

	if (button == TOOLBAR_BT_FORWARD || button <= 0 ) {
		bt = find_button(tb, TOOLBAR_BT_FORWARD);
		enable = browser_window_forward_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

	if (button == TOOLBAR_BT_RELOAD || button <= 0 ) {
	    bt = find_button(tb, TOOLBAR_BT_RELOAD);
		enable = browser_window_reload_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

	if (button == TOOLBAR_BT_STOP || button <= 0) {
	    bt = find_button(tb, TOOLBAR_BT_STOP);
		enable = browser_window_stop_available(bw);
        if (enable) {
            bt->state = button_on;
        } else {
            bt->state = button_off;
        }
	}

    if (tb->attached) {
        if (button > 0) {
            toolbar_get_grect(tb, button, &area);
            window_schedule_redraw_grect(tb->owner, &area);
        }
        else {
            toolbar_get_grect(tb, TOOLBAR_AREA_LEFT, &area);
            window_schedule_redraw_grect(tb->owner, &area);

            toolbar_get_grect(tb, TOOLBAR_AREA_RIGHT, &area);
            window_schedule_redraw_grect(tb->owner, &area);
        }
    }
}

void toolbar_set_width(struct s_toolbar *tb, short w)
{
	GRECT cur;

	toolbar_get_grect(tb, 0, &cur);

	if (w != cur.g_w) {

		tb->area.g_w = w;

        /* reflow now, just for url input calucation: */
        toolbar_reflow(tb);
        /* this will request an textarea redraw: */
        textarea_set_dimensions(tb->url.textarea,
                                tb->form[TOOLBAR_AREA_URL].ob_width,
                                tb->form[TOOLBAR_AREA_URL].ob_height);
		tb->reflow = true;
	}
}

void toolbar_set_origin(struct s_toolbar *tb, short x, short y)
{
	GRECT cur;

	toolbar_get_grect(tb, 0, &cur);

	if (x != cur.g_x || y != cur.g_y) {
		tb->area.g_x = x;
		tb->area.g_y = y;
		tb->reflow = true;
	}
}

void toolbar_set_dimensions(struct s_toolbar *tb, GRECT *area)
{
    if (area->g_w != tb->area.g_w)  {

        tb->area = *area;

        /* reflow now, just for url input calucation: */
        toolbar_reflow(tb);
        /* this will request an textarea redraw: */
        textarea_set_dimensions(tb->url.textarea,
                                tb->form[TOOLBAR_AREA_URL].ob_width,
                                tb->form[TOOLBAR_AREA_URL].ob_height-1);
    }
    else {
        tb->area = *area;
    }
	/* reflow for next redraw: */
    /* TODO: that's only required because we do not reset others toolbars reflow
             state on reflow */
    tb->reflow = true;
}


void toolbar_set_url(struct s_toolbar *tb, const char * text)
{
    LOG((""));
    textarea_set_text(tb->url.textarea, text);

    if (tb->attached && tb->visible) {
        GRECT area;
        toolbar_get_grect(tb, TOOLBAR_AREA_URL, &area);
        window_schedule_redraw_grect(tb->owner, &area);
        struct gui_window * gw = window_get_active_gui_window(tb->owner);
        assert(gw != NULL);
        toolbar_update_buttons(tb, gw->browser->bw , 0);
	}
}

void toolbar_set_throbber_state(struct s_toolbar *tb, bool active)
{
    GRECT throbber_area;

    tb->throbber.running = active;
    if (active) {
        tb->throbber.index = THROBBER_MIN_INDEX;
    } else {
        tb->throbber.index = THROBBER_INACTIVE_INDEX;
    }

    tb->reflow = true;
    toolbar_get_grect(tb, TOOLBAR_THROBBER_AREA, &throbber_area);
    window_schedule_redraw_grect(tb->owner, &throbber_area);
}

void toolbar_set_visible(struct s_toolbar *tb, short area, bool visible)
{
	if (area == 0) {
		if ((visible == false) && (tb->visible == true)) {
			tb->visible = false;
			tb->reflow = true;
		} else if((visible == true) && (tb->visible == false)) {
			tb->visible = false;
			tb->reflow = true;
		}
	}
	else if (area == TOOLBAR_AREA_SEARCH) {
		tb->search_visible = visible;
		tb->reflow = true;
		OBJECT *frm = toolbar_get_form(tb);
		if(visible == false){
			frm[TOOLBAR_AREA_SEARCH].ob_flags |= OF_HIDETREE;
		} else {
			frm[TOOLBAR_AREA_SEARCH].ob_flags &= ~OF_HIDETREE;
		}
	}
}

void toolbar_set_reflow(struct s_toolbar *tb, bool do_reflow)
{
	tb->reflow = do_reflow;
}

void toolbar_set_attached(struct s_toolbar *tb, bool attached)
{
    tb->attached = attached;

}

void toolbar_throbber_progress(struct s_toolbar *tb)
{
    GRECT throbber_area;

    assert(tb->throbber.running == true);

    if(tb->throbber.running == false)
        return;

    tb->throbber.index++;
    if(tb->throbber.index > THROBBER_MAX_INDEX)
        tb->throbber.index = THROBBER_MIN_INDEX;

    tb->reflow = true;
    toolbar_get_grect(tb, TOOLBAR_THROBBER_AREA, &throbber_area);
    window_schedule_redraw_grect(tb->owner, &throbber_area);
}

bool toolbar_text_input(struct s_toolbar *tb, char *text)
{
    bool handled = true;

    LOG((""));

    return(handled);
}

bool toolbar_key_input(struct s_toolbar *tb, short nkc)
{

	assert(tb!=NULL);

	GRECT work;
	bool ret = false;
	struct gui_window *gw = window_get_active_gui_window(tb->owner);

	assert( gw != NULL );

	long ucs4;
	long ik = nkc_to_input_key(nkc, &ucs4);

	if (ik == 0) {
		if ((nkc&0xFF) >= 9) {
			ret = textarea_keypress(tb->url.textarea, ucs4);
		}
	}
	else if (ik == KEY_CR || ik == KEY_NL) {
		nsurl *url;
		char tmp_url[PATH_MAX];
		if ( textarea_get_text( tb->url.textarea, tmp_url, PATH_MAX) > 0 ) {
			window_set_focus(tb->owner, BROWSER, gw->browser);
			if (nsurl_create((const char*)&tmp_url, &url) != NSERROR_OK) {
				warn_user("NoMemory", 0);
			} else {
				browser_window_navigate(gw->browser->bw, url, NULL,
					BW_NAVIGATE_HISTORY,
					NULL, NULL, NULL);
				nsurl_unref(url);
			}

			ret = true;
		}
	}
	else if (ik == KEY_COPY_SELECTION) {
		// copy whole text
		char * text;
		int len;
		len = textarea_get_text( tb->url.textarea, NULL, 0 );
		text = malloc( len+1 );
		if (text){
			textarea_get_text( tb->url.textarea, text, len+1 );
			scrap_txt_write(text);
			free( text );
		}
	}
	else if ( ik == KEY_PASTE) {
		char * clip = scrap_txt_read();
		if ( clip != NULL ){
			int clip_length = strlen( clip );
			if ( clip_length > 0 ) {
				char *utf8;
				nserror res;
				/* Clipboard is in local encoding so
				 * convert to UTF8 */
				res = utf8_from_local_encoding( clip, clip_length, &utf8 );
				if ( res == NSERROR_OK ) {
					toolbar_set_url(tb, utf8);
					free(utf8);
					ret = true;
				}
			}
			free( clip );
		}
	}
	else if (ik == KEY_ESCAPE) {
		textarea_keypress( tb->url.textarea, KEY_SELECT_ALL );
		textarea_keypress( tb->url.textarea, KEY_DELETE_LEFT );
	}
	else {
		ret = textarea_keypress( tb->url.textarea, ik );
	}

	return( ret );
}


void toolbar_mouse_input(struct s_toolbar *tb, short obj, short button)
{
    LOG((""));
    GRECT work;
	short mx, my, mb, kstat;
	int old;
	OBJECT * toolbar_tree;
	struct gui_window * gw;


    if (obj==TOOLBAR_AREA_URL){

        graf_mkstate(&mx, &my, &mb,  &kstat);
        toolbar_get_grect(tb, TOOLBAR_AREA_URL, &work);
        mx -= work.g_x;
        my -= work.g_y;

	    /* TODO: reset mouse state of browser window? */
	    /* select whole text when newly focused, otherwise set caret to
	        end of text */
        if (!window_url_widget_has_focus(tb->owner)) {
            window_set_focus(tb->owner, URL_WIDGET, (void*)&tb->url);
        }
        /* url widget has focus and mouse button is still pressed... */
        else if (mb & 1) {

            textarea_mouse_action(tb->url.textarea, BROWSER_MOUSE_DRAG_1,
									mx, my );
			short prev_x = mx;
			short prev_y = my;
			do {
				if (abs(prev_x-mx) > 5 || abs(prev_y-my) > 5) {
					textarea_mouse_action( tb->url.textarea,
										BROWSER_MOUSE_HOLDING_1, mx, my );
					prev_x = mx;
					prev_y = my;
					window_schedule_redraw_grect(tb->owner, &work);
					window_process_redraws(tb->owner);
				}
				graf_mkstate( &mx, &my, &mb,  &kstat );
				mx = mx - (work.g_x);
				my = my - (work.g_y);
            } while (mb & 1);

			textarea_mouse_action( tb->url.textarea, BROWSER_MOUSE_HOVER, mx,
									my);
        }
        else if (button & 2) {
			// TODO: open a context popup
        }
        else {
            /* when execution reaches here, mouse input is a click or dclick */
            /* TODO: recognize click + shitoolbar_update_buttonsft key */
			int mstate = BROWSER_MOUSE_PRESS_1;
			if ((kstat & (K_LSHIFT|K_RSHIFT)) != 0) {
				mstate = BROWSER_MOUSE_MOD_1;
			}
            if (aes_event_out.emo_mclicks == 2 ) {
				textarea_mouse_action( tb->url.textarea,
						BROWSER_MOUSE_DOUBLE_CLICK | BROWSER_MOUSE_CLICK_1, mx,
						my);
                toolbar_get_grect(tb, TOOLBAR_AREA_URL, &work);
                window_schedule_redraw_grect(tb->owner, &work);
			} else {
				textarea_mouse_action(tb->url.textarea,
						BROWSER_MOUSE_PRESS_1, mx, my );
			}
        }
    }
    else if(obj==TOOLBAR_TB_SRCH) {
		window_set_focus(tb->owner, SEARCH_INPUT, NULL);
    }
	else if (obj==TOOLBAR_BT_SEARCH_FWD) {
		gw = tb->owner->active_gui_window;
		assert(gw->search);
		nsatari_search_perform(gw->search, tb->form, SEARCH_FLAG_FORWARDS);
	}
	else if (obj==TOOLBAR_BT_SEARCH_BACK) {
		gw = tb->owner->active_gui_window;
		assert(gw->search);
		nsatari_search_perform(gw->search, tb->form, 0);
	}
	else if (obj==TOOLBAR_BT_CLOSE_SEARCH) {
		tb->form[TOOLBAR_BT_CLOSE_SEARCH].ob_state &= ~OS_SELECTED;
		window_close_search(tb->owner);
	}
    else {
        struct s_tb_button *bt = find_button(tb, obj);
        if (bt != NULL && bt->state != button_off) {
            bt->cb_click(tb);
            struct gui_window * gw = window_get_active_gui_window(tb->owner);
            toolbar_update_buttons(tb, gw->browser->bw, 0);
        }

    }
}


/**
* Receive a specific region of the toolbar.
* @param tb - the toolbar pointer
* @param which - the area to retrieve:  0 to receive the workarea,
										all other values must be
										an resource ID of the TOOLBAR tree.
* @param dst - GRECT pointer receiving the area.
*/

void toolbar_get_grect(struct s_toolbar *tb, short which, GRECT *dst)
{
	#define LAST_TOOLBAR_AREA TOOLBAR_AREA_SEARCH

    if (tb->reflow == true) {
        toolbar_reflow(tb);
    }

    objc_offset(tb->form, which, &dst->g_x, &dst->g_y);

    dst->g_w = tb->form[which].ob_width;
    dst->g_h = tb->form[which].ob_height;
    //tb->form[which].ob_height;

    //printf("Toolbar get grect (%d): ", which);
    //dbg_grect("", dst);

    #undef LAST_TOOLBAR_AREA
}


struct textarea *toolbar_get_textarea(struct s_toolbar *tb,
                                       enum toolbar_textarea which)
{
    return(tb->url.textarea);
}

char *toolbar_get_url(struct s_toolbar *tb)
{
    char * c_url = NULL;
    int c_url_len = 0;

    c_url_len = textarea_get_text(tb->url.textarea, NULL, 0);

    if (c_url_len > -1) {
        c_url = malloc(c_url_len+1);
        textarea_get_text(tb->url.textarea, c_url, c_url_len+1);
    }

    return(c_url);
}

nsurl * toolbar_get_nsurl(struct s_toolbar * tb)
{

    nsurl * ns_url = NULL;
    char * c_url;

    c_url = toolbar_get_url(tb);
    if (c_url) {
        nsurl_create(c_url, &ns_url);
    }

    return(ns_url);
}


OBJECT *toolbar_get_form(struct s_toolbar *tb)
{
	return(tb->form);
}


/* public event handler */
void toolbar_back_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

    if( browser_window_back_available(bw) )
		browser_window_history_back(bw, false);
}

void toolbar_reload_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

	browser_window_reload(bw, true);
}

void toolbar_forward_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);
    assert(gw != NULL);
    bw = gw->browser->bw;
    assert(bw != NULL);

	if (browser_window_forward_available(bw))
		browser_window_history_forward(bw, false);
}

void toolbar_home_click(struct s_toolbar *tb)
{
	struct browser_window * bw;
	struct gui_window * gw;
	nsurl *url;
	char * use_url = NULL;

	gw = window_get_active_gui_window(tb->owner);
	assert(gw != NULL);
	bw = gw->browser->bw;
	assert(bw != NULL);

	use_url = nsoption_charp(homepage_url);
	if(use_url == NULL || strlen(use_url) == 0){
        use_url = (char*)"about:welcome";
	}

	if (nsurl_create(use_url, &url) != NSERROR_OK) {
		warn_user("NoMemory", 0);
	} else {
		browser_window_navigate(bw,
					url,
					NULL,
					BW_NAVIGATE_HISTORY,
					NULL,
					NULL,
					NULL);
			nsurl_unref(url);
	}
}


void toolbar_stop_click(struct s_toolbar *tb)
{
    struct browser_window * bw;
    struct gui_window * gw;

    gw = window_get_active_gui_window(tb->owner);

    assert(gw != NULL);

    bw = gw->browser->bw;

    assert(bw != NULL);

	browser_window_stop(bw);
}

void toolbar_favorite_click(struct s_toolbar *tb)
{
    nsurl * ns_url = NULL;
    char * c_url;
    int c_url_len = 0;

    c_url = toolbar_get_url(tb);
    c_url_len = strlen(c_url);

    nsurl_create(c_url, &ns_url);

    if (hotlist_has_url(ns_url)) {
        char msg[c_url_len+100];
        snprintf(msg, c_url_len+100, "Really delete from favorites: \"%s\"",
                c_url);
        if(gemtk_msg_box_show(GEMTK_MSG_BOX_CONFIRM, msg)) {
            hotlist_remove_url(ns_url);
        }
    }
    else {
        hotlist_add_url(ns_url);
    }

    nsurl_unref(ns_url);
    free(c_url);
}

void toolbar_crypto_click(struct s_toolbar *tb)
{

}

