
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_delwin.c
**
**	The routine delwin().
**
*/

#include <stdlib.h>
#include "curses.priv.h"

int delwin(WINDOW *win)
{
int	i;

	T(("delwin(%x) called", win));

	if (! (win->_flags & _SUBWIN)) {
	    for (i = 0; i < win->_maxy  &&  win->_line[i]; i++)
			free(win->_line[i]);
	}

	free(win->_firstchar);
	free(win->_lastchar);
	free(win->_line);
	free(win);

	touchwin(curscr);

	return(OK);
}
