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

/* $FreeBSD$ */

/*
 *	raw.c
 *
 *	Routines:
 *		raw()
 *		cbreak()
 *		noraw()
 *		nocbreak()
 *		qiflush()
 *		noqiflush()
 *		intrflush()
 *
 */

#include <curses.priv.h>
#include <term.h>		/* cur_term */

MODULE_ID("$Id: lib_raw.c,v 1.8 2000/09/02 18:08:48 tom Exp $")

#if SVR4_TERMIO && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#if HAVE_SYS_TERMIO_H
#include <sys/termio.h>		/* needed for ISC */
#endif

#ifdef __EMX__
#include <io.h>
#endif

#define COOKED_INPUT	(IXON|BRKINT|PARMRK)

#ifdef TRACE
#define BEFORE(N)	if (_nc_tracing&TRACE_BITS) _tracef("%s before bits: %s", N, _nc_tracebits())
#define AFTER(N)	if (_nc_tracing&TRACE_BITS) _tracef("%s after bits: %s", N, _nc_tracebits())
#else
#define BEFORE(s)
#define AFTER(s)
#endif /* TRACE */

int
raw(void)
{
    T((T_CALLED("raw()")));
    if (SP != 0 && cur_term != 0) {

	SP->_raw = TRUE;
	SP->_cbreak = 1;

#ifdef __EMX__
	setmode(SP->_ifd, O_BINARY);
#endif

#ifdef TERMIOS
	BEFORE("raw");
	cur_term->Nttyb.c_lflag &= ~(ICANON | ISIG | IEXTEN);
	cur_term->Nttyb.c_iflag &= ~(COOKED_INPUT);
	cur_term->Nttyb.c_cc[VMIN] = 1;
	cur_term->Nttyb.c_cc[VTIME] = 0;
	AFTER("raw");
#else
	cur_term->Nttyb.sg_flags |= RAW;
#endif
	returnCode(_nc_set_tty_mode(&cur_term->Nttyb));
    }
    returnCode(ERR);
}

int
cbreak(void)
{
    T((T_CALLED("cbreak()")));

    SP->_cbreak = 1;

#ifdef __EMX__
    setmode(SP->_ifd, O_BINARY);
#endif

#ifdef TERMIOS
    BEFORE("cbreak");
    cur_term->Nttyb.c_lflag &= ~ICANON;
    cur_term->Nttyb.c_iflag &= ~ICRNL;
    cur_term->Nttyb.c_lflag |= ISIG;
    cur_term->Nttyb.c_cc[VMIN] = 1;
    cur_term->Nttyb.c_cc[VTIME] = 0;
    AFTER("cbreak");
#else
    cur_term->Nttyb.sg_flags |= CBREAK;
#endif
    returnCode(_nc_set_tty_mode(&cur_term->Nttyb));
}

void
qiflush(void)
{
    T((T_CALLED("qiflush()")));

    /*
     * Note: this implementation may be wrong.  See the comment under
     * intrflush().
     */

#ifdef TERMIOS
    BEFORE("qiflush");
    cur_term->Nttyb.c_lflag &= ~(NOFLSH);
    AFTER("qiflush");
    (void) _nc_set_tty_mode(&cur_term->Nttyb);
    returnVoid;
#endif
}

int
noraw(void)
{
    T((T_CALLED("noraw()")));

    SP->_raw = FALSE;
    SP->_cbreak = 0;

#ifdef __EMX__
    setmode(SP->_ifd, O_TEXT);
#endif

#ifdef TERMIOS
    BEFORE("noraw");
    cur_term->Nttyb.c_lflag |= ISIG | ICANON |
	(cur_term->Ottyb.c_lflag & IEXTEN);
    cur_term->Nttyb.c_iflag |= COOKED_INPUT;
    AFTER("noraw");
#else
    cur_term->Nttyb.sg_flags &= ~(RAW | CBREAK);
#endif
    returnCode(_nc_set_tty_mode(&cur_term->Nttyb));
}

int
nocbreak(void)
{
    T((T_CALLED("nocbreak()")));

    SP->_cbreak = 0;

#ifdef __EMX__
    setmode(SP->_ifd, O_TEXT);
#endif

#ifdef TERMIOS
    BEFORE("nocbreak");
    cur_term->Nttyb.c_lflag |= ICANON;
    cur_term->Nttyb.c_iflag |= ICRNL;
    AFTER("nocbreak");
#else
    cur_term->Nttyb.sg_flags &= ~CBREAK;
#endif
    returnCode(_nc_set_tty_mode(&cur_term->Nttyb));
}

void
noqiflush(void)
{
    T((T_CALLED("noqiflush()")));

    /*
     * Note: this implementation may be wrong.  See the comment under
     * intrflush().
     */

#ifdef TERMIOS
    BEFORE("noqiflush");
    cur_term->Nttyb.c_lflag |= NOFLSH;
    AFTER("noqiflush");
    (void) _nc_set_tty_mode(&cur_term->Nttyb);
    returnVoid;
#endif
}

int
intrflush(WINDOW *win GCC_UNUSED, bool flag)
{
    T((T_CALLED("intrflush(%d)"), flag));

    /*
     * This call does the same thing as the qiflush()/noqiflush() pair.  We
     * know for certain that SVr3 intrflush() tweaks the NOFLSH bit; on the
     * other hand, the match (in the SVr4 man pages) between the language
     * describing NOFLSH in termio(7) and the language describing
     * qiflush()/noqiflush() in curs_inopts(3x) is too exact to be coincidence.
     */

#ifdef TERMIOS
    BEFORE("intrflush");
    if (flag)
	cur_term->Nttyb.c_lflag &= ~(NOFLSH);
    else
	cur_term->Nttyb.c_lflag |= (NOFLSH);
    AFTER("intrflush");
    returnCode(_nc_set_tty_mode(&cur_term->Nttyb));
#else
    returnCode(ERR);
#endif
}
