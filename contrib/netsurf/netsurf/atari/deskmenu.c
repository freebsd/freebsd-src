#include <stdlib.h>
#include <cflib.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/url.h"
#include "desktop/browser.h"
#include "desktop/browser_private.h"
#include "utils/nsoption.h"
#include "desktop/save_complete.h"
#include "atari/res/netsurf.rsh"
#include "atari/gemtk/gemtk.h"
#include "atari/deskmenu.h"
#include "atari/hotlist.h"
#include "atari/history.h"
#include "atari/cookies.h"
#include "atari/toolbar.h"
#include "atari/settings.h"
#include "atari/search.h"
#include "atari/misc.h"
#include "atari/gui.h"
#include "atari/findfile.h"
#include "atari/about.h"
#include "atari/plot/plot.h"

#include "atari/rootwin.h"

typedef void __CDECL (*menu_evnt_func)(short item, short title, void * data);

struct s_accelerator
{
	char ascii;	/* either ascii or */
	long keycode; /* normalised keycode is valid  */
	short mod;  /* shift / ctrl etc */
};

struct s_menu_item_evnt {
	short title; /* to which menu this item belongs */
	short rid; /* resource ID */
	menu_evnt_func menu_func; /* click handler */
	struct s_accelerator accel; /* accelerator info */
	char * menustr;
};

static void register_menu_str(struct s_menu_item_evnt * mi);
//static void __CDECL evnt_menu(WINDOW * win, short buff[8]);

extern void *h_gem_rsrc;
extern bool html_redraw_debug;
extern struct gui_window * input_window;
extern char options[PATH_MAX];
extern const char * option_homepage_url;
extern int option_window_width;
extern int option_window_height;
extern int option_window_x;
extern int option_window_y;

static OBJECT * h_gem_menu;


/* Zero based resource tree ids: */
#define T_ABOUT 0
#define T_FILE MAINMENU_T_FILE - MAINMENU_T_FILE + 1
#define T_EDIT MAINMENU_T_EDIT - MAINMENU_T_FILE + 1
#define T_VIEW MAINMENU_T_VIEW - MAINMENU_T_FILE + 1
#define T_NAV	MAINMENU_T_NAVIGATE - MAINMENU_T_FILE + 1
#define T_UTIL MAINMENU_T_UTIL - MAINMENU_T_FILE + 1
#define T_HELP MAINMENU_T_NAVIGATE - MAINMENU_T_FILE + 1
/* Count of the above defines: */
#define NUM_MENU_TITLES 7


static void __CDECL menu_about(short item, short title, void *data);
static void __CDECL menu_new_win(short item, short title, void *data);
static void __CDECL menu_open_url(short item, short title, void *data);
static void __CDECL menu_open_file(short item, short title, void *data);
static void __CDECL menu_close_win(short item, short title, void *data);
static void __CDECL menu_save_page(short item, short title, void *data);
static void __CDECL menu_quit(short item, short title, void *data);
static void __CDECL menu_cut(short item, short title, void *data);
static void __CDECL menu_copy(short item, short title, void *data);
static void __CDECL menu_paste(short item, short title, void *data);
static void __CDECL menu_find(short item, short title, void *data);
static void __CDECL menu_choices(short item, short title, void *data);
static void __CDECL menu_stop(short item, short title, void *data);
static void __CDECL menu_reload(short item, short title, void *data);
static void __CDECL menu_dec_scale(short item, short title, void *data);
static void __CDECL menu_inc_scale(short item, short title, void *data);
static void __CDECL menu_toolbars(short item, short title, void *data);
static void __CDECL menu_savewin(short item, short title, void *data);
static void __CDECL menu_debug_render(short item, short title, void *data);
static void __CDECL menu_fg_images(short item, short title, void *data);
static void __CDECL menu_bg_images(short item, short title, void *data);
static void __CDECL menu_back(short item, short title, void *data);
static void __CDECL menu_forward(short item, short title, void *data);
static void __CDECL menu_home(short item, short title, void *data);
static void __CDECL menu_lhistory(short item, short title, void *data);
static void __CDECL menu_ghistory(short item, short title, void *data);
static void __CDECL menu_add_bookmark(short item, short title, void *data);
static void __CDECL menu_bookmarks(short item, short title, void *data);
static void __CDECL menu_cookies(short item, short title, void *data);
static void __CDECL menu_vlog(short item, short title, void *data);
static void __CDECL menu_help_content(short item, short title, void *data);

struct s_menu_item_evnt menu_evnt_tbl[] =
{
	{T_ABOUT,MAINMENU_M_ABOUT, menu_about, {0,0,0}, NULL },
	{T_FILE, MAINMENU_M_NEWWIN, menu_new_win, {0,0,0}, NULL},
	{T_FILE, MAINMENU_M_OPENURL, menu_open_url, {'G',0,K_CTRL}, NULL},
	{T_FILE, MAINMENU_M_OPENFILE, menu_open_file, {'O',0,K_CTRL}, NULL},
	{T_FILE, MAINMENU_M_CLOSEWIN, menu_close_win, {0,0,0}, NULL},
	{T_FILE, MAINMENU_M_SAVEPAGE, menu_save_page, {0,NK_F3,0}, NULL},
	{T_FILE, MAINMENU_M_QUIT, menu_quit, {'Q',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_CUT, menu_cut, {'X',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_COPY, menu_copy, {'C',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_PASTE, menu_paste, {'V',0,K_CTRL}, NULL},
	{T_EDIT, MAINMENU_M_FIND, menu_find, {0,NK_F4,0}, NULL},
	{T_VIEW, MAINMENU_M_RELOAD, menu_reload, {0,NK_F5,0}, NULL},
	{T_VIEW, MAINMENU_INC_SCALE, menu_inc_scale, {'+',0,K_CTRL}, NULL},
    {T_VIEW, MAINMENU_DEC_SCALE, menu_dec_scale, {'-',0,K_CTRL}, NULL},
	{T_VIEW, MAINMENU_M_TOOLBARS, menu_toolbars, {0,NK_F1,K_CTRL}, NULL},
	{T_VIEW, MAINMENU_M_SAVEWIN, menu_savewin, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_DEBUG_RENDER, menu_debug_render, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_FG_IMAGES, menu_fg_images, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_BG_IMAGES, menu_bg_images, {0,0,0}, NULL},
	{T_VIEW, MAINMENU_M_STOP, menu_stop, {0,NK_ESC,K_ALT}, NULL},
	{T_NAV, MAINMENU_M_BACK, menu_back, {0,NK_LEFT,K_ALT}, NULL},
	{T_NAV, MAINMENU_M_FORWARD, menu_forward, {0,NK_RIGHT,K_ALT}, NULL},
	{T_NAV, MAINMENU_M_HOME, menu_home, {0,0,0}, NULL},
	{T_UTIL, MAINMENU_M_LHISTORY,menu_lhistory, {0,NK_F7,0}, NULL},
	{T_UTIL, MAINMENU_M_GHISTORY, menu_ghistory, {0,NK_F7,K_CTRL}, NULL},
	{T_UTIL, MAINMENU_M_ADD_BOOKMARK, menu_add_bookmark, {'D',0,K_CTRL}, NULL},
	{T_UTIL, MAINMENU_M_BOOKMARKS, menu_bookmarks, {0,NK_F6,0}, NULL},
	{T_UTIL, MAINMENU_M_COOKIES, menu_cookies, {0,0,0}, NULL},
	{T_UTIL, MAINMENU_M_CHOICES, menu_choices, {0,0,0}, NULL},
	{T_UTIL, MAINMENU_M_VLOG, menu_vlog, {'V',0,K_ALT}, NULL},
	{T_HELP, MAINMENU_M_HELP_CONTENT, menu_help_content, {0,NK_F1,0}, NULL},
	{-1, -1, NULL,{0,0,0}, NULL }
};


/*
static void __CDECL evnt_menu(WINDOW * win, short buff[8])
{
	int title = buff[3];
	INT16 x,y;
	char *str;
	struct gui_window * gw = window_list;
	int i=0;

	deskmenu_dispatch_item(buff[3], buff[4]);
}
*/

/*
	Menu item event handlers:
*/

static void __CDECL menu_about(short item, short title, void *data)
{
	/*
	nsurl *url;
	nserror error;
	char buf[PATH_MAX];

	LOG(("%s", __FUNCTION__));
	strcpy((char*)&buf, "file://");
	strncat((char*)&buf, (char*)"./doc/README.TXT",
			PATH_MAX - (strlen("file://")+1) );

	error = nsurl_create(buf, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
	*/
	atari_about_show();
}

static void __CDECL menu_new_win(short item, short title, void *data)
{
	nsurl *url;
	nserror error;
	const char *addr;

	LOG(("%s", __FUNCTION__));

	if (nsoption_charp(homepage_url) != NULL) {
		addr = nsoption_charp(homepage_url);
	} else {
		addr = NETSURF_HOMEPAGE;
	}

	/* create an initial browser window */
	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY,
					      url,
					      NULL,
					      NULL,
					      NULL);
		nsurl_unref(url);

	}
	if (error != NSERROR_OK) {
		warn_user(messages_get_errorcode(error), 0);
	}
}

static void __CDECL menu_open_url(short item, short title, void *data)
{
	struct gui_window * gw;
	struct browser_window * bw ;
	LOG(("%s", __FUNCTION__));

	gw = input_window;
	if( gw == NULL ) {
		browser_window_create(BW_CREATE_HISTORY,
				      NULL,
				      NULL,
				      NULL,
				      &bw);
		gw = bw->window;
	}
	/* Loose focus: */
	window_set_focus(gw->root, WIDGET_NONE, NULL );

	/* trigger on-focus event (select all text): */
	window_set_focus(gw->root, URL_WIDGET, NULL);

	/* delete selection: */
	toolbar_key_input(gw->root->toolbar, NK_DEL);
}

static void __CDECL menu_open_file(short item, short title, void *data)
{

	LOG(("%s", __FUNCTION__));

	const char * filename = file_select(messages_get("OpenFile"), "");
	if( filename != NULL ){
		char * urltxt = local_file_to_url( filename );
		if( urltxt ){
			nsurl *url;
			nserror error;

			error = nsurl_create(urltxt, &url);
			if (error == NSERROR_OK) {
				error = browser_window_create(BW_CREATE_HISTORY,
							      url,
							      NULL,
							      NULL,
							      NULL);
				nsurl_unref(url);

			}
			if (error != NSERROR_OK) {
				warn_user(messages_get_errorcode(error), 0);
			}
			free( urltxt );
		}
	}
}

static void __CDECL menu_close_win(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	gui_window_destroy( input_window );
}

static void __CDECL menu_save_page(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	static bool init = true;
	bool is_folder=false;
	const char * path;

	if( !input_window )
		return;

	if( init ){
		init = false;
		save_complete_init();
	}

	do {
		// TODO: localize string
		path = file_select("Select folder", "");
		if (path)
			is_folder = is_dir(path);
	} while ((is_folder == false) && (path != NULL));

	if( path != NULL ){
		save_complete(input_window->browser->bw->current_content, path, NULL);
	}

}

static void __CDECL menu_quit(short item, short title, void *data)
{
	short buff[8];
	memset( &buff, 0, sizeof(short)*8 );
	LOG(("%s", __FUNCTION__));
	gemtk_wm_send_msg(NULL, AP_TERM, 0, 0, 0, 0);
}

static void __CDECL menu_cut(short item, short title, void *data)
{
	if( input_window != NULL )
		browser_window_key_press( input_window->browser->bw, KEY_CUT_SELECTION);
}

static void __CDECL menu_copy(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window != NULL )
		browser_window_key_press( input_window->browser->bw, KEY_COPY_SELECTION);
}

static void __CDECL menu_paste(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window != NULL )
		browser_window_key_press( input_window->browser->bw, KEY_PASTE);
}

static void __CDECL menu_find(short item, short title, void *data)
{
	static bool visible = false;
	LOG(("%s", __FUNCTION__));
	if (input_window != NULL) {
		if (input_window->search) {
			window_close_search(input_window->root);
		}
		else {
			window_open_search(input_window->root, true);
		}
	}
}

static void __CDECL menu_choices(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	open_settings();
}

static void __CDECL menu_stop(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;

    assert(input_window && input_window->root);
	toolbar_stop_click(input_window->root->toolbar);

}

static void __CDECL menu_reload(short item, short title, void *data)
{
	if(input_window == NULL)
		return;
	toolbar_reload_click(input_window->root->toolbar);
	LOG(("%s", __FUNCTION__));
}


static void __CDECL menu_inc_scale(short item, short title, void *data)
{
	if(input_window == NULL)
		return;

    gui_window_set_scale(input_window, gui_window_get_scale(input_window)+0.25);
}


static void __CDECL menu_dec_scale(short item, short title, void *data)
{
	if(input_window == NULL)
		return;

    gui_window_set_scale(input_window, gui_window_get_scale(input_window)-0.25);
}



static void __CDECL menu_toolbars(short item, short title, void *data)
{
	static int state = 0;
	LOG(("%s", __FUNCTION__));
	if( input_window != null && input_window->root->toolbar != null ){
		state = !state;
		// TODO: implement toolbar hide
		//toolbar_hide(input_window->root->toolbar, state );
	}
}

static void __CDECL menu_savewin(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if (input_window && input_window->browser) {
		GRECT rect;
		wind_get_grect(gemtk_wm_get_handle(input_window->root->win), WF_CURRXYWH,
                 &rect);
		option_window_width = rect.g_w;
		option_window_height = rect.g_h;
		option_window_x = rect.g_x;
		option_window_y = rect.g_y;
		nsoption_set_int(window_width, rect.g_w);
		nsoption_set_int(window_height, rect.g_h);
		nsoption_set_int(window_x, rect.g_x);
		nsoption_set_int(window_y, rect.g_y);
		nsoption_write((const char*)&options, NULL, NULL);
	}

}

static void __CDECL menu_debug_render(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	html_redraw_debug = !html_redraw_debug;
	if( input_window != NULL ) {
		if ( input_window->browser != NULL
			&& input_window->browser->bw != NULL) {
			GRECT rect;
			window_get_grect(input_window->root, BROWSER_AREA_CONTENT, &rect);
			browser_window_reformat(input_window->browser->bw, false,
									rect.g_w, rect.g_h );
			menu_icheck(h_gem_menu, MAINMENU_M_DEBUG_RENDER,
						(html_redraw_debug) ? 1 : 0);
		}
	}
}

static void __CDECL menu_fg_images(short item, short title, void *data)
{
	nsoption_set_bool(foreground_images, !nsoption_bool(foreground_images));
	menu_icheck(h_gem_menu, MAINMENU_M_FG_IMAGES,
				(nsoption_bool(foreground_images)) ? 1 : 0);
}

static void __CDECL menu_bg_images(short item, short title, void *data)
{
	nsoption_set_bool(background_images, !nsoption_bool(background_images));
	menu_icheck(h_gem_menu, MAINMENU_M_BG_IMAGES,
				(nsoption_bool(background_images)) ? 1 : 0);
}

static void __CDECL menu_back(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	toolbar_back_click(input_window->root->toolbar);
}

static void __CDECL menu_forward(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	toolbar_forward_click(input_window->root->toolbar);
}

static void __CDECL menu_home(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
	toolbar_home_click(input_window->root->toolbar);
}

static void __CDECL menu_lhistory(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if( input_window == NULL )
		return;
}

static void __CDECL menu_ghistory(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	atari_global_history_open();
}

static void __CDECL menu_add_bookmark(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	if (input_window) {
		if( input_window->browser->bw->current_content != NULL ){
			atari_hotlist_add_page(
				nsurl_access(hlcache_handle_get_url(input_window->browser->bw->current_content)),
				NULL
			);
		}
	}
}

static void __CDECL menu_bookmarks(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	atari_hotlist_open();
}

static void __CDECL menu_cookies(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	atari_cookie_manager_open();
}

static void __CDECL menu_vlog(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
	verbose_log = !verbose_log;
	menu_icheck(h_gem_menu, MAINMENU_M_VLOG, (verbose_log) ? 1 : 0);
}

static void __CDECL menu_help_content(short item, short title, void *data)
{
	LOG(("%s", __FUNCTION__));
}

/*
	Public deskmenu interface:
*/


/*
	Parse encoded menu key shortcut

	The format is:

	"[" 		-   marks start of the shortcut
	"@,^,<" 	-   If the keyshortcut is only valid
					with modifier keys, one of these characters must directly
					follow the start mark.
					Meaning:
					@ -> Alternate
					^ -> Control
	"#"			-   keycode or ascii character.
					The value is handled as keycode if the character value
					is <= 28 ( Atari chracter table )
					or if it is interpreted as function key string.
					(strings: F1 - F10)

*/
static void register_menu_str( struct s_menu_item_evnt * mi )
{
	assert(h_gem_menu != NULL);

	struct s_accelerator * accel = &mi->accel;
	int i, l=0, x=-1;
	char str[255];
	bool is_std_shortcut = false;

	get_string(h_gem_menu, mi->rid, str);

	i = l = strlen(str);
	while (i > 2) {
	    if ((strncmp("  ", &str[i], 2) == 0) && (strlen(&str[i]) > 2)) {
            // "Standard" Keyboard Shortcut Element found:
            LOG(("Standard Keyboard Shortcut: \"%s\"\n", &str[i]));
            x = i+2;
            is_std_shortcut = true;
            break;
	    }

		if( str[i] == '['){
		    LOG(("Keyboard Shortcut: \"%s\"\n", &str[i]));
		    // "Custom" Keyboard Shortcut Element found (identified by [):
			x = i;
			break;
		}
		i--;
	}

	// Parse keyboard shortcut value:
	if( x > -1 ){

	    if (is_std_shortcut == false) {
	        // create a new menu string to hide the "[" mark:
            mi->menustr = malloc( l+1 );
            strcpy(mi->menustr, str);
            mi->menustr[x]=' ';
            x++;
	    }

        // find & register modifiers:
		if (str[x] == '@') {
			accel->mod = K_ALT;
			if (is_std_shortcut == false) {
			    // only modify menu items when it is malloc'd:
                mi->menustr[x] = 0x07;
			}
			x++;
		}
		else if (str[x] == '^') {
			accel->mod = K_CTRL;
			x++;
		}
		else if (str[x] == 0x01) { // the arrow up chracter (atari-st encoding)
            accel->mod = K_LSHIFT;
            x++;
		}

        // find keycodes / chracters:
		if( str[x] <= 28 ){
			// parse symbol
			unsigned short keycode=0;
			switch( str[x] ){
					case 0x03:
					accel->keycode = NK_RIGHT;
				break;
					case 0x04:
					accel->keycode = NK_LEFT;
				break;
					case 0x1B:
					accel->keycode = NK_ESC;
				break;
					default:
				break;
			}
		} else {
			if(str[x] == 'F' && ( str[x+1] >= '1' && str[x+1] <= '9') ){
				// parse function key
				short fkey = atoi(  &str[x+1] );
				if( (fkey >= 0) && (fkey <= 10) ){
					accel->keycode = NK_F1 - 1 + fkey;
				}
			}
			else if (strncmp(&str[x], "Home", 4) == 0) {
                accel->keycode = NK_CLRHOME;
			}
			else if (strncmp(&str[x], "Undo", 4) == 0) {
                accel->keycode = NK_UNDO;
			}
			else if (strncmp(&str[x], "Help", 4) == 0) {
                accel->keycode = NK_HELP;
			}
			else {
				accel->ascii = str[x];
			}
		}

		LOG(("Registered keyboard shortcut for \"%s\" => mod: %d, "
                "keycode: %d, ascii: %c\n", str, accel->mod, accel->keycode,
                                                accel->ascii));
	}
}

/**
*	Setup & display an desktop menu.
*/

void deskmenu_init(void)
{
	int i;

	h_gem_menu = gemtk_obj_get_tree(MAINMENU);


	/* Install menu: */
	menu_bar(h_gem_menu, MENU_INSTALL);

	/* parse and update menu items:  */
	i = 0;
	while (menu_evnt_tbl[i].rid != -1) {
	    if(menu_evnt_tbl[i].rid > 0 && menu_evnt_tbl[i].title > 0){
            register_menu_str( &menu_evnt_tbl[i] );
            /* Update menu string if not null: */
            if( menu_evnt_tbl[i].menustr != NULL ){
                menu_text(h_gem_menu, menu_evnt_tbl[i].rid,
                            menu_evnt_tbl[i].menustr);
            }
	    }
        i++;
	}
	deskmenu_update();
	/* Redraw menu: */
	menu_bar(h_gem_menu, MENU_UPDATE);
}

/**
* Uninstall the desktop menu
*/
void deskmenu_destroy(void)
{
	int i;

	/* Remove menu from desktop: */
	menu_bar(h_gem_menu, MENU_REMOVE);

	/* Free modified menu titles: */
	i=0;
	while(menu_evnt_tbl[i].rid != -1) {
		if( menu_evnt_tbl[i].menustr != NULL )
			free(menu_evnt_tbl[i].menustr);
		i++;
	}
}

/**
*	Return the deskmenu AES OBJECT tree
*/
OBJECT * deskmenu_get_obj_tree(void)
{
	return(h_gem_menu);
}

/**
*	Handle an menu item event
*/
int deskmenu_dispatch_item(short title, short item)
{
	int i=0;
	int retval = 0;
	OBJECT * menu_root = deskmenu_get_obj_tree();

    menu_tnormal(menu_root, item, 1);
	menu_tnormal(menu_root, title, 1);
	menu_bar(menu_root, 1);

	// legacy code, is this sensible?:
	/*
	while( gw ) {
		window_set_focus( gw, WIDGET_NONE, NULL  );
		gw = gw->next;
	}
	*/


	while (menu_evnt_tbl[i].rid != -1) {
		 if (menu_evnt_tbl[i].rid == item) {
		 	if (menu_evnt_tbl[i].menu_func != NULL) {
				menu_evnt_tbl[i].menu_func(item, title, NULL);
			}
			break;
		}
		i++;
	}

	return(retval);
}

/**
*	Handle an keypress (check for accelerator)
*/
int deskmenu_dispatch_keypress(unsigned short kcode, unsigned short kstate,
						unsigned short nkc)
{
	char sascii;
	bool done = 0;
	int i = 0;

    sascii = gemtk_keybd2ascii(kcode, 0);
    if(sascii >= 'a' && sascii <= 'z'){
        sascii = gemtk_keybd2ascii(kcode, K_LSHIFT);
    }

	/* Iterate through the menu function table: */
	while( menu_evnt_tbl[i].rid != -1 && done == false) {
		if( kstate == menu_evnt_tbl[i].accel.mod
			&& menu_evnt_tbl[i].accel.ascii != 0) {
			if( menu_evnt_tbl[i].accel.ascii == sascii) {
			    if (menu_evnt_tbl[i].title > 0 && menu_evnt_tbl[i].rid > 0) {
                    deskmenu_dispatch_item(menu_evnt_tbl[i].title,
									menu_evnt_tbl[i].rid);
			    }
			    else {
			        /* Keyboard shortcut not displayed within menu: */
                    menu_evnt_tbl[i].menu_func(0, 0, NULL);
			    }
				done = true;
				break;
			}
		} else {
			/* the accel code hides in the keycode: */
			if( menu_evnt_tbl[i].accel.keycode != 0) {
				if( menu_evnt_tbl[i].accel.keycode == (nkc & 0xFF) &&
					kstate == menu_evnt_tbl[i].accel.mod) {
						if (menu_evnt_tbl[i].title > 0 && menu_evnt_tbl[i].rid > 0) {
                            deskmenu_dispatch_item(menu_evnt_tbl[i].title,
                                            menu_evnt_tbl[i].rid);
                        }
                        else {
                            /* Keyboard shortcut not displayed within menu: */
                            menu_evnt_tbl[i].menu_func(0, 0, NULL);
                        }
						done = true;
						break;
				}
			}
		}
		i++;
	}
	return((done==true) ? 1 : 0);
}

/**
*	Refresh the desk menu, reflecting netsurf current state.
*/
void deskmenu_update(void)
{
	menu_icheck(h_gem_menu, MAINMENU_M_DEBUG_RENDER, (html_redraw_debug) ? 1 : 0);
	menu_icheck(h_gem_menu, MAINMENU_M_FG_IMAGES,
				(nsoption_bool(foreground_images)) ? 1 : 0);
	menu_icheck(h_gem_menu, MAINMENU_M_BG_IMAGES,
				(nsoption_bool(background_images)) ? 1 : 0);
    menu_icheck(h_gem_menu, MAINMENU_M_VLOG, ((verbose_log == true) ? 1 : 0));
}

