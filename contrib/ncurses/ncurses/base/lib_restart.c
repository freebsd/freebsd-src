/****************************************************************************
 * Copyright (c) 1998-2002,2006 Free Software Foundation, Inc.              *
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
 *     and: Thomas E. Dickey                        1996-on                 *
 ****************************************************************************/

/*
 * Terminfo-only terminal setup routines:
 *
 *		int restartterm(const char *, int, int *)
 *		TERMINAL *set_curterm(TERMINAL *)
 *		int del_curterm(TERMINAL *)
 */

#include <curses.priv.h>

#if SVR4_TERMIO && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>		/* lines, columns, cur_term */

MODULE_ID("$Id: lib_restart.c,v 1.6 2006/01/14 15:58:23 tom Exp $")

NCURSES_EXPORT(int)
restartterm(NCURSES_CONST char *termp, int filenum, int *errret)
{
    int saveecho = SP->_echo;
    int savecbreak = SP->_cbreak;
    int saveraw = SP->_raw;
    int savenl = SP->_nl;
    int result;

    T((T_CALLED("restartterm(%s,%d,%p)"), termp, filenum, errret));

    _nc_handle_sigwinch(0);
    if (setupterm(termp, filenum, errret) != OK) {
	result = ERR;
    } else {

	if (saveecho)
	    echo();
	else
	    noecho();

	if (savecbreak) {
	    cbreak();
	    noraw();
	} else if (saveraw) {
	    nocbreak();
	    raw();
	} else {
	    nocbreak();
	    noraw();
	}
	if (savenl)
	    nl();
	else
	    nonl();

	reset_prog_mode();

#if USE_SIZECHANGE
	_nc_update_screensize();
#endif

	result = OK;
    }
    _nc_handle_sigwinch(1);
    returnCode(result);
}
