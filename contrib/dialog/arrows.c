/*
 *  $Id: arrows.c,v 1.29 2010/02/24 09:17:00 Samuel.Martin.Moro Exp $
 *
 *  arrows.c -- draw arrows to indicate end-of-range for lists
 *
 * Copyright 2000-2009,2010   Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 */

#include <dialog.h>

#ifdef USE_WIDE_CURSES
#define add_acs(win, code) wadd_wch(win, W ## code)
#else
#define add_acs(win, code) waddch(win, dlg_boxchar(code))
#endif

#ifdef HAVE_COLOR
static chtype
merge_colors(chtype foreground, chtype background)
{
    chtype result = foreground;
    if ((foreground & A_COLOR) != (background & A_COLOR)) {
	short fg_f, bg_f;
	short fg_b, bg_b;
	short fg_pair = (short) PAIR_NUMBER(foreground);
	short bg_pair = (short) PAIR_NUMBER(background);

	if (pair_content(fg_pair, &fg_f, &bg_f) != ERR
	    && pair_content(bg_pair, &fg_b, &bg_b) != ERR) {
	    result &= ~A_COLOR;
	    result |= dlg_color_pair(fg_f, bg_b);
	}
    }
    return result;
}
#else
#define merge_colors(f,b) (f)
#endif

void
dlg_draw_arrows2(WINDOW *win,
		 int top_arrow,
		 int bottom_arrow,
		 int x,
		 int top,
		 int bottom,
		 chtype attr,
		 chtype borderattr)
{
    chtype save = getattrs(win);
    int cur_x, cur_y;
    int limit_x = getmaxx(win);
    bool draw_top = TRUE;

    getyx(win, cur_y, cur_x);

    /*
     * If we're drawing a centered title, do not overwrite with the arrows.
     */
    if (dialog_vars.title) {
	int have = (limit_x - dlg_count_columns(dialog_vars.title)) / 2;
	int need = x + 5;
	if (need > have)
	    draw_top = FALSE;
    }

    if (draw_top) {
	(void) wmove(win, top, x);
	if (top_arrow) {
	    wattrset(win, merge_colors(uarrow_attr, attr));
	    (void) add_acs(win, ACS_UARROW);
	    (void) waddstr(win, "(-)");
	} else {
	    wattrset(win, attr);
	    (void) whline(win, dlg_boxchar(ACS_HLINE), 4);
	}
    }
    mouse_mkbutton(top, x - 1, 6, KEY_PPAGE);

    (void) wmove(win, bottom, x);
    if (bottom_arrow) {
	wattrset(win, merge_colors(darrow_attr, attr));
	(void) add_acs(win, ACS_DARROW);
	(void) waddstr(win, "(+)");
    } else {
	wattrset(win, borderattr);
	(void) whline(win, dlg_boxchar(ACS_HLINE), 4);
    }
    mouse_mkbutton(bottom, x - 1, 6, KEY_NPAGE);

    (void) wmove(win, cur_y, cur_x);
    wrefresh(win);

    wattrset(win, save);
}

void
dlg_draw_scrollbar(WINDOW *win,
		   long first_data,
		   long this_data,
		   long next_data,
		   long total_data,
		   int left,
		   int right,
		   int top,
		   int bottom,
		   chtype attr,
		   chtype borderattr)
{
    char buffer[80];
    int percent;
    int len;
    int oldy, oldx, maxy, maxx;

    chtype save = getattrs(win);
    int top_arrow = (first_data != 0);
    int bottom_arrow = (next_data < total_data);

    getyx(win, oldy, oldx);
    getmaxyx(win, maxy, maxx);

    if (bottom_arrow || top_arrow || dialog_state.use_scrollbar) {
	percent = (!total_data
		   ? 100
		   : (int) ((next_data * 100)
			    / total_data));

	if (percent < 0)
	    percent = 0;
	else if (percent > 100)
	    percent = 100;

	wattrset(win, position_indicator_attr);
	(void) sprintf(buffer, "%d%%", percent);
	(void) wmove(win, bottom, right - 7);
	(void) waddstr(win, buffer);
	if ((len = dlg_count_columns(buffer)) < 4) {
	    wattrset(win, border_attr);
	    whline(win, dlg_boxchar(ACS_HLINE), 4 - len);
	}
    }
#define BARSIZE(num) ((all_high * (num)) + all_high - 1) / total_data

    if (dialog_state.use_scrollbar) {
	int all_high = (bottom - top - 1);

	if (total_data > 0 && all_high > 0) {
	    int bar_high;
	    int bar_y;

	    bar_high = BARSIZE(next_data - this_data);
	    if (bar_high <= 0)
		bar_high = 1;

	    if (bar_high < all_high) {
		wmove(win, top + 1, right);

		wattrset(win, save);
		wvline(win, ACS_VLINE | A_REVERSE, all_high);

		bar_y = BARSIZE(this_data);
		if (bar_y > all_high - bar_high)
		    bar_y = all_high - bar_high;

		wmove(win, top + 1 + bar_y, right);

		wattrset(win, position_indicator_attr);
		wattron(win, A_REVERSE);
		wvline(win, ACS_BLOCK, bar_high);
	    }
	}
    }
    dlg_draw_arrows2(win,
		     top_arrow,
		     bottom_arrow,
		     left + ARROWS_COL,
		     top,
		     bottom,
		     attr,
		     borderattr);

    wattrset(win, save);
    wmove(win, oldy, oldx);
}

void
dlg_draw_arrows(WINDOW *win,
		int top_arrow,
		int bottom_arrow,
		int x,
		int top,
		int bottom)
{
    dlg_draw_arrows2(win,
		     top_arrow,
		     bottom_arrow,
		     x,
		     top,
		     bottom,
		     menubox_attr,
		     menubox_border_attr);
}
