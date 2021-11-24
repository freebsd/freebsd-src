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
#include <string.h>

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/* "Menu": checklist - menu - mixedlist - radiolist - treeview - buildlist */

#define DEPTHSPACE	4
#define MIN_HEIGHT	VBORDERS + 6 /* 2 buttons 1 text 3 menu */

extern struct bsddialog_theme t;

enum menumode {
	BUILDLISTMODE,
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

static int checkradiolist(int nitems, struct bsddialog_menuitem *items)
{
	int i, error;

	error = 0;
	for (i=0; i<nitems; i++) {
		if (error > 0)
			items[i].on = false;

		if (items[i].on == true)
			error++;
	}

	return (error == 0 ? 0 : -1);
}

static int checkmenu(int nitems, struct bsddialog_menuitem *items) // useful?
{
	int i, error;

	error = 0;
	for (i=0; i<nitems; i++) {
		if (items[i].on == true)
			error++;

		items[i].on = false;
	}

	return (error == 0 ? 0 : -1);
}

static void
getfirst(int ngroups, struct bsddialog_menugroup *groups, int *abs, int *group,
    int *rel)
{
	int i, a;

	*abs = *rel = *group = -1;
	a = 0;
	for (i=0; i<ngroups; i++) {
		if (groups[i].type == BSDDIALOG_SEPARATOR) {
			a += groups[i].nitems;
			continue;
		}
		if (groups[i].nitems != 0) {
			*group = i;
			*abs = a;
			*rel = 0;
			break;
		}
	}
}

static void
getfirst_with_default(struct bsddialog_conf conf, int ngroups,
    struct bsddialog_menugroup *groups, int *abs, int *group, int *rel)
{
	int i, j, a;
	struct bsddialog_menuitem *item;

	getfirst(ngroups, groups, abs, group, rel);
	if (*abs < 0)
		return;
	
	a = *abs;

	for (i=*group; i<ngroups; i++) {
		if (groups[i].type == BSDDIALOG_SEPARATOR) {
			a += groups[i].nitems;
			continue;
		}
		for (j = 0; j < (int) groups[i].nitems; j++) {
			item = &groups[i].items[j];
			if (conf.menu.default_item != NULL && item->name != NULL) {
				if (strcmp(item->name, conf.menu.default_item) == 0) {
					*abs = a;
					*group = i;
					*rel = j;
					return;
				}
			}
			a++;
		}
	}
}

static void
getlast(int totnitems, int ngroups, struct bsddialog_menugroup *groups,
    int *abs, int *group, int *rel)
{
	int i, a;

	a = totnitems - 1;
	for (i = ngroups-1; i>=0; i--) {
		if (groups[i].type == BSDDIALOG_SEPARATOR) {
			a -= groups[i].nitems;
			continue;
		}
		if (groups[i].nitems != 0) {
			*group = i;
			*abs = a;
			*rel = groups[i].nitems - 1;
			break;
		}
	}
}

static void
getnext(int ngroups, struct bsddialog_menugroup *groups, int *abs, int *group,
    int *rel)
{
	int i, a;

	if (*abs < 0 || *group < 0 || *rel < 0)
		return;

	if (*rel + 1 < (int) groups[*group].nitems) {
		*rel = *rel + 1;
		*abs = *abs + 1;
		return;
	}

	if (*group + 1 > ngroups)
		return;

	a = *abs;
	for (i = *group + 1; i < ngroups; i++) {
		if (groups[i].type == BSDDIALOG_SEPARATOR) {
			a += groups[i].nitems;
			continue;
		}
		if (groups[i].nitems != 0) {
			*group = i;
			*abs = a + 1;
			*rel = 0;
			break;
		}
	}
}

static void
getfastnext(int menurows, int ngroups, struct bsddialog_menugroup *groups,
    int *abs, int *group, int *rel)
{
	int a, start, i;

	start = *abs;
	i = menurows;
	do {
		a = *abs;
		getnext(ngroups, groups, abs, group, rel);
		i--;
	} while (*abs != a && *abs < start + menurows && i > 0);
}

static void
getprev(struct bsddialog_menugroup *groups, int *abs, int *group, int *rel)
{
	int i, a;

	if (*abs < 0 || *group < 0 || *rel < 0)
		return;

	if (*rel > 0) {
		*rel = *rel - 1;
		*abs = *abs - 1;
		return;
	}

	if (*group - 1 < 0)
		return;

	a = *abs;
	for (i = *group - 1; i >= 0; i--) {
		if (groups[i].type == BSDDIALOG_SEPARATOR) {
			a -= (int) groups[i].nitems;
			continue;
		}
		if (groups[i].nitems != 0) {
			*group = i;
			*abs = a - 1;
			*rel = (int) groups[i].nitems - 1;
			break;
		}
	}
}

static void
getfastprev(int menurows, struct bsddialog_menugroup *groups, int *abs,
    int *group, int *rel)
{
	int a, start, i;

	start = *abs;
	i = menurows;
	do {
		a = *abs;
		getprev(groups, abs, group, rel);
		i--;
	} while (*abs != a && *abs > start - menurows && i > 0);
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

	return mode;
}

static void
drawitem(struct bsddialog_conf conf, WINDOW *pad, int y,
    struct bsddialog_menuitem item, enum menumode mode, struct lineposition pos,
    bool curr)
{
	int color, colorname, linech;

	color = curr ? t.curritemcolor : t.itemcolor;
	colorname = curr ? t.currtagcolor : t.tagcolor;

	if (mode == SEPARATORMODE) {
		if (conf.no_lines == false) {
			wattron(pad, t.itemcolor);
			linech = conf.ascii_lines ? '-' : ACS_HLINE;
			mvwhline(pad, y, 0, linech, pos.line);
			wattroff(pad, t.itemcolor);
		}
		wmove(pad, y, pos.line/2 - (strlen(item.name)+strlen(item.desc))/2);
		wattron(pad, t.namesepcolor);
		waddstr(pad, item.name);
		wattroff(pad, t.namesepcolor);
		if (strlen(item.name) > 0 && strlen(item.desc) > 0)
			waddch(pad, ' ');
		wattron(pad, t.descsepcolor);
		waddstr(pad, item.desc);
		wattroff(pad, t.descsepcolor);
		return;
	}

	/* prefix */
	if (item.prefix != NULL && item.prefix[0] != '\0')
		mvwaddstr(pad, y, 0, item.prefix);

	/* selector */
	wmove(pad, y, pos.xselector);
	wattron(pad, color);
	if (mode == CHECKLISTMODE)
		wprintw(pad, "[%c]", item.on ? 'X' : ' ');
	if (mode == RADIOLISTMODE)
		wprintw(pad, "(%c)", item.on ? '*' : ' ');
	wattroff(pad, color);

	/* name */
	if (mode != BUILDLISTMODE && conf.menu.no_tags == false) {
		wattron(pad, colorname);
		mvwaddstr(pad, y, pos.xname + item.depth * DEPTHSPACE, item.name);
		wattroff(pad, colorname);
	}

	/* description */
	if (conf.menu.no_items == false) {
		if ((mode == BUILDLISTMODE || conf.menu.no_tags) && curr == false)
			color = item.on ? t.tagcolor : t.itemcolor;
		wattron(pad, color);
		if (conf.menu.no_tags)
			mvwaddstr(pad, y, pos.xname + item.depth * DEPTHSPACE, item.desc);
		else
			mvwaddstr(pad, y, pos.xdesc, item.desc);
		wattroff(pad, color);
	}

	/* bottom desc (item help) */
	if (item.bottomdesc != NULL && item.bottomdesc[0] != '\0') {
		move(LINES-1, 2);
		clrtoeol();
		addstr(item.bottomdesc);

		refresh();
	}
}

static void
menu_autosize(struct bsddialog_conf conf, int rows, int cols, int *h, int *w,
    char *text, int linelen, unsigned int *menurows, int nitems,
    struct buttons bs)
{
	int textrow, menusize;

	textrow = text != NULL && strlen(text) > 0 ? 1 : 0;

	if (cols == BSDDIALOG_AUTOSIZE) {
		*w = VBORDERS;
		/* buttons size */
		*w += bs.nbuttons * bs.sizebutton;
		*w += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.buttonspace : 0;
		/* line size */
		*w = MAX(*w, linelen + 6);
		/*
		* avoid terminal overflow,
		* -1 fix false negative with big menu over the terminal and
		* autosize, for example "portconfig /usr/ports/www/apache24/".
		*/
		*w = MIN(*w, widget_max_width(conf)-1);
	}

	if (rows == BSDDIALOG_AUTOSIZE) {
		*h = HBORDERS + 2 /* buttons */ + textrow;

		if (*menurows == 0) {
			*h += nitems + 2;
			*h = MIN(*h, widget_max_height(conf));
			menusize = MIN(nitems + 2, *h - (HBORDERS + 2 + textrow));
			menusize -=2;
			*menurows = menusize < 0 ? 0 : menusize;
		}
		else /* h autosize with a fixed menurows */
			*h = *h + *menurows + 2;

		/* avoid terminal overflow */
		*h = MIN(*h, widget_max_height(conf));
	}
	else {
		if (*menurows == 0)
			*menurows = MIN(rows-6-textrow, nitems);
	}
}

static int
menu_checksize(int rows, int cols, char *text, int menurows, int nitems,
    struct buttons bs)
{
	int mincols, textrow, menusize;

	mincols = VBORDERS;
	/* buttons */
	mincols += bs.nbuttons * bs.sizebutton;
	mincols += bs.nbuttons > 0 ? (bs.nbuttons-1) * t.buttonspace : 0;
	/* line, comment to permet some cols hidden */
	/* mincols = MAX(mincols, linelen); */

	if (cols < mincols)
		RETURN_ERROR("Few cols, width < size buttons or "\
		    "name+descripion of the items");

	textrow = text != NULL && strlen(text) > 0 ? 1 : 0;

	if (nitems > 0 && menurows == 0)
		RETURN_ERROR("items > 0 but menurows == 0, probably terminal "\
		    "too small");

	menusize = nitems > 0 ? 3 : 0;
	if (rows < 2  + 2 + menusize + textrow)
		RETURN_ERROR("Few lines for this menus");

	return 0;
}

/* the caller has to call prefresh(menupad, ymenupad, 0, ys, xs, ye, xe); */
static void
update_menuwin(struct bsddialog_conf conf, WINDOW *menuwin, int h, int w,
    int totnitems, unsigned int menurows, int ymenupad)
{

	if (totnitems > (int) menurows) {
		draw_borders(conf, menuwin, h, w, LOWERED);

		if (ymenupad > 0) {
			wattron(menuwin, t.lineraisecolor);
			mvwprintw(menuwin, 0, 2, "^^");
			wattroff(menuwin, t.lineraisecolor);
		}
		if ((int) (ymenupad + menurows) < totnitems) {
			wattron(menuwin, t.linelowercolor);
			mvwprintw(menuwin, h-1, 2, "vv");
			wattroff(menuwin, t.linelowercolor);
		}

		mvwprintw(menuwin, h-1, w-10, "%3d%%",
		    100 * (ymenupad + menurows) / totnitems);
	}
}

static int
do_mixedlist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, enum menumode mode, int ngroups,
    struct bsddialog_menugroup *groups, int *focuslist, int *focusitem)
{
	WINDOW  *shadow, *widget, *textpad, *menuwin, *menupad;
	int i, j, y, x, h, w, htextpad, output, input;
	int ymenupad, ys, ye, xs, xe, abs, g, rel, totnitems;
	bool loop, automenurows;
	struct buttons bs;
	struct bsddialog_menuitem *item;
	enum menumode currmode;
	struct lineposition pos = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	
	automenurows = menurows == BSDDIALOG_AUTOSIZE ? true : false;

	totnitems = 0;
	for (i=0; i < ngroups; i++) {
		currmode = getmode(mode, groups[i]);
		if (currmode == RADIOLISTMODE)
			checkradiolist(groups[i].nitems, groups[i].items);

		if (currmode == MENUMODE)
			checkmenu(groups[i].nitems, groups[i].items);

		if (currmode == RADIOLISTMODE || currmode == CHECKLISTMODE)
			pos.selectorlen = 3;

		for (j=0; j < (int) groups[i].nitems; j++) {
			totnitems++;
			item = &groups[i].items[j];

			if (groups[i].type == BSDDIALOG_SEPARATOR) {
				pos.maxsepstr = MAX(pos.maxsepstr,
				    strlen(item->name) + strlen(item->desc));
				continue;
			}

			pos.maxprefix = MAX(pos.maxprefix, strlen(item->prefix));
			pos.maxdepth  = MAX((int) pos.maxdepth, item->depth);
			pos.maxname   = MAX(pos.maxname, strlen(item->name));
			pos.maxdesc   = MAX(pos.maxdesc, strlen(item->desc));
		}
	}
	pos.maxname = conf.menu.no_tags ? 0 : pos.maxname;
	pos.maxdesc = conf.menu.no_items ? 0 : pos.maxdesc;
	pos.maxdepth *= DEPTHSPACE;

	pos.xselector = pos.maxprefix + (pos.maxprefix != 0 ? 1 : 0);
	pos.xname = pos.xselector + pos.selectorlen + (pos.selectorlen > 0 ? 1 : 0);
	pos.xdesc = pos.maxdepth + pos.xname + pos.maxname;
	pos.xdesc += (pos.maxname != 0 ? 1 : 0);
	pos.line = MAX(pos.maxsepstr + 3, pos.xdesc + pos.maxdesc);


	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	if (set_widget_size(conf, rows, cols, &h, &w) != 0)
		return BSDDIALOG_ERROR;
	menu_autosize(conf, rows, cols, &h, &w, text, pos.line, &menurows,
	    totnitems, bs);
	if (menu_checksize(h, w, text, menurows, totnitems, bs) != 0)
		return BSDDIALOG_ERROR;
	if (set_widget_position(conf, &y, &x, h, w) != 0)
		return BSDDIALOG_ERROR;

	if (new_widget_withtextpad(conf, &shadow, &widget, y, x, h, w, RAISED,
	    &textpad, &htextpad, text, true) != 0)
		return BSDDIALOG_ERROR;

	prefresh(textpad, 0, 0, y + 1, x + 1 + t.texthmargin,
	    y + h - menurows, x + 1 + w - t.texthmargin);

	menuwin = new_boxed_window(conf, y + h - 5 - menurows, x + 2,
	    menurows+2, w-4, LOWERED);

	menupad = newpad(totnitems, pos.line);
	wbkgd(menupad, t.widgetcolor);

	getfirst_with_default(conf, ngroups, groups, &abs, &g, &rel);
	ymenupad = 0;
	for (i=0; i<ngroups; i++) {
		currmode = getmode(mode, groups[i]);
		for (j=0; j < (int) groups[i].nitems; j++) {
			item = &groups[i].items[j];
			drawitem(conf, menupad, ymenupad, *item, currmode,
			    pos, ymenupad == abs);
			ymenupad++;
		}
	}

	ys = y + h - 5 - menurows + 1;
	ye = y + h - 5 ;
	if (conf.menu.align_left || (int)pos.line > w - 6) {
		xs = x + 3;
		xe = xs + w - 7;
	}
	else { /* center */
		xs = x + 3 + (w-6)/2 - pos.line/2;
		xe = xs + w - 5;
	}

	ymenupad = 0; /* now ymenupad is pminrow for prefresh() */
	if ((int)(ymenupad + menurows) - 1 < abs)
		ymenupad = abs - menurows + 1;
	update_menuwin(conf, menuwin, menurows+2, w-4, totnitems, menurows, ymenupad);
	wrefresh(menuwin);
	prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);
	
	draw_buttons(widget, h-2, w, bs, true);
	wrefresh(widget);

	item = &groups[g].items[rel];
	currmode = getmode(mode, groups[g]);
	loop = true;
	while(loop) {
		input = getch();
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			output = bs.value[bs.curr];
			if (currmode == MENUMODE)
				item->on = true;
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			draw_buttons(widget, h-2, w, bs, true);
			wrefresh(widget);
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				draw_buttons(widget, h-2, w, bs, true);
				wrefresh(widget);
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				draw_buttons(widget, h-2, w, bs, true);
				wrefresh(widget);
			}
			break;
		case KEY_CTRL('E'): /* add conf.menu.extrahelpkey ? */
		case KEY_F(1):
			if (conf.hfile == NULL)
				break;
			if (f1help(conf) != 0)
				return BSDDIALOG_ERROR;
			/* No break! the terminal size can change */
		case KEY_RESIZE:
			hide_widget(y, x, h, w,conf.shadow);

			/*
			 * Unnecessary, but, when the columns decrease the
			 * following "refresh" seem not work
			 */
			refresh();

			if (set_widget_size(conf, rows, cols, &h, &w) != 0)
				return BSDDIALOG_ERROR;
			menurows = automenurows ? 0 : menurows;
			menu_autosize(conf, rows, cols, &h, &w, text, pos.line,
			    &menurows, totnitems, bs);
			if (menu_checksize(h, w, text, menurows, totnitems, bs) != 0)
				return BSDDIALOG_ERROR;
			if (set_widget_position(conf, &y, &x, h, w) != 0)
				return BSDDIALOG_ERROR;

			wclear(shadow);
			mvwin(shadow, y + t.shadowrows, x + t.shadowcols);
			wresize(shadow, h, w);

			wclear(widget);
			mvwin(widget, y, x);
			wresize(widget, h, w);

			htextpad = 1;
			wclear(textpad);
			wresize(textpad, 1, w - HBORDERS - t.texthmargin * 2);

			if(update_widget_withtextpad(conf, shadow, widget, h, w,
			    RAISED, textpad, &htextpad, text, true) != 0)
			return BSDDIALOG_ERROR;
			
			draw_buttons(widget, h-2, w, bs, true);
			wrefresh(widget);

			prefresh(textpad, 0, 0, y + 1, x + 1 + t.texthmargin,
			    y + h - menurows, x + 1 + w - t.texthmargin);

			wclear(menuwin);
			mvwin(menuwin, y + h - 5 - menurows, x + 2);
			wresize(menuwin,menurows+2, w-4);
			update_menuwin(conf, menuwin, menurows+2, w-4, totnitems,
			    menurows, ymenupad);
			wrefresh(menuwin);
			
			ys = y + h - 5 - menurows + 1;
			ye = y + h - 5 ;
			if (conf.menu.align_left || (int)pos.line > w - 6) {
				xs = x + 3;
				xe = xs + w - 7;
			}
			else { /* center */
				xs = x + 3 + (w-6)/2 - pos.line/2;
				xe = xs + w - 5;
			}

			if ((int)(ymenupad + menurows) - 1 < abs)
				ymenupad = abs - menurows + 1;
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);

			refresh();

			break;
		default:
			for (i = 0; i < (int) bs.nbuttons; i++)
				if (tolower(input) == tolower((bs.label[i])[0])) {
					output = bs.value[i];
					loop = false;
			}
		
		}

		if (abs < 0)
			continue;
		switch(input) {
		case KEY_HOME:
		case KEY_UP:
		case KEY_PPAGE:
			if (abs == 0) /* useless, just to save cpu refresh */
				break;
			drawitem(conf, menupad, abs, *item, currmode, pos, false);
			if (input == KEY_HOME)
				getfirst(ngroups, groups, &abs, &g, &rel);
			else if (input == KEY_UP)
				getprev(groups, &abs, &g, &rel);
			else /* input == KEY_PPAGE*/
				getfastprev(menurows, groups, &abs, &g, &rel);
			item = &groups[g].items[rel];
			currmode= getmode(mode, groups[g]);
			drawitem(conf, menupad, abs, *item, currmode, pos, true);
			if (ymenupad > abs && ymenupad > 0)
				ymenupad = abs;
			update_menuwin(conf, menuwin, menurows+2, w-4, totnitems,
			    menurows, ymenupad);
			wrefresh(menuwin);
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);
			break;
		case KEY_END:
		case KEY_DOWN:
		case KEY_NPAGE:
			if (abs == totnitems -1)
				break; /* useless, just to save cpu refresh */
			drawitem(conf, menupad, abs, *item, currmode, pos, false);
			if (input == KEY_END)
				getlast(totnitems, ngroups, groups, &abs, &g, &rel);
			else if (input == KEY_DOWN)
				getnext(ngroups, groups, &abs, &g, &rel);
			else /* input == KEY_NPAGE*/
				getfastnext(menurows, ngroups, groups, &abs, &g, &rel);
			item = &groups[g].items[rel];
			currmode= getmode(mode, groups[g]);
			drawitem(conf, menupad, abs, *item, currmode, pos, true);
			if ((int)(ymenupad + menurows) <= abs)
				ymenupad = abs - menurows + 1;
			update_menuwin(conf, menuwin, menurows+2, w-4, totnitems,
			    menurows, ymenupad);
			wrefresh(menuwin);
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);
			break;
		case ' ': /* Space */
			if (currmode == MENUMODE)
				break;
			else if (currmode == CHECKLISTMODE)
				item->on = !item->on;
			else { /* RADIOLISTMODE */
				if (item->on == true)
					break;
				for (i=0; i < (int) groups[g].nitems; i++)
					if (groups[g].items[i].on == true) {
						groups[g].items[i].on = false;
						drawitem(conf, menupad,
						    abs - rel + i, groups[g].items[i],
						    currmode, pos, false);
					}
				item->on = true;
			}
			drawitem(conf, menupad, abs, *item, currmode, pos, true);
			prefresh(menupad, ymenupad, 0, ys, xs, ye, xe);
		}
	}

	if (focuslist != NULL)
		*focuslist = g;
	if (focusitem !=NULL)
		*focusitem = rel;

	delwin(menupad);
	delwin(menuwin);
	end_widget_withtextpad(conf, widget, h, w, textpad, shadow);

	return output;
}

/*
 * API
 */

int bsddialog_mixedlist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int ngroups, struct bsddialog_menugroup *groups,
    int *focuslist, int *focusitem)
{
	int output;

	output = do_mixedlist(conf, text, rows, cols, menurows, MIXEDLISTMODE,
	    ngroups, groups, focuslist, focusitem);

	return output;
}

int
bsddialog_checklist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem)
{
	int output;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_CHECKLIST /* unused */, nitems, items};

	output = do_mixedlist(conf, text, rows, cols, menurows, CHECKLISTMODE,
	    1, &group, NULL, focusitem);

	return output;
}

int
bsddialog_menu(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem)
{
	int output;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_CHECKLIST /* unused */, nitems, items};

	output = do_mixedlist(conf, text, rows, cols, menurows, MENUMODE, 1,
	    &group, NULL, focusitem);

	return output;
}

int
bsddialog_radiolist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem)
{
	int output;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_RADIOLIST /* unused */, nitems, items};

	output = do_mixedlist(conf, text, rows, cols, menurows, RADIOLISTMODE,
	    1, &group, NULL, focusitem);

	return output;
}

int
bsddialog_treeview(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem)
{
	int output;
	struct bsddialog_menugroup group = {
	    BSDDIALOG_RADIOLIST /* unused */, nitems, items};

	conf.menu.no_tags = true;
	conf.menu.align_left = true;

	output = do_mixedlist(conf, text, rows, cols, menurows, RADIOLISTMODE,
	    1, &group, NULL, focusitem);

	return output;
}

int
bsddialog_buildlist(struct bsddialog_conf conf, char* text, int rows, int cols,
    unsigned int menurows, int nitems, struct bsddialog_menuitem *items,
    int *focusitem)
{
	WINDOW *widget, *leftwin, *leftpad, *rightwin, *rightpad, *shadow;
	int output, i, x, y, input;
	bool loop, buttupdate, padsupdate, startleft;
	int nlefts, nrights, leftwinx, rightwinx, winsy, padscols, curr;
	enum side {LEFT, RIGHT} currV;
	int currH;
	struct buttons bs;
	struct lineposition pos = {0,0,0,0,0,0,0,0,0,0};

	startleft = false;
	for (i=0; i<nitems; i++) {
		pos.line = MAX(pos.line, strlen(items[i].desc));
		if (items[i].on == false)
			startleft = true;
	}

	if (new_widget(conf, &widget, &y, &x, text, &rows, &cols, &shadow,
	    true) <0)
		return -1;

	winsy = y + rows - 5 - menurows;
	leftwinx = x+2;
	leftwin = new_boxed_window(conf, winsy, leftwinx, menurows+2, (cols-5)/2,
	    LOWERED);
	rightwinx = x + cols - 2 -(cols-5)/2;
	rightwin = new_boxed_window(conf, winsy, rightwinx, menurows+2,
	    (cols-5)/2, LOWERED);

	wrefresh(leftwin);
	wrefresh(rightwin);

	padscols = (cols-5)/2 - 2;
	leftpad  = newpad(nitems, pos.line);
	rightpad = newpad(nitems, pos.line);
	wbkgd(leftpad, t.widgetcolor);
	wbkgd(rightpad, t.widgetcolor);

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	currH = 0;
	currV = startleft ? LEFT : RIGHT;
	loop = buttupdate = padsupdate = true;
	while(loop) {
		if (buttupdate) {
			draw_buttons(widget, rows-2, cols, bs, true);
			wrefresh(widget);
			buttupdate = false;
		}

		if (padsupdate) {
			werase(leftpad);
			werase(rightpad);
			curr = -1;
			nlefts = nrights = 0;
			for (i=0; i<nitems; i++) {
				if (items[i].on == false) {
					if (currV == LEFT && currH == nlefts)
						curr = i;
					drawitem(conf, leftpad, nlefts, items[i],
					    BUILDLISTMODE, pos, curr == i);
					nlefts++;
				} else {
					if (currV == RIGHT && currH == nrights)
						curr = i;
					drawitem(conf, rightpad, nrights, items[i],
					    BUILDLISTMODE, pos, curr == i);
					nrights++;
				}
			}
			prefresh(leftpad, 0, 0, winsy+1, leftwinx+1,
			    winsy+1+menurows, leftwinx + 1 + padscols);
			prefresh(rightpad, 0, 0, winsy+1, rightwinx+1,
			    winsy+1+menurows, rightwinx + 1 + padscols);
			padsupdate = false;
		}

		input = getch();
		switch(input) {
		case 10: // Enter
			output = bs.value[bs.curr]; // -> buttvalues[selbutton]
			loop = false;
			break;
		case 27: // Esc
			output = BSDDIALOG_ERROR;
			loop = false;
			break;
		case '\t': // TAB
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			buttupdate = true;
			break;
		}

		if (nitems <= 0)
			continue;

		switch(input) {
		case KEY_LEFT:
			if (currV == RIGHT && nrights > 0) {
				currV = LEFT;
				currH = 0;
				padsupdate = true;
			}
			break;
		case KEY_RIGHT:
			if (currV == LEFT && nrights > 0) {
				currV = RIGHT;
				currH = 0;
				padsupdate = true;
			}
			break;
		case KEY_UP:
			currH = (currH > 0) ? currH - 1 : 0;
			padsupdate = true;
			break;
		case KEY_DOWN:
			if (currV == LEFT)
				currH = (currH < nlefts-1) ? currH +1 : currH;
			else
				currH = (currH < nrights-1)? currH +1 : currH;
			padsupdate = true;
			break;
		case ' ': // Space
			items[curr].on = ! items[curr].on;
			if (currV == LEFT) {
				if (nlefts > 1)
					currH = currH > 0 ? currH-1 : 0;
				else {
					currH = 0;
					currV = RIGHT;
				}
			} else {
				if (nrights > 1)
					currH = currH > 0 ? currH-1 : 0;
				else {
					currH = 0;
					currV = LEFT;
				}
			}
			padsupdate = true;
			break;
		default:

			break;
		}
	}

	if(focusitem != NULL)
		*focusitem = curr;

	delwin(leftpad);
	delwin(leftwin);
	delwin(rightpad);
	delwin(rightwin);
	end_widget(conf, widget, rows, cols, shadow);

	return output;
}
