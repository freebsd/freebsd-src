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

/****************************************************************************
 *  Author: Thomas E. Dickey <dickey@clark.net> 1997                        *
 ****************************************************************************/

/*
**	lib_printw.c
**
**	The routines printw(), wprintw() and friends.
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_printw.c,v 1.7 1998/04/11 22:53:44 tom Exp $")

int printw(NCURSES_CONST char *fmt, ...)
{
	va_list argp;
	int code;

	T(("printw(%s,...) called", _nc_visbuf(fmt)));

	va_start(argp, fmt);
	code = vwprintw(stdscr, fmt, argp);
	va_end(argp);

	return code;
}

int wprintw(WINDOW *win, NCURSES_CONST char *fmt, ...)
{
	va_list argp;
	int code;

	T(("wprintw(%p,%s,...) called", win, _nc_visbuf(fmt)));

	va_start(argp, fmt);
	code = vwprintw(win, fmt, argp);
	va_end(argp);

	return code;
}

int mvprintw(int y, int x, NCURSES_CONST char *fmt, ...)
{
	va_list argp;
	int code = move(y, x);

	if (code != ERR) {
		va_start(argp, fmt);
		code = vwprintw(stdscr, fmt, argp);
		va_end(argp);
	}
	return code;
}

int mvwprintw(WINDOW *win, int y, int x, NCURSES_CONST char *fmt, ...)
{
	va_list argp;
	int code = wmove(win, y, x);

	if (code != ERR) {
		va_start(argp, fmt);
		code = vwprintw(win, fmt, argp);
		va_end(argp);
	}
	return code;
}

int vwprintw(WINDOW *win, NCURSES_CONST char *fmt, va_list argp)
{
	char *buf = _nc_printf_string(fmt, argp);
	int code = ERR;

	if (buf != 0) {
		code = waddstr(win, buf);
#if USE_SAFE_SPRINTF
		free(buf);
#endif
	}
	return code;
}
