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

#ifndef NS_ATARI_TOOLBAR_H
#define NS_ATARI_TOOLBAR_H

#include <stdbool.h>
#include <stdint.h>

#include "desktop/textarea.h"
#include "desktop/browser.h"

struct s_toolbar;

enum toolbar_textarea {
    URL_INPUT_TEXT_AREA = 1
};

struct s_url_widget
{
	struct textarea *textarea;
	GRECT area;
};

struct s_throbber_widget
{
	short index;
	short max_index;
	bool running;
};

struct s_toolbar
{
	struct s_gui_win_root *owner;
	struct s_url_widget url;
	struct s_throbber_widget throbber;
	OBJECT *form;
	GRECT area;
	/* size & location of buttons: */
	struct s_tb_button * buttons;
	int btcnt;
	int style;
	bool attached;
    bool reflow;
    bool visible;
    bool search_visible;
};


void toolbar_init(void);
struct s_toolbar *toolbar_create(struct s_gui_win_root *owner);
void toolbar_destroy(struct s_toolbar * tb);
void toolbar_exit( void );
bool toolbar_text_input(struct s_toolbar *tb, char *text);
bool toolbar_key_input(struct s_toolbar *tb, short nkc);
void toolbar_mouse_input(struct s_toolbar *tb, short obj, short mbut);
void toolbar_update_buttons(struct s_toolbar *tb, struct browser_window *bw,
                            short idx);
void toolbar_get_grect(struct s_toolbar *tb, short which, GRECT *g);
OBJECT *toolbar_get_form(struct s_toolbar *tb);
struct textarea *toolbar_get_textarea(struct s_toolbar *tb,
                                       enum toolbar_textarea which);
char *toolbar_get_url(struct s_toolbar *tb);
nsurl * toolbar_get_nsurl(struct s_toolbar * tb);
void toolbar_set_throbber_state(struct s_toolbar *tb, bool active);
void toolbar_set_attached(struct s_toolbar *tb, bool attached);
void toolbar_set_visible(struct s_toolbar *tb, short area, bool visible);
void toolbar_set_reflow(struct s_toolbar *tb, bool do_reflow);
void toolbar_set_width(struct s_toolbar *tb, short w);
void toolbar_set_origin(struct s_toolbar *tb, short x, short y);
void toolbar_set_dimensions(struct s_toolbar *tb, GRECT *area);
void toolbar_set_url(struct s_toolbar *tb, const char *text);
void toolbar_redraw(struct s_toolbar *tb, GRECT *clip);
void toolbar_throbber_progress(struct s_toolbar *tb);
/* public events handlers: */
void toolbar_back_click(struct s_toolbar *tb);
void toolbar_reload_click(struct s_toolbar *tb);
void toolbar_forward_click(struct s_toolbar *tb);
void toolbar_home_click(struct s_toolbar *tb);
void toolbar_stop_click(struct s_toolbar *tb);
void toolbar_favorite_click(struct s_toolbar *tb);
void toolbar_crypto_click(struct s_toolbar *tb);


#endif
