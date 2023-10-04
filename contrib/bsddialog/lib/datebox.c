/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022-2023 Alfonso Sabato Siciliano
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

/* Calendar */
#define MIN_YEAR_CAL   0
#define MAX_YEAR_CAL   999999999
#define MINHCAL        13
#define MINWCAL        36 /* 34 calendar, 1 + 1 margins */
/* Datebox */
#define MIN_YEAR_DATE  0
#define MAX_YEAR_DATE  9999
#define MINWDATE       23 /* 3 windows and their borders */

#define ISLEAP(year) ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)

static int minyear;
static int maxyear;

static const char *m[12] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

enum operation {
	UP_DAY,
	DOWN_DAY,
	LEFT_DAY,
	RIGHT_DAY,
	UP_MONTH,
	DOWN_MONTH,
	UP_YEAR,
	DOWN_YEAR
};

/* private datebox item */
struct dateitem {
	enum operation up;
	enum operation down;
	WINDOW *win;
	int width;
	const char *fmt;
	int *value;
};

static int month_days(int yy, int mm)
{
	int days;

	if (mm == 2)
		days = ISLEAP(yy) ? 29 : 28;
	else if (mm == 4 || mm == 6 || mm == 9 || mm == 11)
		days = 30;
	else
		days = 31;

	return (days);
}

static int week_day(int yy, int mm, int dd)
{
	int wd;

	dd += mm < 3 ? yy-- : yy - 2;
	wd = 23*mm/9 + dd + 4 + yy/4 - yy/100 + yy/400;
	wd %= 7;

	return (wd);
}

static void
init_date(unsigned int *year, unsigned int *month, unsigned int *day, int *yy,
    int *mm, int *dd)
{
	*yy = MIN(*year, (unsigned int)maxyear);
	if (*yy < minyear)
		*yy = minyear;
	*mm = MIN(*month, 12);
	if (*mm == 0)
		*mm = 1;
	*dd = (*day == 0) ? 1 : *day;
	if(*dd > month_days(*yy, *mm))
		*dd = month_days(*yy, *mm);
}

static void datectl(enum operation op, int *yy, int *mm, int *dd)
{
	int ndays;

	ndays = month_days(*yy, *mm);

	switch (op) {
	case UP_DAY:
		if (*dd > 7)
			*dd -= 7;
		else {
			if (*mm == 1) {
				*yy -= 1;
				*mm = 12;
			} else
				*mm -= 1;
			ndays = month_days(*yy, *mm);
			*dd = ndays - abs(7 - *dd);
		}
		break;
	case DOWN_DAY:
		if (*dd + 7 < ndays)
			*dd += 7;
		else {
			if (*mm == 12) {
				*yy += 1;
				*mm = 1;
			} else
				*mm += 1;
			*dd = *dd + 7 - ndays;
		}
		break;
	case LEFT_DAY:
		if (*dd > 1)
			*dd -= 1;
		else {
			if (*mm == 1) {
				*yy -= 1;
				*mm = 12;
			} else
				*mm -= 1;
			*dd = month_days(*yy, *mm);
		}
		break;
	case RIGHT_DAY:
		if (*dd < ndays)
			*dd += 1;
		else {
			if (*mm == 12) {
				*yy += 1;
				*mm = 1;
			} else
				*mm += 1;
			*dd = 1;
		}
		break;
	case UP_MONTH:
		if (*mm == 1) {
			*mm = 12;
			*yy -= 1;
		} else
			*mm -= 1;
		ndays = month_days(*yy, *mm);
		if (*dd > ndays)
			*dd = ndays;
		break;
	case DOWN_MONTH:
		if (*mm == 12) {
			*mm = 1;
			*yy += 1;
		} else
			*mm += 1;
		ndays = month_days(*yy, *mm);
		if (*dd > ndays)
			*dd = ndays;
		break;
	case UP_YEAR:
		*yy -= 1;
		ndays = month_days(*yy, *mm);
		if (*dd > ndays)
			*dd = ndays;
		break;
	case DOWN_YEAR:
		*yy += 1;
		ndays = month_days(*yy, *mm);
		if (*dd > ndays)
			*dd = ndays;
		break;
	}

	if (*yy < minyear) {
		*yy = minyear;
		*mm = 1;
		*dd = 1;
	}
	if (*yy > maxyear) {
		*yy = maxyear;
		*mm = 12;
		*dd = 31;
	}
}

static void
drawsquare(struct bsddialog_conf *conf, WINDOW *win, enum elevation elev,
    const char *fmt, int value, bool focus)
{
	int h, l, w;

	getmaxyx(win, h, w);
	draw_borders(conf, win, elev);
	if (focus) {
		l = 2 + w%2;
		wattron(win, t.dialog.arrowcolor);
		mvwhline(win, 0, w/2 - l/2,
		    conf->ascii_lines ? '^' : ACS_UARROW, l);
		mvwhline(win, h-1, w/2 - l/2,
		    conf->ascii_lines ? 'v' : ACS_DARROW, l);
		wattroff(win, t.dialog.arrowcolor);
	}

	if (focus)
		wattron(win, t.menu.f_namecolor);
	if (strchr(fmt, 's') != NULL)
		mvwprintw(win, 1, 1, fmt, m[value - 1]);
	else
		mvwprintw(win, 1, 1, fmt, value);
	if (focus)
		wattroff(win, t.menu.f_namecolor);

	wnoutrefresh(win);
}

static void
print_calendar(struct bsddialog_conf *conf, WINDOW *win, int yy, int mm, int dd,
    bool active)
{
	int ndays, i, y, x, wd, h, w;

	getmaxyx(win, h, w);
	wclear(win);
	draw_borders(conf, win, RAISED);
	if (active) {
		wattron(win, t.dialog.arrowcolor);
		mvwhline(win, 0, 15, conf->ascii_lines ? '^' : ACS_UARROW, 4);
		mvwhline(win, h-1, 15, conf->ascii_lines ? 'v' : ACS_DARROW, 4);
		mvwvline(win, 3, 0, conf->ascii_lines ? '<' : ACS_LARROW, 3);
		mvwvline(win, 3, w-1, conf->ascii_lines ? '>' : ACS_RARROW, 3);
		wattroff(win, t.dialog.arrowcolor);
	}

	mvwaddstr(win, 1, 5, "Sun Mon Tue Wed Thu Fri Sat");
	ndays = month_days(yy, mm);
	y = 2;
	wd = week_day(yy, mm, 1);
	for (i = 1; i <= ndays; i++) {
		x = 5 + (4 * wd); /* x has to be 6 with week number */
		wmove(win, y, x);
		mvwprintw(win, y, x, "%2d", i);
		if (i == dd) {
			wattron(win, t.menu.f_namecolor);
			mvwprintw(win, y, x, "%2d", i);
			wattroff(win, t.menu.f_namecolor);
		}
		wd++;
		if (wd > 6) {
			wd = 0;
			y++;
		}
	}

	wnoutrefresh(win);
}

static int
calendar_redraw(struct dialog *d, WINDOW *yy_win, WINDOW *mm_win,
    WINDOW *dd_win)
{
	int ycal, xcal;

	if (d->built) {
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	if (dialog_size_position(d, MINHCAL, MINWCAL, NULL) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0)
		return (BSDDIALOG_ERROR);
	if (d->built)
		refresh(); /* Important to fix grey lines expanding screen */
	TEXTPAD(d, MINHCAL + HBUTTONS);

	ycal = d->y + d->h - 15;
	xcal = d->x + d->w/2 - 17;
	mvwaddstr(d->widget, d->h - 16, d->w/2 - 17, "Month");
	update_box(d->conf, mm_win, ycal, xcal, 3, 17, RAISED);
	mvwaddstr(d->widget, d->h - 16, d->w/2, "Year");
	update_box(d->conf, yy_win, ycal, xcal + 17, 3, 17, RAISED);
	update_box(d->conf, dd_win, ycal + 3, xcal, 9, 34, RAISED);
	wnoutrefresh(d->widget);

	return (0);
}

int
bsddialog_calendar(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int *year, unsigned int *month, unsigned int *day)
{
	bool loop, focusbuttons;
	int retval, sel, yy, mm, dd;
	wint_t input;
	WINDOW *yy_win, *mm_win, *dd_win;
	struct dialog d;

	CHECK_PTR(year);
	CHECK_PTR(month);
	CHECK_PTR(day);
	minyear = MIN_YEAR_CAL;
	maxyear = MAX_YEAR_CAL;
	init_date(year, month, day, &yy, &mm, &dd);

	if (prepare_dialog(conf, text, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&d, true, OK_LABEL, CANCEL_LABEL);
	if ((yy_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for yy");
	wbkgd(yy_win, t.dialog.color);
	if ((mm_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for mm");
	wbkgd(mm_win, t.dialog.color);
	if ((dd_win = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW for dd");
	wbkgd(dd_win, t.dialog.color);
	if (calendar_redraw(&d, yy_win, mm_win, dd_win) != 0)
		return (BSDDIALOG_ERROR);

	sel = -1;
	loop = focusbuttons = true;
	while (loop) {
		drawsquare(conf, mm_win, RAISED, "%15s", mm, sel == 0);
		drawsquare(conf, yy_win, RAISED, "%15d", yy, sel == 1);
		print_calendar(conf, dd_win, yy, mm, dd, sel == 2);
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
		case '\t': /* TAB */
			if (focusbuttons) {
				d.bs.curr++;
				if (d.bs.curr >= (int)d.bs.nbuttons) {
					focusbuttons = false;
					sel = 0;
					d.bs.curr = conf->button.always_active ?
					    0 : -1;
				}
			} else {
				sel++;
				if (sel > 2) {
					focusbuttons = true;
					sel = -1;
					d.bs.curr = 0;
				}
			}
			DRAW_BUTTONS(d);
			break;
		case KEY_RIGHT:
			if (focusbuttons) {
				d.bs.curr++;
				if (d.bs.curr >= (int)d.bs.nbuttons) {
					focusbuttons = false;
					sel = 0;
					d.bs.curr = conf->button.always_active ?
					    0 : -1;
				}
			} else if (sel == 2) {
				datectl(RIGHT_DAY, &yy, &mm, &dd);
			} else { /* Month or Year*/
				sel++;
			}
			DRAW_BUTTONS(d);
			break;
		case KEY_LEFT:
			if (focusbuttons) {
				d.bs.curr--;
				if (d.bs.curr < 0) {
					focusbuttons = false;
					sel = 2;
					d.bs.curr = conf->button.always_active ?
					    0 : -1;
				}
			} else if (sel == 2) {
				datectl(LEFT_DAY, &yy, &mm, &dd);
			} else if (sel == 1) {
				sel = 0;
			} else { /* sel = 0, Month */
				focusbuttons = true;
				sel = -1;
				d.bs.curr = 0;
			}
			DRAW_BUTTONS(d);
			break;
		case KEY_UP:
			if (focusbuttons) {
				sel = 2;
				focusbuttons = false;
				d.bs.curr = conf->button.always_active ? 0 : -1;
				DRAW_BUTTONS(d);
			} else if (sel == 0) {
				datectl(UP_MONTH, &yy, &mm, &dd);
			} else if (sel == 1) {
				datectl(UP_YEAR, &yy, &mm, &dd);
			} else { /* sel = 2 */
				datectl(UP_DAY, &yy, &mm, &dd);
			}
			break;
		case KEY_DOWN:
			if (focusbuttons) {
				break;
			} else if (sel == 0) {
				datectl(DOWN_MONTH, &yy, &mm, &dd);
			} else if (sel == 1) {
				datectl(DOWN_YEAR, &yy, &mm, &dd);
			} else { /* sel = 2 */
				datectl(DOWN_DAY, &yy, &mm, &dd);
			}
			break;
		case KEY_HOME:
			datectl(UP_MONTH, &yy, &mm, &dd);
			break;
		case KEY_END:
			datectl(DOWN_MONTH, &yy, &mm, &dd);
			break;
		case KEY_PPAGE:
			datectl(UP_YEAR, &yy, &mm, &dd);
			break;
		case KEY_NPAGE:
			datectl(DOWN_YEAR, &yy, &mm, &dd);
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(conf) != 0)
				return (BSDDIALOG_ERROR);
			if (calendar_redraw(&d, yy_win, mm_win, dd_win) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_RESIZE:
			if (calendar_redraw(&d, yy_win, mm_win, dd_win) != 0)
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

	*year  = yy;
	*month = mm;
	*day   = dd;

	delwin(yy_win);
	delwin(mm_win);
	delwin(dd_win);
	end_dialog(&d);

	return (retval);
}

static int datebox_redraw(struct dialog *d, struct dateitem *di)
{
	int y, x;

	if (d->built) {
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	if (dialog_size_position(d, 3 /*windows*/, MINWDATE, NULL) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0)
		return (BSDDIALOG_ERROR);
	if (d->built)
		refresh(); /* Important to fix grey lines expanding screen */
	TEXTPAD(d, 3 /*windows*/ + HBUTTONS);

	y = d->y + d->h - 6;
	x = (d->x + d->w / 2) - 11;
	update_box(d->conf, di[0].win, y, x, 3, di[0].width, LOWERED);
	mvwaddch(d->widget, d->h - 5, x - d->x + di[0].width, '/');
	x += di[0].width + 1;
	update_box(d->conf, di[1].win, y, x , 3, di[1].width, LOWERED);
	mvwaddch(d->widget, d->h - 5, x - d->x + di[1].width, '/');
	x += di[1].width + 1;
	update_box(d->conf, di[2].win, y, x, 3, di[2].width, LOWERED);
	wnoutrefresh(d->widget);

	return (0);
}

static int
build_dateitem(const char *format, int *yy, int *mm, int *dd,
    struct dateitem *dt)
{
	int i;
	wchar_t *wformat;
	struct dateitem init[3] = {
		{UP_YEAR,  DOWN_YEAR,  NULL, 6,  "%4d",  yy},
		{UP_MONTH, DOWN_MONTH, NULL, 11, "%9s",  mm},
		{LEFT_DAY, RIGHT_DAY,  NULL, 4,  "%02d", dd},
	};

	for (i = 0; i < 3; i++) {
		if ((init[i].win = newwin(1, 1, 1, 1)) == NULL)
			RETURN_FMTERROR("Cannot build WINDOW dateitem[%d]", i);
		wbkgd(init[i].win, t.dialog.color);
	}

	if ((wformat = alloc_mbstows(CHECK_STR(format))) == NULL)
		RETURN_ERROR("Cannot allocate conf.date.format in wchar_t*");
	if (format == NULL || wcscmp(wformat, L"d/m/y") == 0) {
		dt[0] = init[2];
		dt[1] = init[1];
		dt[2] = init[0];
	} else if (wcscmp(wformat, L"m/d/y") == 0) {
		dt[0] = init[1];
		dt[1] = init[2];
		dt[2] = init[0];
	} else if (wcscmp(wformat, L"y/m/d") == 0) {
		dt[0] = init[0];
		dt[1] = init[1];
		dt[2] = init[2];
	} else
		RETURN_FMTERROR("Invalid conf.date.format=\"%s\"", format);
	free(wformat);

	return (0);
}

int
bsddialog_datebox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int *year, unsigned int *month, unsigned int *day)
{
	bool loop, focusbuttons;
	int retval, i, sel, yy, mm, dd;
	wint_t input;
	struct dateitem  di[3];
	struct dialog d;

	CHECK_PTR(year);
	CHECK_PTR(month);
	CHECK_PTR(day);
	minyear = MIN_YEAR_DATE;
	maxyear = MAX_YEAR_DATE;
	init_date(year, month, day, &yy, &mm, &dd);

	if (prepare_dialog(conf, text, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&d, true, OK_LABEL, CANCEL_LABEL);
	if (build_dateitem(conf->date.format, &yy, &mm, &dd, di) != 0)
		return (BSDDIALOG_ERROR);
	if (datebox_redraw(&d, di) != 0)
		return (BSDDIALOG_ERROR);

	sel = -1;
	loop = focusbuttons = true;
	while (loop) {
		for (i = 0; i < 3; i++)
			drawsquare(conf, di[i].win, LOWERED, di[i].fmt,
			    *di[i].value, sel == i);
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
					d.bs.curr = conf->button.always_active ?
					    0 : -1;
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
					d.bs.curr = conf->button.always_active ?
					    0 : -1;
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
				datectl(di[sel].up, &yy, &mm, &dd);
			}
			break;
		case KEY_DOWN:
			if (focusbuttons)
				break;
			datectl(di[sel].down, &yy, &mm, &dd);
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(conf) != 0)
				return (BSDDIALOG_ERROR);
			if (datebox_redraw(&d, di) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_RESIZE:
			if (datebox_redraw(&d, di) != 0)
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

	*year  = yy;
	*month = mm;
	*day   = dd;

	for (i = 0; i < 3 ; i++)
		delwin(di[i].win);
	end_dialog(&d);

	return (retval);
}