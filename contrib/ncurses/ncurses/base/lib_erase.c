/****************************************************************************
 * Copyright (c) 1998,2000 Free Software Foundation, Inc.                   *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_erase.c
**
**	The routine werase().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_erase.c,v 1.13 2000/12/10 02:43:27 tom Exp $")

NCURSES_EXPORT(int)
werase(WINDOW *win)
{
    int code = ERR;
    int y;
    chtype blank;
    chtype *sp, *end, *start;

    T((T_CALLED("werase(%p)"), win));

    if (win) {
	blank = _nc_background(win);
	for (y = 0; y <= win->_maxy; y++) {
	    start = win->_line[y].text;
	    end = &start[win->_maxx];

	    for (sp = start; sp <= end; sp++)
		*sp = blank;

	    win->_line[y].firstchar = 0;
	    win->_line[y].lastchar = win->_maxx;
	}
	win->_curx = win->_cury = 0;
	win->_flags &= ~_WRAPPED;
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
