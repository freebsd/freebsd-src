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

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

#define GET_COLOR(bg, fg) (COLOR_PAIR(bg * 8 + fg +1))

struct bsddialog_theme t;

static struct bsddialog_theme bsddialogtheme = {
#define bgwidget  COLOR_WHITE
#define bgcurr    COLOR_YELLOW
	.shadow.color = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.h     = 1,
	.shadow.w     = 2,

	.terminal.color          = GET_COLOR(COLOR_BLACK, COLOR_CYAN),
	.widget.delimtitle       = true,
	.widget.titlecolor       = GET_COLOR(COLOR_YELLOW, bgwidget),
	.widget.lineraisecolor   = GET_COLOR(COLOR_BLACK,  bgwidget),
	.widget.linelowercolor   = GET_COLOR(COLOR_BLACK,  bgwidget),
	.widget.color            = GET_COLOR(COLOR_BLACK,  bgwidget),
	.widget.bottomtitlecolor = GET_COLOR(COLOR_BLACK,  bgwidget),

	.text.hmargin = 1,

	.menu.arrowcolor   = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.f_desccolor  = GET_COLOR(COLOR_WHITE,  bgcurr),
	.menu.desccolor    = GET_COLOR(COLOR_BLACK,  bgwidget),
	.menu.f_namecolor  = GET_COLOR(COLOR_BLACK,  bgcurr),
	.menu.namecolor    = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.namesepcolor = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.descsepcolor = GET_COLOR(COLOR_YELLOW, bgwidget),

	.form.f_fieldcolor  = GET_COLOR(COLOR_WHITE,  COLOR_BLUE),
	.form.fieldcolor    = GET_COLOR(COLOR_WHITE,  COLOR_CYAN),
	.form.readonlycolor = GET_COLOR(COLOR_CYAN,COLOR_WHITE),

	.bar.f_color = GET_COLOR(COLOR_WHITE, COLOR_BLUE),
	.bar.color   = GET_COLOR(COLOR_BLUE,  COLOR_WHITE),

	.button.space        = 3,
	.button.leftch       = '[',
	.button.rightch      = ']',
	.button.f_delimcolor = GET_COLOR(COLOR_WHITE,  bgcurr),
	.button.delimcolor   = GET_COLOR(COLOR_BLACK,  bgwidget),
	.button.f_color      = GET_COLOR(COLOR_WHITE,  bgcurr)    | A_UNDERLINE,
	.button.color        = GET_COLOR(COLOR_BLACK,  bgwidget)  | A_UNDERLINE,
	.button.f_shortcutcolor = GET_COLOR(COLOR_BLACK, bgcurr)  | A_UNDERLINE,
	.button.shortcutcolor = GET_COLOR(COLOR_YELLOW, bgwidget) | A_UNDERLINE
};

static struct bsddialog_theme blackwhite = {
#define bk  COLOR_BLACK
#define fg  COLOR_WHITE
	.shadow.color = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.h     = 1,
	.shadow.w     = 2,

	.terminal.color          = GET_COLOR(fg, bk),
	.widget.delimtitle       = true,
	.widget.titlecolor       = GET_COLOR(fg, bk),
	.widget.lineraisecolor   = GET_COLOR(fg, bk),
	.widget.linelowercolor   = GET_COLOR(fg, bk),
	.widget.color            = GET_COLOR(fg, bk),
	.widget.bottomtitlecolor = GET_COLOR(fg, bk),

	.text.hmargin = 1,

	.menu.arrowcolor   = GET_COLOR(fg, bk),
	.menu.f_desccolor  = GET_COLOR(fg, bk) | A_REVERSE,
	.menu.desccolor    = GET_COLOR(fg, bk),
	.menu.f_namecolor  = GET_COLOR(fg, bk) | A_REVERSE,
	.menu.namecolor    = GET_COLOR(fg, bk),
	.menu.namesepcolor = GET_COLOR(fg, bk),
	.menu.descsepcolor = GET_COLOR(fg, bk),

	.form.f_fieldcolor  = GET_COLOR(fg, bk) | A_REVERSE,
	.form.fieldcolor    = GET_COLOR(fg, bk),
	.form.readonlycolor = GET_COLOR(fg, bk),

	.bar.f_color = GET_COLOR(fg, bk) | A_REVERSE,
	.bar.color   = GET_COLOR(fg, bk),

	.button.space           = 3,
	.button.leftch          = '[',
	.button.rightch         = ']',
	.button.f_delimcolor    = GET_COLOR(fg, bk),
	.button.delimcolor      = GET_COLOR(fg, bk),
	.button.f_color         = GET_COLOR(fg, bk) | A_UNDERLINE | A_REVERSE,
	.button.color           = GET_COLOR(fg, bk) | A_UNDERLINE,
	.button.f_shortcutcolor = GET_COLOR(fg, bk) | A_UNDERLINE | A_REVERSE,
	.button.shortcutcolor   = GET_COLOR(fg, bk) | A_UNDERLINE
};

static struct bsddialog_theme dialogtheme = {
	.shadow.color = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.h     = 1,
	.shadow.w     = 2,

	.terminal.color          = GET_COLOR(COLOR_CYAN,  COLOR_BLUE)  | A_BOLD,
	.widget.delimtitle       = false,
	.widget.titlecolor       = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,
	.widget.lineraisecolor   = GET_COLOR(COLOR_WHITE, COLOR_WHITE) | A_BOLD,
	.widget.linelowercolor   = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,
	.widget.color            = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.widget.bottomtitlecolor = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,

	.text.hmargin = 1,

	.menu.arrowcolor   = GET_COLOR(COLOR_GREEN, COLOR_WHITE),
	.menu.f_desccolor  = GET_COLOR(COLOR_WHITE, COLOR_BLUE)  | A_BOLD,
	.menu.desccolor    = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,
	.menu.f_namecolor  = GET_COLOR(COLOR_YELLOW,COLOR_BLUE)  | A_BOLD,
	.menu.namecolor    = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,
	.menu.namesepcolor = GET_COLOR(COLOR_RED,   COLOR_WHITE),
	.menu.descsepcolor = GET_COLOR(COLOR_RED,   COLOR_WHITE),

	.form.f_fieldcolor  = GET_COLOR(COLOR_WHITE, COLOR_BLUE) | A_BOLD,
	.form.fieldcolor    = GET_COLOR(COLOR_WHITE, COLOR_CYAN) | A_BOLD,
	.form.readonlycolor = GET_COLOR(COLOR_CYAN,  COLOR_WHITE)| A_BOLD,

	.bar.f_color = GET_COLOR(COLOR_WHITE, COLOR_BLUE)  | A_BOLD,
	.bar.color   = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,

	.button.space           = 3,
	.button.leftch          = '<',
	.button.rightch         = '>',
	.button.f_delimcolor    = GET_COLOR(COLOR_WHITE,  COLOR_BLUE)  | A_BOLD,
	.button.delimcolor      = GET_COLOR(COLOR_BLACK,  COLOR_WHITE),
	.button.f_color         = GET_COLOR(COLOR_YELLOW, COLOR_BLUE)  | A_BOLD,
	.button.color           = GET_COLOR(COLOR_BLACK,  COLOR_WHITE),
	.button.f_shortcutcolor = GET_COLOR(COLOR_WHITE,  COLOR_BLUE)  | A_BOLD,
	.button.shortcutcolor   = GET_COLOR(COLOR_RED,    COLOR_WHITE) | A_BOLD
};

void bsddialog_set_theme(struct bsddialog_theme newtheme)
{
	t.shadow.color = newtheme.shadow.color;
	t.shadow.h     = newtheme.shadow.h;
	t.shadow.w     = newtheme.shadow.w;

	t.terminal.color          = newtheme.terminal.color;
	t.widget.delimtitle       = newtheme.widget.delimtitle;
	t.widget.titlecolor       = newtheme.widget.titlecolor;
	t.widget.lineraisecolor   = newtheme.widget.lineraisecolor;
	t.widget.linelowercolor   = newtheme.widget.linelowercolor;
	t.widget.color            = newtheme.widget.color;
	t.widget.bottomtitlecolor = newtheme.widget.bottomtitlecolor;

	t.text.hmargin = newtheme.text.hmargin;

	t.menu.arrowcolor   = newtheme.menu.arrowcolor;
	t.menu.f_desccolor  = newtheme.menu.f_desccolor;
	t.menu.desccolor    = newtheme.menu.desccolor;
	t.menu.f_namecolor  = newtheme.menu.f_namecolor;
	t.menu.namecolor    = newtheme.menu.namecolor;
	t.menu.namesepcolor = newtheme.menu.namesepcolor;
	t.menu.descsepcolor = newtheme.menu.descsepcolor;

	t.form.f_fieldcolor  = newtheme.form.f_fieldcolor;
	t.form.fieldcolor    = newtheme.form.fieldcolor;
	t.form.readonlycolor = newtheme.form.readonlycolor;

	t.bar.f_color = newtheme.bar.f_color;
	t.bar.color   = newtheme.bar.color;

	t.button.space           = newtheme.button.space;
	t.button.leftch          = newtheme.button.leftch;
	t.button.rightch         = newtheme.button.rightch;
	t.button.f_delimcolor    = newtheme.button.f_delimcolor;
	t.button.delimcolor      = newtheme.button.delimcolor;
	t.button.f_color         = newtheme.button.f_color;
	t.button.color           = newtheme.button.color;
	t.button.f_shortcutcolor = newtheme.button.f_shortcutcolor;
	t.button.shortcutcolor   = newtheme.button.shortcutcolor;

	bkgd(t.terminal.color);

	refresh();
}

int bsddialog_set_default_theme(enum bsddialog_default_theme newtheme)
{

	if (newtheme == BSDDIALOG_THEME_DEFAULT)
		bsddialog_set_theme(dialogtheme);
	else if (newtheme == BSDDIALOG_THEME_BSDDIALOG)
		bsddialog_set_theme(bsddialogtheme);
	else if (newtheme == BSDDIALOG_THEME_BLACKWHITE)
		bsddialog_set_theme(blackwhite);
	else if (newtheme == BSDDIALOG_THEME_DIALOG)
		bsddialog_set_theme(dialogtheme);
	else
		RETURN_ERROR("Unknow default theme");

	return 0;
}

int
bsddialog_color(enum bsddialog_color background,
    enum bsddialog_color foreground)
{

	return GET_COLOR(background, foreground);
}

struct bsddialog_theme bsddialog_get_theme()
{

	return t;
}
