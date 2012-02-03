/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

__FBSDID("$FreeBSD$");

#ifndef lint
static const char sccsid[] = "@(#)display.c	8.1 (Berkeley) 6/6/93";
#endif

/*
 * The window 'manager', initializes curses and handles the actual
 * displaying of text
 */
#include <ctype.h>
#include <unistd.h>

#include "talk.h"

xwin_t	my_win;
xwin_t	his_win;
WINDOW	*line_win;

int	curses_initialized = 0;

/*
 * max HAS to be a function, it is called with
 * an argument of the form --foo at least once.
 */
int
max(int a, int b)
{

	return (a > b ? a : b);
}

/*
 * Display some text on somebody's window, processing some control
 * characters while we are at it.
 */
void
display(xwin_t *win, char *text, int size)
{
	int i;
	char cch;

	for (i = 0; i < size; i++) {
		if (*text == '\n' || *text == '\r') {
			waddch(win->x_win, '\n');
			getyx(win->x_win, win->x_line, win->x_col);
			text++;
			continue;
		}
		if (*text == 004 && win == &my_win) {
			/* control-D clears the screen */
			werase(my_win.x_win);
			getyx(my_win.x_win, my_win.x_line, my_win.x_col);
			wrefresh(my_win.x_win);
			werase(his_win.x_win);
			getyx(his_win.x_win, his_win.x_line, his_win.x_col);
			wrefresh(his_win.x_win);
			text++;
			continue;
		}

		/* erase character */
		if (   *text == win->cerase
		    || *text == 010     /* BS */
		    || *text == 0177    /* DEL */
		   ) {
			wmove(win->x_win, win->x_line, max(--win->x_col, 0));
			getyx(win->x_win, win->x_line, win->x_col);
			waddch(win->x_win, ' ');
			wmove(win->x_win, win->x_line, win->x_col);
			getyx(win->x_win, win->x_line, win->x_col);
			text++;
			continue;
		}
		/*
		 * On word erase search backwards until we find
		 * the beginning of a word or the beginning of
		 * the line.
		 */
		if (   *text == win->werase
		    || *text == 027     /* ^W */
		   ) {
			int endcol, xcol, ii, c;

			endcol = win->x_col;
			xcol = endcol - 1;
			while (xcol >= 0) {
				c = readwin(win->x_win, win->x_line, xcol);
				if (c != ' ')
					break;
				xcol--;
			}
			while (xcol >= 0) {
				c = readwin(win->x_win, win->x_line, xcol);
				if (c == ' ')
					break;
				xcol--;
			}
			wmove(win->x_win, win->x_line, xcol + 1);
			for (ii = xcol + 1; ii < endcol; ii++)
				waddch(win->x_win, ' ');
			wmove(win->x_win, win->x_line, xcol + 1);
			getyx(win->x_win, win->x_line, win->x_col);
			text++;
			continue;
		}
		/* line kill */
		if (   *text == win->kill
		    || *text == 025     /* ^U */
		   ) {
			wmove(win->x_win, win->x_line, 0);
			wclrtoeol(win->x_win);
			getyx(win->x_win, win->x_line, win->x_col);
			text++;
			continue;
		}
		if (*text == '\f') {
			if (win == &my_win)
				wrefresh(curscr);
			text++;
			continue;
		}
		if (*text == '\7') {
			write(STDOUT_FILENO, text, 1);
			text++;
			continue;
		}
		if (!isprint((unsigned char)*text) && *text != '\t') {
			waddch(win->x_win, '^');
			getyx(win->x_win, win->x_line, win->x_col);
			cch = (*text & 63) + 64;
			waddch(win->x_win, cch);
		} else
			waddch(win->x_win, (unsigned char)*text);
		getyx(win->x_win, win->x_line, win->x_col);
		text++;
	}
	wrefresh(win->x_win);
}

/*
 * Read the character at the indicated position in win
 */
int
readwin(WINDOW *win, int line, int col)
{
	int oldline, oldcol;
	int c;

	getyx(win, oldline, oldcol);
	wmove(win, line, col);
	c = winch(win);
	wmove(win, oldline, oldcol);
	return (c);
}
