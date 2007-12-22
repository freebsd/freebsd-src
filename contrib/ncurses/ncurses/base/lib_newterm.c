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
**	lib_newterm.c
**
**	The newterm() function.
**
*/

#include <curses.priv.h>

#if SVR4_TERMIO && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>		/* clear_screen, cup & friends, cur_term */
#include <tic.h>

MODULE_ID("$Id: lib_newterm.c,v 1.64 2006/01/14 15:36:24 tom Exp $")

#ifndef ONLCR			/* Allows compilation under the QNX 4.2 OS */
#define ONLCR 0
#endif

/*
 * SVr4/XSI Curses specify that hardware echo is turned off in initscr, and not
 * restored during the curses session.  The library simulates echo in software.
 * (The behavior is unspecified if the application enables hardware echo).
 *
 * The newterm function also initializes terminal settings, and since initscr
 * is supposed to behave as if it calls newterm, we do it here.
 */
static NCURSES_INLINE int
_nc_initscr(void)
{
    int result = ERR;

    /* for extended XPG4 conformance requires cbreak() at this point */
    /* (SVr4 curses does this anyway) */
    if (cbreak() == OK) {
	TTY buf;

	buf = cur_term->Nttyb;
#ifdef TERMIOS
	buf.c_lflag &= ~(ECHO | ECHONL);
	buf.c_iflag &= ~(ICRNL | INLCR | IGNCR);
	buf.c_oflag &= ~(ONLCR);
#elif HAVE_SGTTY_H
	buf.sg_flags &= ~(ECHO | CRMOD);
#else
	memset(&buf, 0, sizeof(buf));
#endif
	if ((result = _nc_set_tty_mode(&buf)) == OK)
	    cur_term->Nttyb = buf;
    }
    return result;
}

/*
 * filter() has to be called before either initscr() or newterm(), so there is
 * apparently no way to make this flag apply to some terminals and not others,
 * aside from possibly delaying a filter() call until some terminals have been
 * initialized.
 */
static bool filter_mode = FALSE;

NCURSES_EXPORT(void)
filter(void)
{
    START_TRACE();
    T((T_CALLED("filter")));
    filter_mode = TRUE;
    returnVoid;
}

#if NCURSES_EXT_FUNCS
/*
 * An extension, allowing the application to open a new screen without
 * requiring it to also be filtered.
 */
NCURSES_EXPORT(void)
nofilter(void)
{
    START_TRACE();
    T((T_CALLED("nofilter")));
    filter_mode = FALSE;
    returnVoid;
}
#endif

NCURSES_EXPORT(SCREEN *)
newterm(NCURSES_CONST char *name, FILE *ofp, FILE *ifp)
{
    int value;
    int errret;
    int slk_format = _nc_slk_format;
    SCREEN *current;
    SCREEN *result = 0;

    START_TRACE();
    T((T_CALLED("newterm(\"%s\",%p,%p)"), name, ofp, ifp));

    _nc_handle_sigwinch(0);

    /* allow user to set maximum escape delay from the environment */
    if ((value = _nc_getenv_num("ESCDELAY")) >= 0) {
	ESCDELAY = value;
    }

    /* this loads the capability entry, then sets LINES and COLS */
    if (setupterm(name, fileno(ofp), &errret) == ERR) {
	result = 0;
    } else {
	/*
	 * This actually allocates the screen structure, and saves the original
	 * terminal settings.
	 */
	current = SP;
	_nc_set_screen(0);
	if (_nc_setupscreen(LINES, COLS, ofp, filter_mode, slk_format) == ERR) {
	    _nc_set_screen(current);
	    result = 0;
	} else {
	    /* if the terminal type has real soft labels, set those up */
	    if (slk_format && num_labels > 0 && SLK_STDFMT(slk_format))
		_nc_slk_initialize(stdscr, COLS);

	    SP->_ifd = fileno(ifp);
	    typeahead(fileno(ifp));
#ifdef TERMIOS
	    SP->_use_meta = ((cur_term->Ottyb.c_cflag & CSIZE) == CS8 &&
			     !(cur_term->Ottyb.c_iflag & ISTRIP));
#else
	    SP->_use_meta = FALSE;
#endif
	    SP->_endwin = FALSE;

	    /*
	     * Check whether we can optimize scrolling under dumb terminals in
	     * case we do not have any of these capabilities, scrolling
	     * optimization will be useless.
	     */
	    SP->_scrolling = ((scroll_forward && scroll_reverse) ||
			      ((parm_rindex ||
				parm_insert_line ||
				insert_line) &&
			       (parm_index ||
				parm_delete_line ||
				delete_line)));

	    baudrate();		/* sets a field in the SP structure */

	    SP->_keytry = 0;

	    /*
	     * Check for mismatched graphic-rendition capabilities.  Most SVr4
	     * terminfo trees contain entries that have rmul or rmso equated to
	     * sgr0 (Solaris curses copes with those entries).  We do this only
	     * for curses, since many termcap applications assume that
	     * smso/rmso and smul/rmul are paired, and will not function
	     * properly if we remove rmso or rmul.  Curses applications
	     * shouldn't be looking at this detail.
	     */
#define SGR0_TEST(mode) (mode != 0) && (exit_attribute_mode == 0 || strcmp(mode, exit_attribute_mode))
	    SP->_use_rmso = SGR0_TEST(exit_standout_mode);
	    SP->_use_rmul = SGR0_TEST(exit_underline_mode);

	    /* compute movement costs so we can do better move optimization */
	    _nc_mvcur_init();

	    /* initialize terminal to a sane state */
	    _nc_screen_init();

	    /* Initialize the terminal line settings. */
	    _nc_initscr();

	    _nc_signal_handler(TRUE);

	    result = SP;
	}
    }
    _nc_handle_sigwinch(1);
    returnSP(result);
}
