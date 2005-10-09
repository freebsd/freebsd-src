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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
**	lib_data.c
**
**	Common data that may/may not be allocated, but is referenced globally
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_data.c,v 1.16 2000/12/10 02:55:07 tom Exp $")

/*
 * OS/2's native linker complains if we don't initialize public data when
 * constructing a dll (reported by J.J.G.Ripoll).
 */
NCURSES_EXPORT_VAR(WINDOW *)
stdscr = 0;
NCURSES_EXPORT_VAR(WINDOW *)
curscr = 0;
NCURSES_EXPORT_VAR(WINDOW *)
newscr = 0;

NCURSES_EXPORT_VAR(SCREEN *) _nc_screen_chain = 0;

/*
 * The variable 'SP' will be defined as a function on systems that cannot link
 * data-only modules, since it is used in a lot of places within ncurses and we
 * cannot guarantee that any application will use any particular function.  We
 * put the WINDOW variables in this module, because it appears that any
 * application that uses them will also use 'SP'.
 *
 * This module intentionally does not reference other ncurses modules, to avoid
 * module coupling that increases the size of the executable.
 */
#if BROKEN_LINKER
     static SCREEN *my_screen;

NCURSES_EXPORT(SCREEN *)
_nc_screen(void)
{
    return my_screen;
}

NCURSES_EXPORT(int)
_nc_alloc_screen(void)
{
    return ((my_screen = typeCalloc(SCREEN, 1)) != 0);
}

NCURSES_EXPORT(void)
_nc_set_screen(SCREEN * sp)
{
    my_screen = sp;
}

#else
NCURSES_EXPORT_VAR(SCREEN *) SP = NULL;		/* Some linkers require initialized data... */
#endif
