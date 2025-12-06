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

#include <curses.h>
#include <stdlib.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

enum menumode {
	CHECKLISTMODE,
	MENUMODE,
	MIXEDLISTMODE,
	RADIOLISTMODE,
	SEPARATORMODE
};

struct privateitem {
	const char *prefix;
	bool on;               /* menu changes, not API on */
	unsigned int depth;
	const char *name;
	const char *desc;
	const char *bottomdesc;
	int group;             /* index menu in menugroup */
	int index;             /* real item index inside its menu */
	enum menumode type;
	wchar_t shortcut;
};

struct privatemenu {
	WINDOW *box;              /* only for borders */
	WINDOW *pad;              /* pad for the private items */
	int ypad;                 /* start pad line */
	int ys, ye, xs, xe;       /* pad pos */
	unsigned int xselector;   /* [] */
	unsigned int xname;       /* real x: xname + item.depth */
	unsigned int xdesc;       /* real x: xdesc + item.depth */
	unsigned int line;        /* wpad: prefix [] depth name desc */
	unsigned int apimenurows;
	unsigned int menurows;    /* real menurows after menu_size_position() */
	int nitems;               /* total nitems (all groups * all items) */
	struct privateitem *pritems;
	int sel;                  /* current focus item, can be -1 */
	bool hasbottomdesc;
};

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

static int
build_privatemenu(struct bsddialog_conf *conf, struct privatemenu *m,
    enum menumode mode, unsigned int ngroups,
    struct bsddialog_menugroup *groups)
{
	bool onetrue;
	int i, j, abs;
	unsigned int maxsepstr, maxprefix, selectorlen, maxdepth;
	unsigned int maxname, maxdesc;
	struct bsddialog_menuitem *item;
	struct privateitem *pritem;

	/* nitems and fault checks */
	CHECK_ARRAY(ngroups, groups);
	m->nitems = 0;
	for (i = 0; i < (int)ngroups; i++) {
		CHECK_ARRAY(groups[i].nitems, groups[i].items);
		m->nitems += (int)groups[i].nitems;
	}

	/* alloc and set private items */
	m->pritems = calloc(m->nitems, sizeof (struct privateitem));
	if (m->pritems == NULL)
		RETURN_ERROR("Cannot allocate memory for internal menu items");
	m->hasbottomdesc = false;
	abs = 0;
	for (i = 0; i < (int)ngroups; i++) {
		onetrue = false;
		for (j = 0; j < (int)groups[i].nitems; j++) {
			item = &groups[i].items[j];
			pritem = &m->pritems[abs];

			if (getmode(mode, groups[i]) == MENUMODE) {
				m->pritems[abs].on = false;
			} else if (getmode(mode, groups[i]) == RADIOLISTMODE) {
				m->pritems[abs].on = onetrue ? false : item->on;
				if (m->pritems[abs].on)
					onetrue = true;
			} else { /* CHECKLISTMODE */
				m->pritems[abs].on = item->on;
			}
			pritem->group = i;
			pritem->index = j;
			pritem->type = getmode(mode, groups[i]);

			pritem->prefix = CHECK_STR(item->prefix);
			pritem->depth = item->depth;
			pritem->name = CHECK_STR(item->name);
			pritem->desc = CHECK_STR(item->desc);
			pritem->bottomdesc = CHECK_STR(item->bottomdesc);
			if (item->bottomdesc != NULL)
				m->hasbottomdesc = true;

			mbtowc(&pritem->shortcut, conf->menu.no_name ?
			    pritem->desc : pritem->name, MB_CUR_MAX);

			abs++;
		}
	}

	/* positions */
	m->xselector = m->xname = m->xdesc = m->line = 0;
	maxsepstr = maxprefix = selectorlen = maxdepth = maxname = maxdesc = 0;
	for (i = 0; i < m->nitems; i++) {
		if (m->pritems[i].type == RADIOLISTMODE ||
		    m->pritems[i].type == CHECKLISTMODE)
			selectorlen = 4;

		if (m->pritems[i].type == SEPARATORMODE) {
			maxsepstr = MAX(maxsepstr,
			    strcols(m->pritems[i].name) +
			    strcols(m->pritems[i].desc));
			continue;
		}

		maxprefix = MAX(maxprefix, strcols(m->pritems[i].prefix));
		maxdepth  = MAX(maxdepth, m->pritems[i].depth);
		maxname   = MAX(maxname, strcols(m->pritems[i].name));
		maxdesc   = MAX(maxdesc, strcols(m->pritems[i].desc));
	}
	maxname = conf->menu.no_name ? 0 : maxname;
	maxdesc = conf->menu.no_desc ? 0 : maxdesc;

	m->xselector = maxprefix + (maxprefix != 0 ? 1 : 0);
	m->xname = m->xselector + selectorlen;
	m->xdesc = maxdepth + m->xname + maxname;
	m->xdesc += (maxname != 0 ? 1 : 0);
	m->line = MAX(maxsepstr + 3, m->xdesc + maxdesc);

	return (0);
}

static void
set_return_on(struct privatemenu *m, struct bsddialog_menugroup *groups)
{
	int i;
	struct privateitem *pritem;

	for (i = 0; i < m->nitems; i++) {
		if (m->pritems[i].type == SEPARATORMODE)
			continue;
		pritem = &m->pritems[i];
		groups[pritem->group].items[pritem->index].on = pritem->on;
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
getnextshortcut(int npritems, struct privateitem *pritems, int abs, wint_t key)
{
	int i, next;

	next = -1;
	for (i = 0; i < npritems; i++) {
		if (pritems[i].type == SEPARATORMODE)
			continue;
		if (pritems[i].shortcut == (wchar_t)key) {
			if (i > abs)
				return (i);
			if (i < abs && next == -1)
				next = i;
		}
	}

	return (next != -1 ? next : abs);
}

static void drawseparators(struct bsddialog_conf *conf, struct privatemenu *m)
{
	int i, realw, labellen;
	const char *desc, *name;

	for (i = 0; i < m->nitems; i++) {
		if (m->pritems[i].type != SEPARATORMODE)
			continue;
		if (conf->no_lines == false) {
			wattron(m->pad, t.menu.desccolor);
			if (conf->ascii_lines)
				mvwhline(m->pad, i, 0, '-', m->line);
			else
				mvwhline_set(m->pad, i, 0, WACS_HLINE, m->line);
			wattroff(m->pad, t.menu.desccolor);
		}
		name = m->pritems[i].name;
		desc = m->pritems[i].desc;
		realw = m->xe - m->xs;
		labellen = strcols(name) + strcols(desc) + 1;
		wmove(m->pad, i, (labellen < realw) ? realw/2 - labellen/2 : 0);
		wattron(m->pad, t.menu.sepnamecolor);
		waddstr(m->pad, name);
		wattroff(m->pad, t.menu.sepnamecolor);
		if (strcols(name) > 0 && strcols(desc) > 0)
			waddch(m->pad, ' ');
		wattron(m->pad, t.menu.sepdesccolor);
		waddstr(m->pad, desc);
		wattroff(m->pad, t.menu.sepdesccolor);
	}
}

static void
drawitem(struct bsddialog_conf *conf, struct privatemenu *m, int y, bool focus)
{
	int colordesc, colorname, colorshortcut;
	struct privateitem *pritem;

	pritem = &m->pritems[y];

	/* prefix */
	wattron(m->pad, focus ? t.menu.f_prefixcolor : t.menu.prefixcolor);
	mvwaddstr(m->pad, y, 0, pritem->prefix);
	wattroff(m->pad, focus ? t.menu.f_prefixcolor : t.menu.prefixcolor);

	/* selector */
	wmove(m->pad, y, m->xselector);
	wattron(m->pad, focus ? t.menu.f_selectorcolor : t.menu.selectorcolor);
	if (pritem->type == CHECKLISTMODE)
		wprintw(m->pad, "[%c]", pritem->on ? 'X' : ' ');
	if (pritem->type == RADIOLISTMODE)
		wprintw(m->pad, "(%c)", pritem->on ? '*' : ' ');
	wattroff(m->pad, focus ? t.menu.f_selectorcolor : t.menu.selectorcolor);

	/* name */
	colorname = focus ? t.menu.f_namecolor : t.menu.namecolor;
	if (conf->menu.no_name == false) {
		wattron(m->pad, colorname);
		mvwaddstr(m->pad, y, m->xname + pritem->depth, pritem->name);
		wattroff(m->pad, colorname);
	}

	/* description */
	if (conf->menu.no_name)
		colordesc = focus ? t.menu.f_namecolor : t.menu.namecolor;
	else
		colordesc = focus ? t.menu.f_desccolor : t.menu.desccolor;

	if (conf->menu.no_desc == false) {
		wattron(m->pad, colordesc);
		if (conf->menu.no_name)
			mvwaddstr(m->pad, y, m->xname + pritem->depth,
			    pritem->desc);
		else
			mvwaddstr(m->pad, y, m->xdesc, pritem->desc);
		wattroff(m->pad, colordesc);
	}

	/* shortcut */
	if (conf->menu.shortcut_buttons == false) {
		colorshortcut = focus ?
		    t.menu.f_shortcutcolor : t.menu.shortcutcolor;
		wattron(m->pad, colorshortcut);
		mvwaddwch(m->pad, y, m->xname + pritem->depth, pritem->shortcut);
		wattroff(m->pad, colorshortcut);
	}

	/* bottom description */
	if (m->hasbottomdesc) {
		move(SCREENLINES - 1, 2);
		clrtoeol();
		if (focus) {
			attron(t.menu.bottomdesccolor);
			addstr(pritem->bottomdesc);
			attroff(t.menu.bottomdesccolor);
			wnoutrefresh(stdscr);
		}
	}
}

static void update_menubox(struct bsddialog_conf *conf, struct privatemenu *m)
{
	int h, w;

	draw_borders(conf, m->box, LOWERED);
	getmaxyx(m->box, h, w);

	if (m->nitems > (int)m->menurows) {
		wattron(m->box, t.dialog.arrowcolor);
		if (m->ypad > 0)
			mvwhline(m->box, 0, 2, UARROW(conf), 3);

		if ((m->ypad + (int)m->menurows) < m->nitems)
			mvwhline(m->box, h-1, 2, DARROW(conf), 3);

		mvwprintw(m->box, h-1, w-6, "%3d%%",
		    100 * (m->ypad + m->menurows) / m->nitems);
		wattroff(m->box, t.dialog.arrowcolor);
	}
}

static int menu_size_position(struct dialog *d, struct privatemenu *m)
{
	int htext, hmenu;

	if (set_widget_size(d->conf, d->rows, d->cols, &d->h, &d->w) != 0)
		return (BSDDIALOG_ERROR);

	hmenu = (int)(m->menurows == BSDDIALOG_AUTOSIZE) ?
	    (int)m->nitems : (int)m->menurows;
	hmenu += 2; /* menu borders */
	/*
	 * algo 1: notext = 1 (grows vertically).
	 * algo 2: notext = hmenu (grows horizontally, better for little term).
	 */
	if (set_widget_autosize(d->conf, d->rows, d->cols, &d->h, &d->w,
	    d->text, &htext, &d->bs, hmenu, m->line + 4) != 0)
		return (BSDDIALOG_ERROR);
	/* avoid menurows overflow and menurows becomes "at most menurows" */
	if (d->h - BORDERS - htext - HBUTTONS <= 2 /* menuborders */)
		m->menurows = (m->nitems > 0) ? 1 : 0; /* widget_checksize() */
	else
		m->menurows = MIN(d->h - BORDERS - htext - HBUTTONS, hmenu) - 2;

	/*
	 * no minw=linelen to avoid big menu fault, then some col can be
	 * hidden (example portconfig www/apache24).
	 */
	if (widget_checksize(d->h, d->w, &d->bs,
	    2 /* border box */ + MIN(m->menurows, 1), 0) != 0)
		return (BSDDIALOG_ERROR);

	if (set_widget_position(d->conf, &d->y, &d->x, d->h, d->w) != 0)
		return (BSDDIALOG_ERROR);

	return (0);
}

static int mixedlist_draw(struct dialog *d, bool redraw, struct privatemenu *m)
{
	if (redraw) {
		hide_dialog(d);
		refresh(); /* Important for decreasing screen */
	}
	m->menurows = m->apimenurows;
	if (menu_size_position(d, m) != 0)
		return (BSDDIALOG_ERROR);
	if (draw_dialog(d) != 0) /* doupdate() in main loop */
		return (BSDDIALOG_ERROR);
	if (redraw)
		refresh(); /* Important to fix grey lines expanding screen */
	TEXTPAD(d, 2/*bmenu*/ + m->menurows + HBUTTONS);

	/* selected item in view*/
	if (m->ypad > m->sel && m->ypad > 0)
		m->ypad = m->sel;
	if ((int)(m->ypad + m->menurows) <= m->sel)
		m->ypad = m->sel - m->menurows + 1;
	/* lower pad after a terminal expansion */
	if (m->ypad > 0 && (m->nitems - m->ypad) < (int)m->menurows)
		m->ypad = m->nitems - m->menurows;

	update_box(d->conf, m->box, d->y + d->h - 5 - m->menurows, d->x + 2,
	    m->menurows+2, d->w-4, LOWERED);
	update_menubox(d->conf, m);
	wnoutrefresh(m->box);

	m->ys = d->y + d->h - 5 - m->menurows + 1;
	m->ye = d->y + d->h - 5 ;
	if (d->conf->menu.align_left || (int)m->line > d->w - 6) {
		m->xs = d->x + 3;
		m->xe = m->xs + d->w - 7;
	} else { /* center */
		m->xs = d->x + 3 + (d->w-6)/2 - m->line/2;
		m->xe = m->xs + d->w - 5;
	}
	drawseparators(d->conf, m); /* uses xe - xs */
	pnoutrefresh(m->pad, m->ypad, 0, m->ys, m->xs, m->ye, m->xe);

	return (0);
}

static int
do_mixedlist(struct bsddialog_conf *conf, const char *text, int rows, int cols,
    unsigned int menurows, enum menumode mode, unsigned int ngroups,
    struct bsddialog_menugroup *groups, int *focuslist, int *focusitem)
{
	bool loop, changeitem;
	int i, next, retval;
	wint_t input;
	struct privatemenu m;
	struct dialog d;

	if (prepare_dialog(conf, text, rows, cols, &d) != 0)
		return (BSDDIALOG_ERROR);
	set_buttons(&d, conf->menu.shortcut_buttons, OK_LABEL, CANCEL_LABEL);
	if (d.conf->menu.no_name && d.conf->menu.no_desc)
		RETURN_ERROR("Both conf.menu.no_name and conf.menu.no_desc");

	if (build_privatemenu(conf, &m, mode, ngroups, groups) != 0)
		return (BSDDIALOG_ERROR);

	if ((m.box = newwin(1, 1, 1, 1)) == NULL)
		RETURN_ERROR("Cannot build WINDOW box menu");
	wbkgd(m.box, t.dialog.color);
	m.pad = newpad(m.nitems, m.line);
	wbkgd(m.pad, t.dialog.color);

	for (i = 0; i < m.nitems; i++)
		drawitem(conf, &m, i, false);
	m.sel = getfirst_with_default(m.nitems, m.pritems, ngroups, groups,
	    focuslist, focusitem);
	if (m.sel >= 0)
		drawitem(d.conf, &m, m.sel, true);
	m.ypad = 0;
	m.apimenurows = menurows;
	if (mixedlist_draw(&d, false, &m) != 0)
		return (BSDDIALOG_ERROR);

	changeitem = false;
	loop = true;
	while (loop) {
		doupdate();
		if (get_wch(&input) == ERR)
			continue;
		switch(input) {
		case KEY_ENTER:
		case 10: /* Enter */
			retval = BUTTONVALUE(d.bs);
			if (m.sel >= 0 && m.pritems[m.sel].type == MENUMODE)
				m.pritems[m.sel].on = true;
			loop = false;
			break;
		case 27: /* Esc */
			if (conf->key.enable_esc) {
				retval = BSDDIALOG_ESC;
				if (m.sel >= 0 &&
				   m.pritems[m.sel].type == MENUMODE)
					m.pritems[m.sel].on = true;
				loop = false;
			}
			break;
		case '\t': /* TAB */
		case KEY_RIGHT:
			d.bs.curr = (d.bs.curr + 1) % d.bs.nbuttons;
			DRAW_BUTTONS(d);
			break;
		case KEY_LEFT:
			d.bs.curr--;
			if (d.bs.curr < 0)
				 d.bs.curr = d.bs.nbuttons - 1;
			DRAW_BUTTONS(d);
			break;
		case KEY_F(1):
			if (conf->key.f1_file == NULL &&
			    conf->key.f1_message == NULL)
				break;
			if (f1help_dialog(conf) != 0)
				return (BSDDIALOG_ERROR);
			if (mixedlist_draw(&d, true, &m) != 0)
				return (BSDDIALOG_ERROR);
			break;
		case KEY_CTRL('l'):
		case KEY_RESIZE:
			if (mixedlist_draw(&d, true, &m) != 0)
				return (BSDDIALOG_ERROR);
			break;
		}

		if (m.sel < 0)
			continue;
		switch(input) {
		case KEY_HOME:
			next = getnext(m.nitems, m.pritems, -1);
			changeitem = next != m.sel;
			break;
		case '-':
		case KEY_CTRL('p'):
		case KEY_UP:
			next = getprev(m.pritems, m.sel);
			changeitem = next != m.sel;
			break;
		case KEY_PPAGE:
			next = getfastprev(m.menurows, m.pritems, m.sel);
			changeitem = next != m.sel;
			break;
		case KEY_END:
			next = getprev(m.pritems, m.nitems);
			changeitem = next != m.sel;
			break;
		case '+':
		case KEY_CTRL('n'):
		case KEY_DOWN:
			next = getnext(m.nitems, m.pritems, m.sel);
			changeitem = next != m.sel;
			break;
		case KEY_NPAGE:
			next = getfastnext(m.menurows, m.nitems, m.pritems, m.sel);
			changeitem = next != m.sel;
			break;
		case ' ': /* Space */
			if (m.pritems[m.sel].type == MENUMODE) {
				retval = BUTTONVALUE(d.bs);
				m.pritems[m.sel].on = true;
				loop = false;
			} else if (m.pritems[m.sel].type == CHECKLISTMODE) {
				m.pritems[m.sel].on = !m.pritems[m.sel].on;
			} else { /* RADIOLISTMODE */
				for (i = m.sel - m.pritems[m.sel].index;
				    i < m.nitems &&
				    m.pritems[i].group == m.pritems[m.sel].group;
				    i++) {
					if (i != m.sel && m.pritems[i].on) {
						m.pritems[i].on = false;
						drawitem(conf, &m, i, false);
					}
				}
				m.pritems[m.sel].on = !m.pritems[m.sel].on;
			}
			drawitem(conf, &m, m.sel, true);
			pnoutrefresh(m.pad, m.ypad, 0, m.ys, m.xs, m.ye, m.xe);
			break;
		default:
			if (conf->menu.shortcut_buttons) {
				if (shortcut_buttons(input, &d.bs)) {
					DRAW_BUTTONS(d);
					doupdate();
					retval = BUTTONVALUE(d.bs);
					if (m.pritems[m.sel].type == MENUMODE)
						m.pritems[m.sel].on = true;
					loop = false;
				}
				break;
			}

			/* shourtcut items */
			next = getnextshortcut(m.nitems, m.pritems, m.sel,
			    input);
			changeitem = next != m.sel;
		} /* end switch get_wch() */

		if (changeitem) {
			drawitem(conf, &m, m.sel, false);
			m.sel = next;
			drawitem(conf, &m, m.sel, true);
			if (m.ypad > m.sel && m.ypad > 0)
				m.ypad = m.sel;
			if ((int)(m.ypad + m.menurows) <= m.sel)
				m.ypad = m.sel - m.menurows + 1;
			update_menubox(conf, &m);
			wnoutrefresh(m.box);
			pnoutrefresh(m.pad, m.ypad, 0, m.ys, m.xs, m.ye, m.xe);
			changeitem = false;
		}
	} /* end while (loop) */

	set_return_on(&m, groups);

	if (focuslist != NULL)
		*focuslist = m.sel < 0 ? -1 : m.pritems[m.sel].group;
	if (focusitem !=NULL)
		*focusitem = m.sel < 0 ? -1 : m.pritems[m.sel].index;

	if (m.hasbottomdesc && conf->clear) {
		move(SCREENLINES - 1, 2);
		clrtoeol();
	}
	delwin(m.pad);
	delwin(m.box);
	end_dialog(&d);
	free(m.pritems);

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
	    BSDDIALOG_CHECKLIST /* unused */, nitems, items, 0};

	CHECK_ARRAY(nitems, items); /* efficiency, avoid do_mixedlist() */
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
	    BSDDIALOG_CHECKLIST /* unused */, nitems, items, 0};

	CHECK_ARRAY(nitems, items); /* efficiency, avoid do_mixedlist() */
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
	    BSDDIALOG_RADIOLIST /* unused */, nitems, items, 0};

	CHECK_ARRAY(nitems, items); /* efficiency, avoid do_mixedlist() */
	retval = do_mixedlist(conf, text, rows, cols, menurows, RADIOLISTMODE,
	    1, &group, &focuslist, focusitem);

	return (retval);
}
