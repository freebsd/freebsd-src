
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992, 1993, 1994                 *
*                          by Zeyd M. Ben-Halim                            *
*                          zmbenhal@netcom.com                             *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, not removed   *
*        from header files, and is reproduced in any documentation         *
*        accompanying it or the applications linked with it.               *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

#include "curses.h"
#include "curses.priv.h"

int wbkgd(WINDOW *win, chtype ch)
{
int x, y;

	T(("wbkgd(%x, %x) called", win, ch));
	for (y = 0; y < win->_maxy; y++)
		for (x = 0; x < win->_maxx; x++)
			if ((win->_line[y][x]&A_CHARTEXT) == ' ')
				win->_line[y][x] |= ch;
			else
				win->_line[y][x] |= (ch&A_ATTRIBUTES);
	touchwin(win);
	return OK;
}

