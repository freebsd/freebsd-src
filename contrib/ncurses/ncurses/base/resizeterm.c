/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1996,1997                   *
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

MODULE_ID("$Id: resizeterm.c,v 1.7 1998/09/19 19:27:43 Alexander.V.Lukyanov Exp $")

/*
 * This function reallocates NCURSES window structures.  It is invoked in
 * response to a SIGWINCH interrupt.  Other user-defined windows may also need
 * to be reallocated.
 *
 * Because this performs memory allocation, it should not (in general) be
 * invoked directly from the signal handler.
 */
int
resizeterm(int ToLines, int ToCols)
{
	int stolen = screen_lines - SP->_lines_avail;
	int bottom = screen_lines + SP->_topstolen - stolen;

	T((T_CALLED("resizeterm(%d,%d) old(%d,%d)"),
		ToLines, ToCols,
		screen_lines, screen_columns));

	SP->_sig_winch = FALSE;

	if (ToLines != screen_lines
	 || ToCols  != screen_columns) {
		WINDOWLIST *wp;

#if USE_SIGWINCH
		ungetch(KEY_RESIZE);	/* so application can know this */
		clearok(curscr, TRUE);	/* screen contents are unknown */
#endif

		for (wp = _nc_windows; wp != 0; wp = wp->next) {
			WINDOW *win = wp->win;
			int myLines = win->_maxy + 1;
			int myCols  = win->_maxx + 1;

			/* pads aren't treated this way */
			if (win->_flags & _ISPAD)
				continue;

			if (win->_begy >= bottom) {
				win->_begy += (ToLines - screen_lines);
			} else {
				if (myLines == screen_lines - stolen
				 && ToLines != screen_lines)
				 	myLines = ToLines - stolen;
				else
				if (myLines == screen_lines
				 && ToLines != screen_lines)
				 	myLines = ToLines;
			}

			if (myCols  == screen_columns
			 && ToCols  != screen_columns)
			 	myCols = ToCols;

			if (wresize(win, myLines, myCols) != OK)
				returnCode(ERR);
		}

		screen_lines   = lines    = ToLines;
		screen_columns = columns  = ToCols;

		SP->_lines_avail = lines - stolen;

		if (SP->oldhash) { FreeAndNull(SP->oldhash); }
		if (SP->newhash) { FreeAndNull(SP->newhash); }
	}

	/*
	 * Always update LINES, to allow for call from lib_doupdate.c which
	 * needs to have the count adjusted by the stolen (ripped off) lines.
	 */
	LINES = ToLines - stolen;
	COLS  = ToCols;

	returnCode(OK);
}
