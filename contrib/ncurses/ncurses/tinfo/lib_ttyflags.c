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

/*
 *		def_prog_mode()
 *		def_shell_mode()
 *		reset_prog_mode()
 *		reset_shell_mode()
 *		savetty()
 *		resetty()
 */

#include <curses.priv.h>
#include <term.h>	/* cur_term */

MODULE_ID("$Id: lib_ttyflags.c,v 1.3 1999/10/22 21:38:55 tom Exp $")

#undef tabs

#ifdef TAB3
# define tabs TAB3
#else
# ifdef XTABS
#  define tabs XTABS
# else
#  ifdef OXTABS
#   define tabs OXTABS
#  else
#   define tabs 0
#  endif
# endif
#endif

int _nc_get_tty_mode(TTY *buf)
{
	if (cur_term == 0
	 || GET_TTY(cur_term->Filedes, buf) != 0)
		return(ERR);
	TR(TRACE_BITS,("_nc_get_tty_mode: %s", _nc_tracebits()));
	return (OK);
}

int _nc_set_tty_mode(TTY *buf)
{
	if (cur_term == 0
	 || SET_TTY(cur_term->Filedes, buf) != 0)
		return(ERR);
	TR(TRACE_BITS,("_nc_set_tty_mode: %s", _nc_tracebits()));
	return (OK);
}

int def_shell_mode(void)
{
	T((T_CALLED("def_shell_mode()")));

	/*
	 * Turn off the XTABS bit in the tty structure if it was on.  If XTABS
	 * was on, remove the tab and backtab capabilities.
	 */

	if (_nc_get_tty_mode(&cur_term->Ottyb) != OK)
		returnCode(ERR);
#ifdef TERMIOS
	if (cur_term->Ottyb.c_oflag & tabs)
		tab = back_tab = NULL;
#else
	if (cur_term->Ottyb.sg_flags & XTABS)
		tab = back_tab = NULL;
#endif
	returnCode(OK);
}

int def_prog_mode(void)
{
	T((T_CALLED("def_prog_mode()")));

	if (_nc_get_tty_mode(&cur_term->Nttyb) != OK)
		returnCode(ERR);
#ifdef TERMIOS
	cur_term->Nttyb.c_oflag &= ~tabs;
#else
	cur_term->Nttyb.sg_flags &= ~XTABS;
#endif
	returnCode(OK);
}

int reset_prog_mode(void)
{
	T((T_CALLED("reset_prog_mode()")));

	if (cur_term != 0) {
		_nc_set_tty_mode(&cur_term->Nttyb);
		if (SP) {
			if (stdscr && stdscr->_use_keypad)
				_nc_keypad(TRUE);
			NC_BUFFERED(TRUE);
		}
		returnCode(OK);
	}
	returnCode(ERR);
}

int reset_shell_mode(void)
{
	T((T_CALLED("reset_shell_mode()")));

	if (cur_term != 0) {
		if (SP)
		{
			_nc_keypad(FALSE);
			_nc_flush();
			NC_BUFFERED(FALSE);
		}
		returnCode(_nc_set_tty_mode(&cur_term->Ottyb));
	}
	returnCode(ERR);
}

/*
**	savetty()  and  resetty()
**
*/

static TTY   buf;

int savetty(void)
{
	T((T_CALLED("savetty()")));

	returnCode(_nc_get_tty_mode(&buf));
}

int resetty(void)
{
	T((T_CALLED("resetty()")));

	returnCode(_nc_set_tty_mode(&buf));
}
