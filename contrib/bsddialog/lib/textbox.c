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

extern struct bsddialog_theme t;

static void
textbox_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h,
    int *w, int hpad, int wpad, struct buttons bs)
{
	if (cols == BSDDIALOG_AUTOSIZE)
		*w = widget_min_width(conf, 0, wpad, &bs);

	if (rows == BSDDIALOG_AUTOSIZE)
		*h = widget_min_height(conf, 0, hpad, true);
}

static int
textbox_checksize(int rows, int cols, int hpad, struct buttons bs)
{
	int mincols;

	mincols = VBORDERS + bs.sizebutton;

	if (cols < mincols)
		RETURN_ERROR("Few cols for the textbox");

	if (rows < 4 /* HBORDERS + button*/ + (hpad > 0 ? 1 : 0))
		RETURN_ERROR("Few rows for the textbox");

	return (0);
}

/* API */
int
bsddialog_textbox(struct bsddialog_conf *conf, const char* file, int rows,
    int cols)
{
	bool loop;
	int i, output, input;
	int y, x, h, w, hpad, wpad, ypad, xpad, ys, ye, xs, xe, printrows;
	char buf[BUFSIZ];
	FILE *fp;
	struct buttons bs;
	WINDOW *shadow, *widget, *pad;

	if ((fp = fopen(file, "r")) == NULL)
		RETURN_ERROR("Cannot open file");

	hpad = 1;
	wpad = 1;
	pad = newpad(hpad, wpad);
	wbkgd(pad, t.dialog.color);
	i = 0;
	while (fgets(buf, BUFSIZ, fp) != NULL) {
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

	bs.nbuttons = 1;
	bs.label[0] = "EXIT";
	if (conf->button.ok_label != NULL)
		bs.label[0] = conf->button.ok_label;
	bs.value[0] = BSDDIALOG_OK;
	bs.curr = 0;
	bs.sizebutton = strlen(bs.label[0]) + 2;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	textbox_autosize(conf, rows, cols, &h, &w, hpad, wpad, bs);
	if (textbox_checksize(h, w, hpad, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, NULL, NULL, &bs,
	    true) != 0)
		return (BSDDIALOG_ERROR);

	ys = y + 1;
	xs = x + 1;
	ye = ys + h - 5;
	xe = xs + w - 3;
	ypad = xpad = 0;
	printrows = h-4;
	loop = true;
	while (loop) {
		wnoutrefresh(widget);
		pnoutrefresh(pad, ypad, xpad, ys, xs, ye, xe);
		doupdate();
		input = getch();
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			output = BSDDIALOG_OK;
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				output = BSDDIALOG_ESC;
				loop = false;
			}
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
			if (ypad + printrows > hpad)
				ypad = hpad - printrows;
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
			if (conf->f1_file == NULL && conf->f1_message == NULL)
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
			textbox_autosize(conf, rows, cols, &h, &w, hpad, wpad,
			    bs);
			if (textbox_checksize(h, w, hpad, bs) != 0)
				return (BSDDIALOG_ERROR);
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return (BSDDIALOG_ERROR);

			ys = y + 1;
			xs = x + 1;
			ye = ys + h - 5;
			xe = xs + w - 3;
			ypad = xpad = 0;
			printrows = h - 4;

			if (update_dialog(conf, shadow, widget, y, x, h, w,
			    NULL, NULL, &bs, true) != 0)
				return (BSDDIALOG_ERROR);

			/* Important to fix grey lines expanding screen */
			refresh();
			break;
		default:
			if (shortcut_buttons(input, &bs)) {
				output = bs.value[bs.curr];
				loop = false;
			}
		}
	}

	end_dialog(conf, shadow, widget, pad);

	return (output);
}