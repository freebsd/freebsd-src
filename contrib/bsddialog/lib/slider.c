/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Braulio Rivas
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
#include <stdlib.h>
#include <string.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define MINHSLIDER 13
#define MINWSLIDER 36

#define NULLWIN -1
#define START_WIN 0
#define END_WIN 1
#define STEP_WIN 2
#define SLIDER_WIN 3
#define NWIN 4

enum operation {
	MOVERIGHT,
	MOVEFARRIGHT,
	MOVEFASTRIGHT,
	MOVELEFT,
	MOVEFARLEFT,
	MOVEFASTLEFT,
	INCREASELEFT,
	DECREASELEFT,
	INCREASERIGHT,
	DECREASERIGHT,
	INCREASESTEP,
	DECREASESTEP,
};

struct sliderctl {
	enum operation op;
	unsigned long (*spaces)[2];
	int nspaces; /* api unsigned, but segfault handlesliderctl():MOVELEFT */
	unsigned long length;
	unsigned long *start;
	unsigned long *end;
	unsigned long step;
};

static int crashes(long x, long y, long a, long b)
{
	return ((x <= a && a <= y) || (x <= b && b <= y));
}

static int fits(long x, long y, long a, long b)
{
	return ((x <= a) && (b <= y));
}

static void handlesliderctl(struct sliderctl *sliderctl)
{
	int i, step, tmpstep;
	unsigned long x, y, size, old_start, old_end;
	signed long new_start, new_end;

	step = sliderctl->step;
	old_start = *(sliderctl->start);
	new_start = old_start;
	old_end = *(sliderctl->end);
	new_end = old_end;
	size = old_end - old_start + 1;

	switch (sliderctl->op) {
	case MOVERIGHT:
		new_start = old_start + step;
		new_end = old_end + step;

		for (i = 0; i < sliderctl->nspaces; i++) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_start = y + 1;
				new_end = new_start + size - 1;
				break;
			}
		}
		break;
	case MOVELEFT:
		new_start = old_start - step;
		new_end = old_end - step;

		for (i = sliderctl->nspaces - 1; i >= 0; i--) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_end = x - 1;
				new_start = new_end - size + 1;
				break;
			}
		}
		break;
	case INCREASELEFT:
		new_start = old_start + step;
		break;
	case DECREASELEFT:
		new_start = old_start - step;
		for (i = 0; i < sliderctl->nspaces; i++) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_start = old_start;
				break;
			}
		}
		break;
	case INCREASERIGHT:
		new_end = old_end + step;
		for (i = 0; i < sliderctl->nspaces; i++) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_end = old_end;
				break;
			}
		}
		break;
	case DECREASERIGHT:
		new_end = old_end - step;
		break;
	case MOVEFARLEFT:
		new_start = 0;
		new_end = size - 1;
		for (i = 0; i < sliderctl->nspaces; i++) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_start = y + 1;
				new_end = new_start + size - 1;
				break;
			}
		}
		break;
	case MOVEFARRIGHT:
		new_end = (sliderctl->length) - 1;
		new_start = new_end - size + 1;
		for (i = sliderctl->nspaces - 1; i >= 0; i--) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_end = x - 1;
				new_start = new_end - size + 1;
				break;
			}
		}
		break;
	case MOVEFASTLEFT:
		if (size < 10) {
			tmpstep = 1;
		} else {
			tmpstep = ((sliderctl->length) * 10) / 100;
		}
		new_start = old_start - tmpstep;
		new_end = old_end - tmpstep;

		for (i = sliderctl->nspaces - 1; i >= 0; i--) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_end = x - 1;
				new_start = new_end - size + 1;
				break;
			}
		}
		break;
	case MOVEFASTRIGHT:
		if (size < 10) {
			tmpstep = 1;
		} else {
			tmpstep = ((sliderctl->length) * 10) / 100;
		}
		new_start = old_start + tmpstep;
		new_end = old_end + tmpstep;

		for (i = 0; i < sliderctl->nspaces; i++) {
			x = (sliderctl->spaces)[i][0];
			y = (sliderctl->spaces)[i][1];

			if (crashes(x, y, new_start, new_end)) {
				new_start = y + 1;
				new_end = new_start + size - 1;
				break;
			}
		}
		break;
	case INCREASESTEP:
		++step;
		break;
	case DECREASESTEP:
		if (step > 1) {
			--step;
		}
		break;
	}

	if (fits(0, (sliderctl->length) - 1, new_start, new_end) != 1) {
		new_start = old_start;
		new_end = old_end;
	}

	if (new_start > new_end) {
		new_start = old_start;
		new_end = old_end;
	}

	sliderctl->step = step;

	*(sliderctl->start) = new_start;
	*(sliderctl->end) = new_end;
}

static void
drawsquare(struct bsddialog_conf *conf, WINDOW *win, enum elevation elev,
    bool focus, const char *fmt, unsigned long value)
{
	int h, l, w;

	getmaxyx(win, h, w);
	draw_borders(conf, win, elev);
	if (focus) {
		l = 2 + w % 2;
		wattron(win, t.dialog.arrowcolor);
		mvwhline(win, 0, w / 2 - l / 2, UARROW(conf), l);
		mvwhline(win, h - 1, w / 2 - l / 2, DARROW(conf), l);
		wattroff(win, t.dialog.arrowcolor);
	}

	if (focus)
		wattron(win, t.menu.f_namecolor);

	mvwprintw(win, 1, 1, fmt, value);

	if (focus)
		wattroff(win, t.menu.f_namecolor);

	wnoutrefresh(win);
}

static void
print_slider(struct bsddialog_conf *conf, WINDOW *win,
    unsigned long spaces[][2], int nspaces, unsigned long length,
    unsigned long *start, unsigned long *end, bool active)
{
	int i, y, x, l, height, width;
	unsigned long s, e;
	chtype ch;

	getmaxyx(win, height, width);
	wclear(win);
	draw_borders(conf, win, RAISED);

	if (active) {
		wattron(win, t.dialog.arrowcolor);
		mvwvline(win, 1, 0, LARROW(conf), 1);
		mvwvline(win, 1, width - 1, RARROW(conf), 1);
		wattroff(win, t.dialog.arrowcolor);
	}

	y = height / 2;
	width -= 1;

	ch = ' ' | bsddialog_color(BSDDIALOG_RED, BSDDIALOG_RED, 0);
	for (i = 0; i < nspaces; i++) {
		s = spaces[i][0];
		e = spaces[i][1];

		x = (s * width) / length;
		l = ((e - s) * width) / length;

		if ((e - s) == 0) {
			l = 0;
		} else if (l == 0) {
			l = 1;
		}

		mvwhline(win, y, x + 1, ch, l);
	}

	ch = ' ' | t.bar.f_color;
	s = ((*start) * width) / length;
	l = (((*end) - (*start)) * width) / length;
	if ((*end - *start) == 0) {
		l = 0;
	} else if (l == 0) {
		l = 1;
	}
	mvwhline(win, y, s + 1, ch, l);

	wnoutrefresh(win);
}

static int
slider_draw(struct dialog *d, bool redraw, WINDOW *start_win, WINDOW *end_win,
    WINDOW *size_win, WINDOW *step_win, WINDOW *slider_win, const char *unit)
{
	char *buf;
	int yslider, xslider;

	if (redraw) {
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	if (dialog_size_position(d, MINHSLIDER, MINWSLIDER, NULL) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0) /* doupdate in main loop */
		return (BSDDIALOG_ERROR);
	if (redraw)
		refresh(); /* Important to fix grey lines expanding screen */
	TEXTPAD(d, MINHSLIDER + HBUTTONS);

	yslider = d->y + d->h - 15;
	xslider = d->x + d->w / 2 - 17;
	asprintf(&buf, "Start (%s)", unit);
	mvwaddstr(d->widget, d->h - 16, d->w / 2 - 17, buf);
	free(buf);
	update_box(d->conf, start_win, yslider, xslider, 3, 17, RAISED);
	asprintf(&buf, "End (%s)", unit);
	mvwaddstr(d->widget, d->h - 16, d->w / 2, buf);
	free(buf);
	update_box(d->conf, end_win, yslider, xslider + 17, 3, 17, RAISED);
	asprintf(&buf, "Size (%s)", unit);
	mvwaddstr(d->widget, d->h - 12, d->w / 2 - 17, buf);
	free(buf);
	update_box(d->conf, size_win, yslider + 4, xslider, 3, 17, RAISED);
	asprintf(&buf, "Step (%s)", unit);
	mvwaddstr(d->widget, d->h - 12, d->w / 2, buf);
	free(buf);
	update_box(d->conf, step_win, yslider + 4, xslider + 17, 3, 17, RAISED);

	update_box(d->conf, slider_win, yslider + 7, xslider, 3, 34, RAISED);
	wnoutrefresh(d->widget);

	return (0);
}

/* API */
int
bsddialog_slider(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, const char *unit, unsigned long length, unsigned long *start,
    unsigned long *end, bool resize, unsigned int nblocks,
    unsigned long blocks[][2])
{
	struct sliderctl ctl;
	bool loop, focusbuttons;
	int retval, sel;
	wint_t input;
	unsigned long size;
	WINDOW *start_win, *end_win, *size_win, *step_win, *slider_win;
	struct dialog dialog;

	CHECK_PTR(start);
	CHECK_PTR(end);

	ctl.spaces = blocks;
	ctl.nspaces = nblocks;
	ctl.length = length;
	ctl.start = start;
	ctl.end = end;
	ctl.step = 1;

	if (prepare_dialog(conf, text, rows, cols, &dialog) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&dialog, true, OK_LABEL, CANCEL_LABEL);

	if ((start_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for start");
	wbkgd(start_win, t.dialog.color);

	if ((end_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for end");
	wbkgd(end_win, t.dialog.color);

	if ((step_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for step");
	wbkgd(step_win, t.dialog.color);

	if ((size_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for size");
	wbkgd(size_win, t.dialog.color);

	if ((slider_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for slider");
	wbkgd(slider_win, t.dialog.color);

	if (slider_draw(&dialog, false, start_win, end_win, size_win, step_win,
	    slider_win, unit) != 0)
		return (BSDDIALOG_ERROR);

	sel = NULLWIN;
	loop = focusbuttons = true;
	while (loop) {
		size = *(ctl.end) - *(ctl.start) + 1;
		drawsquare(conf, start_win, RAISED, sel == START_WIN, "%15lu", *start); 
		drawsquare(conf, end_win, RAISED, sel == END_WIN, "%15lu", *end);
		drawsquare(conf, size_win, RAISED, 0, "%15lu", size);
		drawsquare(conf, step_win, RAISED, sel == STEP_WIN, "%15d", ctl.step);
		print_slider(conf, slider_win, blocks, nblocks, length, start,
		    end, sel == SLIDER_WIN);
		doupdate();

		if (get_wch(&input) == ERR)
			continue;
		switch (input) {
		case KEY_ENTER:
		case 10: /* Enter */
			if (focusbuttons || conf->button.always_active) {
				retval = BUTTONVALUE(dialog.bs);
				loop = false;
			}
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case '\t': /* TAB */
			if (focusbuttons) {
				dialog.bs.curr++;
				if (dialog.bs.curr >= (int)dialog.bs.nbuttons) {
					focusbuttons = false;
					sel = START_WIN;
					dialog.bs.curr =
					    conf->button.always_active ? 0 : -1;
				}
			} else {
				sel++;
				if ((sel + 1) > NWIN) {
					focusbuttons = true;
					sel = NULLWIN;
					dialog.bs.curr = 0;
				}
			}
			DRAW_BUTTONS(dialog);
			break;
		case KEY_CTRL('n'):
		case KEY_RIGHT:
			if (focusbuttons) {
				dialog.bs.curr++;
				if (dialog.bs.curr >= (int)dialog.bs.nbuttons) {
					focusbuttons = false;
					sel = START_WIN;
					dialog.bs.curr =
					    conf->button.always_active ? 0 : -1;
				}
			} else if (sel == SLIDER_WIN) {
				ctl.op = MOVERIGHT;
				handlesliderctl(&ctl);
			} else {
				sel++;
			}
			DRAW_BUTTONS(dialog);
			break;
		case KEY_CTRL('p'):
		case KEY_LEFT:
			if (focusbuttons) {
				dialog.bs.curr--;
				if (dialog.bs.curr < 0) {
					focusbuttons = false;
					sel = SLIDER_WIN;
					dialog.bs.curr =
					    conf->button.always_active ? 0 : -1;
				}
			} else if (sel == SLIDER_WIN) {
				ctl.op = MOVELEFT;
				handlesliderctl(&ctl);
			} else if (sel == END_WIN) {
				sel = START_WIN;
			} else {
				focusbuttons = true;
				sel = NULLWIN;
				dialog.bs.curr = 0;
			}
			DRAW_BUTTONS(dialog);
			break;
		case KEY_UP:
			if (focusbuttons) {
				sel = SLIDER_WIN;
				focusbuttons = false;
				dialog.bs.curr =
				     conf->button.always_active ? 0 : -1;
				DRAW_BUTTONS(dialog);
			} else if (sel == START_WIN) {
				if (resize) {
					ctl.op = INCREASELEFT;
				} else {
					ctl.op = MOVERIGHT;
				}
				handlesliderctl(&ctl);
			} else if (sel == END_WIN) {
				if (resize) {
					ctl.op = INCREASERIGHT;
				} else {
					ctl.op = MOVERIGHT;
				}
				handlesliderctl(&ctl);
			} else if (sel == STEP_WIN) {
				ctl.op = INCREASESTEP;
				handlesliderctl(&ctl);
			}
			break;
		case KEY_DOWN:
			if (focusbuttons) {
				break;
			} else if (sel == START_WIN) {
				if (resize) {
					ctl.op = DECREASELEFT;
				} else {
					ctl.op = MOVELEFT;
				}
				handlesliderctl(&ctl);
			} else if (sel == END_WIN) {
				if (resize) {
					ctl.op = DECREASERIGHT;
				} else {
					ctl.op = MOVELEFT;
				}
				handlesliderctl(&ctl);
			} else if (sel == STEP_WIN) {
				ctl.op = DECREASESTEP;
				handlesliderctl(&ctl);
			}
			break;
		case '-':
			if (focusbuttons) {
				break;
			} else if (sel == START_WIN) {
				if (resize) {
					ctl.op = DECREASELEFT;
				} else {
					ctl.op = MOVELEFT;
				}
				handlesliderctl(&ctl);
			} else if (sel == END_WIN) {
				if (resize) {
					ctl.op = DECREASERIGHT;
				} else {
					ctl.op = MOVELEFT;
				}
				handlesliderctl(&ctl);
			} else if (sel == STEP_WIN) {
				ctl.op = DECREASESTEP;
				handlesliderctl(&ctl);
			}
			break;
		case '+':
			if (focusbuttons) {
				break;
			} else if (sel == START_WIN) {
				if (resize) {
					ctl.op = INCREASELEFT;
				} else {
					ctl.op = MOVERIGHT;
				}
				handlesliderctl(&ctl);
			} else if (sel == END_WIN) {
				if (resize) {
					ctl.op = INCREASERIGHT;
				} else {
					ctl.op = MOVERIGHT;
				}
				handlesliderctl(&ctl);
			} else if (sel == STEP_WIN) {
				ctl.op = INCREASESTEP;
				handlesliderctl(&ctl);
			}
			break;
		case KEY_HOME:
			if (focusbuttons) {
				break;
			} else if (sel == SLIDER_WIN) {
				ctl.op = MOVEFARLEFT;
				handlesliderctl(&ctl);
			}
			break;
		case KEY_END:
			if (focusbuttons) {
				break;
			} else if (sel == SLIDER_WIN) {
				ctl.op = MOVEFARRIGHT;
				handlesliderctl(&ctl);
			}
			break;
		case KEY_PPAGE:
			if (focusbuttons) {
				break;
			} else if (sel == SLIDER_WIN) {
				ctl.op = MOVEFASTLEFT;
				handlesliderctl(&ctl);
			}
			break;
		case KEY_NPAGE:
			if (focusbuttons) {
				break;
			} else if (sel == SLIDER_WIN) {
				ctl.op = MOVEFASTRIGHT;
				handlesliderctl(&ctl);
			}
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(conf) != 0)
				return (BSDDIALOG_ERROR);
			if (slider_draw(&dialog, true, start_win, end_win, size_win,
			    step_win, slider_win, unit) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_CTRL('l'):
		case KEY_RESIZE:
			if (slider_draw(&dialog, true, start_win, end_win, size_win,
			    step_win, slider_win, unit) != 0)
				return (BSDDIALOG_ERROR);
			break;
		default:
			if (shortcut_buttons(input, &dialog.bs)) {
				DRAW_BUTTONS(dialog);
				doupdate();
				retval = BUTTONVALUE(dialog.bs);
				loop = false;
			}
		}
	}

	delwin(start_win);
	delwin(end_win);
	delwin(step_win);
	delwin(slider_win);
	end_dialog(&dialog);

	return (retval);
}
