
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_insch.c
**
**	The routine winsch().
**
*/

#include "curses.priv.h"

int  winsch(WINDOW *win, chtype c)
{
chtype	*temp1, *temp2;
chtype	*end;

	T(("winsch(%x,'%x') called", win, c));

	end = &win->_line[win->_cury][win->_curx];
	temp1 = &win->_line[win->_cury][win->_maxx];
	temp2 = temp1 - 1;

	while (temp1 > end)
	    *temp1-- = *temp2--;

	*temp1 = _nc_render(win, c);

	win->_lastchar[win->_cury] = win->_maxx;
	if (win->_firstchar[win->_cury] == _NOCHANGE
	    			||  win->_firstchar[win->_cury] > win->_curx)
	    win->_firstchar[win->_cury] = win->_curx;
	return OK;
}
