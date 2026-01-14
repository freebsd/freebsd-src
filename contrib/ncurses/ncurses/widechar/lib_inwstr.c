/****************************************************************************
 * Copyright 2020-2024,2025 Thomas E. Dickey                                *
 * Copyright 2002-2016,2017 Free Software Foundation, Inc.                  *
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
 * Author: Thomas Dickey                                                    *
 ****************************************************************************/

/*
**	lib_inwstr.c
**
**	The routine winnwstr().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_inwstr.c,v 1.14 2025/01/19 00:51:54 tom Exp $")

NCURSES_EXPORT(int)
winnwstr(WINDOW *win, wchar_t *wstr, int n)
{
    int count = 0;
    const cchar_t *text;

    T((T_CALLED("winnwstr(%p,%p,%d)"), (void *) win, (void *) wstr, n));
    if (wstr != NULL) {
	if (win) {
	    int row, col;
	    int last = 0;
	    bool done = FALSE;

	    getyx(win, row, col);

	    if (n < 0)
		n = CCHARW_MAX * (win->_maxx - win->_curx + 1);

	    text = win->_line[row].text;
	    while (count < n && !done && count != ERR) {

		if (!isWidecExt(text[col])) {
		    int inx;
		    wchar_t wch;

		    for (inx = 0; (inx < CCHARW_MAX)
			 && ((wch = text[col].chars[inx]) != 0);
			 ++inx) {
			if (count + 1 > n) {
			    done = TRUE;
			    if (last == 0) {
				count = ERR;	/* error if we store nothing */
			    } else {
				count = last;	/* only store complete chars */
			    }
			    break;
			}
			wstr[count++] = wch;
		    }
		}
		last = count;
		if (++col > win->_maxx) {
		    break;
		}
	    }
	}
	if (count > 0) {
	    wstr[count] = '\0';
	    T(("winnwstr returns %s", _nc_viswbuf(wstr)));
	}
    }
    returnCode(count);
}
