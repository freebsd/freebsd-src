
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_clrbot.c
**
**	The routine wclrtobot().
**
*/

#include "curses.priv.h"

#define BLANK ' '|A_NORMAL

int wclrtobot(WINDOW *win)
{
chtype	*ptr, *end, *maxx = NULL;
int	y, startx, minx;

	T(("wclrtobot(%x) called", win));

	startx = win->_curx;

	T(("clearing from y = %d to y = %d with maxx =  %d", win->_cury, win->_maxy, win->_maxx));

	for (y = win->_cury; y <= win->_maxy; y++) {
	    minx = _NOCHANGE;
	    end = &win->_line[y][win->_maxx];

	    for (ptr = &win->_line[y][startx]; ptr <= end; ptr++) {
			if (*ptr != BLANK) {
			    maxx = ptr;
			    if (minx == _NOCHANGE)
					minx = ptr - win->_line[y];
			    *ptr = BLANK;
			}
	    }

	    if (minx != _NOCHANGE) {
			if (win->_firstchar[y] > minx
					||  win->_firstchar[y] == _NOCHANGE)
			    win->_firstchar[y] = minx;

			if (win->_lastchar[y] < maxx - win->_line[y])
			    win->_lastchar[y] = maxx - win->_line[y];
	    }

	    startx = 0;
	}
	return OK;
}
