
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_printw.c
**
**	The routines printw(), wprintw() and friends.
**
*/

#include "curses.priv.h"

int printw(char *fmt, ...)
{
va_list argp;
char buf[BUFSIZ];

	T(("printw(\"%s\",...) called", fmt));

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);
	return(waddstr(stdscr, buf));
}



int wprintw(WINDOW *win, char *fmt, ...)
{
va_list argp;
char buf[BUFSIZ];

	T(("wprintw(%x,\"%s\",...) called", win, fmt));

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);
	return(waddstr(win, buf));
}



int mvprintw(int y, int x, char *fmt, ...)
{
va_list argp;
char buf[BUFSIZ];

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);
	return(move(y, x) == OK ? waddstr(stdscr, buf) : ERR);
}



int mvwprintw(WINDOW *win, int y, int x, char *fmt, ...)
{
va_list argp;
char buf[BUFSIZ];

	va_start(argp, fmt);
	vsprintf(buf, fmt, argp);
	va_end(argp);
	return(wmove(win, y, x) == OK ? waddstr(win, buf) : ERR);
}

int vwprintw(WINDOW *win, char *fmt, va_list argp)
{
char buf[BUFSIZ];

	vsprintf(buf, fmt, argp);
	return(waddstr(win, buf));
}
