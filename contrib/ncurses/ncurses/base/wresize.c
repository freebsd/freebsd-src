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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1996,1997                   *
 ****************************************************************************/

#include <curses.priv.h>

MODULE_ID("$Id: wresize.c,v 1.16 2000/03/05 00:14:35 tom Exp $")

/*
 * Reallocate a curses WINDOW struct to either shrink or grow to the specified
 * new lines/columns.  If it grows, the new character cells are filled with
 * blanks.  The application is responsible for repainting the blank area.
 */

#define DOALLOC(p,t,n)  typeRealloc(t, n, p)
#define	ld_ALLOC(p,n)	DOALLOC(p,struct ldat,n)
#define	c_ALLOC(p,n)	DOALLOC(p,chtype,n)

int
wresize(WINDOW *win, int ToLines, int ToCols)
{
    register int row;
    int size_x, size_y;
    struct ldat *pline;
    chtype blank;

#ifdef TRACE
    T((T_CALLED("wresize(%p,%d,%d)"), win, ToLines, ToCols));
    if (win) {
	TR(TRACE_UPDATE, ("...beg (%d, %d), max(%d,%d), reg(%d,%d)",
		win->_begy, win->_begx,
		win->_maxy, win->_maxx,
		win->_regtop, win->_regbottom));
	if (_nc_tracing & TRACE_UPDATE)
	    _tracedump("...before", win);
    }
#endif

    if (!win || --ToLines < 0 || --ToCols < 0)
	returnCode(ERR);

    size_x = win->_maxx;
    size_y = win->_maxy;

    if (ToLines == size_y
	&& ToCols == size_x)
	returnCode(OK);

    if ((win->_flags & _SUBWIN)) {
	/*
	 * Check if the new limits will fit into the parent window's size.  If
	 * not, do not resize.  We could adjust the location of the subwindow,
	 * but the application may not like that.
	 */
	if (win->_pary + ToLines > win->_parent->_maxy
	    || win->_parx + ToCols > win->_parent->_maxx) {
	    returnCode(ERR);
	}
	pline = win->_parent->_line;
    } else {
	pline = 0;
    }

    /*
     * If the number of lines has changed, adjust the size of the overall
     * vector:
     */
    if (ToLines != size_y) {
	if (!(win->_flags & _SUBWIN)) {
	    for (row = ToLines + 1; row <= size_y; row++)
		free((char *) (win->_line[row].text));
	}

	win->_line = ld_ALLOC(win->_line, ToLines + 1);
	if (win->_line == 0)
	    returnCode(ERR);

	for (row = size_y + 1; row <= ToLines; row++) {
	    win->_line[row].text = 0;
	    win->_line[row].firstchar = 0;
	    win->_line[row].lastchar = ToCols;
	    if ((win->_flags & _SUBWIN)) {
		win->_line[row].text =
		    &pline[win->_pary + row].text[win->_parx];
	    }
	}
    }

    /*
     * Adjust the width of the columns:
     */
    blank = _nc_background(win);
    for (row = 0; row <= ToLines; row++) {
	chtype *s = win->_line[row].text;
	int begin = (s == 0) ? 0 : size_x + 1;
	int end = ToCols;

	if_USE_SCROLL_HINTS(win->_line[row].oldindex = row);

	if (ToCols != size_x || s == 0) {
	    if (!(win->_flags & _SUBWIN)) {
		win->_line[row].text = s = c_ALLOC(s, ToCols + 1);
		if (win->_line[row].text == 0)
		    returnCode(ERR);
	    } else if (s == 0) {
		win->_line[row].text = s =
		    &pline[win->_pary + row].text[win->_parx];
	    }

	    if (end >= begin) {	/* growing */
		if (win->_line[row].firstchar < begin)
		    win->_line[row].firstchar = begin;
		win->_line[row].lastchar = ToCols;
		do {
		    s[end] = blank;
		} while (--end >= begin);
	    } else {		/* shrinking */
		win->_line[row].firstchar = 0;
		win->_line[row].lastchar = ToCols;
	    }
	}
    }

    /*
     * Finally, adjust the parameters showing screen size and cursor
     * position:
     */
    win->_maxx = ToCols;
    win->_maxy = ToLines;

    if (win->_regtop > win->_maxy)
	win->_regtop = win->_maxy;
    if (win->_regbottom > win->_maxy
	|| win->_regbottom == size_y)
	win->_regbottom = win->_maxy;

    if (win->_curx > win->_maxx)
	win->_curx = win->_maxx;
    if (win->_cury > win->_maxy)
	win->_cury = win->_maxy;

#ifdef TRACE
    TR(TRACE_UPDATE, ("...beg (%d, %d), max(%d,%d), reg(%d,%d)",
	    win->_begy, win->_begx,
	    win->_maxy, win->_maxx,
	    win->_regtop, win->_regbottom));
    if (_nc_tracing & TRACE_UPDATE)
	_tracedump("...after:", win);
#endif
    returnCode(OK);
}
