
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_scanw.c
**
**	The routines scanw(), wscanw() and friends.
**
*/

#include <stdio.h>
#include "curses.priv.h"

int vwscanw(WINDOW *win, char *fmt, va_list argp)
{
char buf[BUFSIZ];

	if (wgetstr(win, buf) == ERR)
	    return(ERR);
	
	return(vsscanf(buf, fmt, argp));
}

int scanw(char *fmt, ...)
{
va_list ap;

	T(("scanw(\"%s\",...) called", fmt));

	va_start(ap, fmt);
	return(vwscanw(stdscr, fmt, ap));
}

int wscanw(WINDOW *win, char *fmt, ...)
{
va_list ap;

	T(("wscanw(%x,\"%s\",...) called", win, fmt));

	va_start(ap, fmt);
	return(vwscanw(win, fmt, ap));
}



int mvscanw(int y, int x, char *fmt, ...)
{
va_list ap;

	va_start(ap, fmt);
	return(move(y, x) == OK ? vwscanw(stdscr, fmt, ap) : ERR);
}



int mvwscanw(WINDOW *win, int y, int x, char *fmt, ...)
{
va_list ap;

	va_start(ap, fmt);
	return(wmove(win, y, x) == OK ? vwscanw(win, fmt, ap) : ERR);
}


