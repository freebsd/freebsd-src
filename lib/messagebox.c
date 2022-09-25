/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2022 Alfonso Sabato Siciliano
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

#include <sys/param.h>

#include <curses.h>
#include <string.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

static int
message_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h,
    int *w, const char *text, bool *hastext, struct buttons bs)
{
	int htext, wtext;

	if (cols == BSDDIALOG_AUTOSIZE || rows == BSDDIALOG_AUTOSIZE ||
	    hastext != NULL) {
		if (text_size(conf, rows, cols, text, &bs, 0, 1, &htext,
		    &wtext) != 0)
			return (BSDDIALOG_ERROR);
		if (hastext != NULL)
			*hastext = htext > 0 ? true : false;
	}

	if (cols == BSDDIALOG_AUTOSIZE)
		*w = widget_min_width(conf, wtext, 0, &bs);

	if (rows == BSDDIALOG_AUTOSIZE)
		*h = widget_min_height(conf, htext, 0, true);

	return (0);
}

static int
message_checksize(int rows, int cols, bool hastext, struct buttons bs)
{
	int mincols;

	mincols = VBORDERS;
	mincols += buttons_min_width(bs);

	if (cols < mincols)
		RETURN_ERROR("Few cols, Msgbox and Yesno need at least width "
		    "for borders, buttons and spaces between buttons");

	if (rows < HBORDERS + 2 /* buttons */)
		RETURN_ERROR("Msgbox and Yesno need at least 4 rows");
	if (hastext && rows < HBORDERS + 2 /*buttons*/ + 1 /* text row */)
		RETURN_ERROR("Msgbox and Yesno with text need at least 5 rows");

	return (0);
}

static void
textupdate(WINDOW *widget, WINDOW *textpad, int htextpad, int ytextpad,
    bool hastext)
{
	int y, x, h, w;

	getbegyx(widget, y, x);
	getmaxyx(widget, h, w);

	if (hastext && htextpad > h - 4) {
		wattron(widget, t.dialog.arrowcolor);
		mvwprintw(widget, h-3, w-6, "%3d%%",
		    100 * (ytextpad+h-4)/ htextpad);
		wattroff(widget, t.dialog.arrowcolor);
		wnoutrefresh(widget);
	}

	pnoutrefresh(textpad, ytextpad, 0, y+1, x+2, y+h-4, x+w-2);
}

static int
do_message(struct bsddialog_conf *conf, const char *text, int rows, int cols,
    struct buttons bs)
{
	bool hastext, loop;
	int y, x, h, w, retval, ytextpad, htextpad, printrows, unused;
	WINDOW *widget, *textpad, *shadow;
	wint_t input;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (message_autosize(conf, rows, cols, &h, &w, text, &hastext, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (message_checksize(h, w, hastext, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, &bs,
	    true) != 0)
		return (BSDDIALOG_ERROR);

	printrows = h - 4;
	ytextpad = 0;
	getmaxyx(textpad, htextpad, unused);
	unused++; /* fix unused error */
	loop = true;
	while (loop) {
		textupdate(widget, textpad, htextpad, ytextpad, hastext);
		doupdate();
		if (get_wch(&input) == ERR)
			continue;
		switch (input) {
		case KEY_ENTER:
		case 10: /* Enter */
			retval = bs.value[bs.curr];
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			draw_buttons(widget, bs, true);
			wnoutrefresh(widget);
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				draw_buttons(widget, bs, true);
				wnoutrefresh(widget);
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int)bs.nbuttons - 1) {
				bs.curr++;
				draw_buttons(widget, bs, true);
				wnoutrefresh(widget);
			}
			break;
		case KEY_UP:
			if (ytextpad > 0)
				ytextpad--;
			break;
		case KEY_DOWN:
			if (ytextpad + printrows < htextpad)
				ytextpad++;
			break;
		case KEY_HOME:
			ytextpad = 0;
			break;
		case KEY_END:
			ytextpad = htextpad - printrows;
			ytextpad = ytextpad < 0 ? 0 : ytextpad;
			break;
		case KEY_PPAGE:
			ytextpad -= printrows;
			ytextpad = ytextpad < 0 ? 0 : ytextpad;
			break;
		case KEY_NPAGE:
			ytextpad += printrows;
			if (ytextpad + printrows > htextpad)
				ytextpad = htextpad - printrows;
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help(conf) != 0)
				return (BSDDIALOG_ERROR);
			/* No break, screen size can change */
		case KEY_RESIZE:
			/* Important for decreasing screen */
			hide_widget(y, x, h, w, conf->shadow);
			refresh();

			if (set_widget_size(conf, rows, cols, &h, &w) != 0)
				return (BSDDIALOG_ERROR);
			if (message_autosize(conf, rows, cols, &h, &w, text,
			    NULL, bs) != 0)
				return (BSDDIALOG_ERROR);
			if (message_checksize(h, w, hastext, bs) != 0)
				return (BSDDIALOG_ERROR);
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return (BSDDIALOG_ERROR);

			if (update_dialog(conf, shadow, widget, y, x, h, w,
			    textpad, text, &bs, true) != 0)
				return (BSDDIALOG_ERROR);

			printrows = h - 4;
			getmaxyx(textpad, htextpad, unused);
			ytextpad = 0;
			textupdate(widget, textpad, htextpad, ytextpad, hastext);

			/* Important to fix grey lines expanding screen */
			refresh();
			break;
		default:
			if (shortcut_buttons(input, &bs)) {
				retval = bs.value[bs.curr];
				loop = false;
			}
		}
	}

	end_dialog(conf, shadow, widget, textpad);

	return (retval);
}

/* API */
int
bsddialog_msgbox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols)
{
	struct buttons bs;

	get_buttons(conf, &bs, BUTTON_OK_LABEL, NULL);

	return (do_message(conf, text, rows, cols, bs));
}

int
bsddialog_yesno(struct bsddialog_conf *conf, const char *text, int rows,
    int cols)
{
	struct buttons bs;

	get_buttons(conf, &bs, "Yes", "No");

	return (do_message(conf, text, rows, cols, bs));
}