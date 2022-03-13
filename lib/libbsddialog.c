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

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bsddialog.h"
#include "bsddialog_theme.h"
#include "lib_util.h"

int bsddialog_init(void)
{
	int i, j, c, error;
	enum bsddialog_default_theme theme;

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

	c = 1;
	error += start_color();
	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			error += init_pair(c, i, j);
			c++;
		}
	}

	if (error == OK && has_colors())
		theme = BSDDIALOG_THEME_FLAT;
	else
		theme = BSDDIALOG_THEME_BLACKWHITE;

	if (bsddialog_set_default_theme(theme) != 0) {
		bsddialog_end();
		return (BSDDIALOG_ERROR);
	}

	return (BSDDIALOG_OK);
}

int bsddialog_end(void)
{
	if (endwin() != OK)
		RETURN_ERROR("Cannot end curses (endwin)");

	return (BSDDIALOG_OK);
}

int bsddialog_backtitle(struct bsddialog_conf *conf, const char *backtitle)
{
	mvaddstr(0, 1, backtitle);
	if (conf->no_lines != true)
		mvhline(1, 1, conf->ascii_lines ? '-' : ACS_HLINE,
		    SCREENCOLS - 2);

	refresh();

	return (BSDDIALOG_OK);
}

const char *bsddialog_geterror(void)
{
	return (get_error_string());
}

int bsddialog_initconf(struct bsddialog_conf *conf)
{
	if (conf == NULL)
		RETURN_ERROR("conf is NULL");
	if (sizeof(*conf) != sizeof(struct bsddialog_conf))
		RETURN_ERROR("Bad conf size");

	memset(conf, 0, sizeof(struct bsddialog_conf));
	conf->y = BSDDIALOG_CENTER;
	conf->x = BSDDIALOG_CENTER;
	conf->shadow = true;

	return (BSDDIALOG_OK);
}

int bsddialog_clearterminal(void)
{
	if (clear() != OK)
		RETURN_ERROR("Cannot clear the terminal");
	refresh();

	return (BSDDIALOG_OK);
}