
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_inchstr.c
**
**	The routine winchnstr().
**
*/

#include "curses.priv.h"

int  winchnstr(WINDOW *win, chtype *chstr, int i)
{
chtype	*point, *end;

	T(("winschnstr(%x,'%x',%d) called", win, chstr, i));

	point = &win->_line[win->_cury][win->_curx];
	end = &win->_line[win->_cury][win->_maxx];
	if (point + i - 1 < end)
		end = point + i - 1;

	chstr = (chtype *)malloc((end - point + 1)*sizeof(chtype));
	chstr[end - point] = '\0';
	return OK;
}
