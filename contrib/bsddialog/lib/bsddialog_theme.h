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

#ifndef _LIBBSDDIALOG_THEME_H_
#define _LIBBSDDIALOG_THEME_H_

/* color flags */
#define BSDDIALOG_BLINK       1U
#define BSDDIALOG_BOLD        2U
#define BSDDIALOG_HALFBRIGHT  4U
#define BSDDIALOG_HIGHLIGHT   8U
#define BSDDIALOG_REVERSE    16U
#define BSDDIALOG_UNDERLINE  32U

struct bsddialog_theme {
	struct {
		int color;
	} screen;
	struct {
		int color;
		unsigned int y;
		unsigned int x;
	} shadow;
	struct {
		int  color;
		bool delimtitle;
		int  titlecolor;
		int  lineraisecolor;
		int  linelowercolor;
		int  bottomtitlecolor;
		int  arrowcolor;
	} dialog;
	struct {
		int f_prefixcolor;
		int prefixcolor;
		int f_selectorcolor;
		int selectorcolor;
		int f_namecolor;
		int namecolor;
		int f_desccolor;
		int desccolor;
		int f_shortcutcolor;
		int shortcutcolor;
		int bottomdesccolor;
		int sepnamecolor;
		int sepdesccolor;
	} menu;
	struct {
		int f_fieldcolor;
		int fieldcolor;
		int readonlycolor;
		int bottomdesccolor;
	} form;
	struct {
		int f_color;
		int color;
	} bar;
	struct {
		unsigned int minmargin;
		unsigned int maxmargin;
		char leftdelim;
		char rightdelim;
		int f_delimcolor;
		int delimcolor;
		int f_color;
		int color;
		int f_shortcutcolor;
		int shortcutcolor;
	} button;
};

enum bsddialog_default_theme {
	BSDDIALOG_THEME_3D,
	BSDDIALOG_THEME_BLACKWHITE,
	BSDDIALOG_THEME_FLAT
};

enum bsddialog_color {
	BSDDIALOG_BLACK = 0,
	BSDDIALOG_RED,
	BSDDIALOG_GREEN,
	BSDDIALOG_YELLOW,
	BSDDIALOG_BLUE,
	BSDDIALOG_MAGENTA,
	BSDDIALOG_CYAN,
	BSDDIALOG_WHITE
};

int
bsddialog_color(enum bsddialog_color foreground,
    enum bsddialog_color background, unsigned int flags);
int
bsddialog_color_attrs(int color, enum bsddialog_color *foreground,
    enum bsddialog_color *background, unsigned int *flags);
int bsddialog_get_theme(struct bsddialog_theme *theme);
bool bsddialog_hascolors(void);
int bsddialog_set_default_theme(enum bsddialog_default_theme theme);
int bsddialog_set_theme(struct bsddialog_theme *theme);

#endif