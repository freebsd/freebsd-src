
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_move.c
**
**	The routine wmove().
**
*/

#include "curses.priv.h"

int
wmove(WINDOW *win, int y, int x)
{
	T(("wmove(%x,%d,%d) called", win, y, x));

	if (x >= 0  &&  x <= win->_maxx  &&
		y >= 0  &&  y <= win->_maxy)
	{
		win->_curx = x;
		win->_cury = y;

		win->_flags |= _HASMOVED;
		return(OK);
	} else
		return(ERR);
}
