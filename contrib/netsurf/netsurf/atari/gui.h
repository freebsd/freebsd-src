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

#ifndef NS_ATARI_GUI_H_
#define NS_ATARI_GUI_H_

#include <stdbool.h>
#include <mt_gem.h>

#include "atari/search.h"
#include "atari/redrawslots.h"
#include "atari/gemtk/gemtk.h"

#define CARET_STATE_VISIBLE		0x01
#define CARET_STATE_ENABLED		0x02

struct s_caret {
	GRECT dimensions;
	MFDB symbol;
	int fd_size;
	unsigned short state;
};

struct point_s {
	int x;
	int y;
};

typedef struct point_s POINT;

#define MFORM_EX_FLAG_USERFORM 0x01

struct mform_ex_s
{
	unsigned char flags;
	int number;
	OBJECT * tree;
};

typedef struct mform_ex_s MFORM_EX;

struct s_gem_cursors {
	MFORM_EX hand;
	MFORM_EX ibeam;
	MFORM_EX cross;
	MFORM_EX sizeall;
	MFORM_EX sizewe;
	MFORM_EX sizens;
	MFORM_EX sizenesw;
	MFORM_EX sizenwse;
	MFORM_EX wait;
	MFORM_EX appstarting;
	MFORM_EX nodrop;
	MFORM_EX deny;
	MFORM_EX help;
	MFORM_EX menu;
	MFORM_EX arrow;
} gem_cursors;

enum focus_element_type {
	WIDGET_NONE=0,
	URL_WIDGET,
	SEARCH_INPUT,
	BROWSER
};


struct s_focus_info
{
	enum focus_element_type type;
	void * element;
};

/* defines for data attached to components: */
#define CDT_OBJECT	            0x004f424aUL
#define CDT_OWNER 	            0x03UL
#define CDT_ICON 	            0x04UL
#define CDT_ICON_TYPE	        0x05UL
#	define CDT_ICON_TYPE_NONE      0x00UL
#	define CDT_ICON_TYPE_OBJECT    0x01UL
#	define CDT_ICON_TYPE_BITMAP    0x02UL


struct gui_window;
struct s_browser;
struct s_statusbar;
struct s_toolbar;

typedef struct s_toolbar * CMP_TOOLBAR;
typedef struct s_statusbar * CMP_STATUSBAR;
typedef struct s_browser * CMP_BROWSER;

/*
	This is the "main" window. It can consist of several components
	and also holds information shared by several frames within
	the window.
*/
struct s_gui_win_root
{
	short aes_handle;
	GUIWIN *win;
	CMP_TOOLBAR toolbar;
	CMP_STATUSBAR statusbar;
	struct s_focus_info focus;
	float scale;
	char * title;
	struct bitmap * icon;
	struct gui_window *active_gui_window;
	struct s_redrw_slots redraw_slots;
	struct s_caret caret;
	/* current size of window on screen: */
	GRECT loc;
};
typedef struct s_gui_win_root ROOTWIN;

struct s_browser
{
	struct browser_window * bw;
	bool attached;
};

/*
	This is the part of the gui which is known by netsurf core.
	You must implement it. Altough, you are free how to do it.
	Each of the browser "viewports" managed by netsurf are bound
	to this structure.
*/
struct gui_window {
	struct s_gui_win_root * root;
	struct s_browser * browser;
	MFORM_EX *cursor;
    /* icon to be drawn when iconified, or NULL for default resource. */
    char * status;
    char * title;
    char * url;
	struct bitmap * icon;
	float scale;
	struct s_caret caret;
	struct s_search_form_session *search;
	struct gui_window *next, *prev;
};

extern struct gui_window *window_list;

/* -------------------------------------------------------------------------- */
/* Public - non core gui window functions     		                          */
/* -------------------------------------------------------------------------- */
void gui_set_input_gui_window(struct gui_window *gw);
struct gui_window *gui_get_input_window(void);
char *gui_window_get_url(struct gui_window *gw);
char *gui_window_get_title(struct gui_window *gw);

void gui_window_set_status(struct gui_window *w, const char *text);
void gui_window_set_pointer(struct gui_window *gw, gui_pointer_shape shape);
void gui_window_destroy(struct gui_window *w);
void gui_window_set_scale(struct gui_window *gw, float scale);
float gui_window_get_scale(struct gui_window *gw);

#endif
