/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/*
 * Author:  Thomas E. Dickey <dickey@clark.net> 1998
 *
 * $Id: filter.c,v 1.5 2001/02/24 22:11:58 tom Exp $
 */
#include <test.priv.h>

/*
 * An example of the 'filter()' function in ncurses, this program prompts
 * for commands and executes them (like a command shell).  It illustrates
 * how ncurses can be used to implement programs that are not full-screen.
 *
 * Ncurses differs slightly from SVr4 curses.  The latter does not flush its
 * state when exiting program mode, so the attributes on the command lines of
 * this program 'bleed' onto the executed commands.  Rather than use the
 * reset_shell_mode() and reset_prog_mode() functions, we could invoke endwin()
 * and refresh(), but that does not work any better.
 */

#ifndef NCURSES_VERSION
#define attr_t long
#define getnstr(s,n) getstr(s)
#endif

static int
new_command(char *buffer, int length, attr_t underline)
{
    int code;

    attron(A_BOLD);
    printw("Command: ");
    attron(underline);
    code = getnstr(buffer, length);
    attroff(underline);
    attroff(A_BOLD);
    printw("\n");

    return code;
}

int
main(int argc GCC_UNUSED, char *argv[]GCC_UNUSED)
{
    char buffer[80];
    attr_t underline;

    filter();
    (void) newterm((char *) 0, stdout, stdin);
    cbreak();
    keypad(stdscr, TRUE);

    if (has_colors()) {
	int background = COLOR_BLACK;
	start_color();
#if HAVE_USE_DEFAULT_COLORS
	if (use_default_colors() != ERR)
	    background = -1;
#endif
	init_pair(1, COLOR_CYAN, background);
	underline = COLOR_PAIR(1);
    } else {
	underline = A_UNDERLINE;
    }

    while (new_command(buffer, sizeof(buffer) - 1, underline) != ERR
	   && strlen(buffer) != 0) {
	reset_shell_mode();
	printf("\n");
	fflush(stdout);
	system(buffer);
	reset_prog_mode();
	touchwin(stdscr);
	erase();
	refresh();
    }
    printw("done");
    refresh();
    endwin();
    return 0;
}
