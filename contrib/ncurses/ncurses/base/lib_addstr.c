/****************************************************************************
 * Copyright (c) 1998,1999,2000 Free Software Foundation, Inc.              *
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
**	lib_addstr.c
*
**	The routines waddnstr(), waddchnstr().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_addstr.c,v 1.17 2000/04/29 21:15:55 tom Exp $")

int
waddnstr(WINDOW *win, const char *const astr, int n)
{
    unsigned const char *str = (unsigned const char *) astr;
    int code = ERR;

    T((T_CALLED("waddnstr(%p,%s,%d)"), win, _nc_visbuf(astr), n));

    if (win && (str != 0)) {
	T(("... current %s", _traceattr(win->_attrs)));
	TR(TRACE_VIRTPUT, ("str is not null"));
	code = OK;
	if (n < 0)
	    n = (int) strlen(astr);

	while ((n-- > 0) && (*str != '\0')) {
	    TR(TRACE_VIRTPUT, ("*str = %#x", *str));
	    if (_nc_waddch_nosync(win, (chtype) * str++) == ERR) {
		code = ERR;
		break;
	    }
	}
	_nc_synchook(win);
    }
    TR(TRACE_VIRTPUT, ("waddnstr returns %d", code));
    returnCode(code);
}

int
waddchnstr(WINDOW *win, const chtype * const astr, int n)
{
    NCURSES_SIZE_T y = win->_cury;
    NCURSES_SIZE_T x = win->_curx;
    int code = OK;
    struct ldat *line;

    T((T_CALLED("waddchnstr(%p,%p,%d)"), win, astr, n));

    if (!win)
	returnCode(ERR);

    if (n < 0) {
	const chtype *str;
	n = 0;
	for (str = (const chtype *) astr; *str != 0; str++)
	    n++;
    }
    if (n > win->_maxx - x + 1)
	n = win->_maxx - x + 1;
    if (n == 0)
	returnCode(code);

    line = &(win->_line[y]);
    memcpy(line->text + x, astr, n * sizeof(*astr));
    CHANGED_RANGE(line, x, x + n - 1);

    _nc_synchook(win);
    returnCode(code);
}
