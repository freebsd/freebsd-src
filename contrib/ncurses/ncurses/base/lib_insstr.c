/****************************************************************************
 * Copyright (c) 1998,1999,2000,2001 Free Software Foundation, Inc.         *
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
**	lib_insstr.c
**
**	The routine winsnstr().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_insstr.c,v 1.19 2001/06/09 23:43:02 skimo Exp $")

NCURSES_EXPORT(int)
winsnstr(WINDOW *win, const char *s, int n)
{
    int code = ERR;
    NCURSES_SIZE_T oy;
    NCURSES_SIZE_T ox;
    const unsigned char *str = (const unsigned char *) s;
    const unsigned char *cp;

    T((T_CALLED("winsnstr(%p,%s,%d)"), win, _nc_visbuf(s), n));

    if (win && str) {
	oy = win->_cury;
	ox = win->_curx;
	for (cp = str; *cp && (n <= 0 || (cp - str) < n); cp++) {
	    if (*cp == '\n' || *cp == '\r' || *cp == '\t' || *cp == '\b') {
		NCURSES_CH_T wch;
		SetChar2(wch, *cp);
		_nc_waddch_nosync(win, wch);
	    } else if (is7bits(*cp) && iscntrl(*cp)) {
		winsch(win, ' ' + (chtype) (*cp));
		winsch(win, (chtype) '^');
		win->_curx += 2;
	    } else {
		winsch(win, (chtype) (*cp));
		win->_curx++;
	    }
	    if (win->_curx > win->_maxx)
		win->_curx = win->_maxx;
	}

	win->_curx = ox;
	win->_cury = oy;
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
