
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_clear.c
**
**	The routine wclear().
**
*/

#include "curses.priv.h"

int wclear(WINDOW *win)
{
	T(("wclear(%x) called", win));

	werase(win);

	win->_clear = TRUE;

	return OK;
}
