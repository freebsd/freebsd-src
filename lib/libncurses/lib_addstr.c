
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_addstr.c
*
**	The routines waddnstr(), waddchnstr().
**
*/

#include "curses.priv.h"

int
waddnstr(WINDOW *win, char *str, int n)
{
	T(("waddnstr(%x,\"%s\",%d) called", win, visbuf(str), n));

	if (str == NULL)
		return ERR;

	if (n < 0) {
		while (*str != '\0') {
		    if (waddch(win, (chtype)(unsigned char)*str++) == ERR)
			return(ERR);
		}
		return OK;
	}

	while((n-- > 0) && (*str != '\0')) {
		if (waddch(win, (chtype)(unsigned char)*str++) == ERR)
			return ERR;
	}
	return OK;
}

int
waddchnstr(WINDOW *win, chtype *str, int n)
{
	T(("waddchnstr(%x,%x,%d) called", win, str, n));

	if (n < 0) {
		while (*str) {
		    if (waddch(win, (chtype)(unsigned char)*str++) == ERR)
			return(ERR);
		}
		return OK;
	}

	while(n-- > 0) {
		if (waddch(win, (chtype)(unsigned char)*str++) == ERR)
			return ERR;
	}
	return OK;
}
