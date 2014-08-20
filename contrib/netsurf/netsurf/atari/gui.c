/*
 * Copyright 2010 <ole@monochrom.net>
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

/*
	This File provides all the mandatory functions prefixed with gui_
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <hubbub/hubbub.h>

#include "utils/url.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/nsoption.h"
#include "content/urldb.h"
#include "content/fetch.h"
#include "content/fetchers/resource.h"
#include "css/utils.h"
#include "desktop/gui.h"
#include "desktop/plotters.h"
#include "desktop/netsurf.h"
#include "desktop/save_complete.h"
#include "desktop/textinput.h"
#include "desktop/treeview.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "render/font.h"

#include "atari/gemtk/gemtk.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/findfile.h"
#include "atari/schedule.h"
#include "atari/rootwin.h"
#include "atari/statusbar.h"
#include "atari/toolbar.h"
#include "atari/hotlist.h"
#include "atari/cookies.h"
#include "atari/certview.h"
#include "atari/history.h"
#include "atari/login.h"
#include "atari/encoding.h"
#include "atari/res/netsurf.rsh"
#include "atari/plot/plot.h"
#include "atari/clipboard.h"
#include "atari/osspec.h"
#include "atari/search.h"
#include "atari/deskmenu.h"
#include "atari/download.h"
#include "atari/filetype.h"
#include "cflib.h"

#define TODO() (0)/*printf("%s Unimplemented!\n", __FUNCTION__)*/

struct gui_window *input_window = NULL;
struct gui_window *window_list = NULL;
void * h_gem_rsrc;
long next_poll;
bool rendering = false;
GRECT desk_area;


/* Comandline / Options: */
int option_window_width;
int option_window_height;
int option_window_x;
int option_window_y;

/* Defaults to option_homepage_url, commandline options overwrites that value */
const char * option_homepage_url;

/* path to choices file: */
char options[PATH_MAX];

EVMULT_IN aes_event_in = {
    .emi_flags = MU_MESAG | MU_TIMER | MU_KEYBD | MU_BUTTON | MU_M1,
    .emi_bclicks = 258,
    .emi_bmask = 3,
    .emi_bstate = 0,
    .emi_m1leave = MO_ENTER,
    .emi_m1 = {0,0,0,0},
    .emi_m2leave = 0,
    .emi_m2 = {0,0,0,0},
    .emi_tlow = 0,
    .emi_thigh = 0
};
EVMULT_OUT aes_event_out;
short aes_msg_out[8];

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy);
static void gui_window_set_url(struct gui_window *w, const char *url);

/**
 * Return the filename part of a full path
 *
 * \param path full path and filename
 * \return filename (will be freed with free())
 */
static char *filename_from_path(char *path)
{
	char *leafname;

	leafname = strrchr(path, '\\');
	if( !leafname )
		leafname = strrchr(path, '/');
	if (!leafname)
		leafname = path;
	else
		leafname += 1;

	return strdup(leafname);
}


static void gui_poll(bool active)
{

	struct gui_window *tmp;
    short mx, my, dummy;
    unsigned short nkc = 0;

    aes_event_in.emi_tlow = schedule_run();

    if(active || rendering){
        aes_event_in.emi_tlow = nsoption_int(atari_gui_poll_timeout);
	}

    if(aes_event_in.emi_tlow < 0) {
        aes_event_in.emi_tlow = 10000;
        printf("long poll!\n");
    }

    struct gui_window * g;

    if( !active ) {
        if(input_window && input_window->root->redraw_slots.areas_used > 0) {
            window_process_redraws(input_window->root);
        }
    }

    graf_mkstate(&mx, &my, &dummy, &dummy);
    aes_event_in.emi_m1.g_x = mx;
    aes_event_in.emi_m1.g_y = my;
    evnt_multi_fast(&aes_event_in, aes_msg_out, &aes_event_out);
    if(gemtk_wm_dispatch_event(&aes_event_in, &aes_event_out, aes_msg_out) == 0) {
        if( (aes_event_out.emo_events & MU_MESAG) != 0 ) {
            LOG(("WM: %d\n", aes_msg_out[0]));
            switch(aes_msg_out[0]) {

            case MN_SELECTED:
                LOG(("Menu Item: %d\n",aes_msg_out[4]));
                deskmenu_dispatch_item(aes_msg_out[3], aes_msg_out[4]);
                break;

            case AP_TERM:
                netsurf_quit = true;
                break;
            default:
                break;
            }
        }
        if((aes_event_out.emo_events & MU_KEYBD) != 0) {
            uint16_t nkc = gem_to_norm( (short)aes_event_out.emo_kmeta,
                                        (short)aes_event_out.emo_kreturn);
            deskmenu_dispatch_keypress(aes_event_out.emo_kreturn,
                                       aes_event_out.emo_kmeta, nkc);
        }
    }

    tmp = window_list;
    while(tmp){
		if(tmp->root->redraw_slots.areas_used > 0){
			window_process_redraws(tmp->root);
		}
		tmp = tmp->next;
    }

	// TODO: implement generic treeview redraw function
	// TODO: rename hl to atari_hotlist or create getter for it...

	atari_treeview_flush_redraws();

}


static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags) {
    struct gui_window *gw=NULL;
    LOG(( "gw: %p, BW: %p, existing %p, flags: %d\n" , gw,  bw, existing,
          (int)flags
        ));

    gw = calloc(1, sizeof(struct gui_window));
    if (gw == NULL)
        return NULL;

    LOG(("new window: %p, bw: %p\n", gw, bw));
    window_create(gw, bw, existing, WIDGET_STATUSBAR|WIDGET_TOOLBAR|WIDGET_RESIZE\
                  |WIDGET_SCROLL);
    if (gw->root->win) {
        GRECT pos = {
            option_window_x, option_window_y,
            option_window_width, option_window_height
        };
        gui_window_set_scale(gw, 1.0);
        gui_window_set_url(gw, "");
        gui_window_set_pointer(gw, BROWSER_POINTER_DEFAULT);
        gui_set_input_gui_window(gw);
        window_open(gw->root, gw, pos);
    }

    /* add the window to the window list: */
    if( window_list == NULL ) {
        window_list = gw;
        gw->next = NULL;
        gw->prev = NULL;
    } else {
        struct gui_window * tmp = window_list;
        while( tmp->next != NULL ) {
            tmp = tmp->next;
        }
        tmp->next = gw;
        gw->prev = tmp;
        gw->next = NULL;
    }

    return( gw );

}

void gui_window_destroy(struct gui_window *w)
{
    if (w == NULL)
        return;

    LOG(("%s\n", __FUNCTION__ ));

    if (input_window == w) {
        gui_set_input_gui_window(NULL);
    }

    nsatari_search_session_destroy(w->search);
    free(w->browser);
    free(w->status);
    free(w->title);
    free(w->url);

    /* unlink the window: */
    if(w->prev != NULL ) {
        w->prev->next = w->next;
    } else {
        window_list = w->next;
    }
    if( w->next != NULL ) {
        w->next->prev = w->prev;
    }

    window_unref_gui_window(w->root, w);

    free(w);
    w = NULL;

    if(input_window == NULL) {
        w = window_list;
        while( w != NULL ) {
            if(w->root) {
                gui_set_input_gui_window(w);
                break;
            }
            w = w->next;
        }
    }
}

static void
gui_window_get_dimensions(struct gui_window *w,
			  int *width,
			  int *height,
			  bool scaled)
{
    if (w == NULL)
        return;
    GRECT rect;
    window_get_grect(w->root, BROWSER_AREA_CONTENT, &rect);
    *width = rect.g_w;
    *height = rect.g_h;
}

static void gui_window_set_title(struct gui_window *gw, const char *title)
{

    if (gw == NULL)
        return;

    if (gw->root) {

        int l;
        char * conv;
        l = strlen(title)+1;
        if (utf8_to_local_encoding(title, l-1, &conv) == NSERROR_OK ) {
            l = MIN((uint32_t)atari_sysinfo.aes_max_win_title_len, strlen(conv));
            if(gw->title == NULL)
                gw->title = malloc(l);
            else
                gw->title = realloc(gw->title, l);

            strncpy(gw->title, conv, l);
            free( conv );
        } else {
            l = MIN((size_t)atari_sysinfo.aes_max_win_title_len, strlen(title));
            if(gw->title == NULL)
                gw->title = malloc(l);
            else
                gw->title = realloc(gw->title, l);
            strncpy(gw->title, title, l);
        }
        gw->title[l] = 0;
        if(input_window == gw)
            window_set_title(gw->root, gw->title);
    }
}

/**
 * set the status bar message
 */
void gui_window_set_status(struct gui_window *w, const char *text)
{
    int l;
    if (w == NULL || text == NULL)
        return;

    assert(w->root);

    l = strlen(text)+1;
    if(w->status == NULL)
        w->status = malloc(l);
    else
        w->status = realloc(w->status, l);

    strncpy(w->status, text, l);
    w->status[l] = 0;

    if(input_window == w)
        window_set_stauts(w->root, (char*)text);
}

float gui_window_get_scale(struct gui_window *gw)
{
    return(gw->scale);
}

void gui_window_set_scale(struct gui_window *gw, float scale)
{
    int width = 0, heigth = 0;

	LOG(("scale: %f", scale));

    gw->scale = MAX(scale, 0.25);

	gui_window_get_dimensions(gw, &width, &heigth, true);
 	browser_window_reformat(gw->browser->bw, false, width, heigth);
}

static void gui_window_redraw_window(struct gui_window *gw)
{
    CMP_BROWSER b;
    GRECT rect;
    if (gw == NULL)
        return;
    b = gw->browser;
    window_get_grect(gw->root, BROWSER_AREA_CONTENT, &rect);
    window_schedule_redraw_grect(gw->root, &rect);
}

static void gui_window_update_box(struct gui_window *gw, const struct rect *rect)
{
    GRECT area;
    struct gemtk_wm_scroll_info_s *slid;

    if (gw == NULL)
        return;

    slid = gemtk_wm_get_scroll_info(gw->root->win);

    window_get_grect(gw->root, BROWSER_AREA_CONTENT, &area);
    area.g_x += rect->x0 - (slid->x_pos * slid->x_unit_px);
    area.g_y += rect->y0 - (slid->y_pos * slid->y_unit_px);
    area.g_w = rect->x1 - rect->x0;
    area.g_h = rect->y1 - rect->y0;
    //dbg_grect("update box", &area);
    window_schedule_redraw_grect(gw->root, &area);
}

bool gui_window_get_scroll(struct gui_window *w, int *sx, int *sy)
{
    int x,y;
    if (w == NULL)
        return false;

    window_get_scroll(w->root, sx, sy);

    return( true );
}

static void gui_window_set_scroll(struct gui_window *w, int sx, int sy)
{
    int units = 0;
    if ((w == NULL)
            || (w->browser->bw == NULL)
            || (w->browser->bw->current_content == NULL))
        return;

    LOG(("scroll (gui_window: %p) %d, %d\n", w, sx, sy));
    window_scroll_by(w->root, sx, sy);
    return;

}

/* It seems this method is called when content size got adjusted,
	so that we can adjust scroll info. We also have to call it when tab
	change occurs.
*/
static void gui_window_update_extent(struct gui_window *gw)
{

    if( gw->browser->bw->current_content != NULL ) {
        // TODO: store content size!
        if(window_get_active_gui_window(gw->root) == gw) {
            window_set_content_size( gw->root,
                                     content_get_width(gw->browser->bw->current_content),
                                     content_get_height(gw->browser->bw->current_content)
                                   );
            window_update_back_forward(gw->root);
            GRECT area;
            window_get_grect(gw->root, BROWSER_AREA_CONTENT, &area);
            window_schedule_redraw_grect(gw->root, &area);
        }
    }
}


/**
 * set the pointer shape
 */
void gui_window_set_pointer(struct gui_window *gw, gui_pointer_shape shape)
{
    if (gw == NULL)
        return;

    switch (shape) {
    case GUI_POINTER_POINT: /* link */
        gw->cursor = &gem_cursors.hand;
        break;

    case GUI_POINTER_MENU:
        gw->cursor = &gem_cursors.menu;
        break;

    case GUI_POINTER_CARET: /* input */
        gw->cursor = &gem_cursors.ibeam;
        break;

    case GUI_POINTER_CROSS:
        gw->cursor = &gem_cursors.cross;
        break;

    case GUI_POINTER_MOVE:
        gw->cursor = &gem_cursors.sizeall;
        break;

    case GUI_POINTER_RIGHT:
    case GUI_POINTER_LEFT:
        gw->cursor = &gem_cursors.sizewe;
        break;

    case GUI_POINTER_UP:
    case GUI_POINTER_DOWN:
        gw->cursor = &gem_cursors.sizens;
        break;

    case GUI_POINTER_RU:
    case GUI_POINTER_LD:
        gw->cursor = &gem_cursors.sizenesw;
        break;

    case GUI_POINTER_RD:
    case GUI_POINTER_LU:
        gw->cursor = &gem_cursors.sizenwse;
        break;

    case GUI_POINTER_WAIT:
        gw->cursor = &gem_cursors.wait;
        break;

    case GUI_POINTER_PROGRESS:
        gw->cursor = &gem_cursors.appstarting;
        break;

    case GUI_POINTER_NO_DROP:
        gw->cursor = &gem_cursors.nodrop;
        break;

    case GUI_POINTER_NOT_ALLOWED:
        gw->cursor = &gem_cursors.deny;
        break;

    case GUI_POINTER_HELP:
        gw->cursor = &gem_cursors.help;
        break;

    default:
        gw->cursor = &gem_cursors.arrow;
        break;
    }

    if (input_window == gw) {
        gem_set_cursor(gw->cursor);
    }
}


static void gui_window_set_url(struct gui_window *w, const char *url)
{
    int l;

    if (w == NULL)
        return;

    l = strlen(url)+1;

    if (w->url == NULL) {
        w->url = malloc(l);
    } else {
        w->url = realloc(w->url, l);
    }
    strncpy(w->url, url, l);
    w->url[l] = 0;
    if(input_window == w->root->active_gui_window) {
        toolbar_set_url(w->root->toolbar, url);
    }
}

char * gui_window_get_url(struct gui_window *gw)
{
	if (gw == NULL) {
		return(NULL);
	}
	return(gw->url);
}

char * gui_window_get_title(struct gui_window *gw)
{
	if (gw == NULL) {
		return(NULL);
	}
	return(gw->title);
}

static void throbber_advance( void * data )
{

    struct gui_window * gw = (struct gui_window *)data;

    if (gw->root == NULL)
        return;
    if (gw->root->toolbar == NULL)
        return;

    if (gw->root->toolbar->throbber.running == false)
        return;

    toolbar_throbber_progress(gw->root->toolbar);
    atari_schedule(1000, throbber_advance, gw );
}

static void gui_window_start_throbber(struct gui_window *w)
{
    GRECT work;
    if (w == NULL)
        return;

    toolbar_set_throbber_state(w->root->toolbar, true);
    atari_schedule(1000, throbber_advance, w );
    rendering = true;
}

static void gui_window_stop_throbber(struct gui_window *w)
{
    if (w == NULL)
        return;
    if (w->root->toolbar->throbber.running == false)
        return;

    atari_schedule(-1, throbber_advance, w);

    toolbar_set_throbber_state(w->root->toolbar, false);

    rendering = false;
}

/* Place caret in window */
static void gui_window_place_caret(struct gui_window *w, int x, int y, int height,
		const struct rect *clip)
{
    window_place_caret(w->root, 1, x, y, height, NULL);
    w->root->caret.state |= CARET_STATE_ENABLED;
    return;
}


/**
 * clear window caret
 */
static void
gui_window_remove_caret(struct gui_window *w)
{
    if (w == NULL)
        return;

    if ((w->root->caret.state & CARET_STATE_ENABLED) != 0) {
        //printf("gw hide caret\n");
        window_place_caret(w->root, 0, -1, -1, -1, NULL);
        w->root->caret.state &= ~CARET_STATE_ENABLED;
    }
    return;
}

static void
gui_window_set_icon(struct gui_window *g, hlcache_handle *icon)
{
    struct bitmap *bmp_icon;

    bmp_icon = (icon != NULL) ? content_get_bitmap(icon) : NULL;
    g->icon = bmp_icon;
    if(input_window == g) {
        window_set_icon(g->root, bmp_icon);
    }
}

static void gui_window_new_content(struct gui_window *w)
{
    struct gemtk_wm_scroll_info_s *slid = gemtk_wm_get_scroll_info(w->root->win);
    slid->x_pos = 0;
    slid->y_pos = 0;
    gemtk_wm_update_slider(w->root->win, GEMTK_WM_VH_SLIDER);
    gui_window_redraw_window(w);
}


/**
 * Core asks front end for clipboard contents.
 *
 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
 * \param  length  Byte length of UTF-8 text in buffer
 */
static void gui_get_clipboard(char **buffer, size_t *length)
{
    char *clip;
    size_t clip_len;

    *length = 0;
    *buffer = 0;

    clip = scrap_txt_read();

    if(clip == NULL) {
        return;
    } else {

        // clipboard is in atari encoding, convert it to utf8:

        char *utf8 = NULL;
        nserror ret;

        clip_len = strlen(clip);
        if (clip_len > 0) {
            ret = utf8_to_local_encoding(clip, clip_len, &utf8);
            if (ret == NSERROR_OK && utf8 != NULL) {
                *buffer = utf8;
                *length = strlen(utf8);
            } else {
                assert(ret == NSERROR_OK && utf8 != NULL);
            }
        }

        free(clip);
    }
}

/**
 * Core tells front end to put given text in clipboard
 *
 * \param  buffer    UTF-8 text, owned by core
 * \param  length    Byte length of UTF-8 text in buffer
 * \param  styles    Array of styles given to text runs, owned by core, or NULL
 * \param  n_styles  Number of text run styles in array
 */
static void gui_set_clipboard(const char *buffer, size_t length,
                       nsclipboard_styles styles[], int n_styles)
{
    if (length > 0 && buffer != NULL) {

        // convert utf8 input to atari encoding:

        nserror ret;
        char *clip = NULL;

        ret = utf8_to_local_encoding(buffer,length, &clip);
        if (ret == NSERROR_OK) {
            scrap_txt_write(clip);
        } else {
            assert(ret == NSERROR_OK);
        }
        free(clip);
    }
}

static void gui_401login_open(nsurl *url, const char *realm,
                       nserror (*cb)(bool proceed, void *pw), void *cbpw)
{
    bool bres;
    char * out = NULL;
    bres = login_form_do( url, (char*)realm, &out);
    if (bres) {
        LOG(("url: %s, realm: %s, auth: %s\n", url, realm, out ));
        urldb_set_auth_details(url, realm, out);
    }
    if (out != NULL) {
        free( out );
    }
    if (cb != NULL) {
        cb(bres, cbpw);
    }

}

static void gui_cert_verify(nsurl *url, const struct ssl_cert_info *certs,
                    unsigned long num, nserror (*cb)(bool proceed, void *pw),
					void *cbpw)
{
	struct sslcert_session_data *data;
    LOG((""));

    bool bres;

    // TODO: localize string
    int b = form_alert(1, "[2][SSL Verify failed, continue?][Continue|Abort|Details...]");
    if(b == 1){
		// Accept
		urldb_set_cert_permissions(url, true);
		cb(true, cbpw);
    } else if(b == 2) {
    	// Reject
		urldb_set_cert_permissions(url, false);
		cb(false, cbpw);
    } else if(b == 3) {
    	// Inspect
    	sslcert_viewer_create_session_data(num, url, cb, cbpw, certs,
			&data);
		atari_sslcert_viewer_open(data);
    }

}

void gui_set_input_gui_window(struct gui_window *gw)
{
    LOG(("Setting input window from: %p to %p\n", input_window, gw));
    input_window = gw;
}

struct gui_window * gui_get_input_window(void)
{
	return(input_window);
}

static void gui_quit(void)
{
    LOG((""));

    struct gui_window * gw = window_list;
    struct gui_window * tmp = window_list;

	/* Destroy all remaining browser windows: */
    while (gw) {
        tmp = gw->next;
        browser_window_destroy(gw->browser->bw);
        gw = tmp;
    }

	/* destroy the treeview windows: */
    atari_global_history_destroy();
    atari_hotlist_destroy();
    atari_cookie_manager_destroy();

	/* shutdown netsurf treeview framework: */
    treeview_fini();

	/* shutdown the toolbar framework: */
    toolbar_exit();

	/* save persistent informations: */
    urldb_save_cookies(nsoption_charp(cookie_file));
    urldb_save(nsoption_charp(url_file));

    deskmenu_destroy();
    gemtk_wm_exit();

    rsrc_free();

    LOG(("Shutting down plotter"));
    plot_finalise();
    LOG(("done"));
}




static bool
process_cmdline(int argc, char** argv)
{
    int opt;
    bool set_default_dimensions = true;

    LOG(("argc %d, argv %p", argc, argv));

    if ((nsoption_int(window_width) != 0) && (nsoption_int(window_height) != 0)) {

        option_window_width = nsoption_int(window_width);
        option_window_height = nsoption_int(window_height);
        option_window_x = nsoption_int(window_x);
        option_window_y = nsoption_int(window_y);

        if (option_window_width <= desk_area.g_w
                && option_window_height < desk_area.g_h) {
            set_default_dimensions = false;
        }
    }

    if (set_default_dimensions) {
        if( sys_type() == SYS_TOS ) {
            /* on single tasking OS, start as fulled window: */
            option_window_width = desk_area.g_w;
            option_window_height = desk_area.g_h;
            option_window_x = desk_area.g_w/2-(option_window_width/2);
            option_window_y = (desk_area.g_h/2)-(option_window_height/2);
        } else {
            option_window_width = 600;
            option_window_height = 360;
            option_window_x = 10;
            option_window_y = 30;
        }
    }

    if (nsoption_charp(homepage_url) != NULL)
        option_homepage_url = nsoption_charp(homepage_url);
    else
        option_homepage_url = NETSURF_HOMEPAGE;

    while((opt = getopt(argc, argv, "w:h:")) != -1) {
        switch (opt) {
        case 'w':
            option_window_width = atoi(optarg);
            break;

        case 'h':
            option_window_height = atoi(optarg);
            break;

        default:
            fprintf(stderr,
                    "Usage: %s [w,h,v] url\n",
                    argv[0]);
            return false;
        }
    }

    if (optind < argc) {
        option_homepage_url = argv[optind];
    }
    return true;
}

static inline void create_cursor(int flags, short mode, void * form,
                                 MFORM_EX * m)
{
    m->flags = flags;
    m->number = mode;
    if( flags & MFORM_EX_FLAG_USERFORM ) {
        m->number = mode;
        m->tree = (OBJECT*)form;
    }
}

static nsurl *gui_get_resource_url(const char *path)
{
    char buf[PATH_MAX];
    char *raw;
    nsurl *url = NULL;

    atari_find_resource((char*)&buf, path, path);
    raw = path_to_url((char*)&buf);
    if (raw != NULL) {
        nsurl_create(raw, &url);
        free(raw);
    }

    return url;
}

/**
 * Set option defaults for atari frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
    /* Set defaults for absent option strings */
    nsoption_setnull_charp(cookie_file, strdup("cookies"));
    if (nsoption_charp(cookie_file) == NULL) {
        LOG(("Failed initialising string options"));
	return NSERROR_BAD_PARAMETER;
    }
    return NSERROR_OK;
}

static void gui_init(int argc, char** argv)
{
    char buf[PATH_MAX];
    OBJECT * cursors;

    atari_find_resource(buf, "netsurf.rsc", "./res/netsurf.rsc");
    LOG(("Using RSC file: %s ", (char*)&buf));
    if (rsrc_load(buf)==0) {

		char msg[1024];

		snprintf(msg, 1024, "Unable to open GEM Resource file (%s)!", buf);
        die(msg);
    }

    wind_get_grect(0, WF_WORKXYWH, &desk_area);

    create_cursor(0, POINT_HAND, NULL, &gem_cursors.hand );
    create_cursor(0, TEXT_CRSR,  NULL, &gem_cursors.ibeam );
    create_cursor(0, THIN_CROSS, NULL, &gem_cursors.cross);
    create_cursor(0, BUSY_BEE, NULL, &gem_cursors.wait);
    create_cursor(0, ARROW, NULL, &gem_cursors.arrow);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizeall);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenesw);
    create_cursor(0, OUTLN_CROSS, NULL, &gem_cursors.sizenwse);
    cursors = gemtk_obj_get_tree(CURSOR);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_APPSTART,
                  cursors, &gem_cursors.appstarting);
    gem_set_cursor( &gem_cursors.appstarting );
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_SIZEWE,
                  cursors, &gem_cursors.sizewe);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_SIZENS,
                  cursors, &gem_cursors.sizens);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_NODROP,
                  cursors, &gem_cursors.nodrop);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_DENY,
                  cursors, &gem_cursors.deny);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_MENU,
                  cursors, &gem_cursors.menu);
    create_cursor(MFORM_EX_FLAG_USERFORM, CURSOR_HELP,
                  cursors, &gem_cursors.help);

    LOG(("Enabling core select menu"));
    nsoption_set_bool(core_select_menu, true);

    LOG(("Loading url.db from: %s", nsoption_charp(url_file) ));
    if( strlen(nsoption_charp(url_file)) ) {
        urldb_load(nsoption_charp(url_file));
    }

    LOG(("Loading cookies from: %s", nsoption_charp(cookie_file) ));
    if( strlen(nsoption_charp(cookie_file)) ) {
        urldb_load_cookies(nsoption_charp(cookie_file));
    }

    if (process_cmdline(argc,argv) != true)
        die("unable to process command line.\n");

    LOG(("Initializing NKC..."));
    nkc_init();


    LOG(("Initializing plotters..."));
    plot_init(nsoption_charp(atari_font_driver));

    aes_event_in.emi_m1leave = MO_LEAVE;
    aes_event_in.emi_m1.g_w = 1;
    aes_event_in.emi_m1.g_h = 1;
    //next_poll = clock() + (CLOCKS_PER_SEC>>3);
}

static char *theapp = (char*)"NetSurf";
static void gui_init2(int argc, char** argv)
{
    deskmenu_init();
    menu_register( -1, theapp);
    if (sys_type() & (SYS_MAGIC|SYS_NAES|SYS_XAAES)) {
        menu_register( _AESapid, (char*)"  NetSurf ");
    }
    gemtk_wm_init();

    /* Initialize the netsurf treeview framework with default font size: */
    treeview_init(0);

	/* Initialize the specific treeview windows: */
    atari_global_history_init();
    atari_hotlist_init();
    atari_cookie_manager_init();

    /* Initialize the toolbar framework: */
    toolbar_init();
}

static struct gui_window_table atari_window_table = {
    .create = gui_window_create,
    .destroy = gui_window_destroy,
    .redraw = gui_window_redraw_window,
    .update = gui_window_update_box,
    .get_scroll = gui_window_get_scroll,
    .set_scroll = gui_window_set_scroll,
    .get_dimensions = gui_window_get_dimensions,
    .update_extent = gui_window_update_extent,

    .set_title = gui_window_set_title,
    .set_url = gui_window_set_url,
    .set_icon = gui_window_set_icon,
    .set_status = gui_window_set_status,
    .set_pointer = gui_window_set_pointer,
    .place_caret = gui_window_place_caret,
    .remove_caret = gui_window_remove_caret,
    .new_content = gui_window_new_content,
    .start_throbber = gui_window_start_throbber,
    .stop_throbber = gui_window_stop_throbber,
};

static struct gui_clipboard_table atari_clipboard_table = {
    .get = gui_get_clipboard,
    .set = gui_set_clipboard,
};

static struct gui_fetch_table atari_fetch_table = {
    .filename_from_path = filename_from_path,
    .path_add_part = path_add_part,
    .filetype = fetch_filetype,
    .path_to_url = path_to_url,
    .url_to_path = url_to_path,

    .get_resource_url = gui_get_resource_url,
    .mimetype = fetch_mimetype,
};

static struct gui_browser_table atari_browser_table = {
    .poll = gui_poll,
    .schedule = atari_schedule,

    .quit = gui_quit,
    .cert_verify = gui_cert_verify,
    .login = gui_401login_open,
};

/* #define WITH_DBG_LOGFILE 1 */
/** Entry point from OS.
 *
 * /param argc The number of arguments in the string vector.
 * /param argv The argument string vector.
 * /return The return code to the OS
 */
int main(int argc, char** argv)
{
    char messages[PATH_MAX];
    const char *addr;
    char * file_url = NULL;
    struct stat stat_buf;
    nsurl *url;
    nserror ret;
    struct gui_table atari_gui_table = {
        .browser = &atari_browser_table,
	.window = &atari_window_table,
	.clipboard = &atari_clipboard_table,
	.download = atari_download_table,
	.fetch = &atari_fetch_table,
	.utf8 = atari_utf8_table,
	.search = atari_search_table,
    };

    /* @todo logging file descriptor update belongs in a nslog_init callback */
    setbuf(stderr, NULL);
    setbuf(stdout, NULL);
#ifdef WITH_DBG_LOGFILE
    freopen("stdout.log", "a+", stdout);
    freopen("stderr.log", "a+", stderr);
#endif

    graf_mouse(BUSY_BEE, NULL);

    init_app(NULL);

    init_os_info();

    atari_find_resource((char*)&messages, "messages", "res/messages");
    atari_find_resource((char*)&options, "Choices", "Choices");

    /* initialise logging - not fatal if it fails but not much we can
     * do about it
     */
    nslog_init(NULL, &argc, argv);

    /* user options setup */
    ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
    if (ret != NSERROR_OK) {
		die("Options failed to initialise");
    }
    nsoption_read(options, NULL);
    nsoption_commandline(&argc, argv, NULL);

    /* common initialisation */
    LOG(("Initialising core..."));
    ret = netsurf_init(messages, &atari_gui_table);
    if (ret != NSERROR_OK) {
	die("NetSurf failed to initialise");
    }

    LOG(("Initializing GUI..."));
    gui_init(argc, argv);

    LOG(("Initializing GUI2"));
    gui_init2(argc, argv);

    graf_mouse( ARROW , NULL);

    LOG(("Creating initial browser window..."));
    addr = option_homepage_url;
    if (strncmp(addr, "file://", 7) && strncmp(addr, "http://", 7)) {
		if (stat(addr, &stat_buf) == 0) {
			file_url = local_file_to_url(addr);
			addr = file_url;
		}
    }

    /* create an initial browser window */
    ret = nsurl_create(addr, &url);
    if (ret == NSERROR_OK) {
	ret = browser_window_create(BW_CREATE_HISTORY,
				    url,
				    NULL,
				    NULL,
				    NULL);
	nsurl_unref(url);
    }
    if (ret != NSERROR_OK) {
	warn_user(messages_get_errorcode(ret), 0);
    } else {
	LOG(("Entering NetSurf mainloop..."));
	netsurf_main_loop();
    }

    netsurf_exit();

    free(file_url);

#ifdef WITH_DBG_LOGFILE
    fclose(stdout);
    fclose(stderr);
#endif
	LOG(("exit_gem"));
    exit_gem();

    return 0;
}

