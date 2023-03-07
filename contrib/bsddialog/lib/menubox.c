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

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define MIN_HEIGHT  VBORDERS + 6 /* 2 buttons 1 text 3 menu */

enum menumode {
	CHECKLISTMODE,
	MENUMODE,
	MIXEDLISTMODE,
	RADIOLISTMODE,
	SEPARATORMODE
};

struct lineposition {
	unsigned int maxsepstr;
	unsigned int maxprefix;
	unsigned int xselector;
	unsigned int selectorlen;
	unsigned int maxdepth;
	unsigned int xname;
	unsigned int maxname;
	unsigned int xdesc;
	unsigned int maxdesc;
	unsigned int line;
};

struct privateitem {
	bool on;
	int group;
	int index;
	enum menumode type;
	struct bsddialog_menuitem *item;
};

static void
set_on_output(struct bsddialog_conf *conf, int output, int ngroups,
    struct bsddialog_menugroup *groups, struct privateitem *pritems)
{
	int i, j, abs;

	if (output != BSDDIALOG_OK && !conf->menu.on_without_ok)
		return;

	for(i = abs = 0; i < ngroups; i++) {
		if (groups[i].type == BSDDIALOG_SEPARATOR) {
			abs += groups[i].nitems;
			continue;
		}

		for(j = 0; j < (int)groups[i].nitems; j++) {
			groups[i].items[j].on = pritems[abs].on;
			abs++;
		}
	}
}

static int getprev(struct privateitem *pritems, int abs)
{
	int i;

	for (i = abs - 1; i >= 0; i--) {
		if (pritems[i].type == SEPARATORMODE)
			continue;
		return (i);
	}

	return (abs);
}

static int getnext(int npritems, struct privateitem *pritems, int abs)
{
	int i;

	for (i = abs + 1; i < npritems; i++) {
		if (pritems[i].type == SEPARATORMODE)
			continue;
		return (i);
	}

	return (abs);
}

static int
getfirst_with_default(int npritems, struct privateitem *pritems, int ngroups,
    struct bsddialog_menugroup *groups, int *focusgroup, int *focusitem)
{
	int i, abs;

	if ((abs =  getnext(npritems, pritems, -1)) < 0)
		return (abs);

	if (focusgroup == NULL || focusitem == NULL)
		return (abs);
	if (*focusgroup < 0 || *focusgroup >= ngroups)
		return (abs);
	if (groups[*focusgroup].type == BSDDIALOG_SEPARATOR)
		return (abs);
	if (*focusitem < 0 || *focusitem >= (int)groups[*focusgroup].nitems)
		return (abs);

	for (i = abs; i < npritems; i++) {
		if (pritems[i].group == *focusgroup &&
		    pritems[i].index == *focusitem)
			return (i);
	}

	return (abs);
}

static int
getfastnext(int menurows, int npritems, struct privateitem *pritems, int abs)
{
	int a, start, i;

	start = abs;
	i = menurows;
	do {
		a = abs;
		abs = getnext(npritems, pritems, abs);
		i--;
	} while (abs != a && abs < start + menurows && i > 0);

	return (abs);
}

static int
getfastprev(int menurows, struct privateitem *pritems, int abs)
{
	int a, start, i;

	start = abs;
	i = menurows;
	do {
		a = abs;
		abs = getprev(pritems, abs);
		i--;
	} while (abs != a && abs > start - menurows && i > 0);

	return (abs);
}

static int
getnextshortcut(struct bsddialog_conf *conf, int npritems,
    struct privateitem *pritems, int abs, wint_t key)
{
	int i, next;
	wchar_t wch;

	next = -1;
	for (i = 0; i < npritems; i++) {
		if (pritems[i].type == SEPARATORMODE)
			continue;

		if (conf->menu.no_name)
			mbtowc(&wch, pritems[i].item->desc, MB_CUR_MAX);
		else
			mbtowc(&wch, pritems[i].item->name, MB_CUR_MAX);

		if (wch == (wchar_t)key) {
			if (i > abs)
				return (i);

			if (i < abs && next == -1)
				next = i;
		}
	}

	return (next != -1 ? next : abs);
}

static enum menumode
getmode(enum menumode mode, struct bsddialog_menugroup group)
{
	if (mode == MIXEDLISTMODE) {
		if (group.type == BSDDIALOG_SEPARATOR)
			mode = SEPARATORMODE;
		else if (group.type == BSDDIALOG_RADIOLIST)
			mode = RADIOLISTMODE;
		else if (group.type == BSDDIALOG_CHECKLIST)
			mode = CHECKLISTMODE;
	}

	return (mode);
}

static void
drawseparators(struct bsddialog_conf *conf, WINDOW *pad, int linelen,
    int nitems, struct privateitem *pritems)
{
	int i, linech, labellen;
	const char *desc, *name;

	for (i = 0; i < nitems; i++) {
		if (pritems[i].type != SEPARATORMODE)
			continue;
		if (conf->no_lines == false) {
			wattron(pad, t.menu.desccolor);
			linech = conf->ascii_lines ? '-' : ACS_HLINE;
			mvwhline(pad, i, 0, linech, linelen);
			wattroff(pad, t.menu.desccolor);
		}
		name = pritems[i].item->name;
		desc = pritems[i].item->desc;
		labellen = strcols(name) + strcols(desc) + 1;
		wmove(pad, i, labellen < linelen ? linelen/2 - labellen/2 : 0);
		wattron(pad, t.menu.namesepcolor);
		waddstr(pad, name);
		wattroff(pad, t.menu.namesepcolor);
		if (strcols(name) > 0 && strcols(desc) > 0)
			waddch(pad, ' ');
		wattron(pad, t.menu.descsepcolor);
		waddstr(pad, desc);
		wattroff(pad, t.menu.descsepcolor);
	}
}

static void
drawitem(struct bsddialog_conf *conf, WINDOW *pad, int y,
    struct lineposition pos, struct privateitem *pritem, bool focus)
{
	int colordesc, colorname, colorshortcut;
	wchar_t shortcut;
	struct bsddialog_menuitem *item;

	item = pritem->item;

	/* prefix */
	if (item->prefix != NULL && item->prefix[0] != '\0')
		mvwaddstr(pad, y, 0, item->prefix);

	/* selector */
	wmove(pad, y, pos.xselector);
	wattron(pad, focus ? t.menu.f_selectorcolor : t.menu.selectorcolor);
	if (pritem->type == CHECKLISTMODE)
		wprintw(pad, "[%c]", pritem->on ? 'X' : ' ');
	if (pritem->type == RADIOLISTMODE)
		wprintw(pad, "(%c)", pritem->on ? '*' : ' ');
	wattroff(pad, focus ? t.menu.f_selectorcolor : t.menu.selectorcolor);

	/* name */
	colorname = focus ? t.menu.f_namecolor : t.menu.namecolor;
	if (conf->menu.no_name == false) {
		wattron(pad, colorname);
		mvwaddstr(pad, y, pos.xname + item->depth, item->name);
		wattroff(pad, colorname);
	}

	/* description */
	if (conf->menu.no_name)
		colordesc = focus ? t.menu.f_namecolor : t.menu.namecolor;
	else
		colordesc = focus ? t.menu.f_desccolor : t.menu.desccolor;

	if (conf->menu.no_desc == false) {
		wattron(pad, colordesc);
		if (conf->menu.no_name)
			mvwaddstr(pad, y, pos.xname + item->depth, item->desc);
		else
			mvwaddstr(pad, y, pos.xdesc, item->desc);
		wattroff(pad, colordesc);
	}

	/* shortcut */
	if (conf->menu.shortcut_buttons == false) {
		colorshortcut = focus ?
		    t.menu.f_shortcutcolor : t.menu.shortcutcolor;
		wattron(pad, colorshortcut);

		if (conf->menu.no_name)
			mbtowc(&shortcut, item->desc, MB_CUR_MAX);
		else
			mbtowc(&shortcut, item->name, MB_CUR_MAX);
		mvwaddwch(pad, y, pos.xname + item->depth, shortcut);
		wattroff(pad, colorshortcut);
	}

	/* bottom description */
	move(SCREENLINES - 1, 2);
	clrtoeol();
	if (item->bottomdesc != NULL && focus) {
		attron(t.menu.bottomdesccolor);
		addstr(item->bottomdesc);
		attroff(t.menu.bottomdesccolor);
		refresh();
	}
}

/* the caller has to call prefresh(menupad, ymenupad, 0, ys, xs, ye, xe); */
static void
update_menuwin(struct bsddialog_conf *conf, WINDOW *menuwin, int h, int w,
    int totnitems, unsigned int menurows, int ymenupad)
{
	draw_borders(conf, menuwin, h, w, LOWERED);

	if (totnitems > (int)menurows) {
		wattron(menuwin, t.dialog.arrowcolor);
		if (ymenupad > 0)
			mvwhline(menuwin, 0, 2,
			    conf->ascii_lines ? '^' : ACS_UARROW, 3);

		if ((ymenupad + (int)menurows) < totnitems)
			mvwhline(menuwin, h-1, 2,
			    conf->ascii_lines ? 'v' : ACS_DARROW, 3);

		mvwprintw(menuwin, h-1, w-6, "%3d%%",
		    100 * (ymenupad + menurows) / totnitems);
		wattroff(menuwin, t.dialog.arrowcolor);
	}
}

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
				*menurows = 0; /* menu_checksize() will check */
			else
				*menurows = MIN(*h-6-htext, nitems);
		}
	}

	/* avoid menurows overflow and menurows becomes at most menurows */
	if (*h - 6 - htext <= 0)
		*menurows = 0; /* menu_checksize() will check */
	else
		*menurows = MIN(*h - 6 - htext, (int)*menurows);

	return (0);
}

static int
menu_checksize(int rows, int cols, const char *text, int menurows, int nitems,
    struct buttons bs)
{
	int mincols, textrow, menusize;

	mincols = VBORDERS;
	/* buttons */
	mincols += buttons_min_width(bs);
	/*
	 * linelen check, comment to allow some hidden col otherwise portconfig
	 * could not show big menus like www/apache24
	 */
	/* mincols = MAX(mincols, linelen); */

	if (cols < mincols)
		RETURN_ERROR("Few cols, width < size buttons or "
		    "name + descripion of the items");

	textrow = text != NULL && text[0] != '\0' ? 1 : 0;

	if (nitems > 0 && menurows == 0)
		RETURN_ERROR("items > 0 but menurows == 0, if menurows = 0 "
		    "terminal too small");

	menusize = nitems > 0 ? 3 : 0;
	if (rows < 2  + 2 + menusize + textrow)
		RETURN_ERROR("Few lines for this menus");

	return (0);
}

static int
do_mixedlist(struct bsddialog_conf *conf, const char *text, int rows, int cols,
    unsigned int menurows, enum menumode mode, unsigned int ngroups,
    struct bsddialog_menugroup *groups, int *focuslist, int *focusitem)
{
	bool loop, onetrue, movefocus, automenurows, shortcut_butts;
	int i, j, y, x, h, w, retval;
	int ymenupad, ys, ye, xs, xe, abs, next, totnitems;
	wint_t input;
	WINDOW  *shadow, *widget, *textpad, *menuwin, *menupad;
	struct buttons bs;
	struct lineposition pos = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	struct bsddialog_menuitem *item;
	struct privateitem *pritems;

	shortcut_butts = conf->menu.shortcut_buttons;

	automenurows = (menurows == BSDDIALOG_AUTOSIZE) ? true : false;

	totnitems = 0;
	for (i = 0; i < (int)ngroups; i++) {
		if (getmode(mode, groups[i]) == RADIOLISTMODE ||
		    getmode(mode, groups[i]) == CHECKLISTMODE)
			pos.selectorlen = 3;

		for (j = 0; j < (int)groups[i].nitems; j++) {
			totnitems++;
			item = &groups[i].items[j];

			if (groups[i].type == BSDDIALOG_SEPARATOR) {
				pos.maxsepstr = MAX(pos.maxsepstr,
				    strcols(item->name) + strcols(item->desc));
				continue;
			}

			pos.maxprefix = MAX(pos.maxprefix,strcols(item->prefix));
			pos.maxdepth  = MAX(pos.maxdepth, item->depth);
			pos.maxname   = MAX(pos.maxname, strcols(item->name));
			pos.maxdesc   = MAX(pos.maxdesc, strcols(item->desc));
		}
	}
	pos.maxname = conf->menu.no_name ? 0 : pos.maxname;
	pos.maxdesc = conf->menu.no_desc ? 0 : pos.maxdesc;

	pos.xselector = pos.maxprefix + (pos.maxprefix != 0 ? 1 : 0);
	pos.xname = pos.xselector + pos.selectorlen +
	    (pos.selectorlen > 0 ? 1 : 0);
	pos.xdesc = pos.maxdepth + pos.xname + pos.maxname;
	pos.xdesc += (pos.maxname != 0 ? 1 : 0);
	pos.line = MAX(pos.maxsepstr + 3, pos.xdesc + pos.maxdesc);

	get_buttons(conf, &bs, BUTTON_OK_LABEL, BUTTON_CANCEL_LABEL);

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return (BSDDIALOG_ERROR);
	if (menu_autosize(conf, rows, cols, &h, &w, text, pos.line, &menurows,
	    totnitems, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (menu_checksize(h, w, text, menurows, totnitems, bs) != 0)
		return (BSDDIALOG_ERROR);
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return (BSDDIALOG_ERROR);

	if (new_dialog(conf, &shadow, &widget, y, x, h, w, &textpad, text, &bs,
	     shortcut_butts) != 0)
		return (BSDDIALOG_ERROR);

	doupdate();

	prefresh(textpad, 0, 0, y + 1, x + 1 + TEXTHMARGIN, y + h - menurows,
	    x + 1 + w - TEXTHMARGIN);

	menuwin = new_boxed_window(conf, y + h - 5 - menurows, x + 2,
	    menurows + 2, w - 4, LOWERED);

	menupad = newpad(totnitems, pos.line);
	wbkgd(menupad, t.dialog.color);

	if ((pritems = calloc(totnitems, sizeof (struct privateitem))) == NULL)
		RETURN_ERROR("Cannot allocate memory for internal menu items");

	abs = 0;
	for (i = 0; i < (int)ngroups; i++) {
		onetrue = false;
		for (j = 0; j < (int)groups[i].nitems; j++) {
			item = &groups[i].items[j];

			if (getmode(mode, groups[i]) == MENUMODE) {
				pritems[abs].on = false;
			} else if (getmode(mode, groups[i]) == RADIOLISTMODE) {
				pritems[abs].on = onetrue ? false : item->on;
				if (pritems[abs].on)
					onetrue = true;
			} else {
				pritems[abs].on = item->on;
			}
			pritems[abs].group = i;
			pritems[abs].index = j;
			pritems[abs].type = getmode(mode, groups[i]);
			pritems[abs].item = item;

			drawitem(conf, menupad, abs, pos, &pritems[abs], false);
			abs++;
		}
	}
	drawseparators(conf, menupad, MIN((int)pos.line, w-6), totnitems,
	    pritems);
	abs = getfirst_with_default(totnitems, pritems, ngroups, groups,
	    focuslist, focusitem);
	if (abs >= 0)
		drawitem(conf, menupad, abs, pos, &pritems[abs], true);

	ys = y + h - 5 - menurows + 1;
	ye = y + h - 5 ;
	if (conf->menu.align_left || (int)pos.line > w - 6) {
		xs = x + 3;
		xe = xs + w - 7;
	} else { /* center */
		xs = x + 3 + (w-6)/2 - pos.line/2;
		xe = xs + w - 5;
	}

	ymenupad = 0;
	if ((int)(ymenupad + menurows) - 1 < abs)
		ymenupad = abs - menurows + 1;
	update_menuwin(conf, menuwin, menurows+2, w-4, totnitems, menurows,
	    ymenupad);
	wrefresh(menuwin);
	prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);

	movefocus = false;
	loop = true;
	while (loop) {
		if (get_wch(&input) == ERR)
			continue;
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			retval = bs.value[bs.curr];
			if (abs >= 0 && pritems[abs].type == MENUMODE)
				pritems[abs].on = true;
			set_on_output(conf, retval, ngroups, groups, pritems);
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				if (abs >= 0 && pritems[abs].type == MENUMODE)
					pritems[abs].on = true;
				set_on_output(conf, retval, ngroups, groups,
				    pritems);
				loop = false;
			}
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			draw_buttons(widget, bs, shortcut_butts);
			wrefresh(widget);
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				draw_buttons(widget, bs, shortcut_butts);
				wrefresh(widget);
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				draw_buttons(widget, bs, shortcut_butts);
				wrefresh(widget);
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
			menurows = automenurows ? 0 : menurows;
			if (menu_autosize(conf, rows, cols, &h, &w, text,
			    pos.line, &menurows, totnitems, bs) != 0)
				return (BSDDIALOG_ERROR);
			if (menu_checksize(h, w, text, menurows, totnitems,
			    bs) != 0)
				return (BSDDIALOG_ERROR);
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return (BSDDIALOG_ERROR);

			if (update_dialog(conf, shadow, widget, y, x, h, w,
			    textpad, text, &bs, shortcut_butts) != 0)
				return (BSDDIALOG_ERROR);

			doupdate();

			prefresh(textpad, 0, 0, y + 1, x + 1 + TEXTHMARGIN,
			    y + h - menurows, x + 1 + w - TEXTHMARGIN);

			wclear(menuwin);
			mvwin(menuwin, y + h - 5 - menurows, x + 2);
			wresize(menuwin,menurows+2, w-4);
			update_menuwin(conf, menuwin, menurows+2, w-4,
			    totnitems, menurows, ymenupad);
			wrefresh(menuwin);

			ys = y + h - 5 - menurows + 1;
			ye = y + h - 5 ;
			if (conf->menu.align_left || (int)pos.line > w - 6) {
				xs = x + 3;
				xe = xs + w - 7;
			} else { /* center */
				xs = x + 3 + (w-6)/2 - pos.line/2;
				xe = xs + w - 5;
			}

			drawseparators(conf, menupad, MIN((int)pos.line, w-6),
			    totnitems, pritems);

			if ((int)(ymenupad + menurows) - 1 < abs)
				ymenupad = abs - menurows + 1;
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);

			refresh();

			break;
		}

		if (abs < 0)
			continue;
		switch(input) {
		case KEY_HOME:
			next = getnext(totnitems, pritems, -1);
			movefocus = next != abs;
			break;
		case KEY_UP:
			next = getprev(pritems, abs);
			movefocus = next != abs;
			break;
		case KEY_PPAGE:
			next = getfastprev(menurows, pritems, abs);
			movefocus = next != abs;
			break;
		case KEY_END:
			next = getprev(pritems, totnitems);
			movefocus = next != abs;
			break;
		case KEY_DOWN:
			next = getnext(totnitems, pritems, abs);
			movefocus = next != abs;
			break;
		case KEY_NPAGE:
			next = getfastnext(menurows, totnitems, pritems, abs);
			movefocus = next != abs;
			break;
		case ' ': /* Space */
			if (pritems[abs].type == MENUMODE) {
				retval = bs.value[bs.curr];
				pritems[abs].on = true;
				set_on_output(conf, retval, ngroups, groups,
				    pritems);
				loop = false;
			} else if (pritems[abs].type == CHECKLISTMODE) {
				pritems[abs].on = !pritems[abs].on;
			} else { /* RADIOLISTMODE */
				for (i = abs - pritems[abs].index;
				    i < totnitems &&
				    pritems[i].group == pritems[abs].group;
				    i++) {
					if (i != abs && pritems[i].on) {
						pritems[i].on = false;
						drawitem(conf, menupad, i, pos,
						    &pritems[i], false);
					}
				}
				pritems[abs].on = !pritems[abs].on;
			}
			drawitem(conf, menupad, abs, pos, &pritems[abs], true);
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);
			break;
		default:
			if (shortcut_butts) {
				if (shortcut_buttons(input, &bs)) {
					retval = bs.value[bs.curr];
					if (pritems[abs].type == MENUMODE)
						pritems[abs].on = true;
					set_on_output(conf, retval, ngroups,
					    groups, pritems);
					loop = false;
				}
				break;
			}

			/* shourtcut items */
			next = getnextshortcut(conf, totnitems, pritems, abs,
			    input);
			movefocus = next != abs;
		} /* end switch handler */

		if (movefocus) {
			drawitem(conf, menupad, abs, pos, &pritems[abs], false);
			abs = next;
			drawitem(conf, menupad, abs, pos, &pritems[abs], true);
			if (ymenupad > abs && ymenupad > 0)
				ymenupad = abs;
			if ((int)(ymenupad + menurows) <= abs)
				ymenupad = abs - menurows + 1;
			update_menuwin(conf, menuwin, menurows+2, w-4,
			    totnitems, menurows, ymenupad);
			wrefresh(menuwin);
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);
			movefocus = false;
		}
	} /* end while handler */

	if (focuslist != NULL)
		*focuslist = abs < 0 ? -1 : pritems[abs].group;
	if (focusitem !=NULL)
		*focusitem = abs < 0 ? -1 : pritems[abs].index;

	delwin(menupad);
	delwin(menuwin);
	end_dialog(conf, shadow, widget, textpad);
	free(pritems);

	return (retval);
}

/* API */
int
bsddialog_mixedlist(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int ngroups,
    struct bsddialog_menugroup *groups, int *focuslist, int *focusitem)
{
	int retval;

	retval = do_mixedlist(conf, text, rows, cols, menurows, MIXEDLISTMODE,
	    ngroups, groups, focuslist, focusitem);

	return (retval);
}

int
bsddialog_checklist(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int nitems,
    struct bsddialog_menuitem *items, int *focusitem)
{
	int retval, focuslist = 0;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_CHECKLIST /* unused */, nitems, items};

	retval = do_mixedlist(conf, text, rows, cols, menurows, CHECKLISTMODE,
	    1, &group, &focuslist, focusitem);

	return (retval);
}

int
bsddialog_menu(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int nitems,
    struct bsddialog_menuitem *items, int *focusitem)
{
	int retval, focuslist = 0;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_CHECKLIST /* unused */, nitems, items};

	retval = do_mixedlist(conf, text, rows, cols, menurows, MENUMODE, 1,
	    &group, &focuslist, focusitem);

	return (retval);
}

int
bsddialog_radiolist(struct bsddialog_conf *conf, const char *text, int rows,
    int cols, unsigned int menurows, unsigned int nitems,
    struct bsddialog_menuitem *items, int *focusitem)
{
	int retval, focuslist = 0;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_RADIOLIST /* unused */, nitems, items};

	retval = do_mixedlist(conf, text, rows, cols, menurows, RADIOLISTMODE,
	    1, &group, &focuslist, focusitem);

	return (retval);
}
