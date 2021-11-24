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

#include <stdlib.h>
#include <string.h>

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/* "Bar": gauge - mixedgauge - rangebox - pause */

extern struct bsddialog_theme t;

static void
draw_perc_bar(WINDOW *win, int y, int x, int size, int perc, bool withlabel, int label)
{
	char labelstr[128];
	int i, blue_x, color;

	blue_x = (int)((perc*(size))/100);

	wmove(win, y, x);
	for (i = 0; i < size; i++) {
		color = (i <= blue_x) ? t.currbarcolor : t.barcolor;
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
		color = ( (blue_x + 1) <= (size/2 - (int) strlen(labelstr)/2 + i) ) ?
		    t.barcolor : t.currbarcolor;
		wattron(win, color);
		waddch(win, labelstr[i]);
		wattroff(win, color);
	}
}

int bsddialog_gauge(struct bsddialog_conf conf, char* text, int rows, int cols, int perc)
{
	WINDOW *widget, *bar, *shadow;
	char input[2048];
	int i, y, x;
	bool mainloop = true;

	if (new_widget(conf, &widget, &y, &x, text, &rows, &cols, &shadow,
	    false) <0)
		return -1;

	bar = new_boxed_window(conf, y+rows -4, x+3, 3, cols-6, RAISED);

	wrefresh(widget);
	wrefresh(bar);

	while (mainloop) {
		draw_perc_bar(bar, 1, 1, cols-8, perc, false, -1 /*unused*/);

		wrefresh(widget);
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
		i = 2;
		wmove(widget, 1, 1);
		wclrtoeol(widget);
		while (true) {
			scanf("%s", input);
			if (strcmp(input,"EOF") == 0) {
				mainloop = false;
				break;
			}
			if (strcmp(input,"XXX") == 0)
				break;
			//print_text(conf, widget, 1, 1, cols-2, input);
			mvwaddstr(widget, 1, i, input);
			i = i + strlen(input) + 1;
			wrefresh(widget);
		}
	}

	delwin(bar);
	end_widget(conf, widget, rows, cols, shadow);

	return BSDDIALOG_YESOK;
}

int bsddialog_mixedgauge(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int perc, int argc, char **argv)
{
	WINDOW *widget, *bar, *shadow;
	int i, miniperc, y, x;
	char states[11][16] = {
	    "[  Succeeded  ]",
	    "[   Failed    ]",
	    "[   Passed    ]",
	    "[  Completed  ]",
	    "[   Checked   ]",
	    "[    Done     ]",
	    "[   Skipped   ]",
	    "[ In Progress ]",
	    "!!!  BLANK  !!!",
	    "[     N/A     ]",
	    "[   UNKNOWN   ]",};

	if (new_widget(conf, &widget, &y, &x, NULL, &rows, &cols, &shadow,
	    false) <0)
		return -1;

	bar = new_boxed_window(conf, y+rows -4, x+3, 3, cols-6, RAISED);

	/* mini bars */
	for (i=0; i < (argc/2); i++) {
		miniperc = atol(argv[i*2 + 1]);
		if (miniperc == 8)
			continue;
		mvwaddstr(widget, i+1, 2, argv[i*2]);
		if (miniperc > 9)
			mvwaddstr(widget, i+1, cols-2-15, states[10]);
		else if (miniperc >= 0 && miniperc <= 9)
			mvwaddstr(widget, i+1, cols-2-15, states[miniperc]);
		else { //miniperc < 0
			miniperc = abs(miniperc);
			mvwaddstr(widget, i+1, cols-2-15, "[             ]");
			draw_perc_bar(widget, i+1, 1+cols-2-15, 13, miniperc,
			    false, -1 /*unused*/);
		}
	}

	print_text(conf, widget, rows-6, 2, cols-2, text);

	/* main bar */
	draw_perc_bar(bar, 1, 1, cols-8, perc, false, -1 /*unused*/);

	wattron(bar, t.barcolor);
	mvwaddstr(bar, 0, 2, "Overall Progress");
	wattroff(bar, t.barcolor);

	wrefresh(widget);
	wrefresh(bar);

	getch();

	delwin(bar);
	end_widget(conf, widget, rows, cols, shadow);

	return BSDDIALOG_YESOK;
}

int
bsddialog_rangebox(struct bsddialog_conf conf, char* text, int rows, int cols, int min,
    int max, int *value)
{
	WINDOW *widget, *bar, *shadow;
	int y, x;
	bool loop, buttupdate, barupdate;
	int input, currvalue, output, sizebar;
	float perc;
	int positions = max - min + 1;
	struct buttons bs;

	if (new_widget(conf, &widget, &y, &x, text, &rows, &cols, &shadow,
	    true) <0)
		return -1;

	bar = new_boxed_window(conf, y + rows - 6, x +7, 3, cols-14, RAISED);

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	if (value == NULL)
		RETURN_ERROR("*value == NULL");
	
	currvalue = *value;
	sizebar = cols - 16;
	loop = buttupdate = barupdate = true;
	while(loop) {
		if (barupdate) {
			perc = ((float)(currvalue - min)*100) / ((float)positions-1);
			draw_perc_bar(bar, 1, 1, sizebar, perc, true, currvalue);
			barupdate = false;
			wrefresh(bar);
		}

		if (buttupdate) {
			draw_buttons(widget, rows-2, cols, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}

		input = getch();
		switch(input) {
		case 10: // Enter
			output = bs.value[bs.curr]; // values -> outputs
			*value = currvalue;
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case '\t': // TAB
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
		}
	}

	delwin(bar);
	end_widget(conf, widget, rows, cols, shadow);

	return output;
}

int bsddialog_pause(struct bsddialog_conf conf, char* text, int rows, int cols, int sec)
{
	WINDOW *widget, *bar, *shadow;
	int output, y, x;
	bool loop, buttupdate, barupdate;
	int input, currvalue, sizebar;
	float perc;
	struct buttons bs;

	if (new_widget(conf, &widget, &y, &x, text, &rows, &cols, &shadow,
	    true) <0)
		return -1;

	bar = new_boxed_window(conf, y + rows - 6, x +7, 3, cols-14, RAISED);

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	currvalue = sec;
	sizebar = cols-16;
	nodelay(stdscr, TRUE);
	timeout(1000);
	//wtimeout(buttwin, 2);
	loop = buttupdate = barupdate = true;
	while(loop) {
		if (barupdate) {
			perc = ((float)(currvalue*100)) / ((float)sec);
			draw_perc_bar(bar, 1, 1, sizebar, perc, true, currvalue);
			barupdate = false;
			wrefresh(bar);
		}

		if (buttupdate) {
			draw_buttons(widget, rows-2, cols, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}

		input = getch();
		if(input < 0) {
			currvalue--;
			if (currvalue < 0) {
				output = BSDDIALOG_ERROR;
				break;
			}
			else {
				barupdate = true;
				continue;
			}
		}
		switch(input) {
		case 10: // Enter
			output = bs.value[bs.curr]; // values -> outputs
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case '\t': // TAB
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
		}
	}

	nodelay(stdscr, FALSE);

	delwin(bar);
	end_widget(conf, widget, rows, cols, shadow);

	return output;
}

