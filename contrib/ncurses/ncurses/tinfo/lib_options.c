/****************************************************************************
 * Copyright (c) 1998-2005,2006 Free Software Foundation, Inc.              *
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
**	lib_options.c
**
**	The routines to handle option setting.
**
*/

#include <curses.priv.h>

#include <term.h>

MODULE_ID("$Id: lib_options.c,v 1.49 2006/03/04 19:28:25 tom Exp $")

NCURSES_EXPORT(int)
idlok(WINDOW *win, bool flag)
{
    T((T_CALLED("idlok(%p,%d)"), win, flag));

    if (win) {
	_nc_idlok = win->_idlok = (flag && (has_il() || change_scroll_region));
	returnCode(OK);
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(void)
idcok(WINDOW *win, bool flag)
{
    T((T_CALLED("idcok(%p,%d)"), win, flag));

    if (win)
	_nc_idcok = win->_idcok = (flag && has_ic());

    returnVoid;
}

NCURSES_EXPORT(int)
halfdelay(int t)
{
    T((T_CALLED("halfdelay(%d)"), t));

    if (t < 1 || t > 255 || SP == 0)
	returnCode(ERR);

    cbreak();
    SP->_cbreak = t + 1;
    returnCode(OK);
}

NCURSES_EXPORT(int)
nodelay(WINDOW *win, bool flag)
{
    T((T_CALLED("nodelay(%p,%d)"), win, flag));

    if (win) {
	if (flag == TRUE)
	    win->_delay = 0;
	else
	    win->_delay = -1;
	returnCode(OK);
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(int)
notimeout(WINDOW *win, bool f)
{
    T((T_CALLED("notimeout(%p,%d)"), win, f));

    if (win) {
	win->_notimeout = f;
	returnCode(OK);
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(void)
wtimeout(WINDOW *win, int delay)
{
    T((T_CALLED("wtimeout(%p,%d)"), win, delay));

    if (win) {
	win->_delay = delay;
    }
    returnVoid;
}

NCURSES_EXPORT(int)
keypad(WINDOW *win, bool flag)
{
    T((T_CALLED("keypad(%p,%d)"), win, flag));

    if (win) {
	win->_use_keypad = flag;
	returnCode(_nc_keypad(flag));
    } else
	returnCode(ERR);
}

NCURSES_EXPORT(int)
meta(WINDOW *win GCC_UNUSED, bool flag)
{
    int result = ERR;

    /* Ok, we stay relaxed and don't signal an error if win is NULL */
    T((T_CALLED("meta(%p,%d)"), win, flag));

    if (SP != 0) {
	SP->_use_meta = flag;

	if (flag && meta_on) {
	    TPUTS_TRACE("meta_on");
	    putp(meta_on);
	} else if (!flag && meta_off) {
	    TPUTS_TRACE("meta_off");
	    putp(meta_off);
	}
	result = OK;
    }
    returnCode(result);
}

/* curs_set() moved here to narrow the kernel interface */

NCURSES_EXPORT(int)
curs_set(int vis)
{
    int result = ERR;

    T((T_CALLED("curs_set(%d)"), vis));
    if (SP != 0 && vis >= 0 && vis <= 2) {
	int cursor = SP->_cursor;

	if (vis == cursor) {
	    result = cursor;
	} else {
	    result = (cursor == -1 ? 1 : cursor);
	    switch (vis) {
	    case 2:
		if (cursor_visible) {
		    TPUTS_TRACE("cursor_visible");
		    putp(cursor_visible);
		} else
		    result = ERR;
		break;
	    case 1:
		if (cursor_normal) {
		    TPUTS_TRACE("cursor_normal");
		    putp(cursor_normal);
		} else
		    result = ERR;
		break;
	    case 0:
		if (cursor_invisible) {
		    TPUTS_TRACE("cursor_invisible");
		    putp(cursor_invisible);
		} else
		    result = ERR;
		break;
	    }
	    SP->_cursor = vis;
	    _nc_flush();
	}
    }
    returnCode(result);
}

NCURSES_EXPORT(int)
typeahead(int fd)
{
    T((T_CALLED("typeahead(%d)"), fd));
    if (SP != 0) {
	SP->_checkfd = fd;
	returnCode(OK);
    } else {
	returnCode(ERR);
    }
}

/*
**      has_key()
**
**      Return TRUE if the current terminal has the given key
**
*/

#if NCURSES_EXT_FUNCS
static int
has_key_internal(int keycode, struct tries *tp)
{
    if (tp == 0)
	return (FALSE);
    else if (tp->value == keycode)
	return (TRUE);
    else
	return (has_key_internal(keycode, tp->child)
		|| has_key_internal(keycode, tp->sibling));
}

NCURSES_EXPORT(int)
has_key(int keycode)
{
    T((T_CALLED("has_key(%d)"), keycode));
    returnCode(SP != 0 ? has_key_internal(keycode, SP->_keytry) : FALSE);
}
#endif /* NCURSES_EXT_FUNCS */

/* Turn the keypad on/off
 *
 * Note:  we flush the output because changing this mode causes some terminals
 * to emit different escape sequences for cursor and keypad keys.  If we don't
 * flush, then the next wgetch may get the escape sequence that corresponds to
 * the terminal state _before_ switching modes.
 */
NCURSES_EXPORT(int)
_nc_keypad(bool flag)
{
    if (flag && keypad_xmit) {
	TPUTS_TRACE("keypad_xmit");
	putp(keypad_xmit);
	_nc_flush();
    } else if (!flag && keypad_local) {
	TPUTS_TRACE("keypad_local");
	putp(keypad_local);
	_nc_flush();
    }

    if (SP != 0) {
	if (flag && !SP->_tried) {
	    _nc_init_keytry();
	    SP->_tried = TRUE;
	}
	SP->_keypad_on = flag;
    }
    return (OK);
}
