
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

#include "curses.priv.h"

int
winsdelln(WINDOW *win, int n)
{
int code = ERR;

	T(("winsdel(%x,%d) called", win, n));

	if (win) {
	  if (n != 0) {
	    _nc_scroll_window(win, -n, win->_cury, win->_maxy, _nc_background(win));	  
	  }
	  code = OK;
	}
	return code;
}
