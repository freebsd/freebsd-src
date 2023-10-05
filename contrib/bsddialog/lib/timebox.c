/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Alfonso Sabato Siciliano
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <curses.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define MINWTIME   14 /* 3 windows and their borders */
#define HBOX        3
#define WBOX        4

struct clock {
	unsigned int max;
	unsigned int value;
	WINDOW *win;
};

static void
drawsquare(struct bsddialog_conf *conf, WINDOW *win, unsigned int value,
    bool focus)
{
	draw_borders(conf, win, LOWERED);
	if (focus) {
		wattron(win, t.dialog.arrowcolor);
		mvwhline(win, 0, 1, conf->ascii_lines ? '^' : ACS_UARROW, 2);
		mvwhline(win, 2, 1, conf->ascii_lines ? 'v' : ACS_DARROW, 2);
		wattroff(win, t.dialog.arrowcolor);
	}

	if (focus)
		wattron(win, t.menu.f_namecolor);
	mvwprintw(win, 1, 1, "%02u", value);
	if (focus)
		wattroff(win, t.menu.f_namecolor);

	wnoutrefresh(win);
}

static int timebox_redraw(struct dialog *d, struct clock *c)
{
	int y, x;

	if (d->built) {
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	if (dialog_size_position(d, HBOX, MINWTIME, NULL) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0)
		return (BSDDIALOG_ERROR);
	if (d->built)
		refresh(); /* Important to fix grey lines expanding screen */
	TEXTPAD(d, HBOX + HBUTTONS);

	y = d->y + d->h - BORDER - HBUTTONS - HBOX;
	x = d->x + d->w/2 - 7;
	update_box(d->conf, c[0].win, y, x, HBOX, WBOX, LOWERED);
	mvwaddch(d->widget, d->h - 5, d->w/2 - 3, ':');
	update_box(d->conf, c[1].win, y, x += 5, HBOX, WBOX, LOWERED);
	mvwaddch(d->widget, d->h - 5, d->w/2 + 2, ':');
	update_box(d->conf, c[2].win, y, x + 5, HBOX, WBOX, LOWERED);
	wnoutrefresh(d->widget); /* for mvwaddch(':') */

	return (0);
}

/* API */
int
bsddialog_timebox(struct bsddialog_conf *conf, const char* text, int rows,
    int cols, unsigned int *hh, unsigned int *mm, unsigned int *ss)
{
	bool loop, focusbuttons;
	int i, retval, sel;
	wint_t input;
	struct dialog d;
	struct clock c[3] = {
		{23, *hh, NULL},
		{59, *mm, NULL},
		{59, *ss, NULL}
	};

	CHECK_PTR(hh);
	CHECK_PTR(mm);
	CHECK_PTR(ss);
	if (prepare_dialog(conf, text, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&d, true, OK_LABEL, CANCEL_LABEL);
	for (i=0; i<3; i++) {
		if ((c[i].win = newwin(1, 1, 1, 1)) == NULL)
			RETURN_FMTERROR("Cannot build WINDOW for time[%d]", i);
		wbkgd(c[i].win, t.dialog.color);
		c[i].value = MIN(c[i].value, c[i].max);
	}
	if (timebox_redraw(&d, c) != 0)
		return (BSDDIALOG_ERROR);

	sel = -1;
	loop = focusbuttons = true;
	while (loop) {
		for (i = 0; i < 3; i++)
			drawsquare(conf, c[i].win, c[i].value, sel == i);
		doupdate();
		if (get_wch(&input) == ERR)
			continue;
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			if (focusbuttons || conf->button.always_active) {
				retval = BUTTONVALUE(d.bs);
				loop = false;
			}
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case KEY_RIGHT:
		case '\t': /* TAB */
			if (focusbuttons) {
				d.bs.curr++;
				focusbuttons = d.bs.curr < (int)d.bs.nbuttons ?
				    true : false;
				if (focusbuttons == false) {
					sel = 0;
					d.bs.curr =
					    conf->button.always_active ? 0 : -1;
				}
			} else {
				sel++;
				focusbuttons = sel > 2 ? true : false;
				if (focusbuttons) {
					d.bs.curr = 0;
				}
			}
			DRAW_BUTTONS(d);
			break;
		case KEY_LEFT:
			if (focusbuttons) {
				d.bs.curr--;
				focusbuttons = d.bs.curr < 0 ? false : true;
				if (focusbuttons == false) {
					sel = 2;
					d.bs.curr =
					    conf->button.always_active ? 0 : -1;
				}
			} else {
				sel--;
				focusbuttons = sel < 0 ? true : false;
				if (focusbuttons)
					d.bs.curr = (int)d.bs.nbuttons - 1;
			}
			DRAW_BUTTONS(d);
			break;
		case KEY_UP:
			if (focusbuttons) {
				sel = 0;
				focusbuttons = false;
				d.bs.curr = conf->button.always_active ? 0 : -1;
				DRAW_BUTTONS(d);
			} else {
				c[sel].value = c[sel].value > 0 ?
				    c[sel].value - 1 : c[sel].max;
			}
			break;
		case KEY_DOWN:
			if (focusbuttons)
				break;
			c[sel].value = c[sel].value < c[sel].max ?
			    c[sel].value + 1 : 0;
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(conf) != 0)
				return (BSDDIALOG_ERROR);
			if (timebox_redraw(&d, c) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_RESIZE:
			if (timebox_redraw(&d, c) != 0)
				return (BSDDIALOG_ERROR);
			break;
		default:
			if (shortcut_buttons(input, &d.bs)) {
				DRAW_BUTTONS(d);
				doupdate();
				retval = BUTTONVALUE(d.bs);
				loop = false;
			}
		}
	}

	*hh = c[0].value;
	*mm = c[1].value;
	*ss = c[2].value;

	for (i = 0; i < 3; i++)
		delwin(c[i].win);
	end_dialog(&d);

	return (retval);
}
