/****************************************************************************
 * Copyright (c) 2007,2008 Free Software Foundation, Inc.                   *
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
 *     Author: Thomas E. Dickey                        2007                 *
 ****************************************************************************/

#include <curses.priv.h>

MODULE_ID("$Id: use_window.c,v 1.7 2008/05/03 14:09:38 tom Exp $")

#ifdef USE_PTHREADS
NCURSES_EXPORT(void)
_nc_lock_window(const WINDOW *win)
{
    WINDOWLIST *p;

    _nc_lock_global(windowlist);
    for (each_window(p)) {
	if (&(p->win) == win) {
	    _nc_mutex_lock(&(p->mutex_use_window));
	    break;
	}
    }
}

NCURSES_EXPORT(void)
_nc_unlock_window(const WINDOW *win)
{
    WINDOWLIST *p;

    for (each_window(p)) {
	if (&(p->win) == win) {
	    _nc_mutex_unlock(&(p->mutex_use_window));
	    break;
	}
    }
    _nc_unlock_global(windowlist);
}
#endif

NCURSES_EXPORT(int)
use_window(WINDOW *win, NCURSES_WINDOW_CB func, void *data)
{
    int code = OK;

    T((T_CALLED("use_window(%p,%p,%p)"), win, func, data));
    _nc_lock_window(win);
    code = func(win, data);
    _nc_unlock_window(win);

    returnCode(code);
}
