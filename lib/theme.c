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
#include <ncurses/ncurses.h>
#else
#include <ncurses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

#define GET_COLOR(bg, fg) (COLOR_PAIR(bg * 8 + fg +1))

struct bsddialog_theme t;

static struct bsddialog_theme bsddialogtheme = {
#define bgwidget  COLOR_WHITE
#define bgcurr    COLOR_YELLOW
	.terminal.color = GET_COLOR(COLOR_BLACK, COLOR_CYAN),

	.shadow.color   = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.h       = 1,
	.shadow.w       = 2,

	.dialog.delimtitle       = true,
	.dialog.titlecolor       = GET_COLOR(COLOR_YELLOW, bgwidget),
	.dialog.lineraisecolor   = GET_COLOR(COLOR_BLACK,  bgwidget),
	.dialog.linelowercolor   = GET_COLOR(COLOR_BLACK,  bgwidget),
	.dialog.color            = GET_COLOR(COLOR_BLACK,  bgwidget),
	.dialog.bottomtitlecolor = GET_COLOR(COLOR_BLACK,  bgwidget),

	.text.hmargin = 1,

	.menu.arrowcolor      = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.selectorcolor   = GET_COLOR(COLOR_BLACK,  bgwidget) | A_BOLD,
	.menu.f_desccolor     = GET_COLOR(COLOR_WHITE,  bgcurr),
	.menu.desccolor       = GET_COLOR(COLOR_BLACK,  bgwidget),
	.menu.f_namecolor     = GET_COLOR(COLOR_BLACK,  bgcurr),
	.menu.namecolor       = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.namesepcolor    = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.descsepcolor    = GET_COLOR(COLOR_YELLOW, bgwidget),
	.menu.f_shortcutcolor = GET_COLOR(COLOR_RED,    bgcurr),
	.menu.shortcutcolor   = GET_COLOR(COLOR_RED,    bgwidget),

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
	.terminal.color = GET_COLOR(fg, bk),

	.shadow.color   = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.h       = 1,
	.shadow.w       = 2,

	.dialog.delimtitle       = true,
	.dialog.titlecolor       = GET_COLOR(fg, bk),
	.dialog.lineraisecolor   = GET_COLOR(fg, bk),
	.dialog.linelowercolor   = GET_COLOR(fg, bk),
	.dialog.color            = GET_COLOR(fg, bk),
	.dialog.bottomtitlecolor = GET_COLOR(fg, bk),

	.text.hmargin = 1,

	.menu.arrowcolor      = GET_COLOR(fg, bk),
	.menu.selectorcolor   = GET_COLOR(fg, bk),
	.menu.f_desccolor     = GET_COLOR(fg, bk) | A_REVERSE,
	.menu.desccolor       = GET_COLOR(fg, bk),
	.menu.f_namecolor     = GET_COLOR(fg, bk) | A_REVERSE,
	.menu.namecolor       = GET_COLOR(fg, bk),
	.menu.namesepcolor    = GET_COLOR(fg, bk),
	.menu.descsepcolor    = GET_COLOR(fg, bk),
	.menu.f_shortcutcolor = GET_COLOR(fg, bk) | A_UNDERLINE | A_REVERSE,
	.menu.shortcutcolor   = GET_COLOR(fg, bk) | A_UNDERLINE,

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
	.terminal.color = GET_COLOR(COLOR_CYAN,  COLOR_BLUE)  | A_BOLD,

	.shadow.color   = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.h       = 1,
	.shadow.w       = 2,

	.dialog.delimtitle       = false,
	.dialog.titlecolor       = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,
	.dialog.lineraisecolor   = GET_COLOR(COLOR_WHITE, COLOR_WHITE) | A_BOLD,
	.dialog.linelowercolor   = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,
	.dialog.color            = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.dialog.bottomtitlecolor = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,

	.text.hmargin = 1,

	.menu.arrowcolor      = GET_COLOR(COLOR_GREEN,  COLOR_WHITE),
	.menu.selectorcolor   = GET_COLOR(COLOR_BLACK,  bgwidget)    | A_BOLD,
	.menu.f_desccolor     = GET_COLOR(COLOR_WHITE,  COLOR_BLUE)  | A_BOLD,
	.menu.desccolor       = GET_COLOR(COLOR_BLACK,  COLOR_WHITE) | A_BOLD,
	.menu.f_namecolor     = GET_COLOR(COLOR_YELLOW, COLOR_BLUE)  | A_BOLD,
	.menu.namecolor       = GET_COLOR(COLOR_BLUE,   COLOR_WHITE) | A_BOLD,
	.menu.namesepcolor    = GET_COLOR(COLOR_RED,    COLOR_WHITE),
	.menu.descsepcolor    = GET_COLOR(COLOR_RED,    COLOR_WHITE),
	.menu.f_shortcutcolor = GET_COLOR(COLOR_RED,    COLOR_BLUE)  | A_BOLD,
	.menu.shortcutcolor   = GET_COLOR(COLOR_RED,    COLOR_WHITE) | A_BOLD,

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

static void
set_theme(struct bsddialog_theme *dst, struct bsddialog_theme *src)
{
	dst->shadow.color = src->shadow.color;
	dst->shadow.h     = src->shadow.h;
	dst->shadow.w     = src->shadow.w;

	dst->terminal.color          = src->terminal.color;
	dst->dialog.delimtitle       = src->dialog.delimtitle;
	dst->dialog.titlecolor       = src->dialog.titlecolor;
	dst->dialog.lineraisecolor   = src->dialog.lineraisecolor;
	dst->dialog.linelowercolor   = src->dialog.linelowercolor;
	dst->dialog.color            = src->dialog.color;
	dst->dialog.bottomtitlecolor = src->dialog.bottomtitlecolor;

	dst->text.hmargin = src->text.hmargin;

	dst->menu.arrowcolor      = src->menu.arrowcolor;
	dst->menu.selectorcolor   = src->menu.selectorcolor;
	dst->menu.f_desccolor     = src->menu.f_desccolor;
	dst->menu.desccolor       = src->menu.desccolor;
	dst->menu.f_namecolor     = src->menu.f_namecolor;
	dst->menu.namecolor       = src->menu.namecolor;
	dst->menu.namesepcolor    = src->menu.namesepcolor;
	dst->menu.descsepcolor    = src->menu.descsepcolor;
	dst->menu.f_shortcutcolor = src->menu.f_shortcutcolor;
	dst->menu.shortcutcolor   = src->menu.shortcutcolor;

	dst->form.f_fieldcolor  = src->form.f_fieldcolor;
	dst->form.fieldcolor    = src->form.fieldcolor;
	dst->form.readonlycolor = src->form.readonlycolor;

	dst->bar.f_color = src->bar.f_color;
	dst->bar.color   = src->bar.color;

	dst->button.space           = src->button.space;
	dst->button.leftch          = src->button.leftch;
	dst->button.rightch         = src->button.rightch;
	dst->button.f_delimcolor    = src->button.f_delimcolor;
	dst->button.delimcolor      = src->button.delimcolor;
	dst->button.f_color         = src->button.f_color;
	dst->button.color           = src->button.color;
	dst->button.f_shortcutcolor = src->button.f_shortcutcolor;
	dst->button.shortcutcolor   = src->button.shortcutcolor;

	bkgd(dst->terminal.color);
	refresh();
}

/* API */
int bsddialog_get_theme(struct bsddialog_theme *theme)
{
	if (theme == NULL)
		RETURN_ERROR("theme is NULL");
	if (sizeof(*theme) != sizeof(struct bsddialog_theme))
		RETURN_ERROR("Bad suze struct bsddialog_theme");

	set_theme(theme, &t);

	return (0);
}

int bsddialog_set_theme(struct bsddialog_theme *theme)
{
	if (theme == NULL)
		RETURN_ERROR("theme is NULL");
	if (sizeof(*theme) != sizeof(struct bsddialog_theme))
		RETURN_ERROR("Bad size struct bsddialog_theme");

	set_theme(&t, theme);

	return (0);
}

int bsddialog_set_default_theme(enum bsddialog_default_theme newtheme)
{

	if (newtheme == BSDDIALOG_THEME_DEFAULT) {
		bsddialog_set_theme(&dialogtheme);
		t.dialog.lineraisecolor = t.dialog.linelowercolor;
	}
	else if (newtheme == BSDDIALOG_THEME_BSDDIALOG)
		bsddialog_set_theme(&bsddialogtheme);
	else if (newtheme == BSDDIALOG_THEME_BLACKWHITE)
		bsddialog_set_theme(&blackwhite);
	else if (newtheme == BSDDIALOG_THEME_DIALOG)
		bsddialog_set_theme(&dialogtheme);
	else
		RETURN_ERROR("Unknow default theme");

	return (0);
}

int
bsddialog_color(enum bsddialog_color background,
    enum bsddialog_color foreground, unsigned int flags)
{
	unsigned int cursesflags = 0;

	if (flags & BSDDIALOG_BOLD)
		cursesflags |= A_BOLD;
	if (flags & BSDDIALOG_REVERSE)
		cursesflags |= A_REVERSE;
	if (flags & BSDDIALOG_UNDERLINE)
		cursesflags |= A_UNDERLINE;

	return (GET_COLOR(background, foreground) | cursesflags);
}
