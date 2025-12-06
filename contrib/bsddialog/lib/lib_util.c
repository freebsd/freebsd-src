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

#include <stdarg.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wctype.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

/*
 * -1- Error and diagnostic
 *
 *	get_error_string();
 *	set_error_string();
 *	set_fmt_error_string();
 *
 * ----------------------------------------------------
 * -2- (Unicode) Multicolumn character strings
 *
 *	alloc_mbstows();
 *	mvwaddwch();
 *	str_props();
 *	strcols();
 *
 * ----------------------------------------------------
 * -3- Buttons
 *
 * [static] buttons_min_width();
 * [static] draw_button();
 *          draw_buttons();
 *          set_buttons(); (to call 1 time after prepare_dialog()).
 *          shortcut_buttons();
 *
 * ----------------------------------------------------
 * -4- (Auto) Sizing and (Auto) Position
 *
 * [static] widget_max_height(conf);
 * [static] widget_max_width(struct bsddialog_conf *conf)
 * [static] is_wtext_attr();
 * [static] text_properties();
 * [static] text_autosize();
 * [static] text_size();
 * [static] widget_min_height(conf, htext, hnotext, bool buttons);
 * [static] widget_min_width(conf, wtext, minw, buttons);
 *          set_widget_size();
 *          set_widget_autosize();  (not for all dialogs).
 *          widget_checksize();     (not for all dialogs).
 *          set_widget_position();
 *          dialog_size_position(struct dialog); (not for all dialogs).
 *
 * ----------------------------------------------------
 * -5- (Dialog) Widget components and utils
 *
 *	hide_dialog(struct dialog);
 *	f1help_dialog(conf);
 *	draw_borders(conf, win, elev);
 *	update_box(conf, win, y, x, h, w, elev);
 *	rtextpad(); (helper for pnoutrefresh(textpad)).
 *
 * ----------------------------------------------------
 * -6- Dialog init/build, update/draw, destroy
 *
 *          end_dialog(struct dialog);
 * [static] check_set_wtext_attr();
 * [static] print_string(); (word wrapping).
 * [static] print_textpad();
 *          draw_dialog(struct dialog);
 *          prepare_dialog(struct dialog);
 */

/*
 * -1- Error and diagnostic
 */
#define ERRBUFLEN    1024

static char errorbuffer[ERRBUFLEN];

const char *get_error_string(void)
{
	return (errorbuffer);
}

void set_error_string(const char *str)
{
	strncpy(errorbuffer, str, ERRBUFLEN-1);
}

void set_fmt_error_string(const char *fmt, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   vsnprintf(errorbuffer, ERRBUFLEN-1, fmt, arg_ptr);
   va_end(arg_ptr);
}

/*
 * -2- (Unicode) Multicolumn character strings
 */
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

/*
 * -3- Buttons
 */
static int buttons_min_width(struct buttons *bs)
{
	unsigned int width;

	width = bs->nbuttons * bs->sizebutton;
	if (bs->nbuttons > 0)
		width += (bs->nbuttons - 1) * t.button.minmargin;

	return (width);
}

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

void draw_buttons(struct dialog *d)
{
	int i, x, startx, y;
	unsigned int newmargin, margin, wbuttons;

	y = d->h - 2;

	newmargin = d->w - BORDERS - (d->bs.nbuttons * d->bs.sizebutton);
	newmargin /= (d->bs.nbuttons + 1);
	newmargin = MIN(newmargin, t.button.maxmargin);
	if (newmargin == 0) {
		margin = t.button.minmargin;
		wbuttons = buttons_min_width(&d->bs);
	} else {
		margin = newmargin;
		wbuttons = d->bs.nbuttons * d->bs.sizebutton;
		wbuttons += (d->bs.nbuttons + 1) * margin;
	}

	startx = d->w/2 - wbuttons/2 + newmargin;
	for (i = 0; i < (int)d->bs.nbuttons; i++) {
		x = i * (d->bs.sizebutton + margin);
		draw_button(d->widget, y, startx + x, d->bs.sizebutton,
		    d->bs.label[i], d->bs.first[i],  i == d->bs.curr,
		    d->bs.shortcut);
	}
}

void
set_buttons(struct dialog *d, bool shortcut, const char *oklabel,
    const char *cancellabel)
{
	int i;
#define SIZEBUTTON              8
#define DEFAULT_BUTTON_LABEL	OK_LABEL
#define DEFAULT_BUTTON_VALUE	BSDDIALOG_OK
	wchar_t first;

	d->bs.nbuttons = 0;
	d->bs.curr = 0;
	d->bs.sizebutton = 0;
	d->bs.shortcut = shortcut;

	if (d->conf->button.left1_label != NULL) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.left1_label;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_LEFT1;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.left2_label != NULL) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.left2_label;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_LEFT2;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.left3_label != NULL) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.left3_label;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_LEFT3;
		d->bs.nbuttons += 1;
	}

	if (oklabel != NULL && d->conf->button.without_ok == false) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.ok_label != NULL ?
		    d->conf->button.ok_label : oklabel;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_OK;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.with_extra) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.extra_label != NULL ?
		    d->conf->button.extra_label : "Extra";
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_EXTRA;
		d->bs.nbuttons += 1;
	}

	if (cancellabel != NULL && d->conf->button.without_cancel == false) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.cancel_label ?
		    d->conf->button.cancel_label : cancellabel;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_CANCEL;
		if (d->conf->button.default_cancel)
			d->bs.curr = d->bs.nbuttons;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.with_help) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.help_label != NULL ?
		    d->conf->button.help_label : "Help";
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_HELP;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.right1_label != NULL) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.right1_label;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_RIGHT1;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.right2_label != NULL) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.right2_label;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_RIGHT2;
		d->bs.nbuttons += 1;
	}

	if (d->conf->button.right3_label != NULL) {
		d->bs.label[d->bs.nbuttons] = d->conf->button.right3_label;
		d->bs.value[d->bs.nbuttons] = BSDDIALOG_RIGHT3;
		d->bs.nbuttons += 1;
	}

	if (d->bs.nbuttons == 0) {
		d->bs.label[0] = DEFAULT_BUTTON_LABEL;
		d->bs.value[0] = DEFAULT_BUTTON_VALUE;
		d->bs.nbuttons = 1;
	}

	for (i = 0; i < (int)d->bs.nbuttons; i++) {
		mbtowc(&first, d->bs.label[i], MB_CUR_MAX);
		d->bs.first[i] = first;
	}

	if (d->conf->button.default_label != NULL) {
		for (i = 0; i < (int)d->bs.nbuttons; i++) {
			if (strcmp(d->conf->button.default_label,
			    d->bs.label[i]) == 0)
				d->bs.curr = i;
		}
	}

	d->bs.sizebutton = MAX(SIZEBUTTON - 2, strcols(d->bs.label[0]));
	for (i = 1; i < (int)d->bs.nbuttons; i++)
		d->bs.sizebutton = MAX(d->bs.sizebutton, strcols(d->bs.label[i]));
	d->bs.sizebutton += 2;
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

/*
 * -4- (Auto) Sizing and (Auto) Position
 */
static int widget_max_height(struct bsddialog_conf *conf)
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

static int widget_max_width(struct bsddialog_conf *conf)
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

static bool is_wtext_attr(const wchar_t *wtext)
{
	bool att;

	if (wcsnlen(wtext, 3) < 3)
		return (false);
	if (wtext[0] != L'\\' || wtext[1] != L'Z')
		return (false);

	att = wcschr(L"nbBdDkKrRsSuU01234567", wtext[2]) == NULL ? false : true;

	return (att);
}

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
		if (conf->text.escape && is_wtext_attr(wtext + i)) {
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

	maxwidth = widget_max_width(conf) - BORDERS - TEXTHMARGINS;
	tablen = (conf->text.tablen == 0) ? TABSIZE : (int)conf->text.tablen;

	if (increasecols) {
		mincols = MAX(mincols, tp->maxwordcols);
		mincols = MAX(mincols,
		    (int)conf->auto_minwidth - BORDERS - TEXTHMARGINS);
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

static int
text_size(struct bsddialog_conf *conf, int rows, int cols, const char *text,
    struct buttons *bs, int rowsnotext, int startwtext, int *htext, int *wtext)
{
	bool changewtext;
	int wbuttons, maxhtext;
	struct textproperties tp;

	wbuttons = 0;
	if (bs->nbuttons > 0)
		wbuttons = buttons_min_width(bs);

	/* Rows */
	if (rows == BSDDIALOG_AUTOSIZE || rows == BSDDIALOG_FULLSCREEN) {
		maxhtext = widget_max_height(conf) - BORDERS - rowsnotext;
	} else { /* fixed */
		maxhtext = rows - BORDERS - rowsnotext;
	}
	if (bs->nbuttons > 0)
		maxhtext -= 2;
	if (maxhtext <= 0)
		maxhtext = 1; /* text_autosize() computes always htext */

	/* Cols */
	if (cols == BSDDIALOG_AUTOSIZE) {
		startwtext = MAX(startwtext, wbuttons - TEXTHMARGINS);
		changewtext = true;
	} else if (cols == BSDDIALOG_FULLSCREEN) {
		startwtext = widget_max_width(conf) - BORDERS - TEXTHMARGINS;
		changewtext = false;
	} else { /* fixed */
		startwtext = cols - BORDERS - TEXTHMARGINS;
		changewtext = false;
	}

	if (startwtext <= 0 && changewtext)
		startwtext = 1;

	/* Sizing calculation */
	if (text_properties(conf, text, &tp) != 0)
		return (BSDDIALOG_ERROR);
	if (tp.nword > 0 && startwtext <= 0)
		RETURN_FMTERROR("(fixed cols or fullscreen) "
		    "needed at least %d cols to draw text",
		    BORDERS + TEXTHMARGINS + 1);
	if (text_autosize(conf, &tp, maxhtext, startwtext, changewtext, htext,
	    wtext) != 0)
		return (BSDDIALOG_ERROR);

	free(tp.words);
	free(tp.wletters);

	return (0);
}

static int
widget_min_height(struct bsddialog_conf *conf, int htext, int hnotext,
    bool withbuttons)
{
	int min;

	/* dialog borders */
	min = BORDERS;

	/* text */
	min += htext;

	/* specific widget lines without text */
	min += hnotext;

	/* buttons */
	if (withbuttons)
		min += HBUTTONS; /* buttons and their up-border */

	/* conf.auto_minheight */
	min = MAX(min, (int)conf->auto_minheight);

	return (min);
}

static int
widget_min_width(struct bsddialog_conf *conf, int wtext, int minwidget,
    struct buttons *bs)

{
	int min, delimtitle, wbottomtitle, wtitle;

	min = 0;

	/* buttons */
	if (bs->nbuttons > 0)
		min += buttons_min_width(bs);

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
	min += BORDERS;
	/* conf.auto_minwidth */
	min = MAX(min, (int)conf->auto_minwidth);

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
	else if (rows > BSDDIALOG_AUTOSIZE) /* fixed rows */
		*h = MIN(rows, maxheight); /* rows is at most maxheight */
	/* rows == AUTOSIZE: each widget has to set its size */

	if ((maxwidth = widget_max_width(conf)) == BSDDIALOG_ERROR)
		return (BSDDIALOG_ERROR);

	if (cols == BSDDIALOG_FULLSCREEN)
		*w = maxwidth;
	else if (cols < BSDDIALOG_FULLSCREEN)
		RETURN_ERROR("Negative (less than -1) width");
	else if (cols > BSDDIALOG_AUTOSIZE) /* fixed cols */
		*w = MIN(cols, maxwidth); /* cols is at most maxwidth */
	/* cols == AUTOSIZE: each widget has to set its size */

	return (0);
}

int
set_widget_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h,
    int *w, const char *text, int *rowstext, struct buttons *bs, int hnotext,
    int minw)
{
	int htext, wtext;

	if (rows == BSDDIALOG_AUTOSIZE || cols == BSDDIALOG_AUTOSIZE ||
	    rowstext != NULL) {
		if (text_size(conf, rows, cols, text, bs, hnotext, minw,
		    &htext, &wtext) != 0)
			return (BSDDIALOG_ERROR);
		if (rowstext != NULL)
			*rowstext = htext;
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = widget_min_height(conf, htext, hnotext, bs->nbuttons > 0);
		*h = MIN(*h, widget_max_height(conf));
	}

	if (cols == BSDDIALOG_AUTOSIZE) {
		*w = widget_min_width(conf, wtext, minw, bs);
		*w = MIN(*w, widget_max_width(conf));
	}

	return (0);
}

int widget_checksize(int h, int w, struct buttons *bs, int hnotext, int minw)
{
	int minheight, minwidth;

	minheight = BORDERS + hnotext;
	if (bs->nbuttons > 0)
		minheight += HBUTTONS;
	if (h < minheight)
		RETURN_FMTERROR("Current rows: %d, needed at least: %d",
		    h, minheight);

	minwidth = 0;
	if (bs->nbuttons > 0)
		minwidth = buttons_min_width(bs);
	minwidth = MAX(minwidth, minw);
	minwidth += BORDERS;
	if (w < minwidth)
		RETURN_FMTERROR("Current cols: %d, nedeed at least %d",
		    w, minwidth);

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

int dialog_size_position(struct dialog *d, int hnotext, int minw, int *htext)
{
	if (set_widget_size(d->conf, d->rows, d->cols, &d->h, &d->w) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_autosize(d->conf, d->rows, d->cols, &d->h, &d->w,
	    d->text, htext, &d->bs, hnotext, minw) != 0)
		return (BSDDIALOG_ERROR);
	if (widget_checksize(d->h, d->w, &d->bs, hnotext, minw) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(d->conf, &d->y, &d->x, d->h, d->w) != 0)
		return (BSDDIALOG_ERROR);

	return (0);
}

/*
 * -5- Widget components and utilities
 */
int hide_dialog(struct dialog *d)
{
	WINDOW *clear;

	if ((clear = newwin(d->h, d->w, d->y, d->x)) == NULL)
		RETURN_ERROR("Cannot hide the widget");
	wbkgd(clear, t.screen.color);
	wrefresh(clear);

	if (d->conf->shadow) {
		mvwin(clear, d->y + t.shadow.y, d->x + t.shadow.x);
		wrefresh(clear);
	}

	delwin(clear);

	return (0);
}

int f1help_dialog(struct bsddialog_conf *conf)
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
	hconf.text.escape     = conf->text.escape;

	output = BSDDIALOG_OK;
	if (conf->key.f1_message != NULL)
		output = bsddialog_msgbox(&hconf, conf->key.f1_message, 0, 0);

	if (output != BSDDIALOG_ERROR && conf->key.f1_file != NULL)
		output = bsddialog_textbox(&hconf, conf->key.f1_file, 0, 0);

	return (output == BSDDIALOG_ERROR ? BSDDIALOG_ERROR : 0);
}

void draw_borders(struct bsddialog_conf *conf, WINDOW *win, enum elevation elev)
{
	int h, w;
	int leftcolor, rightcolor;
	cchar_t *ls, *rs, *ts, *bs, *tl, *tr, *bl, *br;
	cchar_t hline, vline, corner;

	if (conf->no_lines)
		return;

	if (conf->ascii_lines) {
		setcchar(&hline, L"|", 0, 0, NULL);
		ls = rs = &hline;
		setcchar(&vline, L"-", 0, 0, NULL);
		ts = bs = &vline;
		setcchar(&corner, L"+", 0, 0, NULL);
		tl = tr = bl = br = &corner;
	} else {
		ls = rs = WACS_VLINE;
		ts = bs = WACS_HLINE;
		tl = WACS_ULCORNER;
		tr = WACS_URCORNER;
		bl = WACS_LLCORNER;
		br = WACS_LRCORNER;
	}

	getmaxyx(win, h, w);
	leftcolor = (elev == RAISED) ?
	    t.dialog.lineraisecolor : t.dialog.linelowercolor;
	rightcolor = (elev == RAISED) ?
	    t.dialog.linelowercolor : t.dialog.lineraisecolor;

	wattron(win, leftcolor);
	wborder_set(win, ls, rs, ts, bs, tl, tr, bl, br);
	wattroff(win, leftcolor);

	wattron(win, rightcolor);
	mvwadd_wch(win, 0, w-1, tr);
	mvwvline_set(win, 1, w-1, rs, h-2);
	mvwadd_wch(win, h-1, w-1, br);
	mvwhline_set(win, h-1, 1, bs, w-2);
	wattroff(win, rightcolor);
}

void
update_box(struct bsddialog_conf *conf, WINDOW *win, int y, int x, int h, int w,
    enum elevation elev)
{
	wclear(win);
	wresize(win, h, w);
	mvwin(win, y, x);
	draw_borders(conf, win, elev);
}

void
rtextpad(struct dialog *d, int ytext, int xtext, int upnotext, int downnotext)
{
	pnoutrefresh(d->textpad, ytext, xtext,
	    d->y + BORDER + upnotext,
	    d->x + BORDER + TEXTHMARGIN,
	    d->y + d->h - 1 - downnotext - BORDER,
	    d->x + d->w - TEXTHMARGIN - BORDER);
}

/*
 * -6- Dialog init/build, update/draw, destroy
 */
void end_dialog(struct dialog *d)
{
	if (d->conf->sleep > 0)
		sleep(d->conf->sleep);

	delwin(d->textpad);
	delwin(d->widget);
	if (d->conf->shadow)
		delwin(d->shadow);

	if (d->conf->clear)
		hide_dialog(d);

	if (d->conf->get_height != NULL)
		*d->conf->get_height = d->h;
	if (d->conf->get_width != NULL)
		*d->conf->get_width = d->w;
}

static bool check_set_wtext_attr(WINDOW *win, wchar_t *wtext)
{
	enum bsddialog_color bg;

	if (is_wtext_attr(wtext) == false)
		return (false);

	if ((wtext[2] >= L'0') && (wtext[2] <= L'7')) {
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
	case L'd':
		wattron(win, A_DIM);
		break;
	case L'D':
		wattroff(win, A_DIM);
		break;
	case L'k':
		wattron(win, A_BLINK);
		break;
	case L'K':
		wattroff(win, A_BLINK);
		break;
	case L'r':
		wattron(win, A_REVERSE);
		break;
	case L'R':
		wattroff(win, A_REVERSE);
		break;
	case L's':
		wattron(win, A_STANDOUT);
		break;
	case L'S':
		wattroff(win, A_STANDOUT);
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

static void
print_string(WINDOW *win, int *rows, int cols, int *y, int *x, wchar_t *str,
    bool color)
{
	int charwidth, i, j, strlen, strwidth;
	wchar_t ws[2];

	ws[1] = L'\0';

	strlen = wcslen(str);
	if (color) {
		strwidth = 0;
		i=0;
		while (i < strlen) {
			if (is_wtext_attr(str+i) == false) {
				strwidth += wcwidth(str[i]);
				i++;
			} else {
				i += 3;
			}
		}
	} else
		strwidth = wcswidth(str, strlen);

	i = 0;
	while (i < strlen) {
		if (*x + strwidth > cols) {
			if (*x != 0)
				*y = *y + 1;
			if (*y >= *rows) {
				*rows = *y + 1;
				wresize(win, *rows, cols);
			}
			*x = 0;
		}
		j = *x;
		while (i < strlen) {
			if (color && check_set_wtext_attr(win, str+i)) {
				i += 3;
				continue;
			}

			charwidth = wcwidth(str[i]);
			if (j + wcwidth(str[i]) > cols)
				break;
			/* inline mvwaddwch() for efficiency */
			ws[0] = str[i];
			mvwaddwstr(win, *y, j, ws);
			strwidth -= charwidth;
			j += charwidth;
			*x = j;
			i++;
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
			    conf->text.escape);
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

int draw_dialog(struct dialog *d)
{
	int wtitle, wbottomtitle;
	cchar_t ts, ltee, rtee;

	if (d->conf->ascii_lines) {
		setcchar(&ts, L"-", 0, 0, NULL);
		setcchar(&ltee, L"+", 0, 0,NULL);
		setcchar(&rtee, L"+", 0, 0, NULL);
	} else {
		ts = *WACS_HLINE;
		ltee = *WACS_LTEE;
		rtee = *WACS_RTEE;
	}

	if (d->conf->shadow) {
		wclear(d->shadow);
		wresize(d->shadow, d->h, d->w);
		mvwin(d->shadow, d->y + t.shadow.y, d->x + t.shadow.x);
		wnoutrefresh(d->shadow);
	}

	wclear(d->widget);
	wresize(d->widget, d->h, d->w);
	mvwin(d->widget, d->y, d->x);
	draw_borders(d->conf, d->widget, RAISED);

	if (d->conf->title != NULL) {
		if ((wtitle = strcols(d->conf->title)) < 0)
			return (BSDDIALOG_ERROR);
		if (t.dialog.delimtitle && d->conf->no_lines == false) {
			wattron(d->widget, t.dialog.lineraisecolor);
			mvwadd_wch(d->widget, 0, d->w/2 - wtitle/2 -1, &rtee);
			wattroff(d->widget, t.dialog.lineraisecolor);
		}
		wattron(d->widget, t.dialog.titlecolor);
		mvwaddstr(d->widget, 0, d->w/2 - wtitle/2, d->conf->title);
		wattroff(d->widget, t.dialog.titlecolor);
		if (t.dialog.delimtitle && d->conf->no_lines == false) {
			wattron(d->widget, t.dialog.lineraisecolor);
			wadd_wch(d->widget, &ltee);
			wattroff(d->widget, t.dialog.lineraisecolor);
		}
	}

	if (d->bs.nbuttons > 0) {
		if (d->conf->no_lines == false) {
			wattron(d->widget, t.dialog.lineraisecolor);
			mvwadd_wch(d->widget, d->h-3, 0, &ltee);
			mvwhline_set(d->widget, d->h-3, 1, &ts, d->w-2);
			wattroff(d->widget, t.dialog.lineraisecolor);

			wattron(d->widget, t.dialog.linelowercolor);
			mvwadd_wch(d->widget, d->h-3, d->w-1, &rtee);
			wattroff(d->widget, t.dialog.linelowercolor);
		}
		draw_buttons(d);
	}

	if (d->conf->bottomtitle != NULL) {
		if ((wbottomtitle = strcols(d->conf->bottomtitle)) < 0)
			return (BSDDIALOG_ERROR);
		wattron(d->widget, t.dialog.bottomtitlecolor);
		wmove(d->widget, d->h - 1, d->w/2 - wbottomtitle/2 - 1);
		waddch(d->widget, ' ');
		waddstr(d->widget, d->conf->bottomtitle);
		waddch(d->widget, ' ');
		wattroff(d->widget, t.dialog.bottomtitlecolor);
	}

	wnoutrefresh(d->widget);

	wclear(d->textpad);
	/* `infobox "" 0 2` fails but text is empty and textpad remains 1 1 */
	wresize(d->textpad, 1, d->w - BORDERS - TEXTHMARGINS);

	if (print_textpad(d->conf, d->textpad, d->text) != 0)
		return (BSDDIALOG_ERROR);

	d->built = true;

	return (0);
}

int
prepare_dialog(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, struct dialog *d)
{
	CHECK_PTR(conf);

	d->built = false;
	d->conf = conf;
	d->rows = rows;
	d->cols = cols;
	d->text = CHECK_STR(text);
	d->bs.nbuttons = 0;

	if (d->conf->shadow) {
		if ((d->shadow = newwin(1, 1, 1, 1)) == NULL)
			RETURN_ERROR("Cannot build WINDOW shadow");
		wbkgd(d->shadow, t.shadow.color);
	}

	if ((d->widget = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW widget");
	wbkgd(d->widget, t.dialog.color);

	/* fake for textpad */
	if ((d->textpad = newpad(1, 1)) == NULL)
		RETURN_ERROR("Cannot build the pad WINDOW for text");
	wbkgd(d->textpad, t.dialog.color);

	return (0);
}