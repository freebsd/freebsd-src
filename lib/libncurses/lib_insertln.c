
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of	*
*  the copyright conditions.										*/

/*
**	lib_insertln.c
**
**	The routine winsertln().
**
*/

#include "curses.priv.h"
#include <nterm.h>

int  winsertln(WINDOW *win)
{
chtype	*temp, *end;
int	y, touched = 0;

	T(("winsertln(%x) called", win));

	temp = win->_line[win->_regbottom];

	if (win->_idlok && (insert_line != NULL)) {
		wrefresh(win);
		putp(insert_line);
		touched = 1;
	}

	if (!touched) {
		win->_firstchar[win->_cury] = 0;
		win->_lastchar[win->_cury] = win->_maxx;
	}

	for (y = win->_regbottom;  y > win->_cury;  y--) {
		win->_line[y] = win->_line[y-1];

		if (!touched) {
			win->_firstchar[y] = 0;
			win->_lastchar[y] = win->_maxx;
		}
	}

	win->_line[win->_cury] = temp;

	for (end = &temp[win->_maxx];  temp <= end;  temp++)
		*temp = ' ';
	return OK;
}
