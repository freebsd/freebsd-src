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

#include <string.h>

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/* "Text": tailbox - tailboxbg - textbox */

#define BUTTON_TEXTBOX "HELP"

extern struct bsddialog_theme t;

enum textmode { TAILMODE, TAILBGMODE, TEXTMODE};

static void
textbox_autosize(struct bsddialog_conf conf, int rows, int cols, int *h, int *w,
    int hpad, int wpad)
{

	if (cols == BSDDIALOG_AUTOSIZE) {
		*w = VBORDERS;
		/* buttons size */
		*w += strlen(BUTTON_TEXTBOX) + 2 /* text delims*/;
		/* text size */
		*w = MAX(*w, wpad + VBORDERS);
		/* avoid terminal overflow */
		*w = MIN(*w, widget_max_width(conf)-1); /* again -1, fix util.c */
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = hpad + 4; /* HBORDERS + button border */
		/* avoid terminal overflow */
		*h = MIN(*h, widget_max_height(conf));
	}
}

static int textbox_checksize(int rows, int cols, int hpad, int wpad)
{
	int mincols;

	mincols = VBORDERS + strlen(BUTTON_TEXTBOX) + 2 /* text delims */;

	if (cols < mincols)
		RETURN_ERROR("Few cols for the textbox");

	if (rows < 4 /* HBORDERS + button*/ + (hpad > 0 ? 1 : 0))
		RETURN_ERROR("Few rows for the textbox");

	return 0;
}

static int
do_textbox(enum textmode mode, struct bsddialog_conf conf, char* path, int rows, int cols)
{
	WINDOW *widget, *pad, *shadow;
	int i, input, y, x, h, w, hpad, wpad, ypad, xpad, ys, ye, xs, xe, printrows;
	char buf[BUFSIZ], *exitbutt;
	FILE *fp;
	bool loop;
	int output;

	if (mode == TAILMODE || mode == TAILBGMODE) {
		bsddialog_msgbox(conf, "Tailbox and Tailboxbg unimplemented", rows, cols);
		RETURN_ERROR("Tailbox and Tailboxbg unimplemented");
	}

	if ((fp = fopen(path, "r")) == NULL)
		RETURN_ERROR("Cannot open file");
	/*if (mode == TAILMODE) {
		fseek (fp, 0, SEEK_END);
		i = nlines = 0;
		while (i < hpad) {
			line = ;
		}
		for (i=hpad-1; i--; i>=0) {
		}
	}*/
	hpad = 1;
	wpad = 1;
	pad = newpad(hpad, wpad);
	wbkgd(pad, t.widgetcolor);
	i = 0;
	while(fgets(buf, BUFSIZ, fp) != NULL) {
		if ((int) strlen(buf) > wpad) {
			wpad = strlen(buf);
			wresize(pad, hpad, wpad);
		}
		if (i > hpad-1) {
			hpad++;
			wresize(pad, hpad, wpad);
		}
		mvwaddstr(pad, i, 0, buf);
		i++;
	}
	fclose(fp);

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	textbox_autosize(conf, rows, cols, &h, &w, hpad, wpad);
	if (textbox_checksize(h, w, hpad, wpad) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    NULL, NULL, NULL, true) != 0)
		return BSDDIALOG_ERROR;

	exitbutt = conf.button.exit_label == NULL ? BUTTON_TEXTBOX : conf.button.exit_label;
	draw_button(widget, h-2, (w-2)/2 - strlen(exitbutt)/2, strlen(exitbutt)+2,
	    exitbutt, true, true);

	wrefresh(widget);

	ys = y + 1;
	xs = x + 1;
	ye = ys + h - 5;
	xe = xs + w - 3;
	ypad = xpad = 0;
	printrows = h-4;
	loop = true;
	while(loop) {
		prefresh(pad, ypad, xpad, ys, xs, ye, xe);
		input = getch();
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			output = BSDDIALOG_YESOK;
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case KEY_HOME:
			ypad = 0;
			break;
		case KEY_END:
			ypad = hpad - printrows;
			ypad = ypad < 0 ? 0 : ypad;
			break;
		case KEY_PPAGE:
			ypad -= printrows;
			ypad = ypad < 0 ? 0 : ypad;
			break;
		case KEY_NPAGE:
			ypad += printrows;
			ypad = ypad + printrows > hpad ? hpad - printrows : ypad;
			break;
		case '0':
			xpad = 0;
		case KEY_LEFT:
		case 'h':
			xpad = xpad > 0 ? xpad - 1 : 0;
			break;
		case KEY_RIGHT:
		case 'l':
			xpad = (xpad + w-2) < wpad-1 ? xpad + 1 : xpad;
			break;
		case KEY_UP:
		case 'k':
			ypad = ypad > 0 ? ypad - 1 : 0;
			break;
		case KEY_DOWN:
		case'j':
			ypad = ypad + printrows <= hpad -1 ? ypad + 1 : ypad;
			break;
		case KEY_F(1):
			if (conf.hfile == NULL)
				break;
			if (f1help(conf) != 0)
				return BSDDIALOG_ERROR;
			/* No break! the terminal size can change */
		case KEY_RESIZE:
			hide_widget(y, x, h, w,conf.shadow);

			/*
			 * Unnecessary, but, when the columns decrease the
			 * following "refresh" seem not work
			 */
			refresh();

			if (set_widget_size(conf, rows, cols, &h, &w) != 0)
				return BSDDIALOG_ERROR;
			textbox_autosize(conf, rows, cols, &h, &w, hpad, wpad);
			if (textbox_checksize(h, w, hpad, wpad) != 0)
				return BSDDIALOG_ERROR;
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return BSDDIALOG_ERROR;

			wclear(shadow);
			mvwin(shadow, y + t.shadowrows, x + t.shadowcols);
			wresize(shadow, h, w);

			wclear(widget);
			mvwin(widget, y, x);
			wresize(widget, h, w);

			ys = y + 1;
			xs = x + 1;
			ye = ys + h - 5;
			xe = xs + w - 3;
			ypad = xpad = 0;
			printrows = h - 4;

			if(update_widget_withtextpad(conf, shadow, widget, h, w,
			    RAISED, NULL, NULL, NULL, true) != 0)
			return BSDDIALOG_ERROR;

			draw_button(widget, h-2, (w-2)/2 - strlen(exitbutt)/2,
			    strlen(exitbutt)+2, exitbutt, true, true);

			wrefresh(widget); /* for button */

			/* Important to fix grey lines expanding screen */
			refresh();
			break;
		}
	}

	end_widget_withtextpad(conf, widget, h, w, pad, shadow);

	return output;
}

int bsddialog_tailbox(struct bsddialog_conf conf, char* text, int rows, int cols)
{

	return (do_textbox(TAILMODE, conf, text, rows, cols));
}

int bsddialog_tailboxbg(struct bsddialog_conf conf, char* text, int rows, int cols)
{

	return (do_textbox(TAILBGMODE, conf, text, rows, cols));
}


int bsddialog_textbox(struct bsddialog_conf conf, char* text, int rows, int cols)
{

	return (do_textbox(TEXTMODE, conf, text, rows, cols));
}

