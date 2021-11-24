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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

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
	if ((clear = newwin(h, w, y + t.shadowrows, x + t.shadowcols)) == NULL)
		RETURN_ERROR("Cannot hide the widget");
	wbkgd(clear, t.backgroundcolor);

	if (withshadow)
		wrefresh(clear);

	mvwin(clear, y, x);
	wrefresh(clear);

	delwin(clear);

	return 0;
}

/* F1 help */
int f1help(struct bsddialog_conf conf)
{
	char *file = conf.hfile;
	char *title = conf.title;
	int output;

	conf.hfile = NULL;
	conf.clear = true;
	conf.y = BSDDIALOG_CENTER;
	conf.x = BSDDIALOG_CENTER;
	conf.title = "HELP";
	conf.sleep = 0;

	output = bsddialog_textbox(conf, file, BSDDIALOG_AUTOSIZE,
	    BSDDIALOG_AUTOSIZE);
	conf.hfile = file;
	conf.title = title;

	return output;
}

/* Buttons */
void
draw_button(WINDOW *window, int y, int x, int size, char *text, bool selected,
    bool shortkey)
{
	int i, color_arrows, color_shortkey, color_button;

	if (selected) {
		color_arrows = t.currbuttdelimcolor;
		color_shortkey = t.currshortkeycolor;
		color_button = t.currbuttoncolor;
	} else {
		color_arrows = t.buttdelimcolor;
		color_shortkey = t.shortkeycolor;
		color_button = t.buttoncolor;
	}

	wattron(window, color_arrows);
	mvwaddch(window, y, x, t.buttleftch);
	wattroff(window, color_arrows);
	wattron(window, color_button);
	for(i = 1; i < size - 1; i++)
		waddch(window, ' ');
	wattroff(window, color_button);
	wattron(window, color_arrows);
	mvwaddch(window, y, x + i, t.buttrightchar);
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

	start_x = bs.sizebutton * bs.nbuttons + (bs.nbuttons - 1) * t.buttonspace;
	start_x = cols/2 - start_x/2;

	for (i = 0; i < (int) bs.nbuttons; i++) {
		x = i * (bs.sizebutton + t.buttonspace);
		draw_button(window, y, start_x + x, bs.sizebutton, bs.label[i],
		    i == bs.curr, shortkey);
	}
}

void
get_buttons(struct bsddialog_conf conf, struct buttons *bs, char *yesoklabel,
    char *extralabel, char *nocancellabel, char *helplabel)
{
	int i;
#define SIZEBUTTON  8
#define DEFAULT_BUTTON_LABEL	LABEL_ok_label
#define DEFAULT_BUTTON_VALUE	BSDDIALOG_YESOK


	bs->nbuttons = 0;
	bs->curr = 0;
	bs->sizebutton = 0;

	if (yesoklabel != NULL && conf.button.no_ok == false) {
		bs->label[0] = yesoklabel;
		bs->value[0] = BSDDIALOG_YESOK;
		bs->nbuttons += 1;
	}

	if (extralabel != NULL && conf.button.extra_button) {
		bs->label[bs->nbuttons] = extralabel;
		bs->value[bs->nbuttons] = BSDDIALOG_EXTRA;
		bs->nbuttons += 1;
	}

	if (nocancellabel != NULL && conf.button.no_cancel == false) {
		bs->label[bs->nbuttons] = nocancellabel;
		bs->value[bs->nbuttons] = BSDDIALOG_NOCANCEL;
		if (conf.button.defaultno)
			bs->curr = bs->nbuttons;
		bs->nbuttons += 1;
	}

	if (helplabel != NULL && conf.button.help_button) {
		bs->label[bs->nbuttons] = helplabel;
		bs->value[bs->nbuttons] = BSDDIALOG_HELP;
		bs->nbuttons += 1;
	}

	if (bs->nbuttons == 0) {
		bs->label[0] = DEFAULT_BUTTON_LABEL;
		bs->value[0] = DEFAULT_BUTTON_VALUE;
		bs->nbuttons = 1;
	}

	if (conf.button.default_label != NULL) {
		for (i=0; i<(int)bs->nbuttons; i++) {
			if (strcmp(conf.button.default_label, bs->label[i]) == 0)
				bs->curr = i;
		}
	}

	bs->sizebutton = MAX(SIZEBUTTON - 2, strlen(bs->label[0]));
	for (i=1; i < (int) bs->nbuttons; i++)
		bs->sizebutton = MAX(bs->sizebutton, strlen(bs->label[i]));
	bs->sizebutton += 2;
}

/* Text */

// old text, to delete in the future
enum token { TEXT, WS, END };

static bool check_set_ncurses_attr(WINDOW *win, char *text)
{
	bool isattr;
	int colors[8] = {
	    COLOR_BLACK,
	    COLOR_RED,
	    COLOR_GREEN,
	    COLOR_YELLOW,
	    COLOR_BLUE,
	    COLOR_MAGENTA,
	    COLOR_CYAN,
	    COLOR_WHITE
	};

	if (text[0] == '\0' || text[0] != '\\')
		return false;
	if (text[1] == '\0' || text[1] != 'Z')
		return false;
	if (text[2] == '\0')
		return false;

	if ((text[2] - 48) >= 0 && (text[2] - 48) < 8) {
		// tocheck: import BSD_COLOR
		// tofix color background
		wattron(win, COLOR_PAIR(colors[text[2] - 48] * 8 + COLOR_WHITE + 1));
		return true;
	}

	isattr = true;
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
	default:
		isattr = false;
	}

	return isattr;
}

static bool isws(int ch)
{

	return (ch == ' ' || ch == '\t' || ch == '\n');
}

static int
next_token(char *text, char *valuestr)
{
	int i, j;
	enum token tok;

	i = j = 0;

	if (text[0] == '\0')
		return END;

	while (text[i] != '\0') {
		if (isws(text[i])) {
			if (i == 0) {
				valuestr[0] = text[i];
				valuestr[1] = '\0';
				tok = WS;
			}
			break;
		}

		valuestr[j] = text[i];
		j++;
		valuestr[j] = '\0';
		i++;
		tok = TEXT;
	}

	return tok;
}

static void
print_string(WINDOW *win, int *y, int *x, int minx, int maxx, char *str, bool color)
{
	int i, j, len, reallen;

	if(strlen(str) == 0)
		return;

	len = reallen = strlen(str);
	if (color) {
		i=0;
		while (i < len) {
			if (check_set_ncurses_attr(win, str+i))
				reallen -= 3;
			i++;
		}
	}

	i = 0;
	while (i < len) {
		if (*x + reallen > maxx) {
			*y = (*x != minx ? *y+1 : *y);
			*x = minx;
		}
		j = *x;
		while (j < maxx && i < len) {
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

void
print_text(struct bsddialog_conf conf, WINDOW *pad, int starty, int minx, int maxx,
    char *text)
{
	char *valuestr;
	int x, y;
	bool loop;
	enum token tok;

	valuestr = malloc(strlen(text) + 1);

	x = minx;
	y = starty;
	loop = true;
	while (loop) {
		tok = next_token(text, valuestr);
		switch (tok) {
		case END:
			loop = false;
			break;
		case WS:
			text += strlen(valuestr);
			print_string(pad, &y, &x, minx, maxx, valuestr, false /*useless*/);
			break;
		case TEXT:
			text += strlen(valuestr);
			print_string(pad, &y, &x, minx, maxx, valuestr, conf.text.colors);
			break;
		}
	}

	free(valuestr);
}

// new text funcs

static bool is_ncurses_attr(char *text)
{
	bool isattr;

	if (strnlen(text, 3) < 3)
		return false;

	if (text[0] != '\\' || text[1] != 'Z')
		return false;

	if ((text[2] - '0') >= 0 && (text[2] - '0') < 8)
		return true;

	isattr = text[2] == 'n' || text[2] == 'b' || text[2] == 'B' ||
	    text[2] == 'r' || text[2] == 'R' || text[2] == 'u' ||
	    text[2] == 'U';

	return isattr;
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

static void prepare_text(struct bsddialog_conf conf, char *text, char *buf)
{
	int i, j;

	i = j = 0;
	while (text[i] != '\0') {
		switch (text[i]) {
		case '\\':
			buf[j] = '\\';
			switch (text[i+1]) {
			case '\\':
				i++;
				break;
			case 'n':
				if (conf.text.no_nl_expand) {
					j++;
					buf[j] = 'n';
				} else
					buf[j] = '\n';
				i++;
				break;
			case 't':
				if (conf.text.no_collapse) {
					j++;
					buf[j] = 't';
				} else
					buf[j] = '\t';
				i++;
				break;
			}
			break;
		case '\n':
			buf[j] = conf.text.cr_wrap ? ' ' : '\n';
			break;
		case '\t':
			buf[j] = conf.text.no_collapse ? '\t' : ' ';
			break;
		default:
			buf[j] = text[i];
		}
		i++;
		j += (buf[j] == ' ' && conf.text.trim && j > 0 && buf[j-1] == ' ') ?
		    0 : 1;
	}
	buf[j] = '\0';
}

int
get_text_properties(struct bsddialog_conf conf, char *text, int *maxword,
    int *maxline, int *nlines)
{
	char *buf;
	int i, buflen, wordlen, linelen;

	if ((buf = malloc(strlen(text) + 1)) == NULL)
		RETURN_ERROR("Cannot building a buffer to find the properties "\
		    "of the text properties");

	prepare_text(conf, text, buf);

	buflen = strlen(buf) + 1;
	*maxword = 0;
	wordlen = 0;
	for (i=0; i < buflen; i++) {
		if (buf[i] == '\t' || buf[i] == '\n' || buf[i] == ' ' || buf[i] == '\0')
			if (wordlen != 0) {
				*maxword = MAX(*maxword, wordlen);
				wordlen = 0;
				continue;
			}
		if (conf.text.colors && is_ncurses_attr(buf + i))
			i += 3;
		else
			wordlen++;
	}

	*maxline = linelen = 0;
	*nlines = 1;
	for (i=0; i < buflen; i++) {
		switch (buf[i]) {
		case '\n':
			*nlines = *nlines + 1;
		case '\0':
			*maxline = MAX(*maxline, linelen);
			linelen = 0;
			break;
		default:
			if (conf.text.colors && is_ncurses_attr(buf + i))
				i += 3;
			else
				linelen++;
		}
	}
	if (*nlines == 1 && *maxline == 0)
		*nlines = 0;

	free(buf);

	return 0;
}

static int
print_textpad(struct bsddialog_conf conf, WINDOW *pad, int *rows, int cols, char *text)
{
	char *buf, *string;
	int i, j, x, y;
	bool loop;

	if ((buf = malloc(strlen(text) + 1)) == NULL)
		RETURN_ERROR("Cannot build (analyze) text");

	prepare_text(conf, text, buf);

	if ((string = malloc(strlen(text) + 1)) == NULL) {
		free(buf);
		RETURN_ERROR("Cannot build (analyze) text");
	}
	i = j = x = y = 0;
	loop = true;
	while (loop) {
		string[j] = buf[i];

		if (string[j] == '\0' || string[j] == '\n' ||
		    string[j] == '\t' || string[j] == ' ') {
			if (j != 0) {
				string[j] = '\0';
				print_str(pad, rows, &y, &x, cols, string, conf.text.colors);
			}
		}

		switch (buf[i]) {
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
	free(buf);

	return 0;
}

/* autosize */

/*
 * max y, that is from 0 to LINES - 1 - t.shadowrows,
 * could not be max height but avoids problems with checksize
 */
int widget_max_height(struct bsddialog_conf conf)
{
	int maxheight;

	if ((maxheight = conf.shadow ? LINES - 1 - t.shadowrows : LINES - 1) <= 0)
		RETURN_ERROR("Terminal too small, LINES - shadow <= 0");

	if (conf.y > 0)
		if ((maxheight -= conf.y) <=0)
			RETURN_ERROR("Terminal too small, LINES - shadow - y <= 0");

	return maxheight;
}

/*
 * max x, that is from 0 to COLS - 1 - t.shadowcols,
 *  * could not be max height but avoids problems with checksize
 */
int widget_max_width(struct bsddialog_conf conf)
{
	int maxwidth;

	if ((maxwidth = conf.shadow ? COLS - 1 - t.shadowcols : COLS - 1)  <= 0)
		RETURN_ERROR("Terminal too small, COLS - shadow <= 0");
	if (conf.x > 0)
		if ((maxwidth -= conf.x) <=0)
			RETURN_ERROR("Terminal too small, COLS - shadow - x <= 0");

	return maxwidth;
}

int
set_widget_size(struct bsddialog_conf conf, int rows, int cols, int *h, int *w)
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
set_widget_position(struct bsddialog_conf conf, int *y, int *x, int h, int w)
{

	if (conf.y == BSDDIALOG_CENTER)
		*y = LINES/2 - h/2;
	else if (conf.y < BSDDIALOG_CENTER)
		RETURN_ERROR("Negative begin y (less than -1)");
	else if (conf.y >= LINES)
		RETURN_ERROR("Begin Y under the terminal");
	else
		*y = conf.y;

	if ((*y + h + (conf.shadow ? (int) t.shadowrows : 0)) > LINES)
		RETURN_ERROR("The lower of the box under the terminal "\
		    "(begin Y + height (+ shadow) > terminal lines)");


	if (conf.x == BSDDIALOG_CENTER)
		*x = COLS/2 - w/2;
	else if (conf.x < BSDDIALOG_CENTER)
		RETURN_ERROR("Negative begin x (less than -1)");
	else if (conf.x >= COLS)
		RETURN_ERROR("Begin X over the right of the terminal");
	else
		*x = conf.x;

	if ((*x + w + (conf.shadow ? (int) t.shadowcols : 0)) > COLS)
		RETURN_ERROR("The right of the box over the terminal "\
		    "(begin X + width (+ shadow) > terminal cols)");

	return 0;
}

/* Widgets builders */
void
draw_borders(struct bsddialog_conf conf, WINDOW *win, int rows, int cols,
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

	if (conf.no_lines == false) {
		if (conf.ascii_lines) {
			ls = rs = '|';
			ts = bs = '-';
			tl = tr = bl = br = ltee = rtee = '+';
		}
		leftcolor  = elev == RAISED ? t.lineraisecolor : t.linelowercolor;
		rightcolor = elev == RAISED ? t.linelowercolor : t.lineraisecolor;
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
new_boxed_window(struct bsddialog_conf conf, int y, int x, int rows, int cols,
    enum elevation elev)
{
	WINDOW *win;

	if ((win = newwin(rows, cols, y, x)) == NULL) {
		set_error_string("Cannot build boxed window");
		return NULL;
	}

	wbkgd(win, t.widgetcolor);

	draw_borders(conf, win, rows, cols, elev);

	return win;
}

/*
 * `enum elevation elev` could be useless because it should be always RAISED,
 * to check at the end.
 */
static int
draw_widget_withtextpad(struct bsddialog_conf conf, WINDOW *shadow,
    WINDOW *widget, int h, int w, enum elevation elev,
    WINDOW *textpad, int *htextpad, char *text, bool buttons)
{
	int ts, ltee, rtee;
	int colorsurroundtitle;

	ts = conf.ascii_lines ? '-' : ACS_HLINE;
	ltee = conf.ascii_lines ? '+' : ACS_LTEE;
	rtee = conf.ascii_lines ? '+' : ACS_RTEE;
	colorsurroundtitle = elev == RAISED ? t.lineraisecolor : t.linelowercolor;

	if (shadow != NULL)
		wnoutrefresh(shadow);

	// move / resize now or the caller?
	draw_borders(conf, widget, h, w, elev);

	if (conf.title != NULL) {
		if (t.surroundtitle && conf.no_lines == false) {
			wattron(widget, colorsurroundtitle);
			mvwaddch(widget, 0, w/2 - strlen(conf.title)/2 - 1, rtee);
			wattroff(widget, colorsurroundtitle);
		}
		wattron(widget, t.titlecolor);
		mvwaddstr(widget, 0, w/2 - strlen(conf.title)/2, conf.title);
		wattroff(widget, t.titlecolor);
		if (t.surroundtitle && conf.no_lines == false) {
			wattron(widget, colorsurroundtitle);
			waddch(widget, ltee);
			wattroff(widget, colorsurroundtitle);
		}
	}

	if (conf.hline != NULL) {
		wattron(widget, t.bottomtitlecolor);
		wmove(widget, h - 1, w/2 - strlen(conf.hline)/2 - 1);
		waddch(widget, '[');
		waddstr(widget, conf.hline);
		waddch(widget, ']');
		wattroff(widget, t.bottomtitlecolor);
	}

	if (textpad == NULL && text != NULL) /* no pad, text null for textbox */
		print_text(conf, widget, 1, 2, w-3, text);

	if (buttons && conf.no_lines == false) {
		wattron(widget, t.lineraisecolor);
		mvwaddch(widget, h-3, 0, ltee);
		mvwhline(widget, h-3, 1, ts, w-2);
		wattroff(widget, t.lineraisecolor);

		wattron(widget, t.linelowercolor);
		mvwaddch(widget, h-3, w-1, rtee);
		wattroff(widget, t.linelowercolor);
	}

	wnoutrefresh(widget);

	if (textpad == NULL)
		return 0; /* widget_init() ends */

	if (text != NULL) /* programbox etc */
		if (print_textpad(conf, textpad, htextpad,
		    w - HBORDERS - t.texthmargin * 2, text) !=0)
			return BSDDIALOG_ERROR;

	return 0;
}

/*
 * `enum elevation elev` could be useless because it should be always RAISED,
 * to check at the end.
 */
int
update_widget_withtextpad(struct bsddialog_conf conf, WINDOW *shadow,
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
new_widget_withtextpad(struct bsddialog_conf conf, WINDOW **shadow,
    WINDOW **widget, int y, int x, int h, int w, enum elevation elev,
    WINDOW **textpad, int *htextpad, char *text, bool buttons)
{
	int error;

	if (conf.shadow) {
		*shadow = newwin(h, w, y + t.shadowrows, x + t.shadowcols);
		if (*shadow == NULL)
			RETURN_ERROR("Cannot build shadow");
		wbkgd(*shadow, t.shadowcolor);
	}

	if ((*widget = new_boxed_window(conf, y, x, h, w, elev)) == NULL) {
		if (conf.shadow)
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
		*textpad = newpad(*htextpad, w - HBORDERS - t.texthmargin * 2);
		if (*textpad == NULL) {
			delwin(*textpad);
			if (conf.shadow)
				delwin(*shadow);
			RETURN_ERROR("Cannot build the pad window for text");
		}
		wbkgd(*textpad, t.widgetcolor);
	}

	error =  draw_widget_withtextpad(conf, *shadow, *widget, h, w, elev,
	    *textpad, htextpad, text, buttons);

	return error;
}

int
new_widget(struct bsddialog_conf conf, WINDOW **widget, int *y, int *x,
    char *text, int *h, int *w, WINDOW **shadow, bool buttons)
{

	// to delete (each widget has to check its x,y,h,w)
	if (*h <= 0)
		; /* todo */

	if (*w <= 0)
		; /* todo */

	*y = (conf.y < 0) ? (LINES/2 - *h/2) : conf.y;
	*x = (conf.x < 0) ? (COLS/2 - *w/2) : conf.x;

	if (new_widget_withtextpad(conf, shadow, widget, *y, *x, *h, *w, RAISED,
	    NULL, NULL, text, buttons) != 0)
		return BSDDIALOG_ERROR;

	if (conf.shadow)
		wrefresh(*shadow);

	wrefresh(*widget);

	return 0;
}

void
end_widget_withtextpad(struct bsddialog_conf conf, WINDOW *window, int h, int w,
    WINDOW *textpad, WINDOW *shadow)
{
	int y, x;

	getbegyx(window, y, x); /* for clear, add y & x to args? */

	if (conf.sleep > 0)
		sleep(conf.sleep);

	if (textpad != NULL)
		delwin(textpad);

	delwin(window);

	if (conf.shadow)
		delwin(shadow);

	if (conf.clear)
		hide_widget(y, x, h, w, shadow != NULL);

	if (conf.get_height != NULL)
		*conf.get_height = h;
	if (conf.get_width != NULL)
		*conf.get_width = w;
}

void
end_widget(struct bsddialog_conf conf, WINDOW *window, int h, int w,
    WINDOW *shadow)
{

	end_widget_withtextpad(conf, window, h, w, NULL, shadow);
}
