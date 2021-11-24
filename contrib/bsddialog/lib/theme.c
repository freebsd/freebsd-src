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
	.shadowcolor     = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadowrows      = 1,
	.shadowcols      = 2,

	.backgroundcolor = GET_COLOR(COLOR_BLACK, COLOR_CYAN),
	.surroundtitle   = true,
	.titlecolor      = GET_COLOR(COLOR_YELLOW, bgwidget),
	.lineraisecolor  = GET_COLOR(COLOR_BLACK,  bgwidget),
	.linelowercolor  = GET_COLOR(COLOR_BLACK,  bgwidget),
	.widgetcolor     = GET_COLOR(COLOR_BLACK,  bgwidget),

	.texthmargin     = 1,

	.curritemcolor   = GET_COLOR(COLOR_WHITE,  bgcurr),
	.itemcolor       = GET_COLOR(COLOR_BLACK,  bgwidget),
	.currtagcolor    = GET_COLOR(COLOR_BLACK,  bgcurr),
	.tagcolor        = GET_COLOR(COLOR_YELLOW, bgwidget),
	.namesepcolor    = GET_COLOR(COLOR_YELLOW, bgwidget),
	.descsepcolor    = GET_COLOR(COLOR_YELLOW, bgwidget),

	.currfieldcolor  = GET_COLOR(COLOR_WHITE,  COLOR_BLUE),
	.fieldcolor      = GET_COLOR(COLOR_WHITE,  COLOR_CYAN),
	.fieldreadonlycolor = GET_COLOR(COLOR_CYAN,COLOR_WHITE),

	.currbarcolor    = GET_COLOR(COLOR_WHITE, COLOR_BLUE),
	.barcolor        = GET_COLOR(COLOR_BLUE,  COLOR_WHITE),

	.buttonspace     = 3,
	.buttleftch      = '[',
	.buttrightchar   = ']',
	.currbuttdelimcolor = GET_COLOR(COLOR_WHITE,  bgcurr),
	.buttdelimcolor     = GET_COLOR(COLOR_BLACK,  bgwidget),
	.currbuttoncolor    = GET_COLOR(COLOR_WHITE,  bgcurr)    | A_UNDERLINE,
	.buttoncolor        = GET_COLOR(COLOR_BLACK,  bgwidget)  | A_UNDERLINE,
	.currshortkeycolor  = GET_COLOR(COLOR_BLACK,  bgcurr)    | A_UNDERLINE,
	.shortkeycolor      = GET_COLOR(COLOR_YELLOW, bgwidget)  | A_UNDERLINE,

	.bottomtitlecolor= GET_COLOR(COLOR_BLACK, bgwidget)
};

static struct bsddialog_theme blackwhite = {
#define bk  COLOR_BLACK
#define fg  COLOR_WHITE
	.shadowcolor     = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadowrows      = 1,
	.shadowcols      = 2,

	.backgroundcolor = GET_COLOR(fg, bk),
	.surroundtitle   = true,
	.titlecolor      = GET_COLOR(fg, bk),
	.lineraisecolor  = GET_COLOR(fg, bk),
	.linelowercolor  = GET_COLOR(fg, bk),
	.widgetcolor     = GET_COLOR(fg, bk),

	.texthmargin     = 1,

	.curritemcolor   = GET_COLOR(fg, bk) | A_REVERSE,
	.itemcolor       = GET_COLOR(fg, bk),
	.currtagcolor    = GET_COLOR(fg, bk) | A_REVERSE,
	.tagcolor        = GET_COLOR(fg, bk),
	.namesepcolor    = GET_COLOR(fg, bk),
	.descsepcolor    = GET_COLOR(fg, bk),

	.currfieldcolor  = GET_COLOR(fg, bk) | A_REVERSE,
	.fieldcolor      = GET_COLOR(fg, bk),
	.fieldreadonlycolor = GET_COLOR(fg, bk),

	.currbarcolor    = GET_COLOR(fg, bk) | A_REVERSE,
	.barcolor        = GET_COLOR(fg, bk),

	.buttonspace     = 3,
	.buttleftch      = '[',
	.buttrightchar   = ']',
	.currbuttdelimcolor = GET_COLOR(fg, bk),
	.buttdelimcolor     = GET_COLOR(fg, bk),
	.currbuttoncolor    = GET_COLOR(fg, bk) | A_UNDERLINE | A_REVERSE,
	.buttoncolor        = GET_COLOR(fg, bk) | A_UNDERLINE,
	.currshortkeycolor  = GET_COLOR(fg, bk) | A_UNDERLINE | A_REVERSE,
	.shortkeycolor      = GET_COLOR(fg, bk) | A_UNDERLINE,

	.bottomtitlecolor= GET_COLOR(fg, bk)
};

static struct bsddialog_theme dialogtheme = {
	.shadowcolor     = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadowrows      = 1,
	.shadowcols      = 2,

	.backgroundcolor = GET_COLOR(COLOR_CYAN,  COLOR_BLUE)  | A_BOLD,
	.surroundtitle   = false,
	.titlecolor      = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,
	.lineraisecolor  = GET_COLOR(COLOR_WHITE, COLOR_WHITE) | A_BOLD,
	.linelowercolor  = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,
	.widgetcolor     = GET_COLOR(COLOR_BLACK, COLOR_WHITE),

	.texthmargin     = 1,

	.curritemcolor   = GET_COLOR(COLOR_WHITE, COLOR_BLUE)  | A_BOLD,
	.itemcolor       = GET_COLOR(COLOR_BLACK, COLOR_WHITE) | A_BOLD,
	.currtagcolor    = GET_COLOR(COLOR_YELLOW,COLOR_BLUE)  | A_BOLD,
	.tagcolor        = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,
	.namesepcolor    = GET_COLOR(COLOR_RED,   COLOR_WHITE),
	.descsepcolor    = GET_COLOR(COLOR_RED,   COLOR_WHITE),

	.currfieldcolor  = GET_COLOR(COLOR_WHITE,  COLOR_BLUE) | A_BOLD,
	.fieldcolor      = GET_COLOR(COLOR_WHITE,  COLOR_CYAN) | A_BOLD,
	.fieldreadonlycolor = GET_COLOR(COLOR_CYAN,COLOR_WHITE)| A_BOLD,

	.currbarcolor    = GET_COLOR(COLOR_WHITE, COLOR_BLUE)  | A_BOLD,
	.barcolor        = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,

	.buttonspace     = 3,
	.buttleftch      = '<',
	.buttrightchar   = '>',
	.currbuttdelimcolor = GET_COLOR(COLOR_WHITE,  COLOR_BLUE)   | A_BOLD,
	.buttdelimcolor     = GET_COLOR(COLOR_BLACK,  COLOR_WHITE),
	.currbuttoncolor    = GET_COLOR(COLOR_YELLOW, COLOR_BLUE)   | A_BOLD,
	.buttoncolor        = GET_COLOR(COLOR_BLACK,  COLOR_WHITE),
	.currshortkeycolor  = GET_COLOR(COLOR_WHITE,  COLOR_BLUE)   | A_BOLD,
	.shortkeycolor      = GET_COLOR(COLOR_RED,    COLOR_WHITE)  | A_BOLD,

	.bottomtitlecolor   = GET_COLOR(COLOR_BLACK,  COLOR_WHITE)  | A_BOLD
};

static struct bsddialog_theme magentatheme = {
	.shadowcolor     = GET_COLOR(COLOR_BLACK, COLOR_BLACK),
	.shadowrows      = 1,
	.shadowcols      = 2,

	.backgroundcolor = GET_COLOR(COLOR_WHITE,  COLOR_MAGENTA) | A_BOLD,
	.surroundtitle   = true,
	.titlecolor      = GET_COLOR(COLOR_RED,   COLOR_CYAN),
	.lineraisecolor  = GET_COLOR(COLOR_WHITE, COLOR_CYAN)     | A_BOLD,
	.linelowercolor  = GET_COLOR(COLOR_BLACK, COLOR_CYAN),
	.widgetcolor     = GET_COLOR(COLOR_BLACK, COLOR_CYAN),

	.texthmargin     = 1,

	.curritemcolor   = GET_COLOR(COLOR_WHITE, COLOR_BLUE) | A_BOLD,
	.itemcolor       = GET_COLOR(COLOR_BLACK, COLOR_CYAN) | A_BOLD,
	.currtagcolor    = GET_COLOR(COLOR_YELLOW,COLOR_BLUE) | A_BOLD,
	.tagcolor        = GET_COLOR(COLOR_BLUE,  COLOR_CYAN) | A_BOLD,
	.namesepcolor    = GET_COLOR(COLOR_RED,   COLOR_CYAN) | A_BOLD,
	.descsepcolor    = GET_COLOR(COLOR_BLACK, COLOR_CYAN) | A_BOLD,

	.currfieldcolor  = GET_COLOR(COLOR_WHITE,  COLOR_BLUE) | A_BOLD,
	.fieldcolor      = GET_COLOR(COLOR_WHITE,  COLOR_CYAN) | A_BOLD,
	.fieldreadonlycolor = GET_COLOR(COLOR_CYAN,COLOR_WHITE)| A_BOLD,

	.currbarcolor    = GET_COLOR(COLOR_WHITE, COLOR_BLUE)  | A_BOLD,
	.barcolor        = GET_COLOR(COLOR_BLUE,  COLOR_WHITE) | A_BOLD,

	.buttonspace     = 3,
	.buttleftch      = '<',
	.buttrightchar   = '>',
	.currbuttdelimcolor = GET_COLOR(COLOR_WHITE, COLOR_RED)    | A_BOLD,
	.buttdelimcolor     = GET_COLOR(COLOR_BLACK, COLOR_CYAN),
	.currbuttoncolor    = GET_COLOR(COLOR_WHITE, COLOR_RED),
	.buttoncolor        = GET_COLOR(COLOR_BLACK,  COLOR_CYAN),
	.currshortkeycolor  = GET_COLOR(COLOR_WHITE,  COLOR_RED)   | A_BOLD,
	.shortkeycolor      = GET_COLOR(COLOR_BLACK,  COLOR_CYAN),

	.bottomtitlecolor= GET_COLOR(COLOR_BLACK, COLOR_CYAN) | A_BOLD
};

void bsddialog_set_theme(struct bsddialog_theme newtheme)
{
	t.shadowcolor     = newtheme.shadowcolor;
	t.shadowrows      = newtheme.shadowrows;
	t.shadowcols      = newtheme.shadowcols;

	t.backgroundcolor = newtheme.backgroundcolor;
	t.surroundtitle   = newtheme.surroundtitle;
	t.titlecolor      = newtheme.titlecolor;
	t.lineraisecolor  = newtheme.lineraisecolor;
	t.linelowercolor  = newtheme.linelowercolor;
	t.widgetcolor     = newtheme.widgetcolor;

	t.texthmargin     = newtheme.texthmargin;

	t.curritemcolor   = newtheme.curritemcolor;
	t.itemcolor       = newtheme.itemcolor;
	t.currtagcolor    = newtheme.currtagcolor;
	t.tagcolor        = newtheme.tagcolor;
	t.namesepcolor    = newtheme.namesepcolor;
	t.descsepcolor    = newtheme.descsepcolor;

	t.currfieldcolor  = newtheme.currfieldcolor;
	t.fieldcolor      = newtheme.fieldcolor;
	t.fieldreadonlycolor = newtheme.fieldreadonlycolor;

	t.currbarcolor    = newtheme.currbarcolor;
	t.barcolor        = newtheme.barcolor;

	t.buttonspace        = newtheme.buttonspace;
	t.buttleftch         = newtheme.buttleftch;
	t.buttrightchar      = newtheme.buttrightchar;
	t.currbuttdelimcolor = newtheme.currbuttdelimcolor;
	t.buttdelimcolor     = newtheme.buttdelimcolor;
	t.currbuttoncolor    = newtheme.currbuttoncolor;
	t.buttoncolor        = newtheme.buttoncolor;
	t.currshortkeycolor  = newtheme.currshortkeycolor;
	t.shortkeycolor      = newtheme.shortkeycolor;

	t.bottomtitlecolor   = newtheme.bottomtitlecolor;

	bkgd(t.backgroundcolor);

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
	else if (newtheme == BSDDIALOG_THEME_MAGENTA)
		bsddialog_set_theme(magentatheme);
	else
		RETURN_ERROR("Unknow default theme");

	return 0;
}

int
bsddialog_color(enum bsddialog_color background, enum bsddialog_color foreground)
{

	return GET_COLOR(background, foreground);
}

struct bsddialog_theme bsddialog_get_theme()
{

	return t;
}
