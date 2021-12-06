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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/*
 * This file implements some public function not related to a specific widget.
 * utils.h/c provides private functions to implement the library.
 * theme.h/c is public API related to theme.
 * Widgets implementation:
 *  infobox.c    infobox
 *  messgebox.c  msgbox - yesno
 *  menubox.c    buildlist - checklist - menu - mixedlist - radiolist
 *  formbox.c    inputbox - passwordbox - form - passwordform - mixedform
 *  barbox.c     gauge - mixedgauge - rangebox - pause
 *  textbox.c    textbox
 *  timebox.c    timebox - calendar
 */

extern struct bsddialog_theme t;

int bsddialog_init(void)
{
	int i, j, c = 1, error = OK;

	set_error_string("");

	if(initscr() == NULL)
		RETURN_ERROR("Cannot init ncurses (initscr)");

	error += keypad(stdscr, TRUE);
	nl();
	error += cbreak();
	error += noecho();
	curs_set(0);
	if(error != OK) {
		bsddialog_end();
		RETURN_ERROR("Cannot init ncurses (keypad and cursor)");
	}

	error += start_color();
	for (i=0; i<8; i++)
		for(j=0; j<8; j++) {
			error += init_pair(c, i, j);
			c++;
	}
	if(error != OK) {
		bsddialog_end();
		RETURN_ERROR("Cannot init ncurses (colors)");
	}

	if (bsddialog_set_default_theme(BSDDIALOG_THEME_DIALOG) != 0)
		error = BSDDIALOG_ERROR;

	return error;
}

int bsddialog_end(void)
{

	if (endwin() != OK)
		RETURN_ERROR("Cannot end ncurses (endwin)");

	return 0;
}

int bsddialog_backtitle(struct bsddialog_conf *conf, char *backtitle)
{

	mvaddstr(0, 1, backtitle);
	if (conf->no_lines != true)
		mvhline(1, 1, conf->ascii_lines ? '-' : ACS_HLINE, COLS-2);

	refresh();

	return 0;
}

const char *bsddialog_geterror(void)
{

	return get_error_string();
}

int bsddialog_terminalheight(void)
{

	return LINES;
}

int bsddialog_terminalwidth(void)
{

	return COLS;
}

void bsddialog_initconf(struct bsddialog_conf *conf)
{

	memset(conf, 0, sizeof(struct bsddialog_conf));
	conf->x = conf->y = BSDDIALOG_CENTER;
	conf->shadow = true;
}
