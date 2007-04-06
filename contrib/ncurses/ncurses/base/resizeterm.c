/****************************************************************************
 * Copyright (c) 1998-2004,2006 Free Software Foundation, Inc.              *
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
 *  Author: Thomas E. Dickey                                                *
 ****************************************************************************/

/*
 * This is an extension to the curses library.  It provides callers with a hook
 * into the NCURSES data to resize windows, primarily for use by programs
 * running in an X Window terminal (e.g., xterm).  I abstracted this module
 * from my application library for NCURSES because it must be compiled with
 * the private data structures -- T.Dickey 1995/7/4.
 */

#include <curses.priv.h>
#include <term.h>

MODULE_ID("$Id: resizeterm.c,v 1.18 2006/10/14 20:43:31 tom Exp $")

#define stolen_lines (screen_lines - SP->_lines_avail)

static int current_lines;
static int current_cols;

#ifdef TRACE
static void
show_window_sizes(const char *name)
{
    WINDOWLIST *wp;

    _tracef("%s resizing: %2d x %2d (%2d x %2d)", name, LINES, COLS,
	    screen_lines, screen_columns);
    for (wp = _nc_windows; wp != 0; wp = wp->next) {
	_tracef("  window %p is %2ld x %2ld at %2ld,%2ld",
		&(wp->win),
		(long) wp->win._maxy + 1,
		(long) wp->win._maxx + 1,
		(long) wp->win._begy,
		(long) wp->win._begx);
    }
}
#endif

NCURSES_EXPORT(bool)
is_term_resized(int ToLines, int ToCols)
{
    T((T_CALLED("is_term_resized(%d, %d)"), ToLines, ToCols));
    returnCode(ToLines > 0
	       && ToCols > 0
	       && (ToLines != screen_lines
		   || ToCols != screen_columns));
}

/*
 * Return the number of levels of child-windows under the current window.
 */
static int
child_depth(WINDOW *cmp)
{
    int depth = 0;

    if (cmp != 0) {
	WINDOWLIST *wp;

	for (wp = _nc_windows; wp != 0; wp = wp->next) {
	    WINDOW *tst = &(wp->win);
	    if (tst->_parent == cmp) {
		depth = 1 + child_depth(tst);
		break;
	    }
	}
    }
    return depth;
}

/*
 * Return the number of levels of parent-windows above the current window.
 */
static int
parent_depth(WINDOW *cmp)
{
    int depth = 0;

    if (cmp != 0) {
	WINDOW *tst;
	while ((tst = cmp->_parent) != 0) {
	    ++depth;
	    cmp = tst;
	}
    }
    return depth;
}

/*
 * FIXME: must adjust position so it's within the parent!
 */
static int
adjust_window(WINDOW *win, int ToLines, int ToCols, int stolen)
{
    int result;
    int bottom = current_lines + SP->_topstolen - stolen;
    int myLines = win->_maxy + 1;
    int myCols = win->_maxx + 1;

    T((T_CALLED("adjust_window(%p,%d,%d) currently %ldx%ld at %ld,%ld"),
       win, ToLines, ToCols,
       (long) getmaxy(win), (long) getmaxx(win),
       (long) getbegy(win), (long) getbegx(win)));

    if (win->_begy >= bottom) {
	win->_begy += (ToLines - current_lines);
    } else {
	if (myLines == current_lines - stolen
	    && ToLines != current_lines)
	    myLines = ToLines - stolen;
	else if (myLines == current_lines
		 && ToLines != current_lines)
	    myLines = ToLines;
    }

    if (myLines > ToLines)
	myLines = ToLines;

    if (myCols > ToCols)
	myCols = ToCols;

    if (myLines == current_lines
	&& ToLines != current_lines)
	myLines = ToLines;

    if (myCols == current_cols
	&& ToCols != current_cols)
	myCols = ToCols;

    result = wresize(win, myLines, myCols);
    returnCode(result);
}

/*
 * If we're decreasing size, recursively search for windows that have no
 * children, decrease those to fit, then decrease the containing window, etc.
 */
static int
decrease_size(int ToLines, int ToCols, int stolen)
{
    bool found;
    int depth = 0;
    WINDOWLIST *wp;

    T((T_CALLED("decrease_size(%d, %d)"), ToLines, ToCols));

    do {
	found = FALSE;
	TR(TRACE_UPDATE, ("decreasing size of windows to %dx%d, depth=%d",
			  ToLines, ToCols, depth));
	for (wp = _nc_windows; wp != 0; wp = wp->next) {
	    WINDOW *win = &(wp->win);

	    if (!(win->_flags & _ISPAD)) {
		if (child_depth(win) == depth) {
		    found = TRUE;
		    if (adjust_window(win, ToLines, ToCols, stolen) != OK)
			returnCode(ERR);
		}
	    }
	}
	++depth;
    } while (found);
    returnCode(OK);
}

/*
 * If we're increasing size, recursively search for windows that have no
 * parent, increase those to fit, then increase the contained window, etc.
 */
static int
increase_size(int ToLines, int ToCols, int stolen)
{
    bool found;
    int depth = 0;
    WINDOWLIST *wp;

    T((T_CALLED("increase_size(%d, %d)"), ToLines, ToCols));

    do {
	found = FALSE;
	TR(TRACE_UPDATE, ("increasing size of windows to %dx%d, depth=%d",
			  ToLines, ToCols, depth));
	for (wp = _nc_windows; wp != 0; wp = wp->next) {
	    WINDOW *win = &(wp->win);

	    if (!(win->_flags & _ISPAD)) {
		if (parent_depth(win) == depth) {
		    found = TRUE;
		    if (adjust_window(win, ToLines, ToCols, stolen) != OK)
			returnCode(ERR);
		}
	    }
	}
	++depth;
    } while (found);
    returnCode(OK);
}

/*
 * This function reallocates NCURSES window structures, with no side-effects
 * such as ungetch().
 */
NCURSES_EXPORT(int)
resize_term(int ToLines, int ToCols)
{
    int result = OK;
    int was_stolen = (screen_lines - SP->_lines_avail);

    T((T_CALLED("resize_term(%d,%d) old(%d,%d)"),
       ToLines, ToCols,
       screen_lines, screen_columns));

    if (is_term_resized(ToLines, ToCols)) {
	int myLines = current_lines = screen_lines;
	int myCols = current_cols = screen_columns;

#ifdef TRACE
	if (_nc_tracing & TRACE_UPDATE)
	    show_window_sizes("before");
#endif
	if (ToLines > screen_lines) {
	    increase_size(myLines = ToLines, myCols, was_stolen);
	    current_lines = myLines;
	    current_cols = myCols;
	}

	if (ToCols > screen_columns) {
	    increase_size(myLines, myCols = ToCols, was_stolen);
	    current_lines = myLines;
	    current_cols = myCols;
	}

	if (ToLines < myLines ||
	    ToCols < myCols) {
	    decrease_size(ToLines, ToCols, was_stolen);
	}

	screen_lines = lines = ToLines;
	screen_columns = columns = ToCols;

	SP->_lines_avail = lines - was_stolen;

	if (SP->oldhash) {
	    FreeAndNull(SP->oldhash);
	}
	if (SP->newhash) {
	    FreeAndNull(SP->newhash);
	}
#ifdef TRACE
	if (_nc_tracing & TRACE_UPDATE) {
	    LINES = ToLines - was_stolen;
	    COLS = ToCols;
	    show_window_sizes("after");
	}
#endif
    }

    /*
     * Always update LINES, to allow for call from lib_doupdate.c which
     * needs to have the count adjusted by the stolen (ripped off) lines.
     */
    LINES = ToLines - was_stolen;
    COLS = ToCols;

    returnCode(result);
}

/*
 * This function reallocates NCURSES window structures.  It is invoked in
 * response to a SIGWINCH interrupt.  Other user-defined windows may also need
 * to be reallocated.
 *
 * Because this performs memory allocation, it should not (in general) be
 * invoked directly from the signal handler.
 */
NCURSES_EXPORT(int)
resizeterm(int ToLines, int ToCols)
{
    int result = OK;

    SP->_sig_winch = FALSE;

    T((T_CALLED("resizeterm(%d,%d) old(%d,%d)"),
       ToLines, ToCols,
       screen_lines, screen_columns));

    if (is_term_resized(ToLines, ToCols)) {

#if USE_SIGWINCH
	ungetch(KEY_RESIZE);	/* so application can know this */
	clearok(curscr, TRUE);	/* screen contents are unknown */
#endif

	result = resize_term(ToLines, ToCols);
    }

    returnCode(result);
}
