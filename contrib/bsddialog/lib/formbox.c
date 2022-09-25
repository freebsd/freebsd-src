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

#include <curses.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

struct privateitem {
	const char *label;      /* formitem.label */
	unsigned int ylabel;    /* formitem.ylabel */
	unsigned int xlabel;    /* formitem.xlabel */
	unsigned int yfield;    /* formitem.yfield */
	unsigned int xfield;    /* formitem.xfield */
	bool secure;            /* formitem.flags & BSDDIALOG_FIELDHIDDEN */
	bool readonly;          /* formitem.flags & BSDDIALOG_FIELDREADONLY */
	bool fieldnocolor;      /* formitem.flags & BSDDIALOG_FIELDNOCOLOR */
	bool extendfield;       /* formitem.flags & BSDDIALOG_FIELDEXTEND */
	bool fieldonebyte;      /* formitem.flags & BSDDIALOG_FIELDSINGLEBYTE */
	bool cursorend;         /* formitem.flags & BSDDIALOG_FIELDCURSOREND */
	bool cursor;            /* field cursor visibility */
	const char *bottomdesc; /* formitem.bottomdesc */

	wchar_t *privwbuf;       /* formitem.value */
	wchar_t *pubwbuf;        /* string for drawitem() */
	unsigned int maxletters; /* formitem.maxvaluelen, [priv|pub]wbuf size */
	unsigned int nletters;   /* letters in privwbuf and pubwbuf */
	unsigned int pos;        /* pos in privwbuf and pubwbuf */
	unsigned int fieldcols;  /* formitem.fieldlen */
	unsigned int xcursor;    /* position in fieldcols [0 - fieldcols-1] */
	unsigned int xposdraw;   /* first pubwbuf index to draw */
};

struct privateform {
	WINDOW *border;

	WINDOW *pad;
	unsigned int h;    /* only to create pad */
	unsigned int w;    /* only to create pad */
	unsigned int wmin; /* to refresh, w can change for FIELDEXTEND */
	unsigned int ys;   /* to refresh */
	unsigned int ye;   /* to refresh */
	unsigned int xs;   /* to refresh */
	unsigned int xe;   /* to refresh */
	unsigned int y;    /* changes moving focus around items */
	unsigned int viewrows;    /* visible rows, real formheight */
	unsigned int minviewrows; /* min viewrows, ylabel != yfield */

	wchar_t securewch; /* wide char of conf.form.secure[mb]ch */
};

enum operation {
	MOVE_CURSOR_BEGIN,
	MOVE_CURSOR_END,
	MOVE_CURSOR_RIGHT,
	MOVE_CURSOR_LEFT,
	DEL_LETTER
};

static bool fieldctl(struct privateitem *item, enum operation op)
{
	bool change;
	int width, oldwidth, nextwidth, cols;
	unsigned int i;

	change = false;
	switch (op){
	case MOVE_CURSOR_BEGIN:
		if (item->pos == 0 && item->xcursor == 0)
			break;
		/* here the cursor is changed */
		change = true;
		item->pos = 0;
		item->xcursor = 0;
		item->xposdraw = 0;
		break;
	case MOVE_CURSOR_END:
		while (fieldctl(item, MOVE_CURSOR_RIGHT))
			change = true;
		break;
	case MOVE_CURSOR_LEFT:
		if (item->pos == 0)
			break;
		/* check redundant by item->pos == 0 because of 'while' below */
		if (item->xcursor == 0 && item->xposdraw == 0)
			break;
		/* here some letter to left */
		change = true;
		item->pos -= 1;
		width = wcwidth(item->pubwbuf[item->pos]);
		if (((int)item->xcursor) - width < 0) {
			item->xcursor = 0;
			item->xposdraw -= 1;
		} else
			item->xcursor -= width;

		while (true) {
			if (item->xposdraw == 0)
				break;
			if (item->xcursor >= item->fieldcols / 2)
				break;
			if (wcwidth(item->pubwbuf[item->xposdraw - 1]) +
			    item->xcursor + width > item->fieldcols)
				break;

			item->xposdraw -= 1;
			item->xcursor +=
			    wcwidth(item->pubwbuf[item->xposdraw]);
		}
		break;
	case DEL_LETTER:
		if (item->nletters == 0)
			break;
		if (item->pos == item->nletters)
			break;
		/* here a letter under the cursor */
		change = true;
		for (i = item->pos; i < item->nletters; i++) {
			item->privwbuf[i] = item->privwbuf[i+1];
			item->pubwbuf[i] = item->pubwbuf[i+1];
		}
		item->nletters -= 1;
		item->privwbuf[i] = L'\0';
		item->pubwbuf[i] = L'\0';
		break;
	case MOVE_CURSOR_RIGHT: /* used also by "insert", see handler loop */
		if (item->pos + 1 == item->maxletters)
			break;
		if (item->pos == item->nletters)
			break;
		/* here a change to right */
		change = true;
		oldwidth = wcwidth(item->pubwbuf[item->pos]);
		item->pos += 1;
		if (item->pos == item->nletters) { /* empty column */
			nextwidth = 1;
		} else { /* a letter to right */
			nextwidth = wcwidth(item->pubwbuf[item->pos]);
		}
		if (item->xcursor + oldwidth + nextwidth - 1 >= item->fieldcols) {
			cols = nextwidth;
			item->xposdraw = item->pos;
			while (item->xposdraw != 0) {
				cols += wcwidth(item->pubwbuf[item->xposdraw - 1]);
				if (cols > (int)item->fieldcols)
					break;
				item->xposdraw -= 1;
			}
			item->xcursor = 0;
			for (i = item->xposdraw; i < item->pos ; i++)
				item->xcursor += wcwidth(item->pubwbuf[i]);
		}
		else {
			item->xcursor += oldwidth;
		}

		break;
	}

	return (change);
}

static void
drawitem(struct privateform *form, struct privateitem *item, bool focus)
{
	int color;
	unsigned int n, cols;

	/* Label */
	wattron(form->pad, t.dialog.color);
	mvwaddstr(form->pad, item->ylabel, item->xlabel, item->label);
	wattroff(form->pad, t.dialog.color);

	/* Field */
	if (item->readonly)
		color = t.form.readonlycolor;
	else if (item->fieldnocolor)
		color = t.dialog.color;
	else
		color = focus ? t.form.f_fieldcolor : t.form.fieldcolor;
	wattron(form->pad, color);
	mvwhline(form->pad, item->yfield, item->xfield, ' ', item->fieldcols);
	n = 0;
	cols = wcwidth(item->pubwbuf[item->xposdraw]);
	while (cols <= item->fieldcols && item->xposdraw + n <
	    wcslen(item->pubwbuf)) {
		n++;
		cols += wcwidth(item->pubwbuf[item->xposdraw + n]);

	}
	mvwaddnwstr(form->pad, item->yfield, item->xfield,
	    &item->pubwbuf[item->xposdraw], n);
	wattroff(form->pad, color);

	/* Bottom Desc */
	move(SCREENLINES - 1, 2);
	clrtoeol();
	if (item->bottomdesc != NULL && focus) {
		attron(t.form.bottomdesccolor);
		addstr(item->bottomdesc);
		attroff(t.form.bottomdesccolor);
		refresh();
	}

	/* Cursor */
	curs_set((focus && item->cursor) ? 1 : 0);
	wmove(form->pad, item->yfield, item->xfield + item->xcursor);

	prefresh(form->pad, form->y, 0, form->ys, form->xs, form->ye, form->xe);
}

/*
 * Trick: draw 2 times an item switching focus.
 * Problem: curses tries to optimize the rendering but sometimes it misses some
 * updates or draws old stuff. libformw has a similar problem fixed by the
 * same trick.
 * Case 1: KEY_DC and KEY_BACKSPACE, deleted multicolumn letters are drawn
 * again. It seems fixed by new items pad and prefresh(), previously WINDOW.
 * Case2: some terminal, tmux and ssh does not show the cursor.
 */
#define DRAWITEM_TRICK(form,item,focus) do {                                   \
	drawitem(form, item, !focus);                                          \
	drawitem(form, item, focus);                                           \
} while (0)

static bool
insertch(struct privateform *form, struct privateitem *item, wchar_t wch)
{
	int i;

	if (item->nletters >= item->maxletters)
		return (false);

	for (i = (int)item->nletters - 1; i >= (int)item->pos; i--) {
		item->privwbuf[i+1] = item->privwbuf[i];
		item->pubwbuf[i+1] = item->pubwbuf[i];
	}

	item->privwbuf[item->pos] = wch;
	item->pubwbuf[item->pos] = item->secure ? form->securewch : wch;
	item->nletters += 1;
	item->privwbuf[item->nletters] = L'\0';
	item->pubwbuf[item->nletters] = L'\0';

	return (true);
}

static char* alloc_wstomb(wchar_t *wstr)
{
	int len, nbytes, i;
	char mbch[MB_LEN_MAX], *mbstr;

	nbytes = MB_LEN_MAX; /* to ensure a null terminated string */
	len = wcslen(wstr);
	for (i = 0; i < len; i++) {
		wctomb(mbch, wstr[i]);
		nbytes += mblen(mbch, MB_LEN_MAX);
	}
	if((mbstr = malloc(nbytes)) == NULL)
		return (NULL);

	wcstombs(mbstr,	wstr, nbytes);

	return (mbstr);
}

static int
return_values(struct bsddialog_conf *conf, int output, int nitems,
    struct bsddialog_formitem *apiitems, struct privateitem *items)
{
	int i;

	if (output != BSDDIALOG_OK && conf->form.value_without_ok == false)
		return (output);

	for (i = 0; i < nitems; i++) {
		if (conf->form.value_wchar) {
			apiitems[i].value = (char*)wcsdup(items[i].privwbuf);
		} else {
			apiitems[i].value = alloc_wstomb(items[i].privwbuf);
		}
		if (apiitems[i].value == NULL)
			RETURN_ERROR("Cannot allocate memory for form value");
	}

	return (output);
}

static unsigned int firstitem(unsigned int nitems, struct privateitem *items)
{
	int i;

	for (i = 0; i < (int)nitems; i++)
		if (items[i].readonly == false)
			break;

	return (i);
}

static unsigned int lastitem(unsigned int nitems, struct privateitem *items)
{
	int i;

	for (i = nitems - 1; i >= 0 ; i--)
		if (items[i].readonly == false)
			break;

	return (i);
}

static unsigned int
previtem(unsigned int nitems, struct privateitem *items, int curritem)
{
	int i;

	for (i = curritem - 1; i >= 0; i--)
		if (items[i].readonly == false)
			return(i);

	for (i = nitems - 1; i > curritem - 1; i--)
		if (items[i].readonly == false)
			return(i);

	return (curritem);
}

static unsigned int
nextitem(unsigned int nitems, struct privateitem *items, int curritem)
{
	int i;

	for (i = curritem + 1; i < (int)nitems; i++)
		if (items[i].readonly == false)
			return(i);

	for (i = 0; i < curritem; i++)
		if (items[i].readonly == false)
			return(i);

	return (curritem);
}

static void
redrawbuttons(WINDOW *window, struct buttons *bs, bool focus, bool shortcut)
{
	int selected;

	selected = bs->curr;
	if (focus == false)
		bs->curr = -1;
	draw_buttons(window, *bs, shortcut);
	wrefresh(window);
	bs->curr = selected;
}

static void
update_formborders(struct bsddialog_conf *conf, struct privateform *form)
{
	int h, w;

	getmaxyx(form->border, h, w);
	draw_borders(conf, form->border, h, w, LOWERED);

	if (form->viewrows < form->h) {
		wattron(form->border, t.dialog.arrowcolor);
		if (form->y > 0)
			mvwhline(form->border, 0, (w / 2) - 2,
			    conf->ascii_lines ? '^' : ACS_UARROW, 5);

		if (form->y + form->viewrows < form->h)
			mvwhline(form->border, h-1, (w / 2) - 2,
			    conf->ascii_lines ? 'v' : ACS_DARROW, 5);
		wattroff(form->border, t.dialog.arrowcolor);
		wrefresh(form->border);
	}
}

/* use menu autosizing, linelen = form.w, nitems = form.h */
static int
menu_autosize(struct bsddialog_conf *conf, int rows, int cols, int *h, int *w,
    const char *text, int linelen, unsigned int *menurows, int nitems,
    struct buttons bs)
{
	int htext, wtext, menusize, notext;

	notext = 2;
	if (*menurows == BSDDIALOG_AUTOSIZE) {
		/* algo 1): grows vertically */
		/* notext = 1; */
		/* algo 2): grows horizontally, better with little screens */
		notext += nitems;
		notext = MIN(notext, widget_max_height(conf) - HBORDERS - 3);
	} else
		notext += *menurows;

	if (text_size(conf, rows, cols, text, &bs, notext, linelen + 4, &htext,
	    &wtext) != 0)
		return (BSDDIALOG_ERROR);

	if (cols == BSDDIALOG_AUTOSIZE)
		*w = widget_min_width(conf, wtext, linelen + 4, &bs);

	if (rows == BSDDIALOG_AUTOSIZE) {
		if (*menurows == BSDDIALOG_AUTOSIZE) {
			menusize = widget_max_height(conf) - HBORDERS -
			     2 /*buttons*/ - htext;
			menusize = MIN(menusize, nitems + 2);
			*menurows = menusize - 2 < 0 ? 0 : menusize - 2;
		} else /* h autosize with fixed menurows */
			menusize = *menurows + 2;

		*h = widget_min_height(conf, htext, menusize, true);
	} else { /* fixed rows */
		if (*menurows == BSDDIALOG_AUTOSIZE) {
			if (*h - 6 - htext <= 0)
				*menurows = 0; /* form_checksize() will check */
			else
				*menurows = MIN(*h-6-htext, nitems);
		}
	}

	/* avoid menurows overflow and menurows becomes at most menurows */
	if (*h - 6 - htext <= 0)
		*menurows = 0; /* form_checksize() will check */
	else
		*menurows = MIN(*h - 6 - htext, (int)*menurows);

	return (0);
}

static int
form_checksize(int rows, int cols, const char *text, struct privateform *form,
    int nitems, struct buttons bs)
{
	int mincols, textrow, menusize;

	/* cols */
	mincols = VBORDERS;
	mincols += buttons_min_width(bs);
	mincols = MAX(mincols, (int)form->w + 6);

	if (cols < mincols)
		RETURN_ERROR("Form width, cols < buttons or xlabels/xfields");

	/* rows */
	if (nitems > 0 && form->viewrows == 0)
		RETURN_ERROR("items > 0 but viewrows == 0, if formheight = 0 "
		    "terminal too small");

	if (form->viewrows < form->minviewrows)
		RETURN_ERROR("Few formheight rows, if formheight = 0 terminal "
		    "too small");

	textrow = text != NULL && text[0] != '\0' ? 1 : 0;
	menusize = nitems > 0 ? 3 : 0;
	if (rows < 2  + 2 + menusize + textrow)
		RETURN_ERROR("Few lines for this form");

	return (0);
}

static void curriteminview(struct privateform *form, struct privateitem *item)
{
	unsigned int yup, ydown;

	yup = MIN(item->ylabel, item->yfield);
	ydown = MAX(item->ylabel, item->yfield);

	if (form->y > yup && form->y > 0)
		form->y = yup;
	if ((int)(form->y + form->viewrows) - 1 < (int)ydown)
		form->y = ydown - form->viewrows + 1;
}

/* API */
int
bsddialog_form(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int formheight, unsigned int nitems,
    struct bsddialog_formitem *apiitems)
{
	bool switchfocus, changeitem, focusinform, insecurecursor, loop;
	int curritem, mbchsize, next, retval, y, x, h, w, wchtype;
	unsigned int i, j, itemybeg, itemxbeg, tmp;
	wchar_t *winit;
	wint_t input;
	WINDOW *widget, *textpad, *shadow;
	struct privateitem *items, *item;
	struct buttons bs;
	struct privateform form;

	for (i = 0; i < nitems; i++) {
		if (apiitems[i].maxvaluelen == 0)
			RETURN_ERROR("maxvaluelen cannot be zero");
		if (apiitems[i].fieldlen == 0)
			RETURN_ERROR("fieldlen cannot be zero");
	}

	insecurecursor = false;
	if (conf->form.securembch != NULL) {
		mbchsize = mblen(conf->form.securembch, MB_LEN_MAX);
		if(mbtowc(&form.securewch, conf->form.securembch, mbchsize) < 0)
			RETURN_ERROR("Cannot convert securembch to wchar_t");
		insecurecursor = true;
	} else if (conf->form.securech != '\0') {
		form.securewch = btowc(conf->form.securech);
		insecurecursor = true;
	} else {
		form.securewch = L' ';
	}

	if ((items = malloc(nitems * sizeof(struct privateitem))) == NULL)
		RETURN_ERROR("Cannot allocate internal items");
	form.h = form.w = form.minviewrows = 0;
	for (i = 0; i < nitems; i++) {
		item = &items[i];
		item->label = apiitems[i].label;
		item->ylabel = apiitems[i].ylabel;
		item->xlabel = apiitems[i].xlabel;
		item->yfield = apiitems[i].yfield;
		item->xfield = apiitems[i].xfield;
		item->secure = apiitems[i].flags & BSDDIALOG_FIELDHIDDEN;
		item->readonly = apiitems[i].flags & BSDDIALOG_FIELDREADONLY;
		item->fieldnocolor = apiitems[i].flags & BSDDIALOG_FIELDNOCOLOR;
		item->extendfield = apiitems[i].flags & BSDDIALOG_FIELDEXTEND;
		item->fieldonebyte = apiitems[i].flags &
		    BSDDIALOG_FIELDSINGLEBYTE;
		item->cursorend = apiitems[i].flags & BSDDIALOG_FIELDCURSOREND;
		item->bottomdesc = apiitems[i].bottomdesc;
		if (item->readonly || (item->secure && !insecurecursor))
			item->cursor = false;
		else
			item->cursor = true;

		item->maxletters = apiitems[i].maxvaluelen;
		item->privwbuf = calloc(item->maxletters + 1, sizeof(wchar_t));
		if (item->privwbuf == NULL)
			RETURN_ERROR("Cannot allocate item private buffer");
		memset(item->privwbuf, 0, item->maxletters + 1);
		item->pubwbuf = calloc(item->maxletters + 1, sizeof(wchar_t));
		if (item->pubwbuf == NULL)
			RETURN_ERROR("Cannot allocate item private buffer");
		memset(item->pubwbuf, 0, item->maxletters + 1);

		if ((winit = alloc_mbstows(apiitems[i].init)) == NULL)
			RETURN_ERROR("Cannot allocate item.init in wchar_t*");
		wcsncpy(item->privwbuf, winit, item->maxletters);
		wcsncpy(item->pubwbuf, winit, item->maxletters);
		free(winit);
		item->nletters = wcslen(item->pubwbuf);
		if (item->secure) {
			for (j = 0; j < item->nletters; j++)
				item->pubwbuf[j] = form.securewch;
		}

		item->fieldcols = apiitems[i].fieldlen;
		item->xposdraw = 0;
		item->xcursor = 0;
		item->pos = 0;

		form.h = MAX(form.h, items[i].ylabel);
		form.h = MAX(form.h, items[i].yfield);
		form.w = MAX(form.w, items[i].xlabel + strcols(items[i].label));
		form.w = MAX(form.w, items[i].xfield + items[i].fieldcols);
		if (i == 0) {
			itemybeg = MIN(items[i].ylabel, items[i].yfield);
			itemxbeg = MIN(items[i].xlabel, items[i].xfield);
		} else {
			tmp = MIN(items[i].ylabel, items[i].yfield);
			itemybeg = MIN(itemybeg, tmp);
			tmp = MIN(items[i].xlabel, items[i].xfield);
			itemxbeg = MIN(itemxbeg, tmp);
		}
		tmp = abs((int)items[i].ylabel - (int)items[i].yfield);
		form.minviewrows = MAX(form.minviewrows, tmp);
	}
	if (nitems > 0) {
		form.h = form.h + 1 - itemybeg;
		form.w -= itemxbeg;
		form.minviewrows += 1;
	}
	form.wmin = form.w;
	for (i = 0; i < nitems; i++) {
		items[i].ylabel -= itemybeg;
		items[i].yfield -= itemybeg;
		items[i].xlabel -= itemxbeg;
		items[i].xfield -= itemxbeg;
	}

	get_buttons(conf, &bs, BUTTON_OK_LABEL, BUTTON_CANCEL_LABEL);
	form.viewrows = formheight;

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (menu_autosize(conf, rows, cols, &h, &w, text, form.w,
	    &form.viewrows, form.h, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (form_checksize(h, w, text, &form, nitems, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, &bs,
	    true) != 0)
		return (BSDDIALOG_ERROR);

	doupdate();

	prefresh(textpad, 0, 0, y + 1, x + 1 + TEXTHMARGIN,
	    y + h - form.viewrows, x + 1 + w - TEXTHMARGIN);

	form.border = new_boxed_window(conf, y + h - 5 - form.viewrows, x + 2,
	    form.viewrows + 2, w - 4, LOWERED);

	for (i = 0; i < nitems; i++) {
		if (items[i].extendfield) {
			form.w = w - 6;
			items[i].fieldcols = form.w - items[i].xfield;
		}
		if (items[i].cursorend)
			fieldctl(item, MOVE_CURSOR_END);
	}

	form.pad = newpad(form.h, form.w);
	wbkgd(form.pad, t.dialog.color);

	form.ys = y + h - 5 - form.viewrows + 1;
	form.ye = y + h - 5 ;
	if ((int)form.w >= w - 6) { /* left */
		form.xs = x + 3;
		form.xe = form.xs + w - 7;
	} else { /* center */
		form.xs = x + 3 + (w-6)/2 - form.w/2;
		form.xe = form.xs + w - 5;
	}

	curritem = -1;
	for (i=0 ; i < nitems; i++) {
		DRAWITEM_TRICK(&form, &items[i], false);
		if (curritem == -1 && items[i].readonly == false)
			curritem = i;
	}
	if (curritem != -1) {
		focusinform = true;
		redrawbuttons(widget, &bs, conf->button.always_active, false);
		form.y = 0;
		item = &items[curritem];
		curriteminview(&form, item);
		update_formborders(conf, &form);
		wrefresh(form.border);
		DRAWITEM_TRICK(&form, item, true);
	} else {
		item = NULL;
		focusinform = false;
		wrefresh(form.border);
	}

	changeitem = switchfocus = false;
	loop = true;
	while (loop) {
		if ((wchtype = get_wch(&input)) == ERR)
			continue;
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			if (focusinform && conf->button.always_active == false)
				break;
			retval = return_values(conf, bs.value[bs.curr],
			    nitems, apiitems, items);
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = return_values(conf, BSDDIALOG_ESC,
				    nitems, apiitems, items);
				loop = false;
			}
			break;
		case '\t': /* TAB */
			if (focusinform) {
				switchfocus = true;
			} else {
				if (bs.curr + 1 < (int)bs.nbuttons) {
					bs.curr++;
				} else {
					bs.curr = 0;
					if (curritem != -1) {
						switchfocus = true;
					}
				}
				draw_buttons(widget, bs, true);
				wrefresh(widget);
			}
			break;
		case KEY_LEFT:
			if (focusinform) {
				if(fieldctl(item, MOVE_CURSOR_LEFT))
					DRAWITEM_TRICK(&form, item, true);
			} else if (bs.curr > 0) {
				bs.curr--;
				draw_buttons(widget, bs, true);
				wrefresh(widget);
			} else if (curritem != -1) {
				switchfocus = true;
			}
			break;
		case KEY_RIGHT:
			if (focusinform) {
				if(fieldctl(item, MOVE_CURSOR_RIGHT))
					DRAWITEM_TRICK(&form, item, true);
			} else if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				draw_buttons(widget, bs, true);
				wrefresh(widget);
			} else if (curritem != -1) {
				switchfocus = true;
			}
			break;
		case KEY_UP:
			if (focusinform) {
				next = previtem(nitems, items, curritem);
				changeitem = curritem != next;
			} else if (curritem != -1) {
				switchfocus = true;
			}
			break;
		case KEY_DOWN:
			if (focusinform == false)
				break;
			if (nitems == 1) {
				switchfocus = true;
			} else {
				next = nextitem(nitems, items, curritem);
				changeitem = curritem != next;
			}
			break;
		case KEY_PPAGE:
			if (focusinform) {
				next = firstitem(nitems, items);
				changeitem = curritem != next;
			}
			break;
		case KEY_NPAGE:
			if (focusinform) {
				next = lastitem(nitems, items);
				changeitem = curritem != next;
			}
			break;
		case KEY_BACKSPACE:
		case 127: /* Backspace */
			if (focusinform == false)
				break;
			if(fieldctl(item, MOVE_CURSOR_LEFT))
				if(fieldctl(item, DEL_LETTER))
					DRAWITEM_TRICK(&form, item, true);
			break;
		case KEY_DC:
			if (focusinform == false)
				break;
			if(fieldctl(item, DEL_LETTER))
				DRAWITEM_TRICK(&form, item, true);
			break;
		case KEY_HOME:
			if (focusinform == false)
				break;
			if(fieldctl(item, MOVE_CURSOR_BEGIN))
				DRAWITEM_TRICK(&form, item, true);
			break;
		case KEY_END:
			if (focusinform == false)
				break;
			if (fieldctl(item, MOVE_CURSOR_END))
				DRAWITEM_TRICK(&form, item, true);
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			curs_set(0);
			if (f1help(conf) != 0) {
				retval = BSDDIALOG_ERROR;
				loop = false;
			}
			/* No break, screen size can change */
		case KEY_RESIZE:
			/* Important for decreasing screen */
			hide_widget(y, x, h, w, conf->shadow);
			refresh();

			form.viewrows = formheight;
			form.w = form.wmin;
			if (set_widget_size(conf, rows, cols, &h, &w) != 0)
				return (BSDDIALOG_ERROR);
			if (menu_autosize(conf, rows, cols, &h, &w, text, form.w,
			    &form.viewrows, form.h, bs) != 0)
				return (BSDDIALOG_ERROR);
			if (form_checksize(h, w, text, &form, nitems, bs) != 0)
				return (BSDDIALOG_ERROR);
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return (BSDDIALOG_ERROR);

			if (update_dialog(conf, shadow, widget, y, x, h, w,
			    textpad, text, &bs, true) != 0)
			return (BSDDIALOG_ERROR);

			doupdate();

			prefresh(textpad, 0, 0, y + 1, x + 1 + TEXTHMARGIN,
			    y + h - form.viewrows, x + 1 + w - TEXTHMARGIN);

			wclear(form.border);
			mvwin(form.border, y + h - 5 - form.viewrows, x + 2);
			wresize(form.border, form.viewrows + 2, w - 4);

			for (i = 0; i < nitems; i++) {
				fieldctl(&items[i], MOVE_CURSOR_BEGIN);
				if (items[i].extendfield) {
					form.w = w - 6;
					items[i].fieldcols =
					    form.w - items[i].xfield;
				}
				if (items[i].cursorend)
					fieldctl(&items[i], MOVE_CURSOR_END);
			}

			form.ys = y + h - 5 - form.viewrows + 1;
			form.ye = y + h - 5 ;
			if ((int)form.w >= w - 6) { /* left */
				form.xs = x + 3;
				form.xe = form.xs + w - 7;
			} else { /* center */
				form.xs = x + 3 + (w-6)/2 - form.w/2;
				form.xe = form.xs + w - 5;
			}

			if (curritem != -1) {
				redrawbuttons(widget, &bs,
				    conf->button.always_active || !focusinform,
				    !focusinform);
				curriteminview(&form, item);
				update_formborders(conf, &form);
				wrefresh(form.border);
				/* drawitem just to prefresh() pad */
				DRAWITEM_TRICK(&form, item, focusinform);
			} else {
				wrefresh(form.border);
			}
			break;
		default:
			if (wchtype == KEY_CODE_YES)
				break;
			if (focusinform) {
				if (item->fieldonebyte && wctob(input) == EOF)
					break;
				/*
				 * MOVE_CURSOR_RIGHT manages new positions
				 * because the cursor remains on the new letter,
				 * "if" and "while" update the positions.
				 */
				if(insertch(&form, item, input)) {
					fieldctl(item, MOVE_CURSOR_RIGHT);
					/*
					 * no if(fieldctl), update always
					 * because it fails with maxletters.
					 */
					DRAWITEM_TRICK(&form, item, true);
				}
			} else {
				if (shortcut_buttons(input, &bs)) {
					retval = return_values(conf,
					    bs.value[bs.curr], nitems, apiitems,
					    items);
					loop = false;
				}
			}
			break;
		} /* end switch handler */

		if (switchfocus) {
			focusinform = !focusinform;
			bs.curr = 0;
			redrawbuttons(widget, &bs,
			    conf->button.always_active || !focusinform,
			    !focusinform);
			DRAWITEM_TRICK(&form, item, focusinform);
			switchfocus = false;
		}

		if (changeitem) {
			DRAWITEM_TRICK(&form, item, false);
			curritem = next;
			item = &items[curritem];
			curriteminview(&form, item);
			update_formborders(conf, &form);
			DRAWITEM_TRICK(&form, item, true);
			changeitem = false;
		}
	} /* end while handler */

	curs_set(0);

	delwin(form.pad);
	delwin(form.border);
	for (i = 0; i < nitems; i++) {
		free(items[i].privwbuf);
		free(items[i].pubwbuf);
	}
	end_dialog(conf, shadow, widget, textpad);

	return (retval);
}
