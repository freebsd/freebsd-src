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

#include <unistd.h>

#ifdef PORTNCURSES
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#include "bsddialog.h"
#include "lib_util.h"
#include "bsddialog_theme.h"

/* "Command": prgbox - programbox - progressbox */

#define MAXINPUT 2048 /* in bsddialoh.h? in bsddialog.c get/set static maxinput? */

extern struct bsddialog_theme t;

static int
command_handler(WINDOW *window, int y, int cols, struct buttons bs, bool shortkey)
{
	bool loop, update;
	int i, input;
	int output;

	loop = update = true;
	while(loop) {
		if (update) {
			draw_buttons(window, y, cols, bs, shortkey);
			update = false;
		}
		wrefresh(window);
		input = getch();
		switch (input) {
		case 10: /* Enter */
			output = bs.value[bs.curr];
			loop = false;
			break;
		case 27: /* Esc */
			output = BSDDIALOG_ESC;
			loop = false;
			break;
		case '\t': /* TAB */
			bs.curr = (bs.curr + 1) % bs.nbuttons;
			update = true;
			break;
		case KEY_LEFT:
			if (bs.curr > 0) {
				bs.curr--;
				update = true;
			}
			break;
		case KEY_RIGHT:
			if (bs.curr < (int) bs.nbuttons - 1) {
				bs.curr++;
				update = true;
			}
			break;
		default:
			if (shortkey) {
				for (i = 0; i < (int) bs.nbuttons; i++)
					if (input == (bs.label[i])[0]) {
						output = bs.value[i];
						loop = false;
				}
			}
		}
	}

	return output;
}

int
bsddialog_prgbox(struct bsddialog_conf conf, char* text, int rows, int cols, char *command)
{
	char line[MAXINPUT];
	WINDOW *widget, *pad, *shadow;
	int i, y, x, padrows, padcols, ys, ye, xs, xe;
	int output;
	int pipefd[2];
	struct buttons bs;

	if (new_widget(conf, &widget, &y, &x, text, &rows, &cols, &shadow,
	    true) <0)
		return -1;

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    NULL, BUTTONLABEL(help_label));

	if (text != NULL && conf.no_lines == false) {
		print_text(conf, widget, 1, 1, cols-2, text);
		mvwhline(widget, 2, 2, conf.ascii_lines ? '-' : ACS_HLINE, cols -4);
		wrefresh(widget);
	}

	padrows = text == NULL ? rows - 4 : rows - 6;
	padcols = cols - 2;
	ys = text == NULL ? y + 1 : y + 3;
	xs = x + 1;
	ye = ys + padrows;
	xe = xs + padcols;

	pad = newpad(padrows, padcols);
	wbkgd(pad, t.widgetcolor);

	pipe(pipefd);
	if (fork() == 0)
	{
		close(pipefd[0]);    // close reading

		dup2(pipefd[1], 1);  // send stdout to the pipe
		dup2(pipefd[1], 2);  // send stderr to the pipe

		close(pipefd[1]);    // this descriptor is no longer needed

		//const char *ls="/bin/ls";
		execl(command, command, NULL);
		return 0;
	}
	else
	{
		close(pipefd[1]);  // close write

		i = 0;
		while (read(pipefd[0], line, MAXINPUT) != 0) {
			mvwaddstr(pad, i, 0, line);
			prefresh(pad, 0, 0, ys, xs, ye, xe);
			i++;
		}
	}

	output = command_handler(widget, rows-2, cols, bs, true);

	return output;
}

int bsddialog_programbox(struct bsddialog_conf conf, char* text, int rows, int cols)
{
	char line[MAXINPUT];
	WINDOW *widget, *pad, *shadow;
	int i, y, x, padrows, padcols, ys, ye, xs, xe, output;
	struct buttons bs;

	if (new_widget(conf, &widget, &y, &x, text, &rows, &cols, &shadow,
	    true) <0)
		return -1;

	get_buttons(conf, &bs, BUTTONLABEL(ok_label), BUTTONLABEL(extra_label),
	    BUTTONLABEL(cancel_label), BUTTONLABEL(help_label));

	if (text != NULL && conf.no_lines == false) {
		mvwhline(widget, 2, 2, conf.ascii_lines ? '-' : ACS_HLINE, cols -4);
		wrefresh(widget);
	}

	padrows = text == NULL ? rows - 4 : rows - 6;
	padcols = cols - 2;
	ys = text == NULL ? y + 1 : y + 3;
	xs = x + 1;
	ye = ys + padrows;
	xe = xs + padcols;

	pad = newpad(padrows, padcols);

	i = 0;
	//while (fgets(line, MAXINPUT, stdin) != NULL) {
	while(getstr(line) != ERR){
		mvwaddstr(pad, i, 0, line);
		prefresh(pad, 0, 0, ys, xs, ye, xe);
		i++;
	}
	
	output = command_handler(widget, rows-2, cols, bs, true);

	return output;
}

int bsddialog_progressbox(struct bsddialog_conf conf, char* text, int rows, int cols)
{
	text = "Progressbox unimplemented";
	bsddialog_msgbox(conf, text, rows, cols);
	RETURN_ERROR(text);
}

