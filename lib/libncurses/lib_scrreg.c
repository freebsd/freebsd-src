
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_scrreg.c
**
**	The routine wsetscrreg().
**
*/

#include "curses.priv.h"
#include "terminfo.h"

int wsetscrreg(WINDOW *win, int top, int bottom)
{
	T(("wsetscrreg(%x,%d,%d) called", win, top, bottom));

    	if (top >= 0  && top <= win->_maxy &&
		bottom >= 0  &&  bottom <= win->_maxy &&
		bottom > top)
	{
	    	win->_regtop = top;
	    	win->_regbottom = bottom;

		T(("correctly set scrolling region between %d and %d", top, bottom));
		if (change_scroll_region != NULL) {
			T(("changing scroll region"));
			putp(tparm(change_scroll_region, top, bottom));
		}

	    	return(OK);
	} else
	    	return(ERR);
}
