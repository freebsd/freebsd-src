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

#include <ctype.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bsddialog.h"
#include "bsddialog_progressview.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define BARPADDING     2
#define MINBARLEN      15
#define MINBARWIDTH    (2 + 2 * BARPADDING + MINBARLEN)
#define MINMGBARLEN    18
#define MINMGBARWIDTH  (2 + 2 * BARPADDING + MINMGBARLEN)

bool bsddialog_interruptprogview;
bool bsddialog_abortprogview;
int  bsddialog_total_progview;

static void
draw_bar(WINDOW *win, int y, int x, int barlen, int perc, bool withlabel,
    int label)
{
	int i, blue_x, color, stringlen;
	char labelstr[128];

	blue_x = perc > 0 ? (perc * barlen) / 100 : -1;

	wmove(win, y, x);
	for (i = 0; i < barlen; i++) {
		color = (i <= blue_x) ? t.bar.f_color : t.bar.color;
		wattron(win, color);
		waddch(win, ' ');
		wattroff(win, color);
	}

	if (withlabel)
		sprintf(labelstr, "%d", label);
	else
		sprintf(labelstr, "%3d%%", perc);
	stringlen = (int)strlen(labelstr);
	wmove(win, y, x + barlen/2 - stringlen/2);
	for (i = 0; i < stringlen; i++) {
		color = (blue_x + 1 <= barlen/2 - stringlen/2 + i ) ?
		    t.bar.color : t.bar.f_color;
		wattron(win, color);
		waddch(win, labelstr[i]);
		wattroff(win, color);
	}
}

static int
bar_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    const char *text, struct buttons *bs)
{
	int htext, wtext;

	if (cols == BSDDIALOG_AUTOSIZE || rows == BSDDIALOG_AUTOSIZE) {
		if (text_size(conf, rows, cols, text, bs, 3, MINBARWIDTH,
		    &htext, &wtext) != 0)
			return (BSDDIALOG_ERROR);
	}

	if (cols == BSDDIALOG_AUTOSIZE)
		*w = widget_min_width(conf, wtext, MINBARWIDTH, bs);

	if (rows == BSDDIALOG_AUTOSIZE)
		*h = widget_min_height(conf, htext, 3 /* bar */, bs != NULL);

	return (0);
}

static int
bar_checksize(int rows, int cols, struct buttons *bs)
{
	int minheight, minwidth;

	minwidth = 0;
	if (bs != NULL) /* gauge has not buttons */
		minwidth = buttons_width(*bs);

	minwidth = MAX(minwidth, MINBARWIDTH);
	minwidth += VBORDERS;

	if (cols < minwidth)
		RETURN_ERROR("Few cols to draw bar and/or buttons");

	minheight = HBORDERS + 3;
	if (bs != NULL)
		minheight += 2;
	if (rows < minheight)
		RETURN_ERROR("Few rows to draw bar");

	return (0);
}

int
bsddialog_gauge(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int perc, int fd, const char *sep)
{
	bool mainloop;
	int y, x, h, w, fd2;
	FILE *input;
	WINDOW *widget, *textpad, *bar, *shadow;
	char inputbuf[2048], ntext[2048], *pntext;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (bar_autosize(conf, rows, cols, &h, &w, text, NULL) != 0)
		return (BSDDIALOG_ERROR);
	if (bar_checksize(h, w, NULL) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, NULL,
	    false) != 0)
		return (BSDDIALOG_ERROR);

	bar = new_boxed_window(conf, y+h-4, x+3, 3, w-6, RAISED);

	mainloop = (fd < 0) ? false : true;

	if (mainloop) {
		fd2 = dup(fd);
		input = fdopen(fd2, "r");
		if (input == NULL)
			RETURN_ERROR("Cannot build FILE* from fd");
	} else
		input = NULL;

	while (mainloop) {
		wrefresh(widget);
		prefresh(textpad, 0, 0, y+1, x+1+TEXTHMARGIN, y+h-4,
		    x+w-1-TEXTHMARGIN);
		draw_borders(conf, bar, 3, w-6, RAISED);
		draw_bar(bar, 1, 1, w-8, perc, false, -1 /*unused*/);
		wrefresh(bar);

		while (true) {
			fscanf(input, "%s", inputbuf);
			if (strcmp(inputbuf,"EOF") == 0) {
				mainloop = false;
				break;
			}
			if (strcmp(inputbuf, sep) == 0)
				break;
		}
		if (mainloop == false)
			break;
		fscanf(input, "%d", &perc);
		perc = perc > 100 ? 100 : perc;
		pntext = &ntext[0];
		ntext[0] = '\0';
		while (true) {
			fscanf(input, "%s", inputbuf);
			if (strcmp(inputbuf,"EOF") == 0) {
				mainloop = false;
				break;
			}
			if (strcmp(inputbuf, sep) == 0)
				break;
			strcpy(pntext, inputbuf);
			pntext += strlen(inputbuf);
			pntext[0] = ' ';
			pntext++;
		}
		if (update_dialog(conf, shadow, widget, y, x, h, w, textpad,
		    ntext, NULL, false) != 0)
			return (BSDDIALOG_ERROR);
	}

	if (input != NULL)
		fclose(input);
	delwin(bar);
	end_dialog(conf, shadow, widget, textpad);

	return (BSDDIALOG_OK);
}

/* Mixedgauge */
static int
do_mixedgauge(struct bsddialog_conf *conf, const char *text, int rows, int cols,
    unsigned int mainperc, unsigned int nminibars, const char **minilabels,
    int *minipercs, bool color)
{
	int i, output, miniperc, y, x, h, w, ypad, max_minbarlen;
	int htextpad, htext, wtext;
	int colorperc, red, green;
	WINDOW *widget, *textpad, *bar, *shadow;
	char states[12][14] = {
		"  Succeeded  ", /* -1  */
		"   Failed    ", /* -2  */
		"   Passed    ", /* -3  */
		"  Completed  ", /* -4  */
		"   Checked   ", /* -5  */
		"    Done     ", /* -6  */
		"   Skipped   ", /* -7  */
		" In Progress ", /* -8  */
		"(blank)      ", /* -9  */
		"     N/A     ", /* -10 */
		"   Pending   ", /* -11 */
		"   UNKNOWN   ", /* < -11, no API */
	};

	red   = bsddialog_color(BSDDIALOG_WHITE,BSDDIALOG_RED,  BSDDIALOG_BOLD);
	green = bsddialog_color(BSDDIALOG_WHITE,BSDDIALOG_GREEN,BSDDIALOG_BOLD);

	max_minbarlen = 0;
	for (i = 0; i < (int)nminibars; i++)
		max_minbarlen = MAX(max_minbarlen, (int)strlen(minilabels[i]));
	max_minbarlen += 3 + 16; /* seps + [...] */
	max_minbarlen = MAX(max_minbarlen, MINMGBARWIDTH); /* mainbar */

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);

	/* mixedgauge autosize */
	if (cols == BSDDIALOG_AUTOSIZE || rows == BSDDIALOG_AUTOSIZE) {
		if (text_size(conf, rows, cols, text, NULL, nminibars + 3,
		    max_minbarlen, &htext, &wtext) != 0)
			return (BSDDIALOG_ERROR);
	}
	if (cols == BSDDIALOG_AUTOSIZE)
		w = widget_min_width(conf, wtext, max_minbarlen, NULL);
	if (rows == BSDDIALOG_AUTOSIZE)
		h = widget_min_height(conf, htext, nminibars + 3, false);

	/* mixedgauge checksize */
	if (w < max_minbarlen + 2)
		RETURN_ERROR("Few cols for this mixedgauge");
	if (h < 5 + (int)nminibars)
		RETURN_ERROR("Few rows for this mixedgauge");

	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	output = new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text,
	    NULL, false);
	if (output == BSDDIALOG_ERROR)
		return (output);

	/* mini bars */
	for (i = 0; i < (int)nminibars; i++) {
		miniperc = minipercs[i];
		if (miniperc == BSDDIALOG_MG_BLANK)
			continue;
		/* label */
		if (color && (miniperc >= 0))
			wattron(widget, A_BOLD);
		mvwaddstr(widget, i+1, 2, minilabels[i]);
			wattroff(widget, A_BOLD);
		/* perc */
		if (miniperc < -11)
			mvwaddstr(widget, i+1, w-2-15, states[11]);
		else if (miniperc < 0) {
			mvwaddstr(widget, i+1, w-2-15, "[             ]");
			colorperc = -1;
			if (color && miniperc == BSDDIALOG_MG_FAILED)
				colorperc = red;
			if (color && miniperc == BSDDIALOG_MG_DONE)
				colorperc = green;
			if (colorperc != -1)
				wattron(widget, colorperc);
			miniperc = abs(miniperc + 1);
			mvwaddstr(widget, i+1, 1+w-2-15, states[miniperc]);
			if (colorperc != -1)
				wattroff(widget, colorperc);
		}
		else { /* miniperc >= 0 */
			if (miniperc > 100)
				miniperc = 100;
			mvwaddstr(widget, i+1, w-2-15, "[             ]");
			draw_bar(widget, i+1, 1+w-2-15, 13, miniperc, false,
			    -1 /*unused*/);
		}
	}

	wrefresh(widget);
	getmaxyx(textpad, htextpad, i /* unused */);
	ypad =  y + h - 4 - htextpad;
	ypad = ypad < y+(int)nminibars ? y+(int)nminibars : ypad;
	prefresh(textpad, 0, 0, ypad, x+2, y+h-4, x+w-2);

	/* main bar */
	bar = new_boxed_window(conf, y+h -4, x+3, 3, w-6, RAISED);

	draw_bar(bar, 1, 1, w-8, mainperc, false, -1 /*unused*/);

	wattron(bar, t.bar.color);
	mvwaddstr(bar, 0, 2, "Overall Progress");
	wattroff(bar, t.bar.color);

	wrefresh(bar);

	/* getch(); port ncurses shows nothing */

	delwin(bar);
	end_dialog(conf, shadow, widget, textpad);

	return (BSDDIALOG_OK);
}

int
bsddialog_mixedgauge(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int mainperc, unsigned int nminibars,
    const char **minilabels, int *minipercs)
{
	int output;

	output = do_mixedgauge(conf, text, rows, cols, mainperc, nminibars,
	    minilabels, minipercs, false);

	return (output);
}

int
bsddialog_progressview (struct bsddialog_conf *conf, const char *text, int rows,
    int cols, struct bsddialog_progviewconf *pvconf, unsigned int nminibar,
    struct bsddialog_fileminibar *minibar)
{
	bool update;
	int perc, output, *minipercs;
	unsigned int i, mainperc, totaltodo;
	float readforsec;
	const char **minilabels;
	time_t tstart, told, tnew, refresh;

	if ((minilabels = calloc(nminibar, sizeof(char*))) == NULL)
		RETURN_ERROR("Cannot allocate memory for minilabels");
	if ((minipercs = calloc(nminibar, sizeof(int))) == NULL)
		RETURN_ERROR("Cannot allocate memory for minipercs");

	totaltodo = 0;
	for (i = 0; i < nminibar; i++) {
		totaltodo += minibar[i].size;
		minilabels[i] = minibar[i].label;
		minipercs[i] = minibar[i].status;
	}

	refresh = pvconf->refresh == 0 ? 0 : pvconf->refresh - 1;
	output = BSDDIALOG_OK;
	i = 0;
	update = true;
	time(&told);
	tstart = told;
	while (!(bsddialog_interruptprogview || bsddialog_abortprogview)) {
		if (bsddialog_total_progview == 0 || totaltodo == 0)
			mainperc = 0;
		else
			mainperc = (bsddialog_total_progview * 100) / totaltodo;

		time(&tnew);
		if (update || tnew > told + refresh) {
			output = do_mixedgauge(conf, text, rows, cols, mainperc,
			    nminibar, minilabels, minipercs, true);
			if (output == BSDDIALOG_ERROR)
				return (BSDDIALOG_ERROR);

			move(SCREENLINES - 1, 2);
			clrtoeol();
			readforsec = ((tnew - tstart) == 0) ? 0 :
			    bsddialog_total_progview / (float)(tnew - tstart);
			printw(pvconf->fmtbottomstr, bsddialog_total_progview,
			    readforsec);
			refresh();

			time(&told);
			update = false;
		}

		if (i >= nminibar)
			break;
		if (minibar[i].status == BSDDIALOG_MG_FAILED)
			break;

		perc = pvconf->callback(&minibar[i]);

		if (minibar[i].status == BSDDIALOG_MG_DONE) { /*||perc >= 100)*/
			minipercs[i] = BSDDIALOG_MG_DONE;
			update = true;
			i++;
		} else if (minibar[i].status == BSDDIALOG_MG_FAILED || perc < 0) {
			minipercs[i] = BSDDIALOG_MG_FAILED;
			update = true;
		} else /* perc >= 0 */
			minipercs[i] = perc;
	}

	free(minilabels);
	free(minipercs);
	return (output);
}

int
bsddialog_rangebox(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, int min, int max, int *value)
{
	bool loop, buttupdate, barupdate;
	int y, x, h, w;
	int input, currvalue, output, sizebar, bigchange, positions;
	float perc;
	WINDOW *widget, *textpad, *bar, *shadow;
	struct buttons bs;

	if (value == NULL)
		RETURN_ERROR("*value cannot be NULL");

	if (min >= max)
		RETURN_ERROR("min >= max");

	currvalue = *value;
	positions = max - min + 1;

	get_buttons(conf, &bs, BUTTON_OK_LABEL, BUTTON_CANCEL_LABEL);

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (bar_autosize(conf, rows, cols, &h, &w, text, &bs) != 0)
		return (BSDDIALOG_ERROR);
	if (bar_checksize(h, w, &bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, &bs,
	    true) != 0)
		return (BSDDIALOG_ERROR);

	doupdate();

	prefresh(textpad, 0, 0, y+1, x+1+TEXTHMARGIN, y+h-7, x+w-1-TEXTHMARGIN);

	sizebar = w - HBORDERS - (2 * BARPADDING) - 2;
	bigchange = MAX(1, sizebar/10);

	bar = new_boxed_window(conf, y + h - 6, x + 1 + BARPADDING, 3,
	    sizebar + 2, RAISED);

	loop = buttupdate = barupdate = true;
	while (loop) {
		if (buttupdate) {
			draw_buttons(widget, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}
		if (barupdate) {
			perc = ((float)(currvalue - min)*100) / (positions-1);
			draw_bar(bar, 1, 1, sizebar, perc, true, currvalue);
			barupdate = false;
			wrefresh(bar);
		}

		input = getch();
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			output = bs.value[bs.curr];
			*value = currvalue;
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				output = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			buttupdate = true;
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				buttupdate = true;
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				buttupdate = true;
			}
			break;
		case KEY_HOME:
			currvalue = max;
			barupdate = true;
			break;
		case KEY_END:
			currvalue = min;
			barupdate = true;
			break;
		case KEY_NPAGE:
			currvalue -= bigchange;
			if (currvalue < min)
				currvalue = min;
			barupdate = true;
			break;
		case KEY_PPAGE:
			currvalue += bigchange;
			if (currvalue > max)
				currvalue = max;
			barupdate = true;
			break;
		case KEY_UP:
			if (currvalue < max) {
				currvalue++;
				barupdate = true;
			}
			break;
		case KEY_DOWN:
			if (currvalue > min) {
				currvalue--;
				barupdate = true;
			}
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
			if (bar_autosize(conf, rows, cols, &h, &w, text,
			    &bs) != 0)
				return (BSDDIALOG_ERROR);
			if (bar_checksize(h, w, &bs) != 0)
				return (BSDDIALOG_ERROR);
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return (BSDDIALOG_ERROR);

			if (update_dialog(conf, shadow, widget,y, x, h, w,
			    textpad, text, &bs, true) != 0)
				return (BSDDIALOG_ERROR);

			doupdate();

			sizebar = w - HBORDERS - (2 * BARPADDING) - 2;
			bigchange = MAX(1, sizebar/10);
			wclear(bar);
			mvwin(bar, y + h - 6, x + 1 + BARPADDING);
			wresize(bar, 3, sizebar + 2);
			draw_borders(conf, bar, 3, sizebar+2, RAISED);

			prefresh(textpad, 0, 0, y+1, x+1+TEXTHMARGIN, y+h-7,
			    x+w-1-TEXTHMARGIN);

			barupdate = true;
			break;
		default:
			if (shortcut_buttons(input, &bs)) {
				output = bs.value[bs.curr];
				loop = false;
			}
		}
	}

	delwin(bar);
	end_dialog(conf, shadow, widget, textpad);

	return (output);
}

int
bsddialog_pause(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int sec)
{
	bool loop, buttupdate, barupdate;
	int output, y, x, h, w, input, tout, sizebar;
	float perc;
	WINDOW *widget, *textpad, *bar, *shadow;
	struct buttons bs;

	get_buttons(conf, &bs, BUTTON_OK_LABEL, BUTTON_CANCEL_LABEL);

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (bar_autosize(conf, rows, cols, &h, &w, text, &bs) != 0)
		return (BSDDIALOG_ERROR);
	if (bar_checksize(h, w, &bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, &bs,
	    true) != 0)
		return (BSDDIALOG_ERROR);

	doupdate();

	prefresh(textpad, 0, 0, y+1, x+1+TEXTHMARGIN, y+h-7, x+w-1-TEXTHMARGIN);

	sizebar = w - HBORDERS - (2 * BARPADDING) - 2;
	bar = new_boxed_window(conf, y + h - 6, x + 1 + BARPADDING, 3,
	    sizebar + 2, RAISED);

	tout = sec;
	nodelay(stdscr, TRUE);
	timeout(1000);
	loop = buttupdate = barupdate = true;
	while (loop) {
		if (barupdate) {
			perc = (float)tout * 100 / sec;
			draw_bar(bar, 1, 1, sizebar, perc, true, tout);
			barupdate = false;
			wrefresh(bar);
		}

		if (buttupdate) {
			draw_buttons(widget, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}

		input = getch();
		if (input < 0) { /* timeout */
			tout--;
			if (tout < 0) {
				output = BSDDIALOG_TIMEOUT;
				break;
			}
			else {
				barupdate = true;
				continue;
			}
		}
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			output = bs.value[bs.curr];
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				output = BSDDIALOG_ESC;
				loop = false;
			}
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			buttupdate = true;
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				buttupdate = true;
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				buttupdate = true;
			}
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
			if (bar_autosize(conf, rows, cols, &h, &w, text,
			    &bs) != 0)
				return (BSDDIALOG_ERROR);
			if (bar_checksize(h, w, &bs) != 0)
				return (BSDDIALOG_ERROR);
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return (BSDDIALOG_ERROR);

			if (update_dialog(conf, shadow, widget,y, x, h, w,
			    textpad, text, &bs, true) != 0)
				return (BSDDIALOG_ERROR);

			doupdate();

			sizebar = w - HBORDERS - (2 * BARPADDING) - 2;
			wclear(bar);
			mvwin(bar, y + h - 6, x + 1 + BARPADDING);
			wresize(bar, 3, sizebar + 2);
			draw_borders(conf, bar, 3, sizebar+2, LOWERED);

			prefresh(textpad, 0, 0, y+1, x+1+TEXTHMARGIN, y+h-7,
			    x+w-1-TEXTHMARGIN);

			barupdate = true;
			break;
		default:
			if (shortcut_buttons(input, &bs)) {
				output = bs.value[bs.curr];
				loop = false;
			}
		}
	}

	nodelay(stdscr, FALSE);

	delwin(bar);
	end_dialog(conf, shadow, widget, textpad);

	return (output);
}