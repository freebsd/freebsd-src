
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_deleteln.c
**
**	The routine wdeleteln().
**
*/

#include "curses.priv.h"
#include <nterm.h>

int wdeleteln(WINDOW *win)
{
chtype	*end, *temp;
int	y, touched = 0;

    T(("wdeleteln(%x) called", win));

	temp = win->_line[win->_cury];

	if (win->_idlok && (delete_line != NULL)) {
		if (back_color_erase) {
			T(("back_color_erase, turning attributes off"));
			vidattr(curscr->_attrs = A_NORMAL);
		}
		putp(delete_line);
		touched = 1;
	}

	for (y = win->_cury; y < win->_regbottom; y++) {
	    win->_line[y] = win->_line[y+1];

	    if (!touched) {
	        win->_firstchar[y] = 0;
			win->_lastchar[y] = win->_maxx;
	    }
	}

	win->_line[win->_regbottom] = temp;
	if (!touched) {
	    win->_firstchar[win->_regbottom] = 0;
	    win->_lastchar[win->_regbottom] = win->_maxx;
	}

	for (end = &(temp[win->_maxx]); temp <= end; )
	    *temp++ = ' ';
	return OK;
}
