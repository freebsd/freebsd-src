
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/


/*
**	lib_instr.c
**
**	The routine winnstr().
**
*/

#include "curses.priv.h"

int winnstr(WINDOW *win, char *str, int n)
{
	int	i, row, col;

	T(("winnstr(%p,%p,%d) called", win, str, n));

	getyx(win, row, col);

	if (n < 0)
		n = win->_maxx - win->_curx + 1;

	for (i = 0; i < n;) {
		str[i++] = TextOf(win->_line[row][col]);
		if (++col > win->_maxx) {
			col = 0;
			if (++row > win->_maxy)
				break;
		}
	}
	str[i] = '\0';	/* SVr4 does not seem to count the null */

	return (i);
}
