
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_touch.c
**
**	   The routines	untouchwin(),
**			wtouchln(),
**			is_linetouched()
**			is_wintouched().
**
*/

#include "curses.priv.h"

int is_linetouched(WINDOW *win, int line)
{
	if (line > win->_maxy || line < 0)
		return ERR;
	if (win->_firstchar[line] != _NOCHANGE) return TRUE;
	return FALSE;
}

int is_wintouched(WINDOW *win)
{
int i;

	for (i = 0; i <= win->_maxy; i++)
		if (win->_firstchar[i] != _NOCHANGE)
			return TRUE;
	return FALSE;
}

int wtouchln(WINDOW *win, int y, int n, int changed)
{
int i;

	T(("wtouchln(%x,%d,%d,%d)", win, y, n, changed));

	for (i = y; i < y+n; i++) {
		win->_firstchar[i] = changed ? 0 : _NOCHANGE;
		win->_lastchar[i] = changed ? win->_maxx : _NOCHANGE;
	}
	return OK;
}

