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
 *  Author: Thomas E. Dickey 2002                                           *
 ****************************************************************************/

/*
**	lib_get_wstr.c
**
**	The routine wgetn_wstr().
**
*/

#include <curses.priv.h>
#include <term.h>

MODULE_ID("$Id: lib_get_wstr.c,v 1.3 2002/05/11 22:29:43 tom Exp $")

/*
 * This wipes out the last character, no matter whether it was a tab, control
 * or other character, and handles reverse wraparound.
 */
static wchar_t *
WipeOut(WINDOW *win, int y, int x, wchar_t * first, wchar_t * last, bool echoed)
{
    if (last > first) {
	*--last = '\0';
	if (echoed) {
	    int y1 = win->_cury;
	    int x1 = win->_curx;

	    wmove(win, y, x);
	    waddwstr(win, first);
	    getyx(win, y, x);
	    while (win->_cury < y1
		   || (win->_cury == y1 && win->_curx < x1))
		waddch(win, (chtype) ' ');

	    wmove(win, y, x);
	}
    }
    return last;
}

NCURSES_EXPORT(int)
wgetn_wstr(WINDOW *win, wint_t * str, int maxlen)
{
    TTY buf;
    bool oldnl, oldecho, oldraw, oldcbreak;
    wint_t erasec;
    wint_t killc;
    wchar_t *oldstr;
    wchar_t *tmpstr;
    wint_t ch;
    int y, x, code;

    T((T_CALLED("wgetn_wstr(%p,%p, %d)"), win, str, maxlen));

    if (!win)
	returnCode(ERR);

    _nc_get_tty_mode(&buf);

    oldnl = SP->_nl;
    oldecho = SP->_echo;
    oldraw = SP->_raw;
    oldcbreak = SP->_cbreak;
    nl();
    noecho();
    noraw();
    cbreak();

    erasec = erasechar();
    killc = killchar();

    assert(sizeof(wchar_t) == sizeof(wint_t));
    oldstr = (wchar_t *) str;
    tmpstr = (wchar_t *) str;

    getyx(win, y, x);

    if (is_wintouched(win) || (win->_flags & _HASMOVED))
	wrefresh(win);

    while ((code = wget_wch(win, &ch)) != ERR) {
	if (code == KEY_CODE_YES) {
	    /*
	     * Some terminals (the Wyse-50 is the most common) generate a \n
	     * from the down-arrow key.  With this logic, it's the user's
	     * choice whether to set kcud=\n for wget_wch(); terminating
	     * *getn_wstr() with \n should work either way.
	     */
	    if (ch == '\n'
		|| ch == '\r'
		|| ch == KEY_DOWN
		|| ch == KEY_ENTER) {
		if (oldecho == TRUE
		    && win->_cury == win->_maxy
		    && win->_scroll)
		    wechochar(win, (chtype) '\n');
		break;
	    }
	    if (ch == erasec || ch == KEY_LEFT || ch == KEY_BACKSPACE) {
		if (tmpstr > oldstr) {
		    tmpstr = WipeOut(win, y, x, oldstr, tmpstr, oldecho);
		}
	    } else if (ch == killc) {
		while (tmpstr > oldstr) {
		    tmpstr = WipeOut(win, y, x, oldstr, tmpstr, oldecho);
		}
	    } else {
		beep();
	    }
	} else if (maxlen >= 0 && tmpstr - oldstr >= maxlen) {
	    beep();
	} else {
	    *tmpstr++ = ch;
	    if (oldecho == TRUE) {
		int oldy = win->_cury;
		cchar_t tmp;

		setcchar(&tmp, tmpstr - 1, A_NORMAL, 0, NULL);
		if (wadd_wch(win, &tmp) == ERR) {
		    /*
		     * We can't really use the lower-right corner for input,
		     * since it'll mess up bookkeeping for erases.
		     */
		    win->_flags &= ~_WRAPPED;
		    waddch(win, (chtype) ' ');
		    tmpstr = WipeOut(win, y, x, oldstr, tmpstr, oldecho);
		    continue;
		} else if (win->_flags & _WRAPPED) {
		    /*
		     * If the last waddch forced a wrap & scroll, adjust our
		     * reference point for erasures.
		     */
		    if (win->_scroll
			&& oldy == win->_maxy
			&& win->_cury == win->_maxy) {
			if (--y <= 0) {
			    y = 0;
			}
		    }
		    win->_flags &= ~_WRAPPED;
		}
		wrefresh(win);
	    }
	}
    }

    win->_curx = 0;
    win->_flags &= ~_WRAPPED;
    if (win->_cury < win->_maxy)
	win->_cury++;
    wrefresh(win);

    /* Restore with a single I/O call, to fix minor asymmetry between
     * raw/noraw, etc.
     */
    SP->_nl = oldnl;
    SP->_echo = oldecho;
    SP->_raw = oldraw;
    SP->_cbreak = oldcbreak;

    _nc_set_tty_mode(&buf);

    *tmpstr = 0;
    if (code == ERR) {
	if (tmpstr == oldstr) {
	    *tmpstr++ = (wchar_t)WEOF;
	    *tmpstr = 0;
	}
	returnCode(ERR);
    }

    T(("wgetn_wstr returns %s", _nc_viswbuf(oldstr)));

    returnCode(OK);
}
