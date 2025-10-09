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
#include <string.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

#define DEFAULT_COLS_PER_ROW  10   /* Default conf.text.columns_per_row */

static bool in_bsddialog_mode = false;

int bsddialog_init_notheme(void)
{
	int i, j, c, error;

	set_error_string("");

	if (initscr() == NULL)
		RETURN_ERROR("Cannot init curses (initscr)");

	error = OK;
	error += keypad(stdscr, TRUE);
	nl();
	error += cbreak();
	error += noecho();
	curs_set(0);
	if (error != OK) {
		bsddialog_end();
		RETURN_ERROR("Cannot init curses (keypad and cursor)");
	}
	in_bsddialog_mode = true;

	c = 1;
	error += start_color();
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			error += init_pair(c, i, j);
			c++;
		}
	}

	hastermcolors = (error == OK && has_colors()) ? true : false;

	return (BSDDIALOG_OK);
}

int bsddialog_init(void)
{
	enum bsddialog_default_theme theme;

	bsddialog_init_notheme();

	if (bsddialog_hascolors())
		theme = BSDDIALOG_THEME_FLAT;
	else
		theme = BSDDIALOG_THEME_BLACKWHITE;

	if (bsddialog_set_default_theme(theme) != 0) {
		bsddialog_end();
		in_bsddialog_mode = false;
		return (BSDDIALOG_ERROR);
	}

	return (BSDDIALOG_OK);
}

int bsddialog_end(void)
{
	if (endwin() != OK)
		RETURN_ERROR("Cannot end curses (endwin)");
	in_bsddialog_mode = false;

	return (BSDDIALOG_OK);
}

int bsddialog_backtitle(struct bsddialog_conf *conf, const char *backtitle)
{
	CHECK_PTR(conf);

	move(0, 1);
	clrtoeol();
	addstr(CHECK_STR(backtitle));
	if (conf->no_lines != true) {
		if (conf->ascii_lines)
			mvhline(1, 1, '-', SCREENCOLS - 2);
		else
			mvhline_set(1, 1, WACS_HLINE, SCREENCOLS - 2);
	}

	wnoutrefresh(stdscr);

	return (BSDDIALOG_OK);
}

int bsddialog_backtitle_rf(struct bsddialog_conf *conf, const char *backtitle)
{
	int rv;

	rv = bsddialog_backtitle(conf, backtitle);
	doupdate();

	return (rv);
}

bool bsddialog_inmode(void)
{
	return (in_bsddialog_mode);
}

const char *bsddialog_geterror(void)
{
	return (get_error_string());
}

int bsddialog_initconf(struct bsddialog_conf *conf)
{
	CHECK_PTR(conf);

	memset(conf, 0, sizeof(struct bsddialog_conf));
	conf->y = BSDDIALOG_CENTER;
	conf->x = BSDDIALOG_CENTER;
	conf->shadow = true;
	conf->text.cols_per_row = DEFAULT_COLS_PER_ROW;

	return (BSDDIALOG_OK);
}

void bsddialog_refresh(void)
{
	refresh();
}

void bsddialog_clear(unsigned int y)
{
	move(y, 0);
	clrtobot();
	refresh();
}