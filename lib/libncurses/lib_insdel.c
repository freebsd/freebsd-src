
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_insdel.c
**
**	The routine winsdel(win, n).
**  positive n insert n lines above current line
**  negative n delete n lines starting from current line
**
*/

#include <stdlib.h>
#include "curses.priv.h"
#include "terminfo.h"

int
winsdelln(WINDOW *win, int n)
{
	int ret, sscroll, stop, sbot;

	T(("winsdel(%x,%d) called", win, n));

	if (n == 0)
		return OK;
	if (win->_cury == win->_maxy && abs(n) == 1)
		return wclrtoeol(win);
	if (n < 0 && win->_cury - n > win->_maxy)
		/* request to delete too many lines */
		/* should we truncate to an appropriate number? */
		return ERR;

	sscroll = win->_scroll;
	stop = win->_regtop;
	sbot = win->_regbottom;

	win->_scroll = TRUE;
	win->_regtop = win->_cury;
	if (win->_regtop > win->_regbottom)
		win->_regbottom = win->_maxy;

	ret = wscrl(win, -n);

	win->_scroll = sscroll;
	win->_regtop = stop;
	win->_regbottom = sbot;

	return ret;
}
