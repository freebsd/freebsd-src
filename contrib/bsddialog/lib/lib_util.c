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

#ifdef PORTNCURSES
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

extern struct bsddialog_theme t;

/* Error buffer */

#define ERRBUFLEN 1024
static char errorbuffer[ERRBUFLEN];

const char *get_error_string(void)
{
	return errorbuffer;
}

void set_error_string(char *str)
{

	strncpy(errorbuffer, str, ERRBUFLEN-1);
}

/* cleaner */
int hide_widget(int y, int x, int h, int w, bool withshadow)
{
	WINDOW *clear;

	/* no check: y, x, h and w are checked by the builders */
	if ((clear = newwin(h, w, y + t.shadow.h, x + t.shadow.w)) == NULL)
		RETURN_ERROR("Cannot hide the widget");
	wbkgd(clear, t.terminal.color);

	if (withshadow)
		wrefresh(clear);

	mvwin(clear, y, x);
	wrefresh(clear);

	delwin(clear);

	return 0;
}

/* F1 help */
int f1help(struct bsddialog_conf *conf)
{
	int output;
	struct bsddialog_conf hconf;

	//memcpy(&hconf, conf, sizeof(struct bsddialog_conf));
	bsddialog_initconf(&hconf);
	hconf.title = "HELP";
	hconf.button.ok_label = "EXIT";
	hconf.clear = true;
	hconf.ascii_lines = conf->ascii_lines;
	hconf.no_lines = conf->no_lines;
	hconf.shadow = conf->shadow;
	hconf.text.colors = conf->text.colors;

	output = BSDDIALOG_OK;
	if (conf->f1_message != NULL)
		output = bsddialog_msgbox(&hconf, conf->f1_message, 0, 0);

	if (output != BSDDIALOG_ERROR && conf->f1_file != NULL)
		output = bsddialog_textbox(&hconf, conf->f1_file, 0, 0);

	return (output == BSDDIALOG_ERROR ? BSDDIALOG_ERROR : 0);
}

/* Buttons */
void
draw_button(WINDOW *window, int y, int x, int size, char *text, bool selected,
    bool shortkey)
{
	int i, color_arrows, color_shortkey, color_button;

	if (selected) {
		color_arrows = t.button.f_delimcolor;
		color_shortkey = t.button.f_shortcutcolor;
		color_button = t.button.f_color;
	} else {
		color_arrows = t.button.delimcolor;
		color_shortkey = t.button.shortcutcolor;
		color_button = t.button.color;
	}

	wattron(window, color_arrows);
	mvwaddch(window, y, x, t.button.leftch);
	wattroff(window, color_arrows);
	wattron(window, color_button);
	for(i = 1; i < size - 1; i++)
		waddch(window, ' ');
	wattroff(window, color_button);
	wattron(window, color_arrows);
	mvwaddch(window, y, x + i, t.button.rightch);
	wattroff(window, color_arrows);

	x = x + 1 + ((size - 2 - strlen(text))/2);
	wattron(window, color_button);
	mvwaddstr(window, y, x, text);
	wattroff(window, color_button);

	if (shortkey) {
		wattron(window, color_shortkey);
		mvwaddch(window, y, x, text[0]);
		wattroff(window, color_shortkey);
	}
}

void
draw_buttons(WINDOW *window, int y, int cols, struct buttons bs, bool shortkey)
{
	int i, x, start_x;

	start_x = bs.sizebutton * bs.nbuttons + (bs.nbuttons - 1) * t.button.space;
	start_x = cols/2 - start_x/2;

	for (i = 0; i < (int) bs.nbuttons; i++) {
		x = i * (bs.sizebutton + t.button.space);
		draw_button(window, y, start_x + x, bs.sizebutton, bs.label[i],
		    i == bs.curr, shortkey);
	}
}

void
get_buttons(struct bsddialog_conf *conf, struct buttons *bs, char *yesoklabel,
    char *extralabel, char *nocancellabel, char *helplabel)
{
	int i;
#define SIZEBUTTON  8
#define DEFAULT_BUTTON_LABEL	LABEL_ok_label
#define DEFAULT_BUTTON_VALUE	BSDDIALOG_OK


	bs->nbuttons = 0;
	bs->curr = 0;
	bs->sizebutton = 0;

	if (yesoklabel != NULL && conf->button.without_ok == false) {
		bs->label[0] = yesoklabel;
		bs->value[0] = BSDDIALOG_OK;
		bs->nbuttons += 1;
	}

	if (extralabel != NULL && conf->button.with_extra) {
		bs->label[bs->nbuttons] = extralabel;
		bs->value[bs->nbuttons] = BSDDIALOG_EXTRA;
		bs->nbuttons += 1;
	}

	if (nocancellabel != NULL && conf->button.without_cancel == false) {
		bs->label[bs->nbuttons] = nocancellabel;
		bs->value[bs->nbuttons] = BSDDIALOG_CANCEL;
		if (conf->button.default_cancel)
			bs->curr = bs->nbuttons;
		bs->nbuttons += 1;
	}

	if (helplabel != NULL && conf->button.with_help) {
		bs->label[bs->nbuttons] = helplabel;
		bs->value[bs->nbuttons] = BSDDIALOG_HELP;
		bs->nbuttons += 1;
	}

	if (conf->button.generic1_label != NULL) {
		bs->label[bs->nbuttons] = conf->button.generic1_label;
		bs->value[bs->nbuttons] = BSDDIALOG_GENERIC1;
		bs->nbuttons += 1;
	}

	if (conf->button.generic2_label != NULL) {
		bs->label[bs->nbuttons] = conf->button.generic2_label;
		bs->value[bs->nbuttons] = BSDDIALOG_GENERIC2;
		bs->nbuttons += 1;
	}

	if (bs->nbuttons == 0) {
		bs->label[0] = DEFAULT_BUTTON_LABEL;
		bs->value[0] = DEFAULT_BUTTON_VALUE;
		bs->nbuttons = 1;
	}

	if (conf->button.default_label != NULL) {
		for (i=0; i<(int)bs->nbuttons; i++) {
			if (strcmp(conf->button.default_label, bs->label[i]) == 0)
				bs->curr = i;
		}
	}

	bs->sizebutton = MAX(SIZEBUTTON - 2, strlen(bs->label[0]));
	for (i=1; i < (int) bs->nbuttons; i++)
		bs->sizebutton = MAX(bs->sizebutton, strlen(bs->label[i]));
	bs->sizebutton += 2;
}

/* Text */
static bool is_ncurses_attr(char *text)
{

	if (strnlen(text, 3) < 3)
		return false;

	if (text[0] != '\\' || text[1] != 'Z')
		return false;

	return (strchr("nbBrRuU01234567", text[2]) == NULL ? false : true);
}

static bool check_set_ncurses_attr(WINDOW *win, char *text)
{
	
	if (is_ncurses_attr(text) == false)
		return false;

	if ((text[2] - '0') >= 0 && (text[2] - '0') < 8) {
		wattron(win, bsddialog_color( text[2] - '0', COLOR_WHITE, 0));
		return true;
	}

	switch (text[2]) {
	case 'n':
		wattrset(win, A_NORMAL);
		break;
	case 'b':
		wattron(win, A_BOLD);
		break;
	case 'B':
		wattroff(win, A_BOLD);
		break;
	case 'r':
		wattron(win, A_REVERSE);
		break;
	case 'R':
		wattroff(win, A_REVERSE);
		break;
	case 'u':
		wattron(win, A_UNDERLINE);
		break;
	case 'U':
		wattroff(win, A_UNDERLINE);
		break;
	}

	return true;
}

static void
print_str(WINDOW *win, int *rows, int *y, int *x, int cols, char *str, bool color)
{
	int i, j, len, reallen;

	if(strlen(str) == 0)
		return;

	len = reallen = strlen(str);
	if (color) {
		i=0;
		while (i < len) {
			if (is_ncurses_attr(str+i))
				reallen -= 3;
			i++;
		}
	}

	i = 0;
	while (i < len) {
		if (*x + reallen > cols) {
			*y = (*x != 0 ? *y+1 : *y);
			if (*y >= *rows) {
				*rows = *y + 1;
				wresize(win, *rows, cols);
			}
			*x = 0;
		}
		j = *x;
		while (j < cols && i < len) {
			if (color && check_set_ncurses_attr(win, str+i)) {
				i += 3;
			} else {
				mvwaddch(win, *y, j, str[i]);
				i++;
				reallen--;
				j++;
				*x = j;
			}
		}
	}
}

int
get_text_properties(struct bsddialog_conf *conf, char *text, int *maxword,
    int *maxline, int *nlines)
{
	int i, buflen, wordlen, linelen;


	buflen = strlen(text) + 1;
	*maxword = 0;
	wordlen = 0;
	for (i=0; i < buflen; i++) {
		if (text[i] == '\t' || text[i] == '\n' || text[i] == ' ' || text[i] == '\0')
			if (wordlen != 0) {
				*maxword = MAX(*maxword, wordlen);
				wordlen = 0;
				continue;
			}
		if (conf->text.colors && is_ncurses_attr(text + i))
			i += 3;
		else
			wordlen++;
	}

	*maxline = linelen = 0;
	*nlines = 1;
	for (i=0; i < buflen; i++) {
		switch (text[i]) {
		case '\n':
			*nlines = *nlines + 1;
		case '\0':
			*maxline = MAX(*maxline, linelen);
			linelen = 0;
			break;
		default:
			if (conf->text.colors && is_ncurses_attr(text + i))
				i += 3;
			else
				linelen++;
		}
	}
	if (*nlines == 1 && *maxline == 0)
		*nlines = 0;

	//free(buf);

	return 0;
}

int
print_textpad(struct bsddialog_conf *conf, WINDOW *pad, int *rows, int cols,
    char *text)
{
	char *string;
	int i, j, x, y;
	bool loop;

	if ((string = malloc(strlen(text) + 1)) == NULL)
		RETURN_ERROR("Cannot build (analyze) text");

	i = j = x = y = 0;
	loop = true;
	while (loop) {
		string[j] = text[i];

		if (string[j] == '\0' || string[j] == '\n' ||
		    string[j] == '\t' || string[j] == ' ') {
			if (j != 0) {
				string[j] = '\0';
				print_str(pad, rows, &y, &x, cols, string,
				    conf->text.colors);
			}
		}

		switch (text[i]) {
		case '\0':
			loop = false;
			break;
		case '\n':
			j = -1;
			x = 0;
			y++;
			break;
		case '\t':
			for (j=0; j<4 /*tablen*/; j++) {
				x++;
				if (x >= cols) {
					x = 0;
					y++;
				}
			}
			j = -1;
			break;
		case ' ':
			x++;
			if (x >= cols) {
				x = 0;
				y++;
			}
			j = -1;
		}

		if (y >= *rows) { /* check for whitespaces */
			*rows = y + 1;
			wresize(pad, *rows, cols);
		}

		j++;
		i++;
	}

	free(string);

	return 0;
}

/* autosize */

/*
 * max y, that is from 0 to LINES - 1 - t.shadowrows,
 * could not be max height but avoids problems with checksize
 */
int widget_max_height(struct bsddialog_conf *conf)
{
	int maxheight;

	if ((maxheight = conf->shadow ? LINES - 1 - t.shadow.h : LINES - 1) <= 0)
		RETURN_ERROR("Terminal too small, LINES - shadow <= 0");

	if (conf->y > 0)
		if ((maxheight -= conf->y) <=0)
			RETURN_ERROR("Terminal too small, LINES - shadow - y <= 0");

	return maxheight;
}

/*
 * max x, that is from 0 to COLS - 1 - t.shadowcols,
 *  * could not be max height but avoids problems with checksize
 */
int widget_max_width(struct bsddialog_conf *conf)
{
	int maxwidth;

	if ((maxwidth = conf->shadow ? COLS - 1 - t.shadow.w : COLS - 1)  <= 0)
		RETURN_ERROR("Terminal too small, COLS - shadow <= 0");
	if (conf->x > 0)
		if ((maxwidth -= conf->x) <=0)
			RETURN_ERROR("Terminal too small, COLS - shadow - x <= 0");

	return maxwidth;
}

int
set_widget_size(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w)
{
	int maxheight, maxwidth;

	if ((maxheight = widget_max_height(conf)) == BSDDIALOG_ERROR)
		return BSDDIALOG_ERROR;

	if (rows == BSDDIALOG_FULLSCREEN)
		*h = maxheight;
	else if (rows < BSDDIALOG_FULLSCREEN)
		RETURN_ERROR("Negative (less than -1) height");
	else if (rows > BSDDIALOG_AUTOSIZE) {
		if ((*h = rows) > maxheight)
			RETURN_ERROR("Height too big (> terminal height - "\
			    "shadow");
	}
	/* rows == AUTOSIZE: each widget has to set its size */

	if ((maxwidth = widget_max_width(conf)) == BSDDIALOG_ERROR)
		return BSDDIALOG_ERROR;

	if (cols == BSDDIALOG_FULLSCREEN)
		*w = maxwidth;
	else if (cols < BSDDIALOG_FULLSCREEN)
		RETURN_ERROR("Negative (less than -1) width");
	else if (cols > BSDDIALOG_AUTOSIZE) {
		if ((*w = cols) > maxwidth)
			RETURN_ERROR("Width too big (> terminal width - shadow)");
	}
	/* cols == AUTOSIZE: each widget has to set its size */

	return 0;
}

int
set_widget_position(struct bsddialog_conf *conf, int *y, int *x, int h, int w)
{

	if (conf->y == BSDDIALOG_CENTER)
		*y = LINES/2 - h/2;
	else if (conf->y < BSDDIALOG_CENTER)
		RETURN_ERROR("Negative begin y (less than -1)");
	else if (conf->y >= LINES)
		RETURN_ERROR("Begin Y under the terminal");
	else
		*y = conf->y;

	if ((*y + h + (conf->shadow ? (int) t.shadow.h : 0)) > LINES)
		RETURN_ERROR("The lower of the box under the terminal "\
		    "(begin Y + height (+ shadow) > terminal lines)");


	if (conf->x == BSDDIALOG_CENTER)
		*x = COLS/2 - w/2;
	else if (conf->x < BSDDIALOG_CENTER)
		RETURN_ERROR("Negative begin x (less than -1)");
	else if (conf->x >= COLS)
		RETURN_ERROR("Begin X over the right of the terminal");
	else
		*x = conf->x;

	if ((*x + w + (conf->shadow ? (int) t.shadow.w : 0)) > COLS)
		RETURN_ERROR("The right of the box over the terminal "\
		    "(begin X + width (+ shadow) > terminal cols)");

	return 0;
}

/* Widgets builders */
void
draw_borders(struct bsddialog_conf *conf, WINDOW *win, int rows, int cols,
    enum elevation elev)
{
	int leftcolor, rightcolor;
	int ls, rs, ts, bs, tl, tr, bl, br;
	int ltee, rtee;

	ls = rs = ACS_VLINE;
	ts = bs = ACS_HLINE;
	tl = ACS_ULCORNER;
	tr = ACS_URCORNER;
	bl = ACS_LLCORNER;
	br = ACS_LRCORNER;
	ltee = ACS_LTEE;
	rtee = ACS_RTEE;

	if (conf->no_lines == false) {
		if (conf->ascii_lines) {
			ls = rs = '|';
			ts = bs = '-';
			tl = tr = bl = br = ltee = rtee = '+';
		}
		leftcolor  = elev == RAISED ?
		    t.dialog.lineraisecolor : t.dialog.linelowercolor;
		rightcolor = elev == RAISED ?
		    t.dialog.linelowercolor : t.dialog.lineraisecolor;
		wattron(win, leftcolor);
		wborder(win, ls, rs, ts, bs, tl, tr, bl, br);
		wattroff(win, leftcolor);

		wattron(win, rightcolor);
		mvwaddch(win, 0, cols-1, tr);
		mvwvline(win, 1, cols-1, rs, rows-2);
		mvwaddch(win, rows-1, cols-1, br);
		mvwhline(win, rows-1, 1, bs, cols-2);
		wattroff(win, rightcolor);
	}
}

WINDOW *
new_boxed_window(struct bsddialog_conf *conf, int y, int x, int rows, int cols,
    enum elevation elev)
{
	WINDOW *win;

	if ((win = newwin(rows, cols, y, x)) == NULL) {
		set_error_string("Cannot build boxed window");
		return NULL;
	}

	wbkgd(win, t.dialog.color);

	draw_borders(conf, win, rows, cols, elev);

	return win;
}

/*
 * `enum elevation elev` could be useless because it should be always RAISED,
 * to check at the end.
 */
static int
draw_widget_withtextpad(struct bsddialog_conf *conf, WINDOW *shadow,
    WINDOW *widget, int h, int w, enum elevation elev,
    WINDOW *textpad, int *htextpad, char *text, bool buttons)
{
	int ts, ltee, rtee;
	int colordelimtitle;

	ts = conf->ascii_lines ? '-' : ACS_HLINE;
	ltee = conf->ascii_lines ? '+' : ACS_LTEE;
	rtee = conf->ascii_lines ? '+' : ACS_RTEE;
	colordelimtitle = elev == RAISED ?
	    t.dialog.lineraisecolor : t.dialog.linelowercolor;

	if (shadow != NULL)
		wnoutrefresh(shadow);

	// move / resize now or the caller?
	draw_borders(conf, widget, h, w, elev);

	if (conf->title != NULL) {
		if (t.dialog.delimtitle && conf->no_lines == false) {
			wattron(widget, colordelimtitle);
			mvwaddch(widget, 0, w/2 - strlen(conf->title)/2 - 1, rtee);
			wattroff(widget, colordelimtitle);
		}
		wattron(widget, t.dialog.titlecolor);
		mvwaddstr(widget, 0, w/2 - strlen(conf->title)/2, conf->title);
		wattroff(widget, t.dialog.titlecolor);
		if (t.dialog.delimtitle && conf->no_lines == false) {
			wattron(widget, colordelimtitle);
			waddch(widget, ltee);
			wattroff(widget, colordelimtitle);
		}
	}

	if (conf->bottomtitle != NULL) {
		wattron(widget, t.dialog.bottomtitlecolor);
		wmove(widget, h - 1, w/2 - strlen(conf->bottomtitle)/2 - 1);
		waddch(widget, '[');
		waddstr(widget, conf->bottomtitle);
		waddch(widget, ']');
		wattroff(widget, t.dialog.bottomtitlecolor);
	}

	//if (textpad == NULL && text != NULL) /* no pad, text null for textbox */
	//	print_text(conf, widget, 1, 2, w-3, text);

	if (buttons && conf->no_lines == false) {
		wattron(widget, t.dialog.lineraisecolor);
		mvwaddch(widget, h-3, 0, ltee);
		mvwhline(widget, h-3, 1, ts, w-2);
		wattroff(widget, t.dialog.lineraisecolor);

		wattron(widget, t.dialog.linelowercolor);
		mvwaddch(widget, h-3, w-1, rtee);
		wattroff(widget, t.dialog.linelowercolor);
	}

	wnoutrefresh(widget);

	if (textpad == NULL)
		return 0; /* widget_init() ends */

	if (text != NULL) /* programbox etc */
		if (print_textpad(conf, textpad, htextpad,
		    w - HBORDERS - t.text.hmargin * 2, text) !=0)
			return BSDDIALOG_ERROR;

	return 0;
}

/*
 * `enum elevation elev` could be useless because it should be always RAISED,
 * to check at the end.
 */
int
update_widget_withtextpad(struct bsddialog_conf *conf, WINDOW *shadow,
    WINDOW *widget, int h, int w, enum elevation elev,
    WINDOW *textpad, int *htextpad, char *text, bool buttons)
{
	int error;

	/* nothing for now */

	error =  draw_widget_withtextpad(conf, shadow, widget, h, w,
	    elev, textpad, htextpad, text, buttons);

	return error;
}

/*
 * `enum elevation elev` could be useless because it should be always RAISED,
 * to check at the end.
 */
int
new_widget_withtextpad(struct bsddialog_conf *conf, WINDOW **shadow,
    WINDOW **widget, int y, int x, int h, int w, enum elevation elev,
    WINDOW **textpad, int *htextpad, char *text, bool buttons)
{
	int error;

	if (conf->shadow) {
		*shadow = newwin(h, w, y + t.shadow.h, x + t.shadow.w);
		if (*shadow == NULL)
			RETURN_ERROR("Cannot build shadow");
		wbkgd(*shadow, t.shadow.color);
	}

	if ((*widget = new_boxed_window(conf, y, x, h, w, elev)) == NULL) {
		if (conf->shadow)
			delwin(*shadow);
		return BSDDIALOG_ERROR;
	}

	if (textpad == NULL) { /* widget_init() */
		error =  draw_widget_withtextpad(conf, *shadow, *widget, h, w,
		    elev, NULL, NULL, text, buttons);
		return error;
	}

	if (text != NULL) { /* programbox etc */
		*htextpad = 1;
		*textpad = newpad(*htextpad, w - HBORDERS - t.text.hmargin * 2);
		if (*textpad == NULL) {
			delwin(*textpad);
			if (conf->shadow)
				delwin(*shadow);
			RETURN_ERROR("Cannot build the pad window for text");
		}
		wbkgd(*textpad, t.dialog.color);
	}

	error =  draw_widget_withtextpad(conf, *shadow, *widget, h, w, elev,
	    *textpad, htextpad, text, buttons);

	return error;
}

void
end_widget_withtextpad(struct bsddialog_conf *conf, WINDOW *window, int h, int w,
    WINDOW *textpad, WINDOW *shadow)
{
	int y, x;

	getbegyx(window, y, x); /* for clear, add y & x to args? */

	if (conf->sleep > 0)
		sleep(conf->sleep);

	if (textpad != NULL)
		delwin(textpad);

	delwin(window);

	if (conf->shadow)
		delwin(shadow);

	if (conf->clear)
		hide_widget(y, x, h, w, shadow != NULL);

	if (conf->get_height != NULL)
		*conf->get_height = h;
	if (conf->get_width != NULL)
		*conf->get_width = w;
}
