/****************************************************************************
 * Copyright (c) 1998,2000 Free Software Foundation, Inc.                   *
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
**	lib_box.c
**
**	The routine wborder().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_box.c,v 1.13 2000/12/10 02:43:26 tom Exp $")

NCURSES_EXPORT(int)
wborder
(WINDOW *win,
 chtype ls, chtype rs, chtype ts, chtype bs,
 chtype tl, chtype tr, chtype bl, chtype br)
{
    NCURSES_SIZE_T i;
    NCURSES_SIZE_T endx, endy;

    T((T_CALLED("wborder(%p,%s,%s,%s,%s,%s,%s,%s,%s)"),
       win,
       _tracechtype2(1, ls),
       _tracechtype2(2, rs),
       _tracechtype2(3, ts),
       _tracechtype2(4, bs),
       _tracechtype2(5, tl),
       _tracechtype2(6, tr),
       _tracechtype2(7, bl),
       _tracechtype2(8, br)));

    if (!win)
	returnCode(ERR);

    if (ls == 0)
	ls = ACS_VLINE;
    if (rs == 0)
	rs = ACS_VLINE;
    if (ts == 0)
	ts = ACS_HLINE;
    if (bs == 0)
	bs = ACS_HLINE;
    if (tl == 0)
	tl = ACS_ULCORNER;
    if (tr == 0)
	tr = ACS_URCORNER;
    if (bl == 0)
	bl = ACS_LLCORNER;
    if (br == 0)
	br = ACS_LRCORNER;

    ls = _nc_render(win, ls);
    rs = _nc_render(win, rs);
    ts = _nc_render(win, ts);
    bs = _nc_render(win, bs);
    tl = _nc_render(win, tl);
    tr = _nc_render(win, tr);
    bl = _nc_render(win, bl);
    br = _nc_render(win, br);

    T(("using %#lx, %#lx, %#lx, %#lx, %#lx, %#lx, %#lx, %#lx",
       ls, rs, ts, bs, tl, tr, bl, br));

    endx = win->_maxx;
    endy = win->_maxy;

    for (i = 0; i <= endx; i++) {
	win->_line[0].text[i] = ts;
	win->_line[endy].text[i] = bs;
    }
    win->_line[endy].firstchar = win->_line[0].firstchar = 0;
    win->_line[endy].lastchar = win->_line[0].lastchar = endx;

    for (i = 0; i <= endy; i++) {
	win->_line[i].text[0] = ls;
	win->_line[i].text[endx] = rs;
	win->_line[i].firstchar = 0;
	win->_line[i].lastchar = endx;
    }
    win->_line[0].text[0] = tl;
    win->_line[0].text[endx] = tr;
    win->_line[endy].text[0] = bl;
    win->_line[endy].text[endx] = br;

    _nc_synchook(win);
    returnCode(OK);
}
