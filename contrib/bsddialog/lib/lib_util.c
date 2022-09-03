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
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define ERRBUFLEN    1024 /* Error buffer len */

/* Error */
static char errorbuffer[ERRBUFLEN];

const char *get_error_string(void)
{
	return (errorbuffer);
}

void set_error_string(const char *str)
{
	strncpy(errorbuffer, str, ERRBUFLEN-1);
}

/* Unicode */
wchar_t* alloc_mbstows(const char *mbstring)
{
	size_t charlen, nchar;
	mbstate_t mbs;
	const char *pmbstring;
	wchar_t *wstring;

	nchar = 1;
	pmbstring = mbstring;
	memset(&mbs, 0, sizeof(mbs));
	while ((charlen = mbrlen(pmbstring, MB_CUR_MAX, &mbs)) != 0 &&
	    charlen != (size_t)-1 && charlen != (size_t)-2) {
		pmbstring += charlen;
		nchar++;
	}

	if ((wstring = calloc(nchar, sizeof(wchar_t))) == NULL)
		return (NULL);
	mbstowcs(wstring, mbstring, nchar);

	return (wstring);
}

void mvwaddwch(WINDOW *w, int y, int x, wchar_t wch)
{
	wchar_t ws[2];

	ws[0] = wch;
	ws[1] = L'\0';
	mvwaddwstr(w, y, x, ws);

}

int str_props(const char *mbstring, unsigned int *cols, bool *has_multi_col)
{
	bool multicol;
	int w;
	unsigned int ncol;
	size_t charlen, mb_cur_max;
	wchar_t wch;
	mbstate_t mbs;

	multicol = false;
	mb_cur_max = MB_CUR_MAX;
	ncol = 0;
	memset(&mbs, 0, sizeof(mbs));
	while ((charlen = mbrlen(mbstring, mb_cur_max, &mbs)) != 0 &&
	    charlen != (size_t)-1 && charlen != (size_t)-2) {
		if (mbtowc(&wch, mbstring, mb_cur_max) < 0)
			return (-1);
		w = (wch == L'\t') ? TABSIZE : wcwidth(wch);
		ncol += (w < 0) ? 0 : w;
		if (w > 1 && wch != L'\t')
			multicol = true;
		mbstring += charlen;
	}

	if (cols != NULL)
		*cols = ncol;
	if (has_multi_col != NULL)
		*has_multi_col = multicol;

	return (0);
}

unsigned int strcols(const char *mbstring)
{
	int w;
	unsigned int ncol;
	size_t charlen, mb_cur_max;
	wchar_t wch;
	mbstate_t mbs;

	mb_cur_max = MB_CUR_MAX;
	ncol = 0;
	memset(&mbs, 0, sizeof(mbs));
	while ((charlen = mbrlen(mbstring, mb_cur_max, &mbs)) != 0 &&
	    charlen != (size_t)-1 && charlen != (size_t)-2) {
		if (mbtowc(&wch, mbstring, mb_cur_max) < 0)
			return (0);
		w = (wch == L'\t') ? TABSIZE : wcwidth(wch);
		ncol += (w < 0) ? 0 : w;
		mbstring += charlen;
	}

	return (ncol);
}

/* Clear */
int hide_widget(int y, int x, int h, int w, bool withshadow)
{
	WINDOW *clear;

	if ((clear = newwin(h, w, y + t.shadow.y, x + t.shadow.x)) == NULL)
		RETURN_ERROR("Cannot hide the widget");
	wbkgd(clear, t.screen.color);

	if (withshadow)
		wrefresh(clear);

	mvwin(clear, y, x);
	wrefresh(clear);

	delwin(clear);

	return (0);
}

/* F1 help */
int f1help(struct bsddialog_conf *conf)
{
	int output;
	struct bsddialog_conf hconf;

	bsddialog_initconf(&hconf);
	hconf.title           = "HELP";
	hconf.button.ok_label = "EXIT";
	hconf.clear           = true;
	hconf.ascii_lines     = conf->ascii_lines;
	hconf.no_lines        = conf->no_lines;
	hconf.shadow          = conf->shadow;
	hconf.text.highlight  = conf->text.highlight;

	output = BSDDIALOG_OK;
	if (conf->key.f1_message != NULL)
		output = bsddialog_msgbox(&hconf, conf->key.f1_message, 0, 0);

	if (output != BSDDIALOG_ERROR && conf->key.f1_file != NULL)
		output = bsddialog_textbox(&hconf, conf->key.f1_file, 0, 0);

	return (output == BSDDIALOG_ERROR ? BSDDIALOG_ERROR : 0);
}

/* Buttons */
static void
draw_button(WINDOW *window, int y, int x, int size, const char *text,
    wchar_t first, bool selected, bool shortcut)
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
	mvwaddch(window, y, x, t.button.leftdelim);
	wattroff(window, color_arrows);
	wattron(window, color_button);
	for (i = 1; i < size - 1; i++)
		waddch(window, ' ');
	wattroff(window, color_button);
	wattron(window, color_arrows);
	mvwaddch(window, y, x + i, t.button.rightdelim);
	wattroff(window, color_arrows);

	x = x + 1 + ((size - 2 - strcols(text))/2);
	wattron(window, color_button);
	mvwaddstr(window, y, x, text);
	wattroff(window, color_button);

	if (shortcut) {
		wattron(window, color_shortkey);
		mvwaddwch(window, y, x, first);
		wattroff(window, color_shortkey);
	}
}

void
draw_buttons(WINDOW *window, struct buttons bs, bool shortcut)
{
	int i, x, startx, y, rows, cols;
	unsigned int newmargin, margin, wbuttons;

	getmaxyx(window, rows, cols);
	y = rows - 2;

	newmargin = cols - VBORDERS - (bs.nbuttons * bs.sizebutton);
	newmargin /= (bs.nbuttons + 1);
	newmargin = MIN(newmargin, t.button.maxmargin);
	if (newmargin == 0) {
		margin = t.button.minmargin;
		wbuttons = buttons_min_width(bs);
	} else {
		margin = newmargin;
		wbuttons = bs.nbuttons * bs.sizebutton;
		wbuttons += (bs.nbuttons + 1) * margin;
	}

	startx = (cols)/2 - wbuttons/2 + newmargin;
	for (i = 0; i < (int)bs.nbuttons; i++) {
		x = i * (bs.sizebutton + margin);
		draw_button(window, y, startx + x, bs.sizebutton, bs.label[i],
		    bs.first[i],  i == bs.curr, shortcut);
	}
}

void
get_buttons(struct bsddialog_conf *conf, struct buttons *bs,
    const char *yesoklabel, const char *nocancellabel)
{
	int i;
#define SIZEBUTTON              8
#define DEFAULT_BUTTON_LABEL	BUTTON_OK_LABEL
#define DEFAULT_BUTTON_VALUE	BSDDIALOG_OK
	wchar_t first;

	bs->nbuttons = 0;
	bs->curr = 0;
	bs->sizebutton = 0;

	if (yesoklabel != NULL && conf->button.without_ok == false) {
		bs->label[0] = conf->button.ok_label != NULL ?
		    conf->button.ok_label : yesoklabel;
		bs->value[0] = BSDDIALOG_OK;
		bs->nbuttons += 1;
	}

	if (conf->button.with_extra) {
		bs->label[bs->nbuttons] = conf->button.extra_label != NULL ?
		    conf->button.extra_label : "Extra";
		bs->value[bs->nbuttons] = BSDDIALOG_EXTRA;
		bs->nbuttons += 1;
	}

	if (nocancellabel != NULL && conf->button.without_cancel == false) {
		bs->label[bs->nbuttons] = conf->button.cancel_label ?
		    conf->button.cancel_label : nocancellabel;
		bs->value[bs->nbuttons] = BSDDIALOG_CANCEL;
		if (conf->button.default_cancel)
			bs->curr = bs->nbuttons;
		bs->nbuttons += 1;
	}

	if (conf->button.with_help) {
		bs->label[bs->nbuttons] = conf->button.help_label != NULL ?
		    conf->button.help_label : "Help";
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

	for (i = 0; i < (int)bs->nbuttons; i++) {
		mbtowc(&first, bs->label[i], MB_CUR_MAX);
		bs->first[i] = first;
	}

	if (conf->button.default_label != NULL) {
		for (i = 0; i < (int)bs->nbuttons; i++) {
			if (strcmp(conf->button.default_label,
			    bs->label[i]) == 0)
				bs->curr = i;
		}
	}

	bs->sizebutton = MAX(SIZEBUTTON - 2, strcols(bs->label[0]));
	for (i = 1; i < (int)bs->nbuttons; i++)
		bs->sizebutton = MAX(bs->sizebutton, strcols(bs->label[i]));
	bs->sizebutton += 2;
}

int buttons_min_width(struct buttons bs)
{
	unsigned int width;

	width = bs.nbuttons * bs.sizebutton;
	if (bs.nbuttons > 0)
		width += (bs.nbuttons - 1) * t.button.minmargin;

	return (width);
}

bool shortcut_buttons(wint_t key, struct buttons *bs)
{
	bool match;
	unsigned int i;

	match = false;
	for (i = 0; i < bs->nbuttons; i++) {
		if (towlower(key) == towlower(bs->first[i])) {
			bs->curr = i;
			match = true;
			break;
		}
	}

	return (match);
}

/* Text */
static bool is_wtext_attr(const wchar_t *wtext)
{
	if (wcsnlen(wtext, 3) < 3)
		return (false);

	if (wtext[0] != L'\\' || wtext[1] != L'Z')
		return (false);

	return (wcschr(L"nbBrRuU01234567", wtext[2]) == NULL ? false : true);
}

static bool check_set_wtext_attr(WINDOW *win, wchar_t *wtext)
{
	enum bsddialog_color bg;

	if (is_wtext_attr(wtext) == false)
		return (false);

	if ((wtext[2] - L'0') >= 0 && (wtext[2] - L'0') < 8) {
		bsddialog_color_attrs(t.dialog.color, NULL, &bg, NULL);
		wattron(win, bsddialog_color(wtext[2] - L'0', bg, 0));
		return (true);
	}

	switch (wtext[2]) {
	case L'n':
		wattron(win, t.dialog.color);
		wattrset(win, A_NORMAL);
		break;
	case L'b':
		wattron(win, A_BOLD);
		break;
	case L'B':
		wattroff(win, A_BOLD);
		break;
	case L'r':
		wattron(win, A_REVERSE);
		break;
	case L'R':
		wattroff(win, A_REVERSE);
		break;
	case L'u':
		wattron(win, A_UNDERLINE);
		break;
	case L'U':
		wattroff(win, A_UNDERLINE);
		break;
	}

	return (true);
}

/* Word Wrapping */
static void
print_string(WINDOW *win, int *rows, int cols, int *y, int *x, wchar_t *str,
    bool color)
{
	int i, j, len, reallen, wc;
	wchar_t ws[2];

	ws[1] = L'\0';

	len = wcslen(str);
	if (color) {
		reallen = 0;
		i=0;
		while (i < len) {
			if (is_wtext_attr(str+i) == false)
				reallen += wcwidth(str[i]);
			i++;
		}
	} else
		reallen = wcswidth(str, len);

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
			if (color && check_set_wtext_attr(win, str+i)) {
				i += 3;
			} else if (j + wcwidth(str[i]) > cols) {
				break;
			} else {
				/* inline mvwaddwch() for efficiency */
				ws[0] = str[i];
				mvwaddwstr(win, *y, j, ws);
				wc = wcwidth(str[i]);;
				reallen -= wc;
				j += wc;
				i++;
				*x = j;
			}
		}
	}
}

static int
print_textpad(struct bsddialog_conf *conf, WINDOW *pad, const char *text)
{
	bool loop;
	int i, j, z, rows, cols, x, y, tablen;
	wchar_t *wtext, *string;

	if ((wtext = alloc_mbstows(text)) == NULL)
		RETURN_ERROR("Cannot allocate/print text in wchar_t*");

	if ((string = calloc(wcslen(wtext) + 1, sizeof(wchar_t))) == NULL)
		RETURN_ERROR("Cannot build (analyze) text");

	getmaxyx(pad, rows, cols);
	tablen = (conf->text.tablen == 0) ? TABSIZE : (int)conf->text.tablen;

	i = j = x = y = 0;
	loop = true;
	while (loop) {
		string[j] = wtext[i];

		if (wcschr(L"\n\t  ", string[j]) != NULL || string[j] == L'\0') {
			string[j] = L'\0';
			print_string(pad, &rows, cols, &y, &x, string,
			    conf->text.highlight);
		}

		switch (wtext[i]) {
		case L'\0':
			loop = false;
			break;
		case L'\n':
			x = 0;
			y++;
			j = -1;
			break;
		case L'\t':
			for (z = 0; z < tablen; z++) {
				if (x >= cols) {
					x = 0;
					y++;
				}
				x++;
			}
			j = -1;
			break;
		case L' ':
			x++;
			if (x >= cols) {
				x = 0;
				y++;
			}
			j = -1;
		}

		if (y >= rows) {
			rows = y + 1;
			wresize(pad, rows, cols);
		}

		j++;
		i++;
	}

	free(wtext);
	free(string);

	return (0);
}

/* Text Autosize */
#define NL  -1
#define WS  -2
#define TB  -3

struct textproperties {
	int nword;
	int *words;
	uint8_t *wletters;
	int maxwordcols;
	int maxline;
	bool hasnewline;
};

static int
text_properties(struct bsddialog_conf *conf, const char *text,
    struct textproperties *tp)
{
	int i, l, currlinecols, maxwords, wtextlen, tablen, wordcols;
	wchar_t *wtext;

	tablen = (conf->text.tablen == 0) ? TABSIZE : (int)conf->text.tablen;

	maxwords = 1024;
	if ((tp->words = calloc(maxwords, sizeof(int))) == NULL)
		RETURN_ERROR("Cannot alloc memory for text autosize");

	if ((wtext = alloc_mbstows(text)) == NULL)
		RETURN_ERROR("Cannot allocate/autosize text in wchar_t*");
	wtextlen = wcslen(wtext);
	if ((tp->wletters = calloc(wtextlen, sizeof(uint8_t))) == NULL)
		RETURN_ERROR("Cannot allocate wletters for text autosizing");

	tp->nword = 0;
	tp->maxline = 0;
	tp->maxwordcols = 0;
	tp->hasnewline = false;
	currlinecols = 0;
	wordcols = 0;
	l = 0;
	for (i = 0; i < wtextlen; i++) {
		if (conf->text.highlight && is_wtext_attr(wtext + i)) {
			i += 2; /* +1 for update statement */
			continue;
		}

		if (tp->nword + 1 >= maxwords) {
			maxwords += 1024;
			tp->words = realloc(tp->words, maxwords * sizeof(int));
			if (tp->words == NULL)
				RETURN_ERROR("Cannot realloc memory for text "
				    "autosize");
		}

		if (wcschr(L"\t\n  ", wtext[i]) != NULL) {
			tp->maxwordcols = MAX(wordcols, tp->maxwordcols);

			if (wordcols != 0) {
				/* line */
				currlinecols += wordcols;
				/* word */
				tp->words[tp->nword] = wordcols;
				tp->nword += 1;
				wordcols = 0;
			}

			switch (wtext[i]) {
			case L'\t':
				/* line */
				currlinecols += tablen;
				/* word */
				tp->words[tp->nword] = TB;
				break;
			case L'\n':
				/* line */
				tp->hasnewline = true;
				tp->maxline = MAX(tp->maxline, currlinecols);
				currlinecols = 0;
				/* word */
				tp->words[tp->nword] = NL;
				break;
			case L' ':
				/* line */
				currlinecols += 1;
				/* word */
				tp->words[tp->nword] = WS;
				break;
			}
			tp->nword += 1;
		} else {
			tp->wletters[l] = wcwidth(wtext[i]);
			wordcols += tp->wletters[l];
			l++;
		}
	}
	/* word */
	if (wordcols != 0) {
		tp->words[tp->nword] = wordcols;
		tp->nword += 1;
		tp->maxwordcols = MAX(wordcols, tp->maxwordcols);
	}
	/* line */
	tp->maxline = MAX(tp->maxline, currlinecols);

	free(wtext);

	return (0);
}


static int
text_autosize(struct bsddialog_conf *conf, struct textproperties *tp,
    int maxrows, int mincols, bool increasecols, int *h, int *w)
{
	int i, j, x, y, z, l, line, maxwidth, tablen;

	maxwidth = widget_max_width(conf) - HBORDERS - TEXTHMARGINS;
	tablen = (conf->text.tablen == 0) ? TABSIZE : (int)conf->text.tablen;

	if (increasecols) {
		mincols = MAX(mincols, tp->maxwordcols);
		mincols = MAX(mincols,
		    (int)conf->auto_minwidth - HBORDERS - TEXTHMARGINS);
		mincols = MIN(mincols, maxwidth);
	}

	while (true) {
		x = 0;
		y = 1;
		line=0;
		l = 0;
		for (i = 0; i < tp->nword; i++) {
			switch (tp->words[i]) {
			case TB:
				for (j = 0; j < tablen; j++) {
					if (x >= mincols) {
						x = 0;
						y++;
					}
				x++;
				}
				break;
			case NL:
				y++;
				x = 0;
				break;
			case WS:
				x++;
				if (x >= mincols) {
					x = 0;
					y++;
				}
				break;
			default:
				if (tp->words[i] + x <= mincols) {
					x += tp->words[i];
					for (z = 0 ; z != tp->words[i]; l++ )
						z += tp->wletters[l];
				} else if (tp->words[i] <= mincols) {
					y++;
					x = tp->words[i];
					for (z = 0 ; z != tp->words[i]; l++ )
						z += tp->wletters[l];
				} else {
					for (j = tp->words[i]; j > 0; ) {
						y = (x == 0) ? y : y + 1;
						z = 0;
						while (z != j && z < mincols) {
							z += tp->wletters[l];
							l++;
						}
						x = z;
						line = MAX(line, x);
						j -= z;
					}
				}
			}
			line = MAX(line, x);
		}

		if (increasecols == false)
			break;
		if (mincols >= maxwidth)
			break;
		if (line >= y * (int)conf->text.cols_per_row && y <= maxrows)
			break;
		mincols++;
	}

	*h = (tp->nword == 0) ? 0 : y;
	*w = MIN(mincols, line); /* wtext can be less than mincols */

	return (0);
}

int
text_size(struct bsddialog_conf *conf, int rows, int cols, const char *text,
    struct buttons *bs, int rowsnotext, int startwtext, int *htext, int *wtext)
{
	bool changewtext;
	int wbuttons, maxhtext;
	struct textproperties tp;

	wbuttons = 0;
	if (bs != NULL)
		wbuttons = buttons_min_width(*bs);

	/* Rows */
	if (rows == BSDDIALOG_AUTOSIZE || rows == BSDDIALOG_FULLSCREEN) {
		maxhtext = widget_max_height(conf) - VBORDERS - rowsnotext;
	} else { /* fixed */
		maxhtext = rows - VBORDERS - rowsnotext;
	}
	if (bs != NULL)
		maxhtext -= 2;
	if (maxhtext <= 0)
		maxhtext = 1; /* text_autosize() computes always htext */

	/* Cols */
	if (cols == BSDDIALOG_AUTOSIZE) {
		startwtext = MAX(startwtext, wbuttons - TEXTHMARGINS);
		changewtext = true;
	} else if (cols == BSDDIALOG_FULLSCREEN) {
		startwtext = widget_max_width(conf) - VBORDERS - TEXTHMARGINS;
		changewtext = false;
	} else { /* fixed */
		startwtext = cols - VBORDERS - TEXTHMARGINS;
		changewtext = false;
	}

	if (startwtext <= 0 && changewtext)
		startwtext = 1;
	if (startwtext <= 0)
		RETURN_ERROR("Fullscreen or fixed cols to print text <=0");

	/* Sizing calculation */
	if (text_properties(conf, text, &tp) != 0)
		return (BSDDIALOG_ERROR);
	if (text_autosize(conf, &tp, maxhtext, startwtext, changewtext, htext,
	    wtext) != 0)
		return (BSDDIALOG_ERROR);

	free(tp.words);
	free(tp.wletters);

	return (0);
}

/* Widget size and position */
int widget_max_height(struct bsddialog_conf *conf)
{
	int maxheight;

	maxheight = conf->shadow ? SCREENLINES - (int)t.shadow.y : SCREENLINES;
	if (maxheight <= 0)
		RETURN_ERROR("Terminal too small, screen lines - shadow <= 0");

	if (conf->y != BSDDIALOG_CENTER && conf->auto_topmargin > 0)
		RETURN_ERROR("conf.y > 0 and conf->auto_topmargin > 0");
	else if (conf->y == BSDDIALOG_CENTER) {
		maxheight -= conf->auto_topmargin;
		if (maxheight <= 0)
			RETURN_ERROR("Terminal too small, screen lines - top "
			    "margins <= 0");
	} else if (conf->y > 0) {
		maxheight -= conf->y;
		if (maxheight <= 0)
			RETURN_ERROR("Terminal too small, screen lines - "
			    "shadow - y <= 0");
	}

	maxheight -= conf->auto_downmargin;
	if (maxheight <= 0)
		RETURN_ERROR("Terminal too small, screen lines - Down margins "
		    "<= 0");

	return (maxheight);
}

int widget_max_width(struct bsddialog_conf *conf)
{
	int maxwidth;

	maxwidth = conf->shadow ? SCREENCOLS - (int)t.shadow.x : SCREENCOLS;
	if (maxwidth <= 0)
		RETURN_ERROR("Terminal too small, screen cols - shadow <= 0");

	if (conf->x > 0) {
		maxwidth -= conf->x;
		if (maxwidth <= 0)
			RETURN_ERROR("Terminal too small, screen cols - shadow "
			    "- x <= 0");
	}

	return (maxwidth);
}

int
widget_min_height(struct bsddialog_conf *conf, int htext, int minwidget,
    bool withbuttons)
{
	int min;

	min = 0;

	/* buttons */
	if (withbuttons)
		min += 2; /* buttons and border */

	/* text */
	min += htext;

	/* specific widget min height */
	min += minwidget;

	/* dialog borders */
	min += HBORDERS;
	/* conf.auto_minheight */
	min = MAX(min, (int)conf->auto_minheight);
	/* avoid terminal overflow */
	min = MIN(min, widget_max_height(conf));

	return (min);
}

int
widget_min_width(struct bsddialog_conf *conf, int wtext, int minwidget,
    struct buttons *bs)

{
	int min, delimtitle, wbottomtitle, wtitle;

	min = 0;

	/* buttons */
	if (bs != NULL)
		min += buttons_min_width(*bs);

	/* text */
	if (wtext > 0)
		min = MAX(min, wtext + TEXTHMARGINS);

	/* specific widget min width */
	min = MAX(min, minwidget);

	/* title */
	if (conf->title != NULL) {
		delimtitle = t.dialog.delimtitle ? 2 : 0;
		wtitle = strcols(conf->title);
		min = MAX(min, wtitle + 2 + delimtitle);
	}

	/* bottom title */
	if (conf->bottomtitle != NULL) {
		wbottomtitle = strcols(conf->bottomtitle);
		min = MAX(min, wbottomtitle + 4);
	}

	/* dialog borders */
	min += VBORDERS;
	/* conf.auto_minwidth */
	min = MAX(min, (int)conf->auto_minwidth);
	/* avoid terminal overflow */
	min = MIN(min, widget_max_width(conf));

	return (min);
}

int
set_widget_size(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w)
{
	int maxheight, maxwidth;

	if ((maxheight = widget_max_height(conf)) == BSDDIALOG_ERROR)
		return (BSDDIALOG_ERROR);

	if (rows == BSDDIALOG_FULLSCREEN)
		*h = maxheight;
	else if (rows < BSDDIALOG_FULLSCREEN)
		RETURN_ERROR("Negative (less than -1) height");
	else if (rows > BSDDIALOG_AUTOSIZE) {
		if ((*h = rows) > maxheight)
			RETURN_ERROR("Height too big (> terminal height - "
			    "shadow)");
	}
	/* rows == AUTOSIZE: each widget has to set its size */

	if ((maxwidth = widget_max_width(conf)) == BSDDIALOG_ERROR)
		return (BSDDIALOG_ERROR);

	if (cols == BSDDIALOG_FULLSCREEN)
		*w = maxwidth;
	else if (cols < BSDDIALOG_FULLSCREEN)
		RETURN_ERROR("Negative (less than -1) width");
	else if (cols > BSDDIALOG_AUTOSIZE) {
		if ((*w = cols) > maxwidth)
			RETURN_ERROR("Width too big (> terminal width - "
			    "shadow)");
	}
	/* cols == AUTOSIZE: each widget has to set its size */

	return (0);
}

int
set_widget_position(struct bsddialog_conf *conf, int *y, int *x, int h, int w)
{
	int hshadow = conf->shadow ? (int)t.shadow.y : 0;
	int wshadow = conf->shadow ? (int)t.shadow.x : 0;

	if (conf->y == BSDDIALOG_CENTER) {
		*y = SCREENLINES/2 - (h + hshadow)/2;
		if (*y < (int)conf->auto_topmargin)
			*y = conf->auto_topmargin;
		if (*y + h + hshadow > SCREENLINES - (int)conf->auto_downmargin)
			*y = SCREENLINES - h - hshadow - conf->auto_downmargin;
	}
	else if (conf->y < BSDDIALOG_CENTER)
		RETURN_ERROR("Negative begin y (less than -1)");
	else if (conf->y >= SCREENLINES)
		RETURN_ERROR("Begin Y under the terminal");
	else
		*y = conf->y;

	if (*y + h + hshadow > SCREENLINES)
		RETURN_ERROR("The lower of the box under the terminal "
		    "(begin Y + height (+ shadow) > terminal lines)");


	if (conf->x == BSDDIALOG_CENTER)
		*x = SCREENCOLS/2 - (w + wshadow)/2;
	else if (conf->x < BSDDIALOG_CENTER)
		RETURN_ERROR("Negative begin x (less than -1)");
	else if (conf->x >= SCREENCOLS)
		RETURN_ERROR("Begin X over the right of the terminal");
	else
		*x = conf->x;

	if ((*x + w + wshadow) > SCREENCOLS)
		RETURN_ERROR("The right of the box over the terminal "
		    "(begin X + width (+ shadow) > terminal cols)");

	return (0);
}

/* Widgets build, update, destroy */
void
draw_borders(struct bsddialog_conf *conf, WINDOW *win, int rows, int cols,
    enum elevation elev)
{
	int leftcolor, rightcolor;
	int ls, rs, ts, bs, tl, tr, bl, br, ltee, rtee;

	if (conf->no_lines)
		return;

	if (conf->ascii_lines) {
		ls = rs = '|';
		ts = bs = '-';
		tl = tr = bl = br = ltee = rtee = '+';
	} else {
		ls = rs = ACS_VLINE;
		ts = bs = ACS_HLINE;
		tl = ACS_ULCORNER;
		tr = ACS_URCORNER;
		bl = ACS_LLCORNER;
		br = ACS_LRCORNER;
		ltee = ACS_LTEE;
		rtee = ACS_RTEE;
	}

	leftcolor = elev == RAISED ?
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

WINDOW *
new_boxed_window(struct bsddialog_conf *conf, int y, int x, int rows, int cols,
    enum elevation elev)
{
	WINDOW *win;

	if ((win = newwin(rows, cols, y, x)) == NULL) {
		set_error_string("Cannot build boxed window");
		return (NULL);
	}

	wbkgd(win, t.dialog.color);

	draw_borders(conf, win, rows, cols, elev);

	return (win);
}

static int
draw_dialog(struct bsddialog_conf *conf, WINDOW *shadow, WINDOW *widget,
    WINDOW *textpad, const char *text, struct buttons *bs, bool shortcutbuttons)
{
	int h, w, wtitle, wbottomtitle, ts, ltee, rtee;

	ts = conf->ascii_lines ? '-' : ACS_HLINE;
	ltee = conf->ascii_lines ? '+' : ACS_LTEE;
	rtee = conf->ascii_lines ? '+' : ACS_RTEE;

	getmaxyx(widget, h, w);

	if (conf->shadow)
		wnoutrefresh(shadow);

	draw_borders(conf, widget, h, w, RAISED);

	if (conf->title != NULL) {
		if ((wtitle = strcols(conf->title)) < 0)
			return (BSDDIALOG_ERROR);
		if (t.dialog.delimtitle && conf->no_lines == false) {
			wattron(widget, t.dialog.lineraisecolor);
			mvwaddch(widget, 0, w/2 - wtitle/2 -1, rtee);
			wattroff(widget, t.dialog.lineraisecolor);
		}
		wattron(widget, t.dialog.titlecolor);
		mvwaddstr(widget, 0, w/2 - wtitle/2, conf->title);
		wattroff(widget, t.dialog.titlecolor);
		if (t.dialog.delimtitle && conf->no_lines == false) {
			wattron(widget, t.dialog.lineraisecolor);
			waddch(widget, ltee);
			wattroff(widget, t.dialog.lineraisecolor);
		}
	}

	if (bs != NULL) {
		if (conf->no_lines == false) {
			wattron(widget, t.dialog.lineraisecolor);
			mvwaddch(widget, h-3, 0, ltee);
			mvwhline(widget, h-3, 1, ts, w-2);
			wattroff(widget, t.dialog.lineraisecolor);

			wattron(widget, t.dialog.linelowercolor);
			mvwaddch(widget, h-3, w-1, rtee);
			wattroff(widget, t.dialog.linelowercolor);
		}
		draw_buttons(widget, *bs, shortcutbuttons);
	}

	if (conf->bottomtitle != NULL) {
		if ((wbottomtitle = strcols(conf->bottomtitle)) < 0)
			return (BSDDIALOG_ERROR);
		wattron(widget, t.dialog.bottomtitlecolor);
		wmove(widget, h - 1, w/2 - wbottomtitle/2 - 1);
		waddch(widget, ' ');
		waddstr(widget, conf->bottomtitle);
		waddch(widget, ' ');
		wattroff(widget, t.dialog.bottomtitlecolor);
	}

	wnoutrefresh(widget);

	if (textpad != NULL && text != NULL) /* textbox */
		if (print_textpad(conf, textpad, text) !=0)
			return (BSDDIALOG_ERROR);

	return (0);
}

int
update_dialog(struct bsddialog_conf *conf, WINDOW *shadow, WINDOW *widget,
    int y, int x, int h, int w, WINDOW *textpad, const char *text,
    struct buttons *bs, bool shortcutbuttons)
{
	int error;

	if (conf->shadow) {
		wclear(shadow);
		mvwin(shadow, y + t.shadow.y, x + t.shadow.x);
		wresize(shadow, h, w);
	}

	wclear(widget);
	mvwin(widget, y, x);
	wresize(widget, h, w);

	if (textpad != NULL) {
		wclear(textpad);
		wresize(textpad, 1, w - HBORDERS - TEXTHMARGINS);
	}

	error = draw_dialog(conf, shadow, widget, textpad, text, bs,
	    shortcutbuttons);

	return (error);
}

int
new_dialog(struct bsddialog_conf *conf, WINDOW **shadow, WINDOW **widget, int y,
    int x, int h, int w, WINDOW **textpad, const char *text, struct buttons *bs,
    bool shortcutbuttons)
{
	int error;

	if (conf->shadow) {
		*shadow = newwin(h, w, y + t.shadow.y, x + t.shadow.x);
		if (*shadow == NULL)
			RETURN_ERROR("Cannot build shadow");
		wbkgd(*shadow, t.shadow.color);
	}

	if ((*widget = new_boxed_window(conf, y, x, h, w, RAISED)) == NULL) {
		if (conf->shadow)
			delwin(*shadow);
		return (BSDDIALOG_ERROR);
	}

	if (textpad != NULL && text != NULL) { /* textbox */
		*textpad = newpad(1, w - HBORDERS - TEXTHMARGINS);
		if (*textpad == NULL) {
			delwin(*widget);
			if (conf->shadow)
				delwin(*shadow);
			RETURN_ERROR("Cannot build the pad window for text");
		}
		wbkgd(*textpad, t.dialog.color);
	}

	error = draw_dialog(conf, *shadow, *widget,
	    textpad == NULL ? NULL : *textpad, text, bs, shortcutbuttons);

	return (error);
}

void
end_dialog(struct bsddialog_conf *conf, WINDOW *shadow, WINDOW *widget,
    WINDOW *textpad)
{
	int y, x, h, w;

	getbegyx(widget, y, x);
	getmaxyx(widget, h, w);

	if (conf->sleep > 0)
		sleep(conf->sleep);

	if (textpad != NULL)
		delwin(textpad);

	delwin(widget);

	if (conf->shadow)
		delwin(shadow);

	if (conf->clear)
		hide_widget(y, x, h, w, conf->shadow);

	if (conf->get_height != NULL)
		*conf->get_height = h;
	if (conf->get_width != NULL)
		*conf->get_width = w;
}
