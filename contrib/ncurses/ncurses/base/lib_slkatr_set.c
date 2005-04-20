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
 *  Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1998                  *
 ****************************************************************************/

/*
 *	lib_slkatr_set.c
 *	Soft key routines.
 *      Set the labels attributes
 */
#include <curses.priv.h>

MODULE_ID("$Id: lib_slkatr_set.c,v 1.5 2000/12/10 02:43:27 tom Exp $")

NCURSES_EXPORT(int)
slk_attr_set
(const attr_t attr, short color_pair_number, void *opts)
{
    T((T_CALLED("slk_attr_set(%s,%d)"), _traceattr(attr), color_pair_number));

    if (SP != 0 && SP->_slk != 0 && !opts &&
	color_pair_number >= 0 && color_pair_number < COLOR_PAIRS) {
	SP->_slk->attr = attr;
	toggle_attr_on(SP->_slk->attr, COLOR_PAIR(color_pair_number));
	returnCode(OK);
    } else
	returnCode(ERR);
}
