/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2025 Alfonso Sabato Siciliano
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

struct scroll {
	int ypad;      /* y scrollable pad */
	int htext;     /* real h text to draw, to use with htextpad */
	int htextpad;  /* h textpad, draw_dialog() set at least 1 */
	int printrows; /* h - BORDER - HBUTTONS - BORDER */
};

static void textupdate(struct dialog *d, struct scroll *s)
{
	if (s->htext > 0 && s->htextpad > s->printrows) {
		wattron(d->widget, t.dialog.arrowcolor);
		mvwprintw(d->widget, d->h - HBUTTONS - BORDER,
		    d->w - 4 - TEXTHMARGIN - BORDER,
		    "%3d%%", 100 * (s->ypad + s->printrows) / s->htextpad);
		wattroff(d->widget, t.dialog.arrowcolor);
		wnoutrefresh(d->widget);
	}
	rtextpad(d, s->ypad, 0, 0, HBUTTONS);
}

static int message_size_position(struct dialog *d, int *htext)
{
	int minw;

	if (set_widget_size(d->conf, d->rows, d->cols, &d->h, &d->w) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_autosize(d->conf, d->rows, d->cols, &d->h, &d->w,
	    d->text, (*htext < 0) ? htext : NULL, &d->bs, 0, 0) != 0)
		return (BSDDIALOG_ERROR);
	minw = (*htext > 0) ? 1 + TEXTHMARGINS : 0 ;
	if (widget_checksize(d->h, d->w, &d->bs, MIN(*htext, 1), minw) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(d->conf, &d->y, &d->x, d->h, d->w) != 0)
		return (BSDDIALOG_ERROR);

	return (0);
}

static int message_draw(struct dialog *d, bool redraw, struct scroll *s)
{
	int unused;

	if (redraw) { /* redraw: RESIZE or F1 */
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	if (message_size_position(d, &s->htext) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0) /* doupdate() in main loop */
		return (BSDDIALOG_ERROR);
	if (redraw)
		refresh(); /* Important to fix grey lines expanding screen */

	s->printrows = d->h - BORDER - HBUTTONS - BORDER;
	s->ypad = 0;
	getmaxyx(d->textpad, s->htextpad, unused);
	(void)unused; /* fix unused error */

	return (0);
}

static int
do_message(struct bsddialog_conf *conf, const char *text, int rows, int cols,
    const char *oklabel, const char *cancellabel)
{
	bool loop;
	int retval;
	wint_t input;
	struct scroll s;
	struct dialog d;

	if (prepare_dialog(conf, text, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&d, true, oklabel, cancellabel);
	s.htext = -1;
	if (message_draw(&d, false, &s) != 0)
		return (BSDDIALOG_ERROR);

	loop = true;
	while (loop) {
		textupdate(&d, &s);
		doupdate();
		if (get_wch(&input) == ERR)
			continue;
		switch (input) {
		case KEY_ENTER:
		case 10: /* Enter */
			retval = BUTTONVALUE(d.bs);
			loop = false;
			break;
		case 27: /* Esc */
			if (d.conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case '\t': /* TAB */
		case KEY_RIGHT:
			d.bs.curr = (d.bs.curr + 1) % d.bs.nbuttons;
			DRAW_BUTTONS(d);
			break;
		case KEY_LEFT:
			d.bs.curr--;
			if (d.bs.curr < 0)
				 d.bs.curr = d.bs.nbuttons - 1;
			DRAW_BUTTONS(d);
			break;
		case '-':
		case KEY_CTRL('p'):
		case KEY_UP:
			if (s.ypad > 0)
				s.ypad--;
			break;
		case '+':
		case KEY_CTRL('n'):
		case KEY_DOWN:
			if (s.ypad + s.printrows < s.htextpad)
				s.ypad++;
			break;
		case KEY_HOME:
			s.ypad = 0;
			break;
		case KEY_END:
			s.ypad = MAX(s.htextpad - s.printrows, 0);
			break;
		case KEY_PPAGE:
			s.ypad = MAX(s.ypad - s.printrows, 0);
			break;
		case KEY_NPAGE:
			s.ypad += s.printrows;
			if (s.ypad + s.printrows > s.htextpad)
				s.ypad = s.htextpad - s.printrows;
			break;
		case KEY_F(1):
			if (d.conf->key.f1_file == NULL &&
			    d.conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(d.conf) != 0)
				return (BSDDIALOG_ERROR);
			if (message_draw(&d, true, &s) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_CTRL('l'):
		case KEY_RESIZE:
			if (message_draw(&d, true, &s) != 0)
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

	end_dialog(&d);

	return (retval);
}

/* API */
int
bsddialog_msgbox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols)
{
	return (do_message(conf, text, rows, cols, OK_LABEL, NULL));
}

int
bsddialog_yesno(struct bsddialog_conf *conf, const char *text, int rows,
    int cols)
{
	return (do_message(conf, text, rows, cols, "Yes", "No"));
}

int
bsddialog_infobox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols)
{
	int htext;
	struct dialog d;

	if (prepare_dialog(conf, text, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	htext = -1;
	if (message_size_position(&d, &htext) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(&d) != 0)
		return (BSDDIALOG_ERROR);
	TEXTPAD(&d, 0);
	doupdate();

	end_dialog(&d);

	return (BSDDIALOG_OK);
}
