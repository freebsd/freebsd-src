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
#include <stdlib.h>
#include <string.h>

#ifdef PORTNCURSES
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

#define BARMARGIN	3
#define MINBARWIDTH	10
#define MINWIDTH	(VBORDERS + MINBARWIDTH + BARMARGIN * 2)
#define MINHEIGHT	7 /* without text */

/* "Bar": gauge - mixedgauge - rangebox - pause */

extern struct bsddialog_theme t;

static void
draw_perc_bar(WINDOW *win, int y, int x, int size, int perc, bool withlabel,
    int label)
{
	char labelstr[128];
	int i, blue_x, color;

	blue_x = (int)((perc*(size))/100);

	wmove(win, y, x);
	for (i = 0; i < size; i++) {
		color = (i <= blue_x) ? t.bar.f_color : t.bar.color;
		wattron(win, color);
		waddch(win, ' ');
		wattroff(win, color);
	}

	if (withlabel)
		sprintf(labelstr, "%d", label);
	else
		sprintf(labelstr, "%3d%%", perc);
	wmove(win, y, x + size/2 - 2);
	for (i=0; i < (int) strlen(labelstr); i++) {
		color = (blue_x + 1 <= size/2 - (int)strlen(labelstr)/2 + i ) ?
		    t.bar.color : t.bar.f_color;
		wattron(win, color);
		waddch(win, labelstr[i]);
		wattroff(win, color);
	}
}

static int
bar_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    char *text, struct buttons *bs)
{
	int maxword, maxline, nlines, buttonswidth;

	if (get_text_properties(conf, text, &maxword, &maxline, &nlines) != 0)
		return BSDDIALOG_ERROR;

	buttonswidth = 0;
	if (bs != NULL) { /* gauge has not buttons */
		buttonswidth= bs->nbuttons * bs->sizebutton;
		if (bs->nbuttons > 0)
			buttonswidth += (bs->nbuttons-1) * t.button.space;
	}

	if (cols == BSDDIALOG_AUTOSIZE) {
		*w = VBORDERS;
		/* buttons size */
		*w += buttonswidth;
		/* bar size */
		*w = MAX(*w, MINWIDTH);
		/* text size*/
		*w = MAX((int)(maxline + VBORDERS + t.text.hmargin * 2), *w);
		/* conf.auto_minwidth */
		*w = MAX(*w, (int)conf->auto_minwidth);
		/* avoid terminal overflow */
		*w = MIN(*w, widget_max_width(conf));
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = MINHEIGHT;
		if (maxword > 0)
			*h += 1;
		/* conf.auto_minheight */
		*h = MAX(*h, (int)conf->auto_minheight);
		/* avoid terminal overflow */
		*h = MIN(*h, widget_max_height(conf));
	}

	return (0);
}

static int
bar_checksize(char *text, int rows, int cols, struct buttons *bs)
{
	int minheight, minwidth;

	minwidth = 0;
	if (bs != NULL) { /* gauge has not buttons */
		minwidth = bs->nbuttons * bs->sizebutton;
		if (bs->nbuttons > 0)
			minwidth += (bs->nbuttons-1) * t.button.space;
	}
	minwidth = MAX(minwidth + VBORDERS, MINBARWIDTH);

	if (cols< minwidth)
		RETURN_ERROR("Few cols for this widget");

	minheight = MINHEIGHT + ((text != NULL && strlen(text) > 0) ? 1 : 0);
	if (rows < minheight)
		RETURN_ERROR("Few rows for this mixedgauge");

	return 0;
}

int
bsddialog_gauge(struct bsddialog_conf *conf, char* text, int rows, int cols,
    unsigned int perc)
{
	WINDOW *widget, *textpad, *bar, *shadow;
	char input[2048], ntext[2048], *pntext;
	int y, x, h, w, htextpad;
	bool mainloop;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	if (bar_autosize(conf, rows, cols, &h, &w, text, NULL) != 0)
		return BSDDIALOG_ERROR;
	if (bar_checksize(text, h, w, NULL) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, false) != 0)
		return BSDDIALOG_ERROR;

	bar = new_boxed_window(conf, y+h-4, x+3, 3, w-6, RAISED);

	mainloop = true;
	while (mainloop) {
		wrefresh(widget);
		prefresh(textpad, 0, 0, y+1, x+1+t.text.hmargin, y+h-4,
		    x+w-1-t.text.hmargin);
		draw_perc_bar(bar, 1, 1, w-8, perc, false, -1 /*unused*/);
		wrefresh(bar);

		while (true) {
			scanf("%s", input);
			if (strcmp(input,"EOF") == 0) {
				mainloop = false;
				break;
			}
			if (strcmp(input,"XXX") == 0)
				break;
		}
		scanf("%d", &perc);
		perc = perc < 0 ? 0 : perc;
		perc = perc > 100 ? 100 : perc;
		htextpad = 1;
		wclear(textpad);
		pntext = &ntext[0];
		ntext[0] = '\0';
		while (true) {
			scanf("%s", input);
			if (strcmp(input,"EOF") == 0) {
				mainloop = false;
				break;
			}
			if (strcmp(input,"XXX") == 0)
				break;
			pntext[0] = ' ';
			pntext++;
			strcpy(pntext, input);
			pntext += strlen(input);
		}
		print_textpad(conf, textpad, &htextpad, w-2-t.text.hmargin*2,
		    ntext);
	}

	delwin(bar);
	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return BSDDIALOG_OK;
}

int
bsddialog_mixedgauge(struct bsddialog_conf *conf, char* text, int rows,
    int cols, unsigned int mainperc, unsigned int nminibars, char **minilabels,
    int *minipercs)
{
	WINDOW *widget, *textpad, *bar, *shadow;
	int i, output, miniperc, y, x, h, w, max_minbarlen;
	int maxword, maxline, nlines, htextpad, ypad;
	char states[12][16] = {
	    "[  Succeeded  ]", /*  0  */
	    "[   Failed    ]", /*  1  */
	    "[   Passed    ]", /*  2  */
	    "[  Completed  ]", /*  3  */
	    "[   Checked   ]", /*  4  */
	    "[    Done     ]", /*  5  */
	    "[   Skipped   ]", /*  6  */
	    "[ In Progress ]", /*  7  */
	    "(blank)        ", /*  8  */
	    "[     N/A     ]", /*  9  */
	    "[   Pending   ]", /* 10  */
	    "[   UNKNOWN   ]", /* 10+ */
	};

	max_minbarlen = 0;
	for (i=0; i < (int)nminibars; i++)
		max_minbarlen = MAX(max_minbarlen, (int)strlen(minilabels[i]));
	max_minbarlen += 3 + 16 /* seps + [...] or mainbar */;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;

	/* mixedgauge autosize */
	if (get_text_properties(conf, text, &maxword, &maxline, &nlines) != 0)
		return BSDDIALOG_ERROR;

	if (cols == BSDDIALOG_AUTOSIZE) {
		w = max_minbarlen + HBORDERS;
		w = MAX(max_minbarlen, maxline + 4);
		w = MAX(w, (int)conf->auto_minwidth);
		w = MIN(w, widget_max_width(conf) - 1);
	}
	if (rows == BSDDIALOG_AUTOSIZE) {
		h = 5; /* borders + mainbar */
		h += nminibars;
		h += (strlen(text) > 0 ? 3 : 0);
		h = MAX(h, (int)conf->auto_minheight);
		h = MIN(h, widget_max_height(conf) -1);
	}

	/* mixedgauge checksize */
	if (w < max_minbarlen + 2)
		RETURN_ERROR("Few cols for this mixedgauge");
	if (h < 5 + (int)nminibars + (strlen(text) > 0 ? 1 : 0))
		RETURN_ERROR("Few rows for this mixedgauge");

	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	output = new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w,
	    RAISED, &textpad, &htextpad, text, false);
	if (output == BSDDIALOG_ERROR)
		return output;

	/* mini bars */
	for (i=0; i < (int)nminibars; i++) {
		miniperc = minipercs[i];
		if (miniperc == 8)
			continue;
		mvwaddstr(widget, i+1, 2, minilabels[i]);
		if (miniperc > 10)
			mvwaddstr(widget, i+1, w-2-15, states[11]);
		else if (miniperc >= 0 && miniperc <= 10)
			mvwaddstr(widget, i+1, w-2-15, states[miniperc]);
		else { /* miniperc < 0 */
			miniperc = abs(miniperc);
			mvwaddstr(widget, i+1, w-2-15, "[             ]");
			draw_perc_bar(widget, i+1, 1+w-2-15, 13, miniperc,
			    false, -1 /*unused*/);
		}
	}

	wrefresh(widget);
	ypad =  y + h - 5 - htextpad;
	ypad = ypad < y+(int)nminibars ? y+nminibars : ypad;
	prefresh(textpad, 0, 0, ypad, x+2, y+h-4, x+w-2);
	
	/* main bar */
	bar = new_boxed_window(conf, y+h -4, x+3, 3, w-6, RAISED);
	
	draw_perc_bar(bar, 1, 1, w-8, mainperc, false, -1 /*unused*/);

	wattron(bar, t.bar.color);
	mvwaddstr(bar, 0, 2, "Overall Progress");
	wattroff(bar, t.bar.color);

	wrefresh(bar);

	/* getch(); port ncurses shows nothing */

	delwin(bar);
	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return BSDDIALOG_OK;
}

int
bsddialog_rangebox(struct bsddialog_conf *conf, char* text, int rows, int cols,
    int min, int max, int *value)
{
	WINDOW *widget, *textpad, *bar, *shadow;
	int i, y, x, h, w, htextpad;
	bool loop, buttupdate, barupdate;
	int input, currvalue, output, sizebar, bigchange, positions;
	float perc;
	struct buttons bs;

	if (value == NULL)
		RETURN_ERROR("*value cannot be NULL");

	if (min >= max)
		RETURN_ERROR("min >= max");

	currvalue = *value;
	positions = max - min + 1;

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	if (bar_autosize(conf, rows, cols, &h, &w, text, &bs) != 0)
		return BSDDIALOG_ERROR;
	if (bar_checksize(text, h, w, &bs) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, true) != 0)
		return BSDDIALOG_ERROR;

	prefresh(textpad, 0, 0, y+1, x+1+t.text.hmargin, y+h-7, 
			    x+w-1-t.text.hmargin);

	sizebar = w - HBORDERS - 2 - BARMARGIN * 2;
	bigchange = MAX(1, sizebar/10);

	bar = new_boxed_window(conf, y + h - 6, x + 1 + BARMARGIN, 3,
	    sizebar + 2, RAISED);

	loop = buttupdate = barupdate = true;
	while(loop) {
		if (buttupdate) {
			draw_buttons(widget, h-2, w, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}
		if (barupdate) {
			perc = ((float)(currvalue - min)*100) / (positions-1);
			draw_perc_bar(bar, 1, 1, sizebar, perc, true, currvalue);
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
			output = BSDDIALOG_ESC;
			loop = false;
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
			if (bar_autosize(conf, rows, cols, &h, &w, text, &bs) != 0)
				return BSDDIALOG_ERROR;
			if (bar_checksize(text, h, w, &bs) != 0)
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

			sizebar = w - HBORDERS - 2 - BARMARGIN * 2;
			bigchange = MAX(1, sizebar/10);
			wclear(bar);
			mvwin(bar, y + h - 6, x + 1 + BARMARGIN);
			wresize(bar, 3, sizebar + 2);

			if(update_widget_withtextpad(conf, shadow, widget, h, w,
			    RAISED, textpad, &htextpad, text, true) != 0)
				return BSDDIALOG_ERROR;

			prefresh(textpad, 0, 0, y+1, x+1+t.text.hmargin, y+h-7, 
			    x+w-1-t.text.hmargin);
			
			draw_borders(conf, bar, 3, sizebar + 2, RAISED);

			barupdate = true;
			buttupdate = true;
			break;
		default:
			for (i = 0; i < (int) bs.nbuttons; i++)
				if (tolower(input) == tolower((bs.label[i])[0])) {
					output = bs.value[i];
					loop = false;
			}
		}
	}

	delwin(bar);
	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return output;
}

int
bsddialog_pause(struct bsddialog_conf *conf, char* text, int rows, int cols,
    unsigned int sec)
{
	WINDOW *widget, *textpad, *bar, *shadow;
	int i, output, y, x, h, w, htextpad;
	bool loop, buttupdate, barupdate;
	int input, tout, sizebar;
	float perc;
	struct buttons bs;

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	if (bar_autosize(conf, rows, cols, &h, &w, text, &bs) != 0)
		return BSDDIALOG_ERROR;
	if (bar_checksize(text, h, w, &bs) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, true) != 0)
		return BSDDIALOG_ERROR;
	
	prefresh(textpad, 0, 0, y+1, x+1+t.text.hmargin, y+h-7, 
	    x+w-1-t.text.hmargin);

	sizebar = w - HBORDERS - 2 - BARMARGIN * 2;
	bar = new_boxed_window(conf, y + h - 6, x + 1 + BARMARGIN, 3,
	    sizebar + 2, RAISED);

	tout = sec;
	nodelay(stdscr, TRUE);
	timeout(1000);
	loop = buttupdate = barupdate = true;
	while(loop) {
		if (barupdate) {
			perc = (float)tout * 100 / sec;
			draw_perc_bar(bar, 1, 1, sizebar, perc, true, tout);
			barupdate = false;
			wrefresh(bar);
		}

		if (buttupdate) {
			draw_buttons(widget, h-2, w, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}

		input = getch();
		if(input < 0) { /* timeout */
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
			output = BSDDIALOG_ESC;
			loop = false;
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
			if (bar_autosize(conf, rows, cols, &h, &w, text, &bs) != 0)
				return BSDDIALOG_ERROR;
			if (bar_checksize(text, h, w, &bs) != 0)
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

			sizebar = w - HBORDERS - 2 - BARMARGIN * 2;
			wclear(bar);
			mvwin(bar, y + h - 6, x + 1 + BARMARGIN);
			wresize(bar, 3, sizebar + 2);

			if(update_widget_withtextpad(conf, shadow, widget, h, w,
			    RAISED, textpad, &htextpad, text, true) != 0)
				return BSDDIALOG_ERROR;

			prefresh(textpad, 0, 0, y+1, x+1+t.text.hmargin, y+h-7, 
			    x+w-1-t.text.hmargin);
			
			draw_borders(conf, bar, 3, sizebar + 2, RAISED);

			barupdate = true;
			buttupdate = true;
			break;
		default:
			for (i = 0; i < (int) bs.nbuttons; i++)
				if (tolower(input) == tolower((bs.label[i])[0])) {
					output = bs.value[i];
					loop = false;
			}
		}
	}

	nodelay(stdscr, FALSE);

	delwin(bar);
	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return output;
}
