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
**	lib_newterm.c
**
**	The newterm() function.
**
*/

#include <curses.priv.h>

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>		/* clear_screen, cup & friends, cur_term */
#include <tic.h>

MODULE_ID("$Id: lib_newterm.c,v 1.46 2000/07/01 22:26:22 tom Exp $")

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
static inline int
_nc_initscr(void)
{
    /* for extended XPG4 conformance requires cbreak() at this point */
    /* (SVr4 curses does this anyway) */
    cbreak();

#ifdef TERMIOS
    cur_term->Nttyb.c_lflag &= ~(ECHO | ECHONL);
    cur_term->Nttyb.c_iflag &= ~(ICRNL | INLCR | IGNCR);
    cur_term->Nttyb.c_oflag &= ~(ONLCR);
#else
    cur_term->Nttyb.sg_flags &= ~(ECHO | CRMOD);
#endif
    return _nc_set_tty_mode(&cur_term->Nttyb);
}

/*
 * filter() has to be called before either initscr() or newterm(), so there is
 * apparently no way to make this flag apply to some terminals and not others,
 * aside from possibly delaying a filter() call until some terminals have been
 * initialized.
 */
static int filter_mode = FALSE;

void
filter(void)
{
    filter_mode = TRUE;
}

SCREEN *
newterm(NCURSES_CONST char *name, FILE * ofp, FILE * ifp)
{
    int errret;
    int slk_format = _nc_slk_format;
    SCREEN *current;
#ifdef TRACE
    int t = _nc_getenv_num("NCURSES_TRACE");

    if (t >= 0)
	trace(t);
#endif

    T((T_CALLED("newterm(\"%s\",%p,%p)"), name, ofp, ifp));

    /* this loads the capability entry, then sets LINES and COLS */
    if (setupterm(name, fileno(ofp), &errret) == ERR)
	return 0;

    /* implement filter mode */
    if (filter_mode) {
	LINES = 1;

	if (VALID_NUMERIC(init_tabs))
	    TABSIZE = init_tabs;
	else
	    TABSIZE = 8;

	T(("TABSIZE = %d", TABSIZE));

	clear_screen = 0;
	cursor_down = parm_down_cursor = 0;
	cursor_address = 0;
	cursor_up = parm_up_cursor = 0;
	row_address = 0;

	cursor_home = carriage_return;
    }

    /* If we must simulate soft labels, grab off the line to be used.
       We assume that we must simulate, if it is none of the standard
       formats (4-4  or 3-2-3) for which there may be some hardware
       support. */
    if (num_labels <= 0 || !SLK_STDFMT(slk_format))
	if (slk_format) {
	    if (ERR == _nc_ripoffline(-SLK_LINES(slk_format),
		    _nc_slk_initialize))
		return 0;
	}
    /* this actually allocates the screen structure, and saves the
     * original terminal settings.
     */
    current = SP;
    _nc_set_screen(0);
    if (_nc_setupscreen(LINES, COLS, ofp) == ERR) {
	_nc_set_screen(current);
	return 0;
    }

    /* if the terminal type has real soft labels, set those up */
    if (slk_format && num_labels > 0 && SLK_STDFMT(slk_format))
	_nc_slk_initialize(stdscr, COLS);

    SP->_ifd = fileno(ifp);
    SP->_checkfd = fileno(ifp);
    typeahead(fileno(ifp));
#ifdef TERMIOS
    SP->_use_meta = ((cur_term->Ottyb.c_cflag & CSIZE) == CS8 &&
	!(cur_term->Ottyb.c_iflag & ISTRIP));
#else
    SP->_use_meta = FALSE;
#endif
    SP->_endwin = FALSE;

    /* Check whether we can optimize scrolling under dumb terminals in case
     * we do not have any of these capabilities, scrolling optimization
     * will be useless.
     */
    SP->_scrolling = ((scroll_forward && scroll_reverse) ||
	((parm_rindex || parm_insert_line || insert_line) &&
	    (parm_index || parm_delete_line || delete_line)));

    baudrate();			/* sets a field in the SP structure */

    SP->_keytry = 0;

    /*
     * Check for mismatched graphic-rendition capabilities.  Most SVr4
     * terminfo trees contain entries that have rmul or rmso equated to
     * sgr0 (Solaris curses copes with those entries).  We do this only for
     * curses, since many termcap applications assume that smso/rmso and
     * smul/rmul are paired, and will not function properly if we remove
     * rmso or rmul.  Curses applications shouldn't be looking at this
     * detail.
     */
#define SGR0_TEST(mode) (mode != 0) && (exit_attribute_mode == 0 || strcmp(mode, exit_attribute_mode))
    SP->_use_rmso = SGR0_TEST(exit_standout_mode);
    SP->_use_rmul = SGR0_TEST(exit_underline_mode);

#ifdef USE_WIDEC_SUPPORT
    /*
     * XFree86 xterm can be configured to support UTF-8 based on environment
     * variable settings.
     */
    {
	char *s;
	if (((s = getenv("LC_ALL")) != 0
		|| (s = getenv("LC_CTYPE")) != 0
		|| (s = getenv("LANG")) != 0)
	    && strstr(s, "UTF-8") != 0) {
	    SP->_outch = _nc_utf8_outch;
	}
    }
#endif

    /* compute movement costs so we can do better move optimization */
    _nc_mvcur_init();

    /* initialize terminal to a sane state */
    _nc_screen_init();

    /* Initialize the terminal line settings. */
    _nc_initscr();

    _nc_signal_handler(TRUE);

    T((T_RETURN("%p"), SP));
    return (SP);
}
