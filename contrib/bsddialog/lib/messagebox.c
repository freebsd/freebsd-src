/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alfonso Sabato Siciliano
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

#include <ctype.h>
#include <string.h>

#ifdef PORTNCURSES
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/* "Message": msgbox - yesno */

#define AUTO_WIDTH	(COLS / 3U)
/*
 * Min height = 5: 2 up & down borders + 2 label & up border buttons + 1 line
 * for text, at least 1 line is important for widget_withtextpad_init() to avoid
 * "Cannot build the pad window for text".
 */
#define MIN_HEIGHT	5

extern struct bsddialog_theme t;

static int
message_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    char *text, struct buttons bs)
{
	int maxword, maxline, nlines, line;

	if (get_text_properties(conf, text, &maxword, &maxline, &nlines) != 0)
		return BSDDIALOG_ERROR;

	if (cols == BSDDIALOG_AUTOSIZE) {
		*w = VBORDERS;
		/* buttons size */
		*w += bs.nbuttons * bs.sizebutton;
		*w += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.button.space : 0;
		/* text size */
		line = MIN(maxline + VBORDERS + t.text.hmargin * 2, AUTO_WIDTH);
		line = MAX(line, (int) (maxword + VBORDERS + t.text.hmargin * 2));
		*w = MAX(*w, line);
		/* conf.auto_minwidth */
		*w = MAX(*w, (int)conf->auto_minwidth);
		/* avoid terminal overflow */
		*w = MIN(*w, widget_max_width(conf));
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = MIN_HEIGHT - 1;
		if (maxword > 0)
			*h += MAX(nlines, (int)(*w / GET_ASPECT_RATIO(conf)));
		*h = MAX(*h, MIN_HEIGHT);
		/* conf.auto_minheight */
		*h = MAX(*h, (int)conf->auto_minheight);
		/* avoid terminal overflow */
		*h = MIN(*h, widget_max_height(conf));
	}

	return 0;
}

static int message_checksize(int rows, int cols, struct buttons bs)
{
	int mincols;

	mincols = VBORDERS;
	mincols += bs.nbuttons * bs.sizebutton;
	mincols += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.button.space : 0;

	if (cols < mincols)
		RETURN_ERROR("Few cols, Msgbox and Yesno need at least width "\
		    "for borders, buttons and spaces between buttons");

	if (rows < MIN_HEIGHT)
		RETURN_ERROR("Msgbox and Yesno need at least height 5");

	return 0;
}

static void
buttonsupdate(WINDOW *widget, int h, int w, struct buttons bs, bool shortkey)
{
	draw_buttons(widget, h-2, w, bs, shortkey);
	wnoutrefresh(widget);
}

static void
textupdate(WINDOW *widget, int y, int x, int h, int w, WINDOW *textpad,
    int htextpad, int textrow)
{

	if (htextpad > h - 4) {
		mvwprintw(widget, h-3, w-6, "%3d%%",
		    100 * (textrow+h-4)/ htextpad);
		wnoutrefresh(widget);
	}

	pnoutrefresh(textpad, textrow, 0, y+1, x+2, y+h-4, x+w-2);
}

static int
do_widget(struct bsddialog_conf *conf, char *text, int rows, int cols,
    struct buttons bs, bool shortkey)
{
	WINDOW *widget, *textpad, *shadow;
	bool loop;
	int i, y, x, h, w, input, output, htextpad, textrow;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	if (message_autosize(conf, rows, cols, &h, &w, text, bs) != 0)
		return BSDDIALOG_ERROR;
	if (message_checksize(h, w, bs) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, true) != 0)
		return BSDDIALOG_ERROR;

	textrow = 0;
	loop = true;
	buttonsupdate(widget, h, w, bs, shortkey);
	textupdate(widget, y, x, h, w, textpad, htextpad, textrow);
	while(loop) {
		doupdate();
		input = getch();
		switch (input) {
		case 10: /* Enter */
			output = bs.value[bs.curr];
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			buttonsupdate(widget, h, w, bs, shortkey);
			break;
		case KEY_F(1):
			if (conf->f1_file == NULL && conf->f1_message == NULL)
				break;
			if (f1help(conf) != 0)
				return BSDDIALOG_ERROR;
			/* No break! the terminal size can change */
		case KEY_RESIZE:
			hide_widget(y, x, h, w,conf->shadow);

			/*
			 * Unnecessary, but, when the columns decrease the
			 * following "refresh" seem not work
			 */
			refresh();

			if (set_widget_size(conf, rows, cols, &h, &w) != 0)
				return BSDDIALOG_ERROR;
			if (message_autosize(conf, rows, cols, &h, &w, text, bs) != 0)
				return BSDDIALOG_ERROR;
			if (message_checksize(h, w, bs) != 0)
				return BSDDIALOG_ERROR;
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return BSDDIALOG_ERROR;

			wclear(shadow);
			mvwin(shadow, y + t.shadow.h, x + t.shadow.w);
			wresize(shadow, h, w);

			wclear(widget);
			mvwin(widget, y, x);
			wresize(widget, h, w);

			htextpad = 1;
			wclear(textpad);
			wresize(textpad, 1, w - HBORDERS - t.text.hmargin * 2);

			if(update_widget_withtextpad(conf, shadow, widget, h, w,
			    RAISED, textpad, &htextpad, text, true) != 0)
				return BSDDIALOG_ERROR;

			buttonsupdate(widget, h, w, bs, shortkey);
			textupdate(widget, y, x, h, w, textpad, htextpad, textrow);

			/* Important to fix grey lines expanding screen */
			refresh();
			break;
		case KEY_UP:
			if (textrow == 0)
				break;
			textrow--;
			textupdate(widget, y, x, h, w, textpad, htextpad, textrow);
			break;
		case KEY_DOWN:
			if (textrow + h - 4 >= htextpad)
				break;
			textrow++;
			textupdate(widget, y, x, h, w, textpad, htextpad, textrow);
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				buttonsupdate(widget, h, w, bs, shortkey);
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				buttonsupdate(widget, h, w, bs, shortkey);
			}
			break;
		default:
			if (shortkey == false)
				break;

			for (i = 0; i < (int) bs.nbuttons; i++)
				if (tolower(input) == tolower((bs.label[i])[0])) {
					output = bs.value[i];
					loop = false;
			}
		}
	}

	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return output;
}

/* API */

int
bsddialog_msgbox(struct bsddialog_conf *conf, char* text, int rows, int cols)
{
	struct buttons bs;

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    NULL /* nocancel */, BUTTONLABEL(help_label));

	return (do_widget(conf, text, rows, cols, bs, true));
}

int
bsddialog_yesno(struct bsddialog_conf *conf, char* text, int rows, int cols)
{
	struct buttons bs;

	get_buttons(conf, &bs,
	    conf->button.ok_label == NULL ? "Yes" : conf->button.ok_label,
	    BUTTONLABEL(extra_label),
	    conf->button.cancel_label == NULL ? "No" : conf->button.cancel_label,
	    BUTTONLABEL(help_label));

	return (do_widget(conf, text, rows, cols, bs, true));
}
