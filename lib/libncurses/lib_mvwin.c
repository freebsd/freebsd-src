
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_mvwin.c
**
**	The routine mvwin().
**
*/

#include "curses.priv.h"
#include "terminfo.h"

int mvwin(WINDOW *win, int by, int bx)
{
	T(("mvwin(%x,%d,%d) called", win, by, bx));

	if (by + win->_maxy > lines - 1  ||  bx + win->_maxx > columns - 1)
	    return(ERR);

	win->_begy = by;
	win->_begx = bx;

	touchwin(win);

	return(OK);
}
