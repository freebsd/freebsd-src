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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1997,1999                   *
 ****************************************************************************/

#include <curses.priv.h>
#include <term.h>

MODULE_ID("$Id: lib_dft_fgbg.c,v 1.13 2000/07/07 16:50:27 tom Exp $")

/*
 * Modify the behavior of color-pair 0 so that the library doesn't assume that
 * it is white on black.  This is an extension to XSI curses.
 */
int
use_default_colors(void)
{
    T((T_CALLED("use_default_colors()")));
    returnCode(assume_default_colors(C_MASK, C_MASK));
}

/*
 * Modify the behavior of color-pair 0 so that the library assumes that it
 * is something specific, possibly not white on black.
 */
int
assume_default_colors(int fg, int bg)
{
    T((T_CALLED("assume_default_colors(%d,%d)"), fg, bg));

    if (!orig_pair && !orig_colors)
	returnCode(ERR);

    if (initialize_pair)	/* don't know how to handle this */
	returnCode(ERR);

    SP->_default_color = (fg < 0 || fg == C_MASK) || (bg < 0 || bg == C_MASK);
    SP->_has_sgr_39_49 = (tigetflag("AX") == TRUE);
    SP->_default_fg = (fg >= 0) ? (fg & C_MASK) : C_MASK;
    SP->_default_bg = (bg >= 0) ? (bg & C_MASK) : C_MASK;
    if (SP->_color_pairs != 0)
	init_pair(0, fg, bg);
    returnCode(OK);
}
