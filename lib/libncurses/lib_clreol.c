
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_clreol.c
**
**	The routine wclrtoeol().
**
*/

#include "curses.priv.h"

#define BLANK ' '|A_NORMAL

int  wclrtoeol(WINDOW *win)
{
chtype	*maxx, *ptr, *end;
int	y, x, minx;

	T(("wclrtoeol(%x) called", win));

	y = win->_cury;
	x = win->_curx;

	end = &win->_line[y][win->_maxx];
	minx = _NOCHANGE;
	maxx = &win->_line[y][x];

	for (ptr = maxx; ptr <= end; ptr++) {
	    if (*ptr != BLANK) {
			maxx = ptr;
			if (minx == _NOCHANGE)
			    minx = ptr - win->_line[y];
			*ptr = BLANK;
	    }
	}

	if (minx != _NOCHANGE) {
	    if (win->_firstchar[y] > minx || win->_firstchar[y] == _NOCHANGE)
			win->_firstchar[y] = minx;

	    if (win->_lastchar[y] < maxx - win->_line[y])
			win->_lastchar[y] = maxx - win->_line[y];
	}
	return(OK);
}
