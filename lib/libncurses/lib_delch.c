
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_delch.c
**
**	The routine wdelch().
**
*/

#include "curses.priv.h"
#include "terminfo.h"

int wdelch(WINDOW *win)
{
chtype	*temp1, *temp2;
chtype	*end;
chtype	blank = _nc_background(win);

	T(("wdelch(%x) called", win));

	end = &win->_line[win->_cury][win->_maxx];
	temp2 = &win->_line[win->_cury][win->_curx + 1];
	temp1 = temp2 - 1;

	while (temp1 < end)
	    *temp1++ = *temp2++;

	*temp1 = blank;

	win->_lastchar[win->_cury] = win->_maxx;

	if (win->_firstchar[win->_cury] == _NOCHANGE
				   || win->_firstchar[win->_cury] > win->_curx)
	    win->_firstchar[win->_cury] = win->_curx;
	return OK;
}
