/****************************************************************************
 * Copyright (c) 2002 Free Software Foundation, Inc.                        *
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
 * Author: Thomas Dickey 2002                                               *
 ****************************************************************************/

/*
**	lib_ins_nwstr.c
**
**	The routine wins_nwstr().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_ins_nwstr.c,v 1.2 2002/03/10 22:43:12 tom Exp $")

NCURSES_EXPORT(int)
wins_nwstr(WINDOW *win, const wchar_t * wstr, int n)
{
    int code = ERR;
    NCURSES_SIZE_T oy;
    NCURSES_SIZE_T ox;
    const wchar_t *cp;

    T((T_CALLED("wins_nwstr(%p,%s,%d)"), win, _nc_viswbuf(wstr), n));

    if (win != 0
	&& wstr != 0
	&& wcwidth(*wstr) > 0) {
	code = OK;
	if (n < 1)
	    n = wcslen(wstr);
	oy = win->_cury;
	ox = win->_curx;
	for (cp = wstr; *cp && ((cp - wstr) < n); cp++) {
	    NCURSES_CH_T wch;
	    SetChar2(wch, *cp);
	    if (*cp == '\n' || *cp == '\r' || *cp == '\t' || *cp == '\b') {
		_nc_waddch_nosync(win, wch);
	    } else if (is7bits(*cp) && iscntrl(*cp)) {
		winsch(win, ' ' + (chtype) (*cp));
		winsch(win, (chtype) '^');
		win->_curx += 2;
	    } else if (wins_wch(win, &wch) == ERR
		       || win->_curx > win->_maxx) {
		break;
	    }
	}

	win->_curx = ox;
	win->_cury = oy;
	_nc_synchook(win);
	code = OK;
    }
    returnCode(code);
}
