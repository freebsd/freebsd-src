/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Alfonso Sabato Siciliano
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

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define GET_COLOR(bg, fg) (COLOR_PAIR(bg * 8 + fg +1))
#define WHITE GET_COLOR(COLOR_WHITE, COLOR_BLACK)
#define BLACK GET_COLOR(COLOR_WHITE, COLOR_BLACK) | A_REVERSE
#define NFLAGS 6

struct bsddialog_theme t;
bool hastermcolors;

struct flag_converter {
	unsigned int public;
	unsigned int private;
};

static struct flag_converter flagconv[NFLAGS] = {
	{BSDDIALOG_BLINK,      A_BLINK},
	{BSDDIALOG_BOLD,       A_BOLD},
	{BSDDIALOG_HALFBRIGHT, A_DIM},
	{BSDDIALOG_HIGHLIGHT,  A_STANDOUT},
	{BSDDIALOG_REVERSE,    A_REVERSE},
	{BSDDIALOG_UNDERLINE,  A_UNDERLINE}
};

static struct bsddialog_theme blackwhite = {
	.screen.color = WHITE,

	.shadow.color = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.y     = 1,
	.shadow.x     = 2,

	.dialog.delimtitle       = true,
	.dialog.titlecolor       = WHITE,
	.dialog.lineraisecolor   = WHITE,
	.dialog.linelowercolor   = WHITE,
	.dialog.color            = WHITE,
	.dialog.bottomtitlecolor = WHITE,
	.dialog.arrowcolor       = WHITE,

	.menu.f_prefixcolor   = WHITE,
	.menu.prefixcolor     = WHITE,
	.menu.f_selectorcolor = BLACK,
	.menu.selectorcolor   = WHITE,
	.menu.f_desccolor     = BLACK,
	.menu.desccolor       = WHITE,
	.menu.f_namecolor     = BLACK,
	.menu.namecolor       = WHITE,
	.menu.f_shortcutcolor = BLACK | A_UNDERLINE,
	.menu.shortcutcolor   = WHITE | A_UNDERLINE,
	.menu.bottomdesccolor = WHITE,
	.menu.sepnamecolor    = WHITE,
	.menu.sepdesccolor    = WHITE,

	.form.f_fieldcolor    = BLACK,
	.form.fieldcolor      = WHITE,
	.form.readonlycolor   = WHITE,
	.form.bottomdesccolor = WHITE,

	.bar.f_color = BLACK,
	.bar.color   = WHITE,

	.button.minmargin       = 1,
	.button.maxmargin       = 5,
	.button.leftdelim       = '[',
	.button.rightdelim      = ']',
	.button.f_delimcolor    = WHITE,
	.button.delimcolor      = WHITE,
	.button.f_color         = BLACK,
	.button.color           = WHITE,
	.button.f_shortcutcolor = BLACK | A_UNDERLINE | A_BOLD,
	.button.shortcutcolor   = WHITE | A_UNDERLINE | A_BOLD
};

static struct bsddialog_theme flat = {
	.screen.color = GET_COLOR(COLOR_CYAN, COLOR_BLUE) | A_BOLD,

	.shadow.color = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadow.y     = 1,
	.shadow.x     = 2,

	.dialog.delimtitle       = true,
	.dialog.titlecolor       = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,
	.dialog.lineraisecolor   = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.dialog.linelowercolor   = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.dialog.color            = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.dialog.bottomtitlecolor = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,
	.dialog.arrowcolor       = GET_COLOR(COLOR_BLUE,  COLOR_WHITE),

	.menu.f_prefixcolor   = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.menu.prefixcolor     = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.menu.f_selectorcolor = GET_COLOR(COLOR_WHITE, COLOR_BLUE),
	.menu.selectorcolor   = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.menu.f_desccolor     = GET_COLOR(COLOR_WHITE, COLOR_BLUE),
	.menu.desccolor       = GET_COLOR(COLOR_BLACK, COLOR_WHITE),
	.menu.f_namecolor     = GET_COLOR(COLOR_WHITE, COLOR_BLUE),
	.menu.namecolor       = GET_COLOR(COLOR_BLUE,  COLOR_WHITE),
	.menu.f_shortcutcolor = GET_COLOR(COLOR_RED,   COLOR_BLUE),
	.menu.shortcutcolor   = GET_COLOR(COLOR_RED,   COLOR_WHITE),
	.menu.bottomdesccolor = GET_COLOR(COLOR_WHITE, COLOR_BLUE),
	.menu.sepnamecolor    = GET_COLOR(COLOR_RED,   COLOR_WHITE),
	.menu.sepdesccolor    = GET_COLOR(COLOR_RED,   COLOR_WHITE),

	.form.f_fieldcolor    = GET_COLOR(COLOR_WHITE, COLOR_BLUE) | A_BOLD,
	.form.fieldcolor      = GET_COLOR(COLOR_WHITE, COLOR_CYAN) | A_BOLD,
	.form.readonlycolor   = GET_COLOR(COLOR_CYAN,  COLOR_WHITE)| A_BOLD,
	.form.bottomdesccolor = GET_COLOR(COLOR_WHITE, COLOR_BLUE),

	.bar.f_color = GET_COLOR(COLOR_WHITE, COLOR_BLUE)  | A_BOLD,
	.bar.color   = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,

	.button.minmargin       = 1,
	.button.maxmargin       = 5,
	.button.leftdelim       = '[',
	.button.rightdelim      = ']',
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
	dst->screen.color = src->screen.color;

	dst->shadow.color = src->shadow.color;
	dst->shadow.y     = src->shadow.y;
	dst->shadow.x     = src->shadow.x;

	dst->dialog.delimtitle       = src->dialog.delimtitle;
	dst->dialog.titlecolor       = src->dialog.titlecolor;
	dst->dialog.lineraisecolor   = src->dialog.lineraisecolor;
	dst->dialog.linelowercolor   = src->dialog.linelowercolor;
	dst->dialog.color            = src->dialog.color;
	dst->dialog.bottomtitlecolor = src->dialog.bottomtitlecolor;
	dst->dialog.arrowcolor       = src->dialog.arrowcolor;

	dst->menu.f_prefixcolor   = src->menu.f_prefixcolor;
	dst->menu.prefixcolor     = src->menu.prefixcolor;
	dst->menu.f_selectorcolor = src->menu.f_selectorcolor;
	dst->menu.selectorcolor   = src->menu.selectorcolor;
	dst->menu.f_desccolor     = src->menu.f_desccolor;
	dst->menu.desccolor       = src->menu.desccolor;
	dst->menu.f_namecolor     = src->menu.f_namecolor;
	dst->menu.namecolor       = src->menu.namecolor;
	dst->menu.f_shortcutcolor = src->menu.f_shortcutcolor;
	dst->menu.shortcutcolor   = src->menu.shortcutcolor;
	dst->menu.bottomdesccolor = src->menu.bottomdesccolor;
	dst->menu.sepnamecolor    = src->menu.sepnamecolor;
	dst->menu.sepdesccolor    = src->menu.sepdesccolor;

	dst->form.f_fieldcolor    = src->form.f_fieldcolor;
	dst->form.fieldcolor      = src->form.fieldcolor;
	dst->form.readonlycolor   = src->form.readonlycolor;
	dst->form.bottomdesccolor = src->form.bottomdesccolor;

	dst->bar.f_color = src->bar.f_color;
	dst->bar.color   = src->bar.color;

	dst->button.minmargin       = src->button.minmargin;
	dst->button.maxmargin       = src->button.maxmargin;
	dst->button.leftdelim       = src->button.leftdelim;
	dst->button.rightdelim      = src->button.rightdelim;
	dst->button.f_delimcolor    = src->button.f_delimcolor;
	dst->button.delimcolor      = src->button.delimcolor;
	dst->button.f_color         = src->button.f_color;
	dst->button.color           = src->button.color;
	dst->button.f_shortcutcolor = src->button.f_shortcutcolor;
	dst->button.shortcutcolor   = src->button.shortcutcolor;

	bkgd(dst->screen.color);
}

/* API */
int bsddialog_get_theme(struct bsddialog_theme *theme)
{
	CHECK_PTR(theme);
	set_theme(theme, &t);

	return (BSDDIALOG_OK);
}

int bsddialog_set_theme(struct bsddialog_theme *theme)
{
	CHECK_PTR(theme);
	set_theme(&t, theme);
	refresh();

	return (BSDDIALOG_OK);
}

int bsddialog_set_default_theme(enum bsddialog_default_theme newtheme)
{
	if (newtheme == BSDDIALOG_THEME_3D) {
		set_theme(&t, &flat);
		t.dialog.lineraisecolor   =
		    GET_COLOR(COLOR_WHITE, COLOR_WHITE) | A_BOLD;
		t.dialog.delimtitle       = false;
		t.dialog.bottomtitlecolor = t.dialog.bottomtitlecolor | A_BOLD;
	} else if (newtheme == BSDDIALOG_THEME_BLACKWHITE) {
		set_theme(&t, &blackwhite);
	} else if (newtheme == BSDDIALOG_THEME_FLAT) {
		set_theme(&t, &flat);
	} else {
		RETURN_FMTERROR("Unknown default theme (%d), "
		    "to use enum bsddialog_default_theme",
		    newtheme);
	}
	refresh();

	return (BSDDIALOG_OK);
}

int
bsddialog_color(enum bsddialog_color foreground,
    enum bsddialog_color background, unsigned int flags)
{
	unsigned int i, f;

	f = 0;
	for (i=0; i < NFLAGS; i++)
		if (flags & flagconv[i].public)
			f |= flagconv[i].private;

	return (GET_COLOR(foreground, background) | f);
}

int
bsddialog_color_attrs(int color, enum bsddialog_color *foreground,
    enum bsddialog_color *background, unsigned int *flags)
{
	short fg, bg;
	unsigned int i, f;

	if (flags != NULL) {
		f = 0;
		for (i=0; i < NFLAGS; i++)
			if (color & flagconv[i].private)
				f |= flagconv[i].public;
		*flags = f;
	}
	if (pair_content(PAIR_NUMBER(color), &fg, &bg) != OK)
		RETURN_ERROR("Cannot get color attributes");
	if (foreground != NULL)
		*foreground = fg;
	if (background != NULL)
		*background = bg;

	return (BSDDIALOG_OK);
}

bool bsddialog_hascolors(void)
{
	return (hastermcolors);
}
