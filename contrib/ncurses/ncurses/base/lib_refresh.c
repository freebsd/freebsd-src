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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/



/*
 *	lib_refresh.c
 *
 *	The routines wrefresh() and wnoutrefresh().
 *
 */

#include <curses.priv.h>

MODULE_ID("$Id: lib_refresh.c,v 1.24 1999/07/31 11:36:37 juergen Exp $")

int wrefresh(WINDOW *win)
{
int code;

	T((T_CALLED("wrefresh(%p)"), win));

	if (win == curscr) {
		curscr->_clear = TRUE;
		code = doupdate();
	} else if ((code = wnoutrefresh(win)) == OK) {
		if (win->_clear)
			newscr->_clear = TRUE;
		code = doupdate();
		/*
		 * Reset the clearok() flag in case it was set for the special
		 * case in hardscroll.c (if we don't reset it here, we'll get 2
		 * refreshes because the flag is copied from stdscr to newscr).
		 * Resetting the flag shouldn't do any harm, anyway.
		 */
		win->_clear = FALSE;
	}
	returnCode(code);
}

int wnoutrefresh(WINDOW *win)
{
short	limit_x;
short	i, j;
short	begx;
short	begy;
short	m, n;
bool	wide;

	T((T_CALLED("wnoutrefresh(%p)"), win));
#ifdef TRACE
	if (_nc_tracing & TRACE_UPDATE)
	    _tracedump("...win", win);
#endif /* TRACE */

	/*
	 * This function will break badly if we try to refresh a pad.
	 */
	if ((win == 0)
	 || (win->_flags & _ISPAD))
		returnCode(ERR);

	/* put them here so "win == 0" won't break our code */
	begx = win->_begx;
	begy = win->_begy;

	newscr->_bkgd  = win->_bkgd;
	newscr->_attrs = win->_attrs;

	/* merge in change information from all subwindows of this window */
	wsyncdown(win);

	/*
	 * For pure efficiency, we'd want to transfer scrolling information
	 * from the window to newscr whenever the window is wide enough that
	 * its update will dominate the cost of the update for the horizontal
	 * band of newscr that it occupies.  Unfortunately, this threshold
	 * tends to be complex to estimate, and in any case scrolling the
	 * whole band and rewriting the parts outside win's image would look
	 * really ugly.  So.  What we do is consider the window "wide" if it
	 * either (a) occupies the whole width of newscr, or (b) occupies
	 * all but at most one column on either vertical edge of the screen
	 * (this caters to fussy people who put boxes around full-screen
	 * windows).  Note that changing this formula will not break any code,
	 * merely change the costs of various update cases.
	 */
	wide = (begx <= 1 && win->_maxx >= (newscr->_maxx - 1));

	win->_flags &= ~_HASMOVED;

	/*
	 * Microtweaking alert!  This double loop is one of the genuine
	 * hot spots in the code.  Even gcc doesn't seem to do enough
	 * common-subexpression chunking to make it really tense,
	 * so we'll force the issue.
	 */

	/* limit(n) */
	limit_x = win->_maxx;
	/* limit(j) */
	if (limit_x > win->_maxx)
		limit_x = win->_maxx;

	for (i = 0, m = begy + win->_yoffset;
	     i <= win->_maxy && m <= newscr->_maxy;
	     i++, m++) {
		register struct ldat	*nline = &newscr->_line[m];
		register struct ldat	*oline = &win->_line[i];

		if (oline->firstchar != _NOCHANGE) {
			int last = oline->lastchar;

			if (last > limit_x)
				last = limit_x;

			for (j = oline->firstchar, n = j + begx; j <= last; j++, n++) {
				if (oline->text[j] != nline->text[n]) {
					nline->text[n] = oline->text[j];
					CHANGED_CELL(nline, n);
				}
			}

		}

#if USE_SCROLL_HINTS
		if (wide) {
		    int	oind = oline->oldindex;

		    nline->oldindex = (oind == _NEWINDEX) ? _NEWINDEX : begy + oind + win->_yoffset;
		}
#endif /* USE_SCROLL_HINTS */

		oline->firstchar = oline->lastchar = _NOCHANGE;
		if_USE_SCROLL_HINTS(oline->oldindex = i);
	}

	if (win->_clear) {
		win->_clear = FALSE;
		newscr->_clear = TRUE;
	}

	if (! win->_leaveok) {
		newscr->_cury = win->_cury + win->_begy + win->_yoffset;
		newscr->_curx = win->_curx + win->_begx;
	}
	newscr->_leaveok = win->_leaveok;
	
#ifdef TRACE
	if (_nc_tracing & TRACE_UPDATE)
	    _tracedump("newscr", newscr);
#endif /* TRACE */
	returnCode(OK);
}
