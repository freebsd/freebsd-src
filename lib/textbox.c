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

struct scrolltext {
	WINDOW *pad;
	int ypad;
	int xpad;
	int ys;
	int ye;
	int xs;
	int xe;
	int hpad;
	int wpad;
	int margin;    /* 2 with multicolumn char, 0 otherwise */
	int printrows; /* d.h - BORDERS - HBUTTONS */
};

static void updateborders(struct dialog *d, struct scrolltext *st)
{
	chtype arrowch, borderch;

	if (d->conf->no_lines)
		borderch = ' ';
	else if (d->conf->ascii_lines)
		borderch = '|';
	else
		borderch = ACS_VLINE;

	if (st->xpad > 0) {
		arrowch = d->conf->ascii_lines ? '<' : ACS_LARROW;
		arrowch |= t.dialog.arrowcolor;
	} else {
		arrowch = borderch;
		arrowch |= t.dialog.lineraisecolor;
	}
	mvwvline(d->widget, (d->h / 2) - 2, 0, arrowch, 4);

	if (st->xpad + d->w - 2 - st->margin < st->wpad) {
		arrowch = d->conf->ascii_lines ? '>' : ACS_RARROW;
		arrowch |= t.dialog.arrowcolor;
	} else {
		arrowch = borderch;
		arrowch |= t.dialog.linelowercolor;
	}
	mvwvline(d->widget, (d->h / 2) - 2, d->w - 1, arrowch, 4);

	if (st->hpad > d->h - 4) {
		wattron(d->widget, t.dialog.arrowcolor);
		mvwprintw(d->widget, d->h - 3, d->w - 6,
		    "%3d%%", 100 * (st->ypad + d->h - 4) / st->hpad);
		wattroff(d->widget, t.dialog.arrowcolor);
	}
}

static int textbox_size_position(struct dialog *d, struct scrolltext *st)
{
	int minw;

	if (set_widget_size(d->conf, d->rows, d->cols, &d->h, &d->w) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_autosize(d->conf, d->rows, d->cols, &d->h, &d->w,
	    d->text, NULL, &d->bs, st->hpad, st->wpad + st->margin) != 0)
		return (BSDDIALOG_ERROR);
	minw = (st->wpad > 0) ? 2 /*multicolumn char*/ + st->margin : 0 ;
	if (widget_checksize(d->h, d->w, &d->bs, MIN(st->hpad, 1), minw) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(d->conf, &d->y, &d->x, d->h, d->w) != 0)
		return (BSDDIALOG_ERROR);

	return (0);
}

static int textbox_draw(struct dialog *d, struct scrolltext *st)
{
	if (d->built) {
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	if (textbox_size_position(d, st) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0)
		return (BSDDIALOG_ERROR);
	if (d->built)
		refresh(); /* Important to fix grey lines expanding screen */

	st->ys = d->y + 1;
	st->xs = (st->margin == 0) ? d->x + 1 : d->x + 2;
	st->ye = st->ys + d->h - 5;
	st->xe = st->xs + d->w - 3 - st->margin;
	st->ypad = st->xpad = 0;
	st->printrows = d->h-4;

	return (0);
}

/* API */
int
bsddialog_textbox(struct bsddialog_conf *conf, const char *file, int rows,
    int cols)
{
	bool loop, has_multicol_ch;
	int i, retval;
	unsigned int defaulttablen, linecols;
	wint_t input;
	char buf[BUFSIZ];
	FILE *fp;
	struct scrolltext st;
	struct dialog d;

	if (file == NULL)
		RETURN_ERROR("*file is NULL");
	if ((fp = fopen(file, "r")) == NULL)
		RETURN_FMTERROR("Cannot open file \"%s\"", file);

	if (prepare_dialog(conf, "" /* fake */, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&d, true, "EXIT", NULL);

	defaulttablen = TABSIZE;
	if (conf->text.tablen > 0)
		set_tabsize(conf->text.tablen);
	st.hpad = 1;
	st.wpad = 1;
	st.pad = newpad(st.hpad, st.wpad);
	wbkgd(st.pad, t.dialog.color);
	st.margin = 0;
	i = 0;
	while (fgets(buf, BUFSIZ, fp) != NULL) {
		if (str_props(buf, &linecols, &has_multicol_ch) != 0)
			continue;
		if ((int)linecols > st.wpad) {
			st.wpad = linecols;
			wresize(st.pad, st.hpad, st.wpad);
		}
		if (i > st.hpad-1) {
			st.hpad++;
			wresize(st.pad, st.hpad, st.wpad);
		}
		mvwaddstr(st.pad, i, 0, buf);
		i++;
		if (has_multicol_ch)
			st.margin = 2;
	}
	fclose(fp);
	set_tabsize(defaulttablen); /* reset because it is curses global */

	if (textbox_draw(&d, &st) != 0)
		return (BSDDIALOG_ERROR);

	loop = true;
	while (loop) {
		updateborders(&d, &st);
		/*
		 * Overflow multicolumn charchter right border:
		 * wnoutrefresh(widget);
		 * pnoutrefresh(pad, ypad, xpad, ys, xs, ye, xe);
		 * doupdate();
		 */
		wrefresh(d.widget);
		prefresh(st.pad, st.ypad, st.xpad, st.ys, st.xs, st.ye, st.xe);
		if (get_wch(&input) == ERR)
			continue;
		if (shortcut_buttons(input, &d.bs)) {
			DRAW_BUTTONS(d);
			doupdate();
			retval = BUTTONVALUE(d.bs);
			break; /* loop */
		}
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			retval = BSDDIALOG_OK;
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case '\t': /* TAB */
			d.bs.curr = (d.bs.curr + 1) % d.bs.nbuttons;
			DRAW_BUTTONS(d);
			break;
		case KEY_HOME:
			st.ypad = 0;
			break;
		case KEY_END:
			st.ypad = MAX(st.hpad - st.printrows, 0);
			break;
		case KEY_PPAGE:
			st.ypad = MAX(st.ypad - st.printrows, 0);
			break;
		case KEY_NPAGE:
			st.ypad += st.printrows;
			if (st.ypad + st.printrows > st.hpad)
				st.ypad = st.hpad - st.printrows;
			break;
		case '0':
			st.xpad = 0;
			break;
		case KEY_LEFT:
		case 'h':
			st.xpad = MAX(st.xpad - 1, 0);
			break;
		case KEY_RIGHT:
		case 'l':
			if (st.xpad + d.w - 2 - st.margin < st.wpad)
				st.xpad++;
			break;
		case KEY_UP:
		case 'k':
			st.ypad = MAX(st.ypad - 1, 0);
			break;
		case KEY_DOWN:
		case'j':
			if (st.ypad + st.printrows <= st.hpad -1)
				st.ypad++;
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(conf) != 0)
				return (BSDDIALOG_ERROR);
			if (textbox_draw(&d, &st) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_RESIZE:
			if (textbox_draw(&d, &st) != 0)
				return (BSDDIALOG_ERROR);
			break;
		}
	}

	delwin(st.pad);
	end_dialog(&d);

	return (retval);
}
