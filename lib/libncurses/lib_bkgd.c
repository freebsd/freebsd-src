
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
chtype old_bkgd = getbkgd(win);
chtype new_bkgd = ch;

	T(("wbkgd(%x, %x) called", win, ch));

	if (TextOf(new_bkgd) == 0)
		new_bkgd |= BLANK;
	wbkgdset(win, new_bkgd);
	wattrset(win, AttrOf(new_bkgd));

	for (y = 0; y <= win->_maxy; y++) {
		for (x = 0; x <= win->_maxx; x++) {
			if (win->_line[y][x] == old_bkgd)
				win->_line[y][x] = new_bkgd;
			else
				win->_line[y][x] =
					TextOf(win->_line[y][x])
					| AttrOf(new_bkgd);
		}
	}
	touchwin(win);
	return OK;
}

